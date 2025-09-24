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

#ifndef MIRIWAY_MIRIWAY_MAGNIFIER_H
#define MIRIWAY_MIRIWAY_MAGNIFIER_H

#include <miral/append_keyboard_event_filter.h>
#include <miral/magnifier.h>

namespace miriway
{
class Magnifier : miral::Magnifier
{
    miral::AppendKeyboardEventFilter magnifier_filter{[this](auto... args) { return check_on_off(args...); }};

    bool check_on_off(MirKeyboardEvent const* key_event);
public:
    using miral::Magnifier::Magnifier;

    void operator()(mir::Server& server)
    {
        miral::Magnifier::operator()(server);
        magnifier_filter(server);
    }
};
}

#endif //MIRIWAY_MIRIWAY_MAGNIFIER_H