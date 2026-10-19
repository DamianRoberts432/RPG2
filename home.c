// The player's own house. Buy House Supplies from the merchant (or earn them
// with a wood or stone job), press X on open ground to mark the plot, X again
// to cycle the front design, then walk away and the merchant's builders
// finish it. It is home slot 4 for VillageHomeCentre: solid, walk-in,
// furnished with what you buy, and marked on the world map.
#include "game.h"
#include "inventory.h"
#include "physics.h"

int32_t player_home[8]; // state (0 none, 1 planning, 2 built), screen, x*100, y*100, front, furniture bits, grid x, grid y
static const char *roof_names[HOME_ROOFS] = { "thatch", "red tile", "slate", "green shingle" };
static const char *wall_names[HOME_WALLS] = { "whitewash", "timber", "stone" };
const char *furniture_names[FURNITURE_KINDS] = { "Bookshelf", "Armchair", "Potted Plant", "Painting", "Wardrobe", "Lantern" };

int PlayerHomeCentre(float *x, float *y) {
    if (!player_home[0] || in_cave || current_screen_index != player_home[1]) return 0;
    *x = player_home[2] / 100.0f; *y = player_home[3] / 100.0f;
    return 1;
}

COLORREF HomeRoofColor(int front) {
    static const COLORREF c[HOME_ROOFS] = { RGB(196, 152, 74), RGB(170, 70, 50), RGB(95, 100, 115), RGB(80, 120, 70) };
    return c[front % HOME_ROOFS];
}
COLORREF HomeWallColor(int front) {
    static const COLORREF c[HOME_WALLS] = { RGB(232, 226, 210), RGB(160, 110, 70), RGB(150, 148, 140) };
    return c[(front / HOME_ROOFS) % HOME_WALLS];
}

static void DescribeFront(void) {
    int f = player_home[4];
    sprintf(arpg_action_log, "HOME: Front %d of %d: %s roof, %s walls. X for another, walk away to build it.",
            f + 1, HOME_FRONTS, roof_names[f % HOME_ROOFS], wall_names[(f / HOME_ROOFS) % HOME_WALLS]);
}

int HomeBuildInteract(void) {
    float hx, hy;
    if (player_home[0] == 1) {
        if (!PlayerHomeCentre(&hx, &hy)) return 0;
        float dx = player_x + HOME_FEET_OFFSET - hx, dy = player_y + HOME_FEET_OFFSET - hy;
        if (dx * dx + dy * dy > 9.0f) return 0;
        player_home[4] = (player_home[4] + 1) % HOME_FRONTS;
        DescribeFront();
        return 1;
    }
    if (player_home[0] || !HasInventoryItem("House Supplies") || in_cave || current_biome == BIOME_CASTLE || village_npc_count > 0) return 0;
    hx = player_x + HOME_FEET_OFFSET; hy = player_y + HOME_FEET_OFFSET - HOME_HALF_Y - 0.9f;
    if (hx < 3 || hx > MAP_SIZE - 3 || hy < 3 || hy > MAP_SIZE - 3) return 0;
    for (float y = hy - HOME_HALF_Y; y <= hy + HOME_HALF_Y + 0.01f; y += 0.4f)
        for (float x = hx - HOME_HALF_X; x <= hx + HOME_HALF_X + 0.01f; x += 0.5f)
            if (TileBlocksPlayer(x - HOME_FEET_OFFSET, y - HOME_FEET_OFFSET)) {
                strcpy(arpg_action_log, "HOME: Clear the trees, rocks or water in front of you first.");
                return 1;
            }
    player_home[0] = 1; player_home[1] = current_screen_index;
    player_home[2] = (int)(hx * 100); player_home[3] = (int)(hy * 100);
    player_home[4] = 0; player_home[6] = screen_grid_x; player_home[7] = screen_grid_y;
    DescribeFront();
    return 1;
}

// Called every frame: a plan is built once you walk away from it, and called
// off if you leave the screen first.
void UpdatePlayerHome(void) {
    if (player_home[0] != 1) return;
    if (current_screen_index != player_home[1] || in_cave) {
        player_home[0] = 0;
        strcpy(arpg_action_log, "HOME: You left the plot, so the plan is off. X on open ground to start again.");
        return;
    }
    float dx = player_x + HOME_FEET_OFFSET - player_home[2] / 100.0f, dy = player_y + HOME_FEET_OFFSET - player_home[3] / 100.0f;
    if (dx * dx + dy * dy < 25.0f) return;
    if (!ConsumeMaterialFromInventory("House Supplies", 1)) { player_home[0] = 0; return; }
    player_home[0] = 2;
    strcpy(arpg_action_log, "HOME: The merchant's builders finish your house! It's on your map. The merchant sells furniture.");
}

// Decorations: the built-in ones plus any NewDecor lines from the Update folder.
typedef struct { char name[32]; int light, comfort; } DecorDef;
static DecorDef decor[DECOR_MAX] = {
    { "Flower Vase", 0, 1 }, { "Pothos Ivy", 0, 1 }, { "Stack of Books", 0, 1 },
    { "Snow Globe", 0, 2 }, { "Jack-o-Lantern", 1, 2 }, { "Candle", 1, 1 } };
static int decor_count = 6;

int DecorCount(void) { return decor_count; }
const char *DecorName(int k) { return k >= 0 && k < decor_count ? decor[k].name : ""; }
int DecorLight(int k) { return k >= 0 && k < decor_count && decor[k].light; }
int HomeHasDecor(int k) { return k >= 0 && k < decor_count && ((player_home[5] >> (FURNITURE_KINDS + k)) & 1); }

int AddRuntimeDecor(const char *name, int light, int comfort) {
    for (int k = 0; k < decor_count; k++)
        if (_stricmp(decor[k].name, name) == 0) { decor[k].light = light != 0; decor[k].comfort = comfort; return k; }
    if (decor_count >= DECOR_MAX) return -1;
    snprintf(decor[decor_count].name, sizeof(decor[0].name), "%s", name);
    decor[decor_count].light = light != 0; decor[decor_count].comfort = comfort;
    return decor_count++;
}

int HomeComfort(void) {
    int c = 0;
    for (int k = 0; k < FURNITURE_KINDS; k++) c += (player_home[5] >> k) & 1;
    for (int k = 0; k < decor_count; k++) if (HomeHasDecor(k)) c += decor[k].comfort;
    return c;
}

int MerchSeasonLocked(const MerchantItem *o) {
    if (strcmp(o->name, "Snow Globe") == 0) return current_season != SEASON_WINTER;
    if (strcmp(o->name, "Jack-o-Lantern") == 0) return current_season != SEASON_FALL;
    return 0;
}

int PlaceFurniture(void) {
    int placed = 0;
    for (int k = 0; k < FURNITURE_KINDS; k++)
        if (!(player_home[5] & (1 << k)) && ConsumeMaterialFromInventory(furniture_names[k], 1)) { player_home[5] |= 1 << k; placed++; }
    for (int k = 0; k < decor_count; k++)
        if (!HomeHasDecor(k) && ConsumeMaterialFromInventory(decor[k].name, 1)) { player_home[5] |= 1 << (FURNITURE_KINDS + k); placed++; }
    if (placed) sprintf(arpg_action_log, "HOME: You set out %d new piece%s. Home comfort: %d.", placed, placed == 1 ? "" : "s", HomeComfort());
    return placed;
}
