#ifndef WORLDMAP_H
#define WORLDMAP_H

#define POI_VILLAGE        0x01
#define POI_CASTLE         0x02
#define POI_TREASURE_FOUND 0x04

void RecordCurrentScreenOnMap(void);
void MarkMapTreasureFound(int origin_cell);
int IsMapTreasureFound(int origin_cell);
void DrawWorldMapPanel(HDC hdc, int left, int top, int max_w, int max_h);
int SaveWorldMapState(void);
int LoadWorldMapState(void);

#endif // WORLDMAP_H
