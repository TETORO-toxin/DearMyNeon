#include "EditModeManager.h"
#include "DxLib.h"
#include "define.h"
#include "Code/debug_utils.h"
#include "game_manager.h"

// EditModeManager::Draw 

static std::string toLower(const std::string &s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
    return out;
}

static bool matchesFilterString(const std::string &filter, const char* label) {
    if (filter.empty()) return true;
    std::string l = toLower(std::string(label));
    std::string f = toLower(filter);
    return l.find(f) != std::string::npos;
}

static std::string Ellipsize(const std::string &s, int availablePixels, float fs) {
    if (s.empty() || availablePixels <= 0) return std::string();
    const int approxCharW = std::max(4, static_cast<int>(8 * fs));
    int maxChars = availablePixels / approxCharW;
    if ((int)s.size() <= maxChars) return s;
    int take = std::max(0, maxChars - 3);
    if (take <= 0) return std::string("...");
    return s.substr(0, take) + std::string("...");
}

static int MeasureTextWidthPixels(const std::string &s, float fs) {
    if (s.empty()) return 0;
    int w = 0;
    // Use DxLib's precise string width measurement. DX_DEFAULT_FONT_HANDLE selects the default font.
    // Signature: int GetDrawFormatStringWidthToHandle(int FontHandle, const TCHAR *FormatString, ...)
    w = GetDrawFormatStringWidthToHandle(DX_DEFAULT_FONT_HANDLE, "%s", s.c_str());
    if (w <= 0) {
        const int approxCharW = std::max(4, static_cast<int>(8 * fs));
        w = (int)s.size() * approxCharW;
    }
    return w;
}

static bool RectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0) return false;
    return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
}

void EditModeManager::Draw() {
    if (!isEditMode) return;

    // UI constants (scaled by font scale)
    float fs = inspectorFontScale;
    const int pad = static_cast<int>(8 * fs);
    const int headerH = static_cast<int>(40 * fs); // increased header height to avoid overlap with search box
    const int searchH = static_cast<int>(26 * fs);
    const int rowH = static_cast<int>(32 * fs); // increased row height for spacing
    const int labelW = static_cast<int>(88 * fs);
    const int valueW = static_cast<int>(120 * fs); // value width
    const int smallBtnW = static_cast<int>(28 * fs); // slightly larger buttons
    const int smallBtnH = static_cast<int>(22 * fs);

    const int colBg = GetColor(18,18,18);
    const int colPanelBorder = GetColor(140,140,140);
    const int colHeader = GetColor(40,40,40);
    const int colText = GetColor(230,230,230);
    const int colSubText = GetColor(170,170,170);
    const int colRowBg = GetColor(40,40,40);
    const int colHighlight = GetColor(255,200,64);
    const int colButton = GetColor(90,90,90);
    const int colButtonHover = GetColor(120,120,120);
    const int colClearBtn = GetColor(150,60,60);
    const int colSeparator = GetColor(60,60,60);

    GameManager& gm = GameManager::GetInstance();
    Player& player = gm.player;
    auto& enemies = gm.enemies;
    Map* map = &gm.GetMap();

    // mouse position for hover
    int mouseX = 0, mouseY = 0;
    GetMousePoint(&mouseX, &mouseY);

    // if docked, align to right
    if (inspectorDocked) {
        inspectorPosX = SCREEN_WIDTH - inspectorWidth - 8;
        inspectorPosY = 20;
    }

    int x = inspectorPosX;
    int y = inspectorPosY;
    int w = inspectorWidth;
    int h = inspectorHeight;

    // clamp inspector to screen so content calculations are sane
    if (x < 0) x = inspectorPosX = 0;
    if (y < 0) y = inspectorPosY = 0;
    if (x + w > SCREEN_WIDTH) { inspectorPosX = x = SCREEN_WIDTH - w; }
    if (y + h > SCREEN_HEIGHT) { inspectorPosY = y = SCREEN_HEIGHT - h; }

    // If a room is selected in inspector, ensure inspector is wide enough to display the
    // Selected Room Preview text without clipping/ellipsize. Increase inspectorWidth until
    // the preview area can fit the overlay text (approximate), up to screen width.
    if (inspectorTarget == InspectorTarget::Room && selectedRoomIndex >= 0 && map) {
        auto &rooms = map->GetRooms();
        if (selectedRoomIndex < (int)rooms.size()) {
            const Room &sr = rooms[selectedRoomIndex];
            if (!sr.overlayText.empty()) {
                // approximate required preview width in pixels
                int requiredPreviewW = MeasureTextWidthPixels(sr.overlayText, fs) + static_cast<int>(12 * fs);
                // existing content area available inside inspector
                int contentAvail = inspectorWidth - pad * 2;
                int desiredContent = std::max(contentAvail, requiredPreviewW);
                int maxInspectorW = std::max(0, SCREEN_WIDTH - 16);
                int desiredInspectorW = std::min(maxInspectorW, desiredContent + pad * 2);
                if (desiredInspectorW > inspectorWidth) {
                    inspectorWidth = desiredInspectorW;
                    // update local w and x/clamp
                    w = inspectorWidth;
                    if (inspectorDocked) inspectorPosX = SCREEN_WIDTH - inspectorWidth - 8;
                    if (inspectorPosX < 0) inspectorPosX = 0;
                    x = inspectorPosX;
                }
            }
        }
    }

    // panel background
    DrawBox(x, y, x + w, y + h, colBg, TRUE);
    DrawBox(x, y, x + w, y + h, colPanelBorder, FALSE);
    // header
    DrawBox(x, y, x + w, y + headerH, colHeader, TRUE);
    DrawFormatString(x + 6, y + 6, colHighlight, "Inspector");

    // draw current edit mode label in header (compact)
    const char* modeName = "Tile";
    switch (currentEditMode) {
        case EditModeType::Tile: modeName = "Tile"; break;
        case EditModeType::Enemy: modeName = "Enemy"; break;
        case EditModeType::Point: modeName = "Point"; break;
        default: modeName = "Unknown"; break;
    }
    DrawFormatString(x + 6 + static_cast<int>(56 * fs), y + 6, colSubText, "Mode: %s", modeName);

    // header controls: compute base positions
    int collapseBtnX1 = x + w - static_cast<int>(36 * fs);
    int collapseBtnY1 = y + static_cast<int>(6 * fs);
    int collapseBtnW = static_cast<int>(28 * fs);
    int collapseBtnH = static_cast<int>(18 * fs);

    // hover for collapse
    bool hoverCollapse = (mouseX >= collapseBtnX1 && mouseX <= collapseBtnX1 + collapseBtnW && mouseY >= collapseBtnY1 && mouseY <= collapseBtnY1 + collapseBtnH);

    // draw collapse button (highlight on hover)
    DrawBox(collapseBtnX1, collapseBtnY1, collapseBtnX1 + collapseBtnW, collapseBtnY1 + collapseBtnH, hoverCollapse ? colButtonHover : GetColor(80,80,80), TRUE);
    DrawFormatString(collapseBtnX1 + static_cast<int>(8 * fs), collapseBtnY1 + static_cast<int>(2 * fs), GetColor(255,255,255), inspectorCollapsed ? ">" : "<");

    // draw font size - / + buttons to left of collapse
    int ctrlGap = static_cast<int>(6 * fs);
    int fontBtnW = static_cast<int>(20 * fs);
    int fontBtnH = static_cast<int>(18 * fs);
    int fontPlusX = collapseBtnX1 - ctrlGap - fontBtnW;
    int fontMinusX = fontPlusX - ctrlGap - fontBtnW;
    int fontBtnsY = y + static_cast<int>(6 * fs);
    bool hoverFontPlus = (mouseX >= fontPlusX && mouseX <= fontPlusX + fontBtnW && mouseY >= fontBtnsY && mouseY <= fontBtnsY + fontBtnH);
    bool hoverFontMinus = (mouseX >= fontMinusX && mouseX <= fontMinusX + fontBtnW && mouseY >= fontBtnsY && mouseY <= fontBtnsY + fontBtnH);
    DrawBox(fontMinusX, fontBtnsY, fontMinusX + fontBtnW, fontBtnsY + fontBtnH, hoverFontMinus ? colButtonHover : colButton, TRUE);
    DrawFormatString(fontMinusX + static_cast<int>(4 * fs), fontBtnsY + static_cast<int>(2 * fs), GetColor(255,255,255), "-");
    DrawBox(fontPlusX, fontBtnsY, fontPlusX + fontBtnW, fontBtnsY + fontBtnH, hoverFontPlus ? colButtonHover : colButton, TRUE);
    DrawFormatString(fontPlusX + static_cast<int>(4 * fs), fontBtnsY + static_cast<int>(2 * fs), GetColor(255,255,255), "+");

    // draw dock toggle to left of font buttons
    int dockBtnW = fontBtnW;
    int dockBtnX = fontMinusX - ctrlGap - dockBtnW;
    bool hoverDock = (mouseX >= dockBtnX && mouseX <= dockBtnX + dockBtnW && mouseY >= fontBtnsY && mouseY <= fontBtnsY + fontBtnH);
    DrawBox(dockBtnX, fontBtnsY, dockBtnX + dockBtnW, fontBtnsY + fontBtnH, (inspectorDocked ? GetColor(70,120,170) : (hoverDock ? colButtonHover : colButton)), TRUE);
    DrawFormatString(dockBtnX + static_cast<int>(2 * fs), fontBtnsY + static_cast<int>(2 * fs), GetColor(255,255,255), inspectorDocked ? "D" : "d");

    int contentX = x + pad;
    int contentY = y + headerH + pad;
    int yCursor = contentY;

    // Draw mode help as its own line above the search box so it never overlaps
    std::string help = GetModeHelpText();
    if (!help.empty()) {
        int helpAvail = w - pad * 2;
        std::string helpShown = Ellipsize(help, helpAvail, fs * 0.95f);
        DrawFormatString(contentX, yCursor, colSubText, "%s", helpShown.c_str());
        yCursor += static_cast<int>(18 * fs); // space for help line
    }

    if (inspectorCollapsed) return; // don't draw contents when collapsed

    // Set clipping to inspector content area so no text draws outside
    int clipLeft = x + 2;
    int clipTop = y + headerH + 2;
    int clipRight = x + w - 2;
    int clipBottom = y + h - 2;
    SetDrawArea(clipLeft, clipTop, clipRight, clipBottom);

    // Search box
    int searchBoxX = contentX;
    int searchBoxY = yCursor;
    int searchBoxW = w - pad * 2;
    // ensure search box width leaves space for clear button when scaled
    int clearBtnW_est = smallBtnW - static_cast<int>(8 * fs);
    if (clearBtnW_est < 12) clearBtnW_est = 12;
    if (searchBoxW < clearBtnW_est + 24) searchBoxW = clearBtnW_est + 24;
    int searchBoxH = searchH;
    DrawBox(searchBoxX, searchBoxY, searchBoxX + searchBoxW, searchBoxY + searchBoxH, GetColor(60,60,60), TRUE);
    DrawBox(searchBoxX, searchBoxY, searchBoxX + searchBoxW, searchBoxY + searchBoxH, GetColor(120,120,120), FALSE);
    // compute available width for search text: leave 8px left padding and clear button space
    int textX = searchBoxX + static_cast<int>(8 * fs);
    int availableW = searchBoxW - (textX - searchBoxX) - (inspectorSearchBuffer.empty() ? 12 : (clearBtnW_est + static_cast<int>(12 * fs)));
    std::string showSearch = Ellipsize(inspectorSearchBuffer, availableW, fs);
    DrawFormatString(textX, searchBoxY + static_cast<int>(6 * fs), colSubText, "Search: %s", showSearch.c_str());

    // clear button
    bool hoverClear = false;
    if (!inspectorSearchBuffer.empty()) {
        int clearBtnW = clearBtnW_est;
        int clearBtnH = smallBtnH - static_cast<int>(6 * fs);
        int clearBtnX1 = searchBoxX + searchBoxW - clearBtnW - static_cast<int>(8 * fs);
        int clearBtnY1 = searchBoxY + (searchBoxH - clearBtnH) / 2;
        hoverClear = (mouseX >= clearBtnX1 && mouseX <= clearBtnX1 + clearBtnW && mouseY >= clearBtnY1 && mouseY <= clearBtnY1 + clearBtnH);
        DrawBox(clearBtnX1, clearBtnY1, clearBtnX1 + clearBtnW, clearBtnY1 + clearBtnH, hoverClear ? colButtonHover : colClearBtn, TRUE);
        DrawFormatString(clearBtnX1 + static_cast<int>(4 * fs), clearBtnY1 + static_cast<int>(1 * fs), GetColor(255,255,255), "X");
    }

    yCursor += searchBoxH + static_cast<int>(10 * fs);

    // draw a subtle separator
    DrawBox(x + 4, yCursor - static_cast<int>(6 * fs), x + w - 4, yCursor - static_cast<int>(6 * fs) + 1, colSeparator, TRUE);

    // Player foldout header
    DrawFormatString(contentX, yCursor, colSubText, "%s Player", inspectorFoldoutPlayer ? "v" : ">");
    DrawFormatString(contentX + static_cast<int>(28 * fs), yCursor, colText, "Player");
    yCursor += rowH - static_cast<int>(6 * fs);

    // Player fields (only those matching filter)
    if (inspectorFoldoutPlayer) {
        const int fields = 7;
        const char* labels[] = { "PosX", "PosY", "Life", "Bullets", "SavedX", "SavedY", "FacingRight" };
        int drawn = 0;
        bool wroteExtraForEditing = false;
        for (int i = 0; i < fields; ++i) {
            if (!matchesFilterString(inspectorSearchBuffer, labels[i])) continue;
            int rowY = yCursor + drawn * rowH;
            // stop drawing rows that start beyond clipBottom to avoid unnecessary work
            if (rowY > clipBottom - rowH) break;
            int color = (inspectorTarget == InspectorTarget::Player && inspectorFieldIndex == i) ? colHighlight : colSubText;
            char buf[64] = {0};
            switch (i) {
                case 0: sprintf_s(buf, "%d", player.GetX()); break;
                case 1: sprintf_s(buf, "%d", player.GetY()); break;
                case 2: sprintf_s(buf, "%d", player.GetSaveData().pLife); break;
                case 3: sprintf_s(buf, "%d", player.GetSaveData().pBullet); break;
                case 4: sprintf_s(buf, "%d", player.savedX); break;
                case 5: sprintf_s(buf, "%d", player.savedY); break;
                case 6: sprintf_s(buf, "%s", player.isFacingRight ? "true" : "false"); break;
            }

            // label
            DrawFormatString(contentX, rowY + static_cast<int>(6 * fs), color, "%s", labels[i]);
            // value box
            int valueX = contentX + labelW;
            DrawBox(valueX, rowY + static_cast<int>(2 * fs), valueX + valueW, rowY + rowH - static_cast<int>(6 * fs), colRowBg, TRUE);
            DrawFormatString(valueX + static_cast<int>(6 * fs), rowY + static_cast<int>(8 * fs), color, "%s", buf);

            // highlight focus
            if (inspectorTarget == InspectorTarget::Player && inspectorFieldIndex == i) {
                DrawBox(valueX, rowY + static_cast<int>(2 * fs), valueX + valueW, rowY + rowH - static_cast<int>(6 * fs), colHighlight, FALSE);
                if (inspectorEditingText) {
                    // draw editable text truncated with ellipsis if needed
                    int editTextAvail = valueW - static_cast<int>(12 * fs);
                    std::string ed = Ellipsize(inspectorEditBuffer, editTextAvail, fs);
                    DrawFormatString(valueX + static_cast<int>(6 * fs), rowY + rowH + static_cast<int>(4 * fs), GetColor(255,255,255), "%s", ed.c_str());
                    wroteExtraForEditing = true;
                }
            }

            // small +/- buttons
            int btnY = rowY + static_cast<int>(6 * fs);
            int minusBtnX = x + w - pad - smallBtnW - (smallBtnW + static_cast<int>(8 * fs));
            int plusBtnX = x + w - pad - smallBtnW;
            bool hoverMinus = (mouseX >= minusBtnX && mouseX <= minusBtnX + smallBtnW && mouseY >= btnY && mouseY <= btnY + smallBtnH);
            bool hoverPlus = (mouseX >= plusBtnX && mouseX <= plusBtnX + smallBtnW && mouseY >= btnY && mouseY <= btnY + smallBtnH);
            DrawBox(plusBtnX, btnY, plusBtnX + smallBtnW, btnY + smallBtnH, hoverPlus ? colButtonHover : colButton, TRUE);
            DrawFormatString(plusBtnX + static_cast<int>(8 * fs), btnY + static_cast<int>(2 * fs), GetColor(255,255,255), "-");
            DrawBox(minusBtnX, btnY, minusBtnX + smallBtnW, btnY + smallBtnH, hoverMinus ? colButtonHover : colButton, TRUE);
            DrawFormatString(minusBtnX + static_cast<int>(8 * fs), btnY + static_cast<int>(2 * fs), GetColor(255,255,255), "+");

            drawn++;
        }
        if (drawn == 0) {
            DrawFormatString(contentX, yCursor + static_cast<int>(6 * fs), colSubText, "(no matches)");
            yCursor += rowH;
        } else {
            yCursor += drawn * rowH;
            if (wroteExtraForEditing) yCursor += rowH; // leave an extra line when editing text to avoid overlap
        }
        yCursor += static_cast<int>(10 * fs);
    }

    // subtle separator between sections
    DrawBox(x + 6, yCursor - static_cast<int>(6 * fs), x + w - 6, yCursor - static_cast<int>(6 * fs) + 1, colSeparator, TRUE);

    // Enemy foldout header
    DrawFormatString(contentX, yCursor, colSubText, "%s Enemy", inspectorFoldoutEnemy ? "v" : ">");
    DrawFormatString(contentX + static_cast<int>(28 * fs), yCursor, colText, "Enemy");
    yCursor += rowH - static_cast<int>(6 * fs);

    // Enemy fields (only those matching filter)
    if (inspectorFoldoutEnemy) {
        const int fields = 5;
        const char* labels[] = { "PosX", "PosY", "Life", "Type", "Active" };
        int drawn = 0;
        bool wroteExtraForEditing = false;
        if (inspectorEnemyIndex >= 0 && inspectorEnemyIndex < (int)enemies.size()) {
            Enemy::EnemySaveData sd = enemies[inspectorEnemyIndex].GetSaveData();
            for (int i = 0; i < fields; ++i) {
                if (!matchesFilterString(inspectorSearchBuffer, labels[i])) continue;
                int rowY = yCursor + drawn * rowH;
                if (rowY > clipBottom - rowH) break;
                int color = (inspectorTarget == InspectorTarget::Enemy && inspectorFieldIndex == i) ? colHighlight : colSubText;
                char buf[64] = {0};
                switch (i) {
                    case 0: sprintf_s(buf, "%d", sd.ex); break;
                    case 1: sprintf_s(buf, "%d", sd.ey); break;
                    case 2: sprintf_s(buf, "%d", sd.eLife); break;
                    case 3: sprintf_s(buf, "%d", sd.eType); break;
                    case 4: sprintf_s(buf, "%s", sd.eIsActive ? "true" : "false"); break;
                }
                int valueX = contentX + labelW;
                DrawFormatString(contentX, rowY + static_cast<int>(6 * fs), color, "%s", labels[i]);
                DrawBox(valueX, rowY + static_cast<int>(2 * fs), valueX + valueW, rowY + rowH - static_cast<int>(6 * fs), colRowBg, TRUE);
                DrawFormatString(valueX + static_cast<int>(6 * fs), rowY + static_cast<int>(8 * fs), color, "%s", buf);
                if (inspectorTarget == InspectorTarget::Enemy && inspectorFieldIndex == i) {
                    DrawBox(valueX, rowY + static_cast<int>(2 * fs), valueX + valueW, rowY + rowH - static_cast<int>(6 * fs), colHighlight, FALSE);
                    if (inspectorEditingText) {
                        int editTextAvail = valueW - static_cast<int>(12 * fs);
                        std::string ed = Ellipsize(inspectorEditBuffer, editTextAvail, fs);
                        DrawFormatString(valueX + static_cast<int>(6 * fs), rowY + rowH + static_cast<int>(4 * fs), GetColor(255,255,255), "%s", ed.c_str());
                        wroteExtraForEditing = true;
                    }
                }
                int btnY = rowY + static_cast<int>(6 * fs);
                int minusBtnX = x + w - pad - smallBtnW - (smallBtnW + static_cast<int>(8 * fs));
                int plusBtnX = x + w - pad - smallBtnW;
                bool hoverMinus = (mouseX >= minusBtnX && mouseX <= minusBtnX + smallBtnW && mouseY >= btnY && mouseY <= btnY + smallBtnH);
                bool hoverPlus = (mouseX >= plusBtnX && mouseX <= plusBtnX + smallBtnW && mouseY >= btnY && mouseY <= btnY + smallBtnH);
                DrawBox(plusBtnX, btnY, plusBtnX + smallBtnW, btnY + smallBtnH, hoverPlus ? colButtonHover : colButton, TRUE);
                DrawFormatString(plusBtnX + static_cast<int>(8 * fs), btnY + static_cast<int>(2 * fs), GetColor(255,255,255), "-");
                DrawBox(minusBtnX, btnY, minusBtnX + smallBtnW, btnY + smallBtnH, hoverMinus ? colButtonHover : colButton, TRUE);
                DrawFormatString(minusBtnX + static_cast<int>(8 * fs), btnY + static_cast<int>(2 * fs), GetColor(255,255,255), "+");
                drawn++;
            }
            if (drawn == 0) {
                DrawFormatString(contentX, yCursor + static_cast<int>(6 * fs), colSubText, "(no matches)");
                yCursor += rowH;
            } else {
                yCursor += drawn * rowH;
                if (wroteExtraForEditing) yCursor += rowH;
            }
        } else {
            DrawFormatString(contentX, yCursor + static_cast<int>(6 * fs), colSubText, "No enemy selected");
            yCursor += rowH;
        }
        yCursor += static_cast<int>(10 * fs);
    }

    // separator
    DrawBox(x + 6, yCursor - static_cast<int>(6 * fs), x + w - 6, yCursor - static_cast<int>(6 * fs) + 1, colSeparator, TRUE);

    // Room foldout header
    DrawFormatString(contentX, yCursor, colSubText, "%s Rooms", inspectorFoldoutRoom ? "v" : ">");
    DrawFormatString(contentX + static_cast<int>(28 * fs), yCursor, colText, "Rooms");
    yCursor += rowH - static_cast<int>(6 * fs);

    if (inspectorFoldoutRoom && map) {
        auto &rooms = map->GetRooms();
        if (rooms.empty()) {
            DrawFormatString(contentX, yCursor + static_cast<int>(6 * fs), colSubText, "(no rooms)");
            yCursor += rowH;
        } else {
            int drawn = 0;
            bool wroteExtraForEditing = false;
            for (int i = 0; i < (int)rooms.size(); ++i) {
                const auto &r = rooms[i];
                char buf[128]; sprintf_s(buf, "Room %d: (%d,%d)-(%d,%d)", r.id, r.xStart, r.yStart, r.xEnd, r.yEnd);
                if (!matchesFilterString(inspectorSearchBuffer, buf)) continue;
                int rowY = yCursor + drawn * rowH;
                if (rowY > clipBottom - rowH) break;
                int color = (inspectorTarget == InspectorTarget::Room && selectedRoomIndex == i) ? colHighlight : colSubText;
                DrawFormatString(contentX, rowY + static_cast<int>(6 * fs), color, "%s", buf);
                // edit button
                int btnX = x + w - pad - smallBtnW;
                int btnY = rowY + static_cast<int>(6 * fs);
                bool hoverRoomEdit = (mouseX >= btnX && mouseX <= btnX + smallBtnW && mouseY >= btnY && mouseY <= btnY + smallBtnH);
                DrawBox(btnX, btnY, btnX + smallBtnW, btnY + smallBtnH, hoverRoomEdit ? colButtonHover : colButton, TRUE);
                DrawFormatString(btnX + static_cast<int>(8 * fs), rowY + static_cast<int>(7 * fs), GetColor(255,255,255), "E");
                if (inspectorTarget == InspectorTarget::Room && selectedRoomIndex == i && inspectorEditingText) {
                    int editTextAvail = w - (contentX - x) - static_cast<int>(16 * fs);
                    std::string ed = Ellipsize(inspectorEditBuffer, editTextAvail, fs);
                    DrawFormatString(contentX, rowY + rowH + static_cast<int>(4 * fs), GetColor(255,255,255), "%s", ed.c_str());
                    wroteExtraForEditing = true;
                }
                drawn++;
            }
            yCursor += drawn * rowH;
            if (wroteExtraForEditing) yCursor += rowH;
        }
        yCursor += static_cast<int>(10 * fs);

        // If inspector has a selected room, show quick preview and text controls
        if (inspectorTarget == InspectorTarget::Room && selectedRoomIndex >= 0 && map) {
            auto &rooms = map->GetRooms();
            if (selectedRoomIndex >= 0 && selectedRoomIndex < (int)rooms.size()) {
                const Room &sr = rooms[selectedRoomIndex];
                // preview label
                DrawFormatString(contentX, yCursor, colSubText, "Selected Room Preview:");
                yCursor += static_cast<int>(rowH * 0.8f);
                // Draw a translucent box representing room area in inspector coords
                DrawBox(contentX, yCursor, contentX + static_cast<int>(std::min(w - pad*2, 200)), yCursor + static_cast<int>(60 * fs), GetColor(30,30,30), TRUE);
                // Draw overlay text if set
                if (sr.tag != -1) {
                    // show different text based on tag if overlayText empty
                    if (!sr.overlayText.empty()) {
                        int col = sr.overlayColor;
                        int rcol = (col >> 16) & 0xFF;
                        int gcol = (col >> 8) & 0xFF;
                        int bcol = col & 0xFF;
                        DrawFormatString(contentX + static_cast<int>(6 * fs), yCursor + static_cast<int>(6 * fs), GetColor(rcol,gcol,bcol), "%s", sr.overlayText.c_str());
                    } else {
                        // default text per tag: check runtime mapping first, then switch-case fallback
                        GameManager &gm = GameManager::GetInstance();
                        std::string dyn = gm.GetRoomTagText(sr.tag);
                        std::string txt = !dyn.empty() ? dyn : gm.GetTextForTagSwitch(sr.tag);
                        int tagCol = gm.GetRoomTagColor(sr.tag);
                        int rcol = 200, gcol = 200, bcol = 200;
                        if (tagCol != 0) { rcol = (tagCol>>16)&0xFF; gcol = (tagCol>>8)&0xFF; bcol = tagCol&0xFF; }
                        DrawFormatString(contentX + static_cast<int>(6 * fs), yCursor + static_cast<int>(6 * fs), GetColor(rcol,gcol,bcol), "%s", txt.c_str());
                    }
                } else {
                    if (!sr.overlayText.empty()) {
                        int col = sr.overlayColor;
                        int rcol = (col >> 16) & 0xFF;
                        int gcol = (col >> 8) & 0xFF;
                        int bcol = col & 0xFF;
                        DrawFormatString(contentX + static_cast<int>(6 * fs), yCursor + static_cast<int>(6 * fs), GetColor(rcol,gcol,bcol), "%s", sr.overlayText.c_str());
                    } else {
                        DrawFormatString(contentX + static_cast<int>(6 * fs), yCursor + static_cast<int>(6 * fs), colSubText, "(no overlay text)");
                    }
                }
                yCursor += static_cast<int>(64 * fs);

                // Color and size controls (display only; interaction in Update)
                DrawFormatString(contentX, yCursor, colSubText, "Text Color: #%06X", sr.overlayColor);
                yCursor += static_cast<int>(rowH * 0.85f);
                DrawFormatString(contentX, yCursor, colSubText, "Font size: %d", sr.overlayFontSize);
                yCursor += static_cast<int>(rowH * 0.85f) + static_cast<int>(10 * fs);
            }
        }

        // Draw Tag->Text list UI
        // compute area
        int tagAreaX = contentX + static_cast<int>(std::min(w - pad*2, 220));
        int tagAreaW = std::max(140, w - pad*2 - static_cast<int>(std::min(w - pad*2, 220)));
        int tagAreaY = yCursor - static_cast<int>(64 * fs);
        int rowH_tag = static_cast<int>(20 * fs);
        DrawFormatString(contentX, yCursor, colSubText, "Tag Texts:");
        yCursor += rowH_tag;
        // list entries
        for (int i=0;i<(int)inspectorTagList.size();++i) {
            int t = inspectorTagList[i];
            std::string txt = GameManager::GetInstance().GetRoomTagText(t);
            int rowY = yCursor + i * rowH_tag;
            int color = (inspectorTagSelectedIndex == i) ? colHighlight : colSubText;
            int tagCol = GameManager::GetInstance().GetRoomTagColor(t);
            if (tagCol != 0) color = GetColor((tagCol>>16)&0xFF, (tagCol>>8)&0xFF, tagCol&0xFF);
            DrawFormatString(contentX, rowY + static_cast<int>(2*fs), color, "#%d: %s", t, txt.c_str());
        }
        int tagsDrawn = (int)inspectorTagList.size();
        yCursor += std::max(1, tagsDrawn) * rowH_tag;
        // Draw edit buffer if editing
        if (inspectorEditingTagText && inspectorTagSelectedIndex >= 0 && inspectorTagSelectedIndex < (int)inspectorTagList.size()) {
            int selTag = inspectorTagList[inspectorTagSelectedIndex];
            DrawFormatString(contentX, yCursor, GetColor(255,255,255), "Editing #%d: %s", selTag, inspectorTagEditBuffer.c_str());
            yCursor += rowH_tag;
        }
        // Draw hint for tag list operations
        DrawFormatString(contentX, yCursor, colSubText, "Ctrl+N: add, Del: remove, E: edit selected");
        yCursor += static_cast<int>(rowH_tag * 1.2f);
    }

    // Draw Save Rooms (OK) button at bottom-left of inspector panel
    int saveBtnW = static_cast<int>(100 * fs);
    int saveBtnH = static_cast<int>(28 * fs);
    int saveBtnX = x + pad;
    int saveBtnY = y + h - pad - saveBtnH;
    bool hoverSave = (mouseX >= saveBtnX && mouseX <= saveBtnX + saveBtnW && mouseY >= saveBtnY && mouseY <= saveBtnY + saveBtnH);
    DrawBox(saveBtnX, saveBtnY, saveBtnX + saveBtnW, saveBtnY + saveBtnH, hoverSave ? GetColor(90,150,90) : GetColor(70,120,70), TRUE);
    DrawFormatString(saveBtnX + static_cast<int>(10 * fs), saveBtnY + static_cast<int>(6 * fs), GetColor(255,255,255), "Save Rooms");

    // Show last save status near the button
    if (lastSaveStatus == SaveStatus::Success) {
        DrawFormatString(saveBtnX + saveBtnW + static_cast<int>(8 * fs), saveBtnY + static_cast<int>(6 * fs), GetColor(0,255,0), "Saved");
    } else if (lastSaveStatus == SaveStatus::Failure) {
        DrawFormatString(saveBtnX + saveBtnW + static_cast<int>(8 * fs), saveBtnY + static_cast<int>(6 * fs), GetColor(255,0,0), "Save failed");
    }

    DEBUG_ONLY(
        if (lastSaveStatus == SaveStatus::Success) {
            DrawFormatString(10, 200, GetColor(0, 255, 0), "Rooms saved to rooms.csv");
        }
        else if (lastSaveStatus == SaveStatus::Failure) {
            DrawFormatString(10, 200, GetColor(255, 0, 0), "Rooms save failed");
        }
    );

    // Draw resize grip on right edge
    int gripW = static_cast<int>(8 * fs);
    DrawBox(x + w - gripW - 2, y + headerH, x + w - 2, y + headerH + static_cast<int>(24 * fs), GetColor(80,80,80), TRUE);

    // Draw bottom height resize grip (centered)
    int bottomGripW = static_cast<int>(48 * fs);
    int bottomGripH = static_cast<int>(10 * fs);
    int bottomGripX = x + (w / 2) - (bottomGripW / 2);
    int bottomGripY = y + h - bottomGripH - 4;
    bool hoverBottomGrip = (mouseX >= bottomGripX && mouseX <= bottomGripX + bottomGripW && mouseY >= bottomGripY && mouseY <= bottomGripY + bottomGripH);
    DrawBox(bottomGripX, bottomGripY, bottomGripX + bottomGripW, bottomGripY + bottomGripH, hoverBottomGrip ? colButtonHover : GetColor(80,80,80), TRUE);

    // restore draw area to full screen
    SetDrawArea(0,0,SCREEN_WIDTH-1,SCREEN_HEIGHT-1);

    // Draw room rectangles (world -> screen coords)
    Map &mref_for_rooms = *map;
    Camera &cam_for_rooms = gm.GetCamera();
    auto &rooms_for_draw = mref_for_rooms.GetRooms();
    for (int i = 0; i < (int)rooms_for_draw.size(); ++i) {
        const Room &r = rooms_for_draw[i];
        int sx0 = r.xStart - cam_for_rooms.GetOffsetX();
        int sy0 = r.yStart - cam_for_rooms.GetOffsetY();
        int sx1 = r.xEnd - cam_for_rooms.GetOffsetX();
        int sy1 = r.yEnd - cam_for_rooms.GetOffsetY();
        int minx = std::min(sx0, sx1);
        int miny = std::min(sy0, sy1);
        int maxx = std::max(sx0, sx1);
        int maxy = std::max(sy0, sy1);
        // skip completely off-screen
        if (maxx < 0 || maxy < 0 || minx > SCREEN_WIDTH || miny > SCREEN_HEIGHT) continue;
        // semi-transparent fill
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
        int fillCol = GetColor(30, 120, 80);
        // If room has a tag with a configured color, use a tinted fill based on that color
        if (r.tag != -1) {
            int tagCol = GameManager::GetInstance().GetRoomTagColor(r.tag);
            if (tagCol != 0) {
                int tr = (tagCol >> 16) & 0xFF;
                int tg = (tagCol >> 8) & 0xFF;
                int tb = tagCol & 0xFF;
                // darken for fill
                int fr = std::max(0, tr / 3);
                int fg = std::max(0, tg / 3);
                int fb = std::max(0, tb / 3);
                fillCol = GetColor(fr, fg, fb);
            }
        }
        DrawBox(minx, miny, maxx, maxy, fillCol, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        // outline: highlight if selected
        int outlineCol = (inspectorTarget == InspectorTarget::Room && selectedRoomIndex == i) ? GetColor(255,200,64) : GetColor(80,200,120);
        // If room has a tag color, use it for outline so tags are visually indicated
        if (r.tag != -1) {
            int tagCol = GameManager::GetInstance().GetRoomTagColor(r.tag);
            if (tagCol != 0) {
                int tr = (tagCol >> 16) & 0xFF;
                int tg = (tagCol >> 8) & 0xFF;
                int tb = tagCol & 0xFF;
                outlineCol = GetColor(tr, tg, tb);
            }
        }
        // If a tag is selected in the inspector tag list and this room matches that tag, emphasize outline
        if (inspectorTagSelectedIndex >= 0 && inspectorTagSelectedIndex < (int)inspectorTagList.size()) {
            int selTag = inspectorTagList[inspectorTagSelectedIndex];
            if (r.tag == selTag) {
                // brighter emphasis
                outlineCol = GetColor(255, 240, 160);
            }
        }
        DrawBox(minx, miny, maxx, maxy, outlineCol, FALSE);
        // draw room id
        DrawFormatString(minx + 4, miny + 4, GetColor(255,255,255), "Room %d", r.id);
        // draw tag number if set
        if (r.tag != -1) {
            int tagColor = GameManager::GetInstance().GetRoomTagColor(r.tag);
            int color = tagColor != 0 ? GetColor((tagColor>>16)&0xFF, (tagColor>>8)&0xFF, tagColor&0xFF) : GetColor(255,220,80);
            DrawFormatString(minx + 4, miny + 20, color, "#%d", r.tag);
        }
        // draw overlay text if present (small)
        if (!r.overlayText.empty()) {
            DrawFormatString(minx + 4, miny + 20, GetColor(200,200,200), "%s", r.overlayText.c_str());
            DrawFormatString(minx + 4, miny + 36, GetColor(200,200,200), "%s", r.overlayText.c_str());
        }
    }

    // Draw tile palette overlay if visible
    if (tilePaletteVisible && tilePalette) {
        tilePalette->draw();
    }

    // draw per-room save dots on map (bottom-right corner)
    Map &mref = *map;
    Camera &cam = gm.GetCamera();
    auto &rooms2 = mref.GetRooms();
    for (int i = 0; i < (int)rooms2.size(); ++i) {
        const Room &r = rooms2[i];
        int sx = r.xEnd - cam.GetOffsetX();
        int sy = r.yEnd - cam.GetOffsetY();
        // small filled circle as save dot
        int radius = static_cast<int>(6 * fs);
        int color = GetColor(80,200,120);
        DrawCircle(sx, sy, radius, color, TRUE);
    }

    // Draw selection rectangle in world -> screen coords
    if (rectSelecting && map) {
        Camera &cam = gm.GetCamera();
        int sx0 = rectStartX - cam.GetOffsetX();
        int sy0 = rectStartY - cam.GetOffsetY();
        int sx1 = rectCurX - cam.GetOffsetX();
        int sy1 = rectCurY - cam.GetOffsetY();
        int left = std::min(sx0,sx1);
        int top = std::min(sy0,sy1);
        int right = std::max(sx0,sx1);
        int bottom = std::max(sy0,sy1);
        // semi-transparent fill
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
        DrawBox(left, top, right, bottom, GetColor(30,120,80), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        // highlighted outline
        DrawBox(left, top, right, bottom, GetColor(255,200,64), FALSE);
        // size label near top-left
        int wbox = std::max(0, right - left);
        int hbox = std::max(0, bottom - top);
        char lbl[64]; sprintf_s(lbl, "%d x %d", wbox, hbox);
        int lblX = left + 6;
        int lblY = top - static_cast<int>(18 * fs);
        if (lblY < 0) lblY = top + 6; // if there's no space above, place inside
        DrawBox(lblX - 4, lblY - 2, lblX + static_cast<int>(GetDrawFormatStringWidthToHandle(DX_DEFAULT_FONT_HANDLE, "%s", lbl) + 8), lblY + static_cast<int>(14 * fs), GetColor(40,40,40), TRUE);
        DrawFormatString(lblX, lblY, GetColor(255,255,255), "%s", lbl);
    }
}
