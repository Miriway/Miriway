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

miriway::WindowManagerPolicy::WindowManagerPolicy(WindowManagerTools const& tools, ShellCommands& commands) :
    MinimalWindowManager{tools},
    WorkspaceManager{tools},
    commands{&commands}
{
    commands.init_window_manager(this);
}

miral::WindowSpecification miriway::WindowManagerPolicy::place_new_window(
    miral::ApplicationInfo const& app_info, miral::WindowSpecification const& request_parameters)
{
    auto result = MinimalWindowManager::place_new_window(app_info, request_parameters);

    result.userdata() = make_workspace_info();
    return result;
}

void miriway::WindowManagerPolicy::advise_new_window(const miral::WindowInfo &window_info)
{
    MinimalWindowManager::advise_new_window(window_info);

    if (is_application(window_info.depth_layer()))
    {
        commands->advise_new_window_for(window_info.window().application());
    }

    WorkspaceManager::advise_new_window(window_info);
}

void miriway::WindowManagerPolicy::advise_delete_window(const miral::WindowInfo &window_info)
{
    MinimalWindowManager::advise_delete_window(window_info);
    if (is_application(window_info.depth_layer()))
    {
        commands->advise_delete_window_for(window_info.window().application());
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

void miriway::WindowManagerPolicy::handle_modify_window(WindowInfo& window_info, WindowSpecification const& modifications)
{
    auto mods = modifications;

    if (WorkspaceManager::in_hidden_workspace(window_info))
    {
        // Don't allow state changes in hidden workspaces
        if (mods.state().is_set()) mods.state().consume();

        // Don't allow size changes in hidden workspaces
        if (mods.size().is_set()) mods.size().consume();
    }

    MinimalWindowManager::handle_modify_window(window_info, mods);
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

void miriway::WindowManagerPolicy::advise_adding_to_workspace(
    std::shared_ptr<Workspace> const& workspace,
    std::vector<Window> const& windows)
{
    WindowManagementPolicy::advise_adding_to_workspace(workspace, windows);
    WorkspaceManager::advise_adding_to_workspace(workspace, windows);
}
