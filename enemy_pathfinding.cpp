#include "enemy.h"
#include "Map.h"
#include <queue>
#include <algorithm>
#include <cmath>

// 経路探索（A*）に関する処理を切り出したファイルです。
// Enemy::UpdatePathfindingIfNeeded の実装をここに移動します。

void Enemy::UpdatePathfindingIfNeeded(int playerX, int playerY, Map& map)
{
    // 追跡中でない場合は経路をクリアして処理を抜ける
    if (!isChasing) { path.clear(); pathIndex = 0; return; }

    // 経路再計算のクールダウンを尊重する
    if (pathRecalcTimer > 0) { pathRecalcTimer--; return; }

    // ワールド座標 -> タイル座標へ変換
    int  tileSize = map.GetTileSize();
    auto WorldToTile = [tileSize](int wx, int wy) { return std::make_pair(wx / tileSize, wy / tileSize); };
    auto start = WorldToTile(x, y);
    auto goal  = WorldToTile(playerX, playerY);

    // マップ端でクランプして安全にする（マクロ min/max を避けて明示的にクランプ）
    int sx = start.first;
    int sy = start.second;
    int gx = goal.first;
    int gy = goal.second;
    int maxW = map.GetWidth() - 1;
    int maxH = map.GetHeight() - 1;
    if (sx < 0) sx = 0; if (sx > maxW) sx = maxW;
    if (sy < 0) sy = 0; if (sy > maxH) sy = maxH;
    if (gx < 0) gx = 0; if (gx > maxW) gx = maxW;
    if (gy < 0) gy = 0; if (gy > maxH) gy = maxH;

    const int W = map.GetWidth();
    const int H = map.GetHeight();

    // ヒューリスティック：モードによりマンハッタンまたはチェビシェフ（8方向）
    const auto heuristic = [this](int ax, int ay, int bx, int by) -> int {
        int dx = std::abs(ax - bx);
        int dy = std::abs(ay - by);
        if (pathMode == PATH_8DIR) return (dx > dy) ? dx : dy; // Chebyshev distance
        return dx + dy; // Manhattan distance
    };
    const int INF = 1000000000; // use integer literal to avoid double->int conversion

    // g: 開始からそのセルまでのコスト
    // parent: 経路復元用の親ノード
    // closed: 処理済みフラグ
    std::vector<std::vector<int>> g(H, std::vector<int>(W, INF));
    std::vector<std::vector<std::pair<int,int>>> parent(H, std::vector<std::pair<int,int>>(W, {-1, -1}));
    std::vector<std::vector<char>> closed(H, std::vector<char>(W, 0));

    // 優先度付きキューのノード: (f = g + h, x, y)
    using Node = std::tuple<int, int, int>;
    struct Cmp { bool operator()(Node const &a, Node const &b) const { return std::get<0>(a) > std::get<0>(b); } };

    std::priority_queue<Node, std::vector<Node>, Cmp> open;

    // A* 初期化
    g[sy][sx] = 0;
    open.emplace(heuristic(sx, sy, gx, gy), sx, sy);

    // 移動方向は敵ごとのモードで切り替え（4方向または8方向）
    const int dirs4[4][2] = {{1,0}, {-1,0}, {0,-1}, {0,1}};
    const int dirs8[8][2] = {{1,0}, {-1,0}, {0,-1}, {0,1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};
    const int (*dirs)[2] = nullptr;
    int dirCount = 0;
    if (pathMode == PATH_8DIR) { dirs = dirs8; dirCount = 8; }
    else { dirs = dirs4; dirCount = 4; }

    while (!open.empty()) {
        Node node = open.top(); open.pop();
        int cx = std::get<1>(node);
        int cy = std::get<2>(node);
        if (closed[cy][cx]) continue; // 既に処理済みならスキップ
        closed[cy][cx] = 1;

        if (cx == gx && cy == gy) break; // ゴール到達

        for (int i = 0; i < dirCount; ++i) {
            int nx = cx + dirs[i][0]; int ny = cy + dirs[i][1];
            // 範囲チェック
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (closed[ny][nx]) continue;

            // 目的のタイルの衝突判定を確認
            CollisionType ct = map.GetCollisionTypeAt(nx, ny);
            if (ct == CollisionType::Wall) continue; // 壁は通れない

            // 床があるか、あるいは自分がゴール（落下許容）であることを確認する
            CollisionType below = map.GetCollisionTypeAt(nx, ny + 1);
            if (!(nx == gx && ny == gy) && !(below == CollisionType::Floor || ct == CollisionType::Floor)) continue;

            int moveCost = 1;
            // diagonal move cost stays 1 but heuristic uses Chebyshev for admissibility
            int tentative_g = g[cy][cx] + moveCost;
            if (tentative_g < g[ny][nx]) {
                g[ny][nx] = tentative_g;
                parent[ny][nx] = { cx, cy };
                int nf = tentative_g + heuristic(nx, ny, gx, gy);
                open.emplace(nf, nx, ny);
            }
        }
    }

    // 経路復元
    path.clear();
    if (g[gy][gx] == INF) { pathIndex = 0; pathRecalcTimer = PATH_RECALC_INTERVAL; return; }

    std::pair<int,int> cur = { gx, gy };
    while (!(cur.first == sx && cur.second == sy)) {
        path.push_back(cur);
        cur = parent[cur.second][cur.first];
    }
    std::reverse(path.begin(), path.end());
    pathIndex = 0;
    pathRecalcTimer = PATH_RECALC_INTERVAL;
}
