#include "game.h"
#include <wininet.h>

// Seasons and world-wide weather. One global weather state covers every
// screen, so moving between screens never changes the weather; it shifts
// gradually on its own timer. Seasons last SEASON_LENGTH_SECONDS of real
// time and start from the PC's date. If weather.ini names a location, the
// starting weather comes from Open-Meteo (free, no API key).

SeasonType current_season = SEASON_SPRING;
float weather_intensity = 0.0f;
float snow_cover = 0.0f;
float wind_gust = 0.0f;
const char *season_names[] = { "Spring", "Summer", "Fall", "Winter" };
const char *weather_names[] = { "Fair", "Rain", "Snow" };
char weather_source_label[96] = "PC clock/date";

static float season_elapsed = 0.0f;
static DWORD last_update_tick = 0;
static WeatherType target_weather = WEATHER_CLEAR;
static float target_intensity = 0.0f;
static float front_seconds_left = 0.0f;
static float gust_time = 0.0f, gust_duration = 0.0f, gust_power = 0.0f, gust_check_timer = 0.0f;
static int gust_sign = 1;
static uint32_t weather_rng = 0x9E3779B9u;

// Separate generator so weather never depends on the per-screen srand() seed.
static float WeatherRandom(void) {
    weather_rng ^= weather_rng << 13; weather_rng ^= weather_rng >> 17; weather_rng ^= weather_rng << 5;
    return (float)(weather_rng & 0xFFFFFF) / (float)0x1000000;
}

static void RollNextFront(void) {
    static const float odds[4][3] = { // clear, rain, snow
        { 0.55f, 0.40f, 0.05f }, { 0.70f, 0.30f, 0.00f },
        { 0.55f, 0.35f, 0.10f }, { 0.35f, 0.20f, 0.45f } };
    float r = WeatherRandom();
    const float *p = odds[current_season];
    target_weather = r < p[0] ? WEATHER_CLEAR : (r < p[0] + p[1] ? WEATHER_RAIN : WEATHER_SNOW);
    target_intensity = target_weather == WEATHER_CLEAR ? 0.0f : 0.5f + 0.5f * WeatherRandom();
    front_seconds_left = 120.0f + 240.0f * WeatherRandom();
}

static void SetSeasonFromDate(const SYSTEMTIME *t) {
    static const int first_month[4] = { 3, 6, 9, 12 };
    int m = t->wMonth;
    SeasonType s = (m >= 3 && m <= 5) ? SEASON_SPRING : (m >= 6 && m <= 8) ? SEASON_SUMMER :
                   (m >= 9 && m <= 11) ? SEASON_FALL : SEASON_WINTER;
    int months_in = (m - first_month[s] + 12) % 12;
    float frac = ((float)months_in * 30.4f + (float)(t->wDay - 1)) / 91.0f;
    if (frac > 0.99f) frac = 0.99f;
    current_season = s;
    season_elapsed = frac * SEASON_LENGTH_SECONDS;
}

// ---- Optional real-world weather (Open-Meteo via WinINet, loaded at runtime
// so the build line does not need an extra library) ----
static char cfg_city[64] = "";
static double cfg_lat = 0.0, cfg_lon = 0.0;
static int cfg_has_coords = 0;
static volatile LONG live_state = 0; // 0 pending/none, 1 ready, 2 applied
static int live_code = -1;
static double live_wind_kmh = 0.0, live_snow_m = 0.0, live_lat = 0.0;

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
    return WEATHER_CLEAR;
}

static void ApplyLiveWeather(void) {
    InterlockedExchange(&live_state, 2);
    if (live_lat < 0.0) current_season = (SeasonType)((current_season + 2) % 4); // southern hemisphere
    current_weather = target_weather = WeatherFromWmo(live_code);
    weather_intensity = target_intensity = current_weather == WEATHER_CLEAR ? 0.0f : 0.8f;
    front_seconds_left = 300.0f;
    if (live_snow_m >= 0.01 || current_weather == WEATHER_SNOW) snow_cover = 0.7f;
    if (live_wind_kmh > 25.0) gust_check_timer = 0.0f;
    snprintf(weather_source_label, sizeof(weather_source_label), "Real weather: %s (%s)",
             cfg_has_coords ? "your coordinates" : cfg_city, weather_names[current_weather]);
}

void InitSeasonAndWeather(void) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    weather_rng ^= (uint32_t)GetTickCount() * 2654435761u;
    SetSeasonFromDate(&now);
    // |sin| of the accumulator drives daylight: 0 = midnight, PI/2 = noon.
    day_night_cycle_accumulator = ((float)now.wHour + now.wMinute / 60.0f) / 24.0f * 3.14159265f;
    RollNextFront();
    current_weather = target_weather;
    weather_intensity = target_intensity;
    if (current_season == SEASON_WINTER && current_weather == WEATHER_SNOW) snow_cover = 0.4f;
    ReadWeatherConfig();
    if (cfg_has_coords || cfg_city[0]) {
        strcpy(weather_source_label, "Checking real-world weather...");
        HANDLE h = CreateThread(NULL, 0, FetchLiveWeatherThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
    last_update_tick = GetTickCount();
}

static void UpdateGusts(float dt) {
    if (gust_duration > 0.0f) {
        gust_time += dt;
        if (gust_time >= gust_duration) { gust_duration = 0.0f; wind_gust = 0.0f; }
        else wind_gust = gust_sign * gust_power * (float)sin(3.14159265f * gust_time / gust_duration);
        return;
    }
    gust_check_timer -= dt;
    if (gust_check_timer > 0.0f) return;
    gust_check_timer = 8.0f;
    float chance = current_season == SEASON_FALL ? 0.35f : 0.10f;
    if (live_state == 2 && live_wind_kmh > 25.0) chance += 0.25f;
    if (WeatherRandom() < chance) {
        gust_time = 0.0f;
        gust_duration = 3.0f + 3.0f * WeatherRandom();
        gust_power = 0.6f + 0.4f * WeatherRandom();
        gust_sign = WeatherRandom() < 0.5f ? -1 : 1;
    }
}

void UpdateSeasonAndWeather(void) {
    DWORD now = GetTickCount();
    float dt = (float)(DWORD)(now - last_update_tick) / 1000.0f;
    last_update_tick = now;
    if (dt > 1.0f) dt = 1.0f;
    if (live_state == 1) ApplyLiveWeather();

    season_elapsed += dt;
    if (season_elapsed >= SEASON_LENGTH_SECONDS) {
        season_elapsed -= SEASON_LENGTH_SECONDS;
        current_season = (SeasonType)((current_season + 1) % 4);
        sprintf(arpg_action_log, "SEASON: %s has arrived.", season_names[current_season]);
        RollNextFront();
    }

    front_seconds_left -= dt;
    if (front_seconds_left <= 0.0f) RollNextFront();

    // Gradual change: the old weather fades out completely before the new one
    // fades in, so snow eases off into a dry spell or rain instead of flipping.
    if (current_weather != target_weather) {
        weather_intensity -= dt / 20.0f;
        if (weather_intensity <= 0.0f) { weather_intensity = 0.0f; current_weather = target_weather; }
    } else if (weather_intensity < target_intensity) {
        weather_intensity += dt / 25.0f;
        if (weather_intensity > target_intensity) weather_intensity = target_intensity;
    } else if (weather_intensity > target_intensity) {
        weather_intensity -= dt / 25.0f;
        if (weather_intensity < target_intensity) weather_intensity = target_intensity;
    }

    if (current_weather == WEATHER_SNOW) snow_cover += 0.006f * weather_intensity * dt;
    else snow_cover -= (current_season == SEASON_WINTER ? 0.002f : 0.01f) * dt;
    if (snow_cover < 0.0f) snow_cover = 0.0f;
    if (snow_cover > 1.0f) snow_cover = 1.0f;

    UpdateGusts(dt);
    // The original gentle breeze is unchanged (just no longer re-phased per
    // screen); gusts are layered on top of it.
    wind_dir = (int)(sin(world_tick * 0.01f) * 3.0f) + (int)(wind_gust * 6.0f);
}

float SeasonSecondsRemaining(void) { return SEASON_LENGTH_SECONDS - season_elapsed; }

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

static int Mix(int a, int b, float t) { return a + (int)((b - a) * t); }

COLORREF ApplySeasonToGround(COLORREF base) {
    int r = GetRValue(base), g = GetGValue(base), b = GetBValue(base);
    if (current_season == SEASON_FALL) { r = Mix(r, r + 30, 0.6f); g = Mix(g, g - 20, 0.6f); }
    if (snow_cover > 0.0f) {
        float light = (float)fabs(sin(day_night_cycle_accumulator));
        int white = 120 + (int)(light * 120.0f);
        float t = snow_cover * 0.85f;
        r = Mix(r, white, t); g = Mix(g, white, t); b = Mix(b, white + 10, t);
    }
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return RGB(r, g, b);
}

static int WrapMod(int v, int m) { v %= m; return v < 0 ? v + m : v; }

static void DrawSnowfall(HDC hdc) {
    if (current_weather != WEATHER_SNOW || weather_intensity < 0.02f) return;
    int flakes = (int)(60.0f + 340.0f * weather_intensity);
    HBRUSH flake_b = CreateSolidBrush(RGB(245, 248, 255));
    float t = world_tick;
    for (int i = 0; i < flakes; i++) {
        int h = RainHash(i * 7919 + 13);
        float fall_speed = 18.0f + (float)(h % 23);
        int x = WrapMod(h % WINDOW_WIDTH - cam_x + (int)(t * (wind_dir * 4.0f)) + (int)(sin(t * 0.8f + i) * 8.0f), WINDOW_WIDTH);
        int y = WrapMod((h / 7) % WINDOW_HEIGHT - cam_y + (int)(t * fall_speed), WINDOW_HEIGHT);
        int s = 2 + (h >> 5) % 2;
        RECT r = { x, y, x + s, y + s };
        FillRect(hdc, &r, flake_b);
    }
    DeleteObject(flake_b);
}

// Fall leaves are stateless: each leaf's position is a function of time, so
// nothing is stored and they simply drift off screen like the birds do.
static void DrawFallingLeaves(HDC hdc) {
    if (current_season != SEASON_FALL) return;
    static const COLORREF leaf_colors[] = { RGB(215, 120, 40), RGB(190, 65, 40), RGB(225, 180, 60), RGB(160, 95, 45) };
    int leaves = 14 + (int)(fabs(wind_gust) * 60.0f);
    float t = world_tick;
    float drift = 6.0f + wind_dir * 6.0f + wind_gust * 70.0f;
    for (int i = 0; i < leaves; i++) {
        int h = RainHash(i * 4241 + 77);
        float speed = 0.7f + (float)(h % 50) / 50.0f;
        int x = WrapMod(h % WINDOW_WIDTH - cam_x + (int)(t * drift * speed), WINDOW_WIDTH);
        int y = WrapMod((h / 11) % WINDOW_HEIGHT - cam_y + (int)(t * 9.0f * speed) + (int)(sin(t * 1.7f + i) * 10.0f), WINDOW_HEIGHT);
        int flip = (int)(sin(t * 3.0f + i) * 3.0f);
        HBRUSH b = CreateSolidBrush(leaf_colors[h % 4]);
        HGDIOBJ old = SelectObject(hdc, b);
        POINT leaf[] = { {x - 4, y}, {x, y - 2 - flip}, {x + 4, y}, {x, y + 2 + flip} };
        Polygon(hdc, leaf, 4);
        SelectObject(hdc, old); DeleteObject(b);
    }
}

int IsAdverseWeather(void) {
    return (current_weather == WEATHER_RAIN || current_weather == WEATHER_SNOW) && weather_intensity > 0.15f;
}

// Ground cover is stateless like the falling leaves: which tiles carry a snow
// drift or a leaf pile is a hash of the tile, and how many do follows
// snow_cover / how far into Fall it is, so it builds up and melts away.
static int NearActiveCampfire(float x, float y, float radius) {
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        float dx = campfires[i].x - x, dy = campfires[i].y - y;
        if (dx * dx + dy * dy <= radius * radius) return 1;
    }
    return 0;
}

// A lumpy drift: a shaded base with 3-5 stacked, shrinking mounds on top.
static void DrawSnowClump(HDC hdc, int x, int y, int h, HBRUSH lit, HBRUSH shade, float depth) {
    int size = 6 + (int)(depth * 8.0f) + h % 5;
    int lumps = 3 + (h >> 5) % 3;
    SelectObject(hdc, shade);
    Ellipse(hdc, x - size - 2, y - size / 3, x + size + 2, y + size / 2 + 2);
    SelectObject(hdc, lit);
    for (int k = 0; k < lumps; k++) {
        int hk = RainHash(h + k * 977) & 0x7fffffff;
        int r = size / 2 + hk % (size / 2 + 1) - k * 2;
        if (r < 3) r = 3;
        int lx = x + (hk >> 4) % (size + 1) - size / 2;
        int ly = y - (k * size) / (lumps + 1) - (hk >> 9) % 3;
        Ellipse(hdc, lx - r, ly - r * 2 / 3, lx + r, ly + r / 3);
    }
}

// Snow melts in a ring around burning campfires: repaint the plain grass there.
static void DrawCampfireMeltPatches(HDC hdc) {
    if (snow_cover < 0.05f) return;
    float amb = (float)fabs(sin(day_night_cycle_accumulator));
    COLORREF base = RGB((int)(25.0f + amb * 80.0f), (int)(30.0f + amb * 155.0f), (int)(40.0f + amb * 45.0f));
    float saved = snow_cover; snow_cover = 0.0f;
    HBRUSH grass = CreateSolidBrush(ApplySeasonToGround(base));
    snow_cover = saved;
    HGDIOBJ old_b = SelectObject(hdc, grass), old_p = SelectObject(hdc, GetStockObject(NULL_PEN));
    for (int i = 0; i < MAX_CAMPFIRES; i++) {
        if (!campfires[i].active || campfires[i].screen_id != current_screen_index) continue;
        int sx, sy; GetIsoCoords(campfires[i].x, campfires[i].y, &sx, &sy);
        int cy = sy + TILE_HEIGHT / 2;
        Ellipse(hdc, sx - TILE_WIDTH, cy - TILE_HEIGHT, sx + TILE_WIDTH, cy + TILE_HEIGHT);
    }
    SelectObject(hdc, old_b); SelectObject(hdc, old_p); DeleteObject(grass);
}

void DrawSeasonalGroundCover(HDC hdc) {
    if (current_biome == BIOME_CAVE) return;
    float leaf_cover = 0.0f;
    if (current_season == SEASON_FALL) leaf_cover = 0.25f + 0.6f * (season_elapsed / SEASON_LENGTH_SECONDS);
    else if (current_season == SEASON_WINTER && season_elapsed < SEASON_LENGTH_SECONDS * 0.2f)
        leaf_cover = 0.3f * (1.0f - season_elapsed / (SEASON_LENGTH_SECONDS * 0.2f));
    leaf_cover *= 1.0f - snow_cover;
    if (snow_cover < 0.02f && leaf_cover < 0.02f) return;
    DrawCampfireMeltPatches(hdc);

    static const COLORREF leaf_colors[] = { RGB(200, 105, 35), RGB(170, 60, 35), RGB(210, 165, 55), RGB(140, 85, 40) };
    int white = 140 + (int)((float)fabs(sin(day_night_cycle_accumulator)) * 110.0f);
    HBRUSH snow_b = CreateSolidBrush(RGB(white, white, white + 5 > 255 ? 255 : white + 5));
    HBRUSH shade_b = CreateSolidBrush(RGB(white - 35, white - 30, white - 15));
    HBRUSH leaf_b[4];
    for (int i = 0; i < 4; i++) leaf_b[i] = CreateSolidBrush(leaf_colors[i]);
    HGDIOBJ old_b = SelectObject(hdc, snow_b), old_p = SelectObject(hdc, GetStockObject(NULL_PEN));
    int snow_pct = (int)(snow_cover * 22.0f), leaf_pct = (int)(leaf_cover * 25.0f);

    for (int r = 0; r < MAP_SIZE; r++) for (int c = 0; c < MAP_SIZE; c++) {
        uint8_t t = VISUAL_MAP[r][c];
        if (t == 1 || t == 10 || t == 12) continue;
        int sx, sy; GetIsoCoords((float)c, (float)r, &sx, &sy);
        if (sx < -TILE_WIDTH || sx > WINDOW_WIDTH + TILE_WIDTH || sy < -TILE_HEIGHT || sy > WINDOW_HEIGHT + TILE_HEIGHT) continue;
        if (NearActiveCampfire((float)c, (float)r, 2.2f)) continue;
        int cy = sy + TILE_HEIGHT / 2;
        int h = RainHash(current_screen_index * 4099 + r * MAP_SIZE + c) & 0x7fffffff;
        if (h % 100 < snow_pct) {
            int ox = (h >> 4) % 17 - 8, oy = (h >> 9) % 7 - 3;
            DrawSnowClump(hdc, sx + ox, cy + oy, h, snow_b, shade_b, snow_cover);
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
}
