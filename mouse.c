#include "game.h"

#define BELT_SLOTS 10
static volatile int m_x = 0, m_y = 0, m_lclick = 0, m_rclick = 0, m_wheel = 0, m_xbtn = 0;
static volatile DWORD last_mouse_event = 0;
static int walk_active = 0, target_enemy = -1, belt_sel = 0;
static float walk_x, walk_y;
static DWORD last_mouse_attack = 0;

void MouseEvent(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) { m_x = (short)LOWORD(lp); m_y = (short)HIWORD(lp); }
    if (msg == WM_LBUTTONDOWN) m_lclick = 1;
    else if (msg == WM_RBUTTONDOWN) m_rclick = 1;
    else if (msg == WM_MOUSEWHEEL) m_wheel += GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1;
    else if (msg == WM_XBUTTONDOWN) m_xbtn = GET_XBUTTON_WPARAM(wp);
    if (msg != WM_MOUSEMOVE) last_mouse_event = GetTickCount();
}

int MouseBusy(void) { return walk_active || GetTickCount() - last_mouse_event < 1000; }

// Inverse of GetIsoCoords (tile centre is half a tile below its top corner).
static void ScreenToWorld(int sx, int sy, float *wx, float *wy) {
    float a = (float)(sx - WINDOW_WIDTH / 2 + cam_x) / (TILE_WIDTH / 2);
    float b = (float)(sy - TILE_HEIGHT / 2 - WINDOW_HEIGHT / 2 + cam_y) / (TILE_HEIGHT / 2);
    *wx = (a + b) / 2.0f; *wy = (b - a) / 2.0f;
}

// The belt holds the first ten weapons, tools and food in the pack.
static int BeltItems(int *idx) {
    int n = 0;
    for (int i = 0; i < player_item_count && n < BELT_SLOTS; i++) {
        const char *s = player_inventory[i].slot;
        if (!strcmp(s, "Weapon") || !strcmp(s, "Food") || !strcmp(s, "Tool")) idx[n++] = i;
    }
    return n;
}

static void UseBeltSlot(int k) {
    int idx[BELT_SLOTS], n = BeltItems(idx);
    if (k < 0 || k >= n) return;
    selected_inv_index = idx[k];
    const char *s = player_inventory[idx[k]].slot;
    if (!strcmp(s, "Food")) ConsumeSelectedFood();
    else if (!strcmp(s, "Weapon")) ToggleEquipSelectedItem();
    else HandleInventoryPrimaryAction();
}

static RECT BeltRect(int k) {
    int w = 46, l = WINDOW_WIDTH / 2 - BELT_SLOTS * w / 2 + k * w;
    RECT r = { l, WINDOW_HEIGHT - 74, l + 42, WINDOW_HEIGHT - 32 };
    return r;
}

static int BeltSlotAt(int x, int y) {
    POINT p = { x, y };
    for (int k = 0; k < BELT_SLOTS; k++) { RECT r = BeltRect(k); if (PtInRect(&r, p)) return k; }
    return -1;
}

void UpdateMouseInput(float *dx, float *dy) {
    int busy = town_menu || vendor_menu_open || is_menu_open;
    static int key_was[BELT_SLOTS];
    for (int k = 0; k < BELT_SLOTS; k++) {
        int down = (GetAsyncKeyState(k == 9 ? '0' : '1' + k) & 0x8000) != 0;
        if (down && !key_was[k] && !busy) UseBeltSlot(k);
        key_was[k] = down;
    }
    if (m_wheel) { belt_sel = ((belt_sel + m_wheel) % BELT_SLOTS + BELT_SLOTS) % BELT_SLOTS; m_wheel = 0; }
    if (m_xbtn) {
        if (!busy) {
            if (m_xbtn == XBUTTON1) { if (bow_equipped && !player_arrow.active) FirePlayerArrow(1); }
            else UseBeltSlot(belt_sel);
        }
        m_xbtn = 0;
    }
    if (m_rclick) { if (!busy) FireClassAbility(); m_rclick = 0; }
    if (m_lclick) {
        m_lclick = 0;
        int slot = busy ? -1 : BeltSlotAt(m_x, m_y);
        if (slot >= 0) UseBeltSlot(slot);
        else if (!busy) {
            ScreenToWorld(m_x, m_y, &walk_x, &walk_y);
            StoreEnemy();
            target_enemy = EnemyAtPoint(walk_x, walk_y, 1.0f);
            walk_active = 1;
        }
    }
    if (*dx != 0.0f || *dy != 0.0f || busy) { walk_active = 0; return; }
    if (!walk_active) return;
    float tx = walk_x, ty = walk_y;
    if (target_enemy >= 0) {
        if (enemies[target_enemy].hearts <= 0.0f) { walk_active = 0; return; }
        tx = enemies[target_enemy].x; ty = enemies[target_enemy].y;
    }
    float ddx = tx - player_x, ddy = ty - player_y, d = (float)sqrt(ddx * ddx + ddy * ddy);
    player_facing = fabs(ddx) > fabs(ddy) ? (ddx < 0 ? FACE_LEFT : FACE_RIGHT) : (ddy < 0 ? FACE_UP : FACE_DOWN);
    if (target_enemy >= 0 && d < 1.5f) { // walked up to it: keep swinging until it falls
        if (GetTickCount() - last_mouse_attack > 400) { PerformMeleeAttack(); last_mouse_attack = GetTickCount(); }
        return;
    }
    if (d < 0.25f) { walk_active = 0; return; }
    *dx = ddx / d; *dy = ddy / d;
}

static void Doodle(HDC hdc, int cx, int cy, const InventoryItem *it) {
    const char *n = it->name;
    COLORREF c = RGB(200, 200, 210);
    HPEN pen = CreatePen(PS_SOLID, 2, c); HGDIOBJ op = SelectObject(hdc, pen);
    HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    if (strstr(n, "Bow")) { Arc(hdc, cx - 12, cy - 14, cx + 4, cy + 14, cx - 4, cy - 14, cx - 4, cy + 14); MoveToEx(hdc, cx - 4, cy - 13, NULL); LineTo(hdc, cx - 4, cy + 13); }
    else if (!strcmp(it->slot, "Weapon")) { MoveToEx(hdc, cx - 8, cy + 10, NULL); LineTo(hdc, cx + 9, cy - 11); MoveToEx(hdc, cx - 9, cy + 1, NULL); LineTo(hdc, cx - 1, cy + 9); }
    else if (!strcmp(n, "Apple")) { HBRUSH r = CreateSolidBrush(RGB(210, 45, 35)); SelectObject(hdc, r); Ellipse(hdc, cx - 9, cy - 7, cx + 9, cy + 10); SelectObject(hdc, ob); DeleteObject(r); MoveToEx(hdc, cx, cy - 7, NULL); LineTo(hdc, cx + 3, cy - 12); }
    else if (!strcmp(n, "Fish")) { HBRUSH b = CreateSolidBrush(RGB(80, 175, 220)); SelectObject(hdc, b); Ellipse(hdc, cx - 11, cy - 5, cx + 6, cy + 5); POINT t[3] = { { cx + 5, cy }, { cx + 12, cy - 6 }, { cx + 12, cy + 6 } }; Polygon(hdc, t, 3); SelectObject(hdc, ob); DeleteObject(b); }
    else if (!strcmp(n, "Matches")) { for (int k = -1; k <= 1; k++) { MoveToEx(hdc, cx + k * 5, cy + 10, NULL); LineTo(hdc, cx + k * 5, cy - 6); } HBRUSH r = CreateSolidBrush(RGB(220, 50, 40)); RECT tip = { cx - 7, cy - 10, cx + 8, cy - 6 }; FillRect(hdc, &tip, r); DeleteObject(r); }
    else if (strstr(n, "Fishing")) { MoveToEx(hdc, cx - 10, cy + 10, NULL); LineTo(hdc, cx + 10, cy - 10); LineTo(hdc, cx + 10, cy + 6); }
    else if (strstr(n, "Guide")) { Rectangle(hdc, cx - 9, cy - 11, cx + 9, cy + 11); MoveToEx(hdc, cx - 5, cy - 4, NULL); LineTo(hdc, cx + 5, cy - 4); }
    else { HBRUSH b = CreateSolidBrush(RGB(140, 100, 60)); RECT r = { cx - 8, cy - 8, cx + 8, cy + 8 }; FillRect(hdc, &r, b); DeleteObject(b); }
    SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pen);
}

void DrawBelt(HDC hdc) {
    if (is_menu_open || town_menu || vendor_menu_open) return;
    int idx[BELT_SLOTS], n = BeltItems(idx);
    SetBkMode(hdc, TRANSPARENT);
    for (int k = 0; k < BELT_SLOTS; k++) {
        RECT r = BeltRect(k);
        HBRUSH bg = CreateSolidBrush(RGB(22, 24, 30)); FillRect(hdc, &r, bg); DeleteObject(bg);
        HBRUSH fr = CreateSolidBrush(k == belt_sel ? RGB(255, 225, 90) : RGB(90, 95, 105)); FrameRect(hdc, &r, fr); DeleteObject(fr);
        char lab[8]; snprintf(lab, sizeof(lab), "%d", (k + 1) % 10);
        SetTextColor(hdc, RGB(160, 160, 170)); TextOut(hdc, r.left + 3, r.top + 1, lab, (int)strlen(lab));
        if (k >= n) continue;
        InventoryItem *it = &player_inventory[idx[k]];
        Doodle(hdc, (r.left + r.right) / 2, (r.top + r.bottom) / 2 + 2, it);
        if (it->is_equipped) { HBRUSH eq = CreateSolidBrush(RGB(90, 200, 120)); RECT e = { r.left + 2, r.bottom - 4, r.right - 2, r.bottom - 2 }; FillRect(hdc, &e, eq); DeleteObject(eq); }
        if (it->quantity > 1) { snprintf(lab, sizeof(lab), "%d", it->quantity); SetTextColor(hdc, RGB(235, 235, 235)); TextOut(hdc, r.right - 16, r.bottom - 18, lab, (int)strlen(lab)); }
    }
}
