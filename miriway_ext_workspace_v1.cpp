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
#include "wayland-generated/ext-workspace-v1_wrapper.h"

#include <miral/output.h>
#include <miral/version.h>
#if MIRAL_VERSION >= MIR_VERSION_NUMBER(5, 5, 0)
#define MIR_SUPPORTS_XDG_WORKSPACE
#include <miral/wayland_tools.h>
#endif

#include <cstring>
#include <format>
#include <map>
#include <mutex>

using miral::Workspace;

namespace miriway
{
class ExtWorkspaceGroupHandleV1;
class ExtWorkspaceHandleV1;

class ExtWorkspaceManagerV1 : public mir::wayland::ExtWorkspaceManagerV1
{
public:
    explicit ExtWorkspaceManagerV1(wl_resource* new_ext_workspace_manager_v1);
    void commit() override;
    void stop() override;
    void output_added(miral::WaylandTools* wltools, miral::Output const& output);
    void output_deleted(miral::WaylandTools* wltools, miral::Output const& output);

    void workspace_created(unsigned id);
    void workspace_activated(unsigned id);
    void workspace_deactivated(unsigned id);
    void workspace_destroyed(unsigned id);

    void on_activate(ExtWorkspaceHandleV1* wksp);
    void on_destroy(ExtWorkspaceHandleV1* wksp);

    class Global;

private:
    void update_names() const;

    ExtWorkspaceGroupHandleV1* const the_workspace_group;
    std::map<unsigned, ExtWorkspaceHandleV1*> workspaces;
};

class ExtWorkspaceManagerV1::Global : public mir::wayland::ExtWorkspaceManagerV1::Global
{
public:
    Global(miral::WaylandExtensions::Context const* context, miral::WaylandTools& wltools);
    ~Global();
    void bind(wl_resource* new_ext_workspace_manager_v1) override;

    void output_added(miral::Output const& output);
    void output_removed(const miral::Output &output);
    void workspace_created(unsigned id);
    void workspace_activated(unsigned id);
    void workspace_deactivated(unsigned id);
    void workspace_destroyed(unsigned id);

private:
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
    explicit ExtWorkspaceHandleV1(ExtWorkspaceManagerV1& parent);
    void destroy() override;
    void activate() override;
    void deactivate() override;
    void assign(wl_resource* workspace_group) override;
    void remove() override;

private:
    miriway::ExtWorkspaceManagerV1& parent;
};
}

namespace
{
// There will be one global for each connection (client) only updated on Wayland thread, but read by "server side"
std::mutex all_the_globals_mutex;
std::set<miriway::ExtWorkspaceManagerV1::Global*> all_the_globals;

// The output list is maintained "server side" and accessed "frontend side"
std::mutex all_the_outputs_mutex;
std::set<miral::Output, decltype([](auto const& l, auto const& r){ return l.id() < r.id(); })> all_the_outputs;

// The workspace list is maintained "server side" and accessed "frontend side"
std::mutex all_the_workspaces_mutex;
unsigned int the_workspaces_counter = 0;
std::map<std::shared_ptr<Workspace>, unsigned int> all_the_workspaces;

std::function<void(std::shared_ptr<Workspace> const& wksp)> activate = [](auto const&){};
}

void miriway::ext_workspace_hooks::output_created(miral::Output const& output)
{
    {
        std:: lock_guard lock(all_the_outputs_mutex);
        all_the_outputs.insert(output);
    }
    std::lock_guard lock{all_the_globals_mutex};
    for (auto const& global : all_the_globals)
        global->output_added(output);
}

void miriway::ext_workspace_hooks::output_deleted(const miral::Output &output)
{
    {
        std:: lock_guard lock(all_the_outputs_mutex);
        all_the_outputs.insert(output);
    }

    std::lock_guard lock{all_the_globals_mutex};
    for (auto const& global : all_the_globals)
        global->output_removed(output);
}

void miriway::ext_workspace_hooks::workspace_activator(
        std::function<void(std::shared_ptr<miral::Workspace> const&)> f)
{
    ::activate = std::move(f);
}

void miriway::ext_workspace_hooks::workspace_created(const std::shared_ptr<miral::Workspace> &wksp)
{
    unsigned int id;
    {
        std::lock_guard lock(all_the_workspaces_mutex);
        all_the_workspaces.emplace(wksp, id = the_workspaces_counter++);
    }

    std::lock_guard lock{all_the_globals_mutex};
    for (auto const& global : all_the_globals)
        global->workspace_created(id);
}

void miriway::ext_workspace_hooks::workspace_activated(const std::shared_ptr<miral::Workspace> &wksp)
{
    std::unique_lock lock(all_the_workspaces_mutex);
    if (auto const i = all_the_workspaces.find(wksp); i != all_the_workspaces.end())
    {
        unsigned int const id = i->second;
        lock.unlock();

        std::lock_guard lock_g{all_the_globals_mutex};
        for (auto const &global: all_the_globals)
            global->workspace_activated(id);
    }
}

void miriway::ext_workspace_hooks::workspace_deactivated(const std::shared_ptr<miral::Workspace> &wksp)
{
    std::unique_lock lock(all_the_workspaces_mutex);
    if (auto const i = all_the_workspaces.find(wksp); i != all_the_workspaces.end())
    {
        unsigned int const id = i->second;
        lock.unlock();

        std::lock_guard lock_g{all_the_globals_mutex};
        for (auto const &global: all_the_globals)
            global->workspace_deactivated(id);
    }
}

void miriway::ext_workspace_hooks::workspace_destroyed(const std::shared_ptr<miral::Workspace> &wksp)
{
    std::unique_lock lock(all_the_workspaces_mutex);
    if (auto const i = all_the_workspaces.find(wksp); i != all_the_workspaces.end())
    {
        unsigned int const id = i->second;
        all_the_workspaces.erase(i);

        lock.unlock();

        std::lock_guard lock_g{all_the_globals_mutex};
        for (auto const &global: all_the_globals)
            global->workspace_destroyed(id);
    }
}

auto miriway::ext_workspace_hooks::build_global(miral::WaylandTools& wltools) -> miral::WaylandExtensions::Builder
{
    return miral::WaylandExtensions::Builder
    {
        .name = ExtWorkspaceManagerV1::interface_name,
        .build = [&](auto* context) { return std::make_shared<ExtWorkspaceManagerV1::Global>(context, wltools); }
    };
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
#ifdef MIR_SUPPORTS_XDG_WORKSPACE
    wltools->for_each_binding(client, output, [this](wl_resource* the_output)
    {
        the_workspace_group->send_output_enter_event(the_output);
    });
#else
    (void)wltools;
    (void)output;
#endif
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
#ifdef MIR_SUPPORTS_XDG_WORKSPACE
    wltools->for_each_binding(client, output, [this](wl_resource* the_output)
    {
        the_workspace_group->send_output_leave_event(the_output);
    });
#else
    (void)wltools;
    (void)output;
#endif
}

void miriway::ExtWorkspaceManagerV1::workspace_created(unsigned id)
{
    auto const wksp = new ExtWorkspaceHandleV1{*this};
    workspaces.emplace(id, wksp);
    send_workspace_event(wksp->resource);
    wksp->send_capabilities_event(ExtWorkspaceHandleV1::WorkspaceCapabilities::activate);

    update_names();

    uint32_t const source[2] = {id, 0};
    wl_array coordinates;
    wl_array_init(&coordinates);
    auto target = wl_array_add(&coordinates, sizeof(source));
    memcpy(target, source, sizeof(source));
    wksp->send_coordinates_event(&coordinates);
    wl_array_release(&coordinates);

    the_workspace_group->send_workspace_enter_event(wksp->resource);
}

void miriway::ExtWorkspaceManagerV1::update_names() const
{
    unsigned int i = 0;

    for (auto const& [_, w]: workspaces)
    {
        w->send_name_event(std::format("Wksp {}", ++i));
    }
}

void miriway::ExtWorkspaceManagerV1::workspace_activated(unsigned id)
{
    auto const wksp = workspaces[id];
    wksp->send_state_event(ExtWorkspaceHandleV1::State::active);
}

void miriway::ExtWorkspaceManagerV1::workspace_deactivated(unsigned id)
{
    auto const wksp = workspaces[id];
    wksp->send_state_event(ExtWorkspaceHandleV1::State::hidden);
}

void miriway::ExtWorkspaceManagerV1::workspace_destroyed(unsigned id)
{
    auto const wksp = workspaces[id];
    the_workspace_group->send_workspace_leave_event(wksp->resource);
    wksp->send_removed_event();
    workspaces.erase(id);
    update_names();
}

void miriway::ExtWorkspaceManagerV1::on_destroy(miriway::ExtWorkspaceHandleV1* wksp)
{
    auto p = std::find_if(workspaces.begin(), workspaces.end(), [wksp](auto const& wksp_pair) { return wksp_pair.second == wksp; });
    if (p != workspaces.end()) workspaces.erase(p);
}

void miriway::ExtWorkspaceManagerV1::on_activate(miriway::ExtWorkspaceHandleV1* wksp)
{
    auto p = std::find_if(workspaces.begin(), workspaces.end(), [wksp](auto const& wksp_pair) { return wksp_pair.second == wksp; });
    if (p != workspaces.end())
    {
        unsigned int id = p->first;
        std::unique_lock lock(all_the_workspaces_mutex);
        auto q = std::find_if(all_the_workspaces.begin(), all_the_workspaces.end(), [id](auto const& wksp_pair) { return wksp_pair.second == id; });
        if (q != all_the_workspaces.end())
        {

            auto const wksp = q->first;
            lock.unlock();

            ::activate(wksp);
        }
    }
}

miriway::ExtWorkspaceManagerV1::Global::Global(miral::WaylandExtensions::Context const* context, miral::WaylandTools& wltools) :
    mir::wayland::ExtWorkspaceManagerV1::Global{context->display(), Version<1>{}},
    context{context},
    the_workspace_manager{nullptr},
    wltools{&wltools}
{
    std::lock_guard lock{all_the_globals_mutex};
    all_the_globals.insert(this);
}

miriway::ExtWorkspaceManagerV1::Global::~Global()
{
    std::lock_guard lock{all_the_globals_mutex};
    all_the_globals.erase(this);
}

void miriway::ExtWorkspaceManagerV1::Global::bind(wl_resource* new_ext_workspace_manager_v1)
{
    the_workspace_manager = new ExtWorkspaceManagerV1{new_ext_workspace_manager_v1};
    {
        std::lock_guard lock(all_the_outputs_mutex);
        for (auto const &output: all_the_outputs)
            the_workspace_manager->output_added(wltools, output);
    }
    {
        std::lock_guard lock(all_the_workspaces_mutex);
        for (auto const& [wksp, id]: all_the_workspaces)
            the_workspace_manager->workspace_created(id);
    }
    the_workspace_manager->send_done_event();
}

void miriway::ExtWorkspaceManagerV1::Global::output_added(miral::Output const& output)
{
    context->run_on_wayland_mainloop([this, output]
    {
        if (the_workspace_manager) the_workspace_manager->output_added(wltools, output);
    });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_destroyed(unsigned int id)
{
    context->run_on_wayland_mainloop([this, id]
        {
            if (the_workspace_manager) the_workspace_manager->workspace_destroyed(id);
        });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_deactivated(unsigned int id)
{
    context->run_on_wayland_mainloop([this, id]
        {
            if (the_workspace_manager) the_workspace_manager->workspace_deactivated(id);
        });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_activated(unsigned int id)
{
    context->run_on_wayland_mainloop([this, id]
        {
            if (the_workspace_manager) the_workspace_manager->workspace_activated(id);
        });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_created(unsigned int id)
{
    context->run_on_wayland_mainloop([this, id]
        {
            if (the_workspace_manager) the_workspace_manager->workspace_created(id);
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
    parent.on_destroy(this);
}

void miriway::ExtWorkspaceHandleV1::activate()
{
    parent.on_activate(this);
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

miriway::ExtWorkspaceHandleV1::ExtWorkspaceHandleV1(miriway::ExtWorkspaceManagerV1& parent) :
    mir::wayland::ExtWorkspaceHandleV1::ExtWorkspaceHandleV1{parent},
    parent{parent}
{
}
