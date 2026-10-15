// Fixed.cpp - Implementation for fixed-point arithmetic (16.16 format)
// Ported from MASM imath.INC, hmul/madd macros in MAT.ASM and F3DVEC.ASM

#include "Fixed.hpp"
#include <iomanip>
#include <ostream>

namespace corncob {

std::ostream& operator<<(std::ostream& os, Fixed f) {
    os << std::fixed << std::setprecision(4) << f.toFloat();
    return os;
}

} // namespace corncob
