#include "game.h"

const char *MonsterName(MonsterType m) {
    static const char *names[] = { "Skeleton", "Zombie", "Vampire", "Goblin", "Satyr", "Yeti", "Sphinx", "Bigfoot", "Bunyip" };
    return names[m];
}

// On full-moon nights (every 8th day of a season) some beasts of the open land are werewolves:
// a Bigfoot in a grey wolf's coat, whose hide only silver cuts through.
int werewolf_active = 0;
static int FullMoonNight(int is_night) { return is_night && SeasonDay() % 8 == 0; }
static const char *FoeName(void) { return bounty_hunter_active ? "Bounty Hunter" : werewolf_active ? "Werewolf" : MonsterName(active_monster); }

// The mythic beast (if any) that belongs in the current land, following where
// each legend comes from: Yeti - snowy Himalayan peaks, Sphinx - Egyptian
// desert, Bigfoot - deep northern forest, Bunyip - Australian swamps/billabongs.
static int MythicForCurrentLand(void) {
    switch (current_biome) {
        case BIOME_TUNDRA: return MONSTER_YETI;
        case BIOME_MOUNTAIN: return (current_season == SEASON_WINTER || snow_cover > FX(0.3) || MountainIsPeak(current_screen_index)) ? MONSTER_YETI : -1;
        case BIOME_DESERT: case BIOME_OASIS: return MONSTER_SPHINX;
        case BIOME_FOREST: return MONSTER_BIGFOOT;
        case BIOME_SWAMP: case BIOME_LAKE: case BIOME_RIVER: return MONSTER_BUNYIP;
        default: return -1;
    }
}

#define MYTHIC_SPAWN_CHANCE 4 // percent per spawn in its home land, doubled at night
static void SpawnTintedBurst(fx x, fx y, fx dir_x, fx dir_y, uint32_t color);

// Zombies near rivers and marshes rise out of the water's edge 4-10 tiles
// from the hero instead of walking in from the screen edge.
static void ZombieRisesFromWater(void) {
    int best = -1; fx best_d = FxI(101);
    for (int r = 1; r < MAP_SIZE - 1; r++) for (int c = 1; c < MAP_SIZE - 1; c++) {
        if (VISUAL_MAP[r][c] != 0) continue;
        if (VISUAL_MAP[r - 1][c] != 12 && VISUAL_MAP[r + 1][c] != 12 && VISUAL_MAP[r][c - 1] != 12 && VISUAL_MAP[r][c + 1] != 12) continue;
        fx dx = FxI(c) - player_x, dy = FxI(r) - player_y, d = FxMul(dx, dx) + FxMul(dy, dy);
        if (d < FxI(16) || d >= best_d) continue;
        best_d = d; best = r * MAP_SIZE + c;
    }
    if (best < 0) return;
    enemy_x = FxI(best % MAP_SIZE); enemy_y = FxI(best / MAP_SIZE);
    SpawnTintedBurst(enemy_x, enemy_y, FxI(0), -FxI(1), RGB(120, 190, 255));
    size_t n = strlen(arpg_action_log);
    snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  A zombie rises out of the water!");
}

void TriggerMonsterRespawn(void) {
    int is_night = FxAbs(FxSin(day_night_cycle_accumulator)) < FX(0.35);
    int spawn_side = rand() % 4;
    if (spawn_side == 0) { enemy_x = FxI(2); enemy_y = FxI(rand() % 24 + 4); }
    else if (spawn_side == 1) { enemy_x = FxI(29); enemy_y = FxI(rand() % 24 + 4); }
    else if (spawn_side == 2) { enemy_x = FxI(rand() % 24 + 4); enemy_y = FxI(2); }
    else { enemy_x = FxI(rand() % 24 + 4); enemy_y = FxI(29); }
    int mythic = MythicForCurrentLand();
    int custom;
    int peak = current_biome == BIOME_MOUNTAIN && MountainIsPeak(current_screen_index); // Yeti haunt the summits
    if (mythic >= 0 && rand() % 100 < MYTHIC_SPAWN_CHANCE * (is_night ? 2 : 1) * (peak ? 3 : 1)) active_monster = (MonsterType)mythic;
    else if (is_night && rand() % 100 < 4 && GetMaterialCount("Garlic") == 0) active_monster = MONSTER_DRACULA;
    else if ((custom = CustomSpawnPick((int)current_biome, is_night)) >= 0) active_monster = (MonsterType)custom;
    else {
        // Satyr cultists keep to the wild Greek-style country: woods, meadows and hills.
        int satyr_land = current_biome == BIOME_FOREST || current_biome == BIOME_GRASSLANDS ||
                         current_biome == BIOME_PRAIRIE || current_biome == BIOME_MOUNTAIN;
        int r = rand() % 100;
        active_monster = r < 32 ? MONSTER_SKELLY : r < 64 ? MONSTER_ZOMBIE :
                         (r < 82 || !satyr_land) ? MONSTER_GOBLIN : MONSTER_SATYR;
    }
    werewolf_active = 0;
    if (!in_cave && FullMoonNight(is_night) && rand() % 100 < 25 &&
        (current_biome == BIOME_FOREST || current_biome == BIOME_GRASSLANDS || current_biome == BIOME_PRAIRIE)) {
        active_monster = MONSTER_BIGFOOT; werewolf_active = 1;
        strcpy(arpg_action_log, "The full moon rises... a WEREWOLF hunts tonight! Only silver bites deep.");
    }
    bounty_hunter_active = 0;
    if (!werewolf_active && !in_cave && village_npc_count == 0 && IsOutlaw() && rand() % 100 < 20) {
        active_monster = MONSTER_GOBLIN; bounty_hunter_active = 1;
        strcpy(arpg_action_log, "A BOUNTY HUNTER has tracked you down! Pay your fine to a town guard to call them off.");
    }
    if (active_monster == MONSTER_ZOMBIE && !in_cave) ZombieRisesFromWater();
    static const fx base_hearts[] = { FxI(3), FxI(3), FxI(6), FxI(2), FX(3.5), FxI(9), FxI(9), FxI(8), FxI(8) };
    enemy_hearts = base_hearts[active_monster] + (is_night ? FX(0.5) : FxI(0));
    if (bounty_hunter_active) enemy_hearts = FxI(6);
    if (IS_MYTHIC_MONSTER(active_monster)) {
        size_t n = strlen(arpg_action_log);
        snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  A %s roams this land!", FoeName());
    }
}

uint32_t hit_splash_color = 0;
int village_kill = 0; // set while villagers land the blow: their kills drop no loot or gold

void SpawnBloodMist(fx x, fx y) {
    // Tight, compact speck burst: small velocities/lifetimes so blood reads as
    // mostly one-pixel specks instead of an oversized expanding splatter.
    // Velocity is drawn from roughly [-0.0078, +0.0078] world units/frame
    // (rand()%100 spread over a 6400.0f divisor, re-centered by subtracting
    // the midpoint) and lifetime from 8-16 frames, both deliberately short
    // so specks fade before they can travel far or grow large on screen.
    for (int i = 0; i < MAX_BLOOD_MIST / 2; i++) {
        blood_mist[i].x = x; blood_mist[i].y = y;
        blood_mist[i].vx = (FxI(rand() % 100) / 6400 - FX(0.0078));
        blood_mist[i].vy = (FxI(rand() % 100) / 6400 - FX(0.0078));
        blood_mist[i].life = 8 + rand() % 9;
        blood_mist[i].max_life = blood_mist[i].life;
        blood_mist[i].active = 1;
        blood_mist[i].color = RGB(200, 25, 25);
    }
}

// Heavy or critical hits spray 8-16 specks away from the hero.
#define HEAVY_HIT_DAMAGE FxI(2)
#define SPEAR_REACH FX(2.6)
#define SPEAR_THRUST_DOT FX(0.7) // narrower than a sword swing: a straight thrust
static void SpawnTintedBurst(fx x, fx y, fx dir_x, fx dir_y, uint32_t color) {
    fx d = FxLen(dir_x, dir_y);
    if (d < FX(0.001)) { dir_x = FxI(0); dir_y = FxI(1); d = FxI(1); }
    dir_x = FxDiv(dir_x, d); dir_y = FxDiv(dir_y, d);
    int want = 8 + rand() % 9, spawned = 0;
    for (int i = 0; i < MAX_BLOOD_MIST && spawned < want; i++) {
        if (blood_mist[i].active) continue;
        fx spd = FX(0.012) + FxI(rand() % 100) / 4000;
        fx spread = FxI((rand() % 100) - 50) / 80;
        blood_mist[i].x = x; blood_mist[i].y = y;
        blood_mist[i].vx = FxMul(dir_x - FxMul(dir_y, spread), spd);
        blood_mist[i].vy = FxMul(dir_y + FxMul(dir_x, spread), spd);
        blood_mist[i].life = 10 + rand() % 9;
        blood_mist[i].max_life = blood_mist[i].life;
        blood_mist[i].active = 1;
        blood_mist[i].color = color;
        spawned++;
    }
}

void SpawnBloodBurst(fx x, fx y, fx dir_x, fx dir_y) { SpawnTintedBurst(x, y, dir_x, dir_y, RGB(200, 25, 25)); }

// Swinging on an empty stamina bar is weak and slow; the third such swing in a row
// leaves the hero exhausted for 10 seconds (no attacks, slow feet).
DWORD exhaustion_until = 0;
static int empty_swings = 0;
int HeroExhausted(void) { return GetTickCount() < exhaustion_until; }

int melee_combo_step = 0; // 0 overhead, 1 side sweep, 2 thrust
static int sword_swipe_len = 10;
void PerformMeleeAttack(void) {
    hit_splash_color = 0;
    WeaponStats *weapon = GetEquippedWeaponStats();
    fx damage = FxMul(weapon ? FxI(weapon->damage + EquippedForgeBonus()) : FxI(1), TownDamageMult());
    if (active_monster == MONSTER_DRACULA && SilverWeaponEquipped()) damage *= 2;
    fx stamina_cost = MELEE_SWING_STAMINA + (weapon ? weapon->stamina_cost * 2 : FxI(0));
    if (HeroExhausted()) { strcpy(arpg_action_log, "EXHAUSTED! Too spent to swing. Catch your breath."); return; }
    PlaySfx(SFX_SWING);
    int drained = player_stamina < stamina_cost;
    if (drained) {
        damage /= 2;
        if (++empty_swings >= 3) {
            empty_swings = 0; exhaustion_until = GetTickCount() + 10000;
            strcpy(arpg_action_log, "You're EXHAUSTED for 10 seconds! Back off and breathe.");
            return;
        }
    } else empty_swings = 0;
    player_stamina -= FxI(stamina_cost);
    if (player_stamina < FxI(0)) player_stamina = FxI(0);
    stamina_rest_timer = 0;
    sword_swipe_frame = drained ? 12 : (weapon && weapon->attack_speed_mult < FxI(1)) ? 9 : 6;
    sword_swipe_len = sword_swipe_frame;
    MeleeHitDesertPlant();
    SelectTargetEnemy();
    fx m_dist = DistanceToEnemy();
    int spear = active_weapon == WEAPON_SPEAR;
    // Sword/axe 3-hit combo: swings within 0.8 s chain overhead -> side sweep (wide arc) -> thrust (long reach, 1.5x).
    static DWORD last_swing = 0; DWORD now_swing = GetTickCount();
    melee_combo_step = (!spear && now_swing - last_swing < 800) ? (melee_combo_step + 1) % 3 : 0;
    last_swing = now_swing;
    fx reach = spear ? SPEAR_REACH : melee_combo_step == 2 ? FX(2.4) : FX(1.8);
    int facing_ok = spear ? IsFacingEnemyWithin(SPEAR_THRUST_DOT) : melee_combo_step == 1 ? IsFacingEnemyWithin(-FX(0.2)) :
                    melee_combo_step == 2 ? IsFacingEnemyWithin(FX(0.8)) : IsFacingEnemy();
    if (melee_combo_step == 1) damage = FxMul(damage, FX(1.1)); else if (melee_combo_step == 2) damage = FxMul(damage, FX(1.5));
    if (m_dist < reach && facing_ok) {
        int broke = WearEquippedWeapon();
        HandleEnemyDamage(FxMul(damage, FxI(1) + FX(0.08) * stat_spent[0]));
        if (broke) strcpy(arpg_action_log, "Your weapon broke!");
    }
}

void SpawnTreeDebris(fx cx, fx cy) {
    for (int i = 0; i < 5; i++) {
        twigs[i].x = cx; twigs[i].y = cy; twigs[i].vx = (FxI(rand() % 100) / 500 - FX(0.1)); twigs[i].vy = (FxI(rand() % 100) / 500 - FX(0.1));
        twigs[i].life = 40; twigs[i].screen_id = current_screen_index; twigs[i].active = 1;
    }
}

// Dirt clods kicked up when a summoned zombie or skeleton claws out of the ground.
void SpawnDirtClods(fx cx, fx cy) {
    int n = 0;
    for (int i = 0; i < DEBRIS_MAX && n < 8; i++) {
        if (twigs[i].active) continue;
        twigs[i].x = cx; twigs[i].y = cy;
        twigs[i].vx = FxI(rand() % 100) / 2500 - FX(0.02); twigs[i].vy = FxI(rand() % 100) / 2500 - FX(0.02);
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

#define JUNK_DROP_RATE FX(0.25)
#define GOLD_DROP_CAP 75
#define OVERKILL_BLOOD FxI(1) // hearts of damage past zero that trigger an extra spray
void HandleEnemyDamage(fx dmg) {
    if (FxAbs(FxSin(day_night_cycle_accumulator)) < FX(0.35)) dmg = FxMul(dmg, FX(0.9));
    if (werewolf_active) dmg = SilverWeaponEquipped() ? dmg * 2 : FxMul(dmg, FX(0.35));
    int crit = dmg > FxI(0) && rand() % 100 < 5 + 2 * stat_spent[4]; // each LCK point spent: +2% crit
    if (crit) dmg *= 2;
    enemy_hearts -= dmg;
    // Class abilities splash their own colour: green venom, blue arcane, gold counter...
    if (!hit_splash_color && selected_class == CLASS_ROGUE && is_sneaking) hit_splash_color = RGB(90, 200, 60);
    if (hit_splash_color) { SpawnTintedBurst(enemy_x, enemy_y, enemy_x - player_x, enemy_y - player_y, hit_splash_color); hit_splash_color = 0; }
    if (active_monster == MONSTER_YETI) enemies[current_enemy].angry = 1;
    fx k_dx = enemy_x - player_x; fx k_dy = enemy_y - player_y; fx k_dist = FxLen(k_dx, k_dy);
    if (crit || dmg >= HEAVY_HIT_DAMAGE) SpawnBloodBurst(enemy_x, enemy_y, k_dx, k_dy);
    if (k_dist > FX(0.01)) { enemy_x += FxMul(FxDiv(k_dx, k_dist), FX(0.25)); enemy_y += FxMul(FxDiv(k_dy, k_dist), FX(0.25)); }
    
    if (enemy_hearts <= FxI(0)) {
        SpawnBloodMist(enemy_x, enemy_y);
        if (-enemy_hearts >= OVERKILL_BLOOD) { SpawnBloodBurst(enemy_x, enemy_y, k_dx, k_dy); SpawnBloodBurst(enemy_x, enemy_y, k_dx, k_dy); }
        int is_night = FxAbs(FxSin(day_night_cycle_accumulator)) < FX(0.35);
        int gained_xp = FxTrunc(FxMul((10 + rand() % 7 + (is_night ? 2 : 0)) * opt_xp_mult, (is_night && night_bonus_active) ? FX(1.25) : FxI(1)));
        if (HolidayToday() == HOLIDAY_NEW_YEAR) gained_xp *= 2;
        else if (HolidayToday() == HOLIDAY_HALLOWEEN && is_night) gained_xp = gained_xp * 3 / 2;
        fx loot_multiplier = FxMul(opt_loot_mult, is_night ? FX(1.5) : FxI(1));
        // Ordinary (junk) drops are 60% rarer than they used to be, to cut ground clutter.
        int loot_chance = FxTrunc(FxMul(70 * JUNK_DROP_RATE, loot_multiplier)) + 3 * stat_spent[4];
        if (loot_chance > 100) loot_chance = 100;
        if (!village_kill && rand() % 100 < loot_chance) {
            int loot_id = LOOT_SHORT_SWORD + rand() % 5;
            int loot_quantity = FxTrunc(loot_multiplier);
            if (loot_quantity < 1) loot_quantity = 1;
            if (rand() % 100 < FxTrunc(FxFrac(loot_multiplier) * 100)) loot_quantity++;
            DropGroundLoot(enemy_x, enemy_y, loot_id, loot_quantity);
        }
        // A little gold, more with level and luck, never more than GOLD_DROP_CAP.
        int coins = 0;
        if (!village_kill && rand() % 100 < 35 + 2 * stat_spent[4]) {
            coins = 1 + rand() % (3 + player_level + 2 * stat_spent[4]);
            if (coins > GOLD_DROP_CAP) coins = GOLD_DROP_CAP;
            DropGroundLoot(enemy_x + FX(0.4), enemy_y, LOOT_GOLD, coins);
        }
        int hoard = 0;
        if (IS_BOSS_MONSTER(active_monster)) {
            // Bosses carry the rare treasure: a gold hoard. (Musashi's Blade only
            // washes up on an eastern beach; see GenerateProceduralScreen.)
            hoard = 150 + rand() % 151;
            DropGroundLoot(enemy_x - FX(0.4), enemy_y, LOOT_GOLD, hoard);
            gained_xp *= 4;
        }
        player_xp += gained_xp;
        if (IS_BOSS_MONSTER(active_monster)) {
            if (!(karma.life_marks & (1u << active_monster))) AddLegend("slew the %s", MonsterName(active_monster));
            karma.life_marks |= (uint16_t)(1u << active_monster); KarmaAdjust(5, 0, 0, 10);
        }
        else if (rand() % 5 == 0) KarmaAdjust(1, 0, 0, 0);
        if (player_xp >= player_next_level_xp) {
            // Each level needs 100 more XP than the last, so attribute points come slower.
            while (player_xp >= player_next_level_xp) {
                player_level++;
                player_xp -= player_next_level_xp;
                player_next_level_xp += 100;
                stat_points += STAT_POINTS_PER_LEVEL;
                if (player_level % 10 == 0) AddLegend("reached level %d", player_level);
            }
            sprintf(arpg_action_log, "LEVEL UP! Reached Level %d! %d attribute points to spend (Hero tab: Y / P).", player_level, stat_points);
        } else {
            if (hoard) sprintf(arpg_action_log, "VICTORY: The %s falls! +%d XP, its hoard of %d gold spills out!", FoeName(), gained_xp, hoard);
            else sprintf(arpg_action_log, "VICTORY: %s slain! +%d XP", FoeName(), gained_xp);
        }
        if (coins) { size_t n = strlen(arpg_action_log); snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  %d gold dropped", coins); }
        if (monster_card_links[active_monster].kills < 65535) monster_card_links[active_monster].kills++;
        TryCardDrop(active_monster);
        NoteLegendaryKill(active_monster);
        NoteQuestKill(active_monster);
    } else {
        sprintf(arpg_action_log, "%s: Hit for %s DMG! Remainder: %s", crit ? "CRITICAL" : "STRIKE", FxStr(dmg, 1), FxStr(enemy_hearts, 1));
    }
    StoreEnemy();
}

// Straight-line tile distance from the player to the active enemy. Shared
// helper for the melee range checks (gamepad/keyboard attack and the
// Knight/Warrior backstep counter-strike) to avoid duplicating the formula.
fx DistanceToEnemy(void) {
    return FxLen(player_x - enemy_x, player_y - enemy_y);
}

// Appropriate facing/range check for melee strikes: the enemy must be roughly
// in front of the player (within ~75.5 degrees of the facing axis, i.e. a
// ~151 degree total cone), not merely within radius, so axes/swords can't
// hit targets directly behind the hero.
// dot > 0.25 corresponds to an angle below ~75.5 degrees from the facing axis.
int IsFacingEnemyWithin(fx dot_threshold) {
    if (enemy_hearts <= FxI(0)) return 0;
    fx ex = enemy_x - player_x, ey = enemy_y - player_y;
    fx dist = FxLen(ex, ey);
    if (dist < FX(0.001)) return 1;
    ex = FxDiv(ex, dist); ey = FxDiv(ey, dist);
    fx hx = facing_x, hy = facing_y; // 8-direction facing
    fx dot = FxMul(ex, hx) + FxMul(ey, hy);
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
int EnemyAtPoint(fx x, fx y, fx radius) {
    StoreEnemy();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts > FxI(0) && FxAbs(x - enemies[i].x) < radius && FxAbs(y - enemies[i].y) < radius) { SelectEnemy(i); return i; }
    }
    return -1;
}

int SelectNearestEnemyTo(fx x, fx y) {
    StoreEnemy();
    int best = -1; fx best_d = INT32_MAX;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= FxI(0)) continue;
        fx dx = enemies[i].x - x, dy = enemies[i].y - y, d = FxMul(dx, dx) + FxMul(dy, dy);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best >= 0) SelectEnemy(best);
    return best;
}

// The player's target: the closest enemy they are facing, else the closest one.
int SelectTargetEnemy(void) {
    if (lock_on_enemy >= 0) {
        StoreEnemy();
        if (enemies[lock_on_enemy].hearts > FxI(0)) { SelectEnemy(lock_on_enemy); return current_enemy; }
    }
    int nearest = SelectNearestEnemyTo(player_x, player_y);
    if (nearest < 0) return -1;
    int facing = -1; fx best_d = INT32_MAX;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= FxI(0)) continue;
        SelectEnemy(i);
        fx d = DistanceToEnemy();
        if (IsFacingEnemy() && d < best_d) { best_d = d; facing = i; }
    }
    SelectEnemy(facing >= 0 ? facing : nearest);
    return current_enemy;
}

static int EnemyCap(void) { return FxAbs(FxSin(day_night_cycle_accumulator)) < FX(0.35) ? MAX_ENEMIES : MAX_ENEMIES_DAY; }

static void SpawnEnemyInFreeSlot(void) {
    StoreEnemy();
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts > FxI(0)) continue;
        SelectEnemy(i); TriggerMonsterRespawn(); enemy_direction = FACE_DOWN; StoreEnemy(); enemies[i].angry = 0; enemies[i].poison = 0;
        return;
    }
}

void SpawnScreenEnemies(void) {
    lock_on_enemy = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].hearts = FxI(0);
    SelectEnemy(0);
    int cap = EnemyCap();
    int n = cap / 2 + rand() % (cap / 2 + 1); // day 2-4, night 3-6
    if (village_light_count > 0) n /= 3; // villages are mostly safe
    for (int k = 0; k < n; k++) SpawnEnemyInFreeSlot();
}

// Village raids: goblin bandits, already angry.
void SpawnRaidBandits(int n) {
    for (int k = 0; k < n; k++) SpawnEnemyInFreeSlot();
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].hearts > FxI(0) && !IS_BOSS_MONSTER(enemies[i].type)) { enemies[i].type = MONSTER_GOBLIN; enemies[i].angry = 1; }
    SelectEnemy(current_enemy);
}

// Every ENEMY_SPAWN_INTERVAL_MS another monster wanders in while the screen is
// under its cap (4 by day, 6 at night); extras left at dawn wander off.
void UpdateEnemySpawns(void) {
    static DWORD last_spawn = 0;
    DWORD now = GetTickCount();
    if (now - last_spawn < ENEMY_SPAWN_INTERVAL_MS) return;
    last_spawn = now;
    if (ScreenStillQuiet()) return; // thinned out recently: monsters stay away for a while
    if (village_light_count > 0 && rand() % 3) return;
    StoreEnemy();
    int alive = 0, farthest = -1; fx far_d = -FxI(1);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= FxI(0)) continue;
        alive++;
        if (IS_BOSS_MONSTER(enemies[i].type)) continue;
        fx dx = enemies[i].x - player_x, dy = enemies[i].y - player_y, d = FxMul(dx, dx) + FxMul(dy, dy);
        if (d > far_d) { far_d = d; farthest = i; }
    }
    int cap = EnemyCap();
    if (alive < cap) SpawnEnemyInFreeSlot();
    else if (alive > cap && farthest >= 0) { enemies[farthest].hearts = FxI(0); SelectEnemy(current_enemy); }
}

// Arrows fly straight along the hero's facing; they only curve toward the
// monster when the hero is actually facing it (within BOW_AIM_CONE_DOT).
#define BOW_AIM_CONE_DOT FX(0.5)
// Holding the draw (RT / F) costs stamina every frame; once winded the
// charge stops building, though the arrow can still be loosed.
#define BOW_DRAW_STAMINA_PER_FRAME FxI(1)
#define BOW_TIRED_MS 10000
DWORD bow_stamina_lock_until = 0;
void ChargeBow(void) {
    if (player_stamina <= FxI(0)) return;
    player_stamina -= BOW_DRAW_STAMINA_PER_FRAME;
    if (player_stamina < FxI(0)) player_stamina = FxI(0);
    bow_charge_time += FX(0.05);
    if (bow_charge_time > FxI(1)) bow_charge_time = FxI(1);
    if (bow_charge_time >= FxI(1)) bow_stamina_lock_until = GetTickCount() + BOW_TIRED_MS; // held too long: arms are spent
}

void FirePlayerArrow(int is_tap) {
    player_arrow.x = player_x; player_arrow.y = player_y; player_arrow.z = FxI(12);
    player_arrow.origin_tx = FxTrunc(player_x); player_arrow.origin_ty = FxTrunc(player_y);
    fx track_dx = FxI(0), track_dy = FxI(0);
    SelectTargetEnemy();
    if (IsFacingEnemyWithin(BOW_AIM_CONE_DOT)) {
        track_dx = enemy_x - player_x; track_dy = enemy_y - player_y;
    } else {
        if (player_facing == FACE_UP) track_dy = -FxI(1);
        else if (player_facing == FACE_DOWN) track_dy = FxI(1);
        else if (player_facing == FACE_LEFT) track_dx = -FxI(1);
        else track_dx = FxI(1);
    }
    fx distance = FxLen(track_dx, track_dy);
    if (distance < FX(0.001)) { track_dx = FxI(1); distance = FxI(1); }
    fx distance_mult = is_tap ? FX(0.5) : FX(0.5) + FxMul(bow_charge_time, FX(1.5));
    fx spd = FxMul(FX(0.35), distance_mult);
    player_arrow.vx = FxMul(FxDiv(track_dx, distance), spd);
    player_arrow.vy = FxMul(FxDiv(track_dy, distance), spd);
    player_arrow.vz = is_tap ? FX(0.25) : FX(0.5);
    player_arrow.damage = is_tap ? FX(0.4) : (FX(0.5) + FxMul(bow_charge_time, FX(1.5)));
    for (int i = 0; i < player_item_count; i++)
        if (player_inventory[i].is_equipped && strcmp(player_inventory[i].name, "Long Bow") == 0) { player_arrow.damage = FxMul(player_arrow.damage, FX(1.4)); player_arrow.vx = FxMul(player_arrow.vx, FX(1.2)); player_arrow.vy = FxMul(player_arrow.vy, FX(1.2)); break; }
    player_arrow.active = 1; // Projectile active only upon release
    if (is_tap) {
        sprintf(arpg_action_log, "BOW: Quick-shot tap fired! (Light dmg: %s)", FxStr(player_arrow.damage, 1));
    } else {
        sprintf(arpg_action_log, "BOW: Charged shot unleashed! (Dmg: %s, Charge: %d%%)", FxStr(player_arrow.damage, 1), FxTrunc(bow_charge_time * 100));
    }
}

// Trees and rocks stop an arrow, and it vanishes at the edge of the screen.
static int ArrowBlockedAt(fx x, fx y) {
    if (x < FX(0.5) || y < FX(0.5) || x >= FxI(MAP_SIZE) - FX(0.5) || y >= FxI(MAP_SIZE) - FX(0.5)) return 1;
    int tx = (int)x, ty = (int)y;
    if (tx == player_arrow.origin_tx && ty == player_arrow.origin_ty) return 0;
    uint8_t t = VISUAL_MAP[ty][tx];
    if (t == 7) { BreakDesertPlant(tx, ty, 0); return 1; }
    return t == 11 || t == 1 || t == 3;
}

void UpdatePlayerArrow(void) {
    if (!player_arrow.active) return;
    for (int step = 0; step < 2; step++) {
        player_arrow.x += player_arrow.vx / 2; player_arrow.y += player_arrow.vy / 2;
        if (ArrowBlockedAt(player_arrow.x, player_arrow.y)) { player_arrow.active = 0; return; }
    }
    player_arrow.z += player_arrow.vz; player_arrow.vz -= FX(0.02);
    fx arrow_radius = FX(0.5) + FxAbs(player_arrow.vx) + FxAbs(player_arrow.vy);
    if (EnemyAtPoint(player_arrow.x, player_arrow.y, arrow_radius) >= 0) {
        player_arrow.active = 0;
        int broke = WearEquippedBow();
        HandleEnemyDamage(player_arrow.damage);
        if (broke) strcpy(arpg_action_log, "Your bow broke!");
    }
    if (player_arrow.z <= FxI(0)) player_arrow.active = 0;
}

// Spear, second mode: RT / F throws the equipped spear. It flies along the
// hero's facing (curving to a faced enemy), drops with gravity and lands as a
// dropped item that X picks back up with its durability intact.
#define SPEAR_THROW_SPEED FX(0.3)
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
    fx tdx = FxI(0), tdy = FxI(0);
    SelectTargetEnemy();
    if (IsFacingEnemyWithin(BOW_AIM_CONE_DOT)) { tdx = enemy_x - player_x; tdy = enemy_y - player_y; }
    else if (player_facing == FACE_UP) tdy = -FxI(1);
    else if (player_facing == FACE_DOWN) tdy = FxI(1);
    else if (player_facing == FACE_LEFT) tdx = -FxI(1);
    else tdx = FxI(1);
    fx d = FxLen(tdx, tdy);
    if (d < FX(0.001)) { tdx = FxI(1); d = FxI(1); }
    thrown_spear.x = player_x; thrown_spear.y = player_y; thrown_spear.z = FxI(14);
    thrown_spear.vx = FxMul(FxDiv(tdx, d), SPEAR_THROW_SPEED); thrown_spear.vy = FxMul(FxDiv(tdy, d), SPEAR_THROW_SPEED);
    thrown_spear.vz = FX(0.3);
    thrown_spear.origin_tx = FxTrunc(player_x); thrown_spear.origin_ty = FxTrunc(player_y);
    thrown_spear.damage = FxMul(FxI(thrown_spear_item.damage) * 3 / 2, FxI(1) + FX(0.08) * stat_spent[0]);
    thrown_spear.active = 1;
    player_stamina -= FxI(5); if (player_stamina < FxI(0)) player_stamina = FxI(0);
    strcpy(arpg_action_log, "SPEAR: Thrown! Stand on it and press X to pick it up.");
}

static int SpearBlockedAt(fx x, fx y) {
    if (x < FX(0.5) || y < FX(0.5) || x >= FxI(MAP_SIZE) - FX(0.5) || y >= FxI(MAP_SIZE) - FX(0.5)) return 1;
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
        fx nx = thrown_spear.x + thrown_spear.vx / 2, ny = thrown_spear.y + thrown_spear.vy / 2;
        if (SpearBlockedAt(nx, ny)) { LandThrownSpear(); return; }
        thrown_spear.x = nx; thrown_spear.y = ny;
    }
    thrown_spear.z += thrown_spear.vz; thrown_spear.vz -= FX(0.03);
    if (EnemyAtPoint(thrown_spear.x, thrown_spear.y, FX(0.6)) >= 0) {
        HandleEnemyDamage(thrown_spear.damage);
        if (--thrown_spear_item.durability_current <= 0) { thrown_spear.active = 0; strcpy(arpg_action_log, "Your spear broke!"); return; }
        LandThrownSpear();
        return;
    }
    if (thrown_spear.z <= FxI(0)) LandThrownSpear();
}

// Debounced/cooldown-bound class special ability bound to the "Y" button
// (gamepad and keyboard). Callers must only invoke this on the rising edge of
// the button press so it fires once per press rather than every held frame.
// See the FACING_CONE_DOT_THRESHOLD / *_COOLDOWN_MS / *_SUPPRESS_MS /
// *_FLASH_FRAMES constants defined near the top of the file for tuning.
// Every class ability grows in three stages: level 1, 3 and 5.
static int AbilityStage(void) { return player_level >= 5 ? 3 : player_level >= 3 ? 2 : 1; }
#define ZOMBIE_EXTRA_MP FxI(10)

void FireClassAbility(void) {
    DWORD now = GetTickCount();
    int stage = AbilityStage();
    // Signed-subtraction comparison (rather than `now < y_ability_cooldown_until`)
    // stays correct across the ~49.7 day GetTickCount() wraparound boundary,
    // matching the (GetTickCount() - last_x > threshold) pattern used elsewhere
    // in this file for other input-debounce timers.
    long cooldown_remaining_ms = (long)(y_ability_cooldown_until - now);
    if (cooldown_remaining_ms > 0) {
        sprintf(arpg_action_log, "ABILITY: On cooldown (%d.%ds left)", (int)(cooldown_remaining_ms / 1000), (int)(cooldown_remaining_ms % 1000 / 100));
        return;
    }

    if (player_mp < CLASS_ABILITY_MP_COST) {
        sprintf(arpg_action_log, "ABILITY: Not enough magic (%d/%d MP).", FxTrunc(player_mp), FxTrunc(CLASS_ABILITY_MP_COST));
        return;
    }

    if (selected_class == CLASS_NECROMANCER || selected_class == CLASS_WIZARD) {
        if (selected_class == CLASS_NECROMANCER) {
            if (summoned_zombie.active) { strcpy(arpg_action_log, "NECROMANCY: Your summoned dead still fight."); return; }
            summoned_zombie.x = player_x; summoned_zombie.y = player_y;
            if (player_facing == FACE_UP) summoned_zombie.y -= FxI(1);
            else if (player_facing == FACE_DOWN) summoned_zombie.y += FxI(1);
            else if (player_facing == FACE_LEFT) summoned_zombie.x -= FxI(1);
            else summoned_zombie.x += FxI(1);
            // Ghost (fast, weakest, fades soonest) from level 1, Skeleton from 3,
            // Zombie (hardest hitting, poison, lingers longest, extra MP) from 5.
            // Each cast raises the next unlocked one.
            static const int unlock_level[3] = { 5, 3, 1 };      // zombie, skeleton, ghost
            static const fx summon_hp[3] = { FxI(4), FxI(5), FxI(2) };
            static const int summon_linger[3] = { 1200, 900, 600 }; // frames
            static const char *summon_msg[3] = { "NECROMANCY: A zombie claws its way from the earth! Its bite poisons.",
                                                 "NECROMANCY: A skeleton warrior rises to shield you!",
                                                 "NECROMANCY: A ghost drifts out of the veil!" };
            int type = player_level >= unlock_level[next_summon_type] ? next_summon_type : SUMMON_GHOST;
            fx summon_mp = CLASS_ABILITY_MP_COST + (type == SUMMON_ZOMBIE ? ZOMBIE_EXTRA_MP : FxI(0));
            if (player_mp < summon_mp) { sprintf(arpg_action_log, "ABILITY: Not enough magic (%d/%d MP).", FxTrunc(player_mp), FxTrunc(summon_mp)); return; }
            next_summon_type = type;
            do next_summon_type = (next_summon_type + 1) % 3; while (player_level < unlock_level[next_summon_type]);
            summoned_zombie.type = type;
            summoned_zombie.hp = summon_hp[type]; summoned_zombie.timer = summon_linger[type];
            summoned_zombie.raise_frame = 0; summoned_zombie.attack_cooldown = 0;
            summoned_zombie.active = 1;
            if (type != SUMMON_GHOST) SpawnDirtClods(summoned_zombie.x, summoned_zombie.y + FX(0.3));
            KarmaAdjust(0, 5, 0, 0);
            ability_visual_frame = SPELL_FLASH_FRAMES;
            player_mp -= summon_mp; y_ability_cooldown_until = now + SPELL_COOLDOWN_MS;
            strcpy(arpg_action_log, summon_msg[type]);
            return;
        }
        if (player_spell.active) {
            strcpy(arpg_action_log, "ABILITY: Bolt still in flight!");
            return;
        }
        player_spell.x = player_x; player_spell.y = player_y; player_spell.z = FxI(10);
        fx track_dx = FxI(0), track_dy = FxI(0);
        SelectTargetEnemy();
        if (enemy_hearts > FxI(0)) {
            track_dx = enemy_x - player_x; track_dy = enemy_y - player_y;
        } else if (player_facing == FACE_UP) track_dy = -FxI(1);
        else if (player_facing == FACE_DOWN) track_dy = FxI(1);
        else if (player_facing == FACE_LEFT) track_dx = -FxI(1);
        else track_dx = FxI(1);

        fx distance = FxLen(track_dx, track_dy);
        if (distance < FX(0.001)) { track_dx = FxI(1); distance = FxI(1); }
        fx spd = FX(0.28) + FX(0.06) * stage;
        player_spell.vx = FxMul(FxDiv(track_dx, distance), spd);
        player_spell.vy = FxMul(FxDiv(track_dy, distance), spd);
        player_spell.vz = FxI(0);
        player_spell.damage = FxI(1) + FX(0.75) * stage;
        player_spell.active = 1;
        ability_visual_frame = SPELL_FLASH_FRAMES;
        player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + SPELL_COOLDOWN_MS;
        strcpy(arpg_action_log, "ARCANE: Magic bolt unleashed!");
    } else if (selected_class == CLASS_KNIGHT || selected_class == CLASS_WARRIOR) {
        fx ndx = FxI(0), ndy = FxI(0);
        if (player_facing == FACE_UP) ndy = FxI(1);
        else if (player_facing == FACE_DOWN) ndy = -FxI(1);
        else if (player_facing == FACE_LEFT) ndx = FxI(1);
        else ndx = -FxI(1);

        fx hop = FX(0.5) + FX(0.5) * stage;
        fx tx = player_x + FxMul(ndx, hop), ty = player_y + FxMul(ndy, hop);
        if (tx >= 0 && tx < MAP_SIZE && ty >= 0 && ty < MAP_SIZE &&
            VISUAL_MAP[(int)ty][(int)tx] != 1 && VISUAL_MAP[(int)ty][(int)tx] != 12) {
            player_x = tx; player_y = ty;
        }

        monster_pursuit_suppressed_until = now + BACKSTEP_SUPPRESS_MS;
        ability_visual_frame = ABILITY_FLASH_FRAMES;
        player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + BACKSTEP_COOLDOWN_MS;

        SelectTargetEnemy();
        fx m_dist = DistanceToEnemy();
        if (enemy_hearts > FxI(0) && m_dist < FxI(2) && IsFacingEnemy()) {
            sword_swipe_frame = 10;
            hit_splash_color = RGB(255, 210, 60);
            HandleEnemyDamage(FxI(1) + FxI(1) * stage);
            strcpy(arpg_action_log, "DEFENSIVE BACKSTEP: Countered with a heavy strike!");
        } else {
            strcpy(arpg_action_log, "DEFENSIVE BACKSTEP: Repositioned, enemies briefly wary!");
        }
    } else { // Rogue
        is_sneaking = 1;
        int sneak_ms = SNEAK_SUPPRESS_MS + 2000 * (stage - 1); // 4 / 6 / 8 seconds
        monster_pursuit_suppressed_until = now + sneak_ms;
        ability_visual_frame = ABILITY_FLASH_FRAMES;
        player_mp -= CLASS_ABILITY_MP_COST; y_ability_cooldown_until = now + SNEAK_COOLDOWN_MS + 2000 * (stage - 1);
        sprintf(arpg_action_log, "SNEAK: Breaking monster detection for %ds!", sneak_ms / 1000);
    }
}

// ---- The hero can be hurt (forgiving for now) ----
DWORD hero_held_until = 0; // a zombie's grab holds the hero in place briefly
int hero_hop_frame = 0;    // counts down while the hero is mid-hop
int hero_hop_len = 18, hero_hop_double = 0;
static fx hero_hop_start = FxI(0), hero_hop_peak = FxI(14);
// Height in pixels: the second hop of a double jump starts from wherever the first one was.
fx HeroHopHeight(void) {
    if (hero_hop_frame <= 0) return FxI(0);
    fx t = FxI(hero_hop_len - hero_hop_frame) / hero_hop_len;
    return FxMul(hero_hop_start, FxI(1) - t) + FxMul(FxSin(FxMul(FX_PI, t)), hero_hop_peak);
}
void StartHeroHop(int second) {
    hero_hop_start = second ? HeroHopHeight() : FxI(0);
    hero_hop_double = second; hero_hop_len = second ? 24 : 18; hero_hop_peak = second ? FxI(18) : FxI(14);
    hero_hop_frame = hero_hop_len;
}

// Nearest known castle town on the world map, else -1.
static int NearestTownCell(void) {
    int best = -1, best_d = 1 << 30;
    for (int gy = 0; gy < META_GRID_SIZE; gy++) for (int gx = 0; gx < META_GRID_SIZE; gx++) {
        if (!WorldIsTown(gx, gy)) continue;
        int d = (gx - screen_grid_x) * (gx - screen_grid_x) + (gy - screen_grid_y) * (gy - screen_grid_y);
        if (d < best_d) { best_d = d; best = gy * META_GRID_SIZE + gx; }
    }
    return best;
}

// Knocked out: a townsfolk carries the hero to the nearest town. Nothing is lost.
static void RescueHero(void) {
    int cell = NearestTownCell();
    if (in_cave) { in_cave = 0; cave_is_hold = 0; pending_forced_biome = -1; }
    if (fishing_active) fishing_active = 0;
    if (cell >= 0) {
        screen_grid_x = cell % META_GRID_SIZE; screen_grid_y = cell / META_GRID_SIZE;
        screens_until_town = 0;
        GenerateProceduralScreen(30000 + cell);
    } else GenerateProceduralScreen(screen_grid_y * META_GRID_SIZE + screen_grid_x);
    player_x = FxI(MAP_SIZE) / 2; player_y = FxI(MAP_SIZE) / 2;
    for (int r = 0; r < MAP_SIZE / 2 && !PlayerCanStandAt(player_x, player_y); r++) player_x = FxI(MAP_SIZE) / 2 + FxI(r * ((r & 1) ? 1 : -1));
    player_hp = FxMul(max_player_hp, FX(0.6)); player_stamina = max_stamina; hero_held_until = 0;
    strcpy(arpg_action_log, cell >= 0 ? "A townsman: \"Found you out cold and carried you here. My son's fetched all your things - nothing's missing.\""
                                      : "A passing villager: \"You took a nasty knock. I watched over you and my son gathered your things.\"");
}

void HurtHero(fx dmg) {
    if (dmg <= FxI(0) || player_hp <= FxI(0)) return;
    player_hp -= dmg;
    if (player_hp <= FxI(0)) {
        player_hp = FxI(0);
        char cause[24]; snprintf(cause, sizeof(cause), "Fell to %s", MonsterName(active_monster));
        RecordHallEntry(cause, 0);
        AddLegend("fell to %s", MonsterName(active_monster));
        RescueHero();
    }
}

// Called for each live monster (selected into enemy_x/enemy_y/active_monster).
void EnemyAttackHero(int e) {
    static DWORD next_hit[MAX_ENEMIES];
    if (e < 0 || e >= MAX_ENEMIES || inside_home >= 0 || town_menu != TM_NONE || is_character_creation) return;
    fx dx = player_x - enemy_x, dy = player_y - enemy_y, d2 = FxMul(dx, dx) + FxMul(dy, dy);
    DWORD now = GetTickCount();
    if (active_monster == MONSTER_DRACULA && GetMaterialCount("Garlic") == 0) { // vampires drain from a distance and heal
        fx reach = FX(1.4) + FxMul(FX(0.35), enemy_hearts > FxI(4) ? FxI(4) : enemy_hearts); // stronger ones reach further
        if (d2 < reach * reach && world_frame % 20 == 0) {
            HurtHero(FX(0.4));
            if (enemy_hearts < FxI(6)) enemy_hearts += FX(0.04);
            if (world_frame % 120 == 0) strcpy(arpg_action_log, "VAMPIRE: It drinks your life from afar! Get away or strike it.");
        }
    }
    if (d2 > FX(1.25 * 1.25) || hero_hop_frame > 0 || (int)(now - next_hit[e]) < 0) return;
    int big = IS_MYTHIC_MONSTER(active_monster);
    next_hit[e] = now + (big ? 1800 : 1400) + rand() % 600;
    fx dmg = big ? FxI(5) : FxI(2);
    if (active_monster == MONSTER_ZOMBIE) { hero_held_until = now + 550; strcpy(arpg_action_log, "ZOMBIE: It grabs you and bites!"); }
    else if (active_monster == MONSTER_BIGFOOT || active_monster == MONSTER_YETI) strcpy(arpg_action_log, "A huge arm swats you!");
    HurtHero(dmg);
}

// The hero faces any of 8 directions (diagonals included).
fx facing_x = FxI(0), facing_y = FxI(1);
void SetHeroFacing8(fx dx, fx dy) {
    if (dx == FxI(0) && dy == FxI(0)) return;
    fx l = FxLen(dx, dy);
    facing_x = FxDiv(dx, l); facing_y = FxDiv(dy, l);
}

// L3 / L lock-on: the hero keeps facing the locked enemy while moving, so you can
// strafe and circle it; swings, spears and arrows aim at it. It lets go when the
// enemy dies, gets more than 10 tiles away, or you leave the screen.
int lock_on_enemy = -1;

void ToggleLockOn(void) {
    if (lock_on_enemy >= 0) { lock_on_enemy = -1; strcpy(arpg_action_log, "Lock-on released."); return; }
    StoreEnemy();
    int best = -1; fx best_d = FxI(64);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].hearts <= FxI(0)) continue;
        fx dx = enemies[i].x - player_x, dy = enemies[i].y - player_y, d = FxMul(dx, dx) + FxMul(dy, dy);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) { strcpy(arpg_action_log, "LOCK-ON: no enemy within 8 tiles."); return; }
    lock_on_enemy = best;
    snprintf(arpg_action_log, sizeof(arpg_action_log), "LOCK-ON: %s. Move to circle it; L3 or L again lets go.", MonsterName(enemies[best].type));
}

void LockOnUpdate(void) {
    static int key_was = 0;
    int key = (GetAsyncKeyState('L') & 0x8000) != 0;
    if (key && !key_was && !is_menu_open && !town_menu && !vendor_menu_open) ToggleLockOn();
    key_was = key;
    if (lock_on_enemy < 0) return;
    StoreEnemy();
    EnemySlot *e = &enemies[lock_on_enemy];
    fx dx = e->x - player_x, dy = e->y - player_y;
    if (e->hearts <= FxI(0) || FxMul(dx, dx) + FxMul(dy, dy) > FxI(100)) { lock_on_enemy = -1; return; }
    SetHeroFacing8(dx, dy);
    player_facing = FxAbs(dx) > FxAbs(dy) ? (dx < 0 ? FACE_LEFT : FACE_RIGHT) : (dy < 0 ? FACE_UP : FACE_DOWN);
}

// A red arrow bobbing over the locked enemy and a ring at its feet.
void DrawLockOnMarker(HDC hdc) {
    if (lock_on_enemy < 0 || enemies[lock_on_enemy].hearts <= FxI(0)) return;
    int sx, sy; GetIsoCoords(enemies[lock_on_enemy].x, enemies[lock_on_enemy].y, &sx, &sy);
    int top = sy - 58 + FxTrunc(FxSinTurn(GetTickCount() * (uint32_t)(4294967296.0 / (6.283185307 * 160))) * 3), feet = sy + TILE_HEIGHT / 2;
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 70, 50));
    HGDIOBJ op = SelectObject(hdc, pen), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    POINT tri[3] = { { sx - 7, top }, { sx + 7, top }, { sx, top + 9 } };
    Polygon(hdc, tri, 3);
    Ellipse(hdc, sx - 20, feet - 8, sx + 20, feet + 8);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(pen);
}
// A sword/axe slash: three streaks sweeping across the facing direction on screen.
// An iron sword from hilt (x0,y0) to tip (x1,y1): leather grip, dark crossguard,
// a pale steel blade with a bright edge line, so it reads as metal, not wood.
void DrawIronSword(HDC hdc, int x0, int y0, int x1, int y1) {
    fx dx = FxI(x1 - x0), dy = FxI(y1 - y0), d = FxLen(dx, dy);
    if (d < FxI(1)) return;
    dx = FxDiv(dx, d); dy = FxDiv(dy, d);
    fx px = -dy, py = dx;
    HPEN grip = CreatePen(PS_SOLID, 3, RGB(70, 45, 30)), guard = CreatePen(PS_SOLID, 2, RGB(80, 82, 92)),
         blade = CreatePen(PS_SOLID, 3, RGB(160, 168, 180)), edge = CreatePen(PS_SOLID, 1, RGB(240, 244, 250));
    HGDIOBJ op = SelectObject(hdc, grip);
    MoveToEx(hdc, x0 - FxTrunc(dx * 5), y0 - FxTrunc(dy * 5), NULL); LineTo(hdc, x0, y0);
    SelectObject(hdc, guard);
    MoveToEx(hdc, x0 - (int)(px * 5), y0 - (int)(py * 5), NULL); LineTo(hdc, x0 + (int)(px * 5), y0 + (int)(py * 5));
    SelectObject(hdc, blade);
    MoveToEx(hdc, x0 + FxTrunc(dx * 2), y0 + FxTrunc(dy * 2), NULL); LineTo(hdc, x1, y1);
    SelectObject(hdc, edge);
    MoveToEx(hdc, x0 + FxTrunc(dx * 3), y0 + FxTrunc(dy * 3), NULL); LineTo(hdc, x1 - FxTrunc(dx * 1), y1 - FxTrunc(dy * 1));
    SelectObject(hdc, op);
    DeleteObject(grip); DeleteObject(guard); DeleteObject(blade); DeleteObject(edge);
}

// Each combo hit has its own path: 1st an overhead chop falling from above the
// head to the ground in front, 2nd a wide sweep across the front, 3rd a thrust.
// The blade itself is drawn once where it is now, with two thin motion lines
// trailing near its tip to sell the speed (no ghost copies of the sword).
static void SlashPoint(fx a0, fx t, fx r, int sx, int cy, int *x, int *y) {
    fx ux = FxCos(a0), uy = FxMul(FxSin(a0), FX(0.6));
    if (melee_combo_step == 2) { fx reach = FxMul(r, FX(0.45) + FxMul(t, FX(0.75))); *x = sx + FxTrunc(FxMul(ux, reach)); *y = cy + FxTrunc(FxMul(uy, reach)); }
    else if (melee_combo_step == 1) { fx a = a0 + FxMul(t, FX(2.6)) - FX(1.3); *x = sx + FxTrunc(FxMul(FxCos(a), r)); *y = cy + FxTrunc(FxMul(FxMul(FxSin(a), r), FX(0.6))); }
    else { fx p = FX(1.55) - FxMul(t, FX(1.9)); *x = sx + FxTrunc(FxMul(FxMul(ux, r), FxCos(p))); *y = cy + FxTrunc(FxMul(FxMul(uy, r), FxCos(p)) - FxMul(r, FxSin(p))); }
}
void DrawSlash(HDC hdc, int sx, int sy) {
    fx a0 = FxAtan2((facing_x + facing_y) / 2, facing_x - facing_y);
    fx t = FxI(1) - FxI(sword_swipe_frame) / sword_swipe_len;
    int cy = sy + 6, hx, hy, tx, ty;
    HPEN motion = CreatePen(PS_SOLID, 1, RGB(225, 232, 242)); HGDIOBJ op = SelectObject(hdc, motion);
    for (int k = 1; k <= 2; k++) { // motion lines: short streaks along the path the tip just travelled
        fx t0 = t - FX(0.12) * k, t1 = t - FX(0.12) * k - FX(0.18), rr = FxI(26 + k * 3);
        if (t0 < FxI(0)) continue;
        if (t1 < FxI(0)) t1 = FxI(0);
        int ax, ay, bx, by;
        if (melee_combo_step == 2) { // thrust: speed lines parallel to the blade, either side
            fx ux = FxCos(a0), uy = FxMul(FxSin(a0), FX(0.6)), s = k == 1 ? FxI(6) : -FxI(6);
            SlashPoint(a0, t0, FxI(20), sx, cy, &ax, &ay); SlashPoint(a0, t1, FxI(20), sx, cy, &bx, &by);
            ax += FxTrunc(FxMul(-uy, s)); ay += FxTrunc(FxMul(FxMul(ux, s), FX(0.6))); bx += FxTrunc(FxMul(-uy, s)); by += FxTrunc(FxMul(FxMul(ux, s), FX(0.6)));
        } else { SlashPoint(a0, t0, rr, sx, cy, &ax, &ay); SlashPoint(a0, t1, rr, sx, cy, &bx, &by); }
        MoveToEx(hdc, ax, ay, NULL); LineTo(hdc, bx, by);
    }
    SelectObject(hdc, op); DeleteObject(motion);
    SlashPoint(a0, t, FxI(7), sx, cy, &hx, &hy);
    SlashPoint(a0, t, FxI(28), sx, cy, &tx, &ty);
    if (melee_combo_step == 2) { hx = tx - FxTrunc(FxCos(a0) * 21); hy = ty - FxTrunc(FxMul(FxSin(a0), FX(0.6)) * 21); }
    DrawIronSword(hdc, hx, hy, tx, ty);
}
