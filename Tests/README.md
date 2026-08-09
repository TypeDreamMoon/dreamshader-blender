# Tests

## What exists

`compile-hlsl.ps1` — the HLSL compile gate. Finds the DreamShader-generated `.ush` for this
package, stages it with `EntryPoints.hlsl`, and compiles both with `fxc` (`ps_5_0`) and `dxc`
(`ps_6_0`) from the Windows 10 SDK. Exit `0` pass, `1` fail.

`M_BL_TexNoise_Parity.dsm` — the material the gate compiles. It calls all four entry points with
non-zero weights so none can be dead-code eliminated. Like everything else under `Packages`, a
`.dsm` here is never compiled in place: copy it into the consuming project's `DShader/` tree
(`DShader/Materials/Blender/` by convention) and build that copy.

Run the DreamShader compile first — `compile-hlsl.ps1` reads what that run generated, it does not
generate anything itself.

```bash
pwsh -File Plugins/DreamShader/.skill/dsc.ps1 compile DShader/Materials/Blender/M_BL_TexNoise_Parity.dsm -Force
```

```bash
pwsh -File DShader/Packages/@typedreammoon/dreamshader-blender/Tests/compile-hlsl.ps1
```

This gate exists because `dsc compile` runs with `-nullrhi` and never hands the generated HLSL to
a shader compiler. A green `dsc` run says the source parsed and the graph built, nothing more. The
first port passed `dsc` while emitting 51 references to undefined symbols.

When you add a node, add its entry point to `EntryPoints.hlsl` — a function the test never calls
is dead code the compiler is free to skip.

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
