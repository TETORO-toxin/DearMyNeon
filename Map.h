#pragma once

/*!
 * @file Map.h
 * @brief マップ（タイルマップ/衝突）クラスの宣言
 *
 * タイルマップの読み込み、描画、当たり判定、歩行可否判定、CSV 保存/読み込みなど
 * マップに関する機能を提供します。各メソッドに日本語コメントを追加しています。
 */

#include <vector>
#include <string>
#include <set>
#include "TilePalette.h"
#include "Camera.h"

struct EnemyData { int x = 0; int y = 0; int type = 0; };
struct ItemData  { int x = 0; int y = 0; int type = 0; };
struct SavePointData { int x = 0; int y = 0; };

struct Room {
    int id = 0;
    int xStart = 0;
    int xEnd = 0;
    // New Y bounds for rectangular room area (world coordinates)
    int yStart = 0;
    int yEnd = 0;
    std::vector<EnemyData> enemies;
    std::vector<ItemData> items;
    std::vector<SavePointData> savePoints;
    std::string backgroundImage;
    std::string floorImage;
    std::vector<std::vector<int>> tileMap;

    // New: tutorial trigger text (empty => no tutorial)
    std::string tutorialText;
    // Whether the tutorial was already shown (runtime flag)
    bool tutorialShown = false;

    // New: room overlay text properties for edit mode
    std::string overlayText; // text to render above room (editable in editor)
    int overlayColor = 0xFFFFFF; // RGB packed as 0xRRGGBB
    int overlayFontSize = 16; // pixels

    // New: numeric tag assigned in editor (-1 = none)
    int tag = -1;
};

class Enemy;
class Camera;

class Map {
public:
    /**
     * @brief コンストラクタ: 空のマップを作成する
     */
    Map();
    /**
     * @brief 指定サイズでマップを初期化する
     * @param width マップ幅（タイル数）
     * @param height マップ高さ（タイル数）
     * @param tileSize 1 タイルのピクセルサイズ
     */
    Map(int width, int height, int tileSize);

    /** CSV からタイル/衝突情報をロードするユーティリティ */
    bool LoadTileMapFromCSV(const std::string& filename);
    bool LoadCollisionMapFromCSV(const std::string& filename);
    bool LoadFromCSV(const std::string& tileFile, const std::string& collisionFile);
    bool LoadFromCSV(const std::string& roomFile);
    bool LoadEnemiesFromCSV(const std::string& filename, std::vector<Enemy>& enemies);

    /** タイル描画用ハンドルの設定/取得 */
    void setTileHandles(const std::vector<int>& handles);
    const std::vector<int>& GetTileHandles() const;

    /** マップの描画（フルマップ） */
    void draw() const; // <-- const 指定で描画以外は行わないことを保証
    void DrawTileMap(const Room& room, const std::vector<int>& tileImages) const;

    /** タイルの編集 */
    void placeTile(int mapX, int mapY, int tileID, CollisionType type);
    void removeTile(int mapX, int mapY);

    int GetTileSize() const noexcept { return tileSize; }
    int GetWidth() const noexcept { return width; }
    int GetHeight() const noexcept { return height; }

    /** 衝突判定や歩行判定 */
    int GetFloorYAt(int mapX, int mapY) const;
    CollisionType GetCollisionTypeAt(int x, int y) const;
    CollisionType GetCollisionTypeAtScreenPos(int screenX, int screenY, const Camera& camera) const;
    bool IsWalkable(int mapX, int mapY, float prevY, float nextY) const;
    bool IsWallBetween(int x1, int y1, int x2, int y2) const;
    bool IsWalkableScreen(int screenX, int screenY, float prevY, float nextY, const Camera& camera) const;

    /** CSV 保存 */
    bool SaveTileMapToCSV(const std::string& filename) const;
    bool SaveCollisionMapToCSV(const std::string& filename) const;

    std::vector<Room>& GetRooms();

    /** タイルIDセット指定: 指定した ID は Wall/Floor/FloorSide と判定する
     *  エディタでタイル種別を設定するのに用いる
     */
    void SetWallTileIDs(const std::set<int>& ids);
    void SetFloorTileIDs(const std::set<int>& ids);
    void SetFloorSideTileIDs(const std::set<int>& ids);

    // タイルID 取得: 範囲外は -1 を返す
    int GetTileIdAt(int x, int y) const;

    // シングルトン風のアクセス（簡易ユーティリティ）
    static Map& GetInstance();

private:
    std::vector<std::string> Split(const std::string& str, char delimiter) const;

    int width = DEFAULT_MAP_WIDTH;
    int height = DEFAULT_MAP_HEIGHT;
    int tileSize = DEFAULT_TILE_SIZE;

    std::vector<std::vector<int>> tileMap;
    std::vector<std::vector<CollisionType>> collisionMap;
    std::vector<int> tileHandles;
    std::vector<Room> rooms;

    std::set<int> wallTileIDs;
    std::set<int> floorTileIDs;
    std::set<int> floorSideTileIDs;

    TilePalette* tilePalette = nullptr;
};

