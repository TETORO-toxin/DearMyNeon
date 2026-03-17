#include "EnemySpawner.h"

// 簡易エネミースポナー: ベクタに直接エネミーを追加するヘルパー群

void EnemySpawner::AddEnemy(std::vector<Enemy>& enemies, int x, int y, int type) {
    enemies.emplace_back(x, y, type);
}

void EnemySpawner::SpawnTutorialEnemies(std::vector<Enemy>& enemies) {
    AddEnemy(enemies, 400, 400, 0); // 近接のチュートリアル敵
    AddEnemy(enemies, 500, 400, 1); // 遠距離のチュートリアル敵
}

void EnemySpawner::SpawnCombatEnemies(std::vector<Enemy>& enemies) {
    AddEnemy(enemies, 800, 400, 0);
    AddEnemy(enemies, 900, 400, 1);
    AddEnemy(enemies, 1000, 400, 0);
}

void EnemySpawner::SpawnBoss(std::vector<Enemy>& enemies) {
    AddEnemy(enemies, 1600, 350, 2); // ボス（タイプ2）を追加
}
