#include "game_manager.h"

void GameManager::TriggerScreenShake(int frames, int magnitude) {
    screenShakeTimer = frames;
    screenShakeMagnitude = magnitude;
}
