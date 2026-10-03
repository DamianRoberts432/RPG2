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
    campfires[slot].timer = 1800.0f;
    campfires[slot].active = 1;
    campfires[slot].screen_id = current_screen_index;
    bedroll_x = player_x + 1.0f;
    bedroll_y = player_y;
    bedroll_screen_id = current_screen_index;
}

#define CAMPFIRE_MAX_TIMER 3600.0f

// Adding Wood to an already-burning campfire extends how long it lasts
// (up to a hard cap), instead of letting it slowly die out unattended.
int AddWoodToNearbyFire(void) {
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        if (!IsNearPoint(campfires[i].x, campfires[i].y, 2.0f)) continue;
        if (GetMaterialCount("Wood") < 1) {
            strcpy(arpg_action_log, "FIRE: Need Wood to feed this campfire.");
            return 1;
        }
        ConsumeMaterialFromInventory("Wood", 1);
        campfires[i].timer += 400.0f;
        if (campfires[i].timer > CAMPFIRE_MAX_TIMER) campfires[i].timer = CAMPFIRE_MAX_TIMER;
        RecalculateCarriedWeight();
        strcpy(arpg_action_log, "FIRE: Added Wood; the campfire burns longer.");
        return 1;
    }
    return 0;
}

// Campfires may only be ignited from the inventory menu (controller X on the
// inventory tab / keyboard '3'). All required resources (wood, a free campfire
// slot, and a match) are validated up-front; nothing is consumed unless every
// check succeeds, so a failed placement never costs the player a match or wood.
void HandleMenuLightFire(void) {
    if (GetMaterialCount("Wood") < 2) {
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
    ConsumeMaterialFromInventory("Wood", 2);

    PlaceCampfireAt(slot);
    strcpy(arpg_action_log, "MENU FIRE: Struck a match from Menu & ignited campfire!");
}
