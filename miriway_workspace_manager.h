/*
 * Copyright © 2023 Octopull Ltd.
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

#ifndef MIRIWAY_MIRIWAY_WORKSPACE_MANAGER_H
#define MIRIWAY_MIRIWAY_WORKSPACE_MANAGER_H

#include <miral/window_manager_tools.h>

#include <list>
#include <map>

namespace miriway
{
using miral::Window;
using miral::WindowManagerTools;
using miral::Workspace;

class WorkspaceManager
{
public:
    explicit WorkspaceManager(WindowManagerTools const& tools);
    virtual ~WorkspaceManager() = default;

    void workspace_begin(bool take_active);

    void workspace_end(bool take_active);

    void workspace_up(bool take_active);

    void workspace_down(bool take_active);

    void apply_workspace_hidden_to(Window const& window);

    void apply_workspace_visible_to(Window const& window);

    void change_active_workspace(std::shared_ptr<Workspace> const& ww,
                                 std::shared_ptr<Workspace> const& old_active,
                                 miral::Window const& window);


    void advise_new_window(const miral::WindowInfo &window_info);

    void advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                    std::vector<Window> const& windows);

    auto active_workspace() const -> std::shared_ptr<Workspace>;

    bool in_hidden_workspace(miral::WindowInfo const& info) const;

    static bool is_application(MirDepthLayer layer);

    struct WorkspaceInfo;

    static auto make_workspace_info() -> std::shared_ptr<WorkspaceInfo>;

    // This implementation assumes the userdata contains WorkspaceInfo
    // but, in case there's another option, made virtual
    virtual auto workspace_info_for(miral::WindowInfo const& info) const ->  WorkspaceInfo&
    {
        return *std::static_pointer_cast<WorkspaceInfo>(info.userdata());
    }

private:
    WindowManagerTools tools_;

    using workspace_list = std::list<std::shared_ptr<Workspace>>;

    workspace_list workspaces;
    workspace_list::iterator active_workspace_;
    std::map<std::shared_ptr<miral::Workspace>, miral::Window> workspace_to_active;

    void erase_if_empty(workspace_list::iterator const& old_workspace);
};

} // miriway

#endif //MIRIWAY_MIRIWAY_WORKSPACE_MANAGER_H
