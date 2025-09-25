/*
 * Copyright © 2024 Octopull Ltd.
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

#include <miral/external_client.h>
#include <miral/runner.h>
#include <miral/wayland_extensions.h>

#include <sys/wait.h>
#include <sys/timerfd.h>

#include <algorithm>
#include <ctime>
#include <format>
#include <iostream>
#include <map>
#include <numeric>

class miriway::ChildControl::Self
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
                    std::cerr <<
                        std::format("Miriway [Warning]: No longer restarting app: '{}'", get_cmd_string(cmd)) <<
                        std::endl;
                    return;
                }

                auto const time_to_wait = time_seconds_wait_foreach_run * (info->runs_in_quick_succession + 1);
                auto const timer_fd = timerfd_create(CLOCK_REALTIME, 0);
                if (timer_fd == -1)
                {
                    std::cerr <<
                        std::format("Miriway [ERROR]: timerfd_create failed, unable to restart app: '{}'", get_cmd_string(cmd)) <<
                        std::endl;
                    return;
                }

                auto const spec = itimerspec
                {
                    { 0, 0 },               // Timer interval
                    { time_to_wait, 0 } // Initial expiration
                };

                if (timerfd_settime(timer_fd, 0, &spec, NULL) == -1)
                {
                    std::cerr <<
                        std::format("Miriway [ERROR]: timerfd_settime failed, unable to restart app: '{}'", get_cmd_string(cmd)) <<
                        std::endl;
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

miriway::ChildControl::ChildControl(MirRunner& runner) :
    self{std::make_shared<Self>(runner)}
{
}

void miriway::ChildControl::operator()(mir::Server& server)
{
    self->client_launcher(server);
}

void miriway::ChildControl::launch_shell(std::vector<std::string> const& cmd)
{
    self->shell_launch(cmd, std::make_shared<Self::ShellComponentRunInfo>());
}

void miriway::ChildControl::launch_shell(std::vector<std::string> const& cmd, std::function<bool()> const should_restart_predicate)
{
    self->shell_launch(cmd, std::make_shared<Self::ShellComponentRunInfo>(should_restart_predicate));
}
void miriway::ChildControl::run_shell(std::vector<std::string> const& cmd)
{
    self->shell_pids.insert(self->client_launcher.launch(cmd));
}
void miriway::ChildControl::run_app(std::vector<std::string> const& cmd)
{
    self->client_launcher.launch(cmd);
}

void miriway::ChildControl::enable_for_shell(WaylandExtensions& extensions, std::string const& protocol)
{
    extensions.conditionally_enable(protocol, self->enable_for_shell_pids);
}

