#ifndef ITEM_H
#define ITEM_H

/*!
 * @file item.h
 * @brief マップ上に存在する回復アイテムや弾アイテムを表すクラス
 */

#include "DxLib.h"
#include "define.h"

// アイテムクラス
class Item {
public:
    Item(int startX, int startY, ItemType itemType);
    ~Item();

    void Draw() const;
    RECT GetRect() const {
        return { x - ITEM_HALF_SIZE, y - ITEM_HALF_SIZE, x + ITEM_HALF_SIZE, y + ITEM_HALF_SIZE };
    }

    int x = 0;
    int y = 0;
    ItemType type = ITEM_RECOVERY;
    bool isActive = false;

private:
    int itemGraph = -1;
};

#endif // ITEM_H
