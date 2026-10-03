#ifndef UI_H
#define UI_H

// HUD, inventory/map/options menu and vendor shop overlays

HFONT GetMenuUnderlineFont(void);
void DrawStatJewels(HDC hdc, int x, int y, const char *label, int value);
void DrawPlayerStatusHUD(HDC hdc);
void DrawVendorShopOverlay(HDC hdc);
void DrawTabbedMenuOverlay(HDC hdc);

#endif // UI_H
