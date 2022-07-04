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
#include <miral/external_client.h>

#include <xkbcommon/xkbcommon-keysyms.h>

#include <utility>

miriway::ShellCommands::ShellCommands(MirRunner& runner, ExternalClientLauncher& launcher, std::string  terminal_cmd) :
    runner{runner}, launcher{launcher}, terminal_cmd{std::move(terminal_cmd)}
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

    MirInputEventModifiers mods = mir_keyboard_event_modifiers(kev);
    if (!(mods & mir_input_event_modifier_alt) || !(mods & mir_input_event_modifier_ctrl))
        return false;

    auto const key_code = mir_keyboard_event_key_code(kev);

    if (key_code == XKB_KEY_Delete && mir_keyboard_event_action(kev) == mir_keyboard_action_down)
    {
        shell_commands_active = !shell_commands_active;
        return true;
    }

    if (!shell_commands_active)
        return false;

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

    case XKB_KEY_T:
    case XKB_KEY_t:
        if (mir_keyboard_event_action(kev) != mir_keyboard_action_down)
            return false;
        launcher.launch({terminal_cmd});
        return true;

    case XKB_KEY_Left:
        wm->dock_active_window_left();
        return true;

    case XKB_KEY_Right:
        wm->dock_active_window_right();
        return true;

    case XKB_KEY_space:
        wm->toggle_maximized_restored();
        return true;

    case XKB_KEY_Up:
        wm->workspace_up(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_Down:
        wm->workspace_down(mods & mir_input_event_modifier_shift);
        return true;

    case XKB_KEY_bracketright:
        wm->focus_next_application();
        return true;

    case XKB_KEY_bracketleft:
        wm->focus_prev_application();
        return true;

    case XKB_KEY_braceright:
        wm->focus_next_within_application();
        return true;

    case XKB_KEY_braceleft:
        wm->focus_prev_within_application();
        return true;

    default:
        return false;
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
