#include "game.h"
#include <stdarg.h>

int town_menu = TM_NONE, town_cursor = 0;
int bank_gold = 0, legendary_quest = 0, legendary_quests_done = 0;
int deity_favor[4] = { 0 };
static int blessing_type = -1;
static DWORD blessing_until = 0;
static int menu_screen = -1, prayed_screen = -1, alms_screen = -1, kid_line = -1;
static char town_note[160] = "";

typedef struct { char text[96]; int action, arg; } TownRow;
enum { A_LEAVE, A_REPAIR, A_FORGE, A_RECYCLE, A_SMITH_BUY, A_CARD, A_PACK, A_KID, A_PET, A_KING,
       A_DEPOSIT, A_WITHDRAW, A_ALMS_GOLD, A_ALMS_FOOD, A_REFUSE, A_PRAY, A_OFFER, A_TAKE, A_STORE, A_PRACTICE, A_GUIDE, A_PET_SIZE, A_PET_COLOR, A_SLEEP, A_FURNISH };
#define TOWN_ROWS 24
static TownRow rows[TOWN_ROWS];
static int row_count = 0;

static const char *deity_names[4] = { "Solara of the Dawn", "Nyx the Veiled", "Baron Samedi", "The Drowned One" };
static const char *deity_kinds[4] = { "Light", "Dark", "Voodoo", "Eldritch" };

typedef struct { const char *name; int price, nocturnal; const char *trait; } FamiliarDef;
static const FamiliarDef familiars[] = {
    { "Dog Familiar",  1500, 0, "Loyal and loud. Trots at your heel all day." },
    { "Cat Familiar",  2500, 1, "Prowls by night, naps in your pack by day." },
    { "Toad Familiar", 1200, 0, "Croaks before rain. Sleeps in your pack at night." },
    { "Hawk Familiar", 6000, 0, "Circles overhead by day, roosts in your pack at night." },
    { "Owl Familiar",  8000, 1, "Silent night watcher. Sleeps in your pack by day." },
};
#define FAMILIAR_COUNT ((int)(sizeof(familiars) / sizeof(familiars[0])))

static const int smith_wares[] = { LOOT_KATANA, LOOT_WAR_HAMMER, LOOT_GREAT_AXE };
#define SMITH_WARE_COUNT ((int)(sizeof(smith_wares) / sizeof(smith_wares[0])))

static void AddRow(int action, int arg, const char *text) {
    if (row_count >= TOWN_ROWS) return;
    rows[row_count].action = action; rows[row_count].arg = arg;
    snprintf(rows[row_count].text, sizeof(rows[row_count].text), "%s", text);
    row_count++;
}

int TownDeityAt(int screen) { return (int)((unsigned int)RainHash(screen * 131 + 7) % 4u); }
int TownDeity(void) { return TownDeityAt(current_screen_index); }
const char *DeityName(int d) { return deity_names[d & 3]; }
const char *DeityKind(int d) { return deity_kinds[d & 3]; }
void TownNameAt(int screen, char *out, size_t n) {
    static const char *a[16] = { "Ash", "Briar", "Cold", "Elder", "Fern", "Gold", "Holly", "Iron", "Mill", "Oak", "Raven", "Stone", "Thorn", "Wolf", "Willow", "Amber" };
    static const char *b[10] = { "ford", "brook", "dale", "haven", "wick", "stead", "moor", "field", "crest", "hollow" };
    unsigned int h = (unsigned int)RainHash(screen * 389 + 11);
    snprintf(out, n, "%s%s", a[h % 16u], b[(h >> 4) % 10u]);
}
// Bit 0: pet shop, bit 1: card shop. They only set up in castle towns, and only some days.
int TownShopsToday(int screen) {
    unsigned int roll = (unsigned int)RainHash(screen * 7919 + SeasonDay());
    return (int)(roll % 2) | (int)(((roll >> 3) % 2) << 1);
}

static int IsNight(void) { int h, m; GetGameClock(&h, &m); return h >= 21 || h < 6; }

static void RemoveItemAt(int idx) {
    for (int i = idx; i < player_item_count - 1; i++) player_inventory[i] = player_inventory[i + 1];
    player_item_count--;
    if (selected_inv_index >= player_item_count) selected_inv_index = player_item_count > 0 ? player_item_count - 1 : 0;
    RecalculateCarriedWeight(); SyncActiveWeapon();
}

static int EquippedMeleeIndex(void) {
    for (int i = 0; i < player_item_count; i++)
        if (player_inventory[i].is_equipped && strcmp(player_inventory[i].slot, "Weapon") == 0 && !strstr(player_inventory[i].name, "Bow")) return i;
    return -1;
}

// Over-forged damage lives on the inventory copy of the weapon: whatever it
// has above its catalog damage is the forge bonus.
int EquippedForgeBonus(void) {
    int i = EquippedMeleeIndex();
    WeaponStats *w = GetEquippedWeaponStats();
    if (i < 0 || !w) return 0;
    int b = player_inventory[i].damage - w->damage;
    return b < 0 ? 0 : b > MAX_FORGE_BONUS ? MAX_FORGE_BONUS : b;
}

static int ForgeCost(int bonus) { return 1000 * (bonus + 1); }

// --- Cards -----------------------------------------------------------------
// Only three sets are in print while the card rules are being revised: Wild
// Creatures (the bestiary monsters plus the animals you meet), Angels and the
// Goetia demons. Every card is a 1/1, 2/2 or 3/3; anything more comes from an
// ability. Ids 0-8 are the monster cards linked to the bestiary.
enum { AB_NONE, AB_FIRST_STRIKE, AB_PUMP3, AB_CURSE, AB_BLESS_BOTH, AB_BLESS_STR, AB_BLESS_LIFE };
typedef struct { const char *name; int set, pt, ability; } CardDef;
static const char *card_sets[3] = { "Wild Creatures", "Angels", "Goetia Demons" };
int card_pool_size = CARD_PRINTED;
static CardDef card_defs[CARD_COUNT] = {
    { "Skeleton", 0, 1, AB_NONE }, { "Zombie", 0, 2, AB_NONE }, { "Dracula", 0, 3, AB_FIRST_STRIKE },
    { "Goblin", 0, 1, AB_FIRST_STRIKE }, { "Satyr", 0, 2, AB_FIRST_STRIKE }, { "Yeti", 0, 3, AB_PUMP3 },
    { "Sphinx", 0, 3, AB_NONE }, { "Bigfoot", 0, 3, AB_PUMP3 }, { "Bunyip", 0, 2, AB_NONE },
    { "Bluebird", 0, 1, AB_FIRST_STRIKE }, { "Squirrel", 0, 1, AB_NONE }, { "Owl", 0, 2, AB_FIRST_STRIKE }, { "Bat", 0, 1, AB_NONE },
    { "Cherub", 1, 1, AB_BLESS_BOTH }, { "Guardian Angel", 1, 1, AB_BLESS_LIFE }, { "Herald Angel", 1, 1, AB_BLESS_STR },
    { "Choir Angel", 1, 1, AB_BLESS_BOTH }, { "Virtue", 1, 2, AB_NONE }, { "Power", 1, 2, AB_FIRST_STRIKE },
    { "Dominion", 1, 2, AB_NONE }, { "Principality", 1, 2, AB_NONE }, { "Throne", 1, 3, AB_NONE },
    { "Seraph", 1, 3, AB_FIRST_STRIKE }, { "Gabriel", 1, 3, AB_NONE }, { "Michael", 1, 3, AB_FIRST_STRIKE },
    { "Vassago", 2, 1, AB_CURSE }, { "Samigina", 2, 1, AB_CURSE }, { "Valefor", 2, 1, AB_CURSE },
    { "Furfur", 2, 1, AB_CURSE }, { "Barbatos", 2, 2, AB_NONE }, { "Marbas", 2, 2, AB_FIRST_STRIKE },
    { "Amon", 2, 2, AB_NONE }, { "Gusion", 2, 2, AB_NONE }, { "Paimon", 2, 3, AB_NONE },
    { "Bael", 2, 3, AB_FIRST_STRIKE }, { "Asmoday", 2, 3, AB_NONE }, { "Belial", 2, 3, AB_FIRST_STRIKE },
};
static int card_offers[10], card_offer_screen = -1, card_offer_day = -1;

static int PoolId(int id) { return id >= 0 && id < CARD_POOL ? id : 0; }

// NewCard lines from the Update folder. Stats are clamped to the rarity cap
// (1 = 1/1, 2 = 2/2, 3 = 3/3) so new cards can't outclass the base set.
int AddRuntimeCard(const char *name, int set, int rarity, int atk, int hp, int ability) {
    static char names[CARD_COUNT - CARD_PRINTED][32];
    if (!name[0]) return 0;
    for (int i = 0; i < card_pool_size; i++) if (_stricmp(card_defs[i].name, name) == 0) return 1;
    if (card_pool_size >= CARD_COUNT) return 0;
    int cap = rarity < 1 ? 1 : rarity > 3 ? 3 : rarity;
    int pt = atk > hp ? atk : hp;
    if (pt > cap) pt = cap;
    if (pt < 1) pt = 1;
    char *slot = names[card_pool_size - CARD_PRINTED];
    snprintf(slot, 32, "%s", name);
    card_defs[card_pool_size++] = (CardDef){ slot, set < 0 || set > 2 ? 0 : set, pt, ability < 0 || ability > AB_BLESS_LIFE ? AB_NONE : ability };
    return 1;
}

void CardName(int id, char *out, size_t n) { snprintf(out, n, "%s", card_defs[PoolId(id)].name); }

static int CardRarity(int id) { return card_defs[PoolId(id)].pt - 1; }
static int CardPrice(int id) { static const int p[3] = { 600, 2500, 15000 }; return p[CardRarity(id)]; }
static int CardAbility(int id) { return card_defs[PoolId(id)].ability; }
// Drops and packs favor small cards: 70% 1/1, 25% 2/2, 5% 3/3.
int RandomPoolCard(int first) {
    int roll = rand() % 100, pt = roll < 70 ? 1 : roll < 95 ? 2 : 3, n = 0;
    for (int i = first; i < CARD_POOL; i++) n += card_defs[i].pt == pt;
    if (n == 0) return first + rand() % (CARD_POOL - first);
    int k = rand() % n;
    for (int i = first; i < CARD_POOL; i++) if (card_defs[i].pt == pt && k-- == 0) return i;
    return first;
}
static int CardBasePower(int id) { int c = PoolId(id); return card_defs[c].pt + (card_defs[c].ability == AB_PUMP3 ? 3 : 0); }

static const char *AbilityText(int ab) {
    switch (ab) {
    case AB_FIRST_STRIKE: return "First Strike: deals its damage before a card without First Strike can hit back.";
    case AB_PUMP3: return "Mighty: +3/+3 (fights as a 6/6).";
    case AB_CURSE: return "Death curse: when it dies, the next enemy card gets -2/-2, which kills a 1/1 or 2/2 before it can fight.";
    case AB_BLESS_BOTH: return "Death blessing: when it dies, your next card gets +1/+1.";
    case AB_BLESS_STR: return "Death blessing: when it dies, your next card gets +2/+0.";
    case AB_BLESS_LIFE: return "Death blessing: when it dies, your next card gets +0/+2.";
    }
    return "No ability.";
}

static void CardDetails(int id, char *out, size_t n) {
    static const char *rar[3] = { "Common", "Uncommon", "Rare" };
    int c = PoolId(id), pt = card_defs[c].pt;
    snprintf(out, n, "%s  |  %s  |  %s  |  %d/%d%s\n%s%s", card_defs[c].name, card_sets[card_defs[c].set], rar[pt - 1], pt, pt,
             HasCard(c) ? "  |  (owned)" : "", AbilityText(card_defs[c].ability),
             c < MONSTER_TYPE_COUNT ? "  Monster card: beat enough of its kind and it may drop." : "");
}

static void RollCardOffers(void) {
    int day = SeasonDay();
    if (card_offer_screen == current_screen_index && card_offer_day == day) return;
    card_offer_screen = current_screen_index; card_offer_day = day;
    for (int k = 0; k < 10; k++) {
        int id, dup, salt = 0;
        do {
            id = (int)((unsigned int)RainHash(current_screen_index * 977 + day * 131 + k * 17 + salt++) % (unsigned int)CARD_POOL);
            dup = 0;
            for (int j = 0; j < k; j++) if (card_offers[j] == id) dup = 1;
        } while (dup);
        card_offers[k] = id;
    }
}

static const char *kid_lines[] = {
    "KID: Wanna learn? It's three on three. You pick the order you lay your three; I lead with mine as dealt.",
    "KID: Every card is a 1/1, 2/2 or 3/3. Bigger numbers come from abilities - Yeti and Bigfoot are 3/3 with +3/+3.",
    "KID: Both cards hit at the same time, like a real stack. Unless one has First Strike - then it hits first.",
    "KID: Don't just lead with your biggest! A Demon 1/1 that dies curses my next card -2/-2. An Angel blesses yours.",
    "KID: Monster cards drop from the beasts themselves. Beat enough of one kind and you might get its card.",
    "KID: Some kids play for ante - each puts a card in the pot and the winner keeps both. Not me.",
    "KID: Right now it's just Angels, Goetia demons and the creatures out in the wild. More sets are coming!",
};
#define KID_LINE_COUNT ((int)(sizeof(kid_lines) / sizeof(kid_lines[0])))

static void Say(const char *fmt, ...);

// Practice hands with the card kid: no ante, ten rounds per visit. Duels
// resolve like a stack: damage lands at the same time unless one card has
// First Strike, and a card's death mark lands on the next card to enter.
#define KID_MAX_ROUNDS 10
#define KID_LOG_LINES 14
static int kid_rounds = 0, kid_round_screen = -1, kid_dealt = 0, kid_played = 0;
static int my_hand[3], kid_hand[3], my_order[3] = { 0, 1, 2 }, duel_result[3];
static const int hand_orders[6][3] = { {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0} };
static char kid_log[KID_LOG_LINES][110];
static int kid_log_n = 0, pend_p[2], pend_t[2];

static void KidLog(const char *fmt, ...) {
    if (kid_log_n >= KID_LOG_LINES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(kid_log[kid_log_n++], sizeof(kid_log[0]), fmt, ap);
    va_end(ap);
}

static void DeathMark(int side, int id) {
    char nm[48]; CardName(id, nm, sizeof(nm));
    const char *who = side ? "His" : "Your";
    int ab = CardAbility(id), bp = ab == AB_BLESS_STR ? 2 : ab == AB_BLESS_BOTH ? 1 : 0, bt = ab == AB_BLESS_LIFE ? 2 : ab == AB_BLESS_BOTH ? 1 : 0;
    if (ab == AB_CURSE) {
        pend_p[!side] -= 2; pend_t[!side] -= 2;
        KidLog("   %s %s dies and curses %s next card -2/-2.", who, nm, side ? "your" : "his");
    } else if (bp || bt) {
        pend_p[side] += bp; pend_t[side] += bt;
        KidLog("   %s %s dies and blesses %s next card +%d/+%d.", who, nm, side ? "his" : "your", bp, bt);
    }
}

static void Duel(int k, int a, int b, int *mine, int *his) {
    char an[48], bn[48]; CardName(a, an, sizeof(an)); CardName(b, bn, sizeof(bn));
    int ap = CardBasePower(a) + pend_p[0], at = CardBasePower(a) + pend_t[0];
    int bp = CardBasePower(b) + pend_p[1], bt = CardBasePower(b) + pend_t[1];
    pend_p[0] = pend_t[0] = pend_p[1] = pend_t[1] = 0;
    if (ap < 0) ap = 0;
    if (bp < 0) bp = 0;
    KidLog("%d. Your %s %d/%d  vs  his %s %d/%d", k + 1, an, ap, at, bn, bp, bt);
    int adead = at <= 0, bdead = bt <= 0;
    if (adead || bdead) {
        if (adead) KidLog("   Your %s withers under the curse before it can fight.", an);
        if (bdead) KidLog("   His %s withers under the curse before it can fight.", bn);
    } else {
        int afs = CardAbility(a) == AB_FIRST_STRIKE, bfs = CardAbility(b) == AB_FIRST_STRIKE;
        if (afs && !bfs) { bt -= ap; if (bt > 0) at -= bp; KidLog("   Your %s strikes first.", an); }
        else if (bfs && !afs) { at -= bp; if (at > 0) bt -= ap; KidLog("   His %s strikes first.", bn); }
        else { at -= bp; bt -= ap; }
        adead = at <= 0; bdead = bt <= 0;
        KidLog("   %s", adead && bdead ? "Both cards fall." : adead ? "Your card falls." : bdead ? "His card falls." : "Both cards survive.");
    }
    if (adead) DeathMark(0, a);
    if (bdead) DeathMark(1, b);
    *mine += bdead; *his += adead;
    duel_result[k] = bdead - adead;
}

static void PracticeHand(int order) {
    if (kid_round_screen != current_screen_index) { kid_round_screen = current_screen_index; kid_rounds = 0; kid_dealt = 0; kid_played = 0; }
    if (kid_rounds >= KID_MAX_ROUNDS) { Say("KID: That's ten - I gotta go home now. Mom's calling!"); return; }
    if (!order) {
        for (int k = 0; k < 3; k++) {
            int id, dup;
            do { id = card_offers[rand() % 10]; dup = 0; for (int j = 0; j < k; j++) if (my_hand[j] == id) dup = 1; } while (dup);
            my_hand[k] = id; kid_hand[k] = rand() % CARD_POOL; my_order[k] = k;
        }
        kid_dealt = 1; kid_played = 0; kid_log_n = 0;
        Say("KID: Here's a test hand from the counter. Pick the order you'll lay yours - I lead with mine left to right.");
        return;
    }
    const int *o = hand_orders[order - 1];
    int mine = 0, his = 0;
    kid_log_n = 0; pend_p[0] = pend_p[1] = pend_t[0] = pend_t[1] = 0;
    for (int k = 0; k < 3; k++) { my_order[k] = o[k]; Duel(k, my_hand[o[k]], kid_hand[k], &mine, &his); }
    kid_dealt = 0; kid_played = 1; kid_rounds++;
    const char *end = kid_rounds >= KID_MAX_ROUNDS ? " That's ten rounds - I have to go home soon!" : "";
    if (mine > his) Say("KID: You knocked out %d to my %d - you win this one! Order matters, huh?%s", mine, his, end);
    else if (mine < his) Say("KID: I knocked out %d to your %d. Try a different order.%s", his, mine, end);
    else Say("KID: %d to %d, a draw!%s", mine, his, end);
}

// --- Menu rows ---------------------------------------------------------------
static void AddLeave(void) { row_count = row_count >= TOWN_ROWS ? TOWN_ROWS - 1 : row_count; AddRow(A_LEAVE, 0, "Leave"); }

int familiar_size = 1, familiar_color = 0; // picked at the pet shop: small/medium/large, coat 0..3

static int FamiliarIndex(const char *name) {
    for (int k = 0; k < FAMILIAR_COUNT; k++) if (strcmp(name, familiars[k].name) == 0) return k;
    return -1;
}

static int OwnedFamiliarSlot(void) {
    for (int i = 0; i < player_item_count; i++) if (strcmp(player_inventory[i].slot, "Familiar") == 0) return i;
    return -1;
}

static void BuildRows(void) {
    char t[96], nm[48];
    row_count = 0;
    if (town_menu == TM_HOME) {
        AddRow(A_SLEEP, 0, inside_home == 4 ? "Sleep in your bed" : "Rest in the guest bed");
        if (inside_home == 4) AddRow(A_FURNISH, 0, "Set out new furniture");
    }
    if (town_menu == TM_SMITH) {
        for (int i = 0; i < player_item_count; i++) {
            InventoryItem *it = &player_inventory[i];
            if (it->durability_max <= 0 || it->durability_max >= 99999 || it->durability_current >= it->durability_max) continue;
            snprintf(t, sizeof(t), "Repair %s (%d/%d) - %d gold", it->name, it->durability_current, it->durability_max,
                     ShopPrice(it->durability_max - it->durability_current));
            AddRow(A_REPAIR, i, t);
        }
        int m = EquippedMeleeIndex();
        if (m >= 0 && GetEquippedWeaponStats() && strcmp(player_inventory[m].name, "Musashi's Blade") != 0) {
            int b = EquippedForgeBonus();
            if (b < MAX_FORGE_BONUS) snprintf(t, sizeof(t), "Over-forge %s +%d -> +%d - %d gold", player_inventory[m].name, b, b + 1, ShopPrice(ForgeCost(b)));
            else snprintf(t, sizeof(t), "%s is forged to the limit (+%d)", player_inventory[m].name, b);
            AddRow(A_FORGE, m, t);
        }
        for (int k = 0; k < SMITH_WARE_COUNT; k++) {
            WeaponStats *w = &weapon_catalog[smith_wares[k] - 1];
            snprintf(t, sizeof(t), "Buy %s (Rare, DMG %d) - %d gold", w->name, w->damage, w->price);
            AddRow(A_SMITH_BUY, k, t);
        }
        for (int i = 0; i < player_item_count; i++) {
            InventoryItem *it = &player_inventory[i];
            if (it->is_equipped || strcmp(it->name, "Musashi's Blade") == 0) continue;
            if (strcmp(it->slot, "Weapon") && strcmp(it->slot, "Armor") && strcmp(it->slot, "Shield")) continue;
            snprintf(t, sizeof(t), "Recycle %s into materials", it->name);
            AddRow(A_RECYCLE, i, t);
        }
    } else if (town_menu == TM_CARDS) {
        RollCardOffers();
        for (int k = 0; k < 10; k++) {
            CardName(card_offers[k], nm, sizeof(nm));
            snprintf(t, sizeof(t), "%s%s - %d gold", nm, HasCard(card_offers[k]) ? " (owned)" : "", CardPrice(card_offers[k]));
            AddRow(A_CARD, k, t);
        }
        AddRow(A_PACK, 3, "3-card booster - 20,000 gold");
        AddRow(A_PACK, 7, "7-card pack - 40,000 gold");
        AddRow(A_PACK, 60, "60-card box - 350,000 gold");
        snprintf(t, sizeof(t), "Master Base Box (all %d cards) - 1,000,000 gold", CARD_POOL);
        AddRow(A_PACK, 0, t);
        AddRow(A_GUIDE, 0, "A&D Ultimate Strategy Guide - 1,000,000 gold");
        AddRow(A_KID, 0, "Talk to the kid at the table");
        int left = kid_round_screen == current_screen_index ? KID_MAX_ROUNDS - kid_rounds : KID_MAX_ROUNDS;
        if (kid_dealt && kid_round_screen == current_screen_index) {
            for (int p = 0; p < 6; p++) {
                const int *o = hand_orders[p];
                char n0[28], n1[28], n2[28];
                CardName(my_hand[o[0]], n0, sizeof(n0)); CardName(my_hand[o[1]], n1, sizeof(n1)); CardName(my_hand[o[2]], n2, sizeof(n2));
                snprintf(t, sizeof(t), "Lay: %s > %s > %s", n0, n1, n2);
                AddRow(A_PRACTICE, 1 + p, t);
            }
        } else if (left > 0) {
            snprintf(t, sizeof(t), "Play the kid a practice hand (%d of %d left, no ante)", left, KID_MAX_ROUNDS);
            AddRow(A_PRACTICE, 0, t);
        }
    } else if (town_menu == TM_PETS) {
        for (int k = 0; k < FAMILIAR_COUNT; k++) {
            snprintf(t, sizeof(t), "%s - %d gold%s", familiars[k].name, familiars[k].price,
                     player_level < FAMILIAR_UNLOCK_LEVEL ? "  (needs level 3)" : "");
            AddRow(A_PET, k, t);
        }
        static const char *sizes[3] = { "Small", "Medium", "Large" };
        snprintf(t, sizeof(t), "Familiar size: %s (press to change)", sizes[familiar_size]); AddRow(A_PET_SIZE, 0, t);
        snprintf(t, sizeof(t), "Familiar coat: %d of 4 (press to change)", familiar_color + 1); AddRow(A_PET_COLOR, 0, t);
    } else if (town_menu == TM_KING) {
        AddRow(A_KING, 0, legendary_quest == 2 ? "Report your Legendary victory" :
                          legendary_quest == 1 ? "Ask about your Legendary quest" : "Ask the King for a Legendary quest");
    } else if (town_menu == TM_BANK) {
        AddRow(A_DEPOSIT, 100, "Deposit 100 gold");
        AddRow(A_DEPOSIT, 1000, "Deposit 1,000 gold");
        AddRow(A_DEPOSIT, -1, "Deposit all gold");
        AddRow(A_WITHDRAW, 100, "Withdraw 100 gold");
        AddRow(A_WITHDRAW, 1000, "Withdraw 1,000 gold");
        AddRow(A_WITHDRAW, -1, "Withdraw everything");
    } else if (town_menu == TM_BEGGAR) {
        AddRow(A_ALMS_GOLD, 10, "Give 10 gold");
        AddRow(A_ALMS_GOLD, 100, "Give 100 gold");
        AddRow(A_ALMS_FOOD, 0, "Give some food");
        AddRow(A_REFUSE, 0, "Refuse and walk on");
    } else if (town_menu == TM_CHEST && open_chest >= 0) {
        StashChest *c = &chests[open_chest];
        for (int k = 0; k < c->count; k++) { snprintf(t, sizeof(t), "Take %s x%d", c->items[k].name, c->items[k].quantity); AddRow(A_TAKE, k, t); }
        for (int i = 0; i < player_item_count && c->count < CHEST_SLOTS; i++) {
            if (player_inventory[i].is_equipped) continue;
            snprintf(t, sizeof(t), "Store %s x%d", player_inventory[i].name, player_inventory[i].quantity); AddRow(A_STORE, i, t);
        }
    } else if (town_menu == TM_ALTAR) {
        static const char *offer[4] = { "Offer Incense", "Offer a Chaos Relic", "Offer Tarot Card, Rum Bottle and Cigar", "Offer a Fish from the deep" };
        int d = TownDeity();
        snprintf(t, sizeof(t), "Pray to %s", deity_names[d]);
        AddRow(A_PRAY, d, t);
        AddRow(A_OFFER, d, offer[d]);
    }
    AddLeave();
    if (town_cursor >= row_count) town_cursor = row_count - 1;
    if (town_cursor < 0) town_cursor = 0;
}

static void RowInfo(const TownRow *r, char *out, size_t n) {
    out[0] = 0;
    switch (r->action) {
    case A_REPAIR: snprintf(out, n, "Mends %s back to full durability. Cost is 1 gold per missing point, less with charisma.", player_inventory[r->arg].name); break;
    case A_FORGE: snprintf(out, n, "Each forge adds +1 damage, up to +%d, but the metal grows brittle: max durability drops 10%% every time.", MAX_FORGE_BONUS); break;
    case A_SMITH_BUY: { WeaponStats *w = &weapon_catalog[smith_wares[r->arg] - 1];
        snprintf(out, n, "%s  |  DMG %d  |  Durability %d  |  Weight %.1f  |  Rare smith work you won't find at the merchant.", w->name, w->damage, w->durability_max, w->weight); break; }
    case A_RECYCLE: { const char *nm = player_inventory[r->arg].name, *slot = player_inventory[r->arg].slot;
        snprintf(out, n, "Breaks the %s down into %s.", nm, strstr(nm, "Bow") ? "2 Wood and 1 String" :
                 strcmp(slot, "Armor") == 0 ? "2 Leather and 1 Iron" : strcmp(slot, "Shield") == 0 ? "2 Wood and 1 Rope" : "2 Iron and 1 Wood"); break; }
    case A_CARD: CardDetails(card_offers[r->arg], out, n); break;
    case A_PACK:
        if (r->arg == 0) snprintf(out, n, "One of every card in print. Completes your binder. Collection: %d / %d.", CardCount(), CARD_POOL);
        else snprintf(out, n, "%d random cards. Collection: %d / %d.", r->arg, CardCount(), CARD_POOL);
        break;
    case A_GUIDE: snprintf(out, n, "Secret card combos: read it from your pack for a 2-second peek. Stealing it means jail."); break;
    case A_KID: snprintf(out, n, "A kid sits at the corner table shuffling a test deck. He won't play for ante."); break;
    case A_PRACTICE: snprintf(out, n, "Three duels, one card each. Both cards hit at once unless one has First Strike; death curses and blessings land on the next card. His cards and yours are on the right."); break;
    case A_SLEEP: snprintf(out, n, "Sleep until the next morning or evening. You wake rested."); break;
    case A_FURNISH: snprintf(out, n, "Furniture bought from the merchant is set out in your home."); break;
    case A_PET_SIZE: case A_PET_COLOR: snprintf(out, n, "Choose the size and coat colour of your dog or cat. Change it here any time."); break;
    case A_PET: snprintf(out, n, "%s  |  %s  |  %s", familiars[r->arg].name, familiars[r->arg].nocturnal ? "Nocturnal" : "Diurnal", familiars[r->arg].trait); break;
    case A_KING: snprintf(out, n, "Legendary quests open at level %d. Quests completed: %d.", LEGENDARY_QUEST_LEVEL, legendary_quests_done); break;
    case A_DEPOSIT: case A_WITHDRAW: snprintf(out, n, "Vault balance: %d gold. Purse: %d gold.", bank_gold, gold_count); break;
    case A_ALMS_GOLD: case A_ALMS_FOOD: snprintf(out, n, "Kindness raises your Soul, cools Town Heat and pleases the Light."); break;
    case A_REFUSE: snprintf(out, n, "Turning away the needy chills the town and draws the Dark's attention."); break;
    case A_PRAY: snprintf(out, n, "A %s shrine to %s. Your favor here: %d.", deity_kinds[r->arg], deity_names[r->arg], deity_favor[r->arg]); break;
    case A_OFFER: snprintf(out, n, "Merchants sell offerings. The gods answer gifts with blessings."); break;
    default: break;
    }
}

// --- Actions -----------------------------------------------------------------
static int alms_day = -1, pray_day = -1;

static void Say(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt); vsnprintf(town_note, sizeof(town_note), fmt, ap); va_end(ap);
    snprintf(arpg_action_log, sizeof(arpg_action_log), "%s", town_note);
}

static const char *greetings[10] = { "",
    "BLACKSMITH: Steel needs mending, or something finer? Pick a job.",
    "CARD SHOP: Ten fresh cards on the counter today. The kid at the table can teach you.",
    "PET SHOP: Every familiar here needs a seasoned keeper.",
    "KING: Approach, adventurer.",
    "BANKER: Your gold is safe in the vault.",
    "BEGGAR: Spare a coin or a crust, kind traveler?",
    "",
    "CHEST: Pick an item to take out, or something from your pack to store.",
    "HOME: \"Come in, come in! Mind the hearth.\"" };

void OpenTownMenu(int menu) {
    town_menu = menu; town_cursor = 0; menu_screen = current_screen_index;
    vendor_menu_open = 0; town_note[0] = 0;
    BuildRows();
    if (menu == TM_ALTAR) Say("ALTAR: A %s shrine to %s. The air grows still as you kneel.", deity_kinds[TownDeity()], deity_names[TownDeity()]);
    else Say("%s", greetings[menu]);
}

void TownMenuMove(int step) { BuildRows(); if (row_count > 0) town_cursor = (town_cursor + step + row_count) % row_count; }
void TownMenuBack(void) { int was_home = town_menu == TM_HOME; town_menu = TM_NONE; if (was_home) LeaveHome(); }

float TownDamageMult(void) {
    if (blessing_type < 0 || GetTickCount() > blessing_until) return 1.0f;
    return blessing_type == 1 ? 1.5f : 1.25f;
}

static void Bless(int type) { blessing_type = type; blessing_until = GetTickCount() + 5 * 60 * 1000; }

void NoteLegendaryKill(int monster) {
    if (legendary_quest != 1 || !IS_BOSS_MONSTER(monster)) return;
    legendary_quest = 2;
    size_t n = strlen(arpg_action_log);
    snprintf(arpg_action_log + n, sizeof(arpg_action_log) - n, "  |  LEGENDARY: Return to the King!");
}

#define MASTER_BOX_PRICE 1000000
static void BuyCardPack(int count) {
    if (count == 0) {
        if (CardCount() >= CARD_POOL) { Say("CARD SHOP: You already own every card in print."); return; }
        if (gold_count < MASTER_BOX_PRICE) { Say("CARD SHOP: The Master Base Box costs %d gold.", MASTER_BOX_PRICE); return; }
        gold_count -= MASTER_BOX_PRICE;
        for (int id = 0; id < CARD_POOL; id++) card_collection_bits[id / 8] |= (uint8_t)(1u << (id % 8));
        Say("CARD SHOP: The Master Base Box! Your binder is complete (%d / %d).", CardCount(), CARD_POOL);
        return;
    }
    int price = count == 3 ? CARD_BOOSTER_PRICE : count == 7 ? 40000 : 350000;
    if (gold_count < price) { Say("CARD SHOP: That costs %d gold.", price); return; }
    gold_count -= price;
    int fresh = 0;
    for (int k = 0; k < count; k++) {
        int id = RandomPoolCard(0);
        if (!HasCard(id)) fresh++;
        card_collection_bits[id / 8] |= (uint8_t)(1u << (id % 8));
    }
    Say("CARD SHOP: Opened %d cards - %d new! (%d / %d)", count, fresh, CardCount(), CARD_POOL);
}

static int TakeOffering(const char *a, const char *b, const char *c) {
    const char *need[3] = { a, b, c };
    for (int k = 0; k < 3; k++) if (need[k] && GetMaterialCount(need[k]) < 1) return 0;
    for (int k = 0; k < 3; k++) if (need[k]) ConsumeMaterialFromInventory(need[k], 1);
    RecalculateCarriedWeight();
    return 1;
}

static void Offer(int d) {
    const char *hint = world_hints[rand() % WORLD_HINT_COUNT];
    if (d == 0) {
        if (!TakeOffering("Incense", NULL, NULL)) { Say("ALTAR: Solara asks for Incense. The merchant sells it."); return; }
        player_hp = max_player_hp; KarmaAdjust(3, 0, -1, 0);
        Say("ALTAR: Dawn's Grace! Solara heals your wounds and lifts your Soul.");
    } else if (d == 1) {
        if (!TakeOffering("Chaos Relic", NULL, NULL)) { Say("ALTAR: Nyx wants a Chaos Relic. Merchants keep a few."); return; }
        Bless(1); KarmaAdjust(-3, 2, 0, 0);
        Say("ALTAR: Night's Edge! Nyx sharpens your strikes (+50%% damage for 5 minutes).");
    } else if (d == 2) {
        if (!TakeOffering("Tarot Card", "Rum Bottle", "Cigar")) { Say("ALTAR: The Baron wants a Tarot Card, a Rum Bottle and a Cigar."); return; }
        Bless(2); KarmaAdjust(0, 0, 0, 2);
        Say("ALTAR: Baron Samedi appears behind you with a toothy grin. Ghost Whisperer: +25%% damage for 5 min. '%s'", hint);
    } else {
        if (!TakeOffering("Fish", NULL, NULL)) { Say("ALTAR: The Drowned One hungers for a Fish from the deep."); return; }
        KarmaAdjust(0, 1, 0, 0);
        if (rand() % 2) { Bless(1); Say("ALTAR: The Drowned One accepts. Strange strength floods you (+50%% damage for 5 minutes)."); }
        else Say("ALTAR: The Drowned One accepts, and something vast whispers: '%s'", hint);
    }
    deity_favor[d] += 3;
}

void TownMenuConfirm(void) {
    BuildRows();
    if (row_count <= 0) return;
    TownRow r = rows[town_cursor];
    switch (r.action) {
    case A_LEAVE: TownMenuBack(); return;
    case A_SLEEP: sleep_fade_frame = 1; Say("HOME: You sleep soundly in the %s bed.", inside_home == 4 ? "warm" : "cottage"); return;
    case A_FURNISH: if (PlaceFurniture()) Say("%s", arpg_action_log); else Say("HOME: Nothing new to set out. The merchant sells furniture."); return;
    case A_REPAIR: {
        InventoryItem *it = &player_inventory[r.arg];
        int cost = ShopPrice(it->durability_max - it->durability_current);
        if (gold_count < cost) { Say("BLACKSMITH: Mending your %s costs %d gold.", it->name, cost); return; }
        gold_count -= cost; it->durability_current = it->durability_max;
        Say("BLACKSMITH: %s repaired for %d gold.", it->name, cost);
        return; }
    case A_FORGE: {
        InventoryItem *it = &player_inventory[r.arg];
        WeaponStats *w = GetEquippedWeaponStats();
        int b = EquippedForgeBonus();
        if (!w || b >= MAX_FORGE_BONUS) { Say("BLACKSMITH: Any more heat and that %s will shatter.", it->name); return; }
        int cost = ShopPrice(ForgeCost(b));
        if (gold_count < cost) { Say("BLACKSMITH: Forging it to +%d costs %d gold.", b + 1, cost); return; }
        gold_count -= cost;
        it->damage = w->damage + b + 1;
        it->durability_max = it->durability_max * 9 / 10;
        if (it->durability_max < 10) it->durability_max = 10;
        if (it->durability_current > it->durability_max) it->durability_current = it->durability_max;
        Say("BLACKSMITH: Your %s is now +%d, but more brittle (max durability %d).", it->name, b + 1, it->durability_max);
        return; }
    case A_SMITH_BUY: {
        WeaponStats *w = &weapon_catalog[smith_wares[r.arg] - 1];
        if (gold_count < w->price) { Say("BLACKSMITH: The %s is %d gold. Worth every coin.", w->name, w->price); return; }
        if (!AddLootToInventory(smith_wares[r.arg], 1)) { Say("BLACKSMITH: Your pack is too full or heavy for a %s.", w->name); return; }
        gold_count -= w->price;
        Say("BLACKSMITH: Sold! The %s is yours. Equip it from your pack.", w->name);
        return; }
    case A_RECYCLE: {
        char nm[32], slot[32];
        snprintf(nm, sizeof(nm), "%s", player_inventory[r.arg].name);
        snprintf(slot, sizeof(slot), "%s", player_inventory[r.arg].slot);
        const char *a = "Iron", *b = "Wood";
        if (strstr(nm, "Bow")) { a = "Wood"; b = "String"; }
        else if (strcmp(slot, "Armor") == 0) { a = "Leather"; b = "Iron"; }
        else if (strcmp(slot, "Shield") == 0) { a = "Wood"; b = "Rope"; }
        RemoveItemAt(r.arg);
        AddStackableItem(a, "Material", 2); AddStackableItem(b, "Material", 1);
        RecalculateCarriedWeight();
        Say("BLACKSMITH: Broke the %s down into 2 %s and 1 %s.", nm, a, b);
        return; }
    case A_CARD: {
        int id = card_offers[r.arg], price = CardPrice(id);
        char nm[48]; CardName(id, nm, sizeof(nm));
        if (HasCard(id)) { Say("CARD SHOP: You already own the %s card.", nm); return; }
        if (gold_count < price) { Say("CARD SHOP: The %s card is %d gold.", nm, price); return; }
        gold_count -= price;
        card_collection_bits[id / 8] |= (uint8_t)(1u << (id % 8));
        Say("CARD SHOP: Added %s to your collection. (%d / %d)", nm, CardCount(), CARD_POOL);
        return; }
    case A_PACK: BuyCardPack(r.arg); return;
    case A_GUIDE:
        if (HasInventoryItem("Strategy Guide")) { Say("CARD SHOP: You already own the A&D Strategy Guide."); return; }
        if (gold_count < 1000000) { Say("CARD SHOP: The A&D Strategy Guide costs 1,000,000 gold."); return; }
        gold_count -= 1000000;
        AddStackableItem("Strategy Guide", "Tool", 1); RecalculateCarriedWeight();
        guide_flash_until = GetTickCount() + GUIDE_FLASH_MS;
        Say("CARD SHOP: The A&D Ultimate Strategy Guide is yours. Read it from your pack.");
        return;
    case A_KID: kid_line = (kid_line + 1) % KID_LINE_COUNT; Say("%s", kid_lines[kid_line]); return;
    case A_PRACTICE: PracticeHand(r.arg); return;
    case A_PET_SIZE: familiar_size = (familiar_size + 1) % 3; Say("PET SHOP: A %s companion it is.", familiar_size == 0 ? "small" : familiar_size == 1 ? "medium" : "large"); return;
    case A_PET_COLOR: familiar_color = (familiar_color + 1) % 4; Say("PET SHOP: Coat %d of 4 - a fine look.", familiar_color + 1); return;
    case A_PET: {
        const FamiliarDef *f = &familiars[r.arg];
        if (player_level < FAMILIAR_UNLOCK_LEVEL) { Say("PET SHOP: Familiars only bond with heroes of level %d or higher.", FAMILIAR_UNLOCK_LEVEL); return; }
        if (OwnedFamiliarSlot() >= 0) { Say("PET SHOP: You already have a companion. One familiar at a time."); return; }
        if (gold_count < f->price) { Say("PET SHOP: The %s costs %d gold.", f->name, f->price); return; }
        if (!AddInventoryItem(f->name, "Familiar", 1)) { Say("PET SHOP: Your pack is full."); return; }
        player_inventory[player_item_count - 1].weight = 0.0f;
        gold_count -= f->price;
        RecalculateCarriedWeight();
        Say("PET SHOP: The %s is yours! %s", f->name, f->trait);
        return; }
    case A_KING:
        if (player_level < LEGENDARY_QUEST_LEVEL) Say("KING: Legendary deeds are not for the untested. Return when you reach level %d.", LEGENDARY_QUEST_LEVEL);
        else if (legendary_quest == 0) { legendary_quest = 1; Say("KING: A Legendary quest! Slay a mythic beast - Yeti, Sphinx, Bigfoot, Bunyip - or Dracula himself, then return to me."); }
        else if (legendary_quest == 1) Say("KING: The beast still lives. Seek it in the wild lands beyond these walls.");
        else {
            legendary_quest = 0; legendary_quests_done++; gold_count += 10000; KarmaAdjust(5, 0, -5, 10);
            Say("KING: The realm is in your debt! Take 10,000 gold and the people's love. (Legendary quests: %d)", legendary_quests_done);
        }
        return;
    case A_DEPOSIT: case A_WITHDRAW: {
        int dep = r.action == A_DEPOSIT, have = dep ? gold_count : bank_gold;
        int amt = r.arg < 0 ? have : (r.arg < have ? r.arg : have);
        if (amt <= 0) { Say(dep ? "BANKER: Your purse is empty." : "BANKER: Your vault is empty."); return; }
        if (dep) { gold_count -= amt; bank_gold += amt; } else { bank_gold -= amt; gold_count += amt; }
        Say("BANKER: %s %d gold. Vault: %d   Purse: %d", dep ? "Deposited" : "Withdrew", amt, bank_gold, gold_count);
        return; }
    case A_ALMS_GOLD: case A_ALMS_FOOD: case A_REFUSE:
      { // Give as often as you like; karma and favor count once a day per town.
        int rewarded = alms_screen == current_screen_index && alms_day == SeasonDay();
        if (r.action == A_ALMS_GOLD) {
            if (gold_count < r.arg) { Say("BEGGAR: You haven't that much to spare, friend."); return; }
            gold_count -= r.arg;
            if (!rewarded) { KarmaAdjust(r.arg >= 100 ? 3 : 1, 0, r.arg >= 100 ? -3 : -1, 1); deity_favor[0] += r.arg >= 100 ? 2 : 1; }
            Say("BEGGAR: Bless you, traveler! This town will remember your kindness.");
        } else if (r.action == A_ALMS_FOOD) {
            int fi = -1;
            for (int i = 0; i < player_item_count && fi < 0; i++) if (strcmp(player_inventory[i].slot, "Food") == 0) fi = i;
            if (fi < 0) { Say("BEGGAR: You've no food on you. Bless you for the thought."); return; }
            char nm[32]; snprintf(nm, sizeof(nm), "%s", player_inventory[fi].name);
            ConsumeMaterialFromInventory(nm, 1); RecalculateCarriedWeight();
            if (!rewarded) { KarmaAdjust(2, 0, -2, 1); deity_favor[0]++; }
            Say("BEGGAR: A %s! Bless you - I haven't eaten in days.", nm);
        } else {
            if (!rewarded) { KarmaAdjust(-1, 1, 2, 0); deity_favor[1]++; }
            Say("BEGGAR: ...I see. The townsfolk watching mutter and look away.");
        }
        alms_screen = current_screen_index; alms_day = SeasonDay();
        return; }
    case A_PRAY: {
        int d = r.arg;
        if (prayed_screen == current_screen_index && pray_day == SeasonDay()) { Say("ALTAR: %s has already heard you today.", deity_names[d]); return; }
        prayed_screen = current_screen_index; pray_day = SeasonDay(); deity_favor[d]++;
        if (d == 0) KarmaAdjust(1, 0, 0, 0); else if (d == 1) KarmaAdjust(0, 1, 0, 0); else if (d == 2) KarmaAdjust(0, 0, 0, 1);
        static const char *answer[4] = { "Warm light settles on your shoulders.", "Shadows lean close, curious.",
                                          "Somewhere, a man in a top hat laughs.", "Far below, something vast turns over in its sleep." };
        Say("ALTAR: You pray to %s. %s (favor %d)", deity_names[d], answer[d], deity_favor[d]);
        return; }
    case A_OFFER: Offer(r.arg); return;
    case A_TAKE: {
        StashChest *c = &chests[open_chest];
        if (player_item_count >= MAX_INVENTORY_ITEMS) { Say("CHEST: Your pack is full."); return; }
        InventoryItem it = c->items[r.arg];
        if (IsStackableItem(it.name, it.slot)) { if (!AddStackableItem(it.name, it.slot, it.quantity)) { Say("CHEST: No room in your pack."); return; } }
        else player_inventory[player_item_count++] = it;
        c->items[r.arg] = c->items[--c->count];
        RecalculateCarriedWeight();
        Say("CHEST: Took %s.", it.name);
        return; }
    case A_STORE: {
        StashChest *c = &chests[open_chest];
        if (c->count >= CHEST_SLOTS) { Say("CHEST: The chest is full."); return; }
        c->items[c->count++] = player_inventory[r.arg];
        char nm[32]; snprintf(nm, sizeof(nm), "%s", player_inventory[r.arg].name);
        RemoveItemAt(r.arg);
        Say("CHEST: Stored %s.", nm);
        return; }
    default: return;
    }
}

// --- Drawing -----------------------------------------------------------------
static void TwBox(HDC hdc, int l, int t, int r, int b, COLORREF c) {
    RECT rc = { l, t, r, b }; HBRUSH br = CreateSolidBrush(c); FillRect(hdc, &rc, br); DeleteObject(br);
}
static void TwDisc(HDC hdc, int cx, int cy, int rx, int ry, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c); HPEN pn = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(hdc, br), op = SelectObject(hdc, pn);
    Ellipse(hdc, cx - rx, cy - ry, cx + rx, cy + ry);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(br); DeleteObject(pn);
}
static void TwLine(HDC hdc, int x1, int y1, int x2, int y2, int w, COLORREF c) {
    HPEN pn = CreatePen(PS_SOLID, w, c); HGDIOBJ op = SelectObject(hdc, pn);
    MoveToEx(hdc, x1, y1, NULL); LineTo(hdc, x2, y2); SelectObject(hdc, op); DeleteObject(pn);
}
static void TwPoly(HDC hdc, const POINT *pts, int n, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c); HPEN pn = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(hdc, br), op = SelectObject(hdc, pn);
    Polygon(hdc, pts, n); SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(br); DeleteObject(pn);
}

void DrawCrown(HDC hdc, int cx, int top, int w) {
    POINT p[7] = { { cx - w, top + w }, { cx - w, top }, { cx - w / 2, top + w / 2 }, { cx, top - w / 3 },
                   { cx + w / 2, top + w / 2 }, { cx + w, top }, { cx + w, top + w } };
    TwPoly(hdc, p, 7, RGB(240, 200, 60));
    TwDisc(hdc, cx, top + w / 2 + 1, w / 5 + 1, w / 5 + 1, RGB(200, 40, 60));
}

static void DrawDeitySymbol(HDC hdc, int cx, int cy, int r, int deity, COLORREF bg) {
    if (deity == 0) {
        for (int k = 0; k < 12; k++) {
            double a = k * 3.14159265 / 6.0;
            TwLine(hdc, cx, cy, cx + (int)(cos(a) * r * 1.6), cy + (int)(sin(a) * r * 1.6), r / 6 + 1, RGB(255, 220, 120));
        }
        TwDisc(hdc, cx, cy, r, r, RGB(255, 236, 150));
    } else if (deity == 1) {
        TwDisc(hdc, cx, cy, r, r, RGB(150, 90, 200));
        TwDisc(hdc, cx + r / 2, cy - r / 4, r, r, bg);
    } else if (deity == 2) {
        TwDisc(hdc, cx, cy, r, r, RGB(235, 230, 220));
        TwDisc(hdc, cx - r / 3, cy - r / 6, r / 4 + 1, r / 4 + 1, RGB(20, 20, 20));
        TwDisc(hdc, cx + r / 3, cy - r / 6, r / 4 + 1, r / 4 + 1, RGB(20, 20, 20));
        TwBox(hdc, cx - r / 3, cy + r / 2, cx + r / 3, cy + r / 2 + r / 6 + 1, RGB(20, 20, 20));
        TwBox(hdc, cx - r * 2 / 3 - 2, cy - r - r / 5, cx + r * 2 / 3 + 2, cy - r + r / 10, RGB(15, 15, 15));
        TwBox(hdc, cx - r / 2, cy - r * 2, cx + r / 2, cy - r - r / 5, RGB(15, 15, 15));
    } else {
        for (int k = 0; k < 5; k++) {
            int x0 = cx - r + k * r / 2;
            for (int s = 0; s < 6; s++)
                TwLine(hdc, x0 + (int)(sin(s * 0.9 + k) * r / 4), cy + s * r / 4, x0 + (int)(sin((s + 1) * 0.9 + k) * r / 4),
                       cy + (s + 1) * r / 4, r / 6 + 1, RGB(60, 170, 160));
        }
        TwDisc(hdc, cx, cy, r, r * 3 / 4, RGB(40, 120, 120));
        TwDisc(hdc, cx, cy, r / 3, r / 4, RGB(230, 240, 120));
    }
}

// Outdoor shrine stone with its god's symbol and two candles.
void DrawAltar(HDC hdc, int sx, int sy, int deity) {
    int flick = (int)(GetTickCount() / 120) % 2;
    TwBox(hdc, sx - 14, sy - 14, sx + 14, sy + 2, RGB(120, 115, 108));
    TwBox(hdc, sx - 17, sy - 18, sx + 17, sy - 13, RGB(150, 145, 138));
    DrawDeitySymbol(hdc, sx, sy - 32, 6, deity, RGB(70, 110, 60));
    TwBox(hdc, sx - 14, sy - 25, sx - 10, sy - 18, RGB(240, 235, 220));
    TwBox(hdc, sx + 10, sy - 25, sx + 14, sy - 18, RGB(240, 235, 220));
    TwDisc(hdc, sx - 12, sy - 28 - flick, 2, 3, RGB(255, 190, 60));
    TwDisc(hdc, sx + 12, sy - 29 + flick, 2, 3, RGB(255, 190, 60));
}

#define INT_FLOOR_Y 660
static void DrawCounter(HDC hdc, int l, int r) {
    TwBox(hdc, l, INT_FLOOR_Y - 120, r, INT_FLOOR_Y, RGB(96, 60, 34));
    TwBox(hdc, l - 12, INT_FLOOR_Y - 132, r + 12, INT_FLOOR_Y - 118, RGB(140, 96, 58));
    for (int x = l + 40; x < r; x += 80) TwLine(hdc, x, INT_FLOOR_Y - 110, x, INT_FLOOR_Y - 8, 2, RGB(70, 44, 24));
}

static void DrawShelf(HDC hdc, int l, int r, int y, int seed) {
    TwBox(hdc, l, y, r, y + 10, RGB(70, 44, 24));
    for (int x = l + 8, k = 0; x < r - 20; x += 30, k++) {
        unsigned int h = (unsigned int)RainHash(seed * 53 + k);
        TwBox(hdc, x, y - 26 - (int)(h % 18u), x + 20, y, RGB(80 + h % 160, 60 + (h >> 8) % 160, 60 + (h >> 16) % 160));
    }
}

// Pet-shop animals as flat-shaded side-view polygons with eye dots, matching
// the low-poly look of the AFK view. Points are in figure units, feet at cy.
static void PetPoly(HDC hdc, int cx, int cy, float s, const int *xy, int n, COLORREF c) {
    POINT p[12];
    for (int i = 0; i < n && i < 12; i++) p[i] = (POINT){ cx + (int)(xy[2 * i] * s), cy + (int)(xy[2 * i + 1] * s) };
    HBRUSH b = CreateSolidBrush(c); HGDIOBJ ob = SelectObject(hdc, b), op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, p, n);
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(b);
}

static COLORREF PetDim(COLORREF c, float k) { return RGB((int)(GetRValue(c) * k), (int)(GetGValue(c) * k), (int)(GetBValue(c) * k)); }

// kind 0 dog, 1 cat, 2 toad, 3 owl.
static void DrawPetFigure(HDC hdc, int cx, int cy, float s, int kind, COLORREF c) {
    COLORREF dark = PetDim(c, 0.7f), black = RGB(12, 10, 10);
    int blink = (GetTickCount() / 140 + kind * 7) % 30 == 0;
    if (kind <= 1) {
        static const int legs[4] = { -12, -7, 6, 11 };
        for (int l = 0; l < 4; l++) { int q[8] = { legs[l], -9, legs[l] + 3, -9, legs[l] + 3, 0, legs[l], 0 }; PetPoly(hdc, cx, cy, s, q, 4, l % 2 ? dark : c); }
        static const int body[12] = { -14, -12, -10, -18, 10, -18, 14, -12, 10, -8, -10, -8 };
        static const int head[12] = { 10, -16, 14, -24, 22, -24, 26, -18, 23, -13, 14, -12 };
        PetPoly(hdc, cx, cy, s, body, 6, c);
        if (kind == 0) {
            static const int tail[8] = { -13, -15, -21, -23, -19, -24, -12, -17 }, snout[8] = { 23, -20, 30, -19, 30, -14, 23, -14 };
            static const int ear[6] = { 14, -24, 18, -25, 16, -16 }, nose[8] = { 29, -20, 31, -20, 31, -18, 29, -18 };
            PetPoly(hdc, cx, cy, s, tail, 4, c); PetPoly(hdc, cx, cy, s, head, 6, c);
            PetPoly(hdc, cx, cy, s, snout, 4, dark); PetPoly(hdc, cx, cy, s, nose, 4, black); PetPoly(hdc, cx, cy, s, ear, 3, dark);
        } else {
            static const int tail[12] = { -13, -14, -20, -19, -22, -30, -19, -30, -17, -21, -12, -16 };
            static const int ear1[6] = { 14, -23, 15, -30, 19, -24 }, ear2[6] = { 19, -24, 23, -30, 24, -22 };
            PetPoly(hdc, cx, cy, s, tail, 6, c); PetPoly(hdc, cx, cy, s, head, 6, c);
            PetPoly(hdc, cx, cy, s, ear1, 3, c); PetPoly(hdc, cx, cy, s, ear2, 3, c);
        }
        if (!blink) { int e[8] = { 20, -21, 22, -21, 22, -19, 20, -19 }; PetPoly(hdc, cx, cy, s, e, 4, kind ? RGB(110, 190, 60) : black); }
    } else if (kind == 2) {
        static const int body[12] = { -12, 0, -14, -6, -8, -12, 8, -12, 14, -6, 12, 0 };
        static const int b1[8] = { -7, -12, -5, -16, -1, -16, 0, -12 }, b2[8] = { 1, -12, 2, -16, 6, -16, 8, -12 };
        static const int e1[8] = { -5, -15, -2, -15, -2, -13, -5, -13 }, e2[8] = { 3, -15, 6, -15, 6, -13, 3, -13 };
        PetPoly(hdc, cx, cy, s, body, 6, c); PetPoly(hdc, cx, cy, s, b1, 4, c); PetPoly(hdc, cx, cy, s, b2, 4, c);
        if (!blink) { PetPoly(hdc, cx, cy, s, e1, 4, black); PetPoly(hdc, cx, cy, s, e2, 4, black); }
    } else {
        static const int body[12] = { -8, 0, -10, -12, -7, -22, 7, -22, 10, -12, 8, 0 };
        static const int head[12] = { -8, -21, -9, -30, -5, -34, 5, -34, 9, -30, 8, -21 };
        static const int t1[6] = { -8, -32, -9, -38, -4, -34 }, t2[6] = { 4, -34, 9, -38, 8, -32 };
        static const int e1[8] = { -6, -31, -1, -31, -1, -26, -6, -26 }, e2[8] = { 1, -31, 6, -31, 6, -26, 1, -26 };
        static const int p1[8] = { -4, -29, -2, -29, -2, -27, -4, -27 }, p2[8] = { 3, -29, 5, -29, 5, -27, 3, -27 };
        static const int beak[6] = { -1, -26, 1, -26, 0, -23 };
        PetPoly(hdc, cx, cy, s, body, 6, c); PetPoly(hdc, cx, cy, s, head, 6, c);
        PetPoly(hdc, cx, cy, s, t1, 3, dark); PetPoly(hdc, cx, cy, s, t2, 3, dark);
        PetPoly(hdc, cx, cy, s, e1, 4, RGB(250, 210, 60)); PetPoly(hdc, cx, cy, s, e2, 4, RGB(250, 210, 60));
        if (!blink) { PetPoly(hdc, cx, cy, s, p1, 4, black); PetPoly(hdc, cx, cy, s, p2, 4, black); }
        PetPoly(hdc, cx, cy, s, beak, 3, RGB(230, 170, 50));
    }
}

void DrawInteriorBackdrop(HDC hdc, int menu) {
    if (menu == TM_BEGGAR || menu == TM_CHEST || menu == TM_HOME) return;
    const int W = WINDOW_WIDTH, H = WINDOW_HEIGHT, F = INT_FLOOR_Y;
    int stone = menu == TM_KING || menu == TM_BANK, shrine = menu == TM_ALTAR;
    COLORREF wall = shrine ? RGB(40, 30, 52) : stone ? RGB(96, 96, 104) : RGB(116, 78, 46);
    COLORREF seam = shrine ? RGB(26, 18, 36) : stone ? RGB(70, 70, 80) : RGB(86, 56, 32);
    DWORD t = GetTickCount();
    DrawShopRoom(hdc, stone, shrine, F);

    if (menu == TM_SMITH) {
        TwBox(hdc, 150, 0, 260, 330, RGB(100, 55, 45));
        TwBox(hdc, 80, 330, 330, F, RGB(112, 62, 50));
        TwBox(hdc, 130, 470, 280, F - 12, RGB(30, 20, 20));
        for (int k = 0; k < 5; k++)
            TwDisc(hdc, 150 + k * 28, F - 40 - (int)((t / 90 + k * 7) % 20), 14, 22, k % 2 ? RGB(255, 140, 40) : RGB(255, 210, 80));
        for (int k = 0; k < 4; k++) {
            int x = 460 + k * 85;
            TwLine(hdc, x, 150, x, 300, 6, RGB(200, 205, 215));
            TwLine(hdc, x - 18, 300, x + 18, 300, 6, RGB(120, 90, 40));
            TwLine(hdc, x, 300, x, 340, 6, RGB(80, 50, 30));
        }
        POINT anvil[8] = { { 800, F - 120 }, { 990, F - 120 }, { 960, F - 95 }, { 930, F - 95 }, { 940, F - 40 },
                           { 980, F }, { 830, F }, { 860, F - 40 } };
        TwPoly(hdc, anvil, 8, RGB(60, 62, 70));
        DrawZeldaStyleVillager(hdc, 700, F - 10, 6, 6.0f);
    } else if (menu == TM_CARDS) {
        DrawShelf(hdc, 400, 780, 230, 11); DrawShelf(hdc, 400, 780, 360, 12);
        DrawZeldaStyleVillager(hdc, 190, F - 146, 10, 4.5f); // the kid, seated behind his table
        TwBox(hdc, 70, F - 110, 330, F - 92, RGB(120, 80, 50));
        TwBox(hdc, 90, F - 92, 104, F, RGB(90, 60, 36)); TwBox(hdc, 296, F - 92, 310, F, RGB(90, 60, 36));
        for (int k = 0; k < 4; k++) TwBox(hdc, 110 + k * 46, F - 118, 140 + k * 46, F - 110, k % 2 ? RGB(180, 40, 50) : RGB(240, 240, 230));
    } else if (menu == TM_PETS) {
        for (int k = 0; k < 3; k++) {
            int x0 = 80 + k * 110, y0 = 400;
            TwBox(hdc, x0, y0, x0 + 90, y0 + 90, RGB(60, 50, 40));
            TwBox(hdc, x0 + 4, y0 + 4, x0 + 86, y0 + 86, RGB(70, 46, 28));
            DrawPetFigure(hdc, x0 + 40, y0 + 82, 1.7f, k, k == 0 ? RGB(150, 100, 55) : k == 1 ? RGB(120, 120, 128) : RGB(70, 130, 60));
            for (int x = x0 + 12; x < x0 + 90; x += 15) TwLine(hdc, x, y0, x, y0 + 90, 2, RGB(180, 170, 150));
        }
        TwLine(hdc, 1000, 380, 1080, 380, 6, RGB(90, 60, 30));
        DrawPetFigure(hdc, 1040, 378, 1.4f, 3, RGB(150, 110, 70));
    } else if (menu == TM_KING) {
        TwBox(hdc, 470, F, 650, H, RGB(150, 30, 40));
        TwBox(hdc, 150, 60, 230, 380, RGB(150, 30, 40)); TwBox(hdc, 182, 60, 198, 380, RGB(220, 180, 60));
        TwBox(hdc, 890, 330, 970, 600, RGB(150, 30, 40)); TwBox(hdc, 922, 330, 938, 600, RGB(220, 180, 60));
        TwBox(hdc, 480, 300, 640, F - 60, RGB(200, 160, 50));
        TwBox(hdc, 500, 320, 620, F - 70, RGB(140, 30, 40));
        DrawZeldaStyleVillager(hdc, 560, F - 60, 1, 6.0f);
        DrawCrown(hdc, 560, F - 60 - 105, 18);
        TwBox(hdc, 470, F - 80, 650, F - 50, RGB(200, 160, 50));
        TwBox(hdc, 480, F - 50, 496, F, RGB(170, 130, 40)); TwBox(hdc, 624, F - 50, 640, F, RGB(170, 130, 40));
    } else if (menu == TM_BANK) {
        TwDisc(hdc, 220, 380, 130, 130, RGB(120, 120, 130));
        TwDisc(hdc, 220, 380, 112, 112, RGB(150, 150, 160));
        for (int k = 0; k < 6; k++) {
            double a = k * 3.14159265 / 3.0;
            TwLine(hdc, 220, 380, 220 + (int)(cos(a) * 70), 380 + (int)(sin(a) * 70), 8, RGB(90, 90, 100));
        }
        TwDisc(hdc, 220, 380, 22, 22, RGB(200, 170, 60));
    } else if (menu == TM_ALTAR) {
        int d = TownDeity();
        TwBox(hdc, 420, F - 140, 700, F, RGB(90, 86, 92));
        TwBox(hdc, 400, F - 156, 720, F - 136, RGB(120, 116, 124));
        DrawDeitySymbol(hdc, 560, 300, 60, d, wall);
        for (int k = 0; k < 4; k++) {
            int x = 430 + k * 85;
            TwBox(hdc, x, F - 186, x + 12, F - 156, RGB(235, 230, 210));
            TwDisc(hdc, x + 6, F - 194 - (int)((t / 110 + k) % 3), 5, 9, RGB(255, 190, 70));
        }
    } else { // merchant
        DrawShelf(hdc, 680, 820, 260, 1); DrawShelf(hdc, 680, 820, 400, 2);
        for (int k = 0; k < 3; k++) TwDisc(hdc, 1040 + k * 50, F - 30, 22, 30, RGB(110, 72, 40));
    }
    if (menu == TM_NONE || menu == TM_CARDS || menu == TM_PETS || menu == TM_BANK) {
        int look = menu == TM_NONE ? 3 : menu == TM_CARDS ? 8 : menu == TM_PETS ? 7 : 9;
        DrawZeldaStyleVillager(hdc, 560, F - 125, look, 6.0f);
        DrawCounter(hdc, 380, 760);
        if (menu == TM_BANK)
            for (int k = 0; k < 3; k++) for (int s = 0; s < 5; s++) TwDisc(hdc, 430 + k * 44, F - 136 - s * 6, 16, 5, RGB(230, 190, 60));
    }
    DrawHeroAssetEx(hdc, 900, F + 40, 5.0f);
}

static int KidTableVisible(void) {
    return town_menu == TM_CARDS && kid_round_screen == current_screen_index && (kid_dealt || kid_played);
}

static void DrawCard(HDC hdc, int x, int y, int id, int num, int result) {
    static const COLORREF set_col[3] = { RGB(55, 95, 50), RGB(150, 135, 70), RGB(120, 35, 40) };
    const CardDef *c = &card_defs[PoolId(id)];
    static const char *rar[3] = { "Common", "Uncommon", "Rare" };
    char buf[96];
    int w = 330, h = 160, p = CardBasePower(id);
    TwBox(hdc, x, y, x + w, y + h, RGB(24, 24, 30));
    TwBox(hdc, x, y, x + w, y + 30, set_col[c->set]);
    HPEN pen = CreatePen(PS_SOLID, 3, result > 0 ? RGB(90, 230, 110) : result < 0 ? RGB(230, 80, 70) : RGB(120, 120, 130));
    HGDIOBJ op = SelectObject(hdc, pen), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
    SetTextColor(hdc, RGB(255, 235, 100));
    snprintf(buf, sizeof(buf), "%d. %s", num, c->name);
    TextOut(hdc, x + 10, y + 6, buf, (int)strlen(buf));
    snprintf(buf, sizeof(buf), "%d/%d", p, p);
    TextOut(hdc, x + w - 50, y + 6, buf, (int)strlen(buf));
    SetTextColor(hdc, RGB(180, 180, 190));
    snprintf(buf, sizeof(buf), "%s  -  %s  -  base %d/%d", card_sets[c->set], rar[c->pt - 1], c->pt, c->pt);
    TextOut(hdc, x + 10, y + 36, buf, (int)strlen(buf));
    RECT tr = { x + 10, y + 60, x + w - 10, y + h - 6 };
    SetTextColor(hdc, RGB(170, 220, 255));
    DrawText(hdc, AbilityText(c->ability), -1, &tr, DT_WORDBREAK | DT_LEFT);
}

// The practice table: your three cards (in the order laid) on top, the kid's
// three below, and the play-by-play of the stack underneath.
static void DrawCardTable(HDC hdc) {
    int x0 = 780;
    char buf[96];
    TwBox(hdc, x0 - 10, 40, WINDOW_WIDTH - 14, 930, RGB(16, 18, 24));
    SetTextColor(hdc, RGB(255, 235, 100));
    snprintf(buf, sizeof(buf), "PRACTICE TABLE  -  round %d of %d", kid_rounds + (kid_dealt ? 1 : 0), KID_MAX_ROUNDS);
    TextOut(hdc, x0, 48, buf, (int)strlen(buf));
    SetTextColor(hdc, RGB(230, 230, 230));
    const char *mine = kid_dealt ? "YOUR HAND - pick the order to lay them on the left" : "YOUR CARDS, in the order you laid them";
    TextOut(hdc, x0, 74, mine, (int)strlen(mine));
    for (int k = 0; k < 3; k++) DrawCard(hdc, x0 + k * 340, 98, my_hand[my_order[k]], k + 1, kid_played ? duel_result[k] : 0);
    const char *his = "THE KID'S CARDS - he leads left to right";
    SetTextColor(hdc, RGB(230, 230, 230));
    TextOut(hdc, x0, 270, his, (int)strlen(his));
    for (int k = 0; k < 3; k++) DrawCard(hdc, x0 + k * 340, 294, kid_hand[k], k + 1, kid_played ? -duel_result[k] : 0);
    SetTextColor(hdc, RGB(160, 240, 170));
    if (!kid_played) {
        const char *t = "Damage lands at the same time unless a card has First Strike. A Demon 1/1 that dies curses the next enemy card -2/-2; an Angel 1/1 blesses your next card.";
        RECT r = { x0, 474, WINDOW_WIDTH - 30, 560 };
        DrawText(hdc, t, -1, &r, DT_WORDBREAK | DT_LEFT);
    }
    for (int i = 0; i < kid_log_n; i++) TextOut(hdc, x0, 474 + i * 26, kid_log[i], (int)strlen(kid_log[i]));
}

static int tm_top, tm_start, tm_end, tm_left, tm_right;
int TownMenuClick(int x, int y) {
    if (town_menu == TM_NONE || x < tm_left || x > tm_right || y < tm_top - 3) return 0;
    int i = tm_start + (y - tm_top + 3) / 30;
    if (i >= tm_end) return 0;
    if (i == town_cursor) TownMenuConfirm(); else town_cursor = i;
    return 1;
}

void DrawTownMenu(HDC hdc) {
    if (town_menu == TM_NONE) return;
    if (menu_screen != current_screen_index) { town_menu = TM_NONE; return; }
    BuildRows();
    DrawInteriorBackdrop(hdc, town_menu);
    static const char *titles[10] = { "", "BLACKSMITH", "CARD SHOP", "PET SHOP", "THRONE ROOM", "TOWN BANK", "BEGGAR", "ALTAR", "CHEST", "HOME" };
    RECT box = { 1100, 70, 1790, 910 };
    if (town_menu == TM_BEGGAR || town_menu == TM_CHEST) { box.left = 560; box.right = 1260; box.top = 200; box.bottom = 700; }
    if (town_menu == TM_HOME) { box.left = 1240; box.right = 1840; box.top = 470; box.bottom = 860; }
    int table = KidTableVisible();
    if (table) { box.left = 40; box.right = 760; }
    TwBox(hdc, box.left, box.top, box.right, box.bottom, RGB(18, 20, 28));
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(90, 200, 170));
    HGDIOBJ op = SelectObject(hdc, pen), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, box.left, box.top, box.right, box.bottom);
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ prev_font = SelectObject(hdc, GetMenuUnderlineFont());
    SetTextColor(hdc, RGB(255, 235, 100));
    TextOut(hdc, box.left + 20, box.top + 12, titles[town_menu], (int)strlen(titles[town_menu]));
    SelectObject(hdc, prev_font);

    char line[200];
    if (town_menu == TM_BANK) snprintf(line, sizeof(line), "Gold: %d    Vault: %d", gold_count, bank_gold);
    else if (town_menu == TM_ALTAR) snprintf(line, sizeof(line), "%s (%s)    Favor: %d    Gold: %d", deity_names[TownDeity()], deity_kinds[TownDeity()], deity_favor[TownDeity()], gold_count);
    else snprintf(line, sizeof(line), "Gold: %d", gold_count);
    SetTextColor(hdc, RGB(230, 230, 230));
    TextOut(hdc, box.left + 20, box.top + 42, line, (int)strlen(line));

    int visible = town_menu == TM_BEGGAR || town_menu == TM_HOME ? 6 : 14;
    int start = town_cursor - visible / 2;
    if (start > row_count - visible) start = row_count - visible;
    if (start < 0) start = 0;
    int end = start + visible; if (end > row_count) end = row_count;
    int y = box.top + 78;
    tm_top = y; tm_start = start; tm_end = end; tm_left = box.left; tm_right = box.right;
    for (int i = start; i < end; i++, y += 30) {
        int sel = i == town_cursor;
        if (sel) TwBox(hdc, box.left + 10, y - 3, box.right - 10, y + 25, RGB(40, 64, 74));
        SetTextColor(hdc, sel ? RGB(255, 235, 100) : RGB(210, 214, 220));
        snprintf(line, sizeof(line), "%s %s", sel ? ">" : " ", rows[i].text);
        TextOut(hdc, box.left + 20, y, line, (int)strlen(line));
    }
    char info[256];
    RowInfo(&rows[town_cursor], info, sizeof(info));
    RECT ir = { box.left + 20, y + 14, box.right - 20, y + 110 };
    SetTextColor(hdc, RGB(170, 220, 255));
    DrawText(hdc, info, -1, &ir, DT_WORDBREAK | DT_LEFT);
    RECT nr = { box.left + 20, ir.bottom + 6, box.right - 20, box.bottom - 40 };
    SetTextColor(hdc, RGB(160, 240, 170));
    DrawText(hdc, town_note, -1, &nr, DT_WORDBREAK | DT_LEFT);
    const char *hint = "Up/Down choose    A / SPACE select    B / ESC leave";
    SetTextColor(hdc, RGB(150, 150, 160));
    TextOut(hdc, box.left + 20, box.bottom - 30, hint, (int)strlen(hint));
    if (table) DrawCardTable(hdc);
}

// The familiar trots (or flies) behind the hero while awake; diurnal ones
// sleep in the backpack 21:00-06:00 and nocturnal ones by day.
void DrawFamiliar(HDC hdc) {
    static float fx = -100.0f, fy = -100.0f;
    static int was_asleep = -1;
    familiar_visible = 0;
    int slot = OwnedFamiliarSlot();
    if (slot < 0) { was_asleep = -1; return; }
    int k = FamiliarIndex(player_inventory[slot].name);
    if (k < 0) return;
    int asleep = familiars[k].nocturnal != IsNight();
    if (was_asleep >= 0 && asleep != was_asleep)
        snprintf(arpg_action_log, sizeof(arpg_action_log), asleep ? "Your %s curls up in your pack to sleep." :
                 "Your %s wakes and climbs out of your pack.", familiars[k].name);
    // Asleep it curls up by a nearby campfire; with no fire it rides in the pack and adds a little weight.
    int fire = asleep && !in_cave ? FindNearbyCampfire(4.0f) : -1;
    float pack_w = asleep && fire < 0 ? 2.0f : 0.0f;
    if (player_inventory[slot].weight != pack_w) { player_inventory[slot].weight = pack_w; RecalculateCarriedWeight(); }
    was_asleep = asleep;
    if (in_cave || (asleep && fire < 0)) return;
    static int face = 1; static float prev_fx, prev_fy;
    float tx = player_x - 0.9f, ty = player_y + 0.6f;
    if (fire >= 0) { tx = campfires[fire].x + 0.9f; ty = campfires[fire].y + 0.4f; }
    else if (k <= 1 && AfkViewMs() >= 0 && (AfkViewMs() / 1000) % 15 < 5) {
        // AFK play: the dog chases a squirrel, the cat a butterfly or bird, then
        // trots back. It always stops just short, so it never catches one.
        float best = 64.0f, px = 0.0f, py = 0.0f;
        for (int i = 0; i < MAX_CRITTERS; i++) {
            CritterEntity *c = &critters[i];
            if (!c->active || c->hidden || c->type != (k == 0 ? 1 : 0)) continue;
            float dx = c->x - fx, dy = c->y - fy;
            if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; px = c->x; py = c->y; }
        }
        if (k == 1) for (int i = 0; i < MAX_BUTTERFLIES; i++) {
            if (!butterflies[i].active) continue;
            float dx = butterflies[i].x - fx, dy = butterflies[i].y - fy;
            if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; px = butterflies[i].x; py = butterflies[i].y; }
        }
        if (best < 64.0f) { float d = sqrtf(best) + 0.001f; tx = px - (px - fx) / d * 1.2f; ty = py - (py - fy) / d * 1.2f; }
    }
    if (fabsf(fx - tx) > 10.0f || fabsf(fy - ty) > 10.0f) { fx = tx; fy = ty; }
    fx += (tx - fx) * 0.05f; fy += (ty - fy) * 0.05f;
    float mv = (fx - prev_fx) - (fy - prev_fy); // screen-x direction of travel
    if (fabsf(mv) > 0.003f) face = mv > 0.0f ? 1 : -1;
    else face = (player_facing == FACE_LEFT || player_facing == FACE_UP) ? -1 : 1;
    prev_fx = fx; prev_fy = fy;
    familiar_world_x = fx; familiar_world_y = fy; familiar_visible = k + 1;
    int sx, sy; GetIsoCoords(fx, fy, &sx, &sy);
    DWORD t = GetTickCount();
    int bob = (int)(sin(t / 180.0) * 3.0);
    if (k <= 1) { // dog or cat, faced the way it is going, in the size and coat picked at the pet shop
        static const COLORREF coats[2][4] = { { RGB(150, 100, 55), RGB(40, 35, 30), RGB(235, 230, 220), RGB(205, 160, 80) },
                                              { RGB(120, 120, 128), RGB(220, 130, 50), RGB(35, 35, 40), RGB(240, 240, 240) } };
        COLORREF body = coats[k][familiar_color & 3];
        float S = familiar_size == 0 ? 0.75f : familiar_size == 2 ? 1.3f : 1.0f;
        int m = face;
#define FS(v) ((int)((v) * S))
        if (fire >= 0) { // curled up asleep by the fire
            TwDisc(hdc, sx, sy - FS(5), FS(11), FS(5), body); TwDisc(hdc, sx + m * FS(9), sy - FS(6), FS(5), FS(4), body);
            return;
        }
        for (int l = 0; l < 3; l++) TwLine(hdc, sx + m * FS(-7 + l * 6), sy - FS(6), sx + m * FS(-7 + l * 6), sy, 2, body);
        TwDisc(hdc, sx, sy - FS(9), FS(11), FS(6), body);
        TwDisc(hdc, sx + m * FS(10), sy - FS(15), FS(6), FS(6), body);
        if (k == 0) TwDisc(hdc, sx + m * FS(7), sy - FS(18), FS(3), FS(5), RGB(110, 70, 35));
        else { TwLine(hdc, sx + m * FS(6), sy - FS(20), sx + m * FS(7), sy - FS(25), 2, body); TwLine(hdc, sx + m * FS(13), sy - FS(20), sx + m * FS(13), sy - FS(25), 2, body); }
        TwLine(hdc, sx - m * FS(10), sy - FS(10), sx - m * FS(16), sy - FS(16) + bob, 2, body);
        TwDisc(hdc, sx + m * FS(12), sy - FS(16), 1, 1, RGB(10, 10, 10));
#undef FS
    } else if (k == 2) { // toad
        int hop = bob > 2 ? -3 : 0;
        TwDisc(hdc, sx, sy - 6 + hop, 9, 6, RGB(70, 130, 60));
        TwDisc(hdc, sx - 4, sy - 12 + hop, 3, 3, RGB(70, 130, 60)); TwDisc(hdc, sx + 4, sy - 12 + hop, 3, 3, RGB(70, 130, 60));
        TwDisc(hdc, sx - 4, sy - 13 + hop, 1, 1, RGB(10, 10, 10)); TwDisc(hdc, sx + 4, sy - 13 + hop, 1, 1, RGB(10, 10, 10));
    } else { // hawk or owl
        COLORREF body = k == 3 ? RGB(140, 100, 60) : RGB(170, 150, 120);
        int by = sy - 44 + bob, flap = (t / 150) % 2 ? -6 : 4;
        TwDisc(hdc, sx, sy - 2, 6, 2, RGB(30, 30, 30));
        TwLine(hdc, sx - 3, by, sx - 14, by + flap, 3, body); TwLine(hdc, sx + 3, by, sx + 14, by + flap, 3, body);
        TwDisc(hdc, sx, by, 5, 7, body);
        if (k == 4) { TwDisc(hdc, sx - 2, by - 3, 2, 2, RGB(250, 210, 60)); TwDisc(hdc, sx + 2, by - 3, 2, 2, RGB(250, 210, 60)); }
        else TwDisc(hdc, sx + 2, by - 4, 1, 1, RGB(10, 10, 10));
    }
}

// --- Storage chests ----------------------------------------------------------
float familiar_world_x, familiar_world_y;
int familiar_visible; // familiar index + 1 while it is out walking, else 0

StashChest chests[MAX_CHESTS];
int open_chest = -1;

static int ChestDay(void) { return (int)current_season * DAYS_PER_SEASON + SeasonDay(); }

void BuildChest(void) {
    if (GetMaterialCount("Wood") < 4) { strcpy(arpg_action_log, "CHEST: You need 4 Wood to build a chest."); return; }
    int slot = -1;
    for (int i = 0; i < MAX_CHESTS && slot < 0; i++) if (!chests[i].active) slot = i;
    if (slot < 0) { strcpy(arpg_action_log, "CHEST: You already have 8 chests out in the world."); return; }
    ConsumeMaterialFromInventory("Wood", 4); RecalculateCarriedWeight();
    float x = player_x, y = player_y;
    if (player_facing == FACE_UP) y -= 1.0f; else if (player_facing == FACE_DOWN) y += 1.0f;
    else if (player_facing == FACE_LEFT) x -= 1.0f; else x += 1.0f;
    memset(&chests[slot], 0, sizeof(chests[slot]));
    chests[slot].active = 1; chests[slot].screen_id = current_screen_index; chests[slot].x = x; chests[slot].y = y;
    chests[slot].last_day = ChestDay();
    strcpy(arpg_action_log, "CHEST: Built a chest. Hide it among trees, away from paths, or thieves may find it.");
}

int OpenNearbyChest(void) {
    CheckChestBreakIns();
    for (int i = 0; i < MAX_CHESTS; i++) {
        if (!chests[i].active || chests[i].screen_id != current_screen_index || !IsNearPoint(chests[i].x, chests[i].y, 1.3f)) continue;
        open_chest = i; OpenTownMenu(TM_CHEST);
        return 1;
    }
    return 0;
}

// A chest with few trees around it is in the open; each day there is a 1 in 4
// chance someone breaks in and takes half of what's inside.
void CheckChestBreakIns(void) {
    for (int i = 0; i < MAX_CHESTS; i++) {
        StashChest *c = &chests[i];
        if (!c->active || c->screen_id != current_screen_index || c->last_day == ChestDay()) continue;
        c->last_day = ChestDay();
        int trees = 0;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int tx = (int)c->x + dx, ty = (int)c->y + dy;
            if (tx >= 0 && tx < MAP_SIZE && ty >= 0 && ty < MAP_SIZE && VISUAL_MAP[ty][tx] == 11) trees++;
        }
        if (trees >= 3 || c->count == 0 || rand() % 4) continue;
        int lose = (c->count + 1) / 2;
        for (int k = 0; k < lose; k++) { int j = rand() % c->count; c->items[j] = c->items[--c->count]; }
        strcpy(arpg_action_log, "CHEST: Your chest has been broken into! Some items are gone.");
    }
}

void DrawChests(HDC hdc) {
    for (int i = 0; i < MAX_CHESTS; i++) {
        if (!chests[i].active || chests[i].screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords(chests[i].x, chests[i].y, &sx, &sy);
        TwBox(hdc, sx - 11, sy - 4, sx + 11, sy + 10, RGB(120, 78, 40));
        TwBox(hdc, sx - 12, sy - 9, sx + 12, sy - 3, RGB(150, 100, 52));
        TwBox(hdc, sx - 2, sy - 4, sx + 2, sy + 2, RGB(220, 190, 70));
    }
}
