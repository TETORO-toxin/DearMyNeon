#pragma once
#include "DxLib.h"
class GameManager;

class GameOverScene {
public:
    GameOverScene(GameManager* manager);
    void Update();
    void Draw();

    void Reset();

private:
    GameManager* gameManager;
    int menuIndex = 0; // 0 = YES, 1 = NO
    int inputDelay = 0; // small delay to avoid instant input
    bool initialized = false;
    bool prevMouseLeft = false; // for mouse click edge detection

    // Overlay / fade state
    int fadeTimer = 0; // frames since overlay started
    int fadeDuration = 60; // fade-in duration in frames
    int overlayAlpha = 0; // 0..160 for background dim
    int bandAlpha = 0; // 0..200 for middle black band
    int textAlpha = 0; // 0..255 for DEFEAT text
    int bigFontHandle = -1; // large font used to draw DEFEAT
};

