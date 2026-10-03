#ifndef INTERACT_H
#define INTERACT_H

// Proximity checks and the contextual interact (chop/mine/fish/talk)

int IsNearPoint(float x, float y, float radius);
int IsNearMerchant(void);
int IsNearBedroll(void);
int IsAppleTree(int x, int y);
void RememberChoppedAppleTree(int x, int y);
void HandleContextInteract(void);
int IsFacingWater(void);
int IsFacingFishWater(void);

#endif // INTERACT_H
