#include "game.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CLOSE && !is_character_creation) { ClearLegendaryDrops(); SaveSaveFileToDisk(); }
    if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; } return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = {0}; MSG msg; HBRUSH grass_brush, creation_bg; HPEN clean_null_pen;
    
    wc.lpfnWndProc = WindowProc; wc.hInstance = hInst; wc.lpszClassName = "SaturnParallelEngine"; RegisterClass(&wc);
    HWND hwnd = CreateWindow("SaturnParallelEngine", "Low-Poly ARPG Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInst, NULL);
    SecureZeroMemory(&msg, sizeof(MSG));
    
    grass_brush = CreateSolidBrush(RGB(105, 185, 85)); creation_bg = CreateSolidBrush(RGB(15, 18, 24)); clean_null_pen = CreatePen(PS_NULL, 0, RGB(0,0,0));
    RecalculateCarriedWeight();
    LoadWorldMapState();
    InitSeasonAndWeather();
    ApplyClassAndRaceStats(); GenerateProceduralScreen(screen_grid_y * META_GRID_SIZE + screen_grid_x);

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); } 
        else {
            ProcessGamepadInput(); UpdateGamePhysics();
            
            UpdatePlayerArrow();
            
            if (!in_cave && (player_x <= 0.5f || player_x >= (MAP_SIZE - 1.5f) || player_y <= 0.5f || player_y >= (MAP_SIZE - 1.5f))) {
                // Leaving the Castle steps to its real neighbouring cell like any
                // other screen instead of snapping back to the world centre.
                int leaving_castle = (screens_until_town == 0);
                {
                    // Direction-aware screen stepping: whichever edge the hero
                    // crossed determines which neighboring grid cell (and thus
                    // which remembered/rolled biome) comes next, so left/right/
                    // up/down can each lead somewhere different.
                    int dgx = 0, dgy = 0;
                    float reenter_x = 15.0f, reenter_y = 15.0f;
                    if (player_x <= 0.5f) { dgx = -1; reenter_x = (float)(MAP_SIZE - 3); }
                    else if (player_x >= (MAP_SIZE - 1.5f)) { dgx = 1; reenter_x = 2.0f; }
                    if (player_y <= 0.5f) { dgy = -1; reenter_y = (float)(MAP_SIZE - 3); }
                    else if (player_y >= (MAP_SIZE - 1.5f)) { dgy = 1; reenter_y = 2.0f; }

                    int new_gx = screen_grid_x + dgx;
                    int new_gy = screen_grid_y + dgy;
                    if (new_gx < 0 || new_gx >= META_GRID_SIZE || new_gy < 0 || new_gy >= META_GRID_SIZE) {
                        // Edge of the known world: a hard dead end, bounce back in bounds.
                        if (player_x <= 0.5f) player_x = 2.0f;
                        else if (player_x >= (MAP_SIZE - 1.5f)) player_x = (float)(MAP_SIZE - 3);
                        if (player_y <= 0.5f) player_y = 2.0f;
                        else if (player_y >= (MAP_SIZE - 1.5f)) player_y = (float)(MAP_SIZE - 3);
                    } else {
                        if (leaving_castle) screens_until_town = 3 + (rand() % 8);
                        else screens_until_town--;
                        screen_grid_x = new_gx; screen_grid_y = new_gy;
                        // Castle screens get their own id range (caves use 20000+)
                        // so they never share loot/campfires with an overworld cell.
                        if (screens_until_town == 0) GenerateProceduralScreen(30000 + screen_grid_y * META_GRID_SIZE + screen_grid_x);
                        else GenerateProceduralScreen(screen_grid_y * META_GRID_SIZE + screen_grid_x);
                        {
                            // Make sure the reentry tile is walkable (not water/rock); else find the nearest one that is.
                            int rx = (int)reenter_x, ry = (int)reenter_y;
                            if (VISUAL_MAP[ry][rx] == 12 || VISUAL_MAP[ry][rx] == 1) {
                                int best_d = 1 << 30, bx = rx, by = ry;
                                for (int y = 2; y <= MAP_SIZE - 3; y++) for (int x = 2; x <= MAP_SIZE - 3; x++) {
                                    if (VISUAL_MAP[y][x] == 12 || VISUAL_MAP[y][x] == 1) continue;
                                    int d = (x - rx) * (x - rx) + (y - ry) * (y - ry);
                                    if (d < best_d) { best_d = d; bx = x; by = y; }
                                }
                                reenter_x = (float)bx; reenter_y = (float)by;
                            }
                        }
                        player_x = reenter_x; player_y = reenter_y;
                    }
                }
            }
            
            cam_x = (int)((player_x - player_y) * (TILE_WIDTH / 2)); cam_y = (int)((player_x + player_y) * (TILE_HEIGHT / 2));
            HDC hdc = GetDC(hwnd); HDC memHDC = CreateCompatibleDC(hdc); HBITMAP memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT); SelectObject(memHDC, memBitmap);
            
            if (is_character_creation) {
                RECT r = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT}; FillRect(memHDC, &r, creation_bg);
                SetTextColor(memHDC, RGB(0, 255, 230)); SetBkColor(memHDC, RGB(15, 18, 24)); SetBkMode(memHDC, TRANSPARENT);
                HFONT underline_font = GetMenuUnderlineFont();
                HGDIOBJ prev_cc_font = SelectObject(memHDC, underline_font);
                TextOut(memHDC, 80, 70, "HERO CHARACTER CREATION", 24);
                SelectObject(memHDC, prev_cc_font);

                char label[256];
                SetTextColor(memHDC, (selected_creation_field == 0) ? RGB(255, 255, 0) : RGB(200, 200, 200));
                // Blinking text cursor on the Name field is the visible cue
                // that typing (A-Z / Space / Backspace) now actually edits
                // player_name, fixing the previously non-functional feature.
                const char *name_cursor = (selected_creation_field == 0 && (world_frame % 40) < 20) ? "_" : "";
                sprintf(label, "[1] Hero Name: %s%s", player_name, name_cursor); TextOut(memHDC, 100, 140, label, (int)strlen(label));
                if (selected_creation_field == 0) {
                    int nlen = (int)strlen(player_name);
                    SetTextColor(memHDC, RGB(0, 255, 230));
                    sprintf(label, "Letter %d of %d   D-Pad L/R: [%c]  A: add  X: delete",
                        nlen + 1 > 31 ? 31 : nlen + 1, (int)sizeof(player_name) - 1, (char)('A' + creation_name_letter_cursor));
                    TextOut(memHDC, 120, 160, label, (int)strlen(label));
                }

                SetTextColor(memHDC, (selected_creation_field == 1) ? RGB(255, 255, 0) : RGB(200, 200, 200));
                sprintf(label, "[2] Class: < %s >", class_names[selected_class]); TextOut(memHDC, 100, 180, label, (int)strlen(label));

                SetTextColor(memHDC, (selected_creation_field == 2) ? RGB(255, 255, 0) : RGB(200, 200, 200));
                sprintf(label, "[3] Race:  < %s >", race_names[selected_race]); TextOut(memHDC, 100, 220, label, (int)strlen(label));

                DrawHeroAssetEx(memHDC, 520, 200, 2.0f);

                prev_cc_font = SelectObject(memHDC, underline_font);
                SetTextColor(memHDC, RGB(0, 230, 200));
                TextOut(memHDC, 80, 272, "ATTRIBUTES", 10);
                SelectObject(memHDC, prev_cc_font);

                DrawStatJewels(memHDC, 80, 292, "STR", stat_strength);
                DrawStatJewels(memHDC, 80, 306, "DEX", stat_dexterity);
                DrawStatJewels(memHDC, 80, 320, "STA", stat_stamina);
                DrawStatJewels(memHDC, 80, 334, "MAG", stat_magick);
                DrawStatJewels(memHDC, 80, 348, "LCK", stat_luck);
                DrawStatJewels(memHDC, 80, 362, "INT", stat_intelligence);
                DrawStatJewels(memHDC, 80, 376, "CHR", stat_charisma);

                SetTextColor(memHDC, RGB(255, 255, 255));
                TextOut(memHDC, 80, 410, "Use DPAD/ARROW KEYS to select fields; TYPE A-Z to edit the Name.", 65);
                TextOut(memHDC, 80, 430, "PRESS [ENTER] or [A BUTTON] TO ENTER THE WORLD.", 48);
            } else {
                float ambient_intensity = (float)fabs(sin(day_night_cycle_accumulator));
                int r_tone = (int)(25.0f + ambient_intensity * 80.0f);
                int g_tone = (int)(30.0f + ambient_intensity * 155.0f);
                int b_tone = (int)(40.0f + ambient_intensity * 45.0f);
                
                COLORREF meadow_color = RGB(r_tone, g_tone, b_tone);
                if (current_biome != BIOME_CAVE) meadow_color = ApplySeasonToGround(meadow_color);
                HBRUSH live_ground = CreateSolidBrush(meadow_color); HGDIOBJ old_bg = SelectObject(memHDC, live_ground); PatBlt(memHDC, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, PATCOPY); SelectObject(memHDC, old_bg); DeleteObject(live_ground);
                SelectObject(memHDC, clean_null_pen);

                for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
                    if (VISUAL_MAP[r][c] == 12) {
                        int sx, sy; GetIsoCoords((float)c, (float)r, &sx, &sy);
                        if (sx >= -TILE_WIDTH && sx <= WINDOW_WIDTH + TILE_WIDTH) DrawFlowingRiver(memHDC, sx, sy, r * MAP_SIZE + c);
                    }
                }
                DrawSeasonalGroundCover(memHDC);
                for (int r = 0; r < MAP_SIZE; r++) {
                    for (int c = 0; c < MAP_SIZE; c++) {
                        int sx, sy; GetIsoCoords((float)c, (float)r, &sx, &sy); if (sx < -TILE_WIDTH * 2 || sx > WINDOW_WIDTH + TILE_WIDTH * 2) continue;
                        uint8_t t_id = VISUAL_MAP[r][c];
                        if (t_id == 1 || t_id == 10) {
                            if (current_biome == BIOME_CAVE) { DrawSubterraneanGeology(memHDC, sx, sy, (r * MAP_SIZE + c)); }
                            else {
                                int thick = (t_id == 1) ? 24 : 0; int drawing_sy = sy - thick;
                                POINT top[] = {{sx, drawing_sy}, {sx + TILE_WIDTH/2, drawing_sy + TILE_HEIGHT/2}, {sx, drawing_sy + TILE_HEIGHT}, {sx - TILE_WIDTH/2, drawing_sy + TILE_HEIGHT/2}};
                                HBRUSH b = CreateSolidBrush(GetTileColor(t_id, 0)); HGDIOBJ prev_tile = SelectObject(memHDC, b); Polygon(memHDC, top, 4); SelectObject(memHDC, prev_tile); DeleteObject(b);
                                if (thick > 0) {
                                    POINT wall_left[] = {{top[0].x, top[0].y}, {top[1].x, top[1].y}, {top[1].x, top[1].y + thick}, {top[0].x, top[0].y + thick}};
                                    POINT wall_right[] = {{top[1].x, top[1].y}, {top[2].x, top[2].y}, {top[2].x, top[2].y + thick}, {top[1].x, top[1].y + thick}};
                                    b = CreateSolidBrush(GetTileColor(t_id, 1)); HGDIOBJ prev_wall = SelectObject(memHDC, b); Polygon(memHDC, wall_left, 4); Polygon(memHDC, wall_right, 4); SelectObject(memHDC, prev_wall); DeleteObject(b);
                                }
                            }
                        }
                        if (t_id == 11) DrawLowPolyTree(memHDC, sx, sy, (r * MAP_SIZE + c));
                        if (t_id >= 2 && t_id <= 9 && t_id != 1) DrawGroundSceneryDecals(memHDC, sx, sy, t_id, (r * MAP_SIZE + c));
                        if (current_biome == BIOME_MOUNTAIN && c == torch_gate_x && r == torch_gate_y) {
                            DrawInteractiveTorches(memHDC, sx, sy);
                            if (!in_cave && IsNearPoint((float)torch_gate_x, (float)torch_gate_y, 2.5f)) {
                                SetTextColor(memHDC, RGB(255, 230, 120)); SetBkMode(memHDC, TRANSPARENT);
                                TextOut(memHDC, sx - 16, sy - 46, "CAVE", 4);
                            }
                        }
                        if (in_cave && c == (int)cave_treasure_x && r == (int)cave_treasure_y) {
                            CaveRecord *rec = FindOrCreateCaveRecord(current_cave_origin_cell);
                            HBRUSH chest_b = CreateSolidBrush(rec->treasure_claimed ? RGB(90, 70, 40) : RGB(230, 190, 40));
                            HGDIOBJ old_chest = SelectObject(memHDC, chest_b);
                            RECT chest = { sx - 8, sy - 6, sx + 8, sy + 6 };
                            FillRect(memHDC, &chest, chest_b); SelectObject(memHDC, old_chest); DeleteObject(chest_b);
                            if (!rec->treasure_claimed) {
                                SetTextColor(memHDC, RGB(255, 255, 255)); SetBkMode(memHDC, TRANSPARENT);
                                TextOut(memHDC, sx - 26, sy - 24, "TREASURE", 8);
                            }
                        }
                        if (c == (int)player_x && r == (int)player_y) DrawHeroAsset(memHDC);
                        if (enemy_hearts > 0.0f && c == (int)enemy_x && r == (int)enemy_y) DrawDynamicEnemy(memHDC);
                    }
                }
                DrawCampfires(memHDC); DrawBedroll(memHDC); DrawGroundLoot(memHDC); DrawDroppedItems(memHDC); DrawDebrisTwigs(memHDC); DrawSummonedZombie(memHDC);
                DrawFlyingArrow(memHDC); DrawFlyingSpell(memHDC); DrawClassAbilityFX(memHDC); DrawButterflies(memHDC); DrawBloodMistFX(memHDC);
                DrawEnvironmentalCritters(memHDC); DrawMerchantStoreFront(memHDC); DrawVillage(memHDC);
                DrawWeatherEffects(memHDC); DrawSolitaireSunMoonBeam(memHDC);
                ApplyCameraZoom(memHDC);
                DrawPlayerStatusHUD(memHDC);

                if (is_menu_open) DrawTabbedMenuOverlay(memHDC);

                SetTextColor(memHDC, RGB(255, 255, 255)); SetBkMode(memHDC, TRANSPARENT);
                TextOut(memHDC, 20, HUD_LOG_Y, arpg_action_log, (int)strlen(arpg_action_log));
                if (vendor_menu_open && IsNearMerchant()) {
                    DrawVendorShopOverlay(memHDC);
                }
                DrawSleepFade(memHDC);
            }
            BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memHDC, 0, 0, SRCCOPY); DeleteObject(memBitmap); DeleteDC(memHDC); ReleaseDC(hwnd, hdc); Sleep(16);
        }
    }
    DeleteObject(grass_brush); DeleteObject(creation_bg); DeleteObject(clean_null_pen); return 0;
}
