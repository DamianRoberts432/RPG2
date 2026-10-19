#include "game.h"

// 1st-person AFK view. After AFK_DELAY_MS with no input the camera zooms into
// the hero's head (UpdateAfkCamera), the screen dips to black, and the world
// is drawn through the hero's eyes: sky, ground, water, trees, rocks, homes,
// the campfire, butterflies, animals, the familiar and the bobber with the
// rod in hand. The gaze drifts from one nearby thing to the next and now and
// then up at the sky. Any input snaps straight back to play.
#define FP_ZOOM_MS 4000
#define FP_DIP_MS 900
#define FP_FOV 1.25f
#define FP_RANGE 13
#define FP_EYE 0.55f
#define FP_MAX_THINGS 240
#define GAZE_MS 6500

enum { FP_TREE, FP_ROCK, FP_HOME, FP_FIRE, FP_BUTTERFLY, FP_CRITTER, FP_FAMILIAR, FP_BOBBER, FP_ENEMY, FP_NPC, FP_BARREL, FP_LIGHT, FP_ALTAR, FP_STALL, FP_TENT };
typedef struct { float x, y, z, depth; int kind, seed; } FpThing;
static FpThing things[FP_MAX_THINGS];
static int thing_count, horizon, bobber_sx = -1, bobber_sy;
static float yaw, pitch, focal;
static float eye_x, eye_y, eye_z = FP_EYE;

int AfkViewActive(void) { return AfkViewMs() >= FP_ZOOM_MS + FP_DIP_MS; }

static float Wrap(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

static COLORREF Dim(COLORREF c, float k) {
    if (k > 1.0f) k = 1.0f;
    return RGB((int)(GetRValue(c) * k), (int)(GetGValue(c) * k), (int)(GetBValue(c) * k));
}

static COLORREF Mix(COLORREF a, COLORREF b, float t) {
    return RGB((int)(GetRValue(a) + ((int)GetRValue(b) - (int)GetRValue(a)) * t), (int)(GetGValue(a) + ((int)GetGValue(b) - (int)GetGValue(a)) * t),
               (int)(GetBValue(a) + ((int)GetBValue(b) - (int)GetBValue(a)) * t));
}

static void Blob(HDC hdc, int x0, int y0, int x1, int y1, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); HGDIOBJ ob = SelectObject(hdc, b), op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, x0, y0, x1 + 1, y1 + 1);
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(b);
}

static void Box(HDC hdc, int x0, int y0, int x1, int y1, COLORREF c) {
    RECT r = { x0, y0, x1 + 1, y1 + 1 }; HBRUSH b = CreateSolidBrush(c); FillRect(hdc, &r, b); DeleteObject(b);
}

static void Shape(HDC hdc, POINT *p, int n, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); HGDIOBJ ob = SelectObject(hdc, b), op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, p, n);
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(b);
}

static void Stroke(HDC hdc, int x0, int y0, int x1, int y1, int w, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, w, c); HGDIOBJ op = SelectObject(hdc, p);
    MoveToEx(hdc, x0, y0, NULL); LineTo(hdc, x1, y1);
    SelectObject(hdc, op); DeleteObject(p);
}

// Projects a world point (tile units, z = height in tiles) to the screen.
static int Project(float x, float y, float z, int *sx, int *sy, float *depth) {
    float dx = x - eye_x, dy = y - eye_y, cs = cosf(yaw), sn = sinf(yaw);
    float f = dx * cs + dy * sn, side = -dx * sn + dy * cs;
    if (f < 0.3f) return 0;
    *sx = WINDOW_WIDTH / 2 + (int)(side / f * focal);
    *sy = horizon + (int)((eye_z - z) / f * focal);
    if (depth) *depth = f;
    return 1;
}

static void AddThing(float x, float y, float z, int kind, int seed) {
    float dx = x - player_x, dy = y - player_y;
    if (dx * dx + dy * dy < 0.25f || dx * dx + dy * dy > FP_RANGE * FP_RANGE || thing_count >= FP_MAX_THINGS) return;
    things[thing_count++] = (FpThing){ x, y, z, 0.0f, kind, seed };
}

static void GatherThings(void) {
    thing_count = 0;
    for (int r = (int)player_y - FP_RANGE; r <= (int)player_y + FP_RANGE; r++)
        for (int c = (int)player_x - FP_RANGE; c <= (int)player_x + FP_RANGE; c++) {
            if (r < 0 || c < 0 || r >= MAP_SIZE || c >= MAP_SIZE) continue;
            if (VISUAL_MAP[r][c] == 11) AddThing(c + 0.5f, r + 0.5f, 0.0f, FP_TREE, r * MAP_SIZE + c);
            else if (VISUAL_MAP[r][c] == 1) AddThing(c + 0.5f, r + 0.5f, 0.0f, FP_ROCK, r * MAP_SIZE + c);
        }
    for (int i = 0; i < 5; i++) { // homes sit where they block the hero
        float hx, hy;
        if (VillageHomeCentre(i, &hx, &hy)) AddThing(hx - HOME_FEET_OFFSET, hy - HOME_FEET_OFFSET, 0.0f, FP_HOME, i);
    }
    for (int i = 0; i < MAX_CAMPFIRES; i++)
        if (campfires[i].active && campfires[i].screen_id == current_screen_index) AddThing(campfires[i].x, campfires[i].y, 0.0f, FP_FIRE, i);
    if (!in_cave && !IsAdverseWeather())
        for (int i = 0; i < MAX_BUTTERFLIES; i++)
            if (butterflies[i].active) AddThing(butterflies[i].x, butterflies[i].y, 0.35f + butterflies[i].z / 40.0f, FP_BUTTERFLY, i);
    for (int i = 0; i < MAX_CRITTERS; i++)
        if (critters[i].active && !critters[i].hidden) AddThing(critters[i].x, critters[i].y, 0.0f, FP_CRITTER, i * 8 + (critters[i].type & 7));
    if (familiar_visible) AddThing(familiar_world_x, familiar_world_y, 0.0f, FP_FAMILIAR, familiar_visible - 1);
    for (int i = 0; i < MAX_ENEMIES; i++) { // the live one is held in enemy_x/enemy_y
        int cur = i == current_enemy;
        float hp = cur ? enemy_hearts : enemies[i].hearts;
        if (hp > 0.0f) AddThing(cur ? enemy_x : enemies[i].x, cur ? enemy_y : enemies[i].y, 0.0f, FP_ENEMY, i * 32 + ((cur ? active_monster : enemies[i].type) & 31));
    }
    for (int i = 0; i < village_npc_count; i++)
        if (village_npcs[i].type != 8) AddThing(village_npcs[i].x, village_npcs[i].y, 0.0f, FP_NPC, i * 16 + (village_npcs[i].type & 15));
        else AddThing(village_npcs[i].x, village_npcs[i].y, 0.0f, FP_ALTAR, TownDeity());
    { // the same town details as in play: barrels, torches and fire pits, the castle stall and tent
        float qx, qy; int intact;
        for (int i = 0; BarrelInfo(i, &qx, &qy, &intact); i++) AddThing(qx + 0.5f, qy + 0.5f, 0.0f, FP_BARREL, i * 2 + intact);
        for (int i = 0; i < village_light_count && i < MAX_VILLAGE_LIGHTS; i++)
            AddThing(village_lights[i].x, village_lights[i].y, 0.0f, FP_LIGHT, i * 2 + (village_lights[i].torch != 0));
        if (current_biome == BIOME_CASTLE) { AddThing(11.0f, 14.0f, 0.0f, FP_STALL, 0); AddThing(merchant_x, merchant_y, 0.0f, FP_TENT, 0); }
    }
    float bx, by; int nibble;
    if (FishingBobberPos(&bx, &by, &nibble)) AddThing(bx, by, 0.0f, FP_BOBBER, nibble);
}

static void SkyPoint(float az, float el, int *x, int *y) {
    float rel = Wrap(az - yaw);
    *x = fabsf(rel) < FP_FOV * 0.7f ? WINDOW_WIDTH / 2 + (int)(tanf(rel) / tanf(FP_FOV / 2) * WINDOW_WIDTH / 2) : -1000;
    *y = horizon - (int)(el * WINDOW_HEIGHT * 0.8f);
}

// Now and then (about one 25 s spell in four) a soft shaft of sun or moon
// light slants down to the ground, fading in and out over a few seconds.
static float BeamStrength(int salt) {
    DWORD now = GetTickCount(), cyc = now / 25000, t = now % 25000;
    if (RainHash((int)cyc * 7 + salt) % 4 || t > 15000) return 0.0f;
    return t < 3000 ? t / 3000.0f : t > 12000 ? (15000 - t) / 3000.0f : 1.0f;
}

static void DrawSkyBeam(HDC hdc, int x, int y, int h, COLORREF c, int alpha, float k) {
    if (k <= 0.0f || x < -200 || x > WINDOW_WIDTH + 200) return;
    for (int yy = y; yy < h + 80 && yy < WINDOW_HEIGHT; yy += 8) {
        float t = (float)(yy - y) / (h + 80 - y + 1);
        int w = 20 + (int)(t * 120), cx = x - (int)((yy - y) * 0.35f);
        FillRectAlpha(hdc, cx - w / 2, yy, w, 8, c, (int)(alpha * k * (1.0f - 0.6f * t)));
    }
}

static void DrawSky(HDC hdc, float light) {
    int h = horizon < 0 ? 0 : horizon > WINDOW_HEIGHT ? WINDOW_HEIGHT : horizon;
    if (in_cave || current_biome == BIOME_CAVE) { Box(hdc, 0, 0, WINDOW_WIDTH, h, RGB(28, 24, 22)); return; }
    int gloomy = IsAdverseWeather();
    COLORREF top = Mix(RGB(6, 9, 26), RGB(70, 130, 220), light), low = Mix(RGB(26, 32, 62), RGB(175, 210, 245), light);
    if (gloomy) { top = Mix(top, Dim(RGB(130, 135, 145), 0.3f + 0.7f * light), 0.7f); low = Mix(low, Dim(RGB(160, 165, 170), 0.3f + 0.7f * light), 0.7f); }
    if (h > 0) { // one smooth vertical gradient instead of stacked bands
        TRIVERTEX v[2] = { { 0, 0, (COLOR16)(GetRValue(top) << 8), (COLOR16)(GetGValue(top) << 8), (COLOR16)(GetBValue(top) << 8), 0 },
                           { WINDOW_WIDTH, h, (COLOR16)(GetRValue(low) << 8), (COLOR16)(GetGValue(low) << 8), (COLOR16)(GetBValue(low) << 8), 0 } };
        GRADIENT_RECT gr = { 0, 1 };
        GradientFill(hdc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
    }
    int x, y;
    if (light > 0.45f && !gloomy) { // the sun
        SkyPoint(-0.9f, 0.55f * light, &x, &y);
        Blob(hdc, x - 26, y - 26, x + 26, y + 26, RGB(255, 236, 170)); Blob(hdc, x - 18, y - 18, x + 18, y + 18, RGB(255, 250, 215));
        DrawSkyBeam(hdc, x, y, h, RGB(255, 240, 190), 45, BeamStrength(1));
    }
    if (light > 0.4f && !gloomy) { // a distant flock: little cursive v's drifting and flapping
        float tb = GetTickCount() / 1000.0f;
        for (int i = 0; i < 7; i++) {
            float az = 1.9f + tb * 0.006f + (i % 4) * 0.035f + i * 0.012f, el = 0.30f + 0.025f * sinf(tb * 0.3f + i * 1.3f) + (i % 3) * 0.02f;
            int bx, by; SkyPoint(az, el, &bx, &by);
            if (bx < 0 || bx >= WINDOW_WIDTH || by < 0 || by >= h) continue;
            int s = 3 + (i % 3), flap = (int)(sinf(tb * (5.0f + i % 3) + i) * 2.0f);
            COLORREF bc = Dim(RGB(60, 65, 80), 0.6f + 0.4f * light);
            Stroke(hdc, bx - s, by - 1 - flap, bx, by + 1, 1, bc); Stroke(hdc, bx, by + 1, bx + s, by - 1 - flap, 1, bc);
        }
    }
    if (light >= 0.35f || gloomy) return;
    float k = (0.35f - light) / 0.35f;
    DWORD now = GetTickCount();
    for (int i = 0; i < 180; i++) { // stars hang at fixed bearings so they pan with the gaze
        int hsh = RainHash(i * 7919 + 3);
        SkyPoint((hsh % 6283) / 1000.0f, 0.08f + ((hsh / 6283) % 1000) / 1100.0f, &x, &y);
        if (x < 0 || x >= WINDOW_WIDTH || y < 0 || y >= h) continue;
        int lum = (int)((150.0f + 105.0f * (0.5f + 0.5f * sinf(now / 600.0f + i))) * k), sz = i % 11 == 0 ? 2 : 1;
        Box(hdc, x, y, x + sz - 1, y + sz - 1, RGB(lum, lum, lum));
    }
    SkyPoint(0.8f, 0.42f, &x, &y); // the moon; its phase follows the 30-day season
    if (x > -100) { // moonlight is a soft halo, not a spotlight
        for (int r = 4; r >= 1; r--) FillRectAlpha(hdc, x - 22 * r, y - 22 * r, 44 * r, 44 * r, RGB(200, 215, 240), (int)(7 * k));
        int mr = 22, moon = (int)(235 * k);
        float phase = (SeasonDay() % 30) / 30.0f;
        Blob(hdc, x - mr, y - mr, x + mr, y + mr, RGB(moon, moon, moon * 9 / 10));
        int off = phase <= 0.5f ? (int)(phase * 4.0f * mr) : -(int)((1.0f - phase) * 4.0f * mr);
        if (off < 2 * mr && off > -2 * mr) Blob(hdc, x - mr + off, y - mr, x + mr + off, y + mr, Mix(top, low, 0.45f));
    }
    // A rare shooting star (one every ~40 s on average), pinned to the sky like
    // the stars so its trail glides smoothly when the gaze pans.
    DWORD cyc = now / 4000, t = now % 4000;
    int mh = RainHash((int)cyc * 31 + 5);
    if (mh % 10 == 0 && t < 900) {
        float p = t / 900.0f, az = (mh / 10 % 6283) / 1000.0f, el = 0.35f + (mh / 7 % 300) / 1000.0f;
        int x0, y0, x1, y1;
        SkyPoint(az + p * 0.35f, el - p * 0.12f, &x1, &y1);
        SkyPoint(az + p * 0.35f - 0.09f, el - p * 0.12f + 0.03f, &x0, &y0);
        int fade = (int)(255 * k * (p < 0.8f ? 1.0f : (1.0f - p) / 0.2f));
        if (x1 > 0 && x0 > 0 && y1 < h) Stroke(hdc, x0, y0, x1, y1, 2, RGB(fade, fade, fade * 9 / 10));
    }
}

static COLORREF GroundColor(void) {
    if (in_cave || current_biome == BIOME_CAVE) return RGB(70, 62, 55);
    if (snow_cover > 0.5f || current_biome == BIOME_TUNDRA) return RGB(225, 230, 238);
    if (current_biome == BIOME_DESERT) return RGB(225, 195, 95);
    if (current_biome == BIOME_SWAMP) return RGB(75, 100, 60);
    if (current_season == SEASON_FALL) return RGB(125, 140, 60);
    return RGB(85, 155, 65);
}

static void DrawThing(HDC hdc, const FpThing *t, float lit) {
    int sx, sy; float d;
    if (!Project(t->x, t->y, t->z, &sx, &sy, &d)) return;
    float u = focal / d; // pixels per tile at this distance
    if (u > WINDOW_HEIGHT * 0.6f && t->kind != FP_BOBBER) return; // too close to the eye: it would fill the view
    int U = (int)u;
    DWORD now = GetTickCount();
    switch (t->kind) {
    case FP_TREE: {
        Box(hdc, sx - (int)(0.09f * u), sy - (int)(1.0f * u), sx + (int)(0.09f * u), sy, Dim(RGB(100, 70, 45), lit));
        if (t->seed % 3 == 0) {
            POINT p[3] = { { sx, sy - (int)(2.9f * u) }, { sx + (int)(0.75f * u), sy - (int)(0.7f * u) }, { sx - (int)(0.75f * u), sy - (int)(0.7f * u) } };
            Shape(hdc, p, 3, Dim(RGB(35, 105, 60), lit));
            if (snow_cover > 0.3f) { POINT s[3] = { p[0], { sx + (int)(0.3f * u), sy - (int)(2.0f * u) }, { sx - (int)(0.3f * u), sy - (int)(2.0f * u) } }; Shape(hdc, s, 3, Dim(RGB(235, 240, 245), lit)); }
        } else if (current_season == SEASON_WINTER) {
            for (int b = -1; b <= 1; b++) Stroke(hdc, sx, sy - (int)(0.9f * u), sx + b * (int)(0.5f * u), sy - (int)((1.8f + (b == 0) * 0.5f) * u), U / 12 + 1, Dim(RGB(95, 70, 50), lit));
        } else {
            COLORREF leaf = current_season == SEASON_FALL ? RGB(205, 120, 40) : RGB(60, 140, 55);
            Blob(hdc, sx - (int)(0.8f * u), sy - (int)(2.5f * u), sx + (int)(0.8f * u), sy - (int)(0.85f * u), Dim(leaf, lit));
            Blob(hdc, sx - (int)(0.45f * u), sy - (int)(2.35f * u), sx + (int)(0.15f * u), sy - (int)(1.7f * u), Dim(leaf, lit * 1.2f));
        }
        break; }
    case FP_ROCK:
        Blob(hdc, sx - (int)(0.4f * u), sy - (int)(0.45f * u), sx + (int)(0.4f * u), sy + (int)(0.05f * u), Dim(RGB(125, 125, 130), lit));
        break;
    case FP_HOME: {
        Box(hdc, sx - (int)(0.8f * u), sy - (int)(1.1f * u), sx + (int)(0.8f * u), sy, Dim(RGB(200, 170, 120), lit));
        POINT roof[3] = { { sx - (int)(0.95f * u), sy - (int)(1.1f * u) }, { sx, sy - (int)(1.9f * u) }, { sx + (int)(0.95f * u), sy - (int)(1.1f * u) } };
        Shape(hdc, roof, 3, Dim(RGB(150, 60, 45), lit));
        Box(hdc, sx - (int)(0.15f * u), sy - (int)(0.55f * u), sx + (int)(0.15f * u), sy, Dim(RGB(90, 60, 40), lit));
        Box(hdc, sx + (int)(0.35f * u), sy - (int)(0.85f * u), sx + (int)(0.6f * u), sy - (int)(0.55f * u), lit < 0.5f ? RGB(255, 210, 110) : RGB(130, 170, 210));
        break; }
    case FP_FIRE: {
        Blob(hdc, sx - (int)(0.9f * u), sy - (int)(0.15f * u), sx + (int)(0.9f * u), sy + (int)(0.15f * u), Mix(Dim(GroundColor(), lit), RGB(230, 130, 40), 0.45f));
        Stroke(hdc, sx - (int)(0.32f * u), sy, sx + (int)(0.32f * u), sy - (int)(0.06f * u), U / 8 + 1, RGB(90, 55, 30));
        Stroke(hdc, sx - (int)(0.32f * u), sy - (int)(0.06f * u), sx + (int)(0.32f * u), sy, U / 8 + 1, RGB(70, 45, 25));
        int f = (int)((now / 90 + t->seed) % 3) - 1;
        POINT outer[3] = { { sx - (int)(0.24f * u), sy - (int)(0.05f * u) }, { sx + f * U / 18, sy - (int)((0.7f + 0.05f * f) * u) }, { sx + (int)(0.24f * u), sy - (int)(0.05f * u) } };
        POINT inner[3] = { { sx - (int)(0.12f * u), sy - (int)(0.05f * u) }, { sx - f * U / 24, sy - (int)(0.42f * u) }, { sx + (int)(0.12f * u), sy - (int)(0.05f * u) } };
        Shape(hdc, outer, 3, RGB(245, 130, 30)); Shape(hdc, inner, 3, RGB(255, 225, 90));
        break; }
    case FP_BUTTERFLY: {
        static const COLORREF wing[4] = { RGB(250, 200, 60), RGB(240, 120, 180), RGB(120, 180, 250), RGB(250, 250, 250) };
        int w = (int)((0.05f + 0.05f * fabsf(sinf(now / 110.0f + t->seed))) * u) + 2, hgt = (int)(0.07f * u) + 2;
        COLORREF c = Dim(wing[butterflies[t->seed].color_profile & 3], lit + 0.2f);
        Blob(hdc, sx - w, sy - hgt, sx, sy + hgt / 2, c); Blob(hdc, sx, sy - hgt, sx + w, sy + hgt / 2, c);
        Stroke(hdc, sx, sy - hgt, sx, sy + hgt / 2, 1, RGB(40, 30, 20));
        break; }
    case FP_CRITTER: {
        static const COLORREF fur[4] = { RGB(70, 120, 220), RGB(150, 95, 50), RGB(140, 115, 85), RGB(45, 40, 50) };
        COLORREF c = Dim(fur[t->seed & 3], lit + 0.1f);
        int hop = (now / (190 + 60 * (t->seed % 3)) + t->seed) % 4 == 0 ? (int)(0.06f * u) : 0;
        if ((t->seed & 3) == 1) Blob(hdc, sx - (int)(0.32f * u), sy - (int)(0.45f * u) - hop, sx - (int)(0.08f * u), sy - (int)(0.1f * u) - hop, c);
        Blob(hdc, sx - (int)(0.12f * u), sy - (int)(0.22f * u) - hop, sx + (int)(0.12f * u), sy - hop, c);
        Blob(hdc, sx + (int)(0.04f * u), sy - (int)(0.32f * u) - hop, sx + (int)(0.2f * u), sy - (int)(0.16f * u) - hop, c);
        break; }
    case FP_FAMILIAR: {
        static const COLORREF coat[5] = { RGB(150, 100, 55), RGB(120, 120, 128), RGB(70, 130, 60), RGB(140, 100, 60), RGB(170, 150, 120) };
        COLORREF c = Dim(coat[t->seed % 5], lit + 0.15f);
        int wag = (int)(sinf(now / 150.0f) * 0.08f * u);
        Blob(hdc, sx - (int)(0.32f * u), sy - (int)(0.38f * u), sx + (int)(0.32f * u), sy - (int)(0.05f * u), c);
        Blob(hdc, sx + (int)(0.12f * u), sy - (int)(0.62f * u), sx + (int)(0.42f * u), sy - (int)(0.32f * u), c);
        Stroke(hdc, sx - (int)(0.3f * u), sy - (int)(0.25f * u), sx - (int)(0.5f * u), sy - (int)(0.45f * u) + wag, U / 14 + 1, c);
        Blob(hdc, sx + (int)(0.3f * u), sy - (int)(0.52f * u), sx + (int)(0.34f * u), sy - (int)(0.48f * u), RGB(10, 10, 10));
        break; }
    case FP_BOBBER: {
        int r = (int)(0.07f * u) + 3, bob = t->seed ? (int)(0.05f * u) + 2 : (int)(sinf(now / 300.0f) * 2.0f);
        if (t->seed) { HPEN ring = CreatePen(PS_SOLID, 1, RGB(200, 230, 255)); HGDIOBJ op = SelectObject(hdc, ring), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Ellipse(hdc, sx - r * 3, sy - r / 2, sx + r * 3, sy + r); SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(ring); }
        Blob(hdc, sx - r, sy - r + bob, sx + r, sy + bob, RGB(245, 245, 245));
        Blob(hdc, sx - r, sy - 2 * r + bob, sx + r, sy - r + bob, RGB(220, 40, 40));
        bobber_sx = sx; bobber_sy = sy - r + bob;
        break; }
    }
}

// Virtua Racing-style flat-shaded polygons: every ground tile, tree, rock,
// home and fire is built from faces, clipped at the eye, shaded by the sun,
// faded into the haze with distance and painted far to near.
#define FP_MAX_FACES 4096
#define FP_NEAR 0.08f
typedef struct { float depth; int n, sprite; POINT p[12]; COLORREF c; } FpFace;
static FpFace faces[FP_MAX_FACES];
static int face_count, face_glow;
static float face_bias, scene_lit;
static COLORREF haze;
static float fog_near = 6.0f, fog_far = FP_RANGE, fog_cap = 0.7f; // night pulls these in (further for elves/dwarves)

static void AddFace(float v[][3], int n, COLORREF col, const float *center, int ground) {
    if (face_count >= FP_MAX_FACES || n < 3 || n > 8) return;
    float ax = v[1][0] - v[0][0], ay = v[1][1] - v[0][1], az = v[1][2] - v[0][2];
    float bx = v[2][0] - v[0][0], by = v[2][1] - v[0][1], bz = v[2][2] - v[0][2];
    float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
    float cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < n; i++) { cx += v[i][0]; cy += v[i][1]; cz += v[i][2]; }
    cx /= n; cy /= n; cz /= n;
    float out = center ? nx * (cx - center[0]) + ny * (cy - center[1]) + nz * (cz - center[2]) : nz;
    if (out < 0) { nx = -nx; ny = -ny; nz = -nz; }
    float ex = eye_x - cx, ey = eye_y - cy, ez = eye_z - cz;
    if (nx * ex + ny * ey + nz * ez <= 0.0f) return; // facing away
    float len = sqrtf(nx * nx + ny * ny + nz * nz) + 1e-6f;
    float sun = (-0.45f * nx - 0.35f * ny + 0.82f * nz) / len;
    float shade = face_glow ? 1.0f : (0.5f + 0.5f * (sun > 0 ? sun : 0)) * scene_lit;
    float cs = cosf(yaw), sn = sinf(yaw), in[8][3], cl[12][3];
    for (int i = 0; i < n; i++) {
        float dx = v[i][0] - eye_x, dy = v[i][1] - eye_y;
        in[i][0] = dx * cs + dy * sn; in[i][1] = -dx * sn + dy * cs; in[i][2] = v[i][2];
    }
    int m = 0;
    for (int i = 0; i < n; i++) { // clip against the near plane
        float *a = in[i], *b = in[(i + 1) % n];
        int ia = a[0] >= FP_NEAR, ib = b[0] >= FP_NEAR;
        if (ia) { cl[m][0] = a[0]; cl[m][1] = a[1]; cl[m][2] = a[2]; m++; }
        if (ia != ib) { float t = (FP_NEAR - a[0]) / (b[0] - a[0]); for (int k = 0; k < 3; k++) cl[m][k] = a[k] + t * (b[k] - a[k]); m++; }
    }
    if (m < 3) return;
    FpFace *F = &faces[face_count++];
    for (int i = 0; i < m; i++) {
        F->p[i].x = WINDOW_WIDTH / 2 + (int)(cl[i][1] / cl[i][0] * focal);
        F->p[i].y = horizon + (int)((eye_z - cl[i][2]) / cl[i][0] * focal);
    }
    float d = sqrtf(ex * ex + ey * ey + ez * ez), fog = (d - fog_near) / (fog_far - fog_near);
    fog = fog < 0 ? 0 : fog > 1 ? 1 : fog;
    F->n = m; F->sprite = -1; F->depth = d - face_bias + (ground ? 1000.0f : 0.0f); // ground always first
    F->c = Mix(Dim(col, shade), haze, fog * fog_cap);
}

// Animals, butterflies, the familiar and the bobber stay small figures, sorted in with the polygons.
static void AddSprite(int idx) {
    if (face_count >= FP_MAX_FACES) return;
    float dx = things[idx].x - eye_x, dy = things[idx].y - eye_y;
    FpFace *F = &faces[face_count++];
    F->sprite = idx; F->n = 0; F->depth = sqrtf(dx * dx + dy * dy);
}

static int FaceFarFirst(const void *a, const void *b) {
    float da = ((const FpFace *)a)->depth, db = ((const FpFace *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

static void DrawFaces(HDC hdc, float lit) {
    qsort(faces, face_count, sizeof(FpFace), FaceFarFirst);
    HGDIOBJ ob = SelectObject(hdc, GetStockObject(DC_BRUSH)), op = SelectObject(hdc, GetStockObject(DC_PEN));
    for (int i = 0; i < face_count; i++) {
        FpFace *F = &faces[i];
        if (F->sprite >= 0) {
            DrawThing(hdc, &things[F->sprite], lit);
            SelectObject(hdc, GetStockObject(DC_BRUSH)); SelectObject(hdc, GetStockObject(DC_PEN));
            continue;
        }
        SetDCBrushColor(hdc, F->c); SetDCPenColor(hdc, F->c); // same-colour edge hides seams
        Polygon(hdc, F->p, F->n);
    }
    SelectObject(hdc, ob); SelectObject(hdc, op);
}

// A ring of seg points at z0 joined to a ring at z1 (r1 = 0 makes a cone).
static void Prism(float x, float y, float z0, float z1, float r0, float r1, int seg, float rot, COLORREF col) {
    float c[3] = { x, y, (z0 + z1) / 2 }, ring0[8][2], ring1[8][2];
    for (int i = 0; i < seg; i++) {
        float a = rot + i * 6.2832f / seg;
        ring0[i][0] = x + cosf(a) * r0; ring0[i][1] = y + sinf(a) * r0;
        ring1[i][0] = x + cosf(a) * r1; ring1[i][1] = y + sinf(a) * r1;
    }
    for (int i = 0; i < seg; i++) {
        int j = (i + 1) % seg;
        float v[4][3] = { { ring0[i][0], ring0[i][1], z0 }, { ring0[j][0], ring0[j][1], z0 },
                          { ring1[j][0], ring1[j][1], z1 }, { ring1[i][0], ring1[i][1], z1 } };
        AddFace(v, r1 > 0.001f ? 4 : 3, col, c, 0);
    }
    if (r1 > 0.001f) {
        float v[8][3];
        for (int i = 0; i < seg; i++) { v[i][0] = ring1[i][0]; v[i][1] = ring1[i][1]; v[i][2] = z1; }
        AddFace(v, seg, col, c, 0);
    }
}

// --- Low-poly animals and people ------------------------------------------
// Oriented boxes, cones and hinged wings built from the same shaded faces as
// the trees, so animals keep their gameplay look: legs, body, head, ears,
// tails and dark eye dots. Each figure faces the way it is moving.
extern int familiar_size, familiar_color; // picked at the pet shop (town.c)
static float hcos = 1.0f, hsin;

static void OBox(float cx, float cy, float along, float side, float z0, float z1, float hl, float hw, COLORREF col) {
    float x = cx + hcos * along - hsin * side, y = cy + hsin * along + hcos * side;
    float c[3] = { x, y, (z0 + z1) / 2 }, px[4], py[4];
    static const int sa[4] = { 1, 1, -1, -1 }, sb[4] = { 1, -1, -1, 1 };
    for (int i = 0; i < 4; i++) { px[i] = x + hcos * hl * sa[i] - hsin * hw * sb[i]; py[i] = y + hsin * hl * sa[i] + hcos * hw * sb[i]; }
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        float v[4][3] = { { px[i], py[i], z0 }, { px[j], py[j], z0 }, { px[j], py[j], z1 }, { px[i], py[i], z1 } };
        AddFace(v, 4, col, c, 0);
    }
    float top[4][3];
    for (int i = 0; i < 4; i++) { top[i][0] = px[i]; top[i][1] = py[i]; top[i][2] = z1; }
    AddFace(top, 4, col, c, 0);
}

static void Cone(float cx, float cy, float along, float side, float z0, float z1, float r, COLORREF col) {
    Prism(cx + hcos * along - hsin * side, cy + hsin * along + hcos * side, z0, z1, r, 0.0f, 4, 0.0f, col);
}

// Two dots on the front of a head (drawn just in front of it).
static void Eyes(float cx, float cy, float along, float side, float z, float e, COLORREF col) {
    float saved = face_bias; face_bias = 0.05f; face_glow = 1;
    for (int k = -1; k <= 1; k += 2) {
        float x = cx + hcos * along - hsin * side * k, y = cy + hsin * along + hcos * side * k;
        float v[4][3] = { { x - hsin * e, y + hcos * e, z - e }, { x + hsin * e, y - hcos * e, z - e }, { x + hsin * e, y - hcos * e, z + e }, { x - hsin * e, y + hcos * e, z + e } };
        float c[3] = { x - hcos * 0.05f, y - hsin * 0.05f, z };
        AddFace(v, 4, col, c, 0);
    }
    face_bias = saved; face_glow = 0;
}

// A wing hinged along the body, raised by angle f; seen from above or below.
static void Wing(float cx, float cy, float z, float len, float span, float f, int sidek, COLORREF col) {
    float sx = -hsin * sidek, sy = hcos * sidek, tip = span * cosf(f), up = span * sinf(f);
    float v[4][3] = { { cx + hcos * len, cy + hsin * len, z }, { cx - hcos * len, cy - hsin * len, z },
                      { cx - hcos * len * 0.6f + sx * tip, cy - hsin * len * 0.6f + sy * tip, z + up }, { cx + hcos * len * 1.2f + sx * tip, cy + hsin * len * 1.2f + sy * tip, z + up } };
    float above[3] = { cx, cy, z + 1.0f }, below[3] = { cx, cy, z - 1.0f };
    AddFace(v, 4, col, above, 0); AddFace(v, 4, col, below, 0);
}

static float HeadingFor(int key, float x, float y) {
    static float lx[128], ly[128], hd[128];
    key &= 127;
    float dx = x - lx[key], dy = y - ly[key], d2 = dx * dx + dy * dy;
    if (d2 >= 4.0f) hd[key] = atan2f(player_y - y, player_x - x); // just appeared: facing the hero
    else if (d2 > 0.00002f) hd[key] = Wrap(hd[key] + Wrap(atan2f(dy, dx) - hd[key]) * 0.2f);
    lx[key] = x; ly[key] = y;
    return hd[key];
}

static void AnimalMesh(const FpThing *t) {
    DWORD now = GetTickCount();
    int kind = t->kind, idx = kind == FP_CRITTER ? t->seed >> 3 : t->seed, type = kind == FP_CRITTER ? t->seed & 7 : -1;
    float h = HeadingFor(kind * 16 + (idx & 15), t->x, t->y), x = t->x, y = t->y, z = t->z;
    hcos = cosf(h); hsin = sinf(h);
    COLORREF black = RGB(12, 10, 10);
    if (kind == FP_FAMILIAR && t->seed <= 1) { // dog or cat in the coat and size picked at the pet shop
        static const COLORREF coats[2][4] = { { RGB(150, 100, 55), RGB(40, 35, 30), RGB(235, 230, 220), RGB(205, 160, 80) },
                                              { RGB(120, 120, 128), RGB(220, 130, 50), RGB(35, 35, 40), RGB(240, 240, 240) } };
        int cat = t->seed == 1;
        COLORREF c = coats[cat][familiar_color & 3], dark = Dim(c, 0.7f);
        float S = familiar_size == 0 ? 0.75f : familiar_size == 2 ? 1.3f : 1.0f, st = sinf(now / 110.0f) * 0.02f * S;
        for (int l = 0; l < 4; l++) OBox(x, y, (l < 2 ? 0.11f : -0.11f) * S + (l % 2 ? st : -st), (l % 2 ? 0.05f : -0.05f) * S, 0.0f, 0.14f * S, 0.025f * S, 0.025f * S, dark);
        OBox(x, y, 0.0f, 0.0f, 0.13f * S, 0.27f * S, (cat ? 0.15f : 0.17f) * S, (cat ? 0.055f : 0.07f) * S, c);
        OBox(x, y, 0.2f * S, 0.0f, 0.24f * S, 0.38f * S, 0.07f * S, 0.065f * S, c);
        if (cat) { Cone(x, y, 0.19f * S, 0.04f * S, 0.38f * S, 0.46f * S, 0.03f * S, c); Cone(x, y, 0.19f * S, -0.04f * S, 0.38f * S, 0.46f * S, 0.03f * S, c);
                   OBox(x, y, -0.17f * S, 0.0f, 0.2f * S, 0.32f * S, 0.02f * S, 0.02f * S, c); OBox(x, y, -0.2f * S, 0.0f, 0.32f * S, 0.46f * S, 0.02f * S, 0.02f * S, c); }
        else { OBox(x, y, 0.29f * S, 0.0f, 0.25f * S, 0.31f * S, 0.04f * S, 0.035f * S, dark); OBox(x, y, 0.335f * S, 0.0f, 0.29f * S, 0.315f * S, 0.008f * S, 0.015f * S, black);
               OBox(x, y, 0.18f * S, 0.075f * S, 0.27f * S, 0.37f * S, 0.025f * S, 0.012f * S, dark); OBox(x, y, 0.18f * S, -0.075f * S, 0.27f * S, 0.37f * S, 0.025f * S, 0.012f * S, dark);
               OBox(x, y, -0.2f * S, sinf(now / 90.0f) * 0.03f * S, 0.24f * S, 0.34f * S, 0.03f * S, 0.015f * S, c); }
        Eyes(x, y, 0.273f * S, 0.03f * S, 0.33f * S, 0.012f * S, cat ? RGB(110, 190, 60) : black);
        return;
    }
    if (kind == FP_BUTTERFLY) { // a lazy looping drift on top of its real path
        x += 0.04f * sinf(now / 700.0f + idx); y += 0.03f * cosf(now / 900.0f + idx * 1.7f); z += 0.03f * sinf(now / 380.0f + idx * 2.3f);
        static const COLORREF wing[4] = { RGB(250, 200, 60), RGB(240, 120, 180), RGB(120, 180, 250), RGB(250, 250, 250) };
        COLORREF c = wing[butterflies[idx].color_profile & 3];
        float f = 0.2f + 1.1f * fabsf(sinf(now / 90.0f + idx));
        OBox(x, y, 0.0f, 0.0f, z - 0.008f, z + 0.008f, 0.03f, 0.006f, RGB(40, 30, 20));
        Wing(x, y, z, 0.025f, 0.06f, f, 1, c); Wing(x, y, z, 0.025f, 0.06f, f, -1, c);
        return;
    }
    if (kind == FP_FAMILIAR && t->seed == 2) type = 5; // toad
    if (kind == FP_FAMILIAR && t->seed >= 3) type = t->seed == 4 ? 2 : 6; // owl or hawk
    float hop = (now / (190 + 60 * (idx % 3)) + idx) % 4 == 0 ? 0.03f : 0.0f;
    if (type == 0 || type == 6) { // bird / hawk
        COLORREF c = type == 6 ? RGB(140, 100, 60) : RGB(70, 120, 220);
        float zb = (type == 6 ? 0.9f + sinf(now / 300.0f) * 0.05f : hop), f = type == 6 ? sinf(now / 150.0f) * 0.6f : -0.2f;
        OBox(x, y, 0.0f, 0.0f, zb + 0.05f, zb + 0.13f, 0.06f, 0.035f, c);
        OBox(x, y, 0.065f, 0.0f, zb + 0.11f, zb + 0.18f, 0.03f, 0.028f, c);
        OBox(x, y, 0.105f, 0.0f, zb + 0.135f, zb + 0.15f, 0.015f, 0.008f, RGB(240, 170, 40));
        OBox(x, y, -0.08f, 0.0f, zb + 0.1f, zb + 0.115f, 0.03f, 0.02f, Dim(c, 0.7f));
        Wing(x, y, zb + 0.12f, 0.04f, type == 6 ? 0.16f : 0.05f, f, 1, Dim(c, 0.8f)); Wing(x, y, zb + 0.12f, 0.04f, type == 6 ? 0.16f : 0.05f, f, -1, Dim(c, 0.8f));
        Eyes(x, y, 0.096f, 0.012f, zb + 0.16f, 0.006f, black);
    } else if (type == 1) { // squirrel with a big curled tail
        COLORREF c = RGB(150, 95, 50);
        OBox(x, y, 0.0f, 0.0f, hop, hop + 0.1f, 0.07f, 0.04f, c);
        OBox(x, y, 0.07f, 0.0f, hop + 0.07f, hop + 0.15f, 0.035f, 0.032f, c);
        Cone(x, y, 0.065f, 0.02f, hop + 0.15f, hop + 0.19f, 0.012f, c); Cone(x, y, 0.065f, -0.02f, hop + 0.15f, hop + 0.19f, 0.012f, c);
        OBox(x, y, -0.09f, 0.0f, hop + 0.04f, hop + 0.16f, 0.03f, 0.035f, Mix(c, RGB(200, 160, 110), 0.3f));
        OBox(x, y, -0.11f, 0.0f, hop + 0.16f, hop + 0.25f, 0.035f, 0.035f, Mix(c, RGB(200, 160, 110), 0.3f));
        Eyes(x, y, 0.106f, 0.015f, hop + 0.125f, 0.007f, black);
    } else if (type == 2) { // owl, upright with tufts and big yellow eyes
        COLORREF c = RGB(140, 115, 85);
        OBox(x, y, 0.0f, 0.0f, 0.0f, 0.24f, 0.07f, 0.07f, c);
        OBox(x, y, 0.0f, 0.0f, 0.24f, 0.36f, 0.065f, 0.065f, c);
        Cone(x, y, 0.0f, 0.045f, 0.36f, 0.42f, 0.02f, c); Cone(x, y, 0.0f, -0.045f, 0.36f, 0.42f, 0.02f, c);
        Eyes(x, y, 0.068f, 0.028f, 0.31f, 0.018f, RGB(250, 210, 60));
        face_bias = 0.02f; Eyes(x, y, 0.072f, 0.028f, 0.31f, 0.008f, black); face_bias = 0.0f;
    } else if (type == 3) { // bat
        float zb = 0.6f + sinf(now / 200.0f + idx) * 0.08f;
        OBox(x, y, 0.0f, 0.0f, zb, zb + 0.06f, 0.03f, 0.025f, RGB(45, 40, 50));
        Wing(x, y, zb + 0.04f, 0.04f, 0.12f, sinf(now / 70.0f) * 0.8f, 1, RGB(60, 52, 66)); Wing(x, y, zb + 0.04f, 0.04f, 0.12f, sinf(now / 70.0f) * 0.8f, -1, RGB(60, 52, 66));
    } else if (type == 4) { // scorpion: flat body, pincers, tail curled up and over
        COLORREF c = RGB(150, 95, 45);
        OBox(x, y, 0.0f, 0.0f, 0.0f, 0.03f, 0.05f, 0.03f, c);
        OBox(x, y, 0.07f, 0.035f, 0.0f, 0.02f, 0.025f, 0.012f, c); OBox(x, y, 0.07f, -0.035f, 0.0f, 0.02f, 0.025f, 0.012f, c);
        for (int sgm = 0; sgm < 4; sgm++) OBox(x, y, -0.06f - sgm * 0.012f + (sgm > 2) * 0.03f, 0.0f, 0.02f + sgm * 0.025f, 0.045f + sgm * 0.025f, 0.012f, 0.01f, c);
    } else if (type == 5) { // toad
        COLORREF c = RGB(70, 130, 60);
        OBox(x, y, 0.0f, 0.0f, hop, hop + 0.08f, 0.08f, 0.07f, c);
        OBox(x, y, 0.05f, 0.035f, hop + 0.08f, hop + 0.11f, 0.015f, 0.015f, c); OBox(x, y, 0.05f, -0.035f, hop + 0.08f, hop + 0.11f, 0.015f, 0.015f, c);
        Eyes(x, y, 0.066f, 0.035f, hop + 0.095f, 0.008f, black);
    }
}

// Ground shadows thrown away from nearby campfires and torches at night.
extern int village_light_count;
static void FireShadows(float x, float y, float tall) {
    if (scene_lit > 0.65f || in_cave) return;
    float lx[MAX_CAMPFIRES + MAX_VILLAGE_LIGHTS], ly[MAX_CAMPFIRES + MAX_VILLAGE_LIGHTS]; int n = 0;
    for (int i = 0; i < MAX_CAMPFIRES; i++)
        if (campfires[i].active && campfires[i].screen_id == current_screen_index) { lx[n] = campfires[i].x; ly[n++] = campfires[i].y; }
    for (int i = 0; i < village_light_count && i < MAX_VILLAGE_LIGHTS; i++) { lx[n] = village_lights[i].x; ly[n++] = village_lights[i].y; }
    float saved = face_bias; face_bias = 0.02f; face_glow = 1;
    for (int i = 0; i < n; i++) {
        float dx = x - lx[i], dy = y - ly[i], d = sqrtf(dx * dx + dy * dy);
        if (d < 0.2f || d > 6.0f) continue;
        dx /= d; dy /= d;
        float len = tall * (0.8f + d * 0.35f), w = 0.1f, flick = 1.0f + sinf(GetTickCount() / 90.0f + i) * 0.06f;
        float v[4][3] = { { x - dy * w, y + dx * w, 0.01f }, { x + dy * w, y - dx * w, 0.01f },
                          { x + dx * len * flick + dy * w * 1.6f, y + dy * len * flick - dx * w * 1.6f, 0.01f },
                          { x + dx * len * flick - dy * w * 1.6f, y + dy * len * flick + dx * w * 1.6f, 0.01f } };
        float below[3] = { x, y, -1.0f };
        AddFace(v, 4, RGB(14, 16, 14), below, 0);
    }
    face_bias = saved; face_glow = 0;
}

// Mythic beasts at the size and shape they have in play (about twice a
// monster's height): Yeti and Bigfoot as long-armed apes that club with both
// fists, the Sphinx as a lion with a pharaoh's head, the Bunyip low and tusked.
static void BeastMesh(float x, float y, int type, int idx, float swing) {
    DWORD now = GetTickCount();
    COLORREF black = RGB(12, 10, 10);
    if (type == MONSTER_YETI || type == MONSTER_BIGFOOT) {
        float S = 1.9f, st = sinf(now / 220.0f + idx) * 0.05f * S;
        COLORREF fur = type == MONSTER_YETI ? RGB(232, 238, 246) : RGB(105, 70, 40), face = type == MONSTER_YETI ? RGB(120, 140, 165) : RGB(160, 120, 85);
        FireShadows(x, y, 1.2f * S);
        OBox(x, y, st, 0.1f * S, 0.0f, 0.36f * S, 0.07f * S, 0.07f * S, fur);
        OBox(x, y, -st, -0.1f * S, 0.0f, 0.36f * S, 0.07f * S, 0.07f * S, fur);
        Prism(x, y, 0.32f * S, 0.95f * S, 0.24f * S, 0.2f * S, 7, 0.0f, fur);
        for (int side = -1; side <= 1; side += 2) { // long arms that hang to the knees, raised together to club
            float lift = swing * (side > 0 ? 1.0f : 0.8f);
            OBox(x, y, (0.02f + lift * 0.25f) * S, side * 0.28f * S, (0.28f + lift * 0.55f) * S, (0.9f + lift * 0.35f) * S, 0.06f * S, 0.06f * S, fur);
        }
        OBox(x, y, 0.03f * S, 0.0f, 0.93f * S, 1.18f * S, 0.11f * S, 0.11f * S, fur);
        face_bias = 0.01f; OBox(x, y, 0.12f * S, 0.0f, 0.96f * S, 1.12f * S, 0.025f * S, 0.075f * S, face); face_bias = 0.0f;
        Eyes(x, y, 0.15f * S, 0.035f * S, 1.07f * S, 0.012f * S, black);
    } else if (type == MONSTER_SPHINX) {
        float S = 1.6f, st = sinf(now / 200.0f + idx) * 0.04f * S;
        COLORREF lion = RGB(200, 165, 100), skin = RGB(205, 165, 110), blue = RGB(40, 70, 160), gold = RGB(235, 195, 60);
        FireShadows(x, y, 0.8f * S);
        for (int l = 0; l < 4; l++) OBox(x, y, (l < 2 ? 0.26f : -0.26f) * S + (l % 2 ? st : -st), (l % 2 ? 0.11f : -0.11f) * S, 0.0f, 0.32f * S, 0.05f * S, 0.05f * S, lion);
        OBox(x, y, 0.0f, 0.0f, 0.28f * S, 0.55f * S, 0.38f * S, 0.16f * S, lion);
        OBox(x, y, -0.45f * S, 0.0f, 0.45f * S, 0.5f * S, 0.1f * S, 0.015f * S, lion);
        OBox(x, y, 0.36f * S, 0.0f, 0.5f * S, 0.95f * S, 0.09f * S, 0.14f * S, blue);
        face_bias = 0.01f;
        OBox(x, y, 0.36f * S, 0.0f, 0.68f * S, 0.71f * S, 0.095f * S, 0.145f * S, gold); OBox(x, y, 0.36f * S, 0.0f, 0.58f * S, 0.61f * S, 0.095f * S, 0.145f * S, gold);
        OBox(x, y, 0.45f * S, 0.0f, 0.62f * S, 0.86f * S, 0.02f * S, 0.08f * S, skin);
        face_bias = 0.0f;
        Eyes(x, y, 0.475f * S, 0.03f * S, 0.77f * S, 0.012f * S, black);
    } else { // Bunyip
        float S = 1.6f, st = sinf(now / 260.0f + idx) * 0.03f * S;
        COLORREF hide = RGB(60, 70, 45);
        FireShadows(x, y, 0.5f * S);
        for (int l = 0; l < 4; l++) OBox(x, y, (l < 2 ? 0.25f : -0.25f) * S + (l % 2 ? st : -st), (l % 2 ? 0.13f : -0.13f) * S, 0.0f, 0.16f * S, 0.05f * S, 0.05f * S, hide);
        OBox(x, y, 0.0f, 0.0f, 0.12f * S, 0.38f * S, 0.4f * S, 0.2f * S, hide);
        OBox(x, y, -0.52f * S, 0.0f, 0.16f * S, 0.26f * S, 0.15f * S, 0.05f * S, hide);
        OBox(x, y, 0.48f * S, 0.0f, 0.2f * S, 0.42f * S, 0.12f * S, 0.12f * S, RGB(70, 80, 50));
        OBox(x, y, 0.6f * S, 0.05f * S, 0.12f * S, 0.24f * S, 0.012f * S, 0.012f * S, RGB(235, 230, 210));
        OBox(x, y, 0.6f * S, -0.05f * S, 0.12f * S, 0.24f * S, 0.012f * S, 0.012f * S, RGB(235, 230, 210));
        Eyes(x, y, 0.6f * S, 0.06f * S, 0.36f * S, 0.012f * S, RGB(200, 240, 80));
    }
}

// Monsters and villagers as low-poly people: legs that stride, a body,
// arms (the leading one swings a weapon up close), a head and eye dots.
static void HumanMesh(const FpThing *t) {
    DWORD now = GetTickCount();
    int enemy = t->kind == FP_ENEMY, idx = enemy ? t->seed >> 5 : t->seed >> 4, type = enemy ? t->seed & 31 : t->seed & 15;
    float h = HeadingFor((enemy ? 100 : 108) + (idx & 15), t->x, t->y), x = t->x, y = t->y;
    hcos = cosf(h); hsin = sinf(h);
    float S = 1.0f; int horns = 0, crown = 0, hero = 0;
    COLORREF skin = RGB(225, 185, 150), cloth = RGB(70, 100, 170), legs = RGB(70, 55, 45), eye = RGB(12, 10, 10);
    if (enemy) {
        S = IS_MYTHIC_MONSTER(type) ? 2.0f : 1.0f;
        if (type == MONSTER_ZOMBIE) { skin = RGB(150, 175, 135); cloth = RGB(95, 80, 60); }
        else if (type == MONSTER_SKELLY) { skin = RGB(225, 225, 215); cloth = skin; legs = skin; }
        else if (type == MONSTER_GOBLIN) { skin = RGB(90, 140, 60); cloth = RGB(110, 80, 50); S = 0.8f; }
        else if (type == MONSTER_SATYR) { skin = RGB(185, 140, 100); cloth = skin; legs = RGB(110, 80, 55); horns = 1; }
        else if (type == MONSTER_DRACULA) { skin = RGB(222, 216, 228); cloth = RGB(35, 25, 35); legs = RGB(25, 20, 25); }
        else { skin = RGB(150, 140, 160); cloth = RGB(90, 85, 100); }
        eye = RGB(230, 60, 40);
    } else {
        static const COLORREF jobs[10] = { RGB(70, 100, 170), RGB(60, 130, 70), RGB(120, 110, 100), RGB(60, 55, 50), RGB(210, 130, 50),
                                           RGB(170, 50, 50), RGB(110, 50, 140), RGB(120, 95, 70), RGB(90, 90, 90), RGB(30, 30, 35) };
        cloth = jobs[type % 10]; crown = type == 6;
        if (type == 15) { // the hero, in whatever clothes are worn
            const char *w = WornItem("Clothes"); COLORREF cc = w ? ClothingColor(w) : CLOTH_UNKNOWN;
            cloth = cc != CLOTH_UNKNOWN ? cc : HeroClothColor(RGB(60, 80, 150)); eye = HeroEyeColor(); hero = 1; crown = 0;
        }
    }
    float dx = player_x - x, dy = player_y - y, swing = 0.0f;
    if (enemy && dx * dx + dy * dy < 2.6f) { float ph = fmodf(now / 455.0f, 1.0f); if (ph < 0.45f) swing = sinf(ph / 0.45f * 3.14159f); }
    if (enemy && IS_MYTHIC_MONSTER(type)) { BeastMesh(x, y, type, idx, swing); return; }
    int vamp = enemy && type == MONSTER_DRACULA;
    float st = sinf(now / 140.0f + idx) * 0.04f * S;
    FireShadows(x, y, 0.75f * S);
    if (vamp) { // high-collared cape behind the body
        OBox(x, y, -0.07f * S, 0.0f, 0.18f * S, 0.66f * S, 0.02f * S, 0.15f * S, RGB(25, 12, 18));
        face_bias = 0.01f; OBox(x, y, -0.065f * S, 0.0f, 0.18f * S, 0.62f * S, 0.015f * S, 0.13f * S, RGB(150, 20, 35)); face_bias = 0.0f;
    }
    OBox(x, y, st, 0.05f * S, 0.0f, 0.32f * S, 0.035f * S, 0.035f * S, legs);
    OBox(x, y, -st, -0.05f * S, 0.0f, 0.32f * S, 0.035f * S, 0.035f * S, legs);
    OBox(x, y, 0.0f, 0.0f, 0.32f * S, 0.6f * S, 0.06f * S, 0.1f * S, cloth);
    OBox(x, y, -st, -0.13f * S, 0.36f * S, 0.58f * S, 0.03f * S, 0.03f * S, cloth);
    OBox(x, y, st + swing * 0.12f * S, 0.13f * S, (0.36f + swing * 0.18f) * S, (0.58f + swing * 0.18f) * S, 0.03f * S, 0.03f * S, cloth);
    if (swing > 0.0f) OBox(x, y, (0.1f + swing * 0.2f) * S, 0.13f * S, (0.5f + swing * 0.1f) * S, (0.54f + swing * 0.1f) * S, 0.12f * S, 0.015f * S, RGB(200, 205, 215));
    OBox(x, y, 0.0f, 0.0f, 0.6f * S, 0.74f * S, 0.06f * S, 0.06f * S, skin);
    if (horns) { Cone(x, y, 0.0f, 0.04f * S, 0.74f * S, 0.82f * S, 0.015f * S, RGB(230, 220, 190)); Cone(x, y, 0.0f, -0.04f * S, 0.74f * S, 0.82f * S, 0.015f * S, RGB(230, 220, 190)); }
    if (crown) OBox(x, y, 0.0f, 0.0f, 0.74f * S, 0.79f * S, 0.055f * S, 0.055f * S, RGB(235, 195, 60));
    if (hero) {
        const char *hd = WornItem("Head"); COLORREF hc = hd ? ClothingColor(hd) : CLOTH_UNKNOWN;
        OBox(x, y, -0.01f * S, 0.0f, 0.72f * S, 0.78f * S, 0.066f * S, 0.066f * S, hc != CLOTH_UNKNOWN ? hc : HeroHairColor());
    }
    if (hero && HeroTrimColor() != CLOTH_UNKNOWN) { // trim at the hem and collar
        face_bias = 0.01f;
        OBox(x, y, 0.0f, 0.0f, 0.32f * S, 0.35f * S, 0.065f * S, 0.105f * S, HeroTrimColor());
        OBox(x, y, 0.0f, 0.0f, 0.57f * S, 0.6f * S, 0.065f * S, 0.105f * S, HeroTrimColor());
        face_bias = 0.0f;
    }
    Eyes(x, y, 0.063f * S, 0.025f * S, 0.69f * S, 0.01f * S, eye);
}

static void TreeMesh(const FpThing *t) {
    int s = t->seed, snow = snow_cover > 0.5f;
    float x = t->x + ((s * 37) % 21 - 10) / 100.0f, y = t->y + ((s * 53) % 21 - 10) / 100.0f;
    float h = 0.85f + (s * 29 % 30) / 100.0f, rot = (s % 7) * 0.4f, W = 1.0f;
    if (IsBigTree(s)) { h *= 1.9f; W = 1.6f; } // old giants: their crowns close over part of the sky
    Prism(x, y, 0.0f, 0.5f * h, 0.09f * W, 0.075f * W, 5, rot, RGB(110, 75, 45));
    { float sw = (0.02f + 0.01f * abs(wind_dir)) * sinf(GetTickCount() / 1300.0f + s * 0.7f); x += sw; y += sw * 0.4f; } // canopy sways, trunk stays
    if (IsDeciduousTree(s)) { // round broadleaf, the same trees as in play
        COLORREF leaf = snow ? RGB(205, 215, 225) : current_season == SEASON_FALL ? (s % 2 ? RGB(210, 120, 40) : RGB(220, 175, 50)) : RGB(80, 160, 60);
        Prism(x, y, 0.4f * h, 0.98f * h, 0.32f * W, 0.6f * W, 7, rot, leaf); // same ring at the seam, overlapped, so no gap
        Prism(x, y, 0.95f * h, 1.5f * h, 0.61f * W, 0.28f * W, 7, rot, leaf);
        if (TreeHasApples(s)) // the apples really on this tree
            for (int a = 0; a < 6; a++) {
                float an = rot + a * 1.05f, rr = 0.58f - (a % 2) * 0.12f;
                Prism(x + cosf(an) * rr, y + sinf(an) * rr, (0.85f + (a % 3) * 0.18f) * h, (0.93f + (a % 3) * 0.18f) * h, 0.05f, 0.05f, 4, an, RGB(205, 35, 30));
            }
    } else { // two-tier pine
        COLORREF needle = RGB(35, 115, 55);
        Prism(x, y, 0.3f * h, 1.15f * h, 0.62f * W, 0.22f * W, 7, rot, needle); // flat-topped skirt growing into the top cone
        Prism(x, y, 0.9f * h, 2.0f * h, 0.45f * W, 0.0f, 7, rot + 0.2f, snow ? RGB(225, 232, 240) : needle);
    }
}

static void RockMesh(const FpThing *t) {
    int s = t->seed;
    Prism(t->x, t->y, 0.0f, 0.32f + (s % 5) * 0.03f, 0.42f, 0.2f, 5, (s % 9) * 0.5f, RGB(120, 120, 128));
}

static void FireMesh(const FpThing *t) {
    float x = t->x, y = t->y, fl = 0.08f * sinf(GetTickCount() / 90.0f + t->seed);
    Prism(x, y, 0.0f, 0.1f, 0.32f, 0.3f, 6, 0.3f, RGB(95, 65, 40));
    face_glow = 1;
    DWORD now = GetTickCount(); // a smaller flame (the light it gives is unchanged)
    Prism(x, y, 0.1f, 0.42f + fl * 0.7f, 0.15f, 0.0f, 5, now / 400.0f, RGB(240, 120, 30));
    face_bias = 0.05f;
    Prism(x, y, 0.1f, 0.28f - fl * 0.7f, 0.085f, 0.0f, 5, -(now / 300.0f), RGB(255, 220, 90));
    face_bias = 0.08f; // flickering streaks inside: blue at the base, red and white tongues
    Prism(x, y, 0.1f, 0.07f, 0.07f, 0.0f, 5, now / 250.0f, RGB(90, 130, 255));
    static const COLORREF tongue[3] = { RGB(230, 50, 30), RGB(255, 250, 235), RGB(255, 170, 60) };
    Prism(x + 0.03f * sinf(now / 170.0f), y, 0.14f, 0.2f + fl, 0.03f, 0.0f, 3, now / 200.0f, tongue[(now / 140 + t->seed) % 3]);
    face_bias = 0.0f; face_glow = 0;
}

static void DetailMesh(const FpThing *t) {
    float x = t->x, y = t->y; DWORD now = GetTickCount();
    hcos = 1.0f; hsin = 0.0f;
    if (t->kind == FP_BARREL) {
        COLORREF wood = RGB(140, 90, 45), hoop = RGB(60, 40, 25);
        if (t->seed & 1) {
            Prism(x, y, 0.0f, 0.42f, 0.16f, 0.16f, 8, 0.0f, wood);
            face_bias = 0.01f;
            Prism(x, y, 0.07f, 0.1f, 0.165f, 0.165f, 8, 0.0f, hoop); Prism(x, y, 0.32f, 0.35f, 0.165f, 0.165f, 8, 0.0f, hoop);
            face_bias = 0.0f;
        } else { // smashed: staves on the ground
            OBox(x - 0.1f, y, 0.0f, 0.0f, 0.0f, 0.03f, 0.14f, 0.03f, wood); OBox(x + 0.08f, y + 0.1f, 0.0f, 0.0f, 0.0f, 0.03f, 0.03f, 0.12f, wood);
            OBox(x, y - 0.12f, 0.0f, 0.0f, 0.0f, 0.03f, 0.1f, 0.03f, hoop);
        }
    } else if (t->kind == FP_LIGHT) {
        if (t->seed & 1) { // torch on a post
            float fl = 0.03f * sinf(now / 80.0f + t->seed);
            Prism(x, y, 0.0f, 0.95f, 0.035f, 0.03f, 4, 0.0f, RGB(95, 65, 40));
            face_glow = 1;
            Prism(x, y, 0.95f, 1.12f + fl, 0.06f, 0.0f, 5, now / 300.0f, RGB(245, 140, 40));
            face_bias = 0.04f; Prism(x, y, 0.95f, 1.05f - fl, 0.035f, 0.0f, 5, -(now / 250.0f), RGB(255, 225, 110));
            face_bias = 0.0f; face_glow = 0;
        } else { // village fire pit: a stone ring round the fire
            Prism(x, y, 0.0f, 0.08f, 0.45f, 0.42f, 8, 0.2f, RGB(120, 120, 125));
            FpThing f = *t; f.kind = FP_FIRE; FireMesh(&f);
        }
    } else if (t->kind == FP_ALTAR) {
        static const COLORREF orb[4] = { RGB(255, 210, 90), RGB(150, 90, 230), RGB(120, 220, 120), RGB(60, 190, 200) };
        Prism(x, y, 0.0f, 0.12f, 0.42f, 0.4f, 4, 0.785f, RGB(110, 110, 118));
        Prism(x, y, 0.12f, 0.62f, 0.26f, 0.24f, 4, 0.785f, RGB(140, 140, 148));
        Prism(x, y, 0.62f, 0.7f, 0.34f, 0.34f, 4, 0.785f, RGB(120, 120, 128));
        face_glow = 1; Prism(x, y, 0.72f, 0.86f + 0.02f * sinf(now / 400.0f), 0.08f, 0.02f, 6, now / 900.0f, orb[t->seed & 3]); face_glow = 0;
    } else if (t->kind == FP_STALL) { // market stall: counter, posts and a blue canopy
        OBox(x, y, 0.0f, 0.0f, 0.0f, 0.45f, 0.75f, 0.3f, RGB(170, 115, 65));
        for (int k = 0; k < 4; k++) OBox(x, y, k < 2 ? -0.7f : 0.7f, k % 2 ? 0.28f : -0.28f, 0.45f, 1.05f, 0.03f, 0.03f, RGB(110, 75, 45));
        OBox(x, y, 0.0f, 0.0f, 1.05f, 1.1f, 0.85f, 0.42f, RGB(70, 115, 155));
    } else if (t->kind == FP_TENT) { // the merchant's red tent
        Prism(x, y, 0.0f, 1.25f, 0.85f, 0.0f, 4, 0.785f, RGB(220, 50, 40));
        face_bias = 0.02f; Prism(x, y, 0.0f, 0.04f, 0.87f, 0.87f, 4, 0.785f, RGB(235, 195, 60)); face_bias = 0.0f;
    }
}

static void HomeMesh(const FpThing *t, int night) {
    float x0 = t->x - HOME_HALF_X, x1 = t->x + HOME_HALF_X, y0 = t->y - HOME_HALF_Y, y1 = t->y + HOME_HALF_Y, wh = 0.95f, rz = 1.6f, o = 0.12f;
    float c[3] = { t->x, t->y, 0.7f };
    COLORREF wall = RGB(225, 205, 160), roof = RGB(165, 60, 45);
    float w1[4][3] = { { x0, y1, 0 }, { x1, y1, 0 }, { x1, y1, wh }, { x0, y1, wh } };
    float w2[4][3] = { { x0, y0, 0 }, { x1, y0, 0 }, { x1, y0, wh }, { x0, y0, wh } };
    float w3[5][3] = { { x0, y0, 0 }, { x0, y1, 0 }, { x0, y1, wh }, { x0, t->y, rz }, { x0, y0, wh } };
    float w4[5][3] = { { x1, y0, 0 }, { x1, y1, 0 }, { x1, y1, wh }, { x1, t->y, rz }, { x1, y0, wh } };
    AddFace(w1, 4, wall, c, 0); AddFace(w2, 4, wall, c, 0); AddFace(w3, 5, wall, c, 0); AddFace(w4, 5, wall, c, 0);
    float r1[4][3] = { { x0 - o, y1 + o, wh - 0.08f }, { x1 + o, y1 + o, wh - 0.08f }, { x1 + o, t->y, rz }, { x0 - o, t->y, rz } };
    float r2[4][3] = { { x0 - o, y0 - o, wh - 0.08f }, { x1 + o, y0 - o, wh - 0.08f }, { x1 + o, t->y, rz }, { x0 - o, t->y, rz } };
    AddFace(r1, 4, roof, c, 0); AddFace(r2, 4, roof, c, 0);
    face_bias = 0.08f;
    float door[4][3] = { { t->x - 0.17f, y1 + 0.01f, 0 }, { t->x + 0.17f, y1 + 0.01f, 0 }, { t->x + 0.17f, y1 + 0.01f, 0.58f }, { t->x - 0.17f, y1 + 0.01f, 0.58f } };
    AddFace(door, 4, RGB(105, 70, 40), c, 0);
    face_glow = night; // windows glow at night
    for (int k = 0; k < 2; k++) {
        float wx = k ? x1 + 0.01f : x0 - 0.01f;
        float w[4][3] = { { wx, t->y - 0.18f, 0.4f }, { wx, t->y + 0.18f, 0.4f }, { wx, t->y + 0.18f, 0.7f }, { wx, t->y - 0.18f, 0.7f } };
        AddFace(w, 4, night ? RGB(255, 205, 110) : RGB(85, 115, 150), c, 0);
    }
    face_glow = 0; face_bias = 0.0f;
}

static void GroundMesh(void) {
    COLORREF g = GroundColor();
    for (int r = (int)player_y - FP_RANGE; r <= (int)player_y + FP_RANGE; r++)
        for (int c = (int)player_x - FP_RANGE; c <= (int)player_x + FP_RANGE; c++) {
            if (r < 0 || c < 0 || r >= MAP_SIZE || c >= MAP_SIZE) continue;
            int t = VISUAL_MAP[r][c];
            COLORREF col = t == 12 ? RGB(50, 120, 210) : (t == 10 && !in_cave) ? GetTileColor(10, 0) : g;
            if (t == 12) { // water colours bleed slowly into each other, like wet watercolour
                float now = GetTickCount() / 1000.0f, w = sinf(now * 1.1f + r * 0.9f + c * 0.6f) + sinf(now * 0.7f - r * 0.4f + c * 1.1f);
                col = RGB(52 + (int)(w * 9.0f), 122 + (int)(w * 11.0f), 212 + (int)(w * 6.0f));
            }
            else col = Dim(col, 0.96f + (abs(RainHash(r * 977 + c * 131)) % 5) * 0.01f);
            float v[4][3] = { { (float)c, (float)r, 0 }, { c + 1.0f, (float)r, 0 }, { c + 1.0f, r + 1.0f, 0 }, { (float)c, r + 1.0f, 0 } };
            AddFace(v, 4, col, NULL, 1);
            float fdx = c + 0.5f - player_x, fdy = r + 0.5f - player_y;
            if (t != 1 && t != 10 && t != 11 && t != 12 && !in_cave && snow_cover < 0.5f && fdx * fdx + fdy * fdy < 64.0f &&
                (current_biome == BIOME_GRASSLANDS || current_biome == BIOME_FOREST || current_biome == BIOME_PRAIRIE || current_biome == BIOME_OASIS) &&
                abs(RainHash(r * 613 + c * 37)) % 7 == 0) { // a flower nodding in the breeze
                static const COLORREF petal[4] = { RGB(240, 90, 120), RGB(250, 215, 70), RGB(170, 120, 240), RGB(250, 250, 245) };
                int hsh = abs(RainHash(r * 31 + c * 7));
                float nod = 0.02f * sinf(GetTickCount() / 900.0f + r * 0.8f + c * 0.5f), fx = c + 0.2f + (hsh % 60) / 100.0f, fy = r + 0.2f + (hsh / 60 % 60) / 100.0f;
                hcos = 1.0f; hsin = 0.0f;
                OBox(fx, fy, 0.0f, 0.0f, 0.0f, 0.13f, 0.006f, 0.006f, RGB(60, 130, 50));
                OBox(fx + nod, fy, 0.0f, 0.0f, 0.13f, 0.16f, 0.03f, 0.03f, petal[hsh % 4]);
            }
        }
}

// Past the polygon tiles the ground fades into the haze at the horizon.
static void DrawGround(HDC hdc, float lit) {
    int top = horizon < 0 ? 0 : horizon;
    Box(hdc, 0, top, WINDOW_WIDTH, WINDOW_HEIGHT, Mix(Dim(GroundColor(), lit), haze, 0.85f));
}

static int FarFirst(const void *a, const void *b) {
    float da = ((const FpThing *)a)->depth, db = ((const FpThing *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

// The gaze lingers on one thing for a few seconds, then picks another: the
// bobber while fishing, otherwise the fire, the familiar, animals and
// butterflies before trees and rocks, with a long look up at the sky now and then.
static void UpdateGaze(int fresh) {
    static DWORD gaze_until; static int gaze_n; static float target_yaw, target_pitch;
    static const int interest[10] = { 10, 6, 18, 50, 40, 32, 45, 95, 60, 30 };
    DWORD now = GetTickCount();
    if (fresh) {
        yaw = player_facing == FACE_DOWN ? 1.5708f : player_facing == FACE_UP ? -1.5708f : player_facing == FACE_LEFT ? 3.14159f : 0.0f;
        pitch = 0.0f; target_yaw = yaw; target_pitch = 0.0f; gaze_until = now + 2500;
    }
    if (now >= gaze_until) {
        gaze_until = now + GAZE_MS; gaze_n++;
        int pick = -1, best = -1000;
        for (int i = 0; i < thing_count; i++) {
            float dx = things[i].x - player_x, dy = things[i].y - player_y;
            int s = interest[things[i].kind] + RainHash(i * 17 + gaze_n * 101) % 25 - (int)(sqrtf(dx * dx + dy * dy) * 1.5f);
            if (s > best) { best = s; pick = i; }
        }
        int fishing = pick >= 0 && things[pick].kind == FP_BOBBER;
        float nod = (RainHash(gaze_n * 7 + 3) % 100 - 50) / 350.0f; // look a little up or down, never rigidly level
        if (!fishing && !in_cave && (gaze_n % 4 == 0 || pick < 0)) { target_pitch = 0.6f; target_yaw = yaw + 0.5f; }
        else if (pick >= 0) {
            target_yaw = atan2f(things[pick].y - player_y, things[pick].x - player_x);
            target_pitch = fishing ? -0.15f : (things[pick].kind == FP_TREE || things[pick].kind == FP_HOME ? 0.12f : -0.05f) + nod;
            if (!fishing && gaze_n % 5 == 2) target_pitch = -0.32f; // down at the ground now and then
        }
    }
    // An animal that wanders close is "heard": the head turns to it faster.
    static DWORD heard_at[64], fast_until;
    for (int i = 0; i < thing_count; i++) {
        if (things[i].kind != FP_CRITTER && things[i].kind != FP_BUTTERFLY) continue;
        float dx = things[i].x - player_x, dy = things[i].y - player_y;
        int key = (things[i].kind == FP_CRITTER ? things[i].seed >> 3 : 32 + things[i].seed) & 63;
        if (dx * dx + dy * dy > 25.0f) continue;
        if (now - heard_at[key] > 15000) {
            target_yaw = atan2f(dy, dx); target_pitch = -0.08f;
            gaze_until = now + 3500; fast_until = now + 1500;
        }
        heard_at[key] = now;
    }
    float rate = now < fast_until ? 0.05f : 0.012f;
    yaw += Wrap(target_yaw - yaw) * rate;
    pitch += (target_pitch - pitch) * rate * 1.25f;
}

// Wind in AFK space: gameplay wind_dir pushes along screen x, which is the
// world's (+x, -y) diagonal. Returns how fast that wind crosses the view.
static float WindAcross(float *wx, float *wy) {
    float w = (float)wind_dir * 0.7071f;
    *wx = w; *wy = -w;
    return -*wx * sinf(yaw) + *wy * cosf(yaw);
}

static void CloudPuff(HDC hdc, int cx, int cy, int w, COLORREF c, int alpha, int seed) {
    int x0 = cx - w, y0 = cy - w / 2, W = 2 * w, H = w;
    if (W < 6 || x0 + W < 0 || x0 > WINDOW_WIDTH || y0 + H < 0) return;
    HDC t = CreateCompatibleDC(hdc); HBITMAP b = CreateCompatibleBitmap(hdc, W, H); HGDIOBJ ob = SelectObject(t, b);
    BitBlt(t, 0, 0, W, H, hdc, x0, y0, SRCCOPY);
    float tm = GetTickCount() / 9000.0f;
    for (int k = 0; k < 5; k++) { // each lump swells and shrinks slowly, so no two clouds share a shape
        float n = 0.5f + 0.25f * sinf(seed * 0.37f + k * 2.1f + tm) + 0.25f * cosf(seed * 0.11f + k * 1.3f - tm * 0.7f);
        int lw = (int)(W * (0.28f + 0.22f * n)), lh = (int)(H * (0.5f + 0.45f * n));
        int lx = (int)(W * (0.1f + 0.2f * k)) - lw / 2 + W / 10, ly = H - lh;
        if (lx < 0) lx = 0;
        if (lx + lw > W - 1) lw = W - 1 - lx;
        Blob(t, lx, ly, lx + lw, H - 1, c);
    }
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
    AlphaBlend(hdc, x0, y0, W, H, t, 0, 0, W, H, bf);
    SelectObject(t, ob); DeleteObject(b); DeleteDC(t);
}

// Clouds sit high over an 80-tile patch around the hero and drift with the
// calculated wind; some are see-through like the ghost, some solid.
static void DrawClouds(HDC hdc, float light) {
    if (in_cave || current_biome == BIOME_CAVE) return;
    float wx, wy; WindAcross(&wx, &wy);
    float t = GetTickCount() / 1000.0f, k = light < 0.25f ? 0.25f : light;
    int n = IsAdverseWeather() ? 12 : 3 + (int)(weather_intensity * 6.0f);
    for (int i = 0; i < n; i++) {
        int hs = RainHash(i * 977 + 31);
        // Pinned to bearings like the stars, drifting slowly with the wind, so a
        // turning gaze slides them smoothly and they never jump or vanish.
        float az = (hs % 6283) / 1000.0f + t * 0.004f * (wx - wy), el = 0.10f + (hs / 6283 % 300) / 1000.0f;
        int x, y; SkyPoint(az, el, &x, &y);
        if (x < -400) continue;
        int solid = i % 3 != 0, w = (int)(focal * (0.10f + (hs % 5) * 0.04f) * (0.7f + el));
        COLORREF c = IsAdverseWeather() ? RGB(120, 125, 135) : solid ? RGB(245, 245, 250) : RGB(215, 220, 230);
        CloudPuff(hdc, x, y, w, Dim(c, k + 0.15f), solid ? 215 : 95, hs);
    }
}

// Rain and snow slant and drift with the wind; leaves blow off the trees in
// autumn or when it's gusty.
static void DrawWeather(HDC hdc) {
    if (in_cave || current_biome == BIOME_CAVE) return;
    DWORD now = GetTickCount();
    float wx, wy, across = WindAcross(&wx, &wy);
    // Particles hang in the world: turning or tilting the gaze slides them
    // across the screen instead of dragging them along with the camera.
    int pan = (int)(yaw / FP_FOV * WINDOW_WIDTH), tilt = WINDOW_HEIGHT / 2 - horizon;
    if (IsAdverseWeather()) {
        int snow = current_season == SEASON_WINTER || current_biome == BIOME_TUNDRA, n = 80 + (int)(weather_intensity * 120.0f);
        for (int i = 0; i < n; i++) {
            int hsh = RainHash(i * 613 + 9), z = i % 3; // z: 0 far and small .. 2 near and big
            int x = (int)(((hsh % WINDOW_WIDTH) + (int)(now * across * (1 + z) / (snow ? 80.0f : 24.0f)) - pan + 100 * WINDOW_WIDTH) % WINDOW_WIDTH);
            int y = (int)(((hsh / 3 % WINDOW_HEIGHT) + (int)(now / (snow ? 12 : 2)) * (1 + z) - tilt + 100 * WINDOW_HEIGHT) % WINDOW_HEIGHT);
            if (snow) Box(hdc, x, y, x + 1 + z * 2, y + 1 + z * 2, RGB(240 - (2 - z) * 25, 240 - (2 - z) * 22, 245 - (2 - z) * 15));
            else Stroke(hdc, x, y, x + (int)(across * (1 + z)), y + 7 + z * 8, z == 2 ? 2 : 1, z ? RGB(170, 185, 210) : RGB(140, 155, 180));
        }
    }
    int trees = 0;
    for (int i = 0; i < thing_count; i++) trees += things[i].kind == FP_TREE;
    if (!trees || (current_season != SEASON_FALL && abs(wind_dir) < 4) || snow_cover > 0.5f) return;
    COLORREF lc[3] = { RGB(210, 120, 40), RGB(220, 175, 50), RGB(90, 150, 55) };
    for (int i = 0; i < 26; i++) {
        int hsh = RainHash(i * 311 + 77), z = i % 3, s = 2 + z * 2;
        float fall = now * (1 + z) / (70.0f + hsh % 40);
        int x = (int)(((hsh % WINDOW_WIDTH) + (int)(now * across * (1 + z) / 18.0f) + (int)(sinf(fall / 20.0f + i) * 30.0f) - pan + 100 * WINDOW_WIDTH) % WINDOW_WIDTH);
        int y = (int)(((hsh / 7 % WINDOW_HEIGHT) + (int)fall - tilt + 100 * WINDOW_HEIGHT) % WINDOW_HEIGHT);
        int f = (int)(sinf(now / 150.0f + i) * s);
        POINT p[4] = { { x - s, y }, { x, y - s / 2 + f / 2 }, { x + s, y }, { x, y + s / 2 - f / 2 } };
        Shape(hdc, p, 4, lc[current_season == SEASON_FALL ? i % 2 : 2]);
    }
}

void DrawAfkView(HDC hdc) {
    static int was_on = 0;
    int ms = AfkViewMs();
    if (ms < FP_ZOOM_MS) { was_on = 0; return; }
    if (ms < FP_ZOOM_MS + FP_DIP_MS) { FillWindowAlpha(hdc, RGB(0, 0, 0), (ms - FP_ZOOM_MS) * 255 / FP_DIP_MS); was_on = 0; return; }
    eye_x = player_x; eye_y = player_y; eye_z = FP_EYE;
    GatherThings();
    UpdateGaze(!was_on); was_on = 1;
    focal = (WINDOW_WIDTH / 2) / tanf(FP_FOV / 2);
    horizon = (int)(WINDOW_HEIGHT * 0.55f + pitch * WINDOW_HEIGHT * 0.7f);
    float light = (float)fabs(sin(day_night_cycle_accumulator));
    if (NightVisionActive() && light < 0.6f) light = 0.6f;
    if (in_cave) light = 0.35f;
    float lit = 0.3f + 0.7f * light;
    scene_lit = lit; haze = in_cave ? RGB(18, 16, 14) : Dim(RGB(185, 210, 235), 0.2f + 0.8f * light);
    { // darkness closes in at night as far as the hero's race can see
        float nv = in_cave ? 1.0f : (0.55f - light) / 0.35f;
        nv = nv < 0.0f ? 0.0f : nv > 1.0f ? 1.0f : nv;
        float see = RaceVisionTiles() * 2.0f;
        fog_far = FP_RANGE + (see - FP_RANGE) * nv;
        if (fog_far < 8.0f) fog_far = 8.0f;
        fog_near = 6.0f * (1.0f - nv) + 0.35f * fog_far * nv; fog_cap = 0.7f + 0.22f * nv;
    }
    DrawSky(hdc, light);
    DrawClouds(hdc, light);
    DrawGround(hdc, lit);
    face_count = 0;
    GroundMesh();
    for (int i = 0; i < thing_count; i++) {
        int sx, sy, k = things[i].kind;
        if (!Project(things[i].x, things[i].y, things[i].z, &sx, &sy, &things[i].depth)) things[i].depth = -1.0f;
        if (k == FP_TREE) TreeMesh(&things[i]);
        else if (k == FP_ROCK) RockMesh(&things[i]);
        else if (k == FP_HOME) HomeMesh(&things[i], light < 0.35f);
        else if (k == FP_FIRE) FireMesh(&things[i]);
        else if (k == FP_BUTTERFLY || k == FP_CRITTER || k == FP_FAMILIAR) AnimalMesh(&things[i]);
        else if (k == FP_ENEMY || k == FP_NPC) HumanMesh(&things[i]);
        else if (k >= FP_BARREL) DetailMesh(&things[i]);
        else if (things[i].depth > 0.0f) AddSprite(i);
    }
    bobber_sx = -1;
    DrawFaces(hdc, lit);
    fog_near = 6.0f; fog_far = FP_RANGE; fog_cap = 0.7f; // rooms drawn with the same pipeline keep the daytime fog
    DrawWeather(hdc);
    if (bobber_sx >= 0) { // the rod in hand, line out to the bobber
        int tip_x = WINDOW_WIDTH * 3 / 5, tip_y = WINDOW_HEIGHT * 2 / 5;
        Stroke(hdc, WINDOW_WIDTH * 5 / 6, WINDOW_HEIGHT + 6, tip_x, tip_y, 5, RGB(110, 75, 40));
        Stroke(hdc, tip_x, tip_y, bobber_sx, bobber_sy, 1, RGB(225, 225, 225));
    }
    int fade_in = ms - FP_ZOOM_MS - FP_DIP_MS;
    if (fade_in < FP_DIP_MS) FillWindowAlpha(hdc, RGB(0, 0, 0), 255 - fade_in * 255 / FP_DIP_MS);
}

// Walk-in village homes: X at a cottage door steps into a polygon room drawn
// with the same flat-shaded faces as the AFK view, from a fixed camera by the
// doorway. Inside, everything is a menu (TM_HOME): sleep, set out furniture,
// or Leave; B / ESC always steps back outside, so nobody gets stuck indoors.
#define ROOM_HX 2.6f
#define ROOM_HY 2.0f
int inside_home = -1;
static float out_x, out_y, room_cx, room_cy;

static int HomeDoorNear(void) {
    for (int i = 0; i < 5; i++) {
        float hx, hy;
        if (!VillageHomeCentre(i, &hx, &hy)) continue;
        float dx = player_x + HOME_FEET_OFFSET - hx, dy = player_y + HOME_FEET_OFFSET - (hy + HOME_HALF_Y);
        if (dx * dx + dy * dy < 0.8f) return i;
    }
    return -1;
}

void LeaveHome(void) {
    if (inside_home < 0) return;
    inside_home = -1; player_x = out_x; player_y = out_y;
    if (town_menu == TM_HOME) town_menu = TM_NONE;
    strcpy(arpg_action_log, "HOME: You step back outside.");
}

int InteriorBlocks(float x, float y) {
    if (x < room_cx - ROOM_HX + 0.25f || x > room_cx + ROOM_HX - 0.25f || y < room_cy - ROOM_HY + 0.3f || y > room_cy + ROOM_HY - 0.15f) return 1;
    if (x < room_cx - ROOM_HX + 1.25f && y < room_cy - ROOM_HY + 2.1f) return 1;                    // bed
    if (fabsf(x - (room_cx + 1.0f)) < 0.6f && fabsf(y - (room_cy + 0.2f)) < 0.45f) return 1;       // table
    if (x > room_cx + ROOM_HX - 1.0f && y < room_cy - ROOM_HY + 0.9f) return 1;                    // hearth
    if (inside_home == 4) {
        int f = player_home[5];
        if ((f & 1) && x > room_cx + ROOM_HX - 0.75f && fabsf(y - (room_cy + 1.0f)) < 0.6f) return 1;
        if ((f & 2) && x < room_cx - ROOM_HX + 1.1f && fabsf(y - (room_cy + 1.2f)) < 0.45f) return 1;
        if ((f & 16) && fabsf(x - (room_cx + 0.9f)) < 0.5f && y < room_cy - ROOM_HY + 0.75f) return 1;
    }
    return 0;
}

int HomeInteract(void) {
    if (inside_home >= 0) { if (town_menu != TM_HOME) OpenTownMenu(TM_HOME); return 1; }
    int i = HomeDoorNear();
    if (i < 0 || (i == 4 && player_home[0] != 2)) return 0;
    StoreEnemy();
    for (int k = 0; k < MAX_ENEMIES; k++) {
        float dx = enemies[k].x - player_x, dy = enemies[k].y - player_y;
        if (enemies[k].hearts > 0.0f && enemies[k].angry && dx * dx + dy * dy < 36.0f) { strcpy(arpg_action_log, "HOME: Not with monsters on your heels!"); return 1; }
    }
    float hx, hy; VillageHomeCentre(i, &hx, &hy);
    inside_home = i; out_x = player_x; out_y = player_y; room_cx = hx; room_cy = hy;
    player_x = room_cx - 0.3f; player_y = room_cy + 0.9f;
    OpenTownMenu(TM_HOME);
    if (i == 4) { if (!PlaceFurniture()) strcpy(arpg_action_log, "HOME: Welcome home. Furniture you buy goes in when you come inside."); }
    return 1;
}

static void FloorRect(float x0, float y0, float x1, float y1, float z, COLORREF col, int ground) {
    float v[4][3] = { { x0, y0, z }, { x1, y0, z }, { x1, y1, z }, { x0, y1, z } };
    AddFace(v, 4, col, NULL, ground);
}
// Walls face along +x / +y when the offset puts the centre on the -x / -y side.
static void WallX(float x, float y0, float y1, float z0, float z1, COLORREF col, float off) {
    float c[3] = { x + off, (y0 + y1) / 2, (z0 + z1) / 2 }, v[4][3] = { { x, y0, z0 }, { x, y1, z0 }, { x, y1, z1 }, { x, y0, z1 } };
    AddFace(v, 4, col, c, 0);
}
static void WallY(float y, float x0, float x1, float z0, float z1, COLORREF col, float off) {
    float c[3] = { (x0 + x1) / 2, y + off, (z0 + z1) / 2 }, v[4][3] = { { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } };
    AddFace(v, 4, col, c, 0);
}

static void RoomCamera(float ex, float ey, float ez, float fov, int hz) {
    eye_x = ex; eye_y = ey; eye_z = ez; yaw = -1.5707963f;
    focal = (WINDOW_WIDTH / 2) / tanf(fov / 2); horizon = hz;
    face_count = 0; face_bias = 0.0f; face_glow = 0; hcos = 1.0f; hsin = 0.0f;
}

static void RoomShell(float x0, float y0, float x1, float y1, float wh, COLORREF fa, COLORREF fb, COLORREF wall, int stone) {
    if (stone) {
        int nx = (int)(x1 - x0), ny = (int)(y1 - y0);
        for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) FloorRect(x0 + i, y0 + j, x0 + i + 1, y0 + j + 1, 0, (i + j) % 2 ? fa : fb, 1);
    } else {
        int n = (int)((x1 - x0) / 0.5f);
        for (int i = 0; i < n; i++) FloorRect(x0 + i * 0.5f, y0, x0 + i * 0.5f + 0.5f, y1, 0, i % 2 ? fa : fb, 1);
    }
    WallY(y0, x0, x1, 0, wh, wall, -1.0f);
    WallX(x0, y0, y1, 0, wh, Dim(wall, 0.82f), -1.0f);
    WallX(x1, y0, y1, 0, wh, Dim(wall, 0.82f), 1.0f);
    face_bias = 0.3f; WallY(y0 + 0.02f, x0, x1, 0, 0.12f, Dim(wall, 0.6f), -1.0f); face_bias = 0.0f;
}

static void RoomWindow(float y0, float wx0, float wx1, float z0, float z1, int night) {
    COLORREF frame = RGB(95, 65, 40);
    float mx = (wx0 + wx1) / 2, mz = (z0 + z1) / 2;
    face_bias = 0.3f; WallY(y0 + 0.01f, wx0, wx1, z0, z1, frame, -1.0f);
    face_bias = 0.4f; face_glow = 1;
    WallY(y0 + 0.02f, wx0 + 0.07f, wx1 - 0.07f, z0 + 0.07f, z1 - 0.07f, night ? RGB(20, 26, 62) : RGB(160, 205, 240), -1.0f);
    face_glow = 0; face_bias = 0.5f;
    WallY(y0 + 0.03f, mx - 0.03f, mx + 0.03f, z0 + 0.07f, z1 - 0.07f, frame, -1.0f);
    WallY(y0 + 0.03f, wx0 + 0.07f, wx1 - 0.07f, mz - 0.03f, mz + 0.03f, frame, -1.0f);
    face_bias = 0.0f;
}

static void DrawHomeFurniture(float x0, float x1, float y0, float y1, float cx, float cy) {
    int f = player_home[5];
    static const COLORREF books[3] = { RGB(150, 40, 40), RGB(40, 70, 130), RGB(60, 110, 60) };
    if (f & 1) { // bookshelf on the right wall
        OBox(x1 - 0.25f, cy + 1.0f, 0, 0, 0.0f, 1.6f, 0.18f, 0.5f, RGB(100, 65, 38));
        face_bias = 0.2f;
        for (int k = 0; k < 3; k++) for (int b = 0; b < 4; b++)
            OBox(x1 - 0.45f, cy + 0.68f + b * 0.21f, 0, 0, 0.3f + k * 0.5f, 0.64f + k * 0.5f, 0.03f, 0.08f, books[(b + k) % 3]);
        face_bias = 0.0f;
    }
    if (f & 2) { // armchair by the door
        COLORREF c = RGB(120, 50, 60);
        OBox(x0 + 0.6f, cy + 1.2f, 0, 0, 0.0f, 0.35f, 0.3f, 0.3f, c);
        OBox(x0 + 0.33f, cy + 1.2f, 0, 0, 0.35f, 0.85f, 0.07f, 0.3f, Dim(c, 0.8f));
        OBox(x0 + 0.6f, cy + 0.93f, 0, 0, 0.35f, 0.5f, 0.3f, 0.05f, Dim(c, 0.9f));
        OBox(x0 + 0.6f, cy + 1.47f, 0, 0, 0.35f, 0.5f, 0.3f, 0.05f, Dim(c, 0.9f));
    }
    if (f & 4) { // potted plant in the front-right corner
        Prism(x1 - 0.4f, y1 - 0.4f, 0.0f, 0.3f, 0.14f, 0.11f, 6, 0.0f, RGB(180, 90, 55));
        Prism(x1 - 0.4f, y1 - 0.4f, 0.3f, 0.8f, 0.26f, 0.0f, 5, 0.4f, RGB(60, 130, 55));
    }
    if (f & 8) { // painting on the back wall
        face_bias = 0.3f; WallY(y0 + 0.01f, cx - 1.4f, cx - 0.8f, 1.0f, 1.5f, RGB(150, 110, 40), -1.0f);
        face_bias = 0.4f; WallY(y0 + 0.02f, cx - 1.35f, cx - 0.85f, 1.25f, 1.45f, RGB(120, 170, 220), -1.0f);
        WallY(y0 + 0.02f, cx - 1.35f, cx - 0.85f, 1.05f, 1.25f, RGB(80, 140, 70), -1.0f);
        face_bias = 0.0f;
    }
    if (f & 16) { // wardrobe against the back wall
        OBox(cx + 0.9f, y0 + 0.3f, 0, 0, 0.0f, 1.5f, 0.4f, 0.25f, RGB(90, 58, 34));
        face_bias = 0.2f; WallY(y0 + 0.56f, cx + 0.88f, cx + 0.92f, 0.1f, 1.4f, RGB(60, 38, 22), -1.0f); face_bias = 0.0f;
    }
    if (f & 32) { // hanging lantern
        OBox(cx - 0.3f, cy + 0.6f, 0, 0, 1.75f, 2.0f, 0.01f, 0.01f, RGB(50, 50, 50));
        face_glow = 1; Prism(cx - 0.3f, cy + 0.6f, 1.5f, 1.75f, 0.08f, 0.08f, 6, 0.0f, RGB(255, 210, 110)); face_glow = 0;
        scene_lit = 1.0f;
    }
}

void DrawHomeInterior(HDC hdc) {
    if (inside_home < 0) return;
    if (town_menu != TM_HOME) { LeaveHome(); return; }
    float x0 = room_cx - ROOM_HX, x1 = room_cx + ROOM_HX, y0 = room_cy - ROOM_HY, y1 = room_cy + ROOM_HY, cx = room_cx, cy = room_cy;
    int night = (fabs(sin(day_night_cycle_accumulator)) < 0.35f); DWORD now = GetTickCount();
    COLORREF wood = RGB(120, 80, 45), dark = RGB(85, 55, 32), stone = RGB(130, 125, 120), wall = RGB(222, 200, 158);
    if (inside_home == 4) wall = HomeWallColor(player_home[4]);
    static const COLORREF blankets[4] = { RGB(170, 50, 45), RGB(60, 100, 160), RGB(70, 130, 70), RGB(150, 110, 50) };
    Box(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, RGB(26, 18, 12));
    RoomCamera(cx, y1 + 3.0f, 2.6f, 1.55f, (int)(WINDOW_HEIGHT * 0.16f));
    scene_lit = night ? 0.75f : 0.95f; haze = RGB(26, 18, 12);
    RoomShell(x0, y0, x1, y1, 2.0f, RGB(150, 105, 62), RGB(136, 94, 55), wall, 0);
    RoomWindow(y0, cx - 0.6f, cx + 0.2f, 0.85f, 1.55f, night);
    face_bias = -0.6f; OBox(cx - 0.3f, cy + 0.6f, 0, 0, 0.0f, 0.015f, 0.8f, 0.55f, RGB(150, 60, 60)); face_bias = 0.0f; // rug
    // bed in the back-left corner
    OBox(x0 + 0.6f, y0 + 0.08f, 0, 0, 0.0f, 0.75f, 0.5f, 0.06f, dark);
    OBox(x0 + 0.6f, y0 + 1.0f, 0, 0, 0.0f, 0.3f, 0.5f, 0.95f, wood);
    OBox(x0 + 0.6f, y0 + 1.15f, 0, 0, 0.3f, 0.4f, 0.48f, 0.75f, blankets[inside_home & 3]);
    face_bias = 0.3f; OBox(x0 + 0.6f, y0 + 0.28f, 0, 0, 0.3f, 0.42f, 0.38f, 0.15f, RGB(235, 235, 225)); face_bias = 0.0f;
    // table, stools and a bowl of apples
    for (int k = 0; k < 4; k++) OBox(cx + 1.0f + ((k & 1) ? 0.45f : -0.45f), cy + 0.2f + ((k & 2) ? 0.28f : -0.28f), 0, 0, 0.0f, 0.55f, 0.04f, 0.04f, dark);
    OBox(cx + 1.0f, cy + 0.2f, 0, 0, 0.55f, 0.62f, 0.55f, 0.38f, wood);
    Prism(cx + 1.0f, cy + 0.2f, 0.62f, 0.7f, 0.1f, 0.13f, 6, 0.0f, RGB(200, 60, 40));
    Prism(cx + 0.25f, cy + 0.25f, 0.0f, 0.38f, 0.15f, 0.15f, 6, 0.0f, wood);
    Prism(cx + 1.0f, cy + 0.9f, 0.0f, 0.38f, 0.15f, 0.15f, 6, 0.0f, wood);
    // shelf with jars on the left wall
    OBox(x0 + 0.12f, cy + 0.6f, 0, 0, 1.0f, 1.05f, 0.1f, 0.5f, wood);
    for (int k = 0; k < 3; k++) Prism(x0 + 0.12f, cy + 0.3f + k * 0.3f, 1.05f, 1.2f, 0.05f, 0.05f, 5, 0.0f, k == 1 ? RGB(90, 120, 160) : RGB(190, 160, 110));
    // hearth in the back-right corner with a flickering fire
    OBox(x1 - 0.5f, y0 + 0.4f, 0, 0, 0.0f, 1.1f, 0.45f, 0.35f, stone);
    OBox(x1 - 0.5f, y0 + 0.25f, 0, 0, 1.1f, 2.0f, 0.3f, 0.2f, Dim(stone, 0.8f));
    face_bias = 0.2f; OBox(x1 - 0.5f, y0 + 0.76f, 0, 0, 0.05f, 0.55f, 0.28f, 0.02f, RGB(25, 18, 14));
    face_glow = 1; face_bias = 0.4f;
    Prism(x1 - 0.5f, y0 + 0.8f, 0.05f, 0.38f + 0.08f * sinf(now / 90.0f), 0.16f, 0.0f, 5, now / 300.0f, RGB(240, 140, 40));
    face_bias = 0.5f;
    Prism(x1 - 0.5f, y0 + 0.82f, 0.05f, 0.25f + 0.05f * sinf(now / 70.0f), 0.08f, 0.0f, 5, -(now / 250.0f), RGB(255, 225, 90));
    face_glow = 0; face_bias = 0.0f;
    // low front wall either side of the door
    WallY(y1, x0, cx - 0.55f, 0.0f, 0.3f, Dim(wall, 0.9f), -1.0f);
    WallY(y1, cx + 0.55f, x1, 0.0f, 0.3f, Dim(wall, 0.9f), -1.0f);
    // the villager who lives here, and the hero
    if (inside_home < 4) { FpThing r = { x0 + 1.7f, y0 + 1.6f, 0.0f, 0.0f, FP_NPC, (13 << 4) | (inside_home & 3) }; HumanMesh(&r); }
    else DrawHomeFurniture(x0, x1, y0, y1, cx, cy);
    FpThing h = { player_x, player_y, 0.0f, 0.0f, FP_NPC, (14 << 4) | 15 };
    HumanMesh(&h);
    DrawFaces(hdc, scene_lit);
}

// Shop and castle rooms: the same polygon room, framed so the floor meets the
// back wall at floor_y, where each shop's own props stand.
void DrawShopRoom(HDC hdc, int stone, int shrine, int floor_y) {
    float x0 = -5.5f, x1 = 5.5f, y0 = -3.0f, y1 = 3.0f, ez = 2.3f, ey = y1 + 2.0f, fov = 1.6f;
    float f = (WINDOW_WIDTH / 2) / tanf(fov / 2);
    int hz = floor_y - (int)(ez / (ey - y0) * f);
    COLORREF wall = shrine ? RGB(48, 36, 62) : stone ? RGB(112, 112, 120) : RGB(132, 90, 54);
    COLORREF fa = shrine ? RGB(40, 32, 46) : stone ? RGB(92, 88, 84) : RGB(104, 70, 42);
    Box(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Dim(wall, 0.45f));
    RoomCamera(0.0f, ey, ez, fov, hz);
    scene_lit = 1.0f; haze = Dim(wall, 0.5f);
    RoomShell(x0, y0, x1, y1, 3.6f, fa, Dim(fa, 0.85f), wall, stone || shrine);
    if (!shrine) RoomWindow(y0, -1.0f, 0.6f, 0.9f, 1.9f, (fabs(sin(day_night_cycle_accumulator)) < 0.35f));
    for (int k = 0; k < 4; k++) OBox(x0 + 1.4f + k * 2.75f, 0.0f, 0, 0, 3.4f, 3.6f, 0.12f, 3.0f, Dim(wall, 0.55f));
    DrawFaces(hdc, 1.0f);
}
