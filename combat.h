#ifndef COMBAT_H
#define COMBAT_H

// Melee, enemy damage/respawn, class abilities, blood and debris FX state

void TriggerMonsterRespawn(void);
void SpawnBloodMist(float x, float y);
void PerformMeleeAttack(void);
void SpawnTreeDebris(float cx, float cy);
void UpdateDebrisTwigs(void);
void HandleEnemyDamage(float dmg);
float DistanceToEnemy(void);
int IsFacingEnemyWithin(float dot_threshold);
int IsFacingEnemy(void);
void FirePlayerArrow(int is_tap);
void UpdatePlayerArrow(void);
void FireClassAbility(void);

#endif // COMBAT_H
