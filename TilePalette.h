#pragma once
#include "Camera.h"
#include <vector>
#include <string>

/*!
 * @file TilePalette.h
 * @brief エディタ用のタイルパレット UI を提供するクラス
 *
 * TilePalette はタイル画像の一覧を管理し、選択・ドラッグ操作、タイルごとの
 * 衝突タイプ設定などをサポートします。Map にタイルハンドルを渡して描画に利用されます。
 */

/** グローバルな衝突タイプ列挙は Map と共有 */
enum class CollisionType { None, Wall, Floor, FloorSide, Spike, Water };

class TilePalette {
public:
    TilePalette(int tileSize, int tilesPerRow, int startX, int startY);

    bool loadTiles();
    void draw() const;
    void handleClick(int mouseX, int mouseY);

    void startDrag(int mouseX, int mouseY);
    void updateDrag(int mouseX, int mouseY);
    void endDrag();
    bool wasClick() const;

    int getSelectedTileID() const;
    std::vector<int> getTileHandles() const;

    int getX() const { return startX; }
    int getY() const { return startY; }

    void setCollisionTypeForSelectedTile(CollisionType type);
    CollisionType getCollisionTypeForSelectedTile() const;
    int getWidth() const;
    int getHeight() const;

    void setCamera(Camera* cam);

    // ドラッグ状態は外部からも参照されるため public にしてある
    bool isDragging = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    int dragStartX = 0;
    int dragStartY = 0;
    int dragEndX = 0;
    int dragEndY = 0;
    bool wasDragged = false;

private:

    Camera* camera = nullptr;
    std::vector<CollisionType> collisionTypes;
    std::vector<int> tileHandles;
    int tileSize = 0;
    int tilesPerRow = 0;
    int startX = 0;
    int startY = 0;
    int selectedTileID = 0;

};

