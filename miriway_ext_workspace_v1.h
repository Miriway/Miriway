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

#include <miral/wayland_extensions.h>


namespace miral { class WaylandTools; class Output; class Workspace; }
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
} // miriway

#endif //MIRIWAY_EXT_WORKSPACE_V1_H
