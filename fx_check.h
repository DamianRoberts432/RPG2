// Type checker for the fixed-point maths, for the PC only. Building a file as
// C++ with FX_CHECK makes fx a class that refuses the silent mistakes plain
// int32_t lets through: fx * fx without FxMul, fx / fx without FxDiv, mixing
// in decimals, adding or comparing a whole number, ++ and (int) casts.
//   g++ -fsyntax-only -fpermissive -DFX_CHECK -x c++ combat.c
#include <stdint.h>
#include <cstddef>
struct fx {
    int32_t v;
    fx() = default;
    constexpr explicit fx(int32_t raw) : v(raw) {}
    constexpr fx(std::nullptr_t) : v(0) {} // a literal 0 is the same in both
    constexpr explicit operator bool() const { return v != 0; }
};
constexpr fx FxRaw(int64_t r) { return fx((int32_t)r); }
constexpr fx operator+(fx a, fx b) { return fx(a.v + b.v); }
constexpr fx operator-(fx a, fx b) { return fx(a.v - b.v); }
constexpr fx operator-(fx a) { return fx(-a.v); }
constexpr fx operator+(fx a) { return a; }
constexpr bool operator!(fx a) { return !a.v; }
inline fx &operator+=(fx &a, fx b) { a.v += b.v; return a; }
inline fx &operator-=(fx &a, fx b) { a.v -= b.v; return a; }
constexpr fx operator*(fx a, int b) { return fx(a.v * b); }
constexpr fx operator*(int a, fx b) { return fx(a * b.v); }
constexpr fx operator/(fx a, int b) { return fx(a.v / b); }
inline fx &operator*=(fx &a, int b) { a.v *= b; return a; }
inline fx &operator/=(fx &a, int b) { a.v /= b; return a; }
fx operator*(fx, fx) = delete;
fx operator/(fx, fx) = delete;
fx operator*(fx, double) = delete;
fx operator*(double, fx) = delete;
fx operator/(fx, double) = delete;
fx operator/(int, fx) = delete;
fx operator/(double, fx) = delete;
fx &operator*=(fx &, fx) = delete;
fx &operator/=(fx &, fx) = delete;
fx &operator*=(fx &, double) = delete;
fx &operator/=(fx &, double) = delete;
#define FX_CMP(op) constexpr bool operator op(fx a, fx b) { return a.v op b.v; }
FX_CMP(<) FX_CMP(>) FX_CMP(<=) FX_CMP(>=) FX_CMP(==) FX_CMP(!=)
#undef FX_CMP

#define FX_SHIFT 16
#define FX_ONE 65536
#define FX_HALF 32768
#define FX_PI FxRaw(205887)
#define FX_TWO_PI FxRaw(411775)
#define FX_HALF_PI FxRaw(102944)
#define FX(c) FxRaw((int64_t)((c) * 65536.0 + ((c) >= 0 ? 0.5 : -0.5)))
#define FxI(i) FxRaw((int64_t)(i) * FX_ONE)
static inline int FxTrunc(fx a) { return a.v / FX_ONE; }
static inline int FxFloor(fx a) { return a.v >> FX_SHIFT; }
static inline int FxRound(fx a) { return (a.v + FX_HALF) >> FX_SHIFT; }
static inline int FxCeil(fx a) { return (a.v + FX_ONE - 1) >> FX_SHIFT; }
#define FX_MAX FxRaw(INT32_MAX)
// Raw-bit maths inside 64-bit expressions (angles for FxTurn64) keeps C meaning.
constexpr int64_t operator*(int64_t a, fx b) { return a * b.v; }
constexpr int64_t operator+(int64_t a, fx b) { return a + b.v; }
constexpr fx operator*(long a, fx b) { return fx((int32_t)a * b.v); }
static inline uint32_t FxTurn64(int64_t radians) { return (uint32_t)((radians * 683565276LL) >> 16); }
static inline fx FxFrac(fx a) { return fx(a.v & (FX_ONE - 1)); }
static inline fx FxMul(fx a, fx b) { return fx((int32_t)(((int64_t)a.v * b.v) >> FX_SHIFT)); }
static inline fx FxDiv(fx a, fx b) { return b.v ? fx((int32_t)(((int64_t)a.v * FX_ONE) / b.v)) : fx(INT32_MAX); }
static inline fx FxAbs(fx a) { return a.v < 0 ? -a : a; }
static inline fx FxMin(fx a, fx b) { return a < b ? a : b; }
static inline fx FxMax(fx a, fx b) { return a > b ? a : b; }
static inline fx FxClamp(fx a, fx lo, fx hi) { return a < lo ? lo : a > hi ? hi : a; }
static inline fx FxLerp(fx a, fx b, fx t) { return a + FxMul(b - a, t); }
static inline bool FxAfter(fx a, fx b) { return (int32_t)((uint32_t)a.v - (uint32_t)b.v) > 0; }
static inline bool FxBefore(fx a, fx b) { return (int32_t)((uint32_t)a.v - (uint32_t)b.v) < 0; }
static inline fx FxWrapAdd(fx a, fx b) { return fx((int32_t)((uint32_t)a.v + (uint32_t)b.v)); }
static inline int FxMulI(fx a, fx b) { return (int)(((int64_t)a.v * b.v) >> 32); }
static inline int FxMulInt(int n, fx a) { return (int)(((int64_t)n * a.v + FX_HALF) >> FX_SHIFT); }
static inline float FxToF(fx a) { return a.v / 65536.0f; }
static inline fx FxFromF(float f) { return fx((int32_t)(f * 65536.0f)); }
static inline int32_t FxBits(fx a) { return a.v; }
