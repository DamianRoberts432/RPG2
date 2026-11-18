#ifndef PROCMATH_H
#define PROCMATH_H
// Procedural maths toolbox: small formulas that make big effects. Everything is
// 16.16 fixed point with bounded loops and caller-owned buffers (no heap), so it
// runs the same on the Saturn's SH-2s as on PC.
//
// Ideas: Worley cells -> caustics, cracked mud, scales, stained glass, crystal
// fields. Perlin/simplex + fBm -> clouds, fog drift, terrain, ore veins, fire.
// Ridged noise -> mountain spines, lightning, river beds. Domain warp -> marble,
// smoke, lava. Plasma/interference/moire -> magic portals, mirages, ripples,
// shimmering water. Marching squares -> smooth coastlines, cave walls, lava pools.
// Cellular automata -> cave layouts, moss growth. L-systems -> trees, roots,
// lightning, coral, leaf veins. Golden spiral -> flower heads, star fields,
// even scatter. Bayer dither -> Saturn-style see-through and extra shades.
#include "fixed.h"
#include <stdint.h>

// --- Distances: pick the metric to change a pattern's look ---
enum { METRIC_EUCLID, METRIC_MANHATTAN, METRIC_CHEBYSHEV };
fx DistEuclid(fx dx, fx dy);      // round cells and rings
fx DistManhattan(fx dx, fx dy);   // diamond cells, |dx| + |dy|
fx DistChebyshev(fx dx, fx dy);   // square cells, max(|dx|, |dy|)
fx Dist(int metric, fx dx, fx dy);

// --- Hashing and curves ---
uint32_t Hash2(int x, int y, uint32_t seed);
fx HashFx(int x, int y, uint32_t seed);   // 0..1
fx SmoothStep(fx e0, fx e1, fx x);        // 0..1, eased at both ends
fx SmootherStep(fx t);                    // quintic 6t^5-15t^4+10t^3, t in 0..1
fx EaseInQuad(fx t);
fx EaseOutQuad(fx t);
fx SinTurns(fx turns);                    // sin(2*pi*turns): whole number = full circle

// --- Noise, all about -1..1 ---
fx ValueNoise2(fx x, fx y, uint32_t seed);
fx Perlin2(fx x, fx y, uint32_t seed);
fx Simplex2(fx x, fx y, uint32_t seed);
fx Fbm2(fx x, fx y, int octaves, uint32_t seed);    // layered Perlin, octaves 1..8
fx Ridged2(fx x, fx y, int octaves, uint32_t seed); // 0..1, sharp crests
void DomainWarp2(fx *x, fx *y, fx amount, uint32_t seed);

// --- Worley / Voronoi cells: one jittered point per whole-number cell ---
typedef struct { fx f1, f2; int cell_x, cell_y; } WorleyResult;
WorleyResult Worley2(fx x, fx y, int metric, uint32_t seed); // f2 - f1 near 0 = cell edge

// --- Sine patterns (t in turns: +1 = one full cycle) ---
fx Interference2(fx x, fx y, const fx src[][2], int n, fx wavelength, fx t); // ripples from n sources, -1..1
fx Moire2(fx x, fx y, fx ax, fx ay, fx bx, fx by, fx freq);  // two ring gratings multiplied, -1..1
fx Plasma2(fx x, fx y, fx t);                                // classic demo plasma, -1..1

// --- Marching squares: contour lines through a w*h grid of samples ---
typedef struct { fx x0, y0, x1, y1; } MsSeg; // in grid units
int MarchCase(fx tl, fx tr, fx br, fx bl, fx iso); // 0..15 (TL=8, TR=4, BR=2, BL=1)
int MarchSquares(const fx *field, int w, int h, fx iso, MsSeg *out, int max_out);

// --- Cellular automata cave smoothing: 1 = wall. Edges count as wall. ---
void CaveSmooth(unsigned char *grid, unsigned char *scratch, int w, int h, int passes);

// --- L-systems (Lindenmayer) ---
// F draw forward, f move, + / - turn, [ push, ] pop (branches shrink by 'shrink').
typedef struct { char from; const char *to; } LRule;
typedef struct { fx x0, y0, x1, y1; unsigned char depth; } LSeg;
#define LSYS_STACK 16
int LExpand(const char *axiom, const LRule *rules, int nrules, int iterations, char *out, char *scratch, int max_len);
int LTurtle(const char *s, fx x, fx y, fx step, uint32_t angle, fx shrink, uint32_t jitter_seed, LSeg *out, int max_out);

// --- Scatter, dither, lines ---
void GoldenSpiral(int i, fx spacing, fx *x, fx *y); // i-th point of a sunflower spiral
int Bayer4(int x, int y);                           // 0..15 ordered-dither threshold
int DitherOn(int x, int y, fx coverage);            // 1 if this pixel is lit at 0..1 coverage
int LinePoints(int x0, int y0, int x1, int y1, int (*out)[2], int max_out); // Bresenham
#endif
