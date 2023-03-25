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
#include <map>
#include <mutex>

using namespace miral::toolkit;
namespace miral { class MirRunner; class ExternalClientLauncher; }
namespace miriway
{
using namespace miral;

class WindowManagerPolicy;

// Process `input_event()` to identify commands Miriway needs to handle.
// Commands will be routed to MirRunner, the WindowManagerPolicy, meta_command or ctrl_alt_command as appropriate
class ShellCommands
{
public:
    using CommandFunctor = std::function<bool(xkb_keysym_t key_code, bool with_shift, WindowManagerPolicy* wm)>;

    ShellCommands(
        MirRunner& runner, CommandFunctor meta_command, CommandFunctor ctrl_alt_command);

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
    CommandFunctor meta_command;
    CommandFunctor ctrl_alt_command;
    WindowManagerPolicy* wm = nullptr;
    std::atomic<bool> shell_commands_active = true;

    std::mutex mutex;
    int app_windows = 0;
};

struct WmCommandIndex
{
    using WmFunctor = std::function<void(WindowManagerPolicy* wm, bool with_shift)>;

    static auto dock_active_window_left() -> WmFunctor;
    static auto dock_active_window_right() -> WmFunctor;
    static auto toggle_maximized_restored() -> WmFunctor;
    static auto workspace_begin() -> WmFunctor;
    static auto workspace_end() -> WmFunctor;
    static auto workspace_up() -> WmFunctor;
    static auto workspace_down() -> WmFunctor;

    WmCommandIndex();

    void map_key_to(xkb_keysym_t key_code, WmFunctor);

    bool try_command_for(xkb_keysym_t key_code, bool with_shift, WindowManagerPolicy* wm) const;
private:
    std::map<xkb_keysym_t, WmFunctor> commands;
};

}

#endif //MIRIWAY_COMMANDS_H
