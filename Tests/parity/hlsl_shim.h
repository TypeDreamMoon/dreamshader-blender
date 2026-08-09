// Just enough HLSL to compile the DreamShader-generated .ush as C++.
//
// The generated include uses a small, closed set of constructs — the vector types, one swizzle
// (.xyz), and about twenty intrinsics. This header supplies exactly those, so the port can be
// executed on the CPU and compared against upstream Cycles function by function.
//
// Nothing here is approximate. Every intrinsic maps to the same IEEE operation the GPU would
// perform for these inputs; where HLSL and C differ (integer shifts, asuint) the C++ spelling is
// chosen to match HLSL, not to be convenient.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace hlsl {

using uint = uint32_t;

struct float2;
struct float3;
struct float4;
struct int2;
struct int3;
struct int4;

// ---- float2 ------------------------------------------------------------------------------------

struct float2 {
  float x = 0, y = 0;
  float2() = default;
  float2(float v) : x(v), y(v) {}
  float2(float a, float b) : x(a), y(b) {}
  explicit float2(struct int2 v);
};

inline float2 operator+(float2 a, float2 b) { return {a.x + b.x, a.y + b.y}; }
inline float2 operator-(float2 a, float2 b) { return {a.x - b.x, a.y - b.y}; }
inline float2 operator*(float2 a, float2 b) { return {a.x * b.x, a.y * b.y}; }
inline float2 operator/(float2 a, float2 b) { return {a.x / b.x, a.y / b.y}; }
inline float2 operator*(float2 a, float s) { return {a.x * s, a.y * s}; }
inline float2 operator*(float s, float2 a) { return {a.x * s, a.y * s}; }
inline float2 operator/(float2 a, float s) { return {a.x / s, a.y / s}; }
inline float2 operator-(float2 a) { return {-a.x, -a.y}; }
inline float2 operator+(float2 a, float s) { return {a.x + s, a.y + s}; }
inline float2 operator-(float2 a, float s) { return {a.x - s, a.y - s}; }
inline float2 &operator+=(float2 &a, float2 b) { a = a + b; return a; }
inline float2 &operator*=(float2 &a, float s) { a = a * s; return a; }
inline float2 &operator/=(float2 &a, float s) { a = a / s; return a; }
inline float2 &operator-=(float2 &a, float2 b) { a = a - b; return a; }

// ---- float3 ------------------------------------------------------------------------------------

struct float3 {
  float x = 0, y = 0, z = 0;
  float3() = default;
  float3(float v) : x(v), y(v), z(v) {}
  float3(float a, float b, float c) : x(a), y(b), z(c) {}
  explicit float3(struct int3 v);
};

inline float3 operator+(float3 a, float3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline float3 operator-(float3 a, float3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline float3 operator*(float3 a, float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline float3 operator/(float3 a, float3 b) { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
inline float3 operator*(float3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float3 operator*(float s, float3 a) { return {a.x * s, a.y * s, a.z * s}; }
inline float3 operator/(float3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline float3 operator-(float3 a) { return {-a.x, -a.y, -a.z}; }
inline float3 operator+(float3 a, float s) { return {a.x + s, a.y + s, a.z + s}; }
inline float3 operator-(float3 a, float s) { return {a.x - s, a.y - s, a.z - s}; }
inline float3 &operator+=(float3 &a, float3 b) { a = a + b; return a; }
inline float3 &operator/=(float3 &a, float s) { a = a / s; return a; }
inline float3 &operator-=(float3 &a, float3 b) { a = a - b; return a; }
inline float3 &operator*=(float3 &a, float3 b) { a = a * b; return a; }
inline float3 &operator*=(float3 &a, float s) { a = a * s; return a; }

// ---- float4 ------------------------------------------------------------------------------------
//
// `.xyz` is the only swizzle the generated code uses. It is a method rather than a union member
// because float3 has constructors and so cannot live in an anonymous aggregate; the staging step
// in run.ps1 rewrites `.xyz` to `.xyz()`.

struct float4 {
  float x = 0, y = 0, z = 0, w = 0;
  float4() = default;
  float4(float v) : x(v), y(v), z(v), w(v) {}
  float4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
  float4(float3 v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
  explicit float4(struct int4 v);
  float3 xyz() const { return {x, y, z}; }
};

inline float4 operator+(float4 a, float4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
inline float4 operator-(float4 a, float4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
inline float4 operator*(float4 a, float4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
inline float4 operator/(float4 a, float4 b) { return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w}; }
inline float4 operator*(float4 a, float s) { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
inline float4 operator*(float s, float4 a) { return a * s; }
inline float4 operator/(float4 a, float s) { return {a.x / s, a.y / s, a.z / s, a.w / s}; }
inline float4 operator-(float4 a) { return {-a.x, -a.y, -a.z, -a.w}; }
inline float4 operator+(float4 a, float s) { return {a.x + s, a.y + s, a.z + s, a.w + s}; }
inline float4 operator-(float4 a, float s) { return {a.x - s, a.y - s, a.z - s, a.w - s}; }
inline float4 &operator+=(float4 &a, float4 b) { a = a + b; return a; }
inline float4 &operator*=(float4 &a, float s) { a = a * s; return a; }
inline float4 &operator/=(float4 &a, float s) { a = a / s; return a; }
inline float4 &operator-=(float4 &a, float4 b) { a = a - b; return a; }
inline float4 &operator*=(float4 &a, float4 b) { a = a * b; return a; }

// ---- integer vectors -----------------------------------------------------------------------------

struct int2 {
  int x = 0, y = 0;
  int2() = default;
  int2(int v) : x(v), y(v) {}
  int2(int a, int b) : x(a), y(b) {}
  int2(float2 v) : x(int(v.x)), y(int(v.y)) {}
};
struct int3 {
  int x = 0, y = 0, z = 0;
  int3() = default;
  int3(int v) : x(v), y(v), z(v) {}
  int3(int a, int b, int c) : x(a), y(b), z(c) {}
  int3(float3 v) : x(int(v.x)), y(int(v.y)), z(int(v.z)) {}
};
struct int4 {
  int x = 0, y = 0, z = 0, w = 0;
  int4() = default;
  int4(int v) : x(v), y(v), z(v), w(v) {}
  int4(int a, int b, int c, int d) : x(a), y(b), z(c), w(d) {}
  int4(float4 v) : x(int(v.x)), y(int(v.y)), z(int(v.z)), w(int(v.w)) {}
};

inline int2 operator+(int2 a, int2 b) { return {a.x + b.x, a.y + b.y}; }
inline int3 operator+(int3 a, int3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline int4 operator+(int4 a, int4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
inline int2 operator*(int2 a, int s) { return {a.x * s, a.y * s}; }
inline int3 operator*(int3 a, int s) { return {a.x * s, a.y * s, a.z * s}; }
inline int4 operator*(int4 a, int s) { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
inline int2 operator+(int2 a, int s) { return {a.x + s, a.y + s}; }
inline int3 operator+(int3 a, int s) { return {a.x + s, a.y + s, a.z + s}; }
inline int4 operator+(int4 a, int s) { return {a.x + s, a.y + s, a.z + s, a.w + s}; }
inline int2 operator&(int2 a, int s) { return {a.x & s, a.y & s}; }
inline int3 operator&(int3 a, int s) { return {a.x & s, a.y & s, a.z & s}; }
inline int4 operator&(int4 a, int s) { return {a.x & s, a.y & s, a.z & s, a.w & s}; }
inline int2 operator^(int2 a, int2 b) { return {a.x ^ b.x, a.y ^ b.y}; }
inline int3 operator^(int3 a, int3 b) { return {a.x ^ b.x, a.y ^ b.y, a.z ^ b.z}; }
inline int4 operator^(int4 a, int4 b) { return {a.x ^ b.x, a.y ^ b.y, a.z ^ b.z, a.w ^ b.w}; }
inline int2 operator>>(int2 a, int s) { return {a.x >> s, a.y >> s}; }
inline int3 operator>>(int3 a, int s) { return {a.x >> s, a.y >> s, a.z >> s}; }
inline int4 operator>>(int4 a, int s) { return {a.x >> s, a.y >> s, a.z >> s, a.w >> s}; }

// Defined here because they need the integer types to be complete. HLSL spells these
// `float3(cellOffset)`; C++ needs a real conversion.
inline float2::float2(struct int2 v) : x(float(v.x)), y(float(v.y)) {}
inline float3::float3(struct int3 v) : x(float(v.x)), y(float(v.y)), z(float(v.z)) {}
inline float4::float4(struct int4 v) : x(float(v.x)), y(float(v.y)), z(float(v.z)), w(float(v.w)) {}

// ---- intrinsics ----------------------------------------------------------------------------------

inline float abs(float v) { return std::fabs(v); }
inline float2 abs(float2 v) { return {std::fabs(v.x), std::fabs(v.y)}; }
inline float3 abs(float3 v) { return {std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)}; }
inline float4 abs(float4 v) { return {std::fabs(v.x), std::fabs(v.y), std::fabs(v.z), std::fabs(v.w)}; }
inline int abs(int v) { return v < 0 ? -v : v; }

inline float floor(float v) { return std::floor(v); }
inline float2 floor(float2 v) { return {std::floor(v.x), std::floor(v.y)}; }
inline float3 floor(float3 v) { return {std::floor(v.x), std::floor(v.y), std::floor(v.z)}; }
inline float4 floor(float4 v) { return {std::floor(v.x), std::floor(v.y), std::floor(v.z), std::floor(v.w)}; }

inline float ceil(float v) { return std::ceil(v); }
inline float sqrt(float v) { return std::sqrt(v); }
inline float exp(float v) { return std::exp(v); }
inline float sin(float v) { return std::sin(v); }
inline float cos(float v) { return std::cos(v); }
inline float acos(float v) { return std::acos(v); }
inline float atan2(float y, float x) { return std::atan2(y, x); }
inline float pow(float a, float b) { return std::pow(a, b); }
inline float2 pow(float2 a, float b) { return {std::pow(a.x, b), std::pow(a.y, b)}; }
inline float3 pow(float3 a, float b) { return {std::pow(a.x, b), std::pow(a.y, b), std::pow(a.z, b)}; }
inline float4 pow(float4 a, float b) { return {std::pow(a.x, b), std::pow(a.y, b), std::pow(a.z, b), std::pow(a.w, b)}; }

// HLSL fmod is C fmod: truncated, sign follows the dividend.
inline float fmod(float a, float b) { return std::fmod(a, b); }
inline float2 fmod(float2 a, float b) { return {std::fmod(a.x, b), std::fmod(a.y, b)}; }
inline float3 fmod(float3 a, float b) { return {std::fmod(a.x, b), std::fmod(a.y, b), std::fmod(a.z, b)}; }
inline float4 fmod(float4 a, float b) { return {std::fmod(a.x, b), std::fmod(a.y, b), std::fmod(a.z, b), std::fmod(a.w, b)}; }

inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }
inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }

inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float saturate(float v) { return clamp(v, 0.0f, 1.0f); }

inline float lerp(float a, float b, float t) { return a + t * (b - a); }
inline float2 lerp(float2 a, float2 b, float t) { return a + (b - a) * t; }
inline float3 lerp(float3 a, float3 b, float t) { return a + (b - a) * t; }
inline float4 lerp(float4 a, float4 b, float t) { return a + (b - a) * t; }

inline float step(float edge, float v) { return v >= edge ? 1.0f : 0.0f; }
inline float2 step(float edge, float2 v) { return {step(edge, v.x), step(edge, v.y)}; }
inline float3 step(float edge, float3 v) { return {step(edge, v.x), step(edge, v.y), step(edge, v.z)}; }
inline float4 step(float edge, float4 v) { return {step(edge, v.x), step(edge, v.y), step(edge, v.z), step(edge, v.w)}; }

inline float smoothstep(float e0, float e1, float v) {
  const float t = clamp((v - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

inline float dot(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
inline float dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float dot(float4 a, float4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

inline float length(float2 v) { return std::sqrt(dot(v, v)); }
inline float length(float3 v) { return std::sqrt(dot(v, v)); }
inline float length(float4 v) { return std::sqrt(dot(v, v)); }

inline float2 normalize(float2 v) { return v / length(v); }
inline float3 normalize(float3 v) { return v / length(v); }
inline float4 normalize(float4 v) { return v / length(v); }

// HLSL asuint is a bit reinterpretation, not a value conversion — the same thing Cycles gets from
// __float_as_uint and from its implicit int-to-uint narrowing.
inline uint asuint(float v) { uint r; std::memcpy(&r, &v, 4); return r; }
inline uint asuint(int v) { return static_cast<uint>(v); }

// ---- intrinsics reached by the Converter / Color / Vector families ---------------------------

inline float frac(float v) { return v - std::floor(v); }
inline float2 frac(float2 v) { return {frac(v.x), frac(v.y)}; }
inline float3 frac(float3 v) { return {frac(v.x), frac(v.y), frac(v.z)}; }
inline float trunc(float v) { return std::trunc(v); }
inline float log(float v) { return std::log(v); }
inline float tan(float v) { return std::tan(v); }
inline float asin(float v) { return std::asin(v); }
inline float atan(float v) { return std::atan(v); }
inline float sinh(float v) { return std::sinh(v); }
inline float cosh(float v) { return std::cosh(v); }
inline float tanh(float v) { return std::tanh(v); }

inline float3 sin(float3 v) { return {std::sin(v.x), std::sin(v.y), std::sin(v.z)}; }
inline float3 cos(float3 v) { return {std::cos(v.x), std::cos(v.y), std::cos(v.z)}; }
inline float3 tan(float3 v) { return {std::tan(v.x), std::tan(v.y), std::tan(v.z)}; }
inline float3 ceil(float3 v) { return {std::ceil(v.x), std::ceil(v.y), std::ceil(v.z)}; }

// Component-wise, as in HLSL.
inline float2 min(float2 a, float2 b) { return {min(a.x, b.x), min(a.y, b.y)}; }
inline float3 min(float3 a, float3 b) { return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)}; }
inline float4 min(float4 a, float4 b) { return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w)}; }
inline float2 max(float2 a, float2 b) { return {max(a.x, b.x), max(a.y, b.y)}; }
inline float3 max(float3 a, float3 b) { return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)}; }
inline float4 max(float4 a, float4 b) { return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w)}; }

inline float3 clamp(float3 v, float3 lo, float3 hi) {
  return {clamp(v.x, lo.x, hi.x), clamp(v.y, lo.y, hi.y), clamp(v.z, lo.z, hi.z)};
}
inline float3 saturate(float3 v) { return {saturate(v.x), saturate(v.y), saturate(v.z)}; }

inline float3 cross(float3 a, float3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

}  // namespace hlsl
