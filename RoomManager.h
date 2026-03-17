#pragma once

#include <vector>
#include <string>

// ïKóvÇ»å^ÇÃëOï˚êÈåæ
struct EnemyData;
struct ItemData;
struct SavePointData;
struct Vector2;

class RoomManager {
public:
    RoomManager();

    void Update(int playerX);
    int GetCurrentRoomID(int playerX) const;

    bool IsTutorialDone() const;
    void MarkTutorialDone();

    bool IsCombatSpawned() const;
    void MarkCombatSpawned();

    bool IsBossDefeated() const;
    void MarkBossDefeated();

    int GetRoomIdAtPosition(const Vector2& pos) const;
    void SetRoomTag(int roomId, int tag);
    void SetRoomColorByTag(int roomId, int tag);
    void SetRoomText(int roomId, const std::string& text);
    bool IsPlayerInRoom(int roomId) const;
    std::string GetRoomText(int roomId) const;
    static RoomManager& GetInstance();

private:
    struct Room {
        int id = 0;
        int xStart = 0;
        int xEnd = 0;
        // New Y bounds for rectangular room area (world coordinates)
        int yStart = 0;
        int yEnd = 0;
        std::vector<EnemyData> enemies;
        std::vector<ItemData> items;
        std::vector<SavePointData> savePoints;
        std::string backgroundImage;
        std::string floorImage;
        std::vector<std::vector<int>> tileMap;
        // New: tutorial trigger text (empty => no tutorial)
        std::string tutorialText;
        // Whether the tutorial was already shown (runtime flag)
        bool tutorialShown = false;
        // New: room overlay text properties for edit mode
        std::string overlayText;
        // text to render above room (editable in editor)
        int overlayColor = 0xFFFFFF;
        // RGB packed as 0xRRGGBB
        int overlayFontSize = 16;
        // pixels

        // New: numeric tag assigned in editor (-1 = none)
        int tag = -1;
    };

    std::vector<Room> rooms;
    int currentRoomID = 0;

    bool tutorialDone = false;
    bool combatSpawned = false;
    bool bossDefeated = false;

    static RoomManager* s_instance;
};
