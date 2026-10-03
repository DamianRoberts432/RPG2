#ifndef CAMPFIRE_H
#define CAMPFIRE_H

// Campfire placement, fuel and lighting from the inventory menu

int FindFreeCampfireSlot(void);
void PlaceCampfireAt(int slot);
int AddWoodToNearbyFire(void);
void HandleMenuLightFire(void);

#endif // CAMPFIRE_H
