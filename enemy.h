#ifndef ENEMY_H
#define ENEMY_H

/*!
 * @file enemy.h
 * @brief 敵キャラクターを表す `Enemy` クラス宣言
 *
 * `Enemy` は近接型/遠距離型の挙動を持ち、`Update()` / `Draw()` で AI と描画を行う。
 * 敵は自身でパスファインディングや攻撃タイミング管理などを行う。
 */

#include "DxLib.h"
#include "define.h"

#include <map>
#include <vector>
#include <cmath>

// Shared constants moved to header so each translation unit can use them
#ifndef ENEMY_MELEE_ATTACK_RANGE
constexpr int ENEMY_MELEE_ATTACK_RANGE = 80;
#endif

#ifndef ENEMY_RANGED_ATTACK_RANGE
constexpr int ENEMY_RANGED_ATTACK_RANGE = 200;
#endif

static constexpr int TELEGRAPH_BASE_RADIUS = 16;
static constexpr int TELEGRAPH_EXTRA_RADIUS = 40;
static constexpr int TELEGRAPH_DURATION_FRAMES = 60;
static constexpr int ATTACK_ANIM_TICK_MULTIPLIER = 3;
// predictive aiming constants are defined locally in enemy implementation files to avoid conflicts
static constexpr int DEFAULT_DAMAGE = 1;
static constexpr int DAMAGE_COOLDOWN_FRAMES = 30;
static constexpr int ENEMY_DEAD_DROP_RECOVERY_CHANCE = 30;
static constexpr int ENEMY_DEAD_DROP_BULLET_CHANCE = 10;
static constexpr int ATTACK_COOLDOWN_FRAMES = 120;
static constexpr int ATTACK_STATE_DURATION = 80;
static constexpr int TELEGRAPH_SHRINK_FRAMES = TELEGRAPH_DURATION_FRAMES;
// ATTACK_HIT_WINDOW_TICKS should align with animation tick; use DEFAULT_ANIM_TICK from define.h
static constexpr int ATTACK_HIT_WINDOW_TICKS = DEFAULT_ANIM_TICK;

// 前方宣言（循環 include を避ける）
class Player;
class Map;

class Enemy
{
public:
    enum PathfindingMode {
        PATH_4DIR = 4,
        PATH_8DIR = 8
    };

    // --- コンストラクタ / デストラクタ ---
    Enemy(int startX, int startY, int enemyType, int enemyId = -1);
    ~Enemy() = default;

    // --- 公開 API ---
    void Update(int playerX, int playerY);
    void Draw() const;

    void TakeDamage(int damage);
    void ProcessMeleeAttack(Player& player);

    bool IsActive() const;

    RECT GetRect() const;
    RECT GetAttackRect() const;

    float GetX() const;
    float GetY() const;
    float GetMoveX() const;
    float GetMoveY() const;
    void  SetPosition(float x, float y);

    // --- セーブ / ロード ---
    struct EnemySaveData {
        int id;       // unique id for this enemy instance
        int ex;       // x position (tile or pixel depending on usage)
        int ey;       // y position
        int eLife;    // remaining life
        int eType;    // enemy type
        bool eIsActive;
    };

    EnemySaveData GetSaveData() const;
    void SetSaveData(const EnemySaveData& data);
    void LoadData(const EnemySaveData& data);

    // --- 外部から参照される可能性のある状態 ---
    int  attackStateTimer = 0;
    int  exclamationTimer = 0;
    int  bulletDelayTimer = 0;
    bool pendingBullet = false;

    // 攻撃関連タイマー / フラグ
    int  damageCooldownTimer   = 0; // frames remaining before this enemy can damage player again
    int  attackWindupTimer     = 0; // frames remaining until actual attack (windup)
    int  attackWindupTotal     = 0; // total frames of windup when started
    bool attackPending         = false;

    int  attackTelegraphTimer  = 0; // visible telegraph (shrink circle) timer
    int  attackTelegraphTotal  = 0;

    int  attackHitTimer        = 0; // active hit window after attack executes
    bool attackExecuted        = false;

    // スタン用タイマー: ノックバックではなく一時的に行動不可にするためのフレーム数
    int  stunTimer             = 0;

    void ExecuteMeleeAttack(Player& player);

    int idleMoveDirection = 0; // -1:left, 1:right, 0:none
    int idleMoveStallTimer = 0; // frames to keep MOVE state to prevent flicker

private:
    // --- 補助構造体 ---
    struct SpriteInfo {
        float width = 0.0f;
        float height = 0.0f;
        int frameCount = 0;
        bool vertical = false; // true if frames are stacked vertically; false means horizontal strip
        int columns = 0;       // number of columns in sprite sheet for this animation (0 = auto as single row)
        float frameSpacing = 0.0f;  // pixel spacing between frames (can be fractional)
        float srcOffsetX = 0.0f;    // pixel offset in source image where first frame begins (can be fractional)
        float srcOffsetY = 0.0f;    // pixel offset in source image where first frame begins (can be fractional)
    };

    // --- 基本情報 ---
    int x    = 0;
    int y    = 0;
    int type = 0; // 0: melee, 1: ranged
    int life = 0;

    // --- AI / 状態 ---
    bool isChasing = false;
    int  idleMoveCooltime = 0;

    enum EnemyState {
        ENEMY_IDLE = 0,
        ENEMY_MOVE,
        ENEMY_ATTACK,
        ENEMY_HIT,
        ENEMY_DEAD
    };

    EnemyState previousState = ENEMY_IDLE;
    EnemyState currentState  = ENEMY_IDLE;

    std::map<EnemyState, SpriteInfo> meleeSpriteInfoMap;
    std::map<EnemyState, SpriteInfo> rangedSpriteInfoMap;

    int  animationFrame = 0;
    int  animationTimer = 0;

    bool isFacingRight = true;

    int  attackCooltime   = 0;
    int  attackEffectTimer = 0;

    bool isActive = true;

    // --- グラフィックハンドル（未ロード時は -1） ---
    int idleGraph        = -1;
    int attackGraph      = -1;
    int hitGraph         = -1;
    int deadGraph        = -1;
    int exclamationGraph = -1;
    int moveGraph        = -1;

    // --- 物理 / 移動 ---
    float vy = 0.0f;
    float moveX = 0.0f;
    float moveY = 0.0f;

    // Pathfinding
    std::vector<std::pair<int,int>> path; // list of tile coords (tx, ty)
    size_t pathIndex = 0;
    int    pathRecalcTimer = 0; // frames until next path recalculation
    static constexpr int PATH_RECALC_INTERVAL = 30; // recalc every 30 frames

    int id = -1; // unique id
    static int s_nextId; // defined in cpp

    // Vision / detection
    int  visionRadius = ENEMY_VISION_RADIUS;
    bool playerInSight = false;
    int  forgetSightTimer = 0; // frames until enemy gives up chase after losing sight
    float viewAngleDeg = ENEMY_VIEW_ANGLE_DEG; // field of view angle in degrees
    int  eyeOffsetY = DEFAULT_TILE_SIZE / 2; // eye height offset from y (tile bottom)

    // Physics
    bool isOnGround = false; // whether enemy is considered on ground for gravity

    // Pathfinding mode: allow per-enemy choice of 4- or 8-directional neighbors
    PathfindingMode pathMode = PATH_4DIR;

    // --- 内部ヘルパーメソッド ---
    void SetDefaultVisionForType();

    bool CanMoveToPos(float nx, float ny, Map& map) const;
    void UpdateWhenPlayerDead();
    void UpdatePathfindingIfNeeded(int playerX, int playerY, Map& map);
    void UpdateVisionAndChase(Player& player, int playerX, int playerY, Map& map);
    void HandleAttackTelegraph(Player& player, int playerX, int playerY, Map& map);
    void HandleIdleBehavior(Map& map);
    void UpdateAnimationAndResolveAttacks(Player& player, int startX, int startY);
    void ApplyGravityAndSnap(Map& map, int tileSize);
    void SeparateFromEnemies();
    void SeparateFromPlayer(Player& player, Map& map, int mapPixelH);

public:
    // Getter/setter for pathfinding mode
    void SetPathfindingMode(PathfindingMode m) { pathMode = m; }
    PathfindingMode GetPathfindingMode() const { return pathMode; }
};

#endif // ENEMY_H