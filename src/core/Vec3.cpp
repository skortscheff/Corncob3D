// Vec3.cpp - Implementation for 3D vector math operations
// Ported from f3dvec.asm: matvmul, rotfrm, and coord array access patterns

#include "Vec3.hpp"
#include "../math/Sqrt.hpp"
#include <iomanip>
#include <ostream>

namespace corncob {

Fixed& Vec3::operator[](int i) {
    switch (i) {
        case 0: return x;
        case 1: return y;
        default: return z;
    }
}

Fixed Vec3::operator[](int i) const {
    switch (i) {
        case 0: return x;
        case 1: return y;
        default: return z;
    }
}

Vec3 Vec3::cross(const Vec3& o) const {
    return Vec3(y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x);
}

Vec3 Vec3::normalized() const {
    Fixed magSq = magnitudeSq();
    // Avoid division by zero (same as f3dpt/plfail1 check in f3dvec.asm)
    if (magSq <= Fixed()) return *this;
    return *this / sqrt_fixed(magSq);
}

std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    os << std::fixed << std::setprecision(4)
       << "(" << v.xf() << ", " << v.yf() << ", " << v.zf() << ")";
    return os;
}

} // namespace corncob
