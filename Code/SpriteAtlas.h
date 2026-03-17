#pragma once
#include <string>
#include <unordered_map>

struct SpriteFrame {
    int x = 0, y = 0, w = 0, h = 0;
    int duration = 100; // ms
};

class SpriteAtlas {
public:
    SpriteAtlas() = default;
    ~SpriteAtlas();

    // jsonPath: SpriteSheet.json, imagePath: SpriteSheet.png
    bool LoadFromJson(const std::string& jsonPath, const std::string& imagePath);

    bool HasFrame(const std::string& name) const;
    SpriteFrame GetFrame(const std::string& name) const;

    // ï`âÊÅFç∂è„äÓèÄÅBscale Ç≈ägëÂÅAflip=true Ç≈ç∂âEîΩì]
    void DrawFrame(const std::string& name, int dx, int dy, float scale = 1.0f, bool flip = false) const;

private:
    int imageHandle_ = -1;
    std::unordered_map<std::string, SpriteFrame> frames_;
};

// égópó· (íZÇ≠):
// SpriteAtlas atlas;
// if (atlas.LoadFromJson("SpriteSheet.json", "SpriteSheet.png")) {
//     atlas.DrawFrame("SpriteSheet (Sprites).aseprite", 100, 50, 1.0f, false);
// }
