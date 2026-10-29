# Builds the Windows tester package from an existing Release build:
#   dist\Cortex-<version>-Windows.zip
# containing both plugins (VST3 + Standalone), the tester docs, the licence
# files and a source archive of the tagged version (AGPL-3.0).
#
# Usage (from the repo root, after building Release at the tag):
#   powershell -ExecutionPolicy Bypass -File packaging\make-package.ps1

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Version and commit come from the build, so the package matches what was built.
$versionHeader = Get-Content (Join-Path $root 'build\generated\Version.h') -Raw
$version = [regex]::Match($versionHeader, 'CORTEX_VERSION_STRING "([^"]+)"').Groups[1].Value
$hash = [regex]::Match($versionHeader, 'CORTEX_GIT_HASH "([^"]+)"').Groups[1].Value
if (-not $version -or -not $hash) { throw 'Could not read the version from build\generated\Version.h. Build Release first.' }

$tag = "v$version"
$tagHash = (git rev-parse --short "$tag^{commit}" 2>$null)
if ($LASTEXITCODE -ne 0) { throw "Tag $tag does not exist." }
if ($tagHash -ne $hash) { throw "The build is commit $hash but $tag is $tagHash. Check out $tag and rebuild." }

$name = "Cortex-$version-Windows"
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage 'VST3'), (Join-Path $stage 'Standalone'), (Join-Path $stage 'Source') | Out-Null

# Plugins.
$artefacts = @{ 'Cortex' = 'build\Cortex_artefacts\Release'; 'Cortex Instrument' = 'build\CortexInstrument_artefacts\Release' }
foreach ($plugin in $artefacts.Keys) {
    $dir = Join-Path $root $artefacts[$plugin]
    Copy-Item -Recurse (Join-Path $dir "VST3\$plugin.vst3") (Join-Path $stage 'VST3')
    Copy-Item (Join-Path $dir "Standalone\$plugin.exe") (Join-Path $stage 'Standalone')
}

# Tester docs, with the version filled in and Windows line endings.
foreach ($doc in Get-ChildItem (Join-Path $PSScriptRoot 'docs') -Filter *.txt) {
    $text = (Get-Content $doc.FullName -Raw) -replace '\{\{VERSION\}\}', $version -replace '\{\{HASH\}\}', $hash
    $text = $text -replace "`r?`n", "`r`n"
    [IO.File]::WriteAllText((Join-Path $stage $doc.Name), $text, (New-Object Text.UTF8Encoding $false))
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
try { & (Join-Path $env:SystemRoot 'System32\tar.exe') -a -c -f "$name.zip" $name } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw 'tar failed to create the zip.' }
Write-Output "Package: $zip"
