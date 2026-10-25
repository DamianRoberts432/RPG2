#include "game.h"

// Returns a cached, lazily-created underlined font used for menu section
// headers and labels so menu text reads as clearly organized instead of
// jumbling together with the surrounding plain-text rows.
HFONT GetMenuUnderlineFont(void) {
    static HFONT underline_font = NULL;
    if (!underline_font) {
        underline_font = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, TRUE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    }
    return underline_font;
}

// Draws a labeled row of up to 10 small unfilled jewel (diamond) shapes for a
// character stat, filling the first `value` (capped at 10) of them in green.
// Replaces the old plain "STR: 14" text readout with an at-a-glance gauge.
void DrawStatJewels(HDC hdc, int x, int y, const char *label, int value) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(200, 210, 220));
    TextOut(hdc, x, y, label, (int)strlen(label));

    int filled = value;
    if (filled > 10) filled = 10;
    if (filled < 0) filled = 0;

    int jewel_x = x + 42;
    HPEN outline_p = CreatePen(PS_SOLID, 1, RGB(140, 150, 160));
    HGDIOBJ old_p = SelectObject(hdc, outline_p);
    HBRUSH empty_b = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH filled_b = CreateSolidBrush(RGB(45, 210, 90));

    for (int i = 0; i < 10; i++) {
        int cx = jewel_x + i * 11;
        int cy = y + 6;
        POINT jewel[] = { {cx, cy - 5}, {cx + 5, cy}, {cx, cy + 5}, {cx - 5, cy} };
        SelectObject(hdc, (i < filled) ? filled_b : empty_b);
        Polygon(hdc, jewel, 4);
    }

    SelectObject(hdc, old_p); DeleteObject(outline_p); DeleteObject(filled_b);
}

// Lower-left gameplay HUD: red HP, blue MP and green stamina bars (stamina
// shifts to yellow at 50% and red at 20%), an "Exhausted" message once stamina hits
// zero, and a campfire burning/low status line when one is active nearby.
void DrawPlayerStatusHUD(HDC hdc) {
    int bar_x = 20, bar_w = 160, bar_h = 12;
    int hp_y = WINDOW_HEIGHT - 120;
    int mp_y = WINDOW_HEIGHT - 84;
    int sta_y = WINDOW_HEIGHT - 48;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, bar_x, hp_y - 14, "HP", 2);
    TextOut(hdc, bar_x, mp_y - 14, "MP", 2);
    TextOut(hdc, bar_x, sta_y - 14, "STA", 3);

    RECT hp_bg = { bar_x, hp_y, bar_x + bar_w, hp_y + bar_h };
    HBRUSH bg_b = CreateSolidBrush(RGB(40, 15, 15)); FillRect(hdc, &hp_bg, bg_b); DeleteObject(bg_b);
    float hp_pct = max_player_hp > 0.0f ? player_hp / max_player_hp : 0.0f;
    RECT hp_fill = { bar_x, hp_y, bar_x + (int)(bar_w * hp_pct), hp_y + bar_h };
    HBRUSH hp_b = CreateSolidBrush(RGB(210, 45, 60)); FillRect(hdc, &hp_fill, hp_b); DeleteObject(hp_b);

    RECT mp_bg = { bar_x, mp_y, bar_x + bar_w, mp_y + bar_h };
    HBRUSH mp_bg_b = CreateSolidBrush(RGB(15, 20, 45)); FillRect(hdc, &mp_bg, mp_bg_b); DeleteObject(mp_bg_b);
    float mp_pct = max_player_mp > 0.0f ? player_mp / max_player_mp : 0.0f;
    RECT mp_fill = { bar_x, mp_y, bar_x + (int)(bar_w * mp_pct), mp_y + bar_h };
    HBRUSH mp_b = CreateSolidBrush(RGB(60, 110, 235)); FillRect(hdc, &mp_fill, mp_b); DeleteObject(mp_b);

    RECT sta_bg = { bar_x, sta_y, bar_x + bar_w, sta_y + bar_h };
    HBRUSH sta_bg_b = CreateSolidBrush(RGB(15, 40, 20)); FillRect(hdc, &sta_bg, sta_bg_b); DeleteObject(sta_bg_b);
    float sta_pct = max_stamina > 0.0f ? player_stamina / max_stamina : 0.0f;
    COLORREF sta_color = RGB(60, 200, 80);
    if (sta_pct <= 0.20f) sta_color = RGB(220, 60, 50);
    else if (sta_pct <= 0.50f) sta_color = RGB(230, 210, 60);
    RECT sta_fill = { bar_x, sta_y, bar_x + (int)(bar_w * sta_pct), sta_y + bar_h };
    HBRUSH sta_b = CreateSolidBrush(sta_color); FillRect(hdc, &sta_fill, sta_b); DeleteObject(sta_b);

    if (player_stamina <= 0.0f || HeroExhausted()) {
        SetTextColor(hdc, RGB(230, 70, 60));
        TextOut(hdc, bar_x, sta_y + bar_h + 2, "Exhausted!", 10);
    }

    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        SetTextColor(hdc, campfires[i].timer > 400.0f ? RGB(255, 150, 60) : RGB(255, 230, 120));
        char status[48];
        sprintf(status, "Fire: %s (%d/%d wood)", campfires[i].timer > 400.0f ? "BURNING" : "LOW", CampfireWoodCount(i), CAMPFIRE_MAX_WOOD);
        TextOut(hdc, bar_x, sta_y + bar_h + 16, status, (int)strlen(status));
        break;
    }

    char line[96];
    int underground = current_biome == BIOME_CAVE;
    int hour, minute;
    GetGameClock(&hour, &minute);
    SetTextColor(hdc, RGB(230, 235, 240));
    sprintf(line, "%s, Day %d of %d  |  %s  |  %d:%02d %s", season_names[current_season], SeasonDay(), DAYS_PER_SEASON,
            CalendarMonthName(), (hour + 11) % 12 + 1, minute, hour < 12 ? "AM" : "PM");
    TextOut(hdc, 20, HUD_DATE_Y, line, (int)strlen(line));
    sprintf(line, "Weather: %s%s", underground ? "Underground" : weather_names[current_weather],
            (!underground && fabs(wind_gust) > 0.3f) ? "  |  Windy" : "");
    TextOut(hdc, 20, HUD_DATE_Y + 20, line, (int)strlen(line));
    sprintf(line, "Region: %s%s%s", biome_names[current_biome], stat_points > 0 ? "  |  Attribute points to spend!" : "", IsOutlaw() ? "  |  WANTED" : "");
    TextOut(hdc, 20, HUD_DATE_Y + 40, line, (int)strlen(line));
    DrawTrackedQuest(hdc, 20, HUD_DATE_Y + 60);
}

// Clear Buy/Sell shop UI: a boxed panel with an underlined tab header (shown
// with a highlight on whichever tab is active) and a scrollable list of
// rows - merchant items with price/stock on the Buy tab, or owned items with
// their sell price on the Sell tab - so it's always obvious what is being
// bought or sold, instead of the old single confusing status line.
static WeaponStats *FindWeaponStats(const char *name) {
    for (int i = 0; i < WEAPON_CATALOG_COUNT; i++) if (strcmp(weapon_catalog[i].name, name) == 0) return &weapon_catalog[i];
    if (strcmp(name, "Iron Sword") == 0) return &weapon_catalog[0];
    if (strcmp(name, "Silver Dagger") == 0) return &weapon_catalog[LOOT_DAGGER - 1];
    return NULL;
}

static const char *ItemBlurb(const char *name) {
    if (strcmp(name, "Strategy Guide") == 0) return "Secret card combos. Owning it opens the CARDS tab in your menu for good.";
    if (strcmp(name, "Matches") == 0) return "Light a campfire from the inventory (needs 2 Wood).";
    if (strcmp(name, "Fish") == 0 || strcmp(name, "Apple") == 0 || strcmp(name, "Turnip") == 0) return "Eat to restore health.";
    if (strcmp(name, "Hoe") == 0) return "X on bare grass tills a farm plot.";
    if (strcmp(name, "Wood Cart") == 0) return "Wood and stone weigh a quarter as much; villagers ask for bigger hauls.";
    if (strcmp(name, "House Supplies") == 0) return "X on open ground plans your house, X again changes the front.";
    for (int k = 0; k < FURNITURE_KINDS; k++) if (strcmp(name, furniture_names[k]) == 0) return "Furniture: goes into your house when you go inside.";
    if (strcmp(name, "Seeds") == 0) return "X on tilled soil plants turnips; ripe in 3 days.";
    if (strcmp(name, "Pickaxe") == 0) return "Mine rocks with X.";
    if (strcmp(name, "Axe") == 0) return "Chop trees with X; also a heavy weapon.";
    if (strcmp(name, "Fishing Pole") == 0) return "G to cast at water, hold F and release at 50-80%.";
    if (strcmp(name, "Spear") == 0) return "Long thrust; RT/F throws it.";
    if (strcmp(name, "Night Vision Ring") == 0) return "Worn: the night looks much brighter.";
    if (strcmp(name, "Peyote") == 0) return "Eat it and the colours melt for 30 seconds.";
    if (strcmp(name, "Garlic") == 0) return "Carried: Vampires won't come for you.";
    if (strcmp(name, "Silver Dagger") == 0) return "Silver: double damage to Vampires and Werewolves.";
    if (strcmp(name, "Canoe") == 0 || strcmp(name, "Raft") == 0 || strcmp(name, "Boat") == 0) return "Carry it to paddle across water.";
    if (strcmp(name, "Treasure Map") == 0) return "Red X's on the World Map mark where your chests are.";
    return "";
}

// Shop "item card": stats of the highlighted row, with a frame on the far
// right reserved for the item's picture.
static void DrawShopItemCard(HDC hdc) {
    const char *name, *slot, *rarity = ""; int price = -1, qty = 0, dur = 0, dur_max = 0;
    if (vendor_tab == 0) {
        MerchantItem *o = &merchant_catalog[merchant_selection];
        if ((o->requires_treasure_hunt && cave_treasure_hunts_found <= 0) || MerchSeasonLocked(o)) return;
        name = o->name; slot = o->slot; price = ItemShopPrice(o);
        rarity = o->rarity == RARITY_LEGENDARY ? "Legendary" : o->rarity == RARITY_RARE ? "Rare" : o->rarity == RARITY_UNCOMMON ? "Uncommon" : "Common";
    } else {
        if (player_item_count == 0) return;
        InventoryItem *it = &player_inventory[selected_inv_index];
        name = it->name; slot = it->slot; qty = it->quantity; dur = it->durability_current; dur_max = it->durability_max;
    }
    WeaponStats *w = FindWeaponStats(name);
    RECT card = { 660, 90, WINDOW_WIDTH - 60, 420 };
    HBRUSH bg = CreateSolidBrush(RGB(26, 24, 20)); FillRect(hdc, &card, bg); DeleteObject(bg);
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(200, 170, 90)); HGDIOBJ old_p = SelectObject(hdc, pen);
    HGDIOBJ old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, card.left, card.top, card.right, card.bottom);
    HPEN dash = CreatePen(PS_DOT, 1, RGB(150, 140, 120)); SelectObject(hdc, dash);
    Rectangle(hdc, WINDOW_WIDTH - 320, 110, WINDOW_WIDTH - 80, 400);
    SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(pen); DeleteObject(dash);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(150, 140, 120));
    TextOut(hdc, WINDOW_WIDTH - 236, 248, "(item art)", 10);
    char line[128]; int y = 106;
    SetTextColor(hdc, RGB(255, 215, 0)); TextOut(hdc, 680, y, name, (int)strlen(name)); y += 26;
    SetTextColor(hdc, RGB(225, 225, 225));
    sprintf(line, "Type: %s%s%s", slot, rarity[0] ? "   Rarity: " : "", rarity); TextOut(hdc, 680, y, line, (int)strlen(line)); y += 20;
    sprintf(line, "Weight: %.1f", w ? w->weight : GetNamedItemWeight(name)); TextOut(hdc, 680, y, line, (int)strlen(line)); y += 20;
    if (w) {
        sprintf(line, "Damage: %d   Durability: %d / %d", w->damage, dur_max ? dur : w->durability_max, dur_max ? dur_max : w->durability_max);
        TextOut(hdc, 680, y, line, (int)strlen(line)); y += 20;
        if (strstr(name, "Bow")) strcpy(line, "Ranged: hold RT / F to draw (drains stamina)");
        else sprintf(line, "Stamina per swing: %.0f   Speed: %s", MELEE_SWING_STAMINA + 2.0f * w->stamina_cost, w->attack_speed_mult < 1.0f ? "Slow" : "Normal");
        TextOut(hdc, 680, y, line, (int)strlen(line)); y += 20;
    }
    if (price >= 0) { sprintf(line, "Price: %d gold", price); TextOut(hdc, 680, y, line, (int)strlen(line)); y += 20; }
    if (qty) { sprintf(line, "You own: %d", qty); TextOut(hdc, 680, y, line, (int)strlen(line)); y += 20; }
    const char *blurb = ItemBlurb(name);
    SetTextColor(hdc, RGB(170, 200, 230)); TextOut(hdc, 680, y + 10, blurb, (int)strlen(blurb));
}

void DrawVendorShopOverlay(HDC hdc) {
    DrawInteriorBackdrop(hdc, TM_NONE);
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, 40, WINDOW_HEIGHT - 70, arpg_action_log, (int)strlen(arpg_action_log));
    RECT box = { 160, 90, 640, 420 };
    HBRUSH bg = CreateSolidBrush(RGB(18, 20, 28)); FillRect(hdc, &box, bg); DeleteObject(bg);
    HPEN border_p = CreatePen(PS_SOLID, 2, RGB(90, 200, 170)); HGDIOBJ old_p = SelectObject(hdc, border_p);
    HBRUSH null_b = (HBRUSH)GetStockObject(NULL_BRUSH); HGDIOBJ old_b = SelectObject(hdc, null_b);
    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
    SelectObject(hdc, old_p); SelectObject(hdc, old_b); DeleteObject(border_p);

    SetBkMode(hdc, TRANSPARENT);
    HFONT underline_font = GetMenuUnderlineFont();
    HGDIOBJ prev_font = SelectObject(hdc, underline_font);
    SetTextColor(hdc, (vendor_tab == 0) ? RGB(255, 235, 100) : RGB(170, 180, 190));
    TextOut(hdc, box.left + 20, box.top + 12, "[ LB ] BUY", 10);
    SetTextColor(hdc, (vendor_tab == 1) ? RGB(255, 235, 100) : RGB(170, 180, 190));
    TextOut(hdc, box.left + 160, box.top + 12, "[ RB ] SELL", 11);
    SelectObject(hdc, prev_font);

    SetTextColor(hdc, RGB(230, 230, 230));
    char gold_line[48]; sprintf(gold_line, "Gold: %d", gold_count); TextOut(hdc, box.right - 110, box.top + 14, gold_line, (int)strlen(gold_line));

    int row_y = box.top + 46;
    char row[160];
    if (vendor_tab == 0) {
        // Rare+Legendary catalog can exceed the panel's visible rows, so
        // scroll a small window centered on the current selection instead of
        // letting it overflow the box.
        int visible_rows = 10;
        int start = merchant_selection - visible_rows / 2;
        if (start < 0) start = 0;
        if (start > MERCH_ITEMS - visible_rows) start = MERCH_ITEMS - visible_rows;
        if (start < 0) start = 0;
        int end = start + visible_rows; if (end > MERCH_ITEMS) end = MERCH_ITEMS;
        for (int i = start; i < end; i++) {
            MerchantItem *offer = &merchant_catalog[i];
            const char *rarity_tag = offer->rarity == RARITY_LEGENDARY ? "Legendary" :
                offer->rarity == RARITY_RARE ? "Rare" : offer->rarity == RARITY_UNCOMMON ? "Uncommon" : "Common";
            int price = ItemShopPrice(offer);
            int locked = (offer->requires_treasure_hunt && cave_treasure_hunts_found <= 0) || MerchSeasonLocked(offer);
            SetTextColor(hdc, (i == merchant_selection) ? RGB(255, 255, 0) : RGB(210, 210, 210));
            char stock_str[16];
            if (locked) strcpy(stock_str, "???");
            else if (offer->stock < 0) strcpy(stock_str, "many");
            else if (offer->stock > 0) sprintf(stock_str, "%d", offer->stock);
            else strcpy(stock_str, "OUT");
            sprintf(row, "%s %-20s [%-9s] %5d gold  stock: %s", (i == merchant_selection) ? ">" : " ",
                locked ? "???" : offer->name, rarity_tag, price, stock_str);
            TextOut(hdc, box.left + 20, row_y, row, (int)strlen(row));
            row_y += 20;
        }
        SetTextColor(hdc, RGB(150, 160, 170));
        if (start > 0) TextOut(hdc, box.right - 70, box.top + 46, "^ more", 6);
        if (end < MERCH_ITEMS) TextOut(hdc, box.right - 70, box.top + 46 + (visible_rows - 1) * 20, "v more", 6);
        SetTextColor(hdc, RGB(150, 220, 255));
        TextOut(hdc, box.left + 20, box.bottom - 30, "[ SPACE/A ] Browse / Buy", 24);
    } else {
        if (player_item_count == 0) {
            SetTextColor(hdc, RGB(200, 200, 200));
            TextOut(hdc, box.left + 20, row_y, "(Your inventory is empty.)", 27);
        }
        if (selected_inv_index >= player_item_count) selected_inv_index = player_item_count > 0 ? player_item_count - 1 : 0;
        int visible_rows = 10;
        int start = selected_inv_index - visible_rows / 2;
        if (start > player_item_count - visible_rows) start = player_item_count - visible_rows;
        if (start < 0) start = 0;
        int end = start + visible_rows; if (end > player_item_count) end = player_item_count;
        for (int i = start; i < end; i++) {
            InventoryItem *item = &player_inventory[i];
            int price = 2;
            if (strcmp(item->name, "Wood") == 0) price = 1;
            else if (strcmp(item->name, "Stone") == 0) price = 2;
            else if (strcmp(item->name, "Fish") == 0) price = 5;
            else if (strcmp(item->name, "Apple") == 0) price = 2;
            else if (strcmp(item->name, "Fishing Pole") == 0) price = 7;
            else for (int w = 0; w < WEAPON_CATALOG_COUNT; w++) if (strcmp(item->name, weapon_catalog[w].name) == 0) price = weapon_catalog[w].price / 2;
            SetTextColor(hdc, (i == selected_inv_index) ? RGB(255, 255, 0) : RGB(210, 210, 210));
            if (strcmp(item->name, "Musashi's Blade") == 0) {
                sprintf(row, "%s %-16s x%-3d (cannot be sold)", (i == selected_inv_index) ? ">" : " ", item->name, item->quantity);
            } else {
                sprintf(row, "%s %-16s x%-3d sells for %4d gold", (i == selected_inv_index) ? ">" : " ",
                    item->name, item->quantity, price * item->quantity);
            }
            TextOut(hdc, box.left + 20, row_y, row, (int)strlen(row));
            row_y += 20;
        }
        SetTextColor(hdc, RGB(150, 160, 170));
        if (start > 0) TextOut(hdc, box.right - 70, box.top + 46, "^ more", 6);
        if (end < player_item_count) TextOut(hdc, box.right - 70, box.top + 46 + (visible_rows - 1) * 20, "v more", 6);
        SetTextColor(hdc, RGB(150, 220, 255));
        TextOut(hdc, box.left + 20, box.bottom - 30, "[ SPACE/A ] Sell selected item", 31);
    }
    SetTextColor(hdc, RGB(150, 220, 255));
    TextOut(hdc, box.left + 320, box.bottom - 30, "[ ESC/B ] Close shop", 20);
    if (vendor_tab == 0 && shop_action_menu) {
        RECT menu = { box.left + 300, box.top + 130, box.left + 460, box.top + 230 };
        HBRUSH mb = CreateSolidBrush(RGB(30, 34, 46)); FillRect(hdc, &menu, mb); DeleteObject(mb);
        HPEN mp = CreatePen(PS_SOLID, 2, RGB(255, 235, 100)); HGDIOBJ op = SelectObject(hdc, mp);
        HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, menu.left, menu.top, menu.right, menu.bottom);
        SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(mp);
        static const char *opts[3] = { "Browse", "Buy", "Cancel" };
        for (int i = 0; i < 3; i++) {
            char opt[32]; sprintf(opt, "%s %s", i == shop_action_cursor ? ">" : " ", opts[i]);
            SetTextColor(hdc, i == shop_action_cursor ? RGB(255, 255, 0) : RGB(210, 210, 210));
            TextOut(hdc, menu.left + 20, menu.top + 14 + i * 26, opt, (int)strlen(opt));
        }
    }
    SetTextColor(hdc, RGB(150, 160, 170));
    TextOut(hdc, box.left + 20, box.bottom - 52, "UP/DOWN or [ ] / D-PAD: scroll", 30);
    DrawShopItemCard(hdc);
    DrawStrategyGuideFlash(hdc);
}

// Two-second peek at the secret card combos after reading the Strategy Guide.
void DrawStrategyGuideFlash(HDC hdc) {
    if ((long)(guide_flash_until - GetTickCount()) <= 0) return;
    RECT box = { WINDOW_WIDTH / 2 - 330, WINDOW_HEIGHT / 2 - 110, WINDOW_WIDTH / 2 + 330, WINDOW_HEIGHT / 2 + 110 };
    HBRUSH bg = CreateSolidBrush(RGB(40, 30, 20)); FillRect(hdc, &box, bg); DeleteObject(bg);
    HPEN border = CreatePen(PS_SOLID, 3, RGB(220, 180, 90)); HGDIOBJ old_p = SelectObject(hdc, border);
    HGDIOBJ old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
    SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(border);
    SetBkMode(hdc, TRANSPARENT);
    static const char *lines[] = {
        "CARD GAME STRATEGY GUIDE - SECRET CARD COMBOS",
        "",
        "Skeleton + Zombie + Vampire   ->  NIGHT TERROR",
        "Yeti + Bigfoot + Satyr        ->  WILD HUNT",
        "Sphinx + Bunyip + Goblin      ->  RIDDLE OF THE DEEP",
    };
    for (int i = 0; i < 5; i++) {
        SetTextColor(hdc, i == 0 ? RGB(255, 215, 0) : RGB(235, 225, 205));
        TextOut(hdc, box.left + 30, box.top + 24 + i * 24, lines[i], (int)strlen(lines[i]));
    }
    char buf[64];
    sprintf(buf, "Your binder: %d / %d cards", CardCount(), CARD_POOL);
    SetTextColor(hdc, RGB(150, 220, 255));
    TextOut(hdc, box.left + 30, box.bottom - 40, buf, (int)strlen(buf));
}

void DrawTabbedMenuOverlay(HDC hdc) {
    HBRUSH bg = CreateSolidBrush(RGB(22, 25, 32));
    RECT menu_box = { 60, 40, WINDOW_WIDTH - 60, WINDOW_HEIGHT - 60 };
    FillRect(hdc, &menu_box, bg); DeleteObject(bg);

    HPEN border_p = CreatePen(PS_SOLID, 2, RGB(0, 230, 200)); HGDIOBJ old_p = SelectObject(hdc, border_p);
    HGDIOBJ old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 60, 40, WINDOW_WIDTH - 60, WINDOW_HEIGHT - 60);

    const char* tab_titles[] = { "[ LB ] HERO & INVENTORY", "[ 2 ] WORLD MAP", "[ RB ] OPTIONS", "QUESTS", "CARDS" };
    if (current_menu_tab == MENU_TAB_CARDS && !CardGuideUnlocked()) current_menu_tab = 0;
    HFONT underline_font = GetMenuUnderlineFont();
    for (int t = 0; t < (CardGuideUnlocked() ? 5 : 4); t++) {
        RECT tab_r = { 70 + t * 220, 50, 280 + t * 220, 80 };
        HBRUSH tab_b = CreateSolidBrush((t == current_menu_tab) ? RGB(45, 80, 120) : RGB(30, 35, 45));
        FillRect(hdc, &tab_r, tab_b); DeleteObject(tab_b);
        SetTextColor(hdc, (t == current_menu_tab) ? RGB(255, 255, 255) : RGB(140, 150, 160));
        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ prev_font = SelectObject(hdc, underline_font);
        DrawText(hdc, tab_titles[t], -1, &tab_r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        SelectObject(hdc, prev_font);
    }

    SetTextColor(hdc, RGB(255, 255, 255));
    char buf[256];

    if (current_menu_tab == 0) { 
        DrawPanel(hdc, 70, 84, 800, 292); DrawPanel(hdc, 810, 84, 1310, 322); DrawPanel(hdc, 70, 296, 800, 550);
        DrawHeroAssetEx(hdc, 150, 200, 2.0f);

        const int info_x = 260;
        sprintf(buf, "NAME: %.18s", player_name); TextOut(hdc, info_x, 92, buf, (int)strlen(buf));
        sprintf(buf, "RACE: %-9s CLASS: %s", race_names[selected_race], class_names[selected_class]); TextOut(hdc, info_x, 108, buf, (int)strlen(buf));
        sprintf(buf, "LEVEL: %d (XP: %d/%d)   HEIGHT: %.2fm", player_level, player_xp, player_next_level_xp, player_height_m); TextOut(hdc, info_x, 124, buf, (int)strlen(buf));

        HGDIOBJ prev_font = SelectObject(hdc, underline_font);
        SetTextColor(hdc, RGB(0, 230, 200));
        TextOut(hdc, info_x, 142, "ATTRIBUTES", 10);
        SelectObject(hdc, prev_font);

        DrawStatJewels(hdc, info_x, 160, "STR", stat_strength);
        DrawStatJewels(hdc, info_x, 174, "DEX", stat_dexterity);
        DrawStatJewels(hdc, info_x, 188, "VIT", stat_stamina);
        DrawStatJewels(hdc, info_x, 202, "MAG", stat_magick);
        DrawStatJewels(hdc, info_x, 216, "LCK", stat_luck);
        DrawStatJewels(hdc, info_x, 230, "INT", stat_intelligence);
        DrawStatJewels(hdc, info_x, 244, "CHR", stat_charisma);
        if (stat_points > 0 || stat_assign_mode) {
            SetTextColor(hdc, RGB(255, 215, 0));
            if (stat_assign_mode) sprintf(buf, "POINTS: %d   D-pad/arrows pick, A/Enter add, Y/P done", stat_points);
            else sprintf(buf, "POINTS: %d   press Y / P to assign", stat_points);
            TextOut(hdc, info_x + 110, 142, buf, (int)strlen(buf));
        }
        if (stat_assign_mode) {
            SetTextColor(hdc, RGB(255, 215, 0));
            TextOut(hdc, info_x - 14, 160 + 14 * stat_cursor, ">", 1);
        }

        SetTextColor(hdc, RGB(255, 215, 0));
        sprintf(buf, "LUMBER: %d  STONE: %d  GOLD: %d", GetMaterialCount("Wood"), GetMaterialCount("Stone"), gold_count);
        TextOut(hdc, info_x, 264, buf, (int)strlen(buf));
        sprintf(buf, "WEIGHT: %.1f / %.1f", current_payload_weight, CarryLimit()); TextOut(hdc, info_x, 280, buf, (int)strlen(buf));

        const int lore_x = 820;
        prev_font = SelectObject(hdc, underline_font);
        SetTextColor(hdc, RGB(0, 230, 200));
        TextOut(hdc, lore_x, 92, "KARMA", 5);
        SelectObject(hdc, prev_font);
        SetTextColor(hdc, RGB(220, 220, 220));
        sprintf(buf, "Soul %+d   Miasma %d   Town Heat %d   Grapevine %d", karma.soul, karma.miasma, karma.town_heat, karma.grapevine);
        TextOut(hdc, lore_x, 110, buf, (int)strlen(buf));
        strcpy(buf, "Life marks:");
        for (int m = 0; m < 16; m++) if (IS_BOSS_MONSTER(m) && (karma.life_marks & (1u << m))) { strcat(buf, " "); strcat(buf, MonsterName((MonsterType)m)); }
        if (karma.life_marks == 0) strcat(buf, " none yet (slay a boss)");
        TextOut(hdc, lore_x, 126, buf, (int)strlen(buf));
        SetTextColor(hdc, RGB(255, 215, 0));
        sprintf(buf, "CARDS: %d / %d collected", CardCount(), CARD_POOL);
        TextOut(hdc, lore_x, 146, buf, (int)strlen(buf));
        prev_font = SelectObject(hdc, underline_font);
        SetTextColor(hdc, RGB(0, 230, 200));
        TextOut(hdc, lore_x, 166, "BESTIARY (kills)", 16);
        SelectObject(hdc, prev_font);
        for (int m = 0; m < MONSTER_TYPE_COUNT; m++) {
            SetTextColor(hdc, HasCard(monster_card_links[m].card_id) ? RGB(255, 215, 0) : RGB(200, 200, 200));
            sprintf(buf, "%-8s %5u%s", MonsterName((MonsterType)m), (unsigned)monster_card_links[m].kills,
                    HasCard(monster_card_links[m].card_id) ? "  [card]" : "");
            TextOut(hdc, lore_x, 184 + m * 14, buf, (int)strlen(buf));
        }

        prev_font = SelectObject(hdc, underline_font);
        SetTextColor(hdc, RGB(0, 230, 200));
        sprintf(buf, "INVENTORY (%d/%d)", player_item_count, MAX_INVENTORY_ITEMS);
        TextOut(hdc, 80, 300, buf, (int)strlen(buf));
        SelectObject(hdc, prev_font);

        SetTextColor(hdc, RGB(180, 190, 200));
        TextOut(hdc, 80, 318, "ID", 2);
        TextOut(hdc, 122, 318, "Name", 4);
        TextOut(hdc, 380, 318, "Type", 4);
        TextOut(hdc, 500, 318, "Qty", 3);
        TextOut(hdc, 560, 318, "Wt", 2);
        TextOut(hdc, 620, 318, "State", 5);

        // Only a handful of rows fit on screen at once now that inventory
        // space goes up to 100 slots, so scroll a small window around the
        // current selection instead of overflowing past the menu box.
        const int visible_rows = 10;
        int list_start = selected_inv_index - visible_rows / 2;
        if (list_start > player_item_count - visible_rows) list_start = player_item_count - visible_rows;
        if (list_start < 0) list_start = 0;
        int list_end = list_start + visible_rows;
        if (list_end > player_item_count) list_end = player_item_count;

        for (int i = list_start; i < list_end; i++) {
            InventoryItem *item = &player_inventory[i];
            int row = i - list_start;
            if (i == selected_inv_index) SetTextColor(hdc, RGB(255, 255, 0));
            else SetTextColor(hdc, RGB(220, 220, 220));

            if (i == selected_inv_index && item->durability_max > 0) {
                sprintf(buf, "Durability: %d/%d", item->durability_current, item->durability_max);
                TextOut(hdc, 430, 300, buf, (int)strlen(buf));
            }
            // Name field is a fixed 30 characters (dot padded, "..." when
            // truncated) and every other column sits at a fixed pixel offset
            // so nothing shifts with name length.
            char name_field[32];
            int name_len = (int)strlen(item->name);
            if (name_len > 30) {
                memcpy(name_field, item->name, 27);
                strcpy(name_field + 27, "...");
            } else {
                memset(name_field, '.', 30);
                memcpy(name_field, item->name, name_len);
                name_field[30] = '\0';
            }
            int row_y = 336 + (row * 17);
            sprintf(buf, "[%2d]", i + 1); TextOut(hdc, 80, row_y, buf, (int)strlen(buf));
            TextOut(hdc, 122, row_y, name_field, (int)strlen(name_field));
            sprintf(buf, "[%s]", item->slot); TextOut(hdc, 380, row_y, buf, (int)strlen(buf));
            sprintf(buf, "x%d", item->quantity); TextOut(hdc, 500, row_y, buf, (int)strlen(buf));
            sprintf(buf, "%.1f", item->weight); TextOut(hdc, 560, row_y, buf, (int)strlen(buf));
            const char *state = item->is_equipped ? "[EQUIPPED]" : "";
            TextOut(hdc, 620, row_y, state, (int)strlen(state));
        }

        SetTextColor(hdc, RGB(255, 180, 60));
        const char *actions = "ACTIONS: [1] Equip  [3] Campfire  [4] Drop  [5] Sort  [H] Eat";
        TextOut(hdc, 80, 515, actions, (int)strlen(actions));
        SetTextColor(hdc, RGB(160, 170, 180));
        TextOut(hdc, 80, 531, "Use UP/DOWN ARROWS or D-PAD to select inventory items.", 54);

    } else if (current_menu_tab == 1) { 
        const char *title = "WORLD MAP - every screen you have explored (saved automatically)";
        TextOut(hdc, 90, 100, title, (int)strlen(title));
        sprintf(buf, "Location: (%d, %d)   Biome: %s   |   %d screens to town   |   Zoom x%d (LT/RT or -/+)", screen_grid_x, screen_grid_y,
                biome_names[current_biome], screens_until_town, world_map_zoom);
        TextOut(hdc, 90, 122, buf, (int)strlen(buf));
        DrawWorldMapPanel(hdc, 100, 160, 1200, WINDOW_HEIGHT - 280);
        SetTextColor(hdc, RGB(200, 210, 220));
        sprintf(buf, "Season: %s (day %d of %d)   Weather: %s%s   Start: %s", season_names[current_season],
                SeasonDay(), DAYS_PER_SEASON, weather_names[current_weather],
                fabs(wind_gust) > 0.3f ? ", windy" : "", weather_source_label);
        TextOut(hdc, 90, WINDOW_HEIGHT - 95, buf, (int)strlen(buf));
    } else if (current_menu_tab == MENU_TAB_QUESTS) {
        int mid = 90 + (WINDOW_WIDTH - 240) / 2;
        DrawPanel(hdc, 80, 150, mid - 14, WINDOW_HEIGHT - 112);
        DrawPanel(hdc, mid - 8, 150, WINDOW_WIDTH - 136, WINDOW_HEIGHT - 384);
        DrawPanel(hdc, mid - 8, WINDOW_HEIGHT - 378, WINDOW_WIDTH - 136, WINDOW_HEIGHT - 112);
        DrawQuestJournal(hdc, 90, 100, WINDOW_WIDTH - 240, WINDOW_HEIGHT - 220);
        DrawMsgHistory(hdc, 90 + (WINDOW_WIDTH - 240) / 2, WINDOW_HEIGHT - 370, (WINDOW_WIDTH - 240) / 2, 11);
    } else if (current_menu_tab == MENU_TAB_CARDS) {
        DrawPanel(hdc, 80, 126, 90 + (WINDOW_WIDTH - 240) / 2 - 14, WINDOW_HEIGHT - 112);
        DrawPanel(hdc, 90 + (WINDOW_WIDTH - 240) / 2 - 8, 126, WINDOW_WIDTH - 136, 420);
        DrawCardInspector(hdc, 90, 100, WINDOW_WIDTH - 240, WINDOW_HEIGHT - 220);
    } else { 
        DrawPanel(hdc, 80, 92, 900, 312);
        TextOut(hdc, 90, 100, "GAME VARIABLE OPTIONS:", 22);
        sprintf(buf, "[1] XP Multiplier: %.1fx   (Modify with Left/Right Arrow)", opt_xp_mult); TextOut(hdc, 100, 140, buf, (int)strlen(buf));
        sprintf(buf, "[2] Loot Multiplier: %.1fx", opt_loot_mult); TextOut(hdc, 100, 180, buf, (int)strlen(buf));
        sprintf(buf, "[3] Day/Night Cycle Speed: %.1fx", opt_day_night_speed); TextOut(hdc, 100, 220, buf, (int)strlen(buf));
        SetTextColor(hdc, RGB(255, 120, 100));
        TextOut(hdc, 100, 280, "[9] / Back: Delete save file (press twice to confirm)", 53);
        SetTextColor(hdc, RGB(255, 255, 255));
    }

    SelectObject(hdc, old_p); DeleteObject(border_p); SelectObject(hdc, old_b);
}

// Standing on a drop shows what it holds in a see-through box, colored by
// rarity: grey common, blue forged, green uncommon, yellow rare, purple legendary.
static COLORREF LootRarityColor(const char *name, int legendary, int forged) {
    if (legendary || strcmp(name, "Musashi's Blade") == 0) return RGB(190, 110, 255);
    if (forged) return RGB(90, 160, 255);
    if (!strcmp(name, "Katana") || !strcmp(name, "War Hammer") || !strcmp(name, "Great Axe")) return RGB(255, 235, 60);
    for (int i = 0; i < MERCH_ITEMS; i++) {
        if (strcmp(merchant_catalog[i].name, name) != 0) continue;
        ItemRarity r = merchant_catalog[i].rarity;
        return r >= RARITY_LEGENDARY ? RGB(190, 110, 255) : r >= RARITY_RARE ? RGB(255, 235, 60) :
               r >= RARITY_UNCOMMON ? RGB(90, 210, 110) : RGB(175, 175, 175);
    }
    return RGB(175, 175, 175);
}

void DrawLootPreview(HDC hdc) {
    char lines[8][64]; COLORREF cols[8]; int n = 0;
    for (int i = 0; i < MAX_GROUND_LOOT && n < 8; i++) {
        GroundLoot *g = &ground_loot[i];
        if (!g->active || g->screen_id != current_screen_index || !IsNearPoint(g->x, g->y, 1.2f)) continue;
        const char *nm = g->item_id >= LOOT_SHORT_SWORD && g->item_id <= LOOT_LAST_WEAPON ? weapon_catalog[g->item_id - 1].name :
                         g->item_id == LOOT_FISH ? "Fish" : g->item_id == LOOT_APPLE ? "Apple" : g->item_id == LOOT_GOLD ? "Gold" : "Treasure";
        snprintf(lines[n], sizeof(lines[n]), "%s x%d", nm, g->quantity);
        cols[n++] = LootRarityColor(nm, g->item_id == LOOT_MUSASHI_BLADE, 0);
    }
    for (int i = 0; i < MAX_DROPPED_ITEMS && n < 8; i++) {
        DroppedInventoryItem *d = &dropped_items[i];
        if (!d->active || d->screen_id != current_screen_index || !IsNearPoint(d->x, d->y, 1.2f)) continue;
        int forged = 0;
        for (int j = 0; j < WEAPON_CATALOG_COUNT; j++)
            if (strcmp(d->item.name, weapon_catalog[j].name) == 0 && d->item.damage > weapon_catalog[j].damage) forged = 1;
        snprintf(lines[n], sizeof(lines[n]), d->item.quantity > 1 ? "%s x%d" : "%s", d->item.name, d->item.quantity);
        cols[n++] = LootRarityColor(d->item.name, d->legendary, forged);
    }
    if (n == 0) return;
    int l = WINDOW_WIDTH / 2 + 40, t = WINDOW_HEIGHT / 2 - 90, w = 260, h = 36 + n * 20 + 22;
    HDC mdc = CreateCompatibleDC(hdc);
    HBITMAP px = CreateCompatibleBitmap(hdc, 1, 1);
    HGDIOBJ ob = SelectObject(mdc, px);
    SetPixel(mdc, 0, 0, RGB(0, 0, 0));
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 153, 0 }; // 60% black, monsters still show through
    AlphaBlend(hdc, l, t, w, h, mdc, 0, 0, 1, 1, bf);
    SelectObject(mdc, ob); DeleteObject(px); DeleteDC(mdc);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 235, 235));
    TextOut(hdc, l + 12, t + 8, "On the ground:", 14);
    for (int k = 0; k < n; k++) { SetTextColor(hdc, cols[k]); TextOut(hdc, l + 18, t + 32 + k * 20, lines[k], (int)strlen(lines[k])); }
    SetTextColor(hdc, RGB(150, 150, 150));
    TextOut(hdc, l + 12, t + h - 20, "X / A to pick up", 16);
}

int MenuTabStep(int cur, int dir) {
    int n = CardGuideUnlocked() ? 5 : 4;
    return (cur + dir + n) % n;
}

// --- Message log: the upper-left lines, and the history in the QUESTS tab ---
#define MSG_LOG_MAX 40
#define LOG_SHOW_MS 9000
#define LOG_FADE_MS 3000
typedef struct { char text[256]; DWORD t; } LogLine;
static LogLine msg_log[MSG_LOG_MAX];
static int msg_log_count = 0, msg_log_head = 0;
static char msg_last_seen[256];

static void PushLog(const char *s, size_t len) {
    while (len > 0 && *s == ' ') { s++; len--; }
    while (len > 0 && s[len - 1] == ' ') len--;
    if (len == 0) return;
    if (len > 255) len = 255;
    LogLine *l = &msg_log[msg_log_head];
    memcpy(l->text, s, len); l->text[len] = 0; l->t = GetTickCount();
    msg_log_head = (msg_log_head + 1) % MSG_LOG_MAX;
    if (msg_log_count < MSG_LOG_MAX) msg_log_count++;
}
// New text in arpg_action_log becomes new lines. Messages joined with "  |  " are
// split into separate lines, and text tacked onto the end of the last message
// only logs the new part.
void MsgLogUpdate(void) {
    if (strcmp(arpg_action_log, msg_last_seen) == 0) return;
    const char *s = arpg_action_log;
    size_t old = strlen(msg_last_seen);
    if (old > 0 && strncmp(arpg_action_log, msg_last_seen, old) == 0) s += old;
    for (;;) {
        const char *bar = strstr(s, "  |  ");
        if (!bar) { PushLog(s, strlen(s)); break; }
        PushLog(s, (size_t)(bar - s));
        s = bar + 5;
    }
    snprintf(msg_last_seen, sizeof(msg_last_seen), "%s", arpg_action_log);
}
static const LogLine *LogAt(int back) { return &msg_log[(msg_log_head - 1 - back + 2 * MSG_LOG_MAX) % MSG_LOG_MAX]; }

// Upper left: up to 5 recent lines, oldest on top and newest underneath. Older
// lines fade out after a while; the newest one always stays.
void DrawMsgLog(HDC hdc, int x, int y) {
    DWORD now = GetTickCount();
    int shown = 0, idx[5];
    for (int b = 0; b < msg_log_count && shown < 5; b++) {
        if (b > 0 && now - LogAt(b)->t > LOG_SHOW_MS + LOG_FADE_MS) break;
        idx[shown++] = b;
    }
    SetBkMode(hdc, TRANSPARENT);
    for (int k = shown - 1; k >= 0; k--) {
        const LogLine *l = LogAt(idx[k]);
        DWORD age = now - l->t;
        float a = (idx[k] == 0 || age <= LOG_SHOW_MS) ? 1.0f : 1.0f - (float)(age - LOG_SHOW_MS) / LOG_FADE_MS;
        if (a < 0.0f) a = 0.0f;
        int c = (int)(70 + 185 * a);
        SetTextColor(hdc, RGB(c, c, c));
        RECT r = { x, y, x + 1100, y + 60 };
        int hgt = DrawText(hdc, l->text, -1, &r, DT_WORDBREAK | DT_LEFT | DT_NOPREFIX);
        y += hgt > 0 ? hgt + 2 : 20;
    }
}

// QUESTS tab: the last messages received, newest at the bottom.
void DrawMsgHistory(HDC hdc, int x, int y, int w, int lines) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 215, 0));
    TextOut(hdc, x, y, "RECENT MESSAGES", 15);
    int n = msg_log_count < lines ? msg_log_count : lines;
    for (int k = 0; k < n; k++) {
        const LogLine *l = LogAt(n - 1 - k);
        SetTextColor(hdc, k == n - 1 ? RGB(240, 240, 240) : RGB(165, 170, 180));
        RECT r = { x, y + 24 + k * 20, x + w, y + 44 + k * 20 };
        DrawText(hdc, l->text, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_LEFT | DT_NOPREFIX);
    }
}

// A framed box behind a section of a menu.
void DrawPanel(HDC hdc, int l, int t, int r, int b) {
    HBRUSH fill = CreateSolidBrush(RGB(24, 30, 42));
    HPEN edge = CreatePen(PS_SOLID, 1, RGB(70, 120, 160));
    HGDIOBJ ob = SelectObject(hdc, fill), op = SelectObject(hdc, edge);
    RoundRect(hdc, l, t, r, b, 14, 14);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(fill); DeleteObject(edge);
}
