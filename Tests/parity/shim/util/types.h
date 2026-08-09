// Shadows intern/cycles/util/types.h for the parity harness — the vector types the upstream
// noise and hash kernels use, and nothing else.
//
// Signed overflow: the PCG hashes rely on wraparound and upstream marks them
// ccl_ignore_integer_overflow. The build passes -fwrapv so C++ agrees.

#pragma once

#include "util/defines.h"

#include <cmath>
#include <cstdint>
#include <cstring>

CCL_NAMESPACE_BEGIN

using uint = uint32_t;
using uchar = uint8_t;

struct float2 { float x = 0, y = 0; };
struct float3 { float x = 0, y = 0, z = 0; };
struct float4 { float x = 0, y = 0, z = 0, w = 0; };
struct int2 { int x = 0, y = 0; };
struct int3 { int x = 0, y = 0, z = 0; };
struct int4 { int x = 0, y = 0, z = 0, w = 0; };

// util/color.h opens with byte-colour conversion helpers. Nothing on the compared path uses them,
// but the file has to compile unmodified for the comparison to mean anything.
struct uchar4 { uchar x = 0, y = 0, z = 0, w = 0; };

ccl_device_inline float2 make_float2(const float x, const float y) { return {x, y}; }
ccl_device_inline float2 make_float2(const float v) { return {v, v}; }
ccl_device_inline float3 make_float3(const float x, const float y, const float z) { return {x, y, z}; }
ccl_device_inline float3 make_float3(const float v) { return {v, v, v}; }
ccl_device_inline float4 make_float4(const float x, const float y, const float z, const float w) { return {x, y, z, w}; }
ccl_device_inline float4 make_float4(const float v) { return {v, v, v, v}; }
ccl_device_inline float4 make_float4(const float3 v, const float w) { return {v.x, v.y, v.z, w}; }
ccl_device_inline float4 make_float4(const int4 v) { return {float(v.x), float(v.y), float(v.z), float(v.w)}; }

ccl_device_inline int2 make_int2(const int x, const int y) { return {x, y}; }
ccl_device_inline int2 make_int2(const int v) { return {v, v}; }
ccl_device_inline int3 make_int3(const int x, const int y, const int z) { return {x, y, z}; }
ccl_device_inline int3 make_int3(const int v) { return {v, v, v}; }
ccl_device_inline int3 make_int3(const float3 v) { return {int(v.x), int(v.y), int(v.z)}; }
ccl_device_inline int4 make_int4(const int x, const int y, const int z, const int w) { return {x, y, z, w}; }
ccl_device_inline int4 make_int4(const int v) { return {v, v, v, v}; }
ccl_device_inline int4 make_int4(const float4 v) { return {int(v.x), int(v.y), int(v.z), int(v.w)}; }

ccl_device_inline uchar4 make_uchar4(const uchar x, const uchar y, const uchar z, const uchar w) { return {x, y, z, w}; }

ccl_device_inline float3 zero_float3() { return {0.0f, 0.0f, 0.0f}; }
ccl_device_inline float4 zero_float4() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
ccl_device_inline int4 zero_int4() { return {0, 0, 0, 0}; }
ccl_device_inline float2 one_float2() { return {1.0f, 1.0f}; }
ccl_device_inline float3 one_float3() { return {1.0f, 1.0f, 1.0f}; }
ccl_device_inline float4 one_float4() { return {1.0f, 1.0f, 1.0f, 1.0f}; }

// float2
ccl_device_inline float2 operator+(const float2 a, const float2 b) { return {a.x + b.x, a.y + b.y}; }
ccl_device_inline float2 operator-(const float2 a, const float2 b) { return {a.x - b.x, a.y - b.y}; }
ccl_device_inline float2 operator*(const float2 a, const float2 b) { return {a.x * b.x, a.y * b.y}; }
ccl_device_inline float2 operator*(const float2 a, const float s) { return {a.x * s, a.y * s}; }
ccl_device_inline float2 operator*(const float s, const float2 a) { return {a.x * s, a.y * s}; }
ccl_device_inline float2 operator+(const float2 a, const float s) { return {a.x + s, a.y + s}; }
ccl_device_inline float2 &operator+=(float2 &a, const float2 b) { a = a + b; return a; }
ccl_device_inline float2 &operator*=(float2 &a, const float s) { a = a * s; return a; }

// float3
ccl_device_inline float3 operator+(const float3 a, const float3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
ccl_device_inline float3 operator-(const float3 a, const float3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
ccl_device_inline float3 operator*(const float3 a, const float3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
ccl_device_inline float3 operator*(const float3 a, const float s) { return {a.x * s, a.y * s, a.z * s}; }
ccl_device_inline float3 operator*(const float s, const float3 a) { return {a.x * s, a.y * s, a.z * s}; }
ccl_device_inline float3 operator+(const float3 a, const float s) { return {a.x + s, a.y + s, a.z + s}; }
ccl_device_inline float3 &operator+=(float3 &a, const float3 b) { a = a + b; return a; }
ccl_device_inline float3 &operator*=(float3 &a, const float s) { a = a * s; return a; }

// float4
ccl_device_inline float4 operator+(const float4 a, const float4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
ccl_device_inline float4 operator-(const float4 a, const float4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
ccl_device_inline float4 operator*(const float4 a, const float4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
ccl_device_inline float4 operator*(const float4 a, const float s) { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
ccl_device_inline float4 operator*(const float s, const float4 a) { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
ccl_device_inline float4 operator+(const float4 a, const float s) { return {a.x + s, a.y + s, a.z + s, a.w + s}; }
ccl_device_inline float4 &operator+=(float4 &a, const float4 b) { a = a + b; return a; }
ccl_device_inline float4 &operator*=(float4 &a, const float s) { a = a * s; return a; }

// integer vectors — the PCG hashes need multiply, add, xor, shift and mask
ccl_device_inline int2 operator+(const int2 a, const int2 b) { return {a.x + b.x, a.y + b.y}; }
ccl_device_inline int3 operator+(const int3 a, const int3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
ccl_device_inline int4 operator+(const int4 a, const int4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
ccl_device_inline int2 operator*(const int2 a, const int2 b) { return {a.x * b.x, a.y * b.y}; }
ccl_device_inline int3 operator*(const int3 a, const int3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
ccl_device_inline int4 operator*(const int4 a, const int4 b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
ccl_device_inline int2 operator^(const int2 a, const int2 b) { return {a.x ^ b.x, a.y ^ b.y}; }
ccl_device_inline int3 operator^(const int3 a, const int3 b) { return {a.x ^ b.x, a.y ^ b.y, a.z ^ b.z}; }
ccl_device_inline int4 operator^(const int4 a, const int4 b) { return {a.x ^ b.x, a.y ^ b.y, a.z ^ b.z, a.w ^ b.w}; }
ccl_device_inline int2 operator&(const int2 a, const int2 b) { return {a.x & b.x, a.y & b.y}; }
ccl_device_inline int3 operator&(const int3 a, const int3 b) { return {a.x & b.x, a.y & b.y, a.z & b.z}; }
ccl_device_inline int4 operator&(const int4 a, const int4 b) { return {a.x & b.x, a.y & b.y, a.z & b.z, a.w & b.w}; }
ccl_device_inline int2 operator>>(const int2 a, const int s) { return {a.x >> s, a.y >> s}; }
ccl_device_inline int3 operator>>(const int3 a, const int s) { return {a.x >> s, a.y >> s, a.z >> s}; }
ccl_device_inline int4 operator>>(const int4 a, const int s) { return {a.x >> s, a.y >> s, a.z >> s, a.w >> s}; }

CCL_NAMESPACE_END
