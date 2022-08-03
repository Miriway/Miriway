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
#include <miral/command_line_option.h>
#include <miral/display_configuration_option.h>
#include <miral/external_client.h>
#include <miral/internal_client.h>
#include <miral/keymap.h>
#include <miral/runner.h>
#include <miral/set_window_management_policy.h>
#include <miral/version.h>
#include <miral/wayland_extensions.h>
#include <miral/x11_support.h>
#include <mir/log.h>

using namespace miral;
using namespace miriway;

namespace
{
// Split out the tokens of a (possibly escaped) command
std::vector<std::string> unescape(std::string const& command)
{
    std::vector<std::string> split_command;

    {
        std::string token;
        char in_quote = '\0';
        bool escaping = false;

        auto push_token = [&]()
            {
                if (!token.empty())
                {
                    split_command.push_back(std::move(token));
                    token.clear();
                }
            };

        for (auto c : command)
        {
            if (escaping)
            {
                // end escape
                escaping = false;
                token += c;
                continue;
            }

            switch (c)
            {
            case '\\':
                // start escape
                escaping = true;
                continue;

            case '\'':
            case '\"':
                if (in_quote == '\0')
                {
                    // start quoted sequence
                    in_quote = c;
                    continue;
                }
                else if (c == in_quote)
                {
                    // end quoted sequence
                    in_quote = '\0';
                    continue;
                }
                else
                {
                    break;
                }

            default:
                break;
            }

            if (!isspace(c) || in_quote)
            {
                token += c;
            }
            else
            {
                push_token();
            }
        }

        push_token();
    }

    return split_command;
}

// Split out the tokens of a (possibly escaped) command
auto parse_cmd(std::string const& command, std::vector<std::string>& target)
{
    return unescape(command).swap(target);
}

struct Commands
{
    explicit Commands(ExternalClientLauncher& launcher) : launcher{launcher}{}

    void populate_commands(std::vector<std::string> const& config_cmds)
    {
        for (auto const& command : config_cmds)
        {
            if (command.size() < 3 || command[1] != ':')
            {
                mir::fatal_error("Invalid command option: %s", command.c_str());
            }

            commands[std::tolower(command[0])] = unescape(command.substr(2));
        }
    }

    bool launch_command(char c)
    {
        auto const i = commands.find(std::tolower(c));
        bool const found = i != end(commands);
        if (found) launcher.launch(i->second);
        return found;
    }
private:
    ExternalClientLauncher& launcher;
    std::map<char, std::vector<std::string>> commands;
};
}

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv};


    WaylandExtensions extensions;

    for (auto const& protocol : {
        WaylandExtensions::zwlr_screencopy_manager_v1,
        WaylandExtensions::zxdg_output_manager_v1,
        "zwp_pointer_constraints_v1",
        "zwp_relative_pointer_manager_v1"})
    {
        extensions.conditionally_enable(protocol, [&](WaylandExtensions::EnableInfo const& info)
            {
                return info.user_preference().value_or(true);
            });
    }

    std::set<pid_t> shell_component_pids;
    std::atomic<pid_t> launcher_pid{-1};

    // Protocols we're reserving for shell components_option
    for (auto const& protocol : {
        WaylandExtensions::zwlr_layer_shell_v1,
        WaylandExtensions::zxdg_output_manager_v1,
        WaylandExtensions::zwlr_foreign_toplevel_manager_v1,
        WaylandExtensions::zwp_virtual_keyboard_manager_v1,
        WaylandExtensions::zwp_input_method_manager_v2})
    {
        extensions.conditionally_enable(protocol, [&](WaylandExtensions::EnableInfo const& info)
            {
                pid_t const pid = pid_of(info.app());
                return  launcher_pid == pid ||
                        shell_component_pids.find(pid) != end(shell_component_pids) ||
                        info.user_preference().value_or(false);
            });
    }

    std::string const miriway_path{argv[0]};
    std::string const miriway_root = miriway_path.substr(0, miriway_path.size() - 6);

    std::vector<std::string> launcher_cmd;

    ExternalClientLauncher external_client_launcher;

    Commands ctrl_alt{external_client_launcher};

    ShellCommands commands{
        runner,
        [&](){ if (!launcher_cmd.empty()) launcher_pid = external_client_launcher.launch(launcher_cmd); },
        [&] (auto c) { return ctrl_alt.launch_command(c); }
    };

    CommandLineOption components_option{
        [&](std::vector<std::string> const& apps)
        {
            for (auto const& app : apps)
            {
                shell_component_pids.insert(external_client_launcher.launch(unescape(app)));
            }
        },
        "shell-component",
        "Shell component to launch_command on startup (may be specified multiple times; multiple components_option are started)"};

    CommandLineOption const launcher_option{
        [&launcher_cmd](mir::optional_value<std::string> const& new_cmd)
        {
            if (new_cmd.is_set()) parse_cmd(new_cmd.value(), launcher_cmd);
        },
        "shell-meta-a", "app launch_command"};

    return runner.run_with(
        {
            X11Support{},
            extensions,
            display_configuration_options,
            external_client_launcher,
            components_option,
            launcher_option,
            CommandLineOption{[&](std::vector<std::string> const& cmds) { ctrl_alt.populate_commands(cmds); },
                              "ctrl-alt", "ctrl-alt <key>:<command> shortcut (may be specified multiple times)"},
            Keymap{},
            AppendEventFilter{[&](MirEvent const* e) { return commands.input_event(e); }},
            set_window_management_policy<WindowManagerPolicy>(commands)
        });
}
