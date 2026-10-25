#include "game.h"
#include "town.h"
#include "farming.h"
#include "adventures.h"

extern WornTile worn_tiles[MAX_WORN_TILES];
extern uint8_t adv_state[ADV_MAX];

// --- Highwaymen: a chest within 3 tiles of a worn road is raided far more often ---
int ChestNearRoad(const StashChest *c) {
    for (int i = 0; i < MAX_WORN_TILES; i++) {
        const WornTile *w = &worn_tiles[i];
        if (w->active && w->screen_id == c->screen_id && abs((int)w->x - (int)c->x) <= 3 && abs((int)w->y - (int)c->y) <= 3) return 1;
    }
    return 0;
}

// --- Outlaws: town heat (crimes) of 30+ makes you WANTED ---
#define OUTLAW_HEAT 30
int bounty_hunter_active = 0;
int IsOutlaw(void) { return karma.town_heat >= OUTLAW_HEAT; }
static int OutlawFine(void) { return 50 + karma.town_heat * 10; }

// A town guard offers to take the fine; X again pays it and calls off the hunters.
int OutlawGuardTalk(int npc_type) {
    static DWORD offered_until = 0;
    if (npc_type != 2 || !IsOutlaw()) return 0;
    int fine = OutlawFine();
    if (GetTickCount() < offered_until) {
        offered_until = 0;
        if (gold_count < fine) { snprintf(arpg_action_log, sizeof(arpg_action_log), "GUARD: \"%d gold, or the bounty stands.\"", fine); return 1; }
        gold_count -= fine; karma.town_heat = 0;
        strcpy(arpg_action_log, "GUARD: \"Fine paid. Your name is clear and the bounty hunters are called off.\"");
        return 1;
    }
    offered_until = GetTickCount() + 8000;
    snprintf(arpg_action_log, sizeof(arpg_action_log), "GUARD: \"You're WANTED, outlaw. Pay a %d gold fine to clear your name? X again: pay.\"", fine);
    return 1;
}

// A bounty hunter: long coat, wide-brimmed hat and a crossbow.
void DrawBountyHunter(HDC hdc, int sx, int sy, int face) {
    HBRUSH coat = CreateSolidBrush(RGB(90, 60, 40)), skin = CreateSolidBrush(RGB(210, 170, 130));
    HBRUSH hat = CreateSolidBrush(RGB(40, 30, 25)), boot = CreateSolidBrush(RGB(30, 25, 20));
    HGDIOBJ old = SelectObject(hdc, boot);
    POINT l1[] = {{sx - 6, sy + 16}, {sx - 1, sy + 16}, {sx - 2, sy + 4}, {sx - 5, sy + 4}}; Polygon(hdc, l1, 4);
    POINT l2[] = {{sx + 1, sy + 16}, {sx + 6, sy + 16}, {sx + 5, sy + 4}, {sx + 2, sy + 4}}; Polygon(hdc, l2, 4);
    SelectObject(hdc, coat);
    POINT body[] = {{sx - 8, sy + 8}, {sx + 8, sy + 8}, {sx + 6, sy - 14}, {sx - 6, sy - 14}}; Polygon(hdc, body, 4);
    SelectObject(hdc, skin); Ellipse(hdc, sx - 5, sy - 24, sx + 5, sy - 14);
    SelectObject(hdc, hat);
    POINT brim[] = {{sx - 10, sy - 21}, {sx + 10, sy - 21}, {sx + 8, sy - 19}, {sx - 8, sy - 19}}; Polygon(hdc, brim, 4);
    POINT crown[] = {{sx - 5, sy - 21}, {sx + 5, sy - 21}, {sx + 4, sy - 28}, {sx - 4, sy - 28}}; Polygon(hdc, crown, 4);
    HPEN wood = CreatePen(PS_SOLID, 2, RGB(70, 50, 30)); HGDIOBJ op = SelectObject(hdc, wood);
    MoveToEx(hdc, sx + face * 6, sy - 6, NULL); LineTo(hdc, sx + face * 18, sy - 8);
    MoveToEx(hdc, sx + face * 16, sy - 14, NULL); LineTo(hdc, sx + face * 16, sy - 2);
    SelectObject(hdc, op); DeleteObject(wood);
    SelectObject(hdc, old); DeleteObject(coat); DeleteObject(skin); DeleteObject(hat); DeleteObject(boot);
}

// --- Ancestral legacy: deleting a save leaves something for the next hero ---
#define LEGACY_FILE "legacy.dat"
#define LEGACY_MAGIC 0x4C454731
void WriteLegacy(void) {
    int done = 0;
    for (int q = 0; q < ADV_MAX; q++) if (adv_state[q] == 255) done++;
    FILE *f = fopen(LEGACY_FILE, "wb");
    if (!f) return;
    int32_t d[4] = { LEGACY_MAGIC, player_level, done, gold_count };
    fwrite(d, sizeof(d), 1, f);
    fclose(f);
}
// On a new hero: +1 attribute point per 10 ancestor levels and per 25 quests (max 5),
// and a tenth of the ancestor's gold (max 500). Used up once claimed.
void ApplyLegacy(void) {
    FILE *f = fopen(LEGACY_FILE, "rb");
    if (!f) return;
    int32_t d[4];
    int ok = fread(d, sizeof(d), 1, f) == 1;
    fclose(f); remove(LEGACY_FILE);
    if (!ok || d[0] != LEGACY_MAGIC) return;
    int pts = d[1] / 10 + d[2] / 25, gold = d[3] / 10;
    if (pts > 5) pts = 5;
    if (gold > 500) gold = 500;
    if (gold < 0) gold = 0;
    stat_points += pts; gold_count += gold;
    snprintf(arpg_action_log, sizeof(arpg_action_log), "ANCESTRAL LEGACY: your level %d ancestor (%d quests done) leaves you %d gold and %d attribute points.",
             (int)d[1], (int)d[2], gold, pts);
}

// --- Cave network: every cave entrance opens into one of 8 shared chambers, each with one chest ---
#define CAVE_ROOMS 8
int cave_room_claimed = 0;
int CaveRoom(int cell) { return (int)((((unsigned int)cell * 2246822519u) >> 7) % CAVE_ROOMS); }
int CaveRoomClaimed(int cell) { return (cave_room_claimed >> CaveRoom(cell)) & 1; }
void ClaimCaveRoom(int cell) { cave_room_claimed |= 1 << CaveRoom(cell); }
