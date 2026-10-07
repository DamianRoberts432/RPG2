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
void ApplyCameraZoom(HDC hdc);
void DrawRainZones(HDC hdc);
void DrawSleepFade(HDC hdc);
void DrawBedroll(HDC hdc);
void DrawGroundLoot(HDC hdc);
void DrawDroppedItems(HDC hdc);

#endif // RENDER_WORLD_H
