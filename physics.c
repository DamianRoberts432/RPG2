#include "game.h"

#define PLAYER_COLLISION_RADIUS 0.3f
#define YETI_AGGRO_RADIUS 3.0f
#define YETI_CHARGE_SPEED 0.04f
#define POISON_TICK_DAMAGE 0.2f
#define WINDED_STAMINA_REGEN 0.3f
static int TileBlocksPlayer(float x, float y) {
    if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE) return 1;
    uint8_t t = VISUAL_MAP[(int)y][(int)x];
    return t == 1 || (t == 12 && !HasInventoryItem("Raft") && !HasInventoryItem("Canoe") && !HasInventoryItem("Boat"));
}

// Centre must be passable, and so must the surrounding 3x3 sample grid at the
// safety radius, so the hero never wedges against wall/water tile edges.
int PlayerCanStandAt(float x, float y) {
    int i, j;
    if (TileBlocksPlayer(x, y)) return 0;
    for (j = -1; j <= 1; j++) for (i = -1; i <= 1; i++) {
        if (i == 0 && j == 0) continue;
        if (TileBlocksPlayer(x + i * PLAYER_COLLISION_RADIUS, y + j * PLAYER_COLLISION_RADIUS)) return 0;
    }
    return 1;
}

// Full move, else slide along one axis; if already wedged, accept any move
// whose centre is free so the hero can always work loose.
float ice_vx = 0.0f, ice_vy = 0.0f;

int IsOnSlipperyGround(void) {
    if (in_cave || current_biome == BIOME_CAVE) return 0;
    return current_biome == BIOME_TUNDRA || snow_cover > 0.6f;
}

int PlayerTryMove(float tx, float ty, float* ox, float* oy) {
    int stuck = !PlayerCanStandAt(player_x, player_y);
    if (PlayerCanStandAt(tx, ty) || (stuck && !TileBlocksPlayer(tx, ty))) { *ox = tx; *oy = ty; return 1; }
    if (PlayerCanStandAt(tx, player_y) || (stuck && !TileBlocksPlayer(tx, player_y))) { *ox = tx; *oy = player_y; return 1; }
    if (PlayerCanStandAt(player_x, ty) || (stuck && !TileBlocksPlayer(player_x, ty))) { *ox = player_x; *oy = ty; return 1; }
    return 0;
}

void UpdateGamePhysics(void) {
    if (is_character_creation) return;
    world_frame++;
    world_tick += 0.05f; if (sword_swipe_frame > 0) sword_swipe_frame--;
    if (ability_visual_frame > 0) ability_visual_frame--;
    if (sleep_fade_frame > 0) {
        sleep_fade_frame++;
        if (sleep_fade_frame == 30) {
            player_hp = max_player_hp; player_stamina = max_stamina; player_mp = max_player_mp;
            int dawn = SleepToNextPhase();
            sprintf(arpg_action_log, "You wake at %s: %s, Day %d of %d (%s).", dawn ? "dawn" : "dusk", season_names[current_season],
                    SeasonDay(), DAYS_PER_SEASON, CalendarMonthName());
            night_bonus_active = 1;
            RestockMerchant();
            KarmaAdjust(0, -20, -10, 0); // a night's rest clears miasma and cools town heat
            if (jailed_tonight) {
                jailed_tonight = 0;
                sprintf(arpg_action_log, "The guards release you at dawn. Town Heat: %d.", karma.town_heat);
            }
        } else if (sleep_fade_frame >= 60) sleep_fade_frame = 0;
    }
    UpdateSeasonAndWeather();
    for (int i = 0; i < MAX_CAMPFIRES; i++) if (campfires[i].active) {
        if (campfires[i].screen_id != current_screen_index || --campfires[i].timer <= 0.0f) {
            campfires[i].active = 0;
            if (bedroll_screen_id == current_screen_index && fabs(bedroll_x - campfires[i].x - 1.0f) < 0.01f &&
                fabs(bedroll_y - campfires[i].y) < 0.01f) bedroll_screen_id = -1;
        }
    }
    UpdateVillageNPCs();
    UpdateCritters();
    UpdateFootpaths();
    ExpireDroppedItems();
    UpdateIceSlide();
    UpdateDebrisTwigs();
    if (is_running) {
        stamina_rest_timer = 0;
    } else if (stamina_rest_timer < STAMINA_REST_FRAMES_REQUIRED) {
        stamina_rest_timer++;
    }
    if (player_mp < max_player_mp) {
        player_mp += 0.06f + (stat_magick + stat_intelligence) * 0.01f; // MAG and INT both speed MP regen
        if (player_mp > max_player_mp) player_mp = max_player_mp;
    }
    if (player_stamina < max_stamina && GetTickCount() >= bow_stamina_lock_until) {
        float stamina_floor = max_stamina * 0.30f;
        // Full recovery needs a short pause; while still swinging or running
        // below 30% only a slow trickle comes back, so the hero does tire out.
        if (stamina_rest_timer >= STAMINA_REST_FRAMES_REQUIRED) player_stamina += 1.5f + stat_stamina * 0.1f;
        else if (player_stamina < stamina_floor) player_stamina += WINDED_STAMINA_REGEN;
        if (player_stamina > max_stamina) player_stamina = max_stamina;
    }

    // Resting by a lit campfire slowly refills HP, MP and stamina (sleeping refills them all).
    if (FindNearbyCampfire(2.5f) >= 0) {
        if (player_hp < max_player_hp) { player_hp += 0.02f; if (player_hp > max_player_hp) player_hp = max_player_hp; }
        if (player_mp < max_player_mp) { player_mp += 0.05f; if (player_mp > max_player_mp) player_mp = max_player_mp; }
        if (player_stamina < max_stamina) { player_stamina += 0.5f; if (player_stamina > max_stamina) player_stamina = max_stamina; }
    }

    DWORD now_tick = GetTickCount();
    // Signed-subtraction comparisons keep these correct across GetTickCount()
    // wraparound (~49.7 days uptime), consistent with FireClassAbility().
    long suppression_remaining_ms = (long)(monster_pursuit_suppressed_until - now_tick);
    if (is_sneaking && suppression_remaining_ms <= 0) {
        is_sneaking = 0;
    }
    int pursuit_suppressed = (suppression_remaining_ms > 0);

    for (int i = 0; i < MAX_BLOOD_MIST; i++) {
        if (blood_mist[i].active) {
            blood_mist[i].x += blood_mist[i].vx;
            blood_mist[i].y += blood_mist[i].vy;
            blood_mist[i].life--;
            if (blood_mist[i].life <= 0) blood_mist[i].active = 0;
        }
    }

    if (player_spell.active) {
        player_spell.x += player_spell.vx;
        player_spell.y += player_spell.vy;
        if (EnemyAtPoint(player_spell.x, player_spell.y, 0.5f) >= 0) {
            player_spell.active = 0; hit_splash_color = selected_class == CLASS_WIZARD ? RGB(80, 150, 255) : RGB(170, 80, 230); HandleEnemyDamage(player_spell.damage);
        } else if (player_spell.x < 0 || player_spell.x >= MAP_SIZE || player_spell.y < 0 || player_spell.y >= MAP_SIZE) {
            player_spell.active = 0;
        }
    }

    if (summoned_zombie.active) {
        summoned_zombie.timer--;
        if (summoned_zombie.raise_frame < 6) summoned_zombie.raise_frame++;
        if (summoned_zombie.timer <= 0 || summoned_zombie.hp <= 0) summoned_zombie.active = 0;
        else if (SelectNearestEnemyTo(summoned_zombie.x, summoned_zombie.y) >= 0) {
            float zx = enemy_x - summoned_zombie.x, zy = enemy_y - summoned_zombie.y;
            float zd = (float)sqrt(zx * zx + zy * zy);
            static const float summon_speed[3] = { 0.03f, 0.045f, 0.06f }, summon_dmg[3] = { 1.0f, 0.75f, 0.5f };
            int st = summoned_zombie.type;
            if (zd > 0.65f) {
                summoned_zombie.x += (zx / zd) * summon_speed[st];
                summoned_zombie.y += (zy / zd) * summon_speed[st];
            } else if (summoned_zombie.attack_cooldown > 0) summoned_zombie.attack_cooldown--;
            else {
                summoned_zombie.attack_cooldown = st == SUMMON_GHOST ? 24 : 30;
                // Bigger races raise bigger dead: damage follows hero height, +-15%.
                float race_k = player_height_m / 1.82f;
                hit_splash_color = st == SUMMON_ZOMBIE ? RGB(90, 200, 60) : st == SUMMON_GHOST ? RGB(170, 240, 255) : RGB(235, 230, 210);
                HandleEnemyDamage(summon_dmg[st] * (0.75f + 0.25f * race_k) * (0.85f + (rand() % 31) / 100.0f));
                if (st == SUMMON_ZOMBIE) enemies[current_enemy].poison = 3;
                else if (rand() % 100 < 15) enemies[current_enemy].poison = 2;
            }
        }
    }

    // Cave entry is never automatic - stepping near the crevice only shows the
    // "CAVE" prompt (drawn in WinMain); the player must press the interact
    // button (HandleContextInteract -> EnterCave) to actually go inside.

    // Enemy AI with Campfire Light Deterrent, run for each monster on screen.
    StoreEnemy();
    for (int e = 0; e < MAX_ENEMIES; e++) {
    SelectEnemy(e);
    if (enemy_hearts > 0.0f) {
        if (enemies[e].poison > 0 && world_frame % 45 == 0) {
            enemies[e].poison--;
            enemy_hearts -= POISON_TICK_DAMAGE;
            if (enemy_hearts <= 0.0f) { HandleEnemyDamage(0.0f); continue; }
        }
        int fleeing_from_fire = 0;
        float fire_fx = 0.0f, fire_fy = 0.0f;
        
        for (int i = 0; i < MAX_CAMPFIRES; i++) {
            if (!campfires[i].active) continue;
            float c_dx = enemy_x - campfires[i].x;
            float c_dy = enemy_y - campfires[i].y;
            float c_dist = (float)sqrt(c_dx * c_dx + c_dy * c_dy);
            if (c_dist < 3.5f) {
                fleeing_from_fire = 1;
                fire_fx = c_dx;
                fire_fy = c_dy;
                break;
            }
        }

        // Village fires and torches keep most monsters out; about one in four braves the light.
        if (!fleeing_from_fire && (e + current_screen_index) % 4 != 0) {
            for (int v = 0; v < village_light_count; v++) {
                float v_dx = enemy_x - village_lights[v].x, v_dy = enemy_y - village_lights[v].y;
                if (v_dx * v_dx + v_dy * v_dy < (village_lights[v].torch ? 9.0f : 25.0f)) { fleeing_from_fire = 1; fire_fx = v_dx; fire_fy = v_dy; break; }
            }
        }
        if (fleeing_from_fire) {
            float f_dist = (float)sqrt(fire_fx * fire_fx + fire_fy * fire_fy);
            if (f_dist > 0.01f) {
                enemy_x += (fire_fx / f_dist) * 0.035f;
                enemy_y += (fire_fy / f_dist) * 0.035f;
            }
        } else if (pursuit_suppressed || (is_charging_bow && GetTickCount() - bow_charge_started_at < 500)) {
            // Rogue sneak / Knight-Warrior defensive backstep: monster pursuit
            // and detection are temporarily broken/suppressed.
        } else {
            float target_x = player_x, target_y = player_y;
            float pdx = player_x - enemy_x, pdy = player_y - enemy_y;
            float player_dist = (float)sqrt(pdx * pdx + pdy * pdy);
            // Yeti: a calm wanderer until the hero comes close or strikes it.
            int calm = 0;
            if (active_monster == MONSTER_YETI && !enemies[e].angry) {
                if (player_dist < YETI_AGGRO_RADIUS) { enemies[e].angry = 1; strcpy(arpg_action_log, "The Yeti roars and charges!"); }
                else calm = 1;
            }
            float zdx = summoned_zombie.x - enemy_x, zdy = summoned_zombie.y - enemy_y;
            float zombie_dist = summoned_zombie.active ? (float)sqrt(zdx * zdx + zdy * zdy) : 1000.0f;
            if (zombie_dist < player_dist) { target_x = summoned_zombie.x; target_y = summoned_zombie.y; }
            float s_dx = target_x - enemy_x, s_dy = target_y - enemy_y;
            float dist = (float)sqrt(s_dx * s_dx + s_dy * s_dy);
            if (!calm && dist <= 8.0f && dist > 0.2f) {
                static const float speeds[] = { 0.0275f, 0.014f, 0.03f, 0.034f, 0.03f, 0.022f, 0.02f, 0.024f, 0.018f };
                float speed = speeds[active_monster];
                if (active_monster == MONSTER_YETI) speed = YETI_CHARGE_SPEED;
                if (fabs(sin(day_night_cycle_accumulator)) < 0.35f) speed *= 1.2f;
                if (VISUAL_MAP[(int)enemy_y][(int)enemy_x] == 12) speed *= 0.25f;
                enemy_x += (s_dx / dist) * speed;
                enemy_y += (s_dy / dist) * speed;
                if (summoned_zombie.active && zombie_dist < player_dist && zombie_dist < 0.7f &&
                    world_frame % 40 == 0 && summoned_zombie.type != SUMMON_GHOST) summoned_zombie.hp -= 0.5f; // ghosts phase through blows
            } else if ((calm || dist > 8.0f) && world_frame % 10 == 0) {
                static const int dir_lut[4][3] = {
                    { FACE_LEFT, FACE_DOWN, FACE_RIGHT }, { FACE_RIGHT, FACE_UP, FACE_LEFT },
                    { FACE_DOWN, FACE_LEFT, FACE_UP }, { FACE_UP, FACE_RIGHT, FACE_DOWN }
                };
                int ex = (int)enemy_x, ey = (int)enemy_y;
                int tile = VISUAL_MAP[ey][ex];
                int turn = tile == 1 ? 0 : tile == 12 ? 2 : 1;
                enemy_direction = dir_lut[enemy_direction][turn];
                float vx[] = { 0, 0, -1, 1 }, vy[] = { 1, -1, 0, 0 };
                float tx = enemy_x + vx[enemy_direction] * 0.04f;
                float ty = enemy_y + vy[enemy_direction] * 0.04f;
                if (VISUAL_MAP[(int)ty][(int)tx] != 1 && VISUAL_MAP[(int)ty][(int)tx] != 12) { enemy_x = tx; enemy_y = ty; }
            }
        }
        // Keep monsters from stacking on top of one another.
        for (int o = 0; o < MAX_ENEMIES; o++) {
            if (o == e || enemies[o].hearts <= 0.0f) continue;
            float ox = enemy_x - enemies[o].x, oy = enemy_y - enemies[o].y;
            float od = (float)sqrt(ox * ox + oy * oy);
            if (od < 0.7f && od > 0.001f) {
                float nx = enemy_x + (ox / od) * 0.03f, ny = enemy_y + (oy / od) * 0.03f;
                if (nx > 0.5f && ny > 0.5f && nx < MAP_SIZE - 0.5f && ny < MAP_SIZE - 0.5f &&
                    VISUAL_MAP[(int)ny][(int)nx] != 1 && VISUAL_MAP[(int)ny][(int)nx] != 12) { enemy_x = nx; enemy_y = ny; }
            }
        }
    }
    StoreEnemy();
    }
    UpdateEnemySpawns();
}

void UpdateIceSlide(void) {
    if (!IsOnSlipperyGround() || (fabsf(ice_vx) < 0.002f && fabsf(ice_vy) < 0.002f)) { ice_vx = ice_vy = 0.0f; return; }
    float nx, ny;
    if (PlayerTryMove(player_x + ice_vx, player_y + ice_vy, &nx, &ny)) {
        if (nx == player_x) ice_vx = 0.0f; // bumped into something on that axis
        if (ny == player_y) ice_vy = 0.0f;
        player_x = nx; player_y = ny;
    } else ice_vx = ice_vy = 0.0f;
    ice_vx *= 0.99f; ice_vy *= 0.99f;
}
