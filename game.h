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
#define MAX_CRITTERS 6
#define MERCH_BASE_ITEMS 53
#define MERCH_CAPACITY 72 // room for NewItem lines from the Update folder
#define MERCH_ITEMS merch_item_count
extern int merch_item_count;
#define MAX_BLOOD_MIST 48
#define MAX_CAMPFIRES 8
#define MAX_INVENTORY_ITEMS 100
#define MAX_GROUND_LOOT 32
#define MAX_DROPPED_ITEMS 32
#define MAX_NPCS 8
#define MAX_APPLE_TREES 8

// Class-special "Y" ability tuning (cooldowns, pursuit-suppression windows,
// and GDI flash durations). Defined up front so they can be referenced by
// both FireClassAbility() and the rendering helpers (e.g. DrawClassAbilityFX)
// that run earlier in the file. Suppress windows are intentionally shorter
// than their matching cooldowns so pursuit suppression always lapses before
// the ability can be used again.
#define FACING_CONE_DOT_THRESHOLD 0.25f
#define MELEE_SWING_STAMINA 5.0f // every swing tires the hero; heavy weapons cost more
#define SPELL_COOLDOWN_MS     1500
#define BACKSTEP_COOLDOWN_MS  2000
#define SNEAK_COOLDOWN_MS     5000
#define BACKSTEP_SUPPRESS_MS  1200
#define SNEAK_SUPPRESS_MS     4000
#define SPELL_FLASH_FRAMES    10
#define ABILITY_FLASH_FRAMES  14

typedef enum { FACE_DOWN, FACE_UP, FACE_LEFT, FACE_RIGHT } FacingDirection;
typedef enum { WEAPON_SWORD, WEAPON_BOW, WEAPON_AXE, WEAPON_SPEAR } WeaponType;
typedef enum { MONSTER_SKELLY, MONSTER_ZOMBIE, MONSTER_DRACULA, MONSTER_GOBLIN, MONSTER_SATYR,
               MONSTER_YETI, MONSTER_SPHINX, MONSTER_BIGFOOT, MONSTER_BUNYIP } MonsterType;
// Mythic beasts (Yeti..Bunyip) and Dracula are the bosses: the only source of Legendary loot.
#define IS_MYTHIC_MONSTER(m) ((m) >= MONSTER_YETI)
#define IS_BOSS_MONSTER(m) (IS_MYTHIC_MONSTER(m) || (m) == MONSTER_DRACULA)
const char *MonsterName(MonsterType m);
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
typedef enum { WEATHER_CLEAR, WEATHER_RAIN, WEATHER_SNOW, WEATHER_CLOUDY } WeatherType;

typedef enum { RACE_ELF, RACE_HUMAN, RACE_DWARF, RACE_HALFLING, RACE_GNOME } RaceType;
typedef enum { CLASS_NECROMANCER, CLASS_WIZARD, CLASS_KNIGHT, CLASS_WARRIOR, CLASS_ROGUE } ClassType;

typedef struct { float x, y, z; float vx, vy, vz; int active; float damage; int origin_tx, origin_ty; } ArrowProjectile;
typedef struct { float x, y, z; float timer; int active; int color_profile; } ButterflyParticle;
typedef struct { float x, y; float vx, vy; int life; int screen_id; int active; } DebrisParticle;
typedef struct { float x, y; float vx, vy; int life; int max_life; int active; uint32_t color; } BloodParticle;
typedef struct { float x, y; float timer; int active; int screen_id; } CampfireEntity;
typedef struct { float x, y; int type; int state; float base_x, base_y; int active; int hidden; int timer; } CritterEntity;
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
typedef enum { SUMMON_ZOMBIE, SUMMON_SKELETON, SUMMON_GHOST } SummonType;
typedef struct { float x, y, hp; int active, timer, raise_frame, attack_cooldown, type; } ZombieEntity;
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
// Ordinary drops vanish DROPPED_ITEM_LIFETIME_MS after landing; Legendary
// drops never time out but are cleared when the game is saved on exit.
#define DROPPED_ITEM_LIFETIME_MS (5 * 60 * 1000)
typedef struct { float x, y; InventoryItem item; int screen_id, active; DWORD dropped_at; int legendary; } DroppedInventoryItem;
extern char player_name[32];
extern RaceType selected_race;
// Looks picked on the start screen only: race sets the height.
#define CREATION_FIELDS 9
#define EYE_COLORS 5
#define HAIR_COLORS 6
extern int hero_sex, hero_beard, hero_eyes, hero_hair, hero_cloth, hero_trim;
#define CLOTH_COLORS 8
#define TRIM_COLORS 8
extern const char *cloth_names[], *trim_names[];
COLORREF HeroClothColor(COLORREF class_colour);
COLORREF HeroTrimColor(void); // CLOTH_UNKNOWN when no trim
int BarrelInfo(int i, float *x, float *y, int *intact);
enum { NPC_IDLE, NPC_SWEEP, NPC_FARM, NPC_WATER, NPC_NATURE, NPC_HAMMER, NPC_INSPECT };
int NpcActivity(int i);
int NpcAtWork(int i);
void NoteBarrelBroken(float x, float y);
int IsBigTree(int tile_seed);
void DrawBigCrown(HDC hdc, int sx, int sy, int tile_seed);
extern const char *sex_names[], *beard_names[], *eye_names[], *hair_names[];
void CycleCreationField(int field, int dir);
#define MENU_TAB_CARDS 4
#define MENU_TAB_QUESTS 3
extern int card_guide_unlocked;
extern int familiar_size, familiar_color;
int CardGuideUnlocked(void);
void CardInspectorMove(int d);
void DrawCardInspector(HDC hdc, int x, int y, int w, int h);
int MenuTabStep(int cur, int dir);
extern float facing_x, facing_y;
void SetHeroFacing8(float dx, float dy);
void DrawSlash(HDC hdc, int sx, int sy);
void AdvMapDot(HDC hdc, int town_key, int cx, int cy, int cs);
void AskForWork(int npc_type);
int AdvAskForWork(int npc_type);
int AdvOfferPending(void);
void OpenVillagerTalk(int i);
int VillagerOfferPending(void);
extern int villager_job_ask;
int HeroExhausted(void);
void DrawPanel(HDC hdc, int l, int t, int r, int b);
void MsgLogUpdate(void);
void DrawMsgLog(HDC hdc, int x, int y);
void DrawMsgHistory(HDC hdc, int x, int y, int w, int lines);
extern int werewolf_active;
extern int bounty_hunter_active, cave_room_claimed;
int IsOutlaw(void);
int OutlawGuardTalk(int npc_type);
void DrawBountyHunter(HDC hdc, int sx, int sy, int face);
void WriteLegacy(void);
void ApplyLegacy(void);
int CaveRoom(int cell);
int CaveRoomClaimed(int cell);
void ClaimCaveRoom(int cell);
float AdvTownPriceMult(void);
int AdvTalk(int npc_type);
void AdvNote(char kind, int monster);
void AdvTick(void);
int AdvSave(FILE *f);
int AdvLoad(FILE *f);
void DrawQuestJournal(HDC hdc, int x, int y, int w, int h);
void DrawTrackedQuest(HDC hdc, int x, int y);
void AdvJournalInput(int up, int down, int left, int right, int track, int abandon);
// Clothes: "Head", "Cloak" and "Clothes" slots, wearable by every class.
const char *WornItem(const char *slot);
int IsClothingSlot(const char *slot);
COLORREF ClothingColor(const char *name);
#define CLOTH_UNKNOWN RGB(1, 2, 3)
// World memory per screen (worldstate.c).
#define MAX_SCREEN_EDITS 768
#define MAX_SCREEN_MEMORY 128
#define MAX_MELT_PATCHES 32
typedef struct { int32_t screen_id, minute; uint8_t x, y, tile, active; } ScreenEdit;
typedef struct { int32_t screen_id, left_minute, alive, active; } ScreenMemory;
typedef struct { int32_t screen_id, active; float x, y, fill; } MeltPatch;
extern ScreenEdit screen_edits[MAX_SCREEN_EDITS];
extern ScreenMemory screen_memory[MAX_SCREEN_MEMORY];
extern MeltPatch melt_patches[MAX_MELT_PATCHES];
void RememberTileCleared(int x, int y, int tile);
void RememberLeavingScreen(int new_index);
void ApplyScreenMemory(int index);
int ScreenStillQuiet(void);
void TickMeltPatches(float dt);
int GetMeltPatch(int i, float *x, float *y, float *size);
// Walk-in homes and polygon shop rooms (afkview.c).
extern int inside_home;
int HomeInteract(void);
void LeaveHome(void);
int InteriorBlocks(float x, float y);
void DrawHomeInterior(HDC hdc);
void DrawShopRoom(HDC hdc, int stone, int shrine, int floor_y);
// The player's own house (home.c).
#define HOME_ROOFS 4
#define HOME_WALLS 3
#define HOME_FRONTS (HOME_ROOFS * HOME_WALLS)
#define FURNITURE_KINDS 6
extern int32_t player_home[8];
extern const char *furniture_names[FURNITURE_KINDS];
int PlayerHomeCentre(float *x, float *y);
COLORREF HomeRoofColor(int front);
COLORREF HomeWallColor(int front);
int HomeBuildInteract(void);
void UpdatePlayerHome(void);
int PlaceFurniture(void);
#define DECOR_MAX 22 // home decorations use bits FURNITURE_KINDS.. of player_home[5]
int DecorCount(void);
const char *DecorName(int k);
int DecorLight(int k);
int HomeHasDecor(int k);
int AddRuntimeDecor(const char *name, int light, int comfort);
int HomeComfort(void);
int MerchSeasonLocked(const MerchantItem *o);
#define DECK_MAX 60
int InDeck(int id);
int DeckCount(void);
void GainCard(int id);
void FillEmptyDeck(void);
int MountainIsPeak(int cell);
COLORREF HeroEyeColor(void);
COLORREF HeroHairColor(void);
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
extern uint8_t map_poi[META_GRID_SIZE][META_GRID_SIZE];
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
extern ArrowProjectile thrown_spear;
extern InventoryItem thrown_spear_item;
extern int y_button_was_down;
extern int y_key_was_down;
extern DWORD y_ability_cooldown_until;
extern int ability_visual_frame;
extern DWORD monster_pursuit_suppressed_until;
extern int is_sneaking;
extern ButterflyParticle butterflies[MAX_BUTTERFLIES];
extern DebrisParticle twigs[DEBRIS_MAX];
extern BloodParticle blood_mist[MAX_BLOOD_MIST];
extern uint32_t hit_splash_color; // tint for the next hit's splash (class abilities); 0 = plain blood
extern CampfireEntity campfires[MAX_CAMPFIRES];
extern CritterEntity critters[MAX_CRITTERS];
extern GroundLoot ground_loot[MAX_GROUND_LOOT];
extern DroppedInventoryItem dropped_items[MAX_DROPPED_ITEMS];
extern NPCEntity village_npcs[MAX_NPCS];
extern int village_npc_count;
extern ZombieEntity summoned_zombie;
extern int next_summon_type;
extern AppleTreeState apple_tree_states[MAX_APPLE_TREES];
extern int vendor_menu_open;
extern int fishing_active, fishing_power;
extern int bedroll_screen_id;
extern float bedroll_x, bedroll_y;
extern int night_bonus_active;
extern int wind_dir;
extern int sleep_fade_frame;
extern int jailed_tonight;
#define GUIDE_FLASH_MS 2000
extern DWORD guide_flash_until;
extern int enemy_direction;
extern float weather_next_change;
extern int is_menu_open;
extern int current_menu_tab;
extern int world_map_zoom; // 1 = whole world; 2/4/8 = closer view around the hero
extern int selected_creation_field;
extern int creation_name_letter_cursor;
extern int gold_count;
extern float player_stamina;
extern float max_stamina;
extern float player_mp, max_player_mp;
#define CLASS_ABILITY_MP_COST (20.0f - (float)stat_spent[5]) // each INT point spent saves 1 MP
#define STAT_COUNT 7            // STR DEX STA MAG LCK INT CHR (Hero tab order)
#define STAT_CAP 10             // one jewel row
#define STAT_POINTS_PER_LEVEL 1
extern int stat_points, stat_spent[STAT_COUNT], stat_cursor, stat_assign_mode;
int *StatByIndex(int i);
void SpendStatPoint(void);
int ShopPrice(int base);
// 6-byte karma block: soul alignment, a life mark per boss type slain,
// miasma (dark magic used), town heat (crimes) and grapevine (fame).
#pragma pack(push, 1)
typedef struct { int8_t soul; uint16_t life_marks; uint8_t miasma, town_heat, grapevine; } KarmaState;
#pragma pack(pop)
extern KarmaState karma;
void KarmaAdjust(int soul, int miasma, int heat, int grapevine);
// Card collection: two 21-byte bitmasks (168 bits) for 166 cards. Card ids
// below MONSTER_TYPE_COUNT are monster cards, linked to the bestiary.
#define CARD_COUNT 166
#define CARD_PRINTED 37 // cards in print: Wild Creatures, Angels, Goetia Demons
#define CARD_POOL card_pool_size // CARD_PRINTED plus any NewCard lines from the Update folder
extern int card_pool_size;
#define CARD_BYTES 21
#define CARD_DROP_PERCENT 2
#define MONSTER_TYPE_COUNT 9
typedef struct { uint8_t monster, card_id; uint16_t kills; } MonsterCardLink;
extern uint8_t card_collection_bits[CARD_BYTES], deck_of_cards_bits[CARD_BYTES];
extern MonsterCardLink monster_card_links[MONSTER_TYPE_COUNT];
int HasCard(int id);
int CardCount(void);
void TryCardDrop(int monster); // monster < 0 for chests and fishing
// Upper-left HUD: date/season/region lines start here; the action and
// NPC dialogue log sits below them at HUD_LOG_Y.
#define HUD_DATE_Y 16
#define HUD_LOG_Y 100
extern int stamina_rest_timer;
#define STAMINA_REST_FRAMES_REQUIRED 30
extern float player_hp, max_player_hp;
extern DWORD hero_held_until;
extern int hero_hop_frame;
void HurtHero(float dmg);
void EnemyAttackHero(int e);
float RaceVisionTiles(void);
void DrawNightDarkness(HDC hdc);
void DrawBlessingFx(HDC hdc);
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
#define MAX_ENEMIES 6           // night cap
#define MAX_ENEMIES_DAY 4
#define ENEMY_SPAWN_INTERVAL_MS 20000
typedef struct { float x, y, hearts; MonsterType type; int direction; int angry; int poison; } EnemySlot;
extern EnemySlot enemies[MAX_ENEMIES];
extern int current_enemy;
void SelectEnemy(int i);
void StoreEnemy(void);
int EnemyAtPoint(float x, float y, float radius);
int SelectNearestEnemyTo(float x, float y);
int SelectTargetEnemy(void);
void SpawnScreenEnemies(void);
void UpdateEnemySpawns(void);
extern float gator_x;
extern float gator_y;
extern int gator_active;
extern float gator_swim_timer;
extern float merchant_x;
extern float merchant_y;
extern int merchant_selection;
extern int vendor_tab;

// LOOT_* ids map 1:1 onto weapon_catalog[id - 1] for every entry up through
// LOOT_SPEAR (kept contiguous on purpose - see LootWeight/AddLootToInventory).
enum { LOOT_SHORT_SWORD = 1, LOOT_LONG_SWORD, LOOT_AXE, LOOT_SHORT_BOW, LOOT_RUSTY_SWORD, LOOT_MUSASHI_BLADE, LOOT_SPEAR, LOOT_LONG_BOW, LOOT_MACE, LOOT_DAGGER, LOOT_KATANA, LOOT_WAR_HAMMER, LOOT_GREAT_AXE, LOOT_FISH, LOOT_APPLE, LOOT_GOLD };
#define LOOT_LAST_WEAPON LOOT_GREAT_AXE
extern WeaponStats weapon_catalog[13];
#define WEAPON_CATALOG_COUNT ((int)(sizeof(weapon_catalog) / sizeof(weapon_catalog[0])))
extern MerchantItem merchant_catalog[MERCH_CAPACITY];
extern const char *villager_greetings[];
extern const char *villager_questions[];
extern const char *traveler_barks[];
extern const char *rumor_mill[];
extern const char *fort_barks[];
#define WORLD_HINT_COUNT 10
extern const char *world_hints[WORLD_HINT_COUNT];
extern int is_character_creation;
extern int stat_strength, stat_dexterity, stat_stamina;
extern int stat_magick, stat_luck, stat_intelligence, stat_charisma;
extern char creation_error_msg[64];
extern char arpg_action_log[256];
extern int cam_x;
extern int cam_y;

// ---- Module APIs ----
#define GAME_VERSION "1.0"
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
#include "worldmap.h"
#include "weather.h"
#include "town.h"
#include "fishing.h"
#include "mouse.h"
#include "farming.h"
#include "barrels.h"
#include "peyote.h"
#include "afkview.h"
#include "quests.h"
#include "update.h"

#endif // GAME_H
