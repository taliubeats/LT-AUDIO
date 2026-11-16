# Builds one Windows installer per plugin plus one with every plugin.
param([string]$Version = "0.1.0")
$ErrorActionPreference = "Stop"
$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { choco install innosetup -y --no-progress | Out-Null }
$here = $PSScriptRoot
$root = Resolve-Path "$here\..\.."
$plugins = Get-ChildItem -Path "$root\pkg\VST3" -Directory -Filter *.vst3
foreach ($p in $plugins) {
  $name = $p.Name -replace '\.vst3$', ''
  $out = ($name -replace ' ', '-') + "-Installer-windows"
  & $iscc "/DAppName=$name" "/DSourceDir=$($p.FullName)" "/DOutName=$out" "/DAppVersion=$Version" "$here\plugin.iss"
  if ($LASTEXITCODE -ne 0) { throw "ISCC failed for $name" }
}
if ($plugins.Count -ge 1) {
  & $iscc "/DAppName=LT AUDIO - All plugins" "/DSourceDir=$root\pkg\VST3" "/DOutName=LT-AUDIO-All-Plugins-Installer-windows" "/DAppVersion=$Version" "/DAll=1" "$here\plugin.iss"
  if ($LASTEXITCODE -ne 0) { throw "ISCC failed for all plugins" }
}
