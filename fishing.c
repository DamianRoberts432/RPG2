#include "game.h"

static int fish_state = 0; // 0 idle, 1 bobber floating, 2 fish nibbling (bobber dips)
static float bobber_x, bobber_y, cast_x, cast_y;
static DWORD next_event = 0, dip_until = 0, rumble_until = 0;
static int dips_left = 0, fishy = 0;

static void Rumble(int on) {
    XINPUT_VIBRATION v = { 0 };
    if (on) { v.wLeftMotorSpeed = 9000; v.wRightMotorSpeed = 6000; }
    XInputSetState(0, &v);
}

int FishingActive(void) { return fish_state != 0; }

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
    dips_left = fishy ? 2 + rand() % 3 : 0;
    fish_state = 1; fishing_active = 1;
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
    if (AddLootToInventory(LOOT_FISH, 1)) { EndFishing("FISHING: Got one! A fish goes into your pack."); TryCardDrop(-1); }
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
        if (dips_left <= 0) { EndFishing(fishy ? "FISHING: The fish lost interest." : "FISHING: Nothing's biting here. Try other water."); return; }
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
    HPEN line = CreatePen(PS_SOLID, 1, RGB(225, 225, 225)); HGDIOBJ op = SelectObject(hdc, line);
    MoveToEx(hdc, px + 8, py - 30, NULL); LineTo(hdc, bx, by - 3 + bob);
    SelectObject(hdc, op); DeleteObject(line);
    if (fish_state == 2) {
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
