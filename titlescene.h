#pragma once
//#include "game_manager.h"
#include "DxLib.h"
#include "define.h"
#include "Code/SpriteAtlas.h"
#include <string>
class GameManager;

/*!
 * @file titlescene.h
 * @brief タイトル画面の実装ヘッダ
 */

class TitleScene {
public:
    TitleScene(GameManager* manager);
    ~TitleScene();
    void Update();
    void Draw();
    // Ensure resources used by this scene are loaded (call from main thread)
    void EnsureLoaded();

    int titleImageHandle;
    int childCharacterHandle;
    int logoHandle;
    int selectedIndex; // 0: GAME START, 1: QUIT GAME
    int fontHandle;
    const int optionX = 100;
    const int optionYBase = SCREEN_HEIGHT / 2 - 40;
    const int optionWidth = 300;
    const int optionHeight = 40;
    int bgmHandle;

    // Sprite atlas for title UI (Aseprite export)
    SpriteAtlas atlas;
    bool atlasLoaded = false;
    int atlasAnimX = -200;
    int atlasAnimY = 50;
    int atlasAlpha = 0;

    // Background scrolling for subtle parallax
    float bgScrollX = 0.0f; // current horizontal offset
    float bgScrollY = 0.0f; // current vertical offset
    float bgScrollSpeedX = 10.0f; // pixels per second
    float bgScrollSpeedY = 0.0f;  // vertical scroll disabled (horizontal-only)

    // Logo / screen scanline effect
    float logoScanOffset = 0.0f; // vertical offset for moving scanlines
    float logoScanSpeed = 20.0f; // pixels per second (slower)
    int logoScanSpacing = 14;    // spacing between scanlines in pixels (larger)
    int logoScanThickness = 2;   // thickness of each scanline in pixels
    int logoScanAlpha = 60;      // alpha of scanlines (0-255), lower -> more transparent

    // Occasional noise/glitch overlay
    bool noiseActive = false;
    float noiseTimer = 0.0f;                 // remaining seconds for current noise burst
    float noiseDuration = 0.12f;             // seconds for each noise burst
    float noiseProbabilityPerSecond = 0.6f;  // chance per second to start a noise burst (increased)
    int noiseRects = 20;                     // number of random rectangles to draw during noise (edge-focused)

private:
    GameManager* gameManager;

    // Track previous mouse left state per-instance and require mouse release when entering
    bool prevMouseLeft = false;
    int lastObservedState = -1;
    bool requireMouseReleaseOnEnter = false;
};
