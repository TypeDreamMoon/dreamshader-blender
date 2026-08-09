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

| Blender node | Entry point | Header | State |
| :-- | :-- | :-- | :-- |
| Noise Texture, 1D–4D, all five fractal types | `BL::TexNoise*D`, `BL::TexNoiseTyped*D` | `Noise.dsh` | ported |
| White Noise Texture, 1D–4D | `BL::TexWhiteNoise*D` | `WhiteNoise.dsh` | ported |
| Checker Texture | `BL::TexChecker` | `Checker.dsh` | ported |
| Gradient Texture, all 7 types | `BL::TexGradient` | `Gradient.dsh` | ported |
| Magic Texture | `BL::TexMagic` | `Magic.dsh` | ported |
| Wave Texture, bands/rings × 4 directions × 3 profiles | `BL::TexWave` | `Wave.dsh` | ported |
| Brick Texture | `BL::TexBrick` | `Brick.dsh` | ported |
| Voronoi Texture, all 5 features × 4 dimensions × 4 metrics | `BL::TexVoronoi*_*D` | `Voronoi.dsh` | ported |
| Gabor Texture, 2D and 3D | `BL::TexGabor2D`, `BL::TexGabor3D` | `Gabor.dsh` | ported |
| Math (all 41 operations), Vector Math (all 30), Map Range, Clamp | `BL::Math`, `BL::VectorMath`, `BL::MapRange`, `BL::Clamp` | `Math.dsh` | ported |
| Mix Color (19 modes), Bright/Contrast, Gamma, Hue/Sat/Value, Invert, Separate/Combine Color, RGB to BW | `BL::MixColor`, `BL::Gamma`, … | `Color.dsh` | ported |
| Mapping (4 types), Vector Rotate (5), Normal, Separate/Combine XYZ | `BL::Mapping`, `BL::VectorRotate`, … | `Vector.dsh` | ported |
| Texture Coordinate — Generated, Object, UV, Normal | `BL::CoordGenerated`, … | `Space.dsh` | ported |
| Blackbody, Wavelength | — | — | need their CIE / blackbody coefficient tables |
| Float Curve, RGB Curves, ColorRamp, Vector Curves | — | — | carry baked widget data; need a CurveAtlas design |
| Texture Coordinate — Camera, Window, Reflection | — | — | need view matrices whose handedness differs; deliberately absent |
| BSDF closures | — | — | out of scope without Substrate; see below |

Import a single header when you only need one family — the closure is inlined as text, so a
narrow import means a smaller generated include and a shorter shader compile.

### API

Two entry points per dimension. Inputs follow the node's own socket order; outputs are Fac then
Color.

```c
// fBM — Blender's default type, and the common case.
BL::TexNoise1D(in float  w,  in float scale, in float detail, in float roughness,
               in float lacunarity, in float distortion, in float normalize01,
               out float fac, out float3 color);
BL::TexNoise2D(in float2 co, /* …same… */);
BL::TexNoise3D(in float3 co, /* …same… */);
BL::TexNoise4D(in float3 co, in float w, /* …same… */);

// The full node, including the Type dropdown and the Offset / Gain it enables.
BL::TexNoiseTyped3D(in float3 co, in float scale, in float detail, in float roughness,
                    in float lacunarity, in float offset, in float gain, in float distortion,
                    in float type, in float normalize01,
                    out float fac, out float3 color);
```

`type` is this package's own encoding, ordered to put Blender's default at zero:

| 0 | 1 | 2 | 3 | 4 |
| :-- | :-- | :-- | :-- | :-- |
| fBM | Multifractal | Hybrid Multifractal | Ridged Multifractal | Hetero Terrain |

`normalize01` is Blender's Normalize checkbox as `0.0` / `1.0` — the language has no bool-valued
function input, and a float lets it come from a scalar parameter or a static switch. Blender
applies Normalize to fBM only, and so does this package.

`offset` is used by Hetero Terrain, Hybrid and Ridged; `gain` by Hybrid and Ridged. The fBM
shorthand does not expose either, because fBM ignores both.

Voronoi is one entry point per Feature per dimension — twenty in all — rather than one entry with
a Feature selector:

```c
BL::TexVoronoiF1_3D(in float3 co, in float scale, in float detail, in float roughness,
                    in float lacunarity, in float randomness, in float metric, in float exponent,
                    in float normalize01,
                    out float outDistance, out float3 outColor, out float3 outPosition,
                    out float outW);
BL::TexVoronoiSmoothF1_3D(…, in float smoothness, …);   // smoothness after lacunarity
BL::TexVoronoiF2_3D(…);                                  // same as F1
BL::TexVoronoiDistToEdge_3D(co, scale, detail, roughness, lacunarity, randomness, normalize01,
                            out outDistance);            // no metric
BL::TexVoronoiRadius_3D(co, scale, randomness, out outRadius);   // not fractalised
```

`metric`: 0 Euclidean, 1 Manhattan, 2 Chebychev, 3 Minkowski — ignored by Distance To Edge,
N-Sphere Radius and every 1D entry, exactly as in Blender. There is no runtime Feature selector on
purpose: Smooth F1 scans a 5^N cell window against F1's 3^N — 625 cells per octave in 4D — so one
dispatching entry point would compile all of that into every material that uses any feature. Use a
`StaticSwitchParameter` if you need to choose in a material instance.

### Shader compile cost

**Pass `type` as a literal, or from a static switch — not as a `ScalarParameter` — unless you
genuinely need to animate it.** A constant type lets the translator fold the dispatcher and drop
four of the five fractal paths, which costs the same as calling the fBM shorthand directly. A
dynamic type compiles all five. Measured with `fxc /T ps_5_0` over this library:

| call | ps_5_0 |
| :-- | --: |
| `TexNoise3D` (fBM shorthand) | 0.6 s |
| `TexNoiseTyped3D`, literal type | 0.6 s |
| `TexNoiseTyped3D`, dynamic type | 9.1 s |
| `TexNoise4D` (fBM shorthand) | 3.8 s |
| `TexNoiseTyped4D`, dynamic type | 80.5 s |

The cliff is SM5-specific: DXBC has no function calls, so fxc inlines the whole call tree, and 4D
Perlin is sixteen hashes per octave before the dispatcher multiplies it by five. `dxc -T ps_6_0`
compiles every entry point above, together, in about ten seconds.

Voronoi and Gabor avoid the cliff entirely — 0.1–0.4 s each on both compilers — because their cell
windows carry `[loop]`. The bounds are compile-time constants, so fxc would otherwise unroll 625
cells (Smooth F1 in 4D) or 216 kernel evaluations (Gabor 3D), each copy dragging its own hash
along. The attribute changes no arithmetic.

## Parity

Faithful to Cycles, deliberately, including the parts that look like noise in the source:

- The Jenkins lookup3 hash, bit for bit. Everything downstream depends on it.
- fBM accumulates a separate `fscale`; the other four multiply the coordinate in place. Those are
  not interchangeable — repeated multiplication and one scaled multiply round differently — so
  each is written the way upstream writes it.
- Ridged Multifractal has no fractional-detail tail upstream: it ignores the remainder that the
  other four blend in. Kept.
- Hybrid Multifractal's `weight > 0.001` test is part of the loop condition, not a late-out, and
  the fractional tail is gated on the same test.
- Hetero Terrain and Ridged start their loop at `i = 1`, having already taken one unscaled octave.
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

### Measured

The noise family is verified **bit-exact against upstream Cycles**, not by eye and not against a
render. `Tests/parity` compiles the generated `.ush` as C++ through a small HLSL shim, compiles
`util/hash.h`, `kernel/svm/noise.h` and `kernel/svm/fractal_noise.h` from the Blender repository
verbatim beside it, and compares them function by function over 20 000 inputs each — small, large,
negative, and deliberately sitting on cell boundaries where a sign or rounding mistake would hide.

| covered | worst difference |
| :-- | :-- |
| `hash_uint` 1–4, `hash_float*_to_float`, PCG `hash_int*_to_float*` | 0 ulp |
| `perlin_1d` … `perlin_4d`, `snoise_1d` … `snoise_4d` | 0 ulp |
| `fbm_1d` … `fbm_4d`, normalized and not | 0 ulp |
| Multifractal, Hetero Terrain, Hybrid, Ridged | 0 ulp |
| `rgb_to_hsv`, `hsv_to_rgb`, `rgb_to_hsl`, `hsl_to_rgb` | 0 ulp |

Upstream is the oracle rather than a second transcription, so a wrong constant surfaces as a
mismatch instead of being reproduced identically on both sides. The harness is mutation-tested:
changing `0.9820` to `0.9821` in one function fails exactly the seven checks that depend on 3D
snoise and leaves 1D, 2D, 4D and the hashes green.

**Not yet covered:** Voronoi, Gabor, the Math and Vector Math operation tables, and the node-level
entry points (`BL::Tex*`). Extending the harness to those needs more of the Cycles platform layer
shimmed — the `VoronoiParams` struct, the operation enums, `spherical_to_direction`. The hashes and
colour conversions underneath them *are* verified, which is where the arbitrary constants live.
See [`Tests/README.md`](Tests/README.md).

### Coordinate space

**This is the part that decides whether any of the rest means anything.** Every texture here is a
function of a coordinate; feed it Unreal's coordinate and you get Blender's function of the wrong
input — which looks plausible and is wrong everywhere.

[`Space.dsh`](Library/Space.dsh) implements the conversion and writes out the derivation. The short
version: the standard Blender-to-Unreal export negates Y and scales by 100, so
`bl = float3(ue.x, -ue.y, ue.z) / unitScale`, with `unitScale` a parameter rather than a constant
because a project authored at a different scale would otherwise get silently wrong frequencies.

The Y negation has a consequence worth knowing before you debug it: **Generated's Y is one minus
Unreal's normalised Y**, because negating an axis swaps which end of the bounding box is the
minimum. Normalise-then-convert and convert-then-normalise are not the same operation. UV flips
too, for the unrelated reason that Blender's V origin is bottom-left.

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
HLSL compiles — the commandlet runs with `-nullrhi`. Every defect found so far was invisible to
it. Always run both; the pair takes well under a minute:

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/Blender/M_BL_Textures_Parity.dsm -Force
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
