#ifndef CHARACTER_H
#define CHARACTER_H
#include "fixed.h"

// Character creation helpers, class/race stats, save stub

int IsNameValid(void);
void ApplyClassAndRaceStats(void);
fx GetRaceSpeedMultiplier(void);
int RaceClassSynergyStat(void);
void SaveSaveFileToDisk(void);

#endif // CHARACTER_H
