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

#include <algorithm>

using namespace mir::geometry;
using namespace miral;

miriway::WindowManagerPolicy::WindowManagerPolicy(WindowManagerTools const& tools, ShellCommands& commands) :
    WorkspaceWMStrategy{tools},
    commands{&commands}
{
    commands.init_window_manager(this);
}

miral::WindowSpecification miriway::WindowManagerPolicy::place_new_window(
    miral::ApplicationInfo const& app_info, miral::WindowSpecification const& request_parameters)
{
    auto result = WorkspaceWMStrategy::place_new_window(app_info, request_parameters);

    result.userdata() = make_workspace_info();
    return result;
}

void miriway::WindowManagerPolicy::advise_new_window(const miral::WindowInfo &window_info)
{
    WorkspaceWMStrategy::advise_new_window(window_info);

    if (is_application(window_info.depth_layer()))
    {
        commands->advise_new_window_for(window_info.window().application());
    }
}

void miriway::WindowManagerPolicy::advise_delete_window(const miral::WindowInfo &window_info)
{
    WorkspaceWMStrategy::advise_delete_window(window_info);
    if (is_application(window_info.depth_layer()))
    {
        commands->advise_delete_window_for(window_info.window().application());
    }
}

void miriway::WindowManagerPolicy::dock_active_window_left(bool shift)
{
    tools.invoke_under_lock(
        [this, shift]
        {
            if (!shift)
            {
                dock_active_window_under_lock(
                    MirPlacementGravity(mir_placement_gravity_northwest | mir_placement_gravity_southwest));
            }
            else
            {
                move_active_window_to_next_output(mir_placement_gravity_west);
            }
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

                if (!eligible_to_dock(window_info.type(), window_info.depth_layer()))
                {
                    return;
                }

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

void miriway::WindowManagerPolicy::toggle_always_on_top()
{
    tools.invoke_under_lock(
        [this]
        {
            if (auto const w = tools.active_window())
            {
                auto const& info = tools.info_for(w);
                switch (info.depth_layer())
                {
                case mir_depth_layer_application:
                    {
                        WindowSpecification modifications;

                        tools.remove_tree_from_workspace(w, active_workspace());
                        modifications.depth_layer() = mir_depth_layer_always_on_top;
                        tools.modify_window(tools.info_for(w), modifications);
                        return;
                    }
                case mir_depth_layer_always_on_top:
                    {
                        WindowSpecification modifications;

                        modifications.depth_layer() = mir_depth_layer_application;
                        tools.modify_window(tools.info_for(w), modifications);
                        tools.add_tree_to_workspace(w, active_workspace());
                        return;
                    }
                default:;
                }
            }
        });
}

void miriway::WindowManagerPolicy::dock_active_window_right(bool shift)
{
    tools.invoke_under_lock(
        [this, shift]
        {
            if (!shift)
            {
                dock_active_window_under_lock(
                    MirPlacementGravity(mir_placement_gravity_northeast | mir_placement_gravity_southeast));
            }
            else
            {
                move_active_window_to_next_output(mir_placement_gravity_east);
            }
        });
}

void miriway::WindowManagerPolicy::move_active_window_to_next_output(MirPlacementGravity placement)
{
    if (application_zones.size() < 2)
        return;

    auto const active_window = tools.active_window();
    if (!active_window)
        return;

    auto const active_zone = tools.active_application_zone();

    // Next or previous zone depending on direction
    Zone const target_zone = [&]()
        {
            auto iter = std::find(application_zones.begin(), application_zones.end(), active_zone);

            if (placement & mir_placement_gravity_east)
            {
                return ++iter != application_zones.end() ? *iter : *application_zones.begin();
            }
            else
            {
                return iter != application_zones.begin() ? *--iter : *application_zones.rbegin();
            }
        }();

    auto const target_extents = target_zone.extents();
    auto top_left_offset = active_window.top_left() - active_zone.extents().top_left;
    if (top_left_offset.dx < DeltaX{} || top_left_offset.dx > as_delta(target_extents.size.width))
    {
        top_left_offset.dx = DeltaX{};
    }
    if (top_left_offset.dy < DeltaY{} || top_left_offset.dy > as_delta(target_extents.size.height))
    {
        top_left_offset.dy = DeltaY{};
    }
    WindowSpecification modifications;
    modifications.top_left() = target_extents.top_left + top_left_offset;
    modifications.output_id() = -1;

    auto& window_info = tools.info_for(active_window);
    tools.place_and_size_for_state(modifications, window_info);
    tools.modify_window(window_info, modifications);
}

void miriway::WindowManagerPolicy::advise_application_zone_create(miral::Zone const& application_zone)
{
    application_zones.push_back(application_zone);
    WorkspaceWMStrategy::advise_application_zone_create(application_zone);
}

void miriway::WindowManagerPolicy::advise_application_zone_update(miral::Zone const& updated, miral::Zone const& original)
{
    for (auto& zone : application_zones)
    {
        if (zone.is_same_zone(original))
        {
            zone = updated;
            break;
        }
    }
    WorkspaceWMStrategy::advise_application_zone_update(updated, original);
}

void miriway::WindowManagerPolicy::advise_application_zone_delete(miral::Zone const& application_zone)
{
    application_zones.erase(
        std::remove_if(application_zones.begin(), application_zones.end(),
            [&application_zone](Zone const& z){ return z.is_same_zone(application_zone); }),
        application_zones.end());
    WorkspaceWMStrategy::advise_application_zone_delete(application_zone);
}

bool miriway::WindowManagerPolicy::handle_pointer_event(const MirPointerEvent* event)
{
    bool result = Super::handle_pointer_event(event);

    if (moving_window)
    {
        if (result)
        {
            window_moved = true;
        }

        if (!result)
        {
            moving_window = false;

            if (window_moved)
            {
                if (auto active_window = tools.active_window())
                {
                    Rectangle const window_rect{active_window.top_left(), active_window.size()};
                    auto const active_rect = tools.active_output();
                    auto const overlap = intersection_of(active_rect, window_rect);
                    Point const cursor{
                        mir_pointer_event_axis_value(event, mir_pointer_axis_x),
                        mir_pointer_event_axis_value(event, mir_pointer_axis_y)};

                    if (overlap.size.height < window_rect.size.height)
                    {
                        Rectangle const top_margin{active_rect.top_left, {active_rect.size.width, active_rect.size.height/20}};

                        // If part of the active window is offscreen at top, and cursor near top, maximize it!
                        if (overlap.top() == active_rect.top() && top_margin.contains(cursor))
                        {
                            auto& window_info = tools.info_for(active_window);
                            WindowSpecification modifications;

                            modifications.state() = mir_window_state_maximized;

                            tools.place_and_size_for_state(modifications, window_info);
                            tools.modify_window(window_info, modifications);
                            return result;
                        }
                    }

                    if (overlap.size.width < window_rect.size.width)
                    {
                        Rectangle const left_margin{active_rect.top_left, {active_rect.size.width/20, active_rect.size.height}};
                        Rectangle const right_margin{active_rect.top_left + as_delta(left_margin.size.width*19), left_margin.size};

                        // If part of the active window is offscreen at offscreen horizontally, and cursor near left, dock it!
                        if (overlap.left() == active_rect.left() && left_margin.contains(cursor))
                        {
                            dock_active_window_under_lock(
                                MirPlacementGravity(mir_placement_gravity_northwest | mir_placement_gravity_southwest));
                            return result;
                        }
                        else if (right_margin.contains(cursor))
                        {
                            dock_active_window_under_lock(
                                MirPlacementGravity(mir_placement_gravity_northeast | mir_placement_gravity_southeast));
                            return result;
                        }
                    }
                }
            }
        }
    }

    return result;
}

void miriway::WindowManagerPolicy::handle_request_move(WindowInfo& window_info, const MirInputEvent* input_event)
{
    moving_window = true;
    window_moved = false;
    Super::handle_request_move(window_info, input_event);
}

bool miriway::WindowManagerPolicy::handle_keyboard_event(MirKeyboardEvent const* event)
{
    return commands->shell_keyboard_enabled() && WorkspaceWMStrategy::handle_keyboard_event(event);
}

void miriway::WindowManagerPolicy::dock_active_window_under_lock(MirPlacementGravity placement)
{
    if (auto active_window = tools.active_window())
    {
        auto const active_rect = tools.active_application_zone().extents();
        auto& window_info = tools.info_for(active_window);

        if (!eligible_to_dock(window_info.type(), window_info.depth_layer()))
        {
            return;
        }

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

bool miriway::WindowManagerPolicy::eligible_to_dock(MirWindowType window_type, MirDepthLayer layer)
{
    switch (window_type)
    {
    // Only normal (and freestyle) windows are eligible to dock.
    case mir_window_type_freestyle:
    case mir_window_type_normal:
        return layer == mir_depth_layer_application;

    default:
        return false;
    }
}
