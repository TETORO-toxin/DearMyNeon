#include "SavePoint.h"
#include "DxLib.h" // 描画用
#include "game_manager.h"


SavePoint::SavePoint(int x, int y, Type type) : x(x), y(y), type(type) {}

void SavePoint::Update(Player& player) {
    if (!isActive) return;
    if (!IsPlayerInRange(player)) return;

    // ゲームプレイ中のみインタラクトを許可
    GameState gs = GameManager::GetInstance().GetGameState();
    if (gs != STATE_GAMEPLAY) return;

    if (type == Type::Save) {
        // 触れたらチェックポイントを更新（スポーン位置をこの地点にする）
        player.savedX = x;
        player.savedY = y;
        // Y キーで手動セーブも可能
        if (CheckHitKey(KEY_INPUT_Y) == 1) {
            GameManager::GetInstance().SaveGame();
        }
    } else { // Clear point
        // 触れたらステージクリアへ遷移
        GameManager::GetInstance().SetGameState(STATE_CLEAR);
    }
}

void SavePoint::Draw() const {
    if (!isActive) return;

    // Account for camera offset so world position maps to screen coordinates
    Camera& cam = GameManager::GetInstance().GetCamera();
    int camX = cam.GetOffsetX();
    int camY = cam.GetOffsetY();
    int sx = x - camX;
    int sy = y - camY;

    if (type == Type::Save) {
        // cyan marker with white outline
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
        DrawCircle(sx, sy, 20, GetColor(0, 255, 255), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawCircle(sx, sy, 20, GetColor(255,255,255), FALSE);
        DrawFormatString(sx - 12, sy - 28, GetColor(255,255,255), "SAVE");
    } else {
        // クリアポイントは黄色で描画
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
        DrawCircle(sx, sy, 24, GetColor(255, 200, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawBox(sx-8, sy-8, sx+8, sy+8, GetColor(255,255,255), TRUE);
        DrawFormatString(sx - 16, sy - 32, GetColor(255,255,255), "CLEAR");
    }
}

bool SavePoint::IsPlayerInRange(const Player& player) const {
    int px = player.GetX();
    int py = player.GetY();
    return abs(px - x) < 30 && abs(py - y) < 30;
}
