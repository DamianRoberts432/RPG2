#ifndef GAME_H
#define GAME_H

#ifndef WINVER
#define WINVER 0x0500
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>
#include <xinput.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#define TILE_WIDTH 64
#define TILE_HEIGHT 32
#define WINDOW_WIDTH 1824
#define WINDOW_HEIGHT 986
#define MAP_SIZE 32
#define META_GRID_SIZE 64
#define WORLD_GRID_SIZE (META_GRID_SIZE * META_GRID_SIZE)
#define MAX_BUTTERFLIES 8
#define DEBRIS_MAX 16
#define MAX_CRITTERS 4
#define MERCH_ITEMS 16
#define MAX_BLOOD_MIST 24
#define MAX_CAMPFIRES 8
#define MAX_INVENTORY_ITEMS 100
#define MAX_GROUND_LOOT 16
#define MAX_DROPPED_ITEMS 16
#define MAX_NPCS 8
#define MAX_APPLE_TREES 8

// Class-special "Y" ability tuning (cooldowns, pursuit-suppression windows,
// and GDI flash durations). Defined up front so they can be referenced by
// both FireClassAbility() and the rendering helpers (e.g. DrawClassAbilityFX)
// that run earlier in the file. Suppress windows are intentionally shorter
// than their matching cooldowns so pursuit suppression always lapses before
// the ability can be used again.
#define FACING_CONE_DOT_THRESHOLD 0.25f
#define SPELL_COOLDOWN_MS     1500
#define BACKSTEP_COOLDOWN_MS  2000
#define SNEAK_COOLDOWN_MS     5000
#define BACKSTEP_SUPPRESS_MS  1200
#define SNEAK_SUPPRESS_MS     4000
#define SPELL_FLASH_FRAMES    10
#define ABILITY_FLASH_FRAMES  14

typedef enum { FACE_DOWN, FACE_UP, FACE_LEFT, FACE_RIGHT } FacingDirection;
typedef enum { WEAPON_SWORD, WEAPON_BOW, WEAPON_AXE } WeaponType;
typedef enum { MONSTER_SKELLY, MONSTER_ZOMBIE, MONSTER_DRACULA } MonsterType;
// 13 "world" biomes (Prairie through River) that the procedural generator
// picks between for ordinary overworld screens, plus two special-purpose
// locations (Cave interiors, the Castle endpoint) that are never chosen by
// the regular adjacency picker and are instead entered/forced explicitly.
typedef enum {
    BIOME_PRAIRIE, BIOME_SWAMP, BIOME_BEACH, BIOME_MOUNTAIN,
    BIOME_DESERT, BIOME_TUNDRA, BIOME_GRASSLANDS, BIOME_OASIS, BIOME_FOREST,
    BIOME_ISLAND, BIOME_OCEAN, BIOME_LAKE, BIOME_RIVER,
    BIOME_CAVE, BIOME_CASTLE
} BiomeType;
extern const char *biome_names[];
typedef enum { WEATHER_CLEAR, WEATHER_RAIN, WEATHER_SNOW } WeatherType;

typedef enum { RACE_ELF, RACE_HUMAN, RACE_DWARF, RACE_HALFLING, RACE_GNOME } RaceType;
typedef enum { CLASS_NECROMANCER, CLASS_WIZARD, CLASS_KNIGHT, CLASS_WARRIOR, CLASS_ROGUE } ClassType;

typedef struct { float x, y, z; float vx, vy, vz; int active; float damage; int origin_tx, origin_ty; } ArrowProjectile;
typedef struct { float x, y, z; float timer; int active; int color_profile; } ButterflyParticle;
typedef struct { float x, y; float vx, vy; int life; int screen_id; int active; } DebrisParticle;
typedef struct { float x, y; float vx, vy; int life; int max_life; int active; } BloodParticle;
typedef struct { float x, y; float timer; int active; int screen_id; } CampfireEntity;
typedef struct { float x, y; int type; int state; float base_x, base_y; int active; int hidden; } CritterEntity;
// Rarity tiers drive the merchant's price multiplier directly: the enum
// value itself *is* the multiplier applied to base_price (Common=1x,
// Uncommon=2x, Rare=5x, Legendary=10x+), per the catalog rarity scheme.
typedef enum { RARITY_COMMON = 1, RARITY_UNCOMMON = 2, RARITY_RARE = 5, RARITY_LEGENDARY = 10 } ItemRarity;
typedef struct {
    const char* name;
    const char* slot;   // "Weapon", "Armor", "Shield", "Tool", "Food", "Material"
    int loot_id;         // matching LOOT_* id for items that need full stats (weapons), or 0 for generic items
    int base_price;      // 1x (Common) price; actual price = base_price * rarity
    int stock;           // current stock; -1 == unlimited ("many")
    int max_stock;        // stock restored toward on partial restock (0 for unlimited-stock items)
    ItemRarity rarity;
    int requires_treasure_hunt; // 1 = only sellable after a cave treasure has ever been found
} MerchantItem;
typedef struct { int id; const char *name; int damage, durability_max; float weight; float stamina_cost; int price; float attack_speed_mult; } WeaponStats;
typedef struct { float x, y; int item_id, quantity, screen_id, active; } GroundLoot;
typedef struct { float x, y, home_x, home_y; int type, idle_timer, direction; const char *dialogue; int dialogue_cooldown; } NPCEntity;
typedef struct { float x, y, hp; int active, timer, raise_frame, attack_cooldown; } ZombieEntity;
typedef struct { int x, y, screen_id, left_frame, active, has_left; } AppleTreeState;

typedef struct {
    int id;
    char name[32];
    char slot[32];
    int is_equipped;
    int quantity;
    float weight;
    int durability_current;
    int durability_max;
    int damage;
} InventoryItem;

// Items dropped on the ground from the inventory menu, as opposed to the
// enum-based GroundLoot spawned by chopping/mining/fishing. Stores a full
// copy of the InventoryItem so arbitrary owned items (weapons, armor, tools)
// can be dropped and later picked back up with their original stats intact.
typedef struct { float x, y; InventoryItem item; int screen_id, active; } DroppedInventoryItem;
extern char player_name[32];
extern RaceType selected_race;
extern ClassType selected_class;
extern float player_height_m;
extern const char* race_names[];
extern const char* class_names[];
extern InventoryItem player_inventory[MAX_INVENTORY_ITEMS];
extern int player_item_count;
extern int selected_inv_index;
extern InventoryItem pickaxe_item;
extern int player_level;
extern int player_xp;
extern int player_next_level_xp;
extern int is_poisoned;
extern char quest_log_text[128];
extern float opt_xp_mult;
extern float opt_loot_mult;
extern float opt_day_night_speed;
extern uint8_t visited_biomes[META_GRID_SIZE][META_GRID_SIZE];
extern float player_x;
extern float player_y;
extern FacingDirection player_facing;
extern WeaponType active_weapon;
extern int bow_equipped; // a Bow equipped as secondary weapon (RT / F fires it)
extern int is_running;
extern float run_bob;
extern float world_tick;
extern int world_frame;
extern int sword_swipe_frame;
extern uint8_t VISUAL_MAP[MAP_SIZE][MAP_SIZE];
extern ArrowProjectile player_arrow;
extern float bow_charge_time;
extern int is_charging_bow;
extern DWORD bow_charge_started_at;
extern ArrowProjectile player_spell;
extern int y_button_was_down;
extern int y_key_was_down;
extern DWORD y_ability_cooldown_until;
extern int ability_visual_frame;
extern DWORD monster_pursuit_suppressed_until;
extern int is_sneaking;
extern ButterflyParticle butterflies[MAX_BUTTERFLIES];
extern DebrisParticle twigs[DEBRIS_MAX];
extern BloodParticle blood_mist[MAX_BLOOD_MIST];
extern CampfireEntity campfires[MAX_CAMPFIRES];
extern CritterEntity critters[MAX_CRITTERS];
extern GroundLoot ground_loot[MAX_GROUND_LOOT];
extern DroppedInventoryItem dropped_items[MAX_DROPPED_ITEMS];
extern NPCEntity village_npcs[MAX_NPCS];
extern int village_npc_count;
extern ZombieEntity summoned_zombie;
extern AppleTreeState apple_tree_states[MAX_APPLE_TREES];
extern int vendor_menu_open;
extern int fishing_active, fishing_power;
extern int bedroll_screen_id;
extern float bedroll_x, bedroll_y;
extern int night_bonus_active;
extern int wind_dir;
extern int sleep_fade_frame;
extern int enemy_direction;
extern float weather_next_change;
extern int is_menu_open;
extern int current_menu_tab;
extern int selected_creation_field;
extern int creation_name_letter_cursor;
extern int gold_count;
extern float player_stamina;
extern float max_stamina;
extern int stamina_rest_timer;
#define STAMINA_REST_FRAMES_REQUIRED 45
extern float player_hp, max_player_hp;
extern float current_payload_weight;
extern float day_night_cycle_accumulator;
extern WeatherType current_weather;
extern int screens_until_town;
extern int current_screen_index;
extern int screen_grid_x, screen_grid_y;
extern int meta_grid_x, meta_grid_y;
extern BiomeType current_biome;
extern int current_settlement_type;
extern int pending_forced_biome;

// Cave System: entering a Mountain crevice enters a cave interior keyed to
// the overworld cell it was entered from. Each cave instance remembers
// whether its treasure has already been claimed so re-entering an explored
// cave never respawns it, and the hero can only leave once the treasure in
// a fresh cave has been found.
#define MAX_CAVE_RECORDS 32
typedef struct { int origin_cell; int valid; int treasure_claimed; } CaveRecord;
extern CaveRecord cave_records[MAX_CAVE_RECORDS];
extern int in_cave;
extern int cave_return_grid_x, cave_return_grid_y;
extern float cave_return_player_x, cave_return_player_y;
extern int current_cave_origin_cell;
extern float cave_treasure_x, cave_treasure_y;
extern int cave_treasure_hunts_found;
extern int saturn_save_screen_index;
extern int saturn_save_steps_remaining;
extern int saturn_save_lumber;
extern int saturn_save_gold;
extern int saturn_save_stone;
extern int torch_gate_x;
extern int torch_gate_y;
extern float enemy_x;
extern float enemy_y;
extern float enemy_hearts;
extern MonsterType active_monster;
extern float gator_x;
extern float gator_y;
extern int gator_active;
extern float gator_swim_timer;
extern float merchant_x;
extern float merchant_y;
extern int merchant_selection;
extern int vendor_tab;

// LOOT_* ids map 1:1 onto weapon_catalog[id - 1] for every entry up through
// LOOT_MUSASHI_BLADE (kept contiguous on purpose - see LootWeight/AddLootToInventory).
enum { LOOT_SHORT_SWORD = 1, LOOT_LONG_SWORD, LOOT_AXE, LOOT_SHORT_BOW, LOOT_RUSTY_SWORD, LOOT_MUSASHI_BLADE, LOOT_FISH, LOOT_APPLE };
extern WeaponStats weapon_catalog[6];
#define WEAPON_CATALOG_COUNT ((int)(sizeof(weapon_catalog) / sizeof(weapon_catalog[0])))
extern MerchantItem merchant_catalog[MERCH_ITEMS];
extern const char *villager_greetings[];
extern const char *villager_questions[];
extern const char *traveler_barks[];
extern const char *rumor_mill[];
extern const char *fort_barks[];
extern int is_character_creation;
extern int stat_strength, stat_dexterity, stat_stamina;
extern int stat_magick, stat_luck, stat_intelligence, stat_charisma;
extern char creation_error_msg[64];
extern char arpg_action_log[256];
extern int cam_x;
extern int cam_y;

// ---- Module APIs ----
#include "main.h"
#include "character.h"
#include "campfire.h"
#include "inventory.h"
#include "vendor.h"
#include "interact.h"
#include "world.h"
#include "combat.h"
#include "render_world.h"
#include "render_entities.h"
#include "ui.h"
#include "physics.h"
#include "input.h"
#include "weather.h"

#endif // GAME_H
