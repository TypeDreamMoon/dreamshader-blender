// Every public entry point, called once, so the HLSL compiler has to typecheck all of them.
//
// `Generated.ush` is the DreamShader-generated include, copied here by compile-hlsl.ps1 under a
// stable name — its real filename carries a per-project path hash.
//
// Distortion is deliberately non-zero: it is the only thing that pulls the distortion branch and
// the seed-offset helpers into the compilation.

#include "Generated.ush"

float4 MainPS(float2 uv : TEXCOORD0) : SV_Target
{
    float3 c1, c2, c3, c4;

    float f1 = DreamShaderFn_BL_TexNoise1D(uv.x, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c1);
    float f2 = DreamShaderFn_BL_TexNoise2D(uv, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c2);
    float f3 = DreamShaderFn_BL_TexNoise3D(float3(uv, 0.0), 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c3);
    float f4 = DreamShaderFn_BL_TexNoise4D(float3(uv, 0.0), 0.5, 5.0, 2.5, 0.5, 2.0, 0.3, 1.0, c4);

    return float4(c1 + c2 + c3 + c4, f1 + f2 + f3 + f4);
}
