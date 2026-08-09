// Differential parity test: the DreamShader port against upstream Cycles, on the CPU.
//
// Left side  — the generated .ush, compiled as C++ through hlsl_shim.h. This is the exact text
//              that reaches the shader compiler, not a re-transcription of it.
// Right side — util/hash.h, kernel/svm/noise.h and kernel/svm/fractal_noise.h, downloaded from
//              the Blender repository and compiled verbatim on a minimal platform shim.
//
// Neither side is a rewrite of the other, so a transcription error in the port shows up as a
// mismatch rather than being reproduced identically on both sides. That is the whole point; a
// hand-written "reference" by the same author would agree with its own mistakes.
//
// Both sides run the same operations in the same order on the same libm, so integer results are
// required to be bit-identical and float results to agree within a few ULP. Anything looser would
// not catch a wrong constant, which is the failure mode this is here to find.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hlsl_shim.h"

namespace port {
// Using-declarations rather than a using-directive: they put the names *in* this namespace, so
// unqualified lookup stops here instead of also finding ::abs and ::fmod from <cmath> and
// reporting an ambiguity.
using hlsl::float2;
using hlsl::float3;
using hlsl::float4;
using hlsl::int2;
using hlsl::int3;
using hlsl::int4;
using hlsl::uint;
using hlsl::abs;
using hlsl::floor;
using hlsl::ceil;
using hlsl::sqrt;
using hlsl::exp;
using hlsl::sin;
using hlsl::cos;
using hlsl::acos;
using hlsl::atan2;
using hlsl::pow;
using hlsl::fmod;
using hlsl::min;
using hlsl::max;
using hlsl::clamp;
using hlsl::saturate;
using hlsl::lerp;
using hlsl::step;
using hlsl::smoothstep;
using hlsl::dot;
using hlsl::length;
using hlsl::normalize;
using hlsl::asuint;

#include "Generated.ush"
}  // namespace port

// Upstream, unmodified.
#include "kernel/svm/fractal_noise.h"
#include "kernel/svm/noise.h"
#include "util/hash.h"

// ------------------------------------------------------------------------------------------------

namespace {

int g_failures = 0;
int g_checks = 0;

// A deterministic input generator. Fixed seed: a parity run must be reproducible, and a failure
// must be reproducible from the report alone.
struct Rng {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return uint32_t(s >> 32);
  }
  // Coordinates worth testing: small, large, negative, and near cell boundaries, because floor()
  // and the integer cell hashes are where a sign or rounding mistake would hide.
  float coord() {
    const uint32_t r = next();
    const float u = float(r & 0xFFFFFF) / float(0xFFFFFF);
    switch (r >> 29) {
      case 0: return u * 4.0f - 2.0f;
      case 1: return u * 40.0f - 20.0f;
      case 2: return u * 2000.0f - 1000.0f;
      case 3: return std::floor(u * 20.0f - 10.0f);
      case 4: return std::floor(u * 20.0f - 10.0f) + 1e-6f;
      case 5: return std::floor(u * 20.0f - 10.0f) - 1e-6f;
      case 6: return u * 0.002f - 0.001f;
      default: return u * 200000.0f - 100000.0f;
    }
  }
  float unit() { return float(next() & 0xFFFFFF) / float(0xFFFFFF); }
};

int ulp_diff(const float a, const float b) {
  if (a == b) return 0;
  if (std::isnan(a) || std::isnan(b)) return (std::isnan(a) && std::isnan(b)) ? 0 : 1 << 30;
  int32_t ia, ib;
  std::memcpy(&ia, &a, 4);
  std::memcpy(&ib, &b, 4);
  if ((ia < 0) != (ib < 0)) return 1 << 30;
  const int64_t d = int64_t(ia) - int64_t(ib);
  return int(d < 0 ? -d : d);
}

struct Stat {
  const char *name;
  int worst = 0;
  int count = 0;
  float wa = 0, wb = 0;
};

void report(Stat &s, const int tolerance) {
  ++g_checks;
  const bool ok = s.worst <= tolerance;
  if (!ok) ++g_failures;
  std::printf("%-34s %7d cases   worst %6d ulp   %s\n", s.name, s.count, s.worst,
              ok ? "ok" : "MISMATCH");
  if (!ok) std::printf("    port %.9g   upstream %.9g\n", double(s.wa), double(s.wb));
}

void feed(Stat &s, const float a, const float b) {
  const int d = ulp_diff(a, b);
  ++s.count;
  if (d > s.worst) { s.worst = d; s.wa = a; s.wb = b; }
}

void feed_u(Stat &s, const uint32_t a, const uint32_t b) {
  ++s.count;
  if (a != b && s.worst < (1 << 30)) {
    s.worst = 1 << 30;
    s.wa = float(a);
    s.wb = float(b);
  }
}

}  // namespace

int main() {
  const int N = 20000;
  std::printf("dreamshader-blender parity: port (generated .ush) vs upstream Cycles\n\n");

  // ---- integer hashes: bit-identical or nothing ------------------------------------------------
  {
    Rng rng;
    Stat h1{"hash_uint"}, h2{"hash_uint2"}, h3{"hash_uint3"}, h4{"hash_uint4"};
    for (int i = 0; i < N; i++) {
      const uint32_t a = rng.next(), b = rng.next(), c = rng.next(), d = rng.next();
      feed_u(h1, port::DreamShaderFn_BLHash_uint1(a), ref::hash_uint(a));
      feed_u(h2, port::DreamShaderFn_BLHash_uint2(a, b), ref::hash_uint2(a, b));
      feed_u(h3, port::DreamShaderFn_BLHash_uint3(a, b, c), ref::hash_uint3(a, b, c));
      feed_u(h4, port::DreamShaderFn_BLHash_uint4(a, b, c, d), ref::hash_uint4(a, b, c, d));
    }
    report(h1, 0); report(h2, 0); report(h3, 0); report(h4, 0);
  }

  // ---- float hashes ------------------------------------------------------------------------------
  {
    Rng rng;
    Stat f1{"hash_float_to_float"}, f2{"hash_float2_to_float"};
    Stat f3{"hash_float3_to_float"}, f4{"hash_float4_to_float"};
    for (int i = 0; i < N; i++) {
      const float x = rng.coord(), y = rng.coord(), z = rng.coord(), w = rng.coord();
      feed(f1, port::DreamShaderFn_BLHash_float1_to_float(x), ref::hash_float_to_float(x));
      feed(f2, port::DreamShaderFn_BLHash_float2_to_float({x, y}),
           ref::hash_float2_to_float(ref::make_float2(x, y)));
      feed(f3, port::DreamShaderFn_BLHash_float3_to_float({x, y, z}),
           ref::hash_float3_to_float(ref::make_float3(x, y, z)));
      feed(f4, port::DreamShaderFn_BLHash_float4_to_float({x, y, z, w}),
           ref::hash_float4_to_float(ref::make_float4(x, y, z, w)));
    }
    report(f1, 0); report(f2, 0); report(f3, 0); report(f4, 0);
  }

  // ---- PCG hashes, used by Voronoi -----------------------------------------------------------------
  {
    Rng rng;
    Stat p2{"hash_int2_to_float2"}, p3{"hash_int3_to_float3"}, p4{"hash_int4_to_float4"};
    for (int i = 0; i < N; i++) {
      const int a = int(rng.next()), b = int(rng.next()), c = int(rng.next()), d = int(rng.next());
      const hlsl::float2 q2 = port::DreamShaderFn_BLHash_int2_to_float2({a, b});
      const ref::float2 r2 = ref::hash_int2_to_float2(ref::make_int2(a, b));
      feed(p2, q2.x, r2.x); feed(p2, q2.y, r2.y);

      const hlsl::float3 q3 = port::DreamShaderFn_BLHash_int3_to_float3({a, b, c});
      const ref::float3 r3 = ref::hash_int3_to_float3(ref::make_int3(a, b, c));
      feed(p3, q3.x, r3.x); feed(p3, q3.y, r3.y); feed(p3, q3.z, r3.z);

      const hlsl::float4 q4 = port::DreamShaderFn_BLHash_int4_to_float4({a, b, c, d});
      const ref::float4 r4 = ref::hash_int4_to_float4(ref::make_int4(a, b, c, d));
      feed(p4, q4.x, r4.x); feed(p4, q4.y, r4.y); feed(p4, q4.z, r4.z); feed(p4, q4.w, r4.w);
    }
    report(p2, 0); report(p3, 0); report(p4, 0);
  }

  // ---- Perlin ---------------------------------------------------------------------------------------
  {
    Rng rng;
    Stat p1{"perlin_1d"}, p2{"perlin_2d"}, p3{"perlin_3d"}, p4{"perlin_4d"};
    for (int i = 0; i < N; i++) {
      const float x = rng.coord(), y = rng.coord(), z = rng.coord(), w = rng.coord();
      feed(p1, port::DreamShaderFn_BLNoise_perlin_1d(x), ref::perlin_1d(x));
      feed(p2, port::DreamShaderFn_BLNoise_perlin_2d(x, y), ref::perlin_2d(x, y));
      feed(p3, port::DreamShaderFn_BLNoise_perlin_3d(x, y, z), ref::perlin_3d(x, y, z));
      feed(p4, port::DreamShaderFn_BLNoise_perlin_4d(x, y, z, w), ref::perlin_4d(x, y, z, w));
    }
    report(p1, 2); report(p2, 2); report(p3, 2); report(p4, 2);
  }

  // ---- signed noise, including the precision correction ------------------------------------------
  {
    Rng rng;
    Stat s1{"snoise_1d"}, s2{"snoise_2d"}, s3{"snoise_3d"}, s4{"snoise_4d"};
    for (int i = 0; i < N; i++) {
      const float x = rng.coord(), y = rng.coord(), z = rng.coord(), w = rng.coord();
      feed(s1, port::DreamShaderFn_BLNoise_snoise_1d(x), ref::snoise_1d(x));
      feed(s2, port::DreamShaderFn_BLNoise_snoise_2d({x, y}),
           ref::snoise_2d(ref::make_float2(x, y)));
      feed(s3, port::DreamShaderFn_BLNoise_snoise_3d({x, y, z}),
           ref::snoise_3d(ref::make_float3(x, y, z)));
      feed(s4, port::DreamShaderFn_BLNoise_snoise_4d({x, y, z, w}),
           ref::snoise_4d(ref::make_float4(x, y, z, w)));
    }
    report(s1, 2); report(s2, 2); report(s3, 2); report(s4, 2);
  }

  // ---- fBM, across detail values including fractional ---------------------------------------------
  {
    Rng rng;
    Stat b1{"fbm_1d"}, b2{"fbm_2d"}, b3{"fbm_3d"}, b4{"fbm_4d"}, bn{"fbm_3d unnormalized"};
    for (int i = 0; i < N / 4; i++) {
      const float x = rng.coord(), y = rng.coord(), z = rng.coord(), w = rng.coord();
      const float detail = rng.unit() * 8.0f;      // fractional on purpose
      const float rough = rng.unit();
      const float lac = 1.0f + rng.unit() * 3.0f;

      feed(b1, port::DreamShaderFn_BLNoise_fbm_1d(x, detail, rough, lac, 1.0f),
           ref::noise_fbm(x, detail, rough, lac, true));
      feed(b2, port::DreamShaderFn_BLNoise_fbm_2d({x, y}, detail, rough, lac, 1.0f),
           ref::noise_fbm(ref::make_float2(x, y), detail, rough, lac, true));
      feed(b3, port::DreamShaderFn_BLNoise_fbm_3d({x, y, z}, detail, rough, lac, 1.0f),
           ref::noise_fbm(ref::make_float3(x, y, z), detail, rough, lac, true));
      feed(b4, port::DreamShaderFn_BLNoise_fbm_4d({x, y, z, w}, detail, rough, lac, 1.0f),
           ref::noise_fbm(ref::make_float4(x, y, z, w), detail, rough, lac, true));
      feed(bn, port::DreamShaderFn_BLNoise_fbm_3d({x, y, z}, detail, rough, lac, 0.0f),
           ref::noise_fbm(ref::make_float3(x, y, z), detail, rough, lac, false));
    }
    report(b1, 4); report(b2, 4); report(b3, 4); report(b4, 4); report(bn, 4);
  }

  // ---- the other four fractal types -----------------------------------------------------------------
  {
    Rng rng;
    Stat mf{"multi_fractal_3d"}, ht{"hetero_terrain_3d"};
    Stat hy{"hybrid_multi_fractal_3d"}, rg{"ridged_multi_fractal_3d"};
    Stat mf4{"multi_fractal_4d"}, ht2{"hetero_terrain_2d"};
    for (int i = 0; i < N / 4; i++) {
      const float x = rng.coord(), y = rng.coord(), z = rng.coord(), w = rng.coord();
      const float detail = rng.unit() * 8.0f;
      const float rough = rng.unit();
      const float lac = 1.0f + rng.unit() * 3.0f;
      const float off = rng.unit() * 2.0f - 1.0f;
      const float gain = rng.unit() * 3.0f;

      const ref::float3 r3 = ref::make_float3(x, y, z);

      feed(mf, port::DreamShaderFn_BLNoise_multi_fractal_3d({x, y, z}, detail, rough, lac),
           ref::noise_multi_fractal(r3, detail, rough, lac));
      feed(ht, port::DreamShaderFn_BLNoise_hetero_terrain_3d({x, y, z}, detail, rough, lac, off),
           ref::noise_hetero_terrain(r3, detail, rough, lac, off));
      feed(hy, port::DreamShaderFn_BLNoise_hybrid_mf_3d({x, y, z}, detail, rough, lac, off, gain),
           ref::noise_hybrid_multi_fractal(r3, detail, rough, lac, off, gain));
      feed(rg, port::DreamShaderFn_BLNoise_ridged_mf_3d({x, y, z}, detail, rough, lac, off, gain),
           ref::noise_ridged_multi_fractal(r3, detail, rough, lac, off, gain));
      feed(mf4, port::DreamShaderFn_BLNoise_multi_fractal_4d({x, y, z, w}, detail, rough, lac),
           ref::noise_multi_fractal(ref::make_float4(x, y, z, w), detail, rough, lac));
      feed(ht2, port::DreamShaderFn_BLNoise_hetero_terrain_2d({x, y}, detail, rough, lac, off),
           ref::noise_hetero_terrain(ref::make_float2(x, y), detail, rough, lac, off));
    }
    report(mf, 8); report(ht, 8); report(hy, 8); report(rg, 8); report(mf4, 8); report(ht2, 8);
  }

  std::printf("\n%d checks, %d mismatches\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
