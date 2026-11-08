#include "game.h"

#define PLAYER_COLLISION_RADIUS FX(0.3)
#define YETI_AGGRO_RADIUS FxI(3)
#define YETI_CHARGE_SPEED FX(0.04)
#define POISON_TICK_DAMAGE FX(0.2)
#define WINDED_STAMINA_REGEN FX(0.45)
// Prairie villages have four homes; castle towns have none (just stalls).
int VillageHomeCentre(int i, fx *x, fx *y) {
    if (i == 4) return PlayerHomeCentre(x, y);
    static const int hx[4] = { 11, 15, 19, 15 }, hy[4] = { 11, 10, 11, 18 };
    if (i < 0 || i >= 4 || in_cave || current_biome != BIOME_PRAIRIE || village_npc_count == 0 || VillageStageHere() < 1) return 0;
    *x = FxI(hx[i] + (current_screen_index % 3) - 1) + FX(0.5); *y = FxI(hy[i]) + FX(0.5);
    return 1;
}

int TileBlocksPlayer(fx x, fx y) {
    if (x < 0 || x >= FxI(MAP_SIZE) || y < 0 || y >= FxI(MAP_SIZE)) return 1;
    if (inside_home >= 0) return InteriorBlocks(x, y);
    uint8_t t = VISUAL_MAP[FxTrunc(y)][FxTrunc(x)];
    if (VillageBlocksAt(FxTrunc(x), FxTrunc(y))) return 1;
    for (int i = 0; i < 5; i++) { // village homes are solid exactly where they are drawn
        fx hx, hy;
        if (VillageHomeCentre(i, &hx, &hy) && FxAbs(x + HOME_FEET_OFFSET - hx) < HOME_HALF_X - FX(0.15) && FxAbs(y + HOME_FEET_OFFSET - hy) < HOME_HALF_Y - FX(0.15)) return 1;
    }
    if (t == 12 && AtSea()) return !HasInventoryItem("Boat"); // open sea: rafts and canoes stay by the shore
    return t == 1 || (t == 12 && !HasInventoryItem("Raft") && !HasInventoryItem("Canoe") && !HasInventoryItem("Boat"));
}

// Centre must be passable, and so must the surrounding 3x3 sample grid at the
// safety radius, so the hero never wedges against wall/water tile edges.
int PlayerCanStandAt(fx x, fx y) {
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
fx ice_vx = FxI(0), ice_vy = FxI(0);

int IsOnSlipperyGround(void) {
    if (in_cave || current_biome == BIOME_CAVE) return 0;
    return current_biome == BIOME_TUNDRA || snow_cover > FX(0.6);
}

int PlayerTryMove(fx tx, fx ty, fx* ox, fx* oy) {
    int stuck = !PlayerCanStandAt(player_x, player_y);
    if (PlayerCanStandAt(tx, ty) || (stuck && !TileBlocksPlayer(tx, ty))) { *ox = tx; *oy = ty; return 1; }
    if (PlayerCanStandAt(tx, player_y) || (stuck && !TileBlocksPlayer(tx, player_y))) { *ox = tx; *oy = player_y; return 1; }
    if (PlayerCanStandAt(player_x, ty) || (stuck && !TileBlocksPlayer(player_x, ty))) { *ox = player_x; *oy = ty; return 1; }
    return 0;
}

void UpdateGamePhysics(void) {
    if (is_character_creation) return;
    world_frame++;
    world_tick = FxWrapAdd(world_tick, FX(0.05)); if (sword_swipe_frame > 0) sword_swipe_frame--;
    if (hero_hop_frame > 0) hero_hop_frame--;
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
        if (campfires[i].screen_id != current_screen_index || --campfires[i].timer <= 0) {
            campfires[i].active = 0;
            if (bedroll_screen_id == current_screen_index && FxAbs(bedroll_x - campfires[i].x - FxI(1)) < FX(0.01) &&
                FxAbs(bedroll_y - campfires[i].y) < FX(0.01)) bedroll_screen_id = -1;
        }
    }
    UpdateVillageNPCs();
    UpdateCritters();
    UpdateFootpaths();
    UpdateTownGrowth();
    UpdateCacheDiggers();
    ExpireDroppedItems();
    UpdateIceSlide();
    UpdateDebrisTwigs();
    if (is_running) {
        stamina_rest_timer = 0;
    } else if (stamina_rest_timer < STAMINA_REST_FRAMES_REQUIRED) {
        stamina_rest_timer++;
    }
    if (player_mp < max_player_mp) {
        player_mp += FX(0.06) + FX(0.01) * (stat_magick + stat_intelligence); // MAG and INT both speed MP regen
        if (player_mp > max_player_mp) player_mp = max_player_mp;
    }
    if (player_stamina < max_stamina && GetTickCount() >= bow_stamina_lock_until) {
        fx stamina_floor = FxMul(max_stamina, FX(0.30));
        // Full recovery needs a short pause; while still swinging or running
        // below 30% only a slow trickle comes back, so the hero does tire out.
        if (stamina_rest_timer >= STAMINA_REST_FRAMES_REQUIRED) player_stamina += FX(1.5) + FX(0.1) * stat_stamina;
        else if (player_stamina < stamina_floor) player_stamina += WINDED_STAMINA_REGEN;
        if (player_stamina > max_stamina) player_stamina = max_stamina;
    }

    // Resting by a lit campfire slowly refills HP, MP and stamina (sleeping refills them all).
    if (FindNearbyCampfire(FX(2.5)) >= 0) {
        if (player_hp < max_player_hp) { player_hp += FX(0.02); if (player_hp > max_player_hp) player_hp = max_player_hp; }
        if (player_mp < max_player_mp) { player_mp += FX(0.05); if (player_mp > max_player_mp) player_mp = max_player_mp; }
        if (player_stamina < max_stamina) { player_stamina += FX(0.5); if (player_stamina > max_stamina) player_stamina = max_stamina; }
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
        if (EnemyAtPoint(player_spell.x, player_spell.y, FX(0.5)) >= 0) {
            player_spell.active = 0; hit_splash_color = selected_class == CLASS_WIZARD ? RGB(80, 150, 255) : RGB(170, 80, 230); HandleEnemyDamage(player_spell.damage);
        } else if (player_spell.x < 0 || player_spell.x >= FxI(MAP_SIZE) || player_spell.y < 0 || player_spell.y >= FxI(MAP_SIZE)) {
            player_spell.active = 0;
        }
    }

    if (summoned_zombie.active) {
        summoned_zombie.timer--;
        if (summoned_zombie.raise_frame < 6) summoned_zombie.raise_frame++;
        if (summoned_zombie.timer <= 0 || summoned_zombie.hp <= 0) summoned_zombie.active = 0;
        else if (SelectNearestEnemyTo(summoned_zombie.x, summoned_zombie.y) >= 0) {
            fx zx = enemy_x - summoned_zombie.x, zy = enemy_y - summoned_zombie.y;
            fx zd = FxLen(zx, zy);
            static const fx summon_speed[3] = { FX(0.03), FX(0.045), FX(0.06) }, summon_dmg[3] = { FxI(1), FX(0.75), FX(0.5) };
            int st = summoned_zombie.type;
            if (zd > FX(0.65)) {
                summoned_zombie.x += FxMul(FxDiv(zx, zd), summon_speed[st]);
                summoned_zombie.y += FxMul(FxDiv(zy, zd), summon_speed[st]);
            } else if (summoned_zombie.attack_cooldown > 0) summoned_zombie.attack_cooldown--;
            else {
                summoned_zombie.attack_cooldown = st == SUMMON_GHOST ? 24 : 30;
                // Bigger races raise bigger dead: damage follows hero height, +-15%.
                fx race_k = FxDiv(player_height_m, FX(1.82));
                hit_splash_color = st == SUMMON_ZOMBIE ? RGB(90, 200, 60) : st == SUMMON_GHOST ? RGB(170, 240, 255) : RGB(235, 230, 210);
                minion_kill = 1;
                HandleEnemyDamage(FxMul(FxMul(summon_dmg[st], FX(0.75) + FxMul(FX(0.25), race_k)), FX(0.85) + FxI(rand() % 31) / 100));
                minion_kill = 0;
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
    if (enemy_hearts > FxI(0)) {
        if (enemies[e].poison > 0 && world_frame % 45 == 0) {
            enemies[e].poison--;
            enemy_hearts -= POISON_TICK_DAMAGE;
            if (enemy_hearts <= FxI(0)) { HandleEnemyDamage(FxI(0)); continue; }
        }
        EnemyAttackHero(e);
        int fleeing_from_fire = 0;
        fx fire_fx = FxI(0), fire_fy = FxI(0);
        
        for (int i = 0; i < MAX_CAMPFIRES; i++) {
            if (!campfires[i].active) continue;
            fx c_dx = enemy_x - campfires[i].x;
            fx c_dy = enemy_y - campfires[i].y;
            fx c_dist = FxLen(c_dx, c_dy);
            if (c_dist < FX(3.5)) {
                fleeing_from_fire = 1;
                fire_fx = c_dx;
                fire_fy = c_dy;
                break;
            }
        }

        // Village fires and torches keep most monsters out; about one in four braves the light.
        if (!fleeing_from_fire && (e + current_screen_index) % 4 != 0) {
            for (int v = 0; v < village_light_count; v++) {
                fx v_dx = enemy_x - village_lights[v].x, v_dy = enemy_y - village_lights[v].y;
                if (FxMul(v_dx, v_dx) + FxMul(v_dy, v_dy) < (village_lights[v].torch ? FxI(9) : FxI(25))) { fleeing_from_fire = 1; fire_fx = v_dx; fire_fy = v_dy; break; }
            }
        }
        if (fleeing_from_fire) {
            fx f_dist = FxLen(fire_fx, fire_fy);
            if (f_dist > FX(0.01)) {
                enemy_x += FxMul(FxDiv(fire_fx, f_dist), FX(0.035));
                enemy_y += FxMul(FxDiv(fire_fy, f_dist), FX(0.035));
            }
        } else if (pursuit_suppressed || (is_charging_bow && GetTickCount() - bow_charge_started_at < 500)) {
            // Rogue sneak / Knight-Warrior defensive backstep: monster pursuit
            // and detection are temporarily broken/suppressed.
        } else {
            fx target_x = player_x, target_y = player_y;
            fx pdx = player_x - enemy_x, pdy = player_y - enemy_y;
            fx player_dist = FxLen(pdx, pdy);
            // Yeti: a calm wanderer until the hero comes close or strikes it.
            int calm = 0;
            if (active_monster == MONSTER_YETI && !enemies[e].angry) {
                if (player_dist < YETI_AGGRO_RADIUS) { enemies[e].angry = 1; strcpy(arpg_action_log, "The Yeti roars and charges!"); }
                else calm = 1;
            }
            fx zdx = summoned_zombie.x - enemy_x, zdy = summoned_zombie.y - enemy_y;
            fx zombie_dist = summoned_zombie.active ? FxLen(zdx, zdy) : FxI(1000);
            if (zombie_dist < player_dist) { target_x = summoned_zombie.x; target_y = summoned_zombie.y; }
            fx s_dx = target_x - enemy_x, s_dy = target_y - enemy_y;
            fx dist = FxLen(s_dx, s_dy);
            if (!calm && dist <= FxI(8) && dist > FX(0.2)) {
                static const fx speeds[] = { FX(0.0275), FX(0.014), FX(0.03), FX(0.034), FX(0.03), FX(0.022), FX(0.02), FX(0.024), FX(0.018) };
                fx speed = speeds[active_monster];
                if (active_monster == MONSTER_YETI) speed = YETI_CHARGE_SPEED;
                if (FxAbs(FxSin(day_night_cycle_accumulator)) < FX(0.35)) speed = FxMul(speed, FX(1.2));
                if (VISUAL_MAP[FxTrunc(enemy_y)][FxTrunc(enemy_x)] == 12) speed /= 4;
                fx nx = enemy_x + FxMul(FxDiv(s_dx, dist), speed), ny = enemy_y + FxMul(FxDiv(s_dy, dist), speed);
                int fi = -1; // is the hero sheltering in a fire's light?
                for (int i = 0; i < MAX_CAMPFIRES; i++)
                    if (campfires[i].active && campfires[i].screen_id == current_screen_index &&
                        FxMul(target_x - campfires[i].x, target_x - campfires[i].x) + FxMul(target_y - campfires[i].y, target_y - campfires[i].y) < FX(4.2 * 4.2)) fi = i;
                if (fi >= 0) { // wait it out: lurk at the edge, hide behind a tree, or stalk wildlife when bored
                    int mood = (e + FxTrunc(world_tick / 40)) % 3;
                    fx fx0 = campfires[fi].x, fy0 = campfires[fi].y, ex = enemy_x - fx0, ey = enemy_y - fy0, ed = FxLen(ex, ey);
                    fx gx = fx0 + FxMul(ed > FX(0.01) ? FxDiv(ex, ed) : FxI(1), FX(4.5)), gy = fy0 + FxMul(ed > FX(0.01) ? FxDiv(ey, ed) : FxI(0), FX(4.5)), best;
                    if (mood == 1) {
                        best = FxI(36);
                        for (int ty = FxTrunc(enemy_y) - 5; ty <= FxTrunc(enemy_y) + 5; ty++) for (int tx = FxTrunc(enemy_x) - 5; tx <= FxTrunc(enemy_x) + 5; tx++) {
                            if (tx < 0 || ty < 0 || tx >= MAP_SIZE || ty >= MAP_SIZE || VISUAL_MAP[ty][tx] != 11) continue;
                            fx tfx = FxI(tx) + FX(0.5) - fx0, tfy = FxI(ty) + FX(0.5) - fy0, tfd = FxLen(tfx, tfy);
                            if (tfd < FxI(4)) continue;
                            fx hx = FxI(tx) + FX(0.5) + FxMul(FxDiv(tfx, tfd), FX(0.9)), hy = FxI(ty) + FX(0.5) + FxMul(FxDiv(tfy, tfd), FX(0.9)); // the tree's dark side
                            fx dd = FxMul(hx - enemy_x, hx - enemy_x) + FxMul(hy - enemy_y, hy - enemy_y);
                            if (dd < best) { best = dd; gx = hx; gy = hy; }
                        }
                    } else if (mood == 2) {
                        best = FxI(49);
                        for (int c = 0; c < MAX_CRITTERS; c++) {
                            if (!critters[c].active || critters[c].hidden) continue;
                            fx cfx = critters[c].x - fx0, cfy = critters[c].y - fy0;
                            if (FxMul(cfx, cfx) + FxMul(cfy, cfy) < FX(4.5 * 4.5)) continue;
                            fx dd = FxMul(critters[c].x - enemy_x, critters[c].x - enemy_x) + FxMul(critters[c].y - enemy_y, critters[c].y - enemy_y);
                            if (dd < best) { best = dd; gx = critters[c].x; gy = critters[c].y; }
                        }
                    }
                    fx gdx = gx - enemy_x, gdy = gy - enemy_y, gd = FxLen(gdx, gdy);
                    fx mx = gd > FX(0.15) ? enemy_x + FxMul(FxMul(FxDiv(gdx, gd), speed), FX(0.7)) : enemy_x, my = gd > FX(0.15) ? enemy_y + FxMul(FxMul(FxDiv(gdy, gd), speed), FX(0.7)) : enemy_y;
                    int ok = gd > FX(0.15) && mx >= 0 && my >= 0 && mx < FxI(MAP_SIZE) && my < FxI(MAP_SIZE) &&
                             VISUAL_MAP[FxTrunc(my)][FxTrunc(mx)] != 1 && VISUAL_MAP[FxTrunc(my)][FxTrunc(mx)] != 12 &&
                             FxMul(mx - fx0, mx - fx0) + FxMul(my - fy0, my - fy0) >= FX(4.2 * 4.2);
                    if (ok) { enemy_x = mx; enemy_y = my; }
                    else { enemy_x += FxMul(FxSin(FxMul(world_tick, FX(0.9)) + e * FX(1.7)), FX(0.004)); enemy_y += FxMul(FxCos(FxMul(world_tick, FX(0.7)) + FxI(e)), FX(0.003)); }
                    nx = enemy_x; ny = enemy_y;
                }
                int at_fire_edge = 0; // don't step back into the light: wait at its edge instead of jittering in and out
                for (int i = 0; i < MAX_CAMPFIRES; i++)
                    if (campfires[i].active && FxMul(nx - campfires[i].x, nx - campfires[i].x) + FxMul(ny - campfires[i].y, ny - campfires[i].y) < FX(4.2 * 4.2)) at_fire_edge = 1;
                if (at_fire_edge) { // shift weight from foot to foot while waiting for the fire to die
                    enemy_x += FxMul(FxSin(FxMul(world_tick, FX(0.9)) + e * FX(1.7)), FX(0.004));
                    enemy_y += FxMul(FxCos(FxMul(world_tick, FX(0.7)) + FxI(e)), FX(0.003));
                } else { enemy_x = nx; enemy_y = ny; }
                if (summoned_zombie.active && zombie_dist < player_dist && zombie_dist < FX(0.7) &&
                    world_frame % 40 == 0 && summoned_zombie.type != SUMMON_GHOST) summoned_zombie.hp -= FX(0.5); // ghosts phase through blows
            } else if (calm || dist > FxI(8)) {
                // Roam with a purpose: walk to a tree, the water's edge, an animal or a
                // look-out spot, linger there a while (sniffing, watching), then move on.
                static fx goal_x[MAX_ENEMIES], goal_y[MAX_ENEMIES], goal_until[MAX_ENEMIES];
                static int goal_screen[MAX_ENEMIES];
                if (goal_screen[e] != current_screen_index + 1 || FxAfter(world_tick, goal_until[e])) {
                    int h = abs(RainHash(e * 7919 + FxTrunc(world_tick) * 7 + current_screen_index * 131));
                    int kind = h % 4, found = 0;
                    fx gx = enemy_x, gy = enemy_y;
                    if (kind == 2) { // an animal to stalk
                        fx best = FxI(64);
                        for (int c = 0; c < MAX_CRITTERS; c++) {
                            if (!critters[c].active || critters[c].hidden) continue;
                            fx dd = FxMul(critters[c].x - enemy_x, critters[c].x - enemy_x) + FxMul(critters[c].y - enemy_y, critters[c].y - enemy_y);
                            if (dd < best) { best = dd; gx = critters[c].x; gy = critters[c].y; found = 1; }
                        }
                    }
                    for (int k = 0; k < 24 && !found; k++) { // a tree (0), the water (1) or any open spot nearby
                        int hk = abs(RainHash(h + k * 977));
                        int tx = FxTrunc(enemy_x) + hk % 13 - 6, ty = FxTrunc(enemy_y) + (hk / 13) % 13 - 6;
                        if (tx < 1 || ty < 1 || tx >= MAP_SIZE - 1 || ty >= MAP_SIZE - 1) continue;
                        int want = kind == 0 ? 11 : kind == 1 ? 12 : -1;
                        if (want >= 0 && VISUAL_MAP[ty][tx] != want) continue;
                        if (want >= 0) { // stand beside it, on open ground
                            for (int n = 0; n < 4 && !found; n++) {
                                int nx2 = tx + (n == 0) - (n == 1), ny2 = ty + (n == 2) - (n == 3);
                                if (VISUAL_MAP[ny2][nx2] != 1 && VISUAL_MAP[ny2][nx2] != 12 && VISUAL_MAP[ny2][nx2] != 11) { gx = FxI(nx2) + FX(0.5); gy = FxI(ny2) + FX(0.5); found = 1; }
                            }
                        } else if (VISUAL_MAP[ty][tx] != 1 && VISUAL_MAP[ty][tx] != 12) { gx = FxI(tx) + FX(0.5); gy = FxI(ty) + FX(0.5); found = 1; }
                    }
                    goal_x[e] = gx; goal_y[e] = gy; goal_screen[e] = current_screen_index + 1;
                    fx gd0 = FxLen(gx - enemy_x, gy - enemy_y);
                    goal_until[e] = world_tick + FxMul(gd0, FX(1.5)) + FxI(15 + (h >> 8) % 30); // time to get there, then a 5-15 s linger
                }
                fx gdx = goal_x[e] - enemy_x, gdy = goal_y[e] - enemy_y, gd = FxLen(gdx, gdy);
                if (gd > FX(0.25)) {
                    fx sp = active_monster == MONSTER_ZOMBIE ? FX(0.007) : FX(0.012);
                    fx tx = enemy_x + FxMul(FxDiv(gdx, gd), sp), ty = enemy_y + FxMul(FxDiv(gdy, gd), sp);
                    if (tx > FX(0.5) && ty > FX(0.5) && tx < FxI(MAP_SIZE) - FX(0.5) && ty < FxI(MAP_SIZE) - FX(0.5) &&
                        VISUAL_MAP[FxTrunc(ty)][FxTrunc(tx)] != 1 && VISUAL_MAP[FxTrunc(ty)][FxTrunc(tx)] != 12) { enemy_x = tx; enemy_y = ty; }
                    else goal_until[e] = FxI(0); // blocked: pick somewhere else
                }
            }
        }
        // Keep monsters from stacking on top of one another.
        for (int o = 0; o < MAX_ENEMIES; o++) {
            if (o == e || enemies[o].hearts <= FxI(0)) continue;
            fx ox = enemy_x - enemies[o].x, oy = enemy_y - enemies[o].y;
            fx od = FxLen(ox, oy);
            if (od < FX(0.7) && od > FX(0.001)) {
                fx nx = enemy_x + FxMul(FxDiv(ox, od), FX(0.03)), ny = enemy_y + FxMul(FxDiv(oy, od), FX(0.03));
                if (nx > FX(0.5) && ny > FX(0.5) && nx < FxI(MAP_SIZE) - FX(0.5) && ny < FxI(MAP_SIZE) - FX(0.5) &&
                    VISUAL_MAP[FxTrunc(ny)][FxTrunc(nx)] != 1 && VISUAL_MAP[FxTrunc(ny)][FxTrunc(nx)] != 12) { enemy_x = nx; enemy_y = ny; }
            }
        }
    }
    StoreEnemy();
    }
    UpdateEnemySpawns();
}

void UpdateIceSlide(void) {
    if (!IsOnSlipperyGround() || (FxAbs(ice_vx) < FX(0.002) && FxAbs(ice_vy) < FX(0.002))) { ice_vx = ice_vy = FxI(0); return; }
    fx nx, ny;
    if (PlayerTryMove(player_x + ice_vx, player_y + ice_vy, &nx, &ny)) {
        if (nx == player_x) ice_vx = FxI(0); // bumped into something on that axis
        if (ny == player_y) ice_vy = FxI(0);
        player_x = nx; player_y = ny;
    } else ice_vx = ice_vy = FxI(0);
    ice_vx = FxMul(ice_vx, FX(0.99)); ice_vy = FxMul(ice_vy, FX(0.99));
}
