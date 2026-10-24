#include "game.h"
#include "town.h"
#include "weather.h"
#include "worldmap.h"
#include "adventures.h"

#define ADV_DONE 255
#define ADV_STEPS_MAX 6
uint8_t adv_state[ADV_MAX];       // 0 = still out in the world, 1 + step while active, ADV_DONE once finished
uint16_t adv_prog[ADV_MAX];       // kills / fish / plots counted toward the current step
int32_t adv_giver_town[ADV_MAX], adv_far_town[ADV_MAX], adv_contact_town[ADV_MAX];
uint8_t adv_contact_role[ADV_MAX]; // NPC type who pays out (changes when a visit step is done)
int adv_tracked = -1;

static const char *kin_words[ADV_VARIANTS] = { "brother", "sister", "cousin" };
static const char *rarity_names[4] = { "Common", "Uncommon", "Rare", "Legendary" };
static const COLORREF rarity_colors[4] = { RGB(225, 225, 225), RGB(110, 230, 110), RGB(110, 170, 255), RGB(255, 170, 60) };
static const char role_codes[10] = { 'v', 'm', 'g', 's', 'p', 'c', 'k', 'b', 'a', 'n' };
static const char *role_names[10] = { "villager", "merchant", "guard", "smith", "pet shop keeper", "card kid", "King", "beggar", "altar keeper", "banker" };
typedef struct { char type, role; int monster, count, biome; char item[16]; } AdvStep;

int AdvCount(void) { int n = adv_motif_count * ADV_VARIANTS; return n > ADV_MAX ? ADV_MAX : n; }
static const AdvMotif *Motif(int q) { return &adv_motifs[q / ADV_VARIANTS]; }
static int Variant(int q) { return q % ADV_VARIANTS; }
int AdvRarity(int q) { int r = Motif(q)->rarity + (Variant(q) == 2); return r > 3 ? 3 : r; }
int AdvLevel(int q) { return Motif(q)->level + Variant(q) * 4; }
static int RoleIndex(char c) { for (int i = 0; i < 10; i++) if (role_codes[i] == c) return i; return 0; }

// Steps of quest q; returns how many before the final "return to the contact" step.
static int ParseSteps(int q, AdvStep *out) {
    const char *s = Motif(q)->steps;
    int n = 0, v = Variant(q);
    while (*s && n < ADV_STEPS_MAX - 1) {
        AdvStep st; memset(&st, 0, sizeof(st));
        st.type = *s++;
        if (st.type == 'V' || st.type == 'L') st.role = *s ? *s++ : 'v';
        else if (st.type == 'K') { st.monster = atoi(s); while (*s && *s != ',' && *s != '|') s++; if (*s == ',') s++; st.count = atoi(s); }
        else if (st.type == 'G') {
            int k = 0;
            while (*s && *s != ',' && *s != '|' && k < 15) st.item[k++] = *s++;
            st.item[k] = 0;
            while (*s && *s != ',' && *s != '|') s++;
            if (*s == ',') s++;
            st.count = atoi(s);
        } else if (st.type == 'F' || st.type == 'P') st.count = atoi(s);
        else if (st.type == 'B') st.biome = atoi(s);
        if (st.count > 0) st.count = st.count * (2 + v) / 2;
        while (*s && *s != '|') s++;
        if (*s == '|') s++;
        out[n++] = st;
    }
    AdvStep r; memset(&r, 0, sizeof(r)); r.type = 'R'; out[n] = r;
    return n;
}
static int StepTotal(int q) { AdvStep s[ADV_STEPS_MAX]; return ParseSteps(q, s) + 1; }
static int CurStep(int q, AdvStep *st) {
    AdvStep s[ADV_STEPS_MAX];
    int n = ParseSteps(q, s), i = adv_state[q] == ADV_DONE ? n : adv_state[q] - 1;
    if (i < 0) i = 0;
    if (i > n) i = n;
    *st = s[i];
    return i;
}
static void TownLabel(int key, char *out, size_t n) { if (key < 0) snprintf(out, n, "town"); else TownNameAt(key, out, n); }

// Line i of the quest, with %T (far town), %C (contact's town) and %K (kin) filled in.
static void StepLine(int q, int i, char *out, size_t n) {
    AdvStep s[ADV_STEPS_MAX];
    int total = ParseSteps(q, s);
    char far_t[32], con_t[32];
    TownLabel(adv_far_town[q], far_t, sizeof(far_t)); TownLabel(adv_contact_town[q], con_t, sizeof(con_t));
    if (i >= total) { snprintf(out, n, "Return to the %s in %s for your reward.", role_names[adv_contact_role[q] % 10], con_t); return; }
    const char *t = Motif(q)->texts;
    for (int k = 0; k < i && t; k++) { t = strchr(t, '|'); if (t) t++; }
    size_t o = 0;
    while (t && *t && *t != '|' && o + 1 < n) {
        if (*t == '%' && (t[1] == 'T' || t[1] == 'C' || t[1] == 'K')) {
            const char *w = t[1] == 'T' ? far_t : t[1] == 'C' ? con_t : kin_words[Variant(q)];
            while (*w && o + 1 < n) out[o++] = *w++;
            t += 2;
        } else out[o++] = *t++;
    }
    out[o] = 0;
}
static void ProgressText(int q, const AdvStep *st, char *out, size_t n) {
    if (st->type == 'K' || st->type == 'F' || st->type == 'P') snprintf(out, n, "  (%d / %d)", adv_prog[q], st->count);
    else if (st->type == 'G') snprintf(out, n, "  (you have %d / %d)", GetMaterialCount(st->item), st->count);
    else out[0] = 0;
}
int AdvReward(int q) { return (40 + 25 * StepTotal(q)) * (1 + 2 * AdvRarity(q)) * (10 + AdvLevel(q)) / 10 * (2 + Variant(q)) / 2; }
int AdvHereTown(void) { return (village_npc_count > 0 && !in_cave) ? current_screen_index : -1; }
static int ScreenHasType(int type) { for (int i = 0; i < village_npc_count; i++) if (village_npcs[i].type == type) return 1; return 0; }
// Towns without that kind of NPC let a villager or the merchant stand in.
static int RoleMatches(char role, int npc_type) {
    int want = RoleIndex(role);
    return npc_type == want || (!ScreenHasType(want) && (npc_type == 0 || npc_type == 1));
}
static int PickFarTown(int here, int want_castle) {
    int found = -1, seen = 0;
    for (int gy = 0; gy < META_GRID_SIZE; gy++)
        for (int gx = 0; gx < META_GRID_SIZE; gx++) {
            uint8_t p = map_poi[gy][gx];
            if (!(p & (POI_VILLAGE | POI_CASTLE)) || (want_castle && !(p & POI_CASTLE))) continue;
            int key = gy * META_GRID_SIZE + gx + ((p & POI_TOWN_ALT) ? 30000 : 0);
            if (key != here && rand() % ++seen == 0) found = key;
        }
    return found >= 0 ? found : here;
}

static void AdvFinish(int q) {
    int gold = AdvReward(q), r = AdvRarity(q);
    gold_count += gold;
    adv_state[q] = ADV_DONE; adv_prog[q] = 0;
    if (adv_tracked == q) adv_tracked = -1;
    if (r >= 2) KarmaAdjust(2, 0, 0, 2);
    if (r == 3 && CARD_POOL > 0) GainCard(rand() % CARD_POOL);
    snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST COMPLETE: %s (%s)! You earn %d gold%s.", Motif(q)->title, rarity_names[r], gold,
             r == 3 ? ", good karma and a card for your binder" : r == 2 ? " and good karma" : "");
}

static void AdvAdvance(int q) {
    AdvStep s[ADV_STEPS_MAX];
    int n = ParseSteps(q, s), i = adv_state[q] - 1;
    adv_prog[q] = 0;
    if (i >= n) { AdvFinish(q); return; }
    adv_state[q]++;
    char line[200]; StepLine(q, i + 1, line, sizeof(line));
    snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST %s - next: %s", Motif(q)->title, line);
}

static int AdvActive(int q) { return adv_state[q] != 0 && adv_state[q] != ADV_DONE; }
static int offer_q = -1;
static DWORD offer_until_adv;

// X on any town NPC: finish a visit, hand in items, collect a reward, accept
// an offer, or (sometimes) be offered a quest. Returns 1 if it used the press.
int AdvTalk(int npc_type) {
    int here = AdvHereTown(), n = AdvCount();
    if (here < 0 || npc_type < 0 || npc_type > 9) return 0;
    for (int q = 0; q < n; q++) {
        if (!AdvActive(q)) continue;
        AdvStep st; CurStep(q, &st);
        if ((st.type == 'V' && here == adv_far_town[q] && RoleMatches(st.role, npc_type)) ||
            (st.type == 'L' && here == adv_contact_town[q] && RoleMatches(st.role, npc_type))) {
            adv_contact_town[q] = here; adv_contact_role[q] = (uint8_t)npc_type;
            AdvAdvance(q);
            return 1;
        }
        if (here != adv_contact_town[q] || !RoleMatches(role_codes[adv_contact_role[q] % 10], npc_type)) continue;
        if (st.type == 'R') { AdvAdvance(q); return 1; }
        if (st.type == 'G') {
            int have = GetMaterialCount(st.item);
            if (have >= st.count) { ConsumeMaterialFromInventory(st.item, st.count); RecalculateCarriedWeight(); AdvAdvance(q); return 1; }
            if (npc_type == 0) {
                snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST %s: \"Bring me %d %s.\" You have %d.", Motif(q)->title, st.count, st.item, have);
                return 1;
            }
        }
    }
    if (offer_q >= 0 && offer_q < n && GetTickCount() < offer_until_adv && adv_state[offer_q] == 0) {
        int q = offer_q; offer_q = -1;
        adv_state[q] = 1; adv_prog[q] = 0; adv_giver_town[q] = here; adv_contact_town[q] = here; adv_contact_role[q] = (uint8_t)npc_type;
        if (adv_tracked < 0) adv_tracked = q;
        snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST ACCEPTED: %s (%s). It's in the QUESTS tab of your menu.", Motif(q)->title, rarity_names[AdvRarity(q)]);
        return 1;
    }
    offer_q = -1;
    if (npc_type == 0 && village_quest[0]) return 0; // let the villager settle their small job first
    if (rand() % 100 >= (npc_type == 0 ? 30 : 20)) return 0;
    int roll = rand() % 100, want = roll < 60 ? 0 : roll < 85 ? 1 : roll < 97 ? 2 : 3, best = -1, start = rand() % n;
    for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int k = 0; k < n; k++) {
            int q = (start + k) % n, r = AdvRarity(q);
            if (adv_state[q] != 0 || AdvLevel(q) > player_level || RoleIndex(Motif(q)->giver) != npc_type) continue;
            if (pass == 0 ? r != want : r > want) continue;
            best = q; break;
        }
    if (best < 0) return 0;
    adv_far_town[best] = PickFarTown(here, strstr(Motif(best)->steps, "Vk") != NULL);
    adv_contact_town[best] = here; adv_contact_role[best] = (uint8_t)npc_type;
    offer_q = best; offer_until_adv = GetTickCount() + 12000;
    char line[200]; StepLine(best, 0, line, sizeof(line));
    snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST (%s) %s: \"%s\" X again: yes. Walk away: no.", rarity_names[AdvRarity(best)], Motif(best)->title, line);
    return 1;
}

// Kills ('K' + monster), fish ('F') and planted plots ('P') count toward every active quest.
void AdvNote(char kind, int monster) {
    char prev[sizeof(arpg_action_log)];
    for (int q = 0, n = AdvCount(); q < n; q++) {
        if (!AdvActive(q)) continue;
        AdvStep st; CurStep(q, &st);
        if (st.type != kind || (kind == 'K' && st.monster != monster)) continue;
        if (++adv_prog[q] < st.count) continue;
        snprintf(prev, sizeof(prev), "%s", arpg_action_log);
        AdvAdvance(q);
        char now[sizeof(arpg_action_log)]; snprintf(now, sizeof(now), "%s", arpg_action_log);
        char both[sizeof(prev) + sizeof(now) + 8];
        snprintf(both, sizeof(both), "%s  |  %s", prev, now);
        both[sizeof(arpg_action_log) - 1] = 0;
        strcpy(arpg_action_log, both);
    }
}

// Once a second: caves searched, biomes reached, nights spent outside town.
void AdvTick(void) {
    static DWORD last = 0;
    if (GetTickCount() - last < 1000) return;
    last = GetTickCount();
    int h, m; GetGameClock(&h, &m);
    int night = h >= 21 || h < 5, outside = village_npc_count == 0 && !in_cave;
    for (int q = 0, n = AdvCount(); q < n; q++) {
        if (!AdvActive(q)) continue;
        AdvStep st; CurStep(q, &st);
        if ((st.type == 'C' && in_cave) || (st.type == 'B' && !in_cave && (int)current_biome == st.biome) || (st.type == 'N' && night && outside)) {
            AdvAdvance(q);
            return;
        }
    }
}

// --- Quest journal: the QUESTS tab of the menu -------------------------------
static int jr_filter = 0, jr_cursor = 0, abandon_q = -1;
static DWORD abandon_until = 0;
static const char *filter_names[6] = { "ACTIVE", "READY", "HERE", "HUNTS", "ERRANDS", "DONE" };
static int AdvReady(int q) {
    AdvStep st; CurStep(q, &st);
    return st.type == 'R' || (st.type == 'G' && GetMaterialCount(st.item) >= st.count);
}
static int PassFilter(int q) {
    if (jr_filter == 5) return adv_state[q] == ADV_DONE;
    if (!AdvActive(q)) return 0;
    AdvStep st; CurStep(q, &st);
    int here = AdvHereTown();
    switch (jr_filter) {
    case 1: return AdvReady(q);
    case 2: return here >= 0 && (here == adv_far_town[q] || here == adv_contact_town[q] || here == adv_giver_town[q]);
    case 3: return st.type == 'K';
    case 4: return st.type != 'K';
    }
    return 1;
}
// Tracked first, then ready to hand in, then rarest, then in table order.
static int BuildList(int *out) {
    static int key[ADV_MAX];
    int n = 0;
    for (int q = 0, total = AdvCount(); q < total; q++) {
        if (!PassFilter(q)) continue;
        int k = (q == adv_tracked) * 100000 + (adv_state[q] != ADV_DONE && AdvReady(q)) * 10000 + AdvRarity(q) * 1000 + (999 - q);
        int j = n++;
        while (j > 0 && key[j - 1] < k) { key[j] = key[j - 1]; out[j] = out[j - 1]; j--; }
        key[j] = k; out[j] = q;
    }
    return n;
}

void AdvJournalInput(int up, int down, int left, int right, int track, int abandon) {
    static int list[ADV_MAX];
    if (left) { jr_filter = (jr_filter + 5) % 6; jr_cursor = 0; }
    if (right) { jr_filter = (jr_filter + 1) % 6; jr_cursor = 0; }
    int n = BuildList(list);
    if (n == 0) return;
    if (up) jr_cursor = (jr_cursor + n - 1) % n;
    if (down) jr_cursor = (jr_cursor + 1) % n;
    if (jr_cursor >= n) jr_cursor = n - 1;
    int q = list[jr_cursor];
    if (track && AdvActive(q)) {
        adv_tracked = adv_tracked == q ? -1 : q;
        snprintf(arpg_action_log, sizeof(arpg_action_log), adv_tracked == q ? "QUEST: Tracking %s on your HUD." : "QUEST: Stopped tracking %s.", Motif(q)->title);
        n = BuildList(list);
        for (int i = 0; i < n; i++) if (list[i] == q) jr_cursor = i;
    }
    if (abandon && AdvActive(q)) {
        if (abandon_q == q && GetTickCount() < abandon_until) {
            adv_state[q] = 0; adv_prog[q] = 0; abandon_q = -1;
            if (adv_tracked == q) adv_tracked = -1;
            snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST ABANDONED: %s goes back into the world. Someone may ask you again.", Motif(q)->title);
        } else {
            abandon_q = q; abandon_until = GetTickCount() + 3000;
            snprintf(arpg_action_log, sizeof(arpg_action_log), "QUEST: Press again to abandon %s.", Motif(q)->title);
        }
    }
}

void DrawQuestJournal(HDC hdc, int x, int y, int w, int h) {
    static int list[ADV_MAX];
    char buf[320], town[32];
    int n = BuildList(list), active = 0, done = 0, total = AdvCount();
    for (int q = 0; q < total; q++) { if (AdvActive(q)) active++; else if (adv_state[q] == ADV_DONE) done++; }
    if (jr_cursor >= n) jr_cursor = n > 0 ? n - 1 : 0;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 215, 0));
    snprintf(buf, sizeof(buf), "QUEST JOURNAL   Active: %d   Done: %d   Still out in the world: %d", active, done, total - active - done);
    TextOut(hdc, x, y, buf, (int)strlen(buf));
    for (int f = 0; f < 6; f++) {
        RECT r = { x + f * 110, y + 26, x + f * 110 + 104, y + 48 };
        HBRUSH b = CreateSolidBrush(f == jr_filter ? RGB(45, 80, 120) : RGB(30, 35, 45)); FillRect(hdc, &r, b); DeleteObject(b);
        SetTextColor(hdc, f == jr_filter ? RGB(255, 255, 255) : RGB(140, 150, 160));
        DrawText(hdc, filter_names[f], -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
    const char *help = "LEFT/RIGHT: filter   UP/DOWN: pick   A or T: track on HUD   Y or DEL twice: abandon";
    SetTextColor(hdc, RGB(150, 160, 170)); TextOut(hdc, x + 680, y + 30, help, (int)strlen(help));
    if (n == 0) {
        const char *none = jr_filter == 5 ? "No finished quests yet." : "Nothing here. Talk to villagers, shopkeepers, guards and (from level 25) the King to find quests.";
        SetTextColor(hdc, RGB(200, 200, 200)); TextOut(hdc, x, y + 70, none, (int)strlen(none));
        return;
    }
    int rows = (h - 90) / 22, lw = w / 2 - 20;
    if (rows < 5) rows = 5;
    int first = jr_cursor - rows / 2;
    if (first > n - rows) first = n - rows;
    if (first < 0) first = 0;
    for (int r = 0; r < rows && first + r < n; r++) {
        int q = list[first + r], ry = y + 60 + r * 22;
        AdvStep st; int i = CurStep(q, &st);
        TownLabel(adv_giver_town[q], town, sizeof(town));
        if (first + r == jr_cursor) { RECT hl = { x - 4, ry - 1, x + lw, ry + 20 }; HBRUSH b = CreateSolidBrush(RGB(40, 55, 75)); FillRect(hdc, &hl, b); DeleteObject(b); }
        SetTextColor(hdc, rarity_colors[AdvRarity(q)]);
        snprintf(buf, sizeof(buf), "%s%s", q == adv_tracked ? "* " : "", Motif(q)->title);
        TextOut(hdc, x, ry, buf, (int)strlen(buf));
        if (adv_state[q] == ADV_DONE) snprintf(buf, sizeof(buf), "%s  done", town);
        else snprintf(buf, sizeof(buf), "%s  step %d/%d%s", town, i + 1, StepTotal(q), AdvReady(q) ? "  READY" : "");
        SetTextColor(hdc, RGB(160, 170, 180)); TextOut(hdc, x + lw - 250, ry, buf, (int)strlen(buf));
    }
    int q = list[jr_cursor], dx = x + w / 2, dy = y + 60, steps = StepTotal(q), r = AdvRarity(q);
    AdvStep cur; int ci = CurStep(q, &cur);
    if (adv_state[q] == ADV_DONE) ci = steps;
    SetTextColor(hdc, rarity_colors[r]); TextOut(hdc, dx, dy, Motif(q)->title, (int)strlen(Motif(q)->title));
    TownLabel(adv_giver_town[q], town, sizeof(town));
    snprintf(buf, sizeof(buf), "%s  |  %s  |  level %d+  |  from the %s of %s", rarity_names[r], Motif(q)->origin, AdvLevel(q),
             role_names[RoleIndex(Motif(q)->giver)], town);
    SetTextColor(hdc, RGB(190, 200, 210)); TextOut(hdc, dx, dy + 24, buf, (int)strlen(buf));
    int sy = dy + 56;
    for (int k = 0; k < steps; k++) {
        char line[260], prog[48];
        RECT tr = { dx + 36, sy, x + w, sy + 44 };
        if (k < ci) { SetTextColor(hdc, RGB(120, 170, 120)); TextOut(hdc, dx, sy, "[x]", 3); StepLine(q, k, line, sizeof(line)); }
        else if (k == ci) {
            SetTextColor(hdc, RGB(255, 255, 255)); TextOut(hdc, dx + 8, sy, ">", 1);
            StepLine(q, k, line, sizeof(line)); ProgressText(q, &cur, prog, sizeof(prog));
            strncat(line, prog, sizeof(line) - strlen(line) - 1);
        } else { SetTextColor(hdc, RGB(120, 120, 130)); TextOut(hdc, dx, sy, "[ ]", 3); snprintf(line, sizeof(line), "???   (revealed when the step above is done)"); }
        DrawText(hdc, line, -1, &tr, DT_WORDBREAK | DT_LEFT);
        sy += 48;
    }
    snprintf(buf, sizeof(buf), "Reward: %d gold%s", AdvReward(q), r == 3 ? ", good karma and a card" : r == 2 ? " and good karma" : "");
    SetTextColor(hdc, RGB(255, 215, 0)); TextOut(hdc, dx, sy + 6, buf, (int)strlen(buf));
    if (abandon_q == q && GetTickCount() < abandon_until) {
        const char *warn = "Press Y or DEL again to abandon. It goes back into the world for someone to offer again.";
        SetTextColor(hdc, RGB(255, 120, 100)); TextOut(hdc, dx, sy + 30, warn, (int)strlen(warn));
    }
}

void DrawTrackedQuest(HDC hdc, int x, int y) {
    int q = adv_tracked;
    if (q < 0 || q >= AdvCount() || !AdvActive(q)) return;
    AdvStep st; int i = CurStep(q, &st);
    char line[220], prog[48], buf[320];
    StepLine(q, i, line, sizeof(line)); ProgressText(q, &st, prog, sizeof(prog));
    snprintf(buf, sizeof(buf), "Quest: %s - %s%s", Motif(q)->title, line, prog);
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, rarity_colors[AdvRarity(q)]);
    TextOut(hdc, x, y, buf, (int)strlen(buf));
}

int AdvSave(FILE *f) {
    int32_t t = adv_tracked;
    return fwrite(adv_state, sizeof(adv_state), 1, f) == 1 && fwrite(adv_prog, sizeof(adv_prog), 1, f) == 1 &&
           fwrite(adv_giver_town, sizeof(adv_giver_town), 1, f) == 1 && fwrite(adv_far_town, sizeof(adv_far_town), 1, f) == 1 &&
           fwrite(adv_contact_town, sizeof(adv_contact_town), 1, f) == 1 && fwrite(adv_contact_role, sizeof(adv_contact_role), 1, f) == 1 &&
           fwrite(&t, sizeof(t), 1, f) == 1;
}
int AdvLoad(FILE *f) {
    static uint8_t st[ADV_MAX], role[ADV_MAX];
    static uint16_t pr[ADV_MAX];
    static int32_t gt[ADV_MAX], ft[ADV_MAX], ct[ADV_MAX];
    int32_t t;
    if (!(fread(st, sizeof(st), 1, f) == 1 && fread(pr, sizeof(pr), 1, f) == 1 && fread(gt, sizeof(gt), 1, f) == 1 &&
          fread(ft, sizeof(ft), 1, f) == 1 && fread(ct, sizeof(ct), 1, f) == 1 && fread(role, sizeof(role), 1, f) == 1 &&
          fread(&t, sizeof(t), 1, f) == 1)) return 0;
    memcpy(adv_state, st, sizeof(st)); memcpy(adv_prog, pr, sizeof(pr)); memcpy(adv_giver_town, gt, sizeof(gt));
    memcpy(adv_far_town, ft, sizeof(ft)); memcpy(adv_contact_town, ct, sizeof(ct)); memcpy(adv_contact_role, role, sizeof(role));
    for (int q = AdvCount(); q < ADV_MAX; q++) adv_state[q] = 0;
    adv_tracked = (t >= 0 && t < AdvCount() && AdvActive(t)) ? t : -1;
    return 1;
}
