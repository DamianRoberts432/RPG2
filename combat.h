#ifndef COMBAT_H
#define COMBAT_H
#include "fixed.h"

// Melee, enemy damage/respawn, class abilities, blood and debris FX state

void TriggerMonsterRespawn(void);
void SpawnBloodMist(fx x, fx y);
void SpawnBloodBurst(fx x, fx y, fx dir_x, fx dir_y);
void PerformMeleeAttack(void);
void SpawnTreeDebris(fx cx, fx cy);
void SpawnDirtClods(fx cx, fx cy);
void UpdateDebrisTwigs(void);
void HandleEnemyDamage(fx dmg);
extern int village_kill;
extern int minion_kill;
extern DWORD bow_stamina_lock_until;
fx DistanceToEnemy(void);
int IsFacingEnemyWithin(fx dot_threshold);
int IsFacingEnemy(void);
void ChargeBow(void);
void FirePlayerArrow(int is_tap);
void UpdatePlayerArrow(void);
void FireClassAbility(void);
void ThrowSpear(void);
void UpdateThrownSpear(void);

#endif // COMBAT_H
