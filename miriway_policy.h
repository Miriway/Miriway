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
class WindowManagerPolicy :
    public MinimalWindowManager, public WorkspaceManager
{
public:
    WindowManagerPolicy(WindowManagerTools const& tools, ShellCommands& commands);

    void dock_active_window_left();
    void dock_active_window_right();
    void toggle_maximized_restored();

private:
    auto place_new_window(ApplicationInfo const& app_info, WindowSpecification const& request_parameters)
    -> WindowSpecification override;

    void advise_new_window(const WindowInfo &window_info) override;

    void advise_delete_window(const WindowInfo &window_info) override;

    void handle_modify_window(WindowInfo& window_info, WindowSpecification const& modifications) override;

    void advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                    std::vector<Window> const& windows) override;

    bool handle_keyboard_event(MirKeyboardEvent const* event) override;

    void dock_active_window_under_lock(MirPlacementGravity placement);

    ShellCommands* const commands;
};
}

#endif //MIRIWAY_POLICY_H_
