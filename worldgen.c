#include "game.h"
#include <math.h>
#include <string.h>

#define WG META_GRID_SIZE
#define MAX_TOWNS 12
uint32_t world_seed = 0;
int elysium_gx = WG / 2, elysium_gy = WG / 2, elysium_theme = 0, village_dwarves = 0;
static uint8_t world_biome[WG][WG], world_site[WG][WG], world_road[WG][WG];
static int world_ready = 0, town_x[MAX_TOWNS], town_y[MAX_TOWNS], town_n = 0;

static int In(int x, int y) { return x >= 0 && y >= 0 && x < WG && y < WG; }
static int B(int x, int y) { return In(x, y) ? world_biome[y][x] : -1; }
static int Wet(int b) { return b == BIOME_OCEAN || b == BIOME_LAKE || b == BIOME_RIVER; }
static int Land(int x, int y) {
    int b = B(x, y);
    return b >= 0 && !Wet(b) && b != BIOME_BEACH && x > 0 && y > 0 && x < WG - 1 && y < WG - 1;
}
static int NearBiome(int x, int y, int b) {
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && B(x + dx, y + dy) == b) return 1;
    return 0;
}
static int FarFromSites(int x, int y, int d) {
    for (int yy = y - d; yy <= y + d; yy++) for (int xx = x - d; xx <= x + d; xx++) if (In(xx, yy) && world_site[yy][xx]) return 0;
    return 1;
}
// Elysium themes: 0 central, 1 port by the ocean, 2 lakeside, 3 forest, 4 desert.
static int ThemeFits(int x, int y, int t) {
    if (!Land(x, y)) return 0;
    switch (t) {
    case 0: return B(x, y) == BIOME_PRAIRIE || B(x, y) == BIOME_FOREST;
    case 1: return NearBiome(x, y, BIOME_BEACH);
    case 2: return NearBiome(x, y, BIOME_LAKE);
    case 3: return B(x, y) == BIOME_FOREST;
    case 4: return B(x, y) == BIOME_DESERT;
    }
    return 0;
}
const char *ElysiumThemeName(void) {
    static const char *n[5] = { "in the heartland", "by the ocean port", "on the lakeshore", "in the forest", "in the desert" };
    return n[elysium_theme % 5];
}
static int PlaceElysium(int theme) {
    int tx = theme == 0 ? WG / 2 : 4 + rand() % (WG - 8), ty = theme == 0 ? WG / 2 : 4 + rand() % (WG - 8), best = -1;
    for (int y = 0; y < WG; y++) for (int x = 0; x < WG; x++) {
        if (!ThemeFits(x, y, theme)) continue;
        int d = abs(x - tx) + abs(y - ty);
        if (best < 0 || d < best) { best = d; elysium_gx = x; elysium_gy = y; }
    }
    return best >= 0;
}
static void RoadStep(int x, int y, int nx, int ny, int bit, int back) { world_road[y][x] |= (uint8_t)bit; world_road[ny][nx] |= (uint8_t)back; }
static void Highway(int a, int b) {
    int x = town_x[a], y = town_y[a];
    while (x != town_x[b]) { int nx = x + (town_x[b] > x ? 1 : -1); RoadStep(x, y, nx, y, nx > x ? 2 : 8, nx > x ? 8 : 2); x = nx; }
    while (y != town_y[b]) { int ny = y + (town_y[b] > y ? 1 : -1); RoadStep(x, y, x, ny, ny > y ? 4 : 1, ny > y ? 1 : 4); y = ny; }
}

void GenerateWholeWorld(uint32_t seed) {
    static uint8_t keep[WG][WG];
    memcpy(keep, visited_biomes, sizeof(keep));
    memset(visited_biomes, 0, sizeof(keep));
    srand(seed);
    // Roll every cell in reading order with the same neighbour rules the
    // explorer used, then put back what the hero has already seen.
    for (int y = 0; y < WG; y++) for (int x = 0; x < WG; x++) visited_biomes[y][x] = (uint8_t)(PickBiomeForScreen(x, y) + 1);
    for (int y = 0; y < WG; y++) for (int x = 0; x < WG; x++) world_biome[y][x] = (uint8_t)(keep[y][x] ? keep[y][x] - 1 : visited_biomes[y][x] - 1);
    memcpy(visited_biomes, keep, sizeof(keep));
    memset(world_site, 0, sizeof(world_site));
    memset(world_road, 0, sizeof(world_road));
    elysium_theme = (int)(seed % 5u);
    if (!PlaceElysium(elysium_theme)) { elysium_theme = 0; if (!PlaceElysium(0)) { elysium_gx = WG / 2; elysium_gy = WG / 2; } }
    town_n = 0;
    town_x[town_n] = elysium_gx; town_y[town_n++] = elysium_gy;
    world_site[elysium_gy][elysium_gx] = SITE_ELYSIUM;
    for (int a = 0; a < 6000 && town_n < MAX_TOWNS; a++) {
        int x = 1 + rand() % (WG - 2), y = 1 + rand() % (WG - 2);
        if (!Land(x, y) || B(x, y) == BIOME_MOUNTAIN || !FarFromSites(x, y, 8)) continue;
        town_x[town_n] = x; town_y[town_n++] = y; world_site[y][x] = SITE_TOWN;
    }
    for (int a = 0, n = 0; a < 6000 && n < 45; a++) {
        int x = 1 + rand() % (WG - 2), y = 1 + rand() % (WG - 2);
        if ((B(x, y) != BIOME_PRAIRIE && B(x, y) != BIOME_FOREST) || !FarFromSites(x, y, 3)) continue;
        world_site[y][x] = SITE_VILLAGE; n++;
    }
    // Villages dug into the mountains are always dwarf holds.
    for (int a = 0, n = 0; a < 6000 && n < 6; a++) {
        int x = 1 + rand() % (WG - 2), y = 1 + rand() % (WG - 2);
        if (B(x, y) != BIOME_MOUNTAIN || !FarFromSites(x, y, 4)) continue;
        world_site[y][x] = SITE_DWARF; n++;
    }
    // Highways: the shortest network joining every town (Prim's tree).
    int linked[MAX_TOWNS] = { 1 };
    for (int k = 1; k < town_n; k++) {
        int bi = 0, bj = -1, bd = 1 << 30;
        for (int i = 0; i < town_n; i++) if (linked[i]) for (int j = 0; j < town_n; j++) if (!linked[j]) {
            int d = abs(town_x[i] - town_x[j]) + abs(town_y[i] - town_y[j]);
            if (d < bd) { bd = d; bi = i; bj = j; }
        }
        if (bj < 0) break;
        Highway(bi, bj); linked[bj] = 1;
    }
    world_ready = 1;
    srand((unsigned)GetTickCount());
}

int WorldBiomeAt(int gx, int gy) { return world_ready && In(gx, gy) ? world_biome[gy][gx] : (int)PickBiomeForScreen(gx, gy); }
int WorldSiteAt(int gx, int gy) { return world_ready && In(gx, gy) ? world_site[gy][gx] : SITE_NONE; }
int WorldIsTown(int gx, int gy) { int s = WorldSiteAt(gx, gy); return s == SITE_TOWN || s == SITE_ELYSIUM; }
int WorldRoadBits(int gx, int gy) { return world_ready && In(gx, gy) ? world_road[gy][gx] : 0; }

// A highway is a 2-tile band from the screen's centre to each edge it leaves by.
static int HighwayTile(int x, int y) {
    if (in_cave || current_screen_index >= 20000 || WorldSiteAt(screen_grid_x, screen_grid_y) != SITE_NONE) return 0;
    int bits = WorldRoadBits(screen_grid_x, screen_grid_y), c = MAP_SIZE / 2;
    int across = y == c - 1 || y == c, down = x == c - 1 || x == c;
    return ((bits & 1) && down && y <= c) || ((bits & 4) && down && y >= c - 1) ||
           ((bits & 2) && across && x >= c - 1) || ((bits & 8) && across && x <= c);
}
void ClearHighwayTiles(void) {
    for (int y = 1; y < MAP_SIZE - 1; y++) for (int x = 1; x < MAP_SIZE - 1; x++) if (HighwayTile(x, y)) VISUAL_MAP[y][x] = 0;
}
void DrawHighway(HDC hdc) {
    float a = (float)fabs(sin(day_night_cycle_accumulator));
    if (a < 0.35f) a = 0.35f;
    HBRUSH br = CreateSolidBrush(RGB((int)(150 * a), (int)(138 * a), (int)(118 * a)));
    HGDIOBJ old = SelectObject(hdc, br), old_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int y = 0; y < MAP_SIZE; y++) for (int x = 0; x < MAP_SIZE; x++) {
        if (!HighwayTile(x, y) || VISUAL_MAP[y][x] != 0) continue;
        int sx, sy; GetIsoCoords((float)x, (float)y, &sx, &sy);
        int cy = sy + TILE_HEIGHT / 2;
        POINT p[] = { { sx, cy - TILE_HEIGHT / 2 }, { sx + TILE_WIDTH / 2, cy }, { sx, cy + TILE_HEIGHT / 2 }, { sx - TILE_WIDTH / 2, cy } };
        Polygon(hdc, p, 4);
    }
    SelectObject(hdc, old_pen); SelectObject(hdc, old); DeleteObject(br);
}

// --- Supply and demand: each town's surroundings set what is cheap there,
// and selling a lot of one thing in a town drives its price down for a while.
enum { EC_WOOD, EC_STONE, EC_FOOD, EC_FISH, EC_GEAR, EC_N };
static float sold[MAX_TOWNS][EC_N];
static int sold_day = -1;
static int Cat(const char *n) {
    if (strcmp(n, "Wood") == 0) return EC_WOOD;
    if (strcmp(n, "Stone") == 0 || strcmp(n, "Iron") == 0) return EC_STONE;
    if (strcmp(n, "Fish") == 0) return EC_FISH;
    static const char *food[] = { "Apple", "Bread", "Berr", "Wheat", "Carrot", "Potato", "Meat", "Garlic", "Corn" };
    for (int i = 0; i < 9; i++) if (strstr(n, food[i])) return EC_FOOD;
    return EC_GEAR;
}
static int HereTown(void) {
    if (in_cave) return -1;
    for (int i = 0; i < town_n; i++) if (town_x[i] == screen_grid_x && town_y[i] == screen_grid_y) return i;
    return -1;
}
// Above 1 means plenty nearby (cheap); below 1 means scarce (dear).
static float Supply(int t, int cat) {
    int cnt[EC_N] = { 0 }, desert = 0;
    for (int y = town_y[t] - 3; y <= town_y[t] + 3; y++) for (int x = town_x[t] - 3; x <= town_x[t] + 3; x++) {
        int b = B(x, y);
        if (b == BIOME_FOREST) cnt[EC_WOOD]++;
        else if (b == BIOME_MOUNTAIN) cnt[EC_STONE]++;
        else if (Wet(b) || b == BIOME_BEACH) cnt[EC_FISH]++;
        else if (b == BIOME_PRAIRIE || b == BIOME_GRASSLANDS) cnt[EC_FOOD]++;
        else if (b == BIOME_DESERT) desert++;
    }
    if (cat == EC_GEAR) return 1.0f;
    float s = 0.8f + cnt[cat] / 12.0f - (cat != EC_STONE ? desert / 30.0f : 0.0f);
    return s < 0.7f ? 0.7f : s > 1.4f ? 1.4f : s;
}
static void DecaySales(void) {
    int d = SeasonDay();
    if (d == sold_day) return;
    if (sold_day >= 0) for (int t = 0; t < MAX_TOWNS; t++) for (int c = 0; c < EC_N; c++) sold[t][c] *= 0.6f;
    sold_day = d;
}
float EconomyBuyMult(const char *name) { int t = HereTown(); return t < 0 ? 1.0f : 1.0f / Supply(t, Cat(name)); }
float EconomySellMult(const char *name) {
    int t = HereTown();
    if (t < 0) return 1.0f;
    DecaySales();
    int c = Cat(name);
    float m = 1.0f / Supply(t, c) / (1.0f + sold[t][c] / 40.0f);
    return t == 0 ? m * 1.25f : m; // Elysium is the trading hub and pays best
}
void EconomyNoteSold(const char *name, int qty) { int t = HereTown(); if (t >= 0) { DecaySales(); sold[t][Cat(name)] += (float)qty; } }
int ItemShopPrice(const MerchantItem *o) {
    int p = (int)(ShopPrice(o->base_price * (int)o->rarity) * EconomyBuyMult(o->name) + 0.5f);
    return p < 1 ? 1 : p;
}
