# DreamShader Blender

Blender's shader nodes, ported to DreamShaderLang from the Cycles kernel.

The goal is **numeric parity**, not resemblance: the same inputs should produce the same values
they produce in Blender, so a material authored against Blender's nodes can be moved to Unreal
without re-tuning it by eye.

## Install

```text
DreamShaderLang: Install Package from GitHub
https://github.com/TypeDreamMoon/dreamshader-blender
```

## Import

```c
import "@typedreammoon/dreamshader-blender/Library/Blender.dsh";   // everything
import "@typedreammoon/dreamshader-blender/Library/Noise.dsh";     // just the noise family
```

## Example

```c
Graph = {
    float2 uv = UE.TexCoord(Index=0);
    float3 p  = float3(uv.x, uv.y, 0.0);

    //             co  scale  detail  roughness  lacunarity  distortion  normalize  -> fac, color
    BL::TexNoise3D(p,  5.0,   2.0,    0.5,       2.0,        0.0,        1.0,          Fac, Color);

    EmissiveColor = Color;
}
```

A complete material is in [`Examples/M_BL_TexNoise_Preview.dsm`](Examples/M_BL_TexNoise_Preview.dsm).
Copy it out of `DShader/Packages` before building it — a `.dsm` inside a package is never compiled.

## Status

| Blender node | Entry point | State |
| :-- | :-- | :-- |
| Noise Texture, 1D–4D, fBM | `BL::TexNoise1D` … `BL::TexNoise4D` | ported |
| Noise Texture, other four fractal types | — | next |
| Voronoi, Wave, Magic, Checker, Brick, Gradient, White Noise, Gabor | — | planned |
| Converter / Color / Vector families | — | planned |
| BSDF closures | — | out of scope without Substrate; see below |

### API

Each entry point mirrors one dropdown state of the node. Inputs are in the node's own order,
outputs are Fac then Color.

```c
BL::TexNoise1D(in float  w,  in float scale, in float detail, in float roughness,
               in float lacunarity, in float distortion, in float normalize01,
               out float fac, out float3 color);
BL::TexNoise2D(in float2 co, /* …same… */);
BL::TexNoise3D(in float3 co, /* …same… */);
BL::TexNoise4D(in float3 co, in float w, /* …same… */);
```

`normalize01` is Blender's Normalize checkbox as `0.0` / `1.0` — the language has no bool-valued
function input, and a float lets it come from a scalar parameter or a static switch.

## Parity

Faithful to Cycles, deliberately, including the parts that look like noise in the source:

- The Jenkins lookup3 hash, bit for bit. Everything downstream depends on it.
- `uint_to_float_incl`'s divisor is `2^32`, not `2^32 - 1` — upstream's `(float)0xFFFFFFFFu`
  rounds up, and matching the rounding matters more than matching the literal.
- The OSL noise scale constants (`0.25`, `0.6616`, `0.9820`, `0.8344`). They were measured, not
  derived; there is nothing to re-derive.
- The `fmod(p, 100000)` wrap and the `+0.5` correction past `1000000`.
- The seed-offset scheme, including the fact that the 3D and 4D variants hash a `float2`.
- `detail` clamped to `[0, 15]`, `roughness` floored at `0`, both as the SVM node does.

Known differences you have to handle yourself:

- **Coordinate space.** Blender is metres and right-handed; Unreal is centimetres and left-handed.
  Feed the nodes whatever Blender fed them — if the Blender material used Generated coordinates,
  reproduce Generated coordinates, do not substitute world position. Nothing in this package
  converts spaces for you yet.
- **`Fac` is linear.** Bind it to an Unlit emissive for comparison; a lit material or a tonemapped
  view will not match a Blender viewport render.

Parity is not yet verified against reference renders — see [`Tests/README.md`](Tests/README.md).
Until it is, treat "faithful" above as "ported carefully", not as "measured".

## Porting rules

Rules the language and the generator impose, each learned by hitting it. Read these before adding
a node.

**A sibling `Function` call may not appear inside another sibling call's argument list.** The
generator rewrites only the *outermost* call in an expression to its mangled symbol and leaves
everything in its arguments untouched, emitting a call to an undefined `BLNoise_grad3(...)`.
Hoist every nested call into a local. Nesting inside an HLSL intrinsic or constructor — `lerp`,
`float3(...)`, `asuint(...)` — is fine; it is specifically sibling-inside-sibling that breaks.

```c
result = BLNoise_tri_mix(BLNoise_grad3(BLHash_uint3(...), ...), ...);   // broken, silently
uint  h = BLHash_uint3(...);                                            // correct
float g = BLNoise_grad3(h, ...);
result  = BLNoise_tri_mix(g, ...);
```

**Do not use HLSL reserved words as parameter names.** Blender's coordinate socket is called
Vector, but `vector` is the generic type `vector<T,N>` and fails with `unexpected token 'vector'`.
This package uses Cycles' own name, `co`.

**`mix`, `fract`, `mod`, `vec2..4`, `ivec*`, `uvec*`, `bvec*`, `mat2..4` are rewritten inside a
`Function` body**, case-insensitively, as whole identifiers. A local named `Mix` silently becomes
`lerp`. Upstream's Jenkins `mix` macro is spelled out inline in `Hash.dsh` for exactly this reason.

**The 19 math builtins shadow same-named properties inside a `Graph` block, with no diagnostic.**
`normalize` is one of them, so a material exposing Blender's Normalize checkbox must not name the
parameter `Normalize`. The examples use `NormalizeOutput`.

**Namespace only the public API.** A namespace-qualified call inside a `Function` body is *not*
rewritten — `BL::Foo` is normalized to `BL_Foo` before the rewriter sees it, and it fails to
match. Internals are therefore top-level `BLHash_*` / `BLNoise_*`; only entry points live in
`Namespace(Name="BL")`, and those are called from `Graph`, where `::` works.

## Testing

`dsc compile` proves the source parses and the graph builds. It does **not** prove the generated
HLSL compiles — the commandlet runs with `-nullrhi`. Both defects found in the first port were
invisible to it. Always run both:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/Blender/M_BL_TexNoise_Parity.dsm -Force
```

```bash
pwsh -File DShader/Packages/@typedreammoon/dreamshader-blender/Tests/compile-hlsl.ps1
```

## Scope

Roughly three quarters of Blender's ~95 shader nodes can be reproduced here. The rest cannot, and
the package will say so rather than ship an approximation:

- **BSDF closures** (Principled, Glossy, Glass, Sheen, Toon, Hair…) have no equivalent in a
  deferred GBuffer pipeline. The only real path is Substrate on UE 5.4+, where Mix Shader and Add
  Shader map onto horizontal and vertical mixing. Off Substrate, the honest ceiling is packing the
  Principled parameters into Material Attributes.
- **Path-tracing state** — Light Path, Ray Depth, AO, Bevel, Raycast — does not exist under
  rasterization.
- **Volume nodes** and the **OSL Script** node are out of scope.

## Licence

Apache-2.0. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).

Every port is taken from `intern/cycles`, whose files carry Apache-2.0 and BSD-3-Clause headers,
and **not** from `source/blender/gpu/shaders/material`, which is `GPL-2.0-or-later`. The EEVEE
GLSL was read for behaviour only. `NOTICE` records the upstream licence of each ported file; keep
it accurate as nodes are added — a Cycles kernel that embeds MIT code (Voronoi does) needs its own
entry when it lands.
