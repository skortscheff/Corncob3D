// SasLib.hpp - Stand-ins for the author's sas.lib routines (not in the source tree)
//
// 3.ASM and the other modules call rand_w, randr_w, divrs_dw, divs_dw and the
// .muls_dw macro. Their behavior is inferred from how the callers use them:
//   .muls_dw bx,cx   dx:ax = low 32 bits of (dx:ax) * signed bx   (IMATH.INC, in the source)
//   divrs_dw         dx:ax = (dx:ax) / bx, signed, rounded to nearest (cf. .divr_dw)
//   divs_dw          dx:ax = (dx:ax) / bx, signed, truncated
//   rand_w           16 random bits (a 16-bit LCG stands in for the real generator)
//   randr_w          bx = range: ax in [0, bx)

#pragma once
#include <cstdint>

namespace corncob::sas {

inline int32_t muls_dw(int32_t v, int16_t m) {
    return static_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(v) * m));
}

// divrs_dw: a zero divisor or a quotient that does not fit saturates (the original
// raises INT 0, which DIV0.ASM turns into a quiet fix-up)
inline int32_t divrs_dw(int32_t num, int16_t den) {
    if (den == 0) return num >= 0 ? INT32_MAX : INT32_MIN;
    const bool neg = (num < 0) != (den < 0);
    uint64_t n = num < 0 ? static_cast<uint64_t>(-static_cast<int64_t>(num)) : static_cast<uint64_t>(num);
    const uint64_t d = den < 0 ? static_cast<uint64_t>(-static_cast<int32_t>(den)) : static_cast<uint64_t>(den);
    uint64_t q = n / d;
    if (2 * (n % d) >= d) q++;
    const int64_t r = neg ? -static_cast<int64_t>(q) : static_cast<int64_t>(q);
    return static_cast<int32_t>(r);
}

inline int32_t divs_dw(int32_t num, int16_t den) {
    if (den == 0) return num >= 0 ? INT32_MAX : INT32_MIN;
    return static_cast<int32_t>(static_cast<int64_t>(num) / den);
}

class Random {
public:
    uint16_t rand_w() {
        m_state = static_cast<uint16_t>(m_state * 25173u + 13849u);
        return m_state;
    }
    // randr_w: bx = range, returns [0, range)
    uint16_t randr_w(uint16_t range) {
        return static_cast<uint16_t>((static_cast<uint32_t>(rand_w()) * range) >> 16);
    }
    void seed(uint16_t s) { m_state = s; }

private:
    uint16_t m_state = 0x7777;
};

} // namespace corncob::sas
