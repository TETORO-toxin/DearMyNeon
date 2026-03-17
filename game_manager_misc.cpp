#include "game_manager.h"
#include <fstream>

std::vector<Enemy>& GameManager::GetEnemies() { return enemies; }
Player& GameManager::GetPlayer() { return player; }
Camera& GameManager::GetCamera() { return camera; }
GameState GameManager::GetGameState() const { return currentGameState; }

void GameManager::PlayerTakeDamage(int damage) { player.TakeDamage(damage); }
void GameManager::AddPlayerBullet(int x,int y,int dirX,int dirY,int power) { playerBullets.emplace_back(x,y,dirX,dirY,power); }

void GameManager::ShowLoadingThenStart(int frames) {
    if (loadingInProgress.load()) { startAfterLoad=true; return; }
    if (!loadingDone.load()) { StartAsyncLoad(); startAfterLoad=true; return; }
    loadingFakeMode=true; loadingFakeFramesTotal = frames>0?frames:30; loadingFakeFramesRemaining = loadingFakeFramesTotal; loadingProgress.store(0.0f); startAfterLoad=true;
}

void GameManager::SaveGame() { Player::PlayerSaveData pd = player.GetSaveData(); std::vector<Enemy::EnemySaveData> ed; for (const auto& e: enemies) ed.push_back(e.GetSaveData()); if (SaveGameData(pd, ed)) DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(0,255,0), "Game Saved!"); else DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(255,0,0), "Save Failed!"); }

void GameManager::LoadGame() { Player::PlayerSaveData pd; std::vector<Enemy::EnemySaveData> ed; if (LoadGameData(pd, ed)) { player.LoadData(pd); enemies.clear(); for (const auto& d: ed) { Enemy en(d.ex, d.ey, d.eType); en.LoadData(d); enemies.push_back(en); } DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(0,255,0), "Game Loaded!"); } else { DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(255,0,0), "No Save Data Found or Load Failed!"); InitGame(); } }

Map& GameManager::GetMap() { if (map) return *map; return Map::GetInstance(); }

void GameManager::TriggerSlowMotion(int frames, float timeScale, const char* sePath, int r, int g, int b, float spread) {
    slowMotionActive = true;
    slowMotionTimer = frames;
    slowMotionScale = timeScale;
    flashActive = true; flashTimer = frames; flashDuration = frames; flashIntensity = 1.0f;
    flashR = r; flashG = g; flashB = b; flashSpread = spread;
    if (sePath) { int h = PlaySoundOnce(sePath); (void)h; }
    TriggerScreenShake(static_cast<int>(frames * 0.25f), 6);
}

void GameManager::SaveMap() {
    if (!map) { DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(255,0,0), "No map to save"); return; }
    bool ok = true;

    auto writeRooms = [&](const std::string& filename) -> bool {
        std::ofstream ofs(filename, std::ofstream::out | std::ofstream::trunc);
        if (!ofs.is_open()) return false;
        for (const auto &r : map->GetRooms()) {
            // escape commas in tutorialText and overlayText by replacing with ';'
            auto escape = [](const std::string &s) {    
                std::string out = s;
                for (auto &c : out) if (c == ',') c = ';';
                return out;
            };
            std::string ttxt = escape(r.tutorialText);
            std::string otxt = escape(r.overlayText);
            // write id,xStart,xEnd,yStart,yEnd,tutorialText,overlayText,overlayColor,overlayFontSize,tag
            ofs << r.id << ',' << r.xStart << ',' << r.xEnd << ',' << r.yStart << ',' << r.yEnd << ',' << ttxt << ',' << otxt << ',' << r.overlayColor << ',' << r.overlayFontSize << ',' << r.tag << '\n';
        }
        return true;
    };

    if (currentGameState == STATE_TUTORIAL) {
        ok &= map->SaveTileMapToCSV("tilemap_tutorial.csv");
        ok &= map->SaveCollisionMapToCSV("collisionmap_tutorial.csv");
        ok &= editModeManager.SaveEnemyDataToCSV(enemies, "enemydata_tutorial.csv");
        ok &= editModeManager.SavePointsToCSV(savePoints, "points_tutorial.csv");
        ok &= editModeManager.SaveRespawnLinesToCSV(respawnLines, "respawnlines_tutorial.csv");
        ok &= writeRooms("rooms_tutorial.csv");
    } else {
        ok &= map->SaveTileMapToCSV("tilemap_edited.csv");
        ok &= map->SaveCollisionMapToCSV("collisionmap_edited.csv");
        ok &= editModeManager.SaveEnemyDataToCSV(enemies, "enemydata_edited.csv");
        ok &= editModeManager.SavePointsToCSV(savePoints, "points_edited.csv");
        ok &= editModeManager.SaveRespawnLinesToCSV(respawnLines, "respawnlines_edited.csv");
        ok &= writeRooms("rooms.csv");
    }

    if (ok) DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(0,255,0), "Map Saved!");
    else DrawFormatString(10, SCREEN_HEIGHT-30, GetColor(255,0,0), "Map Save Failed!");
}


