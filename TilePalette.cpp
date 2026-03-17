/*!
 * @file TilePalette.cpp
 * @brief タイルパレット UI の実装
 */

#include "TilePalette.h"
#include "DxLib.h"
#include "define.h"
#include <cstdio>
#include <cmath>

TilePalette::TilePalette(int tileSize_, int tilesPerRow_, int startX_, int startY_)
    : tileSize(tileSize_), tilesPerRow(tilesPerRow_), startX(startX_), startY(startY_), selectedTileID(0), camera(nullptr) {
}

/**
 * @brief タイル画像をファイルから読み込む
 */
bool TilePalette::loadTiles() {
    const int totalTiles = TILE_PALETTE_TOTAL;
    tileHandles.resize(totalTiles);
    collisionTypes.assign(totalTiles, CollisionType::None);

    char filename[256];
    for (int i = 0; i < totalTiles; ++i) {
        sprintf_s(filename, "sprites/lab_tileset_revamped_LITE/lab_tileset_LITE/seperated/tile%03d.png", i + 1);
        int handle = LoadGraph(filename);
        if (handle == -1) {
            printf("読み込み失敗: %s\n", filename);
            return false;
        }
        tileHandles[i] = handle;
    }
    return true;
}

void TilePalette::draw() const {
    if (tileHandles.size() != collisionTypes.size()) return;

    int paletteWidth = getWidth();
    int paletteHeight = getHeight();
    DrawBox(startX - TILE_PALETTE_MARGIN, startY - TILE_PALETTE_MARGIN,
            startX + paletteWidth + TILE_PALETTE_MARGIN,
            startY + paletteHeight + TILE_PALETTE_MARGIN,
            GetColor(200, 200, 200), TRUE);

    for (int i = 0; i < static_cast<int>(tileHandles.size()); ++i) {
        int x = startX + (i % tilesPerRow) * tileSize;
        int y = startY + (i / tilesPerRow) * tileSize;
        DrawGraph(x, y, tileHandles[i], TRUE);

        CollisionType ct = collisionTypes[i];
        unsigned int frameCol = 0;
        switch (ct) {
        case CollisionType::Wall: frameCol = GetColor(255, 0, 0); break;
        case CollisionType::Floor: frameCol = GetColor(0, 255, 0); break;
        case CollisionType::Spike: frameCol = GetColor(255, 255, 0); break;
        case CollisionType::Water: frameCol = GetColor(0, 0, 255); break;
        case CollisionType::FloorSide: frameCol = GetColor(255, 0, 255); break;
        default: break;
        }
        if (ct != CollisionType::None) {
            DrawBox(x + TILE_PALETTE_INSET, y + TILE_PALETTE_INSET, x + tileSize - TILE_PALETTE_INSET, y + tileSize - TILE_PALETTE_INSET, frameCol, FALSE);
        }
        if (i == selectedTileID) {
            DrawBox(x, y, x + tileSize, y + tileSize, GetColor(255, 0, 0), FALSE);
        }
    }
}

void TilePalette::handleClick(int mouseX, int mouseY) {
    int relX = mouseX - startX;
    int relY = mouseY - startY;
    if (relX < 0 || relY < 0) return;
    int paletteW = getWidth();
    int paletteH = getHeight();
    if (relX >= paletteW || relY >= paletteH) return;
    int col = relX / tileSize;
    int row = relY / tileSize;
    int id = row * tilesPerRow + col;
    if (id >= 0 && id < static_cast<int>(tileHandles.size())) selectedTileID = id;
}

int TilePalette::getSelectedTileID() const { return selectedTileID; }
std::vector<int> TilePalette::getTileHandles() const { return tileHandles; }
int TilePalette::getWidth() const { return tilesPerRow * tileSize; }
int TilePalette::getHeight() const { return ((static_cast<int>(tileHandles.size()) + tilesPerRow - 1) / tilesPerRow) * tileSize; }

void TilePalette::startDrag(int mouseX, int mouseY) {
    if (mouseX >= startX - TILE_PALETTE_MARGIN && mouseX <= startX + getWidth() + TILE_PALETTE_MARGIN &&
        mouseY >= startY - TILE_PALETTE_MARGIN && mouseY <= startY + getHeight() + TILE_PALETTE_MARGIN) {
        isDragging = true;
        dragOffsetX = mouseX - startX;
        dragOffsetY = mouseY - startY;
        dragStartX = mouseX;
        dragStartY = mouseY;
        wasDragged = false;
    }
}

void TilePalette::updateDrag(int mouseX, int mouseY) {
    if (isDragging) {
        int dx = mouseX - dragStartX;
        int dy = mouseY - dragStartY;
        // Only treat as a drag if movement exceeds tolerance to avoid accidental small movements
        if (!wasDragged && (std::abs(dx) > PALETTE_CLICK_TOLERANCE || std::abs(dy) > PALETTE_CLICK_TOLERANCE)) {
            wasDragged = true;
        }
        if (wasDragged) {
            startX = mouseX - dragOffsetX;
            startY = mouseY - dragOffsetY;
        }
    }
}

bool TilePalette::wasClick() const {
    // ドラッグ中に移動が発生しなければクリックとみなす
    return !wasDragged;
}

void TilePalette::endDrag() {
    isDragging = false;
    // dragEnd 値はリリース時のマウス位置を概算で保存する
    dragEndX = startX + dragOffsetX;
    dragEndY = startY + dragOffsetY;
}
   
void TilePalette::setCollisionTypeForSelectedTile(CollisionType type) {
    if (selectedTileID >= 0 && selectedTileID < static_cast<int>(collisionTypes.size())) {
        collisionTypes[selectedTileID] = type;
    }
}

CollisionType TilePalette::getCollisionTypeForSelectedTile() const {
    if (selectedTileID >= 0 && selectedTileID < static_cast<int>(collisionTypes.size()))
        return collisionTypes[selectedTileID];
    return CollisionType::None;
}

void TilePalette::setCamera(Camera* cam) { camera = cam; }
