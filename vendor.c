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
    int price = (int)(base * (100 - 3 * stat_spent[6]) / 100 * AdvTownPriceMult());
    if (HolidayToday() == HOLIDAY_WINTER_FEAST) price = price * 85 / 100;
    return price < 1 ? 1 : price;
}

void HandleVendorBuy(void) {
    if (!IsNearMerchant()) { strcpy(arpg_action_log, "MARKET: No vendor nearby."); return; }
    MerchantItem *offer = &merchant_catalog[merchant_selection];
    int price = ItemShopPrice(offer);
    if (offer->requires_treasure_hunt && cave_treasure_hunts_found <= 0) {
        strcpy(arpg_action_log, "MARKET: This treasure-hunt item is still a rumor to this merchant."); return;
    }
    if (MerchSeasonLocked(offer)) { strcpy(arpg_action_log, "MARKET: That one only comes in season."); return; }
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

// Buy tab: A / SPACE on an item opens a small Browse / Buy / Cancel menu.
int shop_action_menu = 0, shop_action_cursor = 1;

void HandleVendorBrowse(void) {
    if (!IsNearMerchant()) return;
    MerchantItem *offer = &merchant_catalog[merchant_selection];
    if (strcmp(offer->name, "Strategy Guide") == 0) {
        guide_flash_until = GetTickCount() + GUIDE_FLASH_MS; card_guide_unlocked = 1;
        strcpy(arpg_action_log, "You browse the Card Game Strategy Guide. Buy it to keep it.");
    } else if (offer->requires_treasure_hunt && cave_treasure_hunts_found <= 0) {
        strcpy(arpg_action_log, "MARKET: This treasure-hunt item is still a rumor to this merchant.");
    } else {
        sprintf(arpg_action_log, "You look over the %s. %d gold.", offer->name, ItemShopPrice(offer));
    }
}

void ShopMoveCursor(int step) { shop_action_cursor = (shop_action_cursor + step + 3) % 3; }

void ShopConfirm(void) {
    if (!shop_action_menu) { shop_action_menu = 1; shop_action_cursor = 1; return; }
    shop_action_menu = 0;
    if (shop_action_cursor == 0) HandleVendorBrowse();
    else if (shop_action_cursor == 1) HandleVendorBuy();
}

// Blacksmith: restores the equipped melee weapon to full durability for 1 gold per point.
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
    int total = (int)(price * item->quantity * EconomySellMult(item->name) + 0.5f);
    if (total < 1) total = 1;
    gold_count += total;
    EconomyNoteSold(item->name, item->quantity);
    sprintf(arpg_action_log, "MARKET: Sold %s for %d gold.", item->name, total);
    for (int i = selected_inv_index; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
    player_item_count--;
    if (selected_inv_index >= player_item_count && player_item_count > 0) selected_inv_index = player_item_count - 1;
    RecalculateCarriedWeight();
}
