#ifndef QUESTS_H
#define QUESTS_H

// Everyday villager jobs: slay or fetch a few of something, scaled to the
// hero's level, paid in gold by any villager once done.
#define QUEST_TEMPLATE_COUNT 8
#define QUEST_TEMPLATE_MAX 16 // room for NewQuest lines from the Update folder
extern int32_t village_quest[5]; // active, template, needed, progress, reward

int VillagerQuestTalk(void);
void NoteQuestKill(int monster);
int EditQuestTemplate(int n, const char *field, const char *val);
int NewQuestTemplate(int n, const char *name, int reward);

#endif
