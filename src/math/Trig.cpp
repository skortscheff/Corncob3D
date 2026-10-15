// Trig.cpp - Trigonometric lookup tables and fast trig functions
// Ported from SIN.ASM, ATN.ASM, f3dvec.asm calcsin/negsin/calcmat

#include "Trig.hpp"
#include <cmath>

namespace corncob {

int16_t sin_table[256];

void init_sin_table() {
    for (int i = 0; i < 256; i++) {
        sin_table[i] = static_cast<int16_t>(std::lround(std::sin(i * 2.0 * PI / 256.0) * 32767.0));
    }
}

float fast_sin(float angleDeg) {
    // Convert to binary angle; the high byte indexes the table, the low byte interpolates
    int32_t a = static_cast<int32_t>(std::lround(angleDeg * (ANGLE_RANGE / 360.0f))) & 0xFFFF;
    int idx = a >> 8;
    int frac = a & 0xFF;
    int s0 = sin_table[idx];
    int s1 = sin_table[(idx + 1) & 0xFF];
    return (s0 + ((s1 - s0) * frac) / 256) / 32767.0f;
}

float fast_atan2(float y, float x) {
    return std::atan2(y, x) * RAD2DEG;
}

void calc_sin_cos_table() {
    init_sin_table();
}

// Matrix rows are the body axes in world space: row0 = right, row1 = up, row2 = forward.
// World: x east, y up, z north.
void extract_euler_from_matrix(const float m[9], float& yaw, float& pitch, float& roll) {
    float fx = m[6], fy = m[7], fz = m[8];
    float horiz = std::sqrt(fx * fx + fz * fz);
    pitch = fast_atan2(fy, horiz);
    if (horiz < 1e-5f) {
        // Pointing straight up/down: yaw is undefined, fold it into roll (same special case as getang)
        yaw = 0.0f;
        roll = fast_atan2(-m[2], m[0]);
        return;
    }
    yaw = fast_atan2(fx, fz);
    // Roll: angle of the right vector below the horizontal plane
    float ry = m[1];
    float uy = m[4];
    roll = fast_atan2(-ry, uy);
}

} // namespace corncob
