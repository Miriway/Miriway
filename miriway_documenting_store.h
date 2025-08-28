/*
 * Copyright © Alan Griffiths <alan@octopull.co.uk>
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

#ifndef MIRIWAY_MIRIWAY_DOCUMENTING_STORE_H
#define MIRIWAY_MIRIWAY_DOCUMENTING_STORE_H

#include <miral/live_config.h>

#include <fstream>
#include <filesystem>

namespace miriway
{
using namespace miral;

/// Documents the available options
class DocumentingStore : public live_config::Store
{
public:
    explicit DocumentingStore(Store& underlying, std::filesystem::path target) :
        underlying{underlying},
        doc{exists(target) ? std::ofstream{} : std::ofstream(target)}
    {
    }

    void add_int_attribute(const live_config::Key& key, std::string_view description, HandleInt handler) override;
    void add_ints_attribute(const live_config::Key& key, std::string_view description, HandleInts handler) override;
    void add_bool_attribute(const live_config::Key& key, std::string_view description, HandleBool handler) override;
    void add_float_attribute(const live_config::Key& key, std::string_view description, HandleFloat handler) override;
    void add_floats_attribute(const live_config::Key& key, std::string_view description, HandleFloats handler) override;
    void add_string_attribute(const live_config::Key& key, std::string_view description, HandleString handler) override;
    void add_strings_attribute(const live_config::Key& key, std::string_view description,
        HandleStrings handler) override;
    void add_int_attribute(const live_config::Key& key, std::string_view description, int preset,
        HandleInt handler) override;
    void add_ints_attribute(const live_config::Key& key, std::string_view description, std::span<int const> preset,
        HandleInts handler) override;
    void add_bool_attribute(const live_config::Key& key, std::string_view description, bool preset,
        HandleBool handler) override;
    void add_float_attribute(const live_config::Key& key, std::string_view description, float preset,
        HandleFloat handler) override;
    void add_floats_attribute(const live_config::Key& key, std::string_view description, std::span<float const> preset,
        HandleFloats handler) override;
    void add_string_attribute(const live_config::Key& key, std::string_view description, std::string_view preset,
        HandleString handler) override;
    void add_strings_attribute(const live_config::Key& key, std::string_view description,
        std::span<std::string const> preset, HandleStrings handler) override;
    void on_done(HandleDone handler) override;

private:
    Store& underlying;
    std::ofstream doc;
};
}

#endif //MIRIWAY_MIRIWAY_DOCUMENTING_STORE_H