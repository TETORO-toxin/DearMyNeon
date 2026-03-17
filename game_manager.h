/*!
 * @file game_manager.h
 * @brief ゲーム全体を管理するシングルトン GameManager の宣言
 *
 * GameManager は以下を統括します:
 *  - シーン遷移（タイトル、ゲームプレイ、ゲームオーバー等）
 *  - リソースの読み込み/解放
 *  - プレイヤー・敵・弾・アイテム等のエンティティ管理
 *  - 非同期ロード進行管理とロード画面の表示
 *  - カメラ、エディットモード、セーブポイントなどの統合
 */

#include "DxLib.h"
#include "define.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "item.h"
#include "save_load.h"
#include "TitleScene.h"
#include "gameover_scene.h"
#include "gameclear_scene.h"
#include <vector>
#include <string>
#include <algorithm>
#include <memory>       // <- unique_ptr
#include "EnemySpawner.h"
#include "RoomManager.h"
#include "SavePoint.h"
#include "Map.h"
#include "TilePalette.h"
#include "EditModeManager.h"
#include "Camera.h"
#include "RespawnLine.h"
#include <thread>
#include <atomic>
#include <unordered_map>

#include "TutorialScene.h"

// ヘルパー関数群（リソースロードのエラーを通知するなど）
inline int SafeLoadSound(const char* path) {
    int h = LoadSoundMem(path);
    if (h == -1) {
        char buf[256]; sprintf_s(buf, "Sound load failed: %s", path);
        MessageBox(NULL, buf, "Error", MB_OK);
    }
    return h;
}
inline int SafeLoadGraph(const char* path) {
    int h = LoadGraph(path);
    if (h == -1) {
        char buf[256]; sprintf_s(buf, "Graph load failed: %s", path);
        MessageBox(NULL, buf, "Error", MB_OK);
    }
    return h;
}
inline float ClampFloat(float v, float lo, float hi) {
    if (v < lo) return lo; if (v > hi) return hi; return v;
}
inline int& EnemyBulletGraphHandle() {
    static int s_handle = -1;
    if (s_handle == -1) s_handle = SafeLoadGraph("enemy_bullet.png");
    return s_handle;
}

void HandleWindowModeToggle(); // ウィンドウモード切替用の古いフリーファンクション宣言

const int TILE_SIZE = 32;

class GameManager
{
private:
    // シングルトン実体
    static GameManager* s_instance;

    // マップや部屋、スポーナーなどの管理オブジェクト
    std::vector<SavePoint> savePoints;
    RoomManager roomManager;
    EnemySpawner enemySpawner;

    // 所有オブジェクトは unique_ptr で管理し、ライフサイクルを明確化
    std::unique_ptr<TilePalette> tilePalette = nullptr;
    std::unique_ptr<Map> map = nullptr;

    // コンストラクタ/デストラクタは private にしてシングルトン化
    GameManager();
    ~GameManager();
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    // 各シーン（タイトル等）は unique_ptr で所有
    std::unique_ptr<TitleScene> titleScene;
    std::unique_ptr<GameOverScene> gameOverScene;
    std::unique_ptr<GameClearScene> gameClearScene;

    // Tutorial scene
    std::unique_ptr<TutorialScene> tutorialScene;

    Camera camera;

    // 現在のゲーム状態（タイトル/プレイ/ゲームオーバー等）
    GameState currentGameState = STATE_TITLE;

    // 非同期読み込み用の状態管理
    std::thread loadingThread; // DxLib と相性が悪いため注意して扱う
    std::atomic<bool> loadingInProgress{false};
    std::atomic<bool> loadingDone{false};
    std::atomic<float> loadingProgress{0.0f};
    int loadingStep = 0; // メインスレッドでのインクリメンタル読み込みステップ
    bool loadingFailed = false;
    std::string loadingError;

    // ロード完了後に自動でゲームを開始するかどうか
    bool startAfterLoad = false;
    // if non-negative, startAfterLoadState causes a transition to that state after loading completes
    int startAfterLoadState = -1;

    // UI 用の擬似ロード（リソース既読でもロード画面を見せたい場合）
    bool loadingFakeMode = false;
    int loadingFakeFramesTotal = 0;
    int loadingFakeFramesRemaining = 0;

    // 全画面切替状態と再初期化フラグ
    bool isFullscreen = false;
    bool reinitInProgress = false;

    // 状態遷移後に数フレーム入力を無視するためのカウンタ
    int ignoreMouseFrames = 0;

    // スローモーション（敵撃破時の演出）管理
    bool slowMotionActive = false; // 現在スローモーション中か
    int slowMotionTimer = 0; // 残りフレーム
    float slowMotionScale = 1.0f; // 時間スケール
    float slowMotionAccumulator = 0.0f; // スローモーション中の更新蓄積用

    // フラッシュオーバーレイ（画面端に色味を出す）管理
    bool flashActive = false;
    int flashTimer = 0;
    int flashDuration = 0;
    float flashIntensity = 0.0f;
    int flashR = 255; int flashG = 255; int flashB = 255;
    float flashSpread = 1.0f;

    // 画面揺れ（スクリーンシェイク）管理
    int screenShakeTimer = 0;
    int screenShakeMagnitude = 0;

    // シーン間フェード遷移の管理
    bool transitionActive = false;
    float transitionTimer = 0.0f;
    float transitionDuration = 30.0f;
    int transitionTargetState = -1;
    bool transitionSwitched = false;

    // Room tag -> display text mapping (set at runtime)
    std::unordered_map<int, std::string> roomTagTexts;

    // Mapping tag -> RGB color (0xRRGGBB)
    std::unordered_map<int,int> roomTagColors;

    // Active room overlay state (display when player is in a room with text/tag)
    std::string activeRoomOverlayText;
    int activeRoomOverlayRoomId = -1;
    int activeRoomOverlayAlpha = 0; // 0..255
    int activeRoomOverlayTargetAlpha = 0;
    int activeRoomOverlayFadeSpeed = 12; // alpha change per frame
    int activeRoomOverlayColor = 0xFFFFFF; // 0xRRGGBB
    int activeRoomOverlayFontSize = 16;
    int activeRoomOverlayScreenX = 0;
    int activeRoomOverlayScreenY = 0;
    // Font handle for room overlay (for Japanese text rendering)
    int roomOverlayFontHandle = -1;

    // Option: hide built-in player control hint overlays (tags 1..8)
    bool showPlayerControlHints = false;

public:
    /**
     * @brief プレイヤーの移動を試行し、マップの衝突に応じて位置を修正する
     * - Player の座標を直接変更するヘルパー
     */
    void TryMove(Player& player, float dx, float dy);
    static GameManager& GetInstance();
    static void DestroyInstance();

    // Draw active room overlay (room tag/text) independently so it can be used in gameplay and tutorial
    void DrawActiveRoomOverlay();

    // BGM/SE ハンドル
    int bgmHandles[BGM_COUNT];
    int seHandles[SE_COUNT];

    // ゲーム内オブジェクトへのアクセス
    std::vector<Enemy>& GetEnemies();
    Player& GetPlayer();
    void LoadResources();
    void ReleaseResources();
    int GetBGMSound(BGM_SOUND_TYPE type) const { return bgmHandles[type]; }
    int GetSESound(SE_SOUND_TYPE type) const { return seHandles[type]; }
    EditModeManager editModeManager;

    bool GetPlayerIsOnGround() const;

    // シーン切替やロード関連API
    void SetGameState(GameState state);
    GameState GetGameState() const;
    void ConsumeMouseForFrames(int frames) { ignoreMouseFrames = frames; }
    int GetIgnoreMouseFrames() const { return ignoreMouseFrames; }

    // 非同期ロード API（メインスレッドで段階的に処理する）
    void StartAsyncLoad();
    void ShowLoadingThenStart(int frames = 30);
    bool IsLoading() const { return loadingInProgress.load() || loadingFakeMode; }
    bool IsLoaded() const { return loadingDone.load(); }
    float GetLoadingProgress() const { return loadingProgress.load(); }

    void SetStartAfterLoad(bool v) { startAfterLoad = v; }
    bool GetStartAfterLoad() const { return startAfterLoad; }
    void SetStartAfterLoadState(int state) { startAfterLoadState = state; }

    // ウィンドウモード関連
    void ApplyWindowMode(bool fullscreen);
    bool IsFullscreen() const { return isFullscreen; }
    void HandleWindowModeToggle(); // メンバー経由でのウィンドウ切替ハンドラ

    // ゲーム内オブジェクト一覧（パブリックにしてアクセスを簡便化）
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Bullet> playerBullets;
    std::vector<Bullet> enemyBullets;
    std::vector<Item> items;
    std::vector<RespawnLine> respawnLines;

    // ゲームプレイの主要ループ用メソッド
    void InitGame();
    void UpdateGame();
    void DrawGame();
    void UpdateGameplay();
    void DrawGameplay();

    // 当たり判定チェック（矩形）
    bool CheckCollision(const RECT& r1, const RECT& r2) const;

    // プレイヤー・弾・アイテムの作成・操作メソッド
    void PlayerTakeDamage(int damage);
    void AddPlayerBullet(int x, int y, int dirX, int dirY, int power);
    void AddEnemyBullet(int x, int y, int dirX, int dirY, int power);
    void AddItem(int x, int y, ItemType type);

    int GetPlayerX() const;
    int GetPlayerY() const;

    void SaveGame();
    void LoadGame();
    // Save current map tile and collision CSVs (used by edit/save points)
    void SaveMap();

    Map& GetMap();

    Camera& GetCamera();
    int backgroundHandle = -1;

    // パララックス背景のレイヤー定義とパラメータ
    static const int PARALLAX_LAYER_COUNT = 6;
    int backgroundLayerHandles[PARALLAX_LAYER_COUNT] = { -1, -1, -1, -1, -1, -1 };
    float backgroundParallaxFactors[PARALLAX_LAYER_COUNT] = { 0.1f, 0.2f, 0.35f, 0.5f, 0.75f, 1.0f };
    float backgroundScaleFactors[PARALLAX_LAYER_COUNT] = { 3.0f, 1.9f, 1.6f, 1.35f, 1.15f, 1.0f };
    int backgroundBaseYOffset[PARALLAX_LAYER_COUNT] = { 0, 160, 220, 300, 360, 420 };

    // デバッグ描画フラグ
    bool debugMode = true;
    void DrawDebugOverlay();

    // SE 再生と演出トリガー
    int PlaySoundOnce(const char* path);
    void TriggerSlowMotion(int frames, float timeScale, const char* sePath = nullptr, int r = 255, int g = 255, int b = 255, float spread = 1.0f);
    void TriggerScreenShake(int frames, int magnitude);

    bool IsPlayerAtClearPoint() const;

    TutorialScene* GetTutorialScene() { return tutorialScene.get(); }

    // Room tag text mapping
    void SetRoomTagText(int tag, const std::string& text);
    std::string GetRoomTagText(int tag) const;
    void ClearRoomTagText(int tag);

    // Returns text for a tag using a switch/case managed mapping in code
    std::string GetTextForTagSwitch(int tag) const;

    // Tag color API (store RGB packed as 0xRRGGBB)
    void SetRoomTagColor(int tag, int rgb);
    int GetRoomTagColor(int tag) const; // returns 0 if none
    void ClearRoomTagColor(int tag);

private:
    void ResetGameEntities();
    void ContinueLoadingStep();
    void StartTransitionToState(GameState targetState, int durationFrames = 30);
    void DrawTransitionOverlay();
    // Load default room tag texts from UTF-8 CSV at startup to ensure correct Japanese encoding
    void LoadDefaultRoomTagTexts();
};

