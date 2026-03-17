#include "enemy.h"
#include "game_manager.h"
#include "Map.h"

// enemy_ai.cpp
// もともと視界・追跡・経路探索が含まれていたファイルですが、
// より細分化するために主要な実装を別ファイルに移動しました。
// このファイルは今後、AI の高レベル制御を残すか、必要な統合ロジックを追加するためのプレースホルダとして使います。

// 元の実装は以下のファイルに分割されています：
// - enemy_pathfinding.cpp : 経路探索（A*）
// - enemy_vision.cpp      : 視界と追跡ロジック
// - enemy_chase.cpp       : 追跡関連の補助関数

// 必要に応じてここへ統合ロジックを追加してください。
