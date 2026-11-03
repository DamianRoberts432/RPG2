#include "game.h"

VillageLight village_lights[MAX_VILLAGE_LIGHTS];
int village_light_count = 0;
int cave_is_hold = 0; // this cave chamber is a dwarf hold (entered from a dwarf-hold mountain)

static void PlaceVillageLights(int shift) {
    static const int lx[5] = { 15, 9, 21, 12, 18 }, ly[5] = { 14, 13, 13, 21, 21 };
    for (int k = 0; k < 5; k++) {
        int x = lx[k] + shift, y = ly[k];
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (k == 0 || (ox == 0 && oy == 0)) VISUAL_MAP[y + oy][x + ox] = 0;
        village_lights[village_light_count++] = (VillageLight){ FxI(x), FxI(y), k != 0 };
    }
}

static int DirectionOf(fx dx, fx dy) {
    return FxAbs(dx) > FxAbs(dy) ? (dx < 0 ? FACE_LEFT : FACE_RIGHT) : (dy < 0 ? FACE_UP : FACE_DOWN);
}

void GenerateVillageNPCs(void) {
    village_npc_count = 0;
    village_light_count = 0;
    int is_market = current_biome == BIOME_CASTLE;
    // Fewer settlements: only about one Prairie screen in three holds a village.
    static const char *dwarf_greetings[3] = { "Dwarf: Mind your head, the hold was cut for us.", "Dwarf: Good stone in these hills. Good ale too.", "Dwarf: Elves? Hmph. Welcome anyway, traveler." };
    int site = in_cave || current_screen_index >= 20000 ? (in_cave && cave_is_hold ? SITE_DWARF : SITE_NONE) : WorldSiteAt(screen_grid_x, screen_grid_y);
    if (site == SITE_DWARF && !in_cave) site = SITE_NONE; // dwarf holds lie in the cave below, not on the slopes
    village_dwarves = site == SITE_DWARF;
    if (!is_market && site != SITE_VILLAGE && site != SITE_DWARF) return;
    if (in_cave) { // a great hall cut into the rock, with four pillars
        static const int pr[4] = { 9, 9, 21, 21 }, pc[4] = { 7, 23, 7, 23 };
        for (int r = 8; r <= 22; r++) for (int c = 6; c <= 24; c++) VISUAL_MAP[r][c] = 0;
        for (int k = 0; k < 4; k++)
            if (abs(pc[k] - FxTrunc(cave_treasure_x)) > 1 || abs(pr[k] - FxTrunc(cave_treasure_y)) > 1) VISUAL_MAP[pr[k]][pc[k]] = 1;
    }
    int shift = (current_screen_index % 3) - 1;
    const fx village_x[4] = { FxI(11), FxI(15), FxI(19), FxI(15) };
    const fx village_y[4] = { FxI(11), FxI(10), FxI(11), FxI(18) };
    for (int i = 0; i < 4; i++) {
        int x = FxTrunc(village_x[i]) + shift, y = FxTrunc(village_y[i]);
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) VISUAL_MAP[y + oy][x + ox] = 0;
    }
    PlaceVillageLights(shift);
    if (is_market) {
        RestockMerchant();
        village_npcs[0] = (NPCEntity){ FxI(15), FxI(17), FxI(15), FxI(17), 1, 0, FACE_DOWN, "Welcome, adventurer!", 90 };
        // Merchant and blacksmith are always here; the pet shop and card shop
        // keepers only sometimes set up in town.
        village_npcs[1] = (NPCEntity){ FxI(11), FxI(16), FxI(11), FxI(16), 3, 0, FACE_DOWN, "Blacksmith: X to step into the forge.", 130 };
        village_npc_count = 2;
        int shops = TownShopsToday(current_screen_index);
        if (shops & 1) village_npcs[village_npc_count++] = (NPCEntity){ FxI(19), FxI(16), FxI(19), FxI(16), 4, 0, FACE_DOWN, "Pet shop: X to meet the familiars.", 160 };
        if (shops & 2) village_npcs[village_npc_count++] = (NPCEntity){ FxI(15), FxI(20), FxI(15), FxI(20), 5, 0, FACE_DOWN, "Card shop: X to see today's ten cards.", 190 };
        merchant_x = village_npcs[0].x; merchant_y = village_npcs[0].y;
        // Castle hall, vault and shrine, plus a beggar in the square.
        fx hall_x = FxI(15 + shift), side_x = FxI(19 + shift), shrine_x = FxI(11 + shift);
        village_npcs[village_npc_count++] = (NPCEntity){ hall_x, FxI(10), hall_x, FxI(10), 6, 0, FACE_DOWN, "King: Heroes of renown may seek Legendary quests. X to approach the throne.", 210 };
        village_npcs[village_npc_count++] = (NPCEntity){ side_x, FxI(11), side_x, FxI(11), 9, 0, FACE_DOWN, "Banker: Gold in the vault stays safe. X to bank.", 150 };
        village_npcs[village_npc_count++] = (NPCEntity){ shrine_x, FxI(11), shrine_x, FxI(11), 8, 0, FACE_DOWN, "An old altar hums with power. X to kneel.", 999 };
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) VISUAL_MAP[19 + oy][12 + ox] = 0;
        village_npcs[village_npc_count++] = (NPCEntity){ FxI(12), FxI(19), FxI(12), FxI(19), 7, 0, FACE_DOWN, "Beggar: Spare a coin or a crust, kind traveler?", 140 };
    } else {
        for (int i = 0; i < 4; i++) {
            fx nx = village_x[i] + FxI(shift) + ((i & 1) ? FxI(1) : -FxI(1)), ny = village_y[i] + FxI(2);
            village_npcs[i] = (NPCEntity){ nx, ny, nx, ny, 0, i * 17, FACE_DOWN,
                village_dwarves ? dwarf_greetings[(current_screen_index + i) % 3] : villager_greetings[(current_screen_index + i) % 3], 100 + (i * 17) % 80 };
        }
        village_npc_count = 4;
        if (in_cave) { // every hold has its forge
            fx sx = village_x[3] + FxI(shift + 1), sy = village_y[3] + FxI(2);
            village_npcs[3] = (NPCEntity){ sx, sy, sx, sy, 3, 0, FACE_DOWN, "Dwarf smith: X to step into the forge.", 130 };
        }
        int stage = VillageStageHere();
        if (stage == 0) { village_npc_count = 2; VISUAL_MAP[14][11] = 0; } // a camp: two settlers and their stall
        if (stage >= 2) { // crop fields and the windmill
            for (int y = 16; y <= 17; y++) for (int x = 6; x <= 8; x++) VISUAL_MAP[y][x] = VISUAL_MAP[y][x + 16] = 0;
            VISUAL_MAP[8][23] = 0;
        }
        if (stage >= 3) for (int y = 6; y <= 24; y++) for (int x = 5; x <= 25; x++) if (VillageBlocksAt(x, y)) VISUAL_MAP[y][x] = 0;
        fx shrine_x = FX(13.5) + FxI(shift), beggar_x = FX(20.5) + FxI(shift); // in front of the homes, not inside them
        village_npcs[village_npc_count++] = (NPCEntity){ shrine_x, FxI(13), shrine_x, FxI(13), 8, 0, FACE_DOWN, "A wayside altar hums with power. X to kneel.", 999 };
        if (RainHash(current_screen_index * 31 + SeasonDay()) % 2)
            village_npcs[village_npc_count++] = (NPCEntity){ beggar_x, FX(13.5), beggar_x, FX(13.5), 7, 0, FACE_DOWN, "Beggar: Spare a coin or a crust, kind traveler?", 140 };
        if (stage >= 3) for (int g = 0; g < 2 && village_npc_count < MAX_NPCS; g++) { // gate guards
            fx gy = g ? FxI(23) : FxI(7);
            VISUAL_MAP[FxTrunc(gy)][17] = 0;
            village_npcs[village_npc_count++] = (NPCEntity){ FxI(17), gy, FxI(17), gy, 2, 0, FACE_DOWN, "Guard: The palisade keeps the bandits out.", 150 };
        }
    }
}

// Finds the CaveRecord for a given overworld origin cell, creating one the
// first time that cave is ever discovered. This is how re-entering the same
// Mountain's cave remembers that its treasure was already claimed.
CaveRecord *FindOrCreateCaveRecord(int origin_cell) {
    int free_slot = -1;
    for (int i = 0; i < MAX_CAVE_RECORDS; i++) {
        if (cave_records[i].valid && CaveRoom(cave_records[i].origin_cell) == CaveRoom(origin_cell)) return &cave_records[i];
        if (free_slot == -1 && !cave_records[i].valid) free_slot = i;
    }
    if (free_slot == -1) free_slot = 0; // extremely unlikely to ever fill up; reuse slot 0 defensively
    cave_records[free_slot].valid = 1;
    cave_records[free_slot].origin_cell = origin_cell;
    cave_records[free_slot].treasure_claimed = CaveRoomClaimed(origin_cell); // remembered across saves
    return &cave_records[free_slot];
}

// Explicit, player-chosen Cave entry (never automatic). Remembers exactly
// where/how to return the hero to the overworld, and keeps the Cave biome
// from ever being clobbered by the normal biome picker/roll.
void EnterCave(void) {
    if (in_cave) return;
    current_cave_origin_cell = screen_grid_y * META_GRID_SIZE + screen_grid_x;
    cave_return_grid_x = screen_grid_x;
    cave_return_grid_y = screen_grid_y;
    cave_return_player_x = player_x;
    cave_return_player_y = player_y;
    in_cave = 1;
    pending_forced_biome = BIOME_CAVE;
    cave_is_hold = WorldSiteAt(screen_grid_x, screen_grid_y) == SITE_DWARF;
    GenerateProceduralScreen(20000 + CaveRoom(current_cave_origin_cell));
    CaveRecord *rec = FindOrCreateCaveRecord(current_cave_origin_cell);
    player_x = FxI(15); player_y = FxI(15);
    if (rec->treasure_claimed) {
        strcpy(arpg_action_log, "CAVE: This cavern's treasure has already been found. Find the way out.");
    } else {
        strcpy(arpg_action_log, "CAVE: Entered the great cavern network. Each of its 8 chambers hides one chest; find this one to leave.");
    }
    if (cave_is_hold) strcpy(arpg_action_log, "DWARF HOLD: Torches light a great hall cut into the rock. The smith is in; the chest deeper in marks the way back up.");
}

// Only about one Mountain screen in four hides a treasure cave. Decided by
// the cell id alone, so the world map and the screen always agree.
int MountainHasCave(int cell) {
    unsigned int h = (unsigned int)cell * 2654435761u;
    return ((h >> 13) % 4) == 0 || WorldSiteAt(cell % META_GRID_SIZE, cell / META_GRID_SIZE) == SITE_DWARF;
}

// Only way out of a Cave: the treasure must be found first (per-cave, so a
// cleared cave never respawns loot on re-entry).
void ClaimCaveTreasureAndExit(void) {
    if (!in_cave) return;
    CaveRecord *rec = FindOrCreateCaveRecord(current_cave_origin_cell);
    if (!rec->treasure_claimed) {
        rec->treasure_claimed = 1;
        MarkMapTreasureFound(current_cave_origin_cell);
        ClaimCaveRoom(current_cave_origin_cell);
        cave_treasure_hunts_found++;
        int loot = LOOT_SHORT_SWORD + rand() % (LOOT_RUSTY_SWORD - LOOT_SHORT_SWORD + 1);
        int gold = 40 + rand() % 81;
        gold_count += gold;
        AddLegend("found a %s in a cavern hoard", weapon_catalog[loot - 1].name);
        DropGroundLoot(cave_treasure_x, cave_treasure_y, loot, 1);
        sprintf(arpg_action_log, "TREASURE: %d gold and a %s! The way out opens behind you.", gold, weapon_catalog[loot - 1].name);
        TryCardDrop(-1);
    }
    in_cave = 0;
    cave_is_hold = 0;
    pending_forced_biome = -1;
    screen_grid_x = cave_return_grid_x;
    screen_grid_y = cave_return_grid_y;
    GenerateProceduralScreen(screen_grid_y * META_GRID_SIZE + screen_grid_x);
    player_x = cave_return_player_x;
    player_y = cave_return_player_y;
}

// Returns the previously-generated biome for grid cell (gx, gy), or -1 if
// that cell has never been visited/generated yet. Out-of-world-bounds
// coordinates also return -1 (treated as "nothing there" by the picker,
// which independently treats the world edge as Beach/Ocean-only).
int GetVisitedBiomeAt(int gx, int gy) {
    if (gx < 0 || gx >= META_GRID_SIZE || gy < 0 || gy >= META_GRID_SIZE) return -1;
    uint8_t v = visited_biomes[gy][gx];
    return v == 0 ? -1 : (int)(v - 1);
}

// Direction-aware, adjacency-respecting biome picker for a *new* overworld
// grid cell. Encodes every logical rule requested for the world map:
//   - Edge-of-map cells are the only place Beach/Ocean ever appear (Ocean is
//     water-only and unreachable without a future boat; Beach always borders
//     it there).
//   - Mountains never sit directly beside Desert/Beach/Ocean ("no sky-island
//     spawns"); mountainous neighborhoods roll more Mountain/Tundra/Forest/
//     Grasslands/Swamp instead.
//   - Desert never borders a Mountain or any open water, *except* through an
//     Oasis, which is the rarest tile and Desert-only.
//   - Swamp clusters near Forest/Beach but stays uncommon; Tundra is the
//     least frequent biome of all; River/Lake are deliberately uncommon.
static BiomeType PickBiomeRaw(int gx, int gy);
// Climate by latitude: tundra crowds the far north and south of the world and
// desert the middle band (mostly far east and west); PickBiomeRaw's neighbour rules decide the rest.
BiomeType PickBiomeForScreen(int gx, int gy) {
    BiomeType b = PickBiomeRaw(gx, gy);
    fx lat = FxDiv(FxAbs(FxI(gy) + FX(0.5) - FxI(META_GRID_SIZE) / 2), FxI(META_GRID_SIZE) / 2); // 0 equator .. 1 pole
    if (b == BIOME_DESERT && lat > FX(0.5)) b = BIOME_PRAIRIE;
    if (b == BIOME_TUNDRA && lat < FX(0.55)) b = BIOME_FOREST;
    if (lat > FX(0.8) && (b == BIOME_PRAIRIE || b == BIOME_FOREST) && rand() % 100 < 65) b = BIOME_TUNDRA;
    // Deserts sit in the middle band, mostly toward the far east and far west.
    fx lon = FxDiv(FxAbs(FxI(gx) + FX(0.5) - FxI(META_GRID_SIZE) / 2), FxI(META_GRID_SIZE) / 2); // 0 centre .. 1 east/west edge
    if (b == BIOME_DESERT && lon < FX(0.4) && rand() % 100 < 70) b = BIOME_PRAIRIE;
    if (lat < FX(0.3) && b == BIOME_PRAIRIE && rand() % 100 < (lon > FX(0.6) ? 45 : 8)) b = BIOME_DESERT;
    return b;
}

static BiomeType PickBiomeRaw(int gx, int gy) {
    int is_edge = (gx == 0 || gx == META_GRID_SIZE - 1 || gy == 0 || gy == META_GRID_SIZE - 1);
    int north = GetVisitedBiomeAt(gx, gy - 1);
    int south = GetVisitedBiomeAt(gx, gy + 1);
    int west  = GetVisitedBiomeAt(gx - 1, gy);
    int east  = GetVisitedBiomeAt(gx + 1, gy);

    int near_forest = (north == BIOME_FOREST || south == BIOME_FOREST || west == BIOME_FOREST || east == BIOME_FOREST);
    int near_beach  = (north == BIOME_BEACH  || south == BIOME_BEACH  || west == BIOME_BEACH  || east == BIOME_BEACH);
    int near_mountain = (north == BIOME_MOUNTAIN || south == BIOME_MOUNTAIN || west == BIOME_MOUNTAIN || east == BIOME_MOUNTAIN);
    int near_water = (north == BIOME_OCEAN || south == BIOME_OCEAN || west == BIOME_OCEAN || east == BIOME_OCEAN ||
                       north == BIOME_LAKE  || south == BIOME_LAKE  || west == BIOME_LAKE  || east == BIOME_LAKE  ||
                       north == BIOME_RIVER || south == BIOME_RIVER || west == BIOME_RIVER || east == BIOME_RIVER);

    if (is_edge) {
        // About half of the world's outer rim is a coast (ocean on the outer
        // side, then sand, then land); the rest is ordinary land. Either way
        // the hero can't go further out, but always arrives on walkable ground.
        if (rand() % 100 < 55) return BIOME_BEACH;
    }

    int roll = rand() % 1000;

    if (near_mountain) {
        if (roll < 260) return BIOME_MOUNTAIN;
        if (roll < 310) return BIOME_TUNDRA;
        if (roll < 520) return BIOME_FOREST;
        if (roll < 720) return BIOME_GRASSLANDS;
        if (roll < 780) return BIOME_SWAMP;
        return BIOME_PRAIRIE;
    }

    if (!near_water) {
        int near_desert = (north == BIOME_DESERT || south == BIOME_DESERT || west == BIOME_DESERT || east == BIOME_DESERT);
        if (near_desert) {
            if (roll < 420) return BIOME_DESERT;
            if (roll < 428) return BIOME_OASIS; // rarest tile in the game, Desert-only
            if (roll < 520) return BIOME_GRASSLANDS;
            return BIOME_PRAIRIE;
        }
    }

    if ((near_forest || near_beach) && roll < 90) return BIOME_SWAMP; // uncommon, hugs Forest/Beach

    if (roll < 15) return BIOME_TUNDRA;   // least frequent biome overall
    if (roll < 45) return BIOME_RIVER;    // uncommon; flows toward Lake/Ocean
    if (roll < 65) return BIOME_LAKE;     // uncommon
    if (roll < 90) return BIOME_SWAMP;
    if (roll < 140 && !near_mountain && !near_water) return BIOME_DESERT;
    if (roll < 400) return BIOME_FOREST;
    if (roll < 650) return BIOME_GRASSLANDS;
    if (roll < 780) return BIOME_MOUNTAIN;
    return BIOME_PRAIRIE;
}

// Mountains: cliff walls along the screen edges that you have to walk around,
// open only where a mountain path comes in (two sides per mountain, picked by
// cell) or where the range carries on into another mountain screen. Screens
// beside a mountain show its cliff face along the shared edge, with a gap
// where its path comes down. One mountain in three is a peak: two rings of
// crags round a snowy summit, reached by a winding path, where Yeti roam.
static int MountainPathSides(int cell) { // bits: 1 north, 2 east, 4 south, 8 west
    static const int pairs[6] = { 1 | 4, 2 | 8, 1 | 2, 2 | 4, 4 | 8, 8 | 1 };
    return pairs[((unsigned int)cell * 2246822519u >> 11) % 6u];
}

int MountainIsPeak(int cell) { return ((unsigned int)cell * 3266489917u >> 15) % 3u == 0 && !MountainHasCave(cell); }

static void CliffEdge(int side, int gap, int depth) {
    for (int k = 0; k < MAP_SIZE; k++) {
        if (gap && k >= 13 && k <= 18) continue;
        for (int d = 0; d < depth; d++) {
            if (d == depth - 1 && RainHash(k * 31 + side) % 3 == 0) continue; // ragged foot of the cliff
            int r = side == 1 ? d : side == 4 ? MAP_SIZE - 1 - d : k, c = side == 8 ? d : side == 2 ? MAP_SIZE - 1 - d : k;
            VISUAL_MAP[r][c] = 1;
        }
    }
}

static void ShapeMountains(int gc, int gr) {
    static const int dx[4] = { 0, 1, 0, -1 }, dy[4] = { -1, 0, 1, 0 };
    if (current_biome == BIOME_CASTLE || current_biome == BIOME_OCEAN || current_biome == BIOME_ISLAND) return;
    int cell = gr * META_GRID_SIZE + gc;
    for (int s = 0; s < 4; s++) {
        int side = 1 << s, nx = gc + dx[s], ny = gr + dy[s];
        if (nx < 0 || ny < 0 || nx >= META_GRID_SIZE || ny >= META_GRID_SIZE) continue;
        int nb = GetVisitedBiomeAt(nx, ny), facing = 1 << ((s + 2) % 4);
        if (current_biome == BIOME_MOUNTAIN) { if (nb != BIOME_MOUNTAIN) CliffEdge(side, MountainPathSides(cell) & side, 3); }
        else if (nb == BIOME_MOUNTAIN) CliffEdge(side, MountainPathSides(ny * META_GRID_SIZE + nx) & facing, 2);
    }
    if (current_biome != BIOME_MOUNTAIN) return;
    for (int r = 4; r < MAP_SIZE - 4; r++) for (int c = 4; c < MAP_SIZE - 4; c++) // fewer loose boulders: easier to cross
        if (VISUAL_MAP[r][c] == 1 && RainHash(r * 41 + c * 7) % 3 != 0) VISUAL_MAP[r][c] = 0;
    if (!MountainIsPeak(cell)) return;
    for (int r = 3; r < MAP_SIZE - 3; r++) for (int c = 3; c < MAP_SIZE - 3; c++) {
        fx ddx = FxI(c) - FX(15.5), ddy = FxI(r) - FX(15.5), d = FxLen(ddx, ddy), a = FxAtan2(ddy, ddx);
        if (d >= FX(9.5) && d < FX(10.6)) VISUAL_MAP[r][c] = (a < -FX(2.6) || a > FX(2.6)) ? 0 : 1;      // outer crags, path in from the west
        else if (d >= FxI(6) && d < FX(7.2)) VISUAL_MAP[r][c] = (a > FX(0.25) && a < FX(0.85)) ? 0 : 1; // inner crags, path on round to the south-east
        else if (d < FxI(6)) VISUAL_MAP[r][c] = d > FX(2.5) && RainHash(r * 13 + c) % 9 == 0 ? 1 : 5; // the snowy summit
    }
}

// Caves: a real cavern system grown by cellular automaton, trimmed to the
// part you can reach from where you come down, with the treasure at the far end.
static void CarveCave(int index) {
    static uint8_t a[MAP_SIZE][MAP_SIZE], b[MAP_SIZE][MAP_SIZE];
    static int dist[MAP_SIZE][MAP_SIZE], qx[MAP_SIZE * MAP_SIZE], qy[MAP_SIZE * MAP_SIZE];
    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++)
        a[r][c] = (r < 1 || c < 1 || r >= MAP_SIZE - 1 || c >= MAP_SIZE - 1) ? 1 : abs(RainHash(index * 7919 + r * 37 + c * 1013)) % 100 < 46;
    for (int it = 0; it < 4; it++) {
        for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
            if (r < 1 || c < 1 || r >= MAP_SIZE - 1 || c >= MAP_SIZE - 1) { b[r][c] = 1; continue; }
            int n = 0;
            for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) if (y || x) n += a[r + y][c + x];
            b[r][c] = n > 4 ? 1 : n < 4 ? 0 : a[r][c];
        }
        memcpy(a, b, sizeof(a));
    }
    for (int r = 13; r <= 17; r++) for (int c = 13; c <= 17; c++) a[r][c] = 0;
    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) dist[r][c] = -1;
    int head = 0, tail = 0, far_x = 15, far_y = 15;
    dist[15][15] = 0; qx[tail] = 15; qy[tail++] = 15;
    while (head < tail) {
        int x = qx[head], y = qy[head++];
        if (dist[y][x] > dist[far_y][far_x]) { far_x = x; far_y = y; }
        for (int n = 0; n < 4; n++) {
            int nx = x + (n == 0) - (n == 1), ny = y + (n == 2) - (n == 3);
            if (a[ny][nx] || dist[ny][nx] >= 0) continue;
            dist[ny][nx] = dist[y][x] + 1; qx[tail] = nx; qy[tail++] = ny;
        }
    }
    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
        if (a[r][c] || dist[r][c] < 0) { VISUAL_MAP[r][c] = 1; continue; } // rock, and sealed-off pockets
        int walls = a[r - 1][c] + a[r + 1][c] + a[r][c - 1] + a[r][c + 1];
        VISUAL_MAP[r][c] = walls >= 2 && abs(RainHash(r * 59 + c * 17 + index)) % 6 == 0 ? 3 : 0; // rubble along the walls
    }
    cave_treasure_x = FxI(far_x); cave_treasure_y = FxI(far_y);
}

void GenerateProceduralScreen(int index) {
    RememberLeavingScreen(index);
    srand((unsigned int)(0x51A7u + index * 313u));
    for (int i = 0; i < MAX_APPLE_TREES; i++) {
        AppleTreeState *tree = &apple_tree_states[i];
        if (tree->active && tree->screen_id != index && !tree->has_left) {
            tree->has_left = 1;
            tree->left_frame = world_frame;
        }
    }
    current_screen_index = index;
    player_arrow.active = 0; // arrows never carry over into the next screen
    ice_vx = ice_vy = FxI(0);
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (campfires[i].active && campfires[i].screen_id != index) campfires[i].active = 0;
    }
    if (bedroll_screen_id != index) bedroll_screen_id = -1;
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        if (ground_loot[i].active && ground_loot[i].screen_id != index) ground_loot[i].active = 0;
    }

    int grid_r = screen_grid_y;
    int grid_c = screen_grid_x;

    if (pending_forced_biome >= 0) {
        // Forced biome (e.g. entering a Cave) bypasses rolling/recall
        // entirely so it can never be overwritten by the normal picker.
        current_biome = (BiomeType)pending_forced_biome;
        pending_forced_biome = -1;
    } else if (screens_until_town == 0) {
        current_biome = BIOME_CASTLE;
        { char town[48]; TownNameAt(30000 + screen_grid_y * META_GRID_SIZE + screen_grid_x, town, sizeof(town)); sprintf(arpg_action_log, "DISCOVERY: Arrived at the gates of %s!", town);
          snprintf(town_banner, sizeof(town_banner), "%s", town); town_banner_until = GetTickCount() + 4000; }
    } else {
        int recalled = GetVisitedBiomeAt(grid_c, grid_r);
        if (recalled != -1) {
            // Revisiting an already-generated screen always shows the same
            // biome that was drawn there before.
            current_biome = (BiomeType)recalled;
        } else {
            current_biome = (BiomeType)WorldBiomeAt(grid_c, grid_r);
        }
    }

    // Cave interiors and the Castle endpoint are special-purpose locations,
    // not part of the ordinary world grid, so they never get written into
    // (or read back out of) the overworld's visited-biome memory.
    if (current_biome != BIOME_CAVE && current_biome != BIOME_CASTLE &&
        grid_r >= 0 && grid_r < META_GRID_SIZE && grid_c >= 0 && grid_c < META_GRID_SIZE) {
        visited_biomes[grid_r][grid_c] = (uint8_t)(current_biome + 1);
    }
    // Re-seed from the cell and its biome: the biome picker only rolls on a
    // first visit, so without this a revisit would draw different tiles than
    // the ones logged on the map.
    srand((unsigned int)(0x51A7u + index * 313u) ^ ((unsigned int)current_biome * 2654435761u));

    for (int r = 0; r < MAP_SIZE; r++) {
        for (int c = 0; c < MAP_SIZE; c++) {
            VISUAL_MAP[r][c] = 0; int noise = rand() % 100;

            if (current_biome == BIOME_PRAIRIE) {
                if (noise < 12) VISUAL_MAP[r][c] = 11;
                else if (noise < 15) VISUAL_MAP[r][c] = 10;
                else if (noise < 18) VISUAL_MAP[r][c] = 2;
            } else if (current_biome == BIOME_GRASSLANDS) {
                if (noise < 30) VISUAL_MAP[r][c] = 6;
                else if (noise < 36) VISUAL_MAP[r][c] = 2;
                else if (noise < 42) VISUAL_MAP[r][c] = 11;
            } else if (current_biome == BIOME_MOUNTAIN) {
                if (noise < 18) VISUAL_MAP[r][c] = 1;
                else if (noise < 22) VISUAL_MAP[r][c] = 3;
            } else if (current_biome == BIOME_CAVE) {
                if (noise < 25) VISUAL_MAP[r][c] = 1;
                else if (noise < 30) VISUAL_MAP[r][c] = 3;
            } else if (current_biome == BIOME_SWAMP) {
                if (noise < 20) VISUAL_MAP[r][c] = 12;
                else if (noise < 27) VISUAL_MAP[r][c] = 11;
                else if (noise < 33) VISUAL_MAP[r][c] = 8;
            } else if (current_biome == BIOME_FOREST) {
                if (noise < 45) VISUAL_MAP[r][c] = 11;
                else if (noise < 52) VISUAL_MAP[r][c] = 2;
                else if (noise < 58) VISUAL_MAP[r][c] = 8;
            } else if (current_biome == BIOME_DESERT) {
                VISUAL_MAP[r][c] = 4;
                if (noise < 6) VISUAL_MAP[r][c] = 7;
            } else if (current_biome == BIOME_TUNDRA) {
                VISUAL_MAP[r][c] = 5;
                if (noise < 10) VISUAL_MAP[r][c] = 3;
                else if (noise < 16) VISUAL_MAP[r][c] = 11;
            } else if (current_biome == BIOME_OASIS) {
                VISUAL_MAP[r][c] = 4;
                if (noise < 18) VISUAL_MAP[r][c] = 12;
                else if (noise < 28) VISUAL_MAP[r][c] = 11;
            } else if (current_biome == BIOME_LAKE) {
                if (r == 0 || r == MAP_SIZE - 1 || c == 0 || c == MAP_SIZE - 1) VISUAL_MAP[r][c] = 0; // visible shore ring
                else if (r > 10 && r < 22 && c > 10 && c < 22) VISUAL_MAP[r][c] = 12;
                else if (noise < 10) VISUAL_MAP[r][c] = 11;
            } else if (current_biome == BIOME_RIVER) {
                int band = (MAP_SIZE / 2) + FxTrunc(FxSinTurn(FxTurn64((int64_t)index * FX_ONE + FxBits(FxI(r) * 3 / 10))) * 3);
                if (c >= band - 2 && c <= band + 2) VISUAL_MAP[r][c] = 12;
                else if (c == band - 3 || c == band + 3) VISUAL_MAP[r][c] = 0; // visible shore buffer
                else if (noise < 8) VISUAL_MAP[r][c] = 2;
            } else if (ShapeSeaTile(r, c, noise)) {
            } else if (current_biome == BIOME_BEACH || current_biome == BIOME_OCEAN) {
                // World-border coast: ocean on the half facing out of the world,
                // a sand beach next to it, and ordinary land on the inner side,
                // so the hero always arrives on land and can turn back. (Ocean
                // cells from older saves are drawn the same way.)
                int d = MAP_SIZE;
                if (grid_c == 0) d = c;
                if (grid_c == META_GRID_SIZE - 1 && MAP_SIZE - 1 - c < d) d = MAP_SIZE - 1 - c;
                if (grid_r == 0 && r < d) d = r;
                if (grid_r == META_GRID_SIZE - 1 && MAP_SIZE - 1 - r < d) d = MAP_SIZE - 1 - r;
                d = SeaSideDistance(r, c, d);
                int along = (grid_c == 0 || grid_c == META_GRID_SIZE - 1) ? r : c;
                int wobble = FxTrunc(FxMul(FxSinTurn(FxTurn64((int64_t)index * FX_ONE + FxBits(FxI(along) * 7 / 20))), FX(1.5)));
                if (d < 12 + wobble) VISUAL_MAP[r][c] = 12;
                else if (d < 20 + wobble) VISUAL_MAP[r][c] = 4;
                else if (noise < 8) VISUAL_MAP[r][c] = 11;
                else if (noise < 14) VISUAL_MAP[r][c] = 6;
            } else if (current_biome == BIOME_ISLAND) {
                VISUAL_MAP[r][c] = 12;
                if (r > 13 && r < 19 && c > 13 && c < 19) VISUAL_MAP[r][c] = (noise < 50) ? 4 : 11;
            } else if (current_biome == BIOME_CASTLE) {
                if (r == 8 || r == 24) VISUAL_MAP[r][c] = 1;
            }
        }
    }
    const char *variant = ApplyBiomeVariant((int)current_biome);
    if (variant) {
        size_t n = strlen(arpg_action_log);
        snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, n ? "  |  The land here is %s." : "The land here is %s.", variant);
    }
    ApplySetPieces((int)current_biome);

    for (int i = 0; i < MAX_APPLE_TREES; i++) {
        AppleTreeState *tree = &apple_tree_states[i];
        if (!tree->active || tree->screen_id != index) continue;
        if (tree->has_left && world_frame - tree->left_frame >= 300) {
            VISUAL_MAP[tree->y][tree->x] = 11;
            tree->active = 0;
        } else VISUAL_MAP[tree->y][tree->x] = 0;
    }

    if (current_biome == BIOME_CAVE) CarveCave(index); else ShapeMountains(grid_c, grid_r);

    // Cave entrances are only ever found on Mountain tiles, as a crevice
    // flanked by torches; stepping onto the crevice no longer auto-teleports
    // (see HandleContextInteract) - approaching it just surfaces the "CAVE"
    // prompt so entry is always an explicit player choice.
    if (current_biome == BIOME_MOUNTAIN && MountainHasCave(index)) {
        torch_gate_x = 12; torch_gate_y = 12;
        VISUAL_MAP[torch_gate_y][torch_gate_x] = 0;
        VISUAL_MAP[torch_gate_y][torch_gate_x - 1] = 1;
        VISUAL_MAP[torch_gate_y][torch_gate_x + 1] = 1;
    } else {
        torch_gate_x = -100; torch_gate_y = -100;
    }

    int spawn_hour, spawn_minute; GetGameClock(&spawn_hour, &spawn_minute);
    int spawn_night = spawn_hour >= 21 || spawn_hour < 6;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        critters[i].x = FxI(5 + rand() % 20);
        critters[i].y = FxI(5 + rand() % 20);
        critters[i].base_x = critters[i].x;
        critters[i].base_y = critters[i].y;
        // 0 bird, 1 squirrel (both diurnal), 2 owl, 3 bat (nocturnal). Day screens
        // get a flock of birds and a couple of squirrels, night screens an owl
        // and a bat; caves only bats.
        critters[i].type = current_biome == BIOME_CAVE ? 3 : current_biome == BIOME_DESERT && i < 2 ? 4 : spawn_night && i < 2 ? 2 + i : i % 3 == 2 ? 1 : 0;
        critters[i].timer = rand() % 90;
        critters[i].state = 0;
        critters[i].active = 1;
        critters[i].hidden = CritterAsleep(&critters[i]);
    }

    for(int i = 13; i <= 17; i++) { for(int j = 13; j <= 17; j++) { VISUAL_MAP[i][j] = 0; } }
    if (current_biome == BIOME_CAVE) {
        int tx = FxTrunc(cave_treasure_x), ty = FxTrunc(cave_treasure_y);
        for (int i = ty - 1; i <= ty + 1; i++) {
            for (int j = tx - 1; j <= tx + 1; j++) {
                if (i > 0 && i < MAP_SIZE - 1 && j > 0 && j < MAP_SIZE - 1) VISUAL_MAP[i][j] = 0;
            }
        }
    }
    ClearHighwayTiles();
    GenerateVillageNPCs();
    if (current_biome == BIOME_SWAMP) { gator_x = FxI(12); gator_y = FxI(12); gator_active = 1; } else { gator_active = 0; }
    SpawnScreenEnemies();
    if (in_cave && cave_is_hold) { for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].hearts = FxI(0); enemy_hearts = FxI(0); } // the dwarves keep their hall clear
    if (AtSea()) { for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].hearts = FxI(0); enemy_hearts = FxI(0); }
    ApplyScreenMemory(index);
    TownGrowthOnEnter();
    CacheDiggersOnEnter();
    OceanOnEnter();
    // Musashi's Blade only washes up on a beach at the world's eastern edge,
    // and only on summer day 13 (Musashi died on June 13, 1645).
    if (current_biome == BIOME_BEACH && grid_c == META_GRID_SIZE - 1 && current_season == SEASON_SUMMER &&
        SeasonDay() == 13 && !HasInventoryItem("Musashi's Blade")) {
        fx mx, my; FindDropSpot(FxI(16), FxI(16), 1, &mx, &my);
        DropGroundLoot(mx, my, LOOT_MUSASHI_BLADE, 1);
    }
    RecordCurrentScreenOnMap();
    SaveSaveFileToDisk();
}

// Daily routines: villagers sweep, work a furrow, fetch water or stop by a
// tree; the blacksmith hammers and quenches; the merchant checks his barrels
// and stares (then walks over) when you smash them.
static int npc_act[MAX_NPCS], npc_screen[MAX_NPCS], npc_leg[MAX_NPCS], npc_busy[MAX_NPCS], merchant_strikes;
static fx npc_gx[MAX_NPCS], npc_gy[MAX_NPCS], npc_until[MAX_NPCS], merchant_stare_until;

int NpcActivity(int i) { return i >= 0 && i < MAX_NPCS && npc_screen[i] == current_screen_index + 1 ? npc_act[i] : NPC_IDLE; }
int NpcAtWork(int i) { return NpcActivity(i) != NPC_IDLE && npc_busy[i]; }

static int OpenTile(int x, int y) { return x > 2 && y > 2 && x < MAP_SIZE - 3 && y < MAP_SIZE - 3 && VISUAL_MAP[y][x] != 1 && VISUAL_MAP[y][x] != 11 && VISUAL_MAP[y][x] != 12; }

static int SpotBeside(int want, fx hx, fx hy, fx range, fx *gx, fx *gy) {
    fx best = FxMul(range, range); int ok = 0;
    for (int y = 1; y < MAP_SIZE - 1; y++) for (int x = 1; x < MAP_SIZE - 1; x++) {
        if (VISUAL_MAP[y][x] != want) continue;
        for (int n = 0; n < 4; n++) {
            int nx = x + (n == 0) - (n == 1), ny = y + (n == 2) - (n == 3);
            if (!OpenTile(nx, ny)) continue;
            fx dx = FxI(nx) - hx, dy = FxI(ny) - hy, d = FxMul(dx, dx) + FxMul(dy, dy);
            if (d < best) { best = d; *gx = FxI(nx); *gy = FxI(ny); ok = 1; }
        }
    }
    return ok;
}

void NoteBarrelBroken(fx bx, fx by) {
    for (int i = 0; i < village_npc_count; i++) {
        if (village_npcs[i].type != 1) continue;
        fx dx = village_npcs[i].x - bx, dy = village_npcs[i].y - by;
        if (FxMul(dx, dx) + FxMul(dy, dy) > FxI(49)) return;
        merchant_strikes++;
        merchant_stare_until = world_tick + (merchant_strikes >= 2 ? FxI(12) : FxI(7));
        if (merchant_strikes == 1) strcpy(arpg_action_log, "MERCHANT: ...stops what he's doing and stares at you.");
        else {
            strcpy(arpg_action_log, "MERCHANT: *shakes his head* Those barrels are folks' livelihoods. Word gets around town.");
            KarmaAdjust(-1, 0, 1, 0);
        }
        return;
    }
}

// Returns 1 while the NPC is walking somewhere (or staring) on purpose.
static int NpcRoutine(int i) {
    NPCEntity *npc = &village_npcs[i];
    if (npc_screen[i] != current_screen_index + 1) {
        npc_screen[i] = current_screen_index + 1; npc_leg[i] = 1; npc_until[i] = FxI(0); npc_busy[i] = 0;
        npc_act[i] = npc->type == 1 ? NPC_INSPECT : npc->type == 3 ? NPC_HAMMER : npc->type == 0 ? NPC_SWEEP + (i + current_screen_index) % 4 : NPC_IDLE;
        if (npc->type == 1) { merchant_strikes = 0; merchant_stare_until = FxI(0); }
    }
    if (npc->type == 1 && FxBefore(world_tick, merchant_stare_until)) {
        fx dx = player_x - npc->x, dy = player_y - npc->y, d = FxLen(dx, dy);
        npc->direction = DirectionOf(dx, dy); npc_busy[i] = 0;
        if (merchant_strikes >= 2 && d > FX(1.3)) { npc->x += FxMul(FxDiv(dx, d), FX(0.035)); npc->y += FxMul(FxDiv(dy, d), FX(0.035)); }
        return 1;
    }
    int act = npc_act[i];
    if (FxAfter(world_tick, npc_until[i])) { // next leg of the routine
        npc_leg[i] ^= 1;
        npc_until[i] = world_tick + FxI(15 + abs(RainHash(i * 131 + FxTrunc(world_tick) * 3)) % 25);
        fx hx = npc->home_x, hy = npc->home_y, gx = hx, gy = hy;
        if (act == NPC_SWEEP) gx = hx + (npc_leg[i] ? FX(0.7) : -FX(0.7));
        else if (act == NPC_FARM && OpenTile(FxTrunc(hx + FX(1.3)), FxTrunc(hy + FX(1.3)))) { gx = hx + FX(1.3); gy = hy + (npc_leg[i] ? FX(1.3) : FX(2.2)); }
        else if (act == NPC_WATER && npc_leg[i] && !SpotBeside(12, hx, hy, FxI(30), &gx, &gy)) npc_act[i] = NPC_NATURE;
        else if (act == NPC_NATURE && npc_leg[i]) SpotBeside(11, hx, hy, FxI(8), &gx, &gy);
        else if (act == NPC_INSPECT && npc_leg[i]) {
            fx bx, by; int intact, n = 0, pick = abs(RainHash(i + FxTrunc(world_tick))) % 6;
            for (int k = 0; BarrelInfo(k, &bx, &by, &intact); k++) {
                if (!intact || FxMul(bx - hx, bx - hx) + FxMul(by - hy, by - hy) > FxI(36)) continue;
                if (n++ <= pick) { gx = bx + FX(0.8); gy = by; }
            }
            if (!n && OpenTile(12, 14)) { gx = FxI(12); gy = FX(14.6); } // no barrels: check the stall's goods
        }
        npc_gx[i] = gx; npc_gy[i] = gy;
    }
    fx dx = npc_gx[i] - npc->x, dy = npc_gy[i] - npc->y, d = FxLen(dx, dy);
    if (d <= (act == NPC_IDLE ? FX(1.6) : FX(0.12))) { npc_busy[i] = 1; return 0; }
    npc_busy[i] = 0;
    fx nx = npc->x + FxMul(FxDiv(dx, d), FX(0.03)), ny = npc->y + FxMul(FxDiv(dy, d), FX(0.03));
    if (VISUAL_MAP[FxTrunc(ny)][FxTrunc(nx)] != 1 && VISUAL_MAP[FxTrunc(ny)][FxTrunc(nx)] != 12) { npc->x = nx; npc->y = ny; }
    else if (VISUAL_MAP[FxTrunc(npc->y)][FxTrunc(nx)] != 1 && VISUAL_MAP[FxTrunc(npc->y)][FxTrunc(nx)] != 12) npc->x = nx;
    else if (VISUAL_MAP[FxTrunc(ny)][FxTrunc(npc->x)] != 1 && VISUAL_MAP[FxTrunc(ny)][FxTrunc(npc->x)] != 12) npc->y = ny;
    else { npc_gx[i] = npc->x; npc_gy[i] = npc->y; } // blocked: get on with the job right here instead of walking into the wall
    npc->direction = DirectionOf(dx, dy);
    return 1;
}

void UpdateVillageNPCs(void) {
    for (int i = 0; i < village_npc_count; i++) {
        NPCEntity *npc = &village_npcs[i];
        npc->idle_timer++;
        if (npc->dialogue_cooldown > 0) npc->dialogue_cooldown--;
        // Townsfolk gang up on a monster that slips past the fires, then head home.
        if ((npc->type == 0 || npc->type == 2) && SelectNearestEnemyTo(npc->x, npc->y) >= 0) {
            fx ex = enemy_x - npc->x, ey = enemy_y - npc->y, ed = FxLen(ex, ey);
            if (ed < FxI(6)) {
                if (ed > FX(0.8)) { npc->x += FxMul(FxDiv(ex, ed), FX(0.04)); npc->y += FxMul(FxDiv(ey, ed), FX(0.04)); npc->direction = DirectionOf(ex, ey); }
                else if ((world_frame + i * 9) % 45 == 0) { village_kill = 1; HandleEnemyDamage(FX(0.3)); village_kill = 0; strcpy(arpg_action_log, "VILLAGERS: The townsfolk gang up on the intruder!"); }
                continue;
            }
        }
        if (npc->type != 8 && NpcRoutine(i)) continue;
        if (IsNearPoint(npc->x, npc->y, FxI(3))) {
            fx dx = player_x - npc->x, dy = player_y - npc->y;
            npc->direction = FxAbs(dx) > FxAbs(dy) ? (dx < 0 ? FACE_LEFT : FACE_RIGHT) : (dy < 0 ? FACE_UP : FACE_DOWN);
            if (npc->dialogue_cooldown == 0) {
                const char *custom = CustomVillagerLine();
                if (npc->type == 0 && custom && rand() % 2 == 0) npc->dialogue = custom;
                else if (npc->type == 0 && (VillageStageHere() >= 0 || current_biome == BIOME_CASTLE)) npc->dialogue = rand() % 3 == 0 ? world_hints[rand() % WORLD_HINT_COUNT] : villager_greetings[rand() % 3];
                else if (npc->type == 2) npc->dialogue = fort_barks[rand() % 3];
                else if (npc->type == 1) npc->dialogue = "Welcome, adventurer!";
                npc->dialogue_cooldown = 120 + rand() % 61;
                if (IsNearPoint(npc->x, npc->y, FxI(2))) strcpy(arpg_action_log, npc->dialogue);
            }
        } else if (npc->type == 2 && world_frame % 45 == (i * 7) % 45) { // guards still pace their post
            int direction = rand() % 4;
            int dx[] = { 0, 0, -1, 1 }, dy[] = { 1, -1, 0, 0 };
            fx nx = npc->x + dx[direction] * FX(0.12), ny = npc->y + dy[direction] * FX(0.12);
            fx home_dx = nx - npc->home_x, home_dy = ny - npc->home_y;
            if (nx > FxI(3) && nx < FxI(MAP_SIZE - 3) && ny > FxI(3) && ny < FxI(MAP_SIZE - 3) &&
                FxMul(home_dx, home_dx) + FxMul(home_dy, home_dy) <= FX(2.25) &&
                VISUAL_MAP[FxTrunc(ny)][FxTrunc(nx)] != 1 && VISUAL_MAP[FxTrunc(ny)][FxTrunc(nx)] != 12) {
                npc->x = nx; npc->y = ny; npc->direction = DirectionOf(FxI(dx[direction]), FxI(dy[direction]));
            }
        }
    }
}

int CritterAsleep(const CritterEntity *critter) {
    int hour, minute; GetGameClock(&hour, &minute);
    int night = hour >= 21 || hour < 6;
    if (critter->type == 3 && current_biome == BIOME_CAVE) return 0; // bats roost in caves at any hour
    return night != (critter->type >= 2);
}

static int NearestTree(fx x, fx y, fx *tx, fx *ty) {
    fx best = FX_MAX;
    for (int yy = 1; yy < MAP_SIZE - 1; yy++) for (int xx = 1; xx < MAP_SIZE - 1; xx++) {
        if (VISUAL_MAP[yy][xx] != 11) continue;
        fx dx = FxI(xx) - x, dy = FxI(yy) - y, d = FxMul(dx, dx) + FxMul(dy, dy);
        if (d < best) { best = d; *tx = FxI(xx); *ty = FxI(yy); }
    }
    return best < FX_MAX;
}

static fx ClampField(fx v) { return v < FxI(2) ? FxI(2) : v > FxI(MAP_SIZE - 3) ? FxI(MAP_SIZE - 3) : v; }

// Sends a bird flying away from (fxv, fy) to land a few tiles off.
static void BirdTakeOff(CritterEntity *b, fx fxv, fx fy) {
    fx dx = b->x - fxv, dy = b->y - fy, d = FxLen(dx, dy);
    if (d < FX(0.01)) { fx a = FxI(rand() % 628) / 100; dx = FxCos(a); dy = FxSin(a); d = FxI(1); }
    fx dist = FxI(6 + rand() % 5);
    b->base_x = ClampField(b->x + FxMul(FxDiv(dx, d), dist) + FxI(rand() % 5 - 2));
    b->base_y = ClampField(b->y + FxMul(FxDiv(dy, d), dist) + FxI(rand() % 5 - 2));
    b->state = 2; b->hidden = 0;
}

// Birds (state 0 ground, 2 flying, 3 perched) hop about, burst into flight
// as a flock when anything comes close, fly to the trees in rain or snow and
// fly back out once it clears or the tree comes down.
static void UpdateBirds(void) {
    int bad = IsAdverseWeather() && current_biome != BIOME_CAVE;
    int trusted = karma.soul >= 50 && stat_luck >= 8;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        CritterEntity *b = &critters[i];
        if (!b->active || b->type != 0) continue;
        if (CritterAsleep(b)) { b->hidden = 1; b->state = 0; continue; }
        if (b->state == 3) {
            b->hidden = 1;
            int tx = FxTrunc(b->x + FX(0.5)), ty = FxTrunc(b->y + FX(0.5));
            if (VISUAL_MAP[ty][tx] != 11) BirdTakeOff(b, player_x, player_y);
            else if (!bad && rand() % 240 == 0) BirdTakeOff(b, b->x + FX(0.5), b->y + FX(0.5));
            continue;
        }
        if (b->state == 2) {
            fx dx = b->base_x - b->x, dy = b->base_y - b->y, d = FxLen(dx, dy);
            if (d > FX(0.15)) { b->x += FxMul(FxDiv(dx, d), FX(0.15)); b->y += FxMul(FxDiv(dy, d), FX(0.15)); continue; }
            b->x = b->base_x; b->y = b->base_y;
            int tx = FxTrunc(b->x + FX(0.5)), ty = FxTrunc(b->y + FX(0.5));
            if (bad && VISUAL_MAP[ty][tx] == 11) { b->state = 3; b->hidden = 1; }
            else { b->state = 0; b->timer = 30 + rand() % 60; }
            continue;
        }
        b->hidden = 0;
        if (bad) {
            fx tx, ty;
            if (NearestTree(b->x, b->y, &tx, &ty)) { b->base_x = tx; b->base_y = ty; b->state = 2; }
            else b->hidden = 1;
            continue;
        }
        fx near_d = FX(6.25), fxv = FxI(0), fy = FxI(0);
        int scared = 0;
        fx pdx = b->x - player_x, pdy = b->y - player_y;
        if (!trusted && FxMul(pdx, pdx) + FxMul(pdy, pdy) < near_d) { scared = 1; fxv = player_x; fy = player_y; near_d = FxMul(pdx, pdx) + FxMul(pdy, pdy); }
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (enemies[e].hearts <= FxI(0)) continue;
            fx edx = b->x - enemies[e].x, edy = b->y - enemies[e].y;
            if (FxMul(edx, edx) + FxMul(edy, edy) < near_d) { scared = 1; fxv = enemies[e].x; fy = enemies[e].y; near_d = FxMul(edx, edx) + FxMul(edy, edy); }
        }
        if (scared) {
            for (int j = 0; j < MAX_CRITTERS; j++) {
                CritterEntity *o = &critters[j];
                if (o->type != 0 || !o->active || o->state != 0 || o->hidden) continue;
                fx odx = o->x - b->x, ody = o->y - b->y;
                if (FxMul(odx, odx) + FxMul(ody, ody) < FxI(16)) BirdTakeOff(o, fxv, fy);
            }
            continue;
        }
        if (--b->timer <= 0) {
            b->x = ClampField(b->x + (rand() % 3 - 1) * FX(0.3));
            b->y = ClampField(b->y + (rand() % 3 - 1) * FX(0.3));
            b->timer = 40 + rand() % 80;
        }
    }
}

// Chopping a tree flushes the birds sheltering in it, and now and then one
// that was roosting there anyway. Returns how many flew out.
int BirdsFleeTree(int x, int y) {
    int flushed = 0;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        CritterEntity *b = &critters[i];
        if (b->type != 0 || !b->active || b->state != 3) continue;
        if (FxTrunc(b->x + FX(0.5)) == x && FxTrunc(b->y + FX(0.5)) == y) { BirdTakeOff(b, player_x, player_y); flushed++; }
    }
    if (!flushed && rand() % 3 == 0) for (int i = 0; i < MAX_CRITTERS; i++) {
        CritterEntity *b = &critters[i];
        if (b->type != 0 || !b->active || CritterAsleep(b) || b->state == 2) continue;
        b->x = FxI(x); b->y = FxI(y);
        BirdTakeOff(b, player_x, player_y);
        flushed++;
        break;
    }
    return flushed;
}

// Two birds or two squirrels sheltering in the same tree raise one baby:
// at most one per screen per day, and only into a free critter slot.
static void NestBabies(void) {
    static int nest_screen = -1, nest_day = -1;
    int day = GameDayOfYear();
    if (nest_screen == current_screen_index && nest_day == day) return;
    for (int i = 0; i < MAX_CRITTERS; i++) for (int j = i + 1; j < MAX_CRITTERS; j++) {
        CritterEntity *a = &critters[i], *b = &critters[j];
        if (!a->active || !b->active || a->type != b->type || a->type > 1 || !a->hidden || !b->hidden) continue;
        if (FxTrunc(a->x + FX(0.5)) != FxTrunc(b->x + FX(0.5)) || FxTrunc(a->y + FX(0.5)) != FxTrunc(b->y + FX(0.5))) continue;
        for (int k = 0; k < MAX_CRITTERS; k++) if (!critters[k].active) {
            critters[k] = *a; critters[k].x += FX(0.3); critters[k].state = 0; critters[k].hidden = 0;
            nest_screen = current_screen_index; nest_day = day;
            sprintf(arpg_action_log, "NATURE: A baby %s peeks out of the tree.", a->type == 0 ? "bird" : "squirrel");
            return;
        }
        return;
    }
}

void UpdateCritters(void) {
    UpdateBirds();
    if (world_frame % 30 != 0) return;
    NestBabies();
    for (int i = 0; i < MAX_CRITTERS; i++) {
        CritterEntity *critter = &critters[i];
        if (!critter->active || critter->type == 0) continue;
        // Day animals sleep from 9 PM to 6 AM; the owl only comes out then.
        if (CritterAsleep(critter)) { critter->hidden = 1; continue; }
        if (critter->type == 2 && current_biome != BIOME_CAVE) { // owls perch in trees and fly to another when disturbed
            fx pdx = critter->x - player_x, pdy = critter->y - player_y;
            int perched = VISUAL_MAP[FxTrunc(critter->base_y)][FxTrunc(critter->base_x)] == 11;
            if (!perched || FxMul(pdx, pdx) + FxMul(pdy, pdy) < FxI(9))
                for (int tries = 0; tries < 60; tries++) {
                    int x = 1 + rand() % (MAP_SIZE - 2), y = 1 + rand() % (MAP_SIZE - 2);
                    fx ddx = FxI(x) - player_x, ddy = FxI(y) - player_y;
                    if (VISUAL_MAP[y][x] == 11 && FxMul(ddx, ddx) + FxMul(ddy, ddy) > FxI(25)) { critter->base_x = FxI(x); critter->base_y = FxI(y); break; }
                }
            critter->x += (critter->base_x - critter->x) / 4; critter->y += (critter->base_y - critter->y) / 4;
            critter->state = FxAbs(critter->base_x - critter->x) + FxAbs(critter->base_y - critter->y) > FX(0.3); // 1 = flying
            critter->hidden = 0;
            continue;
        }
        // Rain or snow sends nature to the nearest tree (or into a burrow if
        // there are none); it comes back out once the weather turns fair.
        if (IsAdverseWeather() && current_biome != BIOME_CAVE) {
            fx best = FxI(1000), tx = critter->x, ty = critter->y;
            for (int y = 1; y < MAP_SIZE - 1; y++) for (int x = 1; x < MAP_SIZE - 1; x++) {
                if (VISUAL_MAP[y][x] != 11) continue;
                fx dx = FxI(x) - critter->x, dy = FxI(y) - critter->y, d = FxMul(dx, dx) + FxMul(dy, dy);
                if (d < best) { best = d; tx = FxI(x); ty = FxI(y); }
            }
            if (best < FxI(1000)) {
                critter->hidden = best <= FxI(4);
                if (best > FX(0.8)) {
                    critter->x += FxMul(tx - critter->x, FX(0.12));
                    critter->y += FxMul(ty - critter->y, FX(0.12));
                }
                continue;
            }
            critter->hidden = 1;
            continue;
        }
        critter->hidden = 0;
        // Wildlife scatters from anything nearby, unless the hero is truly
        // virtuous and lucky enough that animals trust them.
        int trusted = karma.soul >= 50 && stat_luck >= 8;
        fx fxv = FxI(0), fy = FxI(0), near_d = FxI(9);
        fx tdx = critter->x - player_x, tdy = critter->y - player_y;
        if (!trusted && FxMul(tdx, tdx) + FxMul(tdy, tdy) < near_d) { near_d = FxMul(tdx, tdx) + FxMul(tdy, tdy); fxv = tdx; fy = tdy; }
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (enemies[e].hearts <= FxI(0)) continue;
            fx edx = critter->x - enemies[e].x, edy = critter->y - enemies[e].y;
            if (FxMul(edx, edx) + FxMul(edy, edy) < near_d) { near_d = FxMul(edx, edx) + FxMul(edy, edy); fxv = edx; fy = edy; }
        }
        if (near_d < FxI(9)) {
            fx d = FxLen(fxv, fy); if (d < FX(0.01)) { fxv = FxI(1); d = FxI(1); }
            critter->x += FxMul(FxDiv(fxv, d), FX(0.6)); critter->y += FxMul(FxDiv(fy, d), FX(0.6));
            critter->state = 1;
        } else critter->state = 0;
        critter->x += (rand() % 3 - 1) * FX(0.08);
        critter->y += (rand() % 3 - 1) * FX(0.08);
        if (critter->x < FxI(2)) critter->x = FxI(2);
        if (critter->x > FxI(MAP_SIZE - 3)) critter->x = FxI(MAP_SIZE - 3);
        if (critter->y < FxI(2)) critter->y = FxI(2);
        if (critter->y > FxI(MAP_SIZE - 3)) critter->y = FxI(MAP_SIZE - 3);
    }
}

char town_banner[48];
DWORD town_banner_until;
