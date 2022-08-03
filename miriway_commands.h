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

#ifndef MIRIWAY_COMMANDS_H
#define MIRIWAY_COMMANDS_H

#include <miral/application.h>
#include <miral/toolkit_event.h>

#include <atomic>
#include <functional>
#include <set>
#include <string>
#include <mutex>

using namespace miral::toolkit;
namespace miral { class MirRunner; class ExternalClientLauncher; }
namespace miriway
{
using namespace miral;

class WindowManagerPolicy;

class ShellCommands
{
public:
    ShellCommands(MirRunner& runner, std::function<void()> start_launcher, std::function<bool(char)> ctrl_alt_command);

    void init_window_manager(WindowManagerPolicy* wm);

    void advise_new_window_for(Application const& app);
    void advise_delete_window_for(Application const& app);

    auto input_event(MirEvent const* event) -> bool;
    [[nodiscard]] auto shell_keyboard_enabled() const -> bool
        { return shell_commands_active; }

private:
    auto keyboard_shortcuts(MirKeyboardEvent const* kev) -> bool;
    auto touch_shortcuts(MirTouchEvent const* tev) -> bool;

    MirRunner& runner;
    std::function<void()> start_launcher;
    std::function<bool(char)> ctrl_alt_command;
    WindowManagerPolicy* wm = nullptr;
    std::atomic<bool> shell_commands_active = true;

    std::mutex mutex;
    int app_windows = 0;
};
}

#endif //MIRIWAY_COMMANDS_H
