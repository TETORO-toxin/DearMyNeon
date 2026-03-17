#pragma once

#include <vector>
#include "enemy.h"

class EnemySpawner {
public:
    void SpawnTutorialEnemies(std::vector<Enemy>& enemies);
    void SpawnCombatEnemies(std::vector<Enemy>& enemies);
    void SpawnBoss(std::vector<Enemy>& enemies);

private:
    void AddEnemy(std::vector<Enemy>& enemies, int x, int y, int type);
};
