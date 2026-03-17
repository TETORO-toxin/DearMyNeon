#include "TutorialScene.h"
#include "game_manager.h"
#include "define.h"
#include "save_load.h"

TutorialScene::TutorialScene(GameManager* manager) : gameManager(manager) {
    // No tutorial messages by default; keep steps empty to avoid showing messages.
}

void TutorialScene::EnsureLoaded() {
    if (fontHandle == -1) fontHandle = CreateFontToHandle("???C???I", 20, 1);
}

void TutorialScene::StartDemo() {
    // Reset demo playback state
    // Previously this enabled an automatic demo mode that drove the real player.
    // To avoid auto-moving the player during the tutorial, we no longer change the real player state here.
    demoTimer = 0;
}

void TutorialScene::UpdateDemo(float dt) {
    // This function previously fed synthetic input to the real player for demo playback.
    // We intentionally leave it implemented but it is no longer invoked from Update(), so it won't affect gameplay.
    Player::InputState in{};
    in.padInput = 0;
    in.isQJustPressed = false; in.isWJustPressed = false; in.isXJustPressed = false; in.isZJustPressed = false;
    demoTimer++;
    if (demoTimer < 90) {
        in.padInput = PAD_INPUT_RIGHT;
    } else if (demoTimer == 90) {
        in.isWJustPressed = true;
    } else if (demoTimer < 160) {
    } else if (demoTimer < 260) {
        in.padInput = PAD_INPUT_LEFT;
    }

    // Feed demo input to the player and run update + collision (kept for reference but not used)
    Player& p = GameManager::GetInstance().player;
    p.SetDemoInput(in);
    p.Update();
    float dx = p.GetMoveX();
    float dy = p.GetMoveY();
    GameManager::GetInstance().TryMove(p, dx, dy);
    GameManager::GetInstance().GetCamera().Update(p.GetX(), p.GetY());
}

void TutorialScene::FinishTutorial(bool completed) {
    // Disable demo mode if active (harmless if demo not used)
    GameManager::GetInstance().player.SetDemoMode(false);
    // persist completion flag
    SaveTutorialFlag(completed);

    // Reset transient tutorial state so the tutorial can be restarted later.
    tutorialLoaded = false;
    stepIndex = 0;
    overlayActive = false;
    overlayTimer = 0;
    overlayConsumeMouse = true;
    demoTimer = 0;
    requireMouseReleaseOnEnter = false;
    prevMouseLeft = false;
}

void TutorialScene::StartOverlayText(const std::string& text) {
    EnsureLoaded();
    overlayText = text;
    overlayActive = true;
    overlayTimer = 0;
    // consume mouse until release to avoid immediate closing
    overlayConsumeMouse = true;
    int mouseX, mouseY; GetMousePoint(&mouseX, &mouseY);
    prevMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
}

void TutorialScene::UpdateOverlay() {
    if (!overlayActive) return;
    int mouseX, mouseY; GetMousePoint(&mouseX, &mouseY);
    bool curMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;

    // require release first
    if (overlayConsumeMouse) {
        if (!curMouseLeft) overlayConsumeMouse = false;
        prevMouseLeft = curMouseLeft;
        return;
    }

    // Do not close on mouse click anymore to avoid blocking room tag overlays.
    // Keep Enter key as an explicit dismiss, and add an automatic timeout.
    prevMouseLeft = curMouseLeft;
    if (CheckHitKey(KEY_INPUT_RETURN) == 1) {
        overlayActive = false;
        return;
    }

    // Auto-dismiss after a short duration (e.g., 180 frames ~3s at 60fps)
    const int overlayAutoDismissFrames = 180;
    ++overlayTimer;
    if (overlayTimer >= overlayAutoDismissFrames) {
        overlayActive = false;
        return;
    }
}

void TutorialScene::DrawOverlay() {
    // Overlay drawing handled by GameManager now; disable TutorialScene overlay drawing.
    return;
}

void TutorialScene::Update() {
    EnsureLoaded();

    // Load tutorial-specific CSV/resources on first entry
    if (!tutorialLoaded && gameManager) {
        // mark loaded early to avoid recursion
        tutorialLoaded = true;
        gameManager->SetGameState(STATE_TUTORIAL); // ensure state
        gameManager->InitGame(); // InitGame will pick tutorial CSV when currentGameState == STATE_TUTORIAL
    }

    int mouseX, mouseY; GetMousePoint(&mouseX, &mouseY);
    bool curMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;

    // handle require mouse release on entering from title
    if (requireMouseReleaseOnEnter) {
        if (!curMouseLeft) requireMouseReleaseOnEnter = false;
        prevMouseLeft = curMouseLeft;
        // Do not return here; allow gameplay update so the player can move while waiting for release
    }

    // Always update core gameplay so player can move during the tutorial
    if (gameManager) {
        gameManager->UpdateGameplay();
    }

    // Skip by ESC
    if (CheckHitKey(KEY_INPUT_ESCAPE) == 1) {
        FinishTutorial(false);
        gameManager->InitGame();
        gameManager->SetGameState(STATE_GAMEPLAY);
        return;
    }

    // Click SKIP button
    int screenW = SCREEN_WIDTH, screenH = SCREEN_HEIGHT; GetDrawScreenSize(&screenW, &screenH);
    int skipX1 = screenW - skipBtnW - 12;
    int skipY1 = 12;
    int skipX2 = skipX1 + skipBtnW;
    int skipY2 = skipY1 + skipBtnH;
    if (!prevMouseLeft && curMouseLeft && mouseX >= skipX1 && mouseX <= skipX2 && mouseY >= skipY1 && mouseY <= skipY2) {
        FinishTutorial(false);
        gameManager->InitGame();
        gameManager->SetGameState(STATE_GAMEPLAY);
        return;
    }

    // Advance on click otherwise: allow stepping through tutorial messages but do NOT auto-complete the tutorial here.
    if (!prevMouseLeft && curMouseLeft) {
        if (stepIndex + 1 < (int)steps.size()) {
            stepIndex++;
            // If new step is demo, we no longer start an automatic demo
            // if (steps[stepIndex].type == StepType::Demo) StartDemo();
        } else {
            // Reached last step: stay here. Tutorial completion is handled by reaching a clear point (see GameManager::UpdateGame).
        }
    }
    prevMouseLeft = curMouseLeft;

    // Demo update removed to prevent auto-moving the player
    // if (stepIndex < (int)steps.size() && steps[stepIndex].type == StepType::Demo) {
    //     UpdateDemo(1.0f/60.0f);
    // }

    // Enter no longer immediately starts the game; use Enter to advance messages instead
    if (CheckHitKey(KEY_INPUT_RETURN) == 1) {
        if (stepIndex + 1 < (int)steps.size()) {
            stepIndex++;
        }
    }
}

void TutorialScene::Draw() {
    EnsureLoaded();

    // Delegate main gameplay drawing to GameManager so global effects (screen shake, flash, slow-motion)
    // and other overlays are applied consistently in tutorial mode.
    GameManager& gm = GameManager::GetInstance();
    gm.DrawGameplay();

    // Draw skip button on top of gameplay
    int screenW = SCREEN_WIDTH, screenH = SCREEN_HEIGHT; GetDrawScreenSize(&screenW, &screenH);
    int skipX1 = screenW - skipBtnW - 12;
    int skipY1 = 12;
    DrawBox(skipX1, skipY1, skipX1 + skipBtnW, skipY1 + skipBtnH, GetColor(120,120,120), TRUE);
    DrawFormatString(skipX1 + 10, skipY1 + 6, GetColor(255,255,255), "SKIP");

    // Tutorial messages are managed elsewhere; do not draw step text here to keep messages hidden as before.

    // Draw active room overlay is already handled by DrawGameplay(), so no need to call it again here.
}
