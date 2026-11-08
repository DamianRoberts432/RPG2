// Growing villages and travelling settlers. Every village has a stage:
// 0 Camp (a market stall), 1 Hamlet (homes), 2 Farmstead (crop fields and a
// windmill), 3 Walled (a palisade and two guards). Villages grow a little each
// day and a lot when a settler party arrives. Bandits raid unwalled villages,
// and a raid nobody answers by the next day knocks the village back a stage.
// Fixed-size state only: 48 x 8 bytes of villages + 4 x 4 bytes of settlers.
#include "game.h"

#define MAX_GROW_VILLAGES 48
#define MAX_SETTLERS 4
typedef struct { uint8_t gx, gy, stage, growth, raid, arrivals, pad[2]; } VillageGrowth;
typedef struct { uint8_t gx, gy, target, active; } SettlerParty;
static VillageGrowth vg[MAX_GROW_VILLAGES];
static SettlerParty parties[MAX_SETTLERS];
static int vg_n = 0, vg_loaded = 0, here_screen = -1, here_idx = -1;
static const char *stage_names[4] = { "Camp", "Hamlet", "Farmstead", "Walled village" };
char town_banner_sub[64];

static unsigned Hash(unsigned v) { v ^= v >> 16; v *= 0x7feb352dU; v ^= v >> 15; v *= 0x846ca68bU; v ^= v >> 16; return v; }

void VillageNameAt(int gx, int gy, char *out, size_t n) {
    static const char *a[8] = { "Oak", "Ash", "Mill", "Brook", "Stone", "Fern", "Thorn", "Elder" };
    static const char *b[6] = { "ford", "by", "wick", "stead", "ton", "dale" };
    unsigned h = Hash((unsigned)(gy * META_GRID_SIZE + gx) ^ world_seed);
    snprintf(out, n, "%s%s", a[h % 8], b[(h >> 8) % 6]);
}

static int HereIndex(void) {
    if (in_cave || current_screen_index >= 20000) return -1;
    if (here_screen != current_screen_index) {
        here_screen = current_screen_index; here_idx = -1;
        for (int i = 0; i < vg_n; i++) if (vg[i].gx == screen_grid_x && vg[i].gy == screen_grid_y) here_idx = i;
    }
    return here_idx;
}
int VillageStageHere(void) { int i = HereIndex(); return i < 0 ? -1 : vg[i].stage; }

// The palisade (stage 3, gates in the middle of each side) and the windmill (stage 2+).
int VillageBlocksAt(int x, int y) {
    int st = VillageStageHere();
    if (st < 2) return 0;
    if (x == 23 && y == 8) return 1;
    if (st < 3 || abs(x - 15) <= 1 || abs(y - 15) <= 1) return 0;
    return ((y == 6 || y == 24) && x >= 5 && x <= 25) || ((x == 5 || x == 25) && y >= 6 && y <= 24);
}

static void NewParty(SettlerParty *p) {
    int gx, gy;
    p->active = 0;
    if (!vg_n || (!WorldTownCell(rand() % 12, &gx, &gy) && !WorldTownCell(0, &gx, &gy))) return;
    p->gx = (uint8_t)gx; p->gy = (uint8_t)gy; p->target = (uint8_t)(rand() % vg_n); p->active = 1;
}

void TownGrowthInit(void) {
    here_screen = -1;
    if (vg_loaded && vg_n > 0) return;
    vg_n = 0;
    for (int y = 0; y < META_GRID_SIZE; y++) for (int x = 0; x < META_GRID_SIZE; x++) {
        if (WorldSiteAt(x, y) != SITE_VILLAGE || vg_n >= MAX_GROW_VILLAGES) continue;
        VillageGrowth *v = &vg[vg_n++];
        memset(v, 0, sizeof(*v));
        v->gx = (uint8_t)x; v->gy = (uint8_t)y;
        unsigned h = Hash((unsigned)(y * META_GRID_SIZE + x) ^ world_seed);
        v->stage = (uint8_t)(h % 10 < 2 ? 0 : h % 10 < 8 ? 1 : 2);
        v->growth = (uint8_t)((h >> 8) % 60);
    }
    for (int i = 0; i < MAX_SETTLERS; i++) NewParty(&parties[i]);
}

int TownGrowthSave(FILE *f) {
    int32_t n = vg_n;
    return fwrite(&n, sizeof(n), 1, f) == 1 && fwrite(vg, sizeof(vg), 1, f) == 1 && fwrite(parties, sizeof(parties), 1, f) == 1;
}

int TownGrowthLoad(FILE *f) {
    int32_t n;
    static VillageGrowth v[MAX_GROW_VILLAGES];
    static SettlerParty p[MAX_SETTLERS];
    if (fread(&n, sizeof(n), 1, f) != 1 || n <= 0 || n > MAX_GROW_VILLAGES ||
        fread(v, sizeof(v), 1, f) != 1 || fread(p, sizeof(p), 1, f) != 1) return 0;
    memcpy(vg, v, sizeof(v)); memcpy(parties, p, sizeof(p));
    vg_n = n; vg_loaded = 1; here_screen = -1;
    for (int i = 0; i < vg_n; i++) { if (vg[i].stage > 3) vg[i].stage = 3; if (vg[i].growth > 100) vg[i].growth = 100; }
    for (int i = 0; i < MAX_SETTLERS; i++) if (parties[i].target >= vg_n) parties[i].active = 0;
    return 1;
}

static int Grow(VillageGrowth *v, int amount) {
    int g = v->growth + amount, up = 0;
    if (g >= 100 && v->stage < 3) { v->stage++; g = 0; up = 1; }
    v->growth = (uint8_t)(g > 100 ? 100 : g);
    return up;
}

static void MoveSettlers(void) {
    for (int k = 0; k < MAX_SETTLERS; k++) {
        SettlerParty *p = &parties[k];
        if (!p->active) { NewParty(p); continue; }
        VillageGrowth *v = &vg[p->target];
        if (p->gx != v->gx) p->gx += v->gx > p->gx ? 1 : -1;
        else if (p->gy != v->gy) p->gy += v->gy > p->gy ? 1 : -1;
        else {
            int up = Grow(v, 35);
            if (v->arrivals < 255) v->arrivals++;
            if (p->target == HereIndex()) {
                char nm[24]; VillageNameAt(v->gx, v->gy, nm, sizeof(nm));
                snprintf(arpg_action_log, sizeof(arpg_action_log), up ? "SETTLERS: New folk arrive and %s grows into a %s!" : "SETTLERS: A party of settlers arrives in %s and starts building.", nm, stage_names[v->stage]);
            }
            NewParty(p);
        }
    }
}

static void DailyGrowth(void) {
    int here = HereIndex(), sacked = -1;
    for (int i = 0; i < vg_n; i++) {
        VillageGrowth *v = &vg[i];
        if (i == here) { if (!v->raid) Grow(v, 6); continue; } // the hero is here: no new raid, an old one is still being fought
        if (v->raid) {
            v->raid = 0; v->growth = 0;
            if (v->stage > 0) v->stage--;
            if (sacked < 0 && (map_poi[v->gy][v->gx] & POI_VILLAGE)) sacked = i; // only news of villages you have found
            continue;
        }
        Grow(v, 6);
        if (v->stage < 3 && Hash((unsigned)(i * 977 + GameDayOfYear()) ^ world_seed) % 12 == 0) v->raid = 1;
    }
    if (sacked >= 0) {
        char nm[24]; VillageNameAt(vg[sacked].gx, vg[sacked].gy, nm, sizeof(nm));
        snprintf(arpg_action_log, sizeof(arpg_action_log), "GRAPEVINE: Bandits sacked %s while no one came to help. It's a %s again.", nm, stage_names[vg[sacked].stage]);
    }
}

static int AliveEnemies(void) {
    int n = 0;
    StoreEnemy();
    for (int i = 0; i < MAX_ENEMIES; i++) n += enemies[i].hearts > FxI(0);
    return n;
}

void UpdateTownGrowth(void) {
    static int last_hour = -1, last_day = -1;
    int h, m; GetGameClock(&h, &m);
    int day = GameDayOfYear();
    if (last_hour < 0) { last_hour = h; last_day = day; }
    if (h != last_hour) { last_hour = h; MoveSettlers(); }
    if (day != last_day) { last_day = day; DailyGrowth(); }
    int i = HereIndex();
    if (i < 0 || vg[i].raid != 2 || AliveEnemies() > 0) return;
    char nm[24]; VillageNameAt(vg[i].gx, vg[i].gy, nm, sizeof(nm));
    vg[i].raid = 0;
    int up = Grow(&vg[i], 20);
    KarmaAdjust(2, 0, 0, 5);
    AddLegend("drove the bandits from %s", nm);
    snprintf(arpg_action_log, sizeof(arpg_action_log), up ? "RAID OVER: %s is saved, and grows into a %s!" : "RAID OVER: The bandits flee %s. The folk will remember this.", nm, stage_names[vg[i].stage]);
}

static void AddSettlersHere(void) {
    static char lines[MAX_SETTLERS][72];
    if (in_cave || current_screen_index >= 20000 || current_biome == BIOME_CASTLE) return;
    for (int k = 0; k < MAX_SETTLERS; k++) {
        SettlerParty *p = &parties[k];
        if (!p->active || p->gx != screen_grid_x || p->gy != screen_grid_y || p->target == HereIndex()) continue;
        char nm[24]; VillageNameAt(vg[p->target].gx, vg[p->target].gy, nm, sizeof(nm));
        snprintf(lines[k], sizeof(lines[k]), "Settler: We're bound for %s to build a new life.", nm);
        for (int j = 0; j < 3 && village_npc_count < MAX_NPCS; j++) {
            fx x = FxI(13) + j * FX(1.5), y = FxI(16 + (j & 1));
            village_npcs[village_npc_count++] = (NPCEntity){ x, y, x, y, 0, j * 11, FACE_DOWN, lines[k], 60 };
        }
        snprintf(arpg_action_log, sizeof(arpg_action_log), "SETTLERS: A party of settlers rests here on the road to %s.", nm);
    }
}

// Called once a screen is built: the village banner, raid bandits and any settlers passing through.
void TownGrowthOnEnter(void) {
    int i = HereIndex();
    town_banner_sub[0] = 0;
    if (i >= 0) {
        VillageGrowth *v = &vg[i];
        VillageNameAt(v->gx, v->gy, town_banner, sizeof(town_banner));
        if (v->raid) {
            SpawnRaidBandits(4);
            v->raid = 2;
            snprintf(town_banner_sub, sizeof(town_banner_sub), "%s  -  RAID! Drive off the bandits", stage_names[v->stage]);
            snprintf(arpg_action_log, sizeof(arpg_action_log), "RAID: Bandits are raiding %s! Drive them off, or by tomorrow it will fall back a stage.", town_banner);
        } else if (v->stage == 3) snprintf(town_banner_sub, sizeof(town_banner_sub), "%s  -  safe behind its palisade", stage_names[v->stage]);
        else snprintf(town_banner_sub, sizeof(town_banner_sub), "%s  -  growing %d%%", stage_names[v->stage], v->growth);
        town_banner_until = GetTickCount() + 4000;
    }
    AddSettlersHere();
}

// Crop fields and a turning windmill from stage 2, a wooden palisade at stage 3.
void DrawVillageGrowth(HDC hdc) {
    int st = VillageStageHere();
    if (st < 2) return;
    HPEN no_pen = (HPEN)GetStockObject(NULL_PEN);
    HGDIOBJ old_pen = SelectObject(hdc, no_pen);
    HBRUSH soil = CreateSolidBrush(RGB(112, 82, 48)), crop = CreateSolidBrush(RGB(120, 170, 60));
    HGDIOBJ old_br = SelectObject(hdc, soil);
    static const int fxs[2] = { 6, 22 };
    for (int f = 0; f < 2; f++) for (int y = 16; y <= 17; y++) for (int x = fxs[f]; x < fxs[f] + 3; x++) {
        int sx, sy; GetIsoCoords(FxI(x), FxI(y), &sx, &sy);
        int cy = sy + TILE_HEIGHT / 2;
        POINT p[4] = { { sx, sy }, { sx + TILE_WIDTH / 2, cy }, { sx, sy + TILE_HEIGHT }, { sx - TILE_WIDTH / 2, cy } };
        SelectObject(hdc, soil); Polygon(hdc, p, 4);
        SelectObject(hdc, crop);
        for (int r = -1; r <= 1; r++) Ellipse(hdc, sx + r * 8 - 3, cy + r * 4 - 6, sx + r * 8 + 3, cy + r * 4);
    }
    SelectObject(hdc, old_br); DeleteObject(soil); DeleteObject(crop);
    int sx, sy; GetIsoCoords(FxI(23), FxI(8), &sx, &sy);
    int base = sy + TILE_HEIGHT / 2, hub_y = base - 40;
    HBRUSH stone = CreateSolidBrush(RGB(205, 195, 172)), roof = CreateSolidBrush(RGB(150, 70, 50)), sail = CreateSolidBrush(RGB(235, 228, 205));
    POINT tower[4] = { { sx - 10, base }, { sx + 10, base }, { sx + 6, hub_y }, { sx - 6, hub_y } };
    POINT cap[3] = { { sx - 8, hub_y }, { sx + 8, hub_y }, { sx, hub_y - 10 } };
    SelectObject(hdc, stone); Polygon(hdc, tower, 4);
    SelectObject(hdc, roof); Polygon(hdc, cap, 3);
    SelectObject(hdc, sail);
    fx a0 = FxMul(world_tick, FX(1.2));
    for (int b = 0; b < 4; b++) {
        fx a = a0 + b * FX(1.5708), ca = FxCos(a), sa = FxSin(a);
        POINT bl[4] = { { sx, hub_y }, { sx + FxTrunc(ca * 26 - sa * 4), hub_y + FxTrunc(sa * 26 + ca * 4) },
                        { sx + FxTrunc(ca * 26 + sa * 4), hub_y + FxTrunc(sa * 26 - ca * 4) }, { sx, hub_y } };
        Polygon(hdc, bl, 4);
    }
    SelectObject(hdc, old_br); DeleteObject(stone); DeleteObject(roof); DeleteObject(sail);
    if (st >= 3) {
        HBRUSH wood = CreateSolidBrush(RGB(120, 84, 50));
        SelectObject(hdc, wood);
        for (int y = 6; y <= 24; y++) for (int x = 5; x <= 25; x++) {
            if (!VillageBlocksAt(x, y) || (x == 23 && y == 8)) continue;
            GetIsoCoords(FxI(x), FxI(y), &sx, &sy);
            int by = sy + TILE_HEIGHT / 2;
            POINT post[5] = { { sx - 4, by }, { sx + 4, by }, { sx + 4, by - 20 }, { sx, by - 25 }, { sx - 4, by - 20 } };
            Polygon(hdc, post, 5);
        }
        SelectObject(hdc, old_br); DeleteObject(wood);
    }
    SelectObject(hdc, old_pen);
}
