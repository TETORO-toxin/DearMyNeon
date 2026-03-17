#include "player.h"
#include "game_manager.h"
#include "save_load.h"
#include <DxLib.h>
#include "Map.h"
#include "define.h"
#include <cmath>
#include "bullet.h"

// Player のコア実装: コンストラクタ、基本アクセサ、ダメージ/回復、セーブ/ロード補助

// コンストラクタ: グラフィック・サウンドのロードと弾プールの事前確保を行う
Player::Player()
    : x(PLAYER_SPAWN_X), y(static_cast<float>(PLAYER_SPAWN_Y)), vx(0), vy(0.0f),
      life(PLAYER_DEFAULT_MAX_LIFE), maxLife(static_cast<float>(PLAYER_DEFAULT_MAX_LIFE)),
      bulletCount(PLAYER_MAX_BULLETS), jumpCount(0),
      isOnGround(true), isInvincible(false), invincibleTimer(0),
      dodgeCooltime(0), currentState(IDLE), animationFrame(0),
      animationTimer(0), hitEffectTimer(0), gunEffectGraph(-1),
      attackCooldown(0), invincibleDuration(PLAYER_INVINCIBLE_TIME)
{
    // スプライトやサウンドの読み込み
    idleGraph = LoadGraph("graphic/player/moveing/1_Idle_48x48.png");
    moveGraph = LoadGraph("graphic/player/moveing/2_Run_48x48.png");
    jumpGraph = LoadGraph("graphic/player/moveing/3_Jump_48x48.png");
    fallGraph = LoadGraph("graphic/player/moveing/4_Fall_48x48.png");
    attackGraph = LoadGraph("graphic/player/moveing/5_Attack_131x56.png");
    dodgeGraph = LoadGraph("graphic/player/moveing/6_Dash_112x56.png");
    hitGraph = LoadGraph("graphic/player/moveing/7_Hit_48x48.png");
    deadGraph = LoadGraph("graphic/player/moveing/8_Death_76x48.png");

    jumpSound = LoadSoundMem("sound/player/jump.mp3");
    attackSound = LoadSoundMem("sound/player/斬撃5.mp3");
    hitSound = LoadSoundMem("sound/player/hit.mp3");
    deadSound = LoadSoundMem("sound/player/item_get.mp3");
    // 発砲音
    shootSound = LoadSoundMem("sound/player/発砲1.mp3");

    lifeUITexture = LoadGraph("graphic/UI/life/Life2.png");
    bulletUITexture = LoadGraph("graphic/UI/ammo/Ammo.png");
    // スキルアイコン用テクスチャの読み込み（アイコンが横一列に並んだアトラスを想定）
    skillUITexture = LoadGraph("graphic/UI/skills/Skills.png");
    gunEffectGraph = LoadGraph("graphic/player/shooting/[SHOOT WITH CASING AND MUZZLE FLASH] Glock - P80.png");

    gunEffects.reserve(16);

    // 弾プールのプレアロケーション（ゲーム中の動的確保を減らすため）
    const int PREALLOC_BULLETS = 64;
    bullets.reserve(PREALLOC_BULLETS);
    for (int i = 0; i < PREALLOC_BULLETS; ++i) {
        Bullet* b = new Bullet(-1000, -1000, 0, 0, PLAYER_BULLET_POWER);
        b->isActive = false;
        bullets.push_back(b);
    }
}

void Player::SetMap(Map* m) {
    map = m;
}

int Player::GetX() const { return x; }
int Player::GetY() const { return static_cast<int>(y); }
int Player::GetHeight() const { return PLAYER_HEIGHT; }
void Player::SetOnGround(bool value) { isOnGround = value; }
void Player::ResetJumpCount() { jumpCount = 0; }
void Player::SetVelocityY(float value) { vy = value; }

float Player::GetMoveX() const { return moveX; }
float Player::GetMoveY() const { return moveY; }
bool Player::IsOnGround() const { return isOnGround; }
bool Player::IsDead() const { return currentState == DEAD || life <= 0; }

// SetPosition: 不正な値（NaN や非常に大きな値）を受け取った場合はスポーン地点に戻す安全策
void Player::SetPosition(float nx, float ny) {
    if (std::isnan(nx) || std::isnan(ny) || ny < -POSITION_BOUND_Y || ny > POSITION_BOUND_Y) {
        printf("Invalid SetPosition: x=%.2f, y=%.2f\n", nx, ny);
        x = PLAYER_SPAWN_X; y = static_cast<float>(PLAYER_SPAWN_Y); vy = 0.0f; isOnGround = false;
        return;
    }
    x = static_cast<int>(std::round(nx)); y = ny;
}

// TakeDamage: 無敵時間や演出（スローモーション、ヒットエフェクト）を扱う
void Player::TakeDamage(int damage) {
    if (!isInvincible) {
        life -= damage; if (life < 0) life = 0;
        PlaySoundMem(hitSound, DX_PLAYTYPE_BACK);
        GameManager::GetInstance().TriggerSlowMotion(80, 0.28f, nullptr, 255, 0, 0, 0.6f);
        isInvincible = true; invincibleTimer = 0;
        currentState = HIT; hitEffectTimer = 15; animationFrame = 0;

        // 地面に接地している場合は高さをタイル頂点にスナップして落下を止める
        if (map) {
            int ts = map->GetTileSize();
            int tileX = x / ts;
            int tileY = static_cast<int>(std::round(y)) / ts;
            CollisionType ct = map->GetCollisionTypeAt(tileX, tileY);
            if (ct == CollisionType::Floor) {
                float tileTop = static_cast<float>(tileY) * static_cast<float>(ts);
                y = tileTop; // プレイヤーの y はボトム基準なのでタイル上面に合わせる
                vy = 0.0f;
                isOnGround = true;
                ResetJumpCount();
            }
        }
    }
}

// Heal / AddBullet
void Player::Heal(int amount) {
    life += amount; if (life > static_cast<int>(maxLife)) life = static_cast<int>(maxLife);
}

void Player::AddBullet(int count) {
    bulletCount += count; if (bulletCount > PLAYER_MAX_BULLETS) bulletCount = PLAYER_MAX_BULLETS;
}

// GetRect: プレイヤーの当たり判定矩形を返す（底辺基準）
RECT Player::GetRect() {
    RECT r;
    r.left = x - PLAYER_HALF_WIDTH;
    r.top = static_cast<int>(std::round(y)) - PLAYER_HEIGHT;
    r.right = x + PLAYER_HALF_WIDTH;
    r.bottom = static_cast<int>(std::round(y));
    return r;
}

// セーブデータ取得
Player::PlayerSaveData Player::GetSaveData() const {
    PlayerSaveData data{}; data.px = x; data.py = static_cast<int>(std::round(y)); data.pLife = life; data.pBullet = bulletCount; return data;
}

// セーブデータ適用
void Player::LoadData(const PlayerSaveData& data) {
    x = data.px; y = static_cast<float>(data.py); life = data.pLife; bulletCount = data.pBullet;
    vx = 0; vy = 0.0f; jumpCount = 0; isOnGround = false; isInvincible = false; invincibleTimer = 0; invincibleDuration = PLAYER_INVINCIBLE_TIME; dodgeCooltime = 0;
}

// リバイブ: 保存座標を使うかスポーン地点を使うか選べる
void Player::Revive(bool useSavedPosition) {
    if (useSavedPosition) { x = savedX; y = static_cast<float>(savedY); } else { x = PLAYER_SPAWN_X; y = static_cast<float>(PLAYER_SPAWN_Y); }
    life = static_cast<int>(maxLife); currentState = IDLE; animationFrame = animationTimer = 0; isInvincible = false; invincibleTimer = 0; invincibleDuration = PLAYER_INVINCIBLE_TIME;
    bulletCount = PLAYER_MAX_BULLETS; vx = 0; vy = 0.0f; jumpCount = 0; isOnGround = true;
}

// グラフィック/サウンドのリロード: 既存のハンドルを解放して再読み込みする
void Player::ReloadGraphics() {
    if (idleGraph != -1) { DeleteGraph(idleGraph); idleGraph = -1; }
    if (moveGraph != -1) { DeleteGraph(moveGraph); moveGraph = -1; }
    if (jumpGraph != -1) { DeleteGraph(jumpGraph); jumpGraph = -1; }
    if (fallGraph != -1) { DeleteGraph(fallGraph); fallGraph = -1; }
    if (attackGraph != -1) { DeleteGraph(attackGraph); attackGraph = -1; }
    if (dodgeGraph != -1) { DeleteGraph(dodgeGraph); dodgeGraph = -1; }
    if (hitGraph != -1) { DeleteGraph(hitGraph); hitGraph = -1; }
    if (deadGraph != -1) { DeleteGraph(deadGraph); deadGraph = -1; }
    if (lifeUITexture != -1) { DeleteGraph(lifeUITexture); lifeUITexture = -1; }
    if (bulletUITexture != -1) { DeleteGraph(bulletUITexture); bulletUITexture = -1; }
    if (skillUITexture != -1) { DeleteGraph(skillUITexture); skillUITexture = -1; }
    if (gunEffectGraph != -1) { DeleteGraph(gunEffectGraph); gunEffectGraph = -1; }

    idleGraph = LoadGraph("graphic/player/moveing/1_Idle_48x48.png");
    moveGraph = LoadGraph("graphic/player/moveing/2_Run_48x48.png");
    jumpGraph = LoadGraph("graphic/player/moveing/3_Jump_48x48.png");
    fallGraph = LoadGraph("graphic/player/moveing/4_Fall_48x48.png");
    attackGraph = LoadGraph("graphic/player/moveing/5_Attack_131x56.png");
    dodgeGraph = LoadGraph("graphic/player/moveing/6_Dash_112x56.png");
    hitGraph = LoadGraph("graphic/player/moveing/7_Hit_48x48.png");
    deadGraph = LoadGraph("graphic/player/moveing/8_Death_76x48.png");

    lifeUITexture = LoadGraph("graphic/UI/life/Life2.png");
    bulletUITexture = LoadGraph("graphic/UI/ammo/Ammo.png");
    skillUITexture = LoadGraph("graphic/UI/skills/Skills.png");
    gunEffectGraph = LoadGraph("graphic/player/shooting/[SHOOT WITH CASING AND MUZZLE FLASH] Glock - P80.png");

    if (jumpSound != -1) { DeleteSoundMem(jumpSound); jumpSound = -1; }
    if (attackSound != -1) { DeleteSoundMem(attackSound); attackSound = -1; }
    if (hitSound != -1) { DeleteSoundMem(hitSound); hitSound = -1; }
    if (deadSound != -1) { DeleteSoundMem(deadSound); deadSound = -1; }
    if (shootSound != -1) { DeleteSoundMem(shootSound); shootSound = -1; }

    jumpSound = LoadSoundMem("sound/player/jump.mp3");
    attackSound = LoadSoundMem("sound/player/斬撃5.mp3");
    hitSound = LoadSoundMem("sound/player/hit.mp3");
    deadSound = LoadSoundMem("sound/player/item_get.mp3");
    shootSound = LoadSoundMem("sound/player/発砲1.mp3");
}

// Add demo methods implementations
void Player::SetDemoMode(bool active) { demoMode = active; }
void Player::SetDemoInput(const InputState& in) { demoInput = in; }
