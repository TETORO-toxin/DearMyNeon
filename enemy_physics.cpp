#include "enemy.h"
#include "Map.h"
#include "game_manager.h"
#include <cmath>

// 移動可能判定: 指定座標に敵の当たり判定矩形を置いたときに通行可能か判定する
bool Enemy::CanMoveToPos(float nx, float ny, Map& map) const
{
    const float scale = 2.0f;
    SpriteInfo sinfo = (type == 0) ? meleeSpriteInfoMap.at(currentState) : rangedSpriteInfoMap.at(currentState);

    int halfW = static_cast<int>((sinfo.width * scale) / 2.0f);
    int height = static_cast<int>(sinfo.height * scale);

    int tileSize = map.GetTileSize();

    RECT r;
    r.left   = static_cast<int>(nx - halfW);
    r.top    = static_cast<int>(ny - height);
    r.right  = static_cast<int>(nx + halfW);
    r.bottom = static_cast<int>(ny);

    int tx0 = r.left / tileSize;
    int tx1 = (r.right - 1) / tileSize;
    int ty0 = r.top / tileSize;
    int ty1 = (r.bottom - 1) / tileSize;

    if (tx0 < 0 || ty0 < 0 || tx1 >= map.GetWidth() || ty1 >= map.GetHeight()) return false;

    const float lateralTolerance = 4.0f;

    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            CollisionType ct = map.GetCollisionTypeAt(tx, ty);

            if (ct == CollisionType::Wall) return false; // 壁は不可

            // 床の場合は底辺がタイルの上より下に来ていないか確認（突き抜け防止）
            if (ct == CollisionType::Floor || ct == CollisionType::FloorSide) {
                float tileTop = static_cast<float>(ty) * static_cast<float>(tileSize);
                if (static_cast<float>(r.bottom) > tileTop + lateralTolerance) return false;
            }
        }
    }

    return true;
}

// 重力処理と地面スナップ
void Enemy::ApplyGravityAndSnap(Map& map, int tileSize)
{
    #ifdef ENEMY_GRAVITY
    const float gravity = ENEMY_GRAVITY;
    #else
    const float gravity = PLAYER_GRAVITY;
    #endif

    if (std::isnan(vy) || vy < -POSITION_BOUND_Y || vy > POSITION_BOUND_Y) vy = 0.0f;
    vy += gravity;
    float nextY = static_cast<float>(y) + vy;

    // 垂直移動後の矩形範囲を計算 - floatで計算してからintへ
    float nextLeftF = static_cast<float>(x) - static_cast<float>(DEFAULT_TILE_SIZE);
    float nextTopF = nextY - 2.0f * static_cast<float>(DEFAULT_TILE_SIZE);
    float nextRightF = static_cast<float>(x) + static_cast<float>(DEFAULT_TILE_SIZE);
    float nextBottomF = nextY;
    RECT nextRectV = { static_cast<int>(nextLeftF), static_cast<int>(nextTopF), static_cast<int>(nextRightF), static_cast<int>(nextBottomF) };

    int v_tx0 = nextRectV.left / tileSize; int v_tx1 = nextRectV.right / tileSize; int v_ty0 = nextRectV.top / tileSize; int v_ty1 = nextRectV.bottom / tileSize;

    bool snappedToFloor = false; constexpr float VERT_SNAP_TOL = 4.0f;
    for (int ty = v_ty0; ty <= v_ty1 && !snappedToFloor; ++ty) {
        for (int tx = v_tx0; tx <= v_tx1; ++tx) {
            CollisionType ct = map.GetCollisionTypeAt(tx, ty);
            if (ct == CollisionType::Floor) {
                float tileTop = static_cast<float>(ty) * static_cast<float>(tileSize);
                if (vy > 0.0f) {
                    // 上から落ちてきてタイル頂点近傍であればスナップして着地させる
                    if (static_cast<float>(y) < tileTop + VERT_SNAP_TOL && (nextY >= tileTop - 0.5f)) { y = static_cast<int>(tileTop); vy = 0.0f; snappedToFloor = true; break; }
                    if (static_cast<float>(nextRectV.bottom) > tileTop && static_cast<float>(nextRectV.top) < tileTop) { y = static_cast<int>(tileTop); vy = 0.0f; snappedToFloor = true; break; }
                }
            }
        }
    }
    if (!snappedToFloor) { y = static_cast<int>(nextY); isOnGround = false; }
    else { isOnGround = true; vy = 0.0f; }
}

// 他の敵との分離処理（重なりを避ける）
void Enemy::SeparateFromEnemies()
{
    const float separationDist = static_cast<float>(DEFAULT_TILE_SIZE);
    auto &allEnemies = GameManager::GetInstance().GetEnemies();
    for (auto &other : allEnemies) {
        if (&other == this) continue; if (!other.IsActive()) continue;
        float ox = other.GetX(); float oy = other.GetY(); float dx = static_cast<float>(x) - ox; float dy = static_cast<float>(y) - oy;
        float dist2 = dx * dx + dy * dy; float minDist = separationDist;
        if (dist2 == 0.0f) { int jitter = (GetRand(1) == 0) ? -1 : 1; x += jitter; }
        else if (dist2 < minDist * minDist) { float dist = std::sqrt(dist2); float overlap = minDist - dist; float nx = dx / dist; float ny = dy / dist; x = static_cast<int>(x + nx * (overlap * 0.5f)); y = static_cast<int>(y + ny * (overlap * 0.5f)); }
    }
}

// プレイヤーとの分離（近すぎる場合に押し出す）
void Enemy::SeparateFromPlayer(Player& player, Map& map, int mapPixelH)
{
    if (player.IsDead()) return;
    float px = static_cast<float>(player.GetX()); float py = static_cast<float>(player.GetY());
    float dxp = static_cast<float>(x) - px; float dyp = static_cast<float>(y) - py;
    float dist2p = dxp * dxp + dyp * dyp; float minDistP = static_cast<float>(DEFAULT_TILE_SIZE);
    if (dist2p == 0.0f) {
        const float JITTER = 1.0f; bool moved = false;
        for (int sign = -1; sign <= 1 && !moved; sign += 2) {
            float nx = static_cast<float>(x) + sign * JITTER; float ny = static_cast<float>(y);
            if (CanMoveToPos(nx, ny, map)) { x = static_cast<int>(nx); y = static_cast<int>(ny); moved = true; }
        }
    } else if (dist2p < minDistP * minDistP) {
        float distp = std::sqrt(dist2p); float overlap = minDistP - distp; float nxp = dxp / distp; float nyp = dyp / distp;
        const int STEPS = 4; float desiredMove = overlap + 1.0f; bool applied = false;
        for (int s = STEPS; s >= 1; --s) {
            float move = desiredMove * (static_cast<float>(s) / static_cast<float>(STEPS));
            float targetX = static_cast<float>(x) + nxp * move; float targetY = static_cast<float>(y) + nyp * move;
            if (CanMoveToPos(targetX, targetY, map)) { x = static_cast<int>(targetX); y = static_cast<int>(targetY); applied = true; break; }
        }
        (void)applied;
        if (y < 0) vy = std::max(vy, 3.0f);
        const int FALL_MARGIN = DEFAULT_TILE_SIZE * 2; if (y > mapPixelH + FALL_MARGIN) { life = 0; currentState = ENEMY_DEAD; isActive = false; return; }
    }
}
