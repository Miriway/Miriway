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

#include <mir/abnormal_exit.h>
#include <mir/log.h>
#include <miral/append_event_filter.h>
#include <miral/configuration_option.h>
#include <miral/display_configuration_option.h>
#include <miral/external_client.h>
#include <miral/keymap.h>
#include <miral/idle_listener.h>
#include <miral/prepend_event_filter.h>
#include <miral/runner.h>
#include <miral/session_lock_listener.h>
#include <miral/set_window_management_policy.h>
#include <miral/version.h>
#include <miral/wayland_extensions.h>
#include <miral/x11_support.h>

#include <sys/wait.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sys/timerfd.h>
#include <ctime>
#include <numeric>

using namespace miral;
using namespace miriway;

namespace
{

class ChildControl
{
public:
    explicit ChildControl(MirRunner& runner);

    void operator()(mir::Server& server);

    void launch_shell(std::vector<std::string> const& cmd);
    void launch_shell(std::vector<std::string> const& cmd, std::function<bool()> const should_restart_predicate);
    void run_shell(std::vector<std::string> const& cmd);
    void run_app(std::vector<std::string> const& cmd);
    void enable_for_shell(WaylandExtensions& extensions, std::string const& protocol);

private:
    class Self;
    std::shared_ptr<Self> self;
};

class ChildControl::Self
{
public:

    explicit Self(MirRunner& runner) :
        runner{runner},
        shell_pids{runner}
    {
        runner.add_stop_callback([this]{ shell_pids.shutdown(); });
    }

    void enable_for_shell(WaylandExtensions& extensions, std::string const& protocol)
    {
        extensions.conditionally_enable(protocol, enable_for_shell_pids);
    }

    struct ShellComponentRunInfo
    {
        ShellComponentRunInfo() : ShellComponentRunInfo{[]() { return true; }} {}
        explicit ShellComponentRunInfo(std::function<bool()> const should_restart_predicate)
            : should_restart_predicate{std::move(should_restart_predicate)} {}
        time_t last_run_time = 0;
        int runs_in_quick_succession = 0;
        std::unique_ptr<miral::FdHandle> handle = nullptr;
        std::function<bool()> const should_restart_predicate;
    };

    // Keep track of interesting "shell" child processes and call the corresponding
    // `on_reap` if they fail. Note that it will "reap" all child processes not just
    // the ones of interest. (But that's useful as it avoids "zombie" child processes.)
    struct ShellPids
    {
        ShellPids(MirRunner& runner)
        {
            runner.add_start_callback([&] { runner.register_signal_handler({SIGCHLD}, [this](int) { reap(); }); });
        }

        using OnReap = std::function<void()>;

        void insert(pid_t pid, OnReap on_reap = [](){})
        {
            std::lock_guard lock{shell_component_mutex};
            shell_component_pids.insert(std::pair(pid, on_reap));
        };

        bool is_found(pid_t pid) const
        {
            std::lock_guard lock{shell_component_mutex};
            return shell_component_pids.find(pid) != end(shell_component_pids);
        }

        void shutdown()
        {
            std::lock_guard lock{shell_component_mutex};
            for (auto [pid, _] : shell_component_pids)
            {
                kill(pid, SIGTERM);
            }
            shell_component_pids.clear();
        }
    private:
        std::mutex mutable shell_component_mutex;
        std::map<pid_t, OnReap> shell_component_pids;

        void reap()
        {
            int status = 0;
            while (true)
            {
                auto const pid = waitpid(-1, &status, WNOHANG);
                if (pid > 0)
                {
                    OnReap on_reap = [](){};

                    {
                        std::lock_guard lock{shell_component_mutex};

                        if (auto it = shell_component_pids.find(pid); it != shell_component_pids.end())
                        {
                            if ((WIFEXITED(status) && WEXITSTATUS(status))
                                || (WIFSIGNALED(status) && WTERMSIG(status)))
                            {
                                on_reap = it->second;
                            }
                            shell_component_pids.erase(pid);
                        }
                    }
                    on_reap();
                }
                else
                {
                    break;
                }
            }
        }
    };

    MirRunner& runner;

    // To support docks, onscreen keyboards, launchers and the like; enable a number of protocol extensions,
    // but, because they have security implications only for those applications found in `shell_pids`.
    // We'll use `shell_pids` to track "shell-*" processes.
    // We also check `user_preference()` so these can be enabled by the configuration
    ShellPids shell_pids;
    ExternalClientLauncher client_launcher;

    std::function<bool(WaylandExtensions::EnableInfo const& info)> const enable_for_shell_pids =
        [this](WaylandExtensions::EnableInfo const& info)
    {
        pid_t const pid = pid_of(info.app());
        return shell_pids.is_found(pid) || info.user_preference().value_or(false);
    };

    // A configuration option to start applications when compositor starts and record them in `shell_pids`.
    // Because of the previous section, this allows them some extra Wayland extensions
    std::function<void(std::vector<std::string> const&, std::shared_ptr<ShellComponentRunInfo>)> shell_launch
        = [&](std::vector<std::string> const& cmd, std::shared_ptr<ShellComponentRunInfo> info)
        {
            static int constexpr min_time_seconds_allowed_between_runs = 3;
            static int constexpr time_seconds_wait_foreach_run = 3;
            static int const max_too_short_reruns_in_a_row = 3;
            static auto const get_cmd_string = [](std::vector<std::string> const& cmd)
            {
                return std::accumulate(std::begin(cmd), std::end(cmd), std::string(),
                    [](std::string const& current, std::string const& next)
                    {
                       return current.empty() ? next : current + " " + next;
                    });
            };

            time_t const now = std::time(nullptr);
            auto const time_elapsed_since_last_run = now - info->last_run_time;
            if (time_elapsed_since_last_run <= min_time_seconds_allowed_between_runs)
            {
                // The command is trying to restart too soon, so we throttle it. For every premature
                // death that it has in a row, we increasingly throttle its restart time.
                // After max_too_short_reruns_in_a_row is hit, we stop trying to rerun
                if (info->runs_in_quick_succession >= max_too_short_reruns_in_a_row)
                {
                    mir::log_warning("No longer restarting app: %s", get_cmd_string(cmd).c_str());
                    return;
                }

                auto const time_to_wait = time_seconds_wait_foreach_run * (info->runs_in_quick_succession + 1);
                auto const timer_fd = timerfd_create(CLOCK_REALTIME, 0);
                if (timer_fd == -1)
                {
                    mir::log_error("timerfd_create failed, unable to restart application: %s", get_cmd_string(cmd).c_str());
                    return;
                }

                auto const spec = itimerspec
                {
                    { 0, 0 },               // Timer interval
                    { time_to_wait, 0 } // Initial expiration
                };

                if (timerfd_settime(timer_fd, 0, &spec, NULL) == -1)
                {
                    mir::log_error("timerfd_settime failed, unable to restart application: %s", get_cmd_string(cmd).c_str());
                    return;
                }

                info->handle = runner.register_fd_handler(mir::Fd{timer_fd}, [info, this, &cmd](int){
                    // While waiting for the timer, the predicate could have a different value
                    if (!info->should_restart_predicate())
                        return;

                    info->handle.reset();
                    info->runs_in_quick_succession++;
                    info->last_run_time = std::time(nullptr);
                    shell_pids.insert(client_launcher.launch(cmd), [this, info, &cmd] {
                        if (info->should_restart_predicate())
                            shell_launch(cmd, info);
                    });
                });
                return;
            }
            else
            {
                info->runs_in_quick_succession = 0;
                info->last_run_time = now;
                shell_pids.insert(client_launcher.launch(cmd), [this, info, &cmd] {
                    if (info->should_restart_predicate())
                        shell_launch(cmd, info);
                });
            }
        };
};

ChildControl::ChildControl(MirRunner& runner) :
    self{std::make_shared<Self>(runner)}
{
}

void ChildControl::operator()(mir::Server& server)
{
    self->client_launcher(server);
}

void ChildControl::launch_shell(std::vector<std::string> const& cmd)
{
    self->shell_launch(cmd, std::make_shared<Self::ShellComponentRunInfo>());
}

void ChildControl::launch_shell(std::vector<std::string> const& cmd, std::function<bool()> const should_restart_predicate)
{
    self->shell_launch(cmd, std::make_shared<Self::ShellComponentRunInfo>(should_restart_predicate));
}
void ChildControl::run_shell(std::vector<std::string> const& cmd)
{
    self->shell_pids.insert(self->client_launcher.launch(cmd));
}
void ChildControl::run_app(std::vector<std::string> const& cmd)
{
    self->client_launcher.launch(cmd);
}

void ChildControl::enable_for_shell(WaylandExtensions& extensions, std::string const& protocol)
{
    extensions.conditionally_enable(protocol, self->enable_for_shell_pids);
}

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

    // Workaround for Firefox C&P failing if the (unrelated) wp-primary-selection isn't enabled
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1791417
    extensions.conditionally_enable("zwp_primary_selection_device_manager_v1",
        [&](WaylandExtensions::EnableInfo const& info)
        {
            if (std::ifstream cmdline{"/proc/" + std::to_string(miral::pid_of(info.app())) + "/cmdline"})
            {
                std::filesystem::path const path{std::istreambuf_iterator{cmdline}, std::istreambuf_iterator<char>{}};
                auto filename = path.filename().string();
                filename.erase(filename.find('\0'));

                return info.user_preference().value_or(filename == "firefox");
            }
            return info.user_preference().value_or(false);
        });

    ChildControl child_control(runner);


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
        "meta <key>:<command> shortcut with shell priviledges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex shell_ctrl_alt{
        "shell-ctrl-alt",
        "ctrl-alt <key>:<command> shortcut with shell priviledges (may be specified multiple times)",
        [&child_control](auto cmd) { child_control.run_shell(cmd); }};

    CommandIndex shell_alt{
        "shell-alt",
        "alt <key>:<command> shortcut with shell priviledges (may be specified multiple times)",
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
            Keymap{},
            AppendEventFilter{[&](MirEvent const* e) {
                if (is_locked)
                    return false;

                return commands.input_event(e);
            }},
            SessionLockListener(
                [&] { is_locked = true; },
                [&] { is_locked = false; }),
            set_window_management_policy<WindowManagerPolicy>(commands),
            lockscreen
        });
}
