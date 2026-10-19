#ifndef PHYSICS_H
#define PHYSICS_H

// Per-frame simulation: movement, collision, stamina, day/night, weather

// Village homes: footprint centre and half sizes in tiles. The centre is where
// the home is drawn, measured at the hero's feet (drawn HOME_FEET_OFFSET tiles
// in front of player_x/player_y), so the solid area matches the picture.
#define HOME_HALF_X 1.0f
#define HOME_HALF_Y 0.8f
#define HOME_FEET_OFFSET 0.55f
int VillageHomeCentre(int i, float *x, float *y);
int PlayerCanStandAt(float x, float y);
int PlayerTryMove(float tx, float ty, float* ox, float* oy);
extern float ice_vx, ice_vy;
int IsOnSlipperyGround(void);
void UpdateIceSlide(void);
void UpdateGamePhysics(void);

#endif // PHYSICS_H
