/*
 * Copyright © 2022 Octopull Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "miriway_app_switcher.h"
#include "miriway_child_control.h"
#include "miriway_commands.h"
#include "miriway_documenting_store.h"
#include "miriway_magnifier.h"
#include "miriway_policy.h"
#include "miriway_ext_workspace_v1.h"

#include <mir/abnormal_exit.h>
#include <miral/append_event_filter.h>
#include <miral/bounce_keys.h>
#include <miral/config_file.h>
#include <miral/configuration_option.h>
#include <miral/cursor_scale.h>
#include <miral/cursor_theme.h>
#include <miral/decorations.h>
#include <miral/display_configuration_option.h>
#include <miral/external_client.h>
#include <miral/hover_click.h>
#include <miral/idle_listener.h>
#include <miral/input_configuration.h>
#include <miral/keymap.h>
#include <miral/live_config.h>
#include <miral/live_config_ini_file.h>
#include <miral/output_filter.h>
#include <miral/runner.h>
#include <miral/session_lock_listener.h>
#include <miral/set_window_management_policy.h>
#include <miral/slow_keys.h>
#include <miral/sticky_keys.h>
#include <miral/version.h>
#include <miral/wayland_extensions.h>
#include <miral/wayland_tools.h>
#include <miral/x11_support.h>

#include <mir/log.h>

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>

using namespace miral;
using namespace miriway;

namespace
{
auto to_key(std::string_view as_string) -> xkb_keysym_t
{
    if (auto const result = xkb_keysym_from_name(as_string.data(), XKB_KEYSYM_CASE_INSENSITIVE); result != XKB_KEY_NoSymbol)
    {
        return result;
    }

    throw mir::AbnormalExit((std::string{ "Unrecognised key in config: "} + as_string.data()).c_str());
}

std::map<std::string, ShellCommands::CmdFunctor> const wm_command =
    {
        { "dock-left", [](ShellCommands* sc, bool shift) { sc->dock_active_window_left(shift); } },
        { "dock-right", [](ShellCommands* sc, bool shift) { sc->dock_active_window_right(shift); } },
        { "toggle-maximized", [](ShellCommands* sc, bool shift) { sc->toggle_maximized_restored(shift); } },
        { "toggle-always-on-top", [](ShellCommands* sc, bool shift) { sc->toggle_always_on_top(shift); } },
        { "workspace-begin", [](ShellCommands* sc, bool shift) { sc->workspace_begin(shift); } },
        { "workspace-end", [](ShellCommands* sc, bool shift) { sc->workspace_end(shift); } },
        { "workspace-up", [](ShellCommands* sc, bool shift) { sc->workspace_up(shift); } },
        { "workspace-down", [](ShellCommands* sc, bool shift) { sc->workspace_down(shift); } },
        { "exit", [](ShellCommands* sc, bool shift) { sc->exit(shift); } },
    };

// Build a list of shell components from "<commands>" values and launch them all after startup
struct ShellComponents
{
    explicit ShellComponents(std::function<void(std::vector<std::string> const& command_line)> launch) :
        launch{std::move(launch)}{}

    void populate(std::vector<std::string> const& cmds)
    {
        std::transform(begin(cmds), end(cmds), back_inserter(commands), ExternalClientLauncher::split_command);
    }

    void launch_all() const
    {
        for (auto const& command : commands)
        {
            launch(command);
        }
    }
private:
    std::function<void (std::vector<std::string> const& command_line)> const launch;
    std::vector<std::vector<std::string>> commands;
};

class LockScreen
{
public:
    explicit LockScreen(
        std::function<void(std::vector<std::string> const& command_line)> launch,
        std::function<void(std::string name)> const& conditionally_enable) :
        launch{std::move(launch)},
        lockscreen_option{[this](mir::optional_value<std::string> const& app)
            {
                if (app.is_set())
                {
                    lockscreen_app = ExternalClientLauncher::split_command(app.value());
                }
            },
            "lockscreen-app",
            "Lockscreen app to be triggered when the compositor session is locked"
        },
        lockscreen_on_idle{[this](bool on)
            { if (on) idle_listener.on_off([this]() { launch_lockscreen(); }); },
                "lockscreen-on-idle",
                "Trigger lockscreen on idle timeout",
                true
        }
    {
        conditionally_enable(WaylandExtensions::ext_session_lock_manager_v1);
    }
    void operator()(mir::Server& server) const
    {
        lockscreen_option(server);
        session_locker(server);
        lockscreen_on_idle(server);
        idle_listener(server);
    }

private:
    void launch_lockscreen() const
    {
        if (lockscreen_app) launch(lockscreen_app.value());
    }
    std::function<void(std::vector<std::string> const& command_line)> const launch;
    ConfigurationOption const lockscreen_option;
    SessionLockListener const session_locker{
        [this]{ launch_lockscreen(); },
        [] {}
    };
    std::optional<std::vector<std::string>> lockscreen_app;
    ConfigurationOption const lockscreen_on_idle;
    IdleListener idle_listener;
};

// Build an index of commands from "<key>:<commands>" values and launch them by <key> (if found)
class CommandIndex
{
public:
    CommandIndex(
        live_config::Store& store,
        std::string const& option,
        std::string const& description,
        std::function<void(std::vector<std::string> const&)> launch) :
        launch{std::move(launch)}
    {
        auto settings_key = option;
        std::replace(settings_key.begin(), settings_key.end(), '-', '_');
        store.add_strings_attribute(
            live_config::Key{{"command", settings_key}},
            "command " + description,
            [this](live_config::Key const&, std::optional<std::span<std::string const>> value)
            {
                commands.clear();
                if (value) populate(std::vector<std::string>(value->begin(), value->end()));
            });
    }

    bool try_command_for(xkb_keysym_t key_code, bool with_shift, ShellCommands* cmd) const
    {
        auto const i = commands.find(xkb_keysym_to_lower(key_code));
        bool const found = i != end(commands);
        if (found) i->second(cmd, with_shift);
        return found;
    }

private:
    std::function<void (std::vector<std::string> const& command_line)> const launch;
    mutable std::map<xkb_keysym_t, ShellCommands::CmdFunctor> commands;

    void populate(std::vector<std::string> const& config_cmds) const
    {
        for (auto const& command : config_cmds)
        {
            if (auto const split = command.find(':'); split >= 1 && split+1 < command.size())
            {
                if (command[split+1] != '@')
                {
                    commands[to_key(command.substr(0, split))] =
                        [this, argv = ExternalClientLauncher::split_command(command.substr(split+1))](miriway::ShellCommands*, bool)
                        {
                            launch(argv);
                        };
                }
                else
                {
                    if (auto const lookup = wm_command.find(command.substr(split+2)); lookup != std::end(wm_command))
                    {
                        commands[to_key(command.substr(0, split))] = lookup->second;
                    }
                }
            }
            else
            {
                mir::fatal_error("Invalid command option: %s", command.c_str());
            }
        }
    }
};

inline auto config_path(std::filesystem::path path)
{
    auto const basename = path.filename() += ".config";
    auto const miriway_shell = getenv("MIRIWAY_CONFIG_DIR");

    return miriway_shell ? std::filesystem::path(miriway_shell) / basename : basename;
}

// Migrate options from .config to .settings if not already present.
void migrate_config_to_settings(std::filesystem::path const& config_file_path,
                                 std::filesystem::path const& settings_file_path)
{
    static std::pair<std::string, std::string> const options[] = {
        {"shell-meta",                              "command_shell_meta"},
        {"shell-ctrl-alt",                          "command_shell_ctrl_alt"},
        {"shell-alt",                               "command_shell_alt"},
        {"meta",                                    "command_meta"},
        {"ctrl-alt",                                "command_ctrl_alt"},
        {"alt",                                     "command_alt"},
        {"shell-plain",                             "command_shell_plain"},
        {"plain",                                   "command_plain"},
        {"cursor-scale",                            "cursor_scale"},
        {"keymap",                                  "keymap"},
        {"key-repeat-rate",                         "keyboard_repeat_rate"},
        {"key-repeat-delay",                        "keyboard_repeat_delay"},
        {"mouse-handedness",                        "pointer_handedness"},
        {"mouse-cursor-acceleration",               "pointer_acceleration"},
        {"mouse-cursor-acceleration-bias",          "pointer_acceleration_bias"},
        {"mouse-scroll-speed",                      "pointer_vertical_scroll_speed"},
        {"mouse-horizontal-scroll-speed-override",  "pointer_horizontal_scroll_speed"},
        {"mouse-vertical-scroll-speed-override",    "pointer_vertical_scroll_speed"},
        {"touchpad-disable-while-typing",           "touchpad_disable_while_typing"},
        {"touchpad-disable-with-external-mouse",    "touchpad_disable_with_external_mouse"},
        {"touchpad-tap-to-click",                   "touchpad_tap_to_click"},
        {"touchpad-cursor-acceleration",            "touchpad_acceleration"},
        {"touchpad-cursor-acceleration-bias",       "touchpad_acceleration_bias"},
        {"touchpad-scroll-speed",                   "touchpad_vertical_scroll_speed"},
        {"touchpad-horizontal-scroll-speed-override","touchpad_horizontal_scroll_speed"},
        {"touchpad-vertical-scroll-speed-override", "touchpad_vertical_scroll_speed"},
        {"touchpad-scroll-mode",                    "touchpad_scroll_mode"},
        {"touchpad-click-mode",                     "touchpad_click_mode"},
        {"touchpad-middle-mouse-button-emulation",  "touchpad_middle_mouse_button_emulation"},
    };

    // Read the settings file into memory, collecting active keys as we go
    std::vector<std::string> settings_lines;
    std::set<std::string> existing_keys;
    if (std::ifstream settings{settings_file_path})
    {
        std::string line;
        while (std::getline(settings, line))
        {
            settings_lines.push_back(line);
            if (!line.empty() && line[0] != '#')
            {
                if (auto const eq = line.find('='); eq != std::string::npos)
                    existing_keys.insert(line.substr(0, eq));
            }
        }
    }

    // Collect values from .config for keys absent from .settings
    std::map<std::string, std::vector<std::string>> to_migrate;
    if (std::ifstream config{config_file_path})
    {
        std::string line;
        while (std::getline(config, line))
        {
            for (auto const& [config_name, settings_name] : options)
            {
                if (existing_keys.count(settings_name)) continue;
                if (line.rfind(config_name + "=", 0) == 0)
                {
                    auto value = line.substr(config_name.size() + 1);
                    // .config supports # comments; IniFile does not — strip any inline comment
                    if (auto const comment = value.find('#'); comment != std::string::npos)
                        value.erase(comment);
                    // trim trailing whitespace left after comment removal
                    if (auto const end = value.find_last_not_of(" \t"); end != std::string::npos)
                        value.erase(end + 1);
                    else
                        value.clear();
                    if (!value.empty())
                        to_migrate[settings_name].push_back(std::move(value));
                }
            }
        }
    }

    if (to_migrate.empty()) return;

    // For each entry to migrate, look for a commented-out placeholder "#<key>=" in the
    // settings lines and insert the value(s) immediately after it. Entries without a
    // placeholder are collected separately and appended at the end.
    std::map<std::string, std::vector<std::string>> to_append;
    for (auto const& [settings_name, values] : to_migrate)
    {
        std::string const placeholder = "#" + settings_name + "=";
        bool inserted = false;
        for (std::size_t i = 0; i < settings_lines.size(); ++i)
        {
            if (settings_lines[i].rfind(placeholder, 0) == 0)
            {
                for (std::size_t j = 0; j < values.size(); ++j)
                    settings_lines.insert(settings_lines.begin() + static_cast<std::ptrdiff_t>(i + 1 + j),
                                          settings_name + "=" + values[j]);
                inserted = true;
                break;
            }
        }
        if (!inserted)
            to_append[settings_name] = values;
    }

    if (!to_append.empty())
    {
        settings_lines.push_back("");
        settings_lines.push_back("# Migrated from " + config_file_path.filename().string());
        for (auto const& [settings_name, values] : to_append)
            for (auto const& value : values)
                settings_lines.push_back(settings_name + "=" + value);
    }

    if (std::ofstream settings{settings_file_path})
    {
        for (auto const& line : settings_lines)
            settings << line << "\n";
    }
}

auto const config_home = []() -> std::filesystem::path
{
    if (auto config_home = getenv("XDG_CONFIG_HOME"))
    {
        return config_home;
    }
    else if (auto home = getenv("HOME"))
    {
        return std::filesystem::path(home) / ".config";
    }

    return "/dev/null";
}();

auto find_config_file(std::string const& config_file_name) -> std::optional<std::filesystem::path>
{
    if (auto const miriway_config_dir = getenv("MIRIWAY_CONFIG_DIR"))
    {
        auto const path = std::filesystem::path{miriway_config_dir} / config_file_name;
        if (std::filesystem::exists(path)) return path;
    }

    auto const home_path = config_home / config_file_name;
    if (std::filesystem::exists(home_path)) return home_path;

    char const* xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
    std::string const config_dirs = xdg_config_dirs ? xdg_config_dirs : "/etc/xdg";

    std::stringstream ss(config_dirs);
    std::string dir;
    while (std::getline(ss, dir, ':'))
    {
        if (dir.empty()) continue;
        auto const path = std::filesystem::path{dir} / config_file_name;
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) return path;
    }

    return std::nullopt;
}

auto getenv_decorations()
{
    if (auto const strategy = getenv("MIRIWAY_DECORATIONS"))
    {
        if (strcmp(strategy, "always-ssd") == 0) return Decorations::always_ssd();
        if (strcmp(strategy, "prefer-ssd") == 0) return Decorations::prefer_ssd();
        if (strcmp(strategy, "always-csd") == 0) return Decorations::always_csd();
        if (strcmp(strategy, "prefer-csd") == 0) return Decorations::prefer_csd();

        mir::log_warning("Miriway [Warning]: Unknown decoration strategy: %s, using prefer-csd", strategy);
    }
    return Decorations::prefer_csd();
}
}

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv, config_path(argv[0]).c_str()};

    // Change the default Wayland extensions to:
    //  1. to enable screen capture; and,
    //  2. to allow pointer confinement (used by, for example, games)
    // Because we prioritise `user_preference()` these can be disabled by the configuration
    WaylandExtensions extensions;
    for (auto const& protocol : {
        WaylandExtensions::zwlr_screencopy_manager_v1,
        WaylandExtensions::zxdg_output_manager_v1,
        "zwp_idle_inhibit_manager_v1",
        "zwp_pointer_constraints_v1",
        "zwp_relative_pointer_manager_v1"})
    {
        extensions.conditionally_enable(protocol, [&](WaylandExtensions::EnableInfo const& info)
            {
                return info.user_preference().value_or(true);
            });
    }

    // Workaround for Firefox C&P failing if the (unrelated) wp-primary-selection isn't enabled
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1791417
    extensions.conditionally_enable("zwp_primary_selection_device_manager_v1",
        [&](WaylandExtensions::EnableInfo const& info)
        {
            if (std::ifstream cmdline{"/proc/" + std::to_string(miral::pid_of(info.app())) + "/cmdline"})
            {
                std::filesystem::path const path{std::istreambuf_iterator{cmdline}, std::istreambuf_iterator<char>{}};
                auto filename = path.filename().string();
                if (auto const eos = filename.find('\0'); eos != std::string::npos)
                {
                    filename.erase(eos);

                    return info.user_preference().value_or(filename == "firefox");
                }
            }
            return info.user_preference().value_or(false);
        });

    ChildControl child_control(runner);

    WaylandTools wltools;

    extensions.add_extension_disabled_by_default(build_ext_workspace_v1_global(wltools));
    child_control.enable_for_shell(extensions, ext_workspace_v1_name());

    // Protocols we're reserving for shell components_option
    for (auto const& protocol : {
        WaylandExtensions::zwlr_layer_shell_v1,
        WaylandExtensions::zwlr_foreign_toplevel_manager_v1})
    {
        child_control.enable_for_shell(extensions, protocol);
    }

    auto const enable_shell_extensions = [&](std::vector<std::string> const& protocols)
        { for (auto const& protocol : protocols) child_control.enable_for_shell(extensions, protocol); };

    ConfigurationOption shell_extension{
        enable_shell_extensions,
        "shell-add-wayland-extension",
        "Additional Wayland extension to allow shell processes (may be specified multiple times)"};

    ShellComponents shell_components{[&child_control](auto const& cmd) { child_control.launch_shell(cmd); }};
    runner.add_start_callback([&]{ shell_components.launch_all(); });
    ConfigurationOption components_option{
        [&](std::vector<std::string> const& apps) { shell_components.populate(apps); },
        "shell-component",
        "Shell component to launch on startup (may be specified multiple times)"};

    using miriway::Magnifier;   // We want our Magnifier not the miral Magnifier
    auto const settings_file = std::filesystem::path{runner.config_file()}.replace_extension("settings");

    // Migrate any shell command settings from .config to .settings before registering
    if (auto const config_file_path = find_config_file(runner.config_file()))
    {
        migrate_config_to_settings(config_file_path.value(), config_home / settings_file);
    }

    live_config::IniFile config_store;
    auto settings_store = std::make_unique<DocumentingStore>(config_store, config_home / settings_file);

    CursorScale cursor_scale{*settings_store};
    OutputFilter output_filter{*settings_store};

    Magnifier magnifier{*settings_store};
    InputConfiguration input_configuration{*settings_store};
    BounceKeys bounce_keys{*settings_store};
    SlowKeys slow_keys{*settings_store};
    StickyKeys sticky_keys{*settings_store};
    HoverClick hover_click{*settings_store};

    // Register shell command options with the settings store
    // corresponding configuration options. These processes are added to `shell_pids`
    CommandIndex shell_meta{
        *settings_store,
        "shell-meta",
        "meta <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex shell_ctrl_alt{
        *settings_store,
        "shell-ctrl-alt",
        "ctrl-alt <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex shell_alt{
        *settings_store,
        "shell-alt",
        "alt <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    // `meta`, `alt` and `ctrl_alt` provide a lookup to execute the commands configured by the corresponding
    // configuration options. These processes are NOT added to `shell_pids`
    CommandIndex meta{
        *settings_store,
        "meta",
        "meta <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    CommandIndex ctrl_alt{
        *settings_store,
        "ctrl-alt",
        "ctrl-alt <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    CommandIndex alt{
        *settings_store,
        "alt",
        "alt <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    CommandIndex shell_plain{
        *settings_store,
        "shell-plain",
        "unmodified <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex plain{
        *settings_store,
        "plain",
        "unmodified <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    Keymap keymap = getenv("MIRIWAY_SYSTEM_LOCALE1_KEYMAP") ? Keymap::system_locale1() : Keymap{*settings_store};
    settings_store.reset();

    ConfigFile config_file{
        runner,
        settings_file,
        ConfigFile::Mode::reload_on_change,
        [&config_store](auto&... args){ config_store.load_file(args...); }};

    // Process input events to identifies commands Miriway needs to handle
    ShellCommands commands{
        runner,
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_meta.try_command_for(c, s, cmd) || meta.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_ctrl_alt.try_command_for(c, s, cmd) || ctrl_alt.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_alt.try_command_for(c, s, cmd) || alt.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_plain.try_command_for(c, s, cmd) || plain.try_command_for(c, s, cmd); }};

#ifdef MIRIWAY_USE_APP_SWITCHER
    AppSwitcher app_switcher;
#endif

    std::atomic<bool> is_locked = false;
    LockScreen lockscreen(
        [&child_control, &is_locked](auto const& cmd) { child_control.launch_shell(cmd, [&is_locked]() { return is_locked.load(); });
        },
        [&extensions, &child_control](auto protocol) {
            child_control.enable_for_shell(extensions, protocol); });

    runner.add_start_callback([&child_control]
        {
            if (auto const miriway_session_startup = getenv("MIRIWAY_SESSION_STARTUP"))
            {
                child_control.run_app({miriway_session_startup});
            }
        });

    runner.add_stop_callback([&child_control]
        {
            if (auto const miriway_session_shutdown = getenv("MIRIWAY_SESSION_SHUTDOWN"))
            {
                child_control.run_app({miriway_session_shutdown});
            }
        });

    return runner.run_with(
        {
            X11Support{},
            pre_init(shell_extension),
            extensions,
            display_configuration_options,
            child_control,
            components_option,
            keymap,
            AppendEventFilter{[&](MirEvent const* e) {
                if (is_locked)
                    return false;
#ifdef MIRIWAY_USE_APP_SWITCHER
                if (commands.shell_keyboard_enabled() && app_switcher.process_event(e))
                    return true;
#endif
                return commands.input_event(e);
            }},
            SessionLockListener(
                [&] { is_locked = true; },
                [&] { is_locked = false; }),
            set_window_management_policy<WindowManagerPolicy>(commands),
            lockscreen,
            getenv_decorations(),
            CursorTheme{"default"},
            wltools,
            cursor_scale,
            output_filter,
            input_configuration,
            bounce_keys,
            slow_keys,
            sticky_keys,
            hover_click,
            magnifier,
#ifdef MIRIWAY_USE_APP_SWITCHER
            app_switcher,
#endif
        });
}
