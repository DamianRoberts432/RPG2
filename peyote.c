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

void DrawPeyoteMelt(HDC hdc) {
    DWORD now = GetTickCount();
    if (!peyote_until || now >= peyote_until) return;
    float fade = (peyote_until - now) < 4000 ? (peyote_until - now) / 4000.0f : 1.0f;
    for (int band = 0; band < 12; band++) {
        float h = now / 900.0f + band * 0.5f;
        COLORREF c = RGB((int)(127 + 127 * sinf(h)), (int)(127 + 127 * sinf(h + 2.1f)), (int)(127 + 127 * sinf(h + 4.2f)));
        int y = band * WINDOW_HEIGHT / 12 + (int)(sinf(now / 400.0f + band) * 6.0f);
        FillRectAlpha(hdc, 0, y, WINDOW_WIDTH, WINDOW_HEIGHT / 12 + 6, c, (int)(80 * fade));
    }
}
