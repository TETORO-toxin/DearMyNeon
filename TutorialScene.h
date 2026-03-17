#pragma once

#include "DxLib.h"
#include "define.h"
#include <string>
#include <vector>

class GameManager;

class TutorialScene {
public:
    TutorialScene(GameManager* manager);
    void Update();
    void Draw();
    void EnsureLoaded();

    // New API: overlay during gameplay
    void StartOverlayText(const std::string& text); // start showing a tutorial overlay while gameplay continues
    void UpdateOverlay(); // update overlay state each frame during gameplay
    void DrawOverlay(); // draw overlay on top of gameplay
    bool IsOverlayActive() const { return overlayActive; }

    // Allow external end of tutorial (e.g., when player reaches clear point)
    void FinishTutorial(bool completed);

    // Allow external code to request that tutorial consume the mouse on entry
    void SetRequireMouseReleaseOnEnter(bool v) { requireMouseReleaseOnEnter = v; }

private:
    GameManager* gameManager;
    int fontHandle = -1;
    bool tutorialLoaded = false; // whether tutorial-specific CSV/resources were loaded

    enum class StepType { Text, Demo };
    struct Step {
        StepType type;
        std::string text;
    };

    std::vector<Step> steps;
    int stepIndex = 0; // current step

    // demo playback state (for StepType::Demo)
    float demoX = 100.0f;
    float demoY = 300.0f;
    float demoVY = 0.0f;
    bool demoFacingRight = true;
    int demoTimer = 0;

    // skip UI
    const int skipBtnW = 80;
    const int skipBtnH = 28;

    bool requireMouseReleaseOnEnter = false;
    bool prevMouseLeft = false;

    void StartDemo();
    void UpdateDemo(float dt);

    // Overlay state
    bool overlayActive = false;
    std::string overlayText;
    int overlayTimer = 0; // simple auto-dismiss timer if desired
    bool overlayConsumeMouse = true; // whether overlay consumes clicks
};