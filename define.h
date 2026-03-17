#ifndef DEFINE_H
#define DEFINE_H

/*!
 * @file define.h
 * @brief ゲーム内で使用する定数や列挙を定義するヘッダ
 *
 * 画面サイズ、タイルサイズ、プレイヤーパラメータ、エネミーパラメータ、
 * 保存ファイル名などのマクロ／定数をここで管理します。
 */

// 画面
constexpr int SCREEN_WIDTH = 1280;                 //!< 画面の幅
constexpr int SCREEN_HEIGHT = 720;                 //!< 画面の高さ

// 汎用リミット
constexpr int POSITION_BOUND_Y = 10000;            //!< Y 座標の安全上限
constexpr int INVALID_COORD = -10000;              //!< 不正座標判定

// マップ / タイル
constexpr int DEFAULT_TILE_SIZE = 32;              //!< デフォルトのタイルサイズ
constexpr int DEFAULT_MAP_WIDTH = 1000;             //!< デフォルトのマップ幅
constexpr int DEFAULT_MAP_HEIGHT = 1000;            //!< デフォルトのマップ高さ

// タイルパレット
constexpr int TILE_PALETTE_TOTAL = 73;             //!< タイルパレットの総タイル数
constexpr int TILE_PALETTE_COLUMNS = 10;           //!< タイルパレットの列数
constexpr int TILE_PALETTE_START_X = 50;           //!< タイルパレットの左上 X 座標
constexpr int TILE_PALETTE_START_Y = 100;          //!< タイルパレットの左上 Y 座標
constexpr int TILE_PALETTE_MARGIN = 5;             //!< パレット外枠余白
constexpr int TILE_PALETTE_INSET = 4;              //!< パレット内描画インセット
constexpr int PALETTE_CLICK_TOLERANCE = 10;        //!< パレットクリック判定の許容誤差

// プレイヤー
constexpr int PLAYER_SPAWN_X = 150;                //!< プレイヤー出現位置 X 座標
constexpr int PLAYER_SPAWN_Y = 380;                //!< プレイヤー出現位置 Y 座標
constexpr float PLAYER_GRAVITY = 0.15f;              //!< プレイヤーの重力 (increased for stronger gravity)
constexpr int PLAYER_JUMP_POWER = -4;              //!< プレイヤーのジャンプ力
constexpr int PLAYER_MOVE_SPEED = 5;               //!< プレイヤーの移動速度 (increased movement speed)
constexpr int PLAYER_ATTACK_PUSH = 35;             //!< プレイヤー攻撃時の敵押し出し力
constexpr int PLAYER_BULLET_SPEED = 10;            //!< プレイヤー弾の移動速度
constexpr int PLAYER_BULLET_POWER = 1;            //!< プレイヤー弾の攻撃力 (reduced to avoid one-shot kills)
constexpr int PLAYER_MAX_BULLETS = 10;             //!< プレイヤーが同時に持てる弾の最大数
constexpr int PLAYER_DEFAULT_MAX_LIFE = 8;        //!< プレイヤーのデフォルト最大ライフ
constexpr int PLAYER_HEIGHT = 48;                  //!< プレイヤーの高さ
constexpr int PLAYER_HALF_WIDTH = 24;              //!< プレイヤーの半分の幅
constexpr int GUN_EFFECT_OFFSET_Y = 48;            //!< 銃撃エフェクトのYオフセット

// Distance the player may fall below their saved checkpoint before being respawned
constexpr int PLAYER_FALL_RESPAWN_DISTANCE = 256;  //!< プレイヤーがチェックポイントより下に落ちた際のリスポーン閾値 (px)

// アイテム描画サイズ / 画像パス
constexpr int ITEM_SIZE = 32;                      //!< アイテムの描画サイズ
constexpr int ITEM_HALF_SIZE = ITEM_SIZE / 2;      //!< アイテムの半分のサイズ
static constexpr const char* ITEM_RECOVERY_IMAGE = "graphic/item/recovery_item.png"; //!< 回復アイテム画像パス
static constexpr const char* ITEM_BULLET_IMAGE = "graphic/UI/ammo/Ammo.png";           //!< 弾薬アイテム画像パス

// 弾（Bullet）
constexpr int BULLET_OFFSCREEN_PADDING = 100;      //!< 弾のオフスクリーン時の余白
constexpr int BULLET_DRAW_HALF = 8;                //!< 弾の描画サイズの半分
static constexpr const char* BULLET_IMAGE_PATH = "graphic/player/shooting/bullet.png"; //!< 弾画像パス
static constexpr const char* ENEMY_BULLET_IMAGE_PATH = "graphic/enemy/ranged/enemy_bullet.png"; //!< エネミー弾画像パス

// エネミー
constexpr float ENEMY_GRAVITY = 0.1f;              //!< エネミーの重力
constexpr float ENEMY_MOVE_STEP_SMALL = 0.5f;      //!< 小型エネミーの移動ステップ
constexpr float ENEMY_MOVE_STEP_LARGE = 1.0f;      //!< 大型エネミーの移動ステップ
constexpr float ENEMY_CHASE_START_DISTANCE = 200.0f; //!< エネミーがプレイヤーを追い始める距離
constexpr float ENEMY_CHASE_STOP_DISTANCE = 250.0f; //!< エネミーがプレイヤーの追跡をやめる距離
constexpr int ENEMY_EXCLAMATION_DURATION = 30;     //!< エネミーの驚きエフェクト持続時間
constexpr int ENEMY_ATTACK_EFFECT_DURATION = 120;  //!< エネミー攻撃エフェクト持続時間
constexpr int ENEMY_BULLET_SPEED = 3;              //!< エネミー弾の移動速度
constexpr int ENEMY_IDLE_MOVE_COOLDOWN = 15;      //!< エネミーのアイドル移動クールダウン
constexpr int ENEMY_HP_BAR_WIDTH = 64;             //!< エネミーHPバーの幅
constexpr int ENEMY_HP_BAR_HEIGHT = 8;             //!< エネミーHPバーの高さ

// Windup frames before attack actually occurs (telegraph duration)
constexpr int ENEMY_ATTACK_WINDUP = 20;            //!< 攻撃の溜めフレーム数（表示/遅延）

// Enemy vision / detection
constexpr int ENEMY_VISION_RADIUS = 200;          //!< 敵の視界半径（ピクセル） (fallback)
constexpr int ENEMY_MELEE_VISION_RADIUS = 120;    //!< 近接敵の視界半径（ピクセル）
constexpr int ENEMY_RANGED_VISION_RADIUS = 300;   //!< 遠距離敵の視界半径（ピクセル）
constexpr int ENEMY_FORGET_SIGHT_FRAMES = 60;     //!< 視界外になってから見失うまでのフレーム数（60=1秒）
constexpr float ENEMY_VIEW_ANGLE_DEG = 90.0f;     //!< 敵の視界角度（度数法）


// アニメーション / UI
constexpr int DEFAULT_ANIM_TICK = 10;              //!< デフォルトのアニメーション更新間隔
constexpr int DEAD_ANIM_TICK = 20;                 //!< 死亡アニメーションの更新間隔

// 保存ファイル名
static constexpr const char* SAVE_FILE_NAME_CSTR = "save.dat"; //!< 保存ファイル名

// 既存の定数（互換性用）
const int PLAYER_SPEED = 5;                        //!< プレイヤーの移動速度（既存定数）
const int GRAVITY = 1;                            //!< 重力（既存定数）
const int JUMP_POWER = 20;                        //!< ジャンプ力（既存定数）
const int MAX_JUMP_COUNT = 2;                    //!< 最大ジャンプ回数（既存定数）
const int PLAYER_MAX_LIFE = 5;                    //!< プレイヤーの最大ライフ（既存定数）
const int PLAYER_INITIAL_BULLET = 10;             //!< プレイヤーの初期弾薬数（既存定数）
const int ENEMY_MELEE_LIFE = 3;                   //!< エネミー（近接攻撃）のライフ（既存定数）
const int ENEMY_RANGED_LIFE = 3;                  //!< エネミー（遠距離攻撃）のライフ（既存定数）
const int PLAYER_INVINCIBLE_TIME = 60;            //!< プレイヤー無敵時間（既存定数）
const int PLAYER_DODGE_INVINCIBLE_TIME = 90;      //!< 回避時の無敵時間（PLAYER_INVINCIBLE_TIME より長め）
const int PLAYER_DODGE_COOLTIME = 120;            //!< プレイヤーダッジクールタイム（既存定数）
constexpr int PLAYER_DODGE_SPEED = 3;                 //!< プレイヤー回避時の横滑り速度（小さくすると滑りが短くなる）
const int ATTACK_DAMAGE_DEFAULT = 1;              //!< デフォルトの攻撃ダメージ（既存定数）

// --- ゲーム状態列挙（互換） ---
enum GameState
{
    STATE_TITLE,
    STATE_TUTORIAL,
    STATE_GAMEPLAY,
    STATE_PAUSE,
    STATE_GAMEOVER,
    STATE_CLEAR
};

// BGM/SE 列挙（互換）
enum BGM_SOUND_TYPE {
    BGM_MAIN,
    BGM_COUNT
};

enum SE_SOUND_TYPE {
    SE_ATTACK,
    SE_JUMP,
    SE_HIT,
    SE_ITEM_GET,
    SE_COUNT
};

// アイテム種別（互換）
enum ItemType {
    ITEM_RECOVERY,
    ITEM_BULLET,
    ITEM_COUNT
};

#endif // DEFINE_H