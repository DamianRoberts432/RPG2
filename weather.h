#ifndef WEATHER_H
#define WEATHER_H

typedef enum { SEASON_SPRING, SEASON_SUMMER, SEASON_FALL, SEASON_WINTER } SeasonType;
#define SEASON_LENGTH_SECONDS (45.0f * 60.0f)

extern SeasonType current_season;
extern float weather_intensity; // 0..1 strength of current_weather (fades in/out)
extern float snow_cover;        // 0..1 temporary ground snow, never saved
extern float wind_gust;         // -1..1 occasional gust layered on the breeze
extern const char *season_names[];
extern const char *weather_names[];
extern char weather_source_label[96];

void InitSeasonAndWeather(void);
void UpdateSeasonAndWeather(void);
float SeasonSecondsRemaining(void);
int IsDeciduousTree(int tile_seed);
int TreeHasApples(int tile_seed);
COLORREF ApplySeasonToGround(COLORREF base);
void DrawWeatherEffects(HDC hdc);
int IsAdverseWeather(void);
void DrawSeasonalGroundCover(HDC hdc);

#endif // WEATHER_H
