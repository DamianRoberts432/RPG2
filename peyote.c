#include "game.h"

// Peyote: some desert cacti are small peyote buttons. Two arrows split one
// open (melee and X do nothing to it), X then gathers it, and eating it melts
// the screen's colours for 5 game hours.
#define PEYOTE_MELT_MS 300000 // 5 game hours (1 real second = 1 game minute)
static uint8_t peyote_hits[MAP_SIZE][MAP_SIZE];
static int peyote_screen = -1;
static DWORD peyote_until;

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
        int tx = (int)player_x + dx, ty = (int)player_y + dy;
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
    peyote_until = GetTickCount() + PEYOTE_MELT_MS;
    strcpy(arpg_action_log, "PEYOTE: The colours begin to melt...");
}

// Gentle melt: the finished frame is copied and laid back in thin bands that
// sway only a few pixels, under slowly drifting bands of saturated colour
// (a soft rainbow sheen rather than a desert haze). Works in the AFK view too.
static COLORREF HueColor(float h) {
    float r = 0.5f + 0.5f * sinf(h), g = 0.5f + 0.5f * sinf(h + 2.094f), b = 0.5f + 0.5f * sinf(h + 4.189f);
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), k = mx > mn ? 1.0f / (mx - mn) : 1.0f;
    return RGB((int)((r - mn) * k * 255), (int)((g - mn) * k * 255), (int)((b - mn) * k * 255));
}
void DrawPeyoteMelt(HDC hdc) {
    DWORD now = GetTickCount();
    if (!peyote_until || now >= peyote_until) return;
    float fade = (peyote_until - now) < 8000 ? (peyote_until - now) / 8000.0f : 1.0f, t = now / 1000.0f;
    static HDC copy; static HBITMAP bmp;
    if (!copy) { copy = CreateCompatibleDC(hdc); bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT); SelectObject(copy, bmp); }
    BitBlt(copy, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    for (int y = 0; y < WINDOW_HEIGHT; y += 4) {
        int dx = (int)(WaveAt(y * 0.008f, 0.0f, t * 0.35f) * 3.0f * fade);
        BitBlt(hdc, dx, y, WINDOW_WIDTH, 4, copy, 0, y, SRCCOPY);
    }
    const int bands = 14, bh = WINDOW_HEIGHT / bands + 1;
    for (int k = 0; k < bands; k++)
        FillRectAlpha(hdc, 0, k * bh, WINDOW_WIDTH, bh, HueColor(t * 0.25f + k * 0.45f), (int)(26 * fade));
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
    SpawnTreeDebris(tx + 0.5f, ty + 0.5f);
    strcpy(arpg_action_log, "CACTUS: It topples over in a spray of green.");
    return 1;
}

int MeleeHitDesertPlant(void) {
    float fx = facing_x, fy = facing_y, d = sqrtf(fx * fx + fy * fy);
    if (d < 0.01f) return 0;
    fx /= d; fy /= d;
    for (int k = 1; k <= 3; k++) {
        float r = 0.5f * k;
        if (BreakDesertPlant((int)(player_x + fx * r), (int)(player_y + fy * r), 1)) return 1;
    }
    return 0;
}
