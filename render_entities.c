#include "game.h"

void DrawHeroAssetEx(HDC hdc, int render_x, int render_y, float override_scale) {
    int sx = render_x; int sy = render_y;
    float scale = override_scale;
    
    if (override_scale == 1.0f) {
        if (selected_race == RACE_ELF) scale = 1.15f;
        else if (selected_race == RACE_HUMAN) scale = 1.0f;
        else if (selected_race == RACE_DWARF) scale = 0.80f;
        else if (selected_race == RACE_HALFLING) scale = 0.70f;
        else if (selected_race == RACE_GNOME) scale = 0.58f;
    }

    int bob = (is_running && override_scale == 1.0f) ? (int)(sin(run_bob * 1.2f) * 1.5f) : 0; sy += bob;
    
    HBRUSH shadow_b = CreateSolidBrush(RGB(15, 20, 26)); HGDIOBJ old = SelectObject(hdc, shadow_b); 
    Ellipse(hdc, sx - (int)(10 * scale), sy + (int)(14 * scale), sx + (int)(10 * scale), sy + (int)(22 * scale)); SelectObject(hdc, old); DeleteObject(shadow_b);

    COLORREF body_color = RGB(245, 225, 195);
    COLORREF hat_color  = RGB(55, 185, 90);

    if (selected_class == CLASS_NECROMANCER) { body_color = RGB(80, 50, 110); hat_color = RGB(40, 20, 60); }
    else if (selected_class == CLASS_WARRIOR) { body_color = RGB(160, 40, 30); hat_color = RGB(90, 20, 20); }
    else if (selected_class == CLASS_WIZARD) { body_color = RGB(30, 80, 180); hat_color = RGB(15, 40, 120); }

    HBRUSH body_b = CreateSolidBrush(body_color); HGDIOBJ prev_body = SelectObject(hdc, body_b);
    POINT body[] = {
        {sx - (int)(6 * scale), sy + (int)(6 * scale)}, 
        {sx + (int)(6 * scale), sy + (int)(6 * scale)}, 
        {sx + (int)(8 * scale), sy + (int)(20 * scale)}, 
        {sx - (int)(8 * scale), sy + (int)(20 * scale)}
    }; 
    Polygon(hdc, body, 4); SelectObject(hdc, prev_body); DeleteObject(body_b);

    if (selected_race == RACE_DWARF) {
        HBRUSH beard_b = CreateSolidBrush(RGB(240, 240, 245)); HGDIOBJ prev_beard = SelectObject(hdc, beard_b);
        POINT beard[] = {
            {sx - (int)(6 * scale), sy + (int)(6 * scale)},
            {sx + (int)(6 * scale), sy + (int)(6 * scale)},
            {sx, sy + (int)(18 * scale)}
        };
        Polygon(hdc, beard, 3); SelectObject(hdc, prev_beard); DeleteObject(beard_b);
    }

    if (selected_race == RACE_ELF) {
        HPEN ear_p = CreatePen(PS_SOLID, 1, RGB(245, 210, 170)); HGDIOBJ prev_ear_p = SelectObject(hdc, ear_p);
        MoveToEx(hdc, sx - (int)(8 * scale), sy, NULL); LineTo(hdc, sx - (int)(13 * scale), sy - (int)(4 * scale));
        MoveToEx(hdc, sx + (int)(8 * scale), sy, NULL); LineTo(hdc, sx + (int)(13 * scale), sy - (int)(4 * scale));
        SelectObject(hdc, prev_ear_p); DeleteObject(ear_p);
    }

    // The secondary bow is shown in hand only while it is being drawn.
    WeaponType shown_weapon = (is_charging_bow && bow_equipped) ? WEAPON_BOW : active_weapon;
    if (shown_weapon == WEAPON_SWORD) {
        HPEN p = CreatePen(PS_SOLID, 2, RGB(200, 210, 220)); HGDIOBJ prev_p = SelectObject(hdc, p);
        if (sword_swipe_frame > 0) {
            Arc(hdc, sx - 16, sy - 16, sx + 16, sy + 16, sx - 16, sy, sx + 16, sy);
        } else {
            if (player_facing == FACE_UP || player_facing == FACE_LEFT) { MoveToEx(hdc, sx - 8, sy + 8, NULL); LineTo(hdc, sx - 16, sy - 4); }
            else { MoveToEx(hdc, sx + 6, sy + 12, NULL); LineTo(hdc, sx + 16, sy + 20); }
        }
        SelectObject(hdc, prev_p); DeleteObject(p);
    } else if (shown_weapon == WEAPON_AXE) {
        HPEN p = CreatePen(PS_SOLID, 2, RGB(135, 95, 60)); HGDIOBJ prev_p = SelectObject(hdc, p);
        if (sword_swipe_frame > 0) {
            Arc(hdc, sx - 16, sy - 16, sx + 16, sy + 16, sx - 16, sy, sx + 16, sy);
        } else if (player_facing == FACE_UP || player_facing == FACE_LEFT) { MoveToEx(hdc, sx - 6, sy + 8, NULL); LineTo(hdc, sx - 14, sy - 2); }
        else { MoveToEx(hdc, sx + 6, sy + 10, NULL); LineTo(hdc, sx + 14, sy + 18); }
        SelectObject(hdc, prev_p); DeleteObject(p);
    } else {
        HPEN wood_pen = CreatePen(PS_SOLID, 2, RGB(235, 175, 45)); HGDIOBJ prev_wood_p = SelectObject(hdc, wood_pen);
        HPEN string_pen = CreatePen(PS_SOLID, 1, RGB(15, 18, 24)); 
        if (player_facing == FACE_UP || player_facing == FACE_LEFT) { 
            Arc(hdc, sx + 4, sy + 4, sx + 16, sy + 20, sx + 10, sy + 20, sx + 10, sy + 4); 
            SelectObject(hdc, string_pen); MoveToEx(hdc, sx + 10, sy + 4, NULL); LineTo(hdc, sx + 10, sy + 20);
        } else { 
            Arc(hdc, sx - 16, sy + 4, sx - 4, sy + 20, sx - 10, sy + 4, sx - 10, sy + 20); 
            SelectObject(hdc, string_pen); MoveToEx(hdc, sx - 10, sy + 4, NULL); LineTo(hdc, sx - 10, sy + 20);
        }
        SelectObject(hdc, prev_wood_p); DeleteObject(wood_pen); DeleteObject(string_pen);

        // When bow is charging (drawn), display nocked arrow ready on the bow
        if (is_charging_bow) {
            HPEN arrow_pen = CreatePen(PS_SOLID, 1, RGB(220, 225, 235)); HGDIOBJ prev_arrow_p = SelectObject(hdc, arrow_pen);
            if (player_facing == FACE_UP || player_facing == FACE_LEFT) { 
                MoveToEx(hdc, sx + 4, sy + 12, NULL); LineTo(hdc, sx + 16, sy + 12); 
            } else { 
                MoveToEx(hdc, sx - 16, sy + 12, NULL); LineTo(hdc, sx - 4, sy + 12); 
            }
            SelectObject(hdc, prev_arrow_p); DeleteObject(arrow_pen);
        }
    }
    
    if (selected_race == RACE_GNOME) {
        HBRUSH gnome_hat = CreateSolidBrush(RGB(220, 40, 40)); HGDIOBJ prev_hat = SelectObject(hdc, gnome_hat);
        POINT cap[] = {{sx, sy - (int)(32 * scale)}, {sx + (int)(7 * scale), sy + (int)(4 * scale)}, {sx - (int)(7 * scale), sy + (int)(4 * scale)}};
        Polygon(hdc, cap, 3); SelectObject(hdc, prev_hat); DeleteObject(gnome_hat);
    } else {
        HBRUSH hat_b = CreateSolidBrush(hat_color); HGDIOBJ prev_hat = SelectObject(hdc, hat_b);
        POINT hat[] = {{sx, sy - (int)(14 * scale)}, {sx + (int)(9 * scale), sy + (int)(4 * scale)}, {sx, sy + (int)(8 * scale)}, {sx - (int)(9 * scale), sy + (int)(4 * scale)}};
        Polygon(hdc, hat, 4); SelectObject(hdc, prev_hat); DeleteObject(hat_b);
    }

    // Distinct charging ring on hero indicates bow is held and charging; projectile is only released post-release
    if (is_charging_bow && override_scale == 1.0f) {
        int ring_r = 4 + (int)(bow_charge_time * 12.0f);
        HPEN chg_p = CreatePen(PS_SOLID, 2, RGB(255, 215, 0)); HGDIOBJ prev_chg_p = SelectObject(hdc, chg_p);
        HGDIOBJ null_b = GetStockObject(NULL_BRUSH); HGDIOBJ prev_null_b = SelectObject(hdc, null_b);
        Ellipse(hdc, sx - ring_r, sy - ring_r, sx + ring_r, sy + ring_r);
        SelectObject(hdc, prev_null_b);
        SelectObject(hdc, prev_chg_p);
        DeleteObject(chg_p);
    }

    SelectObject(hdc, old);
}

void DrawHeroAsset(HDC hdc) {
    int sx, sy; GetIsoCoords(player_x, player_y, &sx, &sy);
    DrawHeroAssetEx(hdc, sx, sy, 1.0f);
}

void DrawFlyingArrow(HDC hdc) {
    // Projectile arrow is only drawn in flight post-release while player_arrow.active is 1
    if (!player_arrow.active) return;
    int sx, sy; GetIsoCoords(player_arrow.x, player_arrow.y, &sx, &sy); sy -= (int)player_arrow.z;
    float angle = (float)atan2(player_arrow.vy, player_arrow.vx);
    float cos_a = (float)cos(angle), sin_a = (float)sin(angle);
    POINT tip = { sx + (int)(8 * cos_a), sy + (int)(8 * sin_a) };
    POINT base_l = { sx - (int)(4 * cos_a - 3 * sin_a), sy - (int)(4 * sin_a + 3 * cos_a) };
    POINT base_r = { sx - (int)(4 * cos_a + 3 * sin_a), sy - (int)(4 * sin_a - 3 * cos_a) };
    HBRUSH arrow_b = CreateSolidBrush(RGB(220, 225, 235)); HGDIOBJ old_b = SelectObject(hdc, arrow_b);
    POINT wedge[] = { tip, base_l, base_r }; Polygon(hdc, wedge, 3);
    SelectObject(hdc, old_b); DeleteObject(arrow_b);
    HPEN fletcher_p = CreatePen(PS_SOLID, 1, RGB(190, 65, 35)); HGDIOBJ old_p = SelectObject(hdc, fletcher_p);
    MoveToEx(hdc, sx, sy, NULL); LineTo(hdc, sx - (int)(12 * cos_a), sy - (int)(12 * sin_a));
    SelectObject(hdc, old_p); DeleteObject(fletcher_p);
}

void DrawFlyingSpell(HDC hdc) {
    // Visibly distinct ranged bolts for the two caster classes: Necromancer
    // casts a dark, trailing necrotic orb; Wizard casts a bright arcane shard.
    if (!player_spell.active) return;
    int sx, sy; GetIsoCoords(player_spell.x, player_spell.y, &sx, &sy); sy -= (int)player_spell.z;

    if (selected_class == CLASS_NECROMANCER) {
        HBRUSH trail_b = CreateSolidBrush(RGB(60, 20, 90));
        RECT trail = { sx - 2, sy - 2, sx + 2, sy + 2 };
        FillRect(hdc, &trail, trail_b); DeleteObject(trail_b);
        HBRUSH core_b = CreateSolidBrush(RGB(140, 60, 220)); HGDIOBJ old_b = SelectObject(hdc, core_b);
        Ellipse(hdc, sx - 5, sy - 5, sx + 5, sy + 5);
        SelectObject(hdc, old_b); DeleteObject(core_b);
    } else {
        HBRUSH core_b = CreateSolidBrush(RGB(100, 190, 255)); HGDIOBJ old_b = SelectObject(hdc, core_b);
        POINT shard[] = { {sx, sy - 6}, {sx + 5, sy}, {sx, sy + 6}, {sx - 5, sy} };
        Polygon(hdc, shard, 4);
        SelectObject(hdc, old_b); DeleteObject(core_b);
        HPEN spark_p = CreatePen(PS_SOLID, 1, RGB(230, 245, 255)); HGDIOBJ old_p = SelectObject(hdc, spark_p);
        MoveToEx(hdc, sx - 8, sy, NULL); LineTo(hdc, sx + 8, sy);
        SelectObject(hdc, old_p); DeleteObject(spark_p);
    }
}

// Lightweight GDI feedback flash for the Knight/Warrior defensive backstep and
// the Rogue sneak, drawn for a handful of frames after FireClassAbility() runs.
void DrawClassAbilityFX(HDC hdc) {
    if (ability_visual_frame <= 0) return;
    if (selected_class == CLASS_NECROMANCER || selected_class == CLASS_WIZARD) return;

    int sx, sy; GetIsoCoords(player_x, player_y, &sx, &sy);
    int r = 10 + (ABILITY_FLASH_FRAMES - ability_visual_frame);

    if (selected_class == CLASS_ROGUE) {
        HPEN dash_p = CreatePen(PS_DOT, 1, RGB(150, 220, 180)); HGDIOBJ old_p = SelectObject(hdc, dash_p);
        HGDIOBJ old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, sx - r, sy - r / 2, sx + r, sy + r / 2);
        SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(dash_p);
    } else {
        HPEN ring_p = CreatePen(PS_SOLID, 2, RGB(210, 210, 230)); HGDIOBJ old_p = SelectObject(hdc, ring_p);
        HGDIOBJ old_b = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, sx - r, sy - r, sx + r, sy + r);
        SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(ring_p);
    }
}

void DrawButterflies(HDC hdc) {
    if (IsAdverseWeather()) return;
    if (current_biome == BIOME_CAVE) return;
    for (int i = 0; i < MAX_BUTTERFLIES; i++) {
        if (!butterflies[i].active) continue;
        int sx, sy; GetIsoCoords(butterflies[i].x, butterflies[i].y, &sx, &sy); sy -= (int)butterflies[i].z;
        int wing_flap = (int)(sin(butterflies[i].timer * 8.0f) * 2.0f);
        COLORREF c = (butterflies[i].color_profile == 1) ? RGB(255, 110, 20) : ((butterflies[i].color_profile == 2) ? RGB(30, 140, 255) : RGB(245, 220, 50));
        HBRUSH b = CreateSolidBrush(c); RECT r_l = {sx - 3, sy - 2 - wing_flap, sx, sy + 1}, r_r = {sx, sy - 2 - wing_flap, sx + 3, sy + 1};
        FillRect(hdc, &r_l, b); FillRect(hdc, &r_r, b); DeleteObject(b);
    }
}

void DrawBloodMistFX(HDC hdc) {
    HBRUSH blood_b = CreateSolidBrush(RGB(200, 25, 25)); HGDIOBJ old = SelectObject(hdc, blood_b);
    for (int i = 0; i < MAX_BLOOD_MIST; i++) {
        if (!blood_mist[i].active) continue;
        int bsx, bsy; GetIsoCoords(blood_mist[i].x, blood_mist[i].y, &bsx, &bsy);
        int sz = (blood_mist[i].life > blood_mist[i].max_life - 3) ? 2 : 1;
        RECT r = { bsx - sz, bsy - sz, bsx + sz + 1, bsy + sz + 1 };
        FillRect(hdc, &r, blood_b);
    }
    SelectObject(hdc, old); DeleteObject(blood_b);
}

// Shared zombie body renderer used by both the roaming hostile zombie and the
// necromancer's summoned zombie minion: a tattered white shirt, tattered
// brownish pants over shambling legs (with an occasional hashed interval
// where one foot visibly drags instead of lifting), and both arms reaching
// straight out in front toward the viewer rather than out to the sides.
void DrawZombieBody(HDC hdc, int sx, int sy, int seed) {
    int shamble_phase = ((int)(world_tick * 1.5f) + seed) & 63;
    int sway = (int)(sin(shamble_phase * 6.2831853f / 64.0f) * 3.0f);
    int drag_cycle = ((int)(world_tick * 0.5f) + seed) % 50;
    int dragging_left = (drag_cycle < 6);
    int dragging_right = (drag_cycle >= 25 && drag_cycle < 31);

    HPEN leg_pen = CreatePen(PS_SOLID, 4, RGB(90, 70, 45)); HGDIOBJ prev_leg = SelectObject(hdc, leg_pen);
    int left_leg_len = dragging_left ? 14 : 10;
    int right_leg_len = dragging_right ? 14 : 10;
    MoveToEx(hdc, sx - 4, sy + 9, NULL); LineTo(hdc, sx - 4 + sway + (dragging_left ? -4 : 0), sy + 9 + left_leg_len);
    MoveToEx(hdc, sx + 4, sy + 9, NULL); LineTo(hdc, sx + 4 - sway + (dragging_right ? 4 : 0), sy + 9 + right_leg_len);
    SelectObject(hdc, prev_leg); DeleteObject(leg_pen);

    HBRUSH pants_b = CreateSolidBrush(RGB(110, 85, 55)); HGDIOBJ prev_pants = SelectObject(hdc, pants_b);
    POINT pants[] = { {sx - 6, sy}, {sx + 6, sy}, {sx + 5, sy + 11}, {sx + 1, sy + 8}, {sx - 1, sy + 11}, {sx - 5, sy + 8} };
    Polygon(hdc, pants, 6); SelectObject(hdc, prev_pants); DeleteObject(pants_b);

    HBRUSH shirt_b = CreateSolidBrush(RGB(225, 225, 215)); HGDIOBJ prev_shirt = SelectObject(hdc, shirt_b);
    POINT shirt[] = { {sx - 6, sy - 10}, {sx + 6, sy - 10}, {sx + 6, sy - 2}, {sx + 2, sy + 2}, {sx - 2, sy - 2}, {sx - 6, sy + 2} };
    Polygon(hdc, shirt, 6); SelectObject(hdc, prev_shirt); DeleteObject(shirt_b);

    HPEN arm_pen = CreatePen(PS_SOLID, 3, RGB(200, 200, 190)); HGDIOBJ prev_arm = SelectObject(hdc, arm_pen);
    MoveToEx(hdc, sx - 4, sy - 6, NULL); LineTo(hdc, sx - 6, sy + 8);
    MoveToEx(hdc, sx + 4, sy - 6, NULL); LineTo(hdc, sx + 6, sy + 8);
    SelectObject(hdc, prev_arm); DeleteObject(arm_pen);
}

void DrawDynamicEnemy(HDC hdc) {
    if (enemy_hearts <= 0.0f) return;
    int sx, sy; GetIsoCoords(enemy_x, enemy_y, &sx, &sy);
    HBRUSH b1 = CreateSolidBrush(RGB(15, 20, 26)); HGDIOBJ old = SelectObject(hdc, b1); Ellipse(hdc, sx - 8, sy + 12, sx + 8, sy + 18); SelectObject(hdc, old); DeleteObject(b1);
    if (active_monster == MONSTER_ZOMBIE) {
        DrawZombieBody(hdc, sx, sy, (int)(enemy_x * 13.0f + enemy_y * 7.0f));
    } else {
        COLORREF robe_color = RGB(130, 60, 160);
        HBRUSH robe_b = CreateSolidBrush(robe_color); HGDIOBJ prev_robe = SelectObject(hdc, robe_b);
        POINT robe[] = {{sx - 5, sy}, {sx + 5, sy}, {sx + 6, sy + 15}, {sx - 6, sy + 15}}; Polygon(hdc, robe, 4); SelectObject(hdc, prev_robe); DeleteObject(robe_b);
    }
    HBRUSH skull_b = CreateSolidBrush(RGB(220, 220, 230)); HGDIOBJ prev_skull = SelectObject(hdc, skull_b); Ellipse(hdc, sx - 5, sy - 9, sx + 5, sy + 1); SelectObject(hdc, prev_skull); DeleteObject(skull_b);
    int total_bars = (int)ceil(enemy_hearts);
    for (int i = 0; i < total_bars; i++) {
        RECT r = {sx - 15 + (i * 10), sy - 20, sx - 7 + (i * 10), sy - 13};
        HBRUSH red_b = CreateSolidBrush(RGB(255, 40, 40)); FillRect(hdc, &r, red_b); DeleteObject(red_b);
    }
    SelectObject(hdc, old);
}

void DrawDebrisTwigs(HDC hdc) {
    HBRUSH b = CreateSolidBrush(RGB(110, 70, 50));
    for (int i = 0; i < DEBRIS_MAX; i++) {
        if (!twigs[i].active || twigs[i].screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords(twigs[i].x, twigs[i].y, &sx, &sy); RECT r = {sx - 2, sy - 1, sx + 2, sy + 1}; FillRect(hdc, &r, b);
    }
    DeleteObject(b);
}

void DrawMerchantStoreFront(HDC hdc) {
    if (current_biome != BIOME_CASTLE) return;
    int sx, sy; GetIsoCoords(merchant_x, merchant_y, &sx, &sy);
    HBRUSH tent_b = CreateSolidBrush(RGB(220, 50, 40)); HGDIOBJ old = SelectObject(hdc, tent_b);
    POINT tent[] = {{sx - 24, sy - 12}, {sx, sy - 36}, {sx + 24, sy - 12}, {sx, sy}}; Polygon(hdc, tent, 4); SelectObject(hdc, old); DeleteObject(tent_b);
    HBRUSH merchant_body = CreateSolidBrush(RGB(215, 180, 45)); HGDIOBJ prev = SelectObject(hdc, merchant_body);
    Ellipse(hdc, sx - 6, sy - 18, sx + 6, sy - 6); SelectObject(hdc, prev); DeleteObject(merchant_body);
}

// A small, charming villager silhouette reminiscent of classic Zelda-style
// townsfolk: a simple tunic, belt, bare arms, round head and a cap/hair style
// that varies per NPC so each villager in the settlement reads as a distinct
// little character instead of an identical color-swapped blob.
void DrawZeldaStyleVillager(HDC hdc, int sx, int sy, int npc_index, float scale) {
#define VS(v) ((int)((v) * scale))
    static const COLORREF tunic_palette[] = { RGB(60, 150, 70), RGB(190, 90, 60), RGB(90, 110, 170), RGB(200, 170, 70) };
    static const COLORREF hair_palette[] = { RGB(80, 55, 35), RGB(230, 210, 120), RGB(40, 40, 45), RGB(150, 70, 40) };
    COLORREF tunic = tunic_palette[npc_index % 4];
    COLORREF hair = hair_palette[(npc_index + 1) % 4];
    COLORREF skin = RGB(235, 195, 160);

    HBRUSH boot_b = CreateSolidBrush(RGB(90, 65, 45));
    RECT boots = { sx - VS(4), sy + VS(6), sx + VS(4), sy + VS(10) };
    FillRect(hdc, &boots, boot_b); DeleteObject(boot_b);

    // Simple tunic-shaped torso, slightly tapered like a classic adventure outfit.
    HBRUSH tunic_b = CreateSolidBrush(tunic); HGDIOBJ old = SelectObject(hdc, tunic_b);
    POINT body[] = { {sx - VS(6), sy + VS(6)}, {sx - VS(4), sy - VS(6)}, {sx + VS(4), sy - VS(6)}, {sx + VS(6), sy + VS(6)} };
    Polygon(hdc, body, 4);
    SelectObject(hdc, old); DeleteObject(tunic_b);

    HBRUSH belt_b = CreateSolidBrush(RGB(90, 65, 40));
    RECT belt = { sx - VS(5), sy + VS(1), sx + VS(5), sy + VS(3) };
    FillRect(hdc, &belt, belt_b); DeleteObject(belt_b);

    // Bare arms held at the sides.
    HBRUSH skin_b = CreateSolidBrush(skin);
    RECT arm_l = { sx - VS(8), sy - VS(4), sx - VS(5), sy + VS(3) };
    RECT arm_r = { sx + VS(5), sy - VS(4), sx + VS(8), sy + VS(3) };
    FillRect(hdc, &arm_l, skin_b); FillRect(hdc, &arm_r, skin_b);

    // Round, friendly head.
    HGDIOBJ old_head = SelectObject(hdc, skin_b);
    Ellipse(hdc, sx - VS(4), sy - VS(14), sx + VS(4), sy - VS(6));
    SelectObject(hdc, old_head); DeleteObject(skin_b);

    // Hair for half the villagers, a simple pointed cap for the other half -
    // a cheap but effective way to give every NPC its own silhouette.
    HBRUSH hair_b = CreateSolidBrush(hair); old = SelectObject(hdc, hair_b);
    if (npc_index % 2 == 0) {
        POINT cap[] = { {sx - VS(5), sy - VS(11)}, {sx, sy - VS(19)}, {sx + VS(5), sy - VS(11)} };
        Polygon(hdc, cap, 3);
    } else {
        Ellipse(hdc, sx - VS(5), sy - VS(15), sx + VS(5), sy - VS(10));
    }
    SelectObject(hdc, old); DeleteObject(hair_b);
#undef VS
}

void DrawVillage(HDC hdc) {
    if (current_biome == BIOME_PRAIRIE && screens_until_town > 2) {
        const int hx[] = { 11, 15, 19, 15 }, hy[] = { 11, 10, 11, 18 };
        int shift = (current_screen_index % 3) - 1;
        HBRUSH hut = CreateSolidBrush(RGB(150, 105, 65));
        HBRUSH thatch = CreateSolidBrush(RGB(190, 150, 75));
        HBRUSH door_b = CreateSolidBrush(RGB(70, 45, 28));
        HBRUSH window_b = CreateSolidBrush(RGB(205, 225, 235));
        HBRUSH chimney_b = CreateSolidBrush(RGB(130, 130, 135));
        HPEN beam_pen = CreatePen(PS_SOLID, 2, RGB(90, 60, 35));
        HPEN lattice_pen = CreatePen(PS_SOLID, 1, RGB(80, 85, 95));
        // Village homes are scaled up from their original size but dialed back
        // from the earlier 4.5x pass so they read as "larger, detailed
        // cottages" instead of towering over the village. Anchored at the
        // ground-contact point (the bottom edge of the hut body) so the
        // building still sits planted on the tile instead of stretching
        // symmetrically in both directions.
        const float HOME_SCALE = 3.2f;
        for (int i = 0; i < 4; i++) {
            int sx, sy; GetIsoCoords((float)(hx[i] + shift), (float)hy[i], &sx, &sy);
            int ax = sx, ay = sy + 8;
            int hw = (int)(14 * HOME_SCALE), hh = (int)(20 * HOME_SCALE);
            RECT body = { ax - hw, ay - hh, ax + hw, ay };
            FillRect(hdc, &body, hut);

            // Cross-timber framing accents break up the flat wall color.
            HGDIOBJ old_pen = SelectObject(hdc, beam_pen);
            MoveToEx(hdc, ax - hw, ay - hh / 2, NULL); LineTo(hdc, ax + hw, ay - hh / 2);
            MoveToEx(hdc, ax - hw / 2, ay - hh, NULL); LineTo(hdc, ax - hw / 2, ay);
            MoveToEx(hdc, ax + hw / 2, ay - hh, NULL); LineTo(hdc, ax + hw / 2, ay);
            SelectObject(hdc, old_pen);

            // Front door, planted centered on the ground edge.
            int dw = (int)(4 * HOME_SCALE), dh = (int)(9 * HOME_SCALE);
            RECT door = { ax - dw, ay - dh, ax + dw, ay };
            FillRect(hdc, &door, door_b);

            // A small shuttered window with simple lattice cross-bars.
            RECT win = { ax - hw + (int)(3 * HOME_SCALE), ay - hh + (int)(6 * HOME_SCALE),
                         ax - hw + (int)(9 * HOME_SCALE), ay - hh + (int)(12 * HOME_SCALE) };
            FillRect(hdc, &win, window_b);
            old_pen = SelectObject(hdc, lattice_pen);
            MoveToEx(hdc, (win.left + win.right) / 2, win.top, NULL); LineTo(hdc, (win.left + win.right) / 2, win.bottom);
            MoveToEx(hdc, win.left, (win.top + win.bottom) / 2, NULL); LineTo(hdc, win.right, (win.top + win.bottom) / 2);
            SelectObject(hdc, old_pen);

            HGDIOBJ old = SelectObject(hdc, thatch);
            POINT roof[] = { {ax - (int)(18 * HOME_SCALE), ay - hh}, {ax, ay - (int)(36 * HOME_SCALE)}, {ax + (int)(18 * HOME_SCALE), ay - hh} };
            Polygon(hdc, roof, 3); SelectObject(hdc, old);

            // A small stone chimney poking out of the thatch for extra detail.
            RECT chimney = { ax + (int)(8 * HOME_SCALE), ay - (int)(32 * HOME_SCALE), ax + (int)(11 * HOME_SCALE), ay - (int)(18 * HOME_SCALE) };
            FillRect(hdc, &chimney, chimney_b);
        }
        DeleteObject(hut); DeleteObject(thatch); DeleteObject(door_b); DeleteObject(window_b);
        DeleteObject(chimney_b); DeleteObject(beam_pen); DeleteObject(lattice_pen);
    } else if (current_biome == BIOME_CASTLE) {
        int sx, sy; GetIsoCoords(11.0f, 14.0f, &sx, &sy);
        HBRUSH stall = CreateSolidBrush(RGB(170, 115, 65));
        RECT stall_body = { sx - 18, sy - 8, sx + 18, sy + 5 }; FillRect(hdc, &stall_body, stall); DeleteObject(stall);
        HBRUSH canopy = CreateSolidBrush(RGB(70, 115, 155)); HGDIOBJ old = SelectObject(hdc, canopy);
        POINT roof[] = { {sx - 22, sy - 8}, {sx - 17, sy - 20}, {sx + 17, sy - 20}, {sx + 22, sy - 8} };
        Polygon(hdc, roof, 4); SelectObject(hdc, old); DeleteObject(canopy);
    }
    // Every NPC (village and castle) uses the detailed villager figure at the
    // hero's scale. Their lines go to the action log in the upper left; only a
    // "..." bubble is drawn over the head of an NPC close enough to talk to.
    const float NPC_SCALE = 1.2f;
    for (int i = 0; i < village_npc_count; i++) {
        int sx, sy; GetIsoCoords(village_npcs[i].x, village_npcs[i].y, &sx, &sy);
        int look = i;
        if (village_npcs[i].type == 1) look = 3;          // merchant: gold tunic
        else if (village_npcs[i].type == 2) look = 2 + 4 * i; // guards: blue tunic
        DrawZeldaStyleVillager(hdc, sx, sy, look, NPC_SCALE);
        if (IsNearPoint(village_npcs[i].x, village_npcs[i].y, 2.0f)) {
            int top = sy - (int)(19 * NPC_SCALE) - 20;
            HBRUSH white = CreateSolidBrush(RGB(245, 240, 215)); HGDIOBJ old_b = SelectObject(hdc, white);
            HPEN edge = CreatePen(PS_SOLID, 1, RGB(60, 55, 45)); HGDIOBJ old_p = SelectObject(hdc, edge);
            RoundRect(hdc, sx - 14, top, sx + 14, top + 14, 6, 6);
            SelectObject(hdc, old_p); DeleteObject(edge); SelectObject(hdc, old_b); DeleteObject(white);
            RECT bubble = { sx - 14, top - 3, sx + 14, top + 14 };
            SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(25, 25, 25));
            DrawText(hdc, "...", 3, &bubble, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
    }
}

void DrawSummonedZombie(HDC hdc) {
    if (!summoned_zombie.active) return;
    int sx, sy; GetIsoCoords(summoned_zombie.x, summoned_zombie.y, &sx, &sy);
    int rise = (summoned_zombie.raise_frame < 6) ? 12 - summoned_zombie.raise_frame * 2 : 0;
    sy += rise;
    DrawZombieBody(hdc, sx, sy, (int)(summoned_zombie.x * 11.0f + summoned_zombie.y * 5.0f));
    HBRUSH head_b = CreateSolidBrush(RGB(150, 170, 130)); HGDIOBJ old = SelectObject(hdc, head_b);
    Ellipse(hdc, sx - 5, sy - 16, sx + 5, sy - 6);
    SelectObject(hdc, old); DeleteObject(head_b);
}
