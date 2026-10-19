#include "game.h"

const char *biome_names[] = {
    "Prairie", "Swamp", "Beach", "Mountain", "Desert", "Tundra", "Grasslands",
    "Oasis", "Forest", "Island", "Ocean", "Lake", "River", "Cave", "Castle"
};

// Hero Profile Info
char player_name[32] = "Damian";
RaceType selected_race = RACE_ELF;
ClassType selected_class = CLASS_NECROMANCER;
float player_height_m = 1.82f;

const char* race_names[] = { "Elf", "Human", "Dwarf", "Halfling", "Gnome" };
const char* class_names[] = { "Necromancer", "Wizard", "Knight", "Warrior", "Rogue" };

// Hero Inventory Structure
// Every class starts geared with the Axe equipped; the Iron Sword remains in the
// pack unequipped (owned, but not usable as the active weapon until the player
// manually equips it from the inventory menu).
InventoryItem player_inventory[MAX_INVENTORY_ITEMS] = {
    {1, "Iron Sword", "Weapon", 0, 1, 4, 100, 100, 1},
    {2, "Wooden Shield", "Shield", 0, 1, 3, 0, 0, 0},
    {3, "Leather Armor", "Armor", 0, 1, 5, 0, 0, 0},
    {4, "Matches", "Tool", 0, 5, 1, 0, 0, 0},
    {5, "Axe", "Weapon", 1, 1, 4, 100, 100, 3},
    {6, "Wood", "Material", 0, 12, 1, 0, 0, 0}
};
int player_item_count = 6;
int selected_inv_index = 0;

// System Supported Inventory Items
InventoryItem pickaxe_item = {6, "Pickaxe", "Tool", 0, 1, 0, 0, 0, 0};

// Hero Progression & Attributes
int player_level = 1;
int player_xp = 0;
int player_next_level_xp = 100;
int is_poisoned = 0;
char quest_log_text[128] = "Main Quest: Find a castle town.";

// Game Dynamic Options
float opt_xp_mult = 1.0f;
float opt_loot_mult = 1.0f;
float opt_day_night_speed = 1.0f;

// World Navigation Log
uint8_t visited_biomes[META_GRID_SIZE][META_GRID_SIZE] = {0};

// Live Sandbox Tracking State Vectors
float player_x = 15.0f; float player_y = 15.0f;
FacingDirection player_facing = FACE_DOWN;
WeaponType active_weapon = WEAPON_AXE;
int bow_equipped = 0;

int is_running = 0; float run_bob = 0.0f; float world_tick = 0.0f;
int world_frame = 0;
int sword_swipe_frame = 0; 
uint8_t VISUAL_MAP[MAP_SIZE][MAP_SIZE];
ArrowProjectile player_arrow = {0};
float bow_charge_time = 0.0f;
int is_charging_bow = 0;
DWORD bow_charge_started_at = 0;

// Class-special ("Y" button) ability state. Fires once per button press (edge
// triggered) and is further gated by a per-class cooldown so it cannot spam.
ArrowProjectile thrown_spear = {0};
InventoryItem thrown_spear_item;
ArrowProjectile player_spell = {0}; // Reused projectile shape for Necromancer/Wizard bolts
int y_button_was_down = 0;          // Edge-detection latch for gamepad Y
int y_key_was_down = 0;             // Edge-detection latch for keyboard Y
DWORD y_ability_cooldown_until = 0; // GetTickCount() threshold before next use
int ability_visual_frame = 0;       // Countdown used to draw a brief class-ability flash
DWORD monster_pursuit_suppressed_until = 0; // Rogue sneak / backstep defensive window
int is_sneaking = 0;                // True while Rogue sneak suppression is active

ButterflyParticle butterflies[MAX_BUTTERFLIES] = {0};
DebrisParticle twigs[DEBRIS_MAX] = {0}; 
BloodParticle blood_mist[MAX_BLOOD_MIST] = {0};
CampfireEntity campfires[MAX_CAMPFIRES] = {0};
CritterEntity critters[MAX_CRITTERS] = {0};
GroundLoot ground_loot[MAX_GROUND_LOOT] = {0};
DroppedInventoryItem dropped_items[MAX_DROPPED_ITEMS] = {0};
NPCEntity village_npcs[MAX_NPCS] = {0};
int village_npc_count = 0;
ZombieEntity summoned_zombie = {0};
int next_summon_type = SUMMON_ZOMBIE;
AppleTreeState apple_tree_states[MAX_APPLE_TREES] = {0};
int vendor_menu_open = 0;
int fishing_active = 0, fishing_power = 0;
int bedroll_screen_id = -1;
float bedroll_x = 0.0f, bedroll_y = 0.0f;
int night_bonus_active = 0;
int wind_dir = 0;
int sleep_fade_frame = 0;
int jailed_tonight = 0;
DWORD guide_flash_until = 0;
int enemy_direction = FACE_DOWN;
float weather_next_change = 200.0f;

// Menu / Pause System
int is_menu_open = 0; 
int current_menu_tab = 0;
int world_map_zoom = 1; 
int selected_creation_field = 0;
int creation_name_letter_cursor = 0; // 0-55 = A-Z, a-z, space, period, hyphen, apostrophe

int gold_count = 150;
float player_stamina = 100.0f; float max_stamina = 100.0f;
// Frames (at ~60fps) the hero must stand still before stamina is allowed to
// regenerate back past the 30% "winded" threshold - see UpdateGamePhysics.
int stamina_rest_timer = 0;
float player_hp = 100.0f, max_player_hp = 100.0f;
float player_mp = 100.0f, max_player_mp = 100.0f;
// Recomputed at startup (and after every inventory change) from the actual
// unequipped items carried in player_inventory - see RecalculateCarriedWeight().
float current_payload_weight = 0.0f;

// Environment & Climate Engines
float day_night_cycle_accumulator = 1.5f; 
WeatherType current_weather = WEATHER_CLEAR;

// The world is an 8x8 grid of 8x8 meta-maps. The local coordinates remain
// screen_grid_x/y; meta_grid_x/y select the containing map.
int screens_until_town = 5; int current_screen_index = 1;
int screen_grid_x = META_GRID_SIZE / 2, screen_grid_y = META_GRID_SIZE / 2;
int meta_grid_x = 3, meta_grid_y = 3;
BiomeType current_biome = BIOME_PRAIRIE;
int current_settlement_type = 0;
// When >= 0, the next GenerateProceduralScreen() call uses this biome instead
// of rolling/recalling one (used to force BIOME_CAVE on entry so the cave
// interior is never overwritten by the normal biome picker).
int pending_forced_biome = -1;
CaveRecord cave_records[MAX_CAVE_RECORDS] = {0};
int in_cave = 0;
int cave_return_grid_x = 4, cave_return_grid_y = 4;
float cave_return_player_x = 15.0f, cave_return_player_y = 15.0f;
int current_cave_origin_cell = -1;
float cave_treasure_x = 20.0f, cave_treasure_y = 20.0f;
int cave_treasure_hunts_found = 0; // how many cave treasures the player has ever found (unlocks rare merchant stock)

// Save Storage Fallbacks
int saturn_save_screen_index = 1; int saturn_save_steps_remaining = 5;
int saturn_save_lumber = 12; int saturn_save_gold = 150; int saturn_save_stone = 0;

// Interactive Torch Coordinates
int torch_gate_x = 12; int torch_gate_y = 12;

// Living Entity Management
float enemy_x = 27.0f; float enemy_y = 27.0f; float enemy_hearts = 3.0f;
MonsterType active_monster = MONSTER_SKELLY;
// Up to MAX_ENEMIES monsters share a screen. enemy_x/enemy_y/enemy_hearts/
// active_monster/enemy_direction are the working copy of enemies[current_enemy].
EnemySlot enemies[MAX_ENEMIES];
int current_enemy = 0;
float gator_x = 0.0f; float gator_y = 0.0f; int gator_active = 0; float gator_swim_timer = 0.0f;

// Storefront Objects
float merchant_x = 10.0f; float merchant_y = 18.0f;
int merchant_selection = 0;
int vendor_tab = 0; // 0 = Buy (merchant's items), 1 = Sell (player's inventory)
// Weight values follow the player's assigned reference scale: Wood 1, Stone 1,
// Matches 1, Dagger 1, Short Bow 1.5, Fishing Pole 2, Short Sword 2, Long Bow 2,
// Axe 4, Iron Sword 4, Armor 5, Shield 3 (see GetNamedItemWeight).
WeaponStats weapon_catalog[] = {
    { LOOT_SHORT_SWORD, "Short Sword", 1, 100, 2.0f, 0.0f, 20, 1.0f },
    { LOOT_LONG_SWORD, "Long Sword", 2, 100, 5.0f, 1.5f, 35, 0.75f },
    { LOOT_AXE, "Axe", 3, 100, 4.0f, 2.0f, 30, 0.75f },
    { LOOT_SHORT_BOW, "Short Bow", 1, 100, 1.5f, 0.0f, 25, 1.0f },
    { LOOT_RUSTY_SWORD, "Rusty Sword", 1, 25, 2.0f, 0.0f, 8, 1.0f },
    // Legendary: only ever dropped by bosses and mythic beasts (see
    // HandleEnemyDamage). Durability is set absurdly high
    // to stand in for "infinite", and it is specifically excluded from
    // selling/dropping (see HandleVendorSell/DropSelectedItem).
    { LOOT_MUSASHI_BLADE, "Musashi's Blade", 10, 99999, 1.0f, 1.0f, 5000, 1.0f },
    // Thrust reaches further than a sword; RT / F throws it (see ThrowSpear).
    { LOOT_SPEAR, "Spear", 2, 100, 3.0f, 1.0f, 30, 0.85f },
    { LOOT_LONG_BOW, "Long Bow", 2, 120, 2.0f, 0.0f, 40, 1.0f }, // arrows fly harder (see FirePlayerArrow)
    { LOOT_MACE, "Mace", 3, 150, 5.0f, 1.5f, 35, 0.8f },
    { LOOT_DAGGER, "Dagger", 1, 60, 1.0f, 0.0f, 10, 1.0f },
    // Rare smith work, sold only by the blacksmith (see town.c).
    { LOOT_KATANA, "Katana", 5, 200, 2.5f, 0.5f, 12000, 1.0f },
    { LOOT_WAR_HAMMER, "War Hammer", 6, 250, 7.0f, 2.5f, 15000, 0.7f },
    { LOOT_GREAT_AXE, "Great Axe", 7, 220, 8.0f, 3.0f, 18000, 0.65f }
};

// Catalog spans every common weapon/armor/tool/food the
// game knows about plus scarce Rare/Legendary entries. Price shown is always
// base_price * rarity (see DrawVendorShopOverlay/HandleVendorBuy). Rare/
// Legendary stock is intentionally tiny (1-3 / 1) and only partially restocks
// after each day/night cycle (see RestockMerchant).
int merch_item_count = MERCH_BASE_ITEMS;
MerchantItem merchant_catalog[MERCH_CAPACITY] = {
    { "Short Sword",   "Weapon",   LOOT_SHORT_SWORD,  20, 1,  1, RARITY_COMMON, 0 },
    { "Axe",           "Weapon",   LOOT_AXE,          25, 1,  1, RARITY_COMMON, 0 },
    { "Iron Sword",    "Weapon",   0,                 22, 1,  1, RARITY_COMMON, 0 },
    { "Rusty Sword",   "Weapon",   LOOT_RUSTY_SWORD,   8, 2,  2, RARITY_COMMON, 0 },
    { "Wooden Shield", "Shield",   0,                 12, 1,  1, RARITY_COMMON, 0 },
    { "Leather Armor", "Armor",   0,                 18, 1,  1, RARITY_COMMON, 0 },
    { "Pickaxe",       "Tool",     0,                 10, 1,  1, RARITY_COMMON, 0 },
    { "Fishing Pole",  "Tool",     0,                 15, 1,  1, RARITY_COMMON, 0 },
    { "Matches",       "Tool",     0,                  2, -1, 0, RARITY_COMMON, 0 },
    { "Wood",          "Material", 0,                  1, -1, 0, RARITY_COMMON, 0 },
    { "Fish",          "Food",     0,                  5, -1, 0, RARITY_COMMON, 0 },
    { "Apple",         "Food",     0,                  3, -1, 0, RARITY_COMMON, 0 },
    { "Spear",         "Weapon",   LOOT_SPEAR,        30, 2,  2, RARITY_COMMON, 0 },
    { "Dagger",        "Weapon",   LOOT_DAGGER,       10, 2,  2, RARITY_COMMON, 0 },
    { "Mace",          "Weapon",   LOOT_MACE,        600, 1,  1, RARITY_UNCOMMON, 0 },
    { "Long Bow",      "Weapon",   LOOT_LONG_BOW,    750, 1,  1, RARITY_UNCOMMON, 0 },
    { "Night Vision Ring", "Ring", 0,               3000, 1,  1, RARITY_RARE, 0 },
    { "Raft",          "Tool",     0,                 30, 1,  1, RARITY_COMMON, 0 },
    { "Canoe",         "Tool",     0,               1000, 1,  1, RARITY_UNCOMMON, 0 },
    { "Boat",          "Tool",     0,               2400, 1,  1, RARITY_RARE, 0 },
    { "Long Sword",    "Weapon",   LOOT_LONG_SWORD,  700, 2,  2, RARITY_UNCOMMON, 0 },
    { "Short Bow",     "Weapon",   LOOT_SHORT_BOW,   500, 2,  2, RARITY_UNCOMMON, 0 },
    { "Dragon Scale Armor", "Armor", 0,              5000, 1,  1, RARITY_RARE, 0 },
    // Altar offerings (see town.c).
    { "Incense",     "Material", 0, 30,  -1, 0, RARITY_COMMON, 0 },
    { "Tarot Card",  "Material", 0, 40,  -1, 0, RARITY_COMMON, 0 },
    { "Rum Bottle",  "Material", 0, 25,  -1, 0, RARITY_COMMON, 0 },
    { "Cigar",       "Material", 0, 15,  -1, 0, RARITY_COMMON, 0 },
    { "Chaos Relic", "Material", 0, 600,  2, 2, RARITY_UNCOMMON, 0 },
    // Dracula wards (see TriggerMonsterRespawn and PerformMeleeAttack).
    { "Garlic",        "Material", 0,                  8, -1, 0, RARITY_COMMON, 0 },
    { "Silver Dagger", "Weapon",   0,                900, 1,  1, RARITY_UNCOMMON, 0 },
    // Farming (see farming.c).
    { "Hoe",           "Tool",     0,                 12, 1,  1, RARITY_COMMON, 0 },
    { "Seeds",         "Material", 0,                  2, -1, 0, RARITY_COMMON, 0 },
    { "Turnip",        "Food",     0,                  4, -1, 0, RARITY_COMMON, 0 },
    // Practical clothes any class can wear (see ClothingColor in inventory.c).
    { "Cloth Hood",      "Head",    0, 12, 2, 2, RARITY_COMMON, 0 },
    { "Straw Hat",       "Head",    0,  6, 2, 2, RARITY_COMMON, 0 },
    { "Wool Cap",        "Head",    0,  5, 2, 2, RARITY_COMMON, 0 },
    { "Leather Cap",     "Head",    0, 10, 2, 2, RARITY_COMMON, 0 },
    { "Feathered Hat",   "Head",    0, 40, 1, 1, RARITY_UNCOMMON, 0 },
    { "Travel Cloak",    "Cloak",   0, 20, 2, 2, RARITY_COMMON, 0 },
    { "Wool Cloak",      "Cloak",   0, 25, 2, 2, RARITY_COMMON, 0 },
    { "Black Cloak",     "Cloak",   0, 35, 1, 1, RARITY_COMMON, 0 },
    { "Fur Mantle",      "Cloak",   0, 60, 1, 1, RARITY_UNCOMMON, 0 },
    { "Linen Shirt",     "Clothes", 0,  6, 2, 2, RARITY_COMMON, 0 },
    { "Work Smock",      "Clothes", 0,  8, 2, 2, RARITY_COMMON, 0 },
    { "Leather Jerkin",  "Clothes", 0, 22, 2, 2, RARITY_COMMON, 0 },
    { "Padded Gambeson", "Clothes", 0, 45, 1, 1, RARITY_UNCOMMON, 0 }
    // Legendary items are never sold; they only drop from bosses (see HandleEnemyDamage).
};
const char *villager_greetings[] = {
    "Good day, traveler!", "The road treats you kindly?", "Welcome to our village.",
    "Lovely weather for a walk.", "May your pack stay light.", "A fine day to be abroad.",
    "Peace to you, friend.", "You look well-traveled.", "Rest your feet awhile.",
    "Safe travels, hero!", "The flowers are out today.", "Good fortune on your journey."
};
const char *villager_questions[] = {
    "Have you seen the river lately?", "Where does your road lead?",
    "Have you heard the market news?", "Is the weather fair beyond here?",
    "What brings you to our village?", "Have you found any good trails?",
    "Do you travel alone?", "Which way did you come from?",
    "Have the forest paths been quiet?", "Will you stay for a while?"
};
const char *traveler_barks[] = {
    "Fine wares from distant roads.", "Keep your purse close.",
    "I've walked a hundred leagues.", "Fresh supplies for the road!",
    "The northern trail is rough.", "Trade is good beyond the hills.",
    "A sturdy pack saves the day.", "The old roads still have secrets."
};
const char *rumor_mill[] = {
    "I hear tell of a beast in the deep forests... scales like obsidian, eyes like burning coal.",
    "Word has it there's a great castle to the northeast, past the mountains. But I've never seen it.",
    "The merchants speak of a dragon's hoard hidden beyond the tundra. None have returned. I have my doubts.",
    "An old castle stands where the rivers meet. Some say it's cursed, others say it's treasure.",
    "They say a Necromancer built a tower in the swamps... dark magicks bind the very earth.",
    "Travelers whisper of a royal keep somewhere beyond the western grasslands."
};
const char *fort_barks[] = {
    "Keep your eyes on the tree line.", "We watch this road day and night.",
    "The pass is clear for now.", "Rest here, traveler. The watch is strong.",
    "A merchant wagon passed through at dawn.", "No one crosses the camp without a greeting."
};
// Little truths about how the world works, shared now and then by villagers.
const char *world_hints[WORLD_HINT_COUNT] = {
    "Every second you spend here is a minute to us. A whole day passes in 24 of yours.",
    "Each season lasts 30 days. Count them if you like.",
    "Weather never jumps from fair to rain. The clouds always roll in first.",
    "When clouds cover the sky, the sun and moon can't throw their beams down.",
    "Birds and squirrels sleep from 9 at night until 6. The owls keep watch then.",
    "Animals bolt from anyone close, unless your soul is pure and luck is on your side.",
    "No foe carries more than 75 gold, though purses grow with your level and luck.",
    "Two wood and a match from your pack will light a campfire.",
    "Charm the merchant: every point of charisma takes 3 percent off his prices.",
    "Carry a raft, canoe or boat and no river can stop you."
};

// Character Creation & Stats
int is_character_creation = 1;
int stat_strength = 2, stat_dexterity = 2, stat_stamina = 2;
int stat_magick = 2, stat_luck = 2, stat_intelligence = 2, stat_charisma = 2;
// Level-up points: stat_spent[] is what the player bought on top of the class
// base, so it survives re-creating the hero and is what the bonuses scale with.
int stat_points = 0, stat_spent[STAT_COUNT] = { 0 }, stat_cursor = 0, stat_assign_mode = 0;
char creation_error_msg[64] = "";
KarmaState karma = { 0 };
uint8_t card_collection_bits[CARD_BYTES] = { 0 }, deck_of_cards_bits[CARD_BYTES] = { 0 };
MonsterCardLink monster_card_links[MONSTER_TYPE_COUNT] = {
    {0, 0, 0}, {1, 1, 0}, {2, 2, 0}, {3, 3, 0}, {4, 4, 0}, {5, 5, 0}, {6, 6, 0}, {7, 7, 0}, {8, 8, 0}
};

char arpg_action_log[256] = "ARPG ENGINE: Character Creation Online. Config Name/Class/Race.";
int cam_x = 0; int cam_y = 0;
