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
int BirdsFleeTree(int x, int y);
int CritterAsleep(const CritterEntity *critter);
// Village campfires and torches: monsters mostly stay out of their light.
typedef struct { float x, y; int torch; } VillageLight;
#define MAX_VILLAGE_LIGHTS 8
extern VillageLight village_lights[MAX_VILLAGE_LIGHTS];
extern int village_light_count;
void DrawVillageLights(HDC hdc);

int MountainHasCave(int cell);
#endif // WORLD_H
