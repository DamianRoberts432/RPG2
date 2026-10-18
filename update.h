#ifndef UPDATE_H
#define UPDATE_H

// Update folder: text files of EditItem / EditNPC / EditQuest lines change the
// game at launch without recompiling (see update.c).
void IngestUpdates(void);
float UpdatedItemWeight(const char *name);
const char *CustomVillagerLine(void);
int UpdatedFoodHeal(const char *name);

#endif
