/*
* Copyright © 2025 Octopull Ltd.
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

#include "miriway_ext_workspace_v1.h"

#include <miral/output.h>
#include <miral/wayland_tools.h>

namespace
{
std::set<miriway::ExtWorkspaceManagerV1::Global*> all_the_globals;
std::mutex all_the_outputs_mutex;
std::set<miral::Output, decltype([](auto const& l, auto const& r){ return l.id() < r.id(); })> all_the_outputs;
}

miriway::ExtWorkspaceManagerV1::ExtWorkspaceManagerV1(wl_resource* new_ext_workspace_manager_v1) :
    mir::wayland::ExtWorkspaceManagerV1{new_ext_workspace_manager_v1, Version<1>{}},
    the_workspace_group{new ExtWorkspaceGroupHandleV1{*this}}
{
    send_workspace_group_event(the_workspace_group->resource);
    send_done_event();
    the_workspace_group->send_capabilities_event(0);
}

void miriway::ExtWorkspaceManagerV1::commit()
{
}

void miriway::ExtWorkspaceManagerV1::stop()
{
}

void miriway::ExtWorkspaceManagerV1::output_added(miral::WaylandTools* wltools, miral::Output const& output)
{
    wltools->for_each_binding(client, output, [this](wl_resource* the_output)
    {
        the_workspace_group->send_output_enter_event(the_output);
    });
}

void miriway::ExtWorkspaceManagerV1::Global::output_removed(const miral::Output &output)
{
    context->run_on_wayland_mainloop([this, output]
    {
        if (the_workspace_manager) the_workspace_manager->output_deleted(wltools, output);
    });
}

void miriway::ExtWorkspaceManagerV1::output_deleted(miral::WaylandTools* wltools, miral::Output const& output)
{
    wltools->for_each_binding(client, output, [this](wl_resource* the_output)
    {
        the_workspace_group->send_output_leave_event(the_output);
    });
}

miriway::ExtWorkspaceManagerV1::Global::Global(miral::WaylandExtensions::Context const* context, miral::WaylandTools& wltools) :
    mir::wayland::ExtWorkspaceManagerV1::Global{context->display(), Version<1>{}},
    context{context},
    the_workspace_manager{nullptr},
    wltools{&wltools}
{
    all_the_globals.insert(this);
}

miriway::ExtWorkspaceManagerV1::Global::~Global()
{
    all_the_globals.erase(this);
}

void miriway::ExtWorkspaceManagerV1::Global::bind(wl_resource* new_ext_workspace_manager_v1)
{
    the_workspace_manager = new ExtWorkspaceManagerV1{new_ext_workspace_manager_v1};
    std:: lock_guard lock(all_the_outputs_mutex);
    for (auto const& output : all_the_outputs)
        the_workspace_manager->output_added(wltools, output);
}

void miriway::ExtWorkspaceManagerV1::Global::output_created(miral::Output const& output)
{
    {
        std:: lock_guard lock(all_the_outputs_mutex);
        all_the_outputs.insert(output);
    }
    for (auto const& global : all_the_globals)
        global->output_added(output);
}

void miriway::ExtWorkspaceManagerV1::Global::output_deleted(const miral::Output &output)
{
    {
        std:: lock_guard lock(all_the_outputs_mutex);
        all_the_outputs.insert(output);
    }

    for (auto const& global : all_the_globals)
        global->output_removed(output);
}

void miriway::ExtWorkspaceManagerV1::Global::output_added(miral::Output const& output)
{
    context->run_on_wayland_mainloop([this, output]
    {
        if (the_workspace_manager) the_workspace_manager->output_added(wltools, output);
    });
}

miriway::ExtWorkspaceGroupHandleV1::ExtWorkspaceGroupHandleV1(ExtWorkspaceManagerV1& manager) :
    mir::wayland::ExtWorkspaceGroupHandleV1{manager}
{
}

void miriway::ExtWorkspaceGroupHandleV1::create_workspace(std::string const& workspace)
{
    (void)workspace;
}

void miriway::ExtWorkspaceGroupHandleV1::destroy()
{
}

void miriway::ExtWorkspaceHandleV1::destroy()
{
}

void miriway::ExtWorkspaceHandleV1::activate()
{
}

void miriway::ExtWorkspaceHandleV1::deactivate()
{
}

void miriway::ExtWorkspaceHandleV1::assign(wl_resource* workspace_group)
{
    (void)workspace_group;
}

void miriway::ExtWorkspaceHandleV1::remove()
{
}
