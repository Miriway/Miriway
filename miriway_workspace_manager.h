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

#ifndef MIRIWAY_WORKSPACE_MANAGER_H_
#define MIRIWAY_WORKSPACE_MANAGER_H_

#include <miral/window_manager_tools.h>

#include <list>
#include <map>

namespace miriway
{
using miral::Window;
using miral::WindowInfo;
using miral::WindowManagerTools;
using miral::WindowSpecification;
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

    void activate_workspace_containing(Window const& window);

    void advise_new_window(const WindowInfo &window_info);

    void advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                    std::vector<Window> const& windows);

    auto active_workspace() const -> std::shared_ptr<Workspace>;

    bool in_hidden_workspace(WindowInfo const& info) const;

    static bool is_application(MirDepthLayer layer);

    struct WorkspaceInfo;

    static auto make_workspace_info() -> std::shared_ptr<WorkspaceInfo>;

    // This implementation assumes the userdata contains WorkspaceInfo
    // but, in case there's another option, made virtual
    virtual auto workspace_info_for(WindowInfo const& info) const ->  WorkspaceInfo&
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

// Template class to hook WorkspaceManager into a window management strategy
template<typename WMStrategy>
class WorkspaceWMStrategy : public WMStrategy, protected WorkspaceManager
{
protected:
    explicit WorkspaceWMStrategy(WindowManagerTools const& tools) :
        WMStrategy{tools},
        WorkspaceManager{tools}
    {}

    void advise_new_window(const WindowInfo &window_info) override
    {
        WMStrategy::advise_new_window(window_info);
        WorkspaceManager::advise_new_window(window_info);
    }

    void advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                    std::vector<Window> const& windows) override
    {
        WMStrategy::advise_adding_to_workspace(workspace, windows);
        WorkspaceManager::advise_adding_to_workspace(workspace, windows);
    }

    void handle_modify_window(WindowInfo& window_info, WindowSpecification const& modifications) override
    {
        auto mods = modifications;

        if (in_hidden_workspace(window_info))
        {
            // Don't allow state changes in hidden workspaces
            if (mods.state().is_set()) mods.state().consume();

            // Don't allow size changes in hidden workspaces
            if (mods.size().is_set()) mods.size().consume();
        }

        WMStrategy::handle_modify_window(window_info, mods);
    }

    void handle_raise_window(WindowInfo& window_info) override
    {
        if (in_hidden_workspace(window_info))
        {
            activate_workspace_containing(window_info.window());
        }
        WMStrategy::handle_raise_window(window_info);
    }
};
} // miriway

#endif //MIRIWAY_WORKSPACE_MANAGER_H_
