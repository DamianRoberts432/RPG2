#include "game.h"

// Drop .txt files into the Update folder next to game.exe, one change per line:
//   EditItem,Garlic,GoldValue,12
//   EditItem,Katana,AtkBonus,2,Durability,300,Weight,2
//   EditNPC,Villager,Dialogue,"Mind the wolves, friend."
//   EditQuest,3,TargetEntity,Goblin,TargetCount,5,RewardGold,200
// At launch every line is applied and copied into Update\applied.txt (replayed
// on every later launch), failed lines go to Update\errors.txt, and the
// source file is deleted.
#define UPDATE_DIR "Update"
#define UPDATE_CACHE UPDATE_DIR "\\applied.txt"
#define UPDATE_ERRORS UPDATE_DIR "\\errors.txt"
#define MAX_WEIGHT_EDITS 32
#define MAX_VILLAGER_LINES 16

static struct { char name[32]; float weight; } weight_edits[MAX_WEIGHT_EDITS];
static int weight_edit_count;
static char villager_lines[MAX_VILLAGER_LINES][160];
static int villager_line_count;

// Quote-aware CSV field: commas inside "..." stay part of the field.
static char *NextField(char **cur) {
    char *p = *cur;
    if (!p) return NULL;
    while (*p == ' ' || *p == '\t') p++;
    char *start = p, *out = p;
    int quoted = 0;
    while (*p && (quoted || *p != ',')) {
        if (*p == '"') quoted = !quoted; else *out++ = *p;
        p++;
    }
    *cur = *p ? p + 1 : NULL;
    *out = 0;
    while (out > start && (out[-1] == ' ' || out[-1] == '\t')) *--out = 0;
    return start;
}

static MerchantItem *FindMerchantItem(const char *id) {
    if (_strnicmp(id, "item_id_", 8) == 0) { int n = atoi(id + 8); return n >= 0 && n < MERCH_ITEMS ? &merchant_catalog[n] : NULL; }
    for (int i = 0; i < MERCH_ITEMS; i++) if (_stricmp(merchant_catalog[i].name, id) == 0) return &merchant_catalog[i];
    return NULL;
}

static WeaponStats *FindWeapon(const char *id) {
    for (int i = 0; i < WEAPON_CATALOG_COUNT; i++) if (_stricmp(weapon_catalog[i].name, id) == 0) return &weapon_catalog[i];
    return NULL;
}

float UpdatedItemWeight(const char *name) {
    for (int i = 0; i < weight_edit_count; i++) if (strcmp(weight_edits[i].name, name) == 0) return weight_edits[i].weight;
    return -1.0f;
}

const char *CustomVillagerLine(void) {
    return villager_line_count ? villager_lines[rand() % villager_line_count] : NULL;
}

static int EditItemField(const char *id, const char *field, const char *val) {
    MerchantItem *m = FindMerchantItem(id);
    WeaponStats *w = FindWeapon(m ? m->name : id);
    const char *name = m ? m->name : w ? w->name : NULL;
    int v = atoi(val);
    if (!name) return 0;
    if (_stricmp(field, "GoldValue") == 0 || _stricmp(field, "Price") == 0) {
        if (v < 0) return 0;
        if (m) m->base_price = v;
        if (w) w->price = v;
        return 1;
    }
    if (_stricmp(field, "Stock") == 0 && m && v >= -1) { m->stock = v; m->max_stock = v < 0 ? 0 : v; return 1; }
    if (_stricmp(field, "AtkBonus") == 0 && w) { w->damage += v; if (w->damage < 1) w->damage = 1; return 1; }
    if (_stricmp(field, "Damage") == 0 && w && v >= 1) { w->damage = v; return 1; }
    if (_stricmp(field, "Durability") == 0 && w && v >= 1) { w->durability_max = v; return 1; }
    if (_stricmp(field, "Weight") == 0) {
        float f = (float)atof(val);
        if (f < 0.0f) return 0;
        if (w) w->weight = f;
        int k = 0;
        while (k < weight_edit_count && strcmp(weight_edits[k].name, name) != 0) k++;
        if (k == MAX_WEIGHT_EDITS) return 0;
        if (k == weight_edit_count) weight_edit_count++;
        snprintf(weight_edits[k].name, sizeof(weight_edits[k].name), "%s", name);
        weight_edits[k].weight = f;
        return 1;
    }
    return 0;
}

static int EditNpcField(const char *id, const char *field, const char *val) {
    if (_stricmp(id, "Villager") != 0 || _stricmp(field, "Dialogue") != 0 || !val[0]) return 0;
    for (int i = 0; i < villager_line_count; i++) if (strcmp(villager_lines[i], val) == 0) return 1;
    if (villager_line_count == MAX_VILLAGER_LINES) return 0;
    snprintf(villager_lines[villager_line_count++], sizeof(villager_lines[0]), "%s", val);
    return 1;
}

// NewCard,CardID,Name,Set(0 wild/1 angel/2 demon),Rarity(1-3),Atk,Hp[,Ability]
// Stats are clamped to the rarity cap: 1/1, 2/2 or 3/3.
static int NewCardLine(char *cur) {
    char *f[6] = { 0 };
    for (int i = 0; i < 6 && cur; i++) f[i] = NextField(&cur);
    if (!f[4] || cur) return 0;
    return AddRuntimeCard(f[0], atoi(f[1]), atoi(f[2]), atoi(f[3]), atoi(f[4]), f[5] ? atoi(f[5]) : 0);
}

// Applies one line; returns 1 when every field in it was understood.
static int ApplyUpdateLine(char *line) {
    char *cur = line;
    char *cmd = NextField(&cur), *id = NextField(&cur);
    if (!cmd || !id || !cur) return 0;
    if (_stricmp(cmd, "NewCard") == 0) return NewCardLine(cur);
    int quest = 0;
    if (_stricmp(cmd, "EditQuest") == 0) quest = atoi(_strnicmp(id, "quest_id_", 9) == 0 ? id + 9 : id);
    else if (_stricmp(cmd, "EditItem") != 0 && _stricmp(cmd, "EditNPC") != 0) return 0;
    int fields = 0;
    while (cur) {
        char *field = NextField(&cur), *val = NextField(&cur);
        if (!field || !val) return 0;
        int ok = _stricmp(cmd, "EditItem") == 0 ? EditItemField(id, field, val) :
                 _stricmp(cmd, "EditNPC") == 0 ? EditNpcField(id, field, val) : EditQuestTemplate(quest, field, val);
        if (!ok) return 0;
        fields++;
    }
    return fields > 0;
}

// Returns the number of lines applied; copies them to `keep` and failures to `errors`.
static int ApplyUpdateFile(const char *path, FILE *keep, FILE *errors, int *failed) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512], copy[512];
    int applied = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#' || (p[0] == '/' && p[1] == '/')) continue;
        snprintf(copy, sizeof(copy), "%s", p);
        if (ApplyUpdateLine(p)) { applied++; if (keep) fprintf(keep, "%s\n", copy); }
        else { if (failed) (*failed)++; if (errors) fprintf(errors, "%s: %s\n", path, copy); }
    }
    fclose(f);
    return applied;
}

void IngestUpdates(void) {
    CreateDirectoryA(UPDATE_DIR, NULL);
    ApplyUpdateFile(UPDATE_CACHE, NULL, NULL, NULL);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(UPDATE_DIR "\\*.txt", &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    int files = 0, applied = 0, failed = 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || _stricmp(fd.cFileName, "applied.txt") == 0 ||
            _stricmp(fd.cFileName, "errors.txt") == 0) continue;
        char path[MAX_PATH + 16];
        snprintf(path, sizeof(path), UPDATE_DIR "\\%s", fd.cFileName);
        FILE *keep = fopen(UPDATE_CACHE, "a"), *errors = fopen(UPDATE_ERRORS, "a");
        int n = ApplyUpdateFile(path, keep, errors, &failed);
        if (keep) fclose(keep);
        if (errors) fclose(errors);
        if (n >= 0) { applied += n; files++; remove(path); }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (files) sprintf(arpg_action_log, "UPDATE: %d change%s applied from %d file%s%s", applied, applied == 1 ? "" : "s",
                       files, files == 1 ? "" : "s", failed ? " (some lines failed - see Update\\errors.txt)" : ".");
}
