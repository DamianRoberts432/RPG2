#ifndef SOUND_H
#define SOUND_H

// Sound effects: tiny 8-bit mono 11025 Hz WAVs from the sounds folder (see sound.c).
enum { SFX_STEP_GRASS, SFX_STEP_STONE, SFX_STEP_SAND, SFX_STEP_SNOW, SFX_STEP_WOOD, SFX_STEP_WATER,
       SFX_SWING, SFX_COUNT };

void SoundInit(void);
void SoundUpdate(void);
void PlaySfx(int id);

#endif
