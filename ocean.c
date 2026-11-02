// Inland seas. Open-sea screens are all water and need a Boat. Islands are
// a disc of sand and palms with a sand spit toward any land or island beside
// them, so a chain of islands is a land bridge you can walk. Each lone island
// hides one treasure chest (8 at most, one claimed bit each in the save).
#include "game.h"

int isle_treasure_bits = 0;

static int OpenSeaCell(void) {
    return current_biome == BIOME_OCEAN && screen_grid_x > 0 && screen_grid_y > 0 &&
           screen_grid_x < META_GRID_SIZE - 1 && screen_grid_y < META_GRID_SIZE - 1;
}
int AtSea(void) { return !in_cave && inside_home < 0 && (OpenSeaCell() || current_biome == BIOME_ISLAND); }

// Shapes one tile of an open-sea or island screen; returns 0 for any other screen.
int ShapeSeaTile(int r, int c, int noise) {
    static const int ox[4] = { 0, 1, 0, -1 }, oy[4] = { -1, 0, 1, 0 };
    if (in_cave) return 0;
    if (OpenSeaCell()) { VISUAL_MAP[r][c] = 12; return 1; }
    if (current_biome != BIOME_ISLAND) return 0;
    float dx = c - 15.5f, dy = r - 15.5f;
    int land = dx * dx + dy * dy < 36.0f;
    for (int s = 0; s < 4 && !land; s++) {
        int nx = screen_grid_x + ox[s], ny = screen_grid_y + oy[s];
        if (nx < 0 || ny < 0 || nx >= META_GRID_SIZE || ny >= META_GRID_SIZE || WorldBiomeAt(nx, ny) == BIOME_OCEAN) continue;
        int along = ox[s] ? c : r, across = ox[s] ? r : c;
        if (across >= 13 && across <= 18 && (ox[s] + oy[s] > 0 ? along >= 16 : along <= 15)) land = 1;
    }
    VISUAL_MAP[r][c] = land ? (noise < 12 ? 11 : 4) : 12;
    return 1;
}

// Coast screens put their water on every side that faces an open-sea cell.
int SeaSideDistance(int r, int c, int d) {
    int gx = screen_grid_x, gy = screen_grid_y, last = META_GRID_SIZE - 1;
    if (gx > 0 && WorldBiomeAt(gx - 1, gy) == BIOME_OCEAN && c < d) d = c;
    if (gx < last && WorldBiomeAt(gx + 1, gy) == BIOME_OCEAN && MAP_SIZE - 1 - c < d) d = MAP_SIZE - 1 - c;
    if (gy > 0 && WorldBiomeAt(gx, gy - 1) == BIOME_OCEAN && r < d) d = r;
    if (gy < last && WorldBiomeAt(gx, gy + 1) == BIOME_OCEAN && MAP_SIZE - 1 - r < d) d = MAP_SIZE - 1 - r;
    return d;
}

// A small wooden hull drawn over the hero's legs while on water.
void DrawHeroBoat(HDC hdc, int sx, int sy) {
    int big = HasInventoryItem("Boat");
    HBRUSH hull = CreateSolidBrush(big ? RGB(120, 78, 44) : RGB(150, 112, 70));
    HPEN rim = CreatePen(PS_SOLID, 2, RGB(200, 160, 105));
    HGDIOBJ ob = SelectObject(hdc, hull), op = SelectObject(hdc, rim);
    int w = big ? 18 : 14;
    POINT p[4] = { { sx - w, sy - 6 }, { sx + w, sy - 6 }, { sx + w - 5, sy + 5 }, { sx - w + 5, sy + 5 } };
    Polygon(hdc, p, 4);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(hull); DeleteObject(rim);
}

int TreasureIsleHere(void) {
    if (in_cave || current_biome != BIOME_ISLAND) return -1;
    return TreasureIsleIndex(screen_grid_x, screen_grid_y);
}

void DrawIsleTreasure(HDC hdc, int c, int r, int sx, int sy) {
    int i = TreasureIsleHere();
    if (i < 0 || c != 15 || r != 15) return;
    int claimed = (isle_treasure_bits >> i) & 1;
    HBRUSH b = CreateSolidBrush(claimed ? RGB(90, 70, 40) : RGB(230, 190, 40));
    RECT chest = { sx - 8, sy - 6, sx + 8, sy + 6 };
    FillRect(hdc, &chest, b); DeleteObject(b);
    if (!claimed && IsNearPoint(15.0f, 15.0f, 3.0f)) {
        SetTextColor(hdc, RGB(255, 255, 255)); SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, sx - 26, sy - 24, "TREASURE", 8);
    }
}

int TryIsleTreasure(void) {
    int i = TreasureIsleHere();
    if (i < 0 || !IsNearPoint(15.0f, 15.0f, 1.3f)) return 0;
    if ((isle_treasure_bits >> i) & 1) { strcpy(arpg_action_log, "TREASURE ISLE: The chest is empty. Someone got here first... you."); return 1; }
    int gold = 500 + 100 * (i % 4);
    isle_treasure_bits |= 1 << i;
    gold_count += gold;
    AddLegend("found the treasure of a lonely isle");
    snprintf(arpg_action_log, sizeof(arpg_action_log), "TREASURE ISLE: You haul up the chest: %d gold!", gold);
    return 1;
}

void OceanOnEnter(void) {
    if (in_cave) return;
    int i = TreasureIsleHere();
    if (i >= 0) {
        snprintf(town_banner, sizeof(town_banner), "Treasure Isle");
        snprintf(town_banner_sub, sizeof(town_banner_sub), "%s", (isle_treasure_bits >> i) & 1 ? "Its chest is already empty" : "A chest waits on the sand");
        town_banner_until = GetTickCount() + 4000;
    } else if (current_biome == BIOME_ISLAND) strcpy(arpg_action_log, "ISLAND: Sand and palms in the middle of the sea.");
    else if (OpenSeaCell()) strcpy(arpg_action_log, "OPEN SEA: Waves in every direction. Islands lie somewhere out here.");
}
