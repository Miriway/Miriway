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

#include "miriway_documenting_store.h"
#include <iostream>

miriway::DocumentingStore::DocumentingStore(Store& underlying, std::filesystem::path target) :
    underlying{underlying}
{
    if (std::ifstream settings{target})
    {
        std::string line;
        while (std::getline(settings, line))
        {
            if (line.empty()) continue;

            if (line[0] != '#')
            {
                if (auto const eq = line.find('='); eq != std::string::npos)
                    existing_keys.insert(line.substr(0, eq));
            }
            else
            {
                // Also track commented out keys to avoid duplicating them
                auto const content = line.substr(1);
                if (auto const eq = content.find('='); eq != std::string::npos)
                {
                    existing_keys.insert(content.substr(0, eq));
                }
            }
        }
        doc.open(target, std::ios::app);
    }
    else
    {
        doc.open(target);
    }
}


void miriway::DocumentingStore::add_int_attribute(const live_config::Key& key, std::string_view description, HandleInt handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# int: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_int_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_ints_attribute(const live_config::Key& key, std::string_view description, HandleInts handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# int[]: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_ints_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_bool_attribute(const live_config::Key& key, std::string_view description, HandleBool handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# bool: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_bool_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_float_attribute(const live_config::Key& key, std::string_view description, HandleFloat handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# float: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_float_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_floats_attribute(const live_config::Key& key, std::string_view description, HandleFloats handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# float[]: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_floats_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_string_attribute(const live_config::Key& key, std::string_view description, HandleString handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# string: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_string_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_strings_attribute(const live_config::Key& key, std::string_view description,
    HandleStrings handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# string[]: " << description << '\n';
        doc << "#" << key.to_string() << "=\n\n";
        doc.flush();
    }
    underlying.add_strings_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_int_attribute(const live_config::Key& key, std::string_view description, int preset,
    HandleInt handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# int: " << description << '\n';
        doc << "#" << key.to_string() << '=' << preset << "\n\n";
        doc.flush();
    }
    underlying.add_int_attribute(key, description, handler);
}

void miriway::DocumentingStore::add_ints_attribute(const live_config::Key& key, std::string_view description,
    std::span<int const> preset, HandleInts handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# int[]: " << description << '\n';
        for (auto const& p : preset)
            doc << "#" << key.to_string() << '=' << p << '\n';
        doc << '\n';
        doc.flush();
    }
    underlying.add_ints_attribute(key, description, preset, handler);
}

void miriway::DocumentingStore::add_bool_attribute(const live_config::Key& key, std::string_view description, bool preset,
    HandleBool handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# bool: " << description << '\n';
        doc << "#" << key.to_string() << '=' << (preset ? "true" : "false") << "\n\n";
        doc.flush();
    }
    underlying.add_bool_attribute(key, description, preset, handler);
}

void miriway::DocumentingStore::add_float_attribute(const live_config::Key& key, std::string_view description, float preset,
    HandleFloat handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# float: " << description << '\n';
        doc << "#" << key.to_string() << '=' << preset << "\n\n";
        doc.flush();
    }
    underlying.add_float_attribute(key, description, preset, handler);
}

void miriway::DocumentingStore::add_floats_attribute(const live_config::Key& key, std::string_view description,
    std::span<float const> preset, HandleFloats handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# float[]: " << description << '\n';
        for (auto const& p : preset)
            doc << "#" << key.to_string() << '=' << p << '\n';
        doc << '\n';
        doc.flush();
    }
    underlying.add_floats_attribute(key, description, preset, handler);
}

void miriway::DocumentingStore::add_string_attribute(const live_config::Key& key, std::string_view description,
    std::string_view preset, HandleString handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# string: " << description << '\n';
        doc << "#" << key.to_string() << '=' << preset << "\n\n";
        doc.flush();
    }
    underlying.add_string_attribute(key, description, preset, handler);
}

void miriway::DocumentingStore::add_strings_attribute(const live_config::Key& key, std::string_view description,
    std::span<std::string const> preset, HandleStrings handler)
{
    if (!existing_keys.count(key.to_string()))
    {
        doc << "# string[]: " << description << '\n';
        for (auto const& p : preset)
            doc << "#" << key.to_string() << '=' << p << '\n';
        doc << '\n';
        doc.flush();
    }
    underlying.add_strings_attribute(key, description, preset, handler);
}

void miriway::DocumentingStore::on_done(HandleDone handler)
{
    underlying.on_done(handler);
}

