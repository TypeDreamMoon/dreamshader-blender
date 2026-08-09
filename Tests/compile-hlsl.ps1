<#
.SYNOPSIS
    Compile the DreamShader-generated HLSL for this package with a real shader compiler.

.DESCRIPTION
    The DreamShader commandlet runs with -nullrhi. It parses the source, builds the graph and
    writes the .uasset, and it never hands the generated .ush to a shader compiler — so a green
    `dsc compile` says nothing about whether the HLSL is valid. Two real defects in the first
    port got through it: an undefined symbol from a nested sibling call, and a parameter named
    `vector`, which is a reserved HLSL keyword.

    This script closes that gap. Run `dsc compile` first to regenerate the include, then run this.

.PARAMETER Ush
    The generated include. When omitted, searched for under -SearchRoot.

.PARAMETER SearchRoot
    Project root holding Intermediate/DreamShader/GeneratedShaders. Defaults to the current
    directory, then walks up looking for a .uproject.

.PARAMETER Marker
    Symbol that identifies the right include when searching. Defaults to the 3D noise entry point.

.EXAMPLE
    pwsh -File Tests/compile-hlsl.ps1
    pwsh -File Tests/compile-hlsl.ps1 -SearchRoot I:/UnrealProject_Moon/DEV_58/DevTest
#>
[CmdletBinding()]
param(
    [string] $Ush,
    [string] $SearchRoot,
    [string] $Marker = 'DreamShaderFn_BL_TexNoise3D',
    [switch] $KeepTemp
)

$ErrorActionPreference = 'Stop'

function Find-ProjectRoot {
    param([string] $Start)
    $dir = Get-Item -LiteralPath $Start
    while ($null -ne $dir) {
        if (Get-ChildItem -LiteralPath $dir.FullName -Filter '*.uproject' -File -ErrorAction SilentlyContinue |
            Select-Object -First 1) {
            return $dir.FullName
        }
        $dir = $dir.Parent
    }
    return $null
}

# ---- locate the generated include ------------------------------------------------------------

if (-not $Ush) {
    if (-not $SearchRoot) { $SearchRoot = Find-ProjectRoot -Start (Get-Location).Path }
    if (-not $SearchRoot) {
        throw "No .uproject found above '$((Get-Location).Path)'. Pass -SearchRoot or -Ush."
    }

    $genDir = Join-Path $SearchRoot 'Intermediate/DreamShader/GeneratedShaders'
    if (-not (Test-Path -LiteralPath $genDir)) {
        throw "No generated shaders at '$genDir'. Run the DreamShader compile first."
    }

    $Ush = Get-ChildItem -LiteralPath $genDir -Filter '*.ush' -File |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw) -match [regex]::Escape($Marker) } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName

    if (-not $Ush) {
        throw "No generated .ush under '$genDir' defines '$Marker'. Compile a material that imports this package first."
    }
}

if (-not (Test-Path -LiteralPath $Ush)) { throw "Include not found: $Ush" }
Write-Host "include : $Ush"

# ---- locate compilers ------------------------------------------------------------------------

function Find-KitTool {
    param([string] $Name)
    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($root in $roots) {
        $hit = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "x64\$Name" } |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
        if ($hit) { return $hit }
    }
    return $null
}

$fxc = Find-KitTool 'fxc.exe'
$dxc = Find-KitTool 'dxc.exe'
if (-not $fxc -and -not $dxc) {
    throw 'Neither fxc.exe nor dxc.exe found under the Windows 10 SDK. Install the SDK, or add one to PATH.'
}

# ---- stage and compile -------------------------------------------------------------------------

$temp = Join-Path ([System.IO.Path]::GetTempPath()) ("dsb-hlsl-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temp | Out-Null

try {
    Copy-Item -LiteralPath $Ush -Destination (Join-Path $temp 'Generated.ush')
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'EntryPoints.hlsl') -Destination $temp
    $entry = Join-Path $temp 'EntryPoints.hlsl'

    $failed = $false

    if ($fxc) {
        Write-Host "fxc     : $fxc"
        & $fxc /nologo /T ps_5_0 /E MainPS $entry /Fo (Join-Path $temp 'sm5.cso')
        if ($LASTEXITCODE -ne 0) { $failed = $true; Write-Host 'ps_5_0  : FAILED' -ForegroundColor Red }
        else { Write-Host 'ps_5_0  : ok' -ForegroundColor Green }
    }

    if ($dxc) {
        Write-Host "dxc     : $dxc"
        & $dxc -nologo -T ps_6_0 -E MainPS $entry -Fo (Join-Path $temp 'sm6.cso')
        if ($LASTEXITCODE -ne 0) { $failed = $true; Write-Host 'ps_6_0  : FAILED' -ForegroundColor Red }
        else { Write-Host 'ps_6_0  : ok' -ForegroundColor Green }
    }

    if ($failed) { exit 1 }
    Write-Host 'compile-hlsl: OK' -ForegroundColor Green
    exit 0
}
finally {
    if ($KeepTemp) { Write-Host "temp kept: $temp" }
    else { Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue }
}
