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
using hlsl::frac;
using hlsl::trunc;
using hlsl::log;
using hlsl::tan;
using hlsl::asin;
using hlsl::atan;
using hlsl::sinh;
using hlsl::cosh;
using hlsl::tanh;
using hlsl::cross;
using hlsl::Texture2D;
using hlsl::SamplerState;

#include "Generated.ush"
}  // namespace port

// Upstream, unmodified.
#include "kernel/svm/fractal_noise.h"
#include "kernel/svm/noise.h"
#include "util/hash.h"

// Excerpted from the fetched upstream files by run.ps1 — see the comments there for what is cut
// and why. The text compiled here is upstream's, character for character.
#include "color_subset.h"
#include "projection_subset.h"

#include "gabor_subset.h"
#include "voronoi_subset.h"

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

  // ---- colour space conversions ----------------------------------------------------------------
  //
  // The longest hand-written thing in the Color family, and the one where an exact `==` decides
  // which hue sextant a pixel lands in — so the inputs deliberately include ties.
  {
    Rng rng;
    Stat h2v{"rgb_to_hsv"}, v2h{"hsv_to_rgb"}, h2l{"rgb_to_hsl"}, l2h{"hsl_to_rgb"};
    for (int i = 0; i < N; i++) {
      float r0 = rng.unit(), g0 = rng.unit(), b0 = rng.unit();
      if ((i & 7) == 0) { g0 = r0; }        // two channels equal
      if ((i & 7) == 1) { b0 = r0; }
      if ((i & 7) == 2) { r0 = g0 = b0; }   // grey: cmax == cmin

      const hlsl::float3 a1 = port::DreamShaderFn_BLColor_rgb_to_hsv({r0, g0, b0});
      const ref::float3 b1 = ref::rgb_to_hsv(ref::make_float3(r0, g0, b0));
      feed(h2v, a1.x, b1.x); feed(h2v, a1.y, b1.y); feed(h2v, a1.z, b1.z);

      const hlsl::float3 a2 = port::DreamShaderFn_BLColor_hsv_to_rgb({r0, g0, b0});
      const ref::float3 b2 = ref::hsv_to_rgb(ref::make_float3(r0, g0, b0));
      feed(v2h, a2.x, b2.x); feed(v2h, a2.y, b2.y); feed(v2h, a2.z, b2.z);

      const hlsl::float3 a3 = port::DreamShaderFn_BLColor_rgb_to_hsl({r0, g0, b0});
      const ref::float3 b3 = ref::rgb_to_hsl(ref::make_float3(r0, g0, b0));
      feed(h2l, a3.x, b3.x); feed(h2l, a3.y, b3.y); feed(h2l, a3.z, b3.z);

      const hlsl::float3 a4 = port::DreamShaderFn_BLColor_hsl_to_rgb({r0, g0, b0});
      const ref::float3 b4 = ref::hsl_to_rgb(ref::make_float3(r0, g0, b0));
      feed(l2h, a4.x, b4.x); feed(l2h, a4.y, b4.y); feed(l2h, a4.z, b4.z);
    }
    report(h2v, 0); report(v2h, 0); report(h2l, 0); report(l2h, 0);
  }

  // ---- Voronoi ----------------------------------------------------------------------------------
  //
  // The port splits by feature and dimension where upstream branches at run time, so each of the
  // twenty combinations is compared against the same upstream function with `params.feature` set.
  // Randomness stays in 0..1 and smoothness in 0..0.5, which is what the node clamps them to —
  // outside that the two sides are entitled to disagree because the port clamps at its entry
  // point and upstream clamps at the SVM node, which is cut away here.
  {
    Rng rng;
    Stat f1_2{"voronoi_f1_2d"}, f1_3{"voronoi_f1_3d"}, f1_4{"voronoi_f1_4d"};
    Stat sf_2{"voronoi_smooth_f1_2d"}, sf_3{"voronoi_smooth_f1_3d"};
    Stat f2_3{"voronoi_f2_3d"}, f2_4{"voronoi_f2_4d"};
    Stat de_2{"voronoi_dist_to_edge_2d"}, de_3{"voronoi_dist_to_edge_3d"};
    Stat ns_2{"voronoi_n_sphere_2d"}, ns_3{"voronoi_n_sphere_3d"};
    Stat fr_3{"fractal_voronoi_f1_3d"}, frs_2{"fractal_voronoi_smooth_2d"};
    Stat fde_3{"fractal_voronoi_edge_3d"};

    for (int i = 0; i < N / 4; i++) {
      const float x = rng.coord(), y = rng.coord(), z = rng.coord(), w = rng.coord();

      ref::VoronoiParams pr;
      pr.scale = 1.0f + rng.unit() * 4.0f;
      pr.detail = rng.unit() * 6.0f;
      pr.roughness = rng.unit();
      pr.lacunarity = 1.0f + rng.unit() * 3.0f;
      pr.smoothness = rng.unit() * 0.5f;
      pr.exponent = 0.25f + rng.unit() * 4.0f;
      pr.randomness = rng.unit();
      pr.max_distance = 0.5f + 0.5f * pr.randomness;
      pr.normalize = true;
      pr.feature = ref::NODE_VORONOI_F1;
      pr.metric = ref::NodeVoronoiDistanceMetric(rng.next() & 3);

      const float me = float(int(pr.metric));
      const ref::float2 c2 = ref::make_float2(x, y);
      const ref::float3 c3 = ref::make_float3(x, y, z);
      const ref::float4 c4 = ref::make_float4(x, y, z, w);

      // f1 — distance, colour and the packed position, all three
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_f1_2({x, y}, pr.randomness, me, pr.exponent, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_f1(pr, c2);
        feed(f1_2, d, o.distance);
        feed(f1_2, col.x, o.color.x); feed(f1_2, col.y, o.color.y); feed(f1_2, col.z, o.color.z);
        feed(f1_2, pos.x, o.position.x); feed(f1_2, pos.w, o.position.w);
      }
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_f1_3({x, y, z}, pr.randomness, me, pr.exponent, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_f1(pr, c3);
        feed(f1_3, d, o.distance);
        feed(f1_3, col.x, o.color.x); feed(f1_3, col.z, o.color.z);
        feed(f1_3, pos.y, o.position.y);
      }
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_f1_4({x, y, z, w}, pr.randomness, me, pr.exponent, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_f1(pr, c4);
        feed(f1_4, d, o.distance);
        feed(f1_4, col.y, o.color.y); feed(f1_4, pos.w, o.position.w);
      }

      // smooth f1 — the 5^N window and the correction-factor accumulation
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_smooth_f1_2({x, y}, pr.randomness, me, pr.exponent,
                                                              pr.smoothness, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_smooth_f1(pr, c2);
        feed(sf_2, d, o.distance);
        feed(sf_2, col.x, o.color.x); feed(sf_2, pos.x, o.position.x);
      }
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_smooth_f1_3({x, y, z}, pr.randomness, me, pr.exponent,
                                                              pr.smoothness, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_smooth_f1(pr, c3);
        feed(sf_3, d, o.distance);
        feed(sf_3, col.z, o.color.z); feed(sf_3, pos.z, o.position.z);
      }

      // f2 — the one that measures with the full metric where f1 uses the bound
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_f2_3({x, y, z}, pr.randomness, me, pr.exponent, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_f2(pr, c3);
        feed(f2_3, d, o.distance);
        feed(f2_3, col.x, o.color.x); feed(f2_3, pos.y, o.position.y);
      }
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_f2_4({x, y, z, w}, pr.randomness, me, pr.exponent, col, pos);
        const ref::VoronoiOutput o = ref::voronoi_f2(pr, c4);
        feed(f2_4, d, o.distance);
        feed(f2_4, col.z, o.color.z);
      }

      feed(de_2, port::DreamShaderFn_BLVor_dist_to_edge_2({x, y}, pr.randomness),
           ref::voronoi_distance_to_edge(pr, c2));
      feed(de_3, port::DreamShaderFn_BLVor_dist_to_edge_3({x, y, z}, pr.randomness),
           ref::voronoi_distance_to_edge(pr, c3));
      feed(ns_2, port::DreamShaderFn_BLVor_n_sphere_radius_2({x, y}, pr.randomness),
           ref::voronoi_n_sphere_radius(pr, c2));
      feed(ns_3, port::DreamShaderFn_BLVor_n_sphere_radius_3({x, y, z}, pr.randomness),
           ref::voronoi_n_sphere_radius(pr, c3));

      // The fractal wrappers, where the port had to split what upstream selects inside the loop.
      {
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_fractal_f1_3(
            {x, y, z}, pr.detail, pr.roughness, pr.lacunarity, pr.randomness, me, pr.exponent, 0.0f,
            pr.max_distance, 1.0f, pr.scale, col, pos);
        const ref::VoronoiOutput o = ref::fractal_voronoi_x_fx(pr, c3);
        feed(fr_3, d, o.distance);
        feed(fr_3, col.x, o.color.x); feed(fr_3, col.y, o.color.y);
        feed(fr_3, pos.x, o.position.x); feed(fr_3, pos.z, o.position.z);
      }
      {
        ref::VoronoiParams ps = pr;
        ps.feature = ref::NODE_VORONOI_SMOOTH_F1;
        hlsl::float3 col; hlsl::float4 pos;
        const float d = port::DreamShaderFn_BLVor_fractal_smoothf1_2(
            {x, y}, ps.detail, ps.roughness, ps.lacunarity, ps.randomness, me, ps.exponent,
            ps.smoothness, ps.max_distance, 1.0f, ps.scale, col, pos);
        const ref::VoronoiOutput o = ref::fractal_voronoi_x_fx(ps, c2);
        feed(frs_2, d, o.distance);
        feed(frs_2, col.y, o.color.y); feed(frs_2, pos.y, o.position.y);
      }
      feed(fde_3, port::DreamShaderFn_BLVor_fractal_d2e_3({x, y, z}, pr.detail, pr.roughness,
                                                          pr.lacunarity, pr.randomness,
                                                          pr.max_distance, 1.0f),
           ref::fractal_voronoi_distance_to_edge(pr, c3));
    }

    report(f1_2, 4); report(f1_3, 4); report(f1_4, 4);
    report(sf_2, 8); report(sf_3, 8);
    report(f2_3, 4); report(f2_4, 4);
    report(de_2, 4); report(de_3, 4);
    report(ns_2, 4); report(ns_3, 4);
    report(fr_3, 16); report(frs_2, 16); report(fde_3, 16);
  }

  // ---- Gabor -------------------------------------------------------------------------------------
  //
  // polar_to_cartesian and spherical_to_direction come from upstream's util/projection.h rather
  // than the shim. That matters: the port derived both from the inverse operations sitting in
  // gabor.h, and supplying them from the shim would compare that derivation against itself.
  {
    Rng rng;
    Stat k2{"gabor_kernel_2d"}, k3{"gabor_kernel_3d"};
    Stat o3{"gabor_orientation_3d"};
    Stat n2{"gabor_noise_2d"}, n3{"gabor_noise_3d"};
    Stat sd{"gabor_standard_deviation"};

    feed(sd, port::DreamShaderFn_BLGabor_stddev_2d(), ref::compute_2d_gabor_standard_deviation());
    feed(sd, port::DreamShaderFn_BLGabor_stddev_3d(), ref::compute_3d_gabor_standard_deviation());

    for (int i = 0; i < N / 8; i++) {
      const float px = rng.unit() * 2.0f - 1.0f;
      const float py = rng.unit() * 2.0f - 1.0f;
      const float pz = rng.unit() * 2.0f - 1.0f;
      const float freq = 0.5f + rng.unit() * 8.0f;
      const float orient = rng.unit() * 6.2831853f - 3.14159265f;
      const float isotropy = rng.unit();

      {
        const hlsl::float2 a = port::DreamShaderFn_BLGabor_kernel_2d({px, py}, freq, orient);
        const ref::float2 b = ref::compute_2d_gabor_kernel(ref::make_float2(px, py), freq, orient);
        feed(k2, a.x, b.x); feed(k2, a.y, b.y);
      }
      {
        const hlsl::float3 dir = hlsl::normalize(hlsl::float3(px, py, pz + 1.5f));
        const ref::float3 rdir = ref::normalize(ref::make_float3(px, py, pz + 1.5f));
        const hlsl::float2 a = port::DreamShaderFn_BLGabor_kernel_3d({px, py, pz}, freq, dir);
        const ref::float2 b = ref::compute_3d_gabor_kernel(ref::make_float3(px, py, pz), freq, rdir);
        feed(k3, a.x, b.x); feed(k3, a.y, b.y);

        const hlsl::float4 seed = {px, py, pz, float(i)};
        const hlsl::float3 ao = port::DreamShaderFn_BLGabor_orientation_3d(dir, isotropy, seed);
        const ref::float3 bo = ref::compute_3d_orientation(
            rdir, isotropy, ref::make_float4(px, py, pz, float(i)));
        feed(o3, ao.x, bo.x); feed(o3, ao.y, bo.y); feed(o3, ao.z, bo.z);
      }
      {
        const hlsl::float2 a = port::DreamShaderFn_BLGabor_noise_2d({px * 3.0f, py * 3.0f}, freq,
                                                                    isotropy, orient);
        const ref::float2 b = ref::compute_2d_gabor_noise(ref::make_float2(px * 3.0f, py * 3.0f),
                                                          freq, isotropy, orient);
        feed(n2, a.x, b.x); feed(n2, a.y, b.y);
      }
      {
        const hlsl::float3 dir = hlsl::normalize(hlsl::float3(0.3f, -0.5f, 1.0f));
        const ref::float3 rdir = ref::normalize(ref::make_float3(0.3f, -0.5f, 1.0f));
        const hlsl::float2 a = port::DreamShaderFn_BLGabor_noise_3d(
            {px * 3.0f, py * 3.0f, pz * 3.0f}, freq, isotropy, dir);
        const ref::float2 b = ref::compute_3d_gabor_noise(
            ref::make_float3(px * 3.0f, py * 3.0f, pz * 3.0f), freq, isotropy, rdir);
        feed(n3, a.x, b.x); feed(n3, a.y, b.y);
      }
    }

    report(sd, 0); report(k2, 4); report(k3, 4); report(o3, 8);
    report(n2, 16); report(n3, 16);
  }

  std::printf("\n%d checks, %d mismatches\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
