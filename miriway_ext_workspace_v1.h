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

#include "miriway_workspace_observer.h"

#include <miral/wayland_extensions.h>

namespace miral { class WaylandTools; class Output; }
namespace miriway
{

namespace ext_workspace_hooks
{
void output_created(miral::Output const &output);

void output_deleted(const miral::Output &output);

void workspace_created(std::shared_ptr<miral::Workspace> const &wksp);

void workspace_activated(std::shared_ptr<miral::Workspace> const &wksp);

void workspace_deactivated(std::shared_ptr<miral::Workspace> const &wksp);

void workspace_destroyed(std::shared_ptr<miral::Workspace> const &wksp);

void workspace_activator(std::function<void(std::shared_ptr<miral::Workspace> const &wksp)> f);

auto build_global(miral::WaylandTools &wltools) -> miral::WaylandExtensions::Builder;
}

class ExtWorkspaceObserver : public WorkspaceObserver
{
public:
    void on_workspace_create(const std::shared_ptr<Workspace> &wksp) override
    {
        ext_workspace_hooks::workspace_created(wksp);
    }

    void on_workspace_activate(const std::shared_ptr<Workspace> &wksp) override
    {
        ext_workspace_hooks::workspace_activated(wksp);
    }

    void on_workspace_deactivate(const std::shared_ptr<Workspace> &wksp) override
    {
        ext_workspace_hooks::workspace_deactivated(wksp);
    }

    void on_workspace_destroy(const std::shared_ptr<Workspace> &wksp) override
    {
        ext_workspace_hooks::workspace_destroyed(wksp);
    }

    void on_output_create(const Output& output) override
    {
        ext_workspace_hooks::output_created(output);
    }

    void on_output_destroy(const Output& output) override
    {
        ext_workspace_hooks::output_deleted(output);
    }

    void set_workspace_activator_callback(std::function<void(std::shared_ptr<Workspace> const& wksp)> f) override
    {
        ext_workspace_hooks::workspace_activator(f);
    }
};
} // miriway

#endif //MIRIWAY_EXT_WORKSPACE_V1_H
