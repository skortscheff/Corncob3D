// Vec3.hpp - 3D vector type for fixed-point coordinate math
// Ported from F3DEQU.INC (x=0, y=2, z=4 offsets) and f3dvec.asm coord arrays

#pragma once
#include "Fixed.hpp"
#include <iosfwd>

namespace corncob {

class Vec3 {
public:
    Fixed x, y, z; // 16.16 fixed-point coords

    Vec3() = default;
    Vec3(Fixed xi, Fixed yi, Fixed zi) : x(xi), y(yi), z(zi) {}
    Vec3(int xi, int yi, int zi)
        : x(Fixed::fromInt(xi)), y(Fixed::fromInt(yi)), z(Fixed::fromInt(zi)) {}
    Vec3(float xi, float yi, float zi)
        : x(Fixed::fromFloat(xi)), y(Fixed::fromFloat(yi)), z(Fixed::fromFloat(zi)) {}

    // Element access (same order as F3DEQU.INC: x=0, y=2, z=4 in the raw array)
    Fixed& operator[](int i);
    Fixed operator[](int i) const;

    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(Fixed s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(Fixed s) const { return Vec3(x / s, y / s, z / s); }

    Vec3& operator+=(const Vec3& o) { return *this = *this + o; }
    Vec3& operator-=(const Vec3& o) { return *this = *this - o; }
    Vec3& operator*=(Fixed s) { return *this = *this * s; }

    // Dot product (same as xdot in f3dvec.asm)
    Fixed dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const;
    Fixed magnitudeSq() const { return dot(*this); }
    Vec3 normalized() const;

    int xi() const { return x.toInt(); }
    int yi() const { return y.toInt(); }
    int zi() const { return z.toInt(); }
    float xf() const { return x.toFloat(); }
    float yf() const { return y.toFloat(); }
    float zf() const { return z.toFloat(); }
};

inline Vec3 operator*(Fixed s, const Vec3& v) { return v * s; }

inline Vec3 vecX() { return Vec3(1, 0, 0); }
inline Vec3 vecY() { return Vec3(0, 1, 0); }
inline Vec3 vecZ() { return Vec3(0, 0, 1); }
inline Vec3 vecZero() { return Vec3(); }

std::ostream& operator<<(std::ostream& os, const Vec3& v);

} // namespace corncob
