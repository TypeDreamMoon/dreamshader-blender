// Entry points for the HLSL compile gate.
//
// `Generated.ush` is the DreamShader-generated include, copied here by compile-hlsl.ps1 under a
// stable name — its real filename carries a per-project path hash.
//
// Compiled separately, not together. fxc only walks the graph reachable from the entry it is
// given, and DXBC has no function calls, so it inlines whatever it reaches — the union entry
// point costs minutes under fxc while the targeted ones cover the same code in seconds. dxc keeps
// real calls in DXIL and takes the union happily. See README.md for the measurements.
//
// Distortion is non-zero throughout, so distortion branches and the seed-offset helpers are
// compiled rather than folded away.

// Curves.dsh samples a LUT through UE's Common.ush macro. The gate compiles the generated
// include on its own, so the macro has to be declared here — this is UE's own definition, not a
// stand-in that could hide a mistake.
#ifndef Texture2DSampleLevel
#define Texture2DSampleLevel(Tex, Sampler, UV, Mip) Tex.SampleLevel(Sampler, UV, Mip)
#endif

#include "Generated.ush"

// Full coverage: every public entry point, every dropdown dynamic.
float4 MainPS(float2 uv : TEXCOORD0, float4 knobs : TEXCOORD1) : SV_Target
{
    float type = knobs.x;
    float w    = knobs.y;
    float mode = knobs.z;
    float depth = knobs.w;

    float3 white = float3(1.0, 1.0, 1.0);
    float3 black = float3(0.0, 0.0, 0.0);
    float3 grey  = float3(0.5, 0.5, 0.5);
    float3 p     = float3(uv, 0.0);

    float3 c[17];
    float  f[17];

    f[0]  = DreamShaderFn_BL_TexNoise1D(uv.x, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c[0]);
    f[1]  = DreamShaderFn_BL_TexNoise2D(uv, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c[1]);
    f[2]  = DreamShaderFn_BL_TexNoise3D(p, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c[2]);
    f[3]  = DreamShaderFn_BL_TexNoise4D(p, w, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c[3]);

    f[4]  = DreamShaderFn_BL_TexNoiseTyped1D(uv.x, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c[4]);
    f[5]  = DreamShaderFn_BL_TexNoiseTyped2D(uv, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c[5]);
    f[6]  = DreamShaderFn_BL_TexNoiseTyped3D(p, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c[6]);
    f[7]  = DreamShaderFn_BL_TexNoiseTyped4D(p, w, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c[7]);

    f[8]  = DreamShaderFn_BL_TexWhiteNoise1D(uv.x, c[8]);
    f[9]  = DreamShaderFn_BL_TexWhiteNoise2D(uv, c[9]);
    f[10] = DreamShaderFn_BL_TexWhiteNoise3D(p, c[10]);
    f[11] = DreamShaderFn_BL_TexWhiteNoise4D(p, w, c[11]);

    f[12] = DreamShaderFn_BL_TexChecker(p, white, black, 5.0, c[12]);
    f[13] = DreamShaderFn_BL_TexGradient(p, mode, c[13]);
    f[14] = DreamShaderFn_BL_TexMagic(p, 5.0, depth, 1.0, c[14]);
    f[15] = DreamShaderFn_BL_TexWave(p, 5.0, 0.3, 2.5, 5.0, 0.5, w, mode, mode, mode, c[15]);
    f[16] = DreamShaderFn_BL_TexBrick(p, white, grey, black, 5.0, 0.02, 0.1, 0.0, 0.5, 0.25,
                                      0.5, 2.0, 1.0, 2.0, c[16]);

    float3 color = 0.0;
    float  fac   = 0.0;
    [unroll] for (int i = 0; i < 17; i++) { color += c[i]; fac += f[i]; }

    return float4(color, fac);
}

// SM5 pass 1: the heaviest fBM path. 4D pulls in perlin_4d and its sixteen hashes, which is
// everything the 1D-3D shorthands reach and then some.
float4 EP_Fbm4D(float2 uv : TEXCOORD0, float w : TEXCOORD1) : SV_Target
{
    float3 c;
    float f = DreamShaderFn_BL_TexNoise4D(float3(uv, 0.0), w, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c);
    return float4(c, f);
}

// SM5 pass 2: all five fractal types, at 3D. The type is dynamic on purpose — a literal would let
// the compiler fold the dispatcher and drop four of the five paths, which is exactly what makes a
// literal cheap in real materials and useless in a gate.
float4 EP_Typed3D_Dynamic(float2 uv : TEXCOORD0, float type : TEXCOORD1) : SV_Target
{
    float3 c;
    float f = DreamShaderFn_BL_TexNoiseTyped3D(float3(uv, 0.0), 5.0, 2.5, 0.5, 2.0,
                                               0.4, 1.2, 0.3, type, 1.0, c);
    return float4(c, f);
}

// SM5 pass 3: everything that is not the noise family. Cheap — these nodes are shallow — but they
// carry their own dropdowns, so all of those stay dynamic here.
float4 EP_NonNoise(float2 uv : TEXCOORD0, float4 knobs : TEXCOORD1) : SV_Target
{
    float mode = knobs.x;
    float w = knobs.y;
    float depth = knobs.z;

    float3 white = float3(1.0, 1.0, 1.0);
    float3 black = float3(0.0, 0.0, 0.0);
    float3 grey  = float3(0.5, 0.5, 0.5);
    float3 p     = float3(uv, 0.0);

    float3 c0, c1, c2, c3, c4, c5, c6, c7, c8;

    float f0 = DreamShaderFn_BL_TexWhiteNoise1D(uv.x, c0);
    float f1 = DreamShaderFn_BL_TexWhiteNoise2D(uv, c1);
    float f2 = DreamShaderFn_BL_TexWhiteNoise3D(p, c2);
    float f3 = DreamShaderFn_BL_TexWhiteNoise4D(p, w, c3);
    float f4 = DreamShaderFn_BL_TexChecker(p, white, black, 5.0, c4);
    float f5 = DreamShaderFn_BL_TexGradient(p, mode, c5);
    float f6 = DreamShaderFn_BL_TexMagic(p, 5.0, depth, 1.0, c6);
    float f7 = DreamShaderFn_BL_TexWave(p, 5.0, 0.3, 2.5, 5.0, 0.5, w, mode, mode, mode, c7);
    float f8 = DreamShaderFn_BL_TexBrick(p, white, grey, black, 5.0, 0.02, 0.1, 0.0, 0.5, 0.25,
                                         0.5, 2.0, 1.0, 2.0, c8);

    return float4(c0 + c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8,
                  f0 + f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8);
}

// SM5 passes 4-7: Voronoi, split by dimension because the cell window is (2r+1)^N per octave.
// Smooth F1 in 4D gets its own entry — 625 cells against F1's 81 — so a regression there is
// attributable and does not drag the other four features' compile time with it.

float4 EP_Voronoi2D(float2 uv : TEXCOORD0, float3 knobs : TEXCOORD1) : SV_Target
{
    float metric = knobs.x;
    float w = knobs.y;
    float3 p = float3(uv, 0.0);
    float3 vc1, vp1; float vw1;
    float v1 = DreamShaderFn_BL_TexVoronoiF1_2D(uv, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc1, vp1, vw1);
    float3 vc2, vp2; float vw2;
    float v2 = DreamShaderFn_BL_TexVoronoiSmoothF1_2D(uv, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0, metric, 0.5, 1.0, vc2, vp2, vw2);
    float3 vc3, vp3; float vw3;
    float v3 = DreamShaderFn_BL_TexVoronoiF2_2D(uv, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc3, vp3, vw3);
    float v4 = DreamShaderFn_BL_TexVoronoiDistToEdge_2D(uv, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0);
    float v5 = DreamShaderFn_BL_TexVoronoiRadius_2D(uv, 5.0, 1.0);
    return float4(vc1 + vp1 + vc2 + vp2 + vc3 + vp3, v1 + v2 + v3 + v4 + v5 + vw1 + vw2 + vw3);
}

float4 EP_Voronoi3D(float2 uv : TEXCOORD0, float3 knobs : TEXCOORD1) : SV_Target
{
    float metric = knobs.x;
    float w = knobs.y;
    float3 p = float3(uv, 0.0);
    float3 vc1, vp1; float vw1;
    float v1 = DreamShaderFn_BL_TexVoronoiF1_3D(p, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc1, vp1, vw1);
    float3 vc2, vp2; float vw2;
    float v2 = DreamShaderFn_BL_TexVoronoiSmoothF1_3D(p, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0, metric, 0.5, 1.0, vc2, vp2, vw2);
    float3 vc3, vp3; float vw3;
    float v3 = DreamShaderFn_BL_TexVoronoiF2_3D(p, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc3, vp3, vw3);
    float v4 = DreamShaderFn_BL_TexVoronoiDistToEdge_3D(p, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0);
    float v5 = DreamShaderFn_BL_TexVoronoiRadius_3D(p, 5.0, 1.0);
    return float4(vc1 + vp1 + vc2 + vp2 + vc3 + vp3, v1 + v2 + v3 + v4 + v5 + vw1 + vw2 + vw3);
}

float4 EP_Voronoi4D_Fast(float2 uv : TEXCOORD0, float3 knobs : TEXCOORD1) : SV_Target
{
    float metric = knobs.x;
    float w = knobs.y;
    float3 p = float3(uv, 0.0);
    float3 vc1, vp1; float vw1;
    float v1 = DreamShaderFn_BL_TexVoronoiF1_4D(p, w, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc1, vp1, vw1);
    float3 vc2, vp2; float vw2;
    float v2 = DreamShaderFn_BL_TexVoronoiF2_4D(p, w, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc2, vp2, vw2);
    float v3 = DreamShaderFn_BL_TexVoronoiDistToEdge_4D(p, w, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0);
    float v4 = DreamShaderFn_BL_TexVoronoiRadius_4D(p, w, 5.0, 1.0);
    return float4(vc1 + vp1 + vc2 + vp2, v1 + v2 + v3 + v4 + vw1 + vw2);
}

float4 EP_Voronoi4D_Smooth(float2 uv : TEXCOORD0, float3 knobs : TEXCOORD1) : SV_Target
{
    float metric = knobs.x;
    float w = knobs.y;
    float3 p = float3(uv, 0.0);
    float3 vc1, vp1; float vw1;
    float v1 = DreamShaderFn_BL_TexVoronoiSmoothF1_4D(p, w, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0, metric, 0.5, 1.0, vc1, vp1, vw1);
    return float4(vc1 + vp1, v1 + vw1);
}

float4 EP_Voronoi1D(float2 uv : TEXCOORD0, float3 knobs : TEXCOORD1) : SV_Target
{
    float metric = knobs.x;
    float w = knobs.y;
    float3 p = float3(uv, 0.0);
    float3 vc1, vp1; float vw1;
    float v1 = DreamShaderFn_BL_TexVoronoiF1_1D(uv.x, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc1, vp1, vw1);
    float3 vc2, vp2; float vw2;
    float v2 = DreamShaderFn_BL_TexVoronoiSmoothF1_1D(uv.x, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0, metric, 0.5, 1.0, vc2, vp2, vw2);
    float3 vc3, vp3; float vw3;
    float v3 = DreamShaderFn_BL_TexVoronoiF2_1D(uv.x, 5.0, 2.5, 0.5, 2.0, 1.0, metric, 0.5, 1.0, vc3, vp3, vw3);
    float v4 = DreamShaderFn_BL_TexVoronoiDistToEdge_1D(uv.x, 5.0, 2.5, 0.5, 2.0, 1.0, 1.0);
    float v5 = DreamShaderFn_BL_TexVoronoiRadius_1D(uv.x, 5.0, 1.0);
    return float4(vc1 + vp1 + vc2 + vp2 + vc3 + vp3, v1 + v2 + v3 + v4 + v5 + vw1 + vw2 + vw3);
}

// SM5 pass: Gabor. 27 cells x 8 impulses in 3D, kept as real loops rather than unrolled.
float4 EP_Gabor(float2 uv : TEXCOORD0, float3 knobs : TEXCOORD1) : SV_Target
{
    float3 p = float3(uv, 0.0);
    float3 dir = float3(knobs.x, knobs.y, 1.0);

    float ph2, in2, ph3, in3;
    float v2 = DreamShaderFn_BL_TexGabor2D(p, 5.0, 2.0, knobs.z, 0.7853982, ph2, in2);
    float v3 = DreamShaderFn_BL_TexGabor3D(p, 5.0, 2.0, knobs.z, dir, ph3, in3);

    return float4(v2, ph2 + in2, v3, ph3 + in3);
}

// SM5 pass: the Converter, Color and Vector families. Every dropdown dynamic, so all forty Math
// operations, all twenty-seven Vector Math operations and all nineteen mix modes are compiled.
float4 EP_Converter(float2 uv : TEXCOORD0, float4 knobs : TEXCOORD1) : SV_Target
{
    float mathOp = knobs.x;
    float vecOp  = knobs.y;
    float mode   = knobs.z;
    float type   = knobs.w;

    float3 p     = float3(uv, 0.0);
    float3 white = float3(1.0, 1.0, 1.0);
    float3 grey  = float3(0.5, 0.5, 0.5);

    float3 vecMathVec;
    float  vecMathVal = DreamShaderFn_BL_VectorMath(vecOp, p, grey, white, 2.0, vecMathVec);

    float m  = DreamShaderFn_BL_Math(mathOp, uv.x, uv.y, 0.25, 0.0);
    float mr = DreamShaderFn_BL_MapRange(uv.x, 0.0, 1.0, -1.0, 2.0, 4.0, mode, 1.0);
    float cl = DreamShaderFn_BL_Clamp(uv.y, 0.2, 0.8, mode);
    float bw = DreamShaderFn_BL_RGBToBW(grey);

    float3 mix  = DreamShaderFn_BL_MixColor(mode, uv.x, white, grey, 1.0, 1.0);
    float3 bc   = DreamShaderFn_BL_BrightContrast(grey, uv.x, uv.y);
    float3 gam  = DreamShaderFn_BL_Gamma(grey, 2.2);
    float3 hsv  = DreamShaderFn_BL_HueSaturation(grey, uv.x, uv.y, 1.0, 1.0);
    float3 inv  = DreamShaderFn_BL_Invert(uv.x, grey);
    float3 sep  = DreamShaderFn_BL_SeparateColor(grey, mode);
    float3 comb = DreamShaderFn_BL_CombineColor(sep, mode);

    float3 map = DreamShaderFn_BL_Mapping(type, p, grey, white, float3(2.0, 2.0, 2.0));
    float3 rot = DreamShaderFn_BL_VectorRotate(type, p, grey, white, uv.x, white, 0.0);

    float  nrmDot;
    float3 nrm = DreamShaderFn_BL_Normal(white, p, nrmDot);

    float sy, sz;
    float sx = DreamShaderFn_BL_SeparateXYZ(p, sy, sz);
    float3 cxyz = DreamShaderFn_BL_CombineXYZ(sx, sy, sz);

    float3 gen  = DreamShaderFn_BLSpace_generated(p * 100.0, float3(-50.0, -50.0, -50.0),
                                                  float3(50.0, 50.0, 50.0));
    float3 obj  = DreamShaderFn_BLSpace_point_from_unreal(p * 100.0, 100.0);
    float3 back = DreamShaderFn_BLSpace_point_to_unreal(obj, 100.0);

    float3 color = vecMathVec + mix + bc + gam + hsv + inv + comb + map + rot + nrm + cxyz
                 + gen + obj + back;
    float  fac   = vecMathVal + m + mr + cl + bw + nrmDot;
    return float4(color, fac);
}

// SM5 pass: the coefficient tables and the curve lookups. The tables index a static const array
// with a runtime index, which is the thing worth watching for compile blow-up.
float4 EP_TablesCurves(float2 uv : TEXCOORD0, float4 knobs : TEXCOORD1,
                       Texture2D lut : register(t0), SamplerState lutSamp : register(s0)) : SV_Target
{
    float3 bb = DreamShaderFn_BL_Blackbody(knobs.x * 12000.0);
    float3 wl = DreamShaderFn_BL_Wavelength(380.0 + knobs.y * 400.0);

    float rampAlpha;
    float3 ramp = DreamShaderFn_BL_ColorRamp(lut, lutSamp, uv.x, 32.0, knobs.z, rampAlpha);

    float fc = DreamShaderFn_BL_FloatCurve(lut, lutSamp, 1.0, uv.y, 0.0, 1.0, 32.0, knobs.w);
    float3 rc = DreamShaderFn_BL_RGBCurves(lut, lutSamp, 1.0, float3(uv, 0.5), 0.0, 1.0, 32.0,
                                           knobs.w);

    return float4(bb + wl + ramp + rc, rampAlpha + fc);
}
