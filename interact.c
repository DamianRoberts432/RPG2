#include "game.h"

int IsNearPoint(float x, float y, float radius) {
    float dx = player_x - x, dy = player_y - y;
    return dx * dx + dy * dy <= radius * radius;
}

int IsNearMerchant(void) {
    return current_biome == BIOME_CASTLE && IsNearPoint(merchant_x, merchant_y, 2.0f);
}

int IsNearBedroll(void) {
    return bedroll_screen_id == current_screen_index && IsNearPoint(bedroll_x, bedroll_y, 1.2f);
}

int IsAppleTree(int x, int y) {
    return TreeHasApples(y * MAP_SIZE + x);
}

void RememberChoppedAppleTree(int x, int y) {
    for (int i = 0; i < MAX_APPLE_TREES; i++) {
        if (apple_tree_states[i].active && apple_tree_states[i].screen_id == current_screen_index &&
            apple_tree_states[i].x == x && apple_tree_states[i].y == y) return;
    }
    for (int i = 0; i < MAX_APPLE_TREES; i++) if (!apple_tree_states[i].active) {
        apple_tree_states[i] = (AppleTreeState){ x, y, current_screen_index, 0, 1, 0 };
        return;
    }
}

void HandleContextInteract(void) {
    if (TryPickupDroppedItems()) return;
    if (in_cave && IsNearPoint(cave_treasure_x, cave_treasure_y, 1.2f)) {
        ClaimCaveTreasureAndExit();
        return;
    }
    if (!in_cave && current_biome == BIOME_MOUNTAIN && IsNearPoint((float)torch_gate_x, (float)torch_gate_y, 1.3f)) {
        EnterCave();
        return;
    }
    // The bedroll sits one tile from its campfire, so X goes to whichever of
    // the two the hero is standing closer to.
    int fire = FindNearbyCampfire(1.6f);
    if (fire >= 0) {
        float fdx = player_x - campfires[fire].x, fdy = player_y - campfires[fire].y;
        float bdx = player_x - bedroll_x, bdy = player_y - bedroll_y;
        if (!IsNearBedroll() || fdx * fdx + fdy * fdy <= bdx * bdx + bdy * bdy) {
            AddWoodToFire(fire);
            return;
        }
    }
    if (IsNearBedroll()) {
        sleep_fade_frame = 1;
        strcpy(arpg_action_log, "Rested and recovered! You feel the night air...");
        return;
    }
    for (int i = 0; i < village_npc_count; i++) {
        if (!IsNearPoint(village_npcs[i].x, village_npcs[i].y, 1.5f)) continue;
        if (village_npcs[i].type == 1) {
            vendor_menu_open = 1;
            vendor_tab = 0;
            strcpy(arpg_action_log, "MARKET: LB Buy tab / RB Sell tab, SPACE confirms, ESC closes.");
        } else strcpy(arpg_action_log, village_npcs[i].dialogue);
        return;
    }

    int px = (int)player_x;
    int py = (int)player_y;
    int fx = px;
    int fy = py;
    if (player_facing == FACE_UP) fy -= 1;
    else if (player_facing == FACE_DOWN) fy += 1;
    else if (player_facing == FACE_LEFT) fx -= 1;
    else if (player_facing == FACE_RIGHT) fx += 1;

    int target_x = -1, target_y = -1;
    uint8_t target_tile = 0;

    // 1. Check tile directly in front of player
    if (fx >= 0 && fx < MAP_SIZE && fy >= 0 && fy < MAP_SIZE) {
        uint8_t t = VISUAL_MAP[fy][fx];
        if (t == 11 || t == 3 || (t == 1 && fx > 0 && fx < MAP_SIZE - 1 && fy > 0 && fy < MAP_SIZE - 1)) {
            target_x = fx;
            target_y = fy;
            target_tile = t;
        }
    }

    // 2. Check current player tile if facing was not interactable
    if (target_tile == 0 && px >= 0 && px < MAP_SIZE && py >= 0 && py < MAP_SIZE) {
        uint8_t t = VISUAL_MAP[py][px];
        if (t == 11 || t == 3 || (t == 1 && px > 0 && px < MAP_SIZE - 1 && py > 0 && py < MAP_SIZE - 1)) {
            target_x = px;
            target_y = py;
            target_tile = t;
        }
    }

    // 3. Check adjacent tiles if still not found
    if (target_tile == 0) {
        int adj[4][2] = { {px, py - 1}, {px, py + 1}, {px - 1, py}, {px + 1, py} };
        for (int i = 0; i < 4; i++) {
            int ax = adj[i][0];
            int ay = adj[i][1];
            if (ax >= 0 && ax < MAP_SIZE && ay >= 0 && ay < MAP_SIZE) {
                uint8_t t = VISUAL_MAP[ay][ax];
                if (t == 11 || t == 3 || (t == 1 && ax > 0 && ax < MAP_SIZE - 1 && ay > 0 && ay < MAP_SIZE - 1)) {
                    target_x = ax;
                    target_y = ay;
                    target_tile = t;
                    break;
                }
            }
        }
    }

    int has_axe = 0;
    int has_pickaxe = 0;
    for (int i = 0; i < player_item_count; i++) {
        if (strcmp(player_inventory[i].name, "Axe") == 0 && player_inventory[i].quantity > 0) has_axe = 1;
        if (strcmp(player_inventory[i].name, "Pickaxe") == 0 && player_inventory[i].quantity > 0) has_pickaxe = 1;
    }

    if (target_tile == 11) {
        if (has_axe) {
            SpawnTreeDebris((float)target_x, (float)target_y);
            int gained_wood = 3 + (rand() % 3);
            AddMaterialToInventory("Wood", gained_wood);
            if (IsAppleTree(target_x, target_y)) {
                int apples = 2 + rand() % 3;
                RememberChoppedAppleTree(target_x, target_y);
                DropGroundLoot((float)target_x, (float)target_y, LOOT_APPLE, apples);
            }
            VISUAL_MAP[target_y][target_x] = 0;
            sprintf(arpg_action_log, "CHOP: Felled a tree, +%d wood!", gained_wood);
        } else {
            strcpy(arpg_action_log, "CHOP: Need an Axe in inventory to chop trees!");
        }
        return;
    }

    if (target_tile == 1 || target_tile == 3) {
        if (has_pickaxe) {
            SpawnTreeDebris((float)target_x, (float)target_y);
            int gained_stone = 2 + (rand() % 3);
            AddMaterialToInventory("Stone", gained_stone);
            VISUAL_MAP[target_y][target_x] = 0;
            sprintf(arpg_action_log, "MINE: Quarried stone rock, +%d stone!", gained_stone);
        } else {
            strcpy(arpg_action_log, "MINE: Need a Pickaxe in inventory to mine rocks!");
        }
        return;
    }

    // X/C never lights a new campfire (that is done from the inventory menu via
    // HandleMenuLightFire() to avoid accidental wood loss); it only feeds one.
    strcpy(arpg_action_log, "INTERACT: Nothing to chop or mine here.");
}

int IsFacingWater(void) {
    int x = (int)player_x, y = (int)player_y;
    if (VISUAL_MAP[y][x] == 12) return 1;
    if (player_facing == FACE_UP) y--;
    else if (player_facing == FACE_DOWN) y++;
    else if (player_facing == FACE_LEFT) x--;
    else x++;
    return x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE && VISUAL_MAP[y][x] == 12;
}

int IsFacingFishWater(void) {
    int x = (int)player_x, y = (int)player_y;
    if (VISUAL_MAP[y][x] != 12) {
        if (player_facing == FACE_UP) y--;
        else if (player_facing == FACE_DOWN) y++;
        else if (player_facing == FACE_LEFT) x--;
        else x++;
    }
    if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE || VISUAL_MAP[y][x] != 12) return 0;
    return (current_screen_index * 17 + y * MAP_SIZE + x) % 3 == 0;
}
