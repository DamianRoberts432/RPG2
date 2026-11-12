#include "game.h"
#include "render_world.h"
#include <wininet.h>

// Seasons and world-wide weather. One global weather state covers every
// screen, so moving between screens never changes the weather; it shifts
// gradually on its own timer. Seasons last DAYS_PER_SEASON days
// (20 min day + 20 min night each) and start from the PC's date. If weather.ini names a location, the
// starting weather comes from Open-Meteo (free, no API key).

SeasonType current_season = SEASON_SPRING;
fx weather_intensity = FxI(0);
fx snow_cover = FxI(0);
fx wind_gust = FxI(0);
const char *season_names[] = { "Spring", "Summer", "Fall", "Winter" };
const char *weather_names[] = { "Fair", "Rain", "Snow", "Cloudy" };
char weather_source_label[96] = "PC clock/date";

static int32_t season_ms = 0; // milliseconds into the current season
static int calendar_restored = 0; // season/clock came from the save file
static DWORD last_update_tick = 0;
static WeatherType target_weather = WEATHER_CLEAR;
static fx target_intensity = FxI(0);
static fx front_seconds_left = FxI(0);
static fx gust_time = FxI(0), gust_duration = FxI(0), gust_power = FxI(0), gust_check_timer = FxI(0);
static int gust_sign = 1;
static fx cloud_hold = FxI(0); // seconds of dry overcast between weather fronts
#define CLOUD_HOLD_SECONDS FxI(60)
static uint32_t weather_rng = 0x9E3779B9u;

// Separate generator so weather never depends on the per-screen srand() seed.
static fx WeatherRandom(void) {
    weather_rng ^= weather_rng << 13; weather_rng ^= weather_rng >> 17; weather_rng ^= weather_rng << 5;
    return (fx)((weather_rng & 0xFFFFFF) >> 8);
}

// Weather follows a plan in half-day blocks: a fair spell of 1-7 days (now and
// then a partly cloudy half-day that dulls the colors), a windy half-day as a
// storm blows in, overcast, 1-3 days of rain (snow in winter), then overcast
// again before it clears. Each block is 12 game hours.
enum { PLAN_FAIR, PLAN_WINDY, PLAN_CLOUD_IN, PLAN_STORM, PLAN_CLOUD_OUT };
static int plan_phase = PLAN_CLOUD_OUT, plan_blocks_left = 0;
static fx storm_wind = FxI(0);      // extra breeze while a storm blows in
static fx since_rain = FxI(30000);    // seconds since it last rained (fog follows rain)

static void RollNextFront(void) {
    if (plan_blocks_left <= 0) {
        plan_phase = (plan_phase + 1) % 5;
        if (plan_phase == PLAN_FAIR) plan_blocks_left = 2 + FxTrunc(WeatherRandom() * 12);
        else if (plan_phase == PLAN_WINDY) plan_blocks_left = 1;
        else if (plan_phase == PLAN_CLOUD_IN) plan_blocks_left = 1 + (WeatherRandom() < FX(0.5));
        else if (plan_phase == PLAN_STORM) plan_blocks_left = 2 * (1 + FxTrunc(WeatherRandom() * 3));
        else plan_blocks_left = 1 + (WeatherRandom() < FX(0.4));
    }
    plan_blocks_left--;
    storm_wind = plan_phase == PLAN_WINDY ? FxI(1) : plan_phase == PLAN_CLOUD_IN ? FX(0.5) : FxI(0);
    if (plan_phase == PLAN_FAIR || plan_phase == PLAN_WINDY) {
        int partly = plan_phase == PLAN_FAIR && WeatherRandom() < FX(0.25);
        target_weather = partly ? WEATHER_CLOUDY : WEATHER_CLEAR;
        target_intensity = partly ? FX(0.4) : FxI(0);
    } else if (plan_phase == PLAN_STORM) {
        fx snow = current_season == SEASON_WINTER ? FX(0.6) : current_season == SEASON_SUMMER ? FxI(0) : FX(0.1);
        target_weather = WeatherRandom() < snow ? WEATHER_SNOW : WEATHER_RAIN;
        target_intensity = FX(0.5) + WeatherRandom() / 2;
    } else {
        target_weather = WEATHER_CLOUDY;
        target_intensity = FX(0.8);
    }
    front_seconds_left = FxI(GAME_HOUR_SECONDS * 12);
}

static void SetSeasonFromDate(const SYSTEMTIME *t) {
    static const int first_month[4] = { 3, 6, 9, 12 };
    int m = t->wMonth;
    SeasonType s = (m >= 3 && m <= 5) ? SEASON_SPRING : (m >= 6 && m <= 8) ? SEASON_SUMMER :
                   (m >= 9 && m <= 11) ? SEASON_FALL : SEASON_WINTER;
    int months_in = (m - first_month[s] + 12) % 12;
    fx frac = FxDiv(months_in * FX(30.4) + FxI(t->wDay - 1), FxI(91));
    if (frac > FX(0.99)) frac = FX(0.99);
    current_season = s;
    // Start on the matching day of the season, at the PC's time of day.
    int day = FxTrunc(frac * DAYS_PER_SEASON);
    season_ms = day * DAY_MS + (t->wHour * 60 + t->wMinute) * (GAME_HOUR_SECONDS * 1000 / 60);
}

#define DAWN_PHASE FX(0.35757) // asin(0.35): where the |sin| < 0.35 night test flips

static fx HourOfDay(void) { return (fx)((int64_t)(season_ms % DAY_MS) * FX_ONE / (GAME_HOUR_SECONDS * 1000)); }

// |sin| of the accumulator drives daylight (0 = midnight, PI/2 = noon). The
// clock maps 6 AM-6 PM onto the daylight arc and 6 PM-6 AM onto the night arc.
static void UpdateDaylightFromClock(void) {
    fx h = HourOfDay(), phase;
    if (h < FxI(6)) phase = FxMul(DAWN_PHASE, h) / 6;
    else if (h < FxI(18)) phase = DAWN_PHASE + FxMul(FX_PI - 2 * DAWN_PHASE, h - FxI(6)) / 12;
    else phase = FX_PI - DAWN_PHASE + FxMul(DAWN_PHASE, h - FxI(18)) / 6;
    day_night_cycle_accumulator = phase;
}

static void AdvanceClock(int32_t ms) {
    season_ms += ms;
    while (season_ms >= SEASON_MS) {
        season_ms -= SEASON_MS;
        current_season = (SeasonType)((current_season + 1) % 4);
        sprintf(arpg_action_log, "SEASON: %s has arrived.", season_names[current_season]);
        RollNextFront();
    }
    UpdateDaylightFromClock();
}

// Bedroll sleep skips to the next phase: by day you wake at 9 PM, by night
// at 6 AM. Returns 1 when you wake at dawn.
int SleepToNextPhase(void) {
    int32_t day_start = season_ms / DAY_MS * DAY_MS;
    int32_t t = season_ms - day_start, dawn = 6 * GAME_HOUR_SECONDS * 1000, dusk = 21 * GAME_HOUR_SECONDS * 1000;
    if (t >= dawn && t < dusk) { AdvanceClock(dusk - t); return 0; }
    AdvanceClock((t < dawn ? dawn : DAY_MS + dawn) - t);
    return 1;
}

int SeasonDay(void) { return season_ms / DAY_MS + 1; }

// A season's three months are spread across its DAYS_PER_SEASON days.
const char *CalendarMonthName(void) {
    static const char *months[12] = { "January", "February", "March", "April", "May", "June", "July",
                                      "August", "September", "October", "November", "December" };
    static const int first_month[4] = { 3, 6, 9, 12 };
    int m = first_month[current_season] - 1 + (SeasonDay() - 1) * 3 / DAYS_PER_SEASON;
    return months[m % 12];
}

void GetGameClock(int *hour, int *minute) {
    fx h = HourOfDay();
    *hour = FxTrunc(h);
    *minute = FxTrunc((h - FxI(*hour)) * 60);
}

int32_t SeasonClockMs(void) { return season_ms; }

void RestoreCalendar(int season, int32_t ms_into_season) {
    if (season < 0 || season > 3 || !(ms_into_season >= 0 && ms_into_season < SEASON_MS)) return;
    current_season = (SeasonType)season;
    season_ms = ms_into_season;
    calendar_restored = 1;
    UpdateDaylightFromClock();
}

// ---- Optional real-world weather (Open-Meteo via WinINet, loaded at runtime
// so the build line does not need an extra library) ----
static char cfg_city[64] = "";
static double cfg_lat = 0, cfg_lon = 0;
static int cfg_has_coords = 0;
static volatile LONG live_state = 0; // 0 pending/none, 1 ready, 2 applied
static int live_code = -1;
static double live_wind_kmh = 0, live_snow_m = 0, live_lat = 0;

typedef HINTERNET (WINAPI *InternetOpenA_fn)(LPCSTR, DWORD, LPCSTR, LPCSTR, DWORD);
typedef HINTERNET (WINAPI *InternetOpenUrlA_fn)(HINTERNET, LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *InternetReadFile_fn)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL (WINAPI *InternetCloseHandle_fn)(HINTERNET);
typedef BOOL (WINAPI *InternetSetOptionA_fn)(HINTERNET, DWORD, LPVOID, DWORD);

static int HttpGet(const char *url, char *out, int out_size) {
    HMODULE lib = LoadLibraryA("wininet.dll");
    if (!lib) return 0;
    InternetOpenA_fn pOpen = (InternetOpenA_fn)(void *)GetProcAddress(lib, "InternetOpenA");
    InternetOpenUrlA_fn pOpenUrl = (InternetOpenUrlA_fn)(void *)GetProcAddress(lib, "InternetOpenUrlA");
    InternetReadFile_fn pRead = (InternetReadFile_fn)(void *)GetProcAddress(lib, "InternetReadFile");
    InternetCloseHandle_fn pClose = (InternetCloseHandle_fn)(void *)GetProcAddress(lib, "InternetCloseHandle");
    InternetSetOptionA_fn pSetOpt = (InternetSetOptionA_fn)(void *)GetProcAddress(lib, "InternetSetOptionA");
    int total = 0;
    if (pOpen && pOpenUrl && pRead && pClose) {
        HINTERNET net = pOpen("RPG2", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (net) {
            DWORD timeout_ms = 5000;
            if (pSetOpt) {
                pSetOpt(net, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
                pSetOpt(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
            }
            HINTERNET req = pOpenUrl(net, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
            if (req) {
                DWORD got = 0;
                while (total < out_size - 1 && pRead(req, out + total, (DWORD)(out_size - 1 - total), &got) && got > 0) total += (int)got;
                pClose(req);
            }
            pClose(net);
        }
    }
    out[total] = '\0';
    FreeLibrary(lib);
    return total;
}

// Finds `"key":<number>` after `section` (or anywhere when section is NULL).
static int JsonNumber(const char *json, const char *section, const char *key, double *out) {
    const char *p = section ? strstr(json, section) : json;
    if (!p) return 0;
    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    p = strstr(p, pattern);
    if (!p) return 0;
    char *end;
    double v = strtod(p + strlen(pattern), &end);
    if (end == p + strlen(pattern)) return 0;
    *out = v;
    return 1;
}

static DWORD WINAPI FetchLiveWeatherThread(LPVOID unused) {
    (void)unused;
    static char body[16384];
    char url[512];
    double lat = cfg_lat, lon = cfg_lon;
    if (!cfg_has_coords) {
        char encoded[192] = ""; int n = 0;
        for (const char *c = cfg_city; *c && n < (int)sizeof(encoded) - 4; c++) {
            if ((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '.') encoded[n++] = *c;
            else n += snprintf(encoded + n, sizeof(encoded) - n, "%%%02X", (unsigned char)*c);
        }
        encoded[n] = '\0';
        snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1", encoded);
        if (!HttpGet(url, body, sizeof(body)) || !JsonNumber(body, "\"results\"", "latitude", &lat) ||
            !JsonNumber(body, "\"results\"", "longitude", &lon)) return 0;
    }
    snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=weather_code,wind_speed_10m,snow_depth&timezone=auto", lat, lon);
    double code = -1.0;
    if (!HttpGet(url, body, sizeof(body)) || !JsonNumber(body, "\"current\":{", "weather_code", &code)) return 0;
    JsonNumber(body, "\"current\":{", "wind_speed_10m", &live_wind_kmh);
    JsonNumber(body, "\"current\":{", "snow_depth", &live_snow_m);
    live_code = (int)code;
    live_lat = lat;
    InterlockedExchange(&live_state, 1);
    return 0;
}

static void ReadWeatherConfig(void) {
    FILE *f = fopen("weather.ini", "r");
    if (!f) return;
    char line[160];
    int has_lat = 0, has_lon = 0;
    while (fgets(line, sizeof(line), f)) {
        char *v = strchr(line, '=');
        if (line[0] == ';' || line[0] == '#' || !v) continue;
        *v++ = '\0';
        v[strcspn(v, "\r\n")] = '\0';
        while (*v == ' ') v++;
        if (strstr(line, "latitude")) has_lat = sscanf(v, "%lf", &cfg_lat) == 1;
        else if (strstr(line, "longitude")) has_lon = sscanf(v, "%lf", &cfg_lon) == 1;
        else if (strstr(line, "city")) { strncpy(cfg_city, v, sizeof(cfg_city) - 1); cfg_city[sizeof(cfg_city) - 1] = '\0'; }
    }
    fclose(f);
    cfg_has_coords = has_lat && has_lon;
}

// WMO weather codes: 71-77 / 85-86 snow, 51-67 / 80-82 / 95-99 rain.
static WeatherType WeatherFromWmo(int code) {
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WEATHER_SNOW;
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95) return WEATHER_RAIN;
    if (code >= 2 && code <= 48) return WEATHER_CLOUDY; // overcast / fog
    return WEATHER_CLEAR;
}

static void ApplyLiveWeather(void) {
    InterlockedExchange(&live_state, 2);
    if (live_lat < 0 && !calendar_restored) current_season = (SeasonType)((current_season + 2) % 4); // southern hemisphere
    current_weather = target_weather = WeatherFromWmo(live_code);
    weather_intensity = target_intensity = current_weather == WEATHER_CLEAR ? FxI(0) : FX(0.8);
    front_seconds_left = FxI(300);
    if (live_snow_m >= 0.01 || current_weather == WEATHER_SNOW) snow_cover = FX(0.7);
    if (live_wind_kmh > 25.0) gust_check_timer = FxI(0);
    snprintf(weather_source_label, sizeof(weather_source_label), "Real weather: %s (%s)",
             cfg_has_coords ? "your coordinates" : cfg_city, weather_names[current_weather]);
}

// Holidays from the PC's calendar (bounded: one small modifier each).
int HolidayToday(void) {
    SYSTEMTIME t; GetLocalTime(&t);
    if (t.wMonth == 10 && t.wDay >= 24) return HOLIDAY_HALLOWEEN;
    if (t.wMonth == 12 && t.wDay >= 20) return HOLIDAY_WINTER_FEAST;
    if (t.wMonth == 1 && t.wDay == 1) return HOLIDAY_NEW_YEAR;
    return HOLIDAY_NONE;
}

void InitSeasonAndWeather(void) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    static const char *holiday_msg[4] = { "", "HALLOWEEN: The dead are restless - night kills give 50% more XP.",
                                          "WINTER FEAST: Merchants are generous - 15% off everything.",
                                          "NEW YEAR: A fresh start - double XP all day!" };
    if (HolidayToday()) strcpy(arpg_action_log, holiday_msg[HolidayToday()]);
    weather_rng ^= (uint32_t)GetTickCount() * 2654435761u;
    if (!calendar_restored) SetSeasonFromDate(&now);
    UpdateDaylightFromClock();
    RollNextFront();
    current_weather = target_weather;
    weather_intensity = target_intensity;
    if (current_season == SEASON_WINTER && current_weather == WEATHER_SNOW) snow_cover = FX(0.4);
    ReadWeatherConfig();
    if (cfg_has_coords || cfg_city[0]) {
        strcpy(weather_source_label, "Checking real-world weather...");
        HANDLE h = CreateThread(NULL, 0, FetchLiveWeatherThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    last_update_tick = GetTickCount();
}

static void UpdateGusts(fx dt) {
    if (gust_duration > FxI(0)) {
        gust_time += dt;
        if (gust_time >= gust_duration) { gust_duration = FxI(0); wind_gust = FxI(0); }
        else wind_gust = gust_sign * FxMul(gust_power, FxSin(FxMul(FX_PI, FxDiv(gust_time, gust_duration))));
        return;
    }
    gust_check_timer -= dt;
    if (gust_check_timer > FxI(0)) return;
    gust_check_timer = FxI(8);
    fx chance = current_season == SEASON_FALL ? FX(0.35) : FX(0.10);
    if (live_state == 2 && live_wind_kmh > 25.0) chance += FX(0.25);
    if (WeatherRandom() < chance) {
        gust_time = FxI(0);
        gust_duration = FxI(3) + 3 * WeatherRandom();
        gust_power = FX(0.6) + FxMul(FX(0.4), WeatherRandom());
        gust_sign = WeatherRandom() < FX(0.5) ? -1 : 1;
    }
}

void UpdateSeasonAndWeather(void) {
    DWORD now = GetTickCount();
    DWORD ms = now - last_update_tick;
    if (ms > 1000) ms = 1000;
    fx dt = FxI((int)ms) / 1000;
    last_update_tick = now;
    if (dt > FxI(1)) dt = FxI(1);
    if (live_state == 1) ApplyLiveWeather();

    AdvanceClock(FxMulInt((int)ms, opt_day_night_speed));

    front_seconds_left -= dt;
    if (front_seconds_left <= FxI(0)) RollNextFront();

    // Gradual change: the old weather fades out completely before the new one
    // fades in, so snow eases off into a dry spell or rain instead of flipping.
    if (current_weather != target_weather) {
        weather_intensity -= dt / 20;
        if (weather_intensity <= FxI(0)) {
            weather_intensity = FxI(0);
            // Every change passes through a dry overcast spell first.
            if (current_weather != WEATHER_CLOUDY && target_weather != WEATHER_CLOUDY) { current_weather = WEATHER_CLOUDY; cloud_hold = CLOUD_HOLD_SECONDS; }
            else if (current_weather == WEATHER_CLOUDY && (cloud_hold -= dt) > FxI(0)) { /* still overcast */ }
            else current_weather = target_weather;
        }
    } else if (weather_intensity < target_intensity) {
        weather_intensity += dt / 25;
        if (weather_intensity > target_intensity) weather_intensity = target_intensity;
    } else if (weather_intensity > target_intensity) {
        weather_intensity -= dt / 25;
        if (weather_intensity < target_intensity) weather_intensity = target_intensity;
    }

    if (current_weather == WEATHER_SNOW) snow_cover += FxMul(FxMul(FX(0.006), weather_intensity), dt);
    else snow_cover -= FxMul(current_season == SEASON_WINTER ? FX(0.002) : FX(0.01), dt);
    TickMeltPatches(dt);
    if (snow_cover < FxI(0)) snow_cover = FxI(0);
    if (snow_cover > FxI(1)) snow_cover = FxI(1);

    UpdateGusts(dt);
    // The original gentle breeze is unchanged (just no longer re-phased per
    // screen); gusts are layered on top of it.
    fx breeze = FxSin(FxMul(world_tick, FX(0.004))); // slow swing, so the wind holds its direction longer
    wind_dir = FxTrunc(breeze * 3) + FxTrunc(wind_gust * 6) + FxTrunc(storm_wind * (breeze >= 0 ? 4 : -4));
    if (current_weather == WEATHER_RAIN && weather_intensity > FX(0.2)) since_rain = FxI(0); else if (since_rain < FxI(30000)) since_rain += dt;
}


int IsDeciduousTree(int tile_seed) {
    int seed = tile_seed + current_screen_index * 31;
    return ((seed % 3) + 3) % 3 == 0;
}

// Apple trees are a subset of the deciduous trees; apples hang on them in
// summer and fall only.
int TreeHasApples(int tile_seed) {
    int seed = tile_seed + current_screen_index * 31;
    if ((seed % 150 + 150) % 150 != 0) return 0;
    return current_season == SEASON_SUMMER || current_season == SEASON_FALL;
}

static int Mix(int a, int b, fx t) { return a + FxTrunc((b - a) * t); }

COLORREF ApplySeasonToGround(COLORREF base) {
    int r = GetRValue(base), g = GetGValue(base), b = GetBValue(base);
    if (current_season == SEASON_FALL) { r = Mix(r, r + 30, FX(0.6)); g = Mix(g, g - 20, FX(0.6)); }
    if (snow_cover > FxI(0)) {
        fx light = FxAbs(FxSin(day_night_cycle_accumulator));
        int white = 120 + FxTrunc(light * 120);
        fx t = FxMul(snow_cover, FX(0.85));
        r = Mix(r, white, t); g = Mix(g, white, t); b = Mix(b, white + 10, t);
    }
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return RGB(r, g, b);
}

static int WrapMod(int v, int m) { v %= m; return v < 0 ? v + m : v; }

static void DrawSnowfall(HDC hdc) {
    if (current_weather != WEATHER_SNOW || weather_intensity < FX(0.02)) return;
    int flakes = FxTrunc(FxI(60) + 340 * weather_intensity);
    HBRUSH flake_b = CreateSolidBrush(RGB(245, 248, 255));
    fx t = world_tick;
    for (int i = 0; i < flakes; i++) {
        int h = RainHash(i * 7919 + 13);
        fx fall_speed = FxI(18) + FxI(h % 23);
        int x = WrapMod(h % WINDOW_WIDTH - cam_x + FxMulI(t, FxI(wind_dir * 4)) + FxTrunc(FxSin(FxMul(t, FX(0.8)) + FxI(i)) * 8), WINDOW_WIDTH);
        int y = WrapMod((h / 7) % WINDOW_HEIGHT - cam_y + FxMulI(t, fall_speed), WINDOW_HEIGHT);
        int s = 2 + (h >> 5) % 2;
        RECT r = { x, y, x + s, y + s };
        FillRect(hdc, &r, flake_b);
    }
    DeleteObject(flake_b);
}

// Fall leaves are stateless: each leaf's position is a function of time, so
// nothing is stored and they simply drift off screen like the birds do.
// Deserts get blowing sand streaks and a dusty haze instead of leaves.
static void DrawDesertDust(HDC hdc) {
    fx t = world_tick, gust = FxAbs(wind_gust);
    int dir = wind_dir < 0 ? -1 : 1;
    fx drift = FxI(20 + abs(wind_dir) * 14) + gust * 90;
    FillWindowAlpha(hdc, RGB(205, 175, 110), 18 + FxTrunc(gust * 50));
    HPEN light = CreatePen(PS_SOLID, 1, RGB(228, 204, 145)), dark = CreatePen(PS_SOLID, 1, RGB(186, 156, 100));
    HGDIOBJ op = SelectObject(hdc, light);
    int motes = 40 + FxTrunc(gust * 120);
    for (int i = 0; i < motes; i++) {
        int h = RainHash(i * 977 + 13);
        fx speed = FX(0.6) + FxI(h % 60) / 50;
        int x = WrapMod(h % WINDOW_WIDTH - cam_x + dir * FxMulI(t, FxMul(drift, speed)), WINDOW_WIDTH);
        int y = WrapMod((h / 13) % WINDOW_HEIGHT - cam_y + FxTrunc(FxSin(FxMul(t, FX(2.1)) + FxI(i)) * 6), WINDOW_HEIGHT);
        SelectObject(hdc, (h & 1) ? light : dark);
        MoveToEx(hdc, x, y, NULL); LineTo(hdc, x + dir * (2 + h % 5), y);
    }
    SelectObject(hdc, op); DeleteObject(light); DeleteObject(dark);
}

// Fog: soft see-through patches on the ground around dawn and dusk, on some
// days within a day of rain, in meadows, forests, swamps and by water; rarely
// in the desert. Each patch is a cached radial alpha texture.
static void DrawFogPatches(HDC hdc) {
    static HDC fog_dc = NULL; static HBITMAP fog_bm = NULL;
    const int FW = 160, FH = 80;
    fx hour = HourOfDay();
    fx strength = hour >= FxI(4) && hour < FX(8.5) ? (hour < FX(6.5) ? FxI(1) : (FX(8.5) - hour) / 2) :
                     hour >= FX(18.5) && hour < FxI(22) ? (hour < FxI(20) ? FxDiv(hour - FX(18.5), FX(1.5)) : FxI(1)) : FxI(0);
    if (strength <= FxI(0) || in_cave || current_weather == WEATHER_RAIN || current_weather == WEATHER_SNOW) return;
    if (since_rain > FxI(DAY_LENGTH_SECONDS) || (unsigned int)RainHash(SeasonDay() * 131 + current_season * 7) % 3u) return;
    int ok = current_biome == BIOME_PRAIRIE || current_biome == BIOME_FOREST || current_biome == BIOME_SWAMP ||
             current_biome == BIOME_LAKE || current_biome == BIOME_RIVER;
    if (current_biome == BIOME_DESERT) ok = (unsigned int)RainHash(current_screen_index * 3 + SeasonDay()) % 20u == 0;
    if (!ok) return;
    if (!fog_dc) {
        BITMAPINFO bi = { 0 };
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = FW; bi.bmiHeader.biHeight = -FH;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        uint32_t *px = NULL;
        fog_bm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
        if (!fog_bm || !px) return;
        for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) {
            fx dx = FxDiv(FxI(x) - FxI(FW) / 2, FxI(FW) / 2), dy = FxDiv(FxI(y) - FxI(FH) / 2, FxI(FH) / 2), r2 = FxMul(dx, dx) + FxMul(dy, dy);
            int a = r2 >= FxI(1) ? 0 : FxTrunc(255 * FxMul(FxI(1) - r2, FxI(1) - r2));
            px[y * FW + x] = ((uint32_t)a << 24) | ((uint32_t)(a * 225 / 255) << 16) | ((uint32_t)(a * 228 / 255) << 8) | (uint32_t)(a * 232 / 255);
        }
        fog_dc = CreateCompatibleDC(hdc);
        SelectObject(fog_dc, fog_bm);
    }
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)FxTrunc(150 * strength), AC_SRC_ALPHA };
    for (int k = 0; k < 9; k++) {
        unsigned int h = (unsigned int)RainHash(current_screen_index * 53 + k * 17);
        int sx, sy; GetIsoCoords(FxI(3) + FxI(h % 26u), FxI(3) + FxI((h >> 6) % 26u), &sx, &sy);
        int w = 280 + (int)(h % 200u), ht = w / 2;
        int drift = FxTrunc(FxSin(FxMul(world_tick, FX(0.05)) + FxI(k)) * 24);
        AlphaBlend(hdc, sx - w / 2 + drift, sy - ht / 2, w, ht, fog_dc, 0, 0, FW, FH, bf);
    }
}

static void DrawFallingLeaves(HDC hdc) {
    if (current_biome == BIOME_DESERT) { DrawDesertDust(hdc); return; }
    if (current_season != SEASON_FALL) return;
    static const COLORREF leaf_colors[] = { RGB(215, 120, 40), RGB(190, 65, 40), RGB(225, 180, 60), RGB(160, 95, 45) };
    int leaves = 14 + FxTrunc(FxAbs(wind_gust) * 60);
    fx t = world_tick;
    fx drift = FxI(6 + wind_dir * 6) + wind_gust * 70;
    for (int i = 0; i < leaves; i++) {
        int h = RainHash(i * 4241 + 77);
        fx speed = FX(0.7) + FxI(h % 50) / 50;
        int x = WrapMod(h % WINDOW_WIDTH - cam_x + FxMulI(t, FxMul(drift, speed)), WINDOW_WIDTH);
        int y = WrapMod((h / 11) % WINDOW_HEIGHT - cam_y + FxMulI(t, speed * 9) + FxTrunc(FxSin(FxMul(t, FX(1.7)) + FxI(i)) * 10), WINDOW_HEIGHT);
        int flip = FxTrunc(FxSinTurn(FxTurn64((int64_t)FxBits(t) * 3 + FxI(i))) * 3);
        HBRUSH b = CreateSolidBrush(leaf_colors[h % 4]);
        HGDIOBJ old = SelectObject(hdc, b);
        POINT leaf[] = { {x - 4, y}, {x, y - 2 - flip}, {x + 4, y}, {x, y + 2 + flip} };
        Polygon(hdc, leaf, 4);
        SelectObject(hdc, old); DeleteObject(b);
    }
}

int IsAdverseWeather(void) {
    return (current_weather == WEATHER_RAIN || current_weather == WEATHER_SNOW) && weather_intensity > FX(0.15);
}

// Ground cover is stateless like the falling leaves: which tiles carry a snow
// drift or a leaf pile is a hash of the tile, and how many do follows
// snow_cover / how far into Fall it is, so it builds up and melts away.
static int NearActiveCampfire(fx x, fx y, fx radius) {
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        fx dx = campfires[i].x - x, dy = campfires[i].y - y;
        if (FxMul(dx, dx) + FxMul(dy, dy) <= FxMul(radius, radius)) return 1;
    }
    return 0;
}

// Snow lies flat on the ground: a drift is a tile-wide patch, a little larger
// than the tile so neighbouring patches run together into one even cover.
static void DrawSnowDrift(HDC hdc, int c, int r, HBRUSH lit, HBRUSH shade) {
    POINT q[4]; TileCorners(c, r, q);
    int mx = (q[0].x + q[1].x + q[2].x + q[3].x) / 4, my = (q[0].y + q[1].y + q[2].y + q[3].y) / 4;
    POINT p[4], s2[4];
    static const int cc[4] = { 0, 1, 1, 0 }, cr2[4] = { 0, 0, 1, 1 }; // which grid corner each point is
    for (int k = 0; k < 4; k++) {
        int h = RainHash(current_screen_index * 131 + (r + cr2[k]) * 257 + c + cc[k]) & 0x7fffffff; // shared by the tiles meeting there
        p[k].x = mx + (q[k].x - mx) * 5 / 4 + h % 13 - 6; p[k].y = my + (q[k].y - my) * 5 / 4 + (h >> 5) % 7 - 3;
        s2[k].x = p[k].x; s2[k].y = p[k].y + 1;
    }
    SelectObject(hdc, shade); Polygon(hdc, s2, 4);
    SelectObject(hdc, lit); Polygon(hdc, p, 4);
}

// Snow melts in a ring around burning campfires: repaint the plain grass there.
static void DrawCampfireMeltPatches(HDC hdc) {
    if (snow_cover < FX(0.05)) return;
    fx amb = FxAbs(FxSin(day_night_cycle_accumulator));
    COLORREF base = RGB(FxTrunc(FxI(25) + amb * 80), FxTrunc(FxI(30) + amb * 155), FxTrunc(FxI(40) + amb * 45));
    fx saved = snow_cover; snow_cover = FxI(0);
    HBRUSH grass = CreateSolidBrush(ApplySeasonToGround(base));
    snow_cover = saved;
    HGDIOBJ old_b = SelectObject(hdc, grass), old_p = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords(campfires[i].x, campfires[i].y, &sx, &sy);
        int cy = sy + TILE_HEIGHT / 2;
        Ellipse(hdc, sx - TILE_WIDTH, cy - TILE_HEIGHT, sx + TILE_WIDTH, cy + TILE_HEIGHT);
    }
    for (int i = 0; i < MAX_MELT_PATCHES; i++) { // patches melted before, shrinking as new snow falls
        fx mx, my, k;
        if (!GetMeltPatch(i, &mx, &my, &k)) continue;
        int sx, sy; GetIsoCoords(mx, my, &sx, &sy);
        int cy = sy + TILE_HEIGHT / 2, w = FxTrunc(TILE_WIDTH * k), h = FxTrunc(TILE_HEIGHT * k);
        if (w > 2) Ellipse(hdc, sx - w, cy - h, sx + w, cy + h);
    }
    SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(grass);
}

void DrawSeasonalGroundCover(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;
    fx leaf_cover = FxI(0);
    fx sf = (fx)((int64_t)season_ms * FX_ONE / SEASON_MS); // how far into the season, 0..1
    if (current_season == SEASON_FALL) leaf_cover = FX(0.25) + FxMul(FX(0.6), sf);
    else if (current_season == SEASON_WINTER && sf < FX(0.2))
        leaf_cover = FxMul(FX(0.3), FxI(1) - sf * 5);
    leaf_cover = FxMul(leaf_cover, FxI(1) - snow_cover);
    if (snow_cover < FX(0.02) && leaf_cover < FX(0.02)) return;
    DrawCampfireMeltPatches(hdc);

    static const COLORREF leaf_colors[] = { RGB(200, 105, 35), RGB(170, 60, 35), RGB(210, 165, 55), RGB(140, 85, 40) };
    int white = 140 + FxTrunc(FxAbs(FxSin(day_night_cycle_accumulator)) * 110);
    HBRUSH snow_b = CreateSolidBrush(RGB(white, white, white + 5 > 255 ? 255 : white + 5));
    HBRUSH shade_b = CreateSolidBrush(RGB(white - 35, white - 30, white - 15));
    HBRUSH leaf_b[4];
    for (int i = 0; i < 4; i++) leaf_b[i] = CreateSolidBrush(leaf_colors[i]);
    HGDIOBJ old_b = SelectObject(hdc, snow_b), old_p = SelectObject(hdc, GetStockObject(NULL_PEN));
    int snow_pct = FxTrunc(snow_cover * 125), leaf_pct = FxTrunc(leaf_cover * 25);

    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
        uint8_t t = VISUAL_MAP[r][c];
        if (t == 1 || t == 10 || t == 12) continue;
        int sx, sy; GetIsoCoords(FxI(c), FxI(r), &sx, &sy);
        if (sx < -TILE_WIDTH || sx > WINDOW_WIDTH + TILE_WIDTH || sy < -TILE_HEIGHT || sy > WINDOW_HEIGHT + TILE_HEIGHT) continue;
        if (NearActiveCampfire(FxI(c), FxI(r), FX(2.2))) continue;
        int cy = sy + TILE_HEIGHT / 2;
        int h = RainHash(current_screen_index * 4099 + r * MAP_SIZE + c) & 0x7fffffff;
        int patch = (RainHash(current_screen_index * 7919 + (r / 3) * 97 + c / 3) & 0x7fffffff) % 100 + h % 30 - 15; // big soft patches, not specks
        if (patch < snow_pct) {
            DrawSnowDrift(hdc, c, r, snow_b, shade_b);
        } else if ((h >> 3) % 100 < leaf_pct) {
            int leaves = 5 + (h >> 11) % 3;
            for (int k = 0; k < leaves; k++) {
                int hk = RainHash(h + k * 131) & 0x7fffffff;
                int lx = sx + hk % 19 - 9, ly = cy + (hk >> 6) % 9 - 4 - k / 2;
                SelectObject(hdc, leaf_b[hk % 4]);
                POINT leaf[] = { {lx - 4, ly}, {lx, ly - 2}, {lx + 4, ly}, {lx, ly + 2} };
                Polygon(hdc, leaf, 4);
            }
        }
    }
    SelectObject(hdc, old_b); SelectObject(hdc, old_p);
    DeleteObject(snow_b); DeleteObject(shade_b);
    for (int i = 0; i < 4; i++) DeleteObject(leaf_b[i]);
}

void DrawWeatherEffects(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;
    DrawRainZones(hdc);
    DrawSnowfall(hdc);
    DrawFallingLeaves(hdc);
    DrawFogPatches(hdc);
}
