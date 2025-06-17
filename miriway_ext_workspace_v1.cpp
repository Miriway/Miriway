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

miriway::ExtWorkspaceManagerV1::ExtWorkspaceManagerV1(wl_resource* new_ext_workspace_manager_v1) :
    mir::wayland::ExtWorkspaceManagerV1{new_ext_workspace_manager_v1, Version<1>{}}
{
    new ExtWorkspaceGroupHandleV1{*this};
}

void miriway::ExtWorkspaceManagerV1::commit()
{
}

void miriway::ExtWorkspaceManagerV1::stop()
{
}

void miriway::ExtWorkspaceManagerV1::Global::bind(wl_resource* new_ext_workspace_manager_v1)
{
    new ExtWorkspaceManagerV1{new_ext_workspace_manager_v1};
}

miriway::ExtWorkspaceGroupHandleV1::ExtWorkspaceGroupHandleV1(ExtWorkspaceManagerV1& manager) :
    mir::wayland::ExtWorkspaceGroupHandleV1{manager}
{
    send_capabilities_event(0);
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
