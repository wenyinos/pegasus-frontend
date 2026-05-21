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


#include "GameCache.h"

#include "Log.h"
#include "Paths.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "utils/SqliteDb.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>


namespace {
constexpr const char* CONFIG_HASH_KEY = "__config_hash__";

void print_query_error(const QString& log_tag, const QSqlQuery& query)
{
    const auto error = query.lastError();
    if (error.isValid())
        Log::warning(log_tag, error.text());
}

QString asset_type_to_string(AssetType type)
{
    switch (type) {
        case AssetType::BOX_FRONT: return QStringLiteral("boxFront");
        case AssetType::BOX_BACK: return QStringLiteral("boxBack");
        case AssetType::BOX_SPINE: return QStringLiteral("boxSpine");
        case AssetType::BOX_FULL: return QStringLiteral("boxFull");
        case AssetType::CARTRIDGE: return QStringLiteral("cartridge");
        case AssetType::LOGO: return QStringLiteral("logo");
        case AssetType::POSTER: return QStringLiteral("poster");
        case AssetType::ARCADE_MARQUEE: return QStringLiteral("marquee");
        case AssetType::ARCADE_BEZEL: return QStringLiteral("bezel");
        case AssetType::ARCADE_PANEL: return QStringLiteral("panel");
        case AssetType::ARCADE_CABINET_L: return QStringLiteral("cabinetLeft");
        case AssetType::ARCADE_CABINET_R: return QStringLiteral("cabinetRight");
        case AssetType::UI_TILE: return QStringLiteral("tile");
        case AssetType::UI_BANNER: return QStringLiteral("banner");
        case AssetType::UI_STEAMGRID: return QStringLiteral("steam");
        case AssetType::BACKGROUND: return QStringLiteral("background");
        case AssetType::MUSIC: return QStringLiteral("music");
        case AssetType::SCREENSHOT: return QStringLiteral("screenshot");
        case AssetType::TITLESCREEN: return QStringLiteral("titlescreen");
        case AssetType::VIDEO: return QStringLiteral("video");
        default: return QString();
    }
}

AssetType string_to_asset_type(const QString& str)
{
    if (str == QLatin1String("boxFront")) return AssetType::BOX_FRONT;
    if (str == QLatin1String("boxBack")) return AssetType::BOX_BACK;
    if (str == QLatin1String("boxSpine")) return AssetType::BOX_SPINE;
    if (str == QLatin1String("boxFull")) return AssetType::BOX_FULL;
    if (str == QLatin1String("cartridge")) return AssetType::CARTRIDGE;
    if (str == QLatin1String("logo")) return AssetType::LOGO;
    if (str == QLatin1String("poster")) return AssetType::POSTER;
    if (str == QLatin1String("marquee")) return AssetType::ARCADE_MARQUEE;
    if (str == QLatin1String("bezel")) return AssetType::ARCADE_BEZEL;
    if (str == QLatin1String("panel")) return AssetType::ARCADE_PANEL;
    if (str == QLatin1String("cabinetLeft")) return AssetType::ARCADE_CABINET_L;
    if (str == QLatin1String("cabinetRight")) return AssetType::ARCADE_CABINET_R;
    if (str == QLatin1String("tile")) return AssetType::UI_TILE;
    if (str == QLatin1String("banner")) return AssetType::UI_BANNER;
    if (str == QLatin1String("steam")) return AssetType::UI_STEAMGRID;
    if (str == QLatin1String("background")) return AssetType::BACKGROUND;
    if (str == QLatin1String("music")) return AssetType::MUSIC;
    if (str == QLatin1String("screenshot")) return AssetType::SCREENSHOT;
    if (str == QLatin1String("titlescreen")) return AssetType::TITLESCREEN;
    if (str == QLatin1String("video")) return AssetType::VIDEO;
    return AssetType::UNKNOWN;
}

QString assets_to_json(const model::Assets& assets)
{
    const auto& all = assets.allAssets();
    if (all.empty())
        return QString();

    QJsonObject obj;
    for (const auto& pair : all) {
        const QString key = asset_type_to_string(pair.first);
        if (key.isEmpty() || pair.second.isEmpty())
            continue;
        QJsonArray arr;
        for (const QString& uri : pair.second)
            arr.append(uri);
        obj[key] = arr;
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void json_to_assets(const QJsonObject& obj, model::Assets& assets)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const AssetType type = string_to_asset_type(it.key());
        if (type == AssetType::UNKNOWN)
            continue;
        const QJsonArray arr = it.value().toArray();
        for (const QJsonValue& val : arr)
            assets.add_uri(type, val.toString());
    }
}

// Unique key for a game: title + first file path to handle duplicate titles
QString game_unique_key(const model::Game* game)
{
    if (game->filesModel() && !game->filesModel()->entries().empty()) {
        return game->title() + QChar('\0')
            + game->filesModel()->entries().front()->fileinfo().absoluteFilePath();
    }
    return game->title();
}

} // namespace


namespace providers {
namespace game_cache {

GameCache::GameCache(QString db_path)
    : m_db_path(std::move(db_path))
{}

bool GameCache::createTables()
{
    SqliteDb db(m_db_path);
    if (!db.open()) {
        Log::warning(LOGMSG("Could not open game cache database"));
        return false;
    }

    if (!db.hasTable(QStringLiteral("collections"))) {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "CREATE TABLE collections"
              "(" "id INTEGER PRIMARY KEY"
              "," "name TEXT UNIQUE NOT NULL"
              "," "sort_by TEXT"
              "," "short_name TEXT"
              "," "summary TEXT"
              "," "description TEXT"
              "," "launch_cmd TEXT"
              "," "launch_workdir TEXT"
              "," "launch_basedir TEXT"
              "," "assets_json TEXT"
              "," "extra_json TEXT"
            ");"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }
    }

    if (!db.hasTable(QStringLiteral("games"))) {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "CREATE TABLE games"
              "(" "id INTEGER PRIMARY KEY"
              "," "title TEXT NOT NULL"
              "," "sort_by TEXT"
              "," "summary TEXT"
              "," "description TEXT"
              "," "developers TEXT"
              "," "publishers TEXT"
              "," "genres TEXT"
              "," "tags TEXT"
              "," "player_count INTEGER"
              "," "rating REAL"
              "," "release_year INTEGER"
              "," "release_month INTEGER"
              "," "release_day INTEGER"
              "," "favorite INTEGER DEFAULT 0"
              "," "launch_cmd TEXT"
              "," "launch_workdir TEXT"
              "," "launch_basedir TEXT"
              "," "assets_json TEXT"
              "," "extra_json TEXT"
            ");"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }
    }

    if (!db.hasTable(QStringLiteral("game_files"))) {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "CREATE TABLE game_files"
              "(" "id INTEGER PRIMARY KEY"
              "," "game_id INTEGER NOT NULL REFERENCES games(id)"
              "," "path TEXT"
              "," "name TEXT"
              "," "uri TEXT"
            ");"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }
    }

    if (!db.hasTable(QStringLiteral("collection_games"))) {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "CREATE TABLE collection_games"
              "(" "collection_id INTEGER NOT NULL REFERENCES collections(id)"
              "," "game_id INTEGER NOT NULL REFERENCES games(id)"
              "," "PRIMARY KEY (collection_id, game_id)"
            ");"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }
    }

    if (!db.hasTable(QStringLiteral("dir_fingerprints"))) {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "CREATE TABLE dir_fingerprints"
              "(" "dir_path TEXT PRIMARY KEY"
              "," "last_modified INTEGER NOT NULL"
            ");"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }
    }

    return true;
}

bool GameCache::checkDirectoryFingerprint(const QStringList& game_dirs, const QString& config_hash) const
{
    SqliteDb db(m_db_path);
    if (!db.open())
        return false;

    if (!db.hasTable(QStringLiteral("dir_fingerprints")))
        return false;

    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT dir_path, last_modified FROM dir_fingerprints"));
    if (!query.exec()) {
        print_query_error(LOGMSG("GameCache"), query);
        return false;
    }

    QHash<QString, qint64> cached_fingerprints;
    QString cached_config_hash;
    while (query.next()) {
        const QString key = query.value(0).toString();
        if (key == QLatin1String(CONFIG_HASH_KEY)) {
            cached_config_hash = QString::number(query.value(1).toLongLong());
        } else {
            cached_fingerprints[key] = query.value(1).toLongLong();
        }
    }

    // Check config hash (game_dirs + providers)
    if (cached_config_hash != config_hash)
        return false;

    // Check if all game dirs are still valid
    for (const QString& dir : game_dirs) {
        QFileInfo dir_info(dir);
        if (!dir_info.exists() || !dir_info.isDir())
            return false;

        const qint64 current_mtime = dir_info.lastModified().toSecsSinceEpoch();
        const auto it = cached_fingerprints.find(dir);
        if (it == cached_fingerprints.end() || it.value() != current_mtime)
            return false;
    }

    return true;
}

void GameCache::saveDirectoryFingerprint(const QStringList& game_dirs, const QString& config_hash)
{
    SqliteDb db(m_db_path);
    if (!db.open())
        return;

    db.startTransaction();

    QSqlQuery delete_query;
    delete_query.prepare(QStringLiteral("DELETE FROM dir_fingerprints"));
    if (!delete_query.exec()) {
        print_query_error(LOGMSG("GameCache"), delete_query);
        db.rollback();
        return;
    }

    QSqlQuery insert_query;
    insert_query.prepare(QStringLiteral(
        "INSERT INTO dir_fingerprints (dir_path, last_modified) VALUES (?, ?)"
    ));

    // Save config hash
    insert_query.addBindValue(QStringLiteral(CONFIG_HASH_KEY));
    insert_query.addBindValue(config_hash.toLongLong());
    if (!insert_query.exec()) {
        print_query_error(LOGMSG("GameCache"), insert_query);
        db.rollback();
        return;
    }

    // Save directory fingerprints
    for (const QString& dir : game_dirs) {
        QFileInfo dir_info(dir);
        if (!dir_info.exists())
            continue;

        const qint64 mtime = dir_info.lastModified().toSecsSinceEpoch();
        insert_query.addBindValue(dir);
        insert_query.addBindValue(mtime);
        if (!insert_query.exec()) {
            print_query_error(LOGMSG("GameCache"), insert_query);
            db.rollback();
            return;
        }
    }

    db.commit();
}

bool GameCache::isCacheValid(const QStringList& game_dirs, const QString& config_hash) const
{
    if (!QFileInfo::exists(m_db_path))
        return false;

    return checkDirectoryFingerprint(game_dirs, config_hash);
}

bool GameCache::load(std::vector<model::Collection*>& collections,
                     std::vector<model::Game*>& games)
{
    SqliteDb db(m_db_path);
    if (!db.open()) {
        Log::warning(LOGMSG("Could not open game cache database"));
        return false;
    }

    // Load collections
    QHash<int, model::Collection*> collection_map;
    {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "SELECT id, name, sort_by, short_name, summary, description, "
            "launch_cmd, launch_workdir, launch_basedir, assets_json, extra_json "
            "FROM collections"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }

        while (query.next()) {
            auto* coll = new model::Collection(query.value(1).toString());
            coll->setSortBy(query.value(2).toString());
            coll->setShortName(query.value(3).toString());
            coll->setSummary(query.value(4).toString());
            coll->setDescription(query.value(5).toString());
            coll->setCommonLaunchCmd(query.value(6).toString());
            coll->setCommonLaunchWorkdir(query.value(7).toString());
            coll->setCommonLaunchCmdBasedir(query.value(8).toString());

            const QString assets_str = query.value(9).toString();
            if (!assets_str.isEmpty()) {
                const QJsonObject assets_obj = QJsonDocument::fromJson(assets_str.toUtf8()).object();
                json_to_assets(assets_obj, coll->assetsMut());
            }

            const QString extra_str = query.value(10).toString();
            if (!extra_str.isEmpty()) {
                const QJsonObject extra_obj = QJsonDocument::fromJson(extra_str.toUtf8()).object();
                coll->extraMapMut() = extra_obj.toVariantMap();
            }

            collection_map[query.value(0).toInt()] = coll;
            collections.push_back(coll);
        }
    }

    // Load games
    QHash<int, model::Game*> game_map;
    {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "SELECT id, title, sort_by, summary, description, "
            "developers, publishers, genres, tags, "
            "player_count, rating, release_year, release_month, release_day, "
            "favorite, launch_cmd, launch_workdir, launch_basedir, "
            "assets_json, extra_json "
            "FROM games"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }

        while (query.next()) {
            auto* game = new model::Game();
            game->setTitle(query.value(1).toString());
            game->setSortBy(query.value(2).toString());
            game->setSummary(query.value(3).toString());
            game->setDescription(query.value(4).toString());
            game->developerList() = query.value(5).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->publisherList() = query.value(6).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->genreList() = query.value(7).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->tagList() = query.value(8).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->setPlayerCount(query.value(9).toInt());
            game->setRating(query.value(10).toFloat());

            const int year = query.value(11).toInt();
            const int month = query.value(12).toInt();
            const int day = query.value(13).toInt();
            if (year > 0)
                game->setReleaseDate(QDate(year, month > 0 ? month : 1, day > 0 ? day : 1));

            game->setFavorite(query.value(14).toBool());
            game->setLaunchCmd(query.value(15).toString());
            game->setLaunchWorkdir(query.value(16).toString());
            game->setLaunchCmdBasedir(query.value(17).toString());

            const QString assets_str = query.value(18).toString();
            if (!assets_str.isEmpty()) {
                const QJsonObject assets_obj = QJsonDocument::fromJson(assets_str.toUtf8()).object();
                json_to_assets(assets_obj, game->assetsMut());
            }

            const QString extra_str = query.value(19).toString();
            if (!extra_str.isEmpty()) {
                const QJsonObject extra_obj = QJsonDocument::fromJson(extra_str.toUtf8()).object();
                game->extraMapMut() = extra_obj.toVariantMap();
            }

            game_map[query.value(0).toInt()] = game;
            games.push_back(game);
        }
    }

    // Load game files and group by game_id
    QHash<int, std::vector<model::GameFile*>> files_by_game;
    {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "SELECT game_id, path, name, uri FROM game_files"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }

        while (query.next()) {
            const int game_id = query.value(0).toInt();
            auto game_it = game_map.find(game_id);
            if (game_it == game_map.end())
                continue;

            const QString path = query.value(1).toString();
            auto* file = new model::GameFile(path, *game_it.value());
            file->setName(query.value(2).toString());
            file->setUri(query.value(3).toString());
            files_by_game[game_id].push_back(file);
        }
    }

    // Set files on games
    for (auto it = files_by_game.begin(); it != files_by_game.end(); ++it) {
        auto game_it = game_map.find(it.key());
        if (game_it == game_map.end())
            continue;
        game_it.value()->setFiles(std::move(it.value()));
    }

    // Load collection-game associations and build reverse mapping
    QHash<int, std::vector<model::Game*>> coll_games;
    QHash<int, std::vector<model::Collection*>> game_colls;
    {
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "SELECT collection_id, game_id FROM collection_games"
        ));
        if (!query.exec()) {
            print_query_error(LOGMSG("GameCache"), query);
            return false;
        }

        while (query.next()) {
            const int coll_id = query.value(0).toInt();
            const int game_id = query.value(1).toInt();

            auto game_it = game_map.find(game_id);
            if (game_it == game_map.end())
                continue;

            coll_games[coll_id].push_back(game_it.value());

            auto coll_it = collection_map.find(coll_id);
            if (coll_it != collection_map.end())
                game_colls[game_id].push_back(coll_it.value());
        }
    }

    // Set games on collections
    for (auto it = coll_games.begin(); it != coll_games.end(); ++it) {
        auto coll_it = collection_map.find(it.key());
        if (coll_it == collection_map.end())
            continue;
        coll_it.value()->setGames(std::move(it.value()));
    }

    // Set collections on games (reverse mapping)
    for (auto it = game_colls.begin(); it != game_colls.end(); ++it) {
        auto game_it = game_map.find(it.key());
        if (game_it == game_map.end())
            continue;
        game_it.value()->setCollections(std::move(it.value()));
    }

    Log::info(LOGMSG("GameCache"), LOGMSG("Loaded %1 collections and %2 games from cache")
        .arg(collections.size()).arg(games.size()));

    return true;
}

void GameCache::save(const std::vector<model::Collection*>& collections,
                     const std::vector<model::Game*>& games,
                     const QStringList& game_dirs,
                     const QString& config_hash)
{
    if (!createTables())
        return;

    SqliteDb db(m_db_path);
    if (!db.open()) {
        Log::warning(LOGMSG("Could not open game cache database"));
        return;
    }

    db.startTransaction();

    // Clear existing data
    QSqlQuery clear_query;
    clear_query.prepare(QStringLiteral("DELETE FROM collection_games"));
    if (!clear_query.exec()) {
        print_query_error(LOGMSG("GameCache"), clear_query);
        db.rollback();
        return;
    }
    clear_query.prepare(QStringLiteral("DELETE FROM game_files"));
    if (!clear_query.exec()) {
        print_query_error(LOGMSG("GameCache"), clear_query);
        db.rollback();
        return;
    }
    clear_query.prepare(QStringLiteral("DELETE FROM games"));
    if (!clear_query.exec()) {
        print_query_error(LOGMSG("GameCache"), clear_query);
        db.rollback();
        return;
    }
    clear_query.prepare(QStringLiteral("DELETE FROM collections"));
    if (!clear_query.exec()) {
        print_query_error(LOGMSG("GameCache"), clear_query);
        db.rollback();
        return;
    }

    // Save collections (INSERT OR REPLACE handles duplicate names)
    QHash<QString, int> collection_ids;
    {
        QSqlQuery insert_query;
        insert_query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO collections (name, sort_by, short_name, summary, description, "
            "launch_cmd, launch_workdir, launch_basedir, assets_json, extra_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        ));

        for (const model::Collection* coll : collections) {
            insert_query.addBindValue(coll->name());
            insert_query.addBindValue(coll->sortBy());
            insert_query.addBindValue(coll->shortName());
            insert_query.addBindValue(coll->summary());
            insert_query.addBindValue(coll->description());
            insert_query.addBindValue(coll->commonLaunchCmd());
            insert_query.addBindValue(coll->commonLaunchWorkdir());
            insert_query.addBindValue(coll->commonLaunchCmdBasedir());
            insert_query.addBindValue(assets_to_json(coll->assets()));

            if (!coll->extraMap().isEmpty()) {
                QJsonObject extra_obj = QJsonObject::fromVariantMap(coll->extraMap());
                insert_query.addBindValue(QString::fromUtf8(QJsonDocument(extra_obj).toJson(QJsonDocument::Compact)));
            } else {
                insert_query.addBindValue(QString());
            }

            if (!insert_query.exec()) {
                print_query_error(LOGMSG("GameCache"), insert_query);
                db.rollback();
                return;
            }

            collection_ids[coll->name()] = insert_query.lastInsertId().toInt();
        }
    }

    // Save games (use unique key to handle duplicate titles)
    QHash<QString, int> game_ids;
    {
        QSqlQuery insert_query;
        insert_query.prepare(QStringLiteral(
            "INSERT INTO games (title, sort_by, summary, description, "
            "developers, publishers, genres, tags, "
            "player_count, rating, release_year, release_month, release_day, "
            "favorite, launch_cmd, launch_workdir, launch_basedir, "
            "assets_json, extra_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        ));

        for (const model::Game* game : games) {
            insert_query.addBindValue(game->title());
            insert_query.addBindValue(game->sortBy());
            insert_query.addBindValue(game->summary());
            insert_query.addBindValue(game->description());
            insert_query.addBindValue(game->developerListConst().join(QLatin1Char(',')));
            insert_query.addBindValue(game->publisherListConst().join(QLatin1Char(',')));
            insert_query.addBindValue(game->genreListConst().join(QLatin1Char(',')));
            insert_query.addBindValue(game->tagListConst().join(QLatin1Char(',')));
            insert_query.addBindValue(game->playerCount());
            insert_query.addBindValue(game->rating());
            insert_query.addBindValue(game->releaseYear());
            insert_query.addBindValue(game->releaseMonth());
            insert_query.addBindValue(game->releaseDay());
            insert_query.addBindValue(game->isFavorite() ? 1 : 0);
            insert_query.addBindValue(game->launchCmd());
            insert_query.addBindValue(game->launchWorkdir());
            insert_query.addBindValue(game->launchCmdBasedir());
            insert_query.addBindValue(assets_to_json(game->assets()));

            if (!game->extraMap().isEmpty()) {
                QJsonObject extra_obj = QJsonObject::fromVariantMap(game->extraMap());
                insert_query.addBindValue(QString::fromUtf8(QJsonDocument(extra_obj).toJson(QJsonDocument::Compact)));
            } else {
                insert_query.addBindValue(QString());
            }

            if (!insert_query.exec()) {
                print_query_error(LOGMSG("GameCache"), insert_query);
                db.rollback();
                return;
            }

            game_ids[game_unique_key(game)] = insert_query.lastInsertId().toInt();
        }
    }

    // Save game files
    {
        QSqlQuery insert_query;
        insert_query.prepare(QStringLiteral(
            "INSERT INTO game_files (game_id, path, name, uri) VALUES (?, ?, ?, ?)"
        ));

        for (const model::Game* game : games) {
            auto game_it = game_ids.find(game_unique_key(game));
            if (game_it == game_ids.end())
                continue;

            if (!game->filesModel())
                continue;

            for (const model::GameFile* file : game->filesModel()->entries()) {
                insert_query.addBindValue(game_it.value());
                insert_query.addBindValue(file->fileinfo().absoluteFilePath());
                insert_query.addBindValue(file->name());
                insert_query.addBindValue(file->uri());

                if (!insert_query.exec()) {
                    print_query_error(LOGMSG("GameCache"), insert_query);
                    db.rollback();
                    return;
                }
            }
        }
    }

    // Save collection-game associations
    {
        QSqlQuery insert_query;
        insert_query.prepare(QStringLiteral(
            "INSERT INTO collection_games (collection_id, game_id) VALUES (?, ?)"
        ));

        for (const model::Collection* coll : collections) {
            auto coll_it = collection_ids.find(coll->name());
            if (coll_it == collection_ids.end())
                continue;

            if (!coll->gameList())
                continue;

            for (const model::Game* game : coll->gameList()->entries()) {
                auto game_it = game_ids.find(game_unique_key(game));
                if (game_it == game_ids.end())
                    continue;

                insert_query.addBindValue(coll_it.value());
                insert_query.addBindValue(game_it.value());

                if (!insert_query.exec()) {
                    print_query_error(LOGMSG("GameCache"), insert_query);
                    db.rollback();
                    return;
                }
            }
        }
    }

    db.commit();

    // Save directory fingerprints and config hash
    saveDirectoryFingerprint(game_dirs, config_hash);

    Log::info(LOGMSG("GameCache"), LOGMSG("Saved %1 collections and %2 games to cache")
        .arg(collections.size()).arg(games.size()));
}

void GameCache::invalidate()
{
    QFile::remove(m_db_path);
}

} // namespace game_cache
} // namespace providers
