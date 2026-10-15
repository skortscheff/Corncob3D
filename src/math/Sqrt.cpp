// Sqrt.cpp - Fixed-point square root implementations
// Ported from DSQRT.ASM, CSQRT.ASM, CSQRTS.ASM

#include "Sqrt.hpp"
#include <cstdint>

namespace corncob {

// Bitwise integer sqrt on a 64-bit value (the shift/subtract method the ASM uses)
static uint64_t isqrt64(uint64_t n) {
    uint64_t res = 0;
    uint64_t bit = uint64_t(1) << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

Fixed sqrt_fixed(Fixed x) {
    if (x.value <= 0) return Fixed(0);
    // sqrt(v / 2^16) * 2^16 == sqrt(v * 2^16)
    return Fixed(static_cast<int32_t>(isqrt64(static_cast<uint64_t>(x.value) << FIXED_FRAC_BITS)));
}

int32_t sqrt_int(int32_t n) {
    if (n <= 0) return 0;
    return static_cast<int32_t>(isqrt64(static_cast<uint64_t>(n)));
}

FixedComplex csqrt_fixed(FixedComplex z) {
    const Fixed two = Fixed::fromInt(2);
    Fixed mag = sqrt_fixed(z.real * z.real + z.imag * z.imag);
    Fixed re = sqrt_fixed((mag + z.real) / two);
    Fixed im = sqrt_fixed((mag - z.real) / two);
    if (z.imag < Fixed()) im = -im;
    return FixedComplex(re, im);
}

} // namespace corncob
