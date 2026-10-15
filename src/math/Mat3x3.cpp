// Mat3x3.cpp - 3x3 rotation matrix implementation
// Ported from MAT.ASM: calcmat, ncalcmat, matvmul, rolpln, getang

#include "Mat3x3.hpp"
#include "Trig.hpp"
#include <cmath>
#include <iomanip>
#include <ostream>

namespace corncob {

Mat3x3 Mat3x3::rotX(float a) {
    float c = std::cos(a * DEG2RAD), s = std::sin(a * DEG2RAD);
    Mat3x3 r;
    r.m[4] = c; r.m[5] = -s;
    r.m[7] = s; r.m[8] = c;
    return r;
}

Mat3x3 Mat3x3::rotY(float a) {
    float c = std::cos(a * DEG2RAD), s = std::sin(a * DEG2RAD);
    Mat3x3 r;
    r.m[0] = c;  r.m[2] = s;
    r.m[6] = -s; r.m[8] = c;
    return r;
}

Mat3x3 Mat3x3::rotZ(float a) {
    float c = std::cos(a * DEG2RAD), s = std::sin(a * DEG2RAD);
    Mat3x3 r;
    r.m[0] = c; r.m[1] = -s;
    r.m[3] = s; r.m[4] = c;
    return r;
}

Mat3x3 Mat3x3::fromAxisAngle(float a, int axis) {
    switch (axis) {
        case 0: return rotX(a);
        case 1: return rotY(a);
        default: return rotZ(a);
    }
}

Mat3x3 Mat3x3::fromEuler(float yawDeg, float pitchDeg, float rollDeg) {
    Mat3x3 r;
    r.rotate(yawDeg, pitchDeg, rollDeg);
    return r;
}

Mat3x3 Mat3x3::inverse() const {
    Mat3x3 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r.m[i * 3 + j] = m[j * 3 + i];
    return r;
}

Mat3x3 Mat3x3::operator*(const Mat3x3& o) const {
    Mat3x3 r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            r.m[i * 3 + j] = m[i * 3] * o.m[j] + m[i * 3 + 1] * o.m[3 + j] + m[i * 3 + 2] * o.m[6 + j];
    return r;
}

void Mat3x3::transform(const float in[3], float out[3]) const {
    out[0] = m[0] * in[0] + m[1] * in[1] + m[2] * in[2];
    out[1] = m[3] * in[0] + m[4] * in[1] + m[5] * in[2];
    out[2] = m[6] * in[0] + m[7] * in[1] + m[8] * in[2];
}

Vec3 Mat3x3::transform(const Vec3& v) const {
    float in[3] = {v.xf(), v.yf(), v.zf()}, out[3];
    transform(in, out);
    return Vec3(out[0], out[1], out[2]);
}

void Mat3x3::euler(float& yaw, float& pitch, float& roll) const {
    extract_euler_from_matrix(m, yaw, pitch, roll);
}

// Rows of this matrix are body axes in world space, so a body-frame rotation
// is applied by left-multiplying.
void Mat3x3::rotate(float yawDeg, float pitchDeg, float rollDeg) {
    *this = rotZ(rollDeg) * rotX(pitchDeg) * rotY(-yawDeg) * *this;
}

void Mat3x3::orthonormalize() {
    auto norm = [](float* v) {
        float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (l > 1e-8f) { v[0] /= l; v[1] /= l; v[2] /= l; }
    };
    float* r = &m[0]; float* u = &m[3]; float* f = &m[6];
    norm(f);
    // r = u x f, then u = f x r
    r[0] = u[1] * f[2] - u[2] * f[1];
    r[1] = u[2] * f[0] - u[0] * f[2];
    r[2] = u[0] * f[1] - u[1] * f[0];
    norm(r);
    u[0] = f[1] * r[2] - f[2] * r[1];
    u[1] = f[2] * r[0] - f[0] * r[2];
    u[2] = f[0] * r[1] - f[1] * r[0];
}

std::ostream& operator<<(std::ostream& os, const Mat3x3& mat) {
    os << std::fixed << std::setprecision(4);
    for (int i = 0; i < 3; i++)
        os << "[" << mat.m[i * 3] << ", " << mat.m[i * 3 + 1] << ", " << mat.m[i * 3 + 2] << "]\n";
    return os;
}

} // namespace corncob
