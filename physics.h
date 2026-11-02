#ifndef PHYSICS_H
#define PHYSICS_H
#include "fixed.h"

// Per-frame simulation: movement, collision, stamina, day/night, weather

// Village homes: footprint centre and half sizes in tiles. The centre is where
// the home is drawn, measured at the hero's feet (drawn HOME_FEET_OFFSET tiles
// in front of player_x/player_y), so the solid area matches the picture.
#define HOME_HALF_X FxI(1)
#define HOME_HALF_Y FX(0.8)
#define HOME_FEET_OFFSET FX(0.55)
int TileBlocksPlayer(fx x, fx y);
int VillageHomeCentre(int i, fx *x, fx *y);
int PlayerCanStandAt(fx x, fx y);
int PlayerTryMove(fx tx, fx ty, fx* ox, fx* oy);
extern fx ice_vx, ice_vy;
int IsOnSlipperyGround(void);
void UpdateIceSlide(void);
void UpdateGamePhysics(void);

#endif // PHYSICS_H
