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

static struct { char name[32]; fx weight; } weight_edits[MAX_WEIGHT_EDITS];
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

fx UpdatedItemWeight(const char *name) {
    for (int i = 0; i < weight_edit_count; i++) if (strcmp(weight_edits[i].name, name) == 0) return weight_edits[i].weight;
    return -FxI(1);
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
        fx f = FxParse(val);
        if (f < FxI(0)) return 0;
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

static char new_item_names[MERCH_CAPACITY - MERCH_BASE_ITEMS][32];
static struct { char name[32]; int heal; } food_heals[MERCH_CAPACITY - MERCH_BASE_ITEMS];
static int food_heal_count;

int UpdatedFoodHeal(const char *name) {
    for (int i = 0; i < food_heal_count; i++) if (strcmp(food_heals[i].name, name) == 0) return food_heals[i].heal;
    return 0;
}

// NewItem,ItemID,Name,Type,Value[,Stackable,Weight,EffectID]: a new merchant item.
// Type is Food, Material or Tool; for Food, EffectID is the health it restores.
static int NewItemLine(char *cur) {
    char *f[6] = { 0 };
    for (int i = 0; i < 6 && cur; i++) f[i] = NextField(&cur);
    if (!f[2] || cur || !f[0][0] || atoi(f[2]) < 0) return 0;
    const char *slot = _stricmp(f[1], "Food") == 0 ? "Food" : _stricmp(f[1], "Material") == 0 ? "Material" : _stricmp(f[1], "Tool") == 0 ? "Tool" : NULL;
    if (!slot) return 0;
    MerchantItem *m = FindMerchantItem(f[0]);
    if (!m) {
        if (merch_item_count >= MERCH_CAPACITY) return 0;
        char *nm = new_item_names[merch_item_count - MERCH_BASE_ITEMS];
        snprintf(nm, 32, "%s", f[0]);
        m = &merchant_catalog[merch_item_count++];
        *m = (MerchantItem){ nm, slot, 0, atoi(f[2]), -1, 0, RARITY_COMMON, 0 };
    } else m->base_price = atoi(f[2]);
    if (f[4] && f[4][0] && !EditItemField(m->name, "Weight", f[4])) return 0;
    if (slot[0] == 'F' && f[5] && atoi(f[5]) > 0 && food_heal_count < MERCH_CAPACITY - MERCH_BASE_ITEMS) {
        snprintf(food_heals[food_heal_count].name, sizeof(food_heals[0].name), "%s", m->name);
        food_heals[food_heal_count++].heal = atoi(f[5]);
    }
    return 1;
}

// Biome and monster names used by NewSpawner, NewSetPiece and NewBiome.
static int BiomeByName(const char *s) {
    static const char *names[] = { "Prairie", "Swamp", "Beach", "Mountain", "Desert", "Tundra", "Grasslands", "Oasis",
                                   "Forest", "Island", "Ocean", "Lake", "River", "Cave", "Castle" };
    if (_stricmp(s, "Any") == 0) return -2;
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) if (_stricmp(s, names[i]) == 0) return i;
    return -1;
}

static int MonsterByName(const char *s) {
    if (_stricmp(s, "Dracula") == 0) return MONSTER_DRACULA;
    for (int i = 0; i <= MONSTER_BUNYIP; i++) if (_stricmp(s, MonsterName((MonsterType)i)) == 0) return i;
    return -1;
}

// NewSpawner,SpawnerID,Biome,Monster,Percent[,NightOnly]: on that biome (or Any), the monster
// replaces the usual pick that often. Checked after the mythic and Dracula rolls.
#define MAX_SPAWNERS 16
static struct { int8_t biome, monster, night_only; uint8_t pct; } spawners[MAX_SPAWNERS];
static int spawner_count;

static int NewSpawnerLine(char *cur) {
    char *f[4] = { 0 };
    for (int i = 0; i < 4 && cur; i++) f[i] = NextField(&cur);
    if (!f[2] || cur || spawner_count == MAX_SPAWNERS) return 0;
    int b = BiomeByName(f[0]), m = MonsterByName(f[1]), pct = atoi(f[2]);
    if (b == -1 || m < 0 || pct < 1 || pct > 100) return 0;
    spawners[spawner_count].biome = (int8_t)b;
    spawners[spawner_count].monster = (int8_t)m;
    spawners[spawner_count].night_only = (int8_t)(f[3] && atoi(f[3]) != 0);
    spawners[spawner_count++].pct = (uint8_t)pct;
    return 1;
}

int CustomSpawnPick(int biome, int is_night) {
    for (int i = 0; i < spawner_count; i++) {
        if (spawners[i].biome != -2 && spawners[i].biome != biome) continue;
        if (spawners[i].night_only && !is_night) continue;
        if (rand() % 100 < spawners[i].pct) return spawners[i].monster;
    }
    return -1;
}

// Tile letters for NewSetPiece: g ground, w rock wall, s sand, n snow, c cactus, t tree, ~ water, . leave as is.
static int SetPieceTile(char ch) {
    switch (ch) {
    case 'g': return 0; case 'w': return 1; case 's': return 4; case 'n': return 5;
    case 'c': return 7; case 't': return 11; case '~': return 12; case '.': return 255;
    }
    return -1;
}

// NewSetPiece,SetPieceID,Biome,Percent,Rows: a small hand-made patch (up to 7x7) stamped onto
// that share of the biome's screens. Rows are split by '/', e.g. "www/w.w/www".
#define MAX_SET_PIECES 12
#define SET_PIECE_MAX 7
static struct { int8_t biome; uint8_t pct, w, h; uint8_t tiles[SET_PIECE_MAX][SET_PIECE_MAX]; } set_pieces[MAX_SET_PIECES];
static int set_piece_count;

static int NewSetPieceLine(char *cur) {
    char *f[3] = { 0 };
    for (int i = 0; i < 3 && cur; i++) f[i] = NextField(&cur);
    if (!f[2] || cur || set_piece_count == MAX_SET_PIECES) return 0;
    int b = BiomeByName(f[0]), pct = atoi(f[1]);
    if (b == -1 || pct < 1 || pct > 100) return 0;
    int w = 0, h = 0, col = 0;
    memset(set_pieces[set_piece_count].tiles, 255, sizeof(set_pieces[0].tiles));
    for (const char *p = f[2]; ; p++) {
        if (*p == '/' || *p == 0) {
            if (col == 0 || h == SET_PIECE_MAX) return 0;
            if (col > w) w = col;
            h++; col = 0;
            if (*p == 0) break;
            continue;
        }
        int t = SetPieceTile(*p);
        if (t < 0 || col == SET_PIECE_MAX) return 0;
        set_pieces[set_piece_count].tiles[h][col++] = (uint8_t)t;
    }
    set_pieces[set_piece_count].biome = (int8_t)b;
    set_pieces[set_piece_count].pct = (uint8_t)pct;
    set_pieces[set_piece_count].w = (uint8_t)w;
    set_pieces[set_piece_count++].h = (uint8_t)h;
    return 1;
}

void ApplySetPieces(int biome) {
    for (int i = 0; i < set_piece_count; i++) {
        if (set_pieces[i].biome != -2 && set_pieces[i].biome != biome) continue;
        if (rand() % 100 >= set_pieces[i].pct) continue;
        int w = set_pieces[i].w, h = set_pieces[i].h, span_x = MAP_SIZE - 8 - w, span_y = MAP_SIZE - 8 - h;
        int ox = 4 + (span_x > 0 ? rand() % span_x : 0), oy = 4 + (span_y > 0 ? rand() % span_y : 0);
        if (ox <= 14 && ox + w >= 10 && oy <= 14 && oy + h >= 10) continue; // keep clear of the cave mouth
        for (int r = 0; r < h; r++) for (int c = 0; c < w; c++)
            if (set_pieces[i].tiles[r][c] != 255) VISUAL_MAP[oy + r][ox + c] = set_pieces[i].tiles[r][c];
    }
}

// NewBiome,BiomeID,Name,BaseBiome,Percent,Trees,Rocks,Water: that share of the base biome's
// screens are re-laid as a named variant with those percentages of trees, rock and water.
#define MAX_BIOME_VARIANTS 8
static struct { char name[32]; int8_t base; uint8_t pct, trees, rocks, water; } variants[MAX_BIOME_VARIANTS];
static int variant_count;

static int NewBiomeLine(char *cur) {
    char *f[6] = { 0 };
    for (int i = 0; i < 6 && cur; i++) f[i] = NextField(&cur);
    if (!f[5] || cur || !f[0][0] || variant_count == MAX_BIOME_VARIANTS) return 0;
    int b = BiomeByName(f[1]), pct = atoi(f[2]), tr = atoi(f[3]), ro = atoi(f[4]), wa = atoi(f[5]);
    if (b < 0 || b == BIOME_CAVE || b == BIOME_CASTLE || pct < 1 || pct > 100) return 0;
    if (tr < 0 || ro < 0 || wa < 0 || ro > 30 || tr + ro + wa > 90) return 0; // rock walls past 30% box the hero in
    snprintf(variants[variant_count].name, sizeof(variants[0].name), "%s", f[0]);
    variants[variant_count].base = (int8_t)b;
    variants[variant_count].pct = (uint8_t)pct;
    variants[variant_count].trees = (uint8_t)tr;
    variants[variant_count].rocks = (uint8_t)ro;
    variants[variant_count++].water = (uint8_t)wa;
    return 1;
}

const char *ApplyBiomeVariant(int biome) {
    for (int i = 0; i < variant_count; i++) {
        if (variants[i].base != biome || rand() % 100 >= variants[i].pct) continue;
        int ground = biome == BIOME_DESERT || biome == BIOME_BEACH || biome == BIOME_OASIS ? 4 : biome == BIOME_TUNDRA ? 5 : 0;
        for (int r = 1; r < MAP_SIZE - 1; r++) for (int c = 1; c < MAP_SIZE - 1; c++) {
            int n = rand() % 100;
            VISUAL_MAP[r][c] = (uint8_t)(n < variants[i].trees ? 11 : n < variants[i].trees + variants[i].rocks ? 1 :
                                         n < variants[i].trees + variants[i].rocks + variants[i].water ? 12 : ground);
        }
        return variants[i].name;
    }
    return NULL;
}

// NewRecipe,RecipeID,Result,Quantity,Material,Count[,Material,Count[,Material,Count]]:
// the blacksmith lists "Craft" rows that turn carried materials into the result.
#define MAX_RECIPES 16
static struct { char result[32]; uint8_t qty, n; char mat[3][32]; uint8_t count[3]; } recipes[MAX_RECIPES];
static int recipe_count;

static int NewRecipeLine(char *cur) {
    char *f[8] = { 0 };
    int got = 0;
    for (; got < 8 && cur; got++) f[got] = NextField(&cur);
    if (cur || got < 4 || got % 2 || recipe_count == MAX_RECIPES || !f[0][0]) return 0;
    int qty = atoi(f[1]);
    if (qty < 1 || qty > 99) return 0;
    snprintf(recipes[recipe_count].result, sizeof(recipes[0].result), "%s", f[0]);
    recipes[recipe_count].qty = (uint8_t)qty;
    recipes[recipe_count].n = (uint8_t)((got - 2) / 2);
    for (int k = 0; k < recipes[recipe_count].n; k++) {
        int c = atoi(f[3 + k * 2]);
        if (!f[2 + k * 2][0] || c < 1 || c > 99) return 0;
        snprintf(recipes[recipe_count].mat[k], sizeof(recipes[0].mat[0]), "%s", f[2 + k * 2]);
        recipes[recipe_count].count[k] = (uint8_t)c;
    }
    recipe_count++;
    return 1;
}

int RecipeCount(void) { return recipe_count; }

void RecipeLabel(int i, char *out, size_t n) {
    int len = snprintf(out, n, "Craft %d %s (", recipes[i].qty, recipes[i].result);
    for (int k = 0; k < recipes[i].n && len > 0 && (size_t)len < n; k++)
        len += snprintf(out + len, n - len, "%s%d %s", k ? ", " : "", recipes[i].count[k], recipes[i].mat[k]);
    if (len > 0 && (size_t)len < n) snprintf(out + len, n - len, ")");
}

int CraftRecipe(int i, char *msg, size_t n) {
    if (i < 0 || i >= recipe_count) return 0;
    for (int k = 0; k < recipes[i].n; k++)
        if (GetMaterialCount(recipes[i].mat[k]) < recipes[i].count[k]) {
            snprintf(msg, n, "You need %d %s for that.", recipes[i].count[k], recipes[i].mat[k]);
            return 0;
        }
    for (int k = 0; k < recipes[i].n; k++) ConsumeMaterialFromInventory(recipes[i].mat[k], recipes[i].count[k]);
    AddMaterialToInventory(recipes[i].result, recipes[i].qty);
    snprintf(msg, n, "Here you go: %d %s.", recipes[i].qty, recipes[i].result);
    return 1;
}

// NewDecor,DecorID,Name,Price,Light(0/1),Comfort: a home decoration the merchant sells.
static int NewDecorLine(char *cur) {
    char *f[4] = { 0 };
    for (int i = 0; i < 4 && cur; i++) f[i] = NextField(&cur);
    if (!f[1] || cur || !f[0][0] || atoi(f[1]) < 0) return 0;
    if (AddRuntimeDecor(f[0], f[2] ? atoi(f[2]) : 0, f[3] && f[3][0] ? atoi(f[3]) : 1) < 0) return 0;
    char line[64];
    snprintf(line, sizeof(line), "%s,Material,%d", f[0], atoi(f[1]));
    return NewItemLine(line);
}

// NewDialogue,DialogueID,NPCID,Speaker,Text[,NextDialogueID]: villagers say "Speaker: Text".
static int NewDialogueLine(char *cur) {
    char *f[4] = { 0 };
    for (int i = 0; i < 4 && cur; i++) f[i] = NextField(&cur);
    if (!f[2] || cur || !f[2][0]) return 0;
    char line[160];
    if (f[1][0]) snprintf(line, sizeof(line), "%s: %s", f[1], f[2]);
    else snprintf(line, sizeof(line), "%s", f[2]);
    return EditNpcField("Villager", "Dialogue", line);
}

// NewQuest,QuestID,Taxonomy,Name,GiverNPCID,RewardGold[,PrereqQuestID]
static int NewQuestLine(int n, char *cur) {
    char *f[5] = { 0 };
    for (int i = 0; i < 5 && cur; i++) f[i] = NextField(&cur);
    if (!f[3] || cur) return 0;
    if (_stricmp(f[0], "queststory") != 0 && _stricmp(f[0], "questgeneric") != 0 && _stricmp(f[0], "questking") != 0) return 0;
    return NewQuestTemplate(n, f[1], atoi(f[3]));
}

// Applies one line; returns 1 when every field in it was understood.
// NewNPC,NPCID,Name,Job,Line: a named villager whose line joins the villagers' chatter ("Name the Job: Line").
static int NewNpcLine(char *cur) {
    char *f[3] = { 0 };
    for (int i = 0; i < 3 && cur; i++) f[i] = NextField(&cur);
    if (!f[2] || cur || !f[0][0] || !f[2][0]) return 0;
    char line[160];
    if (f[1][0]) snprintf(line, sizeof(line), "%s the %s: %s", f[0], f[1], f[2]);
    else snprintf(line, sizeof(line), "%s: %s", f[0], f[2]);
    return EditNpcField("Villager", "Dialogue", line);
}

// NewEconomy,EconomyID,ItemName,PricePercent: rescales an item's market price (10-1000%).
static int NewEconomyLine(char *cur) {
    char *f[2] = { 0 };
    for (int i = 0; i < 2 && cur; i++) f[i] = NextField(&cur);
    if (!f[1] || cur) return 0;
    int pct = atoi(f[1]);
    MerchantItem *it = FindMerchantItem(f[0]);
    if (!it || pct < 10 || pct > 1000) return 0;
    it->base_price = it->base_price * pct / 100 > 0 ? it->base_price * pct / 100 : 1;
    return 1;
}

static int ApplyUpdateLine(char *line) {
    char *cur = line;
    char *cmd = NextField(&cur), *id = NextField(&cur);
    if (!cmd || !id || !cur) return 0;
    if (_stricmp(cmd, "NewNPC") == 0) return NewNpcLine(cur);
    if (_stricmp(cmd, "NewEconomy") == 0) return NewEconomyLine(cur);
    if (_stricmp(cmd, "NewCard") == 0) return NewCardLine(cur);
    if (_stricmp(cmd, "NewItem") == 0) return NewItemLine(cur);
    if (_stricmp(cmd, "NewDecor") == 0) return NewDecorLine(cur);
    if (_stricmp(cmd, "NewSpawner") == 0) return NewSpawnerLine(cur);
    if (_stricmp(cmd, "NewSetPiece") == 0) return NewSetPieceLine(cur);
    if (_stricmp(cmd, "NewBiome") == 0) return NewBiomeLine(cur);
    if (_stricmp(cmd, "NewRecipe") == 0) return NewRecipeLine(cur);
    if (_stricmp(cmd, "NewDialogue") == 0) return NewDialogueLine(cur);
    if (_stricmp(cmd, "NewQuest") == 0) return NewQuestLine(atoi(_strnicmp(id, "quest_id_", 9) == 0 ? id + 9 : id), cur);
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
