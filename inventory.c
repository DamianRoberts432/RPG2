#include "game.h"

void SyncActiveWeapon(void) {
    active_weapon = WEAPON_SWORD;
    for (int i = 0; i < player_item_count; i++) {
        if (!player_inventory[i].is_equipped || strcmp(player_inventory[i].slot, "Weapon") != 0) continue;
        active_weapon = strstr(player_inventory[i].name, "Bow") != NULL ? WEAPON_BOW :
            strcmp(player_inventory[i].name, "Axe") == 0 ? WEAPON_AXE : WEAPON_SWORD;
        return;
    }
}

void ToggleEquipSelectedItem(void) {
    if (player_item_count == 0) return;
    InventoryItem *item = &player_inventory[selected_inv_index];

    // Raw materials (Wood/Stone) can't be worn or wielded; they always count
    // toward carry weight, so equipping them makes no sense.
    if (strcmp(item->slot, "Material") == 0) {
        strcpy(arpg_action_log, "INVENTORY: That can't be equipped.");
        return;
    }

    if (item->is_equipped) {
        item->is_equipped = 0;
        SyncActiveWeapon();
        sprintf(arpg_action_log, "INVENTORY: Unequipped %s", item->name);
    } else {
        for (int i = 0; i < player_item_count; i++) {
            if (strcmp(player_inventory[i].slot, item->slot) == 0) {
                player_inventory[i].is_equipped = 0;
            }
        }
        item->is_equipped = 1;
        SyncActiveWeapon();
        sprintf(arpg_action_log, "INVENTORY: Equipped %s", item->name);
    }
    RecalculateCarriedWeight();
}

// Single context-sensitive "use" action for whatever item is currently
// selected in the inventory (bound to A on gamepad / "1" on keyboard):
//   Weapon/Armor/Shield -> equip/unequip
//   Matches              -> ignite a campfire (consumes 1 Match + 2 Wood)
//   Fish/Apple            -> eat to restore health
//   Tools (Axe/Pickaxe)   -> already "equipped" automatically for gathering
void HandleInventoryPrimaryAction(void) {
    if (player_item_count == 0) return;
    InventoryItem *item = &player_inventory[selected_inv_index];
    if (strcmp(item->name, "Matches") == 0) {
        HandleMenuLightFire();
    } else if (strcmp(item->name, "Wood") == 0 && AddWoodToNearbyFire()) {
        // handled inside AddWoodToNearbyFire()
    } else if (strcmp(item->name, "Fish") == 0 || strcmp(item->name, "Apple") == 0) {
        ConsumeSelectedFood();
    } else if (strcmp(item->slot, "Weapon") == 0 || strcmp(item->slot, "Armor") == 0 || strcmp(item->slot, "Shield") == 0) {
        ToggleEquipSelectedItem();
    } else if (strcmp(item->slot, "Tool") == 0) {
        strcpy(arpg_action_log, "TOOL: Already ready to gather - just interact with trees/rocks.");
    } else {
        strcpy(arpg_action_log, "INVENTORY: Nothing to do with that item.");
    }
}

WeaponStats *GetEquippedWeaponStats(void) {
    for (int i = 0; i < player_item_count; i++) {
        if (!player_inventory[i].is_equipped || strcmp(player_inventory[i].slot, "Weapon") != 0) continue;
        for (int j = 0; j < WEAPON_CATALOG_COUNT; j++) if (strcmp(player_inventory[i].name, weapon_catalog[j].name) == 0) return &weapon_catalog[j];
        if (strcmp(player_inventory[i].name, "Iron Sword") == 0) return &weapon_catalog[0];
        if (strcmp(player_inventory[i].name, "Axe") == 0) return &weapon_catalog[2];
    }
    return NULL;
}

int WearEquippedWeapon(void) {
    for (int i = 0; i < player_item_count; i++) {
        InventoryItem *item = &player_inventory[i];
        if (!item->is_equipped || strcmp(item->slot, "Weapon") != 0 || item->durability_max <= 0) continue;
        if (--item->durability_current <= 0) {
            sprintf(arpg_action_log, "%s broke!", item->name);
            for (int j = i; j < player_item_count - 1; j++) player_inventory[j] = player_inventory[j + 1];
            player_item_count--;
            RecalculateCarriedWeight();
            SyncActiveWeapon();
            return 1;
        }
        return 0;
    }
    return 0;
}

// Canonical weight-per-item lookup used across the inventory system. Matches
// the player-specified reference scale (Wood 1, Stone 1, Matches 1, Dagger 1,
// Short Bow 1.5, Fishing Pole 2, Short Sword 2, Long Bow 2, Axe 4, Iron Sword
// 4, Armor 5, Shield 3) with sensible proportional defaults for every other
// item already present in the game. This weight only ever counts against the
// carry limit while an item sits unequipped in the pack - see
// RecalculateCarriedWeight(), which excludes anything currently worn/wielded.
float GetNamedItemWeight(const char *name) {
    if (strcmp(name, "Wood") == 0) return 1.0f;
    if (strcmp(name, "Stone") == 0) return 1.0f;
    if (strcmp(name, "Short Sword") == 0) return 2.0f;
    if (strcmp(name, "Long Bow") == 0) return 2.0f;
    if (strcmp(name, "Short Bow") == 0) return 1.5f;
    if (strcmp(name, "Dagger") == 0) return 1.0f;
    if (strcmp(name, "Long Sword") == 0) return 5.0f;
    if (strcmp(name, "Axe") == 0) return 4.0f;
    if (strcmp(name, "Rusty Sword") == 0) return 2.0f;
    if (strcmp(name, "Iron Sword") == 0) return 4.0f;
    if (strcmp(name, "Wooden Shield") == 0) return 3.0f;
    if (strcmp(name, "Leather Armor") == 0) return 5.0f;
    if (strcmp(name, "Pickaxe") == 0) return 3.0f;
    if (strcmp(name, "Fishing Pole") == 0) return 2.0f;
    if (strcmp(name, "Fish") == 0) return 1.0f;
    if (strcmp(name, "Apple") == 0) return 0.5f;
    if (strcmp(name, "Matches") == 0) return 1.0f;
    if (strcmp(name, "Dragon Scale Armor") == 0) return 6.0f;
    if (strcmp(name, "Musashi's Blade") == 0) return 1.0f;
    return 0.0f;
}

// Recomputes current_payload_weight from scratch by summing the weight of
// every inventory stack that is NOT currently equipped/worn. Equipped gear
// (active weapon, worn armor/shield) is carried on the body and never counts
// against the pack's carry limit - only loose inventory items do. Call this
// any time player_inventory, quantities, or equip state changes instead of
// manually incrementing/decrementing the total.
void RecalculateCarriedWeight(void) {
    float total = 0.0f;
    for (int i = 0; i < player_item_count; i++) {
        if (!player_inventory[i].is_equipped) {
            total += player_inventory[i].weight * (float)player_inventory[i].quantity;
        }
    }
    current_payload_weight = total;
}

int AddInventoryItem(const char *name, const char *slot, int quantity) {
    if (player_item_count >= MAX_INVENTORY_ITEMS) return 0;
    player_inventory[player_item_count].id = player_item_count + 1;
    strncpy(player_inventory[player_item_count].name, name, 31);
    player_inventory[player_item_count].name[31] = '\0';
    strncpy(player_inventory[player_item_count].slot, slot, 31);
    player_inventory[player_item_count].slot[31] = '\0';
    player_inventory[player_item_count].is_equipped = 0;
    player_inventory[player_item_count].quantity = quantity;
    player_inventory[player_item_count].weight = GetNamedItemWeight(name);
    player_inventory[player_item_count].durability_current = 0;
    player_inventory[player_item_count].durability_max = 0;
    player_inventory[player_item_count].damage = 0;
    player_item_count++;
    return 1;
}

float CarryLimit(void) {
    return 100.0f;
}

// Returns the inventory slot index owning a stack named `name`, or -1 if the
// player doesn't currently have any.
int FindInventoryItemIndex(const char *name) {
    for (int i = 0; i < player_item_count; i++) {
        if (strcmp(player_inventory[i].name, name) == 0) return i;
    }
    return -1;
}

// Raw gathered quantity (e.g. Wood/Stone) currently sitting in the pack.
int GetMaterialCount(const char *name) {
    int idx = FindInventoryItemIndex(name);
    return idx == -1 ? 0 : player_inventory[idx].quantity;
}

// Adds gathered material (Wood/Stone) straight into the weighted inventory,
// stacking onto an existing slot when one already exists so Wood/Stone show
// up, carry weight, and can be sold or dropped exactly like any other item.
void AddMaterialToInventory(const char *name, int quantity) {
    if (quantity <= 0) return;
    int idx = FindInventoryItemIndex(name);
    if (idx != -1) {
        player_inventory[idx].quantity += quantity;
    } else if (!AddInventoryItem(name, "Material", quantity)) {
        return;
    }
    RecalculateCarriedWeight();
}

// Removes up to `quantity` of a named material, deleting the slot entirely
// once it reaches zero. Returns 1 on success, 0 if not enough is owned.
int ConsumeMaterialFromInventory(const char *name, int quantity) {
    int idx = FindInventoryItemIndex(name);
    if (idx == -1 || player_inventory[idx].quantity < quantity) return 0;
    player_inventory[idx].quantity -= quantity;
    if (player_inventory[idx].quantity <= 0) {
        for (int i = idx; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
        player_item_count--;
        if (selected_inv_index >= player_item_count && player_item_count > 0) selected_inv_index = player_item_count - 1;
    }
    RecalculateCarriedWeight();
    return 1;
}

float LootWeight(int item_id) {
    if (item_id >= LOOT_SHORT_SWORD && item_id <= LOOT_MUSASHI_BLADE) return weapon_catalog[item_id - 1].weight;
    return (item_id == LOOT_FISH || item_id == LOOT_APPLE) ? GetNamedItemWeight(item_id == LOOT_FISH ? "Fish" : "Apple") : 0.0f;
}

int AddLootToInventory(int item_id, int quantity) {
    const char *name;
    const char *slot;
    WeaponStats *weapon = NULL;
    if (item_id >= LOOT_SHORT_SWORD && item_id <= LOOT_MUSASHI_BLADE) {
        weapon = &weapon_catalog[item_id - 1];
        name = weapon->name; slot = item_id == LOOT_SHORT_BOW ? "Weapon" : "Weapon";
    } else if (item_id == LOOT_FISH) { name = "Fish"; slot = "Food"; }
    else if (item_id == LOOT_APPLE) { name = "Apple"; slot = "Food"; }
    else return 0;

    float weight = LootWeight(item_id) * quantity;
    if (current_payload_weight + weight > CarryLimit() || player_item_count >= MAX_INVENTORY_ITEMS) return 0;
    if (!AddInventoryItem(name, slot, quantity)) return 0;
    InventoryItem *item = &player_inventory[player_item_count - 1];
    item->weight = LootWeight(item_id);
    if (weapon) {
        item->damage = weapon->damage;
        item->durability_current = item->durability_max = weapon->durability_max;
    }
    RecalculateCarriedWeight();
    return 1;
}

void DropGroundLoot(float x, float y, int item_id, int quantity) {
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        if (!ground_loot[i].active) {
            ground_loot[i].x = x; ground_loot[i].y = y;
            ground_loot[i].item_id = item_id; ground_loot[i].quantity = quantity;
            ground_loot[i].screen_id = current_screen_index; ground_loot[i].active = 1;
            return;
        }
    }
}

void TryPickupGroundLoot(void) {
    for (int i = 0; i < MAX_GROUND_LOOT; i++) {
        GroundLoot *loot = &ground_loot[i];
        if (!loot->active || loot->screen_id != current_screen_index || !IsNearPoint(loot->x, loot->y, 0.7f)) continue;
        if (AddLootToInventory(loot->item_id, loot->quantity)) {
            sprintf(arpg_action_log, "PICKUP: %s x%d", loot->item_id == LOOT_FISH ? "Fish" :
                loot->item_id == LOOT_APPLE ? "Apple" : weapon_catalog[loot->item_id - 1].name, loot->quantity);
            loot->active = 0;
        } else {
            sprintf(arpg_action_log, "Inventory full! (%.1f/%.1f weight)", current_payload_weight, CarryLimit());
        }
    }
}

// Removes the currently-selected inventory item (its whole stack) and places
// it on the ground just in front of the hero, where it can be walked back
// over and picked up again later. This is the new "drop item" action.
void DropSelectedItem(void) {
    if (player_item_count == 0) { strcpy(arpg_action_log, "No item selected to drop."); return; }
    InventoryItem *item = &player_inventory[selected_inv_index];
    if (strcmp(item->name, "Musashi's Blade") == 0) {
        strcpy(arpg_action_log, "INVENTORY: This blade cannot be sold, traded, or dropped."); return;
    }

    int slot = -1;
    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) if (!dropped_items[i].active) { slot = i; break; }
    if (slot == -1) { strcpy(arpg_action_log, "Too much clutter nearby to drop anything else."); return; }

    float dx = 0.0f, dy = 0.0f;
    if (player_facing == FACE_UP) dy = -1.0f;
    else if (player_facing == FACE_DOWN) dy = 1.0f;
    else if (player_facing == FACE_LEFT) dx = -1.0f;
    else dx = 1.0f;

    dropped_items[slot].x = player_x + dx;
    dropped_items[slot].y = player_y + dy;
    dropped_items[slot].screen_id = current_screen_index;
    dropped_items[slot].item = *item;
    dropped_items[slot].active = 1;

    int was_equipped = item->is_equipped;
    char dropped_name[32]; strcpy(dropped_name, item->name);
    int dropped_qty = item->quantity;

    for (int i = selected_inv_index; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
    player_item_count--;
    if (selected_inv_index >= player_item_count && player_item_count > 0) selected_inv_index = player_item_count - 1;
    RecalculateCarriedWeight();
    sprintf(arpg_action_log, "DROPPED: %s x%d", dropped_name, dropped_qty);
    if (was_equipped) SyncActiveWeapon();
}

void TryPickupDroppedItems(void) {
    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) {
        DroppedInventoryItem *drop = &dropped_items[i];
        if (!drop->active || drop->screen_id != current_screen_index || !IsNearPoint(drop->x, drop->y, 0.7f)) continue;
        if (player_item_count >= MAX_INVENTORY_ITEMS || current_payload_weight + drop->item.weight * drop->item.quantity > CarryLimit()) {
            sprintf(arpg_action_log, "Inventory full! (%.1f/%.1f weight)", current_payload_weight, CarryLimit());
            continue;
        }
        drop->item.is_equipped = 0;
        player_inventory[player_item_count] = drop->item;
        player_inventory[player_item_count].id = player_item_count + 1;
        player_item_count++;
        RecalculateCarriedWeight();
        sprintf(arpg_action_log, "PICKUP: %s x%d", drop->item.name, drop->item.quantity);
        drop->active = 0;
    }
}

int HasInventoryItem(const char *name) {
    for (int i = 0; i < player_item_count; i++) if (player_inventory[i].quantity > 0 && strcmp(player_inventory[i].name, name) == 0) return 1;
    return 0;
}

void ConsumeSelectedFood(void) {
    if (player_item_count <= 0) return;
    InventoryItem *item = &player_inventory[selected_inv_index];
    int heal = strcmp(item->name, "Fish") == 0 ? 10 : strcmp(item->name, "Apple") == 0 ? 15 : 0;
    if (heal == 0) { strcpy(arpg_action_log, "Use a fish or apple to restore health."); return; }
    player_hp += (float)heal;
    if (player_hp > max_player_hp) player_hp = max_player_hp;
    if (item->quantity > 1) item->quantity--;
    else {
        for (int i = selected_inv_index; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
        player_item_count--;
        if (selected_inv_index >= player_item_count && player_item_count > 0) selected_inv_index = player_item_count - 1;
    }
    RecalculateCarriedWeight();
    strcpy(arpg_action_log, "Food eaten; health restored.");
}
