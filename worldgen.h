#ifndef WORLDGEN_H
#define WORLDGEN_H
// Whole-world generation: every biome, town, village and highway is laid out
// from world_seed when the game starts, so the map is fixed for that world.
enum { SITE_NONE, SITE_VILLAGE, SITE_TOWN, SITE_ELYSIUM, SITE_DWARF };
extern uint32_t world_seed;
extern int elysium_gx, elysium_gy, elysium_theme, village_dwarves;
void GenerateWholeWorld(uint32_t seed);
int WorldBiomeAt(int gx, int gy);
int WorldSiteAt(int gx, int gy);
int WorldIsTown(int gx, int gy);
int WorldRoadBits(int gx, int gy); // 1 north, 2 east, 4 south, 8 west
const char *ElysiumThemeName(void);
void ClearHighwayTiles(void);
void DrawHighway(HDC hdc);
float EconomyBuyMult(const char *name);
float EconomySellMult(const char *name);
void EconomyNoteSold(const char *name, int qty);
int ItemShopPrice(const MerchantItem *o);
#endif
