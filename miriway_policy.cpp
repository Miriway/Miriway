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

#include <miral/application_info.h>
#include <miral/window_info.h>
#include <miral/window_manager_tools.h>
#include <miral/zone.h>

using namespace mir::geometry;
using namespace miral;

namespace
{
struct WorkspaceInfo
{
    bool in_hidden_workspace{false};

    MirWindowState old_state = mir_window_state_unknown;
};

inline WorkspaceInfo& workspace_info_for(WindowInfo const& info)
{
    return *std::static_pointer_cast<WorkspaceInfo>(info.userdata());
}

inline bool is_application(MirDepthLayer layer)
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
}

miriway::WindowManagerPolicy::WindowManagerPolicy(WindowManagerTools const& tools, ShellCommands& commands) :
    MinimalWindowManager{tools},
    commands{&commands}
{
    commands.init_window_manager(this);

    workspaces.push_back(this->tools.create_workspace());

    active_workspace = workspaces.begin();
}

miral::WindowSpecification miriway::WindowManagerPolicy::place_new_window(
    miral::ApplicationInfo const& app_info, miral::WindowSpecification const& request_parameters)
{
    auto result = MinimalWindowManager::place_new_window(app_info, request_parameters);

    result.userdata() = std::make_shared<WorkspaceInfo>();
    return result;
}

void miriway::WindowManagerPolicy::advise_new_window(const miral::WindowInfo &window_info)
{
    MinimalWindowManager::advise_new_window(window_info);

    if (is_application(window_info.depth_layer()))
    {
        commands->advise_new_window_for(window_info.window().application());
    }

    if (auto const& parent = window_info.parent())
    {
        if (workspace_info_for(tools.info_for(parent)).in_hidden_workspace)
            apply_workspace_hidden_to(window_info.window());
    }
    else
    {
        tools.add_tree_to_workspace(window_info.window(), *active_workspace);
    }
}

void miriway::WindowManagerPolicy::advise_delete_window(const miral::WindowInfo &window_info)
{
    MinimalWindowManager::advise_delete_window(window_info);
    if (is_application(window_info.depth_layer()))
    {
        commands->advise_delete_window_for(window_info.window().application());
    }
}

void miriway::WindowManagerPolicy::advise_delete_app(miral::ApplicationInfo const& application)
{
    MinimalWindowManager::advise_delete_app(application);
}

void miriway::WindowManagerPolicy::workspace_begin(bool take_active)
{
    tools.invoke_under_lock(
        [this, take_active]
            {
            if (active_workspace != workspaces.begin())
            {
                auto const old_workspace = active_workspace;
                auto const& window = take_active ? tools.active_window() : Window{};
                auto const& old_active = *active_workspace;
                auto const& new_active = *(active_workspace = workspaces.begin());
                change_active_workspace(new_active, old_active, window);
                erase_if_empty(old_workspace);
            }
        });
}

void miriway::WindowManagerPolicy::workspace_end(bool take_active)
{
    tools.invoke_under_lock(
        [this, take_active]
            {
            auto const old_workspace = active_workspace;
            auto const& window = take_active ? tools.active_window() : Window{};
            auto const& old_active = *active_workspace;
            workspaces.push_back(tools.create_workspace());
            active_workspace = --workspaces.end();
            auto const& new_active = *active_workspace;
            change_active_workspace(new_active, old_active, window);
            erase_if_empty(old_workspace);
        });
}

void miriway::WindowManagerPolicy::workspace_up(bool take_active)
{
    tools.invoke_under_lock(
        [this, take_active]
        {
            if (active_workspace != workspaces.begin())
            {
                auto const old_workspace = active_workspace;
                auto const& window = take_active ? tools.active_window() : Window{};
                auto const& old_active = *active_workspace;
                auto const& new_active = *--active_workspace;
                change_active_workspace(new_active, old_active, window);
                erase_if_empty(old_workspace);
            }
        });
}

void miriway::WindowManagerPolicy::workspace_down(bool take_active)
{
    tools.invoke_under_lock(
        [this, take_active]
        {
            auto const old_workspace = active_workspace;
            auto const& window = take_active ? tools.active_window() : Window{};
            auto const& old_active = *active_workspace;
            if (++active_workspace == workspaces.end())
            {
                workspaces.push_back(tools.create_workspace());
                active_workspace = --workspaces.end();
            }
            auto const& new_active = *active_workspace;
            change_active_workspace(new_active, old_active, window);
            erase_if_empty(old_workspace);
        });
}

void miriway::WindowManagerPolicy::erase_if_empty(workspace_list::iterator const& old_workspace)
{
    bool empty = true;
    tools.for_each_window_in_workspace(*old_workspace, [&](auto ww)
        {
        if (is_application(tools.info_for(ww).depth_layer()))
            empty = false;
        });
    if (empty)
    {
        workspace_to_active.erase(*old_workspace);
        workspaces.erase(old_workspace);
    }
}

void miriway::WindowManagerPolicy::dock_active_window_left()
{
    tools.invoke_under_lock(
        [this]
        {
            dock_active_window_under_lock(
                MirPlacementGravity(mir_placement_gravity_northwest | mir_placement_gravity_southwest));
        });
}


void miriway::WindowManagerPolicy::toggle_maximized_restored()
{
    tools.invoke_under_lock(
        [this]
        {
            if (auto active_window = tools.active_window())
            {
                auto& window_info = tools.info_for(active_window);
                WindowSpecification modifications;

                modifications.state() = (window_info.state() != mir_window_state_restored) ?
                                        mir_window_state_restored : mir_window_state_maximized;

                if (window_info.state() != mir_window_state_restored)
                {
                    modifications.state() = mir_window_state_restored;
                    modifications.size() = window_info.restore_rect().size;
                    modifications.top_left() = window_info.restore_rect().top_left;
                }
                else
                {
                    modifications.state() = mir_window_state_maximized;
                }

                tools.place_and_size_for_state(modifications, window_info);
                tools.modify_window(window_info, modifications);
            }
        });
}

void miriway::WindowManagerPolicy::dock_active_window_right()
{
    tools.invoke_under_lock(
        [this]
        {
            dock_active_window_under_lock(
                MirPlacementGravity(mir_placement_gravity_northeast | mir_placement_gravity_southeast));
        });
}

void miriway::WindowManagerPolicy::apply_workspace_hidden_to(Window const& window)
{
    auto const& window_info = tools.info_for(window);
    auto& workspace_info = workspace_info_for(window_info);
    if (!workspace_info.in_hidden_workspace)
    {
        workspace_info.in_hidden_workspace = true;
        workspace_info.old_state = window_info.state();

        WindowSpecification modifications;
        modifications.state() = mir_window_state_hidden;
        tools.place_and_size_for_state(modifications, window_info);
        tools.modify_window(window_info.window(), modifications);
    }
}

void miriway::WindowManagerPolicy::apply_workspace_visible_to(Window const& window)
{
    auto const& window_info = tools.info_for(window);
    auto& workspace_info = workspace_info_for(window_info);
    if (workspace_info.in_hidden_workspace)
    {
        workspace_info.in_hidden_workspace = false;
        WindowSpecification modifications;
        modifications.state() = workspace_info.old_state;
        tools.place_and_size_for_state(modifications, window_info);
        tools.modify_window(window_info.window(), modifications);
    }
}

void miriway::WindowManagerPolicy::handle_modify_window(WindowInfo& window_info, WindowSpecification const& modifications)
{
    auto mods = modifications;

    auto& workspace_info = workspace_info_for(window_info);

    if (workspace_info.in_hidden_workspace)
    {
        // Don't allow state changes in hidden workspaces
        if (mods.state().is_set()) mods.state().consume();

        // Don't allow size changes in hidden workspaces
        if (mods.size().is_set()) mods.size().consume();
    }

    MinimalWindowManager::handle_modify_window(window_info, mods);
}

void miriway::WindowManagerPolicy::change_active_workspace(
    std::shared_ptr<Workspace> const& new_active,
    std::shared_ptr<Workspace> const& old_active,
    Window const& window)
{
    if (new_active == old_active) return;

    auto const old_active_window = tools.active_window();
    auto const old_active_window_shell = old_active_window &&
        !is_application(tools.info_for(old_active_window).depth_layer());

    if (!old_active_window || old_active_window_shell)
    {
        // If there's no active window, the first shown grabs focus: get the right one
        if (auto const ww = workspace_to_active[new_active])
        {
            tools.for_each_workspace_containing(ww, [&](std::shared_ptr<Workspace> const& ws)
            {
                if (ws == new_active)
                {
                    apply_workspace_visible_to(ww);
                }
            });

            // If focus was on a shell window, put it on an app
            if (old_active_window_shell)
                tools.select_active_window(ww);
        }
    }

    tools.remove_tree_from_workspace(window, old_active);
    tools.add_tree_to_workspace(window, new_active);

    tools.for_each_window_in_workspace(new_active, [&](Window const& ww)
    {
        if (is_application(tools.info_for(ww).depth_layer()))
        {
            apply_workspace_visible_to(ww);
        }
    });

    bool hide_old_active = false;
    tools.for_each_window_in_workspace(old_active, [&](Window const& ww)
    {
        if (is_application(tools.info_for(ww).depth_layer()))
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

void miriway::WindowManagerPolicy::advise_adding_to_workspace(std::shared_ptr<Workspace> const& workspace,
                                                            std::vector<Window> const& windows)
{
    if (windows.empty())
        return;

    for (auto const& window : windows)
    {
        if (workspace == *active_workspace)
        {
            apply_workspace_visible_to(window);
        }
        else
        {
            apply_workspace_hidden_to(window);
        }
    }
}

bool miriway::WindowManagerPolicy::handle_keyboard_event(MirKeyboardEvent const* event)
{
    return commands->shell_keyboard_enabled() && MinimalWindowManager::handle_keyboard_event(event);
}

void miriway::WindowManagerPolicy::dock_active_window_under_lock(MirPlacementGravity placement)
{
    if (auto active_window = tools.active_window())
    {
        auto const active_rect = tools.active_application_zone().extents();
        auto& window_info = tools.info_for(active_window);
        WindowSpecification modifications;

        modifications.state() = mir_window_state_attached;
        modifications.attached_edges() = placement;
        modifications.size() = active_window.size();

        if (window_info.state() != mir_window_state_attached ||
            window_info.attached_edges() != placement)
        {
            modifications.size().value().width = active_rect.size.width / 2;
        }
        else
        {
            if (modifications.size().value().width == active_rect.size.width / 2)
            {
                modifications.size().value().width = active_rect.size.width / 3;
            }
            else if (modifications.size().value().width < active_rect.size.width / 2)
            {
                modifications.size().value().width = 2 * active_rect.size.width / 3;
            }
            else
            {
                modifications.size().value().width = active_rect.size.width / 2;
            }
        }

        tools.place_and_size_for_state(modifications, window_info);
        tools.modify_window(window_info, modifications);
    }
}
