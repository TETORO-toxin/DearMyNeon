/*!
 * @file bullet.cpp
 * @brief 弾（Bullet） の実装：移動・描画・当たり判定
 */

#include "bullet.h"
#include "game_manager.h"
#include "define.h"
#include "Code/debug_utils.h"
#include <cstdio>

// コンストラクタ: 弾の初期位置・速度・威力を設定し、グラフィックをロードまたはキャッシュする
Bullet::Bullet(int startX, int startY, int dirX, int dirY, int bulletPower)
    : x(startX), y(startY), vx(dirX), vy(dirY), power(bulletPower), isActive(true)
{
    static int s_default_bullet_graph = -1;
    static int s_default_bullet_w = 0;
    static int s_default_bullet_h = 0;
    if (s_default_bullet_graph == -1) {
        s_default_bullet_graph = LoadGraph(BULLET_IMAGE_PATH);
        if (s_default_bullet_graph != -1) {
            GetGraphSize(s_default_bullet_graph, &s_default_bullet_w, &s_default_bullet_h);
        } else {
            DEBUG_ONLY( DBG_PRINTF("Warning: %s not found. Bullets will be invisible.\n", BULLET_IMAGE_PATH); );
        }
    }
    bulletGraph = s_default_bullet_graph;
    // インスタンスごとに描画半幅/半高をキャッシュ
    if (bulletGraph != -1) {
        int w = 0, h = 0;
        GetGraphSize(bulletGraph, &w, &h);
        instanceHalfW = w / 2;
        instanceHalfH = h / 2;
    } else {
        instanceHalfW = BULLET_DRAW_HALF;
        instanceHalfH = BULLET_DRAW_HALF;
    }
}

Bullet::Bullet(int startX, int startY, int dirX, int dirY, int bulletPower, int graphHandle)
    : x(startX), y(startY), vx(dirX), vy(dirY), power(bulletPower), isActive(true), bulletGraph(graphHandle)
{
    if (bulletGraph != -1) {
        int w = 0, h = 0;
        GetGraphSize(bulletGraph, &w, &h);
        instanceHalfW = (w > 0) ? (w / 2) : BULLET_DRAW_HALF;
        instanceHalfH = (h > 0) ? (h / 2) : BULLET_DRAW_HALF;
    } else {
        DEBUG_ONLY( DBG_PRINTF("Warning: enemy bullet graph handle invalid.\n"); );
        instanceHalfW = BULLET_DRAW_HALF;
        instanceHalfH = BULLET_DRAW_HALF;
    }
}

// 位置更新: 速度を加算し、画面外に出たら非アクティブ化する
void Bullet::Update() {
    if (!isActive) return;
    x += vx; y += vy;
    // カメラ範囲外に出たら無効化（パフォーマンス目的）
    Camera& cam = GameManager::GetInstance().GetCamera();
    int viewX = cam.GetOffsetX();
    int viewY = cam.GetOffsetY();
    if (x < viewX - BULLET_OFFSCREEN_PADDING || x > viewX + SCREEN_WIDTH + BULLET_OFFSCREEN_PADDING ||
        y < viewY - BULLET_OFFSCREEN_PADDING || y > viewY + SCREEN_HEIGHT + BULLET_OFFSCREEN_PADDING) {
        isActive = false;
    }
}

// 描画: カメラオフセットを考慮してワールド座標からスクリーン描画する
void Bullet::Draw() const {
    if (!isActive) return;
    if (bulletGraph == -1) return; // 描画資産がなければ何もしない
    Camera& cam = GameManager::GetInstance().GetCamera();
    int ox = cam.GetOffsetX();
    int oy = cam.GetOffsetY();
    DrawGraph(x - instanceHalfW - ox, y - instanceHalfH - oy, bulletGraph, TRUE);
}

// 衝突矩形を返す（当たり判定で使用）
RECT Bullet::GetRect() const {
    return { x - instanceHalfW, y - instanceHalfH, x + instanceHalfW, y + instanceHalfH };
}

// 弾を発射（プールから再利用する場合に使う）
void Bullet::Fire(int startX, int startY, int dirX, int dirY, int bulletPower) {
    x = startX; y = startY; vx = dirX; vy = dirY; power = bulletPower; isActive = true;
}