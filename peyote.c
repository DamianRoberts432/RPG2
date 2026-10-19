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

// Heat-mirage melt: the finished frame is copied, then laid back in thin
// bands, each slid sideways and up or down by the same crossing swells as the
// water (WaveAt), under a slow colour wash. Works in the AFK view too.
void DrawPeyoteMelt(HDC hdc) {
    DWORD now = GetTickCount();
    if (!peyote_until || now >= peyote_until) return;
    float fade = (peyote_until - now) < 8000 ? (peyote_until - now) / 8000.0f : 1.0f, t = now / 1000.0f;
    static HDC copy; static HBITMAP bmp;
    if (!copy) { copy = CreateCompatibleDC(hdc); bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT); SelectObject(copy, bmp); }
    BitBlt(copy, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdc, 0, 0, SRCCOPY);
    for (int y = 0; y < WINDOW_HEIGHT; y += 4) {
        int dx = (int)(WaveAt(y * 0.02f, 0.0f, t) * 14.0f * fade), dy = (int)(WaveAt(0.0f, y * 0.03f, t * 0.7f) * 3.0f * fade);
        BitBlt(hdc, dx, y, WINDOW_WIDTH, 4, copy, 0, y + dy, SRCCOPY);
    }
    float h = t * 0.4f;
    COLORREF c = RGB((int)(127 + 127 * sinf(h)), (int)(127 + 127 * sinf(h + 2.1f)), (int)(127 + 127 * sinf(h + 4.2f)));
    FillRectAlpha(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, c, (int)(30 * fade));
}
