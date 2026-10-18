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

void GetIsoCoords(float cx, float cy, int *sx, int *sy) {
    int raw_x = (int)((cx - cy) * (TILE_WIDTH / 2));
    int raw_y = (int)((cx + cy) * (TILE_HEIGHT / 2));
    *sx = raw_x - cam_x + (WINDOW_WIDTH / 2);
    *sy = raw_y - cam_y + (WINDOW_HEIGHT / 2);
}

COLORREF GetTileColor(uint8_t id, int side) {
    if (id == 1)  return side ? RGB(80, 85, 95)    : RGB(110, 115, 125); 
    if (id == 10) return side ? RGB(215, 160, 40)  : RGB(255, 205, 70);
    if (id == 12) return side ? RGB(45, 135, 215)  : RGB(56, 176, 248);
    return side ? RGB(85, 155, 70)   : RGB(105, 185, 85);
}

void DrawFlowingRiver(HDC hdc, int sx, int sy, int tile_seed) {
    HBRUSH b = CreateSolidBrush(RGB(40, 140, 235)); HGDIOBJ old = SelectObject(hdc, b);
    POINT water[] = { {sx, sy}, {sx + TILE_WIDTH / 2, sy + TILE_HEIGHT / 2}, {sx, sy + TILE_HEIGHT}, {sx - TILE_WIDTH / 2, sy + TILE_HEIGHT / 2} };
    Polygon(hdc, water, 4);
    SelectObject(hdc, old); DeleteObject(b);
    if ((tile_seed + (int)world_tick) % 3 == 0) {
        HPEN shine = CreatePen(PS_SOLID, 1, RGB(130, 215, 255)); HGDIOBJ prev = SelectObject(hdc, shine);
        MoveToEx(hdc, sx - 9, sy + 16, NULL); LineTo(hdc, sx + 8, sy + 16);
        SelectObject(hdc, prev); DeleteObject(shine);
    }
    if ((tile_seed + current_screen_index * 17) % 3 == 0) {
        HBRUSH fish = CreateSolidBrush(RGB(230, 220, 150)); HGDIOBJ prev = SelectObject(hdc, fish);
        POINT shape[] = { {sx + 3, sy + 13}, {sx + 10, sy + 10}, {sx + 10, sy + 16} };
        Polygon(hdc, shape, 3); SelectObject(hdc, prev); DeleteObject(fish);
    }
}

// Two-frame foam along every edge where water meets land.
void DrawShoreFoam(HDC hdc, int c, int r, int sx, int sy) {
    int frame = (GetTickCount() / 450) & 1, in = frame ? 4 : 2;
    POINT top = { sx, sy + in }, right = { sx + TILE_WIDTH / 2 - in * 2, sy + TILE_HEIGHT / 2 };
    POINT bottom = { sx, sy + TILE_HEIGHT - in }, left = { sx - TILE_WIDTH / 2 + in * 2, sy + TILE_HEIGHT / 2 };
    struct { int dc, dr; POINT a, b; } edge[4] = { { 0, -1, top, right }, { -1, 0, left, top }, { 0, 1, bottom, left }, { 1, 0, right, bottom } };
    HPEN pen = CreatePen(frame ? PS_DOT : PS_SOLID, 1, RGB(230, 245, 255)); HGDIOBJ old = SelectObject(hdc, pen);
    int bk = SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < 4; i++) {
        int nc = c + edge[i].dc, nr = r + edge[i].dr;
        if (nc < 0 || nr < 0 || nc >= MAP_SIZE || nr >= MAP_SIZE || VISUAL_MAP[nr][nc] == 12) continue;
        MoveToEx(hdc, edge[i].a.x, edge[i].a.y, NULL); LineTo(hdc, edge[i].b.x, edge[i].b.y);
    }
    SetBkMode(hdc, bk); SelectObject(hdc, old); DeleteObject(pen);
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
        int flicker = (int)(sin(world_tick * 14.0f + side) * 3.0f);
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

        int flick = (int)(sin(world_tick * 16.0f + i) * 4.0f);
        HBRUSH fire_b = CreateSolidBrush(RGB(255, 140 + flick * 15, 30)); HGDIOBJ prev_fire = SelectObject(hdc, fire_b);
        POINT flame[] = {{csx, csy - 14 + flick}, {csx + 6, csy + 2}, {csx - 6, csy + 2}};
        Polygon(hdc, flame, 3);
        SelectObject(hdc, prev_fire); DeleteObject(fire_b);

        InjectDynamicGlowPass(hdc, csx, csy, 50 + flick * 2, RGB(255, 180, 50));
        if (IsNearPoint(campfires[i].x, campfires[i].y, 2.0f)) {
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
        if (snow_cover > 0.25f) {
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

void DrawLowPolyTree(HDC hdc, int sx, int sy, int tile_seed) {
    static int sway_lut[32], sway_initialized = 0;
    if (!sway_initialized) {
        for (int i = 0; i < 32; i++) sway_lut[i] = (int)(sin(i * 6.2831853f / 32.0f) * 2.0f);
        sway_initialized = 1;
    }
    int wind_strength = abs(wind_dir);
    int sway_phase = ((int)(world_tick * 2.0f) + tile_seed) & 31;
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
        float growth_threshold = 180.0f + (float)(tile_seed % 90);
        if (world_tick >= growth_threshold) {
            int heights[] = {12, 4, -4}, widths[] = {20, 16, 12};
            COLORREF shades[] = {RGB(55, 125, 45), RGB(75, 150, 60), RGB(95, 180, 75)};
            for (int i = 0; i < 3; i++) {
                POINT cap[] = {{sx, sy + heights[i] - 12}, {sx + widths[i], sy + heights[i] + 4}, {sx, sy + heights[i] + 8}, {sx - widths[i], sy + heights[i] + 4}};
                b = CreateSolidBrush(shades[i]); HGDIOBJ prev = SelectObject(hdc, b); Polygon(hdc, cap, 4); SelectObject(hdc, prev); DeleteObject(b);
            }
        } else {
            float grow_scale = 0.5f + 0.4f * (world_tick / growth_threshold);
            int ax = sx, ay = sy + 12; // ground-anchor point so growth stretches upward, not downward through the soil
            POINT raw_left[] = {{sx, sy - 18}, {sx - 18, sy + 6}, {sx, sy + 12}};
            POINT raw_right[] = {{sx, sy - 18}, {sx, sy + 12}, {sx + 18, sy + 6}};
            POINT left_facet[3], right_facet[3];
            for (int i = 0; i < 3; i++) {
                left_facet[i].x = ax + (LONG)((raw_left[i].x - ax) * grow_scale);
                left_facet[i].y = ay + (LONG)((raw_left[i].y - ay) * grow_scale);
                right_facet[i].x = ax + (LONG)((raw_right[i].x - ax) * grow_scale);
                right_facet[i].y = ay + (LONG)((raw_right[i].y - ay) * grow_scale);
            }
            HBRUSH f1 = CreateSolidBrush(RGB(45, 145, 75)); HGDIOBJ prev1 = SelectObject(hdc, f1);
            Polygon(hdc, left_facet, 3); SelectObject(hdc, prev1); DeleteObject(f1);
            HBRUSH f2 = CreateSolidBrush(RGB(65, 175, 95)); HGDIOBJ prev2 = SelectObject(hdc, f2);
            Polygon(hdc, right_facet, 3); SelectObject(hdc, prev2); DeleteObject(f2);
        }
    }
    if (!deciduous && snow_cover > 0.25f && (tile_seed % 2 == 0 || world_tick >= 180.0f + (float)(tile_seed % 90))) {
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
        HPEN blade = CreatePen(PS_SOLID, 1, RGB(150, 210, 90)); HGDIOBJ old = SelectObject(hdc, blade);
        int lean = (int)(sin(world_tick * 1.5f + sx * 0.1f) * 1.5f);
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
        int jump_bob = (critters[i].state == 1) ? (int)(sin(world_tick * 8.0f) * 4.0f) : 0;
        if (critters[i].type == 0) { // bluebird: hops on the ground, flaps when flying
            int t = critters[i].timer, flying = critters[i].state == 2;
            int lift = flying ? 22 : (t <= 8 ? (int)(sin(t * 3.14159f / 8.0f) * 6.0f) : 0);
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
                int flap = (int)(sin(world_tick * 12.0f + i) * 6.0f);
                MoveToEx(hdc, sx - 9, by - 2 - flap, NULL); LineTo(hdc, sx, by - 2); LineTo(hdc, sx + 9, by - 2 - flap);
            } else {
                MoveToEx(hdc, sx - 1, by + 3, NULL); LineTo(hdc, sx - 1, by + 6);
                MoveToEx(hdc, sx + 2, by + 3, NULL); LineTo(hdc, sx + 2, by + 6);
            }
            SelectObject(hdc, old_p); DeleteObject(lp);
        } else if (critters[i].type == 2) { // owl
            HBRUSH owl_b = CreateSolidBrush(RGB(120, 90, 60)); HGDIOBJ old = SelectObject(hdc, owl_b);
            HPEN owl_p = CreatePen(PS_SOLID, 1, RGB(80, 60, 40)); HGDIOBJ old_p = SelectObject(hdc, owl_p);
            Ellipse(hdc, sx - 5, sy - 10, sx + 5, sy + 3);
            SelectObject(hdc, old); SelectObject(hdc, old_p); DeleteObject(owl_b); DeleteObject(owl_p);
            HBRUSH eye = CreateSolidBrush(RGB(250, 210, 60));
            RECT el = { sx - 4, sy - 7, sx - 1, sy - 4 }, er = { sx + 1, sy - 7, sx + 4, sy - 4 };
            FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
        } else if (critters[i].type == 3) { // bat
            int flap = (int)(sin(world_tick * 18.0f + i) * 4.0f), by = sy - 14 + (int)(sin(world_tick * 3.0f + i) * 3.0f);
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

// Night eyes peering out of the tree line: at most a couple of yellow pairs.
// Red pairs foreshadow what comes after level 25 and are rare before it.
// Pairs stay put for ~9 seconds, blink now and then, and never sit right
// next to the hero.
void DrawNightEyes(HDC hdc) {
    if (current_biome == BIOME_CAVE || current_biome == BIOME_CASTLE) return;
    if ((float)fabs(sin(day_night_cycle_accumulator)) >= 0.35f) return;
    int trees[256], n = 0;
    for (int r = 0; r < MAP_SIZE && n < 256; r++) for (int c = 0; c < MAP_SIZE && n < 256; c++) {
        if (VISUAL_MAP[r][c] != 11) continue;
        if (fabs(c - player_x) + fabs(r - player_y) < 5.0f) continue;
        trees[n++] = r * MAP_SIZE + c;
    }
    if (n == 0) return;
    DWORD now = GetTickCount();
    unsigned int h0 = (unsigned int)RainHash(current_screen_index * 977 + (int)(now / 9000));
    int yellow = (int)(h0 % 3);
    int red_chance = (player_level >= 25) ? 30 : 3;
    int red = (int)((h0 >> 8) % 100) < red_chance;
    for (int k = 0; k < yellow + red; k++) {
        unsigned int h = (unsigned int)RainHash((int)h0 + k * 131);
        if (((now / 120) + h) % 30 == 0) continue; // brief blink
        int t = trees[h % (unsigned int)n];
        int sx, sy; GetIsoCoords((float)(t % MAP_SIZE), (float)(t / MAP_SIZE), &sx, &sy);
        int ex = sx - 6 + (int)(h % 13), ey = sy - 4 - (int)((h >> 5) % 6);
        HBRUSH b = CreateSolidBrush((red && k == yellow) ? RGB(235, 30, 25) : RGB(240, 215, 60));
        RECT left = { ex - 4, ey, ex - 1, ey + 2 }, right = { ex + 1, ey, ex + 4, ey + 2 };
        FillRect(hdc, &left, b); FillRect(hdc, &right, b); DeleteObject(b);
    }
}

void DrawSolitaireSunMoonBeam(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;
    if (current_weather != WEATHER_CLEAR) return; // clouds, rain and snow hide the sun and moon

    float ambient_intensity = (float)fabs(sin(day_night_cycle_accumulator));
    int is_night = (ambient_intensity < 0.35f);

    // Warm sunlight by day, cool moonlight by night.
    COLORREF beam_color = is_night ? RGB(210, 230, 255) : RGB(255, 235, 175);

    float beam_world_x = 16.0f;
    float beam_world_y = 16.0f;
    float dist_dx = player_x - beam_world_x;
    float dist_dy = player_y - beam_world_y;
    float distance = (float)sqrt(dist_dx * dist_dx + dist_dy * dist_dy);

    if (distance < 4.0f) return;

    int start_x, start_sy;
    GetIsoCoords(beam_world_x, beam_world_y, &start_x, &start_sy);
    start_x -= 80;

    POINT solitaire_poly[] = {
        {start_x, 0},
        {start_x + 50, 0},
        {start_x + 200, WINDOW_HEIGHT},
        {start_x + 90, WINDOW_HEIGHT}
    };

    // True translucency via AlphaBlend: clip the destination to the beam's
    // trapezoid shape using a polygon region, then alpha-blend a solid-color
    // source over just that clipped area. Unlike the previous 1-bit dithered
    // pattern-brush trick (which still read as solid), this lets the scene
    // underneath clearly show through the beam.
    HRGN beam_rgn = CreatePolygonRgn(solitaire_poly, 4, WINDING);
    HRGN old_clip_rgn = CreateRectRgn(0, 0, 0, 0);
    int had_old_clip = GetClipRgn(hdc, old_clip_rgn);
    SelectClipRgn(hdc, beam_rgn);

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    HGDIOBJ old_mem_bmp = SelectObject(mem_dc, mem_bmp);
    HBRUSH fill_b = CreateSolidBrush(beam_color);
    RECT full = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    FillRect(mem_dc, &full, fill_b);
    DeleteObject(fill_b);

    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 105, 0 };
    AlphaBlend(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, mem_dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, blend);

    SelectObject(mem_dc, old_mem_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);

    SelectClipRgn(hdc, had_old_clip == 1 ? old_clip_rgn : NULL);
    DeleteObject(old_clip_rgn);
    DeleteObject(beam_rgn);
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
static const float zoom_levels[3] = { 1.08f, 1.18f, 1.30f }; // far (default), closer, closest
static int zoom_index = 0;
#define CAMERA_ZOOM zoom_levels[zoom_index]
static float afk_amount; // 0..1 while the AFK camera has eased in

// V / right-stick click steps through the three zoom levels.
void CycleCameraZoom(void) {
    static const char *names[3] = { "Far", "Closer", "Closest" };
    zoom_index = (zoom_index + 1) % 3;
    sprintf(arpg_action_log, "VIEW: %s zoom.", names[zoom_index]);
}
void ApplyCameraZoom(HDC hdc, float zoom) {
    HDC copy_dc = CreateCompatibleDC(hdc);
    HBITMAP copy_bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    HGDIOBJ old_bmp = SelectObject(copy_dc, copy_bmp);
    BitBlt(copy_dc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    int src_w = (int)(WINDOW_WIDTH / zoom), src_h = (int)(WINDOW_HEIGHT / zoom);
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, copy_dc,
               (WINDOW_WIDTH - src_w) / 2, (WINDOW_HEIGHT - src_h) / 2, src_w, src_h, SRCCOPY);
    SelectObject(copy_dc, old_bmp); DeleteObject(copy_bmp); DeleteDC(copy_dc);
}

// AFK camera: after AFK_DELAY_MS with no input the view eases into a closer
// zoom and slowly drifts around the hero; any input snaps it straight back.
// Shifts cam_x/cam_y for this frame and returns the zoom to draw with.
#define AFK_DELAY_MS 45000
#define AFK_EASE_MS 10000
float UpdateAfkCamera(int input_active) {
    static DWORD last_input = 0;
    DWORD now = GetTickCount();
    if (input_active || last_input == 0) last_input = now;
    DWORD idle = now - last_input;
    if (idle < AFK_DELAY_MS) { afk_amount = 0.0f; return CAMERA_ZOOM; }
    float t = (float)(idle - AFK_DELAY_MS) / AFK_EASE_MS;
    if (t > 1.0f) t = 1.0f;
    t = t * t * (3.0f - 2.0f * t);
    afk_amount = t;
    float a = (float)(idle - AFK_DELAY_MS) / 12000.0f;
    cam_x += (int)(sinf(a) * 140.0f * t);
    cam_y += (int)(sinf(a * 0.7f) * 70.0f * t);
    return CAMERA_ZOOM + 0.22f * t;
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

// AFK night sky: once the idle camera has drifted in at night, a starry sky
// fades in over the top of the view, with the moon (its phase follows the
// 30-day season) and the odd shooting star.
void DrawAfkNightSky(HDC hdc) {
    if (afk_amount <= 0.0f || in_cave || current_biome == BIOME_CAVE) return;
    if (fabs(sin(day_night_cycle_accumulator)) >= 0.35f) return;
    float k = afk_amount;
    int sky_h = WINDOW_HEIGHT * 2 / 5;
    for (int band = 0; band < 8; band++)
        FillRectAlpha(hdc, 0, band * sky_h / 8, WINDOW_WIDTH, sky_h / 8 + 1, RGB(8, 12, 32), (int)((220 - band * 24) * k));
    DWORD now = GetTickCount();
    for (int i = 0; i < 70; i++) {
        int h = RainHash(i * 7919 + 13);
        int x = h % WINDOW_WIDTH, y = (h / WINDOW_WIDTH) % sky_h, sz = i % 9 == 0 ? 2 : 1;
        int lum = (int)((150.0f + 105.0f * (0.5f + 0.5f * sinf(now / 700.0f + i))) * k * (1.0f - 0.6f * y / sky_h));
        HBRUSH b = CreateSolidBrush(RGB(lum, lum, lum));
        RECT r = { x, y, x + sz, y + sz };
        FillRect(hdc, &r, b); DeleteObject(b);
    }
    float phase = (SeasonDay() % 30) / 30.0f; // 0 = new moon, 0.5 = full
    int mx = WINDOW_WIDTH * 3 / 4, my = sky_h / 3, mr = 16, moon = (int)(230 * k);
    HBRUSH lit = CreateSolidBrush(RGB(moon, moon, moon * 9 / 10)), dark = CreateSolidBrush(RGB(8, 12, 32));
    HGDIOBJ ob = SelectObject(hdc, lit), op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, mx - mr, my - mr, mx + mr, my + mr);
    int off = phase <= 0.5f ? (int)(phase * 4.0f * mr) : -(int)((1.0f - phase) * 4.0f * mr);
    if (off < 2 * mr && off > -2 * mr) { SelectObject(hdc, dark); Ellipse(hdc, mx - mr + off, my - mr, mx + mr + off, my + mr); }
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(lit); DeleteObject(dark);
    DWORD cycle = now / 6000, t = now % 6000;
    if (t < 700) {
        int h = RainHash((int)cycle * 31 + 5);
        float p = t / 700.0f;
        int x1 = h % (WINDOW_WIDTH / 2) + WINDOW_WIDTH / 8 + (int)(p * 220), y1 = (h / 7) % (sky_h / 2) + (int)(p * 80);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB((int)(255 * k), (int)(255 * k), (int)(230 * k)));
        HGDIOBJ old = SelectObject(hdc, pen);
        MoveToEx(hdc, x1 - 34, y1 - 12, NULL); LineTo(hdc, x1, y1);
        SelectObject(hdc, old); DeleteObject(pen);
    }
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
    if (current_weather != WEATHER_RAIN || weather_intensity < 0.02f) return;
    HPEN rain = CreatePen(PS_SOLID, 1, RGB(155, 190, 220)); HGDIOBJ old = SelectObject(hdc, rain);
    int fall = (int)(world_tick * 160.0f); // drops fall ~8 px every frame
    const int PERIOD_X = 200, PERIOD_Y = 150;
    // Rain cells are keyed off absolute world-space pixel coordinates (offset
    // by the camera, same as GetIsoCoords) instead of raw screen pixels, so
    // the storm stays fixed over the ground and scrolls past as the camera
    // moves, rather than following the character around like a filter glued
    // to the screen.
    FillWindowAlpha(hdc, RGB(70, 85, 105), (int)(45.0f * weather_intensity));
    // Start one cell past the window's top-left corner (cam_x/cam_y is the
    // window centre) so the drops cover the whole view.
    int base_cell_x = (int)floor((double)(cam_x - WINDOW_WIDTH / 2) / PERIOD_X) - 1;
    int base_cell_y = (int)floor((double)(cam_y - WINDOW_HEIGHT / 2) / PERIOD_Y) - 1;
    int cols = WINDOW_WIDTH / PERIOD_X + 3;
    int rows = WINDOW_HEIGHT / PERIOD_Y + 3;
    // Weather is world-wide: rain covers the whole view, its density set by
    // the global weather_intensity rather than by which screen you are on.
    int drops = 3 + (int)(11.0f * weather_intensity);
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
        int flick = (int)(world_tick * 10.0f + v * 3) % 3;
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
