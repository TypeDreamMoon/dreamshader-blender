// Shadows intern/cycles/util/math.h for the parity harness — only the helpers the upstream noise
// and hash kernels reach for. Each body is upstream's own, copied from the file named beside it.

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

#ifndef M_PI_F
#  define M_PI_F 3.14159265358979323846f
#endif

// math_base.h
ccl_device_inline int float_to_int(const float f) { return int(f); }
ccl_device_inline float fractf(const float x) { return x - std::floor(x); }
ccl_device_inline float floorf(const float x) { return std::floor(x); }
ccl_device_inline float ceilf(const float x) { return std::ceil(x); }
ccl_device_inline float fabsf(const float x) { return std::fabs(x); }
ccl_device_inline float sqrtf(const float x) { return std::sqrt(x); }
ccl_device_inline float fmodf(const float a, const float b) { return std::fmod(a, b); }
ccl_device_inline float fminf(const float a, const float b) { return a < b ? a : b; }
ccl_device_inline float fmaxf(const float a, const float b) { return a > b ? a : b; }
ccl_device_inline float min(const float a, const float b) { return a < b ? a : b; }
ccl_device_inline float max(const float a, const float b) { return a > b ? a : b; }
ccl_device_inline float clamp(const float a, const float lo, const float hi) { return a < lo ? lo : (a > hi ? hi : a); }
ccl_device_inline float saturatef(const float a) { return clamp(a, 0.0f, 1.0f); }
ccl_device_inline float mix(const float a, const float b, const float t) { return a + t * (b - a); }

ccl_device_inline uint __float_as_uint(const float f) { uint r; std::memcpy(&r, &f, 4); return r; }
ccl_device_inline float __uint_as_float(const uint u) { float r; std::memcpy(&r, &u, 4); return r; }

// math_base.h — the integer part goes out through the pointer, the fraction is returned
ccl_device_inline float floorfrac(const float x, ccl_private int *i)
{
  const float f = std::floor(x);
  *i = float_to_int(f);
  return x - f;
}

// component-wise fabs / fmod / mask, used by the snoise precision correction
ccl_device_inline float2 fabs(const float2 a) { return {std::fabs(a.x), std::fabs(a.y)}; }
ccl_device_inline float3 fabs(const float3 a) { return {std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)}; }
ccl_device_inline float4 fabs(const float4 a) { return {std::fabs(a.x), std::fabs(a.y), std::fabs(a.z), std::fabs(a.w)}; }

ccl_device_inline float2 fmod(const float2 a, const float b) { return {std::fmod(a.x, b), std::fmod(a.y, b)}; }
ccl_device_inline float3 fmod(const float3 a, const float b) { return {std::fmod(a.x, b), std::fmod(a.y, b), std::fmod(a.z, b)}; }
ccl_device_inline float4 fmod(const float4 a, const float b) { return {std::fmod(a.x, b), std::fmod(a.y, b), std::fmod(a.z, b), std::fmod(a.w, b)}; }

// Upstream's `mask(cond, value)` keeps value where the comparison holds and zeroes it elsewhere.
// The comparison operators below produce the boolean vectors those calls pass in.
struct bool2 { bool x, y; };
struct bool3 { bool x, y, z; };
struct bool4 { bool x, y, z, w; };

ccl_device_inline bool2 operator>=(const float2 a, const float2 b) { return {a.x >= b.x, a.y >= b.y}; }
ccl_device_inline bool3 operator>=(const float3 a, const float3 b) { return {a.x >= b.x, a.y >= b.y, a.z >= b.z}; }
ccl_device_inline bool4 operator>=(const float4 a, const float4 b) { return {a.x >= b.x, a.y >= b.y, a.z >= b.z, a.w >= b.w}; }

ccl_device_inline float2 mask(const bool2 m, const float2 v) { return {m.x ? v.x : 0.0f, m.y ? v.y : 0.0f}; }
ccl_device_inline float3 mask(const bool3 m, const float3 v) { return {m.x ? v.x : 0.0f, m.y ? v.y : 0.0f, m.z ? v.z : 0.0f}; }
ccl_device_inline float4 mask(const bool4 m, const float4 v) { return {m.x ? v.x : 0.0f, m.y ? v.y : 0.0f, m.z ? v.z : 0.0f, m.w ? v.w : 0.0f}; }

// util/color.h's tail (xyY, spectrum) needs these. Spectrum is float3 in a non-spectral build.
using Spectrum = float3;
ccl_device_inline float sqr(const float a) { return a * a; }
ccl_device_inline float3 sqr(const float3 a) { return {a.x * a.x, a.y * a.y, a.z * a.z}; }
ccl_device_inline float2 operator/(const float2 a, const float s) { return {a.x / s, a.y / s}; }
ccl_device_inline float3 operator/(const float3 a, const float s) { return {a.x / s, a.y / s, a.z / s}; }
ccl_device_inline float4 operator/(const float4 a, const float s) { return {a.x / s, a.y / s, a.z / s, a.w / s}; }
ccl_device_inline float3 operator/(const float3 a, const float3 b) { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
ccl_device_inline float3 operator-(const float3 a) { return {-a.x, -a.y, -a.z}; }
ccl_device_inline float3 operator-(const float3 a, const float s) { return {a.x - s, a.y - s, a.z - s}; }

ccl_device_inline float dot(const float2 a, const float2 b) { return a.x * b.x + a.y * b.y; }
ccl_device_inline float dot(const float3 a, const float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
ccl_device_inline float dot(const float4 a, const float4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

// hash.h reaches for these two outside the noise path — a float4-to-float3 truncation and the
// bit-scan behind hash_shuffle_uint. Neither is on the compared path, but both have to exist for
// the upstream file to compile unmodified, which is the constraint that keeps it trustworthy.
ccl_device_inline float3 make_float3(const float4 v) { return {v.x, v.y, v.z}; }
ccl_device_inline uint count_leading_zeros(const uint x)
{
  if (x == 0) return 32;
  uint n = 0;
  uint v = x;
  while ((v & 0x80000000u) == 0) { v <<= 1; ++n; }
  return n;
}

ccl_device_inline float len(const float2 a) { return std::sqrt(dot(a, a)); }
ccl_device_inline float len(const float3 a) { return std::sqrt(dot(a, a)); }
ccl_device_inline float len(const float4 a) { return std::sqrt(dot(a, a)); }

CCL_NAMESPACE_END
