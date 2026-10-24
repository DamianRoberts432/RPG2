#ifndef ADVENTURES_H
#define ADVENTURES_H
// Chained quests ("adventures"). Each motif is written once and offered in 3
// variants (more monsters/items, higher level, rarer), so 104 motifs = 312 quests.
// Step codes, separated by '|':
//   V<role>   visit someone of that role in another town (%T)
//   L<role>   visit someone of that role in the contact's own town (%C)
//   K<m>,<n>  slay n monsters of type m (MonsterType index)
//   G<item>,<n> bring n items to the current contact
//   F<n> catch fish   P<n> plant plots   C search any cave
//   B<b> reach a biome (BiomeType index)   N spend a night outside town
// A last "return to the contact" step is always added. Roles: v villager,
// m merchant, g guard, s smith, p pet shop, c card kid, k King, b beggar,
// a altar, n banker. Text placeholders: %T far town, %C contact town, %K kin.
typedef struct { const char *title, *origin; char giver, rarity, level; const char *steps, *texts; } AdvMotif;
extern const AdvMotif adv_motifs[];
extern const int adv_motif_count;
#define ADV_VARIANTS 3
#define ADV_MAX 360
#endif
