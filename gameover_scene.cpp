#include "gameover_scene.h"
#include <DxLib.h>
#include "define.h"
#include "game_manager.h"

GameOverScene::GameOverScene(GameManager* manager) : gameManager(manager) {}

void GameOverScene::Reset() {
    menuIndex = 0;
    inputDelay = 0;
    initialized = false;
    prevMouseLeft = false;
    // reset overlay state
    fadeTimer = 0;
    overlayAlpha = 0;
    bandAlpha = 0;
    textAlpha = 0;
}

void GameOverScene::Update() {
    // On entering game over, we assume player's DEAD animation already played in gameplay state.
    if (!initialized) {
        initialized = true;
        inputDelay = 8; // small delay to avoid instantaneous input
        menuIndex = 0;
        prevMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
        // start fade
        fadeTimer = 0;
        overlayAlpha = 0;
        bandAlpha = 0;
        textAlpha = 0;
        return;
    }

    // advance fade timer (cap at duration)
    if (fadeTimer < fadeDuration) ++fadeTimer;
    float p = fadeDuration > 0 ? static_cast<float>(fadeTimer) / static_cast<float>(fadeDuration) : 1.0f;
    if (p < 0.0f) p = 0.0f; if (p > 1.0f) p = 1.0f;
    overlayAlpha = static_cast<int>(160.0f * p); // background dim target 160
    bandAlpha = static_cast<int>(200.0f * p); // band target ~200
    textAlpha = static_cast<int>(255.0f * p); // text fully opaque at end

    if (inputDelay > 0) inputDelay--;

    // Keyboard navigation
    if (inputDelay == 0) {
        if (CheckHitKey(KEY_INPUT_UP) == 1 || CheckHitKey(KEY_INPUT_W) == 1) { menuIndex = std::max(0, menuIndex - 1); inputDelay = 8; }
        if (CheckHitKey(KEY_INPUT_DOWN) == 1 || CheckHitKey(KEY_INPUT_S) == 1) { menuIndex = std::min(1, menuIndex + 1); inputDelay = 8; }
    }

    // Mouse handling: highlight based on mouse pos, and click to activate
    int mx, my; GetMousePoint(&mx, &my);
    // Compute the same band and menu Y position used in Draw() so hitboxes align
    int bandH = SCREEN_HEIGHT / 6;
    int bandTop = SCREEN_HEIGHT/2 - bandH/2;
    int bandBottom = bandTop + bandH;
    int baseX = SCREEN_WIDTH/2 - 60;
    int baseY = bandBottom + 20; // same as Draw()'s menu Y
    // YES/NO option rectangles (match DrawFormatString positions)
    // Make rectangles wider/taller to reliably cover drawn text (font size may be large)
    RECT yesRect = { baseX - 10, baseY + 20, baseX + 90, baseY + 20 + 48 };
    RECT noRect  = { baseX + 100 - 10, baseY + 20, baseX + 100 + 90, baseY + 20 + 48 };

    // Hover selection
    if (mx >= yesRect.left && mx <= yesRect.right && my >= yesRect.top && my <= yesRect.bottom) menuIndex = 0;
    else if (mx >= noRect.left && mx <= noRect.right && my >= noRect.top && my <= noRect.bottom) menuIndex = 1;

    bool mouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
    bool mouseJustPressed = mouseLeft && !prevMouseLeft;
    prevMouseLeft = mouseLeft;

    if ((inputDelay == 0 && CheckHitKey(KEY_INPUT_RETURN) == 1) || mouseJustPressed) {
        if (menuIndex == 0) {
            // YES -> restart game
            gameManager->InitGame();
            gameManager->SetGameState(STATE_GAMEPLAY);
        } else {
            // NO -> go to title
            gameManager->SetGameState(STATE_TITLE);
        }
        Reset();
        return;
    }
}

void GameOverScene::Draw() {
    // Ensure large font exists
    if (bigFontHandle == -1) {
        // Create a large, bold-looking font for DEFEAT text
        bigFontHandle = CreateFontToHandle("Arial", 140, 4);
        if (bigFontHandle == -1) {
            // fallback to default small font if creation failed
            bigFontHandle = CreateFontToHandle("Arial", 80, 2);
        }
    }

    // Recompute progress based on fadeTimer in case Draw is called before Update (defensive)
    float p = fadeDuration > 0 ? static_cast<float>(fadeTimer) / static_cast<float>(fadeDuration) : 1.0f;
    if (p < 0.0f) p = 0.0f; if (p > 1.0f) p = 1.0f;
    overlayAlpha = static_cast<int>(160.0f * p);
    bandAlpha = static_cast<int>(200.0f * p);
    textAlpha = static_cast<int>(255.0f * p);

    // Draw underlying dim overlay
    if (overlayAlpha > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, overlayAlpha);
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0,0,0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // Draw center horizontal black band
    int bandH = SCREEN_HEIGHT / 6; // thick band across center
    int bandTop = SCREEN_HEIGHT/2 - bandH/2;
    int bandBottom = bandTop + bandH;
    if (bandAlpha > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, bandAlpha);
        DrawBox(0, bandTop, SCREEN_WIDTH, bandBottom, GetColor(0,0,0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // Draw DEFEAT text (larger, shifted left)
    int tx = SCREEN_WIDTH/2 - 300; // moved further left
    int ty = SCREEN_HEIGHT/2 - (bandH/2) - 10; // slightly above center of band

    if (textAlpha > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, textAlpha);
        // dark red color
        DrawStringToHandle(tx, ty, "DEFEAT", GetColor(128,20,20), bigFontHandle);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // Draw continue menu below the black band
    int mx = SCREEN_WIDTH/2 - 60;
    int my = bandBottom + 20;
    if (textAlpha > 0) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, textAlpha);
        DrawFormatString(mx, my, GetColor(255,255,255), "CONTINUE?");
        DrawFormatString(mx, my + 30, menuIndex == 0 ? GetColor(255,255,0) : GetColor(200,200,200), "YES");
        DrawFormatString(mx + 100, my + 30, menuIndex == 1 ? GetColor(255,255,0) : GetColor(200,200,200), "NO");
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    } else {
        // ensure options drawn even when fully transparent (defensive)
        DrawFormatString(mx, my, GetColor(255,255,255), "CONTINUE?");
        DrawFormatString(mx, my + 30, menuIndex == 0 ? GetColor(255,255,0) : GetColor(200,200,200), "YES");
        DrawFormatString(mx + 100, my + 30, menuIndex == 1 ? GetColor(255,255,0) : GetColor(200,200,200), "NO");
    }
}