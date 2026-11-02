# Sega Saturn memory audit

The Saturn has 2 MB of work RAM (two 1 MB banks), plus its own 1.5 MB of video
RAM and 512 KB of sound RAM. This file tracks how much of the 2 MB the game's
code and fixed data take, measured from the PC build.

## How it is measured

Every `.c` file is compiled on its own with MinGW gcc `-O1` and the sections are
added up with `size -t *.o`:

- **text**: machine code and constant tables.
- **data**: tables with starting values.
- **bss**: fixed arrays that start at zero (the world, saves, the AFK face list).

The game never calls `malloc`, `calloc` or `realloc`, so there is no heap to add.
These are PC (x86-64) numbers. SH-2 code size will differ, so treat `text` as an
estimate until there is a real Saturn build.

## Results

| | text | data | bss | total |
|---|---:|---:|---:|---:|
| Before (main after PR #47) | 459,136 | 32,944 | 742,240 | 1,234,320 (1.18 MB) |
| After the AFK face list shrink | 459,808 | 32,944 | 562,016 | 1,054,768 (1.01 MB) |

That leaves about 1,042,000 bytes (0.99 MB) of the 2 MB for the stack, the
Saturn SDK and anything new.

## Biggest fixed arrays (bss + data, per file)

| File | Bytes | What it holds |
|---|---:|---|
| afkview.c | 286,240 | AFK first-person view: up to 4096 faces at 68 bytes each |
| town.c | 59,904 | chests, card names, card guide |
| sound.c | 58,672 | 7 sound samples of up to 8 KB, plus mixing buffers |
| worldmap.c | 52,048 | World Map markers, saved screen edits and melt patches |
| globals.c | 28,480 | hero, action log |
| worldgen.c | 16,832 | whole-world layout |
| weather.c | 16,608 | weather, snow cover |
| adventures.c | 16,000 | adventure contacts |
| world.c | 14,832 | current screen, village lights |
| worldstate.c | 12,000 | screen memory, edits, melt patches |
| ui.c | 10,720 | message log |

On a Saturn the sound samples would live in the 512 KB sound RAM, which frees
another 57 KB of work RAM.

## Not counted here

- The PC window's back buffer (1824 x 986) belongs to Windows. On a Saturn the
  picture is drawn by VDP1/VDP2 into their own video RAM.
- The stack. The deepest locals are the AFK geometry arrays (under 1 KB per call).
- The Saturn SDK and its libraries.

## Changes made for the budget

- **AFK face list:** screen points are now 16-bit, the same as VDP1 takes them,
  and each face is clipped to a 512-pixel guard band round the screen so its
  points always fit. A face went from 112 to 68 bytes, so the list went from
  448 KB to 272 KB. A compile-time check stops it growing back.

## Whole-number maths (fixed point)

The Saturn's SH-2 CPUs have no floating point unit, so the game now does its
maths in 16.16 fixed point (`fixed.h`, `fixed.c`). An `fx` is a value times
65536, so `FX_ONE` is 1.0.

- **Sine, cosine and atan2:** these use 1 KB quarter-wave and atan tables,
  read in between steps. The largest error is 0.00004 for sine and cosine, and
  0.0004 radians for atan2.
- **Square roots and lengths:** worked out bit by bit on whole numbers.
- **Converted:** movement, collisions, combat, monsters, villagers, critters,
  inventory weights, prices, weather, the calendar and all the 2D drawing.
- **Calendar:** it counts milliseconds, because a season (43,200 seconds) is
  too long for 16.16 fixed point.
- **`world_tick`:** it wraps after 655,360 frames. The villager and monster
  timers that wait on it use wrap-safe compares (`FxAfter`/`FxBefore`). Tree
  growth starts over after a wrap, which only changes how big saplings look.
- **Saves:** they are version 2 now. Version 1 saves still load: their
  decimals are converted once, using whole-number maths only.
- **Not yet converted:** the AFK first-person view (`afkview.c`) is the next
  PR. Until then it reads the game's fixed-point state through
  `FxToF`/`FxFromF`. The PC's optional real-world weather download also reads
  decimals, and a Saturn has no internet to run it.
- **Windows boundaries:** the GDI calls that need decimals (`XFORM`) convert
  right at the call.
