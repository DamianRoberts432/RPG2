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
    for (int i = 0; i < 4; i++) { // homes sit where they block the hero
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
    for (int band = 0; band < 16; band++) Box(hdc, 0, band * h / 16, WINDOW_WIDTH, (band + 1) * h / 16, Mix(top, low, band / 15.0f));
    int x, y;
    if (light > 0.45f && !gloomy) { // the sun
        SkyPoint(-0.9f, 0.55f * light, &x, &y);
        Blob(hdc, x - 26, y - 26, x + 26, y + 26, RGB(255, 236, 170)); Blob(hdc, x - 18, y - 18, x + 18, y + 18, RGB(255, 250, 215));
        DrawSkyBeam(hdc, x, y, h, RGB(255, 240, 190), 45, BeamStrength(1));
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
    DrawSkyBeam(hdc, x, y, h, RGB(200, 215, 240), (int)(35 * k), BeamStrength(2));
    if (x > -100) {
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
    float ex = player_x - cx, ey = player_y - cy, ez = FP_EYE - cz;
    if (nx * ex + ny * ey + nz * ez <= 0.0f) return; // facing away
    float len = sqrtf(nx * nx + ny * ny + nz * nz) + 1e-6f;
    float sun = (-0.45f * nx - 0.35f * ny + 0.82f * nz) / len;
    float shade = face_glow ? 1.0f : (0.5f + 0.5f * (sun > 0 ? sun : 0)) * scene_lit;
    float cs = cosf(yaw), sn = sinf(yaw), in[8][3], cl[12][3];
    for (int i = 0; i < n; i++) {
        float dx = v[i][0] - player_x, dy = v[i][1] - player_y;
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
        F->p[i].y = horizon + (int)((FP_EYE - cl[i][2]) / cl[i][0] * focal);
    }
    float d = sqrtf(ex * ex + ey * ey + ez * ez), fog = (d - 6.0f) / (FP_RANGE - 6.0f);
    fog = fog < 0 ? 0 : fog > 1 ? 1 : fog;
    F->n = m; F->sprite = -1; F->depth = d - face_bias + (ground ? 1000.0f : 0.0f); // ground always first
    F->c = Mix(Dim(col, shade), haze, fog * 0.7f);
}

// Animals, butterflies, the familiar and the bobber stay small figures, sorted in with the polygons.
static void AddSprite(int idx) {
    if (face_count >= FP_MAX_FACES) return;
    float dx = things[idx].x - player_x, dy = things[idx].y - player_y;
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
    static float lx[64], ly[64], hd[64];
    key &= 63;
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
    if (kind == FP_BUTTERFLY) {
        static const COLORREF wing[4] = { RGB(250, 200, 60), RGB(240, 120, 180), RGB(120, 180, 250), RGB(250, 250, 250) };
        COLORREF c = wing[butterflies[idx].color_profile & 3];
        float f = 0.2f + 1.1f * fabsf(sinf(now / 90.0f + idx));
        OBox(x, y, 0.0f, 0.0f, z - 0.008f, z + 0.008f, 0.03f, 0.006f, RGB(40, 30, 20));
        Wing(x, y, z, 0.025f, 0.06f, f, 1, c); Wing(x, y, z, 0.025f, 0.06f, f, -1, c);
        return;
    }
    if (kind == FP_FAMILIAR && t->seed == 2) type = 5; // toad
    if (kind == FP_FAMILIAR && t->seed >= 3) type = t->seed == 4 ? 2 : 6; // owl or hawk
    float hop = (now / 250 + idx) % 4 == 0 ? 0.03f : 0.0f;
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

static void TreeMesh(const FpThing *t) {
    int s = t->seed, snow = snow_cover > 0.5f;
    float x = t->x + ((s * 37) % 21 - 10) / 100.0f, y = t->y + ((s * 53) % 21 - 10) / 100.0f;
    float h = 0.85f + (s * 29 % 30) / 100.0f, rot = (s % 7) * 0.4f;
    Prism(x, y, 0.0f, 0.5f * h, 0.09f, 0.075f, 5, rot, RGB(110, 75, 45));
    if (s % 3 == 0) { // round broadleaf
        COLORREF leaf = snow ? RGB(205, 215, 225) : current_season == SEASON_FALL ? (s % 2 ? RGB(210, 120, 40) : RGB(220, 175, 50)) : RGB(80, 160, 60);
        Prism(x, y, 0.4f * h, 0.98f * h, 0.32f, 0.6f, 7, rot, leaf); // same ring at the seam, overlapped, so no gap
        Prism(x, y, 0.95f * h, 1.5f * h, 0.61f, 0.28f, 7, rot, leaf);
    } else { // two-tier pine
        COLORREF needle = RGB(35, 115, 55);
        Prism(x, y, 0.3f * h, 1.15f * h, 0.62f, 0.22f, 7, rot, needle); // flat-topped skirt growing into the top cone
        Prism(x, y, 0.9f * h, 2.0f * h, 0.45f, 0.0f, 7, rot + 0.2f, snow ? RGB(225, 232, 240) : needle);
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
    Prism(x, y, 0.1f, 0.62f + fl, 0.22f, 0.0f, 5, GetTickCount() / 400.0f, RGB(240, 120, 30));
    face_bias = 0.05f;
    Prism(x, y, 0.1f, 0.4f - fl, 0.12f, 0.0f, 5, -(GetTickCount() / 300.0f), RGB(255, 220, 90));
    face_bias = 0.0f; face_glow = 0;
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
            if (t == 12 && (r * 7 + c * 3 + (int)(GetTickCount() / 500)) % 9 == 0) col = RGB(75, 145, 225);
            else if (t != 12) col = Dim(col, 0.96f + (abs(RainHash(r * 977 + c * 131)) % 5) * 0.01f);
            float v[4][3] = { { (float)c, (float)r, 0 }, { c + 1.0f, (float)r, 0 }, { c + 1.0f, r + 1.0f, 0 }, { (float)c, r + 1.0f, 0 } };
            AddFace(v, 4, col, NULL, 1);
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
    scene_lit = lit; haze = in_cave ? RGB(18, 16, 14) : Dim(RGB(185, 210, 235), 0.2f + 0.8f * light);
    DrawSky(hdc, light);
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
        else if (things[i].depth > 0.0f) AddSprite(i);
    }
    bobber_sx = -1;
    DrawFaces(hdc, lit);
    DrawWeather(hdc);
    if (bobber_sx >= 0) { // the rod in hand, line out to the bobber
        int tip_x = WINDOW_WIDTH * 3 / 5, tip_y = WINDOW_HEIGHT * 2 / 5;
        Stroke(hdc, WINDOW_WIDTH * 5 / 6, WINDOW_HEIGHT + 6, tip_x, tip_y, 5, RGB(110, 75, 40));
        Stroke(hdc, tip_x, tip_y, bobber_sx, bobber_sy, 1, RGB(225, 225, 225));
    }
    int fade_in = ms - FP_ZOOM_MS - FP_DIP_MS;
    if (fade_in < FP_DIP_MS) FillWindowAlpha(hdc, RGB(0, 0, 0), 255 - fade_in * 255 / FP_DIP_MS);
}
