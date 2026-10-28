#include "game.h"

// Barrels: a few stand by the homes in every town and in caves. X smashes one
// for a little gold, an apple or wood, with the usual 2% card chance. Smashed
// barrels stay broken for the rest of the day and are back the next morning.
#define MAX_BARRELS 6
#define BARREL_MEMORY 8
typedef struct { float x, y; int intact; } Barrel;
static Barrel barrels[MAX_BARRELS];
static int barrel_count, barrel_screen = -2, barrel_day = -1, barrel_cave = -1;
static struct { int screen, day, cave, mask; } broken[BARREL_MEMORY];
static int broken_next;

static int *BrokenMask(int create) {
    for (int i = 0; i < BARREL_MEMORY; i++)
        if (broken[i].screen == barrel_screen && broken[i].day == barrel_day && broken[i].cave == barrel_cave && broken[i].mask) return &broken[i].mask;
    if (!create) return NULL;
    int i = broken_next++ % BARREL_MEMORY;
    broken[i].screen = barrel_screen; broken[i].day = barrel_day; broken[i].cave = barrel_cave; broken[i].mask = 0;
    return &broken[i].mask;
}

static void RefreshBarrels(void) {
    int day = GameDayOfYear(), cave = in_cave || current_biome == BIOME_CAVE;
    if (barrel_screen == current_screen_index && barrel_day == day && barrel_cave == cave) return;
    barrel_screen = current_screen_index; barrel_day = day; barrel_cave = cave; barrel_count = 0;
    if (cave) {
        for (int k = 0; k < 40 && barrel_count < 3; k++) {
            int h = RainHash(current_screen_index * 131 + k * 977 + 7);
            int x = 3 + h % (MAP_SIZE - 6), y = 3 + (h / MAP_SIZE) % (MAP_SIZE - 6);
            if (VISUAL_MAP[y][x] == 0) barrels[barrel_count++] = (Barrel){ (float)x, (float)y, 1 };
        }
    } else if (village_npc_count > 0) {
        static const int hx[4] = { 11, 15, 19, 15 }, hy[4] = { 11, 10, 11, 18 };
        int shift = (current_screen_index % 3) - 1;
        // Barrels stand against the sides of houses, never next to a shopkeeper or villager.
        static const int ox[4] = { -3, 3, -3, 3 }, oy[4] = { -1, -1, 1, 1 };
        for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) {
            int x = hx[i] + shift + ox[(k + i) % 4], y = hy[i] + oy[(k + i) % 4], near_npc = 0;
            if (x < 1 || y < 1 || x >= MAP_SIZE - 1 || y >= MAP_SIZE - 1 || VISUAL_MAP[y][x] != 0) continue;
            for (int n = 0; n < village_npc_count && !near_npc; n++) {
                float ndx = village_npcs[n].home_x - x, ndy = village_npcs[n].home_y - y;
                near_npc = ndx * ndx + ndy * ndy < 9.0f;
            }
            if (near_npc) continue;
            barrels[barrel_count++] = (Barrel){ (float)x, (float)y, 1 };
            break;
        }
    }
    int *mask = BrokenMask(0);
    for (int i = 0; mask && i < barrel_count; i++) if (*mask & (1 << i)) barrels[i].intact = 0;
}

int TryBreakBarrel(void) {
    RefreshBarrels();
    for (int i = 0; i < barrel_count; i++) {
        if (!barrels[i].intact) continue;
        float dx = barrels[i].x - player_x, dy = barrels[i].y - player_y;
        if (dx * dx + dy * dy > 1.6f * 1.6f) continue;
        barrels[i].intact = 0;
        *BrokenMask(1) |= 1 << i;
        int roll = rand() % 100;
        if (roll < 35) { int g = 1 + rand() % 8; gold_count += g; sprintf(arpg_action_log, "BARREL: Smashed it open - %d gold inside.", g); }
        else if (roll < 60) { AddStackableItem("Apple", "Food", 1); strcpy(arpg_action_log, "BARREL: Smashed it open - an apple inside."); }
        else if (roll < 75) { AddStackableItem("Wood", "Material", 1); strcpy(arpg_action_log, "BARREL: Smashed it open - some wood for the fire."); }
        else strcpy(arpg_action_log, "BARREL: Smashed it open - empty.");
        NoteBarrelBroken(barrels[i].x, barrels[i].y);
        RecalculateCarriedWeight();
        TryCardDrop(-1);
        return 1;
    }
    return 0;
}

int BarrelInfo(int i, float *x, float *y, int *intact) {
    if (i == 0) RefreshBarrels();
    if (i < 0 || i >= barrel_count) return 0;
    *x = barrels[i].x; *y = barrels[i].y; *intact = barrels[i].intact;
    return 1;
}

void DrawBarrels(HDC hdc) {
    RefreshBarrels();
    HBRUSH wood = CreateSolidBrush(RGB(140, 90, 45)); HPEN hoop = CreatePen(PS_SOLID, 1, RGB(60, 40, 25));
    HGDIOBJ ob = SelectObject(hdc, wood), op = SelectObject(hdc, hoop);
    for (int i = 0; i < barrel_count; i++) {
        int sx, sy; GetIsoCoords(barrels[i].x, barrels[i].y, &sx, &sy); sy += TILE_HEIGHT / 2;
        if (barrels[i].intact) {
            RoundRect(hdc, sx - 6, sy - 16, sx + 6, sy, 5, 5);
            MoveToEx(hdc, sx - 6, sy - 12, NULL); LineTo(hdc, sx + 6, sy - 12);
            MoveToEx(hdc, sx - 6, sy - 4, NULL); LineTo(hdc, sx + 6, sy - 4);
        } else {
            Rectangle(hdc, sx - 8, sy - 3, sx - 1, sy - 1); Rectangle(hdc, sx + 1, sy - 5, sx + 7, sy - 3); Rectangle(hdc, sx - 3, sy - 1, sx + 4, sy + 1);
        }
    }
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(hoop); DeleteObject(wood);
}
