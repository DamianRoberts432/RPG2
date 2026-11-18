#include "game.h"

// Worn footpaths: each step onto a grass tile wears it a little. A tile walked
// PATH_SHOWS_AT times turns to dirt, and every game day each worn tile
// recovers one step, so paths you stop using grow back.
#define PATH_SHOWS_AT 6
#define WEAR_MAX 24

WornTile worn_tiles[MAX_WORN_TILES];

static int WearableBiome(void) {
    switch (current_biome) {
        case BIOME_PRAIRIE: case BIOME_GRASSLANDS: case BIOME_FOREST: case BIOME_SWAMP:
        case BIOME_ISLAND: case BIOME_LAKE: case BIOME_RIVER: return !in_cave;
        default: return 0;
    }
}

void UpdateFootpaths(void) {
    static int last_x = -1, last_y = -1, last_screen = -1, last_day = -1;
    int today = GameDayOfYear();
    if (last_day < 0) last_day = today;
    if (today != last_day) {
        last_day = today;
        for (int i = 0; i < MAX_WORN_TILES; i++) if (worn_tiles[i].active && --worn_tiles[i].wear == 0) worn_tiles[i].active = 0;
    }
    int tx = FxTrunc(player_x), ty = FxTrunc(player_y);
    if (tx == last_x && ty == last_y && current_screen_index == last_screen) return;
    last_x = tx; last_y = ty; last_screen = current_screen_index;
    if (!WearableBiome() || tx < 0 || ty < 0 || tx >= MAP_SIZE || ty >= MAP_SIZE || VISUAL_MAP[ty][tx] != 0) return;
    int slot = -1;
    for (int i = 0; i < MAX_WORN_TILES; i++) {
        WornTile *w = &worn_tiles[i];
        if (w->active && w->screen_id == current_screen_index && w->x == tx && w->y == ty) {
            if (w->wear < WEAR_MAX) w->wear++;
            return;
        }
        if (slot < 0 || (!w->active && worn_tiles[slot].active) ||
            (w->active && worn_tiles[slot].active && w->wear < worn_tiles[slot].wear)) slot = i;
    }
    worn_tiles[slot] = (WornTile){ current_screen_index, (uint8_t)tx, (uint8_t)ty, 1, 1 };
}

int FootpathAt(int x, int y) {
    for (int i = 0; i < MAX_WORN_TILES; i++) {
        const WornTile *w = &worn_tiles[i];
        if (w->active && w->screen_id == current_screen_index && w->x == x && w->y == y && w->wear >= PATH_SHOWS_AT) return 1;
    }
    return 0;
}

void DrawFootpaths(HDC hdc) {
    DrawHighway(hdc);
    fx a = FxAbs(FxSin(day_night_cycle_accumulator));
    if (a < FX(0.35)) a = FX(0.35);
    HGDIOBJ old_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int i = 0; i < MAX_WORN_TILES; i++) {
        WornTile *w = &worn_tiles[i];
        if (!w->active || w->screen_id != current_screen_index || w->wear < PATH_SHOWS_AT) continue;
        if (VISUAL_MAP[w->y][w->x] != 0) continue;
        // Whole tiles on the ground's own (hilly) corners, so neighbouring path tiles
        // always meet edge to edge; well-trodden ones are a little darker.
        POINT dirt[4]; TileCorners(w->x, w->y, dirt);
        fx k = FxI(w->wear - PATH_SHOWS_AT) / (WEAR_MAX - PATH_SHOWS_AT);
        HBRUSH br = CreateSolidBrush(RGB(FxTrunc(FxMul(FxI(162) - 22 * k, a)), FxTrunc(FxMul(FxI(132) - 24 * k, a)), FxTrunc(FxMul(FxI(86) - 18 * k, a)))); HGDIOBJ old = SelectObject(hdc, br);
        Polygon(hdc, dirt, 4);
        SelectObject(hdc, old); DeleteObject(br);
    }
    SelectObject(hdc, old_pen);
}
