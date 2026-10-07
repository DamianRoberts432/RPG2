#ifndef WORLD_H
#define WORLD_H

// Biome selection, procedural screen generation, caves, villages, NPC/critter updates

void GenerateVillageNPCs(void);
CaveRecord *FindOrCreateCaveRecord(int origin_cell);
void EnterCave(void);
void ClaimCaveTreasureAndExit(void);
int GetVisitedBiomeAt(int gx, int gy);
BiomeType PickBiomeForScreen(int gx, int gy);
void GenerateProceduralScreen(int index);
void UpdateVillageNPCs(void);
void UpdateCritters(void);

int MountainHasCave(int cell);
#endif // WORLD_H
