param(
  [string]$BuildDir = "build/windows",
  [string]$Configuration = "Release",
  [string]$ServerUrl = $env:WISP_SERVER_URL
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildPath = Join-Path $Root $BuildDir
$ReleasePath = Join-Path $BuildPath $Configuration
$Exe = Join-Path $ReleasePath "wisp-arena.exe"

if (!(Test-Path $Exe)) {
  $Exe = Join-Path $BuildPath "wisp-arena.exe"
}

if (!(Test-Path $Exe)) {
  throw "Could not find wisp-arena.exe. Build first with: cmake --build --preset windows-release"
}

if ([string]::IsNullOrWhiteSpace($ServerUrl)) {
  $ServerUrl = "ws://localhost:9001"
}

$DistRoot = Join-Path $Root "dist"
$PackageDir = Join-Path $DistRoot "wisp-arena-windows"
$ZipPath = Join-Path $DistRoot "wisp-arena-windows.zip"

New-Item -ItemType Directory -Force $DistRoot | Out-Null
if (Test-Path $PackageDir) {
  Remove-Item -Recurse -Force $PackageDir
}
if (Test-Path $ZipPath) {
  Remove-Item -Force $ZipPath
}
New-Item -ItemType Directory -Force $PackageDir | Out-Null

Copy-Item $Exe $PackageDir
Get-ChildItem (Split-Path $Exe) -Filter "*.dll" | Copy-Item -Destination $PackageDir

Set-Content -Path (Join-Path $PackageDir "server-url.txt") -Value $ServerUrl -Encoding ascii
Set-Content -Path (Join-Path $PackageDir "README.txt") -Encoding ascii -Value @"
Wisp Arena

Run wisp-arena.exe to start the client.

The client connects to the websocket URL in server-url.txt.
Edit server-url.txt if the server address changes.
"@

Compress-Archive -Path (Join-Path $PackageDir "*") -DestinationPath $ZipPath

Write-Host "Wrote $ZipPath"
