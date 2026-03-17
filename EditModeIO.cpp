#include "EditModeManager.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include "Code/debug_utils.h"

static inline std::string trim(const std::string &s) {
    size_t a = 0; while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size(); while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b-a);
}

// SaveEnemyDataToCSV ÇÃà¿ëSé¿ëïÅistd::ofstreamÅj
bool EditModeManager::SaveEnemyDataToCSV(const std::vector<Enemy>& enemies, const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return false;
    for (const auto& enemy : enemies) {
        Enemy::EnemySaveData d = enemy.GetSaveData();
        ofs << d.id << ',' << d.ex << ',' << d.ey << ',' << d.eLife << ',' << d.eType << ',' << (d.eIsActive ? 1 : 0) << '\n';
    }
    return true;
}

bool EditModeManager::LoadEnemyDataFromCSV(std::vector<Enemy>& enemies, const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return false;

    enemies.clear();
    std::string line;
    int lineNo = 0;
    while (std::getline(ifs, line)) {
        ++lineNo;
        // skip empty lines or comments
        std::string tline = trim(line);
        if (tline.empty() || tline[0] == '#') continue;

        std::vector<std::string> cols;
        std::stringstream ss(tline);
        std::string cell;
        while (std::getline(ss, cell, ',')) cols.push_back(trim(cell));

        if (cols.size() != 5 && cols.size() != 6) {
            // malformed line -> skip
            DEBUG_ONLY( DrawFormatString(10, 220, GetColor(255,0,0), "Enemy CSV parse error line %d: expected 5 or 6 cols, got %d", lineNo, (int)cols.size()); );
            continue;
        }

        try {
            int id = -1;
            int ex = 0;
            int ey = 0;
            int eLife = 0;
            int eType = 0;
            int eIsActiveInt = 0;

            if (cols.size() == 6) {
                id = std::stoi(cols[0]);
                ex = std::stoi(cols[1]);
                ey = std::stoi(cols[2]);
                eLife = std::stoi(cols[3]);
                eType = std::stoi(cols[4]);
                eIsActiveInt = std::stoi(cols[5]);
            } else {
                // old format: ex,ey,eLife,eType,eIsActive
                ex = std::stoi(cols[0]);
                ey = std::stoi(cols[1]);
                eLife = std::stoi(cols[2]);
                eType = std::stoi(cols[3]);
                eIsActiveInt = std::stoi(cols[4]);
                id = -1; // let Enemy constructor assign a new id
            }

            // Clamp type to supported range (0 or 1)
            if (eType < 0 || eType > 1) eType = 0;

            Enemy newEnemy(ex, ey, eType, id);
            // If id was -1, obtain the assigned id from the instance
            int assignedId = (id == -1) ? newEnemy.GetSaveData().id : id;
            Enemy::EnemySaveData data = { assignedId, ex, ey, eLife, eType, eIsActiveInt != 0 };
            newEnemy.SetSaveData(data);
            enemies.push_back(newEnemy);
        } catch (const std::exception&) {
            DEBUG_ONLY( DrawFormatString(10, 220, GetColor(255,0,0), "Enemy CSV conversion error line %d", lineNo); );
            continue;
        }
    }

    return true;
}

// Points CSV
bool EditModeManager::SavePointsToCSV(const std::vector<SavePoint>& points, const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return false;
    // CSV format: x,y,type (type: 0=Save,1=Clear)
    for (const auto& p : points) {
        int t = (p.GetType() == SavePoint::Type::Save) ? 0 : 1;
        ofs << p.GetX() << ',' << p.GetY() << ',' << t << '\n';
    }
    return true;
}

bool EditModeManager::LoadPointsFromCSV(std::vector<SavePoint>& points, const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return false;
    points.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        int x=0,y=0,t=0;
        char comma1=0, comma2=0;
        std::istringstream iss(line);
        if (!(iss >> x >> comma1 >> y >> comma2 >> t)) {
            // try alternative parsing using sscanf for robustness
            if (sscanf_s(line.c_str(), "%d,%d,%d", &x, &y, &t) != 3) continue;
        }
        SavePoint::Type ty = (t == 1) ? SavePoint::Type::Clear : SavePoint::Type::Save;
        points.emplace_back(x,y,ty);
    }
    return true;
}

bool EditModeManager::SaveRespawnLinesToCSV(const std::vector<RespawnLine>& lines, const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return false;
    // CSV format: x1,y1,x2,y2
    for (const auto& l : lines) {
        ofs << l.x1 << ',' << l.y1 << ',' << l.x2 << ',' << l.y2 << '\n';
    }
    return true;
}

bool EditModeManager::LoadRespawnLinesFromCSV(std::vector<RespawnLine>& lines, const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return false;
    lines.clear(); std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        int x1=0,y1=0,x2=0,y2=0; if (sscanf_s(line.c_str(), "%d,%d,%d,%d", &x1, &y1, &x2, &y2) != 4) continue; lines.emplace_back(x1,y1,x2,y2);
    }
    return true;
}

bool EditModeManager::UpgradeEnemyCSVToNewFormat(const std::string& filename) {
    // Read file, detect if it's old format (5 columns) and convert by adding incremental ids
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) lines.push_back(line);
    ifs.close();

    bool needsUpgrade = false;
    for (const auto& l : lines) {
        std::string t = trim(l);
        if (t.empty() || t[0] == '#') continue;
        size_t commaCount = std::count(t.begin(), t.end(), ',');
        if (commaCount == 4) { needsUpgrade = true; break; } // 5 columns -> 4 commas
    }

    if (!needsUpgrade) return true; // already new format or empty

    std::vector<Enemy> enemies;
    int nextId = 1;
    for (const auto& l : lines) {
        std::string t = trim(l);
        if (t.empty() || t[0] == '#') continue;
        std::vector<std::string> cols;
        std::stringstream ss(t);
        std::string cell;
        while (std::getline(ss, cell, ',')) cols.push_back(trim(cell));
        if (cols.size() == 5) {
            try {
                int ex = std::stoi(cols[0]);
                int ey = std::stoi(cols[1]);
                int eLife = std::stoi(cols[2]);
                int eType = std::stoi(cols[3]);
                int eIsActive = std::stoi(cols[4]);
                if (eType < 0 || eType > 1) eType = 0;
                Enemy ent(ex, ey, eType, nextId);
                Enemy::EnemySaveData d = { nextId, ex, ey, eLife, eType, eIsActive != 0 };
                ent.SetSaveData(d);
                enemies.push_back(ent);
                ++nextId;
            } catch (...) {
                continue;
            }
        }
    }

    // write back to same filename (overwrite) in new format
    if (!SaveEnemyDataToCSV(enemies, filename)) return false;
    return true;
}
