<#
.SYNOPSIS
    Differential parity test — the DreamShader port against upstream Cycles, compiled on the CPU.

.DESCRIPTION
    `dsc compile` proves the source parses; compile-hlsl.ps1 proves the generated HLSL compiles.
    Neither says the numbers are right. This does: it compiles the generated .ush as C++ through
    a small HLSL shim, compiles the upstream Cycles kernels verbatim beside it on a minimal
    platform shim, and compares the two function by function.

    Upstream is the oracle, not a second transcription — a wrong constant in the port shows up as
    a mismatch instead of being reproduced on both sides.

    Requires g++ on PATH (w64devkit, MSYS2, MinGW) and network access on the first run to fetch
    the upstream headers into a gitignored cache.

.PARAMETER Ref
    Blender git ref to compare against. 'main' tests against the tip, which also tells you when
    upstream has changed under the port; pass a commit SHA to pin.

.PARAMETER Ush
    The generated include. When omitted, searched for the same way compile-hlsl.ps1 searches.
#>
[CmdletBinding()]
param(
    [string] $Ref = 'main',
    [string] $Ush,
    [string] $SearchRoot,
    [switch] $KeepBuild
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot

function Find-ProjectRoot {
    param([string] $Start)
    $dir = Get-Item -LiteralPath $Start
    while ($null -ne $dir) {
        if (Get-ChildItem -LiteralPath $dir.FullName -Filter '*.uproject' -File -ErrorAction SilentlyContinue |
            Select-Object -First 1) { return $dir.FullName }
        $dir = $dir.Parent
    }
    return $null
}

# ---- locate the generated include --------------------------------------------------------------

if (-not $Ush) {
    if (-not $SearchRoot) { $SearchRoot = Find-ProjectRoot -Start (Get-Location).Path }
    if (-not $SearchRoot) { throw "No .uproject found above '$((Get-Location).Path)'. Pass -SearchRoot or -Ush." }
    $genDir = Join-Path $SearchRoot 'Intermediate/DreamShader/GeneratedShaders'
    if (-not (Test-Path -LiteralPath $genDir)) { throw "No generated shaders at '$genDir'. Run the DreamShader compile first." }
    $Ush = Get-ChildItem -LiteralPath $genDir -Filter '*.ush' -File |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw) -match 'DreamShaderFn_BLNoise_perlin_3d' } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $Ush) { throw "No generated .ush defines DreamShaderFn_BLNoise_perlin_3d." }
}
Write-Host "include  : $Ush"

$gpp = (Get-Command g++ -ErrorAction SilentlyContinue).Source
if (-not $gpp) { throw 'g++ not found on PATH. Install w64devkit / MSYS2 / MinGW, or add it to PATH.' }
Write-Host "compiler : $gpp"

# ---- fetch the upstream kernels ------------------------------------------------------------------

$cache = Join-Path $here "cache/$Ref"
$files = @('util/hash.h', 'util/color.h', 'util/projection.h',
           'kernel/svm/noise.h', 'kernel/svm/fractal_noise.h',
           'kernel/svm/voronoi.h', 'kernel/svm/gabor.h')
foreach ($f in $files) {
    $dest = Join-Path $cache $f
    if (Test-Path -LiteralPath $dest) { continue }
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    $url = "https://raw.githubusercontent.com/blender/blender/$Ref/intern/cycles/$f"
    Write-Host "fetching : $f"
    Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
}
Write-Host "upstream : blender/blender@$Ref"

# ---- stage the include as compilable C++ ----------------------------------------------------------

$build = Join-Path $here 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null

$src = Get-Content -LiteralPath $Ush -Raw

# Four mechanical rewrites. None of them touches the arithmetic — that is the whole point, the
# thing under test has to stay the text the shader compiler sees.

# `[loop]` is an HLSL attribute.
$src = $src -replace '\[loop\]\s*', ''

# `out T x` is an HLSL parameter direction; C++ spells it with a reference.
$src = $src -replace '\bout (float4|float3|float2|float|int4|int3|int2|int|uint|bool)\s+', '$1 &'

# `.xyz` is a member in HLSL and a method in the shim, because float3 has constructors and so
# cannot sit in an anonymous union.
$src = $src -replace '\.xyz\b', '.xyz()'

# HLSL floating-point literals are float. In C++ they are double, which would quietly promote
# whole expressions to double precision — `max(0.999999 - sqrt(v), 0.0)` would then be computing
# something the GPU never computes, and the comparison would be measuring the harness rather than
# the port. Suffixing keeps both sides in single precision. Integer and hex literals are left
# alone, and an already-suffixed literal is not touched twice.
$src = $src -replace '(?<![\w.])(\d+\.\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(?![\w.])', '$1f'

Set-Content -LiteralPath (Join-Path $build 'Generated.ush') -Value $src -NoNewline

# Three upstream files are wanted only in part. Everything skipped is a scene-dependent entry
# point — the SVM node functions that read the bytecode stack, and Cycles' spectral pipeline —
# none of which has any bearing on the maths under test, and all of which would drag most of
# kernel/types.h in behind it.
#
# The excerpting is mechanical and by name, never by hand: what gets compiled is still upstream's
# own text, character for character. A rename upstream throws rather than silently comparing less.

function Get-FunctionBody {
    param([string] $Source, [string] $Signature, [string] $File)
    $start = $Source.IndexOf($Signature)
    if ($start -lt 0) { throw "$File no longer contains '$Signature' - upstream moved or renamed it." }
    $depth = 0; $seen = $false; $i = $start
    while ($i -lt $Source.Length) {
        if ($Source[$i] -eq '{') { $depth++; $seen = $true }
        elseif ($Source[$i] -eq '}') { $depth-- }
        if ($seen -and $depth -eq 0) { break }
        $i++
    }
    return $Source.Substring($start, $i - $start + 1)
}

# util/color.h: four conversions; the rest of the file is the spectral pipeline.
$colorSrc = Get-Content -LiteralPath (Join-Path $cache 'util/color.h') -Raw
$excerpt = @('#pragma once', '#include "util/math.h"', 'CCL_NAMESPACE_BEGIN', '')
foreach ($fn in @('rgb_to_hsv', 'hsv_to_rgb', 'rgb_to_hsl', 'hsl_to_rgb')) {
    $excerpt += (Get-FunctionBody $colorSrc "ccl_device float3 $fn(" 'util/color.h')
    $excerpt += ''
}
$excerpt += 'CCL_NAMESPACE_END'
Set-Content -LiteralPath (Join-Path $build 'color_subset.h') -Value ($excerpt -join "`n") -NoNewline

# util/projection.h: the two helpers Gabor leans on. Taking them from upstream rather than
# supplying them from the shim is the whole point — a shim-supplied helper would be compared
# against itself and always agree.
$projSrc = Get-Content -LiteralPath (Join-Path $cache 'util/projection.h') -Raw
$excerpt = @('#pragma once', '#include "util/math.h"', 'CCL_NAMESPACE_BEGIN', '')
$excerpt += (Get-FunctionBody $projSrc 'ccl_device float3 spherical_to_direction(' 'util/projection.h')
$excerpt += ''
$excerpt += (Get-FunctionBody $projSrc 'ccl_device_inline float2 polar_to_cartesian(' 'util/projection.h')
$excerpt += ''
$excerpt += 'CCL_NAMESPACE_END'
Set-Content -LiteralPath (Join-Path $build 'projection_subset.h') -Value ($excerpt -join "`n") -NoNewline

# voronoi.h and gabor.h: everything above the first SVM node entry point. The cut is one rule —
# stop at the function that starts reading the bytecode stack — rather than a list of names to
# keep in sync.
foreach ($pair in @(
    @{ File = 'kernel/svm/voronoi.h'; Cut = 'ccl_device void svm_voronoi_output('; Out = 'voronoi_subset.h' },
    @{ File = 'kernel/svm/gabor.h';   Cut = 'ccl_device_noinline void svm_node_tex_gabor('; Out = 'gabor_subset.h' })) {

    $raw = Get-Content -LiteralPath (Join-Path $cache $pair.File) -Raw
    $cut = $raw.IndexOf($pair.Cut)
    if ($cut -lt 0) { throw "$($pair.File) no longer contains '$($pair.Cut)' - upstream moved it." }

    # Drop upstream's own includes: the platform layer underneath is the shim's job, and
    # kernel/svm/node_types.h would pull in the whole scene representation.
    $body = $raw.Substring(0, $cut) -replace '(?m)^#include .*$', ''
    $body = $body -replace '(?m)^#pragma once$', ''

    $head = @('#pragma once', '#include "util/math.h"', '#include "util/hash.h"',
              '#include "util/svm_enums.h"', '')
    Set-Content -LiteralPath (Join-Path $build $pair.Out) `
                -Value (($head -join "`n") + $body + "`nCCL_NAMESPACE_END`n") -NoNewline
}

# ---- build and run ---------------------------------------------------------------------------------
#
# -fwrapv: the PCG hashes rely on signed wraparound, and upstream marks them
#   ccl_ignore_integer_overflow. Without it the optimiser may assume it never happens.
# -ffp-contract=off: no fused multiply-add behind our back, so both sides round the same way.

$exe = Join-Path $build 'parity.exe'
$buildArgs = @(
    '-std=c++17', '-O1', '-fwrapv', '-ffp-contract=off',
    '-I', $here,
    '-I', (Join-Path $here 'shim'),
    '-I', $cache,
    '-I', $build,
    (Join-Path $here 'main.cpp'),
    '-o', $exe
)
Write-Host 'building ...'
& $gpp @buildArgs
if ($LASTEXITCODE -ne 0) { Write-Host 'parity: BUILD FAILED' -ForegroundColor Red; exit 1 }

& $exe
$code = $LASTEXITCODE

if (-not $KeepBuild) { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue }

if ($code -ne 0) { Write-Host 'parity: FAILED' -ForegroundColor Red; exit 1 }
Write-Host 'parity: OK' -ForegroundColor Green
exit 0
