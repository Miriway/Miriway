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
    MirRunner& runner, std::function<bool(char)> meta_command, std::function<bool(char)> ctrl_alt_command) :
    runner{runner}, meta_command{std::move(meta_command)}, ctrl_alt_command{std::move(ctrl_alt_command)}
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
    auto const key_code = mir_keyboard_event_key_code(kev);

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

    // Actions on "Ctrl+Alt" keys
    if ((mods & ctrl_alt) == ctrl_alt)
    {
        switch (key_code)
        {
        case XKB_KEY_BackSpace:
            if (mir_keyboard_event_action(kev) == mir_keyboard_action_down)
            {
                std::lock_guard<decltype(mutex)> lock{mutex};
                if (app_windows > 0)
                {
                    return false;
                }
            }
            runner.stop();
            return true;

        default:
            return (mir_keyboard_event_action(kev) == mir_keyboard_action_down) &&
                ctrl_alt_command(key_code);
        }
    }

    if (!(mods & mir_input_event_modifier_meta) || (mods & ctrl_alt))
        return false;

    // Actions on "Meta" key
    switch (key_code)
    {
    case XKB_KEY_Left:
        wm->dock_active_window_left();
        return true;

    case XKB_KEY_Right:
        wm->dock_active_window_right();
        return true;

    case XKB_KEY_space:
        wm->toggle_maximized_restored();
        return true;

    case XKB_KEY_Home:
        wm->workspace_begin(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_End:
        wm->workspace_end(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_Page_Up:
        wm->workspace_up(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_Page_Down:
        wm->workspace_down(mods & mir_input_event_modifier_shift);
        return true;

    default:
        return (mir_keyboard_event_action(kev) == mir_keyboard_action_down) &&
               meta_command(key_code);
    }
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
