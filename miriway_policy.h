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

#ifndef MIRIWAY_POLICY_H_
#define MIRIWAY_POLICY_H_

#include "miriway_workspace_manager.h"

#include <miral/minimal_window_manager.h>

namespace miriway
{
using namespace miral;
class ShellCommands;

// A window management policy that adds support for docking and workspaces.
// Co-ordinates with `ShellCommands` for the handling of related commands.
class WindowManagerPolicy : public WorkspaceWMStrategy<MinimalWindowManager>
{
public:
    WindowManagerPolicy(WindowManagerTools const& tools, ShellCommands& commands);

    using WorkspaceWMStrategy::workspace_begin;
    using WorkspaceWMStrategy::workspace_end;
    using WorkspaceWMStrategy::workspace_up;
    using WorkspaceWMStrategy::workspace_down;
    void dock_active_window_left();
    void dock_active_window_right();
    bool handle_pointer_event(const MirPointerEvent* event) override;
    void handle_request_move(WindowInfo& window_info, const MirInputEvent* input_event) override;
    void toggle_maximized_restored();
    void toggle_always_on_top();

private:
    auto place_new_window(ApplicationInfo const& app_info, WindowSpecification const& request_parameters)
    -> WindowSpecification override;

    void advise_new_window(const WindowInfo &window_info) override;

    void advise_delete_window(const WindowInfo &window_info) override;

    bool handle_keyboard_event(MirKeyboardEvent const* event) override;

    void dock_active_window_under_lock(MirPlacementGravity placement);
    static bool eligible_to_dock(MirWindowType window_type);

    ShellCommands* const commands;

    // moving_window and window_moved are a huristic to deduce whether a window has been moved by user
    bool moving_window = false;  // A move request has been made
    bool window_moved = false;   // Pointer events have been processed following a move request
};
}

#endif //MIRIWAY_POLICY_H_
