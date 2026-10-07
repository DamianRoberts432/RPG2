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

    if (player_stamina <= 0.0f) {
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
    int day = (int)((SEASON_LENGTH_SECONDS - SeasonSecondsRemaining()) / (SEASON_LENGTH_SECONDS / 30.0f)) + 1;
    if (day > 30) day = 30;
    SetTextColor(hdc, RGB(230, 235, 240));
    sprintf(line, "Day %d of %s", day, season_names[current_season]);
    TextOut(hdc, 20, HUD_DATE_Y, line, (int)strlen(line));
    sprintf(line, "%s  |  %s%s", season_names[current_season], underground ? "Underground" : weather_names[current_weather],
            (!underground && fabs(wind_gust) > 0.3f) ? "  |  Windy" : "");
    TextOut(hdc, 20, HUD_DATE_Y + 20, line, (int)strlen(line));
    sprintf(line, "Region: %s", biome_names[current_biome]);
    TextOut(hdc, 20, HUD_DATE_Y + 40, line, (int)strlen(line));
}

// Clear Buy/Sell shop UI: a boxed panel with an underlined tab header (shown
// with a highlight on whichever tab is active) and a scrollable list of
// rows - merchant items with price/stock on the Buy tab, or owned items with
// their sell price on the Sell tab - so it's always obvious what is being
// bought or sold, instead of the old single confusing status line.
void DrawVendorShopOverlay(HDC hdc) {
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
            int price = offer->base_price * (int)offer->rarity;
            int locked = offer->requires_treasure_hunt && cave_treasure_hunts_found <= 0;
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
        TextOut(hdc, box.left + 20, box.bottom - 30, "[ SPACE/A ] Buy selected item", 30);
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
    SetTextColor(hdc, RGB(150, 160, 170));
    TextOut(hdc, box.left + 20, box.bottom - 52, "UP/DOWN or [ ] / D-PAD: scroll", 30);
}

void DrawTabbedMenuOverlay(HDC hdc) {
    HBRUSH bg = CreateSolidBrush(RGB(22, 25, 32));
    RECT menu_box = { 60, 40, WINDOW_WIDTH - 60, WINDOW_HEIGHT - 60 };
    FillRect(hdc, &menu_box, bg); DeleteObject(bg);

    HPEN border_p = CreatePen(PS_SOLID, 2, RGB(0, 230, 200)); HGDIOBJ old_p = SelectObject(hdc, border_p);
    HGDIOBJ old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 60, 40, WINDOW_WIDTH - 60, WINDOW_HEIGHT - 60);

    const char* tab_titles[] = { "[ LB ] HERO & INVENTORY", "[ 2 ] WORLD MAP", "[ RB ] OPTIONS" };
    HFONT underline_font = GetMenuUnderlineFont();
    for (int t = 0; t < 3; t++) {
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
        DrawStatJewels(hdc, info_x, 188, "STA", stat_stamina);
        DrawStatJewels(hdc, info_x, 202, "MAG", stat_magick);
        DrawStatJewels(hdc, info_x, 216, "LCK", stat_luck);
        DrawStatJewels(hdc, info_x, 230, "INT", stat_intelligence);
        DrawStatJewels(hdc, info_x, 244, "CHR", stat_charisma);

        SetTextColor(hdc, RGB(255, 215, 0));
        sprintf(buf, "LUMBER: %d  STONE: %d  GOLD: %d", GetMaterialCount("Wood"), GetMaterialCount("Stone"), gold_count);
        TextOut(hdc, info_x, 264, buf, (int)strlen(buf));
        sprintf(buf, "WEIGHT: %.1f / %.1f", current_payload_weight, CarryLimit()); TextOut(hdc, info_x, 280, buf, (int)strlen(buf));

        prev_font = SelectObject(hdc, underline_font);
        SetTextColor(hdc, RGB(0, 230, 200));
        sprintf(buf, "INVENTORY (%d/%d)", player_item_count, MAX_INVENTORY_ITEMS);
        TextOut(hdc, 80, 300, buf, (int)strlen(buf));
        SelectObject(hdc, prev_font);

        SetTextColor(hdc, RGB(180, 190, 200));
        TextOut(hdc, 80, 318, "ID", 2);
        TextOut(hdc, 100, 318, "Name", 4);
        TextOut(hdc, 430, 318, "Type", 4);
        TextOut(hdc, 520, 318, "Qty", 3);
        TextOut(hdc, 590, 318, "Wt", 2);
        TextOut(hdc, 650, 318, "State", 5);

        // Only a handful of rows fit on screen at once now that inventory
        // space goes up to 100 slots, so scroll a small window around the
        // current selection instead of overflowing past the menu box.
        const int visible_rows = 11;
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
            // Name field is a fixed 55 characters (dot padded, "..." when
            // truncated) and every other column sits at a fixed pixel offset
            // so nothing shifts with name length.
            char name_field[56];
            int name_len = (int)strlen(item->name);
            if (name_len > 55) {
                memcpy(name_field, item->name, 52);
                strcpy(name_field + 52, "...");
            } else {
                memset(name_field, '.', 55);
                memcpy(name_field, item->name, name_len);
                name_field[55] = '\0';
            }
            int row_y = 336 + (row * 16);
            sprintf(buf, "[%2d]", i + 1); TextOut(hdc, 80, row_y, buf, (int)strlen(buf));
            TextOut(hdc, 100, row_y, name_field, (int)strlen(name_field));
            sprintf(buf, "[%s]", item->slot); TextOut(hdc, 430, row_y, buf, (int)strlen(buf));
            sprintf(buf, "x%d", item->quantity); TextOut(hdc, 520, row_y, buf, (int)strlen(buf));
            sprintf(buf, "%.1f", item->weight); TextOut(hdc, 590, row_y, buf, (int)strlen(buf));
            const char *state = item->is_equipped ? "[EQUIPPED]" : "[UNEQUIPPED]";
            TextOut(hdc, 650, row_y, state, (int)strlen(state));
        }

        SetTextColor(hdc, RGB(255, 180, 60));
        const char *actions = "ACTIONS: [1] Equip  [3] Campfire  [4] Drop  [H] Eat";
        TextOut(hdc, 80, 515, actions, (int)strlen(actions));
        SetTextColor(hdc, RGB(160, 170, 180));
        TextOut(hdc, 80, 531, "Use UP/DOWN ARROWS or D-PAD to select inventory items.", 54);

    } else if (current_menu_tab == 1) { 
        const char *title = "WORLD MAP - every screen you have explored (saved automatically)";
        TextOut(hdc, 90, 100, title, (int)strlen(title));
        sprintf(buf, "Location: (%d, %d)   Biome: %s   |   %d screens to town", screen_grid_x, screen_grid_y, biome_names[current_biome], screens_until_town);
        TextOut(hdc, 90, 122, buf, (int)strlen(buf));
        DrawWorldMapPanel(hdc, 100, 160, 1200, WINDOW_HEIGHT - 280);
        SetTextColor(hdc, RGB(200, 210, 220));
        sprintf(buf, "Season: %s (%d min left)   Weather: %s%s   Start: %s", season_names[current_season],
                (int)(SeasonSecondsRemaining() / 60.0f) + 1, weather_names[current_weather],
                fabs(wind_gust) > 0.3f ? ", windy" : "", weather_source_label);
        TextOut(hdc, 90, WINDOW_HEIGHT - 95, buf, (int)strlen(buf));
    } else { 
        TextOut(hdc, 90, 100, "GAME VARIABLE OPTIONS:", 22);
        sprintf(buf, "[1] XP Multiplier: %.1fx   (Modify with Left/Right Arrow)", opt_xp_mult); TextOut(hdc, 100, 140, buf, (int)strlen(buf));
        sprintf(buf, "[2] Loot Multiplier: %.1fx", opt_loot_mult); TextOut(hdc, 100, 180, buf, (int)strlen(buf));
        sprintf(buf, "[3] Day/Night Cycle Speed: %.1fx", opt_day_night_speed); TextOut(hdc, 100, 220, buf, (int)strlen(buf));
    }

    SelectObject(hdc, old_p); DeleteObject(border_p); SelectObject(hdc, old_b);
}
