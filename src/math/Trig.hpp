// Trig.hpp - Trigonometric functions with lookup tables
// Ported from SIN.ASM, ATN.ASM, and f3dvec.asm calcsin/negsin/calcmat

#pragma once
#include <cstdint>

namespace corncob {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

// Binary angle: full circle = 65536 units, same as the original 16-bit angle convention
constexpr int ANGLE_RANGE = 65536;
constexpr float DEG_PER_ANGLE = 360.0f / ANGLE_RANGE;

// Sine table: 256 entries over a full circle, values in ±32767 (1.15 format)
extern int16_t sin_table[256];
void init_sin_table();

// Table lookup with linear interpolation; angle in degrees, result in [-1, 1]
float fast_sin(float angleDeg);
inline float fast_cos(float angleDeg) { return fast_sin(angleDeg + 90.0f); }

// Negsin: reverse sign of sines (f3dvec.asm) - used for inverse rotations
inline float neg_sin(float a) { return -fast_sin(a); }

// atan2 in degrees (same role as atn2 in f3dvec.asm / ATN.ASM)
float fast_atan2(float y, float x);

void calc_sin_cos_table();

// Euler angle extraction from a row-major rotation matrix (getang in MAT.ASM)
void extract_euler_from_matrix(const float m[9], float& yaw, float& pitch, float& roll);

} // namespace corncob
