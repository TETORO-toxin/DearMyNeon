/*!
 * @file titlescene.cpp
 * @brief タイトル画面の実装
 */

#include "titlescene.h"
#include <DxLib.h>
#include "define.h"
#include "game_manager.h"
#include <cmath>
#include <random>
#include <cstdio>
#include <fstream>

TitleScene::TitleScene(GameManager* manager) : gameManager(manager) {
    // Defer loading heavy assets until the main thread Draw/Update to avoid blocking startup.
    titleImageHandle = -1; // will be loaded lazily
    childCharacterHandle = -1;
    logoHandle = -1;
    selectedIndex = 0;
    fontHandle = -1;
    bgmHandle = -1;
    atlasLoaded = false;
    atlasAnimX = -200;
    atlasAnimY = 50;
    atlasAlpha = 0;
    bgScrollX = 0.0f;
    bgScrollY = 0.0f;
    logoScanOffset = 0.0f;
    noiseActive = false;
    noiseTimer = 0.0f;

    // seed RNG
    std::random_device rd;
    srand(static_cast<unsigned int>(rd()));
}

TitleScene::~TitleScene() {
}

void TitleScene::EnsureLoaded() {
    // Load lightweight title assets on the main thread if they aren't already loaded
    if (titleImageHandle == -1) titleImageHandle = LoadGraph("graphic/title/backscreen.png"); // 背景
    if (childCharacterHandle == -1) childCharacterHandle = LoadGraph("graphic/title/title player.png"); // キャラクター
    if (logoHandle == -1) logoHandle = LoadGraph("graphic/title/Title.png"); // ロゴ
    if (fontHandle == -1) fontHandle = CreateFontToHandle("メイリオ", 36, 1);

    // Ensure BGM handle exists and is playing
    if (bgmHandle == -1) {
        bgmHandle = LoadSoundMem("sound/title/Crystal brilliance.mp3");
    }
    if (bgmHandle != -1) {
        int st = CheckSoundMem(bgmHandle);
        if (st != 1) PlaySoundMem(bgmHandle, DX_PLAYTYPE_LOOP);
    }

    // load sprite atlas if present
    if (!atlasLoaded) {
        atlasLoaded = atlas.LoadFromJson("SpriteSheet.json", "SpriteSheet.png");
    }
}

void TitleScene::Update() {
    // Ensure title assets are available for sizing / mouse hit testing
    EnsureLoaded();

    int mouseX, mouseY;
    GetMousePoint(&mouseX, &mouseY);

    // Only allow mouse-based selection while GameManager is not consuming mouse input
    if (gameManager->GetIgnoreMouseFrames() == 0) {
        for (int i = 0; i < 3; ++i) {
            int y = optionYBase + i * 60;
            if (mouseX >= optionX && mouseX <= optionX + optionWidth &&
                mouseY >= y && mouseY <= y + optionHeight) {
                selectedIndex = i;
            }
        }
    }

    // キーボード操作
    if (CheckHitKey(KEY_INPUT_UP) == 1) {
        selectedIndex = (selectedIndex - 1 + 3) % 3;
    }
    if (CheckHitKey(KEY_INPUT_DOWN) == 1) {
        selectedIndex = (selectedIndex + 1) % 3;
    }

    // Enter またはマウス左クリックで決定
    bool curMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
    // If we've just entered the title scene, record state and require a mouse release
    if (lastObservedState != gameManager->GetGameState()) {
        lastObservedState = gameManager->GetGameState();
        // If the mouse is currently down, require release before accepting clicks
        requireMouseReleaseOnEnter = curMouseLeft;
        prevMouseLeft = curMouseLeft;
    }

    // If we're requiring a release, wait until the left button is released
    if (requireMouseReleaseOnEnter) {
        if (!curMouseLeft) {
            requireMouseReleaseOnEnter = false;
            prevMouseLeft = false;
        }
    }

    // Only accept mouse clicks if GameManager isn't currently consuming mouse frames
    bool mouseClickAccepted = (gameManager->GetIgnoreMouseFrames() == 0);

    if (CheckHitKey(KEY_INPUT_RETURN) == 1 || (mouseClickAccepted && !requireMouseReleaseOnEnter && (curMouseLeft && !prevMouseLeft))) {

        if (bgmHandle != -1) StopSoundMem(bgmHandle);

        if (selectedIndex == 0) {
            // Start incremental main-thread loading and request automatic start to gameplay when done
            if (!gameManager->IsLoading() && !gameManager->IsLoaded()) {
                gameManager->StartAsyncLoad();
                gameManager->SetStartAfterLoad(true);
            } else if (gameManager->IsLoaded()) {
                // Initialize game state for a fresh playthrough
                gameManager->InitGame();
                // Already loaded: show a short loading screen before starting gameplay
                gameManager->ShowLoadingThenStart(30); // 30 frames of loading animation
            }
        }
        else if (selectedIndex == 1) {
            // Tutorial
            if (!gameManager->IsLoading() && !gameManager->IsLoaded()) {
                gameManager->StartAsyncLoad();
                gameManager->SetStartAfterLoad(true);
                gameManager->SetStartAfterLoadState(STATE_TUTORIAL);
            } else if (gameManager->IsLoaded()) {
                // Ensure tutorial scene consumes the initial click after switching from title
                if (gameManager->GetTutorialScene()) {
                    gameManager->GetTutorialScene()->SetRequireMouseReleaseOnEnter((GetMouseInput() & MOUSE_INPUT_LEFT) != 0);
                }
                // Show a short loading bar even when resources are already loaded, then start tutorial
                gameManager->SetStartAfterLoadState(STATE_TUTORIAL);
                gameManager->ShowLoadingThenStart(30);
            }
        }
        else {
            DxLib_End();
            exit(0);
        }
    }

    prevMouseLeft = curMouseLeft;

    // update atlas UI animation: slide and fade in
    if (atlasLoaded) {
        if (atlasAlpha < 255) atlasAlpha = std::min(255, atlasAlpha + 5);
        if (atlasAnimX < 100) atlasAnimX = std::min(100, atlasAnimX + 4);
    }

    // Advance background scroll offsets (time-based using GetNowCount for smoothness)
    // Calculate delta time in seconds since last frame
    static unsigned int lastTime = 0;
    unsigned int now = GetNowCount();
    float dt = 0.016f; // default to ~60fps
    if (lastTime != 0) dt = (now - lastTime) / 1000.0f;
    lastTime = now;

    // Scroll background slowly. We use very small speeds so effect is subtle.
    bgScrollX += bgScrollSpeedX * dt;
    bgScrollY += bgScrollSpeedY * dt;

    // Advance logo/screen scanline offset (horizontal scanlines moving vertically across entire screen)
    logoScanOffset += logoScanSpeed * dt;
    if (logoScanOffset > static_cast<float>(logoScanSpacing)) logoScanOffset = fmod(logoScanOffset, static_cast<float>(logoScanSpacing));

    // Trigger occasional noise bursts
    if (!noiseActive) {
        // probability scaled by dt
        float p = noiseProbabilityPerSecond * dt;
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        if (r < p) {
            noiseActive = true;
            noiseTimer = noiseDuration;
        }
    } else {
        noiseTimer -= dt;
        if (noiseTimer <= 0.0f) {
            noiseActive = false;
            noiseTimer = 0.0f;
        }
    }

    // wrap offsets to avoid large values
    if (bgScrollX > 100000.0f) bgScrollX = fmod(bgScrollX, 1000.0f);
    if (bgScrollY > 100000.0f) bgScrollY = fmod(bgScrollY, 1000.0f);
}

void TitleScene::Draw() {
    // If GameManager is still loading core resources, we still show the title UI; ensure assets loaded
    EnsureLoaded();

    // Use actual draw buffer size (may differ from compile-time SCREEN_WIDTH/SCREEN_HEIGHT in fullscreen)
    int screenW = SCREEN_WIDTH, screenH = SCREEN_HEIGHT;
    GetDrawScreenSize(&screenW, &screenH);

    // 背景画像を縮小して描画
    int originalWidth = 0, originalHeight = 0;
    if (titleImageHandle != -1) GetGraphSize(titleImageHandle, &originalWidth, &originalHeight);
    // Compute a scale that fits the image into the current screen while preserving aspect ratio
    if (titleImageHandle != -1 && originalWidth > 0 && originalHeight > 0) {
        float sx = static_cast<float>(screenW) / static_cast<float>(originalWidth);
        float sy = static_cast<float>(screenH) / static_cast<float>(originalHeight);
        // Use cover scaling (fill screen) and add a small overscale to make background slightly larger
        float s = std::max(sx, sy) * 1.02f; // cover + 2% overscale
        int drawW = static_cast<int>(originalWidth * s);
        int drawH = static_cast<int>(originalHeight * s);

        // Compute scroll offsets modulo the drawn image size so we can tile/repeat
        int offsetX = static_cast<int>(fmod(bgScrollX, drawW));
        int offsetY = static_cast<int>(fmod(bgScrollY, drawH));
        if (offsetX < 0) offsetX += drawW;
        if (offsetY < 0) offsetY += drawH;

        // We will draw the background image in a 3x3 grid around the screen center to ensure coverage while scrolling
        int centerX = (screenW - drawW) / 2;
        int centerY = (screenH - drawH) / 2;

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                int drawX = centerX + dx * drawW - offsetX;
                int drawY = centerY + dy * drawH - offsetY;
                DrawExtendGraph(drawX, drawY, drawX + drawW, drawY + drawH, titleImageHandle, TRUE);
            }
        }
    } else {
        DrawFormatString(20, 20, GetColor(255,255,255), "Dear:MYNEON");
    }

    int charWidth = 0, charHeight = 0;
    if (childCharacterHandle != -1) GetGraphSize(childCharacterHandle, &charWidth, &charHeight);
    float scale = 0.5f; // 50%縮小
    int charScaledWidth = static_cast<int>(charWidth * scale);
    int charScaledHeight = static_cast<int>(charHeight * scale);
    int x = screenW - charScaledWidth - 20;
    int y = screenH - charScaledHeight - 20;
    if (childCharacterHandle != -1)
        DrawExtendGraph(x, y, x + charScaledWidth, y + charScaledHeight, childCharacterHandle, TRUE);

    int logoWidth = 0, logoHeight = 0;
    if (logoHandle != -1) GetGraphSize(logoHandle, &logoWidth, &logoHeight);

    int logoX = 20;
    int logoY = 20;

    if (logoHandle != -1) DrawGraph(logoX, logoY, logoHandle, TRUE);

    // draw scanlines across entire title screen
    {
        int spacing = std::max(1, logoScanSpacing);
        int thickness = std::max(1, logoScanThickness);
        int offset = static_cast<int>(fmod(logoScanOffset, static_cast<float>(spacing)));
        if (offset < 0) offset += spacing;
        int firstY = -spacing + offset; // start above screen to cover top edge

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, logoScanAlpha);
        for (int sy = firstY; sy <= screenH; sy += spacing) {
            int top = sy;
            int bottom = sy + thickness;
            if (bottom < 0) continue;
            if (top > screenH) break;
            if (top < 0) top = 0;
            if (bottom > screenH) bottom = screenH;
            DrawBox(0, top, screenW, bottom, GetColor(120,120,120), TRUE);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // compute option baseline dynamically using current screen height
    int optionYBaseLocal = screenH / 2 - 40;

    for (int i = 0; i < 3; ++i) {
        int x = optionX;
        int y = optionYBaseLocal + i * 60;
        int color = (i == selectedIndex) ? GetColor(255, 255, 0) : GetColor(255, 255, 255);
        const char* label = (i == 0 ? "GAME START" : (i == 1 ? "TUTORIAL" : "QUIT GAME"));
        if (fontHandle != -1)
            DrawStringToHandle(x, y, label, color, fontHandle);
        else
            DrawFormatString(x, y, color, label);
    }

    // draw atlas effect
    if (atlasLoaded && atlas.HasFrame("SpriteSheet (Sprites).aseprite")) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, atlasAlpha);
        atlas.DrawFrame("SpriteSheet (Sprites).aseprite", atlasAnimX, atlasAnimY, 1.0f, false);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // draw occasional noise bursts if active
    if (noiseActive) {
        // Use configured number of rectangles and compute an alpha for this burst
        int nRects = std::max(1, noiseRects);
        int noiseAlpha = 80 + (rand() % 96); // 80..175
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, noiseAlpha);

        for (int i = 0; i < nRects; ++i) {
            int edge = rand() % 4; // 0=top,1=bottom,2=left,3=right
            int w, h, px, py;
            if (edge == 0) {
                // top edge: wide, short
                w = screenW / 4 + (rand() % (screenW / 2));
                h = 4 + (rand() % 24);
                px = rand() % (screenW - w + 1);
                // move noise closer to the very top edge by shrinking the vertical offset range
                py = rand() % std::max(1, screenH / 16); // nearer top
            } else if (edge == 1) {
                // bottom edge
                w = screenW / 4 + (rand() % (screenW / 2));
                h = 4 + (rand() % 24);
                px = rand() % (screenW - w + 1);
                // move noise closer to the very bottom edge by shrinking the vertical offset range
                py = screenH - 1 - (rand() % std::max(1, screenH / 16)) - h;
            } else if (edge == 2) {
                // left edge: tall, narrow
                w = 4 + (rand() % 24);
                h = screenH / 8 + (rand() % (screenH / 3));
                // keep rectangles near left edge by limiting horizontal offset
                px = rand() % std::max(1, screenW / 16);
                py = rand() % (screenH - h + 1);
            } else {
                // right edge
                w = 4 + (rand() % 24);
                h = screenH / 8 + (rand() % (screenH / 3));
                // keep rectangles near right edge by limiting horizontal offset
                px = screenW - 1 - (rand() % std::max(1, screenW / 16)) - w;
                py = rand() % (screenH - h + 1);
            }
            // clamp
            if (px < 0) px = 0; if (py < 0) py = 0;
            if (px + w > screenW) w = screenW - px;
            if (py + h > screenH) h = screenH - py;
            DrawBox(px, py, px + w, py + h, GetColor(200,200,200), TRUE);
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}
