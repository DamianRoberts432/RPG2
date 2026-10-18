#ifndef RENDER_WORLD_H
#define RENDER_WORLD_H

// Isometric helpers and environment drawing (tiles, trees, fires, weather, loot)

void GetIsoCoords(float cx, float cy, int *sx, int *sy);
COLORREF GetTileColor(uint8_t id, int side);
void DrawFlowingRiver(HDC hdc, int sx, int sy, int tile_seed);
void InjectDynamicGlowPass(HDC hdc, int hx, int hy, int radius, COLORREF light_color);
void DrawInteractiveTorches(HDC hdc, int sx, int sy);
void DrawCampfires(HDC hdc);
void DrawLowPolyTree(HDC hdc, int sx, int sy, int tile_seed);
void DrawSubterraneanGeology(HDC hdc, int sx, int sy, int tile_seed);
void DrawGroundSceneryDecals(HDC hdc, int sx, int sy, int type, int seed);
void DrawEnvironmentalCritters(HDC hdc);
void DrawSolitaireSunMoonBeam(HDC hdc);
void DrawNightEyes(HDC hdc);
int RainHash(int value);
void FillWindowAlpha(HDC hdc, COLORREF color, int alpha);
void ApplyCameraZoom(HDC hdc, float zoom);
void DrawShoreFoam(HDC hdc, int c, int r, int sx, int sy);
float UpdateAfkCamera(int input_active);
void CycleCameraZoom(void);
int AfkViewMs(void);
void FillRectAlpha(HDC hdc, int x, int y, int w, int h, COLORREF color, int alpha);
void DrawRainZones(HDC hdc);
void DrawSleepFade(HDC hdc);
void DrawBedroll(HDC hdc);
void DrawGroundLoot(HDC hdc);
void DrawDroppedItems(HDC hdc);

#endif // RENDER_WORLD_H
