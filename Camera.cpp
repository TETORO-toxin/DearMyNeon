/*!
 * @file Camera.cpp
 * @brief カメラ（Camera）実装: 追従とクランプ処理
 */

#include "Camera.h"
#include "define.h"
#include <algorithm>

// マップ全体のサイズを設定（カメラのクランプ範囲に使用）
void Camera::SetMapSize(int width, int height) {
    mapWidth = width; mapHeight = height;
}

// 画面サイズを設定（カメラの中心計算に使用）
void Camera::SetScreenSize(int width, int height) {
    screenWidth = width;
    screenHeight = height;
}

// プレイヤー等のターゲット座標に滑らかに追従する更新
void Camera::Update(int targetX, int targetY) {
    // 目標となるオフセット（カメラ中心）
    float desiredX = static_cast<float>(targetX) - screenWidth / 2.0f;
    float desiredY = static_cast<float>(targetY) - screenHeight / 2.0f;

    // マップ境界にクランプ
    int maxOffsetX = std::max(0, mapWidth - screenWidth);
    int maxOffsetY = std::max(0, mapHeight - screenHeight);
    desiredX = std::max(0.0f, std::min(desiredX, static_cast<float>(maxOffsetX)));
    desiredY = std::max(0.0f, std::min(desiredY, static_cast<float>(maxOffsetY)));

    // 緩やかな追従（線形補間）
    offsetX += (desiredX - offsetX) * followAlpha;
    offsetY += (desiredY - offsetY) * followAlpha;
}

// 直接移動（スクリーンショック等の一時的オフセットに使用）
void Camera::Update(float moveX, float moveY) {
    offsetX += moveX;
    offsetY += moveY;
    int maxOffsetX = std::max(0, mapWidth - screenWidth);
    int maxOffsetY = std::max(0, mapHeight - screenHeight);
    offsetX = std::max(0.0f, std::min(offsetX, static_cast<float>(maxOffsetX)));
    offsetY = std::max(0.0f, std::min(offsetY, static_cast<float>(maxOffsetY)));
}

int Camera::GetOffsetX() const { return static_cast<int>(offsetX); }
int Camera::GetOffsetY() const { return static_cast<int>(offsetY); }




