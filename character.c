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
