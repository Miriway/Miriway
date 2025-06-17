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

#ifndef MIRIWAY_EXT_WORKSPACE_V1_H
#define MIRIWAY_EXT_WORKSPACE_V1_H

#include "wayland-generated/ext-workspace-v1_wrapper.h"

namespace miriway
{
class ExtWorkspaceManagerV1 : public mir::wayland::ExtWorkspaceManagerV1
{
public:
    using mir::wayland::ExtWorkspaceManagerV1::ExtWorkspaceManagerV1;
    void commit() override;
    void stop() override;

    class Global;
};

class ExtWorkspaceManagerV1::Global : public mir::wayland::ExtWorkspaceManagerV1::Global
{
public:
    explicit Global(wl_display* display) : mir::wayland::ExtWorkspaceManagerV1::Global(display, Version<1>{}) {}
    void bind(wl_resource* new_ext_workspace_manager_v1) override;
};

class ExtWorkspaceGroupHandleV1 :  public mir::wayland::ExtWorkspaceGroupHandleV1
{
    void create_workspace(std::string const& workspace) override;
    void destroy() override;
};

class ExtWorkspaceHandleV1 : public mir::wayland::ExtWorkspaceHandleV1
{
public:
    void destroy() override;
    void activate() override;
    void deactivate() override;
    void assign(wl_resource* workspace_group) override;
    void remove() override;
};
} // miriway

#endif //MIRIWAY_EXT_WORKSPACE_V1_H
