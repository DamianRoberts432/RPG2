#include "game.h"

// Partially restocks Rare/Legendary merchant goods after each day/night
// cycle (sleeping) or whenever the market screen is (re)generated; Common/
// Uncommon goods with a max_stock always top back up since they're not
// scarce, while Rare/Legendary items only have a chance to add a single
// unit, keeping them scarce even after restocking.
void RestockMerchant(void) {
    for (int i = 0; i < MERCH_ITEMS; i++) {
        MerchantItem *offer = &merchant_catalog[i];
        if (offer->max_stock <= 0) continue; // unlimited-stock generic goods
        if (offer->rarity <= RARITY_UNCOMMON) {
            offer->stock = offer->max_stock;
        } else if (offer->stock < offer->max_stock && (rand() % 100) < 35) {
            offer->stock++;
        }
    }
}

// Each CHR point spent takes 3% off merchant prices.
int ShopPrice(int base) {
    int price = base * (100 - 3 * stat_spent[6]) / 100;
    return price < 1 ? 1 : price;
}

void HandleVendorBuy(void) {
    if (!IsNearMerchant()) { strcpy(arpg_action_log, "MARKET: No vendor nearby."); return; }
    MerchantItem *offer = &merchant_catalog[merchant_selection];
    int price = ShopPrice(offer->base_price * (int)offer->rarity);
    if (offer->requires_treasure_hunt && cave_treasure_hunts_found <= 0) {
        strcpy(arpg_action_log, "MARKET: This treasure-hunt item is still a rumor to this merchant."); return;
    }
    if (offer->stock == 0 || gold_count < price) {
        strcpy(arpg_action_log, "MARKET: Out of stock or not enough gold."); return;
    }

    int added = 0;
    if (offer->loot_id > 0) {
        added = AddLootToInventory(offer->loot_id, 1);
    } else {
        float w = GetNamedItemWeight(offer->name);
        if (current_payload_weight + w <= CarryLimit() && (IsStackableItem(offer->name, offer->slot) ? AddStackableItem(offer->name, offer->slot, 1) : AddInventoryItem(offer->name, offer->slot, 1))) {
            RecalculateCarriedWeight(); added = 1;
        }
    }
    if (!added) { strcpy(arpg_action_log, "MARKET: Inventory full."); return; }
    gold_count -= price;
    if (offer->stock > 0) offer->stock--;
    sprintf(arpg_action_log, "MARKET: Bought %s for %d gold.", offer->name, price);
}

// Zero tolerance: any attempt to steal from a shop fails. The guards take a
// quarter of the hero's gold and lock them up until dawn.
void HandleVendorSteal(void) {
    if (!IsNearMerchant() || sleep_fade_frame > 0) return;
    // The Strategy Guide can only be browsed or bought, never stolen.
    if (vendor_tab == 0 && strcmp(merchant_catalog[merchant_selection].name, "Strategy Guide") == 0) {
        guide_flash_until = GetTickCount() + GUIDE_FLASH_MS;
        strcpy(arpg_action_log, "You browse the A&D Strategy Guide. Buy it to keep it.");
        return;
    }
    int fine = gold_count / 4;
    gold_count -= fine;
    vendor_menu_open = 0;
    KarmaAdjust(-10, 0, 25, 0);
    jailed_tonight = 1;
    sleep_fade_frame = 1;
    sprintf(arpg_action_log, "STRAIGHT TO JAIL! The guards seize %d gold. You spend the night in a cell.", fine);
}

// Blacksmith: restores the equipped melee weapon to full durability for 1 gold per point.
void RepairEquippedWeapon(void) {
    for (int i = 0; i < player_item_count; i++) {
        InventoryItem *it = &player_inventory[i];
        if (!it->is_equipped || strcmp(it->slot, "Weapon") != 0 || it->durability_max <= 0) continue;
        int missing = it->durability_max - it->durability_current;
        if (missing <= 0) { sprintf(arpg_action_log, "BLACKSMITH: Your %s is in perfect shape.", it->name); return; }
        int cost = ShopPrice(missing);
        if (gold_count < cost) { sprintf(arpg_action_log, "BLACKSMITH: Repairing that costs %d gold.", cost); return; }
        gold_count -= cost; it->durability_current = it->durability_max;
        sprintf(arpg_action_log, "BLACKSMITH: %s repaired for %d gold.", it->name, cost);
        return;
    }
    strcpy(arpg_action_log, "BLACKSMITH: Equip a weapon and I'll mend it.");
}

// Card shop: a 3-card booster pack of random cards for 20,000 gold.
void BuyCardBooster(void) {
    if (gold_count < CARD_BOOSTER_PRICE) { sprintf(arpg_action_log, "CARD SHOP: A booster costs %d gold.", CARD_BOOSTER_PRICE); return; }
    gold_count -= CARD_BOOSTER_PRICE;
    int fresh = 0;
    for (int k = 0; k < 3; k++) {
        int id = rand() % CARD_COUNT;
        if (!HasCard(id)) fresh++;
        card_collection_bits[id / 8] |= (uint8_t)(1u << (id % 8));
    }
    sprintf(arpg_action_log, "CARD SHOP: Opened a booster - %d new card%s! (%d / %d)", fresh, fresh == 1 ? "" : "s", CardCount(), CARD_COUNT);
}

void HandleVendorSell(void) {
    if (!IsNearMerchant() || player_item_count == 0) { strcpy(arpg_action_log, "MARKET: No vendor or item available."); return; }
    InventoryItem *item = &player_inventory[selected_inv_index];
    // Treasure-hunt legendary rewards are explicitly excluded from trade.
    if (strcmp(item->name, "Musashi's Blade") == 0) {
        strcpy(arpg_action_log, "MARKET: This blade cannot be sold, traded, or dropped."); return;
    }
    int price = 2;
    if (strcmp(item->name, "Wood") == 0) price = 1;
    else if (strcmp(item->name, "Stone") == 0) price = 2;
    else if (strcmp(item->name, "Fish") == 0) price = 5;
    else if (strcmp(item->name, "Apple") == 0) price = 2;
    else if (strcmp(item->name, "Fishing Pole") == 0) price = 7;
    else {
        for (int i = 0; i < WEAPON_CATALOG_COUNT; i++) if (strcmp(item->name, weapon_catalog[i].name) == 0) price = weapon_catalog[i].price / 2;
    }
    gold_count += price * item->quantity;
    sprintf(arpg_action_log, "MARKET: Sold %s for %d gold.", item->name, price * item->quantity);
    for (int i = selected_inv_index; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
    player_item_count--;
    if (selected_inv_index >= player_item_count && player_item_count > 0) selected_inv_index = player_item_count - 1;
    RecalculateCarriedWeight();
}
