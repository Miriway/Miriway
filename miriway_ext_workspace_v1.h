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

#include "miriway_workspace_hooks.h"

#include <miral/wayland_extensions.h>

namespace miral { class Output; class WaylandTools; }
namespace miriway
{
class ExtWorkspaceV1 : public WorkspaceHooks
{
public:
    void on_workspace_create(std::shared_ptr<Workspace> const& wksp) override;
    void on_workspace_activate(std::shared_ptr<Workspace> const& wksp) override;
    void on_workspace_deactivate(std::shared_ptr<Workspace> const& wksp) override;
    void on_workspace_destroy(std::shared_ptr<Workspace> const& wksp) override;
    void on_output_create(Output const& output) override;
    void on_output_destroy(Output const& output) override;
    void set_workspace_activator_callback(std::function<void(std::shared_ptr<Workspace> const& wksp)> f) override;
};

auto ext_workspace_v1_name() -> char const*;
auto build_ext_workspace_v1_global(miral::WaylandTools& wltools) -> miral::WaylandExtensions::Builder;
} // miriway

#endif //MIRIWAY_EXT_WORKSPACE_V1_H
