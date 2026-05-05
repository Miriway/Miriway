/*
 * Copyright © 2022 Octopull Ltd.
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

#include "miriway_commands.h"
#include "miriway_policy.h"

#include <miral/runner.h>

#include <xkbcommon/xkbcommon-keysyms.h>

#include <utility>
#include <mir/log.h>

miriway::ShellCommands::ShellCommands(
    MirRunner& runner, CommandFunctor meta_command, CommandFunctor ctrl_alt_command, CommandFunctor alt_command) :
    runner{runner}, meta_command{std::move(meta_command)}, ctrl_alt_command{std::move(ctrl_alt_command)}, alt_command{std::move(alt_command)}
{
}

void miriway::ShellCommands::advise_new_window_for(miral::Application const& /*app*/)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    ++app_windows;
}

void miriway::ShellCommands::advise_delete_window_for(miral::Application const& /*app*/)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    --app_windows;
}

auto miriway::ShellCommands::keyboard_shortcuts(MirKeyboardEvent const* kev) -> bool
{
    if (mir_keyboard_event_action(kev) == mir_keyboard_action_up)
        return false;

    auto const mods = mir_keyboard_event_modifiers(kev);
    auto const key_code = mir_keyboard_event_keysym(kev);

    auto const ctrl_alt = mir_input_event_modifier_alt | mir_input_event_modifier_ctrl;

    if ((mods & ctrl_alt) == ctrl_alt)
    {
        if (key_code == XKB_KEY_Delete && mir_keyboard_event_action(kev) == mir_keyboard_action_down)
        {
            shell_commands_active = !shell_commands_active;
            return true;
        }
    }

    if (!shell_commands_active)
        return false;


    if (mir_keyboard_event_action(kev) == mir_keyboard_action_down)
    {
        if ((mods & ctrl_alt) == ctrl_alt)
        {
            return ctrl_alt_command(key_code, mods & mir_input_event_modifier_shift, this);
        }

        if ((mods & mir_input_event_modifier_meta) == mir_input_event_modifier_meta)
        {
            return meta_command(key_code, mods & mir_input_event_modifier_shift, this);
        }

        if ((mods & mir_input_event_modifier_alt) == mir_input_event_modifier_alt)
        {
            return alt_command(key_code, mods & mir_input_event_modifier_shift, this);
        }
    }

    return false;
}

auto miriway::ShellCommands::touch_shortcuts(MirTouchEvent const* /*tev*/) -> bool
{
    return false;
}

auto miriway::ShellCommands::input_event(MirEvent const* event) -> bool
{
    if (mir_event_get_type(event) != mir_event_type_input)
        return false;

    auto const* input_event = mir_event_get_input_event(event);

    switch (mir_input_event_get_type(input_event))
    {
    case mir_input_event_type_touch:
        return touch_shortcuts(mir_input_event_get_touch_event(input_event));

    case mir_input_event_type_key:
        return keyboard_shortcuts(mir_input_event_get_keyboard_event(input_event));

    default:
        return false;
    }
}

void miriway::ShellCommands::init_window_manager(WindowManagerPolicy* wm)
{
    this->wm = wm;
}

void miriway::ShellCommands::dock_active_window_left(bool) const
{
    wm->dock_active_window_left();
}

void miriway::ShellCommands::dock_active_window_right(bool) const
{
    wm->dock_active_window_right();
}

void miriway::ShellCommands::toggle_maximized_restored(bool) const
{
    wm->toggle_maximized_restored();
}

void miriway::ShellCommands::toggle_always_on_top(bool) const
{
    wm->toggle_always_on_top();
}

void miriway::ShellCommands::workspace_begin(bool shift) const
{
    wm->workspace_begin(shift);
}

void miriway::ShellCommands::workspace_end(bool shift) const
{
    wm->workspace_end(shift);
}

void miriway::ShellCommands::workspace_up(bool shift) const
{
    wm->workspace_up(shift);
}

void miriway::ShellCommands::workspace_down(bool shift) const
{
    wm->workspace_down(shift);
}

void miriway::ShellCommands::exit(bool shift) const
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    if (app_windows == 0 || shift)
    {
        runner.stop();
    }
}
