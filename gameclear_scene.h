#pragma once
#include "DxLib.h"
class GameManager;

class GameClearScene {
public:
    GameClearScene(GameManager* manager);
    void Update();
    void Draw();
    void Reset();

private:
    GameManager* gameManager;
    int fadeTimer = 0;
    int fadeDuration = 60; // frames for fade-in
    int alphaMax = 200; // max alpha for overlay
    int lastObservedState = -1;

    // UI button for returning to title
    int buttonX = 0;
    int buttonY = 0;
    int buttonW = 200;
    int buttonH = 36;
    bool prevMouseLeft = false;
    bool hover = false;
};