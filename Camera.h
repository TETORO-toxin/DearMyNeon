#pragma once
#include "define.h"

/*!
 * @file Camera.h
 * @brief カメラ（スクロールオフセット）を管理するクラス
 *
 * Camera は描画時に使用するオフセット（GetOffsetX/GetOffsetY）を計算します。
 */

class Camera {
private:
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    int screenWidth = SCREEN_WIDTH;   // 画面幅
    int screenHeight = SCREEN_HEIGHT; // 画面高さ
    int mapWidth = 0;
    int mapHeight = 0;
    float followAlpha = 0.15f; // smoothing factor for delayed follow
public:
    void SetMapSize(int width, int height);
    void Update(int targetX, int targetY);
    int GetOffsetX() const;
    int GetOffsetY() const;
    void Update(float moveX, float moveY);
    // Set actual draw/screen size (needed when switching fullscreen/windowed)
    void SetScreenSize(int width, int height);
    // Adjust how quickly camera follows the target (0..1, higher = snappier)
    void SetFollowAlpha(float a) { if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f; followAlpha = a; }
};
