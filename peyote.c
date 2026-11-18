#include "game.h"
#include "procmath.h"

// Peyote: some desert cacti are small peyote buttons. Two arrows split one
// open (melee and X do nothing to it), X then gathers it, and eating it melts
// the screen's colours for 5 game hours.
#define PEYOTE_MELT_MS 300000 // 5 game hours (1 real second = 1 game minute)
static uint8_t peyote_hits[MAP_SIZE][MAP_SIZE];
static int peyote_screen = -1;
static DWORD peyote_until;
#define PEYOTE_WILD_MS 20000 // three buttons in one melt: 20 seconds of full psychedelia, then you wake somewhere else
static int peyote_doses;
static DWORD peyote_wild_until;

static void SyncScreen(void) {
    if (peyote_screen != current_screen_index) { memset(peyote_hits, 0, sizeof(peyote_hits)); peyote_screen = current_screen_index; }
}

int IsPeyoteSeed(int seed) { return current_biome == BIOME_DESERT && RainHash(seed * 37 + 11) % 5 == 0; }

int PeyoteAt(int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_SIZE || ty >= MAP_SIZE || VISUAL_MAP[ty][tx] != 7) return 0;
    return IsPeyoteSeed(ty * MAP_SIZE + tx);
}

int PeyoteBroken(int seed) { SyncScreen(); return peyote_hits[seed / MAP_SIZE][seed % MAP_SIZE] >= 2; }

void HitPeyote(int tx, int ty) {
    SyncScreen();
    if (peyote_hits[ty][tx] >= 2) return;
    if (++peyote_hits[ty][tx] >= 2) strcpy(arpg_action_log, "PEYOTE: The second arrow splits it open. X to gather it.");
    else strcpy(arpg_action_log, "PEYOTE: An arrow sticks in the peyote. One more should do it.");
}

int TryGatherPeyote(void) {
    SyncScreen();
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int tx = FxTrunc(player_x) + dx, ty = FxTrunc(player_y) + dy;
        if (!PeyoteAt(tx, ty) || peyote_hits[ty][tx] < 2) continue;
        VISUAL_MAP[ty][tx] = 0; peyote_hits[ty][tx] = 0;
        AddStackableItem("Peyote", "Food", 1);
        RecalculateCarriedWeight();
        strcpy(arpg_action_log, "PEYOTE: Gathered a peyote button.");
        return 1;
    }
    return 0;
}

void EatPeyote(void) {
    if (!ConsumeMaterialFromInventory("Peyote", 1)) return;
    RecalculateCarriedWeight();
    DWORD now = GetTickCount();
    peyote_doses = now < peyote_until ? peyote_doses + 1 : 1;
    peyote_until = now + PEYOTE_MELT_MS;
    if (peyote_doses >= 3 && !peyote_wild_until) {
        peyote_wild_until = now + PEYOTE_WILD_MS;
        strcpy(arpg_action_log, "PEYOTE: Far too much. The world folds, swirls and pours away...");
    } else strcpy(arpg_action_log, "PEYOTE: The colours begin to melt...");
}

// Gentle melt: the finished frame is copied and laid back in thin bands that
// sway only a few pixels, under slowly drifting bands of saturated colour
// (a soft rainbow sheen rather than a desert haze). Works in the AFK view too.
static COLORREF HueColor(fx h) {
    fx r = FX(0.5) + FxSin(h) / 2, g = FX(0.5) + FxSin(h + FX(2.094)) / 2, b = FX(0.5) + FxSin(h + FX(4.189)) / 2;
    fx mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), k = mx > mn ? FxDiv(FxI(1), mx - mn) : FxI(1);
    return RGB(FxTrunc(FxMul(r - mn, k) * 255), FxTrunc(FxMul(g - mn, k) * 255), FxTrunc(FxMul(b - mn, k) * 255));
}
// Overdose: the frame is torn into rows and columns that slide on crossing sine
// waves, under a plasma colour wash (a quarter-size plasma image stretched over the
// screen - one VDP2 colour layer on the Saturn). It all ramps up over the 20 seconds.
static void DrawWildMelt(HDC hdc, DWORD now, fx t) {
    enum { PW = WINDOW_WIDTH / 4, PH = WINDOW_HEIGHT / 4 + 1 };
    fx wk = FX(0.35) + FxMul(FX(0.65), FxI(PEYOTE_WILD_MS - (int)(peyote_wild_until - now)) / PEYOTE_WILD_MS);
    static HDC copy, wash; static HBITMAP bmp; static uint32_t *px;
    if (!copy) {
        copy = CreateCompatibleDC(hdc); bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT); SelectObject(copy, bmp);
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), PW, -PH, 1, 32, BI_RGB, 0, 0, 0, 0, 0 } };
        void *bits = NULL; HBITMAP w = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (w && bits) { wash = CreateCompatibleDC(hdc); SelectObject(wash, w); px = (uint32_t *)bits; }
    }
    BitBlt(copy, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    for (int y = 0; y < WINDOW_HEIGHT; y += 3) {
        fx s = SinTurns(FxI(y) / 140 + t / 3) + SinTurns(FxI(y) / 57 - t / 2) / 2;
        BitBlt(hdc, FxTrunc(FxMul(s, wk) * 30), y, WINDOW_WIDTH, 3, copy, 0, y, SRCCOPY);
    }
    BitBlt(copy, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    for (int x = 0; x < WINDOW_WIDTH; x += 4)
        BitBlt(hdc, x, FxTrunc(FxMul(SinTurns(FxI(x) / 170 - t / 4), wk) * 18), 4, WINDOW_HEIGHT, copy, x, 0, SRCCOPY);
    if (!wash) return;
    for (int y = 0; y < PH; y++) for (int x = 0; x < PW; x++) {
        COLORREF c = HueColor(FxMul(Plasma2(FxI(x) * 3 / 4, FxI(y) * 3 / 4, t / 3), FX(4.0)) + t);
        px[y * PW + x] = ((uint32_t)GetRValue(c) << 16) | ((uint32_t)GetGValue(c) << 8) | GetBValue(c);
    }
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)FxTrunc(wk * 95), 0 };
    AlphaBlend(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, wash, 0, 0, PW, PH, bf);
}

static void WakeSomewhere(void) {
    LeaveHome(); // out of any interior first (no-op outdoors)
    if (in_cave) { in_cave = 0; cave_is_hold = 0; pending_forced_biome = -1; }
    if (fishing_active) fishing_active = 0;
    int gx = screen_grid_x, gy = screen_grid_y;
    for (int a = 0; a < 200; a++) {
        int x = 1 + rand() % (META_GRID_SIZE - 2), y = 1 + rand() % (META_GRID_SIZE - 2), b = WorldBiomeAt(x, y);
        if (b == BIOME_OCEAN || b == BIOME_LAKE || SeaNeedsBoat(x, y) || WorldIsTown(x, y) || (x == gx && y == gy)) continue;
        gx = x; gy = y; break;
    }
    screen_grid_x = gx; screen_grid_y = gy;
    screens_until_town = 1; // never a town (otherwise a castle would be built here)
    GenerateProceduralScreen(gy * META_GRID_SIZE + gx);
    player_x = FxI(MAP_SIZE) / 2; player_y = FxI(MAP_SIZE) / 2;
    for (int r = 0; r < MAP_SIZE / 2 && !PlayerCanStandAt(player_x, player_y); r++) player_x = FxI(MAP_SIZE) / 2 + FxI(r * ((r & 1) ? 1 : -1));
    strcpy(arpg_action_log, "That was a wild night you barely remember. You have a clarity of purpose.");
}

void UpdatePeyote(void) {
    if (!peyote_wild_until || (int)(GetTickCount() - peyote_wild_until) < 0) return;
    peyote_wild_until = 0; peyote_until = 0; peyote_doses = 0;
    WakeSomewhere();
}

void DrawPeyoteMelt(HDC hdc) {
    DWORD now = GetTickCount();
    if (!peyote_until || now >= peyote_until) return;
    fx fade = (peyote_until - now) < 8000 ? FxI(peyote_until - now) / 8000 : FxI(1), t = (fx)((int64_t)now * FX_ONE / 1000);
    static HDC copy; static HBITMAP bmp;
    if (!copy) { copy = CreateCompatibleDC(hdc); bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT); SelectObject(copy, bmp); }
    if (peyote_wild_until && (int)(now - peyote_wild_until) < 0) DrawWildMelt(hdc, now, t);
    BitBlt(copy, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    for (int y = 0; y < WINDOW_HEIGHT; y += 4) {
        int dx = FxTrunc(FxMul(WaveAt(y * FX(0.008), FxI(0), t / 4) * 2, fade));
        BitBlt(hdc, dx, y, WINDOW_WIDTH, 4, copy, 0, y, SRCCOPY);
    }
    const int bands = 14, bh = WINDOW_HEIGHT / bands + 1;
    for (int k = 0; k < bands; k++)
        FillRectAlpha(hdc, 0, k * bh, WINDOW_WIDTH, bh, HueColor(FxMul(t, FX(0.18)) + k * FX(0.45)), FxTrunc(18 * fade));
}

// Any weapon breaks cacti and peyote: one melee swing (or one arrow) fells a
// cactus, and a swing splits a peyote open at once (arrows still take two).
int BreakDesertPlant(int tx, int ty, int melee) {
    if (tx < 0 || ty < 0 || tx >= MAP_SIZE || ty >= MAP_SIZE || VISUAL_MAP[ty][tx] != 7) return 0;
    SyncScreen();
    if (PeyoteAt(tx, ty)) {
        if (peyote_hits[ty][tx] >= 2) return 0;
        if (melee) { peyote_hits[ty][tx] = 2; strcpy(arpg_action_log, "PEYOTE: Your blow splits it open. X to gather it."); }
        else HitPeyote(tx, ty);
        return 1;
    }
    VISUAL_MAP[ty][tx] = 0;
    SpawnTreeDebris(FxI(tx) + FX(0.5), FxI(ty) + FX(0.5));
    strcpy(arpg_action_log, "CACTUS: It topples over in a spray of green.");
    return 1;
}

int MeleeHitDesertPlant(void) {
    fx hx = facing_x, hy = facing_y, d = FxLen(hx, hy);
    if (d < FX(0.01)) return 0;
    hx = FxDiv(hx, d); hy = FxDiv(hy, d);
    for (int k = 1; k <= 3; k++) {
        fx r = FX(0.5) * k;
        if (BreakDesertPlant(FxTrunc(player_x + FxMul(hx, r)), FxTrunc(player_y + FxMul(hy, r)), 1)) return 1;
    }
    return 0;
}
