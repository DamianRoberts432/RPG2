#include "game.h"

int FindFreeCampfireSlot(void) {
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active) return i;
    }
    return -1;
}

void PlaceCampfireAt(int slot) {
    campfires[slot].x = player_x;
    campfires[slot].y = player_y;
    campfires[slot].timer = CAMPFIRE_START_WOOD * CAMPFIRE_FRAMES_PER_WOOD;
    campfires[slot].active = 1;
    campfires[slot].screen_id = current_screen_index;
    bedroll_x = player_x + 1.0f;
    bedroll_y = player_y;
    bedroll_screen_id = current_screen_index;
}

// A fire's fuel is its remaining burn time; each log adds
// CAMPFIRE_FRAMES_PER_WOOD frames, and the fire holds CAMPFIRE_MAX_WOOD logs.
int CampfireWoodCount(int i) {
    int wood = (int)(campfires[i].timer / CAMPFIRE_FRAMES_PER_WOOD);
    if (campfires[i].timer > wood * CAMPFIRE_FRAMES_PER_WOOD) wood++;
    return wood;
}

int FindNearbyCampfire(float radius) {
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        if (IsNearPoint(campfires[i].x, campfires[i].y, radius)) return i;
    }
    return -1;
}

void AddWoodToFire(int i) {
    int wood = CampfireWoodCount(i);
    if (wood >= CAMPFIRE_MAX_WOOD) {
        sprintf(arpg_action_log, "FIRE: The campfire is full (%d/%d wood).", CAMPFIRE_MAX_WOOD, CAMPFIRE_MAX_WOOD);
        return;
    }
    if (GetMaterialCount("Wood") < 1) {
        strcpy(arpg_action_log, "FIRE: Need Wood to feed this campfire.");
        return;
    }
    ConsumeMaterialFromInventory("Wood", 1);
    campfires[i].timer += CAMPFIRE_FRAMES_PER_WOOD;
    if (campfires[i].timer > CAMPFIRE_MAX_WOOD * CAMPFIRE_FRAMES_PER_WOOD)
        campfires[i].timer = CAMPFIRE_MAX_WOOD * CAMPFIRE_FRAMES_PER_WOOD;
    RecalculateCarriedWeight();
    sprintf(arpg_action_log, "FIRE: Added Wood (%d/%d).", CampfireWoodCount(i), CAMPFIRE_MAX_WOOD);
}

int AddWoodToNearbyFire(void) {
    int i = FindNearbyCampfire(2.0f);
    if (i < 0) return 0;
    AddWoodToFire(i);
    return 1;
}

// New campfires are lit from the inventory menu (controller X on the
// inventory tab / keyboard '3'); X in the world only feeds an existing fire. All required resources (wood, a free campfire
// slot, and a match) are validated up-front; nothing is consumed unless every
// check succeeds, so a failed placement never costs the player a match or wood.
void HandleMenuLightFire(void) {
    if (GetMaterialCount("Wood") < CAMPFIRE_START_WOOD) {
        strcpy(arpg_action_log, "CAMPFIRE: Insufficient Wood! Need at least 2 logs.");
        return;
    }

    int slot = FindFreeCampfireSlot();
    if (slot == -1) {
        strcpy(arpg_action_log, "CAMPFIRE: No free campfire slots available!");
        return;
    }

    int match_idx = -1;
    for (int i = 0; i < player_item_count; i++) {
        if (strcmp(player_inventory[i].name, "Matches") == 0 && player_inventory[i].quantity > 0) {
            match_idx = i;
            break;
        }
    }

    if (match_idx == -1) {
        strcpy(arpg_action_log, "MENU FIRE: Missing Matches in inventory!");
        return;
    }

    // All preconditions verified; now it is safe to consume resources. The
    // match is removed first (it may delete its own slot and shift the
    // array), then Wood is consumed via a fresh index lookup so neither
    // removal can invalidate the other's position.
    player_inventory[match_idx].quantity--;
    if (player_inventory[match_idx].quantity <= 0) {
        for (int j = match_idx; j < player_item_count - 1; j++) {
            player_inventory[j] = player_inventory[j + 1];
        }
        player_item_count--;
        if (selected_inv_index >= player_item_count && player_item_count > 0) {
            selected_inv_index = player_item_count - 1;
        }
    }
    ConsumeMaterialFromInventory("Wood", CAMPFIRE_START_WOOD);

    PlaceCampfireAt(slot);
    strcpy(arpg_action_log, "MENU FIRE: Struck a match from Menu & ignited campfire!");
}
