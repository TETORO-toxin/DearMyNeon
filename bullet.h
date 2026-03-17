#ifndef BULLET_H
#define BULLET_H

/*!
 * @file bullet.h
 * @brief 弾（プレイヤー/敵が撃つ発射物）を表すクラス宣言
 */

#include "DxLib.h"
#include "define.h"

// 弾のシンプルな構造体/クラス
class Bullet
{
public:
    int x, y;          // 位置
    int vx, vy;        // 速度
    int power;         // ダメージ量
    bool isActive;     // 有効フラグ
    int bulletGraph;   // 表示用グラフィックハンドル

    // キャッシュされた半幅/半高（描画中心計算に使用）
    int instanceHalfW;
    int instanceHalfH;

    Bullet(int startX, int startY, int dirX, int dirY, int bulletPower);
    Bullet(int startX, int startY, int dirX, int dirY, int bulletPower, int graphHandle);

    void Update(); // 毎フレーム更新
    void Draw()const;   // 描画

    // 衝突判定用の矩形を返す
    RECT GetRect() const;

    // Re-initialize an existing bullet instance for reuse (avoids allocation hiccups)
    void Fire(int startX, int startY, int dirX, int dirY, int bulletPower);
};

#endif // BULLET_H