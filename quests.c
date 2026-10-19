#include "game.h"

typedef struct { int fetch, monster; char item[24]; int count, reward; char name[40]; } QuestTemplate;
static QuestTemplate quest_templates[QUEST_TEMPLATE_MAX] = {
    { 0, MONSTER_SKELLY, "", 3, 60 }, { 0, MONSTER_ZOMBIE, "", 3, 70 },
    { 0, MONSTER_GOBLIN, "", 4, 60 }, { 0, MONSTER_SATYR, "", 2, 90 },
    { 1, 0, "Wood", 8, 40 }, { 1, 0, "Stone", 6, 45 }, { 1, 0, "Fish", 3, 50 }, { 1, 0, "Apple", 4, 35 },
};
static int quest_template_count = QUEST_TEMPLATE_COUNT;
int32_t village_quest[5];
int quest_screen = -1; // the town that gave the current job (shown on the map)

#define QUEST_OFFER_PERCENT 35

static const char *Plural(const char *name, int n) {
    static char buf[40];
    int no_s = n == 1 || strcmp(name, "Wood") == 0 || strcmp(name, "Stone") == 0 || strcmp(name, "Fish") == 0;
    snprintf(buf, sizeof(buf), "%s%s", name, no_s ? "" : "s");
    return buf;
}

static const char *Target(const QuestTemplate *t, int n) {
    return Plural(t->fetch ? t->item : MonsterName((MonsterType)t->monster), n);
}

static void PayQuest(void) {
    gold_count += village_quest[4];
    sprintf(arpg_action_log, "QUEST DONE: \"Thank you, friend!\" The villager pays you %d gold.", village_quest[4]);
    village_quest[0] = 0;
}

// Talking to a villager: pay out, report progress, or (sometimes) offer a job.
int VillagerQuestTalk(void) {
    if (village_quest[0]) {
        if (village_quest[1] < 0 || village_quest[1] >= quest_template_count) { village_quest[0] = 0; return 0; }
        QuestTemplate *t = &quest_templates[village_quest[1]];
        int need = village_quest[2];
        if (t->fetch) {
            int have = GetMaterialCount(t->item);
            if (have >= need) { ConsumeMaterialFromInventory(t->item, need); RecalculateCarriedWeight(); PayQuest(); }
            else sprintf(arpg_action_log, "QUEST: \"Bring me %d %s.\" You have %d.", need, Target(t, need), have);
        } else if (village_quest[3] >= need) PayQuest();
        else sprintf(arpg_action_log, "QUEST: %d of %d %s slain. Keep at it!", village_quest[3], need, Target(t, need));
        return 1;
    }
    if (rand() % 100 >= QUEST_OFFER_PERCENT) return 0;
    int k = rand() % quest_template_count;
    QuestTemplate *t = &quest_templates[k];
    if (t->count <= 0) return 0;
    int need = t->count + player_level / 5;
    int reward = t->reward * (10 + player_level) / 10;
    quest_screen = current_screen_index;
    village_quest[0] = 1; village_quest[1] = k; village_quest[2] = need; village_quest[3] = 0; village_quest[4] = reward;
    sprintf(arpg_action_log, "QUEST: \"Could you %s %d %s for me? I'll pay %d gold.\" (Any villager will pay.)",
            t->fetch ? "bring me" : "slay", need, Target(t, need), reward);
    if (t->name[0]) {
        char buf[sizeof(arpg_action_log) + 48];
        snprintf(buf, sizeof(buf), "%s - %s", t->name, arpg_action_log);
        buf[sizeof(arpg_action_log) - 1] = 0;
        strcpy(arpg_action_log, buf);
    }
    return 1;
}

void NoteQuestKill(int monster) {
    if (!village_quest[0] || village_quest[1] < 0 || village_quest[1] >= quest_template_count) return;
    QuestTemplate *t = &quest_templates[village_quest[1]];
    if (t->fetch || t->monster != monster || village_quest[3] >= village_quest[2]) return;
    village_quest[3]++;
    size_t n = strlen(arpg_action_log);
    if (village_quest[3] >= village_quest[2]) snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  QUEST: done! Tell a villager.");
    else snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  QUEST: %d / %d", village_quest[3], village_quest[2]);
}

// EditQuest lines from the Update folder (see update.c). n is 1-based.
int EditQuestTemplate(int n, const char *field, const char *val) {
    if (n < 1 || n > quest_template_count) return 0;
    QuestTemplate *t = &quest_templates[n - 1];
    if (_stricmp(field, "Type") == 0) { t->fetch = _stricmp(val, "fetch") == 0; return 1; }
    if (_stricmp(field, "TargetEntity") == 0) {
        for (int m = 0; m < MONSTER_TYPE_COUNT; m++)
            if (_stricmp(MonsterName((MonsterType)m), val) == 0) { t->fetch = 0; t->monster = m; return 1; }
        t->fetch = 1; snprintf(t->item, sizeof(t->item), "%s", val);
        return 1;
    }
    if (_stricmp(field, "TargetCount") == 0 && atoi(val) > 0) { t->count = atoi(val); return 1; }
    if (_stricmp(field, "RewardGold") == 0 && atoi(val) >= 0) { t->reward = atoi(val); return 1; }
    return 0;
}

// NewQuest lines: slot n (1-16) gets a name and reward. It is offered once
// EditQuest gives it a TargetEntity and TargetCount.
int NewQuestTemplate(int n, const char *name, int reward) {
    if (n < 1 || n > QUEST_TEMPLATE_MAX || reward < 0) return 0;
    while (quest_template_count < n) quest_templates[quest_template_count++] = (QuestTemplate){ 0, 0, "", 0, 0, "" };
    QuestTemplate *t = &quest_templates[n - 1];
    snprintf(t->name, sizeof(t->name), "%s", name);
    t->reward = reward;
    return 1;
}
