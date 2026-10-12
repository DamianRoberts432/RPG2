#include "game.h"

// Blank/whitespace-only names are not allowed to start the game; used both
// to gate finalizing character creation and to drive the error message.
int IsNameValid(void) {
    for (int i = 0; player_name[i] != '\0'; i++) {
        if (player_name[i] != ' ') return 1;
    }
    return 0;
}

int *StatByIndex(int i) {
    int *stats[STAT_COUNT] = { &stat_strength, &stat_dexterity, &stat_stamina, &stat_magick, &stat_luck, &stat_intelligence, &stat_charisma };
    return stats[i];
}

void SpendStatPoint(void) {
    static const char *names[STAT_COUNT] = { "STR", "DEX", "STA", "MAG", "LCK", "INT", "CHR" };
    int *stat = StatByIndex(stat_cursor);
    if (stat_points <= 0) { strcpy(arpg_action_log, "No attribute points left. Level up to earn more."); return; }
    if (*stat >= STAT_CAP) { sprintf(arpg_action_log, "%s is already maxed at %d.", names[stat_cursor], STAT_CAP); return; }
    (*stat)++; stat_spent[stat_cursor]++; stat_points--;
    if (stat_cursor == 2) { max_stamina += 10.0f; player_stamina += 10.0f; }
    if (stat_cursor == 3) { max_player_mp += 10.0f; player_mp += 10.0f; }
    sprintf(arpg_action_log, "%s raised to %d. Points left: %d", names[stat_cursor], *stat, stat_points);
}

// Natural pairings: Elf Wizard (MAG), Human Knight (STA), Dwarf Warrior (STR),
// Halfling Rogue (DEX), Gnome Necromancer (INT). Returns the stat index or -1.
int RaceClassSynergyStat(void) {
    static const int fit[5] = { CLASS_WIZARD, CLASS_KNIGHT, CLASS_WARRIOR, CLASS_ROGUE, CLASS_NECROMANCER };
    static const int stat[5] = { 3, 2, 0, 1, 5 };
    return fit[selected_race] == (int)selected_class ? stat[selected_race] : -1;
}

void ApplyClassAndRaceStats(void) {
    static const int class_stats[5][7] = {
        { 1, 1, 1, 3, 2, 1, 2 }, /* Necromancer */
        { 1, 1, 2, 3, 2, 1, 2 }, /* Wizard */
        { 3, 2, 2, 0, 2, 2, 1 }, /* Knight */
        { 3, 2, 2, 0, 1, 1, 1 }, /* Warrior */
        { 2, 3, 2, 0, 2, 2, 1 }  /* Rogue */
    };
    const int *stats = class_stats[selected_class];
    stat_strength = stats[0]; stat_dexterity = stats[1]; stat_stamina = stats[2];
    stat_magick = stats[3]; stat_intelligence = stats[4]; stat_charisma = stats[5]; stat_luck = stats[6];
    // Race x class matrix: each race adds small bonuses (STR DEX STA MAG LCK
    // INT CHR order) and a natural race/class pairing adds +1 to its key stat.
    static const int race_mods[5][STAT_COUNT] = {
        { 0, 1, 0, 1, 0, 0, 0 }, /* Elf */
        { 0, 0, 0, 0, 1, 0, 1 }, /* Human */
        { 1, 0, 1, 0, 0, 0, 0 }, /* Dwarf */
        { 0, 1, 0, 0, 1, 0, 0 }, /* Halfling */
        { 0, 0, 0, 1, 0, 1, 0 }  /* Gnome */
    };
    for (int i = 0; i < STAT_COUNT; i++) *StatByIndex(i) += race_mods[selected_race][i];
    int synergy = RaceClassSynergyStat();
    if (synergy >= 0) (*StatByIndex(synergy))++;
    for (int i = 0; i < STAT_COUNT; i++) {
        int *stat = StatByIndex(i);
        *stat += stat_spent[i];
        if (*stat > STAT_CAP) *stat = STAT_CAP;
    }
    max_stamina = 100.0f + 10.0f * stat_spent[2];
    max_player_mp = 100.0f + 10.0f * stat_spent[3];

    if (selected_race == RACE_ELF) player_height_m = 1.95f;
    else if (selected_race == RACE_DWARF) player_height_m = 1.42f;
    else if (selected_race == RACE_HALFLING) player_height_m = 1.15f;
    else if (selected_race == RACE_GNOME) player_height_m = 0.98f;
    else player_height_m = 1.82f;
}

static int ClampKarma(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
void KarmaAdjust(int soul, int miasma, int heat, int grapevine) {
    karma.soul = (int8_t)ClampKarma(karma.soul + soul, -100, 100);
    karma.miasma = (uint8_t)ClampKarma(karma.miasma + miasma, 0, 100);
    karma.town_heat = (uint8_t)ClampKarma(karma.town_heat + heat, 0, 100);
    karma.grapevine = (uint8_t)ClampKarma(karma.grapevine + grapevine, 0, 100);
}

int HasCard(int id) { return id >= 0 && id < CARD_COUNT && ((card_collection_bits[id / 8] >> (id % 8)) & 1); }

int CardCount(void) {
    int n = 0;
    for (int i = 0; i < CARD_POOL; i++) n += HasCard(i);
    return n;
}

// 2% chance per kill, chest or catch; a monster usually drops its own card.
void TryCardDrop(int monster) {
    if (rand() % 100 >= CARD_DROP_PERCENT) return;
    int id = (monster >= 0 && rand() % 2 == 0) ? monster_card_links[monster].card_id
             : MONSTER_TYPE_COUNT + rand() % (CARD_POOL - MONSTER_TYPE_COUNT);
    int is_new = !HasCard(id);
    card_collection_bits[id / 8] |= (uint8_t)(1u << (id % 8));
    size_t n = strlen(arpg_action_log);
    if (id < MONSTER_TYPE_COUNT) snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  CARD: %s card%s", MonsterName((MonsterType)id), is_new ? " (new!)" : "");
    else { char nm[48]; CardName(id, nm, sizeof(nm)); snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  CARD: %s%s", nm, is_new ? " (new!)" : ""); }
}

float GetRaceSpeedMultiplier(void) {
    switch (selected_race) {
        case RACE_GNOME:    return 1.40f; 
        case RACE_ELF:      return 1.20f; 
        case RACE_HUMAN:    return 1.05f; 
        case RACE_DWARF:    return 0.95f; 
        case RACE_HALFLING: return 0.85f; 
        default:            return 1.00f;
    }
}

void SaveSaveFileToDisk(void) {
    saturn_save_screen_index = current_screen_index;
    saturn_save_steps_remaining = screens_until_town;
    saturn_save_lumber = GetMaterialCount("Wood");
    saturn_save_gold = gold_count;
    saturn_save_stone = GetMaterialCount("Stone");
    SaveWorldMapState();
}
