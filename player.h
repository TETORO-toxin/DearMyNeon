#pragma once

/*!
 * @file player.h
 * @brief プレイヤー関連の宣言
 *
 * プレイヤーの状態、入力、当たり判定、UI 表示、セーブ/ロードなどを定義します。
 * 各メソッド・メンバに日本語コメントを追加して役割を明確にしています。
 */

#include "DxLib.h"
#include "define.h"
#include <vector>
#include <algorithm>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

// 前方宣言
class Enemy;
class Bullet;
class Map;

// プレイヤーの予測射撃やエフェクト用定数
static constexpr float PLAYER_PREDICT_FRAMES = 6.0f; // 予測フレーム数
static constexpr int PLAYER_AHEAD_OFFSET = 12;      // 予測で前に補正するピクセル数

// プレイヤー状態列挙
enum PlayerState {
    IDLE, MOVE, JUMP, FALL, ATTACK, DODGE, HIT, DEAD
};

/**
 * @brief 銃撃エフェクトの簡易表現
 * 小さな構造体で、Update/Draw を呼んでアニメーションさせる用途
 */
class GunEffect {
public:
    int x = 0;
    int y = 0;
    int frame = 0;
    int timer = 0;
    bool isFacingRight = true;

    static const int FRAME_COUNT = 10;
    static const int FRAME_INTERVAL = 5;
    static const int DURATION = FRAME_COUNT * FRAME_INTERVAL;

    GunEffect(int startX = 0, int startY = 0, bool facingRight = true)
        : x(startX), y(startY), frame(0), timer(0), isFacingRight(facingRight) {}

    // 毎フレーム更新
    void Update() {
        ++timer;
        if ((timer % FRAME_INTERVAL) == 0 && frame < FRAME_COUNT) ++frame;
    }

    bool IsFinished() const { return frame >= FRAME_COUNT; }

    // 描画ヘルパー: カメラオフセットを渡して描画する
    void Draw(int imageHandle, int camOffsetX, int camOffsetY) const {
        const int SPRITE_SIZE = 64;
        if (imageHandle == -1) return; // 何も描画しない
        if (frame < FRAME_COUNT) {
            int srcX = frame * SPRITE_SIZE;
            int srcY = 0;
            int drawX = x - camOffsetX;
            int drawY = y - camOffsetY;
            DrawRectGraph(
                drawX, drawY,
                srcX, srcY,
                SPRITE_SIZE, SPRITE_SIZE,
                imageHandle,
                TRUE,
                isFacingRight ? FALSE : TRUE
            );
        }
    }
};

/**
 * @brief プレイヤー本体クラス宣言
 *
 * 役割:
 *  - 入力処理と状態遷移
 *  - 移動・ジャンプ・回避・攻撃などのロジック
 *  - 当たり判定（GetRect, GetAttackRect）
 *  - UI 表示 (ライフ、弾数)
 *  - セーブ/ロード用データの取得/適用
 */
class Player {
public:
    Player();

    // 位置、サイズなどのアクセサ
    int GetX() const;
    int GetY() const;
    int GetHeight() const;
    bool IsOnGround() const;

    // 移動量取得
    float GetMoveX() const;
    float GetMoveY() const;

    // 毎フレーム更新と描画
    void Update();
    void Draw();

    // 位置・速度の制御
    void SetPosition(float x, float y);
    void SetVelocityY(float value);
    void SetOnGround(bool value);
    void ResetJumpCount();

    // 当たり判定・攻撃処理
    RECT GetRect();
    RECT GetAttackRect() const;
    void ProcessMeleeAttack(std::vector<Enemy>& enemies);

    // ライフや弾薬管理
    void TakeDamage(int damage);
    void Heal(int amount);
    void AddBullet(int count);

    // セーブ/ロード用データ構造
    struct PlayerSaveData {
        int px = PLAYER_SPAWN_X;
        int py = PLAYER_SPAWN_Y;
        int pLife = 0;
        int pBullet = 0;
    };
    PlayerSaveData GetSaveData() const;
    void LoadData(const PlayerSaveData& data);

    // リスポーン処理
    void Revive(bool useSavedPosition = false);

    // UI 描画
    void DrawLifeUI() const;
    void DrawBulletIcons() const;
    void DrawSkillIcons() const;

    // グラフィック/音声再読み込み
    void ReloadGraphics();

    // マップ参照を設定
    void SetMap(Map* m);

    bool IsDead() const;

    // 銃撃エフェクトの蓄積
    std::vector<GunEffect> gunEffects;

    // 入力状態保持（フレーム間エッジ検出用）
    bool wasQPressed = false;
    bool jumpKeyPressed = false;
    bool attackKeyPressed = false;
    bool dodgeKeyPressed = false;
    bool isFacingRight = true;

    // 表示・エフェクト関連ハンドル
    int gunEffectGraph = -1;
    int hitEffectTimer = 0;

    // セーブ位置
    int savedX = PLAYER_SPAWN_X;
    int savedY = PLAYER_SPAWN_Y;

    // 前フレームのパッド入力状態
    int prevPadInput = 0;

    // 入力処理を分離するための構造体
    struct InputState {
        int padInput = 0;               // パッド入力の生値
        bool isQJustPressed = false;    // Q（撃つ）ボタンの瞬間押下
        bool isWJustPressed = false;    // ジャンプボタンの瞬間押下
        bool isXJustPressed = false;    // 攻撃ボタンの瞬間押下
        bool isZJustPressed = false;    // 回避ボタンの瞬間押下
        bool isXPressed = false;        // 攻撃ボタンが押されているか
        bool isZPressed = false;        // 回避ボタンが押されているか
    };

    // 入力処理を外部ファイルに切り出すための関数
    InputState ProcessInput();

    // キー状態配列を保持（Update で参照されるためメンバー化）
    char keyStateArr[256];

    // Demo mode support: allow external code to supply synthetic input for tutorials
    void SetDemoMode(bool active);
    void SetDemoInput(const InputState& in);

    // 現在のプレイヤー状態を外部から参照するためのアクセサ
    PlayerState GetState() const { return currentState; }

private:
    // 位置・速度
    int x = PLAYER_SPAWN_X;
    int vx = 0;
    float moveX = 0.0f;
    float moveY = 0.0f;
    float y = static_cast<float>(PLAYER_SPAWN_Y);
    float vy = 0.0f;

    // ライフ・弾薬
    int life = PLAYER_DEFAULT_MAX_LIFE;
    float maxLife = static_cast<float>(PLAYER_DEFAULT_MAX_LIFE);

    int bulletCount = PLAYER_MAX_BULLETS;
    int jumpCount = 0;

    // 状態フラグ
    bool isOnGround = true;
    bool isInvincible = false;
    int invincibleTimer = 0;
    int invincibleDuration = PLAYER_INVINCIBLE_TIME; // duration (frames) for current invincibility source
    int dodgeCooltime = 0;

    PlayerState currentState = IDLE;
    int animationFrame = 0;
    int animationTimer = 0;

    // グラフィック/サウンドハンドル
    int idleGraph = -1, moveGraph = -1, jumpGraph = -1, fallGraph = -1;
    int attackGraph = -1, dodgeGraph = -1, hitGraph = -1, deadGraph = -1;
    int lifeUITexture = -1, bulletUITexture = -1;
    int skillUITexture = -1;
    int jumpSound = -1, attackSound = -1, hitSound = -1, deadSound = -1;
    int shootSound = -1;

    std::vector<Bullet*> bullets;
    Map* map = nullptr;

    // 攻撃フラグなど
    bool hasHitThisAttack = false;
    int attackCooldown = 0;
    const int attackCooldownMax = 30;
    const int meleeAttackCooldown = 40; // shorter cooldown (frames) applied when a melee attack hits
    float attackPushX = 0.0f;
    int attackPushTimer = 0;

    // 水平方向の加速管理（スムージング）
    float horizSpeedCurrent = 0.0f;
    int prevHorizDir = 0;
    bool prevHorizMoving = false;
    bool jumpInitialFramePending = false; // ジャンプ直後の特別な重力挙動フラグ

    // Demo input storage
    bool demoMode = false;
    InputState demoInput;
};
