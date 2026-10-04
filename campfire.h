#ifndef CAMPFIRE_H
#define CAMPFIRE_H

// Campfire placement, fuel and lighting from the inventory menu

int FindFreeCampfireSlot(void);
void PlaceCampfireAt(int slot);
#define CAMPFIRE_START_WOOD 2
#define CAMPFIRE_MAX_WOOD 10
#define CAMPFIRE_FRAMES_PER_WOOD 900.0f
int CampfireWoodCount(int i);
int FindNearbyCampfire(float radius);
void AddWoodToFire(int i);
int AddWoodToNearbyFire(void);
void HandleMenuLightFire(void);

#endif // CAMPFIRE_H
