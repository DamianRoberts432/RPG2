#ifndef WEATHER_H
#define WEATHER_H

typedef enum { SEASON_SPRING, SEASON_SUMMER, SEASON_FALL, SEASON_WINTER } SeasonType;
#define DAY_LENGTH_SECONDS (40.0f * 60.0f) // 20 min of daylight + 20 min of night
#define GAME_HOUR_SECONDS (DAY_LENGTH_SECONDS / 24.0f)
#define DAYS_PER_SEASON 5
#define SEASON_LENGTH_SECONDS (DAY_LENGTH_SECONDS * DAYS_PER_SEASON)

extern SeasonType current_season;
extern float weather_intensity; // 0..1 strength of current_weather (fades in/out)
extern float snow_cover;        // 0..1 temporary ground snow, never saved
extern float wind_gust;         // -1..1 occasional gust layered on the breeze
extern const char *season_names[];
extern const char *weather_names[];
extern char weather_source_label[96];

void InitSeasonAndWeather(void);
void UpdateSeasonAndWeather(void);
void SleepUntilMorning(void);
int SeasonDay(void);
const char *CalendarMonthName(void);
void GetGameClock(int *hour, int *minute);
float SeasonClockSeconds(void);
void RestoreCalendar(int season, float seconds_into_season);
int IsDeciduousTree(int tile_seed);
int TreeHasApples(int tile_seed);
COLORREF ApplySeasonToGround(COLORREF base);
void DrawWeatherEffects(HDC hdc);
int IsAdverseWeather(void);
void DrawSeasonalGroundCover(HDC hdc);

#endif // WEATHER_H
