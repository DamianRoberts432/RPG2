#include "game.h"

void DrawDroppedItems(HDC hdc) {
    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
        DroppedInventoryItem *drop = &dropped_items[i];
        if (!drop->active || drop->screen_id != current_screen_index) continue;
        DWORD age = GetTickCount() - drop->dropped_at;
        if (!drop->legendary && age > DROPPED_ITEM_LIFETIME_MS - 30000 && (world_frame / 8) % 2) continue;
        int sx, sy; GetIsoCoords(drop->x, drop->y, &sx, &sy);
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
        for (int i = 0; i < 32; i++) sway_lut[i] = (int)(sin(i * 6.2831853f / 32.0f) * 3.0f);
        sway_initialized = 1;
    }
    int wind_strength = abs(wind_dir);
    int sway_phase = ((int)(world_tick * 2.0f) + tile_seed) & 31;
    sx += sway_lut[sway_phase] * (3 + wind_strength) / 3;
    POINT trunk[] = {{sx - 4, sy + 10}, {sx + 4, sy + 10}, {sx + 4, sy + 24}, {sx - 4, sy + 24}};
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
    } else if (type == 4) { // Desert/Beach sand dune ripple
        HBRUSH sand = CreateSolidBrush(RGB(225, 200, 150)); HGDIOBJ old = SelectObject(hdc, sand);
        RECT dune = { sx - 7, sy + 4, sx + 7, sy + 7 };
        FillRect(hdc, &dune, sand); SelectObject(hdc, old); DeleteObject(sand);
    } else if (type == 5) { // Tundra snow mound
        HBRUSH snow = CreateSolidBrush(RGB(235, 240, 250)); HGDIOBJ old = SelectObject(hdc, snow);
        Ellipse(hdc, sx - 6, sy + 1, sx + 6, sy + 9); SelectObject(hdc, old); DeleteObject(snow);
    } else if (type == 6) { // Grasslands tall grass tuft
        HBRUSH tuft = CreateSolidBrush(RGB(150, 210, 90)); HGDIOBJ old = SelectObject(hdc, tuft);
        POINT poly[] = {{sx, sy}, {sx - 3, sy + 9}, {sx + 3, sy + 9}};
        Polygon(hdc, poly, 3); SelectObject(hdc, old); DeleteObject(tuft);
    } else if (type == 7) { // Desert/Oasis cactus
        HBRUSH cactus = CreateSolidBrush(RGB(60, 140, 90)); HGDIOBJ old = SelectObject(hdc, cactus);
        RECT stalk = { sx - 2, sy - 6, sx + 2, sy + 9 }, arm = { sx - 6, sy - 2, sx - 2, sy + 2 };
        FillRect(hdc, &stalk, cactus); FillRect(hdc, &arm, cactus); SelectObject(hdc, old); DeleteObject(cactus);
    } else if (type == 8) { // Swamp/Forest reed
        HBRUSH reed = CreateSolidBrush(RGB(120, 130, 60)); HGDIOBJ old = SelectObject(hdc, reed);
        POINT poly[] = {{sx - 1, sy - 8}, {sx + 1, sy - 8}, {sx + 2, sy + 9}, {sx - 2, sy + 9}};
        Polygon(hdc, poly, 4); SelectObject(hdc, old); DeleteObject(reed);
    }
}

void DrawEnvironmentalCritters(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        if (!critters[i].active) continue;
        if (critters[i].hidden) {
            int tree_x, tree_y; GetIsoCoords(critters[i].x, critters[i].y, &tree_x, &tree_y);
            if (world_frame % 30 == 0) {
                HBRUSH eye = CreateSolidBrush(RGB(245, 235, 190));
                RECT eyes = { tree_x - 4, tree_y - 4, tree_x + 5, tree_y - 1 };
                FillRect(hdc, &eyes, eye); DeleteObject(eye);
            }
            continue;
        }
        int sx, sy; GetIsoCoords(critters[i].x, critters[i].y, &sx, &sy);
        int jump_bob = (critters[i].state == 1) ? (int)(sin(world_tick * 8.0f) * 4.0f) : 0;
        if (critters[i].type == 0) {
            HBRUSH bird_b = CreateSolidBrush(RGB(50, 160, 240)); HGDIOBJ old = SelectObject(hdc, bird_b);
            int flap = (int)(sin(world_tick * 12.0f + i) * 3.0f);
            POINT wing[] = {{sx - 6, sy - 4 - flap}, {sx, sy + jump_bob}, {sx + 6, sy - 4 - flap}, {sx, sy - 2 + jump_bob}};
            Polygon(hdc, wing, 4); SelectObject(hdc, old); DeleteObject(bird_b);
        } else {
            HBRUSH sq_b = CreateSolidBrush(RGB(210, 105, 30)); HGDIOBJ old = SelectObject(hdc, sq_b);
            RECT body = { sx - 4, sy - 4 - jump_bob, sx + 4, sy + 2 - jump_bob }; FillRect(hdc, &body, sq_b);
            POINT tail[] = {{sx - 4, sy - jump_bob}, {sx - 10, sy - 8 - jump_bob}, {sx - 6, sy - jump_bob}};
            Polygon(hdc, tail, 3); SelectObject(hdc, old); DeleteObject(sq_b);
        }
    }
}

void DrawSolitaireSunMoonBeam(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;

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

void DrawGroundLoot(HDC hdc) {
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        GroundLoot *loot = &ground_loot[i];
        if (!loot->active || loot->screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords(loot->x, loot->y, &sx, &sy);
        HBRUSH b = CreateSolidBrush(loot->item_id == LOOT_APPLE ? RGB(210, 45, 35) :
            loot->item_id == LOOT_FISH ? RGB(80, 175, 220) : RGB(205, 205, 215));
        HGDIOBJ old = SelectObject(hdc, b);
        if (loot->item_id == LOOT_APPLE || loot->item_id == LOOT_FISH) Ellipse(hdc, sx - 4, sy - 7, sx + 4, sy + 1);
        else { MoveToEx(hdc, sx - 7, sy + 2, NULL); LineTo(hdc, sx + 7, sy - 5); Rectangle(hdc, sx - 9, sy - 1, sx - 4, sy + 5); }
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
    if (sleep_fade_frame > 0 && sleep_fade_frame < 30) {
        SetTextColor(hdc, RGB(255, 255, 255)); SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, sx - 6, sy - 28, "Zzz", 3);
    }
}

int RainHash(int value) {
    uint32_t hash = (uint32_t)value;
    hash = (hash ^ (hash >> 13)) * 1274126177u;
    return (int)((hash ^ (hash >> 16)) & 0x7fffffffu);
}

void DrawRainZones(HDC hdc) {
    if (current_weather != WEATHER_RAIN || weather_intensity < 0.02f) return;
    HPEN rain = CreatePen(PS_SOLID, 1, RGB(155, 190, 220)); HGDIOBJ old = SelectObject(hdc, rain);
    int phase = (int)(world_tick / 8.0f);
    const int PERIOD_X = 200, PERIOD_Y = 150;
    // Rain cells are keyed off absolute world-space pixel coordinates (offset
    // by the camera, same as GetIsoCoords) instead of raw screen pixels, so
    // the storm stays fixed over the ground and scrolls past as the camera
    // moves, rather than following the character around like a filter glued
    // to the screen.
    int base_cell_x = (int)floor((double)cam_x / PERIOD_X) - 1;
    int base_cell_y = (int)floor((double)cam_y / PERIOD_Y) - 1;
    int cols = WINDOW_WIDTH / PERIOD_X + 3;
    int rows = WINDOW_HEIGHT / PERIOD_Y + 3;
    // Weather is world-wide: rain covers the whole view, its density set by
    // the global weather_intensity rather than by which screen you are on.
    int drops = 3 + (int)(11.0f * weather_intensity);
    for (int zy = 0; zy < rows; zy++) for (int zx = 0; zx < cols; zx++) {
        int cell_x = base_cell_x + zx, cell_y = base_cell_y + zy;
        int seed = phase + cell_x * 31 + cell_y * 67;
        int screen_ox = cell_x * PERIOD_X - cam_x + (WINDOW_WIDTH / 2) + wind_dir * 12;
        int screen_oy = cell_y * PERIOD_Y - cam_y + (WINDOW_HEIGHT / 2);
        for (int n = 0; n < drops; n++) {
            int h = RainHash(seed + n * 17);
            int x = screen_ox + h % 190, y = screen_oy + (h / 191) % 140;
            MoveToEx(hdc, x, y, NULL); LineTo(hdc, x - wind_dir, y + 9);
        }
    }
    SelectObject(hdc, old); DeleteObject(rain);
}

void DrawSleepFade(HDC hdc) {
    if (sleep_fade_frame <= 0) return;
    int coverage = sleep_fade_frame <= 30 ? sleep_fade_frame * 100 / 30 : (60 - sleep_fade_frame) * 100 / 30;
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        if (RainHash(x * 31 + y * 67) % 100 < coverage) {
            RECT cell = { x * 100, y * 75, (x + 1) * 100, (y + 1) * 75 };
            FillRect(hdc, &cell, black);
        }
    }
    DeleteObject(black);
}
