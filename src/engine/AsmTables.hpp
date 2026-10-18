// AsmTables.hpp - Bit-exact SIN.ASM / CSQRT.ASM routines
//
// Angles are 16-bit binary angles: 32768 = pi. ssin/scos return 1.15 fixed point.

#pragma once
#include <cmath>
#include <cstdint>

namespace corncob::engine {

extern const uint16_t kSinTbl[258];
extern const uint16_t kSqrtTbl[1024];
extern const uint16_t kAtnTbl[1024];

// SIN.ASM sin: |sin(a)| * 65536 (unsigned), negative flag returned separately
struct SinResult { uint16_t mag; bool neg; };
inline SinResult sinRaw(int16_t a) {
    uint16_t ax = static_cast<uint16_t>(a);
    if ((ax & 0x7fff) == 0) return {0, false};
    bool neg = false;
    if (ax & 0x8000) { neg = true; ax = static_cast<uint16_t>(-ax); }
    if (ax & 16384) ax = static_cast<uint16_t>(32768 - ax); // fold into first quadrant
    uint16_t frac = ax & 63;
    unsigned i = ax >> 6;
    uint16_t lo = kSinTbl[i], hi = kSinTbl[i + 1];
    uint16_t d = static_cast<uint16_t>(hi - lo);
    uint16_t interp = static_cast<uint16_t>((static_cast<uint32_t>(d) * frac) / 64);
    return {static_cast<uint16_t>(interp + lo), neg};
}

// SIN.ASM ssin / scos: signed 1.15 result
inline int16_t ssin(int16_t a) {
    SinResult r = sinRaw(a);
    int16_t v = static_cast<int16_t>(r.mag >> 1);
    return r.neg ? static_cast<int16_t>(-v) : v;
}
inline int16_t scos(int16_t a) { return ssin(static_cast<int16_t>(static_cast<uint16_t>(a) + 16384)); }

// CSQRT.ASM csqrt: table lookup, 0 (and carry) outside 0..1023
inline uint16_t csqrt(int16_t x) {
    if (x < 0 || x >= 1024) return 0;
    return kSqrtTbl[x];
}

// ATN.ASM atn2: angle of the vector (x, y) as a 16-bit binary angle (32768 = pi),
// 0 along +x. The quadrant bits live in cl (bit 0 negate, bit 1 add 180 degrees) and
// the octant swap is done on them exactly as the ASM does.
inline int16_t atn2(int16_t xIn, int16_t yIn) {
    uint16_t ax = static_cast<uint16_t>(xIn), dx = static_cast<uint16_t>(yIn);
    if (ax == 0x8000) ax++;
    uint8_t cl = 0, ch = 0;
    if (static_cast<int16_t>(ax) < 0) { cl |= 3; ax = static_cast<uint16_t>(-static_cast<int16_t>(ax)); }
    if (static_cast<int16_t>(dx) < 0) { cl ^= 5; dx = static_cast<uint16_t>(-static_cast<int16_t>(dx)); }
    if (ax == dx) {
        ax = 32768;
    } else {
        if (static_cast<uint16_t>(ax - dx) & 0x8000) {   // y bigger than x: work with x/y and add 90 degrees
            ch = static_cast<uint8_t>(ch + 64);
            const uint16_t t = ax; ax = dx; dx = t;
            cl ^= 1;
            const uint8_t bl = cl;
            cl = static_cast<uint8_t>(cl >> 2);
            cl = static_cast<uint8_t>((cl << 1) | (bl & 1));
        }
        const uint32_t q = (static_cast<uint32_t>(dx) << 16) / ax;   // div bx with dx:ax = small:0
        const uint16_t t = static_cast<uint16_t>(q >> 1);              // scale into 0-32767
        ax = kAtnTbl[(t >> 5) & 1023];
    }
    ax = static_cast<uint16_t>(ax >> 2);   // 0-32767 angle into 0-8191
    if (cl & 1) ax = static_cast<uint16_t>(-static_cast<int16_t>(ax));
    if (cl & 2) ch = static_cast<uint8_t>(ch + 128);
    return static_cast<int16_t>(static_cast<uint16_t>(ax + (static_cast<uint16_t>(ch) << 8)));
}

// atn2 without the table: the same angle to the nearest unit. The table (1024 entries per octant, indexed by the floor of the
// slope) leaves the angle on a grid of about 8 units with a bias, which is fine at 20 frames a second but at a shorter frame
// swallows or inflates each frame's small turn.
inline int16_t atn2Fine(int16_t xIn, int16_t yIn) {
    if (xIn == 0 && yIn == 0) return 0;
    const double a = std::atan2(static_cast<double>(yIn), static_cast<double>(xIn));
    const long u = std::lround(a * (65536.0 / 6.283185307179586));
    return static_cast<int16_t>(static_cast<uint16_t>(u & 0xFFFF));
}

// DSQRT.ASM dsqrt: square root of dx:ax (as the ASM does it, including that the result is the
// last quotient of the Newton iteration, and that 32767 means too large)
inline uint16_t dsqrt(uint32_t v) {
    if ((v >> 16) & 0xC000) return 32767;
    const uint16_t hi = static_cast<uint16_t>(v >> 16), lo = static_cast<uint16_t>(v);
    uint32_t w = v << 2;
    uint16_t bx = 32767, di = 23170;
    bool found = false;
    for (int cx = 14; cx; cx--) {
        bool carry = (w >> 31) & 1;
        w <<= 1;
        if (carry) { found = true; break; }          // usebx
        carry = (w >> 31) & 1;
        w <<= 1;
        if (carry) { bx = di; found = true; break; } // usedi
        bx >>= 1;
        di >>= 1;
    }
    if (!found) return 1;                            // don't return zero
    di = lo;
    uint16_t ax = 0;
    for (;;) {
        const uint16_t cx0 = bx;
        const uint32_t q = ((static_cast<uint32_t>(hi) << 16) | di) / (bx ? bx : 1);
        ax = static_cast<uint16_t>(q);
        bx = static_cast<uint16_t>(static_cast<uint16_t>(bx + ax) >> 1);
        int16_t diff = static_cast<int16_t>(cx0 - bx);
        if (diff < 0) diff = static_cast<int16_t>(-diff);
        if (static_cast<int16_t>(diff - 3) < 0) break;   // cmp cx,3 / jns
    }
    return ax;
}

} // namespace corncob::engine
