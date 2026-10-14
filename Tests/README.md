# Tests

## The gate

Two materials, built with `check -Shaders`, which compiles their shaders with the real compiler:

| material | what it proves |
| :-- | :-- |
| `M_BL_Textures_Parity.dss` | every public entry point compiles inline, called with a non-zero weight so none can be dead-code eliminated, every dropdown driven from a parameter so no variant folds away |
| `M_BL_MaterialFunctions_Smoke.dss` | every material function the package generates compiles in a real material — its pins, their types and its Custom node — called through an `extern` prototype |

Like everything else under `Packages`, they are never compiled in place: copy both into the
consuming project's `DShader/Materials/Blender/` and build the copies. Compile the project first,
which builds the package's material functions; the second material needs them.

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile -All
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/Materials/Blender/M_BL_Textures_Parity.dss -Shaders -Platform SM6,SM5
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 check DShader/Materials/Blender/M_BL_MaterialFunctions_Smoke.dss -Shaders -Platform SM6,SM5
```

`M_BL_MaterialFunctions_Smoke.dss` is generated from the headers, one call per `BL_` function after a
`#pragma generate`; regenerate it when an entry point is added or changes its signature.

## Not yet ported to DreamShader 2.0

`compile-hlsl.ps1` and `parity/` below read the `.ush` include the 1.x generator wrote. DreamShader
2.0 puts a function's HLSL into the Custom node instead and writes no include, so both need a new
source for the HLSL before they run again. What follows describes them as they worked on 1.x.

`compile-hlsl.ps1` — the HLSL compile gate. Finds the DreamShader-generated `.ush` for this
package, stages it with `EntryPoints.hlsl`, and compiles it with `dxc` (`ps_6_0`) and `fxc`
(`ps_5_0`) from the Windows 10 SDK. Exit `0` pass, `1` fail. About 23 s.

The two backends are driven differently, and the asymmetry is the point:

| pass | entry point | why |
| :-- | :-- | :-- |
| `ps_6_0` | `MainPS` — all eight public entries, dynamic type | DXIL keeps real function calls, so full coverage is the cheapest option: ~9 s |
| `ps_5_0` | `EP_Fbm4D` | the heaviest fBM path; 4D Perlin subsumes 1D–3D |
| `ps_5_0` | `EP_Typed3D_Dynamic` | all five fractal types, at 3D, with the dispatcher unfolded |

DXBC has no function calls, so fxc inlines the entire call tree into one shader. The union entry
point costs minutes under fxc — `TexNoiseTyped4D` alone with a dynamic type measures 80 s — while
the two targeted entries above reach every function in the library in about 12 s. `-Thorough`
adds the union to the SM5 pass when you want it anyway.

`M_BL_Textures_Parity.dsm` — the material the gate compiles. It calls every public entry point in
the package with a non-zero weight so none can be dead-code eliminated, and drives every dropdown
from a parameter so no variant folds away. Like everything else under `Packages`, a `.dsm` here is
never compiled in place: copy it into the consuming project's `DShader/` tree
(`DShader/Materials/Blender/` by convention) and build that copy.

Run the DreamShader compile first — `compile-hlsl.ps1` reads what that run generated, it does not
generate anything itself.

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/Blender/M_BL_Textures_Parity.dsm -Force
```

```bash
pwsh -File DShader/Packages/@typedreammoon/dreamshader-blender/Tests/compile-hlsl.ps1
```

This gate exists because `dsc compile` runs with `-nullrhi` and never hands the generated HLSL to
a shader compiler. A green `dsc` run says the source parsed and the graph built, nothing more. The
first port passed `dsc` while emitting 51 references to undefined symbols.

When you add a node, call it from `MainPS` — a function nothing calls is dead code the compiler is
free to skip, so an untested entry point is an untested entry point. If the new node has a
variant axis the compiler could fold away (a type or feature dropdown), give it a dedicated SM5
entry that keeps that axis dynamic, the way `EP_Typed3D_Dynamic` does, and add it to the `$sm5`
list in `compile-hlsl.ps1`.

## What does not exist yet

**Numeric parity against Blender.** This is the test that decides whether the package is worth
anything, and it is not written. Compiling is necessary, not sufficient: every constant in
`Noise.dsh` could be subtly wrong and both compilers would still be happy.

The shape it needs to take:

1. In Blender, wire the node's output straight into an Emission shader and render to EXR with the
   view transform set to Standard, so the stored values are the node's values.
2. In Unreal, render the matching Unlit material to a linear target at the same resolution, with
   the same coordinates going in.
3. Compare per pixel and fail on a tolerance.

Two things make this harder than it sounds, and both need deciding before writing it:

- **The coordinates must agree exactly.** Blender's Generated / Object / UV spaces each need a
  documented Unreal equivalent, or the comparison measures the coordinate mismatch instead of the
  noise.
- **Reference EXRs are large.** They are gitignored; decide whether they live in a release
  attachment, an LFS store, or are regenerated from a checked-in `.blend`.

Until this lands, the parity claims in the root README are "ported carefully", not "measured".
