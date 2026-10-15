// AsmTables.hpp - Bit-exact SIN.ASM / CSQRT.ASM routines
//
// Angles are 16-bit binary angles: 32768 = pi. ssin/scos return 1.15 fixed point.

#pragma once
#include <cstdint>

namespace corncob::engine {

extern const uint16_t kSinTbl[258];
extern const uint16_t kSqrtTbl[1024];

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

} // namespace corncob::engine
