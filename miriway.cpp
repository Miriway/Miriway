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

#include "miriway_child_control.h"
#include "miriway_commands.h"
#include "miriway_documenting_store.h"
#include "miriway_policy.h"
#include "miriway_ext_workspace_v1.h"

#include <mir/abnormal_exit.h>
#include <mir/log.h>
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

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>

#if MIRAL_VERSION < MIR_VERSION_NUMBER(5, 6, 0)
#define MIRAL_HAS_BROKEN_INPUT_CONFIGURATION
#endif

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
class CommandIndex : ConfigurationOption
{
public:
    CommandIndex(
        std::string const& option_,
        std::string const& description,
        std::function<void(std::vector<std::string> const&)> launch) :
        ConfigurationOption{[this](std::vector<std::string> const& cmds) { populate(cmds); }, option_, description},
        launch{std::move(launch)}
    {
    }

    bool try_command_for(xkb_keysym_t key_code, bool with_shift, ShellCommands* cmd) const
    {
        auto const i = commands.find(std::tolower(key_code));
        bool const found = i != end(commands);
        if (found) i->second(cmd, with_shift);
        return found;
    }

    using ConfigurationOption::operator();
private:
    std::function<void (std::vector<std::string> const& command_line)> const launch;
    std::map<xkb_keysym_t, ShellCommands::CmdFunctor> commands;

    void populate(std::vector<std::string> const& config_cmds)
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

auto getenv_decorations()
{
    if (auto const strategy = getenv("MIRIWAY_DECORATIONS"))
    {
        if (strcmp(strategy, "always-ssd") == 0) return Decorations::always_ssd();
        if (strcmp(strategy, "prefer-ssd") == 0) return Decorations::prefer_ssd();
        if (strcmp(strategy, "always-csd") == 0) return Decorations::always_csd();
        if (strcmp(strategy, "prefer-csd") == 0) return Decorations::prefer_csd();

        mir::log_warning("Unknown decoration strategy: '%s', using prefer-csd", strategy);
    }
    return Decorations::prefer_csd();
}

#ifdef MIRAL_HAS_BROKEN_INPUT_CONFIGURATION
class FixedInputConfiguration
{
public:
    FixedInputConfiguration(live_config::Store& store);

    void operator()(mir::Server& server);

private:
    struct Self
    {
        Self(live_config::Store& store) : ic{store} {}

        InputConfiguration::Keyboard keyboard;
        InputConfiguration::Mouse mouse;
        InputConfiguration::Touchpad touchpad;

        InputConfiguration ic;

        void done()
        {
            ic.keyboard(keyboard); keyboard = { };
            ic.mouse(mouse); mouse = { };
            ic.touchpad(touchpad); touchpad = { };
        }

        void acceleration(live_config::Key const& key, std::optional<std::string_view> opt_val)
        {
            if (opt_val.has_value())
            {
                auto val = *opt_val;

                if (val == "none")
                {
                    mouse.acceleration(mir_pointer_acceleration_none);
                }
                else if (val == "adaptive")
                {
                    mouse.acceleration(mir_pointer_acceleration_adaptive);
                }
                else
                {
                    mir::log_warning(
                        "Config key '%s' has invalid string value: %s",
                        key.to_string().c_str(),
                        std::format("{}",val).c_str());
                }
            }
        };

        void acceleration_bias(live_config::Key const&, std::optional<float> opt_val)
        {
            if (opt_val.has_value())
            {
                mouse.acceleration_bias(std::clamp(*opt_val, -1.0f, 1.0f));
            }
        };

        void scroll_mode(live_config::Key const& key, std::optional<std::string_view> opt_val)
        {
            if (opt_val.has_value())
            {
                auto val = *opt_val;

                if (val == "none")
                {
                    touchpad.scroll_mode(mir_touchpad_scroll_mode_none);
                }
                else if (val == "edge")
                {
                    touchpad.scroll_mode(mir_touchpad_scroll_mode_edge_scroll);
                }
                else if (val == "two-finger")
                {
                    touchpad.scroll_mode(mir_touchpad_scroll_mode_two_finger_scroll);
                }
                else if (val == "button-down")
                {
                    touchpad.scroll_mode(mir_touchpad_scroll_mode_button_down_scroll);
                }
                else
                {
                    mir::log_warning(
                        "Config key '%s' has invalid string value: %s",
                        key.to_string().c_str(),
                        std::format("{}",val).c_str());
                }
            }
        };

        void tap_to_click(live_config::Key const&, std::optional<bool> opt_val)
        {
            if (opt_val.has_value())
            {
                touchpad.tap_to_click(*opt_val);
            }
        }
    };
    std::shared_ptr<Self> self;
};

void FixedInputConfiguration::operator()(mir::Server& server)
{
    self->ic(server);
}

FixedInputConfiguration::FixedInputConfiguration(live_config::Store& store) :
    self{std::make_shared<Self>(store)}
{
    store.on_done([self=self]() { self->done(); });

    store.add_string_attribute(
        live_config::Key{"pointer", "acceleration"},
        "Acceleration profile for mice and trackballs [none, adaptive]",
        [self=self](auto... args) { self->acceleration(args...); });

    store.add_float_attribute(
        live_config::Key{"pointer", "acceleration_bias"},
        "Acceleration speed of mice within a range of [-1.0, 1.0]",
        [self=self](auto... args) { self->acceleration_bias(args...); });

    store.add_string_attribute(
        live_config::Key{"touchpad", "scroll_mode"},
        "Scroll mode for touchpads: [edge, two-finger, button-down]",
        [self=self](auto... args) { self->scroll_mode(args...); });

    store.add_bool_attribute(
        live_config::Key{"touchpad", "tap_to_click"},
        "1, 2, 3 finger tap mapping to left, right, middle click, respectively [true, false]",
        [self=self](auto... args) { self->tap_to_click(args...); });
}
#endif
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

    ConfigurationOption shell_extension{
        [&](std::vector<std::string> const& protocols)
        { for (auto const& protocol : protocols) child_control.enable_for_shell(extensions, protocol); },
    "shell-add-wayland-extension",
    "Additional Wayland extension to allow shell processes (may be specified multiple times)"};

    ShellComponents shell_components{[&child_control](auto const& cmd) { child_control.launch_shell(cmd); }};
    runner.add_start_callback([&]{ shell_components.launch_all(); });
    ConfigurationOption components_option{
        [&](std::vector<std::string> const& apps) { shell_components.populate(apps); },
        "shell-component",
        "Shell component to launch on startup (may be specified multiple times)"};

    // `shell_meta`, `shell_ctrl_alt` and `shell_alt` provide a lookup to execute the commands configured by the
    // corresponding configuration options. These processes are added to `shell_pids`
    CommandIndex shell_meta{
        "shell-meta",
        "meta <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex shell_ctrl_alt{
        "shell-ctrl-alt",
        "ctrl-alt <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex shell_alt{
        "shell-alt",
        "alt <key>:<command> shortcut with shell privileges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    // `meta`, `alt` and `ctrl_alt` provide a lookup to execute the commands configured by the corresponding
    // configuration options. These processes are NOT added to `shell_pids`
    CommandIndex meta{
        "meta",
        "meta <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    CommandIndex ctrl_alt{
        "ctrl-alt",
        "ctrl-alt <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    CommandIndex alt{
        "alt",
        "alt <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { child_control.run_app(cmd); }};

    // Process input events to identifies commands Miriway needs to handle
    ShellCommands commands{
        runner,
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_meta.try_command_for(c, s, cmd) || meta.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_ctrl_alt.try_command_for(c, s, cmd) || ctrl_alt.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_alt.try_command_for(c, s, cmd) || alt.try_command_for(c, s, cmd); }};

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

#ifdef MIRAL_HAS_BROKEN_INPUT_CONFIGURATION
    using InputConfiguration = ::FixedInputConfiguration;
#endif
    auto const settings_file = std::filesystem::path{runner.config_file()}.replace_extension("settings");

    live_config::IniFile config_store;
    DocumentingStore settings_store{config_store, config_home / settings_file};

    CursorScale cursor_scale{settings_store};
    OutputFilter output_filter{settings_store};

    InputConfiguration input_configuration{settings_store};
    BounceKeys bounce_keys{settings_store};
    SlowKeys slow_keys{settings_store};
    StickyKeys sticky_keys{settings_store};
    HoverClick hover_click{settings_store};

    Keymap keymap = getenv("MIRIWAY_SYSTEM_LOCALE1_KEYMAP") ? Keymap::system_locale1() : Keymap{settings_store};

    ConfigFile config_file{
        runner,
        settings_file,
        ConfigFile::Mode::reload_on_change,
        [&config_store](auto&... args){ config_store.load_file(args...); }};

    return runner.run_with(
        {
            X11Support{},
            pre_init(shell_extension),
            extensions,
            display_configuration_options,
            child_control,
            components_option,
            shell_ctrl_alt,
            shell_alt,
            shell_meta,
            ctrl_alt,
            meta,
            alt,
            keymap,
            AppendEventFilter{[&](MirEvent const* e) {
                if (is_locked)
                    return false;

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
        });
}
