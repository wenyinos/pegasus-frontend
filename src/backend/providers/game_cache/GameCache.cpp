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
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


namespace {
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

QJsonObject assets_to_json(const model::Assets& assets)
{
    const auto& all = assets.allAssets();
    if (all.empty())
        return {};

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
    return obj;
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

QString game_unique_key(const model::Game* game)
{
    if (game->filesModel() && !game->filesModel()->entries().empty()) {
        return game->title() + QChar('\0')
            + game->filesModel()->entries().front()->fileinfo().absoluteFilePath();
    }
    return game->title();
}

QJsonObject game_to_json(const model::Game* game)
{
    QJsonObject obj;
    obj[QLatin1String("key")] = game_unique_key(game);
    obj[QLatin1String("title")] = game->title();
    obj[QLatin1String("sort_by")] = game->sortBy();
    obj[QLatin1String("summary")] = game->summary();
    obj[QLatin1String("description")] = game->description();
    obj[QLatin1String("developers")] = game->developerListConst().join(QLatin1Char(','));
    obj[QLatin1String("publishers")] = game->publisherListConst().join(QLatin1Char(','));
    obj[QLatin1String("genres")] = game->genreListConst().join(QLatin1Char(','));
    obj[QLatin1String("tags")] = game->tagListConst().join(QLatin1Char(','));
    obj[QLatin1String("player_count")] = game->playerCount();
    obj[QLatin1String("rating")] = static_cast<double>(game->rating());
    obj[QLatin1String("release_year")] = game->releaseYear();
    obj[QLatin1String("release_month")] = game->releaseMonth();
    obj[QLatin1String("release_day")] = game->releaseDay();
    obj[QLatin1String("favorite")] = game->isFavorite();
    obj[QLatin1String("launch_cmd")] = game->launchCmd();
    obj[QLatin1String("launch_workdir")] = game->launchWorkdir();
    obj[QLatin1String("launch_basedir")] = game->launchCmdBasedir();

    // Assets
    QJsonObject assets_obj = assets_to_json(game->assets());
    if (!assets_obj.isEmpty())
        obj[QLatin1String("assets")] = assets_obj;

    // Extra
    if (!game->extraMap().isEmpty())
        obj[QLatin1String("extra")] = QJsonObject::fromVariantMap(game->extraMap());

    // Files
    QJsonArray files_arr;
    if (game->filesModel()) {
        for (const model::GameFile* file : game->filesModel()->entries()) {
            QJsonObject file_obj;
            file_obj[QLatin1String("path")] = file->fileinfo().absoluteFilePath();
            file_obj[QLatin1String("name")] = file->name();
            file_obj[QLatin1String("uri")] = file->uri();
            files_arr.append(file_obj);
        }
    }
    obj[QLatin1String("files")] = files_arr;

    // Collection names
    QJsonArray coll_arr;
    if (game->collectionsModel()) {
        for (const model::Collection* coll : game->collectionsModel()->entries())
            coll_arr.append(coll->name());
    }
    obj[QLatin1String("collection_names")] = coll_arr;

    return obj;
}

QJsonObject collection_to_json(const model::Collection* coll)
{
    QJsonObject obj;
    obj[QLatin1String("name")] = coll->name();
    obj[QLatin1String("sort_by")] = coll->sortBy();
    obj[QLatin1String("short_name")] = coll->shortName();
    obj[QLatin1String("summary")] = coll->summary();
    obj[QLatin1String("description")] = coll->description();
    obj[QLatin1String("launch_cmd")] = coll->commonLaunchCmd();
    obj[QLatin1String("launch_workdir")] = coll->commonLaunchWorkdir();
    obj[QLatin1String("launch_basedir")] = coll->commonLaunchCmdBasedir();

    QJsonObject assets_obj = assets_to_json(coll->assets());
    if (!assets_obj.isEmpty())
        obj[QLatin1String("assets")] = assets_obj;

    if (!coll->extraMap().isEmpty())
        obj[QLatin1String("extra")] = QJsonObject::fromVariantMap(coll->extraMap());

    // Game keys
    QJsonArray game_keys;
    if (coll->gameList()) {
        for (const model::Game* game : coll->gameList()->entries())
            game_keys.append(game_unique_key(game));
    }
    obj[QLatin1String("game_keys")] = game_keys;

    return obj;
}

} // namespace


namespace providers {
namespace game_cache {

GameCache::GameCache(QString cache_path)
    : m_cache_path(std::move(cache_path))
{}

bool GameCache::exists() const
{
    return QFileInfo::exists(m_cache_path);
}

bool GameCache::load(std::vector<model::Collection*>& collections,
                     std::vector<model::Game*>& games)
{
    QFile file(m_cache_path);
    if (!file.open(QIODevice::ReadOnly)) {
        Log::warning(LOGMSG("GameCache"), LOGMSG("Could not open cache file: %1").arg(m_cache_path));
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        Log::warning(LOGMSG("GameCache"), LOGMSG("Cache parse error: %1").arg(parse_error.errorString()));
        return false;
    }

    const QJsonObject root = doc.object();

    // Load games first (collections reference game keys)
    QHash<QString, model::Game*> game_by_key;
    {
        const QJsonArray games_arr = root[QLatin1String("games")].toArray();
        for (const QJsonValue& val : games_arr) {
            const QJsonObject obj = val.toObject();

            auto* game = new model::Game();
            game->setTitle(obj[QLatin1String("title")].toString());
            game->setSortBy(obj[QLatin1String("sort_by")].toString());
            game->setSummary(obj[QLatin1String("summary")].toString());
            game->setDescription(obj[QLatin1String("description")].toString());
            game->developerList() = obj[QLatin1String("developers")].toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->publisherList() = obj[QLatin1String("publishers")].toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->genreList() = obj[QLatin1String("genres")].toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->tagList() = obj[QLatin1String("tags")].toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
            game->setPlayerCount(obj[QLatin1String("player_count")].toInt());
            game->setRating(static_cast<float>(obj[QLatin1String("rating")].toDouble()));

            const int year = obj[QLatin1String("release_year")].toInt();
            const int month = obj[QLatin1String("release_month")].toInt();
            const int day = obj[QLatin1String("release_day")].toInt();
            if (year > 0)
                game->setReleaseDate(QDate(year, month > 0 ? month : 1, day > 0 ? day : 1));

            game->setFavorite(obj[QLatin1String("favorite")].toBool());
            game->setLaunchCmd(obj[QLatin1String("launch_cmd")].toString());
            game->setLaunchWorkdir(obj[QLatin1String("launch_workdir")].toString());
            game->setLaunchCmdBasedir(obj[QLatin1String("launch_basedir")].toString());

            // Assets
            const QJsonObject assets_obj = obj[QLatin1String("assets")].toObject();
            if (!assets_obj.isEmpty())
                json_to_assets(assets_obj, game->assetsMut());

            // Extra
            const QJsonObject extra_obj = obj[QLatin1String("extra")].toObject();
            if (!extra_obj.isEmpty())
                game->extraMapMut() = extra_obj.toVariantMap();

            // Files
            const QJsonArray files_arr = obj[QLatin1String("files")].toArray();
            std::vector<model::GameFile*> file_entries;
            for (const QJsonValue& file_val : files_arr) {
                const QJsonObject file_obj = file_val.toObject();
                auto* gfile = new model::GameFile(file_obj[QLatin1String("path")].toString(), *game);
                gfile->setName(file_obj[QLatin1String("name")].toString());
                gfile->setUri(file_obj[QLatin1String("uri")].toString());
                file_entries.push_back(gfile);
            }
            game->setFiles(std::move(file_entries));

            const QString key = obj[QLatin1String("key")].toString();
            game_by_key[key] = game;
            games.push_back(game);
        }
    }

    // Build game→collections mapping from game data
    QHash<QString, std::vector<model::Game*>> coll_name_to_games;
    QHash<QString, std::vector<model::Collection*>> game_colls;  // game_key → collections
    {
        const QJsonArray games_arr = root[QLatin1String("games")].toArray();
        for (const QJsonValue& val : games_arr) {
            const QJsonObject obj = val.toObject();
            const QString key = obj[QLatin1String("key")].toString();
            auto game_it = game_by_key.find(key);
            if (game_it == game_by_key.end())
                continue;

            const QJsonArray coll_names = obj[QLatin1String("collection_names")].toArray();
            for (const QJsonValue& coll_val : coll_names)
                coll_name_to_games[coll_val.toString()].push_back(game_it.value());
        }
    }

    // Load collections, set games, build reverse mapping
    {
        const QJsonArray colls_arr = root[QLatin1String("collections")].toArray();
        for (const QJsonValue& val : colls_arr) {
            const QJsonObject obj = val.toObject();

            auto* coll = new model::Collection(obj[QLatin1String("name")].toString());
            coll->setSortBy(obj[QLatin1String("sort_by")].toString());
            coll->setShortName(obj[QLatin1String("short_name")].toString());
            coll->setSummary(obj[QLatin1String("summary")].toString());
            coll->setDescription(obj[QLatin1String("description")].toString());
            coll->setCommonLaunchCmd(obj[QLatin1String("launch_cmd")].toString());
            coll->setCommonLaunchWorkdir(obj[QLatin1String("launch_workdir")].toString());
            coll->setCommonLaunchCmdBasedir(obj[QLatin1String("launch_basedir")].toString());

            const QJsonObject assets_obj = obj[QLatin1String("assets")].toObject();
            if (!assets_obj.isEmpty())
                json_to_assets(assets_obj, coll->assetsMut());

            const QJsonObject extra_obj = obj[QLatin1String("extra")].toObject();
            if (!extra_obj.isEmpty())
                coll->extraMapMut() = extra_obj.toVariantMap();

            // Set games on collection
            const QString coll_name = coll->name();
            auto games_it = coll_name_to_games.find(coll_name);
            if (games_it != coll_name_to_games.end()) {
                // Build reverse mapping: game_key → this collection
                for (model::Game* game : games_it.value()) {
                    const QString game_key = game_unique_key(game);
                    game_colls[game_key].push_back(coll);
                }
                coll->setGames(std::move(games_it.value()));
            }

            collections.push_back(coll);
        }
    }

    // Set collections on games (reverse mapping via setCollections)
    for (auto it = game_colls.begin(); it != game_colls.end(); ++it) {
        auto game_it = game_by_key.find(it.key());
        if (game_it != game_by_key.end())
            game_it.value()->setCollections(std::move(it.value()));
    }

    Log::info(LOGMSG("GameCache"), LOGMSG("Loaded %1 collections and %2 games from cache")
        .arg(collections.size()).arg(games.size()));

    return true;
}

void GameCache::save(const std::vector<model::Collection*>& collections,
                     const std::vector<model::Game*>& games)
{
    QJsonObject root;

    // Save games
    QJsonArray games_arr;
    for (const model::Game* game : games)
        games_arr.append(game_to_json(game));
    root[QLatin1String("games")] = games_arr;

    // Save collections
    QJsonArray colls_arr;
    for (const model::Collection* coll : collections)
        colls_arr.append(collection_to_json(coll));
    root[QLatin1String("collections")] = colls_arr;

    // Write to file
    QDir().mkpath(QFileInfo(m_cache_path).absolutePath());

    QFile file(m_cache_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Log::warning(LOGMSG("GameCache"), LOGMSG("Could not write cache file: %1").arg(m_cache_path));
        return;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();

    Log::info(LOGMSG("GameCache"), LOGMSG("Saved %1 collections and %2 games to cache")
        .arg(collections.size()).arg(games.size()));
}

void GameCache::invalidate()
{
    QFile::remove(m_cache_path);
}

} // namespace game_cache
} // namespace providers
