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

#include "miriway_workspace_manager.h"

using namespace mir::geometry;
using namespace miral;

miriway::WorkspaceManager::WorkspaceManager(WindowManagerTools const& tools) :
    tools_{tools}
{
    workspaces.push_back(tools_.create_workspace());

    active_workspace_ = workspaces.begin();
}

void miriway::WorkspaceManager::workspace_begin(bool take_active)
{
    tools_.invoke_under_lock(
        [this, take_active]
            {
            if (active_workspace_ != workspaces.begin())
            {
                auto const old_workspace = active_workspace_;
                auto const& window = take_active ? tools_.active_window() : Window{};
                auto const& old_active = *active_workspace_;
                auto const& new_active = *(active_workspace_ = workspaces.begin());
                change_active_workspace(new_active, old_active, window);
                erase_if_empty(old_workspace);
            }
        });
}

void miriway::WorkspaceManager::workspace_end(bool take_active)
{
    tools_.invoke_under_lock(
        [this, take_active]
            {
            auto const old_workspace = active_workspace_;
            auto const& window = take_active ? tools_.active_window() : Window{};
            auto const& old_active = *active_workspace_;
            workspaces.push_back(tools_.create_workspace());
            active_workspace_ = --workspaces.end();
            auto const& new_active = *active_workspace_;
            change_active_workspace(new_active, old_active, window);
            erase_if_empty(old_workspace);
        });
}

void miriway::WorkspaceManager::workspace_up(bool take_active)
{
    tools_.invoke_under_lock(
        [this, take_active]
        {
            if (active_workspace_ != workspaces.begin())
            {
                auto const old_workspace = active_workspace_;
                auto const& window = take_active ? tools_.active_window() : Window{};
                auto const& old_active = *active_workspace_;
                auto const& new_active = *--active_workspace_;
                change_active_workspace(new_active, old_active, window);
                erase_if_empty(old_workspace);
            }
        });
}

void miriway::WorkspaceManager::workspace_down(bool take_active)
{
    tools_.invoke_under_lock(
        [this, take_active]
        {
            auto const old_workspace = active_workspace_;
            auto const& window = take_active ? tools_.active_window() : Window{};
            auto const& old_active = *active_workspace_;
            if (++active_workspace_ == workspaces.end())
            {
                workspaces.push_back(tools_.create_workspace());
                active_workspace_ = --workspaces.end();
            }
            auto const& new_active = *active_workspace_;
            change_active_workspace(new_active, old_active, window);
            erase_if_empty(old_workspace);
        });
}

void miriway::WorkspaceManager::erase_if_empty(workspace_list::iterator const& old_workspace)
{
    bool empty = true;
    tools_.for_each_window_in_workspace(*old_workspace, [&](auto ww)
        {
            if (is_application(tools_.info_for(ww).depth_layer()))
                empty = false;
        });
    if (empty)
    {
        workspace_to_active.erase(*old_workspace);
        workspaces.erase(old_workspace);
    }
}

void miriway::WorkspaceManager::apply_workspace_hidden_to(Window const& window)
{
    auto const& window_info = tools_.info_for(window);
    auto& workspace_info = workspace_info_for(window_info);
    if (!workspace_info.in_hidden_workspace)
    {
        workspace_info.in_hidden_workspace = true;
        workspace_info.old_state = window_info.state();

        WindowSpecification modifications;
        modifications.state() = mir_window_state_hidden;
        tools_.place_and_size_for_state(modifications, window_info);
        tools_.modify_window(window_info.window(), modifications);
    }
}

void miriway::WorkspaceManager::apply_workspace_visible_to(Window const& window)
{
    auto const& window_info = tools_.info_for(window);
    auto& workspace_info = workspace_info_for(window_info);
    if (workspace_info.in_hidden_workspace)
    {
        workspace_info.in_hidden_workspace = false;
        WindowSpecification modifications;
        modifications.state() = workspace_info.old_state;
        tools_.place_and_size_for_state(modifications, window_info);
        tools_.modify_window(window_info.window(), modifications);
    }
}

void miriway::WorkspaceManager::change_active_workspace(
    std::shared_ptr<Workspace> const& new_active,
    std::shared_ptr<Workspace> const& old_active,
    Window const& window)
{
    if (new_active == old_active) return;

    auto const old_active_window = tools_.active_window();
    auto const old_active_window_shell = old_active_window &&
        !is_application(tools_.info_for(old_active_window).depth_layer());

    if (!old_active_window || old_active_window_shell)
    {
        // If there's no active window, the first shown grabs focus: get the right one
        if (auto const ww = workspace_to_active[new_active])
        {
            tools_.for_each_workspace_containing(ww, [&](std::shared_ptr<Workspace> const& ws)
            {
                if (ws == new_active)
                {
                    apply_workspace_visible_to(ww);
                }
            });

            // If focus was on a shell window, put it on an app
            if (old_active_window_shell)
                tools_.select_active_window(ww);
        }
    }

    tools_.remove_tree_from_workspace(window, old_active);
    tools_.add_tree_to_workspace(window, new_active);

    tools_.for_each_window_in_workspace(new_active, [&](Window const& ww)
    {
        if (is_application(tools_.info_for(ww).depth_layer()))
        {
            apply_workspace_visible_to(ww);
        }
    });

    bool hide_old_active = false;
    tools_.for_each_window_in_workspace(old_active, [&](Window const& ww)
    {
        if (is_application(tools_.info_for(ww).depth_layer()))
        {
            if (ww == old_active_window)
            {
                // If we hide the active window focus will shift: do that last
                hide_old_active = true;
                return;
            }

            apply_workspace_hidden_to(ww);
            return;
        }
    });

    if (hide_old_active)
    {
        apply_workspace_hidden_to(old_active_window);

        // Remember the old active_window when we switch away
        workspace_to_active[old_active] = old_active_window;
    }
}

void miriway::WorkspaceManager::advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                                           std::vector<Window> const& windows)
{
    if (windows.empty())
        return;

    for (auto const& window : windows)
    {
        if (workspace == *active_workspace_)
        {
            apply_workspace_visible_to(window);
        }
        else
        {
            apply_workspace_hidden_to(window);
        }
    }
}

auto miriway::WorkspaceManager::active_workspace() const -> std::shared_ptr<Workspace>
{
    return *active_workspace_;
}

bool miriway::WorkspaceManager::is_application(MirDepthLayer layer)
{
    switch (layer)
    {
    case mir_depth_layer_application:
    case mir_depth_layer_always_on_top:
        return true;

    default:;
        return false;
    }
}
