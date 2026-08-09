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
