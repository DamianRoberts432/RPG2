#ifndef WEATHER_H
#include "fixed.h"
#define WEATHER_H

typedef enum { SEASON_SPRING, SEASON_SUMMER, SEASON_FALL, SEASON_WINTER } SeasonType;
#define DAY_LENGTH_SECONDS (24 * 60) // 1 real second = 1 game minute
#define GAME_HOUR_SECONDS (DAY_LENGTH_SECONDS / 24)
#define DAYS_PER_SEASON 30
#define SEASON_LENGTH_SECONDS (DAY_LENGTH_SECONDS * DAYS_PER_SEASON)
// The calendar counts milliseconds: a season is 43,200 seconds, too many for 16.16 fixed point.
#define DAY_MS (DAY_LENGTH_SECONDS * 1000)
#define SEASON_MS (DAY_MS * DAYS_PER_SEASON)

extern SeasonType current_season;
extern fx weather_intensity; // 0..1 strength of current_weather (fades in/out)
extern fx snow_cover;        // 0..1 temporary ground snow, never saved
extern fx wind_gust;         // -1..1 occasional gust layered on the breeze
extern const char *season_names[];
extern const char *weather_names[];
extern char weather_source_label[96];

void InitSeasonAndWeather(void);
void UpdateSeasonAndWeather(void);
int SleepToNextPhase(void);
int SeasonDay(void);
const char *CalendarMonthName(void);
void GetGameClock(int *hour, int *minute);
int32_t SeasonClockMs(void);
void RestoreCalendar(int season, int32_t ms_into_season);
int IsDeciduousTree(int tile_seed);
int TreeHasApples(int tile_seed);
COLORREF ApplySeasonToGround(COLORREF base);
void DrawWeatherEffects(HDC hdc);
int IsAdverseWeather(void);
void DrawSeasonalGroundCover(HDC hdc);

#endif // WEATHER_H
