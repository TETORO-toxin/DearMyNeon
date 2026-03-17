/*!
 * @file Map.cpp
 * @brief Map クラス実装：タイルマップの読み込み・描画・衝突判定など
 *
 * Map.h で宣言した API を実装します。CSV のロード、描画、衝突判定、
 * 歩行可能判定、保存処理などが含まれます。実装の多くは安全な境界チェック
 * を行った上で配列アクセスしています。
 */

#include "Map.h"
#include "Code/debug_utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <DxLib.h>
#include <cmath>
#include "game_manager.h"
#include "define.h"

Map::Map() : Map(DEFAULT_MAP_WIDTH, DEFAULT_MAP_HEIGHT, DEFAULT_TILE_SIZE) {}

Map::Map(int width_, int height_, int tileSize_)
    : width(width_), height(height_), tileSize(tileSize_),
      tileMap(height_, std::vector<int>(width_, -1)),
      collisionMap(height_, std::vector<CollisionType>(width_, CollisionType::None))
{
}

/**
 * @brief カンマ区切り文字列を分割してトークンのベクタを返す
 */
std::vector<std::string> Map::Split(const std::string& str, char delimiter) const {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        // Preserve empty tokens to keep CSV field positions stable
        result.push_back(token);
    }
    return result;
}

/**
 * @brief タイルIDマップを CSV から読み込む
 */
bool Map::LoadTileMapFromCSV(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Tile マップCSVを開けませんでした: " << filename << std::endl;
        return false;
    }
    std::string line;
    int y = 0;
    while (std::getline(ifs, line) && y < height) {
        std::stringstream ss(line);
        std::string token;
        int x = 0;
        while (std::getline(ss, token, ',') && x < width) {
            try {
                int v = token.empty() ? 0 : std::stoi(token);
                tileMap[y][x] = v;
            } catch (...) {
                tileMap[y][x] = -1;
            }
            ++x;
        }
        ++y;
    }
    return true;
}

/**
 * @brief 衝突情報マップを CSV から読み込む
 */
bool Map::LoadCollisionMapFromCSV(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Collision マップCSVを開けませんでした: " << filename << std::endl;
        return false;
    }
    std::string line;
    int y = 0;
    while (std::getline(ifs, line) && y < height) {
        std::stringstream ss(line);
        std::string token;
        int x = 0;
        while (std::getline(ss, token, ',') && x < width) {
            try {
                int v = token.empty() ? 0 : std::stoi(token);
                collisionMap[y][x] = static_cast<CollisionType>(v);
            } catch (...) {
                collisionMap[y][x] = CollisionType::None;
            }
            ++x;
        }
        ++y;
    }
    return true;
}

/**
 * @brief タイル/衝突の両方を CSV から読み込む（成功すれば true）
 */
bool Map::LoadFromCSV(const std::string& tileFile, const std::string& collisionFile) {
    bool t = LoadTileMapFromCSV(tileFile);
    bool c = LoadCollisionMapFromCSV(collisionFile);
    return t && c;
}

/**
 * @brief ルーム定義ファイルから rooms をロードする（未実装のスタブ）
 */
bool Map::LoadFromCSV(const std::string& roomFile) {
    std::ifstream ifs(roomFile);
    if (!ifs.is_open()) {
        // If no room file is present, treat as empty room set (not an error)
        rooms.clear();
        return true;
    }
    rooms.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        // Split by commas
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 5) continue; // malformed
        Room r;
        try {
            r.id = std::stoi(tokens[0]);
            r.xStart = std::stoi(tokens[1]);
            r.xEnd = std::stoi(tokens[2]);
            r.yStart = std::stoi(tokens[3]);
            r.yEnd = std::stoi(tokens[4]);
        } catch (...) {
            continue; // skip malformed line
        }
        if (tokens.size() >= 6) {
            // join remaining tokens in case text contained commas (saved earlier by replacing commas with ';')
            std::string txt = tokens[5];
            for (size_t i = 6; i < tokens.size(); ++i) {
                txt += ',';
                txt += tokens[i];
            }
            // Unescape semicolons back to commas
            for (auto &c : txt) if (c == ';') c = ',';
            r.tutorialText = txt;
        } else {
            r.tutorialText = "";
        }
        r.tutorialShown = false;
        // If file contains additional fields for overlay text/color/size parse them
        if (tokens.size() >= 7) {
            // tokens[6] expected to be overlayText (escaped with ';')
            std::string otxt = tokens[6];
            for (size_t i = 7; i < tokens.size(); ++i) {
                // if more tokens exist, they may belong to overlayText if original text contained commas
                // but since original saving writes overlayText then color then size, we attempt to find color/size at end
                break; // keep simple: overlayText handled if present as tokens[6]
            }
            for (auto &c : otxt) if (c == ';') c = ',';
            r.overlayText = otxt;
        } else {
            r.overlayText = "";
        }
        // parse overlayColor and overlayFontSize if present at end
        if (tokens.size() >= 9) {
            try { r.overlayColor = std::stoi(tokens[7]); } catch(...) { r.overlayColor = 0xFFFFFF; }
            try { r.overlayFontSize = std::stoi(tokens[8]); } catch(...) { r.overlayFontSize = 16; }
        }
        // parse optional tag field at index 9
        if (tokens.size() >= 10) {
            try { r.tag = std::stoi(tokens[9]); } catch(...) { r.tag = -1; }
        } else {
            r.tag = -1;
        }
         rooms.push_back(r);
    }
    return true;
}

/**
 * @brief 敵情報を CSV から読み込んで enemies を構築する
 */
bool Map::LoadEnemiesFromCSV(const std::string& filename, std::vector<Enemy>& enemies) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        return false;
    }
    enemies.clear();
    // 行ベースで安全にパースする方が汎用的
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        int xi = 0, yi = 0, t = 0;
        char comma = 0;
        if (!(ss >> xi)) continue;
        ss >> comma; // skip ','
        if (!(ss >> yi)) continue;
        ss >> comma;
        if (!(ss >> t)) continue;
        enemies.emplace_back(xi, yi, t);
    }
    return true;
}

/**
 * @brief 描画用のタイルハンドルを設定する
 */
void Map::setTileHandles(const std::vector<int>& handles) {
    tileHandles = handles;
}

const std::vector<int>& Map::GetTileHandles() const {
    return tileHandles;
}

/**
 * @brief 指定マップ座標にタイルを配置する（範囲チェックあり）
 */
void Map::placeTile(int mapX, int mapY, int tileID, CollisionType type) {
    if (mapX >= 0 && mapX < width && mapY >= 0 && mapY < height) {
        tileMap[mapY][mapX] = tileID;
        collisionMap[mapY][mapX] = type;
    } else {
        DEBUG_ONLY( DBG_PRINTF("placeTile: 範囲外 (%d,%d)\n", mapX, mapY); );
    }
}

/**
 * @brief 指定マップ座標のタイルを削除する
 */
void Map::removeTile(int mapX, int mapY) {
    if (mapX >= 0 && mapX < width && mapY >= 0 && mapY < height) {
        tileMap[mapY][mapX] = -1;
        collisionMap[mapY][mapX] = CollisionType::None;
    }
}

/**
 * @brief マップ全体をタイルハンドルで描画する
 */
void Map::draw() const {
    Camera& cam = GameManager::GetInstance().GetCamera();
    int camX = cam.GetOffsetX();
    int camY = cam.GetOffsetY();

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            int tileID = tileMap[row][col];
            if (tileID >= 0 && tileID < static_cast<int>(tileHandles.size())) {
                int drawX = col * tileSize - camX;
                int drawY = row * tileSize - camY;
                DrawGraph(drawX, drawY, tileHandles[tileID], TRUE);
            }
        }
    }
}

/**
 * @brief 部分的に Room のタイルを描画するためのヘルパ
 */
void Map::DrawTileMap(const Room& room, const std::vector<int>& tileImages) const {
    for (int row = 0; row < static_cast<int>(room.tileMap.size()); ++row) {
        for (int col = 0; col < static_cast<int>(room.tileMap[row].size()); ++col) {
            int tileID = room.tileMap[row][col];
            if (tileID >= 0 && tileID < static_cast<int>(tileImages.size())) {
                DrawGraph(room.xStart + col * tileSize, row * tileSize, tileImages[tileID], TRUE);
            }
        }
    }
}

/**
 * @brief 指定位置が床タイルならその上端 Y を返す（ワールド座標）
 */
int Map::GetFloorYAt(int mapX, int mapY) const {
    if (mapX < 0 || mapY < 0 || mapY >= height || mapX >= width) return -1;
    if (collisionMap[mapY][mapX] == CollisionType::Floor) {
        return mapY * tileSize; // タイルの上端 Y（ワールド座標）
    }
    return -1;
}

/**
 * @brief 指定タイルの衝突タイプを返す（範囲外は None）
 */
CollisionType Map::GetCollisionTypeAt(int x, int y) const {
    if (x < 0 || y < 0 || y >= static_cast<int>(collisionMap.size()) || x >= static_cast<int>(collisionMap[y].size()))
        return CollisionType::None;
    return collisionMap[y][x];
}

// 追加: タイルID取得
int Map::GetTileIdAt(int x, int y) const {
    if (x < 0 || y < 0 || y >= height || x >= width) return -1;
    return tileMap[y][x];
}

void Map::SetFloorSideTileIDs(const std::set<int>& ids) { floorSideTileIDs = ids; }
void Map::SetFloorTileIDs(const std::set<int>& ids) { floorTileIDs = ids; }
void Map::SetWallTileIDs(const std::set<int>& ids) { wallTileIDs = ids; }

/**
 * @brief 整地（歩行可能）判定。tile 単位で与えられる位置に対して次の y が許容されるかを返す
 */
bool Map::IsWalkable(int mapX, int mapY, float prevY, float nextY) const {
    if (mapX < 0 || mapY < 0 || mapY >= height || mapX >= width) return false;

    CollisionType ct = collisionMap[mapY][mapX];
    // Disallow walking on anything that's not Floor or FloorSide
    if (ct != CollisionType::Floor && ct != CollisionType::FloorSide) return false;

    if (ct == CollisionType::Floor) {
        float tileTop = static_cast<float>(mapY) * tileSize;
        const float tolerance = 6.0f;

        if (nextY > prevY) {
            if (prevY < tileTop && nextY >= tileTop - tolerance) return true;
            if (std::abs(prevY - tileTop) <= tolerance) return true;
            return false;
        } else if (nextY < prevY) {
            return false;
        } else {
            return std::abs(prevY - tileTop) <= tolerance;
        }
    }

    return false; // fallback: not walkable
}

/**
 * @brief 2点間に壁タイルがあるかを簡易サンプリングで判定する
 */
bool Map::IsWallBetween(int x1, int y1, int x2, int y2) const {
    int steps = 10;
    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        int checkX = static_cast<int>(x1 + t * (x2 - x1)) / tileSize;
        int checkY = static_cast<int>(y1 + t * (y2 - y1)) / tileSize;
        if (GetCollisionTypeAt(checkX, checkY) == CollisionType::Wall) return true;
    }
    return false;
}

CollisionType Map::GetCollisionTypeAtScreenPos(int screenX, int screenY, const Camera& camera) const {
    int mapX = (screenX + camera.GetOffsetX()) / tileSize;
    int mapY = (screenY + camera.GetOffsetY()) / tileSize;
    return GetCollisionTypeAt(mapX, mapY);
}

bool Map::IsWalkableScreen(int screenX, int screenY, float prevY, float nextY, const Camera& camera) const {
    int mapX = (screenX + camera.GetOffsetX()) / tileSize;
    int mapY = (screenY + camera.GetOffsetY()) / tileSize;
    return IsWalkable(mapX, mapY, prevY, nextY);
}

bool Map::SaveTileMapToCSV(const std::string& filename) const {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return false;
    for (const auto& row : tileMap) {
        for (size_t i = 0; i < row.size(); ++i) {
            ofs << row[i];
            if (i + 1 < row.size()) ofs << ',';
        }
        ofs << '\n';
    }
    return true;
}

bool Map::SaveCollisionMapToCSV(const std::string& filename) const {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return false;
    for (const auto& row : collisionMap) {
        for (size_t i = 0; i < row.size(); ++i) {
            ofs << static_cast<int>(row[i]);
            if (i + 1 < row.size()) ofs << ',';
        }
        ofs << '\n';
    }
    return true;
}

std::vector<Room>& Map::GetRooms() { return rooms; }

Map& Map::GetInstance() {
    static Map instance(DEFAULT_MAP_WIDTH, DEFAULT_MAP_HEIGHT, DEFAULT_TILE_SIZE);
    return instance;
}
