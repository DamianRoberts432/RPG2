#ifndef FARMING_H
#define FARMING_H

// Farming: a Hoe tills bare ground with X, Seeds planted with X grow into
// Turnips over four stages (one per game day), and X harvests the ripe crop.
#define MAX_FARM_PLOTS 48
typedef struct { int32_t active, screen_id, planted, planted_day; int8_t x, y, pad[2]; } FarmPlot;
extern FarmPlot farm_plots[MAX_FARM_PLOTS];

int GameDayOfYear(void);
int TryFarmPlot(int x, int y);
int TryTillGround(int x, int y);
void DrawFarmPlots(HDC hdc);

// Worn footpaths (see paths.c).
#define MAX_WORN_TILES 384
typedef struct { int32_t screen_id; uint8_t x, y, wear, active; } WornTile;
extern WornTile worn_tiles[MAX_WORN_TILES];
void UpdateFootpaths(void);
void DrawFootpaths(HDC hdc);

#endif
