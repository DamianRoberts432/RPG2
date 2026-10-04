#ifndef INVENTORY_H
#define INVENTORY_H

// Inventory, equipment, weight, ground loot and dropped items

void SyncActiveWeapon(void);
void ToggleEquipSelectedItem(void);
void HandleInventoryPrimaryAction(void);
WeaponStats *GetEquippedWeaponStats(void);
int WearEquippedWeapon(void);
int WearEquippedBow(void);
float GetNamedItemWeight(const char *name);
void RecalculateCarriedWeight(void);
int AddInventoryItem(const char *name, const char *slot, int quantity);
float CarryLimit(void);
int FindInventoryItemIndex(const char *name);
int GetMaterialCount(const char *name);
void AddMaterialToInventory(const char *name, int quantity);
int ConsumeMaterialFromInventory(const char *name, int quantity);
float LootWeight(int item_id);
int AddLootToInventory(int item_id, int quantity);
void DropGroundLoot(float x, float y, int item_id, int quantity);
void TryPickupGroundLoot(void);
void DropSelectedItem(void);
void TryPickupDroppedItems(void);
int IsLegendaryItemName(const char *name);
void ExpireDroppedItems(void);
void ClearLegendaryDrops(void);
int HasInventoryItem(const char *name);
void ConsumeSelectedFood(void);

#endif // INVENTORY_H
