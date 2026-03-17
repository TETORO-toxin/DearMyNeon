#include "game_manager.h"
#include "Code/debug_utils.h"
#include <windows.h>
#include <fstream>

// Implement resource-related methods from GameManager

void GameManager::LoadResources()
{
    bgmHandles[BGM_MAIN] = SafeLoadSound("sound/gameplay/Infraction-Scammer-pr.mp3"); loadingProgress.store(0.1f);
    seHandles[SE_ATTACK] = SafeLoadSound("sound/gameplay/attack.mp3"); loadingProgress.store(0.2f);

    // Folder containing bg_layer images
    const char* bgFolder = "graphic/bg_layer/";

    // Try to load all parallax layers from a single atlas file "bg_layer.png" in the folder.
    // Keep the loop-style assignment so callers/usage are unchanged.
    bool atlasLoaded = false;
    int tmpHandles[PARALLAX_LAYER_COUNT] = { -1 };
    char atlasPath[256]; sprintf_s(atlasPath, "%sbg_layer.png", bgFolder);
    int tmp = LoadGraph(atlasPath); // use silent LoadGraph to probe existence (avoid error popup from SafeLoadGraph)
    if (tmp != -1) {
        int fullW = 0, fullH = 0;
        GetGraphSize(tmp, &fullW, &fullH);
        if (fullW > 0 && fullH > 0) {
            int singleW = fullW / PARALLAX_LAYER_COUNT;
            if (singleW > 0) {
                // Delete temporary full image handle before calling LoadDivGraph
                DeleteGraph(tmp);
                tmp = -1;
                int ret = LoadDivGraph(atlasPath, PARALLAX_LAYER_COUNT, PARALLAX_LAYER_COUNT, 1, singleW, fullH, tmpHandles);
                if (ret != -1) {
                    atlasLoaded = true;
                } else {
                    // ensure any partially created handles are cleaned
                    for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) {
                        if (tmpHandles[i] != -1) { DeleteGraph(tmpHandles[i]); tmpHandles[i] = -1; }
                    }
                }
            }
        }
        if (tmp != -1) DeleteGraph(tmp);
    }

    for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) {
        if (atlasLoaded) {
            backgroundLayerHandles[i] = tmpHandles[i];
        } else {
            char buf[256]; sprintf_s(buf, "%sbg_layer%d.png", bgFolder, i+1);
            backgroundLayerHandles[i] = SafeLoadGraph(buf);
        }
        loadingProgress.store(0.2f + 0.2f * (static_cast<float>(i+1)/PARALLAX_LAYER_COUNT));
        if (backgroundLayerHandles[i] == -1) { DxLib_End(); exit(-1); }
    }

    if (!tilePalette) tilePalette = std::make_unique<TilePalette>(32,10,50,100);
    tilePalette->loadTiles(); loadingProgress.store(0.6f);
    editModeManager.SetTilePalette(tilePalette.get());

    if (!map) map = std::make_unique<Map>(1000,1000,32);
    map->setTileHandles(tilePalette->getTileHandles()); loadingProgress.store(0.8f);

    player.SetMap(map.get());
    camera.SetMapSize(map->GetWidth()*map->GetTileSize(), map->GetHeight()*map->GetTileSize());
    loadingProgress.store(1.0f);
}

void GameManager::ReleaseResources()
{
    for (int i=0;i<BGM_COUNT;++i) {
        if (bgmHandles[i] != -1) { DeleteSoundMem(bgmHandles[i]); bgmHandles[i] = -1; }
    }
    if (tilePalette) tilePalette.reset();
    if (map) map.reset();
}

int GameManager::PlaySoundOnce(const char* path) {
    int h = LoadSoundMem(path);
    if (h == -1) return -1;
    PlaySoundMem(h, DX_PLAYTYPE_BACK);
    return h;
}

// Continue loading step (incremental on main thread)
void GameManager::ContinueLoadingStep()
{
    if (!loadingInProgress.load()) return;
    try {
        switch (loadingStep) {
        case 0: bgmHandles[BGM_MAIN] = SafeLoadSound("sound/gameplay/Infraction-Scammer-pr.mp3"); loadingProgress.store(0.1f); break;
        case 1: seHandles[SE_ATTACK] = SafeLoadSound("sound/gameplay/attack.mp3"); loadingProgress.store(0.2f); break;
        case 2: {
            // Folder containing bg_layer images
            const char* bgFolder = "graphic/bg_layer/";
            // Keep loop-style loading but first try to read from atlas "bg_layer.png" in the folder.
            bool atlasLoaded = false;
            int tmpHandles[PARALLAX_LAYER_COUNT] = { -1 };
            char atlasPath[256]; sprintf_s(atlasPath, "%sbg_layer.png", bgFolder);
            int tmp = LoadGraph(atlasPath); // probe silently
            if (tmp != -1) {
                int fullW = 0, fullH = 0;
                GetGraphSize(tmp, &fullW, &fullH);
                if (fullW > 0 && fullH > 0) {
                    int singleW = fullW / PARALLAX_LAYER_COUNT;
                    if (singleW > 0) {
                        DeleteGraph(tmp);
                        tmp = -1;
                        int ret = LoadDivGraph(atlasPath, PARALLAX_LAYER_COUNT, PARALLAX_LAYER_COUNT, 1, singleW, fullH, tmpHandles);
                        if (ret != -1) atlasLoaded = true;
                        else {
                            for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) {
                                if (tmpHandles[i] != -1) { DeleteGraph(tmpHandles[i]); tmpHandles[i] = -1; }
                            }
                        }
                    }
                }
                if (tmp != -1) DeleteGraph(tmp);
            }

            for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) {
                if (atlasLoaded) {
                    backgroundLayerHandles[i] = tmpHandles[i];
                } else {
                    char buf[256]; sprintf_s(buf, "%sbg_layer%d.png", bgFolder, i+1);
                    backgroundLayerHandles[i] = SafeLoadGraph(buf);
                    if (backgroundLayerHandles[i] == -1) throw std::runtime_error("Failed to load background layer");
                }
            }
            loadingProgress.store(0.4f);
            break; }
        case 3: if (!tilePalette) tilePalette = std::make_unique<TilePalette>(32,10,50,100); tilePalette->loadTiles(); editModeManager.SetTilePalette(tilePalette.get()); loadingProgress.store(0.6f); break;
        case 4: if (!map) map = std::make_unique<Map>(1000,1000,32); map->setTileHandles(tilePalette->getTileHandles()); loadingProgress.store(0.8f); break;
        case 5: player.SetMap(map.get()); camera.SetMapSize(map->GetWidth()*map->GetTileSize(), map->GetHeight()*map->GetTileSize()); loadingProgress.store(1.0f); loadingDone.store(true); loadingInProgress.store(false); if (startAfterLoad) { startAfterLoad=false; StartTransitionToState(STATE_GAMEPLAY, 40); } break;
        default: break;
        }
    } catch (const std::exception& ex) { loadingFailed=true; loadingError=ex.what(); loadingInProgress.store(false); loadingDone.store(false); }
    catch (...) { loadingFailed=true; loadingError="Unknown error during loading"; loadingInProgress.store(false); loadingDone.store(false); }
    loadingStep++;
}

// Set game state implementation
void GameManager::SetGameState(GameState state) {
    // If we are leaving gameplay, stop gameplay BGM
    if (currentGameState == STATE_GAMEPLAY && state != STATE_GAMEPLAY) {
        if (bgmHandles[BGM_MAIN] != -1) {
            StopSoundMem(bgmHandles[BGM_MAIN]);
        }
    }

    // If we are leaving the title scene, ensure the title BGM is stopped so it doesn't persist into other scenes
    if (currentGameState == STATE_TITLE && state != STATE_TITLE) {
        if (titleScene && titleScene->bgmHandle != -1) {
            StopSoundMem(titleScene->bgmHandle);
        }
    }

    currentGameState = state;
    ignoreMouseFrames = 4;

    if (currentGameState == STATE_GAMEPLAY) {
        if (bgmHandles[BGM_MAIN] == -1) {
            bgmHandles[BGM_MAIN] = SafeLoadSound("sound/gameplay/Infraction-Scammer-pr.mp3");
        }
        if (bgmHandles[BGM_MAIN] != -1) {
            StopSoundMem(bgmHandles[BGM_MAIN]);
            PlaySoundMem(bgmHandles[BGM_MAIN], DX_PLAYTYPE_LOOP);
            ChangeVolumeSoundMem(80, bgmHandles[BGM_MAIN]);
        }
    }

    if (currentGameState == STATE_TITLE) {
        if (titleScene) {
            titleScene->EnsureLoaded();
            if (titleScene->bgmHandle != -1) {
                int st = CheckSoundMem(titleScene->bgmHandle);
                if (st != 1) PlaySoundMem(titleScene->bgmHandle, DX_PLAYTYPE_LOOP);
            }
        }
    }
}

int GameManager::GetPlayerX() const { return static_cast<int>(player.GetX()); }
int GameManager::GetPlayerY() const { return static_cast<int>(player.GetY()); }

void GameManager::AddEnemyBullet(int x,int y,int vx,int vy,int damage) { int g = EnemyBulletGraphHandle(); Bullet nb(x,y,vx,vy,damage,g); enemyBullets.push_back(nb); }
void GameManager::AddItem(int x,int y,ItemType type) { items.emplace_back(x,y,type); }



