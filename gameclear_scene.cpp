#include "gameclear_scene.h"
#include <DxLib.h>
#include "define.h"
#include "game_manager.h"

GameClearScene::GameClearScene(GameManager* manager) : gameManager(manager) {}

void GameClearScene::Reset() {
    fadeTimer = 0;
}

void GameClearScene::Update() {
    // Detect entering scene to reset fade
    if (lastObservedState != gameManager->GetGameState()) {
        lastObservedState = gameManager->GetGameState();
        if (lastObservedState == STATE_CLEAR) Reset();
    }

    // Increase fade timer until duration
    if (fadeTimer < fadeDuration) fadeTimer++;

    // Mouse handling for button
    int mx=0,my=0; GetMousePoint(&mx,&my);
    bool curMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;

    // Compute button position centered
    buttonW = 200; buttonH = 36;
    buttonX = SCREEN_WIDTH/2 - buttonW/2;
    buttonY = SCREEN_HEIGHT/2 + 20;

    hover = (mx >= buttonX && mx <= buttonX+buttonW && my >= buttonY && my <= buttonY+buttonH);

    // Click detection: accept press or release while hovering, and Enter key
    bool clicked = false;
    if (hover && (curMouseLeft && !prevMouseLeft)) clicked = true; // press
    if (hover && (!curMouseLeft && prevMouseLeft)) clicked = true; // release
    if (CheckHitKey(KEY_INPUT_RETURN) == 1) clicked = true;

    if (clicked) {
        // Prevent the click that triggered this transition from immediately affecting the title screen
        if (gameManager) gameManager->ConsumeMouseForFrames(10); // ignore mouse for 10 frames
        gameManager->SetGameState(STATE_TITLE);
    }

    prevMouseLeft = curMouseLeft;

    // Debug overlay to help diagnose click issues
    if (gameManager && gameManager->debugMode) {
       /* DrawFormatString(10, 60, GetColor(255,255,0), "ClearScene Mouse: %d,%d hover=%d curLeft=%d prevLeft=%d clicked=%d", mx, my, hover?1:0, curMouseLeft?1:0, prevMouseLeft?1:0, clicked?1:0);*/
    }
}

void GameClearScene::Draw() {
    // Draw semi-transparent black overlay (fade-in)
    float t = static_cast<float>(fadeTimer) / static_cast<float>(fadeDuration);
    if (t > 1.0f) t = 1.0f;
    int alpha = static_cast<int>(alphaMax * t);
    // Use DrawBox with alpha blending
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // Draw GAME CLEAR centered
    DrawFormatString(SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 40, GetColor(255,255,255), "GAME CLEAR");

    // Draw option button
    int bx = buttonX; int by = buttonY; int bw = buttonW; int bh = buttonH;
    unsigned int col = hover ? GetColor(220,220,220) : GetColor(180,180,180);
    DrawBox(bx, by, bx + bw, by + bh, col, TRUE);
    DrawBox(bx, by, bx + bw, by + bh, GetColor(0,0,0), FALSE);
    DrawFormatString(bx + 20, by + 8, GetColor(0,0,0), "RETURN TITLE");
}
