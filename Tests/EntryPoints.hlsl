// Entry points for the HLSL compile gate.
//
// `Generated.ush` is the DreamShader-generated include, copied here by compile-hlsl.ps1 under a
// stable name — its real filename carries a per-project path hash.
//
// Three entry points, compiled separately. fxc only walks the graph reachable from the entry it
// is given, so splitting them is what keeps the SM5 pass affordable: the union costs minutes
// under fxc's inliner, while these two targeted entries cover the same code in seconds. See
// README.md for the measurements.
//
// Distortion is non-zero throughout, so the distortion branch and the seed-offset helpers are
// compiled rather than folded away.

#include "Generated.ush"

// Full coverage: every public entry point, dynamic type. Cheap under dxc, pathological under fxc.
float4 MainPS(float2 uv : TEXCOORD0, float2 knobs : TEXCOORD1) : SV_Target
{
    float type = knobs.x;
    float w    = knobs.y;

    float3 c1, c2, c3, c4, c5, c6, c7, c8;

    float f1 = DreamShaderFn_BL_TexNoise1D(uv.x, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c1);
    float f2 = DreamShaderFn_BL_TexNoise2D(uv, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c2);
    float f3 = DreamShaderFn_BL_TexNoise3D(float3(uv, 0.0), 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c3);
    float f4 = DreamShaderFn_BL_TexNoise4D(float3(uv, 0.0), w, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c4);

    float f5 = DreamShaderFn_BL_TexNoiseTyped1D(uv.x, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c5);
    float f6 = DreamShaderFn_BL_TexNoiseTyped2D(uv, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c6);
    float f7 = DreamShaderFn_BL_TexNoiseTyped3D(float3(uv, 0.0), 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c7);
    float f8 = DreamShaderFn_BL_TexNoiseTyped4D(float3(uv, 0.0), w, 5.0, 2.5, 0.5, 2.0, 0.4, 1.2, 0.3, type, 1.0, c8);

    float3 color = c1 + c2 + c3 + c4 + c5 + c6 + c7 + c8;
    float  fac   = f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8;
    return float4(color, fac);
}

// SM5 pass, part 1: the heaviest fBM path. 4D pulls in perlin_4d and its sixteen hashes, which is
// everything the 1D-3D shorthands reach and then some.
float4 EP_Fbm4D(float2 uv : TEXCOORD0, float w : TEXCOORD1) : SV_Target
{
    float3 c;
    float f = DreamShaderFn_BL_TexNoise4D(float3(uv, 0.0), w, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c);
    return float4(c, f);
}

// SM5 pass, part 2: all five fractal types, at 3D. The type is dynamic on purpose — a literal
// would let the compiler fold the dispatcher and drop four of the five paths, which is exactly
// what makes a literal cheap in real materials and useless in a gate.
float4 EP_Typed3D_Dynamic(float2 uv : TEXCOORD0, float type : TEXCOORD1) : SV_Target
{
    float3 c;
    float f = DreamShaderFn_BL_TexNoiseTyped3D(float3(uv, 0.0), 5.0, 2.5, 0.5, 2.0,
                                               0.4, 1.2, 0.3, type, 1.0, c);
    return float4(c, f);
}
