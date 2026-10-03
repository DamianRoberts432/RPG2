#ifndef PHYSICS_H
#define PHYSICS_H

// Per-frame simulation: movement, collision, stamina, day/night, weather

int PlayerCanStandAt(float x, float y);
int PlayerTryMove(float tx, float ty, float* ox, float* oy);
void UpdateGamePhysics(void);

#endif // PHYSICS_H
