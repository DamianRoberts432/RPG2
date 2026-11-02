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

// Inland seas, carved after the towns and villages are placed so those never
// move: up to 3 round seas with a sandy coast and 2-3 islands each, the first
// joined to the shore by a chain of islands (a land bridge). Cells the hero has
// already seen and cells near sites are left alone. Lone islands hold treasure.
#define MAX_TREASURE_ISLES 8
static uint8_t isle_x[MAX_TREASURE_ISLES], isle_y[MAX_TREASURE_ISLES];
static int isle_n = 0;
static void CarveSeas(uint8_t keep[WG][WG]) {
    isle_n = 0;
    for (int a = 0, seas = 0; a < 400 && seas < 3; a++) {
        int cx = 10 + rand() % (WG - 20), cy = 10 + rand() % (WG - 20), r = 3 + rand() % 3, ok = 1;
        if (abs(cx - elysium_gx) + abs(cy - elysium_gy) < 12) continue;
        for (int y = cy - r - 2; y <= cy + r + 2 && ok; y++) for (int x = cx - r - 2; x <= cx + r + 2; x++) {
            int inner = abs(x - cx) <= r + 1 && abs(y - cy) <= r + 1;
            if (B(x, y) == BIOME_OCEAN || (inner && In(x, y) && (world_site[y][x] || keep[y][x]))) { ok = 0; break; }
        }
        if (!ok) continue;
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > r * r || (d2 > (r - 1) * (r - 1) && rand() % 3 == 0)) continue;
            world_biome[cy + dy][cx + dx] = BIOME_OCEAN;
        }
        for (int y = cy - r - 1; y <= cy + r + 1; y++) for (int x = cx - r - 1; x <= cx + r + 1; x++)
            if (x > 0 && y > 0 && x < WG - 1 && y < WG - 1 && B(x, y) != BIOME_OCEAN && !keep[y][x] && !world_site[y][x] && NearBiome(x, y, BIOME_OCEAN))
                world_biome[y][x] = BIOME_BEACH;
        for (int k = 0, isles = 2 + rand() % 2; k < isles; k++) {
            int span = r > 2 ? 2 * r - 3 : 1;
            int ix = cx - (r - 2) + rand() % span, iy = cy - (r - 2) + rand() % span;
            if (B(ix, iy) != BIOME_OCEAN) continue;
            world_biome[iy][ix] = BIOME_ISLAND;
            if (k == 0) { int dir = ix >= cx ? 1 : -1; for (int x = ix + dir; B(x, iy) == BIOME_OCEAN; x += dir) world_biome[iy][x] = BIOME_ISLAND; }
        }
        seas++;
    }
    for (int y = 1; y < WG - 1; y++) for (int x = 1; x < WG - 1; x++)
        if (B(x, y) == BIOME_ISLAND && isle_n < MAX_TREASURE_ISLES && B(x - 1, y) == BIOME_OCEAN && B(x + 1, y) == BIOME_OCEAN &&
            B(x, y - 1) == BIOME_OCEAN && B(x, y + 1) == BIOME_OCEAN) { isle_x[isle_n] = (uint8_t)x; isle_y[isle_n++] = (uint8_t)y; }
}
int TreasureIsleIndex(int gx, int gy) {
    for (int i = 0; i < isle_n; i++) if (isle_x[i] == gx && isle_y[i] == gy) return i;
    return -1;
}
// Open sea and lone islands can only be reached by Boat (the world-edge coast is excluded).
int SeaNeedsBoat(int gx, int gy) {
    if (!In(gx, gy) || gx == 0 || gy == 0 || gx == WG - 1 || gy == WG - 1 || HasInventoryItem("Boat")) return 0;
    return B(gx, gy) == BIOME_OCEAN || TreasureIsleIndex(gx, gy) >= 0;
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
    CarveSeas(keep);
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
int WorldTownCell(int i, int *gx, int *gy) { if (i < 0 || i >= town_n) return 0; *gx = town_x[i]; *gy = town_y[i]; return 1; }

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
    fx a = FxAbs(FxSin(day_night_cycle_accumulator));
    if (a < FX(0.35)) a = FX(0.35);
    HBRUSH br = CreateSolidBrush(RGB(FxTrunc(150 * a), FxTrunc(138 * a), FxTrunc(118 * a)));
    HGDIOBJ old = SelectObject(hdc, br), old_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int y = 0; y < MAP_SIZE; y++) for (int x = 0; x < MAP_SIZE; x++) {
        if (!HighwayTile(x, y) || VISUAL_MAP[y][x] != 0) continue;
        int sx, sy; GetIsoCoords(FxI(x), FxI(y), &sx, &sy);
        int cy = sy + TILE_HEIGHT / 2;
        POINT p[] = { { sx, cy - TILE_HEIGHT / 2 }, { sx + TILE_WIDTH / 2, cy }, { sx, cy + TILE_HEIGHT / 2 }, { sx - TILE_WIDTH / 2, cy } };
        Polygon(hdc, p, 4);
    }
    SelectObject(hdc, old_pen); SelectObject(hdc, old); DeleteObject(br);
}

// --- Supply and demand: each town's surroundings set what is cheap there,
// and selling a lot of one thing in a town drives its price down for a while.
enum { EC_WOOD, EC_STONE, EC_FOOD, EC_FISH, EC_GEAR, EC_N };
static fx sold[MAX_TOWNS][EC_N];
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
static fx Supply(int t, int cat) {
    int cnt[EC_N] = { 0 }, desert = 0;
    for (int y = town_y[t] - 3; y <= town_y[t] + 3; y++) for (int x = town_x[t] - 3; x <= town_x[t] + 3; x++) {
        int b = B(x, y);
        if (b == BIOME_FOREST) cnt[EC_WOOD]++;
        else if (b == BIOME_MOUNTAIN) cnt[EC_STONE]++;
        else if (Wet(b) || b == BIOME_BEACH) cnt[EC_FISH]++;
        else if (b == BIOME_PRAIRIE || b == BIOME_GRASSLANDS) cnt[EC_FOOD]++;
        else if (b == BIOME_DESERT) desert++;
    }
    if (cat == EC_GEAR) return FxI(1);
    fx s = FX(0.8) + FxI(cnt[cat]) / 12 - (cat != EC_STONE ? FxI(desert) / 30 : FxI(0));
    return s < FX(0.7) ? FX(0.7) : s > FX(1.4) ? FX(1.4) : s;
}
static void DecaySales(void) {
    int d = SeasonDay();
    if (d == sold_day) return;
    if (sold_day >= 0) for (int t = 0; t < MAX_TOWNS; t++) for (int c = 0; c < EC_N; c++) sold[t][c] = FxMul(sold[t][c], FX(0.6));
    sold_day = d;
}
fx EconomyBuyMult(const char *name) { int t = HereTown(); return t < 0 ? FxI(1) : FxDiv(FxI(1), Supply(t, Cat(name))); }
fx EconomySellMult(const char *name) {
    int t = HereTown();
    if (t < 0) return FxI(1);
    DecaySales();
    int c = Cat(name);
    fx m = FxDiv(FxDiv(FxI(1), Supply(t, c)), FxI(1) + sold[t][c] / 40);
    return t == 0 ? FxMul(m, FX(1.25)) : m; // Elysium is the trading hub and pays best
}
void EconomyNoteSold(const char *name, int qty) { int t = HereTown(); if (t >= 0) { DecaySales(); sold[t][Cat(name)] += FxI(qty); } }
int ItemShopPrice(const MerchantItem *o) {
    int p = FxMulInt(ShopPrice(o->base_price * (int)o->rarity), EconomyBuyMult(o->name));
    return p < 1 ? 1 : p;
}
