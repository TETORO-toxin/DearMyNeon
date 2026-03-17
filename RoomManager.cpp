#include "RoomManager.h"
#include "player.h"
#include "Camera.h"

// 必要な型のダミー定義（本来は適切なヘッダをインクルード）
struct EnemyData {};
struct ItemData {};
struct SavePointData {};
struct Vector2 { int x = 0, y = 0; };

RoomManager* RoomManager::s_instance = nullptr;

RoomManager::RoomManager() {
    // 新しいRoom構造体に合わせて初期化
    rooms.push_back({ 1, 0, 500, 0, 0 });     // 部屋1：チュートリアル
    rooms.push_back({ 2, 501, 1000, 0, 0 });
    rooms.push_back({ 3, 1001, 1500, 0, 0 });
    rooms.push_back({ 4, 1501, 2000, 0, 0 });
}

RoomManager& RoomManager::GetInstance() {
    if (!s_instance) {
        s_instance = new RoomManager();
    }
    return *s_instance;
}

void RoomManager::Update(int playerX) {
    for (const auto& room : rooms) {
        if (playerX >= room.xStart && playerX <= room.xEnd) {
            currentRoomID = room.id;
            break;
        }
    }
}

int RoomManager::GetCurrentRoomID(int playerX) const {
    return currentRoomID;
}

bool RoomManager::IsTutorialDone() const { return tutorialDone; }
void RoomManager::MarkTutorialDone() { tutorialDone = true; }

bool RoomManager::IsCombatSpawned() const { return combatSpawned; }
void RoomManager::MarkCombatSpawned() { combatSpawned = true; }

bool RoomManager::IsBossDefeated() const { return bossDefeated; }
void RoomManager::MarkBossDefeated() { bossDefeated = true; }

int RoomManager::GetRoomIdAtPosition(const Vector2& pos) const {
    for (const auto& room : rooms) {
        // y座標も考慮する場合
        if (pos.x >= room.xStart && pos.x <= room.xEnd &&
            pos.y >= room.yStart && pos.y <= room.yEnd) {
            return room.id;
        }
    }
    return -1;
}

void RoomManager::SetRoomTag(int roomId, int tag) {
    for (auto& room : rooms) {
        if (room.id == roomId) {
            room.tag = tag;
            break;
        }
    }
}

void RoomManager::SetRoomColorByTag(int roomId, int tag) {
    for (auto& room : rooms) {
        if (room.id == roomId) {
            // overlayColorをタグに応じて設定
            switch (tag) {
                case 1: room.overlayColor = 0x0000FF; break; // Blue
                case 2: room.overlayColor = 0xFF0000; break; // Red
                case 3: room.overlayColor = 0x00FF00; break; // Green
                case 4: room.overlayColor = 0xFFFF00; break; // Yellow
                case 5: room.overlayColor = 0xFF00FF; break; // Magenta
                default: room.overlayColor = 0x888888; break; // Gray
            }
            break;
        }
    }
}

void RoomManager::SetRoomText(int roomId, const std::string& text) {
    for (auto& room : rooms) {
        if (room.id == roomId) {
            room.overlayText = text;
            break;
        }
    }
}

std::string RoomManager::GetRoomText(int roomId) const {
    for (const auto& room : rooms) {
        if (room.id == roomId) {
            return room.overlayText;
        }
    }
    return "";
}

bool RoomManager::IsPlayerInRoom(int roomId) const {
    // Cannot access Player singleton safely here; provide a conservative implementation
    for (const auto& room : rooms) {
        if (room.id == roomId) {
            // Without runtime player position, assume false
            return false;
        }
    }
    return false;
}

