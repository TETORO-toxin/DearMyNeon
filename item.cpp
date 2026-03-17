/*!
 * @file item.cpp
 * @brief アイテム（回復/弾）実装
 */

#include "item.h"
#include <cstdio>
#include <cstdlib>
#include "game_manager.h"

// コンストラクタ: 種類に応じて画像を読み込み有効化
Item::Item(int startX, int startY, ItemType itemType)
    : x(startX), y(startY), type(itemType), isActive(true), itemGraph(-1)
{
    switch (type) {
    case ITEM_RECOVERY:
        itemGraph = LoadGraph(ITEM_RECOVERY_IMAGE);
        break;
    case ITEM_BULLET:
        itemGraph = LoadGraph(ITEM_BULLET_IMAGE);
        break;
    default:
        itemGraph = LoadGraph(ITEM_RECOVERY_IMAGE);
        break;
    }

    if (itemGraph == -1) {
        char buf[256];
        sprintf_s(buf, "Error: item image not found (%s).", (type == ITEM_RECOVERY) ? ITEM_RECOVERY_IMAGE : ITEM_BULLET_IMAGE);
        MessageBox(NULL, buf, "Image Load Error", MB_OK);
        // 致命的な画像欠落はここで即時終了
        DxLib_End();
        exit(-1);
    }
}

Item::~Item()
{
    if (itemGraph != -1) {
        DeleteGraph(itemGraph);
        itemGraph = -1;
    }
}

// 描画: カメラオフセットを考慮してスクリーンに描画する
void Item::Draw() const {
    if (!isActive) return;
    // Apply camera offset so item is drawn at the correct screen position
    Camera& cam = GameManager::GetInstance().GetCamera();
    int drawX = x - ITEM_HALF_SIZE - cam.GetOffsetX();
    int drawY = y - ITEM_HALF_SIZE - cam.GetOffsetY();
    DrawGraph(drawX, drawY, itemGraph, TRUE);
}
