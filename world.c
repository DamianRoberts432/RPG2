#include "game.h"

void GenerateVillageNPCs(void) {
    village_npc_count = 0;
    int is_market = current_biome == BIOME_CASTLE;
    if (!is_market && !(current_biome == BIOME_PRAIRIE && screens_until_town > 2)) return;
    int shift = (current_screen_index % 3) - 1;
    const float village_x[4] = { 11.0f, 15.0f, 19.0f, 15.0f };
    const float village_y[4] = { 11.0f, 10.0f, 11.0f, 18.0f };
    for (int i = 0; i < 4; i++) {
        int x = (int)village_x[i] + shift, y = (int)village_y[i];
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) VISUAL_MAP[y + oy][x + ox] = 0;
    }
    if (is_market) {
        RestockMerchant();
        village_npcs[0] = (NPCEntity){ 15.0f, 17.0f, 15.0f, 17.0f, 1, 0, FACE_DOWN, "Welcome, adventurer!", 90 };
        village_npcs[1] = (NPCEntity){ 11.0f, 16.0f, 11.0f, 16.0f, 2, 0, FACE_DOWN, "Fine wares from distant roads.", 130 };
        village_npcs[2] = (NPCEntity){ 19.0f, 16.0f, 19.0f, 16.0f, 2, 0, FACE_DOWN, "Keep your purse close.", 160 };
        village_npc_count = 3;
        merchant_x = village_npcs[0].x; merchant_y = village_npcs[0].y;
    } else {
        for (int i = 0; i < 4; i++) {
            float nx = village_x[i] + shift + ((i & 1) ? 1.0f : -1.0f), ny = village_y[i] + 2.0f;
            village_npcs[i] = (NPCEntity){ nx, ny, nx, ny, 0, i * 17, FACE_DOWN,
                villager_greetings[(current_screen_index + i) % 3], 100 + (i * 17) % 80 };
        }
        village_npc_count = 4;
    }
}

// Finds the CaveRecord for a given overworld origin cell, creating one the
// first time that cave is ever discovered. This is how re-entering the same
// Mountain's cave remembers that its treasure was already claimed.
CaveRecord *FindOrCreateCaveRecord(int origin_cell) {
    int free_slot = -1;
    for (int i = 0; i < MAX_CAVE_RECORDS; i++) {
        if (cave_records[i].valid && cave_records[i].origin_cell == origin_cell) return &cave_records[i];
        if (free_slot == -1 && !cave_records[i].valid) free_slot = i;
    }
    if (free_slot == -1) free_slot = 0; // extremely unlikely to ever fill up; reuse slot 0 defensively
    cave_records[free_slot].valid = 1;
    cave_records[free_slot].origin_cell = origin_cell;
    cave_records[free_slot].treasure_claimed = IsMapTreasureFound(origin_cell); // remembered across saves
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
    GenerateProceduralScreen(20000 + current_cave_origin_cell);
    CaveRecord *rec = FindOrCreateCaveRecord(current_cave_origin_cell);
    player_x = 15.0f; player_y = 15.0f;
    if (rec->treasure_claimed) {
        strcpy(arpg_action_log, "CAVE: This cavern's treasure has already been found. Find the way out.");
    } else {
        strcpy(arpg_action_log, "CAVE: Entered the Subterranean Caverns! Find the treasure to leave.");
    }
}

// Only about one Mountain screen in four hides a treasure cave. Decided by
// the cell id alone, so the world map and the screen always agree.
int MountainHasCave(int cell) {
    unsigned int h = (unsigned int)cell * 2654435761u;
    return ((h >> 13) % 4) == 0;
}

// Only way out of a Cave: the treasure must be found first (per-cave, so a
// cleared cave never respawns loot on re-entry).
void ClaimCaveTreasureAndExit(void) {
    if (!in_cave) return;
    CaveRecord *rec = FindOrCreateCaveRecord(current_cave_origin_cell);
    if (!rec->treasure_claimed) {
        rec->treasure_claimed = 1;
        MarkMapTreasureFound(current_cave_origin_cell);
        cave_treasure_hunts_found++;
        int loot = LOOT_SHORT_SWORD + rand() % (LOOT_RUSTY_SWORD - LOOT_SHORT_SWORD + 1);
        int gold = 40 + rand() % 81;
        gold_count += gold;
        DropGroundLoot(cave_treasure_x, cave_treasure_y, loot, 1);
        sprintf(arpg_action_log, "TREASURE: %d gold and a %s! The way out opens behind you.", gold, weapon_catalog[loot - 1].name);
    }
    in_cave = 0;
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
BiomeType PickBiomeForScreen(int gx, int gy) {
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

void GenerateProceduralScreen(int index) {
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
    ice_vx = ice_vy = 0.0f;
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
        sprintf(arpg_action_log, "DISCOVERY: Arrived at Townsville Castle perimeter gates!");
    } else {
        int recalled = GetVisitedBiomeAt(grid_c, grid_r);
        if (recalled != -1) {
            // Revisiting an already-generated screen always shows the same
            // biome that was drawn there before.
            current_biome = (BiomeType)recalled;
        } else {
            current_biome = PickBiomeForScreen(grid_c, grid_r);
        }
    }

    // Cave interiors and the Castle endpoint are special-purpose locations,
    // not part of the ordinary world grid, so they never get written into
    // (or read back out of) the overworld's visited-biome memory.
    if (current_biome != BIOME_CAVE && current_biome != BIOME_CASTLE &&
        grid_r >= 0 && grid_r < META_GRID_SIZE && grid_c >= 0 && grid_c < META_GRID_SIZE) {
        visited_biomes[grid_r][grid_c] = (uint8_t)(current_biome + 1);
    }

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
                int band = (MAP_SIZE / 2) + (int)(sinf((float)r * 0.3f + (float)index) * 3.0f);
                if (c >= band - 2 && c <= band + 2) VISUAL_MAP[r][c] = 12;
                else if (c == band - 3 || c == band + 3) VISUAL_MAP[r][c] = 0; // visible shore buffer
                else if (noise < 8) VISUAL_MAP[r][c] = 2;
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
                int along = (grid_c == 0 || grid_c == META_GRID_SIZE - 1) ? r : c;
                int wobble = (int)(sinf((float)along * 0.35f + (float)index) * 1.5f);
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

    for (int i = 0; i < MAX_APPLE_TREES; i++) {
        AppleTreeState *tree = &apple_tree_states[i];
        if (!tree->active || tree->screen_id != index) continue;
        if (tree->has_left && world_frame - tree->left_frame >= 300) {
            VISUAL_MAP[tree->y][tree->x] = 11;
            tree->active = 0;
        } else VISUAL_MAP[tree->y][tree->x] = 0;
    }

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

    for (int i = 0; i < MAX_CRITTERS; i++) {
        critters[i].x = (float)(5 + rand() % 20);
        critters[i].y = (float)(5 + rand() % 20);
        critters[i].base_x = critters[i].x;
        critters[i].base_y = critters[i].y;
        critters[i].type = rand() % 2; 
        critters[i].state = 0;
        critters[i].active = 1;
        critters[i].hidden = 0;
    }

    for(int i = 13; i <= 17; i++) { for(int j = 13; j <= 17; j++) { VISUAL_MAP[i][j] = 0; } }
    if (current_biome == BIOME_CAVE) {
        int tx = (int)cave_treasure_x, ty = (int)cave_treasure_y;
        for (int i = ty - 1; i <= ty + 1; i++) {
            for (int j = tx - 1; j <= tx + 1; j++) {
                if (i > 0 && i < MAP_SIZE - 1 && j > 0 && j < MAP_SIZE - 1) VISUAL_MAP[i][j] = 0;
            }
        }
    }
    GenerateVillageNPCs();
    if (current_biome == BIOME_SWAMP) { gator_x = 12.0f; gator_y = 12.0f; gator_active = 1; } else { gator_active = 0; }
    TriggerMonsterRespawn();
    RecordCurrentScreenOnMap();
    SaveSaveFileToDisk();
}

void UpdateVillageNPCs(void) {
    for (int i = 0; i < village_npc_count; i++) {
        NPCEntity *npc = &village_npcs[i];
        npc->idle_timer++;
        if (npc->dialogue_cooldown > 0) npc->dialogue_cooldown--;
        if (IsNearPoint(npc->x, npc->y, 3.0f)) {
            float dx = player_x - npc->x, dy = player_y - npc->y;
            npc->direction = fabs(dx) > fabs(dy) ? (dx < 0 ? FACE_LEFT : FACE_RIGHT) : (dy < 0 ? FACE_UP : FACE_DOWN);
            if (npc->dialogue_cooldown == 0) {
                if (npc->type == 0) npc->dialogue = villager_greetings[rand() % 3];
                else if (npc->type == 2) npc->dialogue = fort_barks[rand() % 3];
                else npc->dialogue = "Welcome, adventurer!";
                npc->dialogue_cooldown = 120 + rand() % 61;
                if (IsNearPoint(npc->x, npc->y, 2.0f)) strcpy(arpg_action_log, npc->dialogue);
            }
        } else if (world_frame % 45 == (i * 7) % 45) {
            int direction = rand() % 4;
            float dx[] = { 0, 0, -1, 1 }, dy[] = { 1, -1, 0, 0 };
            float nx = npc->x + dx[direction] * 0.12f, ny = npc->y + dy[direction] * 0.12f;
            float home_dx = nx - npc->home_x, home_dy = ny - npc->home_y;
            if (nx > 3 && nx < MAP_SIZE - 3 && ny > 3 && ny < MAP_SIZE - 3 &&
                home_dx * home_dx + home_dy * home_dy <= 2.25f &&
                VISUAL_MAP[(int)ny][(int)nx] != 1 && VISUAL_MAP[(int)ny][(int)nx] != 12) {
                npc->x = nx; npc->y = ny;
            }
        }
    }
}

void UpdateCritters(void) {
    if (world_frame % 30 != 0) return;
    for (int i = 0; i < MAX_CRITTERS; i++) {
        CritterEntity *critter = &critters[i];
        if (!critter->active) continue;
        // Rain or snow sends nature to the nearest tree (or into a burrow if
        // there are none); it comes back out once the weather turns fair.
        if (IsAdverseWeather()) {
            float best = 1000.0f, tx = critter->x, ty = critter->y;
            for (int y = 1; y < MAP_SIZE - 1; y++) for (int x = 1; x < MAP_SIZE - 1; x++) {
                if (VISUAL_MAP[y][x] != 11) continue;
                float dx = x - critter->x, dy = y - critter->y, d = dx * dx + dy * dy;
                if (d < best) { best = d; tx = (float)x; ty = (float)y; }
            }
            if (best < 1000.0f) {
                critter->hidden = best <= 4.0f;
                if (best > 0.8f) {
                    critter->x += (tx - critter->x) * 0.12f;
                    critter->y += (ty - critter->y) * 0.12f;
                }
                continue;
            }
            critter->hidden = 1;
            continue;
        }
        critter->hidden = 0;
        critter->x += (float)(rand() % 3 - 1) * 0.08f;
        critter->y += (float)(rand() % 3 - 1) * 0.08f;
        if (critter->x < 2) critter->x = 2;
        if (critter->x > MAP_SIZE - 3) critter->x = MAP_SIZE - 3;
        if (critter->y < 2) critter->y = 2;
        if (critter->y > MAP_SIZE - 3) critter->y = MAP_SIZE - 3;
    }
}
