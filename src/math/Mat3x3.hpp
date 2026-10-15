// Mat3x3.hpp - 3x3 rotation matrix
// Float matrix with the same roles as MAT.ASM (calcmat, matvmul, rolpln) - NOT bit-exact;
// used by the placeholder flight model and, later, the presentation layer.

#pragma once
#include "../core/Vec3.hpp"
#include <iosfwd>

namespace corncob {

class Mat3x3 {
public:
    float m[9]; // Row-major 3x3 matrix (same layout as rolmat in MAT.ASM)

    Mat3x3() : m{1, 0, 0, 0, 1, 0, 0, 0, 1} {}

    static Mat3x3 identity() { return Mat3x3(); }

    // Rotation matrices about single axes (angles in degrees)
    static Mat3x3 rotX(float angleDeg);
    static Mat3x3 rotY(float angleDeg);
    static Mat3x3 rotZ(float angleDeg);
    static Mat3x3 fromAxisAngle(float angleDeg, int axis); // 0=x, 1=y, 2=z

    // Body orientation from Euler angles (calcmat): yaw about y, then pitch, then roll
    static Mat3x3 fromEuler(float yawDeg, float pitchDeg, float rollDeg);

    // Transpose == inverse for orthonormal matrices (ncalcmat)
    Mat3x3 inverse() const;

    Mat3x3 operator*(const Mat3x3& o) const;
    Mat3x3& operator*=(const Mat3x3& o) { return *this = *this * o; }

    // matvmul
    Vec3 operator*(const Vec3& v) const { return transform(v); }
    Vec3 transform(const Vec3& v) const;
    void transform(const float in[3], float out[3]) const;

    // getang
    void euler(float& yaw, float& pitch, float& roll) const;

    // rolpln: rotate in the body frame (pitch about right, yaw about up, roll about forward)
    void rotate(float yawDeg, float pitchDeg, float rollDeg);

    // Gram-Schmidt to remove drift from repeated incremental rotations
    void orthonormalize();

    float element(int row, int col) const { return m[row * 3 + col]; }
};

std::ostream& operator<<(std::ostream& os, const Mat3x3& mat);

} // namespace corncob
