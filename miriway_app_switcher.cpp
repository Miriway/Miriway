/*
 * Copyright © 2024 Octopull Ltd.
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

#include "miriway_app_switcher.h"

#include <mir_toolkit/events/enums.h>
#include <miral/application_switcher.h>
#include <miral/toolkit_event.h>
#include <miral/internal_client.h>

#include <linux/input-event-codes.h>

#include <atomic>

using namespace miral::toolkit;


class miriway::AppSwitcher::Self
{
public:
    struct KeybindConfiguration
    {
        /// This is the modifier that must be held for any application switcher
        /// action to begin.
        ///
        /// Defaults to #mir_input_event_modifier_alt.
        MirInputEventModifier primary_modifier = mir_input_event_modifier_alt;

        /// This is the modifier that must be held alongside the primary modifier
        /// in order to reverse the direction of an action being run. For example,
        /// holding the primary modifier and hitting the application key will select
        /// the next application, while holding both the primary and reverse modifiers
        /// and hitting the application will select the previous application.
        ///
        /// Defaults to #mir_input_event_modifier_shift.
        MirInputEventModifier reverse_modifier = mir_input_event_modifier_shift;

        /// This is the key that must be clicked in order to trigger either
        /// #ApplicationSwitcher::next_app or #ApplicationSwitcher::prev_app,
        /// depending on the modifiers that are currently being held.
        ///
        /// Defaults to KEY_TAB.
        int application_key = KEY_TAB;

        /// This is the key that must be clicked in order to trigger either
        /// #ApplicationSwitcher::next_window or #ApplicationSwitcher::prev_window,
        /// depending on the modifiers that are currently being held.
        ///
        /// Defaults to KEY_GRAVE.
        int window_key = KEY_GRAVE;

        /// This is the key that must be clicked in order cancel the
        ///#ApplicationSwitcher
        ///
        /// Defaults to KEY_ESC.
        int escape_key = KEY_ESC;
    };

    explicit Self(KeybindConfiguration const& keybind_configuration)
        : keybind_configuration{keybind_configuration},
        startup_internal_client{switcher, switcher, [switcher=switcher] { switcher.stop(); }}
    {
    }

    void operator()(mir::Server& server)
    {
        startup_internal_client(server);
    }

    bool process_event(MirEvent const* event)
    {
        if (mir_event_get_type(event) != mir_event_type_input)
            return false;

        auto const* input_event = mir_event_get_input_event(event);

        if (mir_input_event_get_type(input_event) != mir_input_event_type_key)
            return false;

        auto const key_event = mir_input_event_get_keyboard_event(input_event);
        auto const modifiers = mir_keyboard_event_modifiers(key_event);

        if (mir_keyboard_event_action(key_event) == mir_keyboard_action_down)
        {
            auto const scancode = mir_keyboard_event_scan_code(key_event);
            if (is_running && scancode == keybind_configuration.escape_key)
            {
                switcher.cancel();
                is_running = false;
                return true;
            }
            if (modifiers & keybind_configuration.primary_modifier)
            {
                if (modifiers & keybind_configuration.reverse_modifier)
                {
                    if (scancode == keybind_configuration.application_key)
                    {
                        switcher.prev_app();
                        is_running = true;
                        return true;
                    }
                    else if (scancode == keybind_configuration.window_key)
                    {
                        switcher.prev_window();
                        is_running = true;
                        return true;
                    }
                }
                else
                {
                    if (scancode == keybind_configuration.application_key)
                    {
                        switcher.next_app();
                        is_running = true;
                        return true;
                    }
                    else if (scancode == keybind_configuration.window_key)
                    {
                        switcher.next_window();
                        is_running = true;
                        return true;
                    }
                }
            }
        }
        else if (is_running && miral::toolkit::mir_keyboard_event_action(key_event) == mir_keyboard_action_up)
        {
            if (!(modifiers & keybind_configuration.primary_modifier))
            {
                switcher.confirm();
                is_running = false;
                return true;
            }
        }

        return false;
     }

    const KeybindConfiguration keybind_configuration;
    miral::ApplicationSwitcher switcher;
    miral::StartupInternalClient startup_internal_client;

    std::atomic<bool> is_running = false;
};

miriway::AppSwitcher::AppSwitcher() :
    self{std::make_shared<Self>(Self::KeybindConfiguration{
        .primary_modifier = mir_input_event_modifier_alt,
        .reverse_modifier = mir_input_event_modifier_shift,
        .application_key = KEY_TAB,
        .window_key = KEY_GRAVE,
        .escape_key = KEY_ESC
    })}
{
}

void miriway::AppSwitcher::operator()(mir::Server& server)
{
    self->operator()(server);
}

bool miriway::AppSwitcher::process_event(MirEvent const* event)
{
    return self->process_event(event);
}
