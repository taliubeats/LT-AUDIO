# Builds dist\LT-AUDIO-Plugins-Setup-windows.exe: one installer with a checkbox for every plugin in pkg\VST3.
param([string]$Version = "0.1.0")
$ErrorActionPreference = "Stop"
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { choco install innosetup -y --no-progress | Out-Null }
$here = $PSScriptRoot
$root = Resolve-Path "$here\..\.."
$plugins = Get-ChildItem -Path "$root\pkg\VST3" -Directory -Filter *.vst3 | Sort-Object Name
if ($plugins.Count -lt 1) { throw "No .vst3 in pkg\VST3" }

$comp = @("[Components]")
$files = @("[Files]")
$del = @("[InstallDelete]")
foreach ($p in $plugins) {
  $name = $p.Name -replace '\.vst3$', ''                       # "LT AIR"
  $id = "p_" + (($name -replace '^LT ', '') -replace '[^A-Za-z0-9]', '_').ToLower()
  $mb = [math]::Ceiling(((Get-ChildItem $p.FullName -Recurse -File | Measure-Object Length -Sum).Sum) / 1MB)
  $comp  += "Name: `"$id`"; Description: `"$name`"; Types: full custom"
  $files += "Source: `"$($p.FullName)\*`"; DestDir: `"{commoncf64}\VST3\$($p.Name)`"; Components: $id; Flags: ignoreversion recursesubdirs createallsubdirs"
  $del   += "Type: filesandordirs; Name: `"{commoncf64}\VST3\$($p.Name)`"; Components: not $id; Check: IsUpgrade"
  Write-Host "  $name ($mb MB)"
}
($comp + "" + $files + "" + $del) | Set-Content -Encoding UTF8 "$here\suite_components.iss"

& $iscc "/DAppVersion=$Version" "$here\suite.iss"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }
