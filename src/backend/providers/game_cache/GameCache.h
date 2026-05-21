// Pegasus Frontend
// Copyright (C) 2017-2024  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#pragma once

#include <QString>
#include <vector>

namespace model { class Collection; }
namespace model { class Game; }


namespace providers {
namespace game_cache {

class GameCache {
public:
    explicit GameCache(QString cache_path);

    bool exists() const;
    bool load(std::vector<model::Collection*>& collections,
              std::vector<model::Game*>& games);
    void save(const std::vector<model::Collection*>& collections,
              const std::vector<model::Game*>& games);
    void invalidate();

private:
    const QString m_cache_path;
};

} // namespace game_cache
} // namespace providers
