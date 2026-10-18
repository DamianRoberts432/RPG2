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

enum { FP_TREE, FP_ROCK, FP_HOME, FP_FIRE, FP_BUTTERFLY, FP_CRITTER, FP_FAMILIAR, FP_BOBBER };
typedef struct { float x, y, z, depth; int kind, seed; } FpThing;
static FpThing things[FP_MAX_THINGS];
static int thing_count, horizon, bobber_sx = -1, bobber_sy;
static float yaw, pitch, focal;

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
    float dx = x - player_x, dy = y - player_y, cs = cosf(yaw), sn = sinf(yaw);
    float f = dx * cs + dy * sn, side = -dx * sn + dy * cs;
    if (f < 0.3f) return 0;
    *sx = WINDOW_WIDTH / 2 + (int)(side / f * focal);
    *sy = horizon + (int)((FP_EYE - z) / f * focal);
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
    if (village_npc_count > 0 && !in_cave) {
        static const int hx[4] = { 11, 15, 19, 15 }, hy[4] = { 11, 10, 11, 18 };
        int shift = (current_screen_index % 3) - 1;
        for (int i = 0; i < 4; i++) AddThing(hx[i] + shift + 0.5f, hy[i] + 0.5f, 0.0f, FP_HOME, i);
    }
    for (int i = 0; i < MAX_CAMPFIRES; i++)
        if (campfires[i].active && campfires[i].screen_id == current_screen_index) AddThing(campfires[i].x, campfires[i].y, 0.0f, FP_FIRE, i);
    if (!in_cave && !IsAdverseWeather())
        for (int i = 0; i < MAX_BUTTERFLIES; i++)
            if (butterflies[i].active) AddThing(butterflies[i].x, butterflies[i].y, 0.35f + butterflies[i].z / 40.0f, FP_BUTTERFLY, i);
    for (int i = 0; i < MAX_CRITTERS; i++)
        if (critters[i].active && !critters[i].hidden) AddThing(critters[i].x, critters[i].y, 0.0f, FP_CRITTER, critters[i].type);
    if (familiar_visible) AddThing(familiar_world_x, familiar_world_y, 0.0f, FP_FAMILIAR, familiar_visible - 1);
    float bx, by; int nibble;
    if (FishingBobberPos(&bx, &by, &nibble)) AddThing(bx, by, 0.0f, FP_BOBBER, nibble);
}

static void SkyPoint(float az, float el, int *x, int *y) {
    float rel = Wrap(az - yaw);
    *x = fabsf(rel) < FP_FOV * 0.7f ? WINDOW_WIDTH / 2 + (int)(tanf(rel) / tanf(FP_FOV / 2) * WINDOW_WIDTH / 2) : -1000;
    *y = horizon - (int)(el * WINDOW_HEIGHT * 0.8f);
}

static void DrawSky(HDC hdc, float light) {
    int h = horizon < 0 ? 0 : horizon > WINDOW_HEIGHT ? WINDOW_HEIGHT : horizon;
    if (in_cave || current_biome == BIOME_CAVE) { Box(hdc, 0, 0, WINDOW_WIDTH, h, RGB(28, 24, 22)); return; }
    int gloomy = IsAdverseWeather();
    COLORREF top = Mix(RGB(6, 9, 26), RGB(70, 130, 220), light), low = Mix(RGB(26, 32, 62), RGB(175, 210, 245), light);
    if (gloomy) { top = Mix(top, Dim(RGB(130, 135, 145), 0.3f + 0.7f * light), 0.7f); low = Mix(low, Dim(RGB(160, 165, 170), 0.3f + 0.7f * light), 0.7f); }
    for (int band = 0; band < 16; band++) Box(hdc, 0, band * h / 16, WINDOW_WIDTH, (band + 1) * h / 16, Mix(top, low, band / 15.0f));
    int x, y;
    if (light > 0.45f && !gloomy) { // the sun
        SkyPoint(-0.9f, 0.55f * light, &x, &y);
        Blob(hdc, x - 26, y - 26, x + 26, y + 26, RGB(255, 236, 170)); Blob(hdc, x - 18, y - 18, x + 18, y + 18, RGB(255, 250, 215));
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
    if (x > -100) {
        int mr = 22, moon = (int)(235 * k);
        float phase = (SeasonDay() % 30) / 30.0f;
        Blob(hdc, x - mr, y - mr, x + mr, y + mr, RGB(moon, moon, moon * 9 / 10));
        int off = phase <= 0.5f ? (int)(phase * 4.0f * mr) : -(int)((1.0f - phase) * 4.0f * mr);
        if (off < 2 * mr && off > -2 * mr) Blob(hdc, x - mr + off, y - mr, x + mr + off, y + mr, Mix(top, low, 0.45f));
    }
    DWORD cyc = now / 7000, t = now % 7000;
    if (t < 700 && h > 80) { // the odd shooting star
        int hsh = RainHash((int)cyc * 31 + 5); float p = t / 700.0f;
        int x1 = hsh % (WINDOW_WIDTH * 3 / 4) + (int)(p * 240), y1 = (hsh / 7) % (h / 2) + (int)(p * 90);
        Stroke(hdc, x1 - 44, y1 - 16, x1, y1, 2, RGB((int)(255 * k), (int)(255 * k), (int)(230 * k)));
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

static void DrawGround(HDC hdc, float lit) {
    COLORREF g = Dim(GroundColor(), lit);
    int top = horizon < 0 ? 0 : horizon;
    for (int band = 0; band < 12; band++) {
        int y0 = top + (WINDOW_HEIGHT - top) * band * band / 144, y1 = top + (WINDOW_HEIGHT - top) * (band + 1) * (band + 1) / 144;
        Box(hdc, 0, y0, WINDOW_WIDTH, y1, Dim(g, 0.8f + (band % 2) * 0.05f + band * 0.015f));
    }
    for (int r = (int)player_y - FP_RANGE; r <= (int)player_y + FP_RANGE; r++)
        for (int c = (int)player_x - FP_RANGE; c <= (int)player_x + FP_RANGE; c++) {
            if (r < 0 || c < 0 || r >= MAP_SIZE || c >= MAP_SIZE) continue;
            int t = VISUAL_MAP[r][c];
            if (t == 12) { // water lies flat on the ground
                POINT q[4]; int ok = 1;
                ok &= Project((float)c, (float)r, 0, (int *)&q[0].x, (int *)&q[0].y, NULL);
                ok &= Project(c + 1.0f, (float)r, 0, (int *)&q[1].x, (int *)&q[1].y, NULL);
                ok &= Project(c + 1.0f, r + 1.0f, 0, (int *)&q[2].x, (int *)&q[2].y, NULL);
                ok &= Project((float)c, r + 1.0f, 0, (int *)&q[3].x, (int *)&q[3].y, NULL);
                if (ok) Shape(hdc, q, 4, Dim(RGB(50, 120, 210), lit));
            } else if (t != 1 && t != 11 && !in_cave && abs(r - (int)player_y) < 7 && abs(c - (int)player_x) < 7) {
                int hsh = RainHash(r * 977 + c * 131 + current_screen_index * 7), sx, sy; float d;
                if (!Project(c + (hsh % 100) / 100.0f, r + ((hsh / 100) % 100) / 100.0f, 0, &sx, &sy, &d)) continue;
                int u = (int)(focal / d * 0.12f) + 1;
                if (current_biome == BIOME_DESERT || snow_cover > 0.5f) Box(hdc, sx, sy, sx + u / 3, sy + u / 4, Dim(g, 0.8f));
                else for (int b = -1; b <= 1; b++) Stroke(hdc, sx + b * u / 2, sy, sx + b * u, sy - u - (b == 0) * u / 2, 1, Dim(g, 1.25f));
            }
        }
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
        int hop = (now / 250 + t->seed) % 4 == 0 ? (int)(0.06f * u) : 0;
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

static int FarFirst(const void *a, const void *b) {
    float da = ((const FpThing *)a)->depth, db = ((const FpThing *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

// The gaze lingers on one thing for a few seconds, then picks another: the
// bobber while fishing, otherwise the fire, the familiar, animals and
// butterflies before trees and rocks, with a long look up at the sky now and then.
static void UpdateGaze(int fresh) {
    static DWORD gaze_until; static int gaze_n; static float target_yaw, target_pitch;
    static const int interest[8] = { 10, 6, 18, 50, 40, 32, 45, 95 };
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
        if (!fishing && !in_cave && (gaze_n % 4 == 0 || pick < 0)) { target_pitch = 0.6f; target_yaw = yaw + 0.5f; }
        else if (pick >= 0) {
            target_yaw = atan2f(things[pick].y - player_y, things[pick].x - player_x);
            target_pitch = fishing ? -0.15f : things[pick].kind == FP_TREE || things[pick].kind == FP_HOME ? 0.12f : -0.05f;
        }
    }
    yaw += Wrap(target_yaw - yaw) * 0.012f;
    pitch += (target_pitch - pitch) * 0.015f;
}

static void DrawWeather(HDC hdc) {
    if (in_cave || current_biome == BIOME_CAVE || !IsAdverseWeather()) return;
    DWORD now = GetTickCount();
    int snow = current_season == SEASON_WINTER || current_biome == BIOME_TUNDRA;
    for (int i = 0; i < 140; i++) {
        int hsh = RainHash(i * 613 + 9), x = (int)((hsh % WINDOW_WIDTH + now / (snow ? 30 : 4) * (1 + i % 3)) % WINDOW_WIDTH);
        int y = (int)((hsh / 3 % WINDOW_HEIGHT + now / (snow ? 12 : 2) * (2 + i % 3)) % WINDOW_HEIGHT);
        if (snow) Box(hdc, x, y, x + 2, y + 2, RGB(240, 240, 245));
        else Stroke(hdc, x, y, x - 4, y + 14, 1, RGB(170, 185, 210));
    }
}

void DrawAfkView(HDC hdc) {
    static int was_on = 0;
    int ms = AfkViewMs();
    if (ms < FP_ZOOM_MS) { was_on = 0; return; }
    if (ms < FP_ZOOM_MS + FP_DIP_MS) { FillWindowAlpha(hdc, RGB(0, 0, 0), (ms - FP_ZOOM_MS) * 255 / FP_DIP_MS); was_on = 0; return; }
    GatherThings();
    UpdateGaze(!was_on); was_on = 1;
    focal = (WINDOW_WIDTH / 2) / tanf(FP_FOV / 2);
    horizon = (int)(WINDOW_HEIGHT * 0.55f + pitch * WINDOW_HEIGHT * 0.7f);
    float light = (float)fabs(sin(day_night_cycle_accumulator));
    if (NightVisionActive() && light < 0.6f) light = 0.6f;
    if (in_cave) light = 0.35f;
    float lit = 0.3f + 0.7f * light;
    DrawSky(hdc, light);
    DrawGround(hdc, lit);
    for (int i = 0; i < thing_count; i++) {
        int sx, sy;
        if (!Project(things[i].x, things[i].y, things[i].z, &sx, &sy, &things[i].depth)) things[i].depth = -1.0f;
    }
    qsort(things, thing_count, sizeof(FpThing), FarFirst);
    bobber_sx = -1;
    for (int i = 0; i < thing_count; i++) if (things[i].depth > 0.0f) DrawThing(hdc, &things[i], lit);
    DrawWeather(hdc);
    if (bobber_sx >= 0) { // the rod in hand, line out to the bobber
        int tip_x = WINDOW_WIDTH * 3 / 5, tip_y = WINDOW_HEIGHT * 2 / 5;
        Stroke(hdc, WINDOW_WIDTH * 5 / 6, WINDOW_HEIGHT + 6, tip_x, tip_y, 5, RGB(110, 75, 40));
        Stroke(hdc, tip_x, tip_y, bobber_sx, bobber_sy, 1, RGB(225, 225, 225));
    }
    int fade_in = ms - FP_ZOOM_MS - FP_DIP_MS;
    if (fade_in < FP_DIP_MS) FillWindowAlpha(hdc, RGB(0, 0, 0), 255 - fade_in * 255 / FP_DIP_MS);
}
