// Hall of Records: a fixed 16-entry ring of your heroes' falls and retirements,
// kept in hall.dat (16 x 48 bytes, no heap) so it survives deleted saves.
#include "game.h"

#define HALL_FILE "hall.dat"
#define HALL_MAX 16
typedef struct { char name[16]; uint8_t cls, race, level, retired; uint16_t day, pad; char cause[24]; } HallRecord;
static HallRecord hall[HALL_MAX];
static int hall_count = -1;

static void LoadHall(void) {
    if (hall_count >= 0) return;
    hall_count = 0;
    FILE *f = fopen(HALL_FILE, "rb");
    if (!f) return;
    hall_count = (int)fread(hall, sizeof(HallRecord), HALL_MAX, f);
    fclose(f);
}

void RecordHallEntry(const char *cause, int retired) {
    LoadHall();
    if (hall_count == HALL_MAX) { memmove(hall, hall + 1, sizeof(HallRecord) * (HALL_MAX - 1)); hall_count--; }
    HallRecord *r = &hall[hall_count++];
    memset(r, 0, sizeof(*r));
    strncpy(r->name, player_name, sizeof(r->name) - 1);
    snprintf(r->cause, sizeof(r->cause), "%s", cause);
    r->cls = (uint8_t)selected_class; r->race = (uint8_t)selected_race;
    r->level = (uint8_t)(player_level > 255 ? 255 : player_level); r->retired = (uint8_t)retired;
    r->day = (uint16_t)GameDayOfYear();
    FILE *f = fopen(HALL_FILE, "wb");
    if (!f) return;
    fwrite(hall, sizeof(HallRecord), hall_count, f);
    fclose(f);
}

void DrawHallOfRecords(HDC hdc, int x, int y) {
    LoadHall();
    char line[128];
    SetTextColor(hdc, RGB(255, 225, 130));
    TextOut(hdc, x, y, "HALL OF RECORDS", 15);
    if (hall_count == 0) {
        SetTextColor(hdc, RGB(170, 170, 180));
        TextOut(hdc, x, y + 28, "No hero has fallen yet.", 23);
        return;
    }
    for (int i = 0; i < 8 && i < hall_count; i++) {
        HallRecord *r = &hall[hall_count - 1 - i];
        snprintf(line, sizeof(line), "%-15s Lv %-3d %s %s  -  %s (day %d)", r->name, r->level,
                 race_names[r->race % 5], class_names[r->cls % 5], r->cause, r->day + 1);
        SetTextColor(hdc, r->retired ? RGB(170, 210, 255) : RGB(230, 160, 150));
        TextOut(hdc, x, y + 28 + i * 24, line, (int)strlen(line));
    }
}
