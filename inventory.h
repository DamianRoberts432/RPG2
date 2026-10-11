#ifndef INVENTORY_H
#define INVENTORY_H

// Inventory, equipment, weight, ground loot and dropped items

void SyncActiveWeapon(void);
void ToggleEquipSelectedItem(void);
void SortInventory(void);
void HandleInventoryPrimaryAction(void);
WeaponStats *GetEquippedWeaponStats(void);
int WearEquippedWeapon(void);
int WearEquippedBow(void);
float GetNamedItemWeight(const char *name);
void RecalculateCarriedWeight(void);
int AddInventoryItem(const char *name, const char *slot, int quantity);
#define MAX_STACK 99
int IsStackableItem(const char *name, const char *slot);
int AddStackableItem(const char *name, const char *slot, int quantity);
float CarryLimit(void);
int FindInventoryItemIndex(const char *name);
int GetMaterialCount(const char *name);
void AddMaterialToInventory(const char *name, int quantity);
int ConsumeMaterialFromInventory(const char *name, int quantity);
float LootWeight(int item_id);
int AddLootToInventory(int item_id, int quantity);
void FindDropSpot(float x, float y, int center_first, float *ox, float *oy);
void DropGroundLoot(float x, float y, int item_id, int quantity);
void TryPickupGroundLoot(void);
void DropSelectedItem(void);
int TryPickupDroppedItems(void);
float WeightSpeedFactor(void);
int IsLegendaryItemName(const char *name);
void ExpireDroppedItems(void);
void ClearLegendaryDrops(void);
int HasInventoryItem(const char *name);
int NightVisionActive(void);
void ConsumeSelectedFood(void);

#endif // INVENTORY_H
