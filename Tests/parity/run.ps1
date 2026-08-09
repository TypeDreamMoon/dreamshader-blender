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
$files = @('util/hash.h', 'util/color.h', 'kernel/svm/noise.h', 'kernel/svm/fractal_noise.h')
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

# util/color.h is fetched whole but only four functions are wanted from it: the rest of the file
# is Cycles' spectral-rendering pipeline, which would drag in most of kernel/types.h for nothing.
# They are excerpted *by name* from the downloaded file rather than copied by hand, so what gets
# compiled is still upstream's own text.
$colorSrc = Get-Content -LiteralPath (Join-Path $cache 'util/color.h') -Raw
$wanted = @('rgb_to_hsv', 'hsv_to_rgb', 'rgb_to_hsl', 'hsl_to_rgb')
$excerpt = @('#pragma once', '#include "util/math.h"', 'CCL_NAMESPACE_BEGIN', '')
foreach ($fn in $wanted) {
    $start = $colorSrc.IndexOf("ccl_device float3 $fn(")
    if ($start -lt 0) { throw "util/color.h no longer defines $fn - upstream moved it." }
    $depth = 0; $seen = $false; $i = $start
    while ($i -lt $colorSrc.Length) {
        if ($colorSrc[$i] -eq '{') { $depth++; $seen = $true }
        elseif ($colorSrc[$i] -eq '}') { $depth-- }
        if ($seen -and $depth -eq 0) { break }
        $i++
    }
    $excerpt += $colorSrc.Substring($start, $i - $start + 1)
    $excerpt += ''
}
$excerpt += 'CCL_NAMESPACE_END'
Set-Content -LiteralPath (Join-Path $build 'color_subset.h') -Value ($excerpt -join "`n") -NoNewline

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
