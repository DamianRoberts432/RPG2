#include "game.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CLOSE && !is_character_creation) { ClearLegendaryDrops(); LeaveHome(); SaveSaveFileToDisk(); }
    if (uMsg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) {
        RECT cr; GetClientRect(hwnd, &cr); // the frame is scaled to the window, so scale the mouse back
        if (uMsg != WM_MOUSEWHEEL && cr.right > 0 && cr.bottom > 0)
            lParam = MAKELPARAM((short)LOWORD(lParam) * WINDOW_WIDTH / cr.right, (short)HIWORD(lParam) * WINDOW_HEIGHT / cr.bottom);
        MouseEvent(uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// The game draws a WINDOW_WIDTH x WINDOW_HEIGHT frame and scales it to the
// window, so the HUD bars along the bottom are never cut off on smaller screens.
static void PresentFrame(HWND hwnd, HDC hdc, HDC frame) {
    RECT cr; GetClientRect(hwnd, &cr);
    if (cr.right == WINDOW_WIDTH && cr.bottom == WINDOW_HEIGHT) { BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, frame, 0, 0, SRCCOPY); return; }
    SetStretchBltMode(hdc, HALFTONE); SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, 0, 0, cr.right, cr.bottom, frame, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SRCCOPY);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = {0}; MSG msg; HBRUSH grass_brush, creation_bg; HPEN clean_null_pen;
    
    // Ask Windows for real pixels; otherwise display scaling (125%, 150%) stretches the window and blurs the text.
    BOOL (WINAPI *set_dpi_aware)(void) = (BOOL (WINAPI *)(void))GetProcAddress(GetModuleHandle("user32.dll"), "SetProcessDPIAware");
    if (set_dpi_aware) set_dpi_aware();
    wc.lpfnWndProc = WindowProc; wc.hInstance = hInst; wc.lpszClassName = "SaturnParallelEngine"; RegisterClass(&wc);
    // Fit the whole frame (client area, not counting the title bar) on the screen.
    RECT work; SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    RECT frame = { 0, 0, 100, 100 }; AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
    int extra_w = (frame.right - frame.left) - 100, extra_h = (frame.bottom - frame.top) - 100;
    fx fit = FxI(1), fw = FxI(work.right - work.left - extra_w) / WINDOW_WIDTH, fh = FxI(work.bottom - work.top - extra_h) / WINDOW_HEIGHT;
    if (fw < fit) fit = fw;
    if (fh < fit) fit = fh;
    int win_w = FxTrunc(WINDOW_WIDTH * fit) + extra_w, win_h = FxTrunc(WINDOW_HEIGHT * fit) + extra_h;
    HWND hwnd = CreateWindow("SaturnParallelEngine", "Low-Poly ARPG Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE, work.left, work.top, win_w, win_h, NULL, NULL, hInst, NULL);
    SecureZeroMemory(&msg, sizeof(MSG));
    
    grass_brush = CreateSolidBrush(RGB(105, 185, 85)); creation_bg = CreateSolidBrush(RGB(15, 18, 24)); clean_null_pen = CreatePen(PS_NULL, 0, RGB(0,0,0));
    RecalculateCarriedWeight();
    int loaded = LoadWorldMapState();
    if (!world_seed) world_seed = 1u + (uint32_t)(GetTickCount() % 2000000000u);
    GenerateWholeWorld(world_seed);
    TownGrowthInit();
    if (!loaded) { screen_grid_x = elysium_gx; screen_grid_y = elysium_gy; }
    screens_until_town = WorldIsTown(screen_grid_x, screen_grid_y) ? 0 : 1;
    InitCardSets();
    IngestUpdates();
    SoundInit();
    InitSeasonAndWeather();
    ApplyClassAndRaceStats(); GenerateProceduralScreen((screens_until_town == 0 ? 30000 : 0) + screen_grid_y * META_GRID_SIZE + screen_grid_x);

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); } 
        else {
            ProcessGamepadInput(); UpdateGamePhysics();
            
            UpdatePlayerArrow(); UpdateThrownSpear(); AdvTick();
            
            if (!in_cave && (player_x <= FX(0.5) || player_x >= (FxI(MAP_SIZE) - FX(1.5)) || player_y <= FX(0.5) || player_y >= (FxI(MAP_SIZE) - FX(1.5)))) {
                // Leaving the Castle steps to its real neighbouring cell like any
                // other screen instead of snapping back to the world centre.
                {
                    // Direction-aware screen stepping: whichever edge the hero
                    // crossed determines which neighboring grid cell (and thus
                    // which remembered/rolled biome) comes next, so left/right/
                    // up/down can each lead somewhere different.
                    int dgx = 0, dgy = 0;
                    fx reenter_x = player_x, reenter_y = player_y; // keep the hero's place along the edge they crossed
                    if (player_x <= FX(0.5)) { dgx = -1; reenter_x = FxI(MAP_SIZE - 3); }
                    else if (player_x >= (FxI(MAP_SIZE) - FX(1.5))) { dgx = 1; reenter_x = FxI(2); }
                    if (player_y <= FX(0.5)) { dgy = -1; reenter_y = FxI(MAP_SIZE - 3); }
                    else if (player_y >= (FxI(MAP_SIZE) - FX(1.5))) { dgy = 1; reenter_y = FxI(2); }
                    if (reenter_x < FxI(2)) reenter_x = FxI(2); else if (reenter_x > FxI(MAP_SIZE - 3)) reenter_x = FxI(MAP_SIZE - 3);
                    if (reenter_y < FxI(2)) reenter_y = FxI(2); else if (reenter_y > FxI(MAP_SIZE - 3)) reenter_y = FxI(MAP_SIZE - 3);

                    int new_gx = screen_grid_x + dgx;
                    int new_gy = screen_grid_y + dgy;
                    int off_world = new_gx < 0 || new_gx >= META_GRID_SIZE || new_gy < 0 || new_gy >= META_GRID_SIZE;
                    int no_boat = !off_world && SeaNeedsBoat(new_gx, new_gy);
                    if (off_world || no_boat) {
                        // Edge of the known world (or open sea without a Boat): bounce back in bounds.
                        strcpy(arpg_action_log, no_boat ? "OPEN SEA: You need a Boat to sail out there. Merchants sell them." : "WORLD EDGE: Nothing lies beyond here... yet. Turn back.");
                        if (player_x <= FX(0.5)) player_x = FxI(2);
                        else if (player_x >= (FxI(MAP_SIZE) - FX(1.5))) player_x = FxI(MAP_SIZE - 3);
                        if (player_y <= FX(0.5)) player_y = FxI(2);
                        else if (player_y >= (FxI(MAP_SIZE) - FX(1.5))) player_y = FxI(MAP_SIZE - 3);
                    } else {
                        screens_until_town = WorldIsTown(new_gx, new_gy) ? 0 : 1;
                        screen_grid_x = new_gx; screen_grid_y = new_gy;
                        // Castle screens get their own id range (caves use 20000+)
                        // so they never share loot/campfires with an overworld cell.
                        if (screens_until_town == 0) GenerateProceduralScreen(30000 + screen_grid_y * META_GRID_SIZE + screen_grid_x);
                        else GenerateProceduralScreen(screen_grid_y * META_GRID_SIZE + screen_grid_x);
                        {
                            // Make sure the reentry tile is walkable (not water/rock); else find the nearest one that is.
                            int rx = FxTrunc(reenter_x), ry = FxTrunc(reenter_y);
                            if ((VISUAL_MAP[ry][rx] == 12 && !HasInventoryItem("Boat")) || VISUAL_MAP[ry][rx] == 1) {
                                int best_d = 1 << 30, bx = rx, by = ry;
                                for (int y = 2; y <= MAP_SIZE - 3; y++) for (int x = 2; x <= MAP_SIZE - 3; x++) {
                                    if (VISUAL_MAP[y][x] == 12 || VISUAL_MAP[y][x] == 1) continue;
                                    int d = (x - rx) * (x - rx) + (y - ry) * (y - ry);
                                    if (d < best_d) { best_d = d; bx = x; by = y; }
                                }
                                reenter_x = FxI(bx); reenter_y = FxI(by);
                            }
                        }
                        player_x = reenter_x; player_y = reenter_y;
                    }
                }
            }
            
            cam_x = FxTrunc((player_x - player_y) * (TILE_WIDTH / 2)); cam_y = FxTrunc((player_x + player_y) * (TILE_HEIGHT / 2));
            fx view_zoom = UpdateAfkCamera(AnyPlayerInput() || MouseBusy() || is_menu_open || vendor_menu_open || town_menu ||
                                              is_character_creation || sleep_fade_frame > 0 || inside_home >= 0);
                view_zoom += DoorwayCamera();
        SoundUpdate();
        LockOnUpdate();
            HDC hdc = GetDC(hwnd); HDC memHDC = CreateCompatibleDC(hdc); HBITMAP memBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT); SelectObject(memHDC, memBitmap);
            
            if (is_character_creation) {
                RECT r = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT}; FillRect(memHDC, &r, creation_bg);
                SetTextColor(memHDC, RGB(0, 255, 230)); SetBkColor(memHDC, RGB(15, 18, 24)); SetBkMode(memHDC, TRANSPARENT);
                HFONT underline_font = GetMenuUnderlineFont();
                HGDIOBJ prev_cc_font = SelectObject(memHDC, underline_font);
                TextOut(memHDC, 80, 70, "HERO CHARACTER CREATION", 24);
                SelectObject(memHDC, prev_cc_font);

                // Version + build time, bottom-right of the start screen only.
                char ver[96]; RECT cr; SIZE vs;
                sprintf(ver, "Version %s  |  Updated %s %s", GAME_VERSION, __DATE__, __TIME__);
                GetClientRect(hwnd, &cr);
                GetTextExtentPoint32(memHDC, ver, (int)strlen(ver), &vs);
                SetTextColor(memHDC, RGB(150, 160, 170));
                TextOut(memHDC, cr.right - vs.cx - 20, cr.bottom - vs.cy - 16, ver, (int)strlen(ver));
                SetTextColor(memHDC, RGB(0, 255, 230));

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
                if (RaceClassSynergyStat() >= 0) {
                    static const char *syn_names[STAT_COUNT] = { "STR", "DEX", "VIT", "MAG", "LCK", "INT", "CHR" };
                    SetTextColor(memHDC, RGB(255, 215, 0));
                    sprintf(label, "Natural fit! +1 %s", syn_names[RaceClassSynergyStat()]);
                    TextOut(memHDC, 120, 242, label, (int)strlen(label));
                }

                {
                    const char *look_lbl[6] = { "Body", "Beard", "Eyes", "Hair", "Clothes", "Trim" };
                    const char *look_val[6] = { sex_names[hero_sex], beard_names[hero_beard], eye_names[hero_eyes], hair_names[hero_hair], cloth_names[hero_cloth], trim_names[hero_trim] };
                    for (int i = 0; i < 6; i++) {
                        SetTextColor(memHDC, (selected_creation_field == 3 + i) ? RGB(255, 255, 0) : RGB(200, 200, 200));
                        sprintf(label, "[%d] %s: < %s >", 4 + i, look_lbl[i], look_val[i]); TextOut(memHDC, 700, 140 + i * 30, label, (int)strlen(label));
                    }
                    SetTextColor(memHDC, RGB(150, 160, 170));
                    strcpy(label, "Your race sets your height. Looks can only be changed here.");
                    TextOut(memHDC, 700, 330, label, (int)strlen(label));
                }
                DrawHeroAssetEx(memHDC, 520, 200, FxI(2));

                prev_cc_font = SelectObject(memHDC, underline_font);
                SetTextColor(memHDC, RGB(0, 230, 200));
                TextOut(memHDC, 80, 272, "ATTRIBUTES", 10);
                SelectObject(memHDC, prev_cc_font);

                DrawStatJewels(memHDC, 80, 292, "STR", stat_strength);
                DrawStatJewels(memHDC, 80, 306, "DEX", stat_dexterity);
                DrawStatJewels(memHDC, 80, 320, "VIT", stat_stamina);
                DrawStatJewels(memHDC, 80, 334, "MAG", stat_magick);
                DrawStatJewels(memHDC, 80, 348, "LCK", stat_luck);
                DrawStatJewels(memHDC, 80, 362, "INT", stat_intelligence);
                DrawStatJewels(memHDC, 80, 376, "CHR", stat_charisma);

                SetTextColor(memHDC, RGB(255, 255, 255));
                TextOut(memHDC, 80, 410, "Use DPAD/ARROW KEYS to select fields; TYPE A-Z to edit the Name.", 65);
                TextOut(memHDC, 80, 430, "PRESS [ENTER] or [A BUTTON] TO ENTER THE WORLD.", 48);
            } else {
                fx ambient_intensity = FxAbs(FxSin(day_night_cycle_accumulator));
                if (NightVisionActive() && ambient_intensity < FX(0.6)) ambient_intensity = FX(0.6);
                // Dark vision: Dwarves and Gnomes, born underground, see further at night and in caves.
                if ((selected_race == RACE_DWARF || selected_race == RACE_GNOME) && ambient_intensity < FX(0.4)) ambient_intensity = FX(0.4);
                int r_tone = FxTrunc(FxI(25) + ambient_intensity * 80);
                int g_tone = FxTrunc(FxI(30) + ambient_intensity * 155);
                int b_tone = FxTrunc(FxI(40) + ambient_intensity * 45);
                
                COLORREF meadow_color = RGB(r_tone, g_tone, b_tone);
                if (current_biome != BIOME_CAVE) meadow_color = ApplySeasonToGround(meadow_color);
                if (current_biome == BIOME_DESERT) // pure yellow sand, dimmed at night
                    meadow_color = RGB(70 + FxTrunc(ambient_intensity * 180), 60 + FxTrunc(ambient_intensity * 150), 15 + FxTrunc(ambient_intensity * 40));
                HBRUSH live_ground = CreateSolidBrush(meadow_color); HGDIOBJ old_bg = SelectObject(memHDC, live_ground); PatBlt(memHDC, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, PATCOPY); SelectObject(memHDC, old_bg); DeleteObject(live_ground);
                SelectObject(memHDC, clean_null_pen);

                for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
                    if (VISUAL_MAP[r][c] == 10 && current_biome != BIOME_CAVE) { // sand patches lie flat, drawn with the ground
                        int sx, sy; GetIsoCoords(FxI(c), FxI(r), &sx, &sy);
                        POINT top[] = {{sx, sy}, {sx + TILE_WIDTH / 2, sy + TILE_HEIGHT / 2}, {sx, sy + TILE_HEIGHT}, {sx - TILE_WIDTH / 2, sy + TILE_HEIGHT / 2}};
                        HBRUSH b = CreateSolidBrush(GetTileColor(10, 0)); HGDIOBJ ob = SelectObject(memHDC, b); Polygon(memHDC, top, 4); SelectObject(memHDC, ob); DeleteObject(b);
                    }
                    if (VISUAL_MAP[r][c] == 12) {
                        int sx, sy; GetIsoCoords(FxI(c), FxI(r), &sx, &sy);
                        if (sx >= -TILE_WIDTH && sx <= WINDOW_WIDTH + TILE_WIDTH) { DrawFlowingRiver(memHDC, sx, sy, r * MAP_SIZE + c); DrawShoreFoam(memHDC, c, r, sx, sy); }
                    }
                }
                DrawSeasonalGroundCover(memHDC);
                DrawFootpaths(memHDC);
                DrawFarmPlots(memHDC);
                DrawCanopyShadows(memHDC);
                for (int r = 0; r < MAP_SIZE; r++) {
                    for (int c = 0; c < MAP_SIZE; c++) {
                        int sx, sy; GetIsoCoords(FxI(c), FxI(r), &sx, &sy); if (sx < -TILE_WIDTH * 2 || sx > WINDOW_WIDTH + TILE_WIDTH * 2) continue;
                        uint8_t t_id = VISUAL_MAP[r][c];
                        if (t_id == 1 || (t_id == 10 && current_biome == BIOME_CAVE)) {
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
                        if (t_id == 11) { DrawLowPolyTree(memHDC, sx, sy, (r * MAP_SIZE + c)); if (IsBigTree(r * MAP_SIZE + c)) DrawBigCrown(memHDC, sx, sy, r * MAP_SIZE + c); }
                        if (t_id >= 2 && t_id <= 9 && t_id != 1) DrawGroundSceneryDecals(memHDC, sx, sy, t_id, (r * MAP_SIZE + c));
                        if (current_biome == BIOME_MOUNTAIN && c == torch_gate_x && r == torch_gate_y) {
                            DrawInteractiveTorches(memHDC, sx, sy);
                            if (!in_cave && IsNearPoint(FxI(torch_gate_x), FxI(torch_gate_y), FX(2.5))) {
                                SetTextColor(memHDC, RGB(255, 230, 120)); SetBkMode(memHDC, TRANSPARENT);
                                TextOut(memHDC, sx - 16, sy - 46, "CAVE", 4);
                            }
                        }
                        if (in_cave && c == FxTrunc(cave_treasure_x) && r == FxTrunc(cave_treasure_y)) {
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
                        DrawIsleTreasure(memHDC, c, r, sx, sy);
                        if (c == FxTrunc(player_x) && r == FxTrunc(player_y)) DrawHeroAsset(memHDC);
                        DrawEnemiesAtTile(memHDC, c, r);
                    }
                }
                DrawCampfires(memHDC); DrawVillageLights(memHDC); DrawChests(memHDC); DrawCacheDiggers(memHDC); DrawBarrels(memHDC); DrawBedroll(memHDC); DrawGroundLoot(memHDC); DrawDroppedItems(memHDC); DrawDebrisTwigs(memHDC); DrawSummonedZombie(memHDC);
                DrawFlyingArrow(memHDC); DrawFlyingSpear(memHDC); DrawFlyingSpell(memHDC); DrawClassAbilityFX(memHDC); DrawButterflies(memHDC); DrawBloodMistFX(memHDC);
                DrawEnvironmentalCritters(memHDC); DrawFamiliar(memHDC); DrawFishing(memHDC); DrawMerchantStoreFront(memHDC); DrawVillage(memHDC); DrawNightDarkness(memHDC); DrawNightEyes(memHDC);
                DrawLockOnMarker(memHDC); DrawWeatherEffects(memHDC); DrawSolitaireSunMoonBeam(memHDC);
                ApplyCameraZoom(memHDC, view_zoom);
                DrawAfkView(memHDC);
                DrawHomeInterior(memHDC);
                DrawPeyoteMelt(memHDC);
                if (!is_menu_open) DrawPlayerStatusHUD(memHDC); // HP, MP and stamina stay on screen, AFK view included
        if (!AfkViewActive() && !is_menu_open) { DrawLootPreview(memHDC); DrawBelt(memHDC); DrawQuickWheel(memHDC); }

                if (is_menu_open) DrawTabbedMenuOverlay(memHDC);
                DrawStrategyGuideFlash(memHDC);

                SetTextColor(memHDC, RGB(255, 255, 255)); SetBkMode(memHDC, TRANSPARENT);
                MsgLogUpdate();
                if (!AfkViewActive() && !is_menu_open) DrawMsgLog(memHDC, 20, HUD_LOG_Y);
                if (!AfkViewActive() && !is_menu_open) DrawTownBanner(memHDC);
                if (vendor_menu_open && IsNearMerchant()) {
                    DrawVendorShopOverlay(memHDC);
                }
                DrawTownMenu(memHDC); DrawBlessingFx(memHDC);
                DrawSleepFade(memHDC);
            }
            PresentFrame(hwnd, hdc, memHDC); DeleteObject(memBitmap); DeleteDC(memHDC); ReleaseDC(hwnd, hdc); Sleep(16);
        }
    }
    DeleteObject(grass_brush); DeleteObject(creation_bg); DeleteObject(clean_null_pen); return 0;
}
