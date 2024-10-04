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

#include "miriway_child_control.h"
#include "miriway_commands.h"

#include <miral/display_configuration_option.h>
#include <miral/keymap.h>
#include <miral/runner.h>
#include <miral/wayland_extensions.h>
#include <miral/minimal_window_manager.h>
#include <miral/set_window_management_policy.h>
#include <miral/x11_support.h>

using namespace miral;
using namespace miriway;

namespace
{
class MiralRunWM : public MinimalWindowManager
{
public:
    using MinimalWindowManager::MinimalWindowManager;

    void handle_request_move(WindowInfo& /*window_info*/, MirInputEvent const* /*input_event*/) override
    {
        // prevent window move
    }

    void handle_request_resize(WindowInfo& /*window_info*/, MirInputEvent const* /*input_event*/, MirResizeEdge /*edge*/) override
    {
        // prevent window resize
    }

    bool handle_pointer_event(MirPointerEvent const* event) override
    {
        // Override default handle_pointer_event() to ensure we don't allow dragging windows
        if (mir_pointer_event_action(event) == mir_pointer_action_button_down)
        {
            Point const cursor{
                mir_pointer_event_axis_value(event, mir_pointer_axis_x),
                mir_pointer_event_axis_value(event, mir_pointer_axis_y)};

            tools.select_active_window(tools.window_at(cursor));
        }
        return false;
    }
};
}

int main(int argc, char const* argv[])
{
    MirRunner runner{argc, argv};

    // Change the default Wayland extensions to enable everything
    miral::WaylandExtensions wayland_extensions;

    for (auto const& ext : wayland_extensions.all_supported())
        wayland_extensions.enable(ext);

    ChildControl child_control(runner);
    return runner.run_with(
        {
            set_window_management_policy<MiralRunWM>(),
            X11Support{},
            wayland_extensions,
            display_configuration_options,
            child_control,
            Keymap{},
        });
}
