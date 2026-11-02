Sound effects
=============

The game plays these WAV files from this folder. Replace any of them with your own
recording, keeping the same file name:

  footstep_grass.wav   grass, dirt, forest floor, swamp
  footstep_stone.wav   rock, mountains, castles, caves
  footstep_sand.wav    desert and beach sand
  footstep_snow.wav    snow and tundra
  footstep_wood.wav    floors inside homes and shops
  footstep_water.wav   wading through shallow water
  sword_swing.wav      the "kwhep" of a melee swing

Format (Sega Saturn friendly - the Saturn's sound chip plays this straight from sound RAM):
  - WAV, PCM, 8-bit, mono, 11025 Hz
  - at most 8 KB of sound data each (about 0.7 seconds); anything longer is cut off

In Audacity: Tracks > Mix > Mix Stereo Down to Mono, set Project Rate to 11025,
then File > Export > Export as WAV with "Unsigned 8-bit PCM".

A file in any other format, or a missing file, is simply silent - the game never crashes
over a sound. All seven together take under 64 KB.
