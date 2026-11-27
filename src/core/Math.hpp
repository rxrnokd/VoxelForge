#pragma once

// Small, dependency free math types used by the engine. The engine library does not
// depend on raylib, so everything that needs vectors/matrices uses these types and the
// render layer converts at the boundary.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace voxelforge {

inline constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    constexpr bool operator==(const Vec3&) const = default;

    constexpr float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    constexpr float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

inline constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline constexpr float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    return len > 1e-8f ? v / len : Vec3{};
}
inline constexpr Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

struct IVec3 {
    int x = 0;
    int y = 0;
    int z = 0;

    constexpr IVec3() = default;
    constexpr IVec3(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    constexpr IVec3 operator+(const IVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr IVec3 operator-(const IVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr bool operator==(const IVec3&) const = default;
    constexpr int& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    constexpr int operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

inline constexpr float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline constexpr float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline constexpr float smoothstep(float edge0, float edge1, float x) {
    const float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Integer division / modulo that round towards negative infinity (needed to map
// negative world coordinates onto chunks).
inline constexpr int floorDiv(int a, int b) {
    const int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}
inline constexpr int floorMod(int a, int b) {
    const int m = a % b;
    return (m != 0 && ((m < 0) != (b < 0))) ? m + b : m;
}
inline int fastFloor(float v) {
    const int i = static_cast<int>(v);
    return (v < static_cast<float>(i)) ? i - 1 : i;
}

// Column-major 4x4 matrix (same memory layout as OpenGL / GLSL).
struct Mat4 {
    std::array<float, 16> m{};

    static constexpr Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    constexpr float& at(int row, int col) { return m[static_cast<std::size_t>(col * 4 + row)]; }
    constexpr float at(int row, int col) const { return m[static_cast<std::size_t>(col * 4 + row)]; }

    constexpr Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c) {
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    s += at(rr, k) * o.at(k, c);
                }
                r.at(rr, c) = s;
            }
        }
        return r;
    }

    // Transforms a point (w = 1) and returns the clip-space xyzw.
    constexpr std::array<float, 4> transform(const Vec3& p) const {
        std::array<float, 4> out{};
        for (int r = 0; r < 4; ++r) {
            out[static_cast<std::size_t>(r)] = at(r, 0) * p.x + at(r, 1) * p.y + at(r, 2) * p.z + at(r, 3);
        }
        return out;
    }

    // Right handed perspective projection with OpenGL clip conventions (z in [-w, w]).
    static Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
        const float f = 1.0f / std::tan(fovYRadians * 0.5f);
        Mat4 r;
        r.at(0, 0) = f / aspect;
        r.at(1, 1) = f;
        r.at(2, 2) = (zFar + zNear) / (zNear - zFar);
        r.at(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
        r.at(3, 2) = -1.0f;
        return r;
    }

    // View matrix looking along `forward` from `eye` (right handed, +Y up).
    static Mat4 lookAt(const Vec3& eye, const Vec3& forward, const Vec3& worldUp) {
        const Vec3 f = normalize(forward);
        const Vec3 s = normalize(cross(f, worldUp));
        const Vec3 u = cross(s, f);
        Mat4 r = identity();
        r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;
        r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;
        r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z;
        r.at(0, 3) = -dot(s, eye);
        r.at(1, 3) = -dot(u, eye);
        r.at(2, 3) = dot(f, eye);
        return r;
    }

    static constexpr Mat4 translation(const Vec3& t) {
        Mat4 r = identity();
        r.at(0, 3) = t.x;
        r.at(1, 3) = t.y;
        r.at(2, 3) = t.z;
        return r;
    }

    // General inverse (cofactor expansion). Returns identity for singular matrices.
    Mat4 inverse() const;
};

inline Mat4 Mat4::inverse() const {
    const auto& a = m;
    std::array<float, 16> inv{};
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

    const float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    if (std::fabs(det) < 1e-12f) {
        return identity();
    }
    Mat4 r;
    const float invDet = 1.0f / det;
    for (std::size_t i = 0; i < 16; ++i) {
        r.m[i] = inv[i] * invDet;
    }
    return r;
}

// Direction the camera looks at for the given yaw/pitch (radians). Yaw 0 looks down -Z.
inline Vec3 directionFromYawPitch(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

} // namespace voxelforge
