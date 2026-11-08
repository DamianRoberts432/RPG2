#include "game.h"

void DrawDroppedItems(HDC hdc) {
    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
        DroppedInventoryItem *drop = &dropped_items[i];
        if (!drop->active || drop->screen_id != current_screen_index) continue;
        DWORD age = GetTickCount() - drop->dropped_at;
        if (!drop->legendary && age > DROPPED_ITEM_LIFETIME_MS - 30000 && (world_frame / 8) % 2) continue;
        int sx, sy; GetIsoCoords(drop->x, drop->y, &sx, &sy);
        if (strcmp(drop->item.name, "Spear") == 0) {
            HPEN shaft = CreatePen(PS_SOLID, 2, RGB(150, 110, 70)); HGDIOBJ old_s = SelectObject(hdc, shaft);
            MoveToEx(hdc, sx - 14, sy + 3, NULL); LineTo(hdc, sx + 12, sy - 3);
            HPEN tip = CreatePen(PS_SOLID, 3, RGB(210, 215, 225)); SelectObject(hdc, tip);
            MoveToEx(hdc, sx + 12, sy - 3, NULL); LineTo(hdc, sx + 17, sy - 4);
            SelectObject(hdc, old_s); DeleteObject(shaft); DeleteObject(tip);
            continue;
        }
        HBRUSH b = CreateSolidBrush(RGB(170, 150, 110)); HGDIOBJ old = SelectObject(hdc, b);
        POINT pack[] = { {sx - 6, sy - 4}, {sx + 6, sy - 4}, {sx + 7, sy + 5}, {sx - 7, sy + 5} };
        Polygon(hdc, pack, 4); SelectObject(hdc, old); DeleteObject(b);
    }
}

static int FloorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

void GetIsoCoords(fx cx, fx cy, int *sx, int *sy) {
    int raw_x = FxTrunc((cx - cy) * (TILE_WIDTH / 2));
    int raw_y = FxTrunc((cx + cy) * (TILE_HEIGHT / 2));
    *sx = raw_x - cam_x + (WINDOW_WIDTH / 2);
    *sy = raw_y - cam_y + (WINDOW_HEIGHT / 2) - HillAt(cx, cy);
}

void TileCorners(int c, int r, POINT q[4]) {
    static const int dc[4] = { 0, 1, 1, 0 }, dr[4] = { 0, 0, 1, 1 };
    for (int k = 0; k < 4; k++) { int x, y; GetIsoCoords(FxI(c + dc[k]), FxI(r + dr[k]), &x, &y); q[k].x = x; q[k].y = y; }
}

// Rolling ground: every tile with a raised corner is redrawn lifted, a little
// brighter on slopes facing the top of the screen and darker on the far side.
void DrawHillGround(HDC hdc, COLORREF base) {
    if (!HillsActive()) return;
    HBRUSH br[9];
    for (int k = 0; k < 9; k++) {
        int m = 256 + (k - 4) * 11, rr = GetRValue(base) * m / 256, gg = GetGValue(base) * m / 256, bb = GetBValue(base) * m / 256;
        br[k] = CreateSolidBrush(RGB(rr > 255 ? 255 : rr, gg > 255 ? 255 : gg, bb > 255 ? 255 : bb));
    }
    HGDIOBJ old = SelectObject(hdc, br[4]);
    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
        int a = HillCorner(c, r), b = HillCorner(c + 1, r), d = HillCorner(c + 1, r + 1), e = HillCorner(c, r + 1);
        if ((a | b | d | e) == 0 || VISUAL_MAP[r][c] == 12) continue;
        POINT q[4]; TileCorners(c, r, q);
        if (q[1].x < -TILE_WIDTH || q[3].x > WINDOW_WIDTH + TILE_WIDTH || q[2].y < -TILE_HEIGHT || q[0].y > WINDOW_HEIGHT + TILE_HEIGHT) continue;
        int gx = (b + d) - (a + e), gy = (e + d) - (a + b), lvl = 4 - (2 * gx + gy) / 2;
        SelectObject(hdc, br[lvl < 0 ? 0 : lvl > 8 ? 8 : lvl]);
        Polygon(hdc, q, 4);
    }
    SelectObject(hdc, old);
    for (int k = 0; k < 9; k++) DeleteObject(br[k]);
}

COLORREF GetTileColor(uint8_t id, int side) {
    if (id == 1)  return side ? RGB(80, 85, 95)    : RGB(110, 115, 125); 
    if (id == 10) return side ? RGB(215, 160, 40)  : RGB(255, 205, 70);
    if (id == 12) return side ? RGB(45, 135, 215)  : RGB(56, 176, 248);
    return side ? RGB(85, 155, 70)   : RGB(105, 185, 85);
}

// Gentle water: two slow sine swells crossing at different angles and speeds.
// Each tile is tinted a touch lighter or darker by the swell under it and a
// faint crest rides the high points. The peyote mirage reuses WaveAt.
fx WaveAt(fx x, fx y, fx t) {
    int64_t a = ((int64_t)FxBits(x) * FX(0.9) + (int64_t)FxBits(y) * FX(0.4) + (int64_t)FxBits(t) * FX(1.3)) >> 16;
    int64_t b = (-(int64_t)FxBits(x) * FX(0.35) + (int64_t)FxBits(y) * FX(1.1) + (int64_t)FxBits(t) * FX(0.8)) >> 16;
    return FxSinTurn(FxTurn64(a)) / 2 + FxSinTurn(FxTurn64(b)) / 2;
}

void DrawFlowingRiver(HDC hdc, int sx, int sy, int tile_seed) {
    fx t = (fx)((int64_t)GetTickCount() * FX_ONE / 1000);
    int c = tile_seed % MAP_SIZE, r = tile_seed / MAP_SIZE, k = FxTrunc(WaveAt(FxI(c), FxI(r), t) * 10);
    HBRUSH b = CreateSolidBrush(RGB(40 + k, 140 + k, 235 + k / 2)); HGDIOBJ old = SelectObject(hdc, b);
    POINT water[] = { {sx, sy}, {sx + TILE_WIDTH / 2, sy + TILE_HEIGHT / 2}, {sx, sy + TILE_HEIGHT}, {sx - TILE_WIDTH / 2, sy + TILE_HEIGHT / 2} };
    Polygon(hdc, water, 4);
    SelectObject(hdc, old); DeleteObject(b);
    if (k >= 6) { // soft crest, just a shade lighter than the water
        int dy = FxTrunc(FxSinTurn(FxTurn64((((int64_t)FxBits(t) * FX(1.1)) >> 16) + c * FX(0.7))) * 3);
        HPEN crest = CreatePen(PS_SOLID, 1, RGB(85, 175, 245)); HGDIOBJ prev = SelectObject(hdc, crest);
        POINT arc[3] = { { sx - 10, sy + 17 + dy }, { sx, sy + 14 + dy }, { sx + 10, sy + 17 + dy } };
        Polyline(hdc, arc, 3);
        SelectObject(hdc, prev); DeleteObject(crest);
    }
    if ((tile_seed + current_screen_index * 17) % 3 == 0) {
        HBRUSH fish = CreateSolidBrush(RGB(230, 220, 150)); HGDIOBJ prev = SelectObject(hdc, fish);
        POINT shape[] = { {sx + 3, sy + 13}, {sx + 10, sy + 10}, {sx + 10, sy + 16} };
        Polygon(hdc, shape, 3); SelectObject(hdc, prev); DeleteObject(fish);
    }
}

// Soft foam along every edge where water meets land, easing in and out with the swell.
void DrawShoreFoam(HDC hdc, int c, int r, int sx, int sy) {
    int in = 3 + FxTrunc(FxMul(FX(1.5), FxSinTurn(FxTurn64((int64_t)GetTickCount() * FX_ONE / 700 + c * FX(0.8) + r * FX(0.5)))));
    POINT top = { sx, sy + in }, right = { sx + TILE_WIDTH / 2 - in * 2, sy + TILE_HEIGHT / 2 };
    POINT bottom = { sx, sy + TILE_HEIGHT - in }, left = { sx - TILE_WIDTH / 2 + in * 2, sy + TILE_HEIGHT / 2 };
    struct { int dc, dr; POINT a, b; } edge[4] = { { 0, -1, top, right }, { -1, 0, left, top }, { 0, 1, bottom, left }, { 1, 0, right, bottom } };
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(150, 205, 245)); HGDIOBJ old = SelectObject(hdc, pen);
    for (int i = 0; i < 4; i++) {
        int nc = c + edge[i].dc, nr = r + edge[i].dr;
        if (nc < 0 || nr < 0 || nc >= MAP_SIZE || nr >= MAP_SIZE || VISUAL_MAP[nr][nc] == 12) continue;
        MoveToEx(hdc, edge[i].a.x, edge[i].a.y, NULL); LineTo(hdc, edge[i].b.x, edge[i].b.y);
    }
    SelectObject(hdc, old); DeleteObject(pen);
}

void InjectDynamicGlowPass(HDC hdc, int hx, int hy, int radius, COLORREF light_color) {
    HBRUSH glow_brush = CreateSolidBrush(light_color); HGDIOBJ old_brush = SelectObject(hdc, glow_brush);
    HRGN glow_region = CreateEllipticRgn(hx - radius, hy - radius / 2, hx + radius, hy + radius / 2);
    FrameRgn(hdc, glow_region, glow_brush, 2, 1);
    DeleteObject(glow_region); SelectObject(hdc, old_brush); DeleteObject(glow_brush);
}

void DrawInteractiveTorches(HDC hdc, int sx, int sy) {
    for (int side = -1; side <= 1; side += 2) {
        int tsx = sx + (side * 28); int tsy = sy + 4;
        RECT iron_bracket = { tsx - 3, tsy + 2, tsx + 3, tsy + 16 };
        HBRUSH iron_b = CreateSolidBrush(RGB(45, 48, 55)); FillRect(hdc, &iron_bracket, iron_b); DeleteObject(iron_b);
        int flicker = FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * 14 + FxI(side))) * 3);
        HBRUSH fire_b = CreateSolidBrush(RGB(255, 130 + flicker * 20, 20));
        RECT fire_core = { tsx - 4, tsy - 6 + flicker, tsx + 4, tsy + 4 };
        FillRect(hdc, &fire_core, fire_b); DeleteObject(fire_b);
        InjectDynamicGlowPass(hdc, tsx, tsy, 24 + flicker, RGB(255, 160, 40));
    }
}

void DrawCampfires(HDC hdc) {
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        int csx, csy; GetIsoCoords(campfires[i].x, campfires[i].y, &csx, &csy);
        
        HBRUSH log_b = CreateSolidBrush(RGB(110, 65, 35)); HGDIOBJ old = SelectObject(hdc, log_b);
        RECT log1 = { csx - 8, csy + 2, csx + 8, csy + 6 };
        RECT log2 = { csx - 4, csy - 2, csx + 4, csy + 8 };
        FillRect(hdc, &log1, log_b); FillRect(hdc, &log2, log_b);
        SelectObject(hdc, old); DeleteObject(log_b);

        int flick = FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * 16 + FxI(i))) * 4);
        HBRUSH fire_b = CreateSolidBrush(RGB(255, 140 + flick * 15, 30)); HGDIOBJ prev_fire = SelectObject(hdc, fire_b);
        POINT flame[] = {{csx, csy - 14 + flick}, {csx + 6, csy + 2}, {csx - 6, csy + 2}};
        Polygon(hdc, flame, 3);
        SelectObject(hdc, prev_fire); DeleteObject(fire_b);

        InjectDynamicGlowPass(hdc, csx, csy, 50 + flick * 2, RGB(255, 180, 50));
        if (IsNearPoint(campfires[i].x, campfires[i].y, FxI(2))) {
            char label[40];
            sprintf(label, "Wood %d/%d  [X] add", CampfireWoodCount(i), CAMPFIRE_MAX_WOOD);
            SetTextColor(hdc, RGB(255, 225, 150)); SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, csx - 55, csy - 36, label, (int)strlen(label));
        }
    }
}

static void DrawCanopyBlob(HDC hdc, int cx, int cy, int r, COLORREF color) {
    int h = r * 4 / 5;
    POINT blob[] = {{cx - r, cy}, {cx - r / 2, cy - h}, {cx + r / 2, cy - h}, {cx + r, cy}, {cx + r / 2, cy + h}, {cx - r / 2, cy + h}};
    HBRUSH b = CreateSolidBrush(color); HGDIOBJ old = SelectObject(hdc, b);
    Polygon(hdc, blob, 6); SelectObject(hdc, old); DeleteObject(b);
}

// Broadleaf trees follow the seasons: green in spring/summer, orange, red or
// gold in fall, and bare branches in winter. Apple trees are among these.
static void DrawDeciduousCanopy(HDC hdc, int sx, int sy, int tile_seed) {
    static const COLORREF spring[3] = { RGB(85, 160, 60), RGB(110, 185, 75), RGB(140, 205, 95) };
    static const COLORREF summer[3] = { RGB(55, 125, 45), RGB(75, 150, 60), RGB(95, 180, 75) };
    static const COLORREF fall[3][3] = {
        { RGB(190, 95, 30), RGB(215, 125, 40), RGB(240, 160, 60) },
        { RGB(150, 45, 30), RGB(180, 65, 40), RGB(210, 95, 50) },
        { RGB(190, 150, 35), RGB(215, 180, 55), RGB(235, 205, 85) } };
    if (current_season == SEASON_WINTER) {
        HPEN branch = CreatePen(PS_SOLID, 2, RGB(110, 80, 60)); HGDIOBJ old_p = SelectObject(hdc, branch);
        MoveToEx(hdc, sx, sy + 10, NULL); LineTo(hdc, sx, sy - 12);
        MoveToEx(hdc, sx, sy + 2, NULL); LineTo(hdc, sx - 11, sy - 8);
        MoveToEx(hdc, sx, sy - 2, NULL); LineTo(hdc, sx + 11, sy - 12);
        MoveToEx(hdc, sx, sy - 6, NULL); LineTo(hdc, sx - 5, sy - 17);
        SelectObject(hdc, old_p); DeleteObject(branch);
        if (snow_cover > FX(0.25)) {
            HPEN snow = CreatePen(PS_SOLID, 2, RGB(240, 244, 250)); old_p = SelectObject(hdc, snow);
            MoveToEx(hdc, sx - 9, sy - 8, NULL); LineTo(hdc, sx - 4, sy - 4);
            MoveToEx(hdc, sx + 4, sy - 7, NULL); LineTo(hdc, sx + 10, sy - 12);
            SelectObject(hdc, old_p); DeleteObject(snow);
        }
        return;
    }
    const COLORREF *c = current_season == SEASON_SPRING ? spring : current_season == SEASON_SUMMER ? summer : fall[(tile_seed / 3) % 3];
    DrawCanopyBlob(hdc, sx, sy - 12, 12, c[0]);
    DrawCanopyBlob(hdc, sx - 9, sy - 2, 11, c[1]);
    DrawCanopyBlob(hdc, sx + 9, sy - 1, 11, c[1]);
    DrawCanopyBlob(hdc, sx + 1, sy - 8, 9, c[2]);
}

// A few trees (many in forests) are tall old giants: a high crown over the
// normal canopy. When the hero walks just behind one its leaves turn lacy so
// you can still see yourself and anything attacking you.
int IsBigTree(int tile_seed) {
    if (in_cave) return 0;
    int h = abs(RainHash(tile_seed * 7919 + 17)) % 100;
    if (current_biome == BIOME_FOREST) return h < 22;
    return (current_biome == BIOME_GRASSLANDS || current_biome == BIOME_SWAMP || current_biome == BIOME_OASIS) && h < 6;
}

void DrawBigCrown(HDC hdc, int sx, int sy, int tile_seed) {
    int r = tile_seed / MAP_SIZE, c = tile_seed % MAP_SIZE, broad = IsDeciduousTree(tile_seed);
    int sway = FxTrunc(FxSinTurn(FxTurn64((((int64_t)FxBits(world_tick) * FX(0.9)) >> 16) + (int64_t)tile_seed * FX_ONE)) * (2 + abs(wind_dir)));
    COLORREF leaf = snow_cover > FX(0.5) ? RGB(200, 210, 222) : !broad ? RGB(30, 95, 45) : current_season == SEASON_FALL ? RGB(190, 105, 40) : RGB(55, 120, 45);
    COLORREF dark = RGB(GetRValue(leaf) * 3 / 4, GetGValue(leaf) * 3 / 4, GetBValue(leaf) * 3 / 4);
    HBRUSH bark = CreateSolidBrush(RGB(115, 82, 60)); HGDIOBJ ob = SelectObject(hdc, bark), op = SelectObject(hdc, GetStockObject(NULL_PEN));
    POINT trunk[] = { {sx - 2 + sway / 2, sy - 30}, {sx + 2 + sway / 2, sy - 30}, {sx + 3, sy - 8}, {sx - 3, sy - 8} };
    Polygon(hdc, trunk, 4);
    fx depth = FxI(c + r) - (player_x + player_y), across = FxI(c - r) - (player_x - player_y);
    int lacy = depth > FX(0.3) && depth < FxI(5) && across > -FX(2.5) && across < FX(2.5);
    HBRUSH lb = lacy ? CreateHatchBrush(HS_DIAGCROSS, leaf) : CreateSolidBrush(leaf), db = lacy ? CreateHatchBrush(HS_DIAGCROSS, dark) : CreateSolidBrush(dark);
    int old_mode = SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, db);
    Ellipse(hdc, sx - 26 + sway, sy - 44, sx + 2 + sway, sy - 20); Ellipse(hdc, sx - 2 + sway, sy - 46, sx + 26 + sway, sy - 22);
    SelectObject(hdc, lb);
    Ellipse(hdc, sx - 16 + sway, sy - 58, sx + 16 + sway, sy - 32);
    SetBkMode(hdc, old_mode);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(bark); DeleteObject(lb); DeleteObject(db);
}

void DrawLowPolyTree(HDC hdc, int sx, int sy, int tile_seed) {
    static int sway_lut[32], sway_initialized = 0;
    if (!sway_initialized) {
        for (int i = 0; i < 32; i++) sway_lut[i] = FxTrunc(FxSinTurn((uint32_t)i << 27) * 2);
        sway_initialized = 1;
    }
    int wind_strength = abs(wind_dir);
    int sway_phase = (FxMulI(world_tick, FxI(2)) + tile_seed) & 31;
    int root_x = sx; // the trunk stays rooted; only the canopy sways
    sx += sway_lut[sway_phase] * (3 + wind_strength) / 3;
    int lean = (sx - root_x) / 2;
    POINT trunk[] = {{root_x - 4 + lean, sy + 10}, {root_x + 4 + lean, sy + 10}, {root_x + 4, sy + 24}, {root_x - 4, sy + 24}};
    HBRUSH b = CreateSolidBrush(RGB(130, 95, 70)); HGDIOBJ old = SelectObject(hdc, b); Polygon(hdc, trunk, 4); SelectObject(hdc, old); DeleteObject(b);
    
    int deciduous = IsDeciduousTree(tile_seed);
    if (deciduous) {
        DrawDeciduousCanopy(hdc, sx, sy, tile_seed);
    } else if (tile_seed % 2 == 0) {
        int heights[] = {12, 4, -4}, widths[] = {20, 16, 12};
        COLORREF shades[] = {RGB(55, 125, 45), RGB(75, 150, 60), RGB(95, 180, 75)};
        for (int i = 0; i < 3; i++) {
            POINT cap[] = {{sx, sy + heights[i] - 12}, {sx + widths[i], sy + heights[i] + 4}, {sx, sy + heights[i] + 8}, {sx - widths[i], sy + heights[i] + 4}};
            b = CreateSolidBrush(shades[i]); HGDIOBJ prev = SelectObject(hdc, b); Polygon(hdc, cap, 4); SelectObject(hdc, prev); DeleteObject(b);
        }
    } else {
        // "Virtua Racing" style saplings start out noticeably smaller than
        // the full 3-polygon trees, then gradually grow into that full-size
        // 3-polygon shape once enough world time has passed on this tile.
        fx growth_threshold = FxI(180) + FxI(tile_seed % 90);
        if (world_tick >= growth_threshold) {
            int heights[] = {12, 4, -4}, widths[] = {20, 16, 12};
            COLORREF shades[] = {RGB(55, 125, 45), RGB(75, 150, 60), RGB(95, 180, 75)};
            for (int i = 0; i < 3; i++) {
                POINT cap[] = {{sx, sy + heights[i] - 12}, {sx + widths[i], sy + heights[i] + 4}, {sx, sy + heights[i] + 8}, {sx - widths[i], sy + heights[i] + 4}};
                b = CreateSolidBrush(shades[i]); HGDIOBJ prev = SelectObject(hdc, b); Polygon(hdc, cap, 4); SelectObject(hdc, prev); DeleteObject(b);
            }
        } else {
            fx grow_scale = FX(0.5) + FxMul(FX(0.4), FxDiv(world_tick, growth_threshold));
            int ax = sx, ay = sy + 12; // ground-anchor point so growth stretches upward, not downward through the soil
            POINT raw_left[] = {{sx, sy - 18}, {sx - 18, sy + 6}, {sx, sy + 12}};
            POINT raw_right[] = {{sx, sy - 18}, {sx, sy + 12}, {sx + 18, sy + 6}};
            POINT left_facet[3], right_facet[3];
            for (int i = 0; i < 3; i++) {
                left_facet[i].x = ax + (LONG)FxTrunc((raw_left[i].x - ax) * grow_scale);
                left_facet[i].y = ay + (LONG)FxTrunc((raw_left[i].y - ay) * grow_scale);
                right_facet[i].x = ax + (LONG)FxTrunc((raw_right[i].x - ax) * grow_scale);
                right_facet[i].y = ay + (LONG)FxTrunc((raw_right[i].y - ay) * grow_scale);
            }
            HBRUSH f1 = CreateSolidBrush(RGB(45, 145, 75)); HGDIOBJ prev1 = SelectObject(hdc, f1);
            Polygon(hdc, left_facet, 3); SelectObject(hdc, prev1); DeleteObject(f1);
            HBRUSH f2 = CreateSolidBrush(RGB(65, 175, 95)); HGDIOBJ prev2 = SelectObject(hdc, f2);
            Polygon(hdc, right_facet, 3); SelectObject(hdc, prev2); DeleteObject(f2);
        }
    }
    if (!deciduous && snow_cover > FX(0.25) && (tile_seed % 2 == 0 || world_tick >= FxI(180) + FxI(tile_seed % 90))) {
        HBRUSH snow_b = CreateSolidBrush(RGB(240, 244, 250)); HGDIOBJ prev_snow = SelectObject(hdc, snow_b);
        POINT cap[] = {{sx, sy - 16}, {sx + 8, sy - 5}, {sx, sy - 2}, {sx - 8, sy - 5}};
        Polygon(hdc, cap, 4); SelectObject(hdc, prev_snow); DeleteObject(snow_b);
    }
    if (TreeHasApples(tile_seed)) {
        HBRUSH apple = CreateSolidBrush(RGB(210, 45, 35)); HGDIOBJ old_apple = SelectObject(hdc, apple);
        Ellipse(hdc, sx - 10, sy - 8, sx - 5, sy - 3); Ellipse(hdc, sx + 5, sy - 4, sx + 10, sy + 1);
        SelectObject(hdc, old_apple); DeleteObject(apple);
    }
    SelectObject(hdc, old);
}

static void DrawRockCone(HDC hdc, int bx, int by, int w, int h) {
    int base_h = w / 2 > 2 ? w / 2 : 2;
    HBRUSH dark = CreateSolidBrush(RGB(70, 72, 80)), light = CreateSolidBrush(RGB(110, 114, 125));
    HGDIOBJ old = SelectObject(hdc, dark);
    Ellipse(hdc, bx - w, by - base_h, bx + w, by + base_h);
    SelectObject(hdc, light);
    Pie(hdc, bx - w, by - base_h, bx + w, by + base_h, bx, by + base_h, bx, by - base_h);
    SelectObject(hdc, dark);
    POINT left[] = {{bx, by - h}, {bx - w, by}, {bx, by + base_h}};
    Polygon(hdc, left, 3);
    SelectObject(hdc, light);
    POINT right[] = {{bx, by - h}, {bx, by + base_h}, {bx + w, by}};
    Polygon(hdc, right, 3);
    SelectObject(hdc, old); DeleteObject(dark); DeleteObject(light);
    HPEN shine = CreatePen(PS_SOLID, 1, RGB(160, 165, 178)); HGDIOBJ old_p = SelectObject(hdc, shine);
    MoveToEx(hdc, bx + 1, by - h + 3, NULL); LineTo(hdc, bx + w / 2, by - 1);
    SelectObject(hdc, old_p); DeleteObject(shine);
}

// Cave formations: clusters of tall, tapering stalagmite cones.
void DrawSubterraneanGeology(HDC hdc, int sx, int sy, int tile_seed) {
    int h = 28 + (tile_seed % 22), w = 8 + (tile_seed % 5);
    if (tile_seed % 4 == 0) DrawRockCone(hdc, sx - 12, sy + 2, w / 2 + 2, h / 2);
    DrawRockCone(hdc, sx, sy + 6, w, h);
    if (tile_seed % 3 != 0) DrawRockCone(hdc, sx + 11, sy + 11, w / 2 + 2, h * 11 / 20);
}

void DrawGroundSceneryDecals(HDC hdc, int sx, int sy, int type, int seed) {
    if (type == 2) { 
        HBRUSH petal = CreateSolidBrush((seed % 2 == 0) ? RGB(245, 90, 120) : RGB(235, 220, 60));
        HGDIOBJ old = SelectObject(hdc, petal);
        RECT h = { sx - 4, sy + 2, sx + 4, sy + 6 }, v = { sx - 1, sy, sx + 2, sy + 8 };
        FillRect(hdc, &h, petal); FillRect(hdc, &v, petal);
        SelectObject(hdc, old); DeleteObject(petal);
    } else if (type == 3) { 
        HBRUSH stone = CreateSolidBrush(RGB(100, 105, 115)); HGDIOBJ old = SelectObject(hdc, stone);
        POINT poly[] = {{sx - 6, sy + 2}, {sx + 8, sy}, {sx + 4, sy + 8}, {sx - 4, sy + 6}};
        Polygon(hdc, poly, 4); SelectObject(hdc, old); DeleteObject(stone);
        unsigned int v = (unsigned int)RainHash(seed * 13 + 5) % 3u;
        if (v == 0) { // mossy stone
            HBRUSH moss = CreateSolidBrush(RGB(85, 125, 60)); HGDIOBJ om = SelectObject(hdc, moss);
            POINT m[] = {{sx - 5, sy + 2}, {sx + 3, sy + 1}, {sx, sy + 4}, {sx - 3, sy + 4}};
            Polygon(hdc, m, 4); SelectObject(hdc, om); DeleteObject(moss);
        } else if (v == 1) { // cracked stone
            HPEN crack = CreatePen(PS_SOLID, 1, RGB(50, 52, 60)); HGDIOBJ oc = SelectObject(hdc, crack);
            MoveToEx(hdc, sx + 2, sy + 1, NULL); LineTo(hdc, sx, sy + 4); LineTo(hdc, sx + 3, sy + 7);
            SelectObject(hdc, oc); DeleteObject(crack);
        }
    } else if (type == 4) { // sand: dark specks and wind lines over the dunes
        unsigned int h = (unsigned int)RainHash(seed * 7 + 3);
        for (int k = 0; k < 3; k++)
            SetPixel(hdc, sx - 12 + (int)((h >> (k * 4)) % 24u), sy + 2 + (int)((h >> (k * 3 + 8)) % 12u), RGB(95, 80, 40));
        if (h % 3 == 0) {
            HPEN p = CreatePen(PS_SOLID, 1, RGB(170, 140, 60)); HGDIOBJ op = SelectObject(hdc, p);
            MoveToEx(hdc, sx - 10, sy + 6, NULL); LineTo(hdc, sx - 2, sy + 4); LineTo(hdc, sx + 9, sy + 7);
            SelectObject(hdc, op); DeleteObject(p);
        }
    } else if (type == 5) { // Tundra snow mound
        HBRUSH snow = CreateSolidBrush(RGB(235, 240, 250)); HGDIOBJ old = SelectObject(hdc, snow);
        Ellipse(hdc, sx - 6, sy + 1, sx + 6, sy + 9); SelectObject(hdc, old); DeleteObject(snow);
    } else if (type == 6) { // Grasslands tall grass tuft: single blades that lean a little in the wind
        if ((unsigned int)RainHash(seed * 17 + 9) % 5u == 0) { // wild weeds: broad leaves and a seed head
            HBRUSH leaf = CreateSolidBrush(RGB(90, 135, 50)); HGDIOBJ ol = SelectObject(hdc, leaf);
            POINT l1[] = {{sx, sy + 9}, {sx - 7, sy + 3}, {sx - 2, sy + 6}}, l2[] = {{sx, sy + 9}, {sx + 7, sy + 2}, {sx + 2, sy + 6}};
            POINT l3[] = {{sx, sy + 9}, {sx - 1, sy - 2}, {sx + 2, sy + 5}};
            Polygon(hdc, l1, 3); Polygon(hdc, l2, 3); Polygon(hdc, l3, 3);
            SelectObject(hdc, ol); DeleteObject(leaf);
            SetPixel(hdc, sx - 1, sy - 3, RGB(205, 190, 120)); SetPixel(hdc, sx, sy - 4, RGB(205, 190, 120));
            return;
        }
        HPEN blade = CreatePen(PS_SOLID, 1, RGB(150, 210, 90)); HGDIOBJ old = SelectObject(hdc, blade);
        int lean = FxTrunc(FxMul(FxSinTurn(FxTurn64((((int64_t)FxBits(world_tick) * FX(1.5)) >> 16) + sx * FX(0.1))), FX(1.5)));
        int hx, hy; GetIsoCoords(player_x, player_y, &hx, &hy);
        if (abs(sx - hx) < 22 && abs(sy - hy) < 16) lean += sx >= hx ? 4 : -4; // pushed aside as the hero walks through
        static const int base_dx[5] = { -3, -1, 0, 2, 3 }, tip_dx[5] = { -5, -2, 0, 2, 5 }, height[5] = { 6, 9, 10, 8, 6 };
        for (int b = 0; b < 5; b++) { MoveToEx(hdc, sx + base_dx[b], sy + 9, NULL); LineTo(hdc, sx + tip_dx[b] + lean, sy + 9 - height[b]); }
        SelectObject(hdc, old); DeleteObject(blade);
    } else if (type == 7) { // Desert/Oasis saguaro cactus
        if (IsPeyoteSeed(seed)) { // small round peyote button; split in two after two arrows
            HBRUSH g = CreateSolidBrush(RGB(110, 150, 120)); HGDIOBJ og = SelectObject(hdc, g);
            if (PeyoteBroken(seed)) { Ellipse(hdc, sx - 9, sy + 3, sx - 1, sy + 9); Ellipse(hdc, sx + 1, sy + 3, sx + 9, sy + 9); }
            else Ellipse(hdc, sx - 7, sy + 1, sx + 7, sy + 9);
            SelectObject(hdc, og); DeleteObject(g);
            if (!PeyoteBroken(seed)) { HBRUSH f = CreateSolidBrush(RGB(245, 120, 190)); RECT fr = { sx - 2, sy - 1, sx + 2, sy + 3 }; FillRect(hdc, &fr, f); DeleteObject(f); }
            return;
        }
        HBRUSH cactus = CreateSolidBrush(RGB(60, 140, 90)); HGDIOBJ old = SelectObject(hdc, cactus);
        HPEN edge = CreatePen(PS_SOLID, 1, RGB(35, 95, 60)); HGDIOBJ old_p = SelectObject(hdc, edge);
        RoundRect(hdc, sx - 4, sy - 26, sx + 4, sy + 9, 8, 8);
        RoundRect(hdc, sx - 12, sy - 6, sx - 3, sy - 1, 4, 4); RoundRect(hdc, sx - 12, sy - 18, sx - 7, sy - 3, 5, 5);
        RoundRect(hdc, sx + 3, sy - 11, sx + 11, sy - 6, 4, 4); RoundRect(hdc, sx + 7, sy - 22, sx + 12, sy - 8, 5, 5);
        SelectObject(hdc, old_p); DeleteObject(edge); SelectObject(hdc, old); DeleteObject(cactus);
    } else if (type == 8) { // Swamp/Forest reed
        HBRUSH reed = CreateSolidBrush(RGB(120, 130, 60)); HGDIOBJ old = SelectObject(hdc, reed);
        POINT poly[] = {{sx - 1, sy - 8}, {sx + 1, sy - 8}, {sx + 2, sy + 9}, {sx - 2, sy + 9}};
        Polygon(hdc, poly, 4); SelectObject(hdc, old); DeleteObject(reed);
    }
}

void DrawEnvironmentalCritters(HDC hdc) {
    for (int i = 0; i < MAX_CRITTERS; i++) {
        if (!critters[i].active) continue;
        if (current_biome == BIOME_CAVE && critters[i].type != 3) continue;
        if (critters[i].hidden) {
            if (CritterAsleep(&critters[i])) continue; // sleeping animals stay out of sight
            int tree_x, tree_y; GetIsoCoords(critters[i].x, critters[i].y, &tree_x, &tree_y);
            if (world_frame % 30 == 0) {
                HBRUSH eye = CreateSolidBrush(RGB(245, 235, 190));
                RECT el = { tree_x - 3, tree_y - 4, tree_x - 1, tree_y - 2 }, er = { tree_x + 1, tree_y - 4, tree_x + 3, tree_y - 2 };
                FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
            }
            continue;
        }
        int sx, sy; GetIsoCoords(critters[i].x, critters[i].y, &sx, &sy);
        int jump_bob = (critters[i].state == 1) ? FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * (6 + 2 * (i % 3)) + i * FX(1.9))) * 4) : 0; // three hop timings
        if (critters[i].type == 0) { // bluebird: hops on the ground, flaps when flying
            int t = critters[i].timer, flying = critters[i].state == 2;
            int lift = flying ? 22 : (t <= 8 ? FxTrunc(FxSin(t * FX_PI / 8) * 6) : 0);
            int by = sy - 4 - lift;
            int dir = flying ? (critters[i].base_x >= critters[i].x ? 1 : -1) : ((i & 1) ? 1 : -1);
            HGDIOBJ old_p = SelectObject(hdc, GetStockObject(NULL_PEN));
            HBRUSH shadow = CreateSolidBrush(RGB(35, 35, 35)), body = CreateSolidBrush(RGB(55, 115, 215));
            HBRUSH belly = CreateSolidBrush(RGB(235, 140, 70)), beak = CreateSolidBrush(RGB(245, 200, 60));
            HGDIOBJ old = SelectObject(hdc, shadow);
            Ellipse(hdc, sx - 4, sy - 1, sx + 5, sy + 2);
            SelectObject(hdc, body);
            Ellipse(hdc, sx - 5, by - 4, sx + 5, by + 3);
            Ellipse(hdc, sx + dir * 3 - 3, by - 8, sx + dir * 3 + 4, by - 2);
            POINT tail[3] = { { sx - dir * 4, by - 1 }, { sx - dir * 9, by - 4 }, { sx - dir * 9, by + 1 } };
            Polygon(hdc, tail, 3);
            SelectObject(hdc, belly);
            Ellipse(hdc, sx - 2 + dir, by - 1, sx + 3 + dir, by + 3);
            SelectObject(hdc, beak);
            POINT bk[3] = { { sx + dir * 6, by - 6 }, { sx + dir * 9, by - 5 }, { sx + dir * 6, by - 4 } };
            Polygon(hdc, bk, 3);
            SelectObject(hdc, old); SelectObject(hdc, old_p);
            DeleteObject(shadow); DeleteObject(body); DeleteObject(belly); DeleteObject(beak);
            SetPixel(hdc, sx + dir * 4, by - 6, RGB(10, 10, 10));
            HPEN lp = CreatePen(PS_SOLID, flying ? 2 : 1, flying ? RGB(40, 90, 190) : RGB(90, 70, 50));
            old_p = SelectObject(hdc, lp);
            if (flying) {
                int flap = FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * 12 + FxI(i))) * 6);
                MoveToEx(hdc, sx - 9, by - 2 - flap, NULL); LineTo(hdc, sx, by - 2); LineTo(hdc, sx + 9, by - 2 - flap);
            } else {
                MoveToEx(hdc, sx - 1, by + 3, NULL); LineTo(hdc, sx - 1, by + 6);
                MoveToEx(hdc, sx + 2, by + 3, NULL); LineTo(hdc, sx + 2, by + 6);
            }
            SelectObject(hdc, old_p); DeleteObject(lp);
        } else if (critters[i].type == 2) { // owl: perched up in a canopy, wings out while flying between trees
            if (current_biome != BIOME_CAVE) sy -= 38;
            if (critters[i].state) {
                int flap = (world_frame / 6) % 2 ? -6 : 3;
                HPEN wing = CreatePen(PS_SOLID, 3, RGB(110, 85, 55)); HGDIOBJ ow = SelectObject(hdc, wing);
                MoveToEx(hdc, sx - 4, sy - 5, NULL); LineTo(hdc, sx - 15, sy - 5 + flap);
                MoveToEx(hdc, sx + 4, sy - 5, NULL); LineTo(hdc, sx + 15, sy - 5 + flap);
                SelectObject(hdc, ow); DeleteObject(wing);
            }
            HBRUSH owl_b = CreateSolidBrush(RGB(120, 90, 60)); HGDIOBJ old = SelectObject(hdc, owl_b);
            HPEN owl_p = CreatePen(PS_SOLID, 1, RGB(80, 60, 40)); HGDIOBJ old_p = SelectObject(hdc, owl_p);
            Ellipse(hdc, sx - 5, sy - 10, sx + 5, sy + 3);
            SelectObject(hdc, old); SelectObject(hdc, old_p); DeleteObject(owl_b); DeleteObject(owl_p);
            HBRUSH eye = CreateSolidBrush(RGB(250, 210, 60));
            RECT el = { sx - 4, sy - 7, sx - 1, sy - 4 }, er = { sx + 1, sy - 7, sx + 4, sy - 4 };
            FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
        } else if (critters[i].type == 4) { // tiny desert scorpion: body, pincers and a curled tail
            HBRUSH sc = CreateSolidBrush(RGB(70, 45, 25)); HGDIOBJ os = SelectObject(hdc, sc);
            HPEN sp = CreatePen(PS_SOLID, 1, RGB(50, 30, 15)); HGDIOBJ osp = SelectObject(hdc, sp);
            int step = (world_frame / 5) % 2;
            Ellipse(hdc, sx - 4, sy - 2, sx + 4, sy + 3);
            MoveToEx(hdc, sx - 3, sy, NULL); LineTo(hdc, sx - 7, sy - 2 + step); MoveToEx(hdc, sx + 3, sy, NULL); LineTo(hdc, sx + 7, sy - 2 + step);
            MoveToEx(hdc, sx, sy + 2, NULL); LineTo(hdc, sx + 1, sy + 6); LineTo(hdc, sx + 4, sy + 4); LineTo(hdc, sx + 4, sy + 1);
            SelectObject(hdc, osp); SelectObject(hdc, os); DeleteObject(sp); DeleteObject(sc);
        } else if (critters[i].type == 3) { // bat
            int flap = FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * 18 + FxI(i))) * 4), by = sy - 14 + FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * 3 + FxI(i))) * 3);
            HBRUSH bat_b = CreateSolidBrush(RGB(45, 35, 50)); HGDIOBJ old = SelectObject(hdc, bat_b);
            HPEN bat_p = CreatePen(PS_SOLID, 1, RGB(30, 22, 34)); HGDIOBJ old_p = SelectObject(hdc, bat_p);
            POINT wing[] = {{sx - 9, by - 2 - flap}, {sx - 5, by + 1}, {sx - 2, by - 1}, {sx, by + 2}, {sx + 2, by - 1}, {sx + 5, by + 1}, {sx + 9, by - 2 - flap}, {sx, by - 3}};
            Polygon(hdc, wing, 8); Ellipse(hdc, sx - 2, by - 4, sx + 3, by + 3);
            SelectObject(hdc, old); SelectObject(hdc, old_p); DeleteObject(bat_b); DeleteObject(bat_p);
            HBRUSH eye = CreateSolidBrush(RGB(230, 60, 60)); RECT e = { sx - 1, by - 2, sx + 2, by - 1 }; FillRect(hdc, &e, eye); DeleteObject(eye);
        } else { // squirrel: round body, head with an eye, little hands, bushy tail
            int y0 = sy - jump_bob;
            HBRUSH sq_b = CreateSolidBrush(RGB(210, 105, 30)); HGDIOBJ old = SelectObject(hdc, sq_b);
            HPEN sq_p = CreatePen(PS_SOLID, 1, RGB(150, 70, 20)); HGDIOBJ old_p = SelectObject(hdc, sq_p);
            POINT tail[] = {{sx - 3, y0}, {sx - 9, y0 - 4}, {sx - 10, y0 - 10}, {sx - 6, y0 - 12}, {sx - 5, y0 - 5}};
            Polygon(hdc, tail, 5);
            Ellipse(hdc, sx - 4, y0 - 5, sx + 4, y0 + 2);
            Ellipse(hdc, sx + 1, y0 - 9, sx + 7, y0 - 3);
            MoveToEx(hdc, sx + 3, y0 - 2, NULL); LineTo(hdc, sx + 6, y0 - 1);
            SelectObject(hdc, old); SelectObject(hdc, old_p); DeleteObject(sq_b); DeleteObject(sq_p);
            HBRUSH eye = CreateSolidBrush(RGB(20, 15, 10));
            RECT e = { sx + 4, y0 - 7, sx + 6, y0 - 5 }; FillRect(hdc, &e, eye); DeleteObject(eye);
        }
    }
}

// Night eyes peering out of the tree line once the hero reaches level 5: at
// most a couple of yellow pairs. Red pairs foreshadow what comes after level
// 25 and are rare before it. Pairs stare from one spot for ~14 seconds, blink
// slowly now and then, and never sit right next to the hero.
void DrawNightEyes(HDC hdc) {
    if (player_level < 5 || current_biome == BIOME_CAVE || current_biome == BIOME_CASTLE) return;
    if (FxAbs(FxSin(day_night_cycle_accumulator)) >= FX(0.35)) return;
    int trees[256], n = 0;
    for (int r = 0; r < MAP_SIZE && n < 256; r++) for (int c = 0; c < MAP_SIZE && n < 256; c++) {
        if (VISUAL_MAP[r][c] != 11) continue;
        if (FxAbs(FxI(c) - player_x) + FxAbs(FxI(r) - player_y) < FxI(5)) continue;
        trees[n++] = r * MAP_SIZE + c;
    }
    if (n == 0) return;
    DWORD now = GetTickCount();
    unsigned int h0 = (unsigned int)RainHash(current_screen_index * 977 + (int)(now / 14000));
    int yellow = (int)(h0 % 3);
    int red_chance = (player_level >= 25) ? 30 : 3;
    int red = (int)((h0 >> 8) % 100) < red_chance;
    for (int k = 0; k < yellow + red; k++) {
        unsigned int h = (unsigned int)RainHash((int)h0 + k * 131);
        int ph = (int)((now + h * 37u) % 5200u), open = 3; // slow blink: lids close and reopen over ~0.9 s
        if (ph < 900) open = ph < 450 ? 3 - ph * 3 / 450 : (ph - 450) * 3 / 450;
        if (open <= 0) continue;
        int t = trees[h % (unsigned int)n];
        int sx, sy; GetIsoCoords(FxI(t % MAP_SIZE), FxI(t / MAP_SIZE), &sx, &sy);
        int ex = sx - 6 + (int)(h % 13), ey = sy - 4 - (int)((h >> 5) % 6);
        HBRUSH b = CreateSolidBrush((red && k == yellow) ? RGB(235, 30, 25) : RGB(240, 215, 60));
        RECT left = { ex - 4, ey + 1 - (open + 1) / 2, ex - 1, ey + 1 + open / 2 }, right = { ex + 1, ey + 1 - (open + 1) / 2, ex + 4, ey + 1 + open / 2 };
        FillRect(hdc, &left, b); FillRect(hdc, &right, b); DeleteObject(b);
    }
}

// Tints just the inside of a region at a fixed alpha, blitting only its bounding box.
static void AlphaRegion(HDC hdc, HRGN rg, COLORREF col, int alpha) {
    if (alpha <= 0) return;
    RECT b; GetRgnBox(rg, &b);
    if (b.left < 0) b.left = 0;
    if (b.top < 0) b.top = 0;
    if (b.right > WINDOW_WIDTH) b.right = WINDOW_WIDTH;
    if (b.bottom > WINDOW_HEIGHT) b.bottom = WINDOW_HEIGHT;
    int w = b.right - b.left, h = b.bottom - b.top;
    if (w <= 0 || h <= 0) return;
    HRGN old = CreateRectRgn(0, 0, 0, 0); int had = GetClipRgn(hdc, old);
    SelectClipRgn(hdc, rg);
    HDC m = CreateCompatibleDC(hdc); HBITMAP bm = CreateCompatibleBitmap(hdc, w, h); HGDIOBJ ob = SelectObject(m, bm);
    HBRUSH br = CreateSolidBrush(col); RECT f = { 0, 0, w, h }; FillRect(m, &f, br); DeleteObject(br);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(alpha > 255 ? 255 : alpha), 0 };
    AlphaBlend(hdc, b.left, b.top, w, h, m, 0, 0, w, h, bf);
    SelectObject(m, ob); DeleteObject(bm); DeleteDC(m);
    SelectClipRgn(hdc, had == 1 ? old : NULL); DeleteObject(old);
}

// A soft lens orb: a faint rim with a slightly denser core.
static void OrbAt(HDC hdc, int x, int y, int r, COLORREF col, int alpha) {
    for (int ring = 0; ring < 2; ring++) {
        int rr = ring ? r * 2 / 3 : r;
        HRGN rg = CreateEllipticRgn(x - rr, y - rr, x + rr + 1, y + rr + 1);
        AlphaRegion(hdc, rg, col, alpha / 2 + ring * alpha / 2);
        DeleteObject(rg);
    }
}

void DrawSolitaireSunMoonBeam(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;
    if (current_weather != WEATHER_CLEAR) return; // clouds, rain and snow hide the sun and moon

    fx ambient_intensity = FxAbs(FxSin(day_night_cycle_accumulator));
    int is_night = (ambient_intensity < FX(0.35));
    if (ambient_intensity > FX(0.25) && ambient_intensity < FX(0.45)) return; // dawn and dusk: no beam

    // Rare: only about one screen in five can get a beam, and there only now and
    // then, fading in and out over a few seconds.
    unsigned h = (unsigned)current_screen_index * 2654435761u;
    if ((h >> 13) % 5) return;
    DWORD now = GetTickCount(), cyc = now / 30000, t = now % 30000;
    if (((h >> 7) + cyc * 40503u) % 3 || t > 18000) return;
    fx k = t < 3000 ? FxI(t) / 3000 : t > 15000 ? FxI(18000 - t) / 3000 : FxI(1);

    // Warm sunlight by day, cool moonlight by night.
    COLORREF beam_color = is_night ? RGB(210, 230, 255) : RGB(255, 235, 175);

    // The shaft comes from where the sun or moon is: from the right as it rises,
    // straight down at its highest, from the left as it sets.
    fx s = FxMod(day_night_cycle_accumulator, FX_PI);
    if (s < 0) s += FX_PI;
    s = FxDiv(s, FX_PI);
    fx p = is_night ? FxFrac(s + FX(0.5)) : s, dir = (p - FX(0.5)) * 2;
    if (dir < -FxI(1)) dir = -FxI(1);
    if (dir > FxI(1)) dir = FxI(1);
    fx beam_world_x = FxI(5) + FxI((h >> 3) % (MAP_SIZE - 10)), beam_world_y = FxI(5) + FxI((h >> 9) % (MAP_SIZE - 10));
    fx dist_dx = player_x - beam_world_x, dist_dy = player_y - beam_world_y;
    if (FxMul(dist_dx, dist_dx) + FxMul(dist_dy, dist_dy) < FxI(16)) return;

    int ground_x, ground_y;
    GetIsoCoords(beam_world_x, beam_world_y, &ground_x, &ground_y);
    int top_x = ground_x - FxTrunc(FxMul(dir, FX(0.8)) * ground_y);
    POINT solitaire_poly[] = {
        {top_x - 25, 0},
        {top_x + 25, 0},
        {ground_x + 75, ground_y + 20},
        {ground_x - 75, ground_y + 20}
    };

    // A soft, see-through shaft: three nested trapezoids, each only faintly lit,
    // so the edges fade out instead of ending in a hard "tractor beam" wall.
    for (int layer = 0; layer < 3; layer++) {
        fx w = FxI(1) - layer * FX(0.3);
        POINT q[4] = { { top_x - FxTrunc(22 * w), 0 }, { top_x + FxTrunc(22 * w), 0 },
                       { ground_x + FxTrunc(70 * w), ground_y + 20 }, { ground_x - FxTrunc(70 * w), ground_y + 20 } };
        HRGN rg = CreatePolygonRgn(q, 4, WINDING);
        AlphaRegion(hdc, rg, beam_color, FxTrunc(16 * k));
        DeleteObject(rg);
    }
    (void)solitaire_poly;
    // Lens flare: little orbs strung along the line from the light through the
    // screen centre, each with a red and a green fringe like light split by a lens.
    int cxs = WINDOW_WIDTH / 2, cys = WINDOW_HEIGHT / 2;
    static const fx at[5] = { FX(0.35), FX(0.6), FX(0.9), FX(1.25), FX(1.55) };
    static const int rad[5] = { 10, 22, 7, 30, 14 };
    for (int i = 0; i < 5; i++) {
        int ox = top_x + FxTrunc((cxs - top_x) * at[i]), oy = FxTrunc(cys * at[i]), r = rad[i];
        OrbAt(hdc, ox - 2, oy, r + 2, RGB(255, 90, 80), FxTrunc(12 * k));
        OrbAt(hdc, ox + 2, oy, r + 2, RGB(90, 255, 140), FxTrunc(12 * k));
        OrbAt(hdc, ox, oy, r, is_night ? RGB(200, 220, 255) : RGB(255, 240, 200), FxTrunc(20 * k));
        OrbAt(hdc, ox, oy, r / 2, RGB(255, 255, 255), FxTrunc(10 * k));
    }
}

// Loot reads as a treasure pile: three stacks of gold coins with a few
// jewels scattered at the base and an occasional glint.
static void DrawTreasurePile(HDC hdc, int sx, int sy, int seed) {
    HGDIOBJ old_p = SelectObject(hdc, GetStockObject(NULL_PEN));
    HBRUSH shadow = CreateSolidBrush(RGB(25, 30, 25));
    HGDIOBJ old_b = SelectObject(hdc, shadow);
    Ellipse(hdc, sx - 22, sy - 3, sx + 22, sy + 10);
    HBRUSH rim = CreateSolidBrush(RGB(165, 115, 20)), face = CreateSolidBrush(RGB(250, 205, 60));
    static const int stack_x[3] = { -11, 2, 13 }, stack_h[3] = { 5, 8, 3 };
    for (int s = 0; s < 3; s++) for (int k = 0; k < stack_h[s]; k++) {
        int cx = sx + stack_x[s], cy = sy + 3 - k * 3;
        SelectObject(hdc, rim);  Ellipse(hdc, cx - 7, cy - 2, cx + 7, cy + 5);
        SelectObject(hdc, face); Ellipse(hdc, cx - 7, cy - 4, cx + 7, cy + 3);
    }
    static const COLORREF jewel_colors[3] = { RGB(225, 40, 65), RGB(60, 125, 245), RGB(50, 205, 115) };
    static const int jewel_x[3] = { -4, 10, -17 }, jewel_y[3] = { 6, 5, 4 };
    for (int j = 0; j < 3; j++) {
        HBRUSH gem_b = CreateSolidBrush(jewel_colors[j]);
        SelectObject(hdc, gem_b);
        int x = sx + jewel_x[j], y = sy + jewel_y[j];
        POINT gem[] = { {x, y - 5}, {x + 4, y}, {x, y + 4}, {x - 4, y} };
        Polygon(hdc, gem, 4);
        SelectObject(hdc, rim); DeleteObject(gem_b);
    }
    SelectObject(hdc, old_b); SelectObject(hdc, old_p);
    DeleteObject(shadow); DeleteObject(rim); DeleteObject(face);
    if (((world_frame / 10) + seed) % 6 == 0) {
        HPEN glint = CreatePen(PS_SOLID, 1, RGB(255, 255, 225));
        HGDIOBJ old_g = SelectObject(hdc, glint);
        int gx = sx + 4, gy = sy - 22;
        MoveToEx(hdc, gx - 4, gy, NULL); LineTo(hdc, gx + 5, gy);
        MoveToEx(hdc, gx, gy - 4, NULL); LineTo(hdc, gx, gy + 5);
        SelectObject(hdc, old_g); DeleteObject(glint);
    }
}

void DrawGroundLoot(HDC hdc) {
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        GroundLoot *loot = &ground_loot[i];
        if (!loot->active || loot->screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords(loot->x, loot->y, &sx, &sy);
        if (loot->item_id != LOOT_APPLE && loot->item_id != LOOT_FISH) { DrawTreasurePile(hdc, sx, sy, i); continue; }
        HBRUSH b = CreateSolidBrush(loot->item_id == LOOT_APPLE ? RGB(210, 45, 35) : RGB(80, 175, 220));
        HGDIOBJ old = SelectObject(hdc, b);
        Ellipse(hdc, sx - 6, sy - 10, sx + 6, sy + 2);
        SelectObject(hdc, old); DeleteObject(b);
    }
}

void DrawBedroll(HDC hdc) {
    if (bedroll_screen_id != current_screen_index) return;
    int sx, sy; GetIsoCoords(bedroll_x, bedroll_y, &sx, &sy);
    HBRUSH roll = CreateSolidBrush(RGB(135, 90, 65)); HGDIOBJ old = SelectObject(hdc, roll);
    // Enlarged ~1.5x over the original 12x10 diamond so multiple characters
    // can visibly share the same bedroll (future-proofing for multiplayer).
    POINT mattress[] = { {sx - 18, sy}, {sx, sy - 8}, {sx + 18, sy}, {sx, sy + 8} };
    Polygon(hdc, mattress, 4); SelectObject(hdc, old); DeleteObject(roll);
    HBRUSH pillow = CreateSolidBrush(RGB(235, 230, 215)); old = SelectObject(hdc, pillow); // pillow at the head (top) end
    Ellipse(hdc, sx - 7, sy - 8, sx + 7, sy - 2); SelectObject(hdc, old); DeleteObject(pillow);
    if (sleep_fade_frame > 0 && sleep_fade_frame < 30) {
        SetTextColor(hdc, RGB(255, 255, 255)); SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, sx - 6, sy - 28, "Zzz", 3);
    }
}

// Very slight zoom-in of the gameplay view (HUD is drawn afterwards, unscaled).
static const fx zoom_levels[3] = { FX(1.08), FX(1.25), FX(1.42) }; // far (default), closer, closest
static int zoom_index = 0;
#define CAMERA_ZOOM zoom_levels[zoom_index]
static fx afk_amount; // 0..1 while the AFK camera has eased in
static DWORD afk_idle_ms;
int AfkViewMs(void) { return afk_amount > FxI(0) ? (int)afk_idle_ms : -1; }

// V / right-stick click steps through the three zoom levels.
void CycleCameraZoom(void) {
    static const char *names[3] = { "Far", "Closer", "Closest" };
    zoom_index = (zoom_index + 1) % 3;
    sprintf(arpg_action_log, "VIEW: %s zoom.", names[zoom_index]);
}
void ApplyCameraZoom(HDC hdc, fx zoom) {
    HDC copy_dc = CreateCompatibleDC(hdc);
    HBITMAP copy_bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    HGDIOBJ old_bmp = SelectObject(copy_dc, copy_bmp);
    BitBlt(copy_dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    int src_w = FxTrunc(FxDiv(FxI(WINDOW_WIDTH), zoom)), src_h = FxTrunc(FxDiv(FxI(WINDOW_HEIGHT), zoom));
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, copy_dc,
               (WINDOW_WIDTH - src_w) / 2, (WINDOW_HEIGHT - src_h) / 2, src_w, src_h, SRCCOPY);
    SelectObject(copy_dc, old_bmp); DeleteObject(copy_bmp); DeleteDC(copy_dc);
}

// AFK camera: after AFK_DELAY_MS with no input the view eases into a closer
// zoom into the hero's head, then DrawAfkView (afkview.c) shows the world
// through their eyes; any input snaps it straight back.
// Shifts cam_x/cam_y for this frame and returns the zoom to draw with.
#define AFK_DELAY_MS 45000
#define AFK_EASE_MS 4000
fx UpdateAfkCamera(int input_active) {
    static DWORD last_input = 0;
    DWORD now = GetTickCount();
    if (input_active || last_input == 0) last_input = now;
    DWORD idle = now - last_input;
    if (idle < AFK_DELAY_MS) { afk_amount = FxI(0); return CAMERA_ZOOM; }
    fx t = idle - AFK_DELAY_MS >= AFK_EASE_MS ? FxI(1) : FxI(idle - AFK_DELAY_MS) / AFK_EASE_MS;
    if (t > FxI(1)) t = FxI(1);
    t = FxMul(FxMul(t, t), FxI(3) - 2 * t);
    afk_amount = t > FX(0.001) ? t : FX(0.001);
    afk_idle_ms = idle - AFK_DELAY_MS;
    cam_y -= FxTrunc(24 * t); // close in on the hero's head; DrawAfkView takes over from there
    return CAMERA_ZOOM + FxMul(FX(1.6), t);
}

void FillRectAlpha(HDC hdc, int x, int y, int w, int h, COLORREF color, int alpha) {
    if (alpha <= 0 || w <= 0 || h <= 0) return;
    HDC src = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, 1, 1);
    HGDIOBJ old = SelectObject(src, bmp);
    SetPixel(src, 0, 0, color);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(alpha > 255 ? 255 : alpha), 0 };
    AlphaBlend(hdc, x, y, w, h, src, 0, 0, 1, 1, bf);
    SelectObject(src, old); DeleteObject(bmp); DeleteDC(src);
}

int RainHash(int value) {
    uint32_t hash = (uint32_t)value;
    hash = (hash ^ (hash >> 13)) * 1274126177u;
    return (int)((hash ^ (hash >> 16)) & 0x7fffffffu);
}

// Blends a solid colour over the entire window (alpha 0..255).
void FillWindowAlpha(HDC hdc, COLORREF color, int alpha) {
    if (alpha <= 0) return;
    RECT all = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    HBRUSH b = CreateSolidBrush(color);
    if (alpha >= 255) { FillRect(hdc, &all, b); DeleteObject(b); return; }
    HDC src = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, 1, 1);
    HGDIOBJ old = SelectObject(src, bmp);
    RECT px = { 0, 0, 1, 1 };
    FillRect(src, &px, b);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
    AlphaBlend(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, src, 0, 0, 1, 1, blend);
    SelectObject(src, old); DeleteObject(bmp); DeleteDC(src); DeleteObject(b);
}

void DrawRainZones(HDC hdc) {
    if (current_weather != WEATHER_RAIN || weather_intensity < FX(0.02)) return;
    HPEN rain = CreatePen(PS_SOLID, 1, RGB(155, 190, 220)); HGDIOBJ old = SelectObject(hdc, rain);
    int fall = FxMulI(world_tick, FxI(160)); // drops fall ~8 px every frame
    const int PERIOD_X = 200, PERIOD_Y = 150;
    // Rain cells are keyed off absolute world-space pixel coordinates (offset
    // by the camera, same as GetIsoCoords) instead of raw screen pixels, so
    // the storm stays fixed over the ground and scrolls past as the camera
    // moves, rather than following the character around like a filter glued
    // to the screen.
    FillWindowAlpha(hdc, RGB(70, 85, 105), FxTrunc(45 * weather_intensity));
    // Start one cell past the window's top-left corner (cam_x/cam_y is the
    // window centre) so the drops cover the whole view.
    int base_cell_x = FloorDiv(cam_x - WINDOW_WIDTH / 2, PERIOD_X) - 1;
    int base_cell_y = FloorDiv(cam_y - WINDOW_HEIGHT / 2, PERIOD_Y) - 1;
    int cols = WINDOW_WIDTH / PERIOD_X + 3;
    int rows = WINDOW_HEIGHT / PERIOD_Y + 3;
    // Weather is world-wide: rain covers the whole view, its density set by
    // the global weather_intensity rather than by which screen you are on.
    int drops = 3 + FxTrunc(11 * weather_intensity);
    for (int zy = 0; zy < rows; zy++) for (int zx = 0; zx < cols; zx++) {
        int cell_x = base_cell_x + zx, cell_y = base_cell_y + zy;
        int seed = cell_x * 31 + cell_y * 67;
        int screen_ox = cell_x * PERIOD_X - cam_x + (WINDOW_WIDTH / 2) + wind_dir * 12;
        int screen_oy = cell_y * PERIOD_Y - cam_y + (WINDOW_HEIGHT / 2);
        for (int n = 0; n < drops; n++) {
            int h = RainHash(seed + n * 17);
            int dy = ((h / 191) + fall + n * 37) % 140;
            int x = screen_ox + h % 190 - dy * wind_dir / 6, y = screen_oy + dy;
            MoveToEx(hdc, x, y, NULL); LineTo(hdc, x - wind_dir * 2, y + 10);
        }
    }
    SelectObject(hdc, old); DeleteObject(rain);
}

void DrawSleepFade(HDC hdc) {
    if (sleep_fade_frame <= 0) return;
    int coverage = sleep_fade_frame <= 30 ? sleep_fade_frame * 100 / 30 : (60 - sleep_fade_frame) * 100 / 30;
    FillWindowAlpha(hdc, RGB(0, 0, 0), coverage * 255 / 100);
}

static void DrawFlame(HDC hdc, int cx, int cy, int rx, int ry, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); HGDIOBJ ob = SelectObject(hdc, b), op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, cx - rx, cy - ry, cx + rx, cy + ry);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(b);
}

void DrawVillageLights(HDC hdc) {
    for (int v = 0; v < village_light_count; v++) {
        int sx, sy; GetIsoCoords(village_lights[v].x, village_lights[v].y, &sx, &sy);
        int flick = (FxMulI(world_tick, FxI(10)) + v * 3) % 3;
        if (village_lights[v].torch) {
            HPEN pole = CreatePen(PS_SOLID, 3, RGB(90, 60, 30)); HGDIOBJ op = SelectObject(hdc, pole);
            MoveToEx(hdc, sx, sy + 10, NULL); LineTo(hdc, sx, sy - 28);
            SelectObject(hdc, op); DeleteObject(pole);
            DrawFlame(hdc, sx, sy - 33 - flick, 5, 8, RGB(255, 140, 40));
            DrawFlame(hdc, sx, sy - 32 - flick, 3, 5, RGB(255, 225, 110));
        } else {
            HPEN log = CreatePen(PS_SOLID, 4, RGB(95, 60, 30)); HGDIOBJ op = SelectObject(hdc, log);
            MoveToEx(hdc, sx - 12, sy + 10, NULL); LineTo(hdc, sx + 12, sy + 4);
            MoveToEx(hdc, sx - 12, sy + 4, NULL); LineTo(hdc, sx + 12, sy + 10);
            SelectObject(hdc, op); DeleteObject(log);
            for (int k = 0; k < 6; k++) DrawFlame(hdc, sx - 15 + k * 6, sy + 12 - (k % 2) * 10, 3, 2, RGB(120, 120, 125));
            DrawFlame(hdc, sx, sy - 2 - flick, 9, 13, RGB(255, 120, 30));
            DrawFlame(hdc, sx - 3, sy + flick, 5, 8, RGB(255, 200, 70));
            DrawFlame(hdc, sx + 4, sy + 1 - flick, 4, 6, RGB(255, 230, 120));
        }
    }
}

// How far the hero sees in the dark, in tiles: elves see further, and
// dwarves and gnomes (born underground) further still.
fx RaceVisionTiles(void) {
    if (NightVisionActive()) return FxI(10);
    switch (selected_race) {
    case RACE_ELF: return FX(7.5);
    case RACE_DWARF: case RACE_GNOME: return FX(8.5);
    case RACE_HALFLING: return FxI(6);
    default: return FX(5.5);
    }
}

// Night closes in round the hero: a low-res light map (one texel per 8x8 px)
// darkens everything beyond the hero's sight and warms everything near a
// fire, so trees, the hero, villagers and monsters all catch the firelight.
extern int village_light_count;
void DrawNightDarkness(HDC hdc) {
    fx light = FxAbs(FxSin(day_night_cycle_accumulator));
    fx night = current_biome == BIOME_CAVE ? FxI(1) : FxDiv(FX(0.55) - light, FX(0.35));
    if (night <= FxI(0)) return;
    if (night > FxI(1)) night = FxI(1);
    enum { LW = (WINDOW_WIDTH + 3) / 4, LH = (WINDOW_HEIGHT + 3) / 4, NL = 1 + MAX_CAMPFIRES + MAX_VILLAGE_LIGHTS };
    static HDC ldc; static HBITMAP lbmp; static uint32_t *px;
    if (!ldc) {
        BITMAPINFO bi = { 0 };
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = LW; bi.bmiHeader.biHeight = -LH;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        lbmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
        if (!lbmp) return;
        ldc = CreateCompatibleDC(hdc); SelectObject(ldc, lbmp);
    }
    fx lx[NL], ly[NL], lr[NL]; int fire[NL], n = 0, sx, sy;
    fx flick = FxI(1) + FxMul(FX(0.05), FxSinTurn(FxTurn64((int64_t)FxBits(world_tick) * 15)));
    GetIsoCoords(player_x, player_y, &sx, &sy);
    lx[n] = FxI(sx); ly[n] = FxI(sy + TILE_HEIGHT / 2); lr[n] = RaceVisionTiles(); fire[n++] = 0;
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        GetIsoCoords(campfires[i].x, campfires[i].y, &sx, &sy);
        lx[n] = FxI(sx); ly[n] = FxI(sy + TILE_HEIGHT / 2); lr[n] = FxMul(FX(5.5), flick); fire[n++] = 1;
    }
    for (int i = 0; i < village_light_count && i < MAX_VILLAGE_LIGHTS; i++) {
        GetIsoCoords(village_lights[i].x, village_lights[i].y, &sx, &sy);
        lx[n] = FxI(sx); ly[n] = FxI(sy + TILE_HEIGHT / 2); lr[n] = 4 * flick; fire[n++] = 1;
    }
    const fx tile_px = TILE_WIDTH * FX(0.75);
    fx inv[NL]; // 1 / light radius in pixels, so the per-texel loop only multiplies
    for (int i = 0; i < n; i++) inv[i] = FxDiv(FxI(1), FxMul(lr[i], tile_px));
    for (int y = 0; y < LH; y++) for (int x = 0; x < LW; x++) {
        fx wx = x * FxI(4) + FxI(2), wy = y * FxI(4) + FxI(2), dark = FxI(1), warm = FxI(0);
        for (int i = 0; i < n; i++) {
            fx dx = FxMul(wx - lx[i], inv[i]), dy = FxMul((wy - ly[i]) * 2, inv[i]), d2 = FxMul(dx, dx) + FxMul(dy, dy);
            if (d2 >= FxI(1)) continue;
            fx d = FxSqrt(d2), v = d < FX(0.4) ? FxI(1) : FxI(1) - FxDiv(d - FX(0.4), FX(0.6));
            dark = FxMul(dark, FxI(1) - FxMul(FxMul(v, v), FxI(3) - 2 * v));
            if (fire[i]) warm += FxMul(FxI(1) - d, FxI(1) - d);
        }
        int ad = FxTrunc(FxMul(dark, night) * 165), aw = FxTrunc(FxMul(warm > FxI(1) ? FxI(1) : warm, night) * 80), a = ad + aw;
        if (a > 255) a = 255;
        px[y * LW + x] = ((uint32_t)a << 24) | ((uint32_t)(255 * aw / 255) << 16) | ((uint32_t)(150 * aw / 255) << 8) | (uint32_t)(60 * aw / 255);
    }
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, 0, 0, LW * 4, LH * 4, ldc, 0, 0, LW, LH, bf);
}

// Direction shadows fall, from the sun by day and the moon by night: each light
// rises on the right and sets on the left, so shadows swing right-to-left and
// stretch long when it's low. ox is -1..1 (sideways), oy 0..1 (lengthening).
void SunShadowOffset(fx *ox, fx *oy) {
    fx a = day_night_cycle_accumulator, elev = FxAbs(FxSin(a)), side = FxSin(a) >= 0 ? FxCos(a) : -FxCos(a);
    *ox = -FxMul(side, FxI(1) - FxMul(elev, FX(0.6)));
    *oy = FxI(1) - elev;
}

// Tunic-style canopy shade: every tree drops a soft, lumpy shadow on the ground,
// cast away from the sun or moon. All of it goes on one layer (a copy of the
// frame) blended back at a fixed alpha, so the cost is a single blit whatever
// the tree count - a Saturn would do the same with one half-transparent pass.
void DrawCanopyShadows(HDC hdc) {
    if (current_biome == BIOME_CAVE || in_cave) return;
    fx ox, oy; SunShadowOffset(&ox, &oy);
    fx lit = FxAbs(FxSin(day_night_cycle_accumulator));
    int alpha = lit < FX(0.35) ? 45 : current_weather == WEATHER_CLEAR ? 95 : 55;
    HDC layer = NULL; HBITMAP bmp = NULL; HGDIOBJ old_bmp = NULL, old_br = NULL, old_pen = NULL;
    HBRUSH shade = NULL;
    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
        if (VISUAL_MAP[r][c] != 11) continue;
        int sx, sy; GetIsoCoords(FxI(c), FxI(r), &sx, &sy);
        if (sx < -TILE_WIDTH * 2 || sx > WINDOW_WIDTH + TILE_WIDTH * 2 || sy < -TILE_HEIGHT * 4 || sy > WINDOW_HEIGHT + TILE_HEIGHT * 2) continue;
        if (!layer) {
            layer = CreateCompatibleDC(hdc); bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
            old_bmp = SelectObject(layer, bmp); BitBlt(layer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
            shade = CreateSolidBrush(RGB(16, 28, 22)); old_br = SelectObject(layer, shade); old_pen = SelectObject(layer, GetStockObject(NULL_PEN));
        }
        int seed = r * MAP_SIZE + c, big = IsBigTree(seed), rad = TILE_WIDTH * (big ? 9 : 6) / 10;
        int cx = sx + FxTrunc(FxMul(ox * TILE_WIDTH, FX(0.9))), cy = sy + TILE_HEIGHT / 2 + FxTrunc(FxMul(oy * TILE_HEIGHT, FX(0.6)));
        for (int k = 0; k < 3; k++) { // three overlapping blobs give a leafy, uneven edge
            unsigned h = (unsigned)RainHash(seed * 7 + k);
            int bx = cx + (int)(h % 21) - 10, by = cy + (int)((h >> 5) % 11) - 5, br = rad * (70 + (int)((h >> 9) % 30)) / 100;
            Ellipse(layer, bx - br, by - br / 2, bx + br, by + br / 2);
        }
    }
    if (!layer) return;
    SelectObject(layer, old_pen); SelectObject(layer, old_br); DeleteObject(shade);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
    AlphaBlend(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, layer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, bf);
    SelectObject(layer, old_bmp); DeleteObject(bmp); DeleteDC(layer);
}

// Doorways and archways: walking up to a cottage door, the camera eases in and
// pans ahead along the hero's heading so it sits just behind them, then glides
// back to the normal isometric view once they're through. Returns extra zoom.
fx DoorwayCamera(void) {
    static fx amt = FxI(0);
    fx target = FxI(0);
    if (!AfkViewActive() && !is_menu_open && inside_home < 0 && !in_cave) {
        for (int i = 0; i < 4; i++) {
            fx hx, hy;
            if (!VillageHomeCentre(i, &hx, &hy)) continue;
            fx dx = player_x - hx, dy = player_y - (hy + HOME_HALF_Y), d = FxLen(dx, dy);
            if (d < FX(2.4) && FxI(1) - FxDiv(d, FX(2.4)) > target) target = FxI(1) - FxDiv(d, FX(2.4));
        }
        if (torch_gate_x >= 0) { // cave mouths count as archways too
            fx dx = player_x - (FxI(torch_gate_x) + FX(0.5)), dy = player_y - (FxI(torch_gate_y) + FX(0.5)), d = FxLen(dx, dy);
            if (d < FX(2.4) && FxI(1) - FxDiv(d, FX(2.4)) > target) target = FxI(1) - FxDiv(d, FX(2.4));
        }
    }
    amt += FxMul(target - amt, FX(0.08));
    if (amt < FX(0.002)) { amt = FxI(0); return FxI(0); }
    fx s = FxMul(FxMul(amt, amt), FxI(3) - 2 * amt);
    cam_x += FxTrunc(FxMul(FxMul((facing_x - facing_y) * (TILE_WIDTH / 2), FX(1.2)), s));
    cam_y += FxTrunc(FxMul(FxMul((facing_x + facing_y) * (TILE_HEIGHT / 2), FX(1.2)), s)) - FxTrunc(10 * s);
    return FxMul(FX(0.55), s);
}
