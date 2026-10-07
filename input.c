#include "game.h"

// 1 while any key, mouse button, gamepad button, trigger or stick is in use.
int AnyPlayerInput(void) {
    for (int vk = 0x01; vk <= 0xFE; vk++) if (GetAsyncKeyState(vk) & 0x8000) return 1;
    XINPUT_STATE s; SecureZeroMemory(&s, sizeof(s));
    if (XInputGetState(0, &s) == ERROR_SUCCESS) {
        XINPUT_GAMEPAD *g = &s.Gamepad;
        if (g->wButtons || g->bLeftTrigger > 30 || g->bRightTrigger > 30) return 1;
        int dl = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, dr = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
        if (g->sThumbLX > dl || g->sThumbLX < -dl || g->sThumbLY > dl || g->sThumbLY < -dl) return 1;
        if (g->sThumbRX > dr || g->sThumbRX < -dr || g->sThumbRY > dr || g->sThumbRY < -dr) return 1;
    }
    return 0;
}

void ProcessGamepadInput(void) {
    if (sleep_fade_frame > 0) return;
    XINPUT_STATE state; SecureZeroMemory(&state, sizeof(XINPUT_STATE));
    float dx = 0.0f, dy = 0.0f;
    
    if (XInputGetState(0, &state) == ERROR_SUCCESS) {
        WORD buttons = state.Gamepad.wButtons;
        if (is_character_creation) {
            static DWORD last_c_tick = 0;
            if (GetTickCount() - last_c_tick > 200) {
                if (buttons & XINPUT_GAMEPAD_DPAD_UP) { selected_creation_field = (selected_creation_field - 1 + 3) % 3; last_c_tick = GetTickCount(); }
                if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) { selected_creation_field = (selected_creation_field + 1) % 3; last_c_tick = GetTickCount(); }
                if (buttons & XINPUT_GAMEPAD_DPAD_LEFT || buttons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                    int dir = (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : -1;
                    if (selected_creation_field == 0) creation_name_letter_cursor = (creation_name_letter_cursor + dir + 26) % 26;
                    if (selected_creation_field == 1) selected_class = (ClassType)((selected_class + dir + 5) % 5);
                    if (selected_creation_field == 2) selected_race = (RaceType)((selected_race + dir + 5) % 5);
                    ApplyClassAndRaceStats(); last_c_tick = GetTickCount();
                }
                if (selected_creation_field == 0) {
                    // Gamepad name entry: cycle a letter with DPAD L/R, then
                    // press A to spell it into the name; X backspaces; Y adds
                    // a space. This replaces the previously non-functional
                    // gamepad name entry (A no longer finalizes creation here).
                    int len = (int)strlen(player_name);
                    if ((buttons & XINPUT_GAMEPAD_A) && len < (int)sizeof(player_name) - 1) {
                        player_name[len] = (char)('A' + creation_name_letter_cursor);
                        player_name[len + 1] = '\0';
                        last_c_tick = GetTickCount();
                    } else if ((buttons & XINPUT_GAMEPAD_X) && len > 0) {
                        player_name[len - 1] = '\0';
                        last_c_tick = GetTickCount();
                    } else if ((buttons & XINPUT_GAMEPAD_Y) && len < (int)sizeof(player_name) - 1) {
                        player_name[len] = ' '; player_name[len + 1] = '\0';
                        last_c_tick = GetTickCount();
                    }
                } else if (buttons & XINPUT_GAMEPAD_A) {
                    if (IsNameValid()) {
                        is_character_creation = 0;
                        creation_error_msg[0] = '\0';
                        sprintf(arpg_action_log, "FORGED: Engine live! Playing as %s %s", race_names[selected_race], class_names[selected_class]);
                    } else {
                        strcpy(creation_error_msg, "Enter a name before starting!");
                    }
                    last_c_tick = GetTickCount();
                }
            } 
            return;
        }
        
        static DWORD last_tab_gp = 0;
        if (is_menu_open && (GetTickCount() - last_tab_gp > 200)) {
            if (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) { current_menu_tab = (current_menu_tab - 1 + 3) % 3; last_tab_gp = GetTickCount(); }
            if (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) { current_menu_tab = (current_menu_tab + 1) % 3; last_tab_gp = GetTickCount(); }
            
            if (current_menu_tab == 0 && player_item_count > 0) {
                if (buttons & XINPUT_GAMEPAD_DPAD_UP) { selected_inv_index = (selected_inv_index - 1 + player_item_count) % player_item_count; last_tab_gp = GetTickCount(); }
                if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) { selected_inv_index = (selected_inv_index + 1) % player_item_count; last_tab_gp = GetTickCount(); }
                if (buttons & XINPUT_GAMEPAD_A) { HandleInventoryPrimaryAction(); last_tab_gp = GetTickCount(); }
                if (buttons & XINPUT_GAMEPAD_X) { DropSelectedItem(); last_tab_gp = GetTickCount(); }
            }
        }

        static DWORD last_menu_gp = 0;
        if ((buttons & XINPUT_GAMEPAD_START) && (GetTickCount() - last_menu_gp > 300)) { is_menu_open = !is_menu_open; last_menu_gp = GetTickCount(); }
        if (!is_menu_open) {
            // "Y" is the class special ability trigger (gameplay-only). Edge
            // detection ensures it fires exactly once per press, and the
            // cooldown inside FireClassAbility() prevents spamming further.
            int y_down_gp = (buttons & XINPUT_GAMEPAD_Y) != 0;
            if (y_down_gp && !y_button_was_down) {
                FireClassAbility();
            }
            y_button_was_down = y_down_gp;

            int shopping = vendor_menu_open && IsNearMerchant();
            if (!shopping) {
                if (buttons & XINPUT_GAMEPAD_DPAD_LEFT)  { dx = -1.0f; player_facing = FACE_LEFT; }
                if (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) { dx = 1.0f; player_facing = FACE_RIGHT; }
                if (buttons & XINPUT_GAMEPAD_DPAD_UP)    { dy = -1.0f; player_facing = FACE_UP; }
                if (buttons & XINPUT_GAMEPAD_DPAD_DOWN)  { dy = 1.0f; player_facing = FACE_DOWN; }
            }

            static DWORD last_interact_gp = 0;
            if ((buttons & XINPUT_GAMEPAD_X) && (GetTickCount() - last_interact_gp > 350)) {
                HandleContextInteract();
                last_interact_gp = GetTickCount();
            }

            // Shop UI: LB switches to the Buy tab (merchant's items for sale),
            // RB switches to the Sell tab (player's inventory). DPAD up/down
            // cycles the highlighted row within whichever tab is active, A
            // confirms the buy/sell, and B closes the shop.
            static DWORD last_vendor_gp = 0;
            if (shopping && (GetTickCount() - last_vendor_gp > 200)) {
                if (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER)  { vendor_tab = 0; last_vendor_gp = GetTickCount(); }
                if (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) { vendor_tab = 1; last_vendor_gp = GetTickCount(); }
                if (vendor_tab == 0) {
                    if (buttons & XINPUT_GAMEPAD_DPAD_UP)   { merchant_selection = (merchant_selection + MERCH_ITEMS - 1) % MERCH_ITEMS; last_vendor_gp = GetTickCount(); }
                    if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) { merchant_selection = (merchant_selection + 1) % MERCH_ITEMS; last_vendor_gp = GetTickCount(); }
                } else if (player_item_count > 0) {
                    if (buttons & XINPUT_GAMEPAD_DPAD_UP)   { selected_inv_index = (selected_inv_index + player_item_count - 1) % player_item_count; last_vendor_gp = GetTickCount(); }
                    if (buttons & XINPUT_GAMEPAD_DPAD_DOWN) { selected_inv_index = (selected_inv_index + 1) % player_item_count; last_vendor_gp = GetTickCount(); }
                }
                if (buttons & XINPUT_GAMEPAD_B) { vendor_menu_open = 0; last_vendor_gp = GetTickCount(); }
            }

            static DWORD last_attack_gp = 0;
            WeaponStats *held_weapon = GetEquippedWeaponStats();
            DWORD attack_delay_gp = held_weapon && held_weapon->attack_speed_mult < 1.0f ?
                (DWORD)(350.0f / held_weapon->attack_speed_mult) : 350;
            if ((buttons & XINPUT_GAMEPAD_A) && (GetTickCount() - last_attack_gp > attack_delay_gp)) {
                if (shopping) {
                    if (vendor_tab == 0) HandleVendorBuy(); else HandleVendorSell();
                } else if (active_weapon != WEAPON_BOW) {
                    PerformMeleeAttack();
                }
                last_attack_gp = GetTickCount();
            } else if ((buttons & XINPUT_GAMEPAD_B) && !shopping && (GetTickCount() - last_attack_gp > attack_delay_gp)) {
                if (active_weapon != WEAPON_BOW) PerformMeleeAttack();
                last_attack_gp = GetTickCount();
            }
        }

        static DWORD bow_charge_start_gp = 0;
        BYTE rt = state.Gamepad.bRightTrigger;
        if (bow_equipped && rt > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
            if (!is_charging_bow) {
                is_charging_bow = 1;
                bow_charge_start_gp = GetTickCount();
                bow_charge_started_at = bow_charge_start_gp;
                bow_charge_time = 0.0f;
            }
            bow_charge_time += 0.05f;
            if (bow_charge_time > 1.0f) bow_charge_time = 1.0f;
        } else if (is_charging_bow && bow_charge_start_gp != 0) {
            DWORD hold_duration = GetTickCount() - bow_charge_start_gp;
            int is_tap = (hold_duration < 150 || bow_charge_time < 0.1f);
            if (!player_arrow.active) FirePlayerArrow(is_tap);
            is_charging_bow = 0; bow_charge_time = 0.0f; bow_charge_start_gp = 0;
        }
    }

    if (is_character_creation) {
        static DWORD last_kb_c = 0;
        if (GetTickCount() - last_kb_c > 200) {
            if (GetAsyncKeyState(VK_UP) & 0x8000) { selected_creation_field = (selected_creation_field - 1 + 3) % 3; last_kb_c = GetTickCount(); }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) { selected_creation_field = (selected_creation_field + 1) % 3; last_kb_c = GetTickCount(); }
            if (selected_creation_field != 0 && ((GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000))) {
                int dir = (GetAsyncKeyState(VK_RIGHT) & 0x8000) ? 1 : -1;
                if (selected_creation_field == 1) selected_class = (ClassType)((selected_class + dir + 5) % 5);
                if (selected_creation_field == 2) selected_race = (RaceType)((selected_race + dir + 5) % 5);
                ApplyClassAndRaceStats(); last_kb_c = GetTickCount();
            }
            // SPACE finalizes creation from the Class/Race fields (legacy
            // shortcut), but while the Name field is selected it is treated
            // as a typed space character instead (see typing block below).
            if (selected_creation_field != 0 && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                is_character_creation = 0; ApplyClassAndRaceStats();
                sprintf(arpg_action_log, "FORGED: Engine live! Playing as %s %s", race_names[selected_race], class_names[selected_class]);
            }
        }

        // ENTER always finalizes creation, regardless of which field is
        // selected, so a typed name can be confirmed without needing to
        // first tab away from the Name field.
        static int enter_was_down = 0;
        int enter_down = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
        if (enter_down && !enter_was_down) {
            is_character_creation = 0; ApplyClassAndRaceStats();
            sprintf(arpg_action_log, "FORGED: Engine live! Playing as %s %s", race_names[selected_race], class_names[selected_class]);
        }
        enter_was_down = enter_down;

        // Live text entry for the Hero Name field. Edge-triggered per key (not
        // gated by the 200ms cursor-navigation cooldown above) so typing feels
        // responsive. This is the fix for the previously-nonfunctional
        // "add name to character" feature: selecting the Name field used to
        // highlight it but no key ever actually wrote into player_name.
        if (selected_creation_field == 0) {
            static int key_was_down[256] = {0};
            int len = (int)strlen(player_name);
            for (int k = 'A'; k <= 'Z'; k++) {
                int down = (GetAsyncKeyState(k) & 0x8000) != 0;
                if (down && !key_was_down[k] && len < (int)sizeof(player_name) - 1) {
                    int shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    player_name[len] = (char)(shift_down ? k : (k + 32));
                    player_name[len + 1] = '\0';
                    len++;
                }
                key_was_down[k] = down;
            }
            int space_down = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (space_down && !key_was_down[VK_SPACE] && len < (int)sizeof(player_name) - 1) {
                player_name[len] = ' '; player_name[len + 1] = '\0';
            }
            key_was_down[VK_SPACE] = space_down;
            int back_down = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;
            if (back_down && !key_was_down[VK_BACK] && len > 0) {
                player_name[len - 1] = '\0';
            }
            key_was_down[VK_BACK] = back_down;
        }
        return;
    }

    static DWORD last_tab_kb = 0;
    if (is_menu_open && (GetTickCount() - last_tab_kb > 200)) {
        if (GetAsyncKeyState('Q') & 0x8000) { current_menu_tab = (current_menu_tab - 1 + 3) % 3; last_tab_kb = GetTickCount(); }
        if (GetAsyncKeyState('E') & 0x8000) { current_menu_tab = (current_menu_tab + 1) % 3; last_tab_kb = GetTickCount(); }
        
        if (current_menu_tab == 0 && player_item_count > 0) {
            if (GetAsyncKeyState(VK_UP) & 0x8000) { selected_inv_index = (selected_inv_index - 1 + player_item_count) % player_item_count; last_tab_kb = GetTickCount(); }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) { selected_inv_index = (selected_inv_index + 1) % player_item_count; last_tab_kb = GetTickCount(); }
            if (GetAsyncKeyState('1') & 0x8000) { HandleInventoryPrimaryAction(); last_tab_kb = GetTickCount(); }
            if (GetAsyncKeyState('3') & 0x8000) { HandleMenuLightFire(); last_tab_kb = GetTickCount(); }
            if (GetAsyncKeyState('4') & 0x8000) { DropSelectedItem(); last_tab_kb = GetTickCount(); }
        } else if (current_menu_tab == 2) {
            if (GetAsyncKeyState('1') & 0x8000) { opt_xp_mult += 0.5f; if (opt_xp_mult > 3.0f) opt_xp_mult = 0.5f; last_tab_kb = GetTickCount(); }
            if (GetAsyncKeyState('2') & 0x8000) { opt_loot_mult += 0.5f; if (opt_loot_mult > 3.0f) opt_loot_mult = 0.5f; last_tab_kb = GetTickCount(); }
            if (GetAsyncKeyState('3') & 0x8000) { opt_day_night_speed += 0.5f; if (opt_day_night_speed > 3.0f) opt_day_night_speed = 0.5f; last_tab_kb = GetTickCount(); }
        }
    }

    static DWORD last_menu_kb = 0;
    if ((GetAsyncKeyState('M') || GetAsyncKeyState('I')) && (GetTickCount() - last_menu_kb > 300)) { is_menu_open = !is_menu_open; last_menu_kb = GetTickCount(); }

    if (!is_menu_open) {
        // Keyboard "Y" mirrors the gamepad class special, also edge-triggered.
        int y_down_kb = (GetAsyncKeyState('Y') & 0x8000) != 0;
        if (y_down_kb && !y_key_was_down) {
            FireClassAbility();
        }
        y_key_was_down = y_down_kb;

        static DWORD last_vendor_kb = 0;
        int shopping_kb = vendor_menu_open && IsNearMerchant();
        if (shopping_kb && GetTickCount() - last_vendor_kb > 220) {
            // Shop UI: Q switches to the Buy tab (merchant's items for sale),
            // E switches to the Sell tab (player's inventory). Bracket keys
            // cycle the highlighted row within the active tab, SPACE/Z
            // confirms, and ESCAPE closes the shop.
            if (GetAsyncKeyState('Q') & 0x8000) { vendor_tab = 0; last_vendor_kb = GetTickCount(); }
            if (GetAsyncKeyState('E') & 0x8000) { vendor_tab = 1; last_vendor_kb = GetTickCount(); }
            if (vendor_tab == 0) {
                if ((GetAsyncKeyState(VK_OEM_4) | GetAsyncKeyState(VK_UP)) & 0x8000) { merchant_selection = (merchant_selection + MERCH_ITEMS - 1) % MERCH_ITEMS; last_vendor_kb = GetTickCount(); }
                if ((GetAsyncKeyState(VK_OEM_6) | GetAsyncKeyState(VK_DOWN)) & 0x8000) { merchant_selection = (merchant_selection + 1) % MERCH_ITEMS; last_vendor_kb = GetTickCount(); }
            } else if (player_item_count > 0) {
                if ((GetAsyncKeyState(VK_OEM_4) | GetAsyncKeyState(VK_UP)) & 0x8000) { selected_inv_index = (selected_inv_index + player_item_count - 1) % player_item_count; last_vendor_kb = GetTickCount(); }
                if ((GetAsyncKeyState(VK_OEM_6) | GetAsyncKeyState(VK_DOWN)) & 0x8000) { selected_inv_index = (selected_inv_index + 1) % player_item_count; last_vendor_kb = GetTickCount(); }
            }
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { vendor_menu_open = 0; last_vendor_kb = GetTickCount(); }
        }

        static DWORD last_eat = 0;
        if ((GetAsyncKeyState('H') & 0x8000) && player_item_count > 0 && GetTickCount() - last_eat > 250) {
            ConsumeSelectedFood(); last_eat = GetTickCount();
        }

        if (!shopping_kb) {
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)  { dx = -1.0f; player_facing = FACE_LEFT; }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { dx = 1.0f; player_facing = FACE_RIGHT; }
            if (GetAsyncKeyState(VK_UP) & 0x8000)    { dy = -1.0f; player_facing = FACE_UP; }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)  { dy = 1.0f; player_facing = FACE_DOWN; }
        }

        static DWORD last_interact_kb = 0;
        if (((GetAsyncKeyState('X') | GetAsyncKeyState('C')) & 0x8000) && (GetTickCount() - last_interact_kb > 350)) {
            HandleContextInteract();
            last_interact_kb = GetTickCount();
        }

        static DWORD last_attack_kb = 0;
        WeaponStats *held_weapon = GetEquippedWeaponStats();
        DWORD attack_delay_kb = held_weapon && held_weapon->attack_speed_mult < 1.0f ?
            (DWORD)(350.0f / held_weapon->attack_speed_mult) : 350;
        if (((GetAsyncKeyState(VK_SPACE) | GetAsyncKeyState('Z')) & 0x8000) && (GetTickCount() - last_attack_kb > attack_delay_kb)) {
            if (shopping_kb) {
                if (vendor_tab == 0) HandleVendorBuy(); else HandleVendorSell();
            } else if (active_weapon != WEAPON_BOW) {
                PerformMeleeAttack();
            }
            last_attack_kb = GetTickCount();
        }

        static DWORD bow_charge_start_kb = 0;
        int bow_key_down = ((GetAsyncKeyState('F') | GetAsyncKeyState('R')) & 0x8000) != 0;
        if (bow_equipped && bow_key_down) {
            if (!is_charging_bow) {
                is_charging_bow = 1;
                bow_charge_start_kb = GetTickCount();
                bow_charge_started_at = bow_charge_start_kb;
                bow_charge_time = 0.0f;
            }
            bow_charge_time += 0.05f;
            if (bow_charge_time > 1.0f) bow_charge_time = 1.0f;
        } else if (is_charging_bow && bow_charge_start_kb != 0) {
            DWORD hold_duration = GetTickCount() - bow_charge_start_kb;
            int is_tap = (hold_duration < 150 || bow_charge_time < 0.1f);
            if (!player_arrow.active) FirePlayerArrow(is_tap);
            is_charging_bow = 0; bow_charge_time = 0.0f; bow_charge_start_kb = 0;
        }

        static int was_g_down = 0, was_fishing_held = 0;
        int g_down = (GetAsyncKeyState('G') & 0x8000) != 0;
        if (g_down && !was_g_down && HasInventoryItem("Fishing Pole")) {
            if (IsFacingWater()) {
                fishing_active = 1; fishing_power = 0; was_fishing_held = 0;
                strcpy(arpg_action_log, "FISHING: Hold F, then release between 50-80%!");
            } else strcpy(arpg_action_log, "FISHING: Face a water tile to cast.");
        }
        was_g_down = g_down;
        if (fishing_active && active_weapon != WEAPON_BOW) {
            int f_down = (GetAsyncKeyState('F') & 0x8000) != 0;
            if (f_down) {
                fishing_power += 2; if (fishing_power > 100) fishing_power = 100;
                was_fishing_held = 1;
            } else if (was_fishing_held) {
                if (fishing_power >= 50 && fishing_power <= 80) {
                    if (!IsFacingFishWater()) strcpy(arpg_action_log, "FISHING: No fish in this water.");
                    else {
                        int count = 1 + rand() % 3;
                        if (AddLootToInventory(LOOT_FISH, count)) sprintf(arpg_action_log, "FISHING: Caught %d fish!", count);
                        else strcpy(arpg_action_log, "FISHING: Catch lost; inventory is too heavy.");
                    }
                } else strcpy(arpg_action_log, "FISHING: The fish got away.");
                fishing_active = 0; was_fishing_held = 0;
            }
        }

        if (dx != 0.0f || dy != 0.0f) {
            is_running = 1; run_bob += 0.4f;
            float race_mult = GetRaceSpeedMultiplier();
            float speed = 0.135f * race_mult * WeightSpeedFactor(); if (speed < 0.01f) speed = 0.0f;
            if (speed <= 0.0f) strcpy(arpg_action_log, "OVERLOADED: Too heavy to move! Drop something.");
            if (player_stamina <= 0.0f) {
                speed *= 0.4f; // exhausted: can still shuffle along, just much slower
                strcpy(arpg_action_log, "Exhausted! Rest to recover stamina.");
            } else {
                player_stamina -= 0.15f; // running slowly drains stamina
                if (player_stamina < 0.0f) player_stamina = 0.0f;
            }
            if (IsOnSlipperyGround()) {
                // On ice the hero speeds up and turns gradually; UpdateIceSlide
                // carries the momentum every frame, so they glide to a stop.
                ice_vx += (dx * speed - ice_vx) * 0.08f;
                ice_vy += (dy * speed - ice_vy) * 0.08f;
            } else {
                float tx = player_x + dx * speed, ty = player_y + dy * speed;
                float nx, ny;
                if (PlayerTryMove(tx, ty, &nx, &ny)) { player_x = nx; player_y = ny; }
            }
        } else is_running = 0;
    }
}
