# Builds a tester package from an existing Release build:
#   dist\Cortex-<version>-Windows.zip   from this machine's build\ folder
#   dist\Cortex-<version>-macOS.zip     from a CI artefact folder (-MacArtefacts)
# Each holds both plugins (VST3 + Standalone, plus AU on macOS), the tester
# docs for that platform, the licence files and a source archive of the
# tagged version (AGPL-3.0).
#
# Usage (from the repo root, after building Release at the tag):
#   powershell -ExecutionPolicy Bypass -File packaging\make-package.ps1
#   powershell -ExecutionPolicy Bypass -File packaging\make-package.ps1 -Platform macOS -MacArtefacts dist\mac-ci\Cortex-macOS
#
# The macOS artefact folder is the "Cortex-macOS" download from the build
# workflow: it holds Cortex_artefacts\Release\{VST3,AU,Standalone} and the
# same for CortexInstrument_artefacts. GitHub's artefact zips carry no Unix
# permissions, so the .app executables arrive without their execute bit;
# INSTALL-macOS.txt has the chmod line that restores it. The plug-in bundles
# are loaded with dlopen and do not need it.
param(
    [ValidateSet('Windows', 'macOS')] [string] $Platform = 'Windows',
    [string] $MacArtefacts
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$tar = Join-Path $env:SystemRoot 'System32\tar.exe'

# Version and commit come from the build, so the package matches what was built.
$versionHeader = Get-Content (Join-Path $root 'build\generated\Version.h') -Raw
$version = [regex]::Match($versionHeader, 'CORTEX_VERSION_STRING "([^"]+)"').Groups[1].Value
$hash = [regex]::Match($versionHeader, 'CORTEX_GIT_HASH "([^"]+)"').Groups[1].Value
if (-not $version -or -not $hash) { throw 'Could not read the version from build\generated\Version.h. Build Release first.' }

$tag = "v$version"
$tagHash = (git rev-parse --short "$tag^{commit}" 2>$null)
if ($LASTEXITCODE -ne 0) { throw "Tag $tag does not exist." }
if ($tagHash -ne $hash) { throw "The build is commit $hash but $tag is $tagHash. Check out $tag and rebuild." }

$name = "Cortex-$version-$Platform"
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
$formats = if ($Platform -eq 'macOS') { 'VST3', 'AU', 'Standalone' } else { 'VST3', 'Standalone' }
New-Item -ItemType Directory -Force (($formats | ForEach-Object { Join-Path $stage $_ }) + (Join-Path $stage 'Source')) | Out-Null

# Plugins.
$plugins = @{ 'Cortex' = 'Cortex_artefacts'; 'Cortex Instrument' = 'CortexInstrument_artefacts' }
if ($Platform -eq 'Windows') {
    foreach ($plugin in $plugins.Keys) {
        $dir = Join-Path $root "build\$($plugins[$plugin])\Release"
        Copy-Item -Recurse (Join-Path $dir "VST3\$plugin.vst3") (Join-Path $stage 'VST3')
        Copy-Item (Join-Path $dir "Standalone\$plugin.exe") (Join-Path $stage 'Standalone')
    }
} else {
    if (-not $MacArtefacts -or -not (Test-Path $MacArtefacts)) { throw 'Pass -MacArtefacts <folder>: the unzipped Cortex-macOS artefact from CI.' }
    $MacArtefacts = (Resolve-Path $MacArtefacts).Path
    $bundles = @{ 'VST3' = '.vst3'; 'AU' = '.component'; 'Standalone' = '.app' }
    foreach ($plugin in $plugins.Keys) {
        foreach ($format in $bundles.Keys) {
            $bundle = Join-Path $MacArtefacts "$($plugins[$plugin])\Release\$format\$plugin$($bundles[$format])"
            if (-not (Test-Path $bundle)) { throw "Missing in the macOS artefact: $bundle" }
            Copy-Item -Recurse $bundle (Join-Path $stage $format)
        }
    }
}

# Tester docs, with the version filled in. Files named *-Windows.txt or
# *-macOS.txt are for one platform and lose the suffix; the rest are shared.
foreach ($doc in Get-ChildItem (Join-Path $PSScriptRoot 'docs') -Filter *.txt) {
    $base = $doc.BaseName
    if ($base -match '-(Windows|macOS)$') {
        if ($Matches[1] -ne $Platform) { continue }
        $base = $base -replace '-(Windows|macOS)$', ''
    }
    $text = (Get-Content $doc.FullName -Raw) -replace '\{\{VERSION\}\}', $version -replace '\{\{HASH\}\}', $hash
    $newline = if ($Platform -eq 'Windows') { "`r`n" } else { "`n" }
    $text = $text -replace "`r?`n", $newline
    [IO.File]::WriteAllText((Join-Path $stage "$base.txt"), $text, (New-Object Text.UTF8Encoding $false))
}

# Licence and project notes.
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $root 'CREDITS.md'), (Join-Path $root 'CHANGELOG.md') $stage
# The embedded fonts' licences (SIL OFL asks for them to travel with the fonts).
Copy-Item (Join-Path $root 'Resources\fonts\Inter-OFL.txt'), (Join-Path $root 'Resources\fonts\Barlow-OFL.txt') $stage

# Source of exactly this version (AGPL-3.0: recipients of the binaries get the source).
git archive --format=zip --prefix="cortex-$version/" -o (Join-Path $stage "Source\cortex-$version-source.zip") $tag
if ($LASTEXITCODE -ne 0) { throw 'git archive failed.' }

# Windows' built-in tar (bsdtar) writes standard zip paths; Compress-Archive in
# Windows PowerShell 5.1 writes backslashes, which other unzippers mangle.
$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Push-Location $dist
try { & $tar -a -c -f "$name.zip" $name } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'tar failed to create the zip.' }
Write-Output "Package: $zip"
