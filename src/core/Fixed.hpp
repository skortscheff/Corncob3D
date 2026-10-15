// Fixed.hpp - 16.16 fixed-point convenience type
// NOT bit-exact with the original: the 16-bit ASM arithmetic is in core/AsmMath.hpp.
// Kept for the presentation layer (HD mode, tools) only.

#pragma once
#include <cstdint>
#include <iosfwd>

namespace corncob {

// Scale factor: 2^16 = 65536
constexpr int FIXED_FRAC_BITS = 16;
constexpr int FIXED_SCALE = (1 << FIXED_FRAC_BITS); // 65536

class Fixed {
public:
    int32_t value; // raw fixed-point representation

    constexpr Fixed() : value(0) {}
    constexpr explicit Fixed(int32_t v) : value(v) {}

    static constexpr Fixed fromInt(int v) { return Fixed(v * FIXED_SCALE); }
    static constexpr Fixed fromFloat(float f) { return Fixed(static_cast<int32_t>(f * FIXED_SCALE)); }
    static constexpr Fixed fromDouble(double d) { return Fixed(static_cast<int32_t>(d * FIXED_SCALE)); }

    int toInt() const { return value >> FIXED_FRAC_BITS; }
    float toFloat() const { return value / static_cast<float>(FIXED_SCALE); }
    double toDouble() const { return value / static_cast<double>(FIXED_SCALE); }

    // Saturate a 64-bit intermediate to the 32-bit raw range (same idea as madd's overflow clamp)
    static Fixed saturate(int64_t raw) {
        if (raw > INT32_MAX) return Fixed(INT32_MAX);
        if (raw < INT32_MIN) return Fixed(INT32_MIN);
        return Fixed(static_cast<int32_t>(raw));
    }

    Fixed operator-() const { return Fixed(-value); }
    Fixed operator+(Fixed o) const { return saturate(static_cast<int64_t>(value) + o.value); }
    Fixed operator-(Fixed o) const { return saturate(static_cast<int64_t>(value) - o.value); }
    // Same as hmul: full-width multiply, then shift back down to 16.16
    Fixed operator*(Fixed o) const {
        return saturate((static_cast<int64_t>(value) * o.value) >> FIXED_FRAC_BITS);
    }
    Fixed operator/(Fixed o) const {
        if (o.value == 0) return Fixed(value >= 0 ? INT32_MAX : INT32_MIN); // div0 trap -> saturate
        return saturate((static_cast<int64_t>(value) << FIXED_FRAC_BITS) / o.value);
    }

    Fixed& operator+=(Fixed o) { return *this = *this + o; }
    Fixed& operator-=(Fixed o) { return *this = *this - o; }
    Fixed& operator*=(Fixed o) { return *this = *this * o; }
    Fixed& operator/=(Fixed o) { return *this = *this / o; }

    bool operator==(Fixed o) const { return value == o.value; }
    bool operator!=(Fixed o) const { return value != o.value; }
    bool operator>(Fixed o) const { return value > o.value; }
    bool operator<(Fixed o) const { return value < o.value; }
    bool operator>=(Fixed o) const { return value >= o.value; }
    bool operator<=(Fixed o) const { return value <= o.value; }
};

std::ostream& operator<<(std::ostream& os, Fixed f);

// High half of a 16x16 signed multiply (the dx result of imul, as used by hmul)
inline int16_t hmul_high(int16_t a, int16_t b) {
    return static_cast<int16_t>((static_cast<int32_t>(a) * b) >> 16);
}

// mmul macro (F3DVEC.ASM): 16x16 multiply of 1.15 fractions, shifted right by 15
inline int16_t mmul(int16_t a, int16_t b) {
    return static_cast<int16_t>((static_cast<int32_t>(a) * b) >> 15);
}

} // namespace corncob
