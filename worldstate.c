#include "game.h"
#include "farming.h"
#include "weather.h"

// World memory: every screen keeps what the hero changed there. Felled trees
// and quarried rocks stay gone until they have had time to grow back (never
// over a tilled plot), monsters that were thinned out stay thin for a while,
// snow melted by a campfire only fills back in while it snows, and angry
// monsters close behind the hero follow onto the next screen.
#define YEAR_MINUTES (4 * DAYS_PER_SEASON * 1440)
#define TREE_REGROW_MIN (3 * 1440)
#define ROCK_REGROW_MIN (10 * 1440)
#define ENEMY_RETURN_MIN 360      // 6 game hours before a screen fills up again
#define ENEMY_BACK_EVERY_MIN 90   // meanwhile one monster wanders back every 1.5 game hours
#define MELT_REFILL_SECONDS 240.0f // seconds of steady snowfall to fill a melted patch

ScreenEdit screen_edits[MAX_SCREEN_EDITS];
ScreenMemory screen_memory[MAX_SCREEN_MEMORY];
MeltPatch melt_patches[MAX_MELT_PATCHES];
static int screen_live;
static struct { MonsterType type; float hearts, dx, dy; } followers[2];
static int follower_count;

static int NowMinute(void) { int h, m; GetGameClock(&h, &m); return GameDayOfYear() * 1440 + h * 60 + m; }
static int MinutesSince(int then) { return ((NowMinute() - then) % YEAR_MINUTES + YEAR_MINUTES) % YEAR_MINUTES; }

static ScreenEdit *FreeEdit(void) {
    int oldest = 0, oldest_age = -1;
    for (int i = 0; i < MAX_SCREEN_EDITS; i++) {
        if (!screen_edits[i].active) return &screen_edits[i];
        int age = MinutesSince(screen_edits[i].minute);
        if (age > oldest_age) { oldest_age = age; oldest = i; }
    }
    return &screen_edits[oldest];
}

static ScreenMemory *MemoryFor(int screen, int create) {
    int oldest = 0, oldest_age = -1, free_slot = -1;
    for (int i = 0; i < MAX_SCREEN_MEMORY; i++) {
        ScreenMemory *m = &screen_memory[i];
        if (m->active && m->screen_id == screen) return m;
        if (!m->active) { if (free_slot < 0) free_slot = i; continue; }
        int age = MinutesSince(m->left_minute);
        if (age > oldest_age) { oldest_age = age; oldest = i; }
    }
    if (!create) return NULL;
    return &screen_memory[free_slot >= 0 ? free_slot : oldest];
}

void RememberTileCleared(int x, int y, int tile) {
    ScreenEdit *e = NULL;
    for (int i = 0; i < MAX_SCREEN_EDITS && !e; i++)
        if (screen_edits[i].active && screen_edits[i].screen_id == current_screen_index && screen_edits[i].x == x && screen_edits[i].y == y) e = &screen_edits[i];
    if (!e) e = FreeEdit();
    e->screen_id = current_screen_index; e->minute = NowMinute();
    e->x = (uint8_t)x; e->y = (uint8_t)y; e->tile = (uint8_t)tile; e->active = 1;
}

// Called before a new screen is built, while enemies[] and campfires still
// describe the screen being left.
void RememberLeavingScreen(int new_index) {
    follower_count = 0;
    if (!screen_live || new_index == current_screen_index) return;
    int old = current_screen_index;
    StoreEnemy();
    int alive = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].hearts > 0.0f) alive++;
    ScreenMemory *m = MemoryFor(old, 1);
    m->screen_id = old; m->left_minute = NowMinute(); m->alive = alive; m->active = 1;

    int at_edge = player_x <= 0.5f || player_x >= MAP_SIZE - 1.5f || player_y <= 0.5f || player_y >= MAP_SIZE - 1.5f;
    if (at_edge && old < 20000 && new_index < 20000) {
        for (int i = 0; i < MAX_ENEMIES && follower_count < 2; i++) {
            EnemySlot *e = &enemies[i];
            if (e->hearts <= 0.0f || !e->angry || IS_BOSS_MONSTER(e->type)) continue;
            float dx = e->x - player_x, dy = e->y - player_y;
            if (dx * dx + dy * dy > 25.0f) continue;
            followers[follower_count].type = e->type; followers[follower_count].hearts = e->hearts;
            followers[follower_count].dx = dx; followers[follower_count].dy = dy; follower_count++;
        }
    }

    if (snow_cover > 0.05f) {
        for (int i = 0; i < MAX_CAMPFIRES; i++) {
            if (!campfires[i].active || campfires[i].screen_id != old) continue;
            MeltPatch *p = NULL;
            for (int k = 0; k < MAX_MELT_PATCHES && !p; k++)
                if (melt_patches[k].active && melt_patches[k].screen_id == old && fabsf(melt_patches[k].x - campfires[i].x) < 1.0f && fabsf(melt_patches[k].y - campfires[i].y) < 1.0f) p = &melt_patches[k];
            for (int k = 0; k < MAX_MELT_PATCHES && !p; k++) if (!melt_patches[k].active) p = &melt_patches[k];
            if (!p) p = &melt_patches[i % MAX_MELT_PATCHES];
            p->screen_id = old; p->active = 1; p->x = campfires[i].x; p->y = campfires[i].y; p->fill = 0.0f;
        }
    }
}

static float ClampInside(float v) { return v < 1.0f ? 1.0f : v > MAP_SIZE - 2.0f ? (float)(MAP_SIZE - 2) : v; }

// Called once the new screen's tiles and monsters are rolled.
void ApplyScreenMemory(int index) {
    for (int i = 0; i < MAX_SCREEN_EDITS; i++) {
        ScreenEdit *e = &screen_edits[i];
        if (!e->active || e->screen_id != index || e->x >= MAP_SIZE || e->y >= MAP_SIZE) continue;
        int grow_min = e->tile == 11 ? TREE_REGROW_MIN : ROCK_REGROW_MIN;
        if (MinutesSince(e->minute) >= grow_min) e->active = 0; // grown back
        else VISUAL_MAP[e->y][e->x] = 0;
    }
    for (int i = 0; i < MAX_FARM_PLOTS; i++) { // nothing grows over tilled soil
        FarmPlot *p = &farm_plots[i];
        if (p->active && p->screen_id == index && p->x >= 0 && p->x < MAP_SIZE && p->y >= 0 && p->y < MAP_SIZE) VISUAL_MAP[p->y][p->x] = 0;
    }

    StoreEnemy();
    ScreenMemory *m = MemoryFor(index, 0);
    if (m) {
        int since = MinutesSince(m->left_minute);
        if (since < ENEMY_RETURN_MIN) {
            int allowed = m->alive + since / ENEMY_BACK_EVERY_MIN, alive = 0;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].hearts <= 0.0f) continue;
                if (alive >= allowed && !IS_BOSS_MONSTER(enemies[i].type)) enemies[i].hearts = 0.0f;
                else alive++;
            }
        } else m->active = 0;
    }

    if (follower_count) {
        float nx = player_x <= 0.5f ? (float)(MAP_SIZE - 3) : player_x >= MAP_SIZE - 1.5f ? 2.0f : player_x;
        float ny = player_y <= 0.5f ? (float)(MAP_SIZE - 3) : player_y >= MAP_SIZE - 1.5f ? 2.0f : player_y;
        for (int f = 0; f < follower_count; f++) {
            int s = MAX_ENEMIES - 1 - f;
            for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].hearts <= 0.0f) { s = i; break; }
            EnemySlot *e = &enemies[s];
            e->type = followers[f].type; e->hearts = followers[f].hearts;
            e->x = ClampInside(nx + followers[f].dx); e->y = ClampInside(ny + followers[f].dy);
            e->angry = 1; e->poison = 0; e->direction = 0;
        }
        if (index < 30000) sprintf(arpg_action_log, "WORLD: The %s followed you here!", MonsterName(followers[0].type));
        follower_count = 0;
    }
    int first = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].hearts > 0.0f) { first = i; break; }
    SelectEnemy(first);
    screen_live = 1;
}

int ScreenStillQuiet(void) {
    ScreenMemory *m = MemoryFor(current_screen_index, 0);
    return m && MinutesSince(m->left_minute) < ENEMY_RETURN_MIN;
}

void TickMeltPatches(float dt) {
    for (int i = 0; i < MAX_MELT_PATCHES; i++) {
        MeltPatch *p = &melt_patches[i];
        if (!p->active) continue;
        if (snow_cover < 0.02f) { p->active = 0; continue; }
        if (current_weather == WEATHER_SNOW) p->fill += dt * weather_intensity / MELT_REFILL_SECONDS;
        if (p->fill >= 1.0f) p->active = 0;
    }
}

int GetMeltPatch(int i, float *x, float *y, float *size) {
    MeltPatch *p = &melt_patches[i];
    if (!p->active || p->screen_id != current_screen_index) return 0;
    *x = p->x; *y = p->y; *size = 1.0f - p->fill;
    return 1;
}
