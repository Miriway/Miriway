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
#else
namespace miral { class WaylandTools; }
#endif
#include <miral/wayland_extensions.h>

#include <cstring>
#include <format>
#include <list>
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

    void workspace_created(std::shared_ptr<Workspace> const& wksp);
    void workspace_activated(std::shared_ptr<Workspace> const& wksp);
    void workspace_deactivated(std::shared_ptr<Workspace> const& wksp);
    void workspace_destroyed(std::shared_ptr<Workspace> const& wksp);

    void on_activate(ExtWorkspaceHandleV1* wh);
    void on_destroy(ExtWorkspaceHandleV1* wh);

    class Global;

private:
    void update_workspace_info(ExtWorkspaceHandleV1 const* wh, unsigned int i) const;

    ExtWorkspaceGroupHandleV1* const the_workspace_group;
    std::list<std::pair<std::shared_ptr<Workspace>, ExtWorkspaceHandleV1*>> workspaces;
};

class ExtWorkspaceManagerV1::Global : public mir::wayland::ExtWorkspaceManagerV1::Global
{
public:
    Global(miral::WaylandExtensions::Context const* context, miral::WaylandTools& wltools);
    ~Global();
    void bind(wl_resource* new_ext_workspace_manager_v1) override;

    void output_added(miral::Output const& output);
    void output_removed(const miral::Output &output);
    void workspace_created(std::shared_ptr<Workspace> const& wksp);
    void workspace_activated(std::shared_ptr<Workspace> const& wksp);
    void workspace_deactivated(std::shared_ptr<Workspace> const& wksp);
    void workspace_destroyed(std::shared_ptr<Workspace> const& wksp);

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
enum class WkspState { active, hidden };
std::map<std::shared_ptr<Workspace>, WkspState> all_the_workspaces;

std::function<void(std::shared_ptr<Workspace> const& wksp)> activate = [](auto const&){};
}

void miriway::ExtWorkspaceV1::on_workspace_create(std::shared_ptr<Workspace> const& wksp)
{
    {
        std::lock_guard lock(all_the_workspaces_mutex);
        all_the_workspaces.emplace(wksp, WkspState::hidden);
    }
    std::lock_guard lock{all_the_globals_mutex};
    for (auto const& global : all_the_globals)
        global->workspace_created(wksp);
}

void miriway::ExtWorkspaceV1::on_workspace_activate(std::shared_ptr<Workspace> const& wksp)
{
    std::unique_lock lock(all_the_workspaces_mutex);
    if (auto const i = all_the_workspaces.find(wksp); i != all_the_workspaces.end())
    {
        i->second = WkspState::active;
        lock.unlock();

        std::lock_guard lock_g{all_the_globals_mutex};
        for (auto const &global: all_the_globals)
            global->workspace_activated(wksp);
    }
}

void miriway::ExtWorkspaceV1::on_workspace_deactivate(std::shared_ptr<Workspace> const& wksp)
{
    std::unique_lock lock(all_the_workspaces_mutex);
    if (auto const i = all_the_workspaces.find(wksp); i != all_the_workspaces.end())
    {
        i->second = WkspState::hidden;
        lock.unlock();

        std::lock_guard lock_g{all_the_globals_mutex};
        for (auto const &global: all_the_globals)
            global->workspace_deactivated(wksp);
    }
}

void miriway::ExtWorkspaceV1::on_workspace_destroy(std::shared_ptr<Workspace> const& wksp)
{
    {
        std::lock_guard lock(all_the_workspaces_mutex);
        all_the_workspaces.erase(wksp);
    }

    std::lock_guard lock_g{all_the_globals_mutex};
    for (auto const &global: all_the_globals)
        global->workspace_destroyed(wksp);
}

void miriway::ExtWorkspaceV1::on_output_create(const Output& output)
{
    {
        std:: lock_guard lock(all_the_outputs_mutex);
        all_the_outputs.insert(output);
    }
    std::lock_guard lock{all_the_globals_mutex};
    for (auto const& global : all_the_globals)
        global->output_added(output);
}

void miriway::ExtWorkspaceV1::on_output_destroy(const Output& output)
{
    {
        std:: lock_guard lock(all_the_outputs_mutex);
        all_the_outputs.insert(output);
    }
    std::lock_guard lock{all_the_globals_mutex};
    for (auto const& global : all_the_globals)
        global->output_removed(output);
}

void miriway::ExtWorkspaceV1::set_workspace_activator_callback(std::function<void(std::shared_ptr<Workspace> const& wksp)> f)
{
    activate = std::move(f);
}

miriway::ExtWorkspaceManagerV1::ExtWorkspaceManagerV1(wl_resource* new_ext_workspace_manager_v1) :
    mir::wayland::ExtWorkspaceManagerV1{new_ext_workspace_manager_v1, Version<1>{}},
    the_workspace_group{new ExtWorkspaceGroupHandleV1{*this}}
{
    send_workspace_group_event(the_workspace_group->resource);
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
        if (the_workspace_manager)
        {
            the_workspace_manager->output_deleted(wltools, output);
            the_workspace_manager->send_done_event();
        }
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

void miriway::ExtWorkspaceManagerV1::workspace_created(std::shared_ptr<Workspace> const& wksp)
{
    auto const wh = new ExtWorkspaceHandleV1{*this};
    workspaces.emplace_back(wksp, wh);
    send_workspace_event(wh->resource);
    wh->send_capabilities_event(ExtWorkspaceHandleV1::WorkspaceCapabilities::activate);

    update_workspace_info(wh, workspaces.size());

    the_workspace_group->send_workspace_enter_event(wh->resource);
}

void miriway::ExtWorkspaceManagerV1::update_workspace_info(ExtWorkspaceHandleV1 const* wh, unsigned int i) const
{
    wh->send_name_event(std::format("Wksp {}", i));
    uint32_t const source[2] = {i, 0};
    wl_array coordinates;
    wl_array_init(&coordinates);
    auto target = wl_array_add(&coordinates, sizeof(source));
    memcpy(target, source, sizeof(source));
    wh->send_coordinates_event(&coordinates);
    wl_array_release(&coordinates);
}

void miriway::ExtWorkspaceManagerV1::workspace_activated(std::shared_ptr<Workspace> const& wksp)
{
    for (auto const& [_, wh]: workspaces)
    {
        if (_ == wksp)
        {
            wh->send_state_event(ExtWorkspaceHandleV1::State::active);
            break;
        }
    }
}

void miriway::ExtWorkspaceManagerV1::workspace_deactivated(std::shared_ptr<Workspace> const& wksp)
{
    for (auto const& [_, wh]: workspaces)
    {
        if (_ == wksp)
        {
            wh->send_state_event(ExtWorkspaceHandleV1::State::hidden);
            break;
        }
    }
}

void miriway::ExtWorkspaceManagerV1::workspace_destroyed(std::shared_ptr<Workspace> const& wksp)
{
    auto searching = true;
    unsigned int i = 0;
    for (auto it = workspaces.begin(); it != workspaces.end();)
    {
        if (it->first == wksp)
        {
            the_workspace_group->send_workspace_leave_event(it->second->resource);
            it->second->send_removed_event();
            it = workspaces.erase(it);
            searching = false;
        }
        else
        {
            ++i;
            if (!searching)
            {
                update_workspace_info(it->second, i);
            }
            ++it;
        }
    }
}

void miriway::ExtWorkspaceManagerV1::on_destroy(miriway::ExtWorkspaceHandleV1* wh)
{
    auto p = std::find_if(workspaces.begin(), workspaces.end(), [wh](auto const& wksp_pair) { return wksp_pair.second == wh; });
    if (p != workspaces.end()) workspaces.erase(p);
}

void miriway::ExtWorkspaceManagerV1::on_activate(miriway::ExtWorkspaceHandleV1* wh)
{
    auto p = std::find_if(workspaces.begin(), workspaces.end(), [wh](auto const& e) { return e.second == wh; });
    if (p != workspaces.end())
    {
        auto const& wksp = p->first;
        std::unique_lock lock(all_the_workspaces_mutex);

        if (auto q = all_the_workspaces.find(wksp); q != all_the_workspaces.end())
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
        for (auto const& [wksp, state]: all_the_workspaces)
        {
            the_workspace_manager->workspace_created(wksp);
            if (state == WkspState::active)
                the_workspace_manager->workspace_activated(wksp);
            else
                the_workspace_manager->workspace_deactivated(wksp);
        }
    }
    the_workspace_manager->send_done_event();
}

void miriway::ExtWorkspaceManagerV1::Global::output_added(miral::Output const& output)
{
    context->run_on_wayland_mainloop([this, output]
    {
        if (the_workspace_manager)
        {
            the_workspace_manager->output_added(wltools, output);
            the_workspace_manager->send_done_event();
        }
    });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_destroyed(std::shared_ptr<Workspace> const& wksp)
{
    context->run_on_wayland_mainloop([this, wksp]
        {
            if (the_workspace_manager)
            {
                the_workspace_manager->workspace_destroyed(wksp);
                the_workspace_manager->send_done_event();
            }
        });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_deactivated(std::shared_ptr<Workspace> const& wksp)
{
    context->run_on_wayland_mainloop([this, wksp]
        {
            if (the_workspace_manager)
            {
                the_workspace_manager->workspace_deactivated(wksp);
                the_workspace_manager->send_done_event();
            }
        });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_activated(std::shared_ptr<Workspace> const& wksp)
{
    context->run_on_wayland_mainloop([this, wksp]
        {
            if (the_workspace_manager)
            {
                the_workspace_manager->workspace_activated(wksp);
                the_workspace_manager->send_done_event();
            }
        });
}

void miriway::ExtWorkspaceManagerV1::Global::workspace_created(std::shared_ptr<Workspace> const& wksp)
{
    context->run_on_wayland_mainloop([this, wksp]
        {
            if (the_workspace_manager)
            {
                the_workspace_manager->workspace_created(wksp);
                the_workspace_manager->send_done_event();
            }
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

auto miriway::build_ext_workspace_v1_global(miral::WaylandTools& wltools) -> miral::WaylandExtensions::Builder
{
    return miral::WaylandExtensions::Builder
    {
        .name = ExtWorkspaceManagerV1::interface_name,
        .build = [&](auto* context) { return std::make_shared<ExtWorkspaceManagerV1::Global>(context, wltools); }
    };
}

auto miriway::ext_workspace_v1_name() -> char const*
{
    return ExtWorkspaceManagerV1::interface_name;
}