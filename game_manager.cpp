/*!
 * @file game_manager.cpp
 * @brief GameManager core: singleton lifetime and high-level flow
 */

#include "game_manager.h"
#include "RoomManager.h"
#include "player.h"
#include "Map.h"
#include <algorithm>
#include "EditModeManager.h"
#include "Camera.h"
#include "Code/debug_utils.h"
#include <fstream>
#include <windows.h>
#include <sstream>
#ifdef max
#undef max
#endif

// シングルトン実体
GameManager* GameManager::s_instance = nullptr;

// コンストラクタ: シーンやハンドル配列の初期化
GameManager::GameManager() : currentGameState(STATE_TITLE)
{
    for (int i=0;i<BGM_COUNT;++i) bgmHandles[i] = -1;
    for (int i=0;i<SE_COUNT;++i) seHandles[i] = -1;
    titleScene = std::make_unique<TitleScene>(this);
    gameOverScene = std::make_unique<GameOverScene>(this);
    gameClearScene = std::make_unique<GameClearScene>(this);
    tutorialScene = std::make_unique<TutorialScene>(this);

    // Load default room tag texts from UTF-8 CSV to avoid source literal encoding issues
    LoadDefaultRoomTagTexts();
}

GameManager::~GameManager()
{
    if (loadingThread.joinable()) loadingThread.join();
    ReleaseResources();
}

GameManager& GameManager::GetInstance()
{
    if (!s_instance) s_instance = new GameManager();
    return *s_instance;
}

void GameManager::DestroyInstance()
{
    if (s_instance) { delete s_instance; s_instance = nullptr; }
}

// 非同期ロードの開始（フラグのみ設定し、メインスレッドで段階的に読み込む）
void GameManager::StartAsyncLoad()
{
    if (loadingInProgress.load()) return;
    loadingInProgress.store(true);
    loadingDone.store(false);
    loadingProgress.store(0.0f);
    loadingStep = 0;
}

// 毎フレームのゲーム全体更新
void GameManager::UpdateGame()
{
    if (ignoreMouseFrames > 0) --ignoreMouseFrames;

    // マウス表示/非表示の切替（タイトルや編集モード時は表示）
    if (currentGameState == STATE_TITLE || currentGameState == STATE_GAMEOVER || currentGameState == STATE_CLEAR || editModeManager.IsEditMode()) {
        SetMouseDispFlag(TRUE);
        ShowCursor(TRUE);
    } else {
        SetMouseDispFlag(FALSE);
        ShowCursor(FALSE);
    }

    HandleWindowModeToggle();

    // 擬似ロード進行（UIだけ）
    if (loadingFakeMode) {
        if (loadingFakeFramesRemaining > 0) {
            loadingFakeFramesRemaining--;
            float p = 1.0f - static_cast<float>(loadingFakeFramesRemaining) / static_cast<float>(loadingFakeFramesTotal);
            loadingProgress.store(p);
            return;
        } else {
            loadingFakeMode = false;
            loadingProgress.store(1.0f);
            if (startAfterLoad) { startAfterLoad=false; if (startAfterLoadState >= 0) StartTransitionToState(static_cast<GameState>(startAfterLoadState), 40); else StartTransitionToState(STATE_GAMEPLAY, 40); startAfterLoadState = -1; }
        }
    }

    // 非同期読み込みステップの継続
    if (loadingInProgress.load() && !loadingDone.load()) { ContinueLoadingStep(); return; }
    if (loadingDone.load() && startAfterLoad) { startAfterLoad=false; if (startAfterLoadState >= 0) StartTransitionToState(static_cast<GameState>(startAfterLoadState), 40); else StartTransitionToState(STATE_GAMEPLAY, 40); startAfterLoadState = -1; }

    if (transitionActive) {
        // フェード中でも背景アニメーション等は更新させる
    }

    // 現在のゲーム状態に応じた更新を呼び出す
    switch (currentGameState) {
    case STATE_TITLE: titleScene->Update(); break;
    case STATE_TUTORIAL: if (tutorialScene) tutorialScene->Update(); break;
    case STATE_GAMEPLAY: UpdateGameplay(); break;
    case STATE_PAUSE: break;
    case STATE_GAMEOVER: gameOverScene->Update(); break;
    case STATE_CLEAR: gameClearScene->Update(); break;
    }

    // Global save hotkey: allow quick save in gameplay
    if (currentGameState == STATE_GAMEPLAY && CheckHitKey(KEY_INPUT_Y) == 1) {
        SaveGame();
    }

    // Allow edit-mode updates while in tutorial so developer tools are available
    if (currentGameState == STATE_TUTORIAL) {
        editModeManager.Update(player, map.get(), enemies, savePoints);
        // Allow saving from edit-mode / tutorial: Check Y key and save
        if (editModeManager.IsEditMode() && CheckHitKey(KEY_INPUT_Y) == 1) {
            // In tutorial edit mode, SaveMap writes to tutorial-specific CSVs (rooms_tutorial.csv etc.)
            SaveMap();
        }
    }

    for (auto& sp : savePoints) sp.Update(player);

    // If in tutorial scene and player reached a clear point, finish tutorial and switch to clear state
    if (currentGameState == STATE_TUTORIAL) {
        if (IsPlayerAtClearPoint()) {
            if (tutorialScene) tutorialScene->FinishTutorial(true);
            SetGameState(STATE_CLEAR);
        }
    }
}

// 描画ルート
void GameManager::DrawGame()
{
    if (loadingFakeMode || (loadingInProgress.load() && !loadingDone.load())) {
        ClearDrawScreen();
        DrawFormatString(SCREEN_WIDTH/2-60, SCREEN_HEIGHT/2-20, GetColor(255,255,255), "Loading...");
        float p = loadingProgress.load(); int barW=300; int filled = static_cast<int>(barW*p);
        DrawBox(SCREEN_WIDTH/2 - barW/2, SCREEN_HEIGHT/2, SCREEN_WIDTH/2 - barW/2 + filled, SCREEN_HEIGHT/2 + 20, GetColor(100,200,100), TRUE);
        DrawBox(SCREEN_WIDTH/2 - barW/2, SCREEN_HEIGHT/2, SCREEN_WIDTH/2 + barW/2, SCREEN_HEIGHT/2 + 20, GetColor(255,255,255), FALSE);
        DrawFormatString(SCREEN_WIDTH/2-80, SCREEN_HEIGHT/2+30, GetColor(200,200,200), "Please wait: %.0f%%", p*100.0f);
        if (loadingFailed) DrawFormatString(SCREEN_WIDTH/2-200, SCREEN_HEIGHT/2+60, GetColor(255,0,0), "Loading failed: %s", loadingError.c_str());
        return;
    }

    if (loadingFailed) {
        static bool wrote=false; if (!wrote) { std::ofstream ofs("load_error.txt", std::ofstream::out|std::ofstream::trunc); if (ofs.is_open()) { ofs << "Loading failed: " << loadingError << "\n"; ofs.close(); } char buf[1024]; sprintf_s(buf, "Loading failed: %s", loadingError.c_str()); MessageBox(NULL, buf, "Loading Error", MB_OK|MB_ICONERROR); wrote=true; }
        ClearDrawScreen(); DrawFormatString(20,20,GetColor(255,0,0), "Loading failed: %s", loadingError.c_str()); return;
    }

    switch (currentGameState) {
    case STATE_TITLE: titleScene->Draw(); break;
    case STATE_TUTORIAL: if (tutorialScene) tutorialScene->Draw(); break;
    case STATE_GAMEPLAY: DrawGameplay(); break;
    case STATE_PAUSE: break;
    case STATE_GAMEOVER:
        DrawGameplay();
        if (gameOverScene) gameOverScene->Draw();
        break;
    case STATE_CLEAR: gameClearScene->Draw(); break;
    }

    // Draw edit-mode UI on top when in tutorial if enabled
    if (currentGameState == STATE_TUTORIAL && editModeManager.IsEditMode()) {
        editModeManager.Draw();
    }

    if (editModeManager.IsEditMode()) {
        for (const auto& sp : savePoints) sp.Draw();
    }

    if (transitionActive) {
        DrawTransitionOverlay();
    }
}

// ゲーム初期化: マップやエネミー、セーブポイントの読み込みとプレイヤー初期化
void GameManager::InitGame()
{
    ResetGameEntities();
    savePoints.clear();

    // Ensure map instance exists
    if (!map) {
        map = std::make_unique<Map>(1000,1000,32);
    }

    // Helper to check file existence
    auto fileExists = [](const std::string& path) -> bool {
        std::ifstream ifs(path);
        return ifs.is_open();
    };

    // Choose filenames: prefer tutorial variants when in tutorial state and file exists
    std::string tileFile = "tilemap_edited.csv";
    std::string collisionFile = "collisionmap_edited.csv";
    std::string roomsFile = "rooms.csv";
    std::string enemyFile = "enemydata_edited.csv";
    std::string pointsFile = "points_edited.csv";
    std::string respawnFile = "respawnlines_edited.csv";

    if (currentGameState == STATE_TUTORIAL) {
        if (fileExists("tilemap_tutorial.csv")) tileFile = "tilemap_tutorial.csv";
        if (fileExists("collisionmap_tutorial.csv")) collisionFile = "collisionmap_tutorial.csv";
        if (fileExists("rooms_tutorial.csv")) roomsFile = "rooms_tutorial.csv";
        if (fileExists("enemydata_tutorial.csv")) enemyFile = "enemydata_tutorial.csv";
        if (fileExists("points_tutorial.csv")) pointsFile = "points_tutorial.csv";
        if (fileExists("respawnlines_tutorial.csv")) respawnFile = "respawnlines_tutorial.csv";
    }

    if (map) {
        map->LoadFromCSV(tileFile, collisionFile);
        // Load rooms if available (non-fatal)
        map->LoadFromCSV(roomsFile);
        enemies.clear();
        editModeManager.UpgradeEnemyCSVToNewFormat(enemyFile);
        editModeManager.LoadEnemyDataFromCSV(enemies, enemyFile);
    }

    editModeManager.LoadPointsFromCSV(savePoints, pointsFile);
    editModeManager.LoadRespawnLinesFromCSV(respawnLines, respawnFile);

    player.SetMap(map.get());
    player.savedX = PLAYER_SPAWN_X;
    player.savedY = PLAYER_SPAWN_Y;
    player.Revive(false);

    camera.SetMapSize(map ? map->GetWidth()*map->GetTileSize() : 0, map ? map->GetHeight()*map->GetTileSize() : 0);
    camera.Update(player.GetX(), player.GetY());

    // Ensure runtime resources (tile palette, tile handles, background layers) are loaded so map drawing works
    if (!tilePalette || map->GetTileHandles().empty()) {
        LoadResources();
    }
}

void GameManager::ResetGameEntities() { enemies.clear(); playerBullets.clear(); enemyBullets.clear(); items.clear(); }

bool GameManager::IsPlayerAtClearPoint() const {
    for (const auto& sp : savePoints) {
        if (sp.GetType() == SavePoint::Type::Clear) {
            if (sp.IsPlayerInRange(player)) return true;
        }
    }
    return false;
}

// SaveMap implementation moved to game_manager_misc.cpp to avoid duplicate definitions.

void GameManager::SetRoomTagText(int tag, const std::string& text) {
    if (tag < 0) return;
    roomTagTexts[tag] = text;
}

std::string GameManager::GetRoomTagText(int tag) const {
    auto it = roomTagTexts.find(tag);
    if (it != roomTagTexts.end()) return it->second;
    return std::string();
}

void GameManager::ClearRoomTagText(int tag) {
    roomTagTexts.erase(tag);
}

void GameManager::SetRoomTagColor(int tag, int rgb) {
    if (tag < 0) return;
    roomTagColors[tag] = rgb & 0xFFFFFF;
}

int GameManager::GetRoomTagColor(int tag) const {
    auto it = roomTagColors.find(tag);
    if (it != roomTagColors.end()) return it->second;
    return 0; // 0 indicates no color set
}

void GameManager::ClearRoomTagColor(int tag) {
    roomTagColors.erase(tag);
}

// Centralized switch/case mapping for room tag texts.
std::string GameManager::GetTextForTagSwitch(int tag) const {
    switch (tag) {
    case 1: return "ここで操作方法を学ぼう！　AとＤで左右に動けるよ";
    case 2: return "スペースキーでジャンプ！";
    case 3: return "Ｑキーで射撃ができるよ！";
    case 4: return "射撃はとっても強いけど弾に注意してね！";
    case 5: return "敵は赤い丸がなくなったら攻撃してくるよ！";
	case 6: return "敵の攻撃は避けよう！　体力がなくなるとゲームオーバーだよ";
	case 7: return "左クリックで近接攻撃ができるよ！　前に行っちゃうから注意だ！";
	case 8: return "回避はとっても便利！　右クリックで使えるよ！";
    default: return std::string();
    }
}

// Helper: convert a byte string in unknown encoding to UTF-8.
static std::string ConvertToUtf8(const std::string &s) {
    if (s.empty()) return std::string();
    // Try interpreting as UTF-8 first
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (wlen > 0) {
        std::wstring wbuf(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &wbuf[0], wlen);
        // convert wide -> UTF-8
        int utf8len = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), wlen, nullptr, 0, nullptr, nullptr);
        if (utf8len > 0) {
            std::string out(utf8len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), wlen, &out[0], utf8len, nullptr, nullptr);
            return out;
        }
    }
    // Fallback: assume system ANSI (CP_ACP / typically CP932 on Japanese Windows)
    wlen = MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (wlen > 0) {
        std::wstring wbuf(wlen, L'\0');
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), static_cast<int>(s.size()), &wbuf[0], wlen);
        int utf8len = WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), wlen, nullptr, 0, nullptr, nullptr);
        if (utf8len > 0) {
            std::string out(utf8len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wbuf.c_str(), wlen, &out[0], utf8len, nullptr, nullptr);
            return out;
        }
    }
    // As last resort return original
    return s;
}

void GameManager::LoadDefaultRoomTagTexts() {
    const char* fname = "room_tag_texts_default.csv";
    std::ifstream ifs(fname, std::ios::binary);
    if (!ifs.is_open()) return;
    // Read whole file into string to handle BOM and line endings reliably
    std::string fileData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (fileData.empty()) return;
    size_t pos = 0;
    // detect UTF-8 BOM
    if (fileData.size() >= 3 && (unsigned char)fileData[0] == 0xEF && (unsigned char)fileData[1] == 0xBB && (unsigned char)fileData[2] == 0xBF) {
        pos = 3; // skip BOM
    }
    while (pos < fileData.size()) {
        size_t eol = fileData.find('\n', pos);
        std::string line;
        if (eol == std::string::npos) { line = fileData.substr(pos); pos = fileData.size(); }
        else { line = fileData.substr(pos, eol - pos); pos = eol + 1; }
        // trim CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t comma = line.find(',');
        if (comma == std::string::npos) continue;
        std::string tagStr = line.substr(0, comma);
        std::string textBytes = line.substr(comma + 1);
        // convert textBytes (unknown encoding) to UTF-8 reliably
        std::string textUtf8 = ConvertToUtf8(textBytes);
        int tag = 0;
        try { tag = std::stoi(tagStr); } catch(...) { continue; }
        roomTagTexts[tag] = textUtf8;
    }
}