#include "player.h"
#include "game_manager.h"
#include "Map.h"
#include "define.h"
#include <DxLib.h>
#include <cmath>
#include "bullet.h"

// プレイヤーの更新処理: 入力読み取り、移動・ジャンプ、攻撃、弾発射、弾プール更新など

void Player::ProcessMeleeAttack(std::vector<Enemy>& enemies) {
    // Don't block hit detection just because a cooldown value was set on the same frame.
    if (currentState != ATTACK || hasHitThisAttack) return;
    RECT attackRect = GetAttackRect();
    for (auto& enemy : enemies) {
        if (!enemy.IsActive()) continue;
        if (GameManager::GetInstance().CheckCollision(attackRect, enemy.GetRect())) {
            enemy.TakeDamage(ATTACK_DAMAGE_DEFAULT);
            hasHitThisAttack = true;
            // Apply a short cooldown when the melee attack hits to allow quicker follow-up attacks
            attackCooldown = meleeAttackCooldown;
            break;
        }
    }
}

RECT Player::GetAttackRect() const {
    const int attackWidth = 64;
    const int attackHeight = 48;
    RECT r;
    if (isFacingRight) {
        r.left = x;
        r.top = static_cast<int>(y) - attackHeight / 2;
        r.right = x + attackWidth;
        r.bottom = static_cast<int>(y) + attackHeight / 2;
    } else {
        r.left = x - attackWidth;
        r.top = static_cast<int>(y) - attackHeight / 2;
        r.right = x;
        r.bottom = static_cast<int>(y) + attackHeight / 2;
    }
    return r;
}

void Player::Update() {
    Camera& camera = GameManager::GetInstance().GetCamera();

 
    InputState input;
    if (demoMode) {
        input = demoInput;
    } else {
        input = ProcessInput();
    }
    int padInput = input.padInput;

    const int moveSpeed = PLAYER_MOVE_SPEED;
    const float gravity = PLAYER_GRAVITY;
    const int jumpPower = PLAYER_JUMP_POWER;
    const float groundYDefault = static_cast<float>(PLAYER_SPAWN_Y);
    float groundY = groundYDefault;
    if (!map) {
        groundY = groundYDefault + 2000.0f;
    }

    if (map) {
        int tileSize = map->GetTileSize();
        int tileX = x / tileSize;
        int tileY = static_cast<int>(y) / tileSize;
        CollisionType under = map->GetCollisionTypeAt(tileX, tileY);
        if (under != CollisionType::Floor) {
            isOnGround = false;
        }
    }

    // ライフが0以下なら死亡処理（アニメーションとゲームオーバー遷移）
    if (life <= 0) {
        if (currentState != DEAD) {
            currentState = DEAD;
            animationFrame = 0;
            animationTimer = 0;
            PlaySoundMem(deadSound, DX_PLAYTYPE_BACK);
        }
        moveX = 0.0f;
        moveY = 0.0f;
        attackPushTimer = 0;
        attackPushX = 0.0f;
        attackCooldown = 0;
        hasHitThisAttack = true;

        animationTimer++;
        static const int maxFramesDead = 8;
        if (animationTimer > DEAD_ANIM_TICK) {
            animationFrame++;
            if (animationFrame >= maxFramesDead) {
                animationFrame = maxFramesDead - 1;
                GameManager::GetInstance().SetGameState(STATE_GAMEOVER);
            }
            animationTimer = 0;
        }
        return;
    }

    // 無敵時間の更新
    if (isInvincible) {
        invincibleTimer++;
        if (invincibleTimer > PLAYER_INVINCIBLE_TIME) {
            isInvincible = false;
            invincibleTimer = 0;
        }
    }

    moveX = 0;
    moveY = 0;
    bool isMoving = false;

    int horizDir = 0;
    // 水平入力はキー配列を直接参照していたのでそのまま使用
    if (keyStateArr[KEY_INPUT_D] != 0 || (padInput & PAD_INPUT_RIGHT)) {
        horizDir = 1;
        isFacingRight = true;
    }
    if (keyStateArr[KEY_INPUT_A] != 0 || (padInput & PAD_INPUT_LEFT)) {
        horizDir = -1;
        isFacingRight = false;
    }

    // 水平移動のスムージング（加速/減速）処理
    const float maxH = static_cast<float>(moveSpeed);
    const float accelPerFrame = 0.30f; // アクセルを速くするために増加（最高速度は moveSpeed で固定）
    const float startSpeedFraction = 0.02f;
    // 減速を速くするために乗数を増加
    const float decelPerFrame = (3.0f * accelPerFrame) / (1.0f - startSpeedFraction);

    if (horizDir != 0) {
        isMoving = true;
        if (currentState != ATTACK && currentState != DODGE) currentState = MOVE;
         if (!prevHorizMoving) {
             horizSpeedCurrent = horizDir * (maxH * startSpeedFraction);
         } else if (prevHorizDir != horizDir) {
             horizSpeedCurrent = horizDir * (maxH * 0.6f);
         } else {
            float target = horizDir * maxH;
            float diff = target - horizSpeedCurrent;
            if (std::fabs(diff) <= accelPerFrame) horizSpeedCurrent = target;
            else horizSpeedCurrent += (diff > 0 ? accelPerFrame : -accelPerFrame);
         }
         moveX = horizSpeedCurrent;
         prevHorizDir = horizDir;
         prevHorizMoving = true;
     } else {
         if (std::fabs(horizSpeedCurrent) <= decelPerFrame) {
             horizSpeedCurrent = 0.0f;
             prevHorizDir = 0;
             prevHorizMoving = false;
         } else {
             horizSpeedCurrent += (horizSpeedCurrent > 0.0f ? -decelPerFrame : decelPerFrame);
             prevHorizMoving = true;
         }
         moveX = horizSpeedCurrent;
     }

    // ジャンプ
    static constexpr float JUMP_INITIAL_IMPULSE_MULT = 1.25f;
    static constexpr float JUMP_INITIAL_GRAVITY_MULT = 3.0f;
    if (input.isWJustPressed) {
        if (isOnGround || jumpCount < MAX_JUMP_COUNT) {
            vy = jumpPower * JUMP_INITIAL_IMPULSE_MULT;
            isOnGround = false;
            jumpCount++;
            PlaySoundMem(jumpSound, DX_PLAYTYPE_BACK);
            currentState = JUMP;
            jumpInitialFramePending = true;
        }
    }

    // 回避
    if (input.isZJustPressed && dodgeCooltime == 0) {
        currentState = DODGE;
        dodgeCooltime = PLAYER_DODGE_COOLTIME;
        animationFrame = 0;
        animationTimer = 0;
        moveX = isFacingRight ? static_cast<float>(PLAYER_DODGE_SPEED) : static_cast<float>(-PLAYER_DODGE_SPEED);
        // 回避時は長めの無敵時間を付与
        isInvincible = true;
        invincibleTimer = 0;
        invincibleDuration = PLAYER_DODGE_INVINCIBLE_TIME;
    }

    // 攻撃入力
    if (input.isXJustPressed) {
        if (attackCooldown == 0) {
            currentState = ATTACK;
            animationFrame = 0;
            animationTimer = 0;
            hasHitThisAttack = false;
            PlaySoundMem(attackSound, DX_PLAYTYPE_BACK);
            attackPushX = isFacingRight ? static_cast<float>(PLAYER_ATTACK_PUSH) : static_cast<float>(-PLAYER_ATTACK_PUSH);
            attackPushTimer = 4;
            // apply a short cooldown immediately on attack start to prevent spamming
            attackCooldown = meleeAttackCooldown;
            auto& enemies = GameManager::GetInstance().GetEnemies();
            ProcessMeleeAttack(enemies);
        }
    }

    // 発砲入力（Q）: 弾薬があれば予測位置に弾をスポーンさせる
    if (input.isQJustPressed && bulletCount > 0) {
        bulletCount--;
        int dirX = isFacingRight ? PLAYER_BULLET_SPEED : -PLAYER_BULLET_SPEED;
        const int muzzleOffset = 20;
        float facingDir = isFacingRight ? 1.0f : -1.0f;
        int predictedExtra = static_cast<int>(std::round(std::fabs(moveX) * PLAYER_PREDICT_FRAMES));
        int spawnX = x + static_cast<int>(facingDir * (muzzleOffset + PLAYER_AHEAD_OFFSET + predictedExtra));
        int spawnY = static_cast<int>(y) - 36;
        bool fired = false;
        for (Bullet* b : bullets) {
            if (b && !b->isActive) {
                b->Fire(spawnX, spawnY, dirX, 0, PLAYER_BULLET_POWER);
                fired = true;
                break;
            }
        }
        if (!fired) {
            bullets.push_back(new Bullet(spawnX, spawnY, dirX, 0, PLAYER_BULLET_POWER));
        }
        if (shootSound != -1) PlaySoundMem(shootSound, DX_PLAYTYPE_BACK);
        int effectX = spawnX;
        int effectY = spawnY;
        gunEffects.emplace_back(effectX, effectY, isFacingRight);
    }

    // 現在の Q / パッド Y の "押されている" 状態を保存するように修正
    wasQPressed = (keyStateArr[KEY_INPUT_Q] != 0) || ((input.padInput & PAD_INPUT_Y) != 0);

    // 多数の入力フラグを見て IDLE に戻す判定
    if (currentState != HIT && currentState != DODGE && currentState != ATTACK && !isMoving && currentState != DEAD &&
        !(keyStateArr[KEY_INPUT_UP] != 0) && !(keyStateArr[KEY_INPUT_DOWN] != 0) &&
        !(keyStateArr[KEY_INPUT_X] != 0) && !(keyStateArr[KEY_INPUT_C] != 0) &&
        !(keyStateArr[KEY_INPUT_V] != 0) && !(keyStateArr[KEY_INPUT_A] != 0) &&
        !input.isZPressed) {
        currentState = IDLE;
    }

    // 安全チェック: vy の範囲が壊れていたらリセット
    if (std::isnan(vy) || vy < -POSITION_BOUND_Y || vy > POSITION_BOUND_Y) {
        vy = 0;
    }

    // 重力処理
    if (!isOnGround) {
        if (jumpInitialFramePending) {
            vy += gravity * JUMP_INITIAL_GRAVITY_MULT;
            jumpInitialFramePending = false;
        } else {
            vy += gravity;
        }
    } else {
        vy = 0;
    }
    moveY = vy;

    // 攻撃で押し出す力を逐次適用
    if (attackPushTimer > 0) {
        moveX += attackPushX / static_cast<float>(attackPushTimer);
        attackPushTimer--;
        if (attackPushTimer == 0) attackPushX = 0.0f;
    }

    // マップがない場合は地面にスナップ
    if (!map) {
        if (vy > 0 && y >= groundY) {
            y = groundY;
            vy = 0;
            isOnGround = true;
            jumpCount = 0;
        }
    }

    // アニメーション更新とフレーム切替
    animationTimer++;
    static const int maxFrames[] = { 10, 7, 4, 4, 6, 6, 2, 8 };

    if (currentState == DEAD) {
        if (animationTimer > DEAD_ANIM_TICK) {
            animationFrame++;
            if (animationFrame >= maxFrames[DEAD]) {
                animationFrame = maxFrames[DEAD] - 1;
                GameManager::GetInstance().SetGameState(STATE_GAMEOVER);
            }
            animationTimer = 0;
        }
        return;
    } else {
        if (animationTimer >= DEFAULT_ANIM_TICK) {
            animationFrame = (animationFrame + 1) % maxFrames[currentState];
            animationTimer = 0;
        }

        for (Bullet* b : bullets) if (b && b->isActive) b->Update();

        if (dodgeCooltime > 0) dodgeCooltime--;
        if (attackCooldown > 0) attackCooldown--;
        else hasHitThisAttack = false;
        if (currentState == DODGE) moveX = isFacingRight ? static_cast<float>(PLAYER_DODGE_SPEED) : static_cast<float>(-PLAYER_DODGE_SPEED);

        if (currentState == DODGE && animationFrame == maxFrames[DODGE] - 1) {
            currentState = IDLE;
            animationFrame = 0;
        }
        if (currentState == ATTACK && animationFrame == maxFrames[ATTACK] - 1) {
            currentState = IDLE;
            animationFrame = 0;
        }

        if (currentState == HIT) {
            if (hitEffectTimer > 0) hitEffectTimer--;
            else { currentState = IDLE; animationFrame = 0; }
        }

        // 銃撃エフェクトのクリーンアップ（終了したものを詰める）
        size_t dst = 0;
        for (size_t i = 0; i < gunEffects.size(); ++i) {
            gunEffects[i].Update();
            if (!gunEffects[i].IsFinished()) {
                if (dst != i) gunEffects[dst] = gunEffects[i];
                ++dst;
            }
        }
        if (dst != gunEffects.size()) gunEffects.resize(dst);

        // 弾と敵の衝突処理
        for (Bullet* b : bullets) {
            if (b && b->isActive) {
                b->Update();
                for (auto& enemy : GameManager::GetInstance().GetEnemies()) {
                    if (enemy.IsActive() && GameManager::GetInstance().CheckCollision(b->GetRect(), enemy.GetRect())) {
                        enemy.TakeDamage(b->power);
                        b->isActive = false;
                        break;
                    }
                }
            }
        }
    }
}
