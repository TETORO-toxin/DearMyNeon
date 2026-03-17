/*!
 @file enemy.cpp
 *  @brief 敵 (Enemy) の実装ファイル (core)
 */

#include <Dxlib.h>
#include <cmath>

#include "enemy.h"
#include "game_manager.h"
#include "item.h"
#include "Map.h"
#include "define.h"

int Enemy::s_nextId = 1; // ?G?C???X?^???XID??J?n?l

// ?R???X?g???N?^
//  - ?X?v???C?g???????A?e??p?????[?^?????????s??
Enemy::Enemy(int startX, int startY, int enemyType, int enemyId)
    : x(startX)
    , y(startY)
    , type(enemyType)
    , life((enemyType == 0) ? ENEMY_MELEE_LIFE : ENEMY_RANGED_LIFE)
    , attackCooltime(0)
    , isActive(true)
    , attackEffectTimer(0)
    , previousState(ENEMY_IDLE)
    , currentState(ENEMY_IDLE)
    , animationFrame(0)
    , animationTimer(0)
    , isFacingRight(true)
    , attackStateTimer(0)
{
    idleMoveCooltime = 0;
    isChasing = false;
    vy = 0.0f;

    // Assign id: use provided id if >0, otherwise allocate next unique id
    if (enemyId > 0) {
        id = enemyId;
        if (enemyId >= s_nextId) s_nextId = enemyId + 1;
    } else {
        id = s_nextId++;
    }

    // ?X?v???C?g??^?C?v????????
    if (type == 0) {
        idleGraph   = LoadGraph("graphic/enemy/melee/enemy_melee_idle2.png");
        attackGraph = LoadGraph("graphic/enemy/melee/enemy_melee_attack2.png");
        hitGraph    = LoadGraph("graphic/enemy/melee/enemy_melee_hit.png");
        deadGraph   = LoadGraph("graphic/enemy/melee/enemy_melee_dead.png");
        moveGraph   = LoadGraph("graphic/enemy/melee/enemy_melee_move2.png");
    }
    else {
        idleGraph   = LoadGraph("graphic/enemy/ranged/enemy_ranged_idle2.png");
        attackGraph = LoadGraph("graphic/enemy/ranged/enemy_ranged_attack.png");
        hitGraph    = LoadGraph("graphic/enemy/ranged/enemy_ranged_hit.png");
        deadGraph   = LoadGraph("graphic/enemy/ranged/enemy_ranged_dead.png");
        moveGraph   = LoadGraph("graphic/enemy/ranged/enemy_ranged_move.png");
    }

    exclamationGraph = LoadGraph("graphic/enemy/exclamation.png");

    // ?X?v???C?g???
    meleeSpriteInfoMap[ENEMY_IDLE]   = { 27, 28, 8, false, 8, 0, 0, 0 };
    meleeSpriteInfoMap[ENEMY_MOVE]   = { 24.25f, 27, 8, false, 8, 0, 0, 0 }; 
    meleeSpriteInfoMap[ENEMY_ATTACK] = { 42, 25, 4, false, 4, 0, 0, 0 };
    meleeSpriteInfoMap[ENEMY_HIT]    = { 64, 27, 2, false, 2, 0, 0, 0 };
    meleeSpriteInfoMap[ENEMY_DEAD]   = { 128, 64, 8, false, 8, 0, 0, 0 };

    rangedSpriteInfoMap[ENEMY_IDLE]   = { 32, 24, 4, false, 4, 0, 0, 0 };
    rangedSpriteInfoMap[ENEMY_MOVE]   = { 59, 28, 4, false, 4, 0, 0, 0 };
    rangedSpriteInfoMap[ENEMY_ATTACK] = { 59, 28, 4, false, 4, 0, 0, 0 };
    rangedSpriteInfoMap[ENEMY_HIT]    = { 60, 25, 4, false, 4, 0, 0, 0 };
    rangedSpriteInfoMap[ENEMY_DEAD]   = { 120, 60, 7, false, 7, 0, 0, 0 };

    // ?r?W?????p?????[?^??^?C?v?????????
    SetDefaultVisionForType();
}


// IsActive
//  - 敵が現在アクティブ（存在する／処理対象）かを返す
bool Enemy::IsActive() const
{
    return isActive;
}


// GetAttackRect
//  - 攻撃時（ENEMY_ATTACK）のヒットウィンドウ中は前方に伸びる当たり判定矩形を返す
//  - それ以外のときはスプライト中央に基づく通常の当たり矩形を返す
RECT Enemy::GetAttackRect() const
{
    const float scale = 2.0f;
    SpriteInfo info = (type == 0) ? meleeSpriteInfoMap.at(currentState) : rangedSpriteInfoMap.at(currentState);

    int halfW = static_cast<int>((info.width * scale) / 2.0f);
    int height = static_cast<int>(info.height * scale);

    RECT r;

    // 攻撃アニメーション中だが、実際のヒットウィンドウのみ前方ヒットボックスを返す
    if (currentState == ENEMY_ATTACK) {
        if (attackHitTimer > 0) {
            int range = (type == 0) ? ENEMY_MELEE_ATTACK_RANGE : ENEMY_RANGED_ATTACK_RANGE;

            if (isFacingRight) {
                r.left  = x;
                r.right = x + range;
            }
            else {
                r.left  = x - range;
                r.right = x;
            }

            r.top    = y - height;
            r.bottom = y;

            return r;
        }
        else {
            // ヒットボックス無し
            r.left = r.right = r.top = r.bottom = 0;
            return r;
        }
    }

    // 通常時はスプライト中央に当たり判定を持つ
    r.left   = x - halfW;
    r.top    = y - height;
    r.right  = x + halfW;
    r.bottom = y;

    return r;
}


// GetRect
//  - 現在のスプライトサイズに基づいた敵の当たり矩形を返す
RECT Enemy::GetRect() const
{
    const float scale = 2.0f;
    SpriteInfo info = (type == 0) ? meleeSpriteInfoMap.at(currentState) : rangedSpriteInfoMap.at(currentState);

    int halfW  = static_cast<int>((info.width * scale) / 2.0f);
    int height = static_cast<int>(info.height * scale);

    RECT rect;
    rect.left   = x - halfW;
    rect.right  = x + halfW;
    rect.top    = y - height;
    rect.bottom = y;

    return rect;
}

// UpdateWhenPlayerDead
//  - プレイヤーが死亡した際に敵の攻撃状態やフラグをリセットする
void Enemy::UpdateWhenPlayerDead()
{
    pendingBullet = false;
    attackCooltime = 0;
    attackEffectTimer = 0;
    currentState = ENEMY_IDLE;
}

// TakeDamage
//  - 敵がダメージを受けた際の状態変更（HP減少・ヒットアニメへの遷移）
void Enemy::TakeDamage(int damage)
{
    life = life - damage;
    currentState = ENEMY_HIT;
    animationFrame = 0;
    animationTimer = 0;
    // 短時間のスタンを付与して攻撃を一時停止させる
    stunTimer = 18; // 約0.3秒(60fps換算)のスタン
    // ノックバックを行わないように移動量を調整
    moveX = 0.0f;
}


// SetPosition
//  - 敵の位置を設定する（float を int に丸めて格納）
void Enemy::SetPosition(float nx, float ny)
{
    x = static_cast<int>(nx);
    y = static_cast<int>(ny);
}


// GetMoveX / GetMoveY / GetX / GetY
//  - 移動ベクトルや座標を返す（外部参照用のゲッター）
float Enemy::GetMoveX() const { return moveX; }
float Enemy::GetMoveY() const { return moveY; }
float Enemy::GetX() const     { return static_cast<float>(x); }
float Enemy::GetY() const     { return static_cast<float>(y); }


// GetSaveData
//  - セーブ用構造体に現在の状態を格納して返す
Enemy::EnemySaveData Enemy::GetSaveData() const
{
    EnemySaveData data;
    data.id = id;
    data.ex = x;
    data.ey = y;
    data.eLife = life;
    data.eType = type;
    data.eIsActive = isActive;
    return data;
}


// SetSaveData
//  - ?Z?[?u?f?[?^?????????????B?O???t???????????????s??
void Enemy::SetSaveData(const EnemySaveData& data)
{
    x = data.ex;
    y = data.ey;

    int t = data.eType;
    if (t < 0 || t > 1) t = 0;
    type = t;

    life = data.eLife;
    isActive = data.eIsActive;

    // If incoming save contains a valid id, use it; otherwise ensure we have a unique id
    if (data.id > 0) {
        id = data.id;
        if (data.id >= s_nextId) s_nextId = data.id + 1;
    } else {
        if (id <= 0) id = s_nextId++;
    }

    attackCooltime = 0;
    attackEffectTimer = 0;
    attackStateTimer = 0;
    exclamationTimer = 0;
    bulletDelayTimer = 0;
    pendingBullet = false;

    // ????O???t??j?????????[?h
    if (idleGraph != -1)    { DeleteGraph(idleGraph); idleGraph = -1; }
    if (attackGraph != -1)  { DeleteGraph(attackGraph); attackGraph = -1; }
    if (hitGraph != -1)     { DeleteGraph(hitGraph); hitGraph = -1; }
    if (deadGraph != -1)    { DeleteGraph(deadGraph); deadGraph = -1; }
    if (moveGraph != -1)    { DeleteGraph(moveGraph); moveGraph = -1; }
    if (exclamationGraph != -1) { DeleteGraph(exclamationGraph); exclamationGraph = -1; }

    if (type == 0) {
        idleGraph   = LoadGraph("graphic/enemy/melee/enemy_melee_idle2.png");
        attackGraph = LoadGraph("graphic/enemy/melee/enemy_melee_attack2.png");
        hitGraph    = LoadGraph("graphic/enemy/melee/enemy_melee_hit.png");
        deadGraph   = LoadGraph("graphic/enemy/melee/enemy_melee_dead.png");
        moveGraph   = LoadGraph("graphic/enemy/melee/enemy_melee_move2.png");
    }
    else {
        idleGraph   = LoadGraph("graphic/enemy/ranged/enemy_ranged_idle2.png");
        attackGraph = LoadGraph("graphic/enemy/ranged/enemy_ranged_attack.png");
        hitGraph    = LoadGraph("graphic/enemy/ranged/enemy_ranged_hit.png");
        deadGraph   = LoadGraph("graphic/enemy/ranged/enemy_ranged_dead.png");
        moveGraph   = LoadGraph("graphic/enemy/ranged/enemy_ranged_move.png");
    }

    exclamationGraph = LoadGraph("graphic/enemy/exclamation.png");

    currentState = ENEMY_IDLE;
    previousState = ENEMY_IDLE;
    animationFrame = 0;
    animationTimer = 0;

    SetDefaultVisionForType();
}


// LoadData
//  - セーブデータ読み込みのラッパー（SetSaveData を呼ぶ）
void Enemy::LoadData(const EnemySaveData& data)
{
    SetSaveData(data);
}


// Refactored Update that orchestrates helpers
//  - 毎フレーム呼ばれる更新。AI、アニメ、物理、分離などの処理を順に実行する
void Enemy::Update(int playerX, int playerY)
{
    if (!isActive) return;

    Player& player = GameManager::GetInstance().GetPlayer();
    if (player.IsDead()) { UpdateWhenPlayerDead(); return; }

    Map& map = GameManager::GetInstance().GetMap();
    int tileSize = map.GetTileSize();
    int mapPixelW = map.GetWidth() * tileSize;
    int mapPixelH = map.GetHeight() * tileSize;
    int startX = x; int startY = y;

    // Death handled earlier
    if (life <= 0) {
        currentState = ENEMY_DEAD;
        isActive = false;

        // アイテムドロップ判定
        if (GetRand(100) < ENEMY_DEAD_DROP_RECOVERY_CHANCE) {
            GameManager::GetInstance().AddItem(x, y, ItemType::ITEM_RECOVERY);
        }
        else if (GetRand(100) < ENEMY_DEAD_DROP_BULLET_CHANCE) {
            GameManager::GetInstance().AddItem(x, y, ItemType::ITEM_BULLET);
        }

        // スローモーション再生（死亡時）
        GameManager::GetInstance().TriggerSlowMotion(30, 0.28f, "sound/enemy/衝撃.mp3");
        return;
    }

    // Type-specific AI
    if (type == 0) {
        UpdatePathfindingIfNeeded(playerX, playerY, map);
        UpdateVisionAndChase(player, playerX, playerY, map);
        HandleAttackTelegraph(player, playerX, playerY, map);
    }

    // Idle behavior when not chasing
    if (!isChasing && currentState != ENEMY_DEAD && isActive) HandleIdleBehavior(map);

    // Animation & attack resolution
    UpdateAnimationAndResolveAttacks(player, startX, startY);

    // Gravity & collision snap
    ApplyGravityAndSnap(map, tileSize);

    // Separation from other enemies and player
    SeparateFromEnemies();
    SeparateFromPlayer(player, map, mapPixelH);
}