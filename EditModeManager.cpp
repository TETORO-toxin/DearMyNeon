#include "EditModeManager.h"
#include "game_manager.h"
#include <fstream>
#include "Code/debug_utils.h"

// EditModeManager の責務分割
// このファイルは EditModeManager クラスの最小限の実装（セッタ／ゲッタ）を保持します。
// 大きな処理である Update() と Draw() は、それぞれ EditModeUpdate.cpp / EditModeDraw.cpp に
// 分割して移動しています。これにより、ファイルの目的を明確にし、可読性と保守性を向上させます。

EditModeManager::EditModeManager() {
    // load persisted inspector state if available
    LoadInspectorState();
}

EditModeManager::~EditModeManager() {
    SaveInspectorState();
}

void EditModeManager::SetTilePalette(TilePalette* palette) { tilePalette = palette; }
// DEBUGビルドでのみエディットモードを有効化できるようにする
void EditModeManager::SetEditMode(bool enabled) {
#if DEBUG_ACTIVE
    isEditMode = enabled;
#else
    // リリースビルドでは通常無効だが、チュートリアルモードではエディタ機能を許可する
    if (enabled) {
        try {
            if (GameManager::GetInstance().GetGameState() == STATE_TUTORIAL) {
                isEditMode = true;
            } else {
                isEditMode = false;
            }
        } catch(...) {
            // fallback: deny edit mode if game manager not available
            isEditMode = false;
        }
    } else {
        isEditMode = false;
    }
#endif
}

bool EditModeManager::IsEditMode() const { return isEditMode; }

void EditModeManager::SaveInspectorState() const {
    std::ofstream ofs("inspector_state.txt", std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open()) return;
    ofs << (inspectorFoldoutPlayer ? 1 : 0) << "\n";
    ofs << (inspectorFoldoutEnemy ? 1 : 0) << "\n";
    ofs << inspectorPosX << " " << inspectorPosY << "\n";
    ofs << inspectorWidth << " " << inspectorHeight << "\n";
    ofs << inspectorFontScale << " " << (inspectorDocked ? 1 : 0) << "\n";
    ofs.close();
}

void EditModeManager::LoadInspectorState() {
    std::ifstream ifs("inspector_state.txt");
    if (!ifs.is_open()) return;
    int p=1,e=1; int px=inspectorPosX, py=inspectorPosY; int w=inspectorWidth, h=inspectorHeight; float fs=inspectorFontScale; int dock=0;
    ifs >> p >> e >> px >> py >> w >> h >> fs >> dock;
    inspectorFoldoutPlayer = (p != 0);
    inspectorFoldoutEnemy = (e != 0);
    inspectorPosX = px; inspectorPosY = py;
    inspectorWidth = std::max(inspectorMinWidth, w);
    inspectorHeight = std::max(inspectorMinHeight, h);
    inspectorFontScale = fs > 0.5f ? fs : 1.0f;
    inspectorDocked = (dock != 0);
    ifs.close();
}

// New APIs for inspector customization
void EditModeManager::SetInspectorFontScale(float scale) {
    if (scale < 0.6f) scale = 0.6f;
    if (scale > 2.0f) scale = 2.0f;
    inspectorFontScale = scale;
}

float EditModeManager::GetInspectorFontScale() const { return inspectorFontScale; }

void EditModeManager::SetInspectorDocked(bool docked) {
    inspectorDocked = docked;
    if (docked) {
        // dock to right side of screen
        inspectorPosX = SCREEN_WIDTH - inspectorWidth - 8;
    }
}

bool EditModeManager::IsInspectorDocked() const { return inspectorDocked; }

void EditModeManager::SetInspectorSize(int w, int h) {
    inspectorWidth = std::max(inspectorMinWidth, w);
    inspectorHeight = std::max(inspectorMinHeight, h);
}

// Returns a short help string describing controls for the current edit mode
std::string EditModeManager::GetModeHelpText() const {
    switch (currentEditMode) {
        case EditModeType::Tile:
            return std::string("Tile Mode: Left-click to paint, Right-click to erase, Drag to pan, P to toggle palette");
        case EditModeType::Enemy:
            return std::string("Enemy Mode: Left-click to place, Right-click to remove, Click enemy to select for properties");
        case EditModeType::Point:
            return std::string("Point Mode: Place Save/Clear points with left-click; press Y to save at save point");
        case EditModeType::Room:
            return std::string("Room Mode: R to toggle, Left-drag to create room rectangle, Right-click to delete room, Save Rooms button saves to rooms.csv");
        default:
            return std::string("");
    }
}

void EditModeManager::SelectRoom(int roomId) {
    selectedRoomId = roomId;
}

int EditModeManager::GetSelectedRoom() const {
    return selectedRoomId;
}

bool EditModeManager::IsRoomEditMode() {
    // simplest behavior: return whether currentEditMode == Room for a global instance if exists
    // if no singleton, assume static false
    return false;
}

EditModeManager& EditModeManager::GetInstance() {
    static EditModeManager instance;
    return instance;
}