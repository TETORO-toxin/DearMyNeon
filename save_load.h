#ifndef SAVE_LOAD_H
#define SAVE_LOAD_H

#include <fstream>      // C++標準ライブラリのファイルストリームを使うため
#include <vector>
#include <string>

#include "player.h"
#include "enemy.h"

const std::string SAVE_FILE_NAME = "save.dat";
const std::string TUTORIAL_FLAG_FILE = "tutorial_flag.dat";

// ゲームデータをファイルに保存する
bool SaveGameData(const Player::PlayerSaveData& playerData, const std::vector<Enemy::EnemySaveData>& enemyData);

// ファイルからゲームデータをロードする
bool LoadGameData(Player::PlayerSaveData& playerData, std::vector<Enemy::EnemySaveData>& enemyData);

// チュートリアル完了フラグの永続化
bool SaveTutorialFlag(bool completed);
bool LoadTutorialFlag(bool& outCompleted);

#endif // SAVE_LOAD_H
