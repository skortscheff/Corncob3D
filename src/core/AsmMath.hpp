// AsmMath.hpp - Bit-exact equivalents of the original 8086 arithmetic macros
//
// Everything here reproduces the ASM *exactly*, including quirks, so that ported
// procs behave like the original. Sources:
//   hmul, bndck, wtadj, uwtadj    3DMAC.INC (hmul also in F3DVEC/MAP/MAT)
//   madd                          3.ASM / F3DVEC.ASM / MAP.ASM (16-bit, sign-flag clamp)
//   madd32 (MAT.ASM "madd")       MAT.ASM (32-bit saturating add)
//   mmul                          F3DVEC.ASM / MAP.ASM
//   dbndck                        3.ASM

#pragma once
#include <cstdint>

namespace corncob::asmx {

using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;

// Sign flag of a 16-bit CMP (a - b), i.e. what JS/JNS test after "cmp a,b".
// Note this is NOT a signed comparison: it ignores overflow.
constexpr bool sf16(i16 a, i16 b) { return (static_cast<u16>(a - b) & 0x8000) != 0; }

// ROL ax,1
constexpr i16 rol1(i16 v) {
    u16 u = static_cast<u16>(v);
    return static_cast<i16>(static_cast<u16>((u << 1) | (u >> 15)));
}

// 32-bit result of IMUL r16 (DX:AX)
constexpr i32 imul16(i16 a, i16 b) { return static_cast<i32>(a) * b; }
constexpr i16 hi16(i32 v) { return static_cast<i16>(static_cast<uint32_t>(v) >> 16); }
constexpr i16 lo16(i32 v) { return static_cast<i16>(static_cast<uint32_t>(v)); }

// hmul var1,var2: 1.15 x 1.15 multiply; result (DX) is the high word of the
// product shifted left once. The DX==8000h fixup is preserved even though a
// 16x16 product can never produce it.
struct HmulResult { i16 dx, ax; };
constexpr HmulResult hmulFull(i16 a, i16 b) {
    i32 p = imul16(a, b);
    u16 dx = static_cast<u16>(hi16(p)), ax = static_cast<u16>(lo16(p));
    if (dx == 0x8000) ax = static_cast<u16>(ax | 0xFF00);
    // shl ax,1 / rcl dx,1
    u16 ndx = static_cast<u16>((dx << 1) | (ax >> 15));
    u16 nax = static_cast<u16>(ax << 1);
    return {static_cast<i16>(ndx), static_cast<i16>(nax)};
}
constexpr i16 hmul(i16 a, i16 b) { return hmulFull(a, b).dx; }

// mmul var1,var2: plain IMUL, DX:AX
constexpr i32 mmul(i16 a, i16 b) { return imul16(a, b); }

// madd (3.ASM/F3DVEC.ASM/MAP.ASM): adds two 1.15 values at half scale, clamps
// to +-16383 using sign-flag tests, then ROLs back. Quirk: because the clamp uses
// JS after CMP, sums below about -16385 wrap the comparison and clamp to +16383.
constexpr i16 madd(i16 ax, i16 dx) {
    i16 v = static_cast<i16>((ax >> 1) + (dx >> 1)); // sar ax,1 / sar dx,1 / add
    if (!sf16(v, 16383)) {
        v = 16383;
    } else if (!sf16(v, -16383)) {
        // within range: keep v
    } else {
        v = -16383;
    }
    return rol1(v);
}

// MAT.ASM madd: true 32-bit add of two 16-bit values, saturated to +-32767
constexpr i16 madd32(i16 ax, i16 dx) {
    i32 s = static_cast<i32>(ax) + dx;
    if (s >= 0) return s - 32767 < 0 ? static_cast<i16>(s) : static_cast<i16>(32767);
    return s + 32767 >= 0 ? static_cast<i16>(s) : static_cast<i16>(-32767);
}

// bndck high,low: signed clamp of AX (JL/JG are proper signed compares)
constexpr i16 bndck(i16 ax, i16 high, i16 low) {
    if (!(ax < high)) ax = high;
    if (!(ax > low)) ax = low;
    return ax;
}

// dbndck high,low (3.ASM): clamp of DX using sign-flag tests (same quirk as madd)
constexpr i16 dbndck(i16 dx, i16 high, i16 low) {
    if (!sf16(dx, high)) dx = high;
    if (sf16(dx, low)) dx = low;
    return dx;
}

// IDIV r16 of DX:AX. On the 8086 a quotient outside 16 bits raises INT 0 (the
// game installs a div0 handler, DIV0.ASM); we saturate instead and flag it.
struct DivResult { i16 q, r; bool overflow; };
constexpr DivResult idiv32(i32 num, i16 den) {
    if (den == 0) return {static_cast<i16>(num >= 0 ? 32767 : -32768), 0, true};
    i32 q = num / den; // truncates toward zero, like IDIV
    i32 r = num % den;
    if (q > 32767 || q < -32768) return {static_cast<i16>(q > 0 ? 32767 : -32768), static_cast<i16>(r), true};
    return {static_cast<i16>(q), static_cast<i16>(r), false};
}

// wtadj: AX = AX * ofrmticks / ticksf  (scales a per-tick quantity by frame time)
constexpr i16 wtadj(i16 ax, i16 ofrmticks, i16 ticksf) { return idiv32(imul16(ax, ofrmticks), ticksf).q; }
// uwtadj: AX = AX * ticksf / ofrmticks (inverse of wtadj)
constexpr i16 uwtadj(i16 ax, i16 ofrmticks, i16 ticksf) { return idiv32(imul16(ax, ticksf), ofrmticks).q; }

} // namespace corncob::asmx
