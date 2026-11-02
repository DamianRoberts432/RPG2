#include "game.h"
#include <mmsystem.h>

// Saturn-friendly sound: every effect is a tiny 8-bit mono 11025 Hz PCM WAV,
// a format the Saturn's SCSP plays straight from sound RAM. Each one loads once
// into a fixed buffer (8 KB at most) and plays through 4 mixed voices; nothing is
// allocated after start-up. winmm is loaded at run time so the build line
// doesn't change. A missing or wrong-format file is just silent.
#define SFX_RATE 11025
#define SFX_MAX_BYTES 8192
#define SFX_VOICES 4
#define MIX_SAMPLES 256
#define MIX_BUFS 4

static const char *sfx_files[SFX_COUNT] = {
    "footstep_grass", "footstep_stone", "footstep_sand", "footstep_snow", "footstep_wood", "footstep_water",
    "sword_swing"
};
static unsigned char sfx_data[SFX_COUNT][SFX_MAX_BYTES];
static int sfx_len[SFX_COUNT];
static struct { int id, pos; } voices[SFX_VOICES];
static unsigned char mix_buf[MIX_BUFS][MIX_SAMPLES];
static WAVEHDR mix_hdr[MIX_BUFS];
static HWAVEOUT wave_out;
typedef MMRESULT (WINAPI *WaveOpenFn)(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
typedef MMRESULT (WINAPI *WaveHdrFn)(HWAVEOUT, LPWAVEHDR, UINT);
static WaveHdrFn wave_write;

static unsigned Le16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned Le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }

static int LoadWav(const char *path, unsigned char *out, int cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char h[16];
    int n = 0, fmt_ok = 0;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return 0; }
    while (fread(h, 1, 8, f) == 8) {
        unsigned size = Le32(h + 4);
        if (!memcmp(h, "fmt ", 4)) {
            if (size < 16 || fread(h, 1, 16, f) != 16) break;
            fmt_ok = Le16(h) == 1 && Le16(h + 2) == 1 && Le32(h + 4) == SFX_RATE && Le16(h + 14) == 8;
            fseek(f, (long)(size - 16 + (size & 1)), SEEK_CUR);
        } else if (!memcmp(h, "data", 4)) {
            if (fmt_ok) n = (int)fread(out, 1, size < (unsigned)cap ? size : (unsigned)cap, f);
            break;
        } else fseek(f, (long)(size + (size & 1)), SEEK_CUR);
    }
    fclose(f);
    return n;
}

void SoundInit(void) {
    char path[MAX_PATH];
    for (int i = 0; i < SFX_COUNT; i++) {
        snprintf(path, sizeof(path), "sounds/%s.wav", sfx_files[i]);
        sfx_len[i] = LoadWav(path, sfx_data[i], SFX_MAX_BYTES);
    }
    for (int v = 0; v < SFX_VOICES; v++) voices[v].id = -1;
    HMODULE mm = LoadLibraryA("winmm.dll");
    if (!mm) return;
    WaveOpenFn wave_open = (WaveOpenFn)(void *)GetProcAddress(mm, "waveOutOpen");
    WaveHdrFn wave_prepare = (WaveHdrFn)(void *)GetProcAddress(mm, "waveOutPrepareHeader");
    wave_write = (WaveHdrFn)(void *)GetProcAddress(mm, "waveOutWrite");
    if (!wave_open || !wave_prepare || !wave_write) return;
    WAVEFORMATEX wf = { WAVE_FORMAT_PCM, 1, SFX_RATE, SFX_RATE, 1, 8, 0 };
    if (wave_open(&wave_out, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { wave_out = NULL; return; }
    for (int b = 0; b < MIX_BUFS; b++) {
        memset(mix_buf[b], 128, MIX_SAMPLES);
        mix_hdr[b].lpData = (LPSTR)mix_buf[b];
        mix_hdr[b].dwBufferLength = MIX_SAMPLES;
        wave_prepare(wave_out, &mix_hdr[b], sizeof(WAVEHDR));
        mix_hdr[b].dwFlags |= WHDR_DONE;
    }
}

void PlaySfx(int id) {
    if (id < 0 || id >= SFX_COUNT || sfx_len[id] <= 0) return;
    int slot = 0;
    for (int v = 0; v < SFX_VOICES; v++) {
        if (voices[v].id < 0) { slot = v; break; }
        if (voices[v].pos > voices[slot].pos) slot = v; // all busy: cut the oldest
    }
    voices[slot].id = id;
    voices[slot].pos = 0;
}

static void PumpMixer(void) {
    if (!wave_out) return;
    for (int b = 0; b < MIX_BUFS; b++) {
        if (!(mix_hdr[b].dwFlags & WHDR_DONE)) continue;
        for (int s = 0; s < MIX_SAMPLES; s++) {
            int acc = 128;
            for (int v = 0; v < SFX_VOICES; v++) {
                int id = voices[v].id;
                if (id < 0) continue;
                acc += sfx_data[id][voices[v].pos++] - 128;
                if (voices[v].pos >= sfx_len[id]) voices[v].id = -1;
            }
            mix_buf[b][s] = (unsigned char)(acc < 0 ? 0 : acc > 255 ? 255 : acc);
        }
        mix_hdr[b].dwFlags &= ~WHDR_DONE;
        wave_write(wave_out, &mix_hdr[b], sizeof(WAVEHDR));
    }
}

static int GroundSfx(void) {
    if (inside_home >= 0) return SFX_STEP_WOOD;
    if (in_cave || current_biome == BIOME_CAVE) return SFX_STEP_STONE;
    int x = (int)player_x, y = (int)player_y;
    int t = (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) ? VISUAL_MAP[y][x] : 0;
    if (t == 12) return SFX_STEP_WATER;
    if (t == 5 || snow_cover > 0.6f || current_biome == BIOME_TUNDRA) return SFX_STEP_SNOW;
    if (t == 4 || current_biome == BIOME_DESERT || current_biome == BIOME_BEACH) return SFX_STEP_SAND;
    if (t == 1 || t == 3 || current_biome == BIOME_MOUNTAIN || current_biome == BIOME_CASTLE) return SFX_STEP_STONE;
    return SFX_STEP_GRASS;
}

// One step sound per ~half tile walked, picked by the ground underfoot.
void SoundUpdate(void) {
    static float last_x = -1.0f, last_y = -1.0f, walked = 0.0f;
    int busy = is_menu_open || vendor_menu_open || town_menu || is_character_creation || AfkViewActive() || hero_hop_frame > 0;
    if (last_x >= 0.0f && !busy) {
        float dx = player_x - last_x, dy = player_y - last_y, d = sqrtf(dx * dx + dy * dy);
        if (d < 1.5f) walked += d; // a screen change jumps the hero: no step
        if (walked >= 0.55f) { walked = 0.0f; PlaySfx(GroundSfx()); }
    }
    last_x = player_x;
    last_y = player_y;
    PumpMixer();
}
