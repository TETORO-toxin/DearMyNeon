#include "Code/debug_utils.h"
#include "game_manager.h"
#include <algorithm>
#include <fstream>
#include <windows.h>

// Helper: quick UTF-8 validation
static bool IsValidUtf8(const std::string &s)
{
    const unsigned char *bytes = reinterpret_cast<const unsigned char*>(s.c_str());
    size_t len = s.size();
    size_t i = 0;
    while (i < len) {
        unsigned char c = bytes[i];
        if (c <= 0x7F) { ++i; continue; }
        size_t need = 0;
        if ((c & 0xE0) == 0xC0) need = 1;
        else if ((c & 0xF0) == 0xE0) need = 2;
        else if ((c & 0xF8) == 0xF0) need = 3;
        else return false;
        if (i + need >= len) return false;
        for (size_t j = 1; j <= need; ++j) {
            if ((bytes[i + j] & 0xC0) != 0x80) return false;
        }
        i += (need + 1);
    }
    return true;
}

// Helper: convert UTF-8 std::string to system ANSI (CP_ACP) std::string
static std::string Utf8ToAcp(const std::string &utf8)
{
    if (utf8.empty()) return std::string();
    // convert UTF-8 -> wide
    int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return utf8;
    std::wstring wbuf(wlen, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wbuf[0], wlen);
    // convert wide -> ACP
    int len = ::WideCharToMultiByte(CP_ACP, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        // fallback: return original
        return utf8;
    }
    std::string out(len, '\0');
    ::WideCharToMultiByte(CP_ACP, 0, wbuf.c_str(), -1, &out[0], len, nullptr, nullptr);
    // strip trailing null
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// 移動・衝突・更新・描画関連のロジックを分離

void GameManager::TryMove(Player& player, float dx, float dy)
{
    float nextX = static_cast<float>(player.GetX()) + dx;
    float nextY = static_cast<float>(player.GetY()) + dy;
    if (std::isnan(nextX) || std::isnan(nextY) || nextY > 10000.0f || nextY < -10000.0f) { player.SetPosition(100.0f,100.0f); player.SetVelocityY(0); player.SetOnGround(false); return; }

    float prevX = static_cast<float>(player.GetX()); float prevY = static_cast<float>(player.GetY());
    int tileSize = map ? map->GetTileSize() : 32;

    // 水平方向の判定
    float desiredX = prevX;
    if (dx != 0.0f && map) {
        float testX = nextX; RECT r = player.GetRect(); int ofsX = static_cast<int>(std::round(testX - prevX)); RECT rH = { r.left+ofsX, r.top, r.right+ofsX, r.bottom };
        int tx0 = rH.left / tileSize; int tx1 = rH.right / tileSize; int ty0 = rH.top / tileSize; int ty1 = (rH.bottom - 1) / tileSize;
        bool blocked = false;
        for (int ty=ty0; ty<=ty1 && !blocked; ++ty) for (int tx=tx0; tx<=tx1; ++tx) { CollisionType ct = map->GetCollisionTypeAt(tx,ty); if (ct==CollisionType::Wall) { blocked=true; break; } if (ct==CollisionType::Floor) { float tileTop = static_cast<float>(ty)*static_cast<float>(tileSize); if (static_cast<float>(rH.bottom) > tileTop + 4.0f) { blocked=true; break; } } }
        if (!blocked) { desiredX = testX; player.SetPosition(desiredX, prevY); }
        else player.SetPosition(prevX, prevY);
    }

    // 垂直方向の判定
    float desiredY = prevY; float testY = nextY;
    if (map) {
        RECT r = player.GetRect(); int ofsY = static_cast<int>(std::round(testY - prevY)); RECT rV = { r.left, r.top+ofsY, r.right, r.bottom+ofsY };
        int tx0 = rV.left / tileSize; int tx1 = rV.right / tileSize; int ty0 = rV.top / tileSize; int ty1 = (rV.bottom - 1) / tileSize;
        bool snapped=false; bool blockedVert=false;
        for (int ty=ty0; ty<=ty1 && !blockedVert; ++ty) {
            for (int tx=tx0; tx<=tx1; ++tx) {
                CollisionType ct = map->GetCollisionTypeAt(tx,ty);
                if (ct==CollisionType::Wall) { blockedVert=true; break; }
                if (ct==CollisionType::Floor) {
                    float tileTop = static_cast<float>(ty)*static_cast<float>(tileSize);
                    if (testY > prevY) {
                        if (prevY < tileTop + 4.0f && (static_cast<float>(rV.bottom) >= tileTop - 0.5f)) { desiredY = tileTop; player.SetPosition(static_cast<float>(player.GetX()), desiredY); player.SetVelocityY(0); player.SetOnGround(true); player.ResetJumpCount(); snapped=true; blockedVert=true; break; }
                        else if (static_cast<float>(rV.bottom) > tileTop + 1.0f) { blockedVert=true; break; }
                    } else if (testY < prevY) {
                        float tileBottom = tileTop + static_cast<float>(tileSize); if (static_cast<float>(rV.top) < tileBottom) { desiredY = prevY; player.SetPosition(static_cast<float>(player.GetX()), desiredY); player.SetVelocityY(0); blockedVert=true; break; }
                    }
                }
            }
        }
        if (!snapped && !blockedVert) { desiredY = testY; player.SetPosition(static_cast<float>(player.GetX()), desiredY); int nfx = static_cast<int>(player.GetX())/tileSize; int nfy = static_cast<int>(player.GetY())/tileSize; CollisionType nu = map->GetCollisionTypeAt(nfx,nfy); if (nu != CollisionType::Floor) player.SetOnGround(false); }
    } else { desiredY = testY; player.SetPosition(static_cast<float>(player.GetX()), desiredY); }

    if (map) { int mw = map->GetWidth()*map->GetTileSize(); int mh = map->GetHeight()*map->GetTileSize(); float px = ClampFloat(static_cast<float>(player.GetX()), 0.0f, static_cast<float>(mw)); float py = ClampFloat(static_cast<float>(player.GetY()), 0.0f, static_cast<float>(mh)); player.SetPosition(px, py); }

    if (map) {
        RECT rr = player.GetRect();
        int tx0 = rr.left / tileSize; int tx1 = (rr.right - 1) / tileSize; int ty0 = rr.top / tileSize; int ty1 = (rr.bottom - 1) / tileSize;
        for (int ty = ty0; ty <= ty1; ++ty) {
            for (int tx = tx0; tx <= tx1; ++tx) {
                CollisionType ct = map->GetCollisionTypeAt(tx, ty);
                if (ct == CollisionType::Floor) {
                    float tileTop = static_cast<float>(ty) * static_cast<float>(tileSize);
                    float playerBottom = static_cast<float>(rr.bottom);
                    if (playerBottom > tileTop && playerBottom < tileTop + static_cast<float>(tileSize)) {
                        player.SetPosition(static_cast<float>(player.GetX()), tileTop);
                        player.SetVelocityY(0);
                        player.SetOnGround(true);
                        player.ResetJumpCount();
                        rr = player.GetRect();
                        tx0 = rr.left / tileSize; tx1 = (rr.right - 1) / tileSize; ty0 = rr.top / tileSize; ty1 = rr.bottom / tileSize;
                        break;
                    }
                }
            }
        }
    }

}

void GameManager::UpdateGameplay()
{
    // Use integer overload to follow target position ? passing floats picked the "move" overload and caused runaway.
    camera.Update(player.GetX(), player.GetY());

    float timeScale = 1.0f;
    if (slowMotionActive && slowMotionTimer > 0) {
        timeScale = slowMotionScale;
        --slowMotionTimer;
        if (slowMotionTimer <= 0) {
            slowMotionActive = false;
            slowMotionScale = 1.0f;
        }
    }

    player.Update();
    float dx = player.GetMoveX() * timeScale;
    float dy = player.GetMoveY() * timeScale;
    TryMove(player, dx, dy);

    {
        if (player.GetY() > static_cast<float>(player.savedY) + static_cast<float>(PLAYER_FALL_RESPAWN_DISTANCE)) {
            player.SetPosition(static_cast<float>(player.savedX), static_cast<float>(player.savedY));
            player.SetVelocityY(0);
            player.SetOnGround(true);
        }

        float fallThreshold = static_cast<float>(map->GetHeight() * map->GetTileSize()) + 30.0f;
        if (player.GetY() > fallThreshold) {
            player.TakeDamage(1);
            player.SetPosition(static_cast<float>(player.savedX), static_cast<float>(player.savedY));
            player.SetVelocityY(0);
            player.SetOnGround(true);
        }
    }

    for (const auto& line : respawnLines) {
        int px = static_cast<int>(player.GetX()); int py = static_cast<int>(player.GetY());
        int minx = std::min(line.x1, line.x2); int maxx = std::max(line.x1, line.x2);
        int miny = std::min(line.y1, line.y2); int maxy = std::max(line.y1, line.y2);
        if (px >= minx-4 && px <= maxx+4 && py >= miny-4 && py <= maxy+4) {
            player.SetPosition(static_cast<float>(player.savedX), static_cast<float>(player.savedY));
            player.SetVelocityY(0);
            player.SetOnGround(true);
        }
    }

    // Check rooms for tutorial triggers
    if (map) {
        auto& rooms = map->GetRooms();
        int px = static_cast<int>(player.GetX());
        int py = static_cast<int>(player.GetY());
        int foundRoomIndex = -1;
        for (int ri = 0; ri < (int)rooms.size(); ++ri) {
            auto& r = rooms[ri];
            // Per-room tutorialText previously triggered TutorialScene overlays.
            // That mechanism has been removed in favor of GameManager showing room overlayText/tag text directly,
            // so we no longer call tutorialScene->StartOverlayText nor use r.tutorialShown here.
            // detect which room player is currently in (for overlay tag/text)
            if (px >= r.xStart && px <= r.xEnd && py >= r.yStart && py <= r.yEnd) {
                foundRoomIndex = ri;
            }
        }

        // Update active room overlay state based on foundRoomIndex
        if (foundRoomIndex != -1) {
            Room &r = rooms[foundRoomIndex];
            std::string textToShow;
            if (!r.overlayText.empty()) textToShow = r.overlayText;
            else if (r.tag != -1) {
                std::string dyn = GetRoomTagText(r.tag);
                textToShow = !dyn.empty() ? dyn : GetTextForTagSwitch(r.tag);
            }

            DEBUG_ONLY( DBG_PRINTF("Room detection: player in room id=%d tag=%d overlayText='%s' dynText='%s'\n", r.id, r.tag, r.overlayText.c_str(), GetRoomTagText(r.tag).c_str()); );
            // Always append to log file so we can inspect behavior regardless of build config
            {
                std::ofstream _ofs("overlay_debug.txt", std::ofstream::out | std::ofstream::app);
                if (_ofs.is_open()) {
                    _ofs << "Room detection: player in room id=" << r.id << " tag=" << r.tag << " overlayText='" << r.overlayText << "' dynText='" << GetRoomTagText(r.tag) << "'\n";
                }
            }

            if (!textToShow.empty()) {
                // if changed room or text, update active overlay content
                if (activeRoomOverlayRoomId != r.id || activeRoomOverlayText != textToShow) {
                    activeRoomOverlayRoomId = r.id;
                    activeRoomOverlayText = textToShow;
                    // pick color: prefer room overlayColor, then tag color
                    if (!r.overlayText.empty()) {
                        activeRoomOverlayColor = r.overlayColor;
                        activeRoomOverlayFontSize = r.overlayFontSize;
                    } else {
                        int tc = GetRoomTagColor(r.tag);
                        activeRoomOverlayColor = (tc != 0) ? tc : r.overlayColor;
                        activeRoomOverlayFontSize = r.overlayFontSize;
                    }
                    DEBUG_ONLY( DBG_PRINTF("Active overlay set: room=%d text='%s' color=%06X font=%d\n", activeRoomOverlayRoomId, activeRoomOverlayText.c_str(), activeRoomOverlayColor, activeRoomOverlayFontSize); );
                    {
                        std::ofstream _ofs("overlay_debug.txt", std::ofstream::out | std::ofstream::app);
                        if (_ofs.is_open()) {
                            _ofs << "Active overlay set: room=" << activeRoomOverlayRoomId << " text='" << activeRoomOverlayText << "' color=" << std::hex << activeRoomOverlayColor << std::dec << " font=" << activeRoomOverlayFontSize << "\n";
                        }
                    }
                }
                activeRoomOverlayTargetAlpha = 255;
                // compute a screen position near top-center of the room
                int worldCX = (r.xStart + r.xEnd) / 2;
                int worldYTop = std::min(r.yStart, r.yEnd);
                activeRoomOverlayScreenX = worldCX - camera.GetOffsetX();
                activeRoomOverlayScreenY = worldYTop - camera.GetOffsetY() - 32; // 32px above room
                DEBUG_ONLY( DBG_PRINTF("Active overlay pos (world cx=%d top=%d) screen=(%d,%d)\n", worldCX, worldYTop, activeRoomOverlayScreenX, activeRoomOverlayScreenY); );
                {
                    std::ofstream _ofs("overlay_debug.txt", std::ofstream::out | std::ofstream::app);
                    if (_ofs.is_open()) {
                        _ofs << "Active overlay pos (world cx=" << worldCX << " top=" << worldYTop << ") screen=(" << activeRoomOverlayScreenX << "," << activeRoomOverlayScreenY << ")\n";
                    }
                }
            } else {
                // room has no text/tag -> fade out
                if (activeRoomOverlayRoomId == r.id) activeRoomOverlayTargetAlpha = 0;
            }
        } else {
            // not in any room -> fade out
            activeRoomOverlayTargetAlpha = 0;
        }
    }

    // Advance overlay alpha toward target
    if (activeRoomOverlayAlpha < activeRoomOverlayTargetAlpha) {
        activeRoomOverlayAlpha = std::min(255, activeRoomOverlayAlpha + activeRoomOverlayFadeSpeed);
    } else if (activeRoomOverlayAlpha > activeRoomOverlayTargetAlpha) {
        activeRoomOverlayAlpha = std::max(0, activeRoomOverlayAlpha - activeRoomOverlayFadeSpeed);
        if (activeRoomOverlayAlpha == 0) {
            // clear when fully faded out
            if (activeRoomOverlayTargetAlpha == 0) {
                activeRoomOverlayText.clear();
                activeRoomOverlayRoomId = -1;
            }
        }
    }

    // 編集モードの場合はエディタ側処理を優先して実行
    editModeManager.Update(player, map.get(), enemies, savePoints);
    if (!editModeManager.IsEditMode()) for ( auto& e: enemies)e.Update(player.GetX(), player.GetY());
    for ( auto& e: enemies) e.ProcessMeleeAttack(player);
    for ( auto& b: enemyBullets) { if (!b.isActive) continue; b.Update(); if (b.isActive && CheckCollision(b.GetRect(), player.GetRect())) { player.TakeDamage(b.power); b.isActive=false; } }
    for ( auto& it: items) { if (it.isActive && CheckCollision(player.GetRect(), it.GetRect())) { if (it.type==ITEM_RECOVERY) player.Heal(1); else if (it.type==ITEM_BULLET) player.AddBullet(5); it.isActive=false; } }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e){ return !e.IsActive(); }), enemies.end());
    enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(), [](const Bullet& b){ return !b.isActive; }), enemyBullets.end());
    items.erase(std::remove_if(items.begin(), items.end(), [](const Item& i){ return !i.isActive; }), items.end());
    if (CheckHitKey(KEY_INPUT_Y) == 1) SaveGame();

    // Update tutorial overlay if active so it can consume input while gameplay runs
    if (tutorialScene && tutorialScene->IsOverlayActive()) tutorialScene->UpdateOverlay();
}

void GameManager::DrawGameplay()
{
    int shakeX = 0, shakeY = 0;
    bool appliedShake = false;
    if (screenShakeTimer > 0 && screenShakeMagnitude > 0) {
        shakeX = GetRand(screenShakeMagnitude * 2) - screenShakeMagnitude;
        shakeY = GetRand(screenShakeMagnitude * 2) - screenShakeMagnitude;
        camera.Update(static_cast<float>(shakeX), static_cast<float>(shakeY));
        appliedShake = true;
        --screenShakeTimer;
        if (screenShakeTimer < 0) screenShakeTimer = 0;
    }

    int camX = camera.GetOffsetX();
    int camY = camera.GetOffsetY();
    for (int i = 0; i < PARALLAX_LAYER_COUNT; ++i) {
        int h = backgroundLayerHandles[i];
        if (h == -1) continue;
        float factor = backgroundParallaxFactors[i];
        float scale = backgroundScaleFactors[i];
        int baseY = backgroundBaseYOffset[i];
        int drawX = static_cast<int>(-camX * factor);
        int drawY = static_cast<int>(-camY * factor);
        int gw = 0, gh = 0; GetGraphSize(h, &gw, &gh);
        if (gw <= 0 || gh <= 0) continue;
        int sw = static_cast<int>(gw * scale);
        int sh = static_cast<int>(gh * scale);
        if (sw <= 0 || sh <= 0) continue;

        int finalY = baseY + drawY;

        int startX = (drawX % sw) - sw; if (startX > 0) startX -= sw;
        for (int tx = startX; tx < SCREEN_WIDTH; tx += sw) {
            DrawExtendGraph(tx, finalY, tx + sw, finalY + sh, h, TRUE);
        }
    }

    if (map) map->draw();
    editModeManager.Draw();
    for (const auto& e: enemies) e.Draw();
    player.Draw();
    for (const auto& b: playerBullets) b.Draw();
    for (const auto& it: items) it.Draw();
    if (editModeManager.IsEditMode()) {
        for (const auto& rl : respawnLines) {
            DrawLine(rl.x1 - camera.GetOffsetX(), rl.y1 - camera.GetOffsetY(), rl.x2 - camera.GetOffsetX(), rl.y2 - camera.GetOffsetY(), GetColor(255, 0, 255));
        }
    }

    // フラッシュオーバーレイ描画（画面端に色を出す演出）
    if (flashActive && flashTimer > 0) {
        float t = 1.0f - static_cast<float>(flashTimer) / static_cast<float>(flashDuration);
        float intensity = flashIntensity * (1.0f - t);
        if (intensity < 0.0f) intensity = 0.0f;
        int alpha = static_cast<int>(intensity * 200.0f);
        if (alpha > 255) alpha = 255;
        if (alpha > 0) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
            int border = static_cast<int>(100.0f * (0.9f - t) * flashSpread);
            if (border < 20) border = 20; if (border > 300) border = 300;
            DrawBox(0, 0, SCREEN_WIDTH, border, GetColor(flashR,flashG,flashB), TRUE);
            DrawBox(0, SCREEN_HEIGHT-border, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(flashR,flashG,flashB), TRUE);
            DrawBox(0, 0, border, SCREEN_HEIGHT, GetColor(flashR,flashG,flashB), TRUE);
            DrawBox(SCREEN_WIDTH-border, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(flashR,flashG,flashB), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
        flashTimer--;
        if (flashTimer <= 0) { flashActive = false; flashTimer = 0; }
    }

    if (appliedShake) {
        camera.Update(static_cast<float>(-shakeX), static_cast<float>(-shakeY));
    }

    // Draw tutorial overlay on top of gameplay if active
    if (tutorialScene && tutorialScene->IsOverlayActive()) tutorialScene->DrawOverlay();

    // Draw active room overlay (tag/text) on top of gameplay
    DrawActiveRoomOverlay();
}

void GameManager::DrawActiveRoomOverlay()
{
    if (activeRoomOverlayText.empty() || activeRoomOverlayAlpha <= 0) return;
    int camX = camera.GetOffsetX();
    int camY = camera.GetOffsetY();
    if (!map) return;
    auto &rooms = map->GetRooms();
    const Room* proom = nullptr;
    for (const auto &rr : rooms) if (rr.id == activeRoomOverlayRoomId) { proom = &rr; break; }
    int drawXpos = activeRoomOverlayScreenX;
    int drawYpos = activeRoomOverlayScreenY;
    if (proom) {
        int worldCX = (proom->xStart + proom->xEnd) / 2;
        int worldYTop = std::min(proom->yStart, proom->yEnd);
        drawXpos = worldCX - camX;
        drawYpos = worldYTop - camY - 32;
    }

    int col = activeRoomOverlayColor;
    int rcol = (col >> 16) & 0xFF;
    int gcol = (col >> 8) & 0xFF;
    int bcol = col & 0xFF;

    // Ensure font handle for Japanese text exists (lazy init)
    if (roomOverlayFontHandle == -1) {
        // Prefer Meiryo if available, fallback to MS Gothic
        roomOverlayFontHandle = CreateFontToHandle("Meiryo", activeRoomOverlayFontSize, 0);
        if (roomOverlayFontHandle == -1) roomOverlayFontHandle = CreateFontToHandle("ＭＳ ゴシック", activeRoomOverlayFontSize, 0);
        if (roomOverlayFontHandle == -1) roomOverlayFontHandle = CreateFontToHandle("MS Gothic", activeRoomOverlayFontSize, 0);
        if (roomOverlayFontHandle == -1) roomOverlayFontHandle = DX_DEFAULT_FONT_HANDLE; // fallback
    }

    // Convert UTF-8 text to wide
    std::wstring wtext;
    {
        const std::string &s = activeRoomOverlayText;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (wlen > 0) {
            wtext.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wtext[0], wlen);
            if (!wtext.empty() && wtext.back() == L'\0') wtext.pop_back();
        }
    }

    // Measure width: use DrawFormatStringToHandle to estimate or fall back to simple width
    int textW = 0;
    if (roomOverlayFontHandle != DX_DEFAULT_FONT_HANDLE) {
        textW = GetDrawFormatStringWidthToHandle(roomOverlayFontHandle, "%s", Utf8ToAcp(activeRoomOverlayText).c_str());
    } else {
        textW = GetDrawFormatStringWidthToHandle(DX_DEFAULT_FONT_HANDLE, "%s", Utf8ToAcp(activeRoomOverlayText).c_str());
    }
    int textH = std::max(12, activeRoomOverlayFontSize);
    int bx = drawXpos - textW/2 - 8;
    int by = drawYpos - (textH/2) - 6;
    if (bx < 6) bx = 6;
    if (by < 6) by = 6;
    if (bx + textW + 16 > SCREEN_WIDTH - 6) bx = SCREEN_WIDTH - textW - 22;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, activeRoomOverlayAlpha);
    DrawBox(bx, by, bx + textW + 16, by + textH + 12, GetColor(10,10,10), TRUE);

    // Draw text with selected font handle. If we have a wide string, use DrawStringToHandleW if available.
#ifdef UNICODE
    if (!wtext.empty()) DrawStringToHandleW(bx + 8, by + 6, wtext.c_str(), GetColor(rcol, gcol, bcol), roomOverlayFontHandle);
    else {
        std::string drawTextA = Utf8ToAcp(activeRoomOverlayText);
        DrawFormatStringToHandle(bx + 8, by + 6, GetColor(rcol, gcol, bcol), roomOverlayFontHandle, "%s", drawTextA.c_str());
    }
#else
    std::string drawTextA = Utf8ToAcp(activeRoomOverlayText);
    DrawFormatStringToHandle(bx + 8, by + 6, GetColor(rcol, gcol, bcol), roomOverlayFontHandle, "%s", drawTextA.c_str());
#endif

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

bool GameManager::CheckCollision(const RECT& r1, const RECT& r2) const { return (r1.left < r2.right && r1.right > r2.left && r1.top < r2.bottom && r1.bottom > r2.top); }


