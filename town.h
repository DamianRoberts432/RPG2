#ifndef TOWN_H
#define TOWN_H

// Town buildings: blacksmith, card shop, pet shop, castle (king and bank),
// beggars and altars. Talking to a keeper opens a Zelda 2-style interior
// with a menu picked with up/down.

enum { TM_NONE, TM_SMITH, TM_CARDS, TM_PETS, TM_KING, TM_BANK, TM_BEGGAR, TM_ALTAR };
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
void CardName(int id, char *out, size_t n);

#endif
