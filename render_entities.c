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

    if (selected_class == CLASS_NECROMANCER) { body_color = RGB(26, 22, 30); hat_color = RGB(14, 12, 18); } // black cloak and hood
    else if (selected_class == CLASS_WARRIOR) { body_color = RGB(160, 40, 30); hat_color = RGB(90, 20, 20); }
    else if (selected_class == CLASS_WIZARD) { body_color = RGB(30, 80, 180); hat_color = RGB(15, 40, 120); }
    // Worn clothes replace the class colours; a Cloth Hood hides the face like the necromancer's.
    const char *head_wear = WornItem("Head"), *clothes = WornItem("Clothes"), *cloak = WornItem("Cloak");
    int hooded = head_wear ? strcmp(head_wear, "Cloth Hood") == 0 : selected_class == CLASS_NECROMANCER;
    if (clothes) body_color = ClothingColor(clothes);
    if (head_wear) hat_color = ClothingColor(head_wear);

    HBRUSH body_b = CreateSolidBrush(body_color); HGDIOBJ prev_body = SelectObject(hdc, body_b);
    POINT body[] = {
        {sx - (int)(6 * scale), sy + (int)(6 * scale)}, 
        {sx + (int)(6 * scale), sy + (int)(6 * scale)}, 
        {sx + (int)(8 * scale), sy + (int)(20 * scale)}, 
        {sx - (int)(8 * scale), sy + (int)(20 * scale)}
    }; 
    if (hero_sex) { // bust, waist and hips, a touch more shapely
        POINT fb[] = { {sx - (int)(5 * scale), sy + (int)(6 * scale)}, {sx + (int)(5 * scale), sy + (int)(6 * scale)}, {sx + (int)(7 * scale), sy + (int)(9 * scale)},
                       {sx + (int)(4 * scale), sy + (int)(13 * scale)}, {sx + (int)(9 * scale), sy + (int)(20 * scale)}, {sx - (int)(9 * scale), sy + (int)(20 * scale)},
                       {sx - (int)(4 * scale), sy + (int)(13 * scale)}, {sx - (int)(7 * scale), sy + (int)(9 * scale)} };
        Polygon(hdc, fb, 8);
    } else Polygon(hdc, body, 4);
    SelectObject(hdc, prev_body); DeleteObject(body_b);
    if (cloak) { // two panels hanging from the shoulders, open at the front
        COLORREF cc = ClothingColor(cloak);
        HBRUSH cb = CreateSolidBrush(cc); HPEN cp = CreatePen(PS_SOLID, 1, RGB(GetRValue(cc) * 3 / 4, GetGValue(cc) * 3 / 4, GetBValue(cc) * 3 / 4));
        HGDIOBJ pcb = SelectObject(hdc, cb), pcp = SelectObject(hdc, cp);
        for (int side = -1; side <= 1; side += 2) {
            POINT panel[] = { {sx + side * (int)(8 * scale), sy + (int)(5 * scale)}, {sx + side * (int)(3 * scale), sy + (int)(6 * scale)},
                              {sx + side * (int)(5 * scale), sy + (int)(20 * scale)}, {sx + side * (int)(11 * scale), sy + (int)(22 * scale)} };
            Polygon(hdc, panel, 4);
        }
        SelectObject(hdc, pcp); SelectObject(hdc, pcb); DeleteObject(cb); DeleteObject(cp);
    }

    if (hero_beard > 0) {
        HBRUSH beard_b = CreateSolidBrush(HeroHairColor()); HGDIOBJ prev_beard = SelectObject(hdc, beard_b);
        POINT beard[] = {
            {sx - (int)(6 * scale), sy + (int)(6 * scale)},
            {sx + (int)(6 * scale), sy + (int)(6 * scale)},
            {sx, sy + (int)((hero_beard == 2 ? 18 : 12) * scale)}
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
    } else if (shown_weapon == WEAPON_SPEAR) {
        // Iso screen direction of the facing; the thrust pushes the spear out.
        int fx = 1, fy = 1;
        if (player_facing == FACE_UP) { fx = 1; fy = -1; }
        else if (player_facing == FACE_DOWN) { fx = -1; fy = 1; }
        else if (player_facing == FACE_LEFT) { fx = -1; fy = -1; }
        int len = sword_swipe_frame > 0 ? 32 : 20;
        int bx = sx - fx * 6, by = sy + 8 - fy * 3;
        int tx = bx + fx * len, ty = by + fy * len / 2;
        HPEN shaft = CreatePen(PS_SOLID, 2, RGB(150, 110, 70)); HGDIOBJ prev_p = SelectObject(hdc, shaft);
        MoveToEx(hdc, bx, by, NULL); LineTo(hdc, tx, ty);
        HPEN tip = CreatePen(PS_SOLID, 3, RGB(210, 215, 225)); SelectObject(hdc, tip);
        MoveToEx(hdc, tx, ty, NULL); LineTo(hdc, tx + fx * 5, ty + fy * 3);
        SelectObject(hdc, prev_p); DeleteObject(shaft); DeleteObject(tip);
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
    
    if (selected_race == RACE_GNOME && !head_wear) {
        HBRUSH gnome_hat = CreateSolidBrush(RGB(220, 40, 40)); HGDIOBJ prev_hat = SelectObject(hdc, gnome_hat);
        POINT cap[] = {{sx, sy - (int)(32 * scale)}, {sx + (int)(7 * scale), sy + (int)(4 * scale)}, {sx - (int)(7 * scale), sy + (int)(4 * scale)}};
        Polygon(hdc, cap, 3); SelectObject(hdc, prev_hat); DeleteObject(gnome_hat);
    } else if (hooded) {
#define HS(v) ((int)((v) * scale))
        // Cloth hood: a soft cowl that falls onto the shoulders, the face lost
        // in shadow inside it. Elf ears and dwarf beards still poke out.
        HBRUSH hood = CreateSolidBrush(hat_color); HPEN fold = CreatePen(PS_SOLID, 1, RGB(48, 40, 56));
        HGDIOBJ prev_hood = SelectObject(hdc, hood), prev_fold = SelectObject(hdc, fold);
        POINT cowl[] = { {sx - HS(2), sy - HS(13)}, {sx + HS(3), sy - HS(12)}, {sx + HS(8), sy - HS(7)}, {sx + HS(10), sy + HS(1)}, {sx + HS(10), sy + HS(9)},
                         {sx + HS(6), sy + HS(12)}, {sx - HS(6), sy + HS(12)}, {sx - HS(10), sy + HS(9)}, {sx - HS(10), sy + HS(1)}, {sx - HS(8), sy - HS(7)} };
        Polygon(hdc, cowl, 10);
        MoveToEx(hdc, sx - HS(7), sy + HS(10), NULL); LineTo(hdc, sx - HS(4), sy + HS(5)); // drape folds
        MoveToEx(hdc, sx + HS(7), sy + HS(10), NULL); LineTo(hdc, sx + HS(4), sy + HS(5));
        HBRUSH shade = CreateSolidBrush(RGB(5, 4, 7)); SelectObject(hdc, shade); SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, sx - HS(5), sy - HS(6), sx + HS(5), sy + HS(6));
        SelectObject(hdc, prev_fold); SelectObject(hdc, prev_hood); DeleteObject(fold); DeleteObject(hood); DeleteObject(shade);
        if (selected_race == RACE_ELF) {
            HPEN ear_p = CreatePen(PS_SOLID, 2, RGB(245, 210, 170)); HGDIOBJ prev_ear_p = SelectObject(hdc, ear_p);
            MoveToEx(hdc, sx - HS(9), sy, NULL); LineTo(hdc, sx - HS(14), sy - HS(5));
            MoveToEx(hdc, sx + HS(9), sy, NULL); LineTo(hdc, sx + HS(14), sy - HS(5));
            SelectObject(hdc, prev_ear_p); DeleteObject(ear_p);
        }
        if (hero_beard > 0) {
            HBRUSH beard_b = CreateSolidBrush(HeroHairColor()); HGDIOBJ prev_beard = SelectObject(hdc, beard_b);
            POINT beard[] = { {sx - HS(4), sy + HS(4)}, {sx + HS(4), sy + HS(4)}, {sx, sy + HS(hero_beard == 2 ? 17 : 11)} };
            Polygon(hdc, beard, 3); SelectObject(hdc, prev_beard); DeleteObject(beard_b);
        }
#undef HS
    } else {
        HBRUSH hat_b = CreateSolidBrush(hat_color); HGDIOBJ prev_hat = SelectObject(hdc, hat_b);
        POINT hat[] = {{sx, sy - (int)(14 * scale)}, {sx + (int)(9 * scale), sy + (int)(4 * scale)}, {sx, sy + (int)(8 * scale)}, {sx - (int)(9 * scale), sy + (int)(4 * scale)}};
        Polygon(hdc, hat, 4); SelectObject(hdc, prev_hat); DeleteObject(hat_b);
        if (head_wear && (strcmp(head_wear, "Straw Hat") == 0 || strcmp(head_wear, "Feathered Hat") == 0)) {
            HBRUSH brim_b = CreateSolidBrush(hat_color); HGDIOBJ pbb = SelectObject(hdc, brim_b);
            POINT brim[] = { {sx - (int)(15 * scale), sy + (int)(3 * scale)}, {sx, sy - (int)(1 * scale)}, {sx + (int)(15 * scale), sy + (int)(3 * scale)}, {sx, sy + (int)(7 * scale)} };
            Polygon(hdc, brim, 4); SelectObject(hdc, pbb); DeleteObject(brim_b);
            if (head_wear[0] == 'F') {
                HPEN fp = CreatePen(PS_SOLID, 2, RGB(200, 40, 40)); HGDIOBJ pfp = SelectObject(hdc, fp);
                MoveToEx(hdc, sx + (int)(3 * scale), sy - (int)(6 * scale), NULL); LineTo(hdc, sx + (int)(10 * scale), sy - (int)(18 * scale));
                SelectObject(hdc, pfp); DeleteObject(fp);
            }
        }
    }

    if (!hooded) { // hair under the hat and eye dots; the hood hides both
        HBRUSH hair_b = CreateSolidBrush(HeroHairColor());
        HGDIOBJ prev_hb = SelectObject(hdc, hair_b), prev_hp = SelectObject(hdc, GetStockObject(NULL_PEN));
        int hair_len = hero_sex ? 15 : 9;
        POINT hl[] = { {sx - (int)(9 * scale), sy + (int)(4 * scale)}, {sx - (int)(5 * scale), sy + (int)(6 * scale)}, {sx - (int)(8 * scale), sy + (int)(hair_len * scale)} };
        POINT hr[] = { {sx + (int)(9 * scale), sy + (int)(4 * scale)}, {sx + (int)(5 * scale), sy + (int)(6 * scale)}, {sx + (int)(8 * scale), sy + (int)(hair_len * scale)} };
        Polygon(hdc, hl, 3); Polygon(hdc, hr, 3);
        HBRUSH eye_b = CreateSolidBrush(HeroEyeColor()); SelectObject(hdc, eye_b);
        int ew = scale >= 1.5f ? 2 : 1, ey = sy + (int)(3 * scale);
        for (int side = -1; side <= 1; side += 2) { int ex = sx + side * (int)(4 * scale); Ellipse(hdc, ex - ew, ey - ew, ex + ew + 1, ey + ew + 1); }
        SelectObject(hdc, prev_hp); SelectObject(hdc, prev_hb); DeleteObject(hair_b); DeleteObject(eye_b);
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

void DrawFlyingSpear(HDC hdc) {
    if (!thrown_spear.active) return;
    int sx, sy; GetIsoCoords(thrown_spear.x, thrown_spear.y, &sx, &sy); sy -= (int)thrown_spear.z;
    int ex, ey; GetIsoCoords(thrown_spear.x - thrown_spear.vx * 3.0f, thrown_spear.y - thrown_spear.vy * 3.0f, &ex, &ey); ey -= (int)thrown_spear.z;
    HPEN shaft = CreatePen(PS_SOLID, 2, RGB(150, 110, 70)); HGDIOBJ old_p = SelectObject(hdc, shaft);
    MoveToEx(hdc, ex, ey, NULL); LineTo(hdc, sx, sy);
    HPEN tip = CreatePen(PS_SOLID, 3, RGB(210, 215, 225)); SelectObject(hdc, tip);
    MoveToEx(hdc, sx, sy, NULL); LineTo(hdc, sx + (sx - ex) / 5, sy + (sy - ey) / 5);
    SelectObject(hdc, old_p); DeleteObject(shaft); DeleteObject(tip);
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
    for (int i = 0; i < MAX_BLOOD_MIST; i++) {
        if (!blood_mist[i].active) continue;
        int bsx, bsy; GetIsoCoords(blood_mist[i].x, blood_mist[i].y, &bsx, &bsy);
        int sz = (blood_mist[i].life > blood_mist[i].max_life - 3) ? 2 : 1;
        RECT r = { bsx - sz, bsy - sz, bsx + sz + 1, bsy + sz + 1 };
        HBRUSH b = CreateSolidBrush(blood_mist[i].color ? (COLORREF)blood_mist[i].color : RGB(200, 25, 25));
        FillRect(hdc, &r, b); DeleteObject(b);
    }
}

// Shared zombie renderer (hostile zombie + necromancer's summoned minion).
// Parts stack without gaps (legs, pants, shirt, head) so the figure never
// reads as two halves. face_dir (-1/1) is the way the arms reach when
// arms_out is set; otherwise they hang. Every so often one foot drags behind.
void DrawZombieBody(HDC hdc, int sx, int sy, int seed, int face_dir, int arms_out, COLORREF head_color) {
    int phase = ((int)(world_tick * 1.5f) + seed) & 63;
    int step = (int)(sin(phase * 6.2831853f / 64.0f) * 3.0f);
    int dragging = (((int)(world_tick * 0.5f) + seed) % 50) < 14;

    int lfx = sx - 3 + (dragging ? 0 : step), lfy = sy + 18;
    int rfx = dragging ? sx + 3 - face_dir * 7 : sx + 3 - step, rfy = dragging ? sy + 19 : sy + 18;
    HPEN leg_pen = CreatePen(PS_SOLID, 3, RGB(90, 70, 45)); HGDIOBJ prev = SelectObject(hdc, leg_pen);
    MoveToEx(hdc, sx - 3, sy + 7, NULL); LineTo(hdc, lfx, lfy);
    MoveToEx(hdc, sx + 3, sy + 7, NULL); LineTo(hdc, rfx, rfy);
    SelectObject(hdc, prev); DeleteObject(leg_pen);
    HBRUSH foot_b = CreateSolidBrush(RGB(60, 50, 40)); prev = SelectObject(hdc, foot_b);
    Ellipse(hdc, lfx - 3, lfy - 2, lfx + 3, lfy + 2);
    Ellipse(hdc, rfx - 3, rfy - 2, rfx + 3, rfy + 2);
    SelectObject(hdc, prev); DeleteObject(foot_b);

    HBRUSH pants_b = CreateSolidBrush(RGB(110, 85, 55)); prev = SelectObject(hdc, pants_b);
    POINT pants[] = { {sx - 6, sy}, {sx + 6, sy}, {sx + 5, sy + 10}, {sx + 2, sy + 8}, {sx, sy + 10}, {sx - 2, sy + 8}, {sx - 5, sy + 10} };
    Polygon(hdc, pants, 7); SelectObject(hdc, prev); DeleteObject(pants_b);

    HBRUSH shirt_b = CreateSolidBrush(RGB(225, 225, 215)); prev = SelectObject(hdc, shirt_b);
    POINT shirt[] = { {sx - 6, sy - 11}, {sx + 6, sy - 11}, {sx + 6, sy + 1}, {sx + 3, sy + 3}, {sx, sy + 1}, {sx - 3, sy + 3}, {sx - 6, sy + 1} };
    Polygon(hdc, shirt, 7); SelectObject(hdc, prev); DeleteObject(shirt_b);

    HBRUSH head_b = CreateSolidBrush(head_color); prev = SelectObject(hdc, head_b);
    Ellipse(hdc, sx - 5, sy - 21, sx + 5, sy - 11);
    SelectObject(hdc, prev);
    HBRUSH eye_b = CreateSolidBrush(RGB(30, 25, 25));
    RECT eye_l = { sx - 3 + face_dir, sy - 17, sx - 1 + face_dir, sy - 15 };
    RECT eye_r = { sx + 1 + face_dir, sy - 17, sx + 3 + face_dir, sy - 15 };
    FillRect(hdc, &eye_l, eye_b); FillRect(hdc, &eye_r, eye_b); DeleteObject(eye_b);

    HPEN arm_pen = CreatePen(PS_SOLID, 3, RGB(200, 200, 190)); prev = SelectObject(hdc, arm_pen);
    int hx1, hy1, hx2, hy2;
    if (arms_out) {
        int bob = (int)(sin(world_tick * 3.0f + seed) * 1.5f);
        hx1 = sx - 4 + face_dir * 14; hy1 = sy - 7 + bob;
        hx2 = sx + 4 + face_dir * 14; hy2 = sy - 5 + bob;
        MoveToEx(hdc, sx - 4, sy - 8, NULL); LineTo(hdc, hx1, hy1);
        MoveToEx(hdc, sx + 4, sy - 6, NULL); LineTo(hdc, hx2, hy2);
    } else {
        hx1 = sx - 8; hy1 = sy + 3; hx2 = sx + 8; hy2 = sy + 3;
        MoveToEx(hdc, sx - 6, sy - 9, NULL); LineTo(hdc, hx1, hy1);
        MoveToEx(hdc, sx + 6, sy - 9, NULL); LineTo(hdc, hx2, hy2);
    }
    SelectObject(hdc, prev); DeleteObject(arm_pen);
    prev = SelectObject(hdc, head_b);
    Ellipse(hdc, hx1 - 2, hy1 - 2, hx1 + 2, hy1 + 2);
    Ellipse(hdc, hx2 - 2, hy2 - 2, hx2 + 2, hy2 + 2);
    SelectObject(hdc, prev); DeleteObject(head_b);
}

static void PenLine(HDC hdc, int w, COLORREF c, int x1, int y1, int x2, int y2) {
    HPEN pen = CreatePen(PS_SOLID, w, c); HGDIOBJ prev = SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, NULL); LineTo(hdc, x2, y2);
    SelectObject(hdc, prev); DeleteObject(pen);
}

static void BrushEllipse(HDC hdc, COLORREF c, int l, int t, int r, int b) {
    HBRUSH br = CreateSolidBrush(c); HGDIOBJ prev = SelectObject(hdc, br);
    Ellipse(hdc, l, t, r, b); SelectObject(hdc, prev); DeleteObject(br);
}

static void BrushPoly(HDC hdc, COLORREF c, POINT *pts, int n) {
    HBRUSH br = CreateSolidBrush(c); HGDIOBJ prev = SelectObject(hdc, br);
    Polygon(hdc, pts, n); SelectObject(hdc, prev); DeleteObject(br);
}

// Small, quick green goblin with big ears and a rusty dagger.
static void DrawGoblin(HDC hdc, int sx, int sy, int face_dir) {
    int step = (int)(sin(world_tick * 9.0f) * 2.0f);
    COLORREF skin = RGB(95, 160, 70);
    PenLine(hdc, 2, RGB(70, 110, 50), sx - 2, sy + 12, sx - 2 + step, sy + 19);
    PenLine(hdc, 2, RGB(70, 110, 50), sx + 2, sy + 12, sx + 2 - step, sy + 19);
    POINT body[] = { {sx - 5, sy + 2}, {sx + 5, sy + 2}, {sx + 6, sy + 13}, {sx - 6, sy + 13} };
    BrushPoly(hdc, RGB(110, 80, 50), body, 4);
    POINT ear_l[] = { {sx - 3, sy - 4}, {sx - 12, sy - 9}, {sx - 3, sy} };
    POINT ear_r[] = { {sx + 3, sy - 4}, {sx + 12, sy - 9}, {sx + 3, sy} };
    BrushPoly(hdc, skin, ear_l, 3); BrushPoly(hdc, skin, ear_r, 3);
    BrushEllipse(hdc, skin, sx - 5, sy - 8, sx + 5, sy + 3);
    HBRUSH eye = CreateSolidBrush(RGB(250, 220, 60));
    RECT el = { sx - 3 + face_dir, sy - 4, sx - 1 + face_dir, sy - 2 }, er = { sx + 1 + face_dir, sy - 4, sx + 3 + face_dir, sy - 2 };
    FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
    PenLine(hdc, 2, RGB(160, 150, 140), sx + face_dir * 5, sy + 7, sx + face_dir * 12, sy + 2);
}

// Pan-style satyr cultist: goat legs with hooves, horns, red cult sash and a spear.
static void DrawSatyr(HDC hdc, int sx, int sy, int face_dir) {
    int step = (int)(sin(world_tick * 7.0f) * 2.0f);
    COLORREF fur = RGB(105, 75, 50), skin = RGB(205, 160, 120);
    for (int side = -1; side <= 1; side += 2) {
        int hip = sx + side * 3, foot = hip + side * step;
        HPEN pen = CreatePen(PS_SOLID, 4, fur); HGDIOBJ prev = SelectObject(hdc, pen);
        MoveToEx(hdc, hip, sy + 6, NULL); LineTo(hdc, hip - face_dir * 3 + side * step, sy + 12); LineTo(hdc, foot, sy + 18);
        SelectObject(hdc, prev); DeleteObject(pen);
        HBRUSH hoof = CreateSolidBrush(RGB(35, 25, 20)); RECT h = { foot - 2, sy + 17, foot + 3, sy + 21 };
        FillRect(hdc, &h, hoof); DeleteObject(hoof);
    }
    BrushEllipse(hdc, fur, sx - 6, sy + 1, sx + 6, sy + 10);
    POINT torso[] = { {sx - 5, sy - 10}, {sx + 5, sy - 10}, {sx + 4, sy + 3}, {sx - 4, sy + 3} };
    BrushPoly(hdc, skin, torso, 4);
    PenLine(hdc, 2, RGB(150, 30, 40), sx - 5, sy - 9, sx + 4, sy + 2);
    BrushEllipse(hdc, skin, sx - 4, sy - 19, sx + 4, sy - 10);
    POINT beard[] = { {sx - 3, sy - 12}, {sx + 3, sy - 12}, {sx + face_dir, sy - 7} };
    BrushPoly(hdc, fur, beard, 3);
    for (int side = -1; side <= 1; side += 2) {
        HPEN horn = CreatePen(PS_SOLID, 2, RGB(225, 215, 190)); HGDIOBJ prev = SelectObject(hdc, horn);
        MoveToEx(hdc, sx + side * 3, sy - 18, NULL); LineTo(hdc, sx + side * 6, sy - 23); LineTo(hdc, sx + side * 4, sy - 26);
        SelectObject(hdc, prev); DeleteObject(horn);
    }
    int ex = sx + face_dir * 16, ey = sy - 14;
    PenLine(hdc, 2, RGB(120, 85, 50), sx - face_dir * 6, sy + 8, ex, ey);
    POINT tip[] = { {ex + face_dir * 5, ey - 5}, {ex - 2, ey - 2}, {ex + 2, ey + 2} };
    BrushPoly(hdc, RGB(170, 175, 185), tip, 3);
    PenLine(hdc, 3, skin, sx + face_dir * 4, sy - 7, sx + face_dir * 6, sy);
}

// Mythic beasts are drawn roughly twice the size of ordinary monsters.
static void DrawApeBeast(HDC hdc, int sx, int sy, int face_dir, COLORREF fur, COLORREF face) {
    int step = (int)(sin(world_tick * 4.0f) * 3.0f);
    PenLine(hdc, 7, fur, sx - 6, sy + 6, sx - 6 + step, sy + 20);
    PenLine(hdc, 7, fur, sx + 6, sy + 6, sx + 6 - step, sy + 20);
    BrushEllipse(hdc, fur, sx - 14, sy - 24, sx + 14, sy + 10);
    PenLine(hdc, 6, fur, sx - 12, sy - 16, sx - 16, sy + 6 - step);
    PenLine(hdc, 6, fur, sx + 12, sy - 16, sx + 16, sy + 6 + step);
    BrushEllipse(hdc, fur, sx - 8, sy - 38, sx + 8, sy - 20);
    BrushEllipse(hdc, face, sx - 5 + face_dir * 2, sy - 33, sx + 5 + face_dir * 2, sy - 23);
    HBRUSH eye = CreateSolidBrush(RGB(20, 20, 25));
    RECT el = { sx - 3 + face_dir * 2, sy - 30, sx - 1 + face_dir * 2, sy - 28 }, er = { sx + 1 + face_dir * 2, sy - 30, sx + 3 + face_dir * 2, sy - 28 };
    FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
}

static void DrawSphinx(HDC hdc, int sx, int sy, int face_dir) {
    COLORREF lion = RGB(200, 165, 100), skin = RGB(205, 165, 110), blue = RGB(40, 70, 160), gold = RGB(235, 195, 60);
    int step = (int)(sin(world_tick * 4.0f) * 3.0f);
    PenLine(hdc, 4, lion, sx - 12, sy + 2, sx - 12 + step, sy + 16);
    PenLine(hdc, 4, lion, sx - 6, sy + 2, sx - 6 - step, sy + 16);
    PenLine(hdc, 4, lion, sx + 6, sy + 2, sx + 6 + step, sy + 16);
    PenLine(hdc, 4, lion, sx + 12, sy + 2, sx + 12 - step, sy + 16);
    PenLine(hdc, 2, lion, sx - face_dir * 18, sy - 2, sx - face_dir * 26, sy - 12);
    BrushEllipse(hdc, lion, sx - 18, sy - 8, sx + 18, sy + 8);
    int hx = sx + face_dir * 14;
    POINT nemes[] = { {hx - 8, sy - 22}, {hx + 8, sy - 22}, {hx + 10, sy + 2}, {hx - 10, sy + 2} };
    BrushPoly(hdc, blue, nemes, 4);
    PenLine(hdc, 2, gold, hx - 9, sy - 6, hx + 9, sy - 6);
    PenLine(hdc, 2, gold, hx - 10, sy - 1, hx + 10, sy - 1);
    BrushEllipse(hdc, skin, hx - 5, sy - 20, hx + 5, sy - 8);
    HBRUSH eye = CreateSolidBrush(RGB(20, 20, 25));
    RECT el = { hx - 3 + face_dir, sy - 16, hx - 1 + face_dir, sy - 14 }, er = { hx + 1 + face_dir, sy - 16, hx + 3 + face_dir, sy - 14 };
    FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
}

static void DrawBunyip(HDC hdc, int sx, int sy, int face_dir) {
    COLORREF hide = RGB(60, 70, 45);
    int step = (int)(sin(world_tick * 3.0f) * 2.0f);
    PenLine(hdc, 5, hide, sx - 11, sy + 2, sx - 11 + step, sy + 13);
    PenLine(hdc, 5, hide, sx - 4, sy + 2, sx - 4 - step, sy + 13);
    PenLine(hdc, 5, hide, sx + 4, sy + 2, sx + 4 + step, sy + 13);
    PenLine(hdc, 5, hide, sx + 11, sy + 2, sx + 11 - step, sy + 13);
    POINT tail[] = { {sx - face_dir * 14, sy}, {sx - face_dir * 26, sy - 7}, {sx - face_dir * 26, sy + 7} };
    BrushPoly(hdc, hide, tail, 3);
    BrushEllipse(hdc, hide, sx - 18, sy - 9, sx + 18, sy + 9);
    int hx = sx + face_dir * 18;
    BrushEllipse(hdc, RGB(70, 80, 50), hx - 8, sy - 16, hx + 8, sy - 2);
    PenLine(hdc, 2, RGB(235, 230, 210), hx + face_dir * 3, sy - 5, hx + face_dir * 5, sy + 3);
    PenLine(hdc, 2, RGB(235, 230, 210), hx - face_dir, sy - 5, hx, sy + 3);
    HBRUSH eye = CreateSolidBrush(RGB(200, 240, 80));
    RECT e = { hx + face_dir * 2 - 1, sy - 12, hx + face_dir * 2 + 2, sy - 10 };
    FillRect(hdc, &e, eye); DeleteObject(eye);
}

static void DrawMythicBeast(HDC hdc, int sx, int sy, int face_dir) {
    switch (active_monster) {
        case MONSTER_YETI: DrawApeBeast(hdc, sx, sy, face_dir, RGB(235, 240, 248), RGB(120, 140, 165)); break;
        case MONSTER_BIGFOOT: DrawApeBeast(hdc, sx, sy, face_dir, RGB(105, 70, 40), RGB(160, 120, 85)); break;
        case MONSTER_SPHINX: DrawSphinx(hdc, sx, sy, face_dir); break;
        default: DrawBunyip(hdc, sx, sy, face_dir); break;
    }
}

void DrawEnemiesAtTile(HDC hdc, int c, int r) {
    StoreEnemy();
    int keep = current_enemy;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= 0.0f || (int)enemies[i].x != c || (int)enemies[i].y != r) continue;
        SelectEnemy(i); DrawDynamicEnemy(hdc);
    }
    SelectEnemy(keep);
}

static void DrawSummonedSkeleton(HDC hdc, int sx, int sy, int face_dir);

// Scales everything drawn until EndScaledDraw by k about (sx, sy).
int BeginScaledDraw(HDC hdc, int sx, int sy, float k) {
    int old_mode = SetGraphicsMode(hdc, GM_ADVANCED);
    XFORM sc = { k, 0.0f, 0.0f, k, sx * (1.0f - k), sy * (1.0f - k) };
    ModifyWorldTransform(hdc, &sc, MWT_LEFTMULTIPLY);
    return old_mode;
}

void EndScaledDraw(HDC hdc, int old_mode) {
    ModifyWorldTransform(hdc, NULL, MWT_IDENTITY);
    SetGraphicsMode(hdc, old_mode);
}

// Risen dead were once people of every race, so each one stands as tall as
// the race it was (elf, human, dwarf, halfling or gnome).
static float UndeadRaceScale(int seed) {
    static const float heights[5] = { 1.95f, 1.82f, 1.42f, 1.15f, 0.98f };
    float k = heights[(unsigned int)RainHash(seed) % 5u] / 1.82f;
    return k < 0.55f ? 0.55f : k;
}

// The attacking arm: raised behind the shoulder, then brought down and
// forward, with the monster's weapon (club, sword, spear or claws) and a
// faint swoosh on the follow-through.
static void DrawEnemySwing(HDC hdc, int sx, int sy, int dir, float p) {
    float big = IS_MYTHIC_MONSTER(active_monster) ? 2.0f : 1.0f, a = -2.0f + p * 2.7f;
    int shx = sx + dir * (int)(3 * big), shy = sy + (int)(2 * big);
    int hx = shx + (int)(dir * cosf(a) * 9 * big), hy = shy + (int)(sinf(a) * 9 * big);
    COLORREF arm = active_monster == MONSTER_ZOMBIE ? RGB(150, 175, 135) : active_monster == MONSTER_GOBLIN ? RGB(90, 140, 60) :
                   active_monster == MONSTER_SKELLY ? RGB(225, 225, 215) : active_monster == MONSTER_SATYR ? RGB(185, 140, 100) : RGB(120, 110, 125);
    PenLine(hdc, (int)(3 * big), arm, shx, shy, hx, hy);
    int wx = hx + (int)(dir * cosf(a) * 14 * big), wy = hy + (int)(sinf(a) * 14 * big);
    if (active_monster == MONSTER_GOBLIN) PenLine(hdc, (int)(4 * big), RGB(110, 75, 40), hx, hy, hx + (wx - hx) * 4 / 5, hy + (wy - hy) * 4 / 5);
    else if (active_monster == MONSTER_SKELLY) PenLine(hdc, 2, RGB(200, 205, 215), hx, hy, wx, wy);
    else if (active_monster == MONSTER_SATYR) PenLine(hdc, 2, RGB(130, 95, 55), hx - (wx - hx) / 3, hy - (wy - hy) / 3, wx + (wx - hx) / 3, wy + (wy - hy) / 3);
    else for (int c = -1; c <= 1; c++) // claws
        PenLine(hdc, 1, RGB(235, 230, 210), hx, hy, hx + (int)(dir * cosf(a + c * 0.35f) * 6 * big), hy + (int)(sinf(a + c * 0.35f) * 6 * big));
    if (p > 0.45f && p < 0.95f) { // swoosh
        float a0 = a - 0.9f;
        int ox = shx + (int)(dir * cosf(a0) * 20 * big), oy = shy + (int)(sinf(a0) * 20 * big);
        PenLine(hdc, 1, RGB(230, 235, 245), ox, oy, wx, wy);
    }
}

void DrawDynamicEnemy(HDC hdc) {
    if (enemy_hearts <= 0.0f) return;
    int sx, sy; GetIsoCoords(enemy_x, enemy_y, &sx, &sy);
    int px, py; GetIsoCoords(player_x, player_y, &px, &py);
    // Monsters face the way they walk, and turn to the hero only up close.
    static float last_x[MAX_ENEMIES], last_y[MAX_ENEMIES]; static int last_dir[MAX_ENEMIES];
    int slot = current_enemy >= 0 && current_enemy < MAX_ENEMIES ? current_enemy : 0;
    float mv = (enemy_x - last_x[slot]) - (enemy_y - last_y[slot]);
    if (fabsf(mv) > 0.002f && fabsf(mv) < 2.0f) last_dir[slot] = mv > 0 ? 1 : -1;
    last_x[slot] = enemy_x; last_y[slot] = enemy_y;
    int face_dir = last_dir[slot] ? last_dir[slot] : (px >= sx ? 1 : -1);
    // Attack: up close the monster swings its arm and weapon in a short rhythm.
    float attack_swing = -1.0f;
    float adx = player_x - enemy_x, ady = player_y - enemy_y;
    if (adx * adx + ady * ady < 2.6f) {
        face_dir = (px >= sx) ? 1 : -1;
        float ph = fmodf((float)world_tick * 2.2f, 1.0f);
        if (ph < 0.45f) attack_swing = ph / 0.45f;
    }
    int sw = IS_MYTHIC_MONSTER(active_monster) ? 20 : 9;
    HBRUSH b1 = CreateSolidBrush(RGB(15, 20, 26)); HGDIOBJ old = SelectObject(hdc, b1); Ellipse(hdc, sx - sw, sy + 16, sx + sw, sy + 22); SelectObject(hdc, old); DeleteObject(b1);
    if (active_monster == MONSTER_ZOMBIE) {
        int seed = (int)(enemy_x * 13.0f + enemy_y * 7.0f);
        int arms_out = (((int)(world_tick * 0.3f) + seed) % 40) < 22; // reaches out some of the time
        int mode = BeginScaledDraw(hdc, sx, sy + 16, UndeadRaceScale(current_screen_index * 7 + current_enemy));
        DrawZombieBody(hdc, sx, sy, seed, face_dir, arms_out, RGB(165, 185, 150));
        EndScaledDraw(hdc, mode);
    } else if (active_monster == MONSTER_GOBLIN) {
        DrawGoblin(hdc, sx, sy, face_dir);
    } else if (active_monster == MONSTER_SATYR) {
        DrawSatyr(hdc, sx, sy, face_dir);
    } else if (IS_MYTHIC_MONSTER(active_monster)) {
        DrawMythicBeast(hdc, sx, sy, face_dir);
    } else if (active_monster == MONSTER_SKELLY) {
        int mode = BeginScaledDraw(hdc, sx, sy + 16, UndeadRaceScale(current_screen_index * 7 + current_enemy));
        DrawSummonedSkeleton(hdc, sx, sy, face_dir); // skull, ribs and bony limbs
        EndScaledDraw(hdc, mode);
    } else {
        // Legs stride under the robe, then the robe and skull on top.
        int step = (int)(sin(world_tick * 6.0f) * 2.0f);
        HPEN leg_pen = CreatePen(PS_SOLID, 3, active_monster == MONSTER_SKELLY ? RGB(225, 225, 215) : RGB(35, 30, 40));
        HGDIOBJ prev_leg = SelectObject(hdc, leg_pen);
        MoveToEx(hdc, sx - 3, sy + 12, NULL); LineTo(hdc, sx - 3 + step, sy + 19);
        MoveToEx(hdc, sx + 3, sy + 12, NULL); LineTo(hdc, sx + 3 - step, sy + 19);
        SelectObject(hdc, prev_leg); DeleteObject(leg_pen);
        HBRUSH robe_b = CreateSolidBrush(active_monster == MONSTER_DRACULA ? RGB(120, 20, 30) : RGB(130, 60, 160)); HGDIOBJ prev_robe = SelectObject(hdc, robe_b);
        POINT robe[] = {{sx - 5, sy}, {sx + 5, sy}, {sx + 6, sy + 14}, {sx - 6, sy + 14}}; Polygon(hdc, robe, 4); SelectObject(hdc, prev_robe); DeleteObject(robe_b);
        HBRUSH skull_b = CreateSolidBrush(RGB(220, 220, 230)); HGDIOBJ prev_skull = SelectObject(hdc, skull_b); Ellipse(hdc, sx - 5, sy - 9, sx + 5, sy + 1); SelectObject(hdc, prev_skull); DeleteObject(skull_b);
    }
    int total_bars = (int)ceil(enemy_hearts);
    int bar_y = IS_MYTHIC_MONSTER(active_monster) ? sy - 46 : sy - 30;
    for (int i = 0; i < total_bars; i++) {
        RECT r = {sx - 15 + (i * 10), bar_y, sx - 7 + (i * 10), bar_y + 7};
        HBRUSH red_b = CreateSolidBrush(RGB(255, 40, 40)); FillRect(hdc, &r, red_b); DeleteObject(red_b);
    }
    SelectObject(hdc, old);
    if (attack_swing >= 0.0f) DrawEnemySwing(hdc, sx, sy, face_dir, attack_swing);
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
    HBRUSH merchant_legs = CreateSolidBrush(RGB(80, 60, 45));
    RECT ml = { sx - 4, sy - 6, sx - 1, sy + 3 }, mr = { sx + 1, sy - 6, sx + 4, sy + 3 };
    FillRect(hdc, &ml, merchant_legs); FillRect(hdc, &mr, merchant_legs); DeleteObject(merchant_legs);
    HBRUSH merchant_body = CreateSolidBrush(RGB(215, 180, 45)); HGDIOBJ prev = SelectObject(hdc, merchant_body);
    Ellipse(hdc, sx - 6, sy - 18, sx + 6, sy - 6); SelectObject(hdc, prev); DeleteObject(merchant_body);
}

// A small, charming villager silhouette reminiscent of classic Zelda-style
// townsfolk: a simple tunic, belt, bare arms, round head and a cap/hair style
// that varies per NPC so each villager in the settlement reads as a distinct
// little character instead of an identical color-swapped blob.
static int villager_facing; // 0 faces the camera, -1/1 looks left/right, 2 walks away
void DrawZeldaStyleVillager(HDC hdc, int sx, int sy, int npc_index, float scale) {
#define VS(v) ((int)((v) * scale))
    static const COLORREF tunic_palette[] = { RGB(60, 150, 70), RGB(190, 90, 60), RGB(90, 110, 170), RGB(200, 170, 70) };
    static const COLORREF hair_palette[] = { RGB(80, 55, 35), RGB(230, 210, 120), RGB(40, 40, 45), RGB(150, 70, 40) };
    COLORREF tunic = tunic_palette[npc_index % 4];
    COLORREF hair = hair_palette[(npc_index + 1) % 4];
    COLORREF skin = RGB(235, 195, 160);

    // Two legs in dark trousers with little boots under the tunic.
    HBRUSH leg_b = CreateSolidBrush(RGB(70, 60, 55));
    RECT leg_l = { sx - VS(4), sy + VS(5), sx - VS(1), sy + VS(12) };
    RECT leg_r = { sx + VS(1), sy + VS(5), sx + VS(4), sy + VS(12) };
    FillRect(hdc, &leg_l, leg_b); FillRect(hdc, &leg_r, leg_b); DeleteObject(leg_b);
    HBRUSH boot_b = CreateSolidBrush(RGB(90, 65, 45));
    RECT boot_l = { sx - VS(5), sy + VS(11), sx - VS(1), sy + VS(14) };
    RECT boot_r = { sx + VS(1), sy + VS(11), sx + VS(5), sy + VS(14) };
    FillRect(hdc, &boot_l, boot_b); FillRect(hdc, &boot_r, boot_b); DeleteObject(boot_b);

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
    if (villager_facing == 2) Ellipse(hdc, sx - VS(4), sy - VS(14), sx + VS(4), sy - VS(7)); // back of the head
    SelectObject(hdc, old); DeleteObject(hair_b);
    if (villager_facing != 2) {
        HBRUSH eye = CreateSolidBrush(RGB(30, 25, 25));
        int ex = sx + villager_facing * VS(1.5f);
        RECT el = { ex - VS(2.5f), sy - VS(10), ex - VS(1), sy - VS(8.5f) }, er = { ex + VS(1), sy - VS(10), ex + VS(2.5f), sy - VS(8.5f) };
        FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
    }
#undef VS
}

static POINT IsoAt(float x, float y, int z) {
    int sx, sy; GetIsoCoords(x, y, &sx, &sy);
    POINT p = { sx, sy - z };
    return p;
}

// A village home as a solid isometric cottage: two lit walls (the gable end
// and the long front with the door), a thatched roof over a ridge, a chimney,
// and windows that glow at night. Its footprint is exactly the solid area.
static void DrawIsoHome(HDC hdc, float cx, float cy, int i) {
    float x0 = cx - HOME_HALF_X, x1 = cx + HOME_HALF_X, y0 = cy - HOME_HALF_Y, y1 = cy + HOME_HALF_Y, e = 0.15f;
    const int H = 58, R = 104;
    int night = fabs(sin(day_night_cycle_accumulator)) < 0.35;
    static const COLORREF roofs[4] = { RGB(196, 152, 74), RGB(170, 70, 50), RGB(120, 110, 100), RGB(186, 140, 70) };
    COLORREF roof = roofs[i & 3], roof_back = RGB(GetRValue(roof) * 4 / 5, GetGValue(roof) * 4 / 5, GetBValue(roof) * 4 / 5);
    COLORREF timber = RGB(95, 62, 36), glass = night ? RGB(255, 205, 110) : RGB(150, 185, 210);
    POINT back[] = { IsoAt(x0 - e, y0 - e, H - 6), IsoAt(x1 + e, y0 - e, H - 6), IsoAt(x1 + e, cy, R), IsoAt(x0 - e, cy, R) };
    BrushPoly(hdc, roof_back, back, 4);
    POINT chim[] = { IsoAt(x0 + 0.45f, y0 + 0.3f, 76), IsoAt(x0 + 0.75f, y0 + 0.3f, 76), IsoAt(x0 + 0.75f, y0 + 0.3f, 122), IsoAt(x0 + 0.45f, y0 + 0.3f, 122) };
    BrushPoly(hdc, RGB(125, 125, 130), chim, 4);
    POINT gable[] = { IsoAt(x1, y0, 0), IsoAt(x1, y1, 0), IsoAt(x1, y1, H), IsoAt(x1, cy, R), IsoAt(x1, y0, H) };
    BrushPoly(hdc, RGB(170, 130, 88), gable, 5);
    POINT front[] = { IsoAt(x0, y1, 0), IsoAt(x1, y1, 0), IsoAt(x1, y1, H), IsoAt(x0, y1, H) };
    BrushPoly(hdc, RGB(208, 172, 122), front, 4);
    POINT a = IsoAt(x0, y1, H / 2), b = IsoAt(x1, y1, H / 2), c0 = IsoAt(x1, y1, 0), c1 = IsoAt(x1, y1, H);
    PenLine(hdc, 2, timber, a.x, a.y, b.x, b.y);
    PenLine(hdc, 3, timber, c0.x, c0.y, c1.x, c1.y);
    POINT door[] = { IsoAt(cx - 0.2f, y1, 0), IsoAt(cx + 0.2f, y1, 0), IsoAt(cx + 0.2f, y1, 38), IsoAt(cx - 0.2f, y1, 38) };
    BrushPoly(hdc, RGB(80, 52, 32), door, 4);
    POINT w1[] = { IsoAt(x0 + 0.25f, y1, 34), IsoAt(x0 + 0.6f, y1, 34), IsoAt(x0 + 0.6f, y1, 50), IsoAt(x0 + 0.25f, y1, 50) };
    POINT w2[] = { IsoAt(x1, cy - 0.22f, 26), IsoAt(x1, cy + 0.22f, 26), IsoAt(x1, cy + 0.22f, 44), IsoAt(x1, cy - 0.22f, 44) };
    BrushPoly(hdc, glass, w1, 4); BrushPoly(hdc, glass, w2, 4);
    POINT fr[] = { IsoAt(x0 - e, y1 + e, H - 6), IsoAt(x1 + e, y1 + e, H - 6), IsoAt(x1 + e, cy, R), IsoAt(x0 - e, cy, R) };
    BrushPoly(hdc, roof, fr, 4);
    PenLine(hdc, 2, roof_back, fr[3].x, fr[3].y, fr[2].x, fr[2].y);
}

void DrawVillage(HDC hdc) {
    float hcx, hcy;
    if (VillageHomeCentre(0, &hcx, &hcy)) {
        for (int i = 0; i < 4; i++) if (VillageHomeCentre(i, &hcx, &hcy)) DrawIsoHome(hdc, hcx, hcy, i);
        // The hero is drawn again when standing in front of a home.
        float fx = player_x + HOME_FEET_OFFSET, fy = player_y + HOME_FEET_OFFSET;
        for (int i = 0; i < 4; i++)
            if (VillageHomeCentre(i, &hcx, &hcy) && fx + fy > hcx + hcy + 0.3f && fabsf((fx - fy) - (hcx - hcy)) < HOME_HALF_X + HOME_HALF_Y + 0.7f) { DrawHeroAsset(hdc); break; }
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
        else if (village_npcs[i].type >= 6) look = village_npcs[i].type; // king, beggar, banker
        if (village_npcs[i].type == 8) DrawAltar(hdc, sx, sy, TownDeity());
        else {
            int d = village_npcs[i].direction; // iso: +y walks screen-left, +x screen-right; up/left turn their backs
            villager_facing = d == FACE_DOWN ? -1 : d == FACE_RIGHT ? 1 : 2;
            DrawZeldaStyleVillager(hdc, sx, sy, look, NPC_SCALE);
            villager_facing = 0;
        }
        if (village_npcs[i].type == 6) DrawCrown(hdc, sx, sy - (int)(19 * NPC_SCALE) - 2, 6);
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

static void DrawSummonedSkeleton(HDC hdc, int sx, int sy, int face_dir) {
    COLORREF bone = RGB(230, 228, 215);
    int step = (int)(sin(world_tick * 6.0f) * 2.0f);
    PenLine(hdc, 2, bone, sx - 3, sy + 8, sx - 3 + step, sy + 19);
    PenLine(hdc, 2, bone, sx + 3, sy + 8, sx + 3 - step, sy + 19);
    PenLine(hdc, 2, bone, sx, sy - 2, sx, sy + 9);
    for (int r = 0; r < 3; r++) PenLine(hdc, 1, bone, sx - 5, sy + r * 3, sx + 5, sy + r * 3);
    PenLine(hdc, 2, bone, sx, sy + 1, sx + face_dir * 9, sy + 4);
    PenLine(hdc, 2, RGB(170, 175, 185), sx + face_dir * 9, sy + 4, sx + face_dir * 15, sy - 6);
    BrushEllipse(hdc, bone, sx - 5, sy - 11, sx + 5, sy - 1);
    HBRUSH eye = CreateSolidBrush(RGB(30, 25, 35));
    RECT el = { sx - 3 + face_dir, sy - 8, sx - 1 + face_dir, sy - 6 }, er = { sx + 1 + face_dir, sy - 8, sx + 3 + face_dir, sy - 6 };
    FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
}

static void DrawGhostShape(HDC hdc, int sx, int sy, int face_dir) {
    sy += (int)(sin(world_tick * 3.0f) * 3.0f) - 4;
    COLORREF mist = RGB(200, 225, 240);
    BrushEllipse(hdc, mist, sx - 8, sy - 12, sx + 8, sy + 6);
    POINT tail[] = { {sx - 8, sy}, {sx + 8, sy}, {sx + 6, sy + 14}, {sx + 2, sy + 9}, {sx - 2, sy + 15}, {sx - 6, sy + 9} };
    BrushPoly(hdc, mist, tail, 6);
    HBRUSH eye = CreateSolidBrush(RGB(40, 50, 70));
    RECT el = { sx - 4 + face_dir * 2, sy - 6, sx - 1 + face_dir * 2, sy - 2 }, er = { sx + 1 + face_dir * 2, sy - 6, sx + 4 + face_dir * 2, sy - 2 };
    FillRect(hdc, &el, eye); FillRect(hdc, &er, eye); DeleteObject(eye);
}

// The ghost is drawn onto a copy of the background, then blended back at
// about 40% so the scene clearly shows through it.
static void DrawSummonedGhost(HDC hdc, int sx, int sy, int face_dir) {
    const int W = 32, H = 40, ox = sx - 16, oy = sy - 22;
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HGDIOBJ old_bmp = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, W, H, hdc, ox, oy, SRCCOPY);
    DrawGhostShape(mem, 16, 22, face_dir);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 105, 0 };
    AlphaBlend(hdc, ox, oy, W, H, mem, 0, 0, W, H, bf);
    SelectObject(mem, old_bmp); DeleteObject(bmp); DeleteDC(mem);
}

void DrawSummonedZombie(HDC hdc) {
    if (!summoned_zombie.active) return;
    int sx, sy; GetIsoCoords(summoned_zombie.x, summoned_zombie.y, &sx, &sy);
    int rise = (summoned_zombie.raise_frame < 6) ? 12 - summoned_zombie.raise_frame * 2 : 0;
    sy += rise;
    // The summoned minion always walks with its arms held straight out,
    // reaching toward the enemy it is hunting (or ahead, if there is none).
    int face_dir = 1;
    if (enemy_hearts > 0.0f) { int ex, ey; GetIsoCoords(enemy_x, enemy_y, &ex, &ey); face_dir = (ex >= sx) ? 1 : -1; }
    // Attack lunge for the first few frames after each blow it lands.
    int full = summoned_zombie.type == SUMMON_GHOST ? 24 : 30, since = full - summoned_zombie.attack_cooldown;
    if (summoned_zombie.attack_cooldown > 0 && since < 8) sx += face_dir * (since < 4 ? since : 8 - since) * 3;
    // Summons stand as tall as the hero's race (gnome dead are small, elves tall).
    float k = player_height_m / 1.82f; if (k < 0.55f) k = 0.55f;
    int old_mode = BeginScaledDraw(hdc, sx, sy + 16, k);
    if (summoned_zombie.type == SUMMON_SKELETON) DrawSummonedSkeleton(hdc, sx, sy, face_dir);
    else if (summoned_zombie.type == SUMMON_GHOST) DrawSummonedGhost(hdc, sx, sy, face_dir);
    else DrawZombieBody(hdc, sx, sy, (int)(summoned_zombie.x * 11.0f + summoned_zombie.y * 5.0f), face_dir, 1, RGB(150, 170, 130));
    EndScaledDraw(hdc, old_mode);
}
