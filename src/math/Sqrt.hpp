// Sqrt.hpp - Fixed-point square root functions
// Ported from DSQRT.ASM, CSQRT.ASM, CSQRTS.ASM

#pragma once
#include "../core/Fixed.hpp"

namespace corncob {

// Fixed-point sqrt (same role as DSQRT.ASM)
Fixed sqrt_fixed(Fixed x);

// Integer sqrt (floor), same role as the 32-bit integer sqrt in SQRT.ASM
int32_t sqrt_int(int32_t n);

struct FixedComplex {
    Fixed real, imag;

    FixedComplex() = default;
    FixedComplex(Fixed r, Fixed i) : real(r), imag(i) {}

    FixedComplex operator*(const FixedComplex& o) const {
        return FixedComplex(real * o.real - imag * o.imag,
                            real * o.imag + imag * o.real);
    }
};

// Principal complex square root (same interface as CSQRT.ASM)
FixedComplex csqrt_fixed(FixedComplex z);

} // namespace corncob
