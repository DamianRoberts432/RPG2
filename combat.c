#include "game.h"

const char *MonsterName(MonsterType m) {
    static const char *names[] = { "Skeleton", "Zombie", "Dracula", "Goblin", "Satyr", "Yeti", "Sphinx", "Bigfoot", "Bunyip" };
    return names[m];
}

// The mythic beast (if any) that belongs in the current land, following where
// each legend comes from: Yeti - snowy Himalayan peaks, Sphinx - Egyptian
// desert, Bigfoot - deep northern forest, Bunyip - Australian swamps/billabongs.
static int MythicForCurrentLand(void) {
    switch (current_biome) {
        case BIOME_TUNDRA: return MONSTER_YETI;
        case BIOME_MOUNTAIN: return (current_season == SEASON_WINTER || snow_cover > 0.3f) ? MONSTER_YETI : -1;
        case BIOME_DESERT: case BIOME_OASIS: return MONSTER_SPHINX;
        case BIOME_FOREST: return MONSTER_BIGFOOT;
        case BIOME_SWAMP: case BIOME_LAKE: case BIOME_RIVER: return MONSTER_BUNYIP;
        default: return -1;
    }
}

#define MYTHIC_SPAWN_CHANCE 4 // percent per spawn in its home land, doubled at night
void TriggerMonsterRespawn(void) {
    int is_night = fabs(sin(day_night_cycle_accumulator)) < 0.35f;
    int spawn_side = rand() % 4;
    if (spawn_side == 0) { enemy_x = 2.0f; enemy_y = (float)(rand() % 24 + 4); }
    else if (spawn_side == 1) { enemy_x = 29.0f; enemy_y = (float)(rand() % 24 + 4); }
    else if (spawn_side == 2) { enemy_x = (float)(rand() % 24 + 4); enemy_y = 2.0f; }
    else { enemy_x = (float)(rand() % 24 + 4); enemy_y = 29.0f; }
    int mythic = MythicForCurrentLand();
    if (mythic >= 0 && rand() % 100 < MYTHIC_SPAWN_CHANCE * (is_night ? 2 : 1)) active_monster = (MonsterType)mythic;
    else if (is_night && rand() % 100 < 4) active_monster = MONSTER_DRACULA;
    else {
        // Satyr cultists keep to the wild Greek-style country: woods, meadows and hills.
        int satyr_land = current_biome == BIOME_FOREST || current_biome == BIOME_GRASSLANDS ||
                         current_biome == BIOME_PRAIRIE || current_biome == BIOME_MOUNTAIN;
        int r = rand() % 100;
        active_monster = r < 32 ? MONSTER_SKELLY : r < 64 ? MONSTER_ZOMBIE :
                         (r < 82 || !satyr_land) ? MONSTER_GOBLIN : MONSTER_SATYR;
    }
    static const float base_hearts[] = { 3.0f, 3.0f, 6.0f, 2.0f, 3.5f, 9.0f, 9.0f, 8.0f, 8.0f };
    enemy_hearts = base_hearts[active_monster] + (is_night ? 0.5f : 0.0f);
    if (IS_MYTHIC_MONSTER(active_monster)) {
        size_t n = strlen(arpg_action_log);
        snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  A %s roams this land!", MonsterName(active_monster));
    }
}

void SpawnBloodMist(float x, float y) {
    // Tight, compact speck burst: small velocities/lifetimes so blood reads as
    // mostly one-pixel specks instead of an oversized expanding splatter.
    // Velocity is drawn from roughly [-0.0078, +0.0078] world units/frame
    // (rand()%100 spread over a 6400.0f divisor, re-centered by subtracting
    // the midpoint) and lifetime from 8-16 frames, both deliberately short
    // so specks fade before they can travel far or grow large on screen.
    for (int i = 0; i < MAX_BLOOD_MIST; i++) {
        blood_mist[i].x = x; blood_mist[i].y = y;
        blood_mist[i].vx = ((rand() % 100) / 6400.0f - 0.0078f);
        blood_mist[i].vy = ((rand() % 100) / 6400.0f - 0.0078f);
        blood_mist[i].life = 8 + rand() % 9;
        blood_mist[i].max_life = blood_mist[i].life;
        blood_mist[i].active = 1;
    }
}

void PerformMeleeAttack(void) {
    WeaponStats *weapon = GetEquippedWeaponStats();
    float damage = weapon ? (float)weapon->damage : 1.0f;
    float stamina_cost = weapon ? weapon->stamina_cost : 0.0f;
    if (player_stamina < stamina_cost) { strcpy(arpg_action_log, "Exhausted! Too tired to swing that weapon."); return; }
    player_stamina -= (float)stamina_cost;
    sword_swipe_frame = (weapon && weapon->attack_speed_mult < 1.0f) ? 14 : 10;
    float m_dist = DistanceToEnemy();
    if (m_dist < 1.8f && IsFacingEnemy()) {
        int broke = WearEquippedWeapon();
        HandleEnemyDamage(damage);
        if (broke) strcpy(arpg_action_log, "Your weapon broke!");
    }
}

void SpawnTreeDebris(float cx, float cy) {
    for (int i = 0; i < 5; i++) {
        twigs[i].x = cx; twigs[i].y = cy; twigs[i].vx = (float)((rand()%100)/500.0f - 0.1f); twigs[i].vy = (float)((rand()%100)/500.0f - 0.1f);
        twigs[i].life = 40; twigs[i].screen_id = current_screen_index; twigs[i].active = 1;
    }
}

// Twigs/chips drift briefly outward from the chopped tree or mined rock, then
// vanish shortly after - debris is no longer left permanently on the ground.
void UpdateDebrisTwigs(void) {
    for (int i = 0; i < DEBRIS_MAX; i++) {
        if (!twigs[i].active) continue;
        twigs[i].x += twigs[i].vx; twigs[i].y += twigs[i].vy;
        if (--twigs[i].life <= 0) twigs[i].active = 0;
    }
}

void HandleEnemyDamage(float dmg) {
    if (fabs(sin(day_night_cycle_accumulator)) < 0.35f) dmg *= 0.9f;
    enemy_hearts -= dmg;
    float k_dx = enemy_x - player_x; float k_dy = enemy_y - player_y; float k_dist = (float)sqrt(k_dx * k_dx + k_dy * k_dy);
    if (k_dist > 0.01f) { enemy_x += (k_dx / k_dist) * 0.25f; enemy_y += (k_dy / k_dist) * 0.25f; }
    
    if (enemy_hearts <= 0.0f) {
        SpawnBloodMist(enemy_x, enemy_y);
        int is_night = fabs(sin(day_night_cycle_accumulator)) < 0.35f;
        int gained_xp = (int)(((25 + rand() % 15 + (is_night ? 5 : 0)) * opt_xp_mult) * ((is_night && night_bonus_active) ? 1.25f : 1.0f));
        float loot_multiplier = opt_loot_mult * (is_night ? 1.5f : 1.0f);
        int loot_chance = (int)(70.0f * loot_multiplier);
        if (loot_chance > 100) loot_chance = 100;
        if (rand() % 100 < loot_chance) {
            int loot_id = LOOT_SHORT_SWORD + rand() % 5;
            int loot_quantity = (int)loot_multiplier;
            if (loot_quantity < 1) loot_quantity = 1;
            if (rand() % 100 < (int)((loot_multiplier - (int)loot_multiplier) * 100.0f)) loot_quantity++;
            DropGroundLoot(enemy_x, enemy_y, loot_id, loot_quantity);
        }
        int hoard = 0;
        if (IS_BOSS_MONSTER(active_monster)) {
            // Bosses carry the rare treasure: a gold hoard and a chance at Legendary loot.
            hoard = 150 + rand() % 151;
            gold_count += hoard;
            gained_xp *= 4;
            if (rand() % 100 < 40) DropGroundLoot(enemy_x, enemy_y, LOOT_MUSASHI_BLADE, 1);
        }
        player_xp += gained_xp;
        if (player_xp >= player_next_level_xp) {
            player_level++;
            player_xp -= player_next_level_xp;
            player_next_level_xp = (int)(player_next_level_xp * 1.5f);
            sprintf(arpg_action_log, "LEVEL UP! Reached Level %d!", player_level);
        } else {
            if (hoard) sprintf(arpg_action_log, "VICTORY: The %s falls! +%d XP, %d gold from its hoard!", MonsterName(active_monster), gained_xp, hoard);
            else sprintf(arpg_action_log, "VICTORY: %s slain! +%d XP", MonsterName(active_monster), gained_xp);
        }
        TriggerMonsterRespawn();
    } else {
        sprintf(arpg_action_log, "STRIKE: Hit for %.1f DMG! Remainder: %.1f", dmg, enemy_hearts);
    }
}

// Straight-line tile distance from the player to the active enemy. Shared
// helper for the melee range checks (gamepad/keyboard attack and the
// Knight/Warrior backstep counter-strike) to avoid duplicating the formula.
float DistanceToEnemy(void) {
    return (float)sqrt((player_x - enemy_x) * (player_x - enemy_x) + (player_y - enemy_y) * (player_y - enemy_y));
}

// Appropriate facing/range check for melee strikes: the enemy must be roughly
// in front of the player (within ~75.5 degrees of the facing axis, i.e. a
// ~151 degree total cone), not merely within radius, so axes/swords can't
// hit targets directly behind the hero.
// dot > 0.25 corresponds to an angle below ~75.5 degrees from the facing axis.
int IsFacingEnemyWithin(float dot_threshold) {
    if (enemy_hearts <= 0.0f) return 0;
    float ex = enemy_x - player_x, ey = enemy_y - player_y;
    float dist = (float)sqrt(ex * ex + ey * ey);
    if (dist < 0.001f) return 1;
    ex /= dist; ey /= dist;
    float fx = 0.0f, fy = 0.0f;
    if (player_facing == FACE_UP) fy = -1.0f;
    else if (player_facing == FACE_DOWN) fy = 1.0f;
    else if (player_facing == FACE_LEFT) fx = -1.0f;
    else fx = 1.0f;
    float dot = ex * fx + ey * fy;
    return dot > dot_threshold;
}

int IsFacingEnemy(void) { return IsFacingEnemyWithin(FACING_CONE_DOT_THRESHOLD); }

// Arrows fly straight along the hero's facing; they only curve toward the
// monster when the hero is actually facing it (within BOW_AIM_CONE_DOT).
#define BOW_AIM_CONE_DOT 0.5f
void FirePlayerArrow(int is_tap) {
    player_arrow.x = player_x; player_arrow.y = player_y; player_arrow.z = 12.0f;
    player_arrow.origin_tx = (int)player_x; player_arrow.origin_ty = (int)player_y;
    float track_dx = 0.0f, track_dy = 0.0f;
    if (IsFacingEnemyWithin(BOW_AIM_CONE_DOT)) {
        track_dx = enemy_x - player_x; track_dy = enemy_y - player_y;
    } else {
        if (player_facing == FACE_UP) track_dy = -1.0f;
        else if (player_facing == FACE_DOWN) track_dy = 1.0f;
        else if (player_facing == FACE_LEFT) track_dx = -1.0f;
        else track_dx = 1.0f;
    }
    float distance = (float)sqrt(track_dx * track_dx + track_dy * track_dy);
    if (distance < 0.001f) { track_dx = 1.0f; distance = 1.0f; }
    float distance_mult = is_tap ? 0.5f : 0.5f + (bow_charge_time * 1.5f);
    float spd = 0.35f * distance_mult;
    player_stamina -= bow_charge_time * 10.0f; if (player_stamina < 0.0f) player_stamina = 0.0f;
    player_arrow.vx = (track_dx / distance) * spd;
    player_arrow.vy = (track_dy / distance) * spd;
    player_arrow.vz = is_tap ? 0.25f : 0.5f;
    player_arrow.damage = is_tap ? 0.4f : (0.5f + (bow_charge_time * 1.5f));
    player_arrow.active = 1; // Projectile active only upon release
    if (is_tap) {
        sprintf(arpg_action_log, "BOW: Quick-shot tap fired! (Light dmg: %.1f)", player_arrow.damage);
    } else {
        sprintf(arpg_action_log, "BOW: Charged shot unleashed! (Dmg: %.1f, Charge: %d%%)", player_arrow.damage, (int)(bow_charge_time * 100));
    }
}

// Trees and rocks stop an arrow, and it vanishes at the edge of the screen.
static int ArrowBlockedAt(float x, float y) {
    if (x < 0.5f || y < 0.5f || x >= MAP_SIZE - 0.5f || y >= MAP_SIZE - 0.5f) return 1;
    int tx = (int)x, ty = (int)y;
    if (tx == player_arrow.origin_tx && ty == player_arrow.origin_ty) return 0;
    uint8_t t = VISUAL_MAP[ty][tx];
    return t == 11 || t == 1 || t == 3;
}

void UpdatePlayerArrow(void) {
    if (!player_arrow.active) return;
    for (int step = 0; step < 2; step++) {
        player_arrow.x += player_arrow.vx * 0.5f; player_arrow.y += player_arrow.vy * 0.5f;
        if (ArrowBlockedAt(player_arrow.x, player_arrow.y)) { player_arrow.active = 0; return; }
    }
    player_arrow.z += player_arrow.vz; player_arrow.vz -= 0.02f;
    float arrow_radius = 0.5f + fabs(player_arrow.vx) + fabs(player_arrow.vy);
    if (enemy_hearts > 0.0f && fabs(player_arrow.x - enemy_x) < arrow_radius && fabs(player_arrow.y - enemy_y) < arrow_radius) {
        player_arrow.active = 0;
        int broke = WearEquippedBow();
        HandleEnemyDamage(player_arrow.damage);
        if (broke) strcpy(arpg_action_log, "Your bow broke!");
    }
    if (player_arrow.z <= 0.0f) player_arrow.active = 0;
}

// Debounced/cooldown-bound class special ability bound to the "Y" button
// (gamepad and keyboard). Callers must only invoke this on the rising edge of
// the button press so it fires once per press rather than every held frame.
// See the FACING_CONE_DOT_THRESHOLD / *_COOLDOWN_MS / *_SUPPRESS_MS /
// *_FLASH_FRAMES constants defined near the top of the file for tuning.
void FireClassAbility(void) {
    DWORD now = GetTickCount();
    // Signed-subtraction comparison (rather than `now < y_ability_cooldown_until`)
    // stays correct across the ~49.7 day GetTickCount() wraparound boundary,
    // matching the (GetTickCount() - last_x > threshold) pattern used elsewhere
    // in this file for other input-debounce timers.
    long cooldown_remaining_ms = (long)(y_ability_cooldown_until - now);
    if (cooldown_remaining_ms > 0) {
        sprintf(arpg_action_log, "ABILITY: On cooldown (%.1fs left)", cooldown_remaining_ms / 1000.0f);
        return;
    }

    if (player_mp < CLASS_ABILITY_MP_COST) {
        sprintf(arpg_action_log, "ABILITY: Not enough magic (%d/%d MP).", (int)player_mp, (int)CLASS_ABILITY_MP_COST);
        return;
    }

    if (selected_class == CLASS_NECROMANCER || selected_class == CLASS_WIZARD) {
        if (selected_class == CLASS_NECROMANCER) {
            if (summoned_zombie.active) { strcpy(arpg_action_log, "NECROMANCY: Your summoned dead still fight."); return; }
            summoned_zombie.x = player_x; summoned_zombie.y = player_y;
            if (player_facing == FACE_UP) summoned_zombie.y -= 1.0f;
            else if (player_facing == FACE_DOWN) summoned_zombie.y += 1.0f;
            else if (player_facing == FACE_LEFT) summoned_zombie.x -= 1.0f;
            else summoned_zombie.x += 1.0f;
            summoned_zombie.hp = 3.0f; summoned_zombie.timer = 900;
            summoned_zombie.raise_frame = 0; summoned_zombie.attack_cooldown = 0;
            summoned_zombie.active = 1;
            ability_visual_frame = SPELL_FLASH_FRAMES;
            player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + SPELL_COOLDOWN_MS;
            strcpy(arpg_action_log, "NECROMANCY: A zombie claws its way from the earth!");
            return;
        }
        if (player_spell.active) {
            strcpy(arpg_action_log, "ABILITY: Bolt still in flight!");
            return;
        }
        player_spell.x = player_x; player_spell.y = player_y; player_spell.z = 10.0f;
        float track_dx = 0.0f, track_dy = 0.0f;
        if (enemy_hearts > 0.0f) {
            track_dx = enemy_x - player_x; track_dy = enemy_y - player_y;
        } else if (player_facing == FACE_UP) track_dy = -1.0f;
        else if (player_facing == FACE_DOWN) track_dy = 1.0f;
        else if (player_facing == FACE_LEFT) track_dx = -1.0f;
        else track_dx = 1.0f;

        float distance = (float)sqrt(track_dx * track_dx + track_dy * track_dy);
        if (distance < 0.001f) { track_dx = 1.0f; distance = 1.0f; }
        float spd = 0.32f;
        player_spell.vx = (track_dx / distance) * spd;
        player_spell.vy = (track_dy / distance) * spd;
        player_spell.vz = 0.0f;
        player_spell.damage = 1.5f;
        player_spell.active = 1;
        ability_visual_frame = SPELL_FLASH_FRAMES;
        player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + SPELL_COOLDOWN_MS;
        strcpy(arpg_action_log, "ARCANE: Magic bolt unleashed!");
    } else if (selected_class == CLASS_KNIGHT || selected_class == CLASS_WARRIOR) {
        float ndx = 0.0f, ndy = 0.0f;
        if (player_facing == FACE_UP) ndy = 1.0f;
        else if (player_facing == FACE_DOWN) ndy = -1.0f;
        else if (player_facing == FACE_LEFT) ndx = 1.0f;
        else ndx = -1.0f;

        float tx = player_x + ndx, ty = player_y + ndy;
        if (tx >= 0 && tx < MAP_SIZE && ty >= 0 && ty < MAP_SIZE &&
            VISUAL_MAP[(int)ty][(int)tx] != 1 && VISUAL_MAP[(int)ty][(int)tx] != 12) {
            player_x = tx; player_y = ty;
        }

        monster_pursuit_suppressed_until = now + BACKSTEP_SUPPRESS_MS;
        ability_visual_frame = ABILITY_FLASH_FRAMES;
        player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + BACKSTEP_COOLDOWN_MS;

        float m_dist = DistanceToEnemy();
        if (enemy_hearts > 0.0f && m_dist < 2.0f && IsFacingEnemy()) {
            sword_swipe_frame = 10;
            HandleEnemyDamage(2.0f);
            strcpy(arpg_action_log, "DEFENSIVE BACKSTEP: Countered with a heavy strike!");
        } else {
            strcpy(arpg_action_log, "DEFENSIVE BACKSTEP: Repositioned, enemies briefly wary!");
        }
    } else { // Rogue
        is_sneaking = 1;
        monster_pursuit_suppressed_until = now + SNEAK_SUPPRESS_MS;
        ability_visual_frame = ABILITY_FLASH_FRAMES;
        player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + SNEAK_COOLDOWN_MS;
        strcpy(arpg_action_log, "SNEAK: Breaking monster detection for 4s!");
    }
}
