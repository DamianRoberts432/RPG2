#include <string.h>
#include "game.h"

// 1st-person AFK view. After AFK_DELAY_MS with no input the camera zooms into
// the hero's head (UpdateAfkCamera), the screen dips to black, and the world
// is drawn through the hero's eyes: sky, ground, water, trees, rocks, homes,
// the campfire, butterflies, animals, the familiar and the bobber with the
// rod in hand. The gaze drifts from one nearby thing to the next and now and
// then up at the sky. Any input snaps straight back to play.
#define FP_ZOOM_MS 4000
#define FP_DIP_MS 900
// GetTickCount() / per as seconds-style fx, wrapped so it never overflows 16.16.
static fx Tk(DWORD ms, int per) { return (fx)(((int64_t)(ms % ((DWORD)per * 16384u)) << 16) / per); }
#define FP_FOV FX(1.25)
#define FP_RANGE 13
#define FP_EYE FX(0.55)
#define FP_MAX_THINGS 240
#define GAZE_MS 6500

enum { FP_TREE, FP_ROCK, FP_HOME, FP_FIRE, FP_BUTTERFLY, FP_CRITTER, FP_FAMILIAR, FP_BOBBER, FP_ENEMY, FP_NPC, FP_BARREL, FP_LIGHT, FP_ALTAR, FP_STALL, FP_TENT };
typedef struct { fx x, y, z, depth; int kind, seed; } FpThing;
static FpThing things[FP_MAX_THINGS];
static int thing_count, horizon, bobber_sx = -1, bobber_sy;
static fx yaw, pitch, focal;
static fx eye_x, eye_y, eye_z = FP_EYE;
static int hill_view = 0; // the outdoor view follows the rolling ground; rooms stay flat

int AfkViewActive(void) { return AfkViewMs() >= FP_ZOOM_MS + FP_DIP_MS; }

static fx Wrap(fx a) {
    while (a > FX(3.14159265)) a -= FX(6.2831853);
    while (a < -FX(3.14159265)) a += FX(6.2831853);
    return a;
}

static COLORREF Dim(COLORREF c, fx k) {
    if (k > FX(1.0)) k = FX(1.0);
    return RGB(FxTrunc(GetRValue(c) * k), FxTrunc(GetGValue(c) * k), FxTrunc(GetBValue(c) * k));
}

static COLORREF Mix(COLORREF a, COLORREF b, fx t) {
    return RGB(GetRValue(a) + FxTrunc(((int)GetRValue(b) - (int)GetRValue(a)) * t), GetGValue(a) + FxTrunc(((int)GetGValue(b) - (int)GetGValue(a)) * t),
               GetBValue(a) + FxTrunc(((int)GetBValue(b) - (int)GetBValue(a)) * t));
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

// num / f * focal in whole pixels, in 64 bits: points near the eye land far off screen.
static int ScreenOf(fx num, fx f) {
    int64_t p = ((int64_t)FxBits(num) * FxBits(focal) / FxBits(f)) >> FX_SHIFT;
    return p > 30000 ? 30000 : p < -30000 ? -30000 : (int)p;
}

// Projects a world point (tile units, z = height in tiles) to the screen.
static int Project(fx x, fx y, fx z, int *sx, int *sy, fx *depth) {
    fx dx = x - eye_x, dy = y - eye_y, cs = FxCos(yaw), sn = FxSin(yaw);
    fx f = FxMul(dx, cs) + FxMul(dy, sn), side = -FxMul(dx, sn) + FxMul(dy, cs);
    if (f < FX(0.3)) return 0;
    *sx = WINDOW_WIDTH / 2 + ScreenOf(side, f);
    *sy = horizon + ScreenOf(eye_z - z - (hill_view ? HillFx(x, y) / 32 : FX(0.0)), f);
    if (depth) *depth = f;
    return 1;
}

static void AddThing(fx x, fx y, fx z, int kind, int seed) {
    fx dx = x - (player_x), dy = y - (player_y);
    if (FxMul(dx, dx) + FxMul(dy, dy) < FX(0.25) || FxMul(dx, dx) + FxMul(dy, dy) > FxI(FP_RANGE * FP_RANGE) || thing_count >= FP_MAX_THINGS) return;
    things[thing_count++] = (FpThing){ x, y, z, FX(0.0), kind, seed };
}

static void GatherThings(void) {
    thing_count = 0;
    for (int r = FxTrunc(player_y) - FP_RANGE; r <= FxTrunc(player_y) + FP_RANGE; r++)
        for (int c = FxTrunc(player_x) - FP_RANGE; c <= FxTrunc(player_x) + FP_RANGE; c++) {
            if (r < 0 || c < 0 || r >= MAP_SIZE || c >= MAP_SIZE) continue;
            if (VISUAL_MAP[r][c] == 11) AddThing(FxI(c) + FX(0.5), FxI(r) + FX(0.5), FX(0.0), FP_TREE, r * MAP_SIZE + c);
            else if (VISUAL_MAP[r][c] == 1) AddThing(FxI(c) + FX(0.5), FxI(r) + FX(0.5), FX(0.0), FP_ROCK, r * MAP_SIZE + c);
        }
    for (int i = 0; i < 5; i++) { // homes sit where they block the hero
        fx hx, hy;
        if (VillageHomeCentre(i, &hx, &hy)) AddThing((hx - HOME_FEET_OFFSET), (hy - HOME_FEET_OFFSET), FX(0.0), FP_HOME, i);
    }
    for (int i = 0; i < MAX_CAMPFIRES; i++)
        if (campfires[i].active && campfires[i].screen_id == current_screen_index) AddThing((campfires[i].x), (campfires[i].y), FX(0.0), FP_FIRE, i);
    if (!in_cave && !IsAdverseWeather())
        for (int i = 0; i < MAX_BUTTERFLIES; i++)
            if (butterflies[i].active) AddThing((butterflies[i].x), (butterflies[i].y), FX(0.35) + FxDiv((butterflies[i].z), FX(40.0)), FP_BUTTERFLY, i);
    for (int i = 0; i < MAX_CRITTERS; i++)
        if (critters[i].active && !critters[i].hidden) AddThing((critters[i].x), (critters[i].y), FX(0.0), FP_CRITTER, i * 8 + (critters[i].type & 7));
    if (familiar_visible) AddThing((familiar_world_x), (familiar_world_y), FX(0.0), FP_FAMILIAR, familiar_visible - 1);
    for (int i = 0; i < MAX_ENEMIES; i++) { // the live one is held in (enemy_x)/(enemy_y)
        int cur = i == current_enemy;
        fx hp = cur ? (enemy_hearts) : (enemies[i].hearts);
        if (hp > FX(0.0)) AddThing(cur ? (enemy_x) : (enemies[i].x), cur ? (enemy_y) : (enemies[i].y), FX(0.0), FP_ENEMY, i * 32 + ((cur ? active_monster : enemies[i].type) & 31));
    }
    for (int i = 0; i < village_npc_count; i++)
        if (village_npcs[i].type != 8) AddThing((village_npcs[i].x), (village_npcs[i].y), FX(0.0), FP_NPC, i * 16 + (village_npcs[i].type & 15));
        else AddThing((village_npcs[i].x), (village_npcs[i].y), FX(0.0), FP_ALTAR, TownDeity());
    { // the same town details as in play: barrels, torches and fire pits, the castle stall and tent
        fx qx, qy; int intact;
        for (int i = 0; BarrelInfo(i, &qx, &qy, &intact); i++) AddThing((qx) + FX(0.5), (qy) + FX(0.5), FX(0.0), FP_BARREL, i * 2 + intact);
        for (int i = 0; i < village_light_count && i < MAX_VILLAGE_LIGHTS; i++)
            AddThing((village_lights[i].x), (village_lights[i].y), FX(0.0), FP_LIGHT, i * 2 + (village_lights[i].torch != 0));
        if (current_biome == BIOME_CASTLE) { AddThing(FX(11.0), FX(14.0), FX(0.0), FP_STALL, 0); AddThing((merchant_x), (merchant_y), FX(0.0), FP_TENT, 0); }
    }
    fx bx, by; int nibble;
    if (FishingBobberPos(&bx, &by, &nibble)) AddThing((bx), (by), FX(0.0), FP_BOBBER, nibble);
}

static void SkyPoint(fx az, fx el, int *x, int *y) {
    fx rel = Wrap(az - yaw);
    *x = FxAbs(rel) < FxMul(FP_FOV, FX(0.7)) ? WINDOW_WIDTH / 2 + FxTrunc(FxDiv(FxTan(rel), FxTan(FP_FOV / 2)) * WINDOW_WIDTH / 2) : -1000;
    *y = horizon - FxTrunc(FxMul(el * WINDOW_HEIGHT, FX(0.8)));
}

// Now and then (about one 25 s spell in eight) a soft shaft of sun or moon
// light slants down to the ground, fading in and out over a few seconds.
static fx BeamStrength(int salt) {
    DWORD now = GetTickCount(), cyc = now / 25000, t = now % 25000;
    if (RainHash((int)cyc * 7 + salt) % 8 || t > 15000) return FX(0.0);
    return t < 3000 ? FxI(t) / 3000 : t > 12000 ? FxI(15000 - t) / 3000 : FX(1.0);
}

static void DrawSkyBeam(HDC hdc, int x, int y, int h, COLORREF c, int alpha, fx k) {
    if (k <= FX(0.0) || x < -200 || x > WINDOW_WIDTH + 200) return;
    for (int yy = y; yy < h + 80 && yy < WINDOW_HEIGHT; yy += 8) {
        fx t = FxI(yy - y) / (h + 80 - y + 1);
        int w = 20 + FxTrunc(t * 120), cx = x + FxTrunc(FxMul((WINDOW_WIDTH / 2 - x) * FX(0.45), t)); // falls away from the sun
        FillRectAlpha(hdc, cx - w / 2, yy, w, 8, c, FxTrunc(FxMul(alpha * k, (FX(1.0) - FxMul(FX(0.6), t)))));
    }
}

// The sun or moon: a disc with a smooth round glow, worked out per pixel so there are
// no hard edges or square halos. phase < 0 is a full disc; otherwise only the moon's
// lit part shows (0 new, 0.5 full) and the dark part is simply sky.
static void SoftLight(HDC hdc, int cx, int cy, int disc_r, int glow_r, COLORREF disc, COLORREF glow, fx glow_a, fx phase, fx fade) {
    int S = 2 * glow_r + 2, x0 = cx - glow_r - 1, y0 = cy - glow_r - 1;
    if (fade <= FX(0.0) || x0 + S < 0 || x0 > WINDOW_WIDTH || y0 + S < 0 || y0 > WINDOW_HEIGHT) return;
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), S, -S, 1, 32, BI_RGB, 0, 0, 0, 0, 0 } };
    void *bits = NULL; HBITMAP b = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!b || !bits) { if (b) DeleteObject(b); return; }
    fx cp = phase < FX(0.0) ? -FX(1.0) : FxCos(FxMul(phase, FX(6.2832))), lit = phase < FX(0.0) ? FX(1.0) : FX(0.5) - FxMul(FX(0.5), cp);
    fx ga = FxMul(FxMul(glow_a, (FX(0.25) + FxMul(FX(0.75), lit))), fade), R = FxI(disc_r);
    unsigned char *px = (unsigned char *)bits;
    for (int j = 0; j < S; j++)
        for (int i = 0; i < S; i++, px += 4) {
            fx dx = FxI(i - glow_r - 1) + FX(0.5), dy = FxI(j - glow_r - 1) + FX(0.5), d = FxSqrt(FxMul(dx, dx) + FxMul(dy, dy));
            fx t = d / glow_r, g = t < FX(1.0) ? FxMul(FxMul((FX(1.0) - t), (FX(1.0) - t)), ga) : FX(0.0);
            fx cov = R + FX(0.5) - d; cov = cov < FX(0.0) ? FX(0.0) : cov > FX(1.0) ? FX(1.0) : cov;
            if (cov > FX(0.0) && phase >= FX(0.0)) { // the terminator: an ellipse sweeping across the disc
                fx w = FxMul(R, R) - FxMul(dy, dy), term = FxMul((w > FX(0.0) ? FxSqrt(w) : FX(0.0)), cp), side = phase <= FX(0.5) ? dx - term : -term - dx;
                side += FX(0.5); cov = FxMul(cov, FxClamp(side, FX(0.0), FX(1.0)));
            }
            cov = FxMul(cov, fade);
            fx a = cov + FxMul(g, (FX(1.0) - cov));
            px[0] = (unsigned char)FxTrunc(GetBValue(disc) * cov + FxMul(GetBValue(glow) * g, (FX(1.0) - cov)));
            px[1] = (unsigned char)FxTrunc(GetGValue(disc) * cov + FxMul(GetGValue(glow) * g, (FX(1.0) - cov)));
            px[2] = (unsigned char)FxTrunc(GetRValue(disc) * cov + FxMul(GetRValue(glow) * g, (FX(1.0) - cov)));
            px[3] = (unsigned char)FxTrunc(FxMul(a, FX(255.0)));
        }
    HDC m = CreateCompatibleDC(hdc); HGDIOBJ om = SelectObject(m, b);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, x0, y0, S, S, m, 0, 0, S, S, bf);
    SelectObject(m, om); DeleteDC(m); DeleteObject(b);
}

static void DrawSky(HDC hdc, fx light) {
    int h = horizon < 0 ? 0 : horizon > WINDOW_HEIGHT ? WINDOW_HEIGHT : horizon;
    if (in_cave || current_biome == BIOME_CAVE) { Box(hdc, 0, 0, WINDOW_WIDTH, h, RGB(28, 24, 22)); return; }
    int gloomy = IsAdverseWeather();
    COLORREF top = Mix(RGB(6, 9, 26), RGB(70, 130, 220), light), low = Mix(RGB(26, 32, 62), RGB(175, 210, 245), light);
    if (gloomy) { top = Mix(top, Dim(RGB(130, 135, 145), FX(0.3) + FxMul(FX(0.7), light)), FX(0.7)); low = Mix(low, Dim(RGB(160, 165, 170), FX(0.3) + FxMul(FX(0.7), light)), FX(0.7)); }
    if (h > 0) { // one smooth vertical gradient instead of stacked bands
        TRIVERTEX v[2] = { { 0, 0, (COLOR16)(GetRValue(top) << 8), (COLOR16)(GetGValue(top) << 8), (COLOR16)(GetBValue(top) << 8), 0 },
                           { WINDOW_WIDTH, h, (COLOR16)(GetRValue(low) << 8), (COLOR16)(GetGValue(low) << 8), (COLOR16)(GetBValue(low) << 8), 0 } };
        GRADIENT_RECT gr = { 0, 1 };
        GradientFill(hdc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
    }
    int x, y;
    if (light > FX(0.45) && !gloomy) { // the sun
        SkyPoint(-FX(0.9), FxMul(FX(0.55), light), &x, &y);
        SoftLight(hdc, x, y, 19, 85, RGB(255, 250, 220), RGB(255, 232, 160), FX(0.6), -FX(1.0), FX(1.0));
        DrawSkyBeam(hdc, x, y, h, RGB(255, 240, 190), 45, BeamStrength(1));
    }
    if (light > FX(0.4) && !gloomy) { // a distant flock: little cursive v's drifting and flapping
        fx tb = Tk(GetTickCount(), 1000);
        for (int i = 0; i < 7; i++) {
            fx az = FX(1.9) + FxMul(tb, FX(0.006)) + (i % 4) * FX(0.035) + i * FX(0.012), el = FX(0.30) + FxMul(FX(0.025), FxSin(FxMul(tb, FX(0.3)) + i * FX(1.3))) + (i % 3) * FX(0.02);
            int bx, by; SkyPoint(az, el, &bx, &by);
            if (bx < 0 || bx >= WINDOW_WIDTH || by < 0 || by >= h) continue;
            int s = 3 + (i % 3), flap = FxTrunc(FxMul(FxSin(FxMul(tb, FX(5.0) + FxI(i % 3)) + FxI(i)), FX(2.0)));
            COLORREF bc = Dim(RGB(60, 65, 80), FX(0.6) + FxMul(FX(0.4), light));
            Stroke(hdc, bx - s, by - 1 - flap, bx, by + 1, 1, bc); Stroke(hdc, bx, by + 1, bx + s, by - 1 - flap, 1, bc);
        }
    }
    if (light >= FX(0.35) || gloomy) return;
    fx k = FxDiv((FX(0.35) - light), FX(0.35));
    DWORD now = GetTickCount();
    for (int i = 0; i < 180; i++) { // stars hang at fixed bearings so they pan with the gaze
        int hsh = RainHash(i * 7919 + 3);
        SkyPoint(FxI(hsh % 6283) / 1000, FX(0.08) + FxI((hsh / 6283) % 1000) / 1100, &x, &y);
        if (x < 0 || x >= WINDOW_WIDTH || y < 0 || y >= h) continue;
        int lum = FxTrunc(FxMul((FX(150.0) + FxMul(FX(105.0), (FX(0.5) + FxMul(FX(0.5), FxSin(Tk(now, 600) + FxI(i)))))), k)), sz = i % 11 == 0 ? 2 : 1;
        Box(hdc, x, y, x + sz - 1, y + sz - 1, RGB(lum, lum, lum));
    }
    SkyPoint(FX(0.8), FX(0.42), &x, &y); // the moon; its phase follows the 30-day season
    if (x > -100) // moonlight is a soft round glow; the unlit part of the moon is just sky
        SoftLight(hdc, x, y, 22, 95, RGB(240, 240, 222), RGB(200, 215, 240), FX(0.28), FxI(SeasonDay() % 30) / 30, k);
    // A rare shooting star (one every ~40 s on average), pinned to the sky like
    // the stars so its trail glides smoothly when the gaze pans.
    DWORD cyc = now / 4000, t = now % 4000;
    int mh = RainHash((int)cyc * 31 + 5);
    if (mh % 10 == 0 && t < 900) {
        fx p = FxI(t) / 900, az = FxI(mh / 10 % 6283) / 1000, el = FX(0.35) + FxI(mh / 7 % 300) / 1000;
        int x0, y0, x1, y1;
        SkyPoint(az + FxMul(p, FX(0.35)), el - FxMul(p, FX(0.12)), &x1, &y1);
        SkyPoint(az + FxMul(p, FX(0.35)) - FX(0.09), el - FxMul(p, FX(0.12)) + FX(0.03), &x0, &y0);
        int fade = FxTrunc(FxMul(255 * k, (p < FX(0.8) ? FX(1.0) : FxDiv((FX(1.0) - p), FX(0.2)))));
        if (x1 > 0 && x0 > 0 && y1 < h) Stroke(hdc, x0, y0, x1, y1, 2, RGB(fade, fade, fade * 9 / 10));
    }
}

static COLORREF GroundColor(void) {
    if (in_cave || current_biome == BIOME_CAVE) return RGB(70, 62, 55);
    if ((snow_cover) > FX(0.5) || current_biome == BIOME_TUNDRA) return RGB(225, 230, 238);
    if (current_biome == BIOME_DESERT) return RGB(225, 195, 95);
    if (current_biome == BIOME_SWAMP) return RGB(75, 100, 60);
    if (current_season == SEASON_FALL) return RGB(125, 140, 60);
    return RGB(85, 155, 65);
}

static void DrawThing(HDC hdc, const FpThing *t, fx lit) {
    int sx, sy; fx d;
    if (!Project(t->x, t->y, t->z, &sx, &sy, &d)) return;
    fx u = FxDiv(focal, d); // pixels per tile at this distance
    if (u > WINDOW_HEIGHT * FX(0.6) && t->kind != FP_BOBBER) return; // too close to the eye: it would fill the view
    int U = FxTrunc(u);
    DWORD now = GetTickCount();
    switch (t->kind) {
    case FP_TREE: {
        Box(hdc, sx - FxTrunc(FxMul(FX(0.09), u)), sy - FxTrunc(FxMul(FX(1.0), u)), sx + FxTrunc(FxMul(FX(0.09), u)), sy, Dim(RGB(100, 70, 45), lit));
        if (t->seed % 3 == 0) {
            POINT p[3] = { { sx, sy - FxTrunc(FxMul(FX(2.9), u)) }, { sx + FxTrunc(FxMul(FX(0.75), u)), sy - FxTrunc(FxMul(FX(0.7), u)) }, { sx - FxTrunc(FxMul(FX(0.75), u)), sy - FxTrunc(FxMul(FX(0.7), u)) } };
            Shape(hdc, p, 3, Dim(RGB(35, 105, 60), lit));
            if ((snow_cover) > FX(0.3)) { POINT s[3] = { p[0], { sx + FxTrunc(FxMul(FX(0.3), u)), sy - FxTrunc(FxMul(FX(2.0), u)) }, { sx - FxTrunc(FxMul(FX(0.3), u)), sy - FxTrunc(FxMul(FX(2.0), u)) } }; Shape(hdc, s, 3, Dim(RGB(235, 240, 245), lit)); }
        } else if (current_season == SEASON_WINTER) {
            for (int b = -1; b <= 1; b++) Stroke(hdc, sx, sy - FxTrunc(FxMul(FX(0.9), u)), sx + b * FxTrunc(FxMul(FX(0.5), u)), sy - FxTrunc(FxMul((FX(1.8) + (b == 0) * FX(0.5)), u)), U / 12 + 1, Dim(RGB(95, 70, 50), lit));
        } else {
            COLORREF leaf = current_season == SEASON_FALL ? RGB(205, 120, 40) : RGB(60, 140, 55);
            Blob(hdc, sx - FxTrunc(FxMul(FX(0.8), u)), sy - FxTrunc(FxMul(FX(2.5), u)), sx + FxTrunc(FxMul(FX(0.8), u)), sy - FxTrunc(FxMul(FX(0.85), u)), Dim(leaf, lit));
            Blob(hdc, sx - FxTrunc(FxMul(FX(0.45), u)), sy - FxTrunc(FxMul(FX(2.35), u)), sx + FxTrunc(FxMul(FX(0.15), u)), sy - FxTrunc(FxMul(FX(1.7), u)), Dim(leaf, FxMul(lit, FX(1.2))));
        }
        break; }
    case FP_ROCK:
        Blob(hdc, sx - FxTrunc(FxMul(FX(0.4), u)), sy - FxTrunc(FxMul(FX(0.45), u)), sx + FxTrunc(FxMul(FX(0.4), u)), sy + FxTrunc(FxMul(FX(0.05), u)), Dim(RGB(125, 125, 130), lit));
        break;
    case FP_HOME: {
        Box(hdc, sx - FxTrunc(FxMul(FX(0.8), u)), sy - FxTrunc(FxMul(FX(1.1), u)), sx + FxTrunc(FxMul(FX(0.8), u)), sy, Dim(RGB(200, 170, 120), lit));
        POINT roof[3] = { { sx - FxTrunc(FxMul(FX(0.95), u)), sy - FxTrunc(FxMul(FX(1.1), u)) }, { sx, sy - FxTrunc(FxMul(FX(1.9), u)) }, { sx + FxTrunc(FxMul(FX(0.95), u)), sy - FxTrunc(FxMul(FX(1.1), u)) } };
        Shape(hdc, roof, 3, Dim(RGB(150, 60, 45), lit));
        Box(hdc, sx - FxTrunc(FxMul(FX(0.15), u)), sy - FxTrunc(FxMul(FX(0.55), u)), sx + FxTrunc(FxMul(FX(0.15), u)), sy, Dim(RGB(90, 60, 40), lit));
        Box(hdc, sx + FxTrunc(FxMul(FX(0.35), u)), sy - FxTrunc(FxMul(FX(0.85), u)), sx + FxTrunc(FxMul(FX(0.6), u)), sy - FxTrunc(FxMul(FX(0.55), u)), lit < FX(0.5) ? RGB(255, 210, 110) : RGB(130, 170, 210));
        break; }
    case FP_FIRE: {
        Blob(hdc, sx - FxTrunc(FxMul(FX(0.9), u)), sy - FxTrunc(FxMul(FX(0.15), u)), sx + FxTrunc(FxMul(FX(0.9), u)), sy + FxTrunc(FxMul(FX(0.15), u)), Mix(Dim(GroundColor(), lit), RGB(230, 130, 40), FX(0.45)));
        Stroke(hdc, sx - FxTrunc(FxMul(FX(0.32), u)), sy, sx + FxTrunc(FxMul(FX(0.32), u)), sy - FxTrunc(FxMul(FX(0.06), u)), U / 8 + 1, RGB(90, 55, 30));
        Stroke(hdc, sx - FxTrunc(FxMul(FX(0.32), u)), sy - FxTrunc(FxMul(FX(0.06), u)), sx + FxTrunc(FxMul(FX(0.32), u)), sy, U / 8 + 1, RGB(70, 45, 25));
        int f = (int)((now / 90 + t->seed) % 3) - 1;
        POINT outer[3] = { { sx - FxTrunc(FxMul(FX(0.24), u)), sy - FxTrunc(FxMul(FX(0.05), u)) }, { sx + f * U / 18, sy - FxTrunc(FxMul((FX(0.7) + FX(0.05) * f), u)) }, { sx + FxTrunc(FxMul(FX(0.24), u)), sy - FxTrunc(FxMul(FX(0.05), u)) } };
        POINT inner[3] = { { sx - FxTrunc(FxMul(FX(0.12), u)), sy - FxTrunc(FxMul(FX(0.05), u)) }, { sx - f * U / 24, sy - FxTrunc(FxMul(FX(0.42), u)) }, { sx + FxTrunc(FxMul(FX(0.12), u)), sy - FxTrunc(FxMul(FX(0.05), u)) } };
        Shape(hdc, outer, 3, RGB(245, 130, 30)); Shape(hdc, inner, 3, RGB(255, 225, 90));
        break; }
    case FP_BUTTERFLY: {
        static const COLORREF wing[4] = { RGB(250, 200, 60), RGB(240, 120, 180), RGB(120, 180, 250), RGB(250, 250, 250) };
        int w = FxTrunc(FxMul((FX(0.05) + FxMul(FX(0.05), FxAbs(FxSin(Tk(now, 110) + FxI(t->seed))))), u)) + 2, hgt = FxTrunc(FxMul(FX(0.07), u)) + 2;
        COLORREF c = Dim(wing[butterflies[t->seed].color_profile & 3], lit + FX(0.2));
        Blob(hdc, sx - w, sy - hgt, sx, sy + hgt / 2, c); Blob(hdc, sx, sy - hgt, sx + w, sy + hgt / 2, c);
        Stroke(hdc, sx, sy - hgt, sx, sy + hgt / 2, 1, RGB(40, 30, 20));
        break; }
    case FP_CRITTER: {
        static const COLORREF fur[4] = { RGB(70, 120, 220), RGB(150, 95, 50), RGB(140, 115, 85), RGB(45, 40, 50) };
        COLORREF c = Dim(fur[t->seed & 3], lit + FX(0.1));
        int hop = (now / (190 + 60 * (t->seed % 3)) + t->seed) % 4 == 0 ? FxTrunc(FxMul(FX(0.06), u)) : 0;
        if ((t->seed & 3) == 1) Blob(hdc, sx - FxTrunc(FxMul(FX(0.32), u)), sy - FxTrunc(FxMul(FX(0.45), u)) - hop, sx - FxTrunc(FxMul(FX(0.08), u)), sy - FxTrunc(FxMul(FX(0.1), u)) - hop, c);
        Blob(hdc, sx - FxTrunc(FxMul(FX(0.12), u)), sy - FxTrunc(FxMul(FX(0.22), u)) - hop, sx + FxTrunc(FxMul(FX(0.12), u)), sy - hop, c);
        Blob(hdc, sx + FxTrunc(FxMul(FX(0.04), u)), sy - FxTrunc(FxMul(FX(0.32), u)) - hop, sx + FxTrunc(FxMul(FX(0.2), u)), sy - FxTrunc(FxMul(FX(0.16), u)) - hop, c);
        break; }
    case FP_FAMILIAR: {
        static const COLORREF coat[5] = { RGB(150, 100, 55), RGB(120, 120, 128), RGB(70, 130, 60), RGB(140, 100, 60), RGB(170, 150, 120) };
        COLORREF c = Dim(coat[t->seed % 5], lit + FX(0.15));
        int wag = FxTrunc(FxMul(FxMul(FxSin(Tk(now, 150)), FX(0.08)), u));
        Blob(hdc, sx - FxTrunc(FxMul(FX(0.32), u)), sy - FxTrunc(FxMul(FX(0.38), u)), sx + FxTrunc(FxMul(FX(0.32), u)), sy - FxTrunc(FxMul(FX(0.05), u)), c);
        Blob(hdc, sx + FxTrunc(FxMul(FX(0.12), u)), sy - FxTrunc(FxMul(FX(0.62), u)), sx + FxTrunc(FxMul(FX(0.42), u)), sy - FxTrunc(FxMul(FX(0.32), u)), c);
        Stroke(hdc, sx - FxTrunc(FxMul(FX(0.3), u)), sy - FxTrunc(FxMul(FX(0.25), u)), sx - FxTrunc(FxMul(FX(0.5), u)), sy - FxTrunc(FxMul(FX(0.45), u)) + wag, U / 14 + 1, c);
        Blob(hdc, sx + FxTrunc(FxMul(FX(0.3), u)), sy - FxTrunc(FxMul(FX(0.52), u)), sx + FxTrunc(FxMul(FX(0.34), u)), sy - FxTrunc(FxMul(FX(0.48), u)), RGB(10, 10, 10));
        break; }
    case FP_BOBBER: {
        int r = FxTrunc(FxMul(FX(0.07), u)) + 3, bob = t->seed ? FxTrunc(FxMul(FX(0.05), u)) + 2 : FxTrunc(FxMul(FxSin(Tk(now, 300)), FX(2.0)));
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
#define FP_NEAR FX(0.08)
// Saturn budget: screen points are 16-bit (as VDP1 takes them), so a face is
// 68 bytes instead of 112 and the 4096-face list is 272 KB instead of 448 KB.
// Points are clipped to a guard band round the screen so they always fit.
#define FP_MAX_PTS 14 // 8 corners + 1 from the near clip + 4 from the guard band, 1 spare
#define FP_GUARD 512
typedef struct { fx depth; COLORREF c; int16_t sprite; uint8_t n; int16_t x[FP_MAX_PTS], y[FP_MAX_PTS]; } FpFace;

#ifndef __cplusplus
_Static_assert(sizeof(FpFace) <= 68, "FpFace grew past its Saturn budget");
#endif

static int ClipEdge(int64_t (*in)[2], int n, int64_t (*out)[2], int axis, int lim_px, int keep_below) {
    int m = 0;
    int64_t lim = (int64_t)lim_px << FX_SHIFT;
    for (int i = 0; i < n && m < FP_MAX_PTS - 1; i++) {
        int64_t *a = in[i], *b = in[(i + 1) % n];
        int ia = keep_below ? a[axis] <= lim : a[axis] >= lim, ib = keep_below ? b[axis] <= lim : b[axis] >= lim;
        if (ia) { out[m][0] = a[0]; out[m][1] = a[1]; m++; }
        if (ia != ib) {
            int64_t t = ((lim - a[axis]) << FX_SHIFT) / (b[axis] - a[axis]);
            out[m][0] = a[0] + ((t * (b[0] - a[0])) >> FX_SHIFT); out[m][1] = a[1] + ((t * (b[1] - a[1])) >> FX_SHIFT); m++;
        }
    }
    return m;
}
static FpFace faces[FP_MAX_FACES];
static int face_count, face_glow;
static fx face_bias, scene_lit;
static COLORREF haze;
static fx fog_near = FX(6.0), fog_far = FxI(FP_RANGE), fog_cap = FX(0.7); // night pulls these in (further for elves/dwarves)

static void AddFace(fx v[][3], int n, COLORREF col, const fx *center, int ground) {
    if (face_count >= FP_MAX_FACES || n < 3 || n > 8) return;
    fx ax = v[1][0] - v[0][0], ay = v[1][1] - v[0][1], az = v[1][2] - v[0][2];
    fx bx = v[2][0] - v[0][0], by = v[2][1] - v[0][1], bz = v[2][2] - v[0][2];
    // The normal only needs a direction: take the cross product in 64 bits, then scale it
    // to 16..64 tiles long so small faces keep their precision and big ones can't overflow.
    int64_t n64[3] = { (int64_t)FxBits(ay) * FxBits(bz) - (int64_t)FxBits(az) * FxBits(by),
                       (int64_t)FxBits(az) * FxBits(bx) - (int64_t)FxBits(ax) * FxBits(bz),
                       (int64_t)FxBits(ax) * FxBits(by) - (int64_t)FxBits(ay) * FxBits(bx) }, big = 0;
    for (int i = 0; i < 3; i++) { int64_t a = n64[i] < 0 ? -n64[i] : n64[i]; if (a > big) big = a; }
    if (!big) return;
    int sh = 0;
    while (big >= ((int64_t)1 << 22)) { big >>= 1; sh++; }
    while (big < ((int64_t)1 << 20)) { big <<= 1; sh--; }
    for (int i = 0; i < 3; i++) n64[i] = sh >= 0 ? n64[i] >> sh : n64[i] * ((int64_t)1 << -sh);
    fx nx = FxRaw((int32_t)n64[0]), ny = FxRaw((int32_t)n64[1]), nz = FxRaw((int32_t)n64[2]);
    fx cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < n; i++) { cx += v[i][0]; cy += v[i][1]; cz += v[i][2]; }
    cx /= n; cy /= n; cz /= n;
    fx out = center ? FxMul(nx, (cx - center[0])) + FxMul(ny, (cy - center[1])) + FxMul(nz, (cz - center[2])) : nz;
    if (out < 0) { nx = -nx; ny = -ny; nz = -nz; }
    fx ex = eye_x - cx, ey = eye_y - cy, ez = eye_z - cz;
    if (FxMul(nx, ex) + FxMul(ny, ey) + FxMul(nz, ez) <= FX(0.0)) return; // facing away
    fx len = FxSqrt(FxMul(nx, nx) + FxMul(ny, ny) + FxMul(nz, nz)) + FX_EPS;
    fx sun = FxDiv((-FxMul(FX(0.45), nx) - FxMul(FX(0.35), ny) + FxMul(FX(0.82), nz)), len);
    fx shade = face_glow ? FX(1.0) : FxMul((FX(0.5) + FxMul(FX(0.5), (sun > 0 ? sun : 0))), scene_lit);
    fx cs = FxCos(yaw), sn = FxSin(yaw), in[8][3], cl[12][3];
    for (int i = 0; i < n; i++) {
        fx dx = v[i][0] - eye_x, dy = v[i][1] - eye_y;
        in[i][0] = FxMul(dx, cs) + FxMul(dy, sn); in[i][1] = -FxMul(dx, sn) + FxMul(dy, cs); in[i][2] = v[i][2];
    }
    int m = 0;
    for (int i = 0; i < n; i++) { // clip against the near plane
        fx *a = in[i], *b = in[(i + 1) % n];
        int ia = a[0] >= FP_NEAR, ib = b[0] >= FP_NEAR;
        if (ia) { cl[m][0] = a[0]; cl[m][1] = a[1]; cl[m][2] = a[2]; m++; }
        if (ia != ib) { fx t = FxDiv((FP_NEAR - a[0]), (b[0] - a[0])); for (int k = 0; k < 3; k++) cl[m][k] = a[k] + FxMul(t, (b[k] - a[k])); m++; }
    }
    if (m < 3) return;
    int64_t s0[FP_MAX_PTS][2], s1[FP_MAX_PTS][2]; // 16.16 screen points in 64 bits, clipped to the guard band below
    for (int i = 0; i < m; i++) {
        s0[i][0] = ((int64_t)(WINDOW_WIDTH / 2) << FX_SHIFT) + (int64_t)FxBits(cl[i][1]) * FxBits(focal) / FxBits(cl[i][0]);
        s0[i][1] = ((int64_t)horizon << FX_SHIFT) + (int64_t)FxBits(eye_z - cl[i][2]) * FxBits(focal) / FxBits(cl[i][0]);
    }
    m = ClipEdge(s0, m, s1, 0, -FP_GUARD, 0);
    m = ClipEdge(s1, m, s0, 0, WINDOW_WIDTH + FP_GUARD, 1);
    m = ClipEdge(s0, m, s1, 1, -FP_GUARD, 0);
    m = ClipEdge(s1, m, s0, 1, WINDOW_HEIGHT + FP_GUARD, 1);
    if (m < 3) return;
    FpFace *F = &faces[face_count++];
    for (int i = 0; i < m; i++) { F->x[i] = (int16_t)(s0[i][0] >> FX_SHIFT); F->y[i] = (int16_t)(s0[i][1] >> FX_SHIFT); }
    fx d = FxSqrt(FxMul(ex, ex) + FxMul(ey, ey) + FxMul(ez, ez)), fog = FxDiv((d - fog_near), (fog_far - fog_near));
    fog = FxClamp(fog, FX(0.0), FX(1.0));
    F->n = m; F->sprite = -1; F->depth = d - face_bias + (ground ? FX(1000.0) : FX(0.0)); // ground always first
    F->c = Mix(Dim(col, shade), haze, FxMul(fog, fog_cap));
}

// Animals, butterflies, the familiar and the bobber stay small figures, sorted in with the polygons.
static void AddSprite(int idx) {
    if (face_count >= FP_MAX_FACES) return;
    fx dx = things[idx].x - eye_x, dy = things[idx].y - eye_y;
    FpFace *F = &faces[face_count++];
    F->sprite = (int16_t)idx; F->n = 0; F->depth = FxSqrt(FxMul(dx, dx) + FxMul(dy, dy));
}

static int FaceFarFirst(const void *a, const void *b) {
    fx da = ((const FpFace *)a)->depth, db = ((const FpFace *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

static void DrawFaces(HDC hdc, fx lit) {
    qsort(faces, face_count, sizeof(FpFace), FaceFarFirst);
    HGDIOBJ ob = SelectObject(hdc, GetStockObject(DC_BRUSH)), op = SelectObject(hdc, GetStockObject(DC_PEN));
    for (int i = 0; i < face_count; i++) {
        FpFace *F = &faces[i];
        if (F->sprite >= 0) {
            DrawThing(hdc, &things[F->sprite], lit);
            SelectObject(hdc, GetStockObject(DC_BRUSH)); SelectObject(hdc, GetStockObject(DC_PEN));
            continue;
        }
        POINT pt[FP_MAX_PTS];
        for (int k = 0; k < F->n; k++) { pt[k].x = F->x[k]; pt[k].y = F->y[k]; }
        SetDCBrushColor(hdc, F->c); SetDCPenColor(hdc, F->c); // same-colour edge hides seams
        Polygon(hdc, pt, F->n);
    }
    SelectObject(hdc, ob); SelectObject(hdc, op);
}

// A ring of seg points at z0 joined to a ring at z1 (r1 = 0 makes a cone).
static void Prism(fx x, fx y, fx z0, fx z1, fx r0, fx r1, int seg, fx rot, COLORREF col) {
    fx c[3] = { x, y, (z0 + z1) / 2 }, ring0[8][2], ring1[8][2];
    for (int i = 0; i < seg; i++) {
        fx a = rot + i * FX(6.2832) / seg;
        ring0[i][0] = x + FxMul(FxCos(a), r0); ring0[i][1] = y + FxMul(FxSin(a), r0);
        ring1[i][0] = x + FxMul(FxCos(a), r1); ring1[i][1] = y + FxMul(FxSin(a), r1);
    }
    for (int i = 0; i < seg; i++) {
        int j = (i + 1) % seg;
        fx v[4][3] = { { ring0[i][0], ring0[i][1], z0 }, { ring0[j][0], ring0[j][1], z0 },
                          { ring1[j][0], ring1[j][1], z1 }, { ring1[i][0], ring1[i][1], z1 } };
        AddFace(v, r1 > FX(0.001) ? 4 : 3, col, c, 0);
    }
    if (r1 > FX(0.001)) {
        fx v[8][3];
        for (int i = 0; i < seg; i++) { v[i][0] = ring1[i][0]; v[i][1] = ring1[i][1]; v[i][2] = z1; }
        AddFace(v, seg, col, c, 0);
    }
}

// A smooth surface of revolution through rings (z[i], r[i]). Neighbouring bands share
// their edges exactly, so no overlapping seams poke through; cap0/cap1 close the ends.
static void Lathe(fx x, fx y, const fx *z, const fx *r, int rings, int seg, fx rot, int cap0, int cap1, COLORREF col) {
    if (seg > 8) seg = 8;
    for (int k = 0; k + 1 < rings; k++) {
        fx nr = z[k + 1] - z[k], nz = r[k] - r[k + 1], nl = FxSqrt(FxMul(nr, nr) + FxMul(nz, nz)) + FX_EPS; // outward normal in (r, z)
        nr = FxDiv(nr, nl); nz = FxDiv(nz, nl);
        for (int i = 0; i < seg; i++) {
            fx a0 = rot + i * FX(6.2832) / seg, a1 = a0 + FX(6.2832) / seg, am = (a0 + a1) / 2;
            fx q[4][3] = { { x + FxMul(FxCos(a0), r[k]), y + FxMul(FxSin(a0), r[k]), z[k] }, { x + FxMul(FxCos(a1), r[k]), y + FxMul(FxSin(a1), r[k]), z[k] },
                              { x + FxMul(FxCos(a1), r[k + 1]), y + FxMul(FxSin(a1), r[k + 1]), z[k + 1] }, { x + FxMul(FxCos(a0), r[k + 1]), y + FxMul(FxSin(a0), r[k + 1]), z[k + 1] } };
            fx c[3] = { 0, 0, 0 };
            for (int j = 0; j < 4; j++) for (int d = 0; d < 3; d++) c[d] += q[j][d] / 4;
            c[0] -= FxMul(FxMul(FxCos(am), nr), FX(0.05)); c[1] -= FxMul(FxMul(FxSin(am), nr), FX(0.05)); c[2] -= FxMul(nz, FX(0.05));
            if (r[k] < FX(0.001)) { fx t[3][3]; memcpy(t[0], q[0], sizeof t[0]); memcpy(t[1], q[2], sizeof t[0]); memcpy(t[2], q[3], sizeof t[0]); AddFace(t, 3, col, c, 0); }
            else AddFace(q, r[k + 1] < FX(0.001) ? 3 : 4, col, c, 0);
        }
    }
    for (int e = 0; e < 2; e++) {
        int k = e ? rings - 1 : 0;
        if (!(e ? cap1 : cap0) || r[k] < FX(0.001)) continue;
        fx v[8][3], c[3] = { x, y, z[k] + (e ? -FX(1.0) : FX(1.0)) };
        for (int i = 0; i < seg; i++) { fx a = rot + i * FX(6.2832) / seg; v[i][0] = x + FxMul(FxCos(a), r[k]); v[i][1] = y + FxMul(FxSin(a), r[k]); v[i][2] = z[k]; }
        AddFace(v, seg, col, c, 0);
    }
}

// --- Low-poly animals and people ------------------------------------------
// Oriented boxes, cones and hinged wings built from the same shaded faces as
// the trees, so animals keep their gameplay look: legs, body, head, ears,
// tails and dark eye dots. Each figure faces the way it is moving.
extern int familiar_size, familiar_color; // picked at the pet shop (town.c)
static fx hcos = FX(1.0), hsin;

static void OBox(fx cx, fx cy, fx along, fx side, fx z0, fx z1, fx hl, fx hw, COLORREF col) {
    fx x = cx + FxMul(hcos, along) - FxMul(hsin, side), y = cy + FxMul(hsin, along) + FxMul(hcos, side);
    fx c[3] = { x, y, (z0 + z1) / 2 }, px[4], py[4];
    static const int sa[4] = { 1, 1, -1, -1 }, sb[4] = { 1, -1, -1, 1 };
    for (int i = 0; i < 4; i++) { px[i] = x + FxMul(hcos, hl) * sa[i] - FxMul(hsin, hw) * sb[i]; py[i] = y + FxMul(hsin, hl) * sa[i] + FxMul(hcos, hw) * sb[i]; }
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        fx v[4][3] = { { px[i], py[i], z0 }, { px[j], py[j], z0 }, { px[j], py[j], z1 }, { px[i], py[i], z1 } };
        AddFace(v, 4, col, c, 0);
    }
    fx top[4][3];
    for (int i = 0; i < 4; i++) { top[i][0] = px[i]; top[i][1] = py[i]; top[i][2] = z1; }
    AddFace(top, 4, col, c, 0);
}

static void Cone(fx cx, fx cy, fx along, fx side, fx z0, fx z1, fx r, COLORREF col) {
    Prism(cx + FxMul(hcos, along) - FxMul(hsin, side), cy + FxMul(hsin, along) + FxMul(hcos, side), z0, z1, r, FX(0.0), 4, FX(0.0), col);
}

// A six-sided tube between two points (along/side/z on the figure): rounded limbs, bodies and tails.
static void Bone(fx cx, fx cy, fx a0, fx s0, fx z0, fx a1, fx s1, fx z1, fx r0, fx r1, COLORREF col) {
    fx p0[3] = { cx + FxMul(hcos, a0) - FxMul(hsin, s0), cy + FxMul(hsin, a0) + FxMul(hcos, s0), z0 }, p1[3] = { cx + FxMul(hcos, a1) - FxMul(hsin, s1), cy + FxMul(hsin, a1) + FxMul(hcos, s1), z1 };
    fx d[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] }, dl = FxSqrt(FxMul(d[0], d[0]) + FxMul(d[1], d[1]) + FxMul(d[2], d[2])) + FX_EPS;
    for (int i = 0; i < 3; i++) d[i] = FxDiv(d[i], dl);
    fx u[3] = { -d[1], d[0], FX(0.0) }, ul = FxSqrt(FxMul(u[0], u[0]) + FxMul(u[1], u[1]));
    if (ul < FX(0.01)) { u[0] = -hsin; u[1] = hcos; ul = FX(1.0); }
    for (int i = 0; i < 3; i++) u[i] = FxDiv(u[i], ul);
    fx w[3] = { FxMul(d[1], u[2]) - FxMul(d[2], u[1]), FxMul(d[2], u[0]) - FxMul(d[0], u[2]), FxMul(d[0], u[1]) - FxMul(d[1], u[0]) };
    fx c[3] = { (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (p0[2] + p1[2]) / 2 }, ring[2][6][3];
    for (int k = 0; k < 6; k++) {
        fx ca = FxCos(k * FX(1.0472)), sa = FxSin(k * FX(1.0472));
        for (int i = 0; i < 3; i++) { ring[0][k][i] = p0[i] + FxMul((FxMul(u[i], ca) + FxMul(w[i], sa)), r0); ring[1][k][i] = p1[i] + FxMul((FxMul(u[i], ca) + FxMul(w[i], sa)), r1); }
    }
    for (int k = 0; k < 6; k++) {
        int j = (k + 1) % 6; fx v[4][3];
        memcpy(v[0], ring[0][k], sizeof(v[0])); memcpy(v[1], ring[0][j], sizeof(v[0])); memcpy(v[2], ring[1][j], sizeof(v[0])); memcpy(v[3], ring[1][k], sizeof(v[0]));
        AddFace(v, 4, col, c, 0);
    }
    AddFace(ring[0], 6, col, c, 0); AddFace(ring[1], 6, col, c, 0);
}

// Two-part limb from a hip or shoulder: th swings it forward from straight down,
// bend folds the lower part at the knee (negative) or elbow (positive).
static fx limb_ea, limb_ez; // where the last limb ended (hand or ankle)
static void Limb(fx x, fx y, fx a, fx s, fx z, fx l1, fx l2, fx th, fx bend, fx r, COLORREF upper, COLORREF lower) {
    fx ka = a + FxMul(FxSin(th), l1), kz = z - FxMul(FxCos(th), l1), t2 = th + bend, fa = ka + FxMul(FxSin(t2), l2), fz = kz - FxMul(FxCos(t2), l2);
    if (fz < FX(0.0)) fz = FX(0.0);
    Bone(x, y, a, s, z, ka, s, kz, r, FxMul(r, FX(0.85)), upper);
    Bone(x, y, ka, s, kz, fa, s, fz, FxMul(r, FX(0.85)), FxMul(r, FX(0.7)), lower);
    fx jr = FxMul(r, FX(0.95)), jz[3] = { kz - jr, kz, kz + jr }, jw[3] = { FX(0.0), jr, FX(0.0) }; // a rounded knee or elbow
    Lathe(x + FxMul(hcos, ka) - FxMul(hsin, s), y + FxMul(hsin, ka) + FxMul(hcos, s), jz, jw, 3, 6, FX(0.0), 0, 0, lower);
    limb_ea = fa; limb_ez = fz;
}

// A small ball (hand, skull) at a point given along/side of the current heading.
static void Ball(fx x, fx y, fx a, fx s, fx z, fx r, COLORREF col) {
    fx jz[4] = { z - r, z - FxMul(r, FX(0.55)), z + FxMul(r, FX(0.55)), z + r }, jw[4] = { FxMul(r, FX(0.5)), r, r, FxMul(r, FX(0.5)) };
    Lathe(x + FxMul(hcos, a) - FxMul(hsin, s), y + FxMul(hsin, a) + FxMul(hcos, s), jz, jw, 4, 6, FX(0.0), 1, 1, col);
}

// A curved band around the figure from angle th0 to th1 (0 = straight ahead), with an
// outside and a lining, the hem ring rippling by flutter: capes and high collars.
static void CapeBand(fx x, fx y, fx th0, fx th1, int n, fx z0, fx r0, fx z1, fx r1, fx flutter, COLORREF out, COLORREF lining) {
    fx tm = Tk(GetTickCount(), 300);
    for (int i = 0; i < n; i++) {
        fx t0 = th0 + (th1 - th0) * i / n, t1 = th0 + (th1 - th0) * (i + 1) / n, tmid = (t0 + t1) / 2;
        fx h0 = r1 + FxMul(flutter, FxSin(tm + i * FX(1.3))), h1 = r1 + FxMul(flutter, FxSin(tm + (i + 1) * FX(1.3)));
        fx pa[4] = { FxMul(FxCos(t0), r0), FxMul(FxCos(t1), r0), FxMul(FxCos(t1), h1), FxMul(FxCos(t0), h0) }, ps[4] = { FxMul(FxSin(t0), r0), FxMul(FxSin(t1), r0), FxMul(FxSin(t1), h1), FxMul(FxSin(t0), h0) }, pz[4] = { z0, z0, z1, z1 };
        for (int side = 0; side < 2; side++) {
            fx v[4][3], sc = side ? FX(0.94) : FX(1.0), reach = side ? FX(3.0) : FX(0.0);
            for (int k = 0; k < 4; k++) { v[k][0] = x + FxMul((FxMul(hcos, pa[k]) - FxMul(hsin, ps[k])), sc); v[k][1] = y + FxMul((FxMul(hsin, pa[k]) + FxMul(hcos, ps[k])), sc); v[k][2] = pz[k]; }
            fx c[3] = { x + FxMul((FxMul(hcos, FxCos(tmid)) - FxMul(hsin, FxSin(tmid))), reach), y + FxMul((FxMul(hsin, FxCos(tmid)) + FxMul(hcos, FxSin(tmid))), reach), (z0 + z1) / 2 };
            AddFace(v, 4, side ? lining : out, c, 0);
        }
    }
}

// Two dots on the front of a head (drawn just in front of it).
static void Eyes(fx cx, fx cy, fx along, fx side, fx z, fx e, COLORREF col) {
    fx saved = face_bias; face_bias = FX(0.05); face_glow = 1;
    for (int k = -1; k <= 1; k += 2) {
        fx x = cx + FxMul(hcos, along) - FxMul(hsin, side) * k, y = cy + FxMul(hsin, along) + FxMul(hcos, side) * k;
        fx v[4][3] = { { x - FxMul(hsin, e), y + FxMul(hcos, e), z - e }, { x + FxMul(hsin, e), y - FxMul(hcos, e), z - e }, { x + FxMul(hsin, e), y - FxMul(hcos, e), z + e }, { x - FxMul(hsin, e), y + FxMul(hcos, e), z + e } };
        fx c[3] = { x - FxMul(hcos, FX(0.05)), y - FxMul(hsin, FX(0.05)), z };
        AddFace(v, 4, col, c, 0);
    }
    face_bias = saved; face_glow = 0;
}

// A wing hinged along the body, raised by angle f; seen from above or below.
static void Wing(fx cx, fx cy, fx z, fx len, fx span, fx f, int sidek, COLORREF col) {
    fx sx = -hsin * sidek, sy = hcos * sidek, tip = FxMul(span, FxCos(f)), up = FxMul(span, FxSin(f));
    fx v[4][3] = { { cx + FxMul(hcos, len), cy + FxMul(hsin, len), z }, { cx - FxMul(hcos, len), cy - FxMul(hsin, len), z },
                      { cx - FxMul(FxMul(hcos, len), FX(0.6)) + FxMul(sx, tip), cy - FxMul(FxMul(hsin, len), FX(0.6)) + FxMul(sy, tip), z + up }, { cx + FxMul(FxMul(hcos, len), FX(1.2)) + FxMul(sx, tip), cy + FxMul(FxMul(hsin, len), FX(1.2)) + FxMul(sy, tip), z + up } };
    fx above[3] = { cx, cy, z + FX(1.0) }, below[3] = { cx, cy, z - FX(1.0) };
    AddFace(v, 4, col, above, 0); AddFace(v, 4, col, below, 0);
}

static int heading_moving; // set by HeadingFor: whether that thing moved in the last 0.3 s
static fx HeadingFor(int key, fx x, fx y) {
    static fx lx[128], ly[128], hd[128]; static DWORD moved[128];
    key &= 127;
    fx dx = x - lx[key], dy = y - ly[key], d2 = FxMul(dx, dx) + FxMul(dy, dy);
    DWORD now = GetTickCount();
    if (d2 >= FX(4.0)) moved[key] = 0; else if (d2 > FX(0.00002)) moved[key] = now;
    heading_moving = moved[key] && now - moved[key] < 300;
    if (d2 >= FX(4.0)) hd[key] = FxAtan2((player_y) - y, (player_x) - x); // just appeared: facing the hero
    else if (d2 > FX(0.00002)) hd[key] = Wrap(hd[key] + FxMul(Wrap(FxAtan2(dy, dx) - hd[key]), FX(0.2)));
    lx[key] = x; ly[key] = y;
    return hd[key];
}

static void AnimalMesh(const FpThing *t) {
    DWORD now = GetTickCount();
    int kind = t->kind, idx = kind == FP_CRITTER ? t->seed >> 3 : t->seed, type = kind == FP_CRITTER ? t->seed & 7 : -1;
    fx h = HeadingFor(kind * 16 + (idx & 15), t->x, t->y), x = t->x, y = t->y, z = t->z;
    hcos = FxCos(h); hsin = FxSin(h);
    COLORREF black = RGB(12, 10, 10);
    if (kind == FP_FAMILIAR && t->seed <= 1) { // dog or cat in the coat and size picked at the pet shop
        static const COLORREF coats[2][4] = { { RGB(150, 100, 55), RGB(40, 35, 30), RGB(235, 230, 220), RGB(205, 160, 80) },
                                              { RGB(120, 120, 128), RGB(220, 130, 50), RGB(35, 35, 40), RGB(240, 240, 240) } };
        int cat = t->seed == 1;
        COLORREF c = coats[cat][familiar_color & 3], dark = Dim(c, FX(0.7));
        fx S = familiar_size == 0 ? FX(0.75) : familiar_size == 2 ? FX(1.3) : FX(1.0);
        // Its attention: a butterfly close by (the cat paws at it, the dog sniffs the air) or nothing (the cat pounces on leaves).
        fx bd = FX(4.0), bx = FX(0.0), by = FX(0.0);
        for (int i = 0; i < MAX_BUTTERFLIES; i++) if (butterflies[i].active) {
            fx dx = (butterflies[i].x) - x, dy = (butterflies[i].y) - y;
            if (FxMul(dx, dx) + FxMul(dy, dy) < bd) { bd = FxMul(dx, dx) + FxMul(dy, dy); bx = (butterflies[i].x); by = (butterflies[i].y); }
        }
        int near_fly = bd < FX(2.6);
        if (near_fly) { fx fa = FxAtan2(by - y, bx - x); hcos = FxCos(fa); hsin = FxSin(fa); }
        static fx lfx, lfy; fx mv = FxAbs(x - lfx) + FxAbs(y - lfy); lfx = x; lfy = y;
        fx gait = mv > FX(0.004) ? FX(1.0) : FX(0.15), ph = Tk(now, cat ? 120 : 105), pc = FxMod(Tk(now, 2600), FX(1.0)), crouch = FX(0.0), leap = FX(0.0);
        if (cat && !near_fly && gait < FX(0.5)) { // stalk low, wiggle, then spring at a leaf
            if (pc < FX(0.55)) crouch = FxDiv(pc, FX(0.55)); else if (pc < FX(0.75)) crouch = FX(1.0); else if (pc < FX(0.9)) leap = FxSin(FxMul(FxDiv((pc - FX(0.75)), FX(0.15)), FX(3.14159)));
        }
        fx lz = FxMul((FxMul(leap, FX(0.12)) - FxMul(crouch, FX(0.05))), S), paw = cat && near_fly ? FxMax(FX(0.0), FxSin(Tk(now, 160))) : FX(0.0);
        for (int l = 0; l < 4; l++) {
            int front = l < 2; fx sd = FxMul((l % 2 ? FX(0.05) : -FX(0.05)), S), a0 = FxMul((front ? FX(0.11) : -FX(0.11)), S), p = ph + (l == 0 || l == 3 ? FX(0.0) : FX(3.14159));
            fx th = FxMul(FxMul(FxSin(p), FX(0.5)), gait) + FxMul((front ? FX(0.7) : -FX(0.7)), leap);
            fx bend = front ? FxMul(FxMul(FX(0.35), FxMax(FX(0.0), FxSin(p + FX(1.2)))), gait) : -(FX(0.4) + FxMul(FxMul(FX(0.3), FxMax(FX(0.0), FxSin(p + FX(1.2)))), gait));
            if (l == 0 && paw > FX(0.0)) { th = FxMul(FX(1.2), paw); bend = FxMul(FX(0.6), paw); }
            Limb(x, y, a0, sd, FxMul(FX(0.17), S) + lz, FxMul(FX(0.085), S), FxMul(FX(0.085), S), th, bend - FxMul(FxMul(crouch, FX(0.6)), (front ? -FX(1.0) : FX(1.0))), FxMul(FX(0.024), S), dark, dark);
        }
        fx bl = FxMul((cat ? FX(0.15) : FX(0.17)), S), br = FxMul((cat ? FX(0.06) : FX(0.075)), S), bz = FxMul(FX(0.21), S) + lz - FxMul(FxMul(crouch, FX(0.03)), S);
        Bone(x, y, -bl, FX(0.0), bz, bl, FX(0.0), bz + FxMul(FX(0.015), S), br, FxMul(br, FX(1.05)), c);
        fx sniff = !cat && near_fly ? FxMul(FX(0.06), S) + FxMul(FxMul(FxSin(Tk(now, 90)), FX(0.012)), S) : FX(0.0), hz = FxMul(FX(0.31), S) + lz + sniff - FxMul(FxMul(crouch, FX(0.05)), S);
        Bone(x, y, FxMul(bl, FX(0.8)), FX(0.0), bz + FxMul(FX(0.02), S), FxMul(FX(0.18), S), FX(0.0), hz, FxMul(FX(0.045), S), FxMul(FX(0.04), S), c); // neck
        Bone(x, y, FxMul(FX(0.16), S), FX(0.0), hz, FxMul(FX(0.25), S), FX(0.0), hz - FxMul(FX(0.005), S), FxMul((cat ? FX(0.058) : FX(0.062)), S), FxMul((cat ? FX(0.05) : FX(0.052)), S), c);
        if (cat) {
            Cone(x, y, FxMul(FX(0.2), S), FxMul(FX(0.035), S), hz + FxMul(FX(0.04), S), hz + FxMul(FX(0.11), S), FxMul(FX(0.026), S), c); Cone(x, y, FxMul(FX(0.2), S), -FxMul(FX(0.035), S), hz + FxMul(FX(0.04), S), hz + FxMul(FX(0.11), S), FxMul(FX(0.026), S), c);
            fx lash = FxMul(FxMul(FxSin(Tk(now, crouch > FX(0.5) ? 120 : 420)), FX(0.04)), S);
            Bone(x, y, -bl, FX(0.0), bz + FxMul(FX(0.02), S), -bl - FxMul(FX(0.05), S), FxMul(lash, FX(0.5)), bz + FxMul(FX(0.12), S), FxMul(FX(0.02), S), FxMul(FX(0.018), S), c);
            Bone(x, y, -bl - FxMul(FX(0.05), S), FxMul(lash, FX(0.5)), bz + FxMul(FX(0.12), S), -bl - FxMul(FX(0.03), S), lash, bz + FxMul(FX(0.24), S), FxMul(FX(0.018), S), FxMul(FX(0.014), S), c);
        } else {
            Bone(x, y, FxMul(FX(0.25), S), FX(0.0), hz - FxMul(FX(0.01), S), FxMul(FX(0.33), S), FX(0.0), hz - FxMul(FX(0.025), S), FxMul(FX(0.034), S), FxMul(FX(0.03), S), dark); // snout
            OBox(x, y, FxMul(FX(0.335), S), FX(0.0), hz - FxMul(FX(0.015), S), hz + FxMul(FX(0.005), S), FxMul(FX(0.008), S), FxMul(FX(0.014), S), black);
            for (int k = -1; k <= 1; k += 2) Bone(x, y, FxMul(FX(0.19), S), FxMul(k * FX(0.055), S), hz + FxMul(FX(0.035), S), FxMul(FX(0.21), S), FxMul(k * FX(0.075), S), hz - FxMul(FX(0.045), S), FxMul(FX(0.02), S), FxMul(FX(0.016), S), dark); // floppy ears
            fx wag = FxMul(FxMul(FxSin(Tk(now, 90)), FX(0.04)), S);
            Bone(x, y, -bl, FX(0.0), bz + FxMul(FX(0.03), S), -bl - FxMul(FX(0.08), S), wag, bz + FxMul(FX(0.12), S), FxMul(FX(0.022), S), FxMul(FX(0.012), S), c);
        }
        Eyes(x, y, FxMul(FX(0.255), S), FxMul(FX(0.028), S), hz + FxMul(FX(0.015), S), FxMul(FX(0.011), S), cat ? RGB(110, 190, 60) : black);
        return;
    }
    if (kind == FP_BUTTERFLY) { // a lazy looping drift on top of its real path
        x += FxMul(FX(0.04), FxSin(Tk(now, 700) + FxI(idx))); y += FxMul(FX(0.03), FxCos(Tk(now, 900) + idx * FX(1.7))); z += FxMul(FX(0.03), FxSin(Tk(now, 380) + idx * FX(2.3)));
        static const COLORREF wing[4] = { RGB(250, 200, 60), RGB(240, 120, 180), RGB(120, 180, 250), RGB(250, 250, 250) };
        COLORREF c = wing[butterflies[idx].color_profile & 3];
        fx f = FX(0.2) + FxMul(FX(1.1), FxAbs(FxSin(Tk(now, 90) + FxI(idx))));
        OBox(x, y, FX(0.0), FX(0.0), z - FX(0.008), z + FX(0.008), FX(0.03), FX(0.006), RGB(40, 30, 20));
        Wing(x, y, z, FX(0.025), FX(0.06), f, 1, c); Wing(x, y, z, FX(0.025), FX(0.06), f, -1, c);
        return;
    }
    if (kind == FP_FAMILIAR && t->seed == 2) type = 5; // toad
    if (kind == FP_FAMILIAR && t->seed >= 3) type = t->seed == 4 ? 2 : 6; // owl or hawk
    fx hop = (now / (190 + 60 * (idx % 3)) + idx) % 4 == 0 ? FX(0.03) : FX(0.0);
    if (type == 0 || type == 6) { // bird / hawk
        COLORREF c = type == 6 ? RGB(140, 100, 60) : RGB(70, 120, 220);
        fx zb = (type == 6 ? FX(0.9) + FxMul(FxSin(Tk(now, 300)), FX(0.05)) : hop), f = type == 6 ? FxMul(FxSin(Tk(now, 150)), FX(0.6)) : -FX(0.2);
        OBox(x, y, FX(0.0), FX(0.0), zb + FX(0.05), zb + FX(0.13), FX(0.06), FX(0.035), c);
        OBox(x, y, FX(0.065), FX(0.0), zb + FX(0.11), zb + FX(0.18), FX(0.03), FX(0.028), c);
        OBox(x, y, FX(0.105), FX(0.0), zb + FX(0.135), zb + FX(0.15), FX(0.015), FX(0.008), RGB(240, 170, 40));
        OBox(x, y, -FX(0.08), FX(0.0), zb + FX(0.1), zb + FX(0.115), FX(0.03), FX(0.02), Dim(c, FX(0.7)));
        Wing(x, y, zb + FX(0.12), FX(0.04), type == 6 ? FX(0.16) : FX(0.05), f, 1, Dim(c, FX(0.8))); Wing(x, y, zb + FX(0.12), FX(0.04), type == 6 ? FX(0.16) : FX(0.05), f, -1, Dim(c, FX(0.8)));
        Eyes(x, y, FX(0.096), FX(0.012), zb + FX(0.16), FX(0.006), black);
    } else if (type == 1) { // squirrel with a big curled tail
        COLORREF c = RGB(150, 95, 50);
        OBox(x, y, FX(0.0), FX(0.0), hop, hop + FX(0.1), FX(0.07), FX(0.04), c);
        OBox(x, y, FX(0.07), FX(0.0), hop + FX(0.07), hop + FX(0.15), FX(0.035), FX(0.032), c);
        Cone(x, y, FX(0.065), FX(0.02), hop + FX(0.15), hop + FX(0.19), FX(0.012), c); Cone(x, y, FX(0.065), -FX(0.02), hop + FX(0.15), hop + FX(0.19), FX(0.012), c);
        OBox(x, y, -FX(0.09), FX(0.0), hop + FX(0.04), hop + FX(0.16), FX(0.03), FX(0.035), Mix(c, RGB(200, 160, 110), FX(0.3)));
        OBox(x, y, -FX(0.11), FX(0.0), hop + FX(0.16), hop + FX(0.25), FX(0.035), FX(0.035), Mix(c, RGB(200, 160, 110), FX(0.3)));
        Eyes(x, y, FX(0.106), FX(0.015), hop + FX(0.125), FX(0.007), black);
    } else if (type == 2) { // owl, upright with tufts and big yellow eyes
        COLORREF c = RGB(140, 115, 85);
        OBox(x, y, FX(0.0), FX(0.0), FX(0.0), FX(0.24), FX(0.07), FX(0.07), c);
        OBox(x, y, FX(0.0), FX(0.0), FX(0.24), FX(0.36), FX(0.065), FX(0.065), c);
        Cone(x, y, FX(0.0), FX(0.045), FX(0.36), FX(0.42), FX(0.02), c); Cone(x, y, FX(0.0), -FX(0.045), FX(0.36), FX(0.42), FX(0.02), c);
        Eyes(x, y, FX(0.068), FX(0.028), FX(0.31), FX(0.018), RGB(250, 210, 60));
        face_bias = FX(0.02); Eyes(x, y, FX(0.072), FX(0.028), FX(0.31), FX(0.008), black); face_bias = FX(0.0);
    } else if (type == 3) { // bat
        fx zb = FX(0.6) + FxMul(FxSin(Tk(now, 200) + FxI(idx)), FX(0.08));
        OBox(x, y, FX(0.0), FX(0.0), zb, zb + FX(0.06), FX(0.03), FX(0.025), RGB(45, 40, 50));
        Wing(x, y, zb + FX(0.04), FX(0.04), FX(0.12), FxMul(FxSin(Tk(now, 70)), FX(0.8)), 1, RGB(60, 52, 66)); Wing(x, y, zb + FX(0.04), FX(0.04), FX(0.12), FxMul(FxSin(Tk(now, 70)), FX(0.8)), -1, RGB(60, 52, 66));
    } else if (type == 4) { // scorpion: flat body, pincers, tail curled up and over
        COLORREF c = RGB(150, 95, 45);
        OBox(x, y, FX(0.0), FX(0.0), FX(0.0), FX(0.03), FX(0.05), FX(0.03), c);
        OBox(x, y, FX(0.07), FX(0.035), FX(0.0), FX(0.02), FX(0.025), FX(0.012), c); OBox(x, y, FX(0.07), -FX(0.035), FX(0.0), FX(0.02), FX(0.025), FX(0.012), c);
        for (int sgm = 0; sgm < 4; sgm++) OBox(x, y, -FX(0.06) - sgm * FX(0.012) + (sgm > 2) * FX(0.03), FX(0.0), FX(0.02) + sgm * FX(0.025), FX(0.045) + sgm * FX(0.025), FX(0.012), FX(0.01), c);
    } else if (type == 5) { // toad
        COLORREF c = RGB(70, 130, 60);
        OBox(x, y, FX(0.0), FX(0.0), hop, hop + FX(0.08), FX(0.08), FX(0.07), c);
        OBox(x, y, FX(0.05), FX(0.035), hop + FX(0.08), hop + FX(0.11), FX(0.015), FX(0.015), c); OBox(x, y, FX(0.05), -FX(0.035), hop + FX(0.08), hop + FX(0.11), FX(0.015), FX(0.015), c);
        Eyes(x, y, FX(0.066), FX(0.035), hop + FX(0.095), FX(0.008), black);
    }
}

// Ground shadows thrown away from nearby campfires and torches at night.
extern int village_light_count;
static void FireShadows(fx x, fx y, fx tall) {
    if (scene_lit > FX(0.65) || in_cave) return;
    fx lx[MAX_CAMPFIRES + MAX_VILLAGE_LIGHTS], ly[MAX_CAMPFIRES + MAX_VILLAGE_LIGHTS]; int n = 0;
    for (int i = 0; i < MAX_CAMPFIRES; i++)
        if (campfires[i].active && campfires[i].screen_id == current_screen_index) { lx[n] = (campfires[i].x); ly[n++] = (campfires[i].y); }
    for (int i = 0; i < village_light_count && i < MAX_VILLAGE_LIGHTS; i++) { lx[n] = (village_lights[i].x); ly[n++] = (village_lights[i].y); }
    fx saved = face_bias; face_bias = FX(0.02); face_glow = 1;
    for (int i = 0; i < n; i++) {
        fx dx = x - lx[i], dy = y - ly[i], d = FxSqrt(FxMul(dx, dx) + FxMul(dy, dy));
        if (d < FX(0.2) || d > FX(6.0)) continue;
        dx = FxDiv(dx, d); dy = FxDiv(dy, d);
        fx len = FxMul(tall, (FX(0.8) + FxMul(d, FX(0.35)))), w = FX(0.1), flick = FX(1.0) + FxMul(FxSin(Tk(GetTickCount(), 90) + FxI(i)), FX(0.06));
        fx v[4][3] = { { x - FxMul(dy, w), y + FxMul(dx, w), FX(0.01) }, { x + FxMul(dy, w), y - FxMul(dx, w), FX(0.01) },
                          { x + FxMul(FxMul(dx, len), flick) + FxMul(FxMul(dy, w), FX(1.6)), y + FxMul(FxMul(dy, len), flick) - FxMul(FxMul(dx, w), FX(1.6)), FX(0.01) },
                          { x + FxMul(FxMul(dx, len), flick) - FxMul(FxMul(dy, w), FX(1.6)), y + FxMul(FxMul(dy, len), flick) + FxMul(FxMul(dx, w), FX(1.6)), FX(0.01) } };
        fx below[3] = { x, y, -FX(1.0) };
        AddFace(v, 4, RGB(14, 16, 14), below, 0);
    }
    face_bias = saved; face_glow = 0;
}

// Mythic beasts at the size and shape they have in play (about twice a
// monster's height): Yeti and Bigfoot as long-armed apes that club with both
// fists, the Sphinx as a lion with a pharaoh's head, the Bunyip low and tusked.
static void BeastMesh(fx x, fx y, int type, int idx, fx swing) {
    DWORD now = GetTickCount();
    COLORREF black = RGB(12, 10, 10);
    if (type == MONSTER_YETI || type == MONSTER_BIGFOOT) {
        fx S = FX(1.9), st = FxMul(FxMul(FxSin(Tk(now, 220) + FxI(idx)), FX(0.05)), S);
        COLORREF fur = type == MONSTER_YETI ? RGB(232, 238, 246) : RGB(105, 70, 40), face = type == MONSTER_YETI ? RGB(120, 140, 165) : RGB(160, 120, 85);
        FireShadows(x, y, FxMul(FX(1.2), S));
        for (int side = -1; side <= 1; side += 2) // heavy rounded legs whose knees fold as they stomp
            Limb(x, y, FX(0.0), FxMul(side * FX(0.1), S), FxMul(FX(0.38), S), FxMul(FX(0.19), S), FxMul(FX(0.19), S), FxMul(side * st, FX(5.0)), -(FX(0.12) + FxMul(FxAbs(st), FX(3.0))), FxMul(FX(0.075), S), fur, fur);
        Prism(x, y, FxMul(FX(0.32), S), FxMul(FX(0.95), S), FxMul(FX(0.24), S), FxMul(FX(0.2), S), 7, FX(0.0), fur);
        for (int side = -1; side <= 1; side += 2) { // long arms that hang to the knees, raised together to club
            fx lift = FxMul(swing, (side > 0 ? FX(1.0) : FX(0.8)));
            Limb(x, y, FxMul(FX(0.02), S), FxMul(side * FX(0.28), S), FxMul(FX(0.9), S), FxMul(FX(0.3), S), FxMul(FX(0.32), S), FxMul(lift, FX(2.4)) - FxMul(side * st, FX(2.0)), FX(0.35) + FxMul(lift, FX(0.5)), FxMul(FX(0.065), S), fur, fur);
        }
        OBox(x, y, FxMul(FX(0.03), S), FX(0.0), FxMul(FX(0.93), S), FxMul(FX(1.18), S), FxMul(FX(0.11), S), FxMul(FX(0.11), S), fur);
        face_bias = FX(0.01); OBox(x, y, FxMul(FX(0.12), S), FX(0.0), FxMul(FX(0.96), S), FxMul(FX(1.12), S), FxMul(FX(0.025), S), FxMul(FX(0.075), S), face); face_bias = FX(0.0);
        Eyes(x, y, FxMul(FX(0.15), S), FxMul(FX(0.035), S), FxMul(FX(1.07), S), FxMul(FX(0.012), S), black);
    } else if (type == MONSTER_SPHINX) {
        fx S = FX(1.6), st = FxMul(FxMul(FxSin(Tk(now, 200) + FxI(idx)), FX(0.04)), S);
        COLORREF lion = RGB(200, 165, 100), skin = RGB(205, 165, 110), blue = RGB(40, 70, 160), gold = RGB(235, 195, 60);
        FireShadows(x, y, FxMul(FX(0.8), S));
        for (int l = 0; l < 4; l++) // lion legs: forelegs straight-ish, hind legs with a backward hock
            Limb(x, y, FxMul((l < 2 ? FX(0.26) : -FX(0.26)), S), FxMul((l % 2 ? FX(0.11) : -FX(0.11)), S), FxMul(FX(0.32), S), FxMul(FX(0.17), S), FxMul(FX(0.16), S),
                 FxMul((l % 2 ? st : -st), FX(6.0)), l < 2 ? -FX(0.15) : FX(0.35), FxMul(FX(0.05), S), lion, lion);
        OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.28), S), FxMul(FX(0.55), S), FxMul(FX(0.38), S), FxMul(FX(0.16), S), lion);
        OBox(x, y, -FxMul(FX(0.45), S), FX(0.0), FxMul(FX(0.45), S), FxMul(FX(0.5), S), FxMul(FX(0.1), S), FxMul(FX(0.015), S), lion);
        OBox(x, y, FxMul(FX(0.36), S), FX(0.0), FxMul(FX(0.5), S), FxMul(FX(0.95), S), FxMul(FX(0.09), S), FxMul(FX(0.14), S), blue);
        face_bias = FX(0.01);
        OBox(x, y, FxMul(FX(0.36), S), FX(0.0), FxMul(FX(0.68), S), FxMul(FX(0.71), S), FxMul(FX(0.095), S), FxMul(FX(0.145), S), gold); OBox(x, y, FxMul(FX(0.36), S), FX(0.0), FxMul(FX(0.58), S), FxMul(FX(0.61), S), FxMul(FX(0.095), S), FxMul(FX(0.145), S), gold);
        OBox(x, y, FxMul(FX(0.45), S), FX(0.0), FxMul(FX(0.62), S), FxMul(FX(0.86), S), FxMul(FX(0.02), S), FxMul(FX(0.08), S), skin);
        face_bias = FX(0.0);
        Eyes(x, y, FxMul(FX(0.475), S), FxMul(FX(0.03), S), FxMul(FX(0.77), S), FxMul(FX(0.012), S), black);
    } else { // Bunyip
        fx S = FX(1.6), st = FxMul(FxMul(FxSin(Tk(now, 260) + FxI(idx)), FX(0.03)), S);
        COLORREF hide = RGB(60, 70, 45);
        FireShadows(x, y, FxMul(FX(0.5), S));
        for (int l = 0; l < 4; l++) // short squat legs splayed out, knees bending as it waddles
            Limb(x, y, FxMul((l < 2 ? FX(0.25) : -FX(0.25)), S), FxMul((l % 2 ? FX(0.13) : -FX(0.13)), S), FxMul(FX(0.17), S), FxMul(FX(0.09), S), FxMul(FX(0.09), S),
                 FxMul((l % 2 ? st : -st), FX(8.0)), l < 2 ? -FX(0.3) : FX(0.3), FxMul(FX(0.05), S), hide, hide);
        OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.12), S), FxMul(FX(0.38), S), FxMul(FX(0.4), S), FxMul(FX(0.2), S), hide);
        OBox(x, y, -FxMul(FX(0.52), S), FX(0.0), FxMul(FX(0.16), S), FxMul(FX(0.26), S), FxMul(FX(0.15), S), FxMul(FX(0.05), S), hide);
        OBox(x, y, FxMul(FX(0.48), S), FX(0.0), FxMul(FX(0.2), S), FxMul(FX(0.42), S), FxMul(FX(0.12), S), FxMul(FX(0.12), S), RGB(70, 80, 50));
        OBox(x, y, FxMul(FX(0.6), S), FxMul(FX(0.05), S), FxMul(FX(0.12), S), FxMul(FX(0.24), S), FxMul(FX(0.012), S), FxMul(FX(0.012), S), RGB(235, 230, 210));
        OBox(x, y, FxMul(FX(0.6), S), -FxMul(FX(0.05), S), FxMul(FX(0.12), S), FxMul(FX(0.24), S), FxMul(FX(0.012), S), FxMul(FX(0.012), S), RGB(235, 230, 210));
        Eyes(x, y, FxMul(FX(0.6), S), FxMul(FX(0.06), S), FxMul(FX(0.36), S), FxMul(FX(0.012), S), RGB(200, 240, 80));
    }
}

// Monsters and villagers as low-poly people: legs that stride, a body,
// arms (the leading one swings a weapon up close), a head and eye dots.
static void HumanMesh(const FpThing *t) {
    DWORD now = GetTickCount();
    int enemy = t->kind == FP_ENEMY, idx = enemy ? t->seed >> 5 : t->seed >> 4, type = enemy ? t->seed & 31 : t->seed & 15;
    fx h = HeadingFor((enemy ? 100 : 108) + (idx & 15), t->x, t->y), x = t->x, y = t->y;
    hcos = FxCos(h); hsin = FxSin(h);
    fx S = FX(1.0); int horns = 0, crown = 0, hero = 0;
    COLORREF skin = RGB(225, 185, 150), cloth = RGB(70, 100, 170), legs = RGB(70, 55, 45), eye = RGB(12, 10, 10);
    if (enemy) {
        S = IS_MYTHIC_MONSTER(type) ? FX(2.0) : FX(1.0);
        if (type == MONSTER_ZOMBIE) { skin = RGB(150, 175, 135); cloth = RGB(95, 80, 60); }
        else if (type == MONSTER_SKELLY) { skin = RGB(225, 225, 215); cloth = skin; legs = skin; }
        else if (type == MONSTER_GOBLIN) { skin = RGB(90, 140, 60); cloth = RGB(110, 80, 50); S = FX(0.8); }
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
    fx dx = (player_x) - x, dy = (player_y) - y, swing = FX(0.0);
    if (enemy && FxMul(dx, dx) + FxMul(dy, dy) < FX(2.6)) { fx ph = FxMod(Tk(now, 455), FX(1.0)); if (ph < FX(0.45)) swing = FxSin(FxMul(FxDiv(ph, FX(0.45)), FX(3.14159))); }
    if (enemy && IS_MYTHIC_MONSTER(type)) { BeastMesh(x, y, type, idx, swing); return; }
    int vamp = enemy && type == MONSTER_DRACULA;
    fx st = FxMul(FxMul(FxSin(Tk(now, 140) + FxI(idx)), FX(0.04)), S);
    FireShadows(x, y, FxMul(FX(0.75), S));
    if (vamp) { // a cape curving round the back and flaring to the hem, under a high collar; red lining inside
        CapeBand(x, y, FX(1.75), FX(4.53), 7, FxMul(FX(0.66), S), FxMul(FX(0.11), S), FxMul(FX(0.14), S), FxMul(FX(0.21), S), FxMul(FX(0.012), S), RGB(25, 12, 18), RGB(150, 20, 35));
        CapeBand(x, y, FX(1.4), FX(4.88), 7, FxMul(FX(0.6), S), FxMul(FX(0.085), S), FxMul(FX(0.8), S), FxMul(FX(0.13), S), FX(0.0), RGB(25, 12, 18), RGB(150, 20, 35));
    }
    int zomb = enemy && type == MONSTER_ZOMBIE, skel = enemy && type == MONSTER_SKELLY; // zombies lurch slowly with arms out
    int moving = heading_moving, act = NPC_IDLE; // walk only when really moving; otherwise show the job they're at
    if (!enemy && !hero && idx != 13 && idx < village_npc_count && !moving && NpcAtWork(idx)) act = NpcActivity(idx);
    fx ph = Tk(now, zomb ? 260 : 140) + FxI(idx), wk = Tk(now, 190) + FxI(idx * 2); // stride and work rhythm
    fx lift = (moving ? FxMul(FxMul(FxAbs(FxSin(ph)), FX(0.012)), S) : FX(0.0)) - (act == NPC_NATURE ? FxMul(FX(0.15), S) : FX(0.0));
    COLORREF shoe = skel ? skin : RGB(58, 40, 28);
    fx lr = FxMul(skel ? FX(0.016) : FX(0.034), S), ar = FxMul(skel ? FX(0.014) : FX(0.028), S);
    for (int k = -1; k <= 1; k += 2) { // legs: stride when walking, planted when standing, folded when kneeling to gather
        fx p = ph + (k > 0 ? FX(0.0) : FX(3.14159)), th = FX(0.03), bend = -FX(0.06);
        if (moving) { th = FxMul(FxSin(p), FX(0.42)); bend = -(FX(0.15) + FxMul(FX(0.75), FxMax(FX(0.0), FxSin(p + FX(1.3))))); }
        else if (act == NPC_NATURE) { th = FX(1.1); bend = -FX(2.2); }
        Limb(x, y, FX(0.0), FxMul(k * FX(0.05), S), FxMul(FX(0.33), S) + lift, FxMul(FX(0.17), S), FxMul(FX(0.17), S), th, bend, lr, legs, legs);
        fx fa = limb_ea + FxMul(FX(0.03), S), fs = FxMul(k * FX(0.05), S), ta = fa + FxMul(FX(0.03), S);
        OBox(x, y, fa - FxMul(FX(0.015), S), fs, limb_ez, limb_ez + FxMul(FX(0.04), S), FxMul(FX(0.045), S), FxMul(FX(0.033), S), shoe); // foot
        Prism(x + FxMul(hcos, ta) - FxMul(hsin, fs), y + FxMul(hsin, ta) + FxMul(hcos, fs), limb_ez, limb_ez + FxMul(FX(0.04), S), FxMul(FX(0.033), S), FxMul(FX(0.028), S), 6, FX(0.0), shoe); // rounded toe
    }
    if (skel) { // the in-game skeleton: spine, three ribs, pelvis and shoulder bar, all bare bone
        Bone(x, y, FX(0.0), FX(0.0), FxMul(FX(0.32), S) + lift, FX(0.0), FX(0.0), FxMul(FX(0.62), S) + lift, FxMul(FX(0.016), S), FxMul(FX(0.014), S), skin);
        OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.3), S) + lift, FxMul(FX(0.35), S) + lift, FxMul(FX(0.04), S), FxMul(FX(0.075), S), skin);
        for (int r = 0; r < 3; r++) { fx rz = FxMul(FX(0.45) + r * FX(0.045), S) + lift; OBox(x, y, FxMul(FX(0.02), S), FX(0.0), rz, rz + FxMul(FX(0.014), S), FxMul(FX(0.03), S), FxMul(FX(0.085), S), skin); }
        OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.58), S) + lift, FxMul(FX(0.6), S) + lift, FxMul(FX(0.03), S), FxMul(FX(0.12), S), skin);
    } else Prism(x, y, FxMul(FX(0.31), S) + lift, FxMul(FX(0.6), S) + lift, FxMul(FX(0.085), S), FxMul(FX(0.1), S), 6, h, cloth); // rounded torso, broader at the shoulders
    fx ha = FX(0.0), hz = FX(0.0), ht = FX(0.0), la = FX(0.0), lz = FX(0.0);
    for (int k = -1; k <= 1; k += 2) { // arms: swing when walking, otherwise whatever the job needs; the lead (k > 0) arm holds the tool
        fx p = ph + (k > 0 ? FX(3.14159) : FX(0.0)), lead = k > 0 ? swing : FX(0.0), th, bend, wv = FxSin(wk);
        if (act == NPC_SWEEP) { th = FX(0.55) + FxMul(wv, FX(0.18)); bend = FX(0.5); }
        else if (act == NPC_FARM) { th = FX(0.6) + FxMul(FxMax(FX(0.0), wv), FX(1.7)); bend = FX(0.3); }
        else if (act == NPC_WATER) { th = FX(1.05) + FxMul(FxMax(FX(0.0), FxSin(wk / 3)), FX(0.12)); bend = FX(0.25); }
        else if (act == NPC_NATURE) { th = k > 0 ? FX(0.75) + FxMul(wv, FX(0.2)) : FX(0.35); bend = k > 0 ? FX(0.2) : FX(0.9); }
        else if (act == NPC_HAMMER) { th = k > 0 ? FX(0.9) + FxMul(FxMax(FX(0.0), FxSin(wk * 2)), FX(1.4)) : FX(0.9); bend = FX(0.35); }
        else if (act == NPC_INSPECT) { th = k > 0 ? FX(0.45) : -FX(0.35); bend = k > 0 ? FX(2.1) : -FX(0.4); }
        else { // walking swing, or at ease and breathing; zombies reach out and the lead arm raises a weapon
            fx amp = moving ? (zomb ? FX(0.1) : FX(0.35)) : FX(0.04);
            th = FxMul(FxSin(moving ? p : Tk(now, 900) + FxI(idx)), amp) + (zomb ? FX(1.35) : FX(0.0)) + FxMul(lead, FX(1.9));
            bend = zomb ? FX(0.1) : FX(0.25) + (moving ? FxMul(FX(0.25), FxMax(FX(0.0), FxSin(p))) : FX(0.0)) + FxMul(lead, FX(0.4));
        }
        Limb(x, y, FX(0.0), FxMul(k * FX(0.12), S), FxMul(FX(0.57), S) + lift, FxMul(FX(0.13), S), FxMul(FX(0.12), S), th, bend, ar, skel ? skin : cloth, skin);
        Ball(x, y, limb_ea, FxMul(k * FX(0.12), S), limb_ez, FxMul(skel ? FX(0.024) : FX(0.032), S), skin); // hand
        if (k > 0) { ha = limb_ea; hz = limb_ez; ht = th + bend; } else { la = limb_ea; lz = limb_ez; }
    }
    { // the tool in hand for the job, carried on along the forearm
        fx sr = FxMul(FX(0.12), S), dA = FxSin(ht), dZ = -FxCos(ht);
        COLORREF wood = RGB(120, 85, 50), iron = RGB(70, 70, 78);
        if (act == NPC_SWEEP) { // broom swishing side to side
            fx bs = FxMul(FxSin(wk), FX(0.12)), ba = ha + FX(0.32);
            Bone(x, y, ha, sr, hz + FX(0.1), ba, bs, FX(0.05), FX(0.012), FX(0.012), wood);
            OBox(x, y, ba, bs, FX(0.0), FX(0.06), FX(0.04), FX(0.1), RGB(205, 175, 90));
        } else if (act == NPC_FARM) { // hoe raised and chopped into the soil
            fx ea = ha + FxMul(dA, FX(0.45)), ez = FxMax(FX(0.03), hz + FxMul(dZ, FX(0.45)));
            Bone(x, y, ha - FxMul(dA, FX(0.08)), sr, hz - FxMul(dZ, FX(0.08)), ea, sr, ez, FX(0.012), FX(0.012), wood);
            OBox(x, y, ea + FX(0.03), sr, ez - FX(0.03), ez + FX(0.02), FX(0.035), FX(0.05), RGB(150, 150, 155));
        } else if (act == NPC_WATER) { // fishing: rod up and out over the water, line down to a bobbing red float
            fx ta = ha + FX(0.75), tz = hz + FX(0.55), fa = ha + FX(1.25), bob = FxMul(FxMax(FX(0.0), FxSin(wk / 2)), FX(0.02));
            Bone(x, y, ha, FX(0.0), hz, ta, FX(0.0), tz, FX(0.012), FX(0.006), wood);
            Bone(x, y, ta, FX(0.0), tz, fa, FX(0.0), FX(0.03) + bob, FX(0.003), FX(0.003), RGB(220, 220, 225));
            OBox(x, y, fa, FX(0.0), bob, FX(0.04) + bob, FX(0.02), FX(0.02), RGB(220, 40, 30));
        } else if (act == NPC_NATURE) { // kneeling to pick herbs into a basket
            OBox(x, y, la, -sr, FxMax(FX(0.0), lz - FX(0.08)), lz, FX(0.06), FX(0.05), RGB(170, 130, 70));
            Prism(x + FxMul(hcos, ha + FX(0.05)), y + FxMul(hsin, ha + FX(0.05)), FX(0.0), FX(0.07), FX(0.03), FX(0.0), 4, FX(0.0), RGB(70, 150, 60));
        } else if (act == NPC_HAMMER) { // blacksmith at the anvil with a glowing bar; sparks on each strike
            fx aa = FX(0.42), ea = ha + FxMul(dA, FX(0.2)), ez = hz + FxMul(dZ, FX(0.2));
            OBox(x, y, aa, FX(0.0), FX(0.0), FX(0.2), FX(0.06), FX(0.06), iron);
            OBox(x, y, aa, FX(0.0), FX(0.2), FX(0.27), FX(0.15), FX(0.08), iron);
            face_glow = 1; OBox(x, y, aa, FX(0.0), FX(0.27), FX(0.29), FX(0.08), FX(0.02), RGB(255, 140, 40));
            if (FxSin(wk * 2) < -FX(0.6))
                for (int s = 0; s < 3; s++) OBox(x, y, aa + (s - 1) * FX(0.06), (s - 1) * FX(0.05), FX(0.31) + s * FX(0.03), FX(0.33) + s * FX(0.03), FX(0.01), FX(0.01), RGB(255, 215, 90));
            face_glow = 0;
            Bone(x, y, ha, sr, hz, ea, sr, ez, FX(0.012), FX(0.012), wood);
            OBox(x, y, ea, sr, ez - FX(0.035), ez + FX(0.035), FX(0.05), FX(0.035), iron);
        } else if (skel) // the skeleton's notched grey sword, as in play
            Bone(x, y, ha, sr, hz, ha + FxMul(dA, FxMul(FX(0.32), S)), sr, hz + FxMul(dZ, FxMul(FX(0.32), S)), FxMul(FX(0.014), S), FxMul(FX(0.008), S), RGB(170, 175, 185));
    }
    if (swing > FX(0.0) && !skel) OBox(x, y, FxMul((FX(0.1) + FxMul(swing, FX(0.2))), S), FxMul(FX(0.13), S), FxMul((FX(0.5) + FxMul(swing, FX(0.1))), S), FxMul((FX(0.54) + FxMul(swing, FX(0.1))), S), FxMul(FX(0.12), S), FxMul(FX(0.015), S), RGB(200, 205, 215));
    if (skel) { // round skull with dark sockets and a jaw
        Ball(x, y, FX(0.0), FX(0.0), FxMul(FX(0.69), S) + lift, FxMul(FX(0.07), S), skin);
        OBox(x, y, FxMul(FX(0.03), S), FX(0.0), FxMul(FX(0.61), S) + lift, FxMul(FX(0.64), S) + lift, FxMul(FX(0.035), S), FxMul(FX(0.04), S), skin);
        Eyes(x, y, FxMul(FX(0.066), S), FxMul(FX(0.028), S), FxMul(FX(0.7), S) + lift, FxMul(FX(0.012), S), RGB(30, 25, 35));
        return;
    }
    Prism(x, y, FxMul(FX(0.6), S) + lift, FxMul(FX(0.75), S) + lift, FxMul(FX(0.058), S), FxMul(FX(0.05), S), 6, h, skin); // head
    if (horns) { Cone(x, y, FX(0.0), FxMul(FX(0.04), S), FxMul(FX(0.74), S), FxMul(FX(0.82), S), FxMul(FX(0.015), S), RGB(230, 220, 190)); Cone(x, y, FX(0.0), -FxMul(FX(0.04), S), FxMul(FX(0.74), S), FxMul(FX(0.82), S), FxMul(FX(0.015), S), RGB(230, 220, 190)); }
    if (crown) OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.74), S) + lift, FxMul(FX(0.79), S) + lift, FxMul(FX(0.055), S), FxMul(FX(0.055), S), RGB(235, 195, 60));
    if (hero) {
        const char *hd = WornItem("Head"); COLORREF hc = hd ? ClothingColor(hd) : CLOTH_UNKNOWN;
        OBox(x, y, -FxMul(FX(0.01), S), FX(0.0), FxMul(FX(0.72), S), FxMul(FX(0.78), S), FxMul(FX(0.066), S), FxMul(FX(0.066), S), hc != CLOTH_UNKNOWN ? hc : HeroHairColor());
    }
    if (hero && HeroTrimColor() != CLOTH_UNKNOWN) { // trim at the hem and collar
        face_bias = FX(0.01);
        OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.32), S), FxMul(FX(0.35), S), FxMul(FX(0.065), S), FxMul(FX(0.105), S), HeroTrimColor());
        OBox(x, y, FX(0.0), FX(0.0), FxMul(FX(0.57), S), FxMul(FX(0.6), S), FxMul(FX(0.065), S), FxMul(FX(0.105), S), HeroTrimColor());
        face_bias = FX(0.0);
    }
    Eyes(x, y, FxMul(FX(0.063), S), FxMul(FX(0.025), S), FxMul(FX(0.69), S) + lift, FxMul(FX(0.01), S), eye);
}

static void TreeMesh(const FpThing *t) {
    int s = t->seed, snow = (snow_cover) > FX(0.5);
    fx x = t->x + FxI((s * 37) % 21 - 10) / 100, y = t->y + FxI((s * 53) % 21 - 10) / 100;
    fx h = FX(0.85) + FxI(s * 29 % 30) / 100, rot = (s % 7) * FX(0.4), W = FX(1.0);
    if (IsBigTree(s)) { h = FxMul(h, FX(1.9)); W = FX(1.6); } // old giants: their crowns close over part of the sky
    Prism(x, y, FX(0.0), FxMul(FX(0.46), h), FxMul(FX(0.09), W), FxMul(FX(0.075), W), 5, rot, RGB(110, 75, 45));
    { fx sw = FxMul((FX(0.02) + FX(0.01) * abs(wind_dir)), FxSin(Tk(GetTickCount(), 1300) + s * FX(0.7))); x += sw; y += FxMul(sw, FX(0.4)); } // canopy sways, trunk stays
    if (IsDeciduousTree(s)) { // round broadleaf, the same trees as in play
        COLORREF leaf = snow ? RGB(205, 215, 225) : current_season == SEASON_FALL ? (s % 2 ? RGB(210, 120, 40) : RGB(220, 175, 50)) : RGB(80, 160, 60);
        // One rounded crown: every band shares its edges with the next, closed underneath.
        fx cz[6] = { FxMul(FX(0.4), h), FxMul(FX(0.6), h), FxMul(FX(0.88), h), FxMul(FX(1.16), h), FxMul(FX(1.4), h), FxMul(FX(1.53), h) }, cr[6] = { FxMul(FX(0.28), W), FxMul(FX(0.5), W), FxMul(FX(0.61), W), FxMul(FX(0.55), W), FxMul(FX(0.36), W), FxMul(FX(0.12), W) };
        Lathe(x, y, cz, cr, 6, 8, rot, 1, 1, leaf);
        if (TreeHasApples(s)) // the apples really on this tree
            for (int a = 0; a < 6; a++) {
                fx an = rot + a * FX(1.05), rr = FxMul((FX(0.62) - (a % 2) * FX(0.03)), W);
                Prism(x + FxMul(FxCos(an), rr), y + FxMul(FxSin(an), rr), FxMul((FX(0.85) + (a % 3) * FX(0.18)), h), FxMul((FX(0.93) + (a % 3) * FX(0.18)), h), FX(0.05), FX(0.05), 4, an, RGB(205, 35, 30));
            }
    } else { // two-tier pine
        COLORREF needle = RGB(35, 115, 55);
        // Skirt, then the top tier flaring out over it; both share the ring where they meet.
        fx lz[2] = { FxMul(FX(0.3), h), FxMul(FX(1.0), h) }, lr[2] = { FxMul(FX(0.62), W), FxMul(FX(0.27), W) };
        fx uz[3] = { FxMul(FX(1.0), h), FxMul(FX(0.94), h), FxMul(FX(2.0), h) }, ur[3] = { FxMul(FX(0.27), W), FxMul(FX(0.46), W), FX(0.0) };
        Lathe(x, y, lz, lr, 2, 8, rot, 1, 0, needle);
        Lathe(x, y, uz, ur, 3, 8, rot, 0, 0, snow ? RGB(225, 232, 240) : needle);
    }
}

static void RockMesh(const FpThing *t) {
    int s = t->seed;
    Prism(t->x, t->y, FX(0.0), FX(0.32) + (s % 5) * FX(0.03), FX(0.42), FX(0.2), 5, (s % 9) * FX(0.5), RGB(120, 120, 128));
}

static void FireMesh(const FpThing *t) {
    fx x = t->x, y = t->y, fl = FxMul(FX(0.08), FxSin(Tk(GetTickCount(), 90) + FxI(t->seed)));
    Prism(x, y, FX(0.0), FX(0.1), FX(0.32), FX(0.3), 6, FX(0.3), RGB(95, 65, 40));
    face_glow = 1;
    DWORD now = GetTickCount(); // a smaller flame (the light it gives is unchanged)
    Prism(x, y, FX(0.1), FX(0.42) + FxMul(fl, FX(0.7)), FX(0.15), FX(0.0), 5, Tk(now, 400), RGB(240, 120, 30));
    face_bias = FX(0.05);
    Prism(x, y, FX(0.1), FX(0.28) - FxMul(fl, FX(0.7)), FX(0.085), FX(0.0), 5, -(Tk(now, 300)), RGB(255, 220, 90));
    face_bias = FX(0.08); // flickering streaks inside: blue at the base, red and white tongues
    Prism(x, y, FX(0.1), FX(0.07), FX(0.07), FX(0.0), 5, Tk(now, 250), RGB(90, 130, 255));
    static const COLORREF tongue[3] = { RGB(230, 50, 30), RGB(255, 250, 235), RGB(255, 170, 60) };
    Prism(x + FxMul(FX(0.03), FxSin(Tk(now, 170))), y, FX(0.14), FX(0.2) + fl, FX(0.03), FX(0.0), 3, Tk(now, 200), tongue[(now / 140 + t->seed) % 3]);
    face_bias = FX(0.0); face_glow = 0;
}

static void DetailMesh(const FpThing *t) {
    fx x = t->x, y = t->y; DWORD now = GetTickCount();
    hcos = FX(1.0); hsin = FX(0.0);
    if (t->kind == FP_BARREL) {
        COLORREF wood = RGB(140, 90, 45), hoop = RGB(60, 40, 25);
        if (t->seed & 1) {
            Prism(x, y, FX(0.0), FX(0.42), FX(0.16), FX(0.16), 8, FX(0.0), wood);
            face_bias = FX(0.01);
            Prism(x, y, FX(0.07), FX(0.1), FX(0.165), FX(0.165), 8, FX(0.0), hoop); Prism(x, y, FX(0.14), FX(0.17), FX(0.165), FX(0.165), 8, FX(0.0), hoop); Prism(x, y, FX(0.32), FX(0.35), FX(0.165), FX(0.165), 8, FX(0.0), hoop);
            face_bias = FX(0.0);
        } else { // smashed: staves on the ground
            OBox(x - FX(0.1), y, FX(0.0), FX(0.0), FX(0.0), FX(0.03), FX(0.14), FX(0.03), wood); OBox(x + FX(0.08), y + FX(0.1), FX(0.0), FX(0.0), FX(0.0), FX(0.03), FX(0.03), FX(0.12), wood);
            OBox(x, y - FX(0.12), FX(0.0), FX(0.0), FX(0.0), FX(0.03), FX(0.1), FX(0.03), hoop);
        }
    } else if (t->kind == FP_LIGHT) {
        if (t->seed & 1) { // torch on a post
            fx fl = FxMul(FX(0.03), FxSin(Tk(now, 80) + FxI(t->seed)));
            Prism(x, y, FX(0.0), FX(0.95), FX(0.035), FX(0.03), 4, FX(0.0), RGB(95, 65, 40));
            face_glow = 1;
            Prism(x, y, FX(0.95), FX(1.12) + fl, FX(0.06), FX(0.0), 5, Tk(now, 300), RGB(245, 140, 40));
            face_bias = FX(0.04); Prism(x, y, FX(0.95), FX(1.05) - fl, FX(0.035), FX(0.0), 5, -(Tk(now, 250)), RGB(255, 225, 110));
            face_bias = FX(0.0); face_glow = 0;
        } else { // village fire pit: a stone ring round the fire
            Prism(x, y, FX(0.0), FX(0.08), FX(0.45), FX(0.42), 8, FX(0.2), RGB(120, 120, 125));
            FpThing f = *t; f.kind = FP_FIRE; FireMesh(&f);
        }
    } else if (t->kind == FP_ALTAR) {
        static const COLORREF orb[4] = { RGB(255, 210, 90), RGB(150, 90, 230), RGB(120, 220, 120), RGB(60, 190, 200) };
        Prism(x, y, FX(0.0), FX(0.12), FX(0.42), FX(0.4), 4, FX(0.785), RGB(110, 110, 118));
        Prism(x, y, FX(0.12), FX(0.62), FX(0.26), FX(0.24), 4, FX(0.785), RGB(140, 140, 148));
        Prism(x, y, FX(0.62), FX(0.7), FX(0.34), FX(0.34), 4, FX(0.785), RGB(120, 120, 128));
        face_glow = 1; Prism(x, y, FX(0.72), FX(0.86) + FxMul(FX(0.02), FxSin(Tk(now, 400))), FX(0.08), FX(0.02), 6, Tk(now, 900), orb[t->seed & 3]); face_glow = 0;
    } else if (t->kind == FP_STALL) { // market stall: counter, posts and a blue canopy
        OBox(x, y, FX(0.0), FX(0.0), FX(0.0), FX(0.45), FX(0.75), FX(0.3), RGB(170, 115, 65));
        for (int k = 0; k < 4; k++) OBox(x, y, k < 2 ? -FX(0.7) : FX(0.7), k % 2 ? FX(0.28) : -FX(0.28), FX(0.45), FX(1.05), FX(0.03), FX(0.03), RGB(110, 75, 45));
        OBox(x, y, FX(0.0), FX(0.0), FX(1.05), FX(1.1), FX(0.85), FX(0.42), RGB(70, 115, 155));
    } else if (t->kind == FP_TENT) { // the merchant's red tent
        Prism(x, y, FX(0.0), FX(1.25), FX(0.85), FX(0.0), 4, FX(0.785), RGB(220, 50, 40));
        face_bias = FX(0.02); Prism(x, y, FX(0.0), FX(0.04), FX(0.87), FX(0.87), 4, FX(0.785), RGB(235, 195, 60)); face_bias = FX(0.0);
    }
}

static void HomeMesh(const FpThing *t, int night) {
    fx x0 = t->x - (HOME_HALF_X), x1 = t->x + (HOME_HALF_X), y0 = t->y - (HOME_HALF_Y), y1 = t->y + (HOME_HALF_Y), wh = FX(0.95), rz = FX(1.6), o = FX(0.12);
    fx c[3] = { t->x, t->y, FX(0.7) };
    COLORREF wall = RGB(225, 205, 160), roof = RGB(165, 60, 45);
    fx w1[4][3] = { { x0, y1, 0 }, { x1, y1, 0 }, { x1, y1, wh }, { x0, y1, wh } };
    fx w2[4][3] = { { x0, y0, 0 }, { x1, y0, 0 }, { x1, y0, wh }, { x0, y0, wh } };
    fx w3[5][3] = { { x0, y0, 0 }, { x0, y1, 0 }, { x0, y1, wh }, { x0, t->y, rz }, { x0, y0, wh } };
    fx w4[5][3] = { { x1, y0, 0 }, { x1, y1, 0 }, { x1, y1, wh }, { x1, t->y, rz }, { x1, y0, wh } };
    AddFace(w1, 4, wall, c, 0); AddFace(w2, 4, wall, c, 0); AddFace(w3, 5, wall, c, 0); AddFace(w4, 5, wall, c, 0);
    fx r1[4][3] = { { x0 - o, y1 + o, wh - FX(0.08) }, { x1 + o, y1 + o, wh - FX(0.08) }, { x1 + o, t->y, rz }, { x0 - o, t->y, rz } };
    fx r2[4][3] = { { x0 - o, y0 - o, wh - FX(0.08) }, { x1 + o, y0 - o, wh - FX(0.08) }, { x1 + o, t->y, rz }, { x0 - o, t->y, rz } };
    AddFace(r1, 4, roof, c, 0); AddFace(r2, 4, roof, c, 0);
    face_bias = FX(0.08);
    fx door[4][3] = { { t->x - FX(0.17), y1 + FX(0.01), 0 }, { t->x + FX(0.17), y1 + FX(0.01), 0 }, { t->x + FX(0.17), y1 + FX(0.01), FX(0.58) }, { t->x - FX(0.17), y1 + FX(0.01), FX(0.58) } };
    AddFace(door, 4, RGB(105, 70, 40), c, 0);
    face_glow = night; // windows glow at night
    for (int k = 0; k < 2; k++) {
        fx wx = k ? x1 + FX(0.01) : x0 - FX(0.01);
        fx w[4][3] = { { wx, t->y - FX(0.18), FX(0.4) }, { wx, t->y + FX(0.18), FX(0.4) }, { wx, t->y + FX(0.18), FX(0.7) }, { wx, t->y - FX(0.18), FX(0.7) } };
        AddFace(w, 4, night ? RGB(255, 205, 110) : RGB(85, 115, 150), c, 0);
    }
    face_glow = 0; face_bias = FX(0.0);
}

static void GroundMesh(void) {
    COLORREF g = GroundColor();
    for (int r = FxTrunc(player_y) - FP_RANGE; r <= FxTrunc(player_y) + FP_RANGE; r++)
        for (int c = FxTrunc(player_x) - FP_RANGE; c <= FxTrunc(player_x) + FP_RANGE; c++) {
            if (r < 0 || c < 0 || r >= MAP_SIZE || c >= MAP_SIZE) continue;
            int t = VISUAL_MAP[r][c];
            COLORREF col = t == 12 ? RGB(50, 120, 210) : (t == 10 && !in_cave) ? GetTileColor(10, 0) : g;
            if (t == 12) { // water colours bleed slowly into each other, like wet watercolour
                fx now = Tk(GetTickCount(), 1000), w = FxSin(FxMul(now, FX(1.1)) + r * FX(0.9) + c * FX(0.6)) + FxSin(FxMul(now, FX(0.7)) - r * FX(0.4) + c * FX(1.1));
                col = RGB(52 + FxTrunc(FxMul(w, FX(9.0))), 122 + FxTrunc(FxMul(w, FX(11.0))), 212 + FxTrunc(FxMul(w, FX(6.0))));
            }
            else col = Dim(col, FX(0.96) + (abs(RainHash(r * 977 + c * 131)) % 5) * FX(0.01));
            fx v[4][3] = { { FxI(c), FxI(r), FX(0.0) }, { FxI(c + 1), FxI(r), FX(0.0) }, { FxI(c + 1), FxI(r + 1), FX(0.0) }, { FxI(c), FxI(r + 1), FX(0.0) } };
            AddFace(v, 4, col, NULL, 1);
            fx fdx = FxI(c) + FX(0.5) - (player_x), fdy = FxI(r) + FX(0.5) - (player_y);
            if (t != 1 && t != 10 && t != 11 && t != 12 && !in_cave && (snow_cover) < FX(0.5) && FxMul(fdx, fdx) + FxMul(fdy, fdy) < FX(64.0) &&
                (current_biome == BIOME_GRASSLANDS || current_biome == BIOME_FOREST || current_biome == BIOME_PRAIRIE || current_biome == BIOME_OASIS) &&
                abs(RainHash(r * 613 + c * 37)) % 7 == 0) { // a flower nodding in the breeze
                static const COLORREF petal[4] = { RGB(240, 90, 120), RGB(250, 215, 70), RGB(170, 120, 240), RGB(250, 250, 245) };
                int hsh = abs(RainHash(r * 31 + c * 7));
                fx nod = FxMul(FX(0.02), FxSin(Tk(GetTickCount(), 900) + r * FX(0.8) + c * FX(0.5))), fxp = FxI(c) + FX(0.2) + FxI(hsh % 60) / 100, fy = FxI(r) + FX(0.2) + FxI(hsh / 60 % 60) / 100;
                hcos = FX(1.0); hsin = FX(0.0);
                OBox(fxp, fy, FX(0.0), FX(0.0), FX(0.0), FX(0.13), FX(0.006), FX(0.006), RGB(60, 130, 50));
                OBox(fxp + nod, fy, FX(0.0), FX(0.0), FX(0.13), FX(0.16), FX(0.03), FX(0.03), petal[hsh % 4]);
            }
        }
}

// Past the polygon tiles the ground fades into the haze at the horizon.
static void DrawGround(HDC hdc, fx lit) {
    int top = horizon < 0 ? 0 : horizon;
    Box(hdc, 0, top, WINDOW_WIDTH, WINDOW_HEIGHT, Mix(Dim(GroundColor(), lit), haze, FX(0.85)));
}

static int FarFirst(const void *a, const void *b) {
    fx da = ((const FpThing *)a)->depth, db = ((const FpThing *)b)->depth;
    return da < db ? 1 : da > db ? -1 : 0;
}

// The gaze lingers on one thing for a few seconds, then picks another: the
// bobber while fishing, otherwise the fire, the familiar, animals and
// butterflies before trees and rocks, with a long look up at the sky now and then.
static void UpdateGaze(int fresh) {
    static DWORD gaze_until; static int gaze_n; static fx target_yaw, target_pitch;
    static const int interest[10] = { 10, 6, 18, 50, 40, 32, 45, 95, 60, 30 };
    DWORD now = GetTickCount();
    if (fresh) {
        yaw = FxAtan2((facing_y), (facing_x)); // look the way the hero faces, diagonals included
        pitch = FX(0.0); target_yaw = yaw; target_pitch = FX(0.0); gaze_until = now + 2500;
    }
    if (now >= gaze_until) {
        gaze_until = now + GAZE_MS; gaze_n++;
        int pick = -1, best = -1000;
        for (int i = 0; i < thing_count; i++) {
            fx dx = things[i].x - (player_x), dy = things[i].y - (player_y);
            int s = interest[things[i].kind] + RainHash(i * 17 + gaze_n * 101) % 25 - FxTrunc(FxMul(FxSqrt(FxMul(dx, dx) + FxMul(dy, dy)), FX(1.5)));
            if (s > best) { best = s; pick = i; }
        }
        int fishing = pick >= 0 && things[pick].kind == FP_BOBBER;
        fx nod = FxI(RainHash(gaze_n * 7 + 3) % 100 - 50) / 350; // look a little up or down, never rigidly level
        if (!fishing && !in_cave && (gaze_n % 4 == 0 || pick < 0)) { target_pitch = FX(0.6); target_yaw = yaw + FX(0.5); }
        else if (pick >= 0) {
            target_yaw = FxAtan2(things[pick].y - (player_y), things[pick].x - (player_x));
            target_pitch = fishing ? -FX(0.15) : (things[pick].kind == FP_TREE || things[pick].kind == FP_HOME ? FX(0.12) : -FX(0.05)) + nod;
            if (!fishing && gaze_n % 5 == 2) target_pitch = -FX(0.32); // down at the ground now and then
        }
    }
    // An animal that wanders close is "heard": the head turns to it faster.
    static DWORD heard_at[64], fast_until;
    for (int i = 0; i < thing_count; i++) {
        if (things[i].kind != FP_CRITTER && things[i].kind != FP_BUTTERFLY) continue;
        fx dx = things[i].x - (player_x), dy = things[i].y - (player_y);
        int key = (things[i].kind == FP_CRITTER ? things[i].seed >> 3 : 32 + things[i].seed) & 63;
        if (FxMul(dx, dx) + FxMul(dy, dy) > FX(25.0)) continue;
        if (now - heard_at[key] > 15000) {
            target_yaw = FxAtan2(dy, dx); target_pitch = -FX(0.08);
            gaze_until = now + 3500; fast_until = now + 1500;
        }
        heard_at[key] = now;
    }
    fx rate = now < fast_until ? FX(0.05) : FX(0.012);
    yaw += FxMul(Wrap(target_yaw - yaw), rate);
    pitch += FxMul(FxMul((target_pitch - pitch), rate), FX(1.25));
}

// Wind in AFK space: gameplay wind_dir pushes along screen x, which is the
// world's (+x, -y) diagonal. Returns how fast that wind crosses the view.
static fx WindAcross(fx *wx, fx *wy) {
    fx w = wind_dir * FX(0.7071);
    *wx = w; *wy = -w;
    return -FxMul(*wx, FxSin(yaw)) + FxMul(*wy, FxCos(yaw));
}

// A cloud as a soft density field from three layers of different shapes: long thin
// wisps (faint), rounded lumps (the body) and small bright tops. Where they overlap
// the density adds up, so it is thick in places and wispy at the edges, a little
// darker underneath. Worked out per pixel at a third of screen resolution.
static void CloudPuff(HDC hdc, int cx, int cy, int w, COLORREF c, int alpha, int seed) {
    int x0 = cx - w, y0 = cy - w / 2, W = 2 * w, H = w;
    if (W < 6 || x0 + W < 0 || x0 > WINDOW_WIDTH || y0 + H < 0) return;
    int q = W > 90 ? 3 : 1, sw = (W + q - 1) / q, sh = (H + q - 1) / q;
    fx tm = Tk(GetTickCount(), 9000), bx[11], by[11], rx[11], ry[11], st[11], br[11];
    int nb = 0;
#define CR(k) (FxI(RainHash(seed * 13 + (k)) & 1023) / 1023)
    for (int k = 0; k < 3; k++, nb++) { // wisps: wide and flat, low down
        rx[nb] = FX(0.26) + FxMul(FX(0.16), CR(k)); ry[nb] = FX(0.1) + FxMul(FX(0.06), (FX(0.5) + FxMul(FX(0.5), FxSin(FxI(seed) + k * FX(2.1) + tm))));
        bx[nb] = rx[nb] + FxMul(FX(1.0) - FxMul(FX(2.0), rx[nb]), CR(k + 10)); by[nb] = FX(0.68) + FxMul(FX(0.14), CR(k + 20)); st[nb] = FX(0.5); br[nb] = FX(0.0);
    }
    for (int k = 0; k < 5; k++, nb++) { // lumps: each swells and shrinks slowly
        fx n = FX(0.5) + FxMul(FX(0.25), FxSin(seed * FX(0.37) + k * FX(2.1) + tm)) + FxMul(FX(0.25), FxCos(seed * FX(0.11) + k * FX(1.3) - FxMul(tm, FX(0.7))));
        rx[nb] = FX(0.08) + FxMul(FX(0.07), n); ry[nb] = FxMul(FxMul(rx[nb], FX(2.0)), FX(0.8) + FxMul(FX(0.3), CR(k + 30)));
        bx[nb] = FX(0.17) + FX(0.165) * k + FxMul(FX(0.05), (CR(k + 35) - FX(0.5))); by[nb] = FX(0.7) - FxMul(ry[nb], FX(0.55)) - FxMul(FX(0.08), CR(k + 40)); st[nb] = FX(0.75); br[nb] = FX(0.2);
    }
    for (int k = 0; k < 3; k++, nb++) { // bright tops where the light catches it
        rx[nb] = FX(0.05) + FxMul(FX(0.04), (FX(0.5) + FxMul(FX(0.5), FxSin(seed * FX(0.5) + k * FX(1.7) + tm)))); ry[nb] = FxMul(rx[nb], FX(1.7));
        bx[nb] = FX(0.25) + FxMul(FX(0.5), CR(k + 60)); by[nb] = FX(0.3) + FxMul(FX(0.15), CR(k + 70)); st[nb] = FX(0.55); br[nb] = FX(1.0);
    }
#undef CR
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), sw, -sh, 1, 32, BI_RGB, 0, 0, 0, 0, 0 } };
    void *bits = NULL; HBITMAP b = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!b || !bits) { if (b) DeleteObject(b); return; }
    unsigned char *px = (unsigned char *)bits;
    fx amax = FxI(alpha) / 255;
    for (int j = 0; j < sh; j++)
        for (int i = 0; i < sw; i++, px += 4) {
            fx u = (FxI(i) + FX(0.5)) / sw, v = (FxI(j) + FX(0.5)) / sh, D = FX(0.0), B = FX(0.0);
            for (int k = 0; k < nb; k++) {
                fx du = FxDiv((u - bx[k]), rx[k]), dv = FxDiv((v - by[k]), ry[k]), d2 = FxMul(du, du) + FxMul(dv, dv);
                if (d2 < FX(1.0)) { fx f = FxMul(FxMul((FX(1.0) - d2), (FX(1.0) - d2)), st[k]); D += f; B += FxMul(f, br[k]); }
            }
            if (D <= FX(0.0)) { px[0] = px[1] = px[2] = px[3] = 0; continue; }
            fx a = FxMul((FX(1.0) - FxExpNeg(FxMul(FX(2.4), D))), amax), lift = FxDiv(FxMul(FX(0.5), B), D), shade = FX(1.0) - FxMul(FX(0.2), v);
            COLORREF col = Mix(Dim(c, shade), RGB(255, 255, 255), lift);
            px[0] = (unsigned char)FxTrunc(GetBValue(col) * a); px[1] = (unsigned char)FxTrunc(GetGValue(col) * a);
            px[2] = (unsigned char)FxTrunc(GetRValue(col) * a); px[3] = (unsigned char)FxTrunc(FxMul(a, FX(255.0)));
        }
    HDC m = CreateCompatibleDC(hdc); HGDIOBJ om = SelectObject(m, b);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(hdc, x0, y0, W, H, m, 0, 0, sw, sh, bf);
    SelectObject(m, om); DeleteDC(m); DeleteObject(b);
}

// Clouds sit high over an 80-tile patch around the hero and drift with the
// calculated wind; some are see-through like the ghost, some solid.
static void DrawClouds(HDC hdc, fx light) {
    if (in_cave || current_biome == BIOME_CAVE) return;
    fx wx, wy; WindAcross(&wx, &wy);
    fx t = Tk(GetTickCount(), 1000), k = light < FX(0.25) ? FX(0.25) : light;
    int n = IsAdverseWeather() ? 12 : 3 + FxTrunc(FxMul((weather_intensity), FX(6.0)));
    for (int i = 0; i < n; i++) {
        int hs = RainHash(i * 977 + 31);
        // Pinned to bearings like the stars, drifting slowly with the wind, so a
        // turning gaze slides them smoothly and they never jump or vanish.
        fx az = FxI(hs % 6283) / 1000 + FxMul(FxMul(t, FX(0.004)), (wx - wy)), el = FX(0.10) + FxI(hs / 6283 % 300) / 1000;
        int x, y; SkyPoint(az, el, &x, &y);
        if (x < -400) continue;
        int solid = i % 3 != 0, w = FxTrunc(FxMul(FxMul(focal, (FX(0.10) + (hs % 5) * FX(0.04))), (FX(0.7) + el)));
        COLORREF c = IsAdverseWeather() ? RGB(120, 125, 135) : solid ? RGB(245, 245, 250) : RGB(215, 220, 230);
        CloudPuff(hdc, x, y, w, Dim(c, k + FX(0.15)), solid ? 215 : 95, hs);
    }
}

// Rain and snow slant and drift with the wind; leaves blow off the trees in
// autumn or when it's gusty.
static void DrawWeather(HDC hdc) {
    if (in_cave || current_biome == BIOME_CAVE) return;
    DWORD now = GetTickCount();
    fx wx, wy, across = WindAcross(&wx, &wy);
    // Particles hang in the world: turning or tilting the gaze slides them
    // across the screen instead of dragging them along with the camera.
    int pan = FxTrunc(FxDiv(yaw, FP_FOV) * WINDOW_WIDTH), tilt = WINDOW_HEIGHT / 2 - horizon;
    if (IsAdverseWeather()) {
        int snow = current_season == SEASON_WINTER || current_biome == BIOME_TUNDRA, n = 80 + FxTrunc(weather_intensity * 120);
        for (int i = 0; i < n; i++) {
            int hsh = RainHash(i * 613 + 9), z = i % 3; // z: 0 far and small .. 2 near and big
            int x = (int)(((hsh % WINDOW_WIDTH) + (int)((((int64_t)now * across) * (1 + z) / (snow ? 80 : 24)) >> 16) - pan + 100 * WINDOW_WIDTH) % WINDOW_WIDTH);
            int y = (int)(((hsh / 3 % WINDOW_HEIGHT) + (int)(now / (snow ? 12 : 2)) * (1 + z) - tilt + 100 * WINDOW_HEIGHT) % WINDOW_HEIGHT);
            if (snow) Box(hdc, x, y, x + 1 + z * 2, y + 1 + z * 2, RGB(240 - (2 - z) * 25, 240 - (2 - z) * 22, 245 - (2 - z) * 15));
            else Stroke(hdc, x, y, x + FxTrunc(across * (1 + z)), y + 7 + z * 8, z == 2 ? 2 : 1, z ? RGB(170, 185, 210) : RGB(140, 155, 180));
        }
    }
    int trees = 0;
    for (int i = 0; i < thing_count; i++) trees += things[i].kind == FP_TREE;
    if (!trees || (current_season != SEASON_FALL && abs(wind_dir) < 4) || (snow_cover) > FX(0.5)) return;
    COLORREF lc[3] = { RGB(210, 120, 40), RGB(220, 175, 50), RGB(90, 150, 55) };
    for (int i = 0; i < 26; i++) {
        int hsh = RainHash(i * 311 + 77), z = i % 3, s = 2 + z * 2;
        int fall = (int)((int64_t)now * (1 + z) / (70 + hsh % 40));
        int x = (int)(((hsh % WINDOW_WIDTH) + (int)((((int64_t)now * across) * (1 + z) / 18) >> 16) + FxTrunc(FxSin(FxRaw((int32_t)((((int64_t)fall << 16) / 20) % FxBits(FX_TWO_PI))) + FxI(i)) * 30) - pan + 100 * WINDOW_WIDTH) % WINDOW_WIDTH);
        int y = (int)(((hsh / 7 % WINDOW_HEIGHT) + fall - tilt + 100 * WINDOW_HEIGHT) % WINDOW_HEIGHT);
        int f = FxTrunc(FxSin(Tk(now, 150) + FxI(i)) * s);
        POINT p[4] = { { x - s, y }, { x, y - s / 2 + f / 2 }, { x + s, y }, { x, y + s / 2 - f / 2 } };
        Shape(hdc, p, 4, lc[current_season == SEASON_FALL ? i % 2 : 2]);
    }
}

void DrawAfkView(HDC hdc) {
    static int was_on = 0;
    int ms = AfkViewMs();
    if (ms < FP_ZOOM_MS) { was_on = 0; return; }
    if (ms < FP_ZOOM_MS + FP_DIP_MS) { FillWindowAlpha(hdc, RGB(0, 0, 0), (ms - FP_ZOOM_MS) * 255 / FP_DIP_MS); was_on = 0; return; }
    eye_x = (player_x); eye_y = (player_y); eye_z = FP_EYE + HillFx(player_x, player_y) / 32; hill_view = 1;
    GatherThings();
    UpdateGaze(!was_on); was_on = 1;
    focal = FxDiv(FxI(WINDOW_WIDTH / 2), FxTan(FP_FOV / 2));
    horizon = FxTrunc(WINDOW_HEIGHT * FX(0.55) + FxMul(pitch * WINDOW_HEIGHT, FX(0.7)));
    fx light = FxAbs(FxSin((day_night_cycle_accumulator)));
    if (NightVisionActive() && light < FX(0.6)) light = FX(0.6);
    if (in_cave) light = FX(0.35);
    fx lit = FX(0.3) + FxMul(FX(0.7), light);
    scene_lit = lit; haze = in_cave ? RGB(18, 16, 14) : Dim(RGB(185, 210, 235), FX(0.2) + FxMul(FX(0.8), light));
    { // darkness closes in at night as far as the hero's race can see
        fx nv = in_cave ? FX(1.0) : FxDiv((FX(0.55) - light), FX(0.35));
        nv = nv < FX(0.0) ? FX(0.0) : nv > FX(1.0) ? FX(1.0) : nv;
        fx see = FxMul(RaceVisionTiles(), FX(2.0));
        fog_far = FxI(FP_RANGE) + FxMul(see - FxI(FP_RANGE), nv);
        if (fog_far < FX(8.0)) fog_far = FX(8.0);
        fog_near = FxMul(FX(6.0), (FX(1.0) - nv)) + FxMul(FxMul(FX(0.35), fog_far), nv); fog_cap = FX(0.7) + FxMul(FX(0.22), nv);
    }
    DrawSky(hdc, light);
    DrawClouds(hdc, light);
    DrawGround(hdc, lit);
    face_count = 0;
    GroundMesh();
    for (int i = 0; i < thing_count; i++) {
        int sx, sy, k = things[i].kind;
        if (!Project(things[i].x, things[i].y, things[i].z, &sx, &sy, &things[i].depth)) things[i].depth = -FX(1.0);
        if (k == FP_TREE) TreeMesh(&things[i]);
        else if (k == FP_ROCK) RockMesh(&things[i]);
        else if (k == FP_HOME) HomeMesh(&things[i], light < FX(0.35));
        else if (k == FP_FIRE) FireMesh(&things[i]);
        else if (k == FP_BUTTERFLY || k == FP_CRITTER || k == FP_FAMILIAR) AnimalMesh(&things[i]);
        else if (k == FP_ENEMY || k == FP_NPC) HumanMesh(&things[i]);
        else if (k >= FP_BARREL) DetailMesh(&things[i]);
        else if (things[i].depth > FX(0.0)) AddSprite(i);
    }
    bobber_sx = -1;
    DrawFaces(hdc, lit);
    fog_near = FX(6.0); fog_far = FxI(FP_RANGE); fog_cap = FX(0.7); // rooms drawn with the same pipeline keep the daytime fog
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
#define ROOM_HX FX(2.6)
#define ROOM_HY FxI(2)
int inside_home = -1;
static fx out_x, out_y, room_cx, room_cy;

static int HomeDoorNear(void) {
    for (int i = 0; i < 5; i++) {
        fx hx, hy;
        if (!VillageHomeCentre(i, &hx, &hy)) continue;
        fx dx = player_x + HOME_FEET_OFFSET - hx, dy = player_y + HOME_FEET_OFFSET - (hy + HOME_HALF_Y);
        if (FxMul(dx, dx) + FxMul(dy, dy) < FX(0.8)) return i;
    }
    return -1;
}

void LeaveHome(void) {
    if (inside_home < 0) return;
    inside_home = -1; player_x = out_x; player_y = out_y;
    if (town_menu == TM_HOME) town_menu = TM_NONE;
    strcpy(arpg_action_log, "HOME: You step back outside.");
}

int InteriorBlocks(fx x, fx y) {
    if (x < room_cx - ROOM_HX + FX(0.25) || x > room_cx + ROOM_HX - FX(0.25) || y < room_cy - ROOM_HY + FX(0.3) || y > room_cy + ROOM_HY - FX(0.15)) return 1;
    if (x < room_cx - ROOM_HX + FX(1.25) && y < room_cy - ROOM_HY + FX(2.1)) return 1;                    // bed
    if (FxAbs(x - (room_cx + FxI(1))) < FX(0.6) && FxAbs(y - (room_cy + FX(0.2))) < FX(0.45)) return 1;       // table
    if (x > room_cx + ROOM_HX - FxI(1) && y < room_cy - ROOM_HY + FX(0.9)) return 1;                    // hearth
    if (inside_home == 4) {
        int f = player_home[5];
        if ((f & 1) && x > room_cx + ROOM_HX - FX(0.75) && FxAbs(y - (room_cy + FxI(1))) < FX(0.6)) return 1;
        if ((f & 2) && x < room_cx - ROOM_HX + FX(1.1) && FxAbs(y - (room_cy + FX(1.2))) < FX(0.45)) return 1;
        if ((f & 16) && FxAbs(x - (room_cx + FX(0.9))) < FX(0.5) && y < room_cy - ROOM_HY + FX(0.75)) return 1;
    }
    return 0;
}

int HomeInteract(void) {
    if (inside_home >= 0) { if (town_menu != TM_HOME) OpenTownMenu(TM_HOME); return 1; }
    int i = HomeDoorNear();
    if (i < 0 || (i == 4 && player_home[0] != 2)) return 0;
    StoreEnemy();
    for (int k = 0; k < MAX_ENEMIES; k++) {
        fx dx = enemies[k].x - player_x, dy = enemies[k].y - player_y;
        if (enemies[k].hearts > FxI(0) && enemies[k].angry && FxMul(dx, dx) + FxMul(dy, dy) < FxI(36)) { strcpy(arpg_action_log, "HOME: Not with monsters on your heels!"); return 1; }
    }
    fx hx, hy; VillageHomeCentre(i, &hx, &hy);
    inside_home = i; out_x = player_x; out_y = player_y; room_cx = hx; room_cy = hy;
    player_x = room_cx - FX(0.3); player_y = room_cy + FX(0.9);
    OpenTownMenu(TM_HOME);
    if (i == 4) { if (!PlaceFurniture()) strcpy(arpg_action_log, "HOME: Welcome home. Furniture you buy goes in when you come inside."); }
    return 1;
}

static void FloorRect(fx x0, fx y0, fx x1, fx y1, fx z, COLORREF col, int ground) {
    fx v[4][3] = { { x0, y0, z }, { x1, y0, z }, { x1, y1, z }, { x0, y1, z } };
    AddFace(v, 4, col, NULL, ground);
}
// Walls face along +x / +y when the offset puts the centre on the -x / -y side.
static void WallX(fx x, fx y0, fx y1, fx z0, fx z1, COLORREF col, fx off) {
    fx c[3] = { x + off, (y0 + y1) / 2, (z0 + z1) / 2 }, v[4][3] = { { x, y0, z0 }, { x, y1, z0 }, { x, y1, z1 }, { x, y0, z1 } };
    AddFace(v, 4, col, c, 0);
}
static void WallY(fx y, fx x0, fx x1, fx z0, fx z1, COLORREF col, fx off) {
    fx c[3] = { (x0 + x1) / 2, y + off, (z0 + z1) / 2 }, v[4][3] = { { x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 } };
    AddFace(v, 4, col, c, 0);
}

static void RoomCamera(fx ex, fx ey, fx ez, fx fov, int hz) {
    eye_x = ex; eye_y = ey; eye_z = ez; yaw = -FX(1.5707963); hill_view = 0;
    focal = FxDiv(FxI(WINDOW_WIDTH / 2), FxTan(fov / 2)); horizon = hz;
    face_count = 0; face_bias = FX(0.0); face_glow = 0; hcos = FX(1.0); hsin = FX(0.0);
}

static void RoomShell(fx x0, fx y0, fx x1, fx y1, fx wh, COLORREF fa, COLORREF fb, COLORREF wall, int stone) {
    if (stone) {
        int nx = FxTrunc(x1 - x0), ny = FxTrunc(y1 - y0);
        for (int j = 0; j < ny; j++) for (int i = 0; i < nx; i++) FloorRect(x0 + FxI(i), y0 + FxI(j), x0 + FxI(i + 1), y0 + FxI(j + 1), 0, (i + j) % 2 ? fa : fb, 1);
    } else {
        int n = FxTrunc(FxDiv((x1 - x0), FX(0.5)));
        for (int i = 0; i < n; i++) FloorRect(x0 + i * FX(0.5), y0, x0 + i * FX(0.5) + FX(0.5), y1, 0, i % 2 ? fa : fb, 1);
    }
    WallY(y0, x0, x1, 0, wh, wall, -FX(1.0));
    WallX(x0, y0, y1, 0, wh, Dim(wall, FX(0.82)), -FX(1.0));
    WallX(x1, y0, y1, 0, wh, Dim(wall, FX(0.82)), FX(1.0));
    face_bias = FX(0.3); WallY(y0 + FX(0.02), x0, x1, 0, FX(0.12), Dim(wall, FX(0.6)), -FX(1.0)); face_bias = FX(0.0);
}

static void RoomWindow(fx y0, fx wx0, fx wx1, fx z0, fx z1, int night) {
    COLORREF frame = RGB(95, 65, 40);
    fx mx = (wx0 + wx1) / 2, mz = (z0 + z1) / 2;
    face_bias = FX(0.3); WallY(y0 + FX(0.01), wx0, wx1, z0, z1, frame, -FX(1.0));
    face_bias = FX(0.4); face_glow = 1;
    WallY(y0 + FX(0.02), wx0 + FX(0.07), wx1 - FX(0.07), z0 + FX(0.07), z1 - FX(0.07), night ? RGB(20, 26, 62) : RGB(160, 205, 240), -FX(1.0));
    face_glow = 0; face_bias = FX(0.5);
    WallY(y0 + FX(0.03), mx - FX(0.03), mx + FX(0.03), z0 + FX(0.07), z1 - FX(0.07), frame, -FX(1.0));
    WallY(y0 + FX(0.03), wx0 + FX(0.07), wx1 - FX(0.07), mz - FX(0.03), mz + FX(0.03), frame, -FX(1.0));
    face_bias = FX(0.0);
}

// Decorations set out at home, on the table, sill, mantle and floor spots in turn.
static void DrawHomeDecor(fx x0, fx x1, fx y0, fx y1, fx cx, fx cy) {
    fx sp[8][3] = { { cx + FX(0.75), cy + FX(0.05), FX(0.62) }, { cx + FX(1.25), cy + FX(0.35), FX(0.62) }, { cx - FX(0.2), y0 + FX(0.12), FX(0.85) }, { x1 - FX(0.5), y0 + FX(0.45), FX(1.1) },
                       { x0 + FX(0.45), y1 - FX(0.45), FX(0.0) }, { x0 + FX(1.35), y0 + FX(0.4), FX(0.0) }, { cx - FX(1.0), y0 + FX(0.35), FX(0.0) }, { cx + FX(0.3), y1 - FX(0.5), FX(0.0) } };
    static const COLORREF petal[3] = { RGB(240, 90, 120), RGB(250, 215, 70), RGB(170, 120, 240) }, cover[3] = { RGB(150, 40, 40), RGB(40, 70, 130), RGB(60, 110, 60) };
    DWORD now = GetTickCount(); int slot = 0, lights = 0;
    hcos = FX(1.0); hsin = FX(0.0);
    for (int k = 0; k < DecorCount(); k++) {
        if (!HomeHasDecor(k)) continue;
        fx x = sp[slot % 8][0] + (slot / 8) * FX(0.18), y = sp[slot % 8][1], z = sp[slot % 8][2];
        const char *nm = DecorName(k);
        slot++;
        if (strcmp(nm, "Flower Vase") == 0) {
            Prism(x, y, z, z + FX(0.18), FX(0.05), FX(0.035), 6, FX(0.0), RGB(120, 160, 210));
            for (int j = 0; j < 3; j++) {
                fx fxp = x + (j - 1) * FX(0.03), top = z + FX(0.28) + j * FX(0.02);
                Prism(fxp, y, z + FX(0.18), top, FX(0.006), FX(0.006), 3, FX(0.0), RGB(60, 130, 50));
                Prism(fxp, y, top, top + FX(0.03), FX(0.025), FX(0.01), 5, FX(0.0), petal[j]);
            }
        } else if (strcmp(nm, "Pothos Ivy") == 0) {
            Prism(x, y, z, z + FX(0.12), FX(0.07), FX(0.06), 6, FX(0.0), RGB(180, 90, 55));
            for (int j = 0; j < 6; j++) {
                fx a = j * FX(1.05), sw = FxMul(FX(0.01), FxSin(Tk(now, 800) + FxI(j))), lz = z > FX(0.3) ? z + FX(0.1) - j * FX(0.07) : z + FX(0.12) + j * FX(0.04);
                OBox(x + FxMul(FxCos(a), FX(0.08)) + sw, y + FxMul(FxSin(a), FX(0.08)), 0, 0, lz, lz + FX(0.04), FX(0.03), FX(0.03), RGB(70, 150, 60));
            }
        } else if (strcmp(nm, "Stack of Books") == 0) {
            for (int j = 0; j < 3; j++) OBox(x, y, 0, 0, z + j * FX(0.04), z + FX(0.04) + j * FX(0.04), FX(0.1) - j * FX(0.01), FX(0.07), cover[j]);
        } else if (strcmp(nm, "Snow Globe") == 0) {
            Prism(x, y, z, z + FX(0.04), FX(0.06), FX(0.06), 6, FX(0.0), RGB(110, 70, 40));
            Prism(x, y, z + FX(0.04), z + FX(0.17), FX(0.065), FX(0.02), 8, FX(0.0), RGB(190, 215, 240));
            face_bias = FX(0.02);
            for (int j = 0; j < 4; j++) {
                fx fz = z + FX(0.06) + FxMul(FxMod(Tk(now, 2500) + j * FX(0.3), FX(1.0)), FX(0.09));
                OBox(x + FxMul(FX(0.03), FxSin(Tk(now, 600) + j * FX(1.7))), y + FX(0.02), 0, 0, fz, fz + FX(0.01), FX(0.006), FX(0.006), RGB(255, 255, 255));
            }
            face_bias = FX(0.0);
        } else if (strcmp(nm, "Jack-o-Lantern") == 0) {
            Prism(x, y, z, z + FX(0.15), FX(0.09), FX(0.07), 8, FX(0.0), RGB(235, 120, 30));
            Prism(x, y, z + FX(0.15), z + FX(0.19), FX(0.015), FX(0.01), 4, FX(0.0), RGB(70, 110, 40));
            face_glow = 1; face_bias = FX(0.02);
            OBox(x - FX(0.03), y + FX(0.085), 0, 0, z + FX(0.08), z + FX(0.1), FX(0.012), FX(0.004), RGB(255, 220, 90));
            OBox(x + FX(0.03), y + FX(0.085), 0, 0, z + FX(0.08), z + FX(0.1), FX(0.012), FX(0.004), RGB(255, 220, 90));
            OBox(x, y + FX(0.085), 0, 0, z + FX(0.04), z + FX(0.055), FX(0.04), FX(0.004), RGB(255, 220, 90));
            face_bias = FX(0.0); face_glow = 0;
        } else if (strcmp(nm, "Candle") == 0) {
            Prism(x, y, z, z + FX(0.12), FX(0.025), FX(0.025), 6, FX(0.0), RGB(240, 230, 200));
            face_glow = 1; Prism(x, y, z + FX(0.12), z + FX(0.17) + FxMul(FX(0.01), FxSin(Tk(now, 90) + FxI(k))), FX(0.015), FX(0.0), 4, FX(0.0), RGB(255, 210, 90)); face_glow = 0;
        } else { // a decoration from the Update folder: a little keepsake, glowing if it gives light
            int h = abs(RainHash(k * 97 + 5));
            Prism(x, y, z, z + FX(0.14), FX(0.06), FX(0.05), 6, FX(0.0), RGB(90 + h % 140, 90 + (h >> 8) % 140, 90 + (h >> 16) % 140));
            if (DecorLight(k)) { face_glow = 1; Prism(x, y, z + FX(0.14), z + FX(0.2) + FxMul(FX(0.01), FxSin(Tk(now, 90) + FxI(k))), FX(0.02), FX(0.0), 4, FX(0.0), RGB(255, 220, 120)); face_glow = 0; }
        }
        lights += DecorLight(k);
    }
    if (lights) { scene_lit += FX(0.06) * lights; if (scene_lit > FX(1.0)) scene_lit = FX(1.0); } // candles warm the whole room
}

static void DrawHomeFurniture(fx x0, fx x1, fx y0, fx y1, fx cx, fx cy) {
    int f = player_home[5];
    static const COLORREF books[3] = { RGB(150, 40, 40), RGB(40, 70, 130), RGB(60, 110, 60) };
    if (f & 1) { // bookshelf on the right wall
        OBox(x1 - FX(0.25), cy + FX(1.0), 0, 0, FX(0.0), FX(1.6), FX(0.18), FX(0.5), RGB(100, 65, 38));
        face_bias = FX(0.2);
        for (int k = 0; k < 3; k++) for (int b = 0; b < 4; b++)
            OBox(x1 - FX(0.45), cy + FX(0.68) + b * FX(0.21), 0, 0, FX(0.3) + k * FX(0.5), FX(0.64) + k * FX(0.5), FX(0.03), FX(0.08), books[(b + k) % 3]);
        face_bias = FX(0.0);
    }
    if (f & 2) { // armchair by the door
        COLORREF c = RGB(120, 50, 60);
        OBox(x0 + FX(0.6), cy + FX(1.2), 0, 0, FX(0.0), FX(0.35), FX(0.3), FX(0.3), c);
        OBox(x0 + FX(0.33), cy + FX(1.2), 0, 0, FX(0.35), FX(0.85), FX(0.07), FX(0.3), Dim(c, FX(0.8)));
        OBox(x0 + FX(0.6), cy + FX(0.93), 0, 0, FX(0.35), FX(0.5), FX(0.3), FX(0.05), Dim(c, FX(0.9)));
        OBox(x0 + FX(0.6), cy + FX(1.47), 0, 0, FX(0.35), FX(0.5), FX(0.3), FX(0.05), Dim(c, FX(0.9)));
    }
    if (f & 4) { // potted plant in the front-right corner
        Prism(x1 - FX(0.4), y1 - FX(0.4), FX(0.0), FX(0.3), FX(0.14), FX(0.11), 6, FX(0.0), RGB(180, 90, 55));
        Prism(x1 - FX(0.4), y1 - FX(0.4), FX(0.3), FX(0.8), FX(0.26), FX(0.0), 5, FX(0.4), RGB(60, 130, 55));
    }
    if (f & 8) { // painting on the back wall
        face_bias = FX(0.3); WallY(y0 + FX(0.01), cx - FX(1.4), cx - FX(0.8), FX(1.0), FX(1.5), RGB(150, 110, 40), -FX(1.0));
        face_bias = FX(0.4); WallY(y0 + FX(0.02), cx - FX(1.35), cx - FX(0.85), FX(1.25), FX(1.45), RGB(120, 170, 220), -FX(1.0));
        WallY(y0 + FX(0.02), cx - FX(1.35), cx - FX(0.85), FX(1.05), FX(1.25), RGB(80, 140, 70), -FX(1.0));
        face_bias = FX(0.0);
    }
    if (f & 16) { // wardrobe against the back wall
        OBox(cx + FX(0.9), y0 + FX(0.3), 0, 0, FX(0.0), FX(1.5), FX(0.4), FX(0.25), RGB(90, 58, 34));
        face_bias = FX(0.2); WallY(y0 + FX(0.56), cx + FX(0.88), cx + FX(0.92), FX(0.1), FX(1.4), RGB(60, 38, 22), -FX(1.0)); face_bias = FX(0.0);
    }
    if (f & 32) { // hanging lantern
        OBox(cx - FX(0.3), cy + FX(0.6), 0, 0, FX(1.75), FX(2.0), FX(0.01), FX(0.01), RGB(50, 50, 50));
        face_glow = 1; Prism(cx - FX(0.3), cy + FX(0.6), FX(1.5), FX(1.75), FX(0.08), FX(0.08), 6, FX(0.0), RGB(255, 210, 110)); face_glow = 0;
        scene_lit = FX(1.0);
    }
    DrawHomeDecor(x0, x1, y0, y1, cx, cy);
}

void DrawHomeInterior(HDC hdc) {
    if (inside_home < 0) return;
    if (town_menu != TM_HOME) { LeaveHome(); return; }
    fx x0 = (room_cx - ROOM_HX), x1 = (room_cx + ROOM_HX), y0 = (room_cy - ROOM_HY), y1 = (room_cy + ROOM_HY), cx = (room_cx), cy = (room_cy);
    int night = (FxAbs(FxSin((day_night_cycle_accumulator))) < FX(0.35)); DWORD now = GetTickCount();
    COLORREF wood = RGB(120, 80, 45), dark = RGB(85, 55, 32), stone = RGB(130, 125, 120), wall = RGB(222, 200, 158);
    if (inside_home == 4) wall = HomeWallColor(player_home[4]);
    static const COLORREF blankets[4] = { RGB(170, 50, 45), RGB(60, 100, 160), RGB(70, 130, 70), RGB(150, 110, 50) };
    Box(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, RGB(26, 18, 12));
    RoomCamera(cx, y1 + FX(3.0), FX(2.6), FX(1.55), FxTrunc(WINDOW_HEIGHT * FX(0.16)));
    scene_lit = night ? FX(0.75) : FX(0.95); haze = RGB(26, 18, 12);
    RoomShell(x0, y0, x1, y1, FX(2.0), RGB(150, 105, 62), RGB(136, 94, 55), wall, 0);
    RoomWindow(y0, cx - FX(0.6), cx + FX(0.2), FX(0.85), FX(1.55), night);
    face_bias = -FX(0.6); OBox(cx - FX(0.3), cy + FX(0.6), 0, 0, FX(0.0), FX(0.015), FX(0.8), FX(0.55), RGB(150, 60, 60)); face_bias = FX(0.0); // rug
    // bed in the back-left corner
    OBox(x0 + FX(0.6), y0 + FX(0.08), 0, 0, FX(0.0), FX(0.75), FX(0.5), FX(0.06), dark);
    OBox(x0 + FX(0.6), y0 + FX(1.0), 0, 0, FX(0.0), FX(0.3), FX(0.5), FX(0.95), wood);
    OBox(x0 + FX(0.6), y0 + FX(1.15), 0, 0, FX(0.3), FX(0.4), FX(0.48), FX(0.75), blankets[inside_home & 3]);
    face_bias = FX(0.3); OBox(x0 + FX(0.6), y0 + FX(0.28), 0, 0, FX(0.3), FX(0.42), FX(0.38), FX(0.15), RGB(235, 235, 225)); face_bias = FX(0.0);
    // table, stools and a bowl of apples
    for (int k = 0; k < 4; k++) OBox(cx + FX(1.0) + ((k & 1) ? FX(0.45) : -FX(0.45)), cy + FX(0.2) + ((k & 2) ? FX(0.28) : -FX(0.28)), 0, 0, FX(0.0), FX(0.55), FX(0.04), FX(0.04), dark);
    OBox(cx + FX(1.0), cy + FX(0.2), 0, 0, FX(0.55), FX(0.62), FX(0.55), FX(0.38), wood);
    Prism(cx + FX(1.0), cy + FX(0.2), FX(0.62), FX(0.7), FX(0.1), FX(0.13), 6, FX(0.0), RGB(200, 60, 40));
    Prism(cx + FX(0.25), cy + FX(0.25), FX(0.0), FX(0.38), FX(0.15), FX(0.15), 6, FX(0.0), wood);
    Prism(cx + FX(1.0), cy + FX(0.9), FX(0.0), FX(0.38), FX(0.15), FX(0.15), 6, FX(0.0), wood);
    // shelf with jars on the left wall
    OBox(x0 + FX(0.12), cy + FX(0.6), 0, 0, FX(1.0), FX(1.05), FX(0.1), FX(0.5), wood);
    for (int k = 0; k < 3; k++) Prism(x0 + FX(0.12), cy + FX(0.3) + k * FX(0.3), FX(1.05), FX(1.2), FX(0.05), FX(0.05), 5, FX(0.0), k == 1 ? RGB(90, 120, 160) : RGB(190, 160, 110));
    // hearth in the back-right corner with a flickering fire
    OBox(x1 - FX(0.5), y0 + FX(0.4), 0, 0, FX(0.0), FX(1.1), FX(0.45), FX(0.35), stone);
    OBox(x1 - FX(0.5), y0 + FX(0.25), 0, 0, FX(1.1), FX(2.0), FX(0.3), FX(0.2), Dim(stone, FX(0.8)));
    face_bias = FX(0.2); OBox(x1 - FX(0.5), y0 + FX(0.76), 0, 0, FX(0.05), FX(0.55), FX(0.28), FX(0.02), RGB(25, 18, 14));
    face_glow = 1; face_bias = FX(0.4);
    Prism(x1 - FX(0.5), y0 + FX(0.8), FX(0.05), FX(0.38) + FxMul(FX(0.08), FxSin(Tk(now, 90))), FX(0.16), FX(0.0), 5, Tk(now, 300), RGB(240, 140, 40));
    face_bias = FX(0.5);
    Prism(x1 - FX(0.5), y0 + FX(0.82), FX(0.05), FX(0.25) + FxMul(FX(0.05), FxSin(Tk(now, 70))), FX(0.08), FX(0.0), 5, -(Tk(now, 250)), RGB(255, 225, 90));
    face_glow = 0; face_bias = FX(0.0);
    // low front wall either side of the door
    WallY(y1, x0, cx - FX(0.55), FX(0.0), FX(0.3), Dim(wall, FX(0.9)), -FX(1.0));
    WallY(y1, cx + FX(0.55), x1, FX(0.0), FX(0.3), Dim(wall, FX(0.9)), -FX(1.0));
    // the villager who lives here, and the hero
    if (inside_home < 4) { FpThing r = { x0 + FX(1.7), y0 + FX(1.6), FX(0.0), FX(0.0), FP_NPC, (13 << 4) | (inside_home & 3) }; HumanMesh(&r); }
    else DrawHomeFurniture(x0, x1, y0, y1, cx, cy);
    FpThing h = { (player_x), (player_y), FX(0.0), FX(0.0), FP_NPC, (14 << 4) | 15 };
    HumanMesh(&h);
    DrawFaces(hdc, scene_lit);
}

// Shop and castle rooms: the same polygon room, framed so the floor meets the
// back wall at floor_y, where each shop's own props stand.
void DrawShopRoom(HDC hdc, int stone, int shrine, int floor_y) {
    fx x0 = -FX(5.5), x1 = FX(5.5), y0 = -FX(3.0), y1 = FX(3.0), ez = FX(2.3), ey = y1 + FX(2.0), fov = FX(1.6);
    fx f = FxDiv(FxI(WINDOW_WIDTH / 2), FxTan(fov / 2));
    int hz = floor_y - FxTrunc(FxMul(FxDiv(ez, (ey - y0)), f));
    COLORREF wall = shrine ? RGB(48, 36, 62) : stone ? RGB(112, 112, 120) : RGB(132, 90, 54);
    COLORREF fa = shrine ? RGB(40, 32, 46) : stone ? RGB(92, 88, 84) : RGB(104, 70, 42);
    Box(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Dim(wall, FX(0.45)));
    RoomCamera(FX(0.0), ey, ez, fov, hz);
    scene_lit = FX(1.0); haze = Dim(wall, FX(0.5));
    RoomShell(x0, y0, x1, y1, FX(3.6), fa, Dim(fa, FX(0.85)), wall, stone || shrine);
    if (!shrine) RoomWindow(y0, -FX(1.0), FX(0.6), FX(0.9), FX(1.9), (FxAbs(FxSin((day_night_cycle_accumulator))) < FX(0.35)));
    for (int k = 0; k < 4; k++) OBox(x0 + FX(1.4) + k * FX(2.75), FX(0.0), 0, 0, FX(3.4), FX(3.6), FX(0.12), FX(3.0), Dim(wall, FX(0.55)));
    DrawFaces(hdc, FX(1.0));
}
