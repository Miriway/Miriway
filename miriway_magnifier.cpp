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

#include "miriway_magnifier.h"

#include <miral/toolkit_event.h>

using namespace miral::toolkit;

bool miriway::Magnifier::check_on_off(MirKeyboardEvent const* key_event)
{
    if (mir_keyboard_event_action(key_event) != mir_keyboard_action_down)
        return false;

    if (mir_keyboard_event_modifiers(key_event) & mir_input_event_modifier_meta)
    {
        // Turn on/off the magnifier Meta+{Plus,Equal}/Meta+Escape
        switch (mir_keyboard_event_keysym(key_event))
        {
        case XKB_KEY_plus:
        case XKB_KEY_equal:
            enable(true);
            return true;
        case XKB_KEY_Escape:
            enable(false);
            return true;
        default:
            return false;
        }
    }

    return false;
}

void miriway::Magnifier::operator()(mir::Server& server)
{
    miral::Magnifier::operator()(server);
    magnifier_filter(server);
}
