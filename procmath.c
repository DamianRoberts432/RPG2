#include "game.h"
#include "procmath.h"

fx DistEuclid(fx dx, fx dy) { return FxLen(dx, dy); }
fx DistManhattan(fx dx, fx dy) { return FxAbs(dx) + FxAbs(dy); }
fx DistChebyshev(fx dx, fx dy) { return FxMax(FxAbs(dx), FxAbs(dy)); }
fx Dist(int metric, fx dx, fx dy) {
    return metric == METRIC_MANHATTAN ? DistManhattan(dx, dy) : metric == METRIC_CHEBYSHEV ? DistChebyshev(dx, dy) : DistEuclid(dx, dy);
}

uint32_t Hash2(int x, int y, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
fx HashFx(int x, int y, uint32_t seed) { return FxRaw((int32_t)(Hash2(x, y, seed) >> 16)); }

fx SmoothStep(fx e0, fx e1, fx x) {
    fx t = FxClamp(FxDiv(x - e0, e1 - e0 ? e1 - e0 : FX_EPS), FX(0.0), FX(1.0));
    return FxMul(FxMul(t, t), FX(3.0) - 2 * t);
}
fx SmootherStep(fx t) { return FxMul(FxMul(FxMul(t, t), t), FxMul(t, 6 * t - FX(15.0)) + FX(10.0)); }
fx EaseInQuad(fx t) { return FxMul(t, t); }
fx EaseOutQuad(fx t) { return FxMul(t, FX(2.0) - t); }
fx SinTurns(fx turns) { return FxSinTurn((uint32_t)FxBits(turns) << 16); }

static fx Lerp(fx a, fx b, fx t) { return a + FxMul(b - a, t); }

fx ValueNoise2(fx x, fx y, uint32_t seed) {
    int xi = FxFloor(x), yi = FxFloor(y);
    fx u = SmootherStep(x - FxI(xi)), v = SmootherStep(y - FxI(yi));
    fx a = Lerp(HashFx(xi, yi, seed), HashFx(xi + 1, yi, seed), u), b = Lerp(HashFx(xi, yi + 1, seed), HashFx(xi + 1, yi + 1, seed), u);
    return 2 * Lerp(a, b, v) - FX(1.0);
}

static fx Grad(uint32_t h, fx dx, fx dy) { // eight directions
    static const fx gx[8] = { FX(1.0), FX(-1.0), FX(0.0), FX(0.0), FX(0.7071), FX(-0.7071), FX(0.7071), FX(-0.7071) };
    static const fx gy[8] = { FX(0.0), FX(0.0), FX(1.0), FX(-1.0), FX(0.7071), FX(0.7071), FX(-0.7071), FX(-0.7071) };
    return FxMul(gx[h & 7], dx) + FxMul(gy[h & 7], dy);
}

fx Perlin2(fx x, fx y, uint32_t seed) {
    int xi = FxFloor(x), yi = FxFloor(y);
    fx xf = x - FxI(xi), yf = y - FxI(yi), u = SmootherStep(xf), v = SmootherStep(yf);
    fx a = Lerp(Grad(Hash2(xi, yi, seed), xf, yf), Grad(Hash2(xi + 1, yi, seed), xf - FX(1.0), yf), u);
    fx b = Lerp(Grad(Hash2(xi, yi + 1, seed), xf, yf - FX(1.0)), Grad(Hash2(xi + 1, yi + 1, seed), xf - FX(1.0), yf - FX(1.0)), u);
    return FxClamp(FxMul(Lerp(a, b, v), FX(1.41)), FX(-1.0), FX(1.0));
}

static fx SimplexCorner(uint32_t h, fx x, fx y) {
    fx t = FX(0.5) - FxMul(x, x) - FxMul(y, y);
    if (t <= FX(0.0)) return FX(0.0);
    t = FxMul(t, t);
    return FxMul(FxMul(t, t), Grad(h, x, y));
}
fx Simplex2(fx x, fx y, uint32_t seed) {
    const fx F2 = FX(0.3660254), G2 = FX(0.2113249);
    fx s = FxMul(x + y, F2);
    int i = FxFloor(x + s), j = FxFloor(y + s);
    fx t = FxMul(FxI(i + j), G2), x0 = x - (FxI(i) - t), y0 = y - (FxI(j) - t);
    int i1 = x0 > y0, j1 = !i1;
    fx x1 = x0 - FxI(i1) + G2, y1 = y0 - FxI(j1) + G2, x2 = x0 - FX(1.0) + 2 * G2, y2 = y0 - FX(1.0) + 2 * G2;
    fx n = SimplexCorner(Hash2(i, j, seed), x0, y0) + SimplexCorner(Hash2(i + i1, j + j1, seed), x1, y1) + SimplexCorner(Hash2(i + 1, j + 1, seed), x2, y2);
    return FxClamp(n * 70, FX(-1.0), FX(1.0));
}

// Each octave doubles the scale and turns by about 37 degrees, so the noise grid
// never lines up between octaves (no straight creases).
static void NextOctave(fx *x, fx *y) {
    fx nx = FxMul(FX(1.2), *x) - FxMul(FX(1.6), *y), ny = FxMul(FX(1.6), *x) + FxMul(FX(1.2), *y);
    *x = nx + FX(17.3); *y = ny + FX(5.9);
}
// Coordinates times 2^(octaves-1) must stay under 32767, so keep |x|,|y| < 128 at 8 octaves.
fx Fbm2(fx x, fx y, int octaves, uint32_t seed) {
    fx sum = FX(0.0), amp = FX(0.5), total = FX(0.0);
    octaves = octaves < 1 ? 1 : octaves > 8 ? 8 : octaves;
    for (int o = 0; o < octaves; o++, amp /= 2) { sum += FxMul(amp, Perlin2(x, y, seed + (uint32_t)o * 101u)); total += amp; NextOctave(&x, &y); }
    return FxDiv(sum, total);
}
fx Ridged2(fx x, fx y, int octaves, uint32_t seed) {
    fx sum = FX(0.0), amp = FX(0.5), total = FX(0.0);
    octaves = octaves < 1 ? 1 : octaves > 8 ? 8 : octaves;
    for (int o = 0; o < octaves; o++, amp /= 2) {
        fx r = FX(1.0) - FxAbs(Perlin2(x, y, seed + (uint32_t)o * 101u));
        sum += FxMul(amp, FxMul(r, r)); total += amp; NextOctave(&x, &y);
    }
    return FxDiv(sum, total);
}
void DomainWarp2(fx *x, fx *y, fx amount, uint32_t seed) {
    fx dx = Perlin2(*x + FX(5.2), *y + FX(1.3), seed + 1), dy = Perlin2(*x + FX(9.7), *y + FX(4.1), seed + 2);
    *x += FxMul(amount, dx); *y += FxMul(amount, dy);
}

WorleyResult Worley2(fx x, fx y, int metric, uint32_t seed) {
    WorleyResult r = { FX_MAX, FX_MAX, 0, 0 };
    int xi = FxFloor(x), yi = FxFloor(y);
    for (int dj = -1; dj <= 1; dj++)
        for (int di = -1; di <= 1; di++) {
            int cx = xi + di, cy = yi + dj;
            fx d = Dist(metric, FxI(cx) + HashFx(cx, cy, seed) - x, FxI(cy) + HashFx(cx, cy, seed ^ 0x9E3779B9u) - y);
            if (d < r.f1) { r.f2 = r.f1; r.f1 = d; r.cell_x = cx; r.cell_y = cy; }
            else if (d < r.f2) r.f2 = d;
        }
    return r;
}

fx Interference2(fx x, fx y, const fx src[][2], int n, fx wavelength, fx t) {
    fx sum = FX(0.0);
    if (n <= 0) return sum;
    for (int i = 0; i < n; i++) sum += SinTurns(FxDiv(FxLen(x - src[i][0], y - src[i][1]), wavelength) - t);
    return sum / n;
}
fx Moire2(fx x, fx y, fx ax, fx ay, fx bx, fx by, fx freq) {
    return FxMul(SinTurns(FxMul(FxLen(x - ax, y - ay), freq)), SinTurns(FxMul(FxLen(x - bx, y - by), freq)));
}
fx Plasma2(fx x, fx y, fx t) {
    return (SinTurns(FxMul(x, FX(0.16)) + t) + SinTurns(FxMul(y, FX(0.13)) - t) + SinTurns(FxMul(x + y, FX(0.09)) + t / 2) + SinTurns(FxMul(FxLen(x, y), FX(0.12)) - t)) / 4;
}

int MarchCase(fx tl, fx tr, fx br, fx bl, fx iso) { return (tl >= iso) * 8 + (tr >= iso) * 4 + (br >= iso) * 2 + (bl >= iso); }
static fx Cross(fx p, fx q, fx iso) { return p == q ? FX(0.5) : FxClamp(FxDiv(iso - p, q - p), FX(0.0), FX(1.0)); }
int MarchSquares(const fx *field, int w, int h, fx iso, MsSeg *out, int max_out) {
    // edges: 0 top, 1 right, 2 bottom, 3 left; -1 ends the list
    static const signed char table[16][4] = { { -1 }, { 3, 2, -1 }, { 2, 1, -1 }, { 3, 1, -1 }, { 0, 1, -1 }, { -1 }, { 0, 2, -1 }, { 3, 0, -1 },
                                              { 3, 0, -1 }, { 0, 2, -1 }, { -1 }, { 0, 1, -1 }, { 3, 1, -1 }, { 2, 1, -1 }, { 3, 2, -1 }, { -1 } };
    static const signed char cut_tl_br[4] = { 3, 0, 2, 1 }, cut_bl_tr[4] = { 3, 2, 0, 1 };
    int n = 0;
    for (int cy = 0; cy + 1 < h; cy++)
        for (int cx = 0; cx + 1 < w; cx++) {
            fx a = field[cy * w + cx], b = field[cy * w + cx + 1], c = field[(cy + 1) * w + cx + 1], d = field[(cy + 1) * w + cx];
            int k = MarchCase(a, b, c, d, iso), centre_in = a / 4 + b / 4 + c / 4 + d / 4 >= iso;
            const signed char *e = k == 5 ? (centre_in ? cut_tl_br : cut_bl_tr) : k == 10 ? (centre_in ? cut_bl_tr : cut_tl_br) : table[k];
            for (int s = 0; s < 4 && e[s] >= 0 && s + 1 < 4 + (k == 5 || k == 10) && n < max_out; s += 2) {
                fx px[2], py[2];
                for (int q = 0; q < 2; q++) {
                    int edge = e[s + q];
                    px[q] = FxI(cx) + (edge == 1 ? FX(1.0) : edge == 0 ? Cross(a, b, iso) : edge == 2 ? Cross(d, c, iso) : FX(0.0));
                    py[q] = FxI(cy) + (edge == 2 ? FX(1.0) : edge == 1 ? Cross(b, c, iso) : edge == 3 ? Cross(a, d, iso) : FX(0.0));
                }
                out[n].x0 = px[0]; out[n].y0 = py[0]; out[n].x1 = px[1]; out[n].y1 = py[1]; n++;
                if (k != 5 && k != 10) break;
            }
        }
    return n;
}

void CaveSmooth(unsigned char *grid, unsigned char *scratch, int w, int h, int passes) {
    for (int p = 0; p < passes; p++) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int walls = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (!dx && !dy) continue;
                        int nx = x + dx, ny = y + dy;
                        walls += nx < 0 || ny < 0 || nx >= w || ny >= h || grid[ny * w + nx];
                    }
                scratch[y * w + x] = walls > 4 ? 1 : walls < 4 ? 0 : grid[y * w + x];
            }
        memcpy(grid, scratch, (size_t)w * h);
    }
}

int LExpand(const char *axiom, const LRule *rules, int nrules, int iterations, char *out, char *scratch, int max_len) {
    int len = 0;
    if (max_len <= 0) return 0;
    while (axiom[len] && len < max_len - 1) { out[len] = axiom[len]; len++; }
    out[len] = 0;
    for (int it = 0; it < iterations; it++) {
        int m = 0;
        for (int i = 0; i < len && m < max_len - 1; i++) {
            const char *to = NULL;
            for (int r = 0; r < nrules; r++) if (rules[r].from == out[i]) { to = rules[r].to; break; }
            if (!to) scratch[m++] = out[i];
            else for (int k = 0; to[k] && m < max_len - 1; k++) scratch[m++] = to[k];
        }
        memcpy(out, scratch, (size_t)m); out[m] = 0; len = m;
    }
    return len;
}

int LTurtle(const char *s, fx x, fx y, fx step, uint32_t angle, fx shrink, uint32_t jitter_seed, LSeg *out, int max_out) {
    struct { fx x, y, len; uint32_t h; int depth; } st[LSYS_STACK];
    uint32_t h = 0xC0000000u; // straight up the screen
    int sp = 0, lost = 0, depth = 0, n = 0;
    fx len = step;
    for (int i = 0; s[i]; i++) {
        char ch = s[i];
        uint32_t wob = jitter_seed ? (uint32_t)((int32_t)(Hash2(i, 7, jitter_seed) >> 6) - (1 << 25)) : 0u;
        if (ch == 'F' || ch == 'f') {
            fx nx = x + FxMul(len, FxCosTurn(h)), ny = y + FxMul(len, FxSinTurn(h));
            if (ch == 'F' && n < max_out) { out[n].x0 = x; out[n].y0 = y; out[n].x1 = nx; out[n].y1 = ny; out[n].depth = (unsigned char)depth; n++; }
            x = nx; y = ny;
        } else if (ch == '+') h += angle + wob;
        else if (ch == '-') h -= angle + wob;
        else if (ch == '[') {
            if (sp < LSYS_STACK) { st[sp].x = x; st[sp].y = y; st[sp].len = len; st[sp].h = h; st[sp].depth = depth; sp++; len = FxMul(len, shrink); depth++; }
            else lost++;
        } else if (ch == ']') {
            if (lost) lost--;
            else if (sp > 0) { sp--; x = st[sp].x; y = st[sp].y; len = st[sp].len; h = st[sp].h; depth = st[sp].depth; }
        }
    }
    return n;
}

void GoldenSpiral(int i, fx spacing, fx *x, fx *y) {
    fx r = FxMul(spacing, FxSqrt(FxI(i)));
    uint32_t turn = (uint32_t)i * 0x61C88647u; // the golden angle, 137.5 degrees
    *x = FxMul(r, FxCosTurn(turn)); *y = FxMul(r, FxSinTurn(turn));
}

int Bayer4(int x, int y) {
    static const unsigned char m[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    return m[y & 3][x & 3];
}
int DitherOn(int x, int y, fx coverage) { return FxI(Bayer4(x, y)) / 16 + FX(1.0 / 32) < coverage; }

int LinePoints(int x0, int y0, int x1, int y1, int (*out)[2], int max_out) {
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 0;
    while (n < max_out) {
        out[n][0] = x0; out[n][1] = y0; n++;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return n;
}
