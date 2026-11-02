#ifndef FISHING_H
#define FISHING_H
#include "fixed.h"

// Fishing on X: cast at water, wait for the bobber to dip (the pad rumbles
// gently), then press X during the dip to land the fish.

int FishingActive(void);
int FishingBobberPos(fx *x, fx *y, int *nibble);
void CastFishingLine(void);
void FishingPressX(void);
void DrawFishing(HDC hdc);

#endif
