#ifndef INVENTORY_H
#define INVENTORY_H
#include "fixed.h"

// Inventory, equipment, weight, ground loot and dropped items

void SyncActiveWeapon(void);
void ToggleEquipSelectedItem(void);
void SortInventory(void);
void HandleInventoryPrimaryAction(void);
WeaponStats *GetEquippedWeaponStats(void);
int WearEquippedWeapon(void);
int WearEquippedBow(void);
fx GetNamedItemWeight(const char *name);
void RecalculateCarriedWeight(void);
int AddInventoryItem(const char *name, const char *slot, int quantity);
#define MAX_STACK 99
int IsStackableItem(const char *name, const char *slot);
int AddStackableItem(const char *name, const char *slot, int quantity);
fx CarryLimit(void);
int FindInventoryItemIndex(const char *name);
int GetMaterialCount(const char *name);
void AddMaterialToInventory(const char *name, int quantity);
int ConsumeMaterialFromInventory(const char *name, int quantity);
fx LootWeight(int item_id);
int AddLootToInventory(int item_id, int quantity);
void FindDropSpot(fx x, fx y, int center_first, fx *ox, fx *oy);
void DropGroundLoot(fx x, fx y, int item_id, int quantity);
int TryPickupGroundLoot(void);
void DropSelectedItem(void);
int TryPickupDroppedItems(void);
fx WeightSpeedFactor(void);
int IsLegendaryItemName(const char *name);
void ExpireDroppedItems(void);
void ClearLegendaryDrops(void);
int HasInventoryItem(const char *name);
int NightVisionActive(void);
int SilverWeaponEquipped(void);
void ConsumeSelectedFood(void);

#endif // INVENTORY_H
