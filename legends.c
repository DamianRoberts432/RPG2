// Legends log: a fixed 24-entry ring of great deeds (first kill of each boss,
// every tenth level, cavern hoards, falls and retirements), kept in legends.dat
// across heroes like the Hall of Records. 24 x 64 bytes, no heap.
#include <stdarg.h>
#include "game.h"

#define LEGENDS_FILE "legends.dat"
#define LEGENDS_MAX 24
typedef struct { char hero[16]; uint16_t day, pad; char text[44]; } LegendRecord;
static LegendRecord legends[LEGENDS_MAX];
static int legend_count = -1;

static void LoadLegends(void) {
    if (legend_count >= 0) return;
    legend_count = 0;
    FILE *f = fopen(LEGENDS_FILE, "rb");
    if (!f) return;
    legend_count = (int)fread(legends, sizeof(LegendRecord), LEGENDS_MAX, f);
    fclose(f);
}

void AddLegend(const char *fmt, ...) {
    LoadLegends();
    if (legend_count == LEGENDS_MAX) { memmove(legends, legends + 1, sizeof(LegendRecord) * (LEGENDS_MAX - 1)); legend_count--; }
    LegendRecord *r = &legends[legend_count++];
    memset(r, 0, sizeof(*r));
    strncpy(r->hero, player_name, sizeof(r->hero) - 1);
    r->day = (uint16_t)GameDayOfYear();
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->text, sizeof(r->text), fmt, ap);
    va_end(ap);
    FILE *f = fopen(LEGENDS_FILE, "wb");
    if (!f) return;
    fwrite(legends, sizeof(LegendRecord), legend_count, f);
    fclose(f);
}

void DrawLegends(HDC hdc, int x, int y, int rows) {
    LoadLegends();
    char line[96];
    SetTextColor(hdc, RGB(255, 225, 130));
    TextOut(hdc, x, y, "LEGENDS", 7);
    if (legend_count == 0) {
        const char *none = "No deeds are told yet. Slay a boss or reach level 10.";
        SetTextColor(hdc, RGB(170, 170, 180));
        TextOut(hdc, x, y + 28, none, (int)strlen(none));
        return;
    }
    SetTextColor(hdc, RGB(225, 215, 190));
    for (int i = 0; i < rows && i < legend_count; i++) {
        LegendRecord *r = &legends[legend_count - 1 - i];
        snprintf(line, sizeof(line), "Day %-3d  %s %s", r->day + 1, r->hero, r->text);
        TextOut(hdc, x, y + 28 + i * 24, line, (int)strlen(line));
    }
}
