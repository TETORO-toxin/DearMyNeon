#include "game_manager.h"
#include <windows.h>
#include <algorithm>

// Window mode and transition related methods

void HandleWindowModeToggle()
{
    try { GameManager::GetInstance().HandleWindowModeToggle(); } catch (...) {}
}

void GameManager::ApplyWindowMode(bool fullscreen)
{
    if (reinitInProgress) return;
    if (isFullscreen == fullscreen) return;
    reinitInProgress = true;

    constexpr int FIXED_W = 1920;
    constexpr int FIXED_H = 1080;

    static int prevW = FIXED_W;
    static int prevH = FIXED_H;
    static bool graphChanged = false;

    HWND hWnd = GetMainWindowHandle();
    if (!hWnd) { reinitInProgress = false; return; }

    RECT targetRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    HMONITOR hm = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi; mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hm, &mi)) targetRect = mi.rcMonitor;

    static LONG_PTR prevStyle = 0;
    static LONG_PTR prevExStyle = 0;
    static RECT prevRect = {0,0,0,0};
    static bool havePrev = false;

    if (fullscreen) {
        if (!havePrev) {
            prevStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
            prevExStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
            GetWindowRect(hWnd, &prevRect);
            havePrev = true;
        }

        if (!graphChanged) {
            prevW = FIXED_W; prevH = FIXED_H; graphChanged = true;
        }

        int newW = FIXED_W;
        int newH = FIXED_H;

        SetGraphMode(newW, newH, 32);
        ChangeWindowMode(FALSE);

        int monW = targetRect.right - targetRect.left;
        int monH = targetRect.bottom - targetRect.top;
        int posX = targetRect.left + std::max(0, (monW - newW) / 2);
        int posY = targetRect.top + std::max(0, (monH - newH) / 2);
        SetWindowPos(hWnd, HWND_TOP, posX, posY, newW, newH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) { if (backgroundLayerHandles[i] != -1) { DeleteGraph(backgroundLayerHandles[i]); backgroundLayerHandles[i] = -1; } }
        for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) { char buf[64]; sprintf_s(buf, "bg_layer%d.png", i+1); backgroundLayerHandles[i] = SafeLoadGraph(buf); }
        if (!tilePalette) tilePalette = std::make_unique<TilePalette>(32,10,50,100);
        tilePalette->loadTiles();
        if (map) map->setTileHandles(tilePalette->getTileHandles());
        { int &g = EnemyBulletGraphHandle(); if (g != -1) { DeleteGraph(g); g = -1; } g = SafeLoadGraph("enemy_bullet.png"); }
        player.ReloadGraphics();

        SetDrawScreen(DX_SCREEN_BACK);
        ClearDrawScreen();
        DrawFormatString(10, 10, GetColor(255,255,0), "Entering fullscreen %dx%d", newW, newH);
        ScreenFlip();
        Sleep(50);

        SetMouseDispFlag(TRUE);
        ShowCursor(TRUE);

        camera = Camera();
        camera.SetScreenSize(newW, newH);
        camera.SetMapSize(map ? map->GetWidth()*map->GetTileSize() : 0, map ? map->GetHeight()*map->GetTileSize() : 0);
        isFullscreen = true;

        if (titleScene) {
            if (titleScene->titleImageHandle != -1) { DeleteGraph(titleScene->titleImageHandle); titleScene->titleImageHandle = -1; }
            if (titleScene->childCharacterHandle != -1) { DeleteGraph(titleScene->childCharacterHandle); titleScene->childCharacterHandle = -1; }
            if (titleScene->logoHandle != -1) { DeleteGraph(titleScene->logoHandle); titleScene->logoHandle = -1; }
            if (titleScene->bgmHandle != -1) { StopSoundMem(titleScene->bgmHandle); DeleteSoundMem(titleScene->bgmHandle); titleScene->bgmHandle = -1; }
            if (titleScene->fontHandle != -1) { DeleteFontToHandle(titleScene->fontHandle); titleScene->fontHandle = -1; }
            titleScene->EnsureLoaded();
        }
    } else {
        if (havePrev) {
            SetWindowLongPtr(hWnd, GWL_STYLE, prevStyle);
            SetWindowLongPtr(hWnd, GWL_EXSTYLE, prevExStyle);
            SetWindowPos(hWnd, NULL, prevRect.left, prevRect.top, prevRect.right - prevRect.left, prevRect.bottom - prevRect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            havePrev = false;
        }

        if (graphChanged) {
            ChangeWindowMode(TRUE);
            SetGraphMode(prevW, prevH, 32);

            int monW = targetRect.right - targetRect.left;
            int monH = targetRect.bottom - targetRect.top;
            int posX = targetRect.left + std::max(0, (monW - prevW) / 2);
            int posY = targetRect.top + std::max(0, (monH - prevH) / 2);
            SetWindowPos(hWnd, HWND_TOP, posX, posY, prevW, prevH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) { if (backgroundLayerHandles[i] != -1) { DeleteGraph(backgroundLayerHandles[i]); backgroundLayerHandles[i] = -1; } }
            for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) { char buf[64]; sprintf_s(buf, "bg_layer%d.png", i+1); backgroundLayerHandles[i] = SafeLoadGraph(buf); }
            if (tilePalette) { tilePalette->loadTiles(); if (map) map->setTileHandles(tilePalette->getTileHandles()); }
            { int &g = EnemyBulletGraphHandle(); if (g != -1) { DeleteGraph(g); g = -1; } g = SafeLoadGraph("enemy_bullet.png"); }

            if (titleScene) {
                if (titleScene->titleImageHandle != -1) { DeleteGraph(titleScene->titleImageHandle); titleScene->titleImageHandle = -1; }
                if (titleScene->childCharacterHandle != -1) { DeleteGraph(titleScene->childCharacterHandle); titleScene->childCharacterHandle = -1; }
                if (titleScene->logoHandle != -1) { DeleteGraph(titleScene->logoHandle); titleScene->logoHandle = -1; }
                if (titleScene->bgmHandle != -1) { StopSoundMem(titleScene->bgmHandle); DeleteSoundMem(titleScene->bgmHandle); titleScene->bgmHandle = -1; }
                if (titleScene->fontHandle != -1) { DeleteFontToHandle(titleScene->fontHandle); titleScene->fontHandle = -1; }
                titleScene->EnsureLoaded();
            }

            player.ReloadGraphics();

            SetDrawScreen(DX_SCREEN_BACK);
            ClearDrawScreen();
            DrawFormatString(10, 10, GetColor(255,255,0), "Returning to windowed %dx%d", prevW, prevH);
            ScreenFlip();
            Sleep(50);

            SetMouseDispFlag(TRUE);
            ShowCursor(TRUE);
            graphChanged = false;
        }

        int gw = prevW, gh = prevH; GetDrawScreenSize(&gw, &gh);
        camera = Camera();
        camera.SetScreenSize(gw, gh);
        camera.SetMapSize(map ? map->GetWidth()*map->GetTileSize() : 0, map ? map->GetHeight()*map->GetTileSize() : 0);
        isFullscreen = false;
    }

    reinitInProgress = false;
}

void GameManager::StartTransitionToState(GameState targetState, int durationFrames)
{
    transitionActive = true;
    transitionDuration = durationFrames > 0 ? static_cast<float>(durationFrames) : 30.0f;
    transitionTimer = transitionDuration; // start from full fade-out
    transitionTargetState = static_cast<int>(targetState);
    transitionSwitched = false;
    ConsumeMouseForFrames(10);
}

void GameManager::DrawTransitionOverlay()
{
    if (!transitionActive) return;
    float half = transitionDuration * 0.5f;
    float t = transitionTimer;
    int alpha = 0;
    if (t > half) {
        float p = (t - half) / half; // 1->0
        alpha = static_cast<int>(255.0f * (1.0f - p)); // invert so fade-out becomes increasing alpha
    } else {
        float p = t / half; // 0->1
        alpha = static_cast<int>(255.0f * (1.0f - p));
    }
    if (alpha < 0) alpha = 0; if (alpha > 255) alpha = 255;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
    DrawBox(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,GetColor(0,0,0),TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND,0);

    transitionTimer -= 1.0f;
    if (!transitionSwitched && transitionTimer <= half) {
        transitionSwitched = true;
        GameState gs = static_cast<GameState>(transitionTargetState);
        if (gs == STATE_GAMEPLAY) {
            InitGame();
        }
        SetGameState(gs);
    }
    if (transitionTimer <= 0.0f) {
        transitionActive = false;
        transitionTimer = 0.0f;
        transitionSwitched = false;
    }
}

void GameManager::HandleWindowModeToggle()
{
    static bool prevF11 = false;
    bool down = (CheckHitKey(KEY_INPUT_F11) == 1);
    if (down && !prevF11) ApplyWindowMode(!isFullscreen);
    prevF11 = down;

    HWND hWnd = GetMainWindowHandle();
    if (hWnd) {
        int ts = map ? map->GetTileSize() : DEFAULT_TILE_SIZE;
        camera.SetMapSize(map ? map->GetWidth()*ts : 0, map ? map->GetHeight()*ts : 0);
    }
}
