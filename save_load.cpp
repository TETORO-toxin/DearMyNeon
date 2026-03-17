/*!
 * @file save_load.cpp
 * @brief セーブ/ロード処理（バイナリ）
 */

#include "save_load.h"
#include <iostream> // std::cerr 使用

// プレイヤーと敵データをファイルに書き出す
bool SaveGameData(const Player::PlayerSaveData& playerData, const std::vector<Enemy::EnemySaveData>& enemyData)
{
    std::ofstream ofs(SAVE_FILE_NAME, std::ios::binary | std::ios::trunc);

    if (!ofs.is_open()) {
        std::cerr << "Failed to open save file for writing: " << SAVE_FILE_NAME << std::endl;
        return false;
    }

    // 構造体をそのままバイナリ出力する簡易フォーマット
    ofs.write(reinterpret_cast<const char*>(&playerData), sizeof(Player::PlayerSaveData));

    size_t enemyCount = enemyData.size();
    ofs.write(reinterpret_cast<const char*>(&enemyCount), sizeof(size_t));

    for (const auto& enemy : enemyData) {
        ofs.write(reinterpret_cast<const char*>(&enemy), sizeof(Enemy::EnemySaveData));
    }

    return true;
}

// ファイルから読み込んでプレイヤー/敵データを復元する
bool LoadGameData(Player::PlayerSaveData& playerData, std::vector<Enemy::EnemySaveData>& enemyData)
{
    std::ifstream ifs(SAVE_FILE_NAME, std::ios::binary);

    if (!ifs.is_open()) {
        std::cerr << "Failed to open save file for reading: " << SAVE_FILE_NAME << " (File might not exist)" << std::endl;
        return false;
    }

    ifs.read(reinterpret_cast<char*>(&playerData), sizeof(Player::PlayerSaveData));
    if (ifs.gcount() != sizeof(Player::PlayerSaveData)) {
        std::cerr << "Error reading player data: Read " << ifs.gcount() << " bytes, expected " << sizeof(Player::PlayerSaveData) << " bytes." << std::endl;
        return false;
    }

    size_t enemyCount;
    ifs.read(reinterpret_cast<char*>(&enemyCount), sizeof(size_t));
    if (ifs.gcount() != sizeof(size_t)) {
        std::cerr << "Error reading enemy count: Read " << ifs.gcount() << " bytes, expected " << sizeof(size_t) << " bytes." << std::endl;
        return false;
    }

    enemyData.clear();
    enemyData.reserve(enemyCount);

    for (size_t i = 0; i < enemyCount; ++i) {
        Enemy::EnemySaveData enemy;
        ifs.read(reinterpret_cast<char*>(&enemy), sizeof(Enemy::EnemySaveData));
        if (ifs.gcount() != sizeof(Enemy::EnemySaveData)) {
            std::cerr << "Error reading enemy data for enemy " << i << ": Read " << ifs.gcount() << " bytes, expected " << sizeof(Enemy::EnemySaveData) << " bytes." << std::endl;
            return false;
        }
        enemyData.push_back(enemy);
    }

    return true;
}

// チュートリアル完了フラグの保存
bool SaveTutorialFlag(bool completed)
{
    std::ofstream ofs(TUTORIAL_FLAG_FILE, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open tutorial flag file for writing: " << TUTORIAL_FLAG_FILE << std::endl;
        return false;
    }
    char v = completed ? 1 : 0;
    ofs.write(&v, 1);
    return true;
}

bool LoadTutorialFlag(bool& outCompleted)
{
    std::ifstream ifs(TUTORIAL_FLAG_FILE, std::ios::binary);
    if (!ifs.is_open()) {
        // no file means not completed
        outCompleted = false;
        return false;
    }
    char v = 0;
    ifs.read(&v, 1);
    outCompleted = (v != 0);
    return true;
}