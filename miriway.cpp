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

#include "miriway_policy.h"
#include "miriway_commands.h"

#include <miral/append_event_filter.h>
#include <miral/configuration_option.h>
#include <miral/display_configuration_option.h>
#include <miral/external_client.h>
#include <miral/internal_client.h>
#include <miral/keymap.h>
#include <miral/prepend_event_filter.h>
#include <miral/runner.h>
#include <miral/set_window_management_policy.h>
#include <miral/wayland_extensions.h>
#include <miral/version.h>
#include <miral/x11_support.h>
#include <mir/log.h>
#include <mir/abnormal_exit.h>

#include <sys/wait.h>

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

// Keep track of interesting "shell" child processes. Somewhat hacky as `reap()` needs to be called
// somewhere, and will reap all child processes not just the ones of interest. (But that's actually
// useful as it avoids "zombie" child processes).
struct ShellPids
{
    void insert(pid_t pid)
    {
        std::lock_guard lock{shell_component_mutex};
        shell_component_pids.insert(pid);
    };

    void reap()
    {
        int status;
        while (true)
        {
            auto const pid = waitpid(-1, &status, WNOHANG);
            if (pid > 0)
            {
                std::lock_guard lock{shell_component_mutex};
                shell_component_pids.erase(pid);
            }
            else
            {
                break;
            }
        }
    }

    bool is_found(pid_t pid) const
    {
        std::lock_guard lock{shell_component_mutex};
        return shell_component_pids.find(pid) != end(shell_component_pids);
    }
private:
    std::mutex mutable shell_component_mutex;
    std::set<pid_t> shell_component_pids;
};
}

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv};

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

    // To support docks, onscreen keyboards, launchers and the like; enable a number of protocol extensions,
    // but, because they have security implications only for those applications found in `shell_pids`.
    // We'll use `shell_pids` to track "shell-*" processes.
    // We also check `user_preference()` so these can be enabled by the configuration
    ShellPids shell_pids;
    // Protocols we're reserving for shell components_option
    for (auto const& protocol : {
        WaylandExtensions::zwlr_layer_shell_v1,
        WaylandExtensions::zwlr_foreign_toplevel_manager_v1,
        WaylandExtensions::zwp_virtual_keyboard_manager_v1,
        WaylandExtensions::zwlr_virtual_pointer_manager_v1,
#if MIRAL_VERSION >= MIR_VERSION_NUMBER(3, 9, 0)
        WaylandExtensions::ext_session_lock_manager_v1,
#endif
        WaylandExtensions::zwp_input_method_manager_v2})
    {
        extensions.conditionally_enable(protocol, [&](WaylandExtensions::EnableInfo const& info)
            {
                pid_t const pid = pid_of(info.app());
                return shell_pids.is_found(pid) || info.user_preference().value_or(false);
            });
    }

    ExternalClientLauncher client_launcher;

    // A configuration option to start applications when compositor starts and record them in `shell_pids`.
    // Because of the previous section, this allows them some extra Wayland extensions
    ShellComponents shell_components{[&](auto cmd){ shell_pids.insert(client_launcher.launch(cmd)); }};
    runner.add_start_callback([&]{ shell_components.launch_all(); });
    ConfigurationOption components_option{
        [&](std::vector<std::string> const& apps) { shell_components.populate(apps); },
        "shell-component",
        "Shell component to launch on startup (may be specified multiple times)"};

    // `shell_meta`, `shell_ctrl_alt` and `shell_alt` provide a lookup to execute the commands configured by the
    // corresponding configuration options. These processes are added to `shell_pids`
    CommandIndex shell_meta{
        "shell-meta",
        "meta <key>:<command> shortcut with shell priviledges (may be specified multiple times)",
        [&](auto cmd) { shell_pids.insert(client_launcher.launch(cmd)); }};

    CommandIndex shell_ctrl_alt{
        "shell-ctrl-alt",
        "ctrl-alt <key>:<command> shortcut with shell priviledges (may be specified multiple times)",
        [&](auto cmd) { shell_pids.insert(client_launcher.launch(cmd)); }};

    CommandIndex shell_alt{
        "shell-alt",
        "alt <key>:<command> shortcut with shell priviledges (may be specified multiple times)",
        [&](auto cmd) { shell_pids.insert(client_launcher.launch(cmd)); }};

    // `meta`, `alt` and `ctrl_alt` provide a lookup to execute the commands configured by the corresponding
    // configuration options. These processes are NOT added to `shell_pids`
    CommandIndex meta{
        "meta",
        "meta <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { client_launcher.launch(cmd); }};

    CommandIndex ctrl_alt{
        "ctrl-alt",
        "ctrl-alt <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { client_launcher.launch(cmd); }};

    CommandIndex alt{
        "alt",
        "alt <key>:<command> shortcut (may be specified multiple times)",
        [&](auto cmd) { client_launcher.launch(cmd); }};

    // Process input events to identifies commands Miriway needs to handle
    ShellCommands commands{
        runner,
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_meta.try_command_for(c, s, cmd) || meta.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_ctrl_alt.try_command_for(c, s, cmd) || ctrl_alt.try_command_for(c, s, cmd); },
        [&] (xkb_keysym_t c, bool s, ShellCommands* cmd) { return shell_alt.try_command_for(c, s, cmd) || alt.try_command_for(c, s, cmd); }};

    return runner.run_with(
        {
            X11Support{},
            extensions,
            display_configuration_options,
            client_launcher,
            components_option,
            shell_ctrl_alt,
            shell_alt,
            shell_meta,
            ctrl_alt,
            meta,
            alt,
            Keymap{},
            PrependEventFilter{[&](MirEvent const*) { shell_pids.reap(); return false; }},
            AppendEventFilter{[&](MirEvent const* e) { return commands.input_event(e); }},
            set_window_management_policy<WindowManagerPolicy>(commands)
        });
}
