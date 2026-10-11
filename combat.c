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
    for (int i = 0; i < MAX_BLOOD_MIST / 2; i++) {
        blood_mist[i].x = x; blood_mist[i].y = y;
        blood_mist[i].vx = ((rand() % 100) / 6400.0f - 0.0078f);
        blood_mist[i].vy = ((rand() % 100) / 6400.0f - 0.0078f);
        blood_mist[i].life = 8 + rand() % 9;
        blood_mist[i].max_life = blood_mist[i].life;
        blood_mist[i].active = 1;
    }
}

// Heavy or critical hits spray 8-16 specks away from the hero.
#define HEAVY_HIT_DAMAGE 2.0f
#define SPEAR_REACH 2.6f
#define SPEAR_THRUST_DOT 0.7f // narrower than a sword swing: a straight thrust
void SpawnBloodBurst(float x, float y, float dir_x, float dir_y) {
    float d = (float)sqrt(dir_x * dir_x + dir_y * dir_y);
    if (d < 0.001f) { dir_x = 0.0f; dir_y = 1.0f; d = 1.0f; }
    dir_x /= d; dir_y /= d;
    int want = 8 + rand() % 9, spawned = 0;
    for (int i = 0; i < MAX_BLOOD_MIST && spawned < want; i++) {
        if (blood_mist[i].active) continue;
        float spd = 0.012f + (rand() % 100) / 4000.0f;
        float spread = ((rand() % 100) - 50) / 80.0f;
        blood_mist[i].x = x; blood_mist[i].y = y;
        blood_mist[i].vx = (dir_x - dir_y * spread) * spd;
        blood_mist[i].vy = (dir_y + dir_x * spread) * spd;
        blood_mist[i].life = 10 + rand() % 9;
        blood_mist[i].max_life = blood_mist[i].life;
        blood_mist[i].active = 1;
        spawned++;
    }
}

void PerformMeleeAttack(void) {
    WeaponStats *weapon = GetEquippedWeaponStats();
    float damage = weapon ? (float)weapon->damage : 1.0f;
    float stamina_cost = MELEE_SWING_STAMINA + (weapon ? weapon->stamina_cost * 2.0f : 0.0f);
    if (player_stamina < stamina_cost) { strcpy(arpg_action_log, "Exhausted! Too tired to swing that weapon."); return; }
    player_stamina -= (float)stamina_cost;
    stamina_rest_timer = 0;
    sword_swipe_frame = (weapon && weapon->attack_speed_mult < 1.0f) ? 14 : 10;
    SelectTargetEnemy();
    float m_dist = DistanceToEnemy();
    int spear = active_weapon == WEAPON_SPEAR;
    if (m_dist < (spear ? SPEAR_REACH : 1.8f) && (spear ? IsFacingEnemyWithin(SPEAR_THRUST_DOT) : IsFacingEnemy())) {
        int broke = WearEquippedWeapon();
        HandleEnemyDamage(damage * (1.0f + 0.08f * stat_spent[0]));
        if (broke) strcpy(arpg_action_log, "Your weapon broke!");
    }
}

void SpawnTreeDebris(float cx, float cy) {
    for (int i = 0; i < 5; i++) {
        twigs[i].x = cx; twigs[i].y = cy; twigs[i].vx = (float)((rand()%100)/500.0f - 0.1f); twigs[i].vy = (float)((rand()%100)/500.0f - 0.1f);
        twigs[i].life = 40; twigs[i].screen_id = current_screen_index; twigs[i].active = 1;
    }
}

// Dirt clods kicked up when a summoned zombie or skeleton claws out of the ground.
void SpawnDirtClods(float cx, float cy) {
    int n = 0;
    for (int i = 0; i < DEBRIS_MAX && n < 8; i++) {
        if (twigs[i].active) continue;
        twigs[i].x = cx; twigs[i].y = cy;
        twigs[i].vx = (rand() % 100) / 2500.0f - 0.02f; twigs[i].vy = (rand() % 100) / 2500.0f - 0.02f;
        twigs[i].life = 20 + rand() % 15; twigs[i].screen_id = current_screen_index; twigs[i].active = 1;
        n++;
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

#define JUNK_DROP_RATE 0.4f
#define GOLD_DROP_CAP 75
#define OVERKILL_BLOOD 1.0f // hearts of damage past zero that trigger an extra spray
void HandleEnemyDamage(float dmg) {
    if (fabs(sin(day_night_cycle_accumulator)) < 0.35f) dmg *= 0.9f;
    int crit = dmg > 0.0f && rand() % 100 < 5 + 2 * stat_spent[4]; // each LCK point spent: +2% crit
    if (crit) dmg *= 2.0f;
    enemy_hearts -= dmg;
    if (active_monster == MONSTER_YETI) enemies[current_enemy].angry = 1;
    float k_dx = enemy_x - player_x; float k_dy = enemy_y - player_y; float k_dist = (float)sqrt(k_dx * k_dx + k_dy * k_dy);
    if (crit || dmg >= HEAVY_HIT_DAMAGE) SpawnBloodBurst(enemy_x, enemy_y, k_dx, k_dy);
    if (k_dist > 0.01f) { enemy_x += (k_dx / k_dist) * 0.25f; enemy_y += (k_dy / k_dist) * 0.25f; }
    
    if (enemy_hearts <= 0.0f) {
        SpawnBloodMist(enemy_x, enemy_y);
        if (-enemy_hearts >= OVERKILL_BLOOD) { SpawnBloodBurst(enemy_x, enemy_y, k_dx, k_dy); SpawnBloodBurst(enemy_x, enemy_y, k_dx, k_dy); }
        int is_night = fabs(sin(day_night_cycle_accumulator)) < 0.35f;
        int gained_xp = (int)(((10 + rand() % 7 + (is_night ? 2 : 0)) * opt_xp_mult) * ((is_night && night_bonus_active) ? 1.25f : 1.0f));
        float loot_multiplier = opt_loot_mult * (is_night ? 1.5f : 1.0f);
        // Ordinary (junk) drops are 60% rarer than they used to be, to cut ground clutter.
        int loot_chance = (int)(70.0f * JUNK_DROP_RATE * loot_multiplier) + 3 * stat_spent[4];
        if (loot_chance > 100) loot_chance = 100;
        if (rand() % 100 < loot_chance) {
            int loot_id = LOOT_SHORT_SWORD + rand() % 5;
            int loot_quantity = (int)loot_multiplier;
            if (loot_quantity < 1) loot_quantity = 1;
            if (rand() % 100 < (int)((loot_multiplier - (int)loot_multiplier) * 100.0f)) loot_quantity++;
            DropGroundLoot(enemy_x, enemy_y, loot_id, loot_quantity);
        }
        // A little gold, more with level and luck, never more than GOLD_DROP_CAP.
        int coins = 0;
        if (rand() % 100 < 35 + 2 * stat_spent[4]) {
            coins = 1 + rand() % (3 + player_level + 2 * stat_spent[4]);
            if (coins > GOLD_DROP_CAP) coins = GOLD_DROP_CAP;
            gold_count += coins;
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
        if (IS_BOSS_MONSTER(active_monster)) { karma.life_marks |= (uint16_t)(1u << active_monster); KarmaAdjust(5, 0, 0, 10); }
        else if (rand() % 5 == 0) KarmaAdjust(1, 0, 0, 0);
        if (player_xp >= player_next_level_xp) {
            // Each level needs 50 more XP than the last (level 10 ~ 200 kills, 25 ~ 1250).
            while (player_xp >= player_next_level_xp) {
                player_level++;
                player_xp -= player_next_level_xp;
                player_next_level_xp += 50;
                stat_points += STAT_POINTS_PER_LEVEL;
            }
            sprintf(arpg_action_log, "LEVEL UP! Reached Level %d! %d attribute points to spend (Hero tab: Y / P).", player_level, stat_points);
        } else {
            if (hoard) sprintf(arpg_action_log, "VICTORY: The %s falls! +%d XP, %d gold from its hoard!", MonsterName(active_monster), gained_xp, hoard);
            else sprintf(arpg_action_log, "VICTORY: %s slain! +%d XP", MonsterName(active_monster), gained_xp);
        }
        if (coins) { size_t n = strlen(arpg_action_log); snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  +%d gold", coins); }
        if (monster_card_links[active_monster].kills < 65535) monster_card_links[active_monster].kills++;
        TryCardDrop(active_monster);
    } else {
        sprintf(arpg_action_log, "%s: Hit for %.1f DMG! Remainder: %.1f", crit ? "CRITICAL" : "STRIKE", dmg, enemy_hearts);
    }
    StoreEnemy();
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

void SelectEnemy(int i) {
    current_enemy = i;
    enemy_x = enemies[i].x; enemy_y = enemies[i].y; enemy_hearts = enemies[i].hearts;
    active_monster = enemies[i].type; enemy_direction = enemies[i].direction;
}

void StoreEnemy(void) {
    EnemySlot *e = &enemies[current_enemy];
    e->x = enemy_x; e->y = enemy_y; e->hearts = enemy_hearts; e->type = active_monster; e->direction = enemy_direction;
}

// Selects the first living enemy inside the box around (x, y).
int EnemyAtPoint(float x, float y, float radius) {
    StoreEnemy();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts > 0.0f && fabs(x - enemies[i].x) < radius && fabs(y - enemies[i].y) < radius) { SelectEnemy(i); return i; }
    }
    return -1;
}

int SelectNearestEnemyTo(float x, float y) {
    StoreEnemy();
    int best = -1; float best_d = 1e9f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= 0.0f) continue;
        float dx = enemies[i].x - x, dy = enemies[i].y - y, d = dx * dx + dy * dy;
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best >= 0) SelectEnemy(best);
    return best;
}

// The player's target: the closest enemy they are facing, else the closest one.
int SelectTargetEnemy(void) {
    int nearest = SelectNearestEnemyTo(player_x, player_y);
    if (nearest < 0) return -1;
    int facing = -1; float best_d = 1e9f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= 0.0f) continue;
        SelectEnemy(i);
        float d = DistanceToEnemy();
        if (IsFacingEnemy() && d < best_d) { best_d = d; facing = i; }
    }
    SelectEnemy(facing >= 0 ? facing : nearest);
    return current_enemy;
}

static int EnemyCap(void) { return fabs(sin(day_night_cycle_accumulator)) < 0.35f ? MAX_ENEMIES : MAX_ENEMIES_DAY; }

static void SpawnEnemyInFreeSlot(void) {
    StoreEnemy();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts > 0.0f) continue;
        SelectEnemy(i); TriggerMonsterRespawn(); enemy_direction = FACE_DOWN; StoreEnemy(); enemies[i].angry = 0; enemies[i].poison = 0;
        return;
    }
}

void SpawnScreenEnemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].hearts = 0.0f;
    SelectEnemy(0);
    int cap = EnemyCap();
    int n = cap / 2 + rand() % (cap / 2 + 1); // day 2-4, night 3-6
    for (int k = 0; k < n; k++) SpawnEnemyInFreeSlot();
}

// Every ENEMY_SPAWN_INTERVAL_MS another monster wanders in while the screen is
// under its cap (4 by day, 6 at night); extras left at dawn wander off.
void UpdateEnemySpawns(void) {
    static DWORD last_spawn = 0;
    DWORD now = GetTickCount();
    if (now - last_spawn < ENEMY_SPAWN_INTERVAL_MS) return;
    last_spawn = now;
    StoreEnemy();
    int alive = 0, farthest = -1; float far_d = -1.0f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= 0.0f) continue;
        alive++;
        if (IS_BOSS_MONSTER(enemies[i].type)) continue;
        float dx = enemies[i].x - player_x, dy = enemies[i].y - player_y, d = dx * dx + dy * dy;
        if (d > far_d) { far_d = d; farthest = i; }
    }
    int cap = EnemyCap();
    if (alive < cap) SpawnEnemyInFreeSlot();
    else if (alive > cap && farthest >= 0) { enemies[farthest].hearts = 0.0f; SelectEnemy(current_enemy); }
}

// Arrows fly straight along the hero's facing; they only curve toward the
// monster when the hero is actually facing it (within BOW_AIM_CONE_DOT).
#define BOW_AIM_CONE_DOT 0.5f
// Holding the draw (RT / F) costs stamina every frame; once winded the
// charge stops building, though the arrow can still be loosed.
#define BOW_DRAW_STAMINA_PER_FRAME 1.0f
void ChargeBow(void) {
    if (player_stamina <= 0.0f) return;
    player_stamina -= BOW_DRAW_STAMINA_PER_FRAME;
    if (player_stamina < 0.0f) player_stamina = 0.0f;
    bow_charge_time += 0.05f;
    if (bow_charge_time > 1.0f) bow_charge_time = 1.0f;
}

void FirePlayerArrow(int is_tap) {
    player_arrow.x = player_x; player_arrow.y = player_y; player_arrow.z = 12.0f;
    player_arrow.origin_tx = (int)player_x; player_arrow.origin_ty = (int)player_y;
    float track_dx = 0.0f, track_dy = 0.0f;
    SelectTargetEnemy();
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
    if (EnemyAtPoint(player_arrow.x, player_arrow.y, arrow_radius) >= 0) {
        player_arrow.active = 0;
        int broke = WearEquippedBow();
        HandleEnemyDamage(player_arrow.damage);
        if (broke) strcpy(arpg_action_log, "Your bow broke!");
    }
    if (player_arrow.z <= 0.0f) player_arrow.active = 0;
}

// Spear, second mode: RT / F throws the equipped spear. It flies along the
// hero's facing (curving to a faced enemy), drops with gravity and lands as a
// dropped item that X picks back up with its durability intact.
#define SPEAR_THROW_SPEED 0.3f
void ThrowSpear(void) {
    if (thrown_spear.active) return;
    int idx = -1;
    for (int i = 0; i < player_item_count; i++)
        if (player_inventory[i].is_equipped && strcmp(player_inventory[i].name, "Spear") == 0) { idx = i; break; }
    if (idx < 0) return;
    thrown_spear_item = player_inventory[idx];
    thrown_spear_item.is_equipped = 0;
    for (int i = idx; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
    player_item_count--;
    if (selected_inv_index >= player_item_count && player_item_count > 0) selected_inv_index = player_item_count - 1;
    RecalculateCarriedWeight();
    SyncActiveWeapon();
    float tdx = 0.0f, tdy = 0.0f;
    SelectTargetEnemy();
    if (IsFacingEnemyWithin(BOW_AIM_CONE_DOT)) { tdx = enemy_x - player_x; tdy = enemy_y - player_y; }
    else if (player_facing == FACE_UP) tdy = -1.0f;
    else if (player_facing == FACE_DOWN) tdy = 1.0f;
    else if (player_facing == FACE_LEFT) tdx = -1.0f;
    else tdx = 1.0f;
    float d = (float)sqrt(tdx * tdx + tdy * tdy);
    if (d < 0.001f) { tdx = 1.0f; d = 1.0f; }
    thrown_spear.x = player_x; thrown_spear.y = player_y; thrown_spear.z = 14.0f;
    thrown_spear.vx = tdx / d * SPEAR_THROW_SPEED; thrown_spear.vy = tdy / d * SPEAR_THROW_SPEED;
    thrown_spear.vz = 0.3f;
    thrown_spear.origin_tx = (int)player_x; thrown_spear.origin_ty = (int)player_y;
    thrown_spear.damage = (float)thrown_spear_item.damage * 1.5f * (1.0f + 0.08f * stat_spent[0]);
    thrown_spear.active = 1;
    player_stamina -= 5.0f; if (player_stamina < 0.0f) player_stamina = 0.0f;
    strcpy(arpg_action_log, "SPEAR: Thrown! Stand on it and press X to pick it up.");
}

static int SpearBlockedAt(float x, float y) {
    if (x < 0.5f || y < 0.5f || x >= MAP_SIZE - 0.5f || y >= MAP_SIZE - 0.5f) return 1;
    int tx = (int)x, ty = (int)y;
    if (tx == thrown_spear.origin_tx && ty == thrown_spear.origin_ty) return 0;
    uint8_t t = VISUAL_MAP[ty][tx];
    return t == 11 || t == 1 || t == 3;
}

static void LandThrownSpear(void) {
    thrown_spear.active = 0;
    int slot = -1;
    for (int i = 0; i < MAX_DROPPED_ITEMS; i++) if (!dropped_items[i].active) { slot = i; break; }
    if (slot < 0) {
        if (player_item_count < MAX_INVENTORY_ITEMS) { player_inventory[player_item_count++] = thrown_spear_item; RecalculateCarriedWeight(); }
        return;
    }
    DroppedInventoryItem *drop = &dropped_items[slot];
    FindDropSpot(thrown_spear.x, thrown_spear.y, 1, &drop->x, &drop->y);
    drop->item = thrown_spear_item;
    drop->screen_id = current_screen_index;
    drop->dropped_at = GetTickCount();
    drop->legendary = 0;
    drop->active = 1;
}

void UpdateThrownSpear(void) {
    if (!thrown_spear.active) return;
    for (int step = 0; step < 2; step++) {
        float nx = thrown_spear.x + thrown_spear.vx * 0.5f, ny = thrown_spear.y + thrown_spear.vy * 0.5f;
        if (SpearBlockedAt(nx, ny)) { LandThrownSpear(); return; }
        thrown_spear.x = nx; thrown_spear.y = ny;
    }
    thrown_spear.z += thrown_spear.vz; thrown_spear.vz -= 0.03f;
    if (EnemyAtPoint(thrown_spear.x, thrown_spear.y, 0.6f) >= 0) {
        HandleEnemyDamage(thrown_spear.damage);
        if (--thrown_spear_item.durability_current <= 0) { thrown_spear.active = 0; strcpy(arpg_action_log, "Your spear broke!"); return; }
        LandThrownSpear();
        return;
    }
    if (thrown_spear.z <= 0.0f) LandThrownSpear();
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
            // Zombie (poison bite) from level 1, Skeleton (tank) from 3, Ghost
            // (fast, phases through blows) from 5; each cast raises the next one.
            static const int unlock_level[3] = { 1, 3, 5 };
            static const float summon_hp[3] = { 3.0f, 6.0f, 2.0f };
            static const char *summon_msg[3] = { "NECROMANCY: A zombie claws its way from the earth! Its bite poisons.",
                                                 "NECROMANCY: A skeleton warrior rises to shield you!",
                                                 "NECROMANCY: A ghost drifts out of the veil!" };
            int type = player_level >= unlock_level[next_summon_type] ? next_summon_type : SUMMON_ZOMBIE;
            next_summon_type = type;
            do next_summon_type = (next_summon_type + 1) % 3; while (player_level < unlock_level[next_summon_type]);
            summoned_zombie.type = type;
            summoned_zombie.hp = summon_hp[type]; summoned_zombie.timer = 900;
            summoned_zombie.raise_frame = 0; summoned_zombie.attack_cooldown = 0;
            summoned_zombie.active = 1;
            if (type != SUMMON_GHOST) SpawnDirtClods(summoned_zombie.x, summoned_zombie.y + 0.3f);
            KarmaAdjust(0, 5, 0, 0);
            ability_visual_frame = SPELL_FLASH_FRAMES;
            player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + SPELL_COOLDOWN_MS;
            strcpy(arpg_action_log, summon_msg[type]);
            return;
        }
        if (player_spell.active) {
            strcpy(arpg_action_log, "ABILITY: Bolt still in flight!");
            return;
        }
        player_spell.x = player_x; player_spell.y = player_y; player_spell.z = 10.0f;
        float track_dx = 0.0f, track_dy = 0.0f;
        SelectTargetEnemy();
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

        SelectTargetEnemy();
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
