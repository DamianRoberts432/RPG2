#ifndef TOWN_H
#define TOWN_H

// Town buildings: blacksmith, card shop, pet shop, castle (king and bank),
// beggars and altars. Talking to a keeper opens a Zelda 2-style interior
// with a menu picked with up/down.

enum { TM_NONE, TM_SMITH, TM_CARDS, TM_PETS, TM_KING, TM_BANK, TM_BEGGAR, TM_ALTAR, TM_CHEST };

// Craftable storage chests (4 Wood) left in the world; saved with the hero.
#define MAX_CHESTS 8
#define CHEST_SLOTS 16
typedef struct { int32_t active, screen_id, last_day, count; float x, y; InventoryItem items[CHEST_SLOTS]; } StashChest;
extern StashChest chests[MAX_CHESTS];
extern int open_chest;
void BuildChest(void);
int OpenNearbyChest(void);
void CheckChestBreakIns(void);
void DrawChests(HDC hdc);
#define LEGENDARY_QUEST_LEVEL 25 // to be raised to 50 later
#define FAMILIAR_UNLOCK_LEVEL 3
#define MAX_FORGE_BONUS 10

extern int town_menu, town_cursor;
extern int bank_gold, legendary_quest, legendary_quests_done;
extern int deity_favor[4];

void OpenTownMenu(int menu);
void TownMenuMove(int step);
void TownMenuConfirm(void);
void TownMenuBack(void);
void DrawInteriorBackdrop(HDC hdc, int menu);
void DrawTownMenu(HDC hdc);
void DrawFamiliar(HDC hdc);
void DrawAltar(HDC hdc, int sx, int sy, int deity);
void DrawCrown(HDC hdc, int cx, int top, int w);
float TownDamageMult(void);
int EquippedForgeBonus(void);
void NoteLegendaryKill(int monster);
int TownDeity(void);
int TownDeityAt(int screen);
const char *DeityName(int d);
const char *DeityKind(int d);
void TownNameAt(int screen, char *out, size_t n);
int TownShopsToday(int screen);
void CardName(int id, char *out, size_t n);

#endif
