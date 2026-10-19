#include "game.h"

#define DAYS_PER_YEAR (DAYS_PER_SEASON * 4)
#define RIPE_STAGE 4

FarmPlot farm_plots[MAX_FARM_PLOTS];

int GameDayOfYear(void) { return (int)current_season * DAYS_PER_SEASON + SeasonDay(); }

// 0 tilled, 1 seed, 2 sprout, 3 leafy, 4 ripe: one stage per game day.
static int PlotStage(const FarmPlot *p) {
    if (!p->planted) return 0;
    int days = (GameDayOfYear() - p->planted_day + DAYS_PER_YEAR) % DAYS_PER_YEAR;
    return days >= RIPE_STAGE - 1 ? RIPE_STAGE : 1 + days;
}

static FarmPlot *FindPlot(int x, int y) {
    for (int i = 0; i < MAX_FARM_PLOTS; i++)
        if (farm_plots[i].active && farm_plots[i].screen_id == current_screen_index && farm_plots[i].x == x && farm_plots[i].y == y)
            return &farm_plots[i];
    return NULL;
}

static int FarmableBiome(void) {
    switch (current_biome) {
        case BIOME_PRAIRIE: case BIOME_GRASSLANDS: case BIOME_FOREST: case BIOME_SWAMP:
        case BIOME_ISLAND: case BIOME_LAKE: case BIOME_RIVER: return !in_cave;
        default: return 0;
    }
}

// X on an existing plot: plant seeds, check growth or harvest.
int TryFarmPlot(int x, int y) {
    FarmPlot *p = FindPlot(x, y);
    if (!p) return 0;
    int stage = PlotStage(p);
    if (stage == 0) {
        if (!ConsumeMaterialFromInventory("Seeds", 1)) { strcpy(arpg_action_log, "FARM: Tilled soil. Buy Seeds at the merchant to plant here."); return 1; }
        p->planted = 1; p->planted_day = GameDayOfYear();
        NoteQuestPlanted();
        strcpy(arpg_action_log, "FARM: Planted turnip seeds. They ripen in 3 days.");
        return 1;
    }
    if (stage < RIPE_STAGE) { sprintf(arpg_action_log, "FARM: The turnips are growing (stage %d of %d).", stage, RIPE_STAGE); return 1; }
    int n = 2 + rand() % 2;
    int seeds = 0;
    for (int k = 0; k < n; k++) seeds += 3 + rand() % 2; // each turnip gives back 3-4 seeds
    AddStackableItem("Turnip", "Food", n);
    AddStackableItem("Seeds", "Material", seeds);
    RecalculateCarriedWeight();
    p->planted = 0;
    sprintf(arpg_action_log, "FARM: Harvested %d turnips and saved %d seeds!", n, seeds);
    return 1;
}

// X with a Hoe on bare ground tills a new plot.
int TryTillGround(int x, int y) {
    if (!HasInventoryItem("Hoe") || !FarmableBiome()) return 0;
    if (x < 1 || y < 1 || x >= MAP_SIZE - 1 || y >= MAP_SIZE - 1 || VISUAL_MAP[y][x] != 0) return 0;
    for (int i = 0; i < MAX_FARM_PLOTS; i++) if (!farm_plots[i].active) {
        farm_plots[i] = (FarmPlot){ 1, current_screen_index, 0, 0, (int8_t)x, (int8_t)y, { 0, 0 } };
        strcpy(arpg_action_log, "FARM: Tilled the soil. Press X with Seeds to plant.");
        return 1;
    }
    sprintf(arpg_action_log, "FARM: You can't tend more than %d plots.", MAX_FARM_PLOTS);
    return 1;
}

static COLORREF Dim(int r, int g, int b, float a) { return RGB((int)(r * a), (int)(g * a), (int)(b * a)); }

static void Fill(HDC hdc, COLORREF c, POINT *pts, int n) {
    HBRUSH br = CreateSolidBrush(c); HGDIOBJ old = SelectObject(hdc, br);
    Polygon(hdc, pts, n);
    SelectObject(hdc, old); DeleteObject(br);
}

void DrawFarmPlots(HDC hdc) {
    float a = (float)fabs(sin(day_night_cycle_accumulator));
    if (a < 0.35f) a = 0.35f;
    HGDIOBJ old_pen = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int i = 0; i < MAX_FARM_PLOTS; i++) {
        FarmPlot *p = &farm_plots[i];
        if (!p->active || p->screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords((float)p->x, (float)p->y, &sx, &sy);
        int hw = TILE_WIDTH / 2 - 4, hh = TILE_HEIGHT / 2 - 2, cy = sy + TILE_HEIGHT / 2;
        POINT soil[] = { { sx, cy - hh }, { sx + hw, cy }, { sx, cy + hh }, { sx - hw, cy } };
        Fill(hdc, Dim(110, 72, 40, a), soil, 4);
        HPEN furrow = CreatePen(PS_SOLID, 1, Dim(70, 44, 24, a)); SelectObject(hdc, furrow);
        for (int k = -1; k <= 1; k++) { MoveToEx(hdc, sx - hw / 2 + k * 10, cy - hh / 2 + k * 5, NULL); LineTo(hdc, sx + hw / 2 + k * 10, cy + hh / 2 + k * 5); }
        SelectObject(hdc, GetStockObject(NULL_PEN)); DeleteObject(furrow);
        int stage = PlotStage(p);
        if (stage == 0) continue;
        for (int k = -1; k <= 1; k++) {
            int px = sx + k * 12, py = cy + k * 6;
            if (stage == 1) { Fill(hdc, Dim(200, 180, 120, a), (POINT[]){ { px - 1, py - 1 }, { px + 1, py - 1 }, { px + 1, py + 1 }, { px - 1, py + 1 } }, 4); continue; }
            int h = stage == 2 ? 4 : stage == 3 ? 8 : 10;
            if (stage == RIPE_STAGE) {
                HBRUSH root = CreateSolidBrush(Dim(220, 200, 230, a)); HGDIOBJ ob = SelectObject(hdc, root);
                Ellipse(hdc, px - 4, py - 3, px + 4, py + 3);
                SelectObject(hdc, ob); DeleteObject(root);
            }
            Fill(hdc, Dim(60, 160, 60, a), (POINT[]){ { px, py }, { px - 4, py - h }, { px - 1, py - h + 2 } }, 3);
            Fill(hdc, Dim(80, 185, 70, a), (POINT[]){ { px, py }, { px + 4, py - h }, { px + 1, py - h + 2 } }, 3);
        }
    }
    SelectObject(hdc, old_pen);
}
