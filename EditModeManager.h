#pragma once
#include "player.h"
#include "map.h"
#include "enemy.h"
#include "TilePalette.h"
#include "SavePoint.h"
#include "RespawnLine.h"
#include <vector>
#include <string>
#include <unordered_map>

enum class EditModeType {
    Tile,
    Enemy,
    Point,
    Room
};

enum class PointType {
    Save,
    Clear
};

enum class SaveStatus {
    None,
    Success,
    Failure
};

// New: inspector target type
enum class InspectorTarget {
    None,
    Player,
    Enemy,
    Room
};

class EditModeManager {
public:
    EditModeManager();
    ~EditModeManager();
    void Update(Player& player, Map* map, std::vector<Enemy>& enemies, std::vector<SavePoint>& points);
    void Draw();
    void SetTilePalette(TilePalette* palette);
    void SetEditMode(bool enabled);
    bool IsEditMode() const;
    // Persist inspector UI state between runs
    void SaveInspectorState() const;
    void LoadInspectorState();
    bool LoadEnemyDataFromCSV(std::vector<Enemy>& enemies, const std::string& filename);
    bool SaveEnemyDataToCSV(const std::vector<Enemy>& enemies, const std::string& filename);
    // Upgrade legacy enemy CSV (old 5-column format) to new 6-column format with IDs
    bool UpgradeEnemyCSVToNewFormat(const std::string& filename);
    // Points (save/clear) CSV
    bool SavePointsToCSV(const std::vector<SavePoint>& points, const std::string& filename);
    bool LoadPointsFromCSV(std::vector<SavePoint>& points, const std::string& filename);
    // Respawn lines CSV
    bool SaveRespawnLinesToCSV(const std::vector<RespawnLine>& lines, const std::string& filename);
    bool LoadRespawnLinesFromCSV(std::vector<RespawnLine>& lines, const std::string& filename);

    // New APIs for inspector customization
    void SetInspectorFontScale(float scale);
    float GetInspectorFontScale() const;
    void SetInspectorDocked(bool docked);
    bool IsInspectorDocked() const;
    void SetInspectorSize(int w, int h);

    // Returns a short help string describing controls for the current edit mode
    std::string GetModeHelpText() const;

    // APIs to manage tag->text mappings locally in the inspector
    // (these manipulate GameManager's mapping or local copy)
    void InspectorAddTagText(int tag, const std::string& text);
    void InspectorRemoveTagText(int tag);
    void InspectorSetTagText(int tag, const std::string& text);

    void SelectRoom(int roomId);
    int GetSelectedRoom() const;
    static bool IsRoomEditMode();
    static EditModeManager& GetInstance();

private:
    bool isEditMode = false;
    EditModeType currentEditMode = EditModeType::Tile;
    PointType selectedPointType = PointType::Save;
    int selectedEnemyType = 0;
    TilePalette* tilePalette = nullptr;
    bool tilePaletteVisible = false; // show/hide tile palette
    void EnsureTilePaletteInitialized();
    
    TilePalette* GetTilePalette() { return tilePalette; }

    bool prevMouseLeft = false;
    bool isDraggingPalette = false;
    bool tilePaletteMouseDown = false; // track down inside palette for clicks
    int tilePaletteMouseDownX = 0;
    int tilePaletteMouseDownY = 0;

    SaveStatus lastSaveStatus = SaveStatus::None;
    int saveMessageTimer = 0; // 保存メッセージ表示用タイマ

    // Selected enemy index for Enemy edit mode (-1 = none)
    int selectedEnemyIndex = -1;
    // Cached mode of selected enemy for display
    Enemy::PathfindingMode selectedEnemyPathMode = Enemy::PATH_4DIR;

    // Inspector state
    InspectorTarget inspectorTarget = InspectorTarget::None; // which object is being inspected
    int inspectorEnemyIndex = -1; // enemy index when inspecting an enemy
    int inspectorFieldIndex = 0; // which numeric field is selected for editing (0..)
    float inspectorEditValue = 0.0f; // current edit value (float for generality)
    bool inspectorActive = false; // whether inspector UI is open

    // Inspector panel UI state (draggable)
    int inspectorPosX = 600;
    int inspectorPosY = 40;
    int inspectorWidth = 320; // increased width for more space
    int inspectorHeight = 360; // increased height for more content
    bool inspectorDragging = false;
    int inspectorDragOffsetX = 0;
    int inspectorDragOffsetY = 0;

    // Text edit state for inspector
    bool inspectorEditingText = false;
    std::string inspectorEditBuffer;

    // New: search box and foldout UI state for Unity-like inspector
    bool inspectorSearchActive = false; // whether search box has focus
    std::string inspectorSearchBuffer; // current search text
    bool inspectorFoldoutPlayer = true; // whether Player section is expanded
    bool inspectorFoldoutEnemy = true;  // whether Enemy section is expanded
    bool inspectorCollapsed = false; // whether whole inspector is collapsed (tabbed)
    bool inspectorFoldoutRoom = true; // whether Room section is expanded

    // Rectangle selection for creating Rooms in edit mode
    bool rectSelecting = false;
    int rectStartX = 0;
    int rectStartY = 0;
    int rectCurX = 0;
    int rectCurY = 0;

    // Room inspector
    int selectedRoomIndex = -1; // index into Map::GetRooms()
    int selectedRoomId = -1;

    // New inspector customization state
    // Which sub-control inside the Room inspector is focused: Text / Size / Color
    enum class RoomInspectorTab { Text = 0, Size = 1, Color = 2 };
    RoomInspectorTab inspectorRoomTab = RoomInspectorTab::Text;

    // New inspector customization state
    float inspectorFontScale = 1.0f; // pseudo-scaling by changing spacing
    bool inspectorDocked = false; // dock to right side of screen
    bool inspectorResizing = false; // resizing via right edge
    int inspectorResizeStartX = 0;
    int inspectorStartWidth = 0;
    int inspectorMinWidth = 220;
    int inspectorMinHeight = 160;

    // Height resizing state
    bool inspectorResizingHeight = false;
    int inspectorResizeStartY = 0;
    int inspectorStartHeight = 0;

    // Text caret/scroll/selection state for inspector edit field
    int inspectorTextCaret = 0; // caret index in inspectorEditBuffer
    int inspectorTextViewOffset = 0; // start index shown
    int inspectorTextSelectStart = -1; // -1 = no selection
    int inspectorCaretBlinkTimer = 0;

    // Search field caret/scroll state
    int inspectorSearchCaret = 0;
    int inspectorSearchViewOffset = 0;

    // Tag->text list for inspector UI (ordered list of tags)
    std::vector<int> inspectorTagList; // order of tags shown
    std::unordered_map<int, std::string> inspectorTagTexts; // mapping tag->text

    // UI state for tag list
    int inspectorTagSelectedIndex = -1; // index into inspectorTagList
    bool inspectorEditingTagText = false; // whether editing the selected tag text
    std::string inspectorTagEditBuffer;
};





















