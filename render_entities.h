#ifndef RENDER_ENTITIES_H
#define RENDER_ENTITIES_H

// Hero, projectiles, enemies, effects, merchant and village drawing

void DrawHeroAssetEx(HDC hdc, int render_x, int render_y, float override_scale);
void DrawHeroAsset(HDC hdc);
void DrawFlyingArrow(HDC hdc);
void DrawFlyingSpell(HDC hdc);
void DrawClassAbilityFX(HDC hdc);
void DrawButterflies(HDC hdc);
void DrawBloodMistFX(HDC hdc);
void DrawZombieBody(HDC hdc, int sx, int sy, int seed, int face_dir, int arms_out, COLORREF head_color);
void DrawDynamicEnemy(HDC hdc);
void DrawEnemiesAtTile(HDC hdc, int c, int r);
void DrawDebrisTwigs(HDC hdc);
void DrawSummonedZombie(HDC hdc);
void DrawMerchantStoreFront(HDC hdc);
void DrawZeldaStyleVillager(HDC hdc, int sx, int sy, int npc_index, float scale);
void DrawVillage(HDC hdc);

#endif // RENDER_ENTITIES_H
