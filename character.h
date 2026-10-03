#ifndef CHARACTER_H
#define CHARACTER_H

// Character creation helpers, class/race stats, save stub

int IsNameValid(void);
void ApplyClassAndRaceStats(void);
float GetRaceSpeedMultiplier(void);
void SaveSaveFileToDisk(void);

#endif // CHARACTER_H
