#include "game.h"

// Explored-world map: every overworld screen the procedural generator has
// produced is remembered in visited_biomes (biome per grid cell); map_poi adds
// the landmarks drawn on top (villages, castles, claimed cave treasure). Both
// arrays are written to SAVE_FILE_NAME whenever the game auto-saves.

#define SAVE_FILE_NAME "rpg2_save.dat"
#define SAVE_TEMP_NAME "rpg2_save.tmp"
#define SAVE_MAGIC   0x32475052u /* "RPG2" */
#define SAVE_VERSION 1u

uint8_t map_poi[META_GRID_SIZE][META_GRID_SIZE] = {0};

typedef struct {
    uint32_t magic, version;
    int32_t grid_size;
} SaveHeader;

static int CellInWorld(int gx, int gy) {
    return gx >= 0 && gx < META_GRID_SIZE && gy >= 0 && gy < META_GRID_SIZE;
}

void RecordCurrentScreenOnMap(void) {
    int gx = screen_grid_x, gy = screen_grid_y;
    if (in_cave || current_biome == BIOME_CAVE || !CellInWorld(gx, gy)) return;
    if (current_biome == BIOME_CASTLE) { map_poi[gy][gx] |= POI_CASTLE; return; }
    if (village_npc_count > 0) map_poi[gy][gx] |= POI_VILLAGE;
    else map_poi[gy][gx] &= (uint8_t)~POI_VILLAGE;
}

void MarkMapTreasureFound(int origin_cell) {
    int gx = origin_cell % META_GRID_SIZE, gy = origin_cell / META_GRID_SIZE;
    if (origin_cell >= 0 && CellInWorld(gx, gy)) map_poi[gy][gx] |= POI_TREASURE_FOUND;
}

int IsMapTreasureFound(int origin_cell) {
    int gx = origin_cell % META_GRID_SIZE, gy = origin_cell / META_GRID_SIZE;
    return origin_cell >= 0 && CellInWorld(gx, gy) && (map_poi[gy][gx] & POI_TREASURE_FOUND);
}

int SaveWorldMapState(void) {
    FILE *f = fopen(SAVE_TEMP_NAME, "wb");
    if (!f) return 0;
    SaveHeader hdr = { SAVE_MAGIC, SAVE_VERSION, META_GRID_SIZE };
    int ok = fwrite(&hdr, sizeof(hdr), 1, f) == 1 &&
             fwrite(visited_biomes, sizeof(visited_biomes), 1, f) == 1 &&
             fwrite(map_poi, sizeof(map_poi), 1, f) == 1;
    int32_t pos[2] = { screen_grid_x, screen_grid_y };
    ok = ok && fwrite(pos, sizeof(pos), 1, f) == 1;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(SAVE_TEMP_NAME); return 0; }
    return MoveFileExA(SAVE_TEMP_NAME, SAVE_FILE_NAME, MOVEFILE_REPLACE_EXISTING) != 0;
}

int LoadWorldMapState(void) {
    static uint8_t biomes[META_GRID_SIZE][META_GRID_SIZE], pois[META_GRID_SIZE][META_GRID_SIZE];
    FILE *f = fopen(SAVE_FILE_NAME, "rb");
    if (!f) return 0;
    SaveHeader hdr;
    int ok = fread(&hdr, sizeof(hdr), 1, f) == 1 && hdr.magic == SAVE_MAGIC &&
             hdr.version == SAVE_VERSION && hdr.grid_size == META_GRID_SIZE &&
             fread(biomes, sizeof(biomes), 1, f) == 1 && fread(pois, sizeof(pois), 1, f) == 1;
    // The hero's grid cell was appended later; older saves simply lack it.
    int32_t pos[2];
    int has_pos = ok && fread(pos, sizeof(pos), 1, f) == 1;
    fclose(f);
    if (!ok) return 0;
    memcpy(visited_biomes, biomes, sizeof(biomes));
    memcpy(map_poi, pois, sizeof(pois));
    if (has_pos && CellInWorld(pos[0], pos[1])) { screen_grid_x = pos[0]; screen_grid_y = pos[1]; }
    return 1;
}

static COLORREF MapBiomeColor(int biome) {
    switch (biome) {
        case BIOME_PRAIRIE:    return RGB(110, 170, 80);
        case BIOME_SWAMP:      return RGB(70, 105, 70);
        case BIOME_BEACH:      return RGB(225, 205, 140);
        case BIOME_MOUNTAIN:   return RGB(125, 118, 110);
        case BIOME_DESERT:     return RGB(215, 185, 110);
        case BIOME_TUNDRA:     return RGB(220, 228, 235);
        case BIOME_GRASSLANDS: return RGB(135, 190, 85);
        case BIOME_OASIS:      return RGB(205, 180, 105);
        case BIOME_FOREST:     return RGB(45, 110, 55);
        case BIOME_ISLAND:     return RGB(40, 95, 175);
        case BIOME_OCEAN:      return RGB(40, 90, 170);
        case BIOME_LAKE:       return RGB(105, 165, 80);
        case BIOME_RIVER:      return RGB(105, 165, 80);
        default:               return RGB(60, 60, 70);
    }
}

// Fills sub-square (sx, sy) of a cell split into a 4x4 grid of q-pixel squares.
static void FillSub(HDC hdc, int cx, int cy, int q, int sx, int sy, int w, int h, HBRUSH b) {
    RECT r = { cx + sx * q, cy + sy * q, cx + (sx + w) * q, cy + (sy + h) * q };
    FillRect(hdc, &r, b);
}

static void DrawLegendSwatch(HDC hdc, int x, int y, COLORREF c, const char *label) {
    HBRUSH b = CreateSolidBrush(c);
    RECT r = { x, y + 2, x + 12, y + 14 };
    FillRect(hdc, &r, b); DeleteObject(b);
    TextOut(hdc, x + 20, y, label, (int)strlen(label));
}

void DrawWorldMapPanel(HDC hdc, int left, int top, int max_w, int max_h) {
    // The whole 64x64 world is always shown at a fixed scale; unexplored
    // cells stay dark until visited.
    int min_x = 0, min_y = 0, max_x = META_GRID_SIZE - 1, max_y = META_GRID_SIZE - 1;
    int cols = META_GRID_SIZE, rows = META_GRID_SIZE;

    // Cells are drawn as a 4x4 grid of small squares so landmarks read as
    // "a few squares".
    int q = max_w / (cols * 4);
    if (max_h / (rows * 4) < q) q = max_h / (rows * 4);
    if (q > 10) q = 10;
    if (q < 1) q = 1;
    int cs = q * 4;

    HBRUSH bg = CreateSolidBrush(RGB(28, 32, 40));
    RECT panel = { left - 4, top - 4, left + cols * cs + 4, top + rows * cs + 4 };
    FillRect(hdc, &panel, bg); DeleteObject(bg);
    HBRUSH fog = CreateSolidBrush(RGB(40, 45, 55));
    RECT world = { left, top, left + cols * cs, top + rows * cs };
    FillRect(hdc, &world, fog); DeleteObject(fog);

    HBRUSH water = CreateSolidBrush(RGB(45, 120, 220));
    HBRUSH home = CreateSolidBrush(RGB(140, 85, 40));
    HBRUSH castle_wall = CreateSolidBrush(RGB(150, 150, 158));
    HBRUSH castle_tower = CreateSolidBrush(RGB(95, 95, 105));
    HBRUSH sand = CreateSolidBrush(RGB(225, 205, 140));
    HBRUSH treasure = CreateSolidBrush(RGB(245, 200, 40));
    HBRUSH treasure_edge = CreateSolidBrush(RGB(90, 60, 15));

    for (int gy = min_y; gy <= max_y; gy++) for (int gx = min_x; gx <= max_x; gx++) {
        int cx = left + (gx - min_x) * cs, cy = top + (gy - min_y) * cs;
        int biome = GetVisitedBiomeAt(gx, gy);
        uint8_t poi = map_poi[gy][gx];
        if (biome < 0 && !(poi & POI_CASTLE)) continue;

        if (biome >= 0) {
            HBRUSH base = CreateSolidBrush(MapBiomeColor(biome));
            RECT cell = { cx, cy, cx + cs, cy + cs };
            FillRect(hdc, &cell, base); DeleteObject(base);
        }

        if (biome == BIOME_LAKE) FillSub(hdc, cx, cy, q, 1, 1, 2, 2, water);
        else if (biome == BIOME_RIVER) { for (int r = 0; r < 4; r++) FillSub(hdc, cx, cy, q, (r == 1 || r == 2) ? 2 : 1, r, 1, 1, water); }
        else if (biome == BIOME_OASIS) FillSub(hdc, cx, cy, q, 1, 1, 1, 1, water);
        else if (biome == BIOME_SWAMP) { FillSub(hdc, cx, cy, q, 0, 2, 1, 1, water); FillSub(hdc, cx, cy, q, 2, 1, 1, 1, water); }
        else if (biome == BIOME_BEACH) { for (int c = 0; c < 4; c++) FillSub(hdc, cx, cy, q, c, 3, 1, 1, water); }
        else if (biome == BIOME_ISLAND) FillSub(hdc, cx, cy, q, 1, 1, 2, 2, sand);

        if (poi & POI_VILLAGE) {
            FillSub(hdc, cx, cy, q, 0, 0, 1, 1, home);
            FillSub(hdc, cx, cy, q, 2, 0, 1, 1, home);
            FillSub(hdc, cx, cy, q, 1, 2, 1, 1, home);
        }
        if (poi & POI_CASTLE) {
            FillSub(hdc, cx, cy, q, 0, 1, 4, 2, castle_wall);
            FillSub(hdc, cx, cy, q, 0, 0, 1, 1, castle_tower);
            FillSub(hdc, cx, cy, q, 3, 0, 1, 1, castle_tower);
            FillSub(hdc, cx, cy, q, 0, 3, 1, 1, castle_tower);
            FillSub(hdc, cx, cy, q, 3, 3, 1, 1, castle_tower);
        }
        // Every Mountain screen hides a cave; its treasure marker disappears
        // once that cave's treasure has been claimed.
        if (biome == BIOME_MOUNTAIN && !(poi & POI_TREASURE_FOUND)) {
            FillSub(hdc, cx, cy, q, 1, 1, 2, 2, treasure_edge);
            RECT inner = { cx + q + 1, cy + q + 1, cx + 3 * q - 1, cy + 3 * q - 1 };
            if (inner.right <= inner.left) inner.right = inner.left + 1;
            if (inner.bottom <= inner.top) inner.bottom = inner.top + 1;
            FillRect(hdc, &inner, treasure);
        }
    }

    if (screen_grid_x >= min_x && screen_grid_x <= max_x && screen_grid_y >= min_y && screen_grid_y <= max_y && (world_frame % 40) < 28) {
        int cx = left + (screen_grid_x - min_x) * cs, cy = top + (screen_grid_y - min_y) * cs;
        HPEN you = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HGDIOBJ old_p = SelectObject(hdc, you), old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, cx - 3, cy - 3, cx + cs + 4, cy + cs + 4);
        SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(you);
    }

    DeleteObject(water); DeleteObject(home); DeleteObject(castle_wall); DeleteObject(castle_tower);
    DeleteObject(sand); DeleteObject(treasure); DeleteObject(treasure_edge);

    int lx = left + cols * cs + 30, ly = top;
    SetTextColor(hdc, RGB(220, 225, 230)); SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, lx, ly, "LEGEND", 6);
    DrawLegendSwatch(hdc, lx, ly + 22, RGB(255, 255, 255), "You are here");
    DrawLegendSwatch(hdc, lx, ly + 42, RGB(140, 85, 40), "Village homes");
    DrawLegendSwatch(hdc, lx, ly + 62, RGB(95, 95, 105), "Castle");
    DrawLegendSwatch(hdc, lx, ly + 82, RGB(45, 120, 220), "River / lake / sea");
    DrawLegendSwatch(hdc, lx, ly + 102, RGB(245, 200, 40), "Cave treasure (unclaimed)");
    DrawLegendSwatch(hdc, lx, ly + 122, RGB(45, 110, 55), "Forest");
    DrawLegendSwatch(hdc, lx, ly + 142, RGB(110, 170, 80), "Prairie / grassland");
    DrawLegendSwatch(hdc, lx, ly + 162, RGB(125, 118, 110), "Mountain");
    DrawLegendSwatch(hdc, lx, ly + 182, RGB(215, 185, 110), "Desert");
    DrawLegendSwatch(hdc, lx, ly + 202, RGB(220, 228, 235), "Tundra");
}
