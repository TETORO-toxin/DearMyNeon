#include "enemy.h"
#include "game_manager.h"
#include "define.h"
#include <Dxlib.h>
#include <cmath>

// 描画処理: 敵のスプライト、HPバー、攻撃テレグラフ、当たり判定可視化などを行う
void Enemy::Draw() const
{
    if (!isActive) return;

    SpriteInfo info = (type == 0) ? meleeSpriteInfoMap.at(currentState) : rangedSpriteInfoMap.at(currentState);

    int graphHandle = idleGraph;

    switch (currentState) {
        case ENEMY_IDLE:   graphHandle = idleGraph;   break;
        case ENEMY_ATTACK: graphHandle = attackGraph; break;
        case ENEMY_HIT:    graphHandle = hitGraph;    break;
        case ENEMY_DEAD:   graphHandle = deadGraph;   break;
        case ENEMY_MOVE:   graphHandle = moveGraph;   break;
        default: break;
    }

    float scale = 2.0f;
    int drawWidth  = static_cast<int>(info.width  * scale);
    int drawHeight = static_cast<int>(info.height * scale);

    Camera& cam = GameManager::GetInstance().GetCamera();
    int camX = cam.GetOffsetX();
    int camY = cam.GetOffsetY();

    int baseX = x - drawWidth / 2 - camX;
    int baseY = y - drawHeight - camY;

    int destLeft  = baseX;
    int destRight = baseX + drawWidth;

    if (!isFacingRight) std::swap(destLeft, destRight);

    // スプライト描画（拡大、フレーム指定）
    float srcXf = 0.0f;
    float srcYf = 0.0f;

    // Determine columns; if not set, assume single row (columns = frameCount)
    int cols = (info.columns > 0) ? info.columns : info.frameCount;
    float spacing = info.frameSpacing;

    int frameIndex = animationFrame % info.frameCount;
    int col = frameIndex % cols;
    int row = frameIndex / cols;

    srcXf = info.srcOffsetX + static_cast<float>(col) * (info.width + spacing);
    srcYf = info.srcOffsetY + static_cast<float>(row) * (info.height + spacing);

    DrawRectExtendGraph(destLeft, baseY, destRight, baseY + drawHeight,
                        static_cast<int>(srcXf), static_cast<int>(srcYf),
                        static_cast<int>(info.width), static_cast<int>(info.height),
                        graphHandle, TRUE);

    // HPバー描画（背景と赤い残量部分）
    int hpBarWidth  = ENEMY_HP_BAR_WIDTH;
    int hpBarHeight = ENEMY_HP_BAR_HEIGHT;
    float hpRatio = static_cast<float>(life) / static_cast<float>((type == 0) ? ENEMY_MELEE_LIFE : ENEMY_RANGED_LIFE);

    int barLeft = x - hpBarWidth / 2 - camX;
    int barTop  = y - drawHeight - 10 - camY;

    DrawBox(barLeft, barTop, barLeft + hpBarWidth, barTop + hpBarHeight, GetColor(100, 100, 100), TRUE);
    DrawBox(barLeft, barTop, barLeft + static_cast<int>(hpBarWidth * hpRatio), barTop + hpBarHeight, GetColor(255, 0, 0), TRUE);

    // 近接攻撃のエフェクト（特定タイミングで円を描画）
    if (attackEffectTimer == ENEMY_ATTACK_EFFECT_DURATION && type == 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
        DrawCircle(x - camX, y - DEFAULT_TILE_SIZE - camY, 15, GetColor(255, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // 攻撃予告（テレグラフ）を円で表示。縮小とアルファで残り時間を視覚化
    if ((attackPending || currentState == ENEMY_ATTACK) && attackTelegraphTotal > 0 && attackTelegraphTimer >= 0) {
        int windupLeft = attackTelegraphTimer;
        if (windupLeft < 0) windupLeft = 0;

        float shrink = 1.0f;
        if (attackTelegraphTotal > 0) shrink = static_cast<float>(windupLeft) / static_cast<float>(attackTelegraphTotal);

        int radius = TELEGRAPH_BASE_RADIUS + static_cast<int>(shrink * TELEGRAPH_EXTRA_RADIUS);
        int alpha  = static_cast<int>(50 + (1.0f - shrink) * 200);

        if (alpha > 255) alpha = 255;
        if (radius < 1) radius = 1;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        DrawCircle(x - camX, y - DEFAULT_TILE_SIZE - camY, radius, GetColor(255, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // 攻撃ヒットボックスの可視化（エディットモードやデバッグ用）
    if (currentState == ENEMY_ATTACK && attackHitTimer > 0) {
        RECT ar = GetAttackRect();
        int ax = ar.left - camX;
        int ay = ar.top  - camY;
        int aright = ar.right - camX;
        int abottom = ar.bottom - camY;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 120);
        DrawBox(ax, ay, aright, abottom, GetColor(255, 0, 0), TRUE);
        DrawBox(ax, ay, aright, abottom, GetColor(255, 255, 255), FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // 驚きの「！」アイコン描画
    if (exclamationTimer > 0) {
        DrawGraph(x - 16 - camX, y - drawHeight - 48 - camY, exclamationGraph, TRUE);
    }

    // エディットモード時は視界の扇形を描画してデバッグ支援
    if (GameManager::GetInstance().editModeManager.IsEditMode()) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);

        int eyeX = x - camX;
        int eyeY = y - eyeOffsetY - camY;

        const float halfAngle = viewAngleDeg * 0.5f;
        float facingAngleDeg = isFacingRight ? 0.0f : 180.0f;

        float startDeg = facingAngleDeg - halfAngle;
        float endDeg   = facingAngleDeg + halfAngle;
        const float stepDeg = 3.0f;

        for (float a = startDeg; a <= endDeg; a += stepDeg) {
            float rad = a * 3.14159265f / 180.0f;
            int px = static_cast<int>(eyeX + cosf(rad) * visionRadius);
            int py = static_cast<int>(eyeY + sinf(rad) * visionRadius);
            DrawLine(eyeX, eyeY, px, py, GetColor(0, 200, 0));
        }

        int prevX = -1, prevY = -1;

        for (float a = startDeg; a <= endDeg + 0.1f; a += stepDeg) {
            float rad = a * 3.14159265f / 180.0f;
            int px = static_cast<int>(eyeX + cosf(rad) * visionRadius);
            int py = static_cast<int>(eyeY + sinf(rad) * visionRadius);

            if (prevX != -1) DrawLine(prevX, prevY, px, py, GetColor(0, 255, 0));

            prevX = px;
            prevY = py;
        }

        float radS = startDeg * 3.14159265f / 180.0f;
        float radE = endDeg   * 3.14159265f / 180.0f;

        int sx = static_cast<int>(eyeX + cosf(radS) * visionRadius);
        int sy = static_cast<int>(eyeY + sinf(radS) * visionRadius);
        int exa = static_cast<int>(eyeX + cosf(radE) * visionRadius);
        int eya = static_cast<int>(eyeY + sinf(radE) * visionRadius);

        DrawLine(eyeX, eyeY, sx, sy, GetColor(0, 255, 0));
        DrawLine(eyeX, eyeY, exa, eya, GetColor(0, 255, 0));

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}
