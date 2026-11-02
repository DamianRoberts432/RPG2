#ifndef UPDATE_H
#define UPDATE_H

// Update folder: text files of EditItem / EditNPC / EditQuest lines change the
// game at launch without recompiling (see update.c).
void IngestUpdates(void);
float UpdatedItemWeight(const char *name);
const char *CustomVillagerLine(void);
int UpdatedFoodHeal(const char *name);
int CustomSpawnPick(int biome, int is_night);
const char *ApplyBiomeVariant(int biome);
void ApplySetPieces(int biome);
int RecipeCount(void);
void RecipeLabel(int i, char *out, size_t n);
int CraftRecipe(int i, char *msg, size_t n);

#endif
