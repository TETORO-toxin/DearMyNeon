#include "enemy.h"
#include "Map.h"
#include "game_manager.h"
#include <cmath>

// ローカルな予測エイム用定数（グローバルシンボルと衝突しないようにここで定義）
static constexpr float ENEMY_PLAYER_PREDICT_FRAMES = 6.0f;
static constexpr int ENEMY_PLAYER_AHEAD_OFFSET = 24;

// 徘徊（アイドル）時の挙動管理
void Enemy::HandleIdleBehavior(Map& map)
{
    if (idleMoveCooltime <= 0) {
        int r = GetRand(99);
        if (r < 60) idleMoveDirection = (GetRand(1) == 0) ? -1 : 1; else idleMoveDirection = 0;
        idleMoveCooltime = ENEMY_IDLE_MOVE_COOLDOWN * (1 + GetRand(3));
    } else idleMoveCooltime--;

    if (idleMoveDirection != 0) {
        if (currentState != ENEMY_ATTACK && currentState != ENEMY_HIT && currentState != ENEMY_DEAD) currentState = ENEMY_MOVE;
        isFacingRight = (idleMoveDirection > 0);
        float step = ENEMY_MOVE_STEP_SMALL; moveX += idleMoveDirection * step;
        int intMove = static_cast<int>(std::floor(std::abs(moveX))) * (moveX < 0 ? -1 : 1);
        if (intMove != 0) {
            int tileSize = map.GetTileSize();
            float nextXf = static_cast<float>(x) + static_cast<float>(intMove);
            int tileXOnly = static_cast<int>(nextXf) / tileSize; int tileYCurrent = static_cast<int>(y) / tileSize;
            if (map.IsWalkable(tileXOnly, tileYCurrent, static_cast<float>(y), static_cast<float>(y)) && CanMoveToPos(nextXf, static_cast<float>(y), map)) {
                x = static_cast<int>(nextXf); moveX -= static_cast<float>(intMove); idleMoveStallTimer = 6;
            } else { idleMoveCooltime = ENEMY_IDLE_MOVE_COOLDOWN * 2; idleMoveDirection = 0; moveX = 0.0f; idleMoveStallTimer = 0; }
        }
    } else { if (idleMoveStallTimer > 0) idleMoveStallTimer--; }
}

// アニメーション更新と攻撃解決（攻撃実行/弾発射のタイミング）
void Enemy::UpdateAnimationAndResolveAttacks(Player& player, int startX, int startY)
{
    if (currentState != previousState) { animationFrame = 0; animationTimer = 0; previousState = currentState; }
    if (currentState == ENEMY_MOVE) { if (x == startX && y == startY && idleMoveStallTimer <= 0) currentState = ENEMY_IDLE; }
    if ((x != startX || y != startY) && currentState != ENEMY_ATTACK && currentState != ENEMY_HIT && currentState != ENEMY_DEAD) currentState = ENEMY_MOVE;

    SpriteInfo info = (type == 0) ? meleeSpriteInfoMap.at(currentState) : rangedSpriteInfoMap.at(currentState);
    animationTimer++;
    int tickLimit = DEFAULT_ANIM_TICK * (currentState == ENEMY_ATTACK ? ATTACK_ANIM_TICK_MULTIPLIER : 1);

    if (animationTimer >= tickLimit) {
        animationTimer = 0;
        if (currentState == ENEMY_IDLE || currentState == ENEMY_MOVE) {
            // ループするアニメーションフレーム
            animationFrame = (animationFrame + 1) % info.frameCount;
        } else if (currentState == ENEMY_ATTACK) {
            // 攻撃アニメーション: ウィンドアップが終わったら攻撃を実行
            if (animationFrame < info.frameCount - 1) animationFrame++;
            if (attackWindupTimer > 0) --attackWindupTimer;
            else {
                if (!attackExecuted) {
                    attackExecuted = true;
                    if (type == 0) { ProcessMeleeAttack(player); }
                    else {
                        // 遠距離攻撃: プレイヤーの移動を予測して弾道を決定する
                        float predictFrames = ENEMY_PLAYER_PREDICT_FRAMES;
                        float playerMoveX = player.GetMoveX();
                        float predictedPlayerX = player.GetX() + playerMoveX * predictFrames;
                        float facingDir = 0.0f;
                        if (playerMoveX > 0.01f) facingDir = 1.0f;
                        else if (playerMoveX < -0.01f) facingDir = -1.0f;
                        else facingDir = (player.GetX() >= static_cast<float>(x)) ? 1.0f : -1.0f;
                        predictedPlayerX += facingDir * ENEMY_PLAYER_AHEAD_OFFSET;
                        float predictedPlayerY = player.GetY() - 36.0f;
                        float angle = atan2(predictedPlayerY - static_cast<float>(y), predictedPlayerX - static_cast<float>(x));
                        float speed = ENEMY_BULLET_SPEED;
                        int bulletVx = static_cast<int>(cos(angle) * speed);
                        int bulletVy = static_cast<int>(sin(angle) * speed);
                        int bulletStartX = x; int bulletStartY = y - DEFAULT_TILE_SIZE;
                        GameManager::GetInstance().AddEnemyBullet(bulletStartX, bulletStartY, bulletVx, bulletVy, ATTACK_DAMAGE_DEFAULT);
                    }
                }
            }
        } else { if (animationFrame < info.frameCount - 1) animationFrame++; }
    }

    if (attackHitTimer > 0) --attackHitTimer;
    if (damageCooldownTimer > 0) --damageCooldownTimer;
    if (attackHitTimer <= 0 && currentState == ENEMY_ATTACK) { currentState = isChasing ? ENEMY_MOVE : ENEMY_IDLE; animationFrame = 0; animationTimer = 0; }
}
