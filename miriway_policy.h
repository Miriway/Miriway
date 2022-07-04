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

#include <miral/minimal_window_manager.h>

#include <map>
#include <vector>

namespace miriway
{
using namespace miral;
class ShellCommands;

class WindowManagerPolicy :
    public MinimalWindowManager
{
public:
    WindowManagerPolicy(
        WindowManagerTools const& tools,
        ShellCommands& commands,
        int const& no_of_workspaces);

    void dock_active_window_left();
    void dock_active_window_right();
    void toggle_maximized_restored();
    void workspace_up(bool take_active);
    void workspace_down(bool take_active);
    void focus_next_application();
    void focus_prev_application();
    void focus_next_within_application();
    void focus_prev_within_application();
    auto active_output() -> mir::geometry::Rectangle const;

private:
    auto place_new_window(ApplicationInfo const& app_info, WindowSpecification const& request_parameters)
    -> WindowSpecification override;

    void advise_new_window(const WindowInfo &window_info) override;

    void advise_delete_app(ApplicationInfo const& application) override;

    void advise_delete_window(const WindowInfo &window_info) override;

    void handle_modify_window(WindowInfo& window_info, WindowSpecification const& modifications) override;

    void advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                    std::vector<Window> const& windows) override;

    bool handle_keyboard_event(MirKeyboardEvent const* event) override;

    void apply_workspace_hidden_to(Window const& window);
    void apply_workspace_visible_to(Window const& window);
    void change_active_workspace(std::shared_ptr<Workspace> const& ww,
                                 std::shared_ptr<Workspace> const& old_active,
                                 miral::Window const& window);

    bool external_wallpaper = false;
    ShellCommands* const commands;

    using ring_buffer = std::vector<std::shared_ptr<Workspace>>;
    ring_buffer workspaces;
    ring_buffer::iterator active_workspace;

private:
    std::map<std::shared_ptr<miral::Workspace>, miral::Window> workspace_to_active;
};
}

#endif //MIRIWAY_POLICY_H_
