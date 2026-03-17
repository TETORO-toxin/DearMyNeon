#pragma once

#include "player.h"

class SavePoint {
public:
    enum class Type { Save, Clear };
    SavePoint(int x, int y, Type type = Type::Save);
    void Update(Player& player);
    void Draw() const;

    bool IsPlayerInRange(const Player& player) const;

    int GetX() const { return x; }
    int GetY() const { return y; }
    Type GetType() const { return type; }

private:
    int x, y;
    Type type = Type::Save;
    bool isActive = true;
};

