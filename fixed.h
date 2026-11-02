#ifndef FIXED_H
#define FIXED_H
#include <stdint.h>
#if defined(__cplusplus) && defined(FX_CHECK)
#include "fx_check.h"
#else

// Saturn-style 16.16 fixed point. The Saturn's SH-2 CPUs have no floating
// point unit, so world, combat and drawing maths run on whole numbers:
// an fx holds a value times 65536 (FX_ONE is 1.0, FX_HALF is 0.5).
typedef int32_t fx;
#define FX_SHIFT 16
#define FX_ONE 65536
#define FX_HALF 32768
#define FX_PI 205887
#define FX_TWO_PI 411775
#define FX_HALF_PI 102944
// FX() is for written-in constants only; the compiler turns it into a whole number.
#define FX(c) ((fx)((c) * 65536.0 + ((c) >= 0 ? 0.5 : -0.5)))
#define FxI(i) ((fx)((int32_t)(i) * FX_ONE))
#define FxTrunc(a) ((int)((a) / FX_ONE))      // toward zero, like (int) on a decimal
#define FxFloor(a) ((int)((a) >> FX_SHIFT))   // always down, also for negatives
#define FxRound(a) ((int)(((a) + FX_HALF) >> FX_SHIFT))
#define FxFrac(a) ((a) & (FX_ONE - 1))
#define FxCeil(a) ((int)(((a) + FX_ONE - 1) >> FX_SHIFT))
#define FxRaw(v) ((fx)(v))
#define FX_MAX ((fx)INT32_MAX)
#define FX_EPS ((fx)1) // smallest step, keeps divisors off zero

static inline fx FxMul(fx a, fx b) { return (fx)(((int64_t)a * b) >> FX_SHIFT); }
static inline fx FxDiv(fx a, fx b) {
    if (!b) return a >= 0 ? INT32_MAX : -INT32_MAX;
    int64_t q = ((int64_t)a * FX_ONE) / b;
    return q > INT32_MAX ? INT32_MAX : q < -INT32_MAX ? -INT32_MAX : (fx)q;
}
static inline fx FxAbs(fx a) { return a < 0 ? -a : a; }
static inline fx FxMin(fx a, fx b) { return a < b ? a : b; }
static inline fx FxMax(fx a, fx b) { return a > b ? a : b; }
static inline fx FxClamp(fx a, fx lo, fx hi) { return a < lo ? lo : a > hi ? hi : a; }
static inline fx FxLerp(fx a, fx b, fx t) { return a + FxMul(b - a, t); }

static inline uint32_t FxTurn64(int64_t radians) { return (uint32_t)((radians * 683565276LL) >> 16); }
// Timers that compare against world_tick, which wraps every 32768 seconds of play.
#define FxAfter(a, b) ((int32_t)((uint32_t)(a) - (uint32_t)(b)) > 0)
#define FxBefore(a, b) ((int32_t)((uint32_t)(a) - (uint32_t)(b)) < 0)
static inline fx FxWrapAdd(fx a, fx b) { return (fx)((uint32_t)a + (uint32_t)b); }
// Whole-number result of n * a, rounded, without overflowing on big prices.
// Whole-number part of a * b, for big products such as time x speed.
static inline int FxMulI(fx a, fx b) { return (int)(((int64_t)a * b) >> 32); }
static inline int FxMulInt(int n, fx a) { return (int)(((int64_t)n * a + FX_HALF) >> FX_SHIFT); }
// Bridges for code not converted yet; to be removed once every file is fixed point.
#define FxToF(a) ((float)(a) * (1.0f / 65536.0f))
#define FxFromF(f) ((fx)((f) * 65536.0f))
#define FxBits(a) ((int32_t)(a)) // the raw 16.16 bits, for hashing and saving
#endif
fx FxSin(fx radians);
fx FxCos(fx radians);
fx FxSinTurn(uint32_t turn); // binary angle: a full turn is 2^32
fx FxCosTurn(uint32_t turn);
uint32_t FxTurn(fx radians);
fx FxAtan2(fx y, fx x);
fx FxSqrt(fx a);
fx FxLen(fx x, fx y);        // sqrt(x*x + y*y) without overflow
fx FxLen3(fx x, fx y, fx z);
fx FxTan(fx radians);
fx FxExpNeg(fx x);       // e^-x for x >= 0
fx FxMod(fx a, fx m);        // like fmodf: keeps the sign of a
// Text for messages: a value with 0-3 decimals, from 4 rotating 16-byte buffers.
const char *FxStr(fx a, int decimals);
fx FxParse(const char *s);
fx FxFromFloatBits(uint32_t bits); // old version 1 saves stored decimals  // "2.5" -> FX(2.5), whole numbers only

#endif
