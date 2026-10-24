#include "game.h"
#include <ctype.h>

// fetch is the job kind: slay monsters, gather items, deliver a letter or
// parcel, till and plant plots, or catch fish (the Update folder's EditQuest
// still sets 0 = slay and 1 = fetch).
enum { Q_SLAY, Q_GATHER, Q_DELIVER, Q_FARM, Q_FISH };
typedef struct { int fetch, monster; char item[24]; int count, reward; char name[40]; } QuestTemplate;
static QuestTemplate quest_templates[QUEST_TEMPLATE_MAX] = {
    { 0, MONSTER_SKELLY, "", 3, 60 }, { 0, MONSTER_ZOMBIE, "", 3, 70 },
    { 0, MONSTER_GOBLIN, "", 4, 60 }, { 0, MONSTER_SATYR, "", 2, 90 },
    { 1, 0, "Wood", 8, 40 }, { 1, 0, "Stone", 6, 45 }, { 1, 0, "Fish", 3, 50 }, { 1, 0, "Apple", 4, 35 },
    { Q_DELIVER, 0, "letter", 1, 40 }, { Q_DELIVER, 0, "parcel", 1, 55 },
    { Q_GATHER, 0, "Apple", 100, 400 }, { Q_GATHER, 0, "Wood", 100, 350 }, { Q_GATHER, 0, "Stone", 100, 380 },
    { Q_FARM, 0, "", 4, 90 }, { Q_FISH, 0, "", 25, 260 },
};
static int quest_template_count = QUEST_TEMPLATE_COUNT;
int32_t village_quest[5]; // active, template, needed (recipient for deliveries), progress, reward
int quest_screen = -1; // the town that gave the current job (shown on the map)

#define QUEST_OFFER_PERCENT 35
#define QUEST_OFFER_MS 12000

// Who a letter or parcel goes to: the NPC type that pays on delivery.
static const struct { int type; const char *who; } recipients[] = {
    { 6, "the King" }, { 1, "the merchant" }, { 5, "the boy at the card shop" },
    { 3, "the blacksmith" }, { 9, "the banker" }, { 4, "the pet shop keeper" },
};
#define RECIPIENT_COUNT ((int)(sizeof(recipients) / sizeof(recipients[0])))

static int offer_k = -1, offer_need, offer_reward;
static DWORD offer_until;

static const char *Plural(const char *name, int n) {
    static char buf[40];
    int no_s = n == 1 || strcmp(name, "Wood") == 0 || strcmp(name, "Stone") == 0 || strcmp(name, "Fish") == 0;
    snprintf(buf, sizeof(buf), "%s%s", name, no_s ? "" : "s");
    return buf;
}

static const char *Target(const QuestTemplate *t, int n) {
    if (t->fetch == Q_FARM) return n == 1 ? "plot" : "plots";
    if (t->fetch == Q_FISH) return "fish";
    return Plural(t->fetch ? t->item : MonsterName((MonsterType)t->monster), n);
}

static void PayQuest(const char *who) {
    gold_count += village_quest[4];
    sprintf(arpg_action_log, "QUEST DONE: \"Thank you, friend!\" %s pays you %d gold.", who, village_quest[4]);
    const QuestTemplate *t = &quest_templates[village_quest[1]];
    if (t->fetch && t->fetch != Q_DELIVER && (strcmp(t->item, "Wood") == 0 || strcmp(t->item, "Stone") == 0) && !player_home[0] && !HasInventoryItem("House Supplies")) {
        AddStackableItem("House Supplies", "Material", 1);
        strcat(arpg_action_log, " The merchant adds House Supplies to help build you a home.");
    }
    village_quest[0] = 0;
}

static QuestTemplate *ActiveQuest(void) {
    if (!village_quest[0] || village_quest[1] < 0 || village_quest[1] >= quest_template_count) return NULL;
    return &quest_templates[village_quest[1]];
}

static void OfferText(const QuestTemplate *t, int need, int reward) {
    if (t->fetch == Q_SLAY) sprintf(arpg_action_log, "QUEST: \"Could you slay %d %s for me? I'll pay %d gold.\"", need, Target(t, need), reward);
    else if (t->fetch == Q_GATHER && need >= 50)
        sprintf(arpg_action_log, "QUEST: \"I'm really busy now, but could use your help gathering %d %s. %d gold for it.\"", need, Target(t, need), reward);
    else if (t->fetch == Q_GATHER) sprintf(arpg_action_log, "QUEST: \"Could you bring me %d %s? I'll pay %d gold.\"", need, Target(t, need), reward);
    else if (t->fetch == Q_DELIVER)
        sprintf(arpg_action_log, "QUEST: \"Can you deliver this %s to %s? They'll pay you %d gold.\"", t->item, recipients[need].who, reward);
    else if (t->fetch == Q_FARM)
        sprintf(arpg_action_log, "QUEST: \"Can you till this field and plant seeds? %d plots should do. %d gold.\"", need, reward);
    else sprintf(arpg_action_log, "QUEST: \"Catch %d fish for me? I'll pay %d gold.\"", need, reward);
    size_t n = strlen(arpg_action_log);
    snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, " X again: yes. Walk away: no.");
}

// Talking to a villager: pay out, report progress, or (sometimes) ask if the
// hero wants a small job. Asking again within a few seconds accepts it.
int VillagerQuestTalk(void) {
    QuestTemplate *t = ActiveQuest();
    if (village_quest[0] && !t) { village_quest[0] = 0; return 0; }
    if (t) {
        int need = village_quest[2];
        if (t->fetch == Q_GATHER) {
            int have = GetMaterialCount(t->item);
            if (have >= need) { ConsumeMaterialFromInventory(t->item, need); RecalculateCarriedWeight(); PayQuest("The villager"); }
            else sprintf(arpg_action_log, "QUEST: \"Bring me %d %s.\" You have %d.", need, Target(t, need), have);
        } else if (t->fetch == Q_DELIVER)
            sprintf(arpg_action_log, "QUEST: \"Please take the %s to %s.\"", t->item, recipients[need % RECIPIENT_COUNT].who);
        else if (village_quest[3] >= need) PayQuest("The villager");
        else sprintf(arpg_action_log, "QUEST: %d of %d %s %s. Keep at it!", village_quest[3], need, Target(t, need),
                     t->fetch == Q_FARM ? "planted" : t->fetch == Q_FISH ? "caught" : "slain");
        return 1;
    }
    if (offer_k >= 0 && offer_k < quest_template_count && GetTickCount() < offer_until) {
        QuestTemplate *o = &quest_templates[offer_k];
        quest_screen = current_screen_index;
        village_quest[0] = 1; village_quest[1] = offer_k; village_quest[2] = offer_need; village_quest[3] = 0; village_quest[4] = offer_reward;
        if (o->fetch == Q_DELIVER) sprintf(arpg_action_log, "QUEST: You take the %s for %s.", o->item, recipients[offer_need].who);
        else sprintf(arpg_action_log, "QUEST: \"Thank you! Any villager can pay you once it's done.\"");
        offer_k = -1;
        return 1;
    }
    offer_k = -1;
    if (rand() % 100 >= QUEST_OFFER_PERCENT) return 0;
    int k = rand() % quest_template_count;
    QuestTemplate *q = &quest_templates[k];
    if (q->count <= 0) return 0;
    int need = q->fetch == Q_DELIVER ? rand() % RECIPIENT_COUNT : q->count >= 25 ? q->count : q->count + player_level / 5;
    int reward = q->reward * (10 + player_level) / 10;
    OfferText(q, need, reward);
    if (q->name[0]) {
        char buf[sizeof(arpg_action_log) + 48];
        snprintf(buf, sizeof(buf), "%s - %s", q->name, arpg_action_log);
        buf[sizeof(arpg_action_log) - 1] = 0;
        strcpy(arpg_action_log, buf);
    }
    offer_k = k; offer_need = need; offer_reward = reward; offer_until = GetTickCount() + QUEST_OFFER_MS;
    return 1;
}

// X on a shopkeeper, the King or the banker: hand over a letter or parcel.
int QuestDeliverTo(int npc_type) {
    QuestTemplate *t = ActiveQuest();
    if (!t || t->fetch != Q_DELIVER || recipients[village_quest[2] % RECIPIENT_COUNT].type != npc_type) return 0;
    char who[48]; snprintf(who, sizeof(who), "%s", recipients[village_quest[2] % RECIPIENT_COUNT].who);
    who[0] = (char)toupper((unsigned char)who[0]);
    PayQuest(who);
    return 1;
}

static void NoteQuestProgress(int kind, const char *what) {
    QuestTemplate *t = ActiveQuest();
    if (!t || t->fetch != kind || village_quest[3] >= village_quest[2]) return;
    village_quest[3]++;
    size_t n = strlen(arpg_action_log);
    if (village_quest[3] >= village_quest[2]) snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  QUEST: done! Tell a villager.");
    else snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  QUEST: %d / %d %s", village_quest[3], village_quest[2], what);
}

void NoteQuestPlanted(void) { NoteQuestProgress(Q_FARM, "planted"); AdvNote('P', 0); }
void NoteQuestFish(void) { NoteQuestProgress(Q_FISH, "fish"); AdvNote('F', 0); }

void NoteQuestKill(int monster) {
    AdvNote('K', monster);
    QuestTemplate *t = ActiveQuest();
    if (!t || t->fetch != Q_SLAY || t->monster != monster || village_quest[3] >= village_quest[2]) return;
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
