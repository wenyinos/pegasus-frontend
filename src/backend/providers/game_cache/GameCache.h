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
#include <QStringList>
#include <vector>

namespace model { class Collection; }
namespace model { class Game; }


namespace providers {
namespace game_cache {

class GameCache {
public:
    explicit GameCache(QString db_path);

    bool isCacheValid(const QStringList& game_dirs, const QString& config_hash) const;
    bool load(std::vector<model::Collection*>& collections,
              std::vector<model::Game*>& games);
    void save(const std::vector<model::Collection*>& collections,
              const std::vector<model::Game*>& games,
              const QStringList& game_dirs,
              const QString& config_hash);
    void invalidate();

private:
    const QString m_db_path;

    bool createTables();
    bool checkDirectoryFingerprint(const QStringList& game_dirs, const QString& config_hash) const;
    void saveDirectoryFingerprint(const QStringList& game_dirs, const QString& config_hash);
};

} // namespace game_cache
} // namespace providers
