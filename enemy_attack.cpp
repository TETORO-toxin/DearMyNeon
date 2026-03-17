#include "enemy.h"
#include "game_manager.h"
#include "Map.h"
#include <cmath>

// 敵の攻撃/予告関連処理
// 攻撃予告（テレグラフ）、攻撃確定、近接攻撃判定などを扱う

void Enemy::HandleAttackTelegraph(Player& player, int playerX, int playerY, Map& map)
{
    // 攻撃クールダウン中の処理
    if (attackCooltime > 0) {
        attackCooltime--; if (type == 0 && attackEffectTimer > 0) attackEffectTimer--;
        attackPending = false; pendingBullet = false;
        if (currentState == ENEMY_ATTACK) { currentState = isChasing ? ENEMY_MOVE : ENEMY_IDLE; animationFrame = 0; animationTimer = 0; }
    }

    // クールダウンが終わっていて攻撃が予定されていない場合、射程や可視性をチェックして予告を開始する
    if (attackCooltime == 0 && !attackPending) {
        double dist = sqrt(pow(static_cast<double>(playerX - x), 2) + pow(static_cast<double>(playerY - y), 2));
        if (type == 0 && dist < ENEMY_MELEE_ATTACK_RANGE) {
            // 近接攻撃の予告を設定
            attackPending = true; attackTelegraphTimer = TELEGRAPH_DURATION_FRAMES; attackTelegraphTotal = attackTelegraphTimer; currentState = ENEMY_MOVE;
        } else if (type == 1 && dist < ENEMY_RANGED_ATTACK_RANGE && !map.IsWallBetween(x, y, playerX, playerY)) {
            // 遠距離攻撃（弾）の予告を設定
            attackPending = true; attackTelegraphTimer = TELEGRAPH_DURATION_FRAMES; attackTelegraphTotal = attackTelegraphTimer; currentState = ENEMY_MOVE; pendingBullet = true;
        }
    }

    // 予告中の更新: プレイヤーの状況によってキャンセルしたり、テレグラフ終了で攻撃を実行する
    if (attackTelegraphTotal > 0 && attackTelegraphTimer > 0) {
        --attackTelegraphTimer; if (attackTelegraphTimer < 0) attackTelegraphTimer = 0;
        bool wasPending = attackPending;
        if (attackPending) {
            double curDist = sqrt(pow(static_cast<double>(playerX - x), 2) + pow(static_cast<double>(playerY - y), 2));
            bool shouldCancel = false;
            if (type == 0) { if (curDist > ENEMY_MELEE_ATTACK_RANGE) shouldCancel = true; }
            else { if (curDist > ENEMY_RANGED_ATTACK_RANGE) shouldCancel = true; else if (map.IsWallBetween(x, y, playerX, playerY)) shouldCancel = true; }
            if (shouldCancel) { attackPending = false; pendingBullet = false; attackTelegraphTimer = 0; attackTelegraphTotal = 0; attackEffectTimer = 0; }
        }
        // テレグラフが終了したら攻撃ステートへ移行し、ヒットウィンドウを開始する
        if (attackTelegraphTimer <= 0 && wasPending) {
            currentState = ENEMY_ATTACK; animationFrame = 0; animationTimer = 0;

            attackExecuted = false;
            attackHitTimer = ATTACK_HIT_WINDOW_TICKS;

            attackWindupTimer = 1;

            attackPending = false;
            pendingBullet = false;

            attackCooltime = ATTACK_COOLDOWN_FRAMES;
            attackStateTimer = ATTACK_STATE_DURATION;
        }
    }
}

// 近接攻撃の当たり判定処理
void Enemy::ProcessMeleeAttack(Player& player)
{
    if (attackHitTimer <= 0 || attackExecuted) return;

    RECT attackRect = GetAttackRect();

    if (GameManager::GetInstance().CheckCollision(attackRect, player.GetRect())) {
        player.TakeDamage(DEFAULT_DAMAGE);
        attackExecuted = true;

        attackHitTimer = 0;

        damageCooldownTimer = DAMAGE_COOLDOWN_FRAMES;
    }
}

// 即時実行の近接攻撃（呼び出し側が当たりを取る場合に使用）
void Enemy::ExecuteMeleeAttack(Player& player)
{
    RECT ar = GetAttackRect();
    if (GameManager::GetInstance().CheckCollision(ar, player.GetRect())) player.TakeDamage(1);
}
