/*
 * Copyright © 2023 Octopull Ltd.
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

#ifndef MIRIWAY_WORKSPACE_HOOKS_H_
#define MIRIWAY_WORKSPACE_HOOKS_H_

#include <functional>
#include <memory>

namespace miral { class Workspace; class Output; }

namespace miriway
{
using miral::Workspace;
using miral::Output;

/// An interface to "hook into" the workspace management.
/// Mostly, this provides notifications of workspace management events via `on...` methods.
/// It also provides a callback to request workspace activation.
class WorkspaceHooks
{
public:
    WorkspaceHooks() = default;
    virtual ~WorkspaceHooks() = default;

    virtual void on_workspace_create(std::shared_ptr<Workspace> const& wksp) = 0;
    virtual void on_workspace_activate(std::shared_ptr<Workspace> const& wksp) = 0;
    virtual void on_workspace_deactivate(std::shared_ptr<Workspace> const& wksp) = 0;
    virtual void on_workspace_destroy(std::shared_ptr<Workspace> const& wksp) = 0;

    virtual void on_output_create(Output const& output) = 0;
    virtual void on_output_destroy(Output const& output) = 0;

    virtual void set_workspace_activator_callback(std::function<void(std::shared_ptr<Workspace> const &wksp)> f) = 0;

private:
    WorkspaceHooks(WorkspaceHooks const&) = delete;
    WorkspaceHooks& operator=(WorkspaceHooks const&) = delete;
};
} // miriway

#endif //MIRIWAY_WORKSPACE_HOOKS_H_
