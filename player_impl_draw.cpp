#include "player.h"
#include "game_manager.h"
#include <DxLib.h>
#include <cmath>

// ライフ UI と弾アイコンの描画、プレイヤー本体の描画を行う

void Player::DrawLifeUI() const {
    const int spriteWidth = 65;
    const int spriteHeight = 17;
    const int lifeStages = 8;
    const int baseX = 10, baseY = 10;
    const float scale = 4.0f; // 拡大: 以前は 3.0f

    float lifeRatio = static_cast<float>(life) / static_cast<float>(maxLife);
    int index = static_cast<int>((1.0f - lifeRatio) * lifeStages);
    index = std::max(0, std::min(index, lifeStages - 1));

    int srcX = index * spriteWidth;

    // 被弾時にライフ UI を揺らす: hitEffectTimer を利用して揺れの振幅を決定
    int shakeOffsetX = 0;
    if (hitEffectTimer > 0) {
        const float maxTimer = 15.0f; // TakeDamage で設定される初期値に合わせる
        float t = static_cast<float>(hitEffectTimer);
        float amplitude = 6.0f * (t / maxTimer); // 残り時間に応じて振幅を小さくする
        shakeOffsetX = static_cast<int>(std::sin(t * 3.0f) * amplitude);
    }

    DrawRectExtendGraph(baseX + shakeOffsetX, baseY, baseX + shakeOffsetX + static_cast<int>(spriteWidth * scale), baseY + static_cast<int>(spriteHeight * scale),
                        srcX, 0, spriteWidth, spriteHeight, lifeUITexture, TRUE);
}

void Player::DrawBulletIcons() const {
    const int iconWidth = 23, iconHeight = 62, baseX = 10, baseY = 90, spacing = 8; // baseY と spacing を調整
    const float scale = 1.0f; // 拡大: 以前は 0.5f
    for (int i = 0; i < bulletCount; ++i) {
        int drawX = baseX + i * (static_cast<int>(iconWidth * scale) + spacing);
        int drawY = baseY;
        DrawExtendGraph(drawX, drawY, drawX + static_cast<int>(iconWidth * scale), drawY + static_cast<int>(iconHeight * scale),
                        bulletUITexture, TRUE);
    }
}

void Player::DrawSkillIcons() const {
    if (skillUITexture == -1) return;
    // Icons placed at bottom-right corner
    const int iconCount = 4; // T E Q R
    const int iconW = 48; // source icon width in atlas
    const int iconH = 48; // source icon height in atlas
    const float iconScale = 1.2f; // draw scale
    const int spacing = 8;

    int totalW = static_cast<int>(iconCount * iconW * iconScale + (iconCount - 1) * spacing);
    int baseX = SCREEN_WIDTH - 10 - totalW; // 10px margin from right
    int baseY = SCREEN_HEIGHT - 10 - static_cast<int>(iconH * iconScale); // 10px margin from bottom

    const char keyChars[4] = { 'T', 'E', 'Q', 'R' };

    for (int i = 0; i < iconCount; ++i) {
        int srcX = i * iconW;
        int drawX = baseX + i * (static_cast<int>(iconW * iconScale) + spacing);
        int drawY = baseY;
        int dstW = static_cast<int>(iconW * iconScale);
        int dstH = static_cast<int>(iconH * iconScale);
        DrawRectExtendGraph(drawX, drawY, drawX + dstW, drawY + dstH,
                            srcX, 0, iconW, iconH, skillUITexture, TRUE);

        // If skill is on cooldown, draw a translucent overlay indicating remaining cooldown.
        int cooldown = 0; int cooldownMax = 1;
        switch (i) {
        case 0: // T -> dodge
            cooldown = dodgeCooltime; cooldownMax = PLAYER_DODGE_COOLTIME; break;
        case 1: // E -> melee attack (use attackCooldown)
            cooldown = attackCooldown; cooldownMax = attackCooldownMax; break;
        case 2: // Q -> shoot (cooldown per-shot not implemented; show 0)
            cooldown = 0; cooldownMax = 1; break;
        case 3: // R -> reload or special (not implemented)
            cooldown = 0; cooldownMax = 1; break;
        }

        if (cooldown > 0) {
            float ratio = static_cast<float>(cooldown) / static_cast<float>(std::max(1, cooldownMax));
            // draw a semi-transparent black rectangle covering part of the icon from top
            int coverH = static_cast<int>(dstH * ratio);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
            DrawBox(drawX, drawY, drawX + dstW, drawY + coverH, GetColor(0,0,0), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            // Draw remaining time as text
            char buf[16]; sprintf_s(buf, "%d", cooldown);
            int textX = drawX + dstW / 2 - 8;
            int textY = drawY + dstH / 2 - 8;
            DrawString(textX, textY, buf, GetColor(255,255,255));
        }

        // Draw key label under icon
        char label[2] = { keyChars[i], '\0' };
        int lx = drawX + dstW / 2 - 6;
        int ly = drawX + dstH + 4; // bugfix: previously used drawY; use drawY for vertical position
        DrawString(lx, drawY + dstH + 4, label, GetColor(220,220,220));
    }
}

void Player::Draw() {
    int graphHandle = idleGraph;
    switch (currentState) {
    case IDLE: graphHandle = idleGraph; break;
    case MOVE: graphHandle = moveGraph; break;
    case JUMP: graphHandle = jumpGraph; break;
    case FALL: graphHandle = fallGraph; break;
    case ATTACK: graphHandle = attackGraph; break;
    case DODGE: graphHandle = dodgeGraph; break;
    case HIT: graphHandle = hitGraph; break;
    case DEAD: graphHandle = deadGraph; break;
    }

    int spriteWidth = 48, spriteHeight = 48;
    if (currentState == DODGE) { spriteWidth = 112; spriteHeight = 56; }
    else if (currentState == ATTACK) { spriteWidth = 131; spriteHeight = 56; }
    else if (currentState == DEAD) { spriteWidth = 76; spriteHeight = 48; }

    // UI を描画
    DrawLifeUI();
    DrawBulletIcons();
    DrawSkillIcons(); // draw skill icons at bottom-right

    if (!GameManager::GetInstance().editModeManager.IsEditMode()) {
        // Player control help removed per user request.
    }

    int drawX = x - spriteWidth / 2 - GameManager::GetInstance().GetCamera().GetOffsetX();
    int drawY = static_cast<int>(y) - spriteHeight - GameManager::GetInstance().GetCamera().GetOffsetY();

    DrawRectGraph(drawX, drawY, animationFrame * spriteWidth, 0, spriteWidth, spriteHeight, graphHandle, TRUE, isFacingRight ? FALSE : TRUE);

    // 弾と銃撃エフェクトの描画
    for (Bullet* b : bullets) if (b && b->isActive) b->Draw();
    Camera& cam = GameManager::GetInstance().GetCamera();
    for (const auto& effect : gunEffects) effect.Draw(gunEffectGraph, cam.GetOffsetX(), cam.GetOffsetY());

    // デバッグ用: 現在タイルの衝突タイプを取得する（未使用だが保持している）
    int tileSize = GameManager::GetInstance().GetMap().GetTileSize();
    int tileX = x / tileSize, tileY = static_cast<int>(y) / tileSize;
    CollisionType type = GameManager::GetInstance().GetMap().GetCollisionTypeAt(tileX, tileY);
}
