#include "SpriteAtlas.h"
#include <DxLib.h>
#include <fstream>
#include <string>
#include <cctype>

static int ParseIntAfterKey(const std::string &s, const std::string &key) {
    size_t p = s.find(key);
    if (p == std::string::npos) return 0;
    p = s.find(':', p);
    if (p == std::string::npos) return 0;
    ++p;
    // skip spaces
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
    // read optional sign
    size_t start = p;
    if (p < s.size() && (s[p] == '-' || s[p] == '+')) ++p;
    while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p]))) ++p;
    if (start == p) return 0;
    try { return std::stoi(s.substr(start, p - start)); } catch(...) { return 0; }
}

static size_t FindMatchingBrace(const std::string &s, size_t openPos) {
    if (openPos >= s.size() || s[openPos] != '{') return std::string::npos;
    int depth = 0;
    for (size_t i = openPos; i < s.size(); ++i) {
        if (s[i] == '{') ++depth;
        else if (s[i] == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

SpriteAtlas::~SpriteAtlas() {
    if (imageHandle_ != -1) {
        DeleteGraph(imageHandle_);
        imageHandle_ = -1;
    }
}

bool SpriteAtlas::LoadFromJson(const std::string& jsonPath, const std::string& imagePath) {
    // ‰æ‘œ“Ç‚Ýž‚Ý
    imageHandle_ = LoadGraph(imagePath.c_str());
    if (imageHandle_ == -1) return false;

    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    // find "frames" object
    size_t framesPos = content.find("\"frames\"");
    if (framesPos == std::string::npos) return true;
    size_t bracePos = content.find('{', framesPos);
    if (bracePos == std::string::npos) return true;
    size_t framesEnd = FindMatchingBrace(content, bracePos);
    if (framesEnd == std::string::npos) return true;

    size_t pos = bracePos + 1;
    while (pos < framesEnd) {
        // find next key string
        size_t keyStart = content.find('"', pos);
        if (keyStart == std::string::npos || keyStart >= framesEnd) break;
        size_t keyEnd = content.find('"', keyStart + 1);
        if (keyEnd == std::string::npos || keyEnd >= framesEnd) break;
        std::string name = content.substr(keyStart + 1, keyEnd - (keyStart + 1));
        // find the value object start for this key
        size_t valueStart = content.find('{', keyEnd);
        if (valueStart == std::string::npos || valueStart >= framesEnd) break;
        size_t valueEnd = FindMatchingBrace(content, valueStart);
        if (valueEnd == std::string::npos || valueEnd > framesEnd) break;
        std::string valueBlock = content.substr(valueStart, valueEnd - valueStart + 1);
        // within this block, find "frame" object
        size_t frameKey = valueBlock.find("\"frame\"");
        if (frameKey != std::string::npos) {
            size_t frameBrace = valueBlock.find('{', frameKey);
            if (frameBrace != std::string::npos) {
                size_t frameClose = FindMatchingBrace(valueBlock, frameBrace);
                if (frameClose != std::string::npos) {
                    std::string frameBlock = valueBlock.substr(frameBrace, frameClose - frameBrace + 1);
                    SpriteFrame f;
                    f.x = ParseIntAfterKey(frameBlock, "\"x\"");
                    f.y = ParseIntAfterKey(frameBlock, "\"y\"");
                    f.w = ParseIntAfterKey(frameBlock, "\"w\"");
                    f.h = ParseIntAfterKey(frameBlock, "\"h\"");
                    f.duration = ParseIntAfterKey(valueBlock, "\"duration\"");
                    frames_.emplace(name, f);
                }
            }
        }
        pos = valueEnd + 1;
    }

    return true;
}

bool SpriteAtlas::HasFrame(const std::string& name) const {
    return frames_.find(name) != frames_.end();
}

SpriteFrame SpriteAtlas::GetFrame(const std::string& name) const {
    auto it = frames_.find(name);
    if (it != frames_.end()) return it->second;
    return SpriteFrame{};
}

void SpriteAtlas::DrawFrame(const std::string& name, int dx, int dy, float scale, bool flip) const {
    if (imageHandle_ == -1) return;
    auto it = frames_.find(name);
    if (it == frames_.end()) return;
    const SpriteFrame &f = it->second;
    if (f.w <= 0 || f.h <= 0) return;

    int destW = static_cast<int>(f.w * scale);
    int destH = static_cast<int>(f.h * scale);
    int left = dx;
    int top = dy;
    int right = dx + destW;
    int bottom = dy + destH;
    if (flip) std::swap(left, right);

    DrawRectExtendGraph(left, top, right, bottom, f.x, f.y, f.w, f.h, imageHandle_, TRUE);
}
