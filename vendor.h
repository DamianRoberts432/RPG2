#ifndef VENDOR_H
#define VENDOR_H

// Merchant restock, buy and sell

void RestockMerchant(void);
void HandleVendorBuy(void);
void HandleVendorSell(void);
void HandleVendorBrowse(void);
void ShopMoveCursor(int step);
void ShopConfirm(void);
extern int shop_action_menu, shop_action_cursor;
void RepairEquippedWeapon(void);
void BuyCardBooster(void);
#define CARD_BOOSTER_PRICE 20000

#endif // VENDOR_H
