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

#include <miral/wayland_extensions.h>
#include <miral/miral/minimal_window_manager.h>

namespace mir::wayland { class Client; }
namespace miral { class WaylandTools; class Output; }
namespace miriway
{
class ExtWorkspaceGroupHandleV1;

class ExtWorkspaceManagerV1 : public mir::wayland::ExtWorkspaceManagerV1
{
public:
    explicit ExtWorkspaceManagerV1(wl_resource* new_ext_workspace_manager_v1);
    void commit() override;
    void stop() override;
    void output_added(miral::WaylandTools* wltools, miral::Output const& output);

    class Global;

private:
    ExtWorkspaceGroupHandleV1* the_workspace_group;
};

class ExtWorkspaceManagerV1::Global : public mir::wayland::ExtWorkspaceManagerV1::Global
{
public:
    Global(miral::WaylandExtensions::Context const* context, miral::WaylandTools& wltools);
    ~Global();
    void bind(wl_resource* new_ext_workspace_manager_v1) override;

    static void output_created(miral::Output const& output);
private:
    void output_added(miral::Output const& output);
    miral::WaylandExtensions::Context const* const context;
    ExtWorkspaceManagerV1* the_workspace_manager;
    miral::WaylandTools* const wltools;
};

class ExtWorkspaceGroupHandleV1 :  public mir::wayland::ExtWorkspaceGroupHandleV1
{
public:
    explicit ExtWorkspaceGroupHandleV1(ExtWorkspaceManagerV1& manager);

private:
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
