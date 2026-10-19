#include "game.h"

static DWORD cast_at = 0; // the bobber flies out in an arc for CAST_MS
#define CAST_MS 450
static int fish_state = 0; // 0 idle, 1 bobber floating, 2 fish nibbling (bobber dips)
static float bobber_x, bobber_y, cast_x, cast_y;
static DWORD next_event = 0, dip_until = 0, rumble_until = 0;
static int dips_left = 0, fishy = 0, told_quiet = 0;

static void Rumble(int on) {
    XINPUT_VIBRATION v = { 0 };
    if (on) { v.wLeftMotorSpeed = 9000; v.wRightMotorSpeed = 6000; }
    XInputSetState(0, &v);
}

int FishingActive(void) { return fish_state != 0; }

int FishingBobberPos(float *x, float *y, int *nibble) {
    if (!fish_state) return 0;
    *x = bobber_x; *y = bobber_y; *nibble = fish_state == 2;
    return 1;
}

static void EndFishing(const char *msg) {
    fish_state = 0; fishing_active = 0;
    if (rumble_until) { Rumble(0); rumble_until = 0; }
    if (msg) strcpy(arpg_action_log, msg);
}

void CastFishingLine(void) {
    int x = (int)player_x, y = (int)player_y;
    if (player_facing == FACE_UP) y--;
    else if (player_facing == FACE_DOWN) y++;
    else if (player_facing == FACE_LEFT) x--;
    else x++;
    bobber_x = x + 0.5f; bobber_y = y + 0.5f;
    cast_x = player_x; cast_y = player_y;
    fishy = IsFacingFishWater();
    dips_left = fishy ? 2 + rand() % 3 : 0; told_quiet = 0;
    fish_state = 1; fishing_active = 1; cast_at = GetTickCount();
    next_event = GetTickCount() + 1500 + rand() % 2500;
    strcpy(arpg_action_log, "FISHING: You cast your line. When the bobber dips, press X.");
}

// Usually 60/40 in your favor, now and then 70/30; luck pushes it up to 90/10.
static int CatchChance(void) {
    int c = (rand() % 5 == 0 ? 70 : 60) + stat_luck * 3;
    return c > 90 ? 90 : c;
}

void FishingPressX(void) {
    if (fish_state != 2) { EndFishing("FISHING: You reel in. Nothing on the line yet."); return; }
    if (rand() % 100 >= CatchChance()) { EndFishing("FISHING: It slipped the hook! Cast again."); return; }
    if (AddLootToInventory(LOOT_FISH, 1)) { EndFishing("FISHING: Got one! A fish goes into your pack."); TryCardDrop(-1); NoteQuestFish(); }
    else EndFishing("FISHING: You caught one, but your pack is too heavy to carry it.");
}

static void UpdateFishing(void) {
    if (!fish_state) return;
    DWORD now = GetTickCount();
    if (rumble_until && now > rumble_until) { Rumble(0); rumble_until = 0; }
    float dx = player_x - cast_x, dy = player_y - cast_y;
    if (dx * dx + dy * dy > 0.25f) { EndFishing("FISHING: You walk off and reel in."); return; }
    if (fish_state == 2 && now > dip_until) {
        fish_state = 1;
        next_event = now + 1200 + rand() % 2200;
    }
    if (fish_state == 1 && now >= next_event) {
        if (dips_left <= 0) { // the line stays out until you reel in with X; fish may wander back
            if (!told_quiet) { strcpy(arpg_action_log, fishy ? "FISHING: The fish lost interest. Wait, or press X to reel in." : "FISHING: Nothing's biting here. Press X to reel in."); told_quiet = 1; }
            next_event = now + 5000 + rand() % 7000;
            if (fishy && rand() % 3 == 0) dips_left = 1 + rand() % 2;
            return;
        }
        dips_left--;
        fish_state = 2; dip_until = now + 650;
        Rumble(1); rumble_until = now + 220;
    }
}

void DrawFishing(HDC hdc) {
    UpdateFishing();
    if (!fish_state) return;
    int px, py, bx, by;
    GetIsoCoords(player_x, player_y, &px, &py);
    GetIsoCoords(bobber_x, bobber_y, &bx, &by);
    int bob = fish_state == 2 ? 4 : (int)(sin(GetTickCount() / 300.0) * 1.5);
    // Rod held in the hand, tipped out toward the water; it bends when a fish nibbles.
    int dir = bx >= px ? 1 : -1;
    int hx = px + dir * 5, hy = py - 16;
    float t = (GetTickCount() - cast_at) / (float)CAST_MS; if (t > 1.0f) t = 1.0f;
    int tipx = hx + dir * (t < 1.0f ? (int)(6 + 16 * t) : 22), tipy = hy - (t < 1.0f ? (int)(32 - 12 * t) : 20) + (fish_state == 2 ? 6 : 0);
    HPEN rod = CreatePen(PS_SOLID, 2, RGB(120, 80, 40)); HGDIOBJ op = SelectObject(hdc, rod);
    MoveToEx(hdc, hx, hy, NULL); LineTo(hdc, tipx, tipy);
    SelectObject(hdc, op); DeleteObject(rod);
    if (t < 1.0f) { // still flying: arc from the rod tip to where it lands
        bx = (int)(tipx + (bx - tipx) * t); by = (int)(tipy + (by - tipy) * t - sin(t * 3.14159f) * 30.0f); bob = 0;
    }
    HPEN line = CreatePen(PS_SOLID, 1, RGB(225, 225, 225)); op = SelectObject(hdc, line);
    POINT sag[4] = { { tipx, tipy }, { tipx + (bx - tipx) / 3, tipy + (by - tipy) / 3 + (t < 1.0f ? 0 : 10) },
                     { tipx + 2 * (bx - tipx) / 3, tipy + 2 * (by - tipy) / 3 + (t < 1.0f ? 0 : 8) }, { bx, by - 3 + bob } };
    PolyBezier(hdc, sag, 4);
    SelectObject(hdc, op); DeleteObject(line);
    if (fish_state == 2 && t >= 1.0f) {
        HPEN ring = CreatePen(PS_SOLID, 1, RGB(200, 230, 255)); op = SelectObject(hdc, ring);
        HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, bx - 9, by - 2, bx + 9, by + 5);
        SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(ring);
    }
    HBRUSH red = CreateSolidBrush(RGB(220, 40, 40)), white = CreateSolidBrush(RGB(245, 245, 245));
    HGDIOBJ ob = SelectObject(hdc, white); op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, bx - 3, by - 3 + bob, bx + 4, by + 3 + bob);
    SelectObject(hdc, red);
    Ellipse(hdc, bx - 3, by - 6 + bob, bx + 4, by + bob);
    SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(red); DeleteObject(white);
}
