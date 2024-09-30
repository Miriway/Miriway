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

#ifndef MIRIWAY_CHILD_CONTROL_H
#define MIRIWAY_CHILD_CONTROL_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mir { class Server; }
namespace miral { class MirRunner; class WaylandExtensions; }

namespace miriway
{
using namespace miral;

class ChildControl
{
public:
    explicit ChildControl(MirRunner& runner);

    void operator()(mir::Server& server);

    void launch_shell(std::vector<std::string> const& cmd);
    void launch_shell(std::vector<std::string> const& cmd, std::function<bool()> const should_restart_predicate);
    void run_shell(std::vector<std::string> const& cmd);
    void run_app(std::vector<std::string> const& cmd);
    void enable_for_shell(WaylandExtensions& extensions, std::string const& protocol);

private:
    class Self;
    std::shared_ptr<Self> self;
};
}

#endif //MIRIWAY_CHILD_CONTROL_H
