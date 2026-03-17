#include "EditModeManager.h"
#include "RoomManager.h"
#include "game_manager.h"
#include "DxLib.h"
#include "define.h"
#include <algorithm>
#include "Code/debug_utils.h"
#include <sstream>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include "RoomManager.h"

static std::string toLower(const std::string &s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
    return out;
}

// Helper: ensure TilePalette exists and loaded
void EditModeManager::EnsureTilePaletteInitialized() {
    if (tilePalette == nullptr) {
        tilePalette = new TilePalette(DEFAULT_TILE_SIZE, TILE_PALETTE_COLUMNS, TILE_PALETTE_START_X, TILE_PALETTE_START_Y);
        if (!tilePalette->loadTiles()) {
            delete tilePalette;
            tilePalette = nullptr;
            DEBUG_ONLY( DBG_PRINTF("TilePalette load failed\n"); );
        }
    }
}

static bool SaveRoomsToCSV(const std::vector<Room>& rooms, const std::string& filename) {
    std::ofstream ofs(filename, std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open()) return false;
    for (const auto &r : rooms) {
        // escape commas in tutorialText and overlayText by replacing with ';'
        auto escape = [](const std::string &s) {
            std::string out = s;
            for (auto &c : out) if (c == ',') c = ';';
            return out;
        };
        std::string ttxt = escape(r.tutorialText);
        std::string otxt = escape(r.overlayText);
        ofs << r.id << ',' << r.xStart << ',' << r.xEnd << ',' << r.yStart << ',' << r.yEnd << ',' << ttxt << ',' << otxt << ',' << r.overlayColor << ',' << r.overlayFontSize << ',' << r.tag << '\n';
    }
    return true;
}

void EditModeManager::Update(Player& player, Map* map, std::vector<Enemy>& enemies, std::vector<SavePoint>& points) {
    static bool prevEKeyState = false;
    static bool prevMouseLeftState = false;
    static bool prevMouseRightState = false;
    static bool prevIKeyState = false;
    static bool prevInspectorKeyState = false;
    static unsigned char prevKeyState[256] = {0}; // previous frame key state for printable keys

    char keyState[256];
    GetHitKeyStateAll(keyState);
    bool currentEKeyState = keyState[KEY_INPUT_E] != 0;
    bool currentMouseLeft = (GetMouseInput() & MOUSE_INPUT_LEFT) != 0;
    bool currentMouseRight = (GetMouseInput() & MOUSE_INPUT_RIGHT) != 0;

    // SHIFT
    bool shiftHeld = (keyState[KEY_INPUT_LSHIFT] != 0) || (keyState[KEY_INPUT_RSHIFT] != 0);

    int mouseX = 0, mouseY = 0;
    GetMousePoint(&mouseX, &mouseY);

    GameManager& gm = GameManager::GetInstance();

    // Toggle edit mode with E
    if (currentEKeyState && !prevEKeyState) {
        SetEditMode(!isEditMode);
    }

    if (!isEditMode) {
        prevEKeyState = currentEKeyState;
        prevMouseLeftState = currentMouseLeft;
        prevMouseRightState = currentMouseRight;
        // copy keyState to prevKeyState
        for (int i=0;i<256;++i) prevKeyState[i] = (unsigned char)keyState[i];
        return;
    }

    // Toggle tile palette visibility with T
    if (keyState[KEY_INPUT_T] && !prevKeyState[KEY_INPUT_T]) {
        // ensure palette created when first toggled on
        if (!tilePaletteVisible) EnsureTilePaletteInitialized();
        tilePaletteVisible = !tilePaletteVisible;
    }

    // If palette visible, allow dragging and clicks
    bool mouseInPalette = false;
    if (tilePaletteVisible && tilePalette) {
        int px = tilePalette->getX();
        int py = tilePalette->getY();
        int pw = tilePalette->getWidth();
        int ph = tilePalette->getHeight();
        mouseInPalette = (mouseX >= px - TILE_PALETTE_MARGIN && mouseX <= px + pw + TILE_PALETTE_MARGIN && mouseY >= py - TILE_PALETTE_MARGIN && mouseY <= py + ph + TILE_PALETTE_MARGIN);

        // mouse down inside palette
        if (currentMouseLeft && !prevMouseLeftState && mouseInPalette) {
            tilePaletteMouseDown = true;
            tilePaletteMouseDownX = mouseX; tilePaletteMouseDownY = mouseY;
            tilePalette->startDrag(mouseX, mouseY);
        }
        // mouse held -> update drag
        if ((GetMouseInput() & MOUSE_INPUT_LEFT) != 0 && tilePalette->isDragging) {
            tilePalette->updateDrag(mouseX, mouseY);
        }
        // mouse release
        if (!currentMouseLeft && prevMouseLeftState) {
            if (tilePaletteMouseDown) {
                bool wasClick = tilePalette->wasClick();
                tilePalette->endDrag();
                if (wasClick) {
                    tilePalette->handleClick(mouseX, mouseY);
                    int sel = tilePalette->getSelectedTileID();
                    DEBUG_ONLY( DBG_PRINTF("Tile selected: %d at mouse %d,%d\n", sel, mouseX, mouseY); );
                    currentEditMode = EditModeType::Tile;
                }
                tilePaletteMouseDown = false;
            }
        }
    }

    // --- Inspector dragging: start, update, end ---
    // header height should match Draw() headerH = static_cast<int>(28 * inspectorFontScale);
    const int headerH = static_cast<int>(28 * inspectorFontScale);
    // start dragging when clicking header (and inspector not collapsed)
    if (!inspectorCollapsed) {
        bool mouseInHeader = (mouseX >= inspectorPosX && mouseX <= inspectorPosX + inspectorWidth && mouseY >= inspectorPosY && mouseY <= inspectorPosY + headerH);
        if (currentMouseLeft && !prevMouseLeftState && mouseInHeader) {
            // begin drag
            inspectorDragging = true;
            inspectorDragOffsetX = mouseX - inspectorPosX;
            inspectorDragOffsetY = mouseY - inspectorPosY;
            // undock when dragging
            inspectorDocked = false;
        }
        if (inspectorDragging && ((GetMouseInput() & MOUSE_INPUT_LEFT) != 0)) {
            // update position following mouse while preserving offset
            inspectorPosX = mouseX - inspectorDragOffsetX;
            inspectorPosY = mouseY - inspectorDragOffsetY;
            // clamp to screen
            if (inspectorPosX < 0) inspectorPosX = 0;
            if (inspectorPosY < 0) inspectorPosY = 0;
            if (inspectorPosX + inspectorWidth > SCREEN_WIDTH) inspectorPosX = SCREEN_WIDTH - inspectorWidth;
            if (inspectorPosY + inspectorHeight > SCREEN_HEIGHT) inspectorPosY = SCREEN_HEIGHT - inspectorHeight;
        }
        if (!currentMouseLeft && prevMouseLeftState && inspectorDragging) {
            // end drag on release
            inspectorDragging = false;
        }
    }

    // Place/remove tiles when clicking on the map (ignore clicks over UI)
    bool mouseOverInspector = false;
    if (!inspectorCollapsed) {
        if (mouseX >= inspectorPosX && mouseX <= inspectorPosX + inspectorWidth && mouseY >= inspectorPosY && mouseY <= inspectorPosY + inspectorHeight) {
            mouseOverInspector = true;
        }
    }

    // place on left mouse down, remove on right mouse down
    if (map && currentEditMode == EditModeType::Tile) {
        if (currentMouseLeft && !prevMouseLeftState && !mouseInPalette && !mouseOverInspector) {
            Camera &cam = gm.GetCamera();
            int mapX = (mouseX + cam.GetOffsetX()) / map->GetTileSize();
            int mapY = (mouseY + cam.GetOffsetY()) / map->GetTileSize();
            if (tilePalette) {
                int tileID = tilePalette->getSelectedTileID();
                CollisionType ct = tilePalette->getCollisionTypeForSelectedTile();
                map->placeTile(mapX, mapY, tileID, ct);
                DEBUG_ONLY( DBG_PRINTF("Placed tile %d at %d,%d (ct=%d)\n", tileID, mapX, mapY, static_cast<int>(ct)); );
            }
        }
        if (currentMouseRight && !prevMouseRightState && !mouseInPalette && !mouseOverInspector) {
            Camera &cam = gm.GetCamera();
            int mapX = (mouseX + cam.GetOffsetX()) / map->GetTileSize();
            int mapY = (mouseY + cam.GetOffsetY()) / map->GetTileSize();
            map->removeTile(mapX, mapY);
            DEBUG_ONLY( DBG_PRINTF("Removed tile at %d,%d\n", mapX, mapY); );
        }
    }

    // UI constants used for text input view calculations
    float fs = inspectorFontScale;
    const int pad = static_cast<int>(8 * fs);
    const int smallBtnW = static_cast<int>(28 * fs);

    // Build justPressed map for keys
    unsigned char justPressed[256];
    for (int i=0;i<256;++i) { justPressed[i] = (unsigned char)(keyState[i] && !prevKeyState[i]); }

    // Only handle collision-setting number keys when we're in Tile edit mode
    if (currentEditMode == EditModeType::Tile && tilePaletteVisible && tilePalette && !inspectorEditingText && !inspectorSearchActive) {
         if (justPressed[KEY_INPUT_0]) { tilePalette->setCollisionTypeForSelectedTile(CollisionType::None); DEBUG_ONLY( DBG_PRINTF("TilePalette: set collision None\n"); ); }
         if (justPressed[KEY_INPUT_1]) { tilePalette->setCollisionTypeForSelectedTile(CollisionType::Wall); DEBUG_ONLY( DBG_PRINTF("TilePalette: set collision Wall\n"); ); }
         if (justPressed[KEY_INPUT_2]) { tilePalette->setCollisionTypeForSelectedTile(CollisionType::Floor); DEBUG_ONLY( DBG_PRINTF("TilePalette: set collision Floor\n"); ); }
         if (justPressed[KEY_INPUT_3]) { tilePalette->setCollisionTypeForSelectedTile(CollisionType::FloorSide); DEBUG_ONLY( DBG_PRINTF("TilePalette: set collision FloorSide\n"); ); }
         if (justPressed[KEY_INPUT_4]) { tilePalette->setCollisionTypeForSelectedTile(CollisionType::Spike); DEBUG_ONLY( DBG_PRINTF("TilePalette: set collision Spike\n"); ); }
         if (justPressed[KEY_INPUT_5]) { tilePalette->setCollisionTypeForSelectedTile(CollisionType::Water); DEBUG_ONLY( DBG_PRINTF("TilePalette: set collision Water\n"); ); }
     }

    // Save map when Y is pressed (edit mode)
    if (justPressed[KEY_INPUT_Y] && !inspectorEditingText && !inspectorSearchActive) {
        gm.SaveMap();
    }

    // Enemy edit mode: toggle with N, select type with 1/2, place with left click, remove with right click
    if (justPressed[KEY_INPUT_N]) {
        // toggle enemy edit mode
        if (currentEditMode == EditModeType::Enemy) {
            currentEditMode = EditModeType::Tile; // switch back to tile mode
            DEBUG_ONLY( DBG_PRINTF("Enemy edit mode off\n"); );
        } else {
            currentEditMode = EditModeType::Enemy;
            // default to melee (0)
            selectedEnemyType = 0;
            DEBUG_ONLY( DBG_PRINTF("Enemy edit mode on\n"); );
        }
    }

    // Point edit mode: toggle with P, select Save/Clear with 1/2, place with left click, remove with right click
    if (justPressed[KEY_INPUT_P]) {
        if (currentEditMode == EditModeType::Point) {
            currentEditMode = EditModeType::Tile;
            DEBUG_ONLY( DBG_PRINTF("Point edit mode off\n"); );
        } else {
            currentEditMode = EditModeType::Point;
            selectedPointType = PointType::Save; // default
            DEBUG_ONLY( DBG_PRINTF("Point edit mode on\n"); );
        }
    }

    // Room edit mode: toggle with R
    if (justPressed[KEY_INPUT_R]) {
        if (currentEditMode == EditModeType::Room) {
            currentEditMode = EditModeType::Tile;
            rectSelecting = false;
            DEBUG_ONLY( DBG_PRINTF("Room edit mode off\n"); );
        } else {
            currentEditMode = EditModeType::Room;
            inspectorFoldoutRoom = true;
            rectSelecting = false;
            DEBUG_ONLY( DBG_PRINTF("Room edit mode on\n"); );
        }
    }

    // Room mode handling
    if (currentEditMode == EditModeType::Room && !inspectorEditingText && !inspectorSearchActive) {
        if (!mouseInPalette && !mouseOverInspector && map) {
            Camera &cam = gm.GetCamera();
            int worldX = mouseX + cam.GetOffsetX();
            int worldY = mouseY + cam.GetOffsetY();
            // start rectangle selection on left down
            if (currentMouseLeft && !prevMouseLeftState) {
                rectSelecting = true;
                rectStartX = worldX;
                rectStartY = worldY;
                rectCurX = worldX;
                rectCurY = worldY;
                DEBUG_ONLY( DBG_PRINTF("Room rect start %d,%d\n", rectStartX, rectStartY); );
            }
            // update current rect while dragging
            if (rectSelecting && (GetMouseInput() & MOUSE_INPUT_LEFT) != 0) {
                rectCurX = worldX;
                rectCurY = worldY;
            }
            // finish rect on left release
            if (!currentMouseLeft && prevMouseLeftState && rectSelecting) {
                rectSelecting = false;
                // create room only if size is non-zero
                int x0 = std::min(rectStartX, rectCurX);
                int x1 = std::max(rectStartX, rectCurX);
                int y0 = std::min(rectStartY, rectCurY);
                int y1 = std::max(rectStartY, rectCurY);
                const int minSize = 8; // require small area
                if (std::abs(x1 - x0) >= minSize && std::abs(y1 - y0) >= minSize) {
                    Room r;
                    // assign id as max existing id + 1
                    int maxid = 0;
                    auto &rooms = map->GetRooms();
                    for (const auto &rr : rooms) if (rr.id > maxid) maxid = rr.id;
                    r.id = maxid + 1;
                    r.xStart = x0; r.xEnd = x1; r.yStart = y0; r.yEnd = y1;
                    r.tutorialText = "";
                    r.tutorialShown = false;
                    rooms.push_back(r);
                    DEBUG_ONLY( DBG_PRINTF("Created room id %d (%d,%d)-(%d,%d)\n", r.id, r.xStart, r.yStart, r.xEnd, r.yEnd); );
                } else {
                    DEBUG_ONLY( DBG_PRINTF("Ignored tiny room selection\n"); );
                }
            }

            // right click within a room to delete it
            if (currentMouseRight && !prevMouseRightState) {
                int wx = worldX; int wy = worldY;
                auto &rooms = map->GetRooms();
                int found = -1;
                for (int i = 0; i < (int)rooms.size(); ++i) {
                    const auto &rr = rooms[i];
                    if (wx >= rr.xStart && wx <= rr.xEnd && wy >= rr.yStart && wy <= rr.yEnd) { found = i; break; }
                }
                if (found >= 0) {
                    DEBUG_ONLY( DBG_PRINTF("Deleted room id %d at index %d\n", rooms[found].id, found); );
                    // if inspector pointed to this room, clear selection
                    if (inspectorTarget == InspectorTarget::Room && selectedRoomIndex == found) {
                        inspectorTarget = InspectorTarget::None;
                        selectedRoomIndex = -1;
                        inspectorActive = false;
                    }
                    rooms.erase(rooms.begin() + found);
                }
            }

            // SHIFT+click inside room selects it for text editing
            if (shiftHeld && currentMouseLeft && !prevMouseLeftState) {
                auto &rooms = map->GetRooms();
                int found = -1;
                for (int i = 0; i < (int)rooms.size(); ++i) {
                    const auto &rr = rooms[i];
                    if (worldX >= rr.xStart && worldX <= rr.xEnd && worldY >= rr.yStart && worldY <= rr.yEnd) { found = i; break; }
                }
                if (found >= 0) {
                    inspectorTarget = InspectorTarget::Room;
                    selectedRoomIndex = found;
                    inspectorActive = true;
                    inspectorEditingText = false;
                    inspectorRoomTab = EditModeManager::RoomInspectorTab::Text; // default
                    inspectorEditBuffer.clear();
                    inspectorTextCaret = 0;
                    DEBUG_ONLY( DBG_PRINTF("Inspector selected room index %d\n", found); );
                }
            }
        }
    }

    // Per-room save button handling: clickable dot at room bottom-right (world coords)
    if (currentMouseLeft && !prevMouseLeftState && map) {
        Camera &cam = gm.GetCamera();
        auto &rooms = map->GetRooms();
        const int btnRadius = 8;
        for (int i = 0; i < (int)rooms.size(); ++i) {
            const Room &r = rooms[i];
            int sx = r.xEnd - cam.GetOffsetX();
            int sy = r.yEnd - cam.GetOffsetY();
            if (mouseX >= sx - btnRadius && mouseX <= sx + btnRadius && mouseY >= sy - btnRadius && mouseY <= sy + btnRadius) {
                // choose filename according to current game state (tutorial vs normal)
                std::string roomsFile = (GameManager::GetInstance().GetGameState() == STATE_TUTORIAL) ? "rooms_tutorial.csv" : "rooms.csv";
                bool ok = SaveRoomsToCSV(rooms, roomsFile);
                if (ok) { lastSaveStatus = SaveStatus::Success; saveMessageTimer = 120; }
                else { lastSaveStatus = SaveStatus::Failure; saveMessageTimer = 120; }
                DEBUG_ONLY( DBG_PRINTF("Save rooms via room button %s\n", ok ? "ok" : "fail"); );
                break; // only handle first clicked
            }
        }

        // Inline UI interactions removed - clicks above/below rooms will no longer open quick controls
     }

    // Point mode handling
    if (currentEditMode == EditModeType::Point && !inspectorEditingText && !inspectorSearchActive) {
        // choose point type with number keys
        if (justPressed[KEY_INPUT_1]) { selectedPointType = PointType::Save; DEBUG_ONLY( DBG_PRINTF("Selected point type: Save\n"); ); }
        if (justPressed[KEY_INPUT_2]) { selectedPointType = PointType::Clear; DEBUG_ONLY( DBG_PRINTF("Selected point type: Clear\n"); ); }

        // Place point on left click
        if (currentMouseLeft && !prevMouseLeftState && !mouseInPalette && !mouseOverInspector && map) {
            Camera &cam = gm.GetCamera();
            int placeX = mouseX + cam.GetOffsetX();
            int placeY = mouseY + cam.GetOffsetY();
            SavePoint::Type spType = (selectedPointType == PointType::Save) ? SavePoint::Type::Save : SavePoint::Type::Clear;
            points.emplace_back(placeX, placeY, spType);
            DEBUG_ONLY( DBG_PRINTF("Placed %s point at %d,%d\n", (spType==SavePoint::Type::Save)?"Save":"Clear", placeX, placeY); );
        }

        // Remove nearest point on right click
        if (currentMouseRight && !prevMouseRightState && !mouseInPalette && !mouseOverInspector && map) {
            Camera &cam = gm.GetCamera();
            int worldX = mouseX + cam.GetOffsetX();
            int worldY = mouseY + cam.GetOffsetY();
            const int deleteRadius = 24;
            int bestIdx = -1;
            int bestDistSq = deleteRadius * deleteRadius;
            for (int i = 0; i < (int)points.size(); ++i) {
                int px = points[i].GetX();
                int py = points[i].GetY();
                int dx = px - worldX; int dy = py - worldY;
                int dsq = dx*dx + dy*dy;
                if (dsq <= bestDistSq) { bestDistSq = dsq; bestIdx = i; }
            }
            if (bestIdx >= 0) {
                DEBUG_ONLY( DBG_PRINTF("Removed point at %d,%d (index %d)\n", points[bestIdx].GetX(), points[bestIdx].GetY(), bestIdx); );
                points.erase(points.begin() + bestIdx);
            }
        }
    }

    // Select enemy type with number keys when in enemy edit mode and not typing in inspector
    if (currentEditMode == EditModeType::Enemy && !inspectorEditingText && !inspectorSearchActive) {
        if (justPressed[KEY_INPUT_1]) {
            selectedEnemyType = 0; // melee
            DEBUG_ONLY( DBG_PRINTF("Selected enemy type: melee (0)\n"); );
        }
        if (justPressed[KEY_INPUT_2]) {
            selectedEnemyType = 1; // ranged
            DEBUG_ONLY( DBG_PRINTF("Selected enemy type: ranged (1)\n"); );
        }

        // SHIFT + left click selects nearest enemy and opens inspector for it
        if (shiftHeld && currentMouseLeft && !prevMouseLeftState && !mouseInPalette && !mouseOverInspector && map) {
            Camera &cam = gm.GetCamera();
            int worldX = mouseX + cam.GetOffsetX();
            int worldY = mouseY + cam.GetOffsetY();
            const int selectRadius = 24;
            int bestIdx = -1; int bestDistSq = selectRadius * selectRadius;
            for (int i = 0; i < (int)enemies.size(); ++i) {
                int ex = static_cast<int>(std::round(enemies[i].GetX()));
                int ey = static_cast<int>(std::round(enemies[i].GetY()));
                int dx = ex - worldX; int dy = ey - worldY; int dsq = dx*dx + dy*dy;
                if (dsq <= bestDistSq) { bestDistSq = dsq; bestIdx = i; }
            }
            if (bestIdx >= 0) {
                inspectorEnemyIndex = bestIdx;
                inspectorTarget = InspectorTarget::Enemy;
                inspectorActive = true;
                inspectorEditingText = false;
                inspectorFieldIndex = 0;
                inspectorFoldoutEnemy = true;
                // sync selectedEnemyPathMode with enemy
                selectedEnemyPathMode = enemies[bestIdx].GetPathfindingMode();
                DEBUG_ONLY( DBG_PRINTF("Inspector selected enemy index %d\n", bestIdx); );
            }
        }

        // Place enemy on left click (only when not selecting with shift)
        if (!shiftHeld && currentMouseLeft && !prevMouseLeftState && !mouseInPalette && !mouseOverInspector && map) {
            Camera &cam = gm.GetCamera();
            int placeX = mouseX + cam.GetOffsetX();
            int placeY = mouseY + cam.GetOffsetY();
            enemies.emplace_back(placeX, placeY, selectedEnemyType);
            DEBUG_ONLY( DBG_PRINTF("Placed enemy type %d at %d,%d\n", selectedEnemyType, placeX, placeY); );
        }

        // Remove enemy on right click: find nearest enemy within threshold and erase
        if (currentMouseRight && !prevMouseRightState && !mouseInPalette && !mouseOverInspector && map) {
            Camera &cam = gm.GetCamera();
            int worldX = mouseX + cam.GetOffsetX();
            int worldY = mouseY + cam.GetOffsetY();
            const int deleteRadius = 24; // pixels
            int bestIdx = -1;
            int bestDistSq = deleteRadius * deleteRadius;
            for (int i = 0; i < (int)enemies.size(); ++i) {
                int ex = static_cast<int>(std::round(enemies[i].GetX()));
                int ey = static_cast<int>(std::round(enemies[i].GetY()));
                int dx = ex - worldX;
                int dy = ey - worldY;
                int dsq = dx*dx + dy*dy;
                if (dsq <= bestDistSq) {
                    bestDistSq = dsq;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0) {
                DEBUG_ONLY( DBG_PRINTF("Removed enemy at %d,%d (index %d)\n", (int)enemies[bestIdx].GetX(), (int)enemies[bestIdx].GetY(), bestIdx); );
                enemies.erase(enemies.begin() + bestIdx);
                // if inspector was pointing to this enemy, clear selection
                if (inspectorEnemyIndex == bestIdx) {
                    inspectorEnemyIndex = -1; inspectorTarget = InspectorTarget::None; inspectorActive = false;
                }
            }
        }
    }

    // Inspector path mode control click handling: a small control above Save Rooms button
    if (inspectorEnemyIndex >= 0 && inspectorEnemyIndex < (int)enemies.size()) {
        // compute inspector panel positions similar to Draw()
        float fs = inspectorFontScale;
        int pad_local = static_cast<int>(8 * fs);
        int rowH_local = static_cast<int>(32 * fs);
        int smallBtnW_local = static_cast<int>(28 * fs);
        int smallBtnH_local = static_cast<int>(22 * fs);
        int x = inspectorPosX;
        int y = inspectorPosY;
        int w = inspectorWidth;
        int h = inspectorHeight;
        int saveBtnW = static_cast<int>(100 * fs);
        int saveBtnH = static_cast<int>(28 * fs);
        int saveBtnY = y + h - pad_local - saveBtnH;
        int modeY = saveBtnY - static_cast<int>(rowH_local + 8 * fs);
        int contentX = x + pad_local;
        int labelW = static_cast<int>(88 * fs);
        int btnW = static_cast<int>(60 * fs);
        int btnH = rowH_local - static_cast<int>(6 * fs);
        int btn4X = contentX + labelW;
        int btn8X = btn4X + btnW + static_cast<int>(8 * fs);
        bool clickedMode4 = (currentMouseLeft && !prevMouseLeftState && mouseX >= btn4X && mouseX <= btn4X + btnW && mouseY >= modeY && mouseY <= modeY + btnH);
        bool clickedMode8 = (currentMouseLeft && !prevMouseLeftState && mouseX >= btn8X && mouseX <= btn8X + btnW && mouseY >= modeY && mouseY <= modeY + btnH);
        if (clickedMode4) {
            enemies[inspectorEnemyIndex].SetPathfindingMode(Enemy::PATH_4DIR);
            selectedEnemyPathMode = Enemy::PATH_4DIR;
            DEBUG_ONLY( DBG_PRINTF("Set enemy %d path mode to 4dir\n", inspectorEnemyIndex); );
        }
        if (clickedMode8) {
            enemies[inspectorEnemyIndex].SetPathfindingMode(Enemy::PATH_8DIR);
            selectedEnemyPathMode = Enemy::PATH_8DIR;
            DEBUG_ONLY( DBG_PRINTF("Set enemy %d path mode to 8dir\n", inspectorEnemyIndex); );
        }
    }

    // Helper to detect printable key presses and append char
    auto handlePrintableKeys = [&](std::string &buf, int &caret) {
        // letters
        for (int k = KEY_INPUT_A; k <= KEY_INPUT_Z; ++k) {
            if (justPressed[k]) {
                char ch = static_cast<char>('a' + (k - KEY_INPUT_A));
                if (shiftHeld) ch = static_cast<char>(std::toupper(ch));
                buf.insert(buf.begin() + caret, ch);
                caret++;
                DEBUG_ONLY( DBG_PRINTF("Inspector typing: inserted '%c' caret=%d buf='%s'\n", ch, caret, buf.c_str()); );
                return;
            }
        }
        // numbers top row
        const char shiftNumMap[10] = {')','!','@','#','$','%','^','&','*','('};
        for (int k = KEY_INPUT_0; k <= KEY_INPUT_9; ++k) {
            if (justPressed[k]) {
                int idx = k - KEY_INPUT_0;
                char ch = shiftHeld ? shiftNumMap[idx] : static_cast<char>('0' + idx);
                buf.insert(buf.begin() + caret, ch);
                caret++;
                return;
            }
        }
        // space
        if (justPressed[KEY_INPUT_SPACE]) { buf.insert(buf.begin() + caret, ' '); caret++; return; }
        // common punctuation supported by DxLib keycodes
        if (justPressed[KEY_INPUT_MINUS]) { buf.insert(buf.begin() + caret, shiftHeld ? '_' : '-'); caret++; return; }
        if (justPressed[KEY_INPUT_BACKSLASH]) { buf.insert(buf.begin() + caret, '\\'); caret++; return; }
        if (justPressed[KEY_INPUT_COMMA]) { buf.insert(buf.begin() + caret, shiftHeld ? '<' : ','); caret++; return; }
        if (justPressed[KEY_INPUT_PERIOD]) { buf.insert(buf.begin() + caret, shiftHeld ? '>' : '.'); caret++; return; }
        if (justPressed[KEY_INPUT_SLASH]) { buf.insert(buf.begin() + caret, shiftHeld ? '?' : '/'); caret++; return; }
        if (justPressed[KEY_INPUT_SEMICOLON]) { buf.insert(buf.begin() + caret, shiftHeld ? ':' : ';'); caret++; return; }
        // omit keys that may not be defined on all platforms (equals/apostrophe)
    };

    // Search field handling
    if (inspectorSearchActive) {
        handlePrintableKeys(inspectorSearchBuffer, inspectorSearchCaret);
        // backspace
        if (justPressed[KEY_INPUT_BACK]) {
            if (inspectorSearchCaret > 0 && !inspectorSearchBuffer.empty()) {
                inspectorSearchBuffer.erase(inspectorSearchCaret - 1, 1);
                inspectorSearchCaret = std::max(0, inspectorSearchCaret - 1);
            }
        }
        // delete
        if (justPressed[KEY_INPUT_DELETE]) {
            if (inspectorSearchCaret < (int)inspectorSearchBuffer.size()) inspectorSearchBuffer.erase(inspectorSearchCaret,1);
        }
        if (justPressed[KEY_INPUT_LEFT]) inspectorSearchCaret = std::max(0, inspectorSearchCaret - 1);
        if (justPressed[KEY_INPUT_RIGHT]) inspectorSearchCaret = std::min((int)inspectorSearchBuffer.size(), inspectorSearchCaret + 1);
        // ensure caret within bounds
        if (inspectorSearchCaret < 0) inspectorSearchCaret = 0;
        if (inspectorSearchCaret > (int)inspectorSearchBuffer.size()) inspectorSearchCaret = (int)inspectorSearchBuffer.size();
        // adjust view offset to keep caret visible (approx char width)
        int approxCharW = std::max(4, static_cast<int>(8 * inspectorFontScale));
        int textX = (inspectorPosX + pad) + static_cast<int>(8 * inspectorFontScale);
        int searchBoxW = inspectorWidth - pad * 2;
        int availableW = searchBoxW - (textX - (inspectorPosX + pad)) - (inspectorSearchBuffer.empty() ? 12 :  ( (smallBtnW - static_cast<int>(8 * inspectorFontScale)) + static_cast<int>(12 * inspectorFontScale) ));
        int visibleChars = std::max(1, availableW / approxCharW);
        if (inspectorSearchCaret < inspectorSearchViewOffset) inspectorSearchViewOffset = inspectorSearchCaret;
        if (inspectorSearchCaret > inspectorSearchViewOffset + visibleChars - 1) inspectorSearchViewOffset = inspectorSearchCaret - (visibleChars - 1);
    }

    // Inspector edit text handling
    if (inspectorEditingText) {
        DEBUG_ONLY( DBG_PRINTF("Inspector editing active. caret=%d buffer='%s'\n", inspectorTextCaret, inspectorEditBuffer.c_str()); );
        handlePrintableKeys(inspectorEditBuffer, inspectorTextCaret);
        if (justPressed[KEY_INPUT_BACK]) {
            if (inspectorTextCaret > 0 && !inspectorEditBuffer.empty()) {
                inspectorEditBuffer.erase(inspectorTextCaret - 1, 1);
                inspectorTextCaret = std::max(0, inspectorTextCaret - 1);
            }
        }
        if (justPressed[KEY_INPUT_DELETE]) {
            if (inspectorTextCaret < (int)inspectorEditBuffer.size()) inspectorEditBuffer.erase(inspectorTextCaret,1);
        }
        if (justPressed[KEY_INPUT_LEFT]) inspectorTextCaret = std::max(0, inspectorTextCaret - 1);
        if (justPressed[KEY_INPUT_RIGHT]) inspectorTextCaret = std::min((int)inspectorEditBuffer.size(), inspectorTextCaret + 1);
        if (inspectorTextCaret < 0) inspectorTextCaret = 0;
        if (inspectorTextCaret > (int)inspectorEditBuffer.size()) inspectorTextCaret = (int)inspectorEditBuffer.size();
        // finish editing with Enter: apply to selected room
        if (justPressed[KEY_INPUT_RETURN]) {
            if (inspectorTarget == InspectorTarget::Room && selectedRoomIndex >= 0 && map) {
                auto &rooms = map->GetRooms();
                if (selectedRoomIndex < (int)rooms.size()) {
                    rooms[selectedRoomIndex].overlayText = inspectorEditBuffer;
                }
            }
            DEBUG_ONLY( DBG_PRINTF("Inspector editing finished. final buffer='%s'\n", inspectorEditBuffer.c_str()); );
            inspectorEditingText = false;
        }
        // cancel editing with Escape
        if (justPressed[KEY_INPUT_ESCAPE]) {
            inspectorEditingText = false;
        }
    }

    // Room inspector quick controls: when a room is selected in inspector (not editing text)
    if (!inspectorEditingText && inspectorTarget == InspectorTarget::Room && selectedRoomIndex >= 0 && map) {
        auto &rooms = map->GetRooms();
        if (selectedRoomIndex >= 0 && selectedRoomIndex < (int)rooms.size()) {
            Room &sr = rooms[selectedRoomIndex];
            // Tab cycles sub-focus between Text, Size, Color
            if (justPressed[KEY_INPUT_TAB]) {
                // when tabbing away, stop text editing
                inspectorEditingText = false;
                // cycle forward or backward with Shift
                if (shiftHeld) {
                    if (inspectorRoomTab == RoomInspectorTab::Text) inspectorRoomTab = RoomInspectorTab::Color;
                    else if (inspectorRoomTab == RoomInspectorTab::Size) inspectorRoomTab = RoomInspectorTab::Text;
                    else inspectorRoomTab = RoomInspectorTab::Size;
                } else {
                    if (inspectorRoomTab == RoomInspectorTab::Text) inspectorRoomTab = RoomInspectorTab::Size;
                    else if (inspectorRoomTab == RoomInspectorTab::Size) inspectorRoomTab = RoomInspectorTab::Color;
                    else inspectorRoomTab = RoomInspectorTab::Text;
                }
            }

            // Text tab: start editing when focused and E or any typing key pressed
            if (inspectorRoomTab == RoomInspectorTab::Text) {
                if (justPressed[KEY_INPUT_E]) {
                    inspectorEditingText = true;
                    inspectorEditBuffer = sr.overlayText;
                    inspectorTextCaret = (int)inspectorEditBuffer.size();
                    DEBUG_ONLY( DBG_PRINTF("Inspector: started editing via E. buffer='%s'\n", inspectorEditBuffer.c_str()); );
                }
            }

            // Color tab: cycle color only when color tab focused
            if (inspectorRoomTab == RoomInspectorTab::Color) {
                if (justPressed[KEY_INPUT_C]) {
                    const int palette[] = { 0xFFFFFF, 0xFFFF00, 0xFF8888, 0x88FF88, 0x88FFFF, 0xFF88FF };
                    int idx = 0;
                    for (int i=0;i< (int)(sizeof(palette)/sizeof(palette[0])); ++i) if (palette[i] == sr.overlayColor) { idx = i; break; }
                    idx = (idx + 1) % (int)(sizeof(palette)/sizeof(palette[0]));
                    sr.overlayColor = palette[idx];
                }
            }

            // Size tab: adjust font size only when size tab focused
            if (inspectorRoomTab == RoomInspectorTab::Size) {
                if (justPressed[KEY_INPUT_K]) {
                    sr.overlayFontSize = std::min(96, sr.overlayFontSize + 2);
                }
                if (justPressed[KEY_INPUT_J]) {
                    sr.overlayFontSize = std::max(8, sr.overlayFontSize - 2);
                }
            }

            // New: allow numeric keys to assign a tag to the selected room when not editing text
            // Direct mapping: 1-9 assign tag numbers 1..9, 0 clears tag. Ignore inspectorTagList here.
            // Only allow room-tagging via number keys when we're in Room edit mode
            if (!inspectorEditingText && currentEditMode == EditModeType::Room) {
                 for (int k = KEY_INPUT_0; k <= KEY_INPUT_9; ++k) {
                     if (justPressed[k]) {
                         int num = (k == KEY_INPUT_0) ? 0 : (k - KEY_INPUT_0);
                         if (num == 0) sr.tag = -1; else sr.tag = num;
                         // Clear overlay text so tag text is used immediately
                         sr.overlayText.clear();
                         DEBUG_ONLY( DBG_PRINTF("Assigned tag %d to room index %d (id=%d)\n", sr.tag, selectedRoomIndex, sr.id); );
                     }
                 }
             }
         }
     }

    // Tag-text list UI interactions (when inspector is visible)
    // We'll place this near where room inspector preview is handled
    if (!inspectorEditingText && inspectorTarget == InspectorTarget::Room && inspectorFoldoutRoom) {
        // tag list interactions: number keys when tag list focused will select
        // but we need an explicit small UI; handle editing using inspectorTagSelectedIndex
        // Start editing selected tag text with key E when inspectorTagSelectedIndex >=0
        if (inspectorTagSelectedIndex >= 0) {
            if (!inspectorEditingTagText && justPressed[KEY_INPUT_E]) {
                inspectorEditingTagText = true;
                int tag = inspectorTagList[inspectorTagSelectedIndex];
                inspectorTagEditBuffer = GameManager::GetInstance().GetRoomTagText(tag);
                inspectorTextCaret = (int)inspectorTagEditBuffer.size();
            }
            if (inspectorEditingTagText) {
                // reuse printable key handler
                handlePrintableKeys(inspectorTagEditBuffer, inspectorTextCaret);
                if (justPressed[KEY_INPUT_RETURN]) {
                    int tag = inspectorTagList[inspectorTagSelectedIndex];
                    GameManager::GetInstance().SetRoomTagText(tag, inspectorTagEditBuffer);
                    inspectorEditingTagText = false;
                }
                if (justPressed[KEY_INPUT_ESCAPE]) inspectorEditingTagText = false;
            }
        }

        // Add new tag with Ctrl+N (or just N while holding Ctrl)
        bool ctrlHeld = (keyState[KEY_INPUT_LCONTROL] != 0) || (keyState[KEY_INPUT_RCONTROL] != 0);
        if (justPressed[KEY_INPUT_N] && ctrlHeld) {
            // add next numeric tag not present (1..9 then incremental)
            int candidate = 1;
            std::unordered_map<int,bool> present;
            for (int t: inspectorTagList) present[t] = true;
            while (present[candidate]) candidate++;
            inspectorTagList.push_back(candidate);
            GameManager::GetInstance().SetRoomTagText(candidate, std::string());
            inspectorTagSelectedIndex = (int)inspectorTagList.size() - 1;
            inspectorEditingTagText = true;
            inspectorTagEditBuffer.clear();
            inspectorTextCaret = 0;
        }

        // Remove selected tag with Delete
        if (inspectorTagSelectedIndex >= 0 && justPressed[KEY_INPUT_DELETE]) {
            int tag = inspectorTagList[inspectorTagSelectedIndex];
            GameManager::GetInstance().ClearRoomTagText(tag);
            inspectorTagTexts.erase(tag);
            inspectorTagList.erase(inspectorTagList.begin() + inspectorTagSelectedIndex);
            if (inspectorTagSelectedIndex >= (int)inspectorTagList.size()) inspectorTagSelectedIndex = (int)inspectorTagList.size() - 1;
        }

        // Navigate tag list with up/down keys
        if (justPressed[KEY_INPUT_UP]) {
            if (inspectorTagSelectedIndex > 0) inspectorTagSelectedIndex--;
        }
        if (justPressed[KEY_INPUT_DOWN]) {
            if (inspectorTagSelectedIndex + 1 < (int)inspectorTagList.size()) inspectorTagSelectedIndex++;
        }
    }

    // Additionally, on entering edit mode or when loading inspector, populate inspectorTagList from GameManager
    if (justPressed[KEY_INPUT_I] && !prevIKeyState) { // example: I toggles inspector focus list reload
        // build list from GameManager map
        inspectorTagList.clear();
        inspectorTagTexts.clear();
        // fetch known tags by checking GameManager mapping for 1..256 (simple) - better to expose mapping but keep simple
        for (int t=1;t<=256;++t) {
            std::string txt = GameManager::GetInstance().GetRoomTagText(t);
            if (!txt.empty()) { inspectorTagList.push_back(t); inspectorTagTexts[t] = txt; }
        }
        if (!inspectorTagList.empty()) inspectorTagSelectedIndex = 0; else inspectorTagSelectedIndex = -1;
    }

    // Fallback: also accept CheckHitKey for numeric keys to ensure keypress is recognized for tagging
    // Only when in Room edit mode
    if (currentEditMode == EditModeType::Room && inspectorTarget == InspectorTarget::Room && selectedRoomIndex >= 0) {
         auto &rooms = map->GetRooms();
         if (selectedRoomIndex >= 0 && selectedRoomIndex < (int)rooms.size()) {
             Room &sr = rooms[selectedRoomIndex];
             // use CheckHitKey which returns 1 only on initial press
             if (CheckHitKey(KEY_INPUT_0) == 1) { sr.tag = -1; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Cleared tag for room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_1) == 1) { sr.tag = 1; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 1 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_2) == 1) { sr.tag = 2; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 2 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_3) == 1) { sr.tag = 3; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 3 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_4) == 1) { sr.tag = 4; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 4 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_5) == 1) { sr.tag = 5; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 5 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_6) == 1) { sr.tag = 6; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 6 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_7) == 1) { sr.tag = 7; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 7 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_8) == 1) { sr.tag = 8; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 8 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
             if (CheckHitKey(KEY_INPUT_9) == 1) { sr.tag = 9; sr.overlayText.clear(); DEBUG_ONLY( DBG_PRINTF("Assigned tag 9 to room index %d (id=%d) via CheckHitKey\n", selectedRoomIndex, sr.id); ); }
         }
     }

    // Note: prevIKeyState must be declared earlier; ensure prevKeyState tracking includes it

    // copy current keyState to prevKeyState for next frame
    for (int i=0;i<256;++i) prevKeyState[i] = (unsigned char)keyState[i];

    prevEKeyState = currentEKeyState;
    prevMouseLeftState = currentMouseLeft;
    prevMouseRightState = currentMouseRight;

    // Room edit mode handling
    // NOTE: The detailed room-mode interactions are implemented above in this function
}

// End of EditModeUpdate.cpp
