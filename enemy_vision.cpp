#include "enemy.h"
#include "game_manager.h"
#include "Map.h"
#include <cmath>

// 視界・追跡に関する処理を切り出したファイルです。
// Enemy::SetDefaultVisionForType と Enemy::UpdateVisionAndChase の実装をここに移動します。

void Enemy::SetDefaultVisionForType()
{
    // 敵のタイプに応じて視界パラメータを初期化する。
    // type == 0: 近接型（視界半径が小さく、目のオフセットが低い）
    // それ以外  : 遠距離型（視界半径が大きく、目のオフセットが高い）
    if (type == 0) {
        visionRadius = ENEMY_MELEE_VISION_RADIUS;
        eyeOffsetY   = DEFAULT_TILE_SIZE + 8;
    }
    else {
        visionRadius = ENEMY_RANGED_VISION_RADIUS;
        eyeOffsetY   = DEFAULT_TILE_SIZE / 2;
    }
}

void Enemy::UpdateVisionAndChase(Player& player, int playerX, int playerY, Map& map)
{
    // プレイヤーとの距離をユークリッド距離で計算し、向きも決定する
    double dist = sqrt(pow(static_cast<double>(playerX - x), 2) + pow(static_cast<double>(playerY - y), 2));
    isFacingRight = (playerX > x);

    // 視線が遮られているか（壁による遮蔽）を判定する
    bool hasLineOfSight = true;
    try {
        Map &m = GameManager::GetInstance().GetMap();
        int eyeX = x; int eyeY = y - eyeOffsetY;
        int playerEyeX = playerX; int playerEyeY = playerY - (DEFAULT_TILE_SIZE/2);
        if (m.IsWallBetween(eyeX, eyeY, playerEyeX, playerEyeY)) hasLineOfSight = false;
    } catch (...) { hasLineOfSight = true; } // エラー時は可視と仮定

    // 視野内かどうか（視界半径と視野角で判定）
    bool withinFOV = false;
    if (dist <= visionRadius) {
        float dx = static_cast<float>(playerX - x);
        float dy = static_cast<float>(playerY - y);
        float angleToPlayer = atan2f(dy, dx) * 180.0f / 3.14159265f;
        float facingAngle = isFacingRight ? 0.0f : 180.0f;
        float delta = fabsf(fmodf(angleToPlayer - facingAngle + 540.0f, 360.0f) - 180.0f);
        if (delta <= viewAngleDeg * 0.5f) withinFOV = true;
    }

    // 初めて見つけたら追跡開始、見えている間は視認タイマーをリセット
    if (!isChasing && withinFOV && hasLineOfSight) {
        isChasing = true; playerInSight = true; forgetSightTimer = ENEMY_FORGET_SIGHT_FRAMES;
    }
    if (isChasing && withinFOV && hasLineOfSight) { playerInSight = true; forgetSightTimer = ENEMY_FORGET_SIGHT_FRAMES; }

    // 視認を失った場合の処理: タイマーで一定時間保持し、その後追跡停止や攻撃状態のキャンセルを行う
    if (!(withinFOV && hasLineOfSight)) {
        playerInSight = false;
        if (forgetSightTimer > 0) forgetSightTimer--;
        else {
            isChasing = false;
            if (attackPending) {
                // 攻撃準備中ならキャンセルして状態をリセット
                attackPending = false; pendingBullet = false; attackWindupTimer = 0;
                if (currentState == ENEMY_ATTACK) currentState = ENEMY_IDLE;
                attackEffectTimer = 0;
            }
        }
    }

    // 距離しきい値による追跡トグル
    if (!isChasing && dist < ENEMY_CHASE_START_DISTANCE) isChasing = true;
    if (isChasing && dist > ENEMY_CHASE_STOP_DISTANCE) isChasing = false;

    // プレイヤーが近ければ「！」アイコンを表示するタイマーをセット
    if (dist < 100 && exclamationTimer == 0) exclamationTimer = ENEMY_EXCLAMATION_DURATION;
    if (exclamationTimer > 0) exclamationTimer--;

    // 近接型の移動処理（追跡中はプレイヤー方向へ水平移動）
    if (type == 0 && isChasing) {
        if (currentState != ENEMY_ATTACK && currentState != ENEMY_HIT && currentState != ENEMY_DEAD) currentState = ENEMY_MOVE;
        float dx = (playerX < x) ? -ENEMY_MOVE_STEP_LARGE : (playerX > x) ? ENEMY_MOVE_STEP_LARGE : 0.0f;
        float nextX = static_cast<float>(x) + dx;
        int tileSize = map.GetTileSize();
        int tileXOnly = static_cast<int>(nextX) / tileSize;
        int tileYCurrent = static_cast<int>(y) / tileSize;
        // マップの歩行可能判定と当たり判定チェックを行い、移動を確定する
        if (map.IsWalkable(tileXOnly, tileYCurrent, static_cast<float>(y), static_cast<float>(y)) && CanMoveToPos(nextX, static_cast<float>(y), map)) x = static_cast<int>(nextX);
        if (currentState != previousState) { animationFrame = 0; animationTimer = 0; previousState = currentState; }
    }
}
