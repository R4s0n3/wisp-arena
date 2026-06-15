param(
  [string]$BuildDir = "build/windows",
  [string]$Configuration = "Release",
  [string]$ServerUrl = $env:WISP_SERVER_URL,
  [switch]$Installer,
  [string]$InnoSetupCompiler = $env:INNO_SETUP_COMPILER
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
$InstallerPath = Join-Path $DistRoot "wisp-arena-setup.exe"

New-Item -ItemType Directory -Force $DistRoot | Out-Null
if (Test-Path $PackageDir) {
  Remove-Item -Recurse -Force $PackageDir
}
if (Test-Path $ZipPath) {
  Remove-Item -Force $ZipPath
}
if (Test-Path $InstallerPath) {
  Remove-Item -Force $InstallerPath
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

if ($Installer) {
  if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler)) {
    $InnoSetupCompiler = (Get-Command "iscc.exe" -ErrorAction SilentlyContinue).Source
  }

  if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler)) {
    $DefaultInnoPath = Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"
    if (Test-Path $DefaultInnoPath) {
      $InnoSetupCompiler = $DefaultInnoPath
    }
  }

  if ([string]::IsNullOrWhiteSpace($InnoSetupCompiler) -or !(Test-Path $InnoSetupCompiler)) {
    throw "Could not find Inno Setup compiler. Install Inno Setup 6 or pass -InnoSetupCompiler C:\Path\To\ISCC.exe"
  }

  $InnoScript = Join-Path $Root "installer\wisp-arena.iss"
  & $InnoSetupCompiler `
    "/DSourceDir=$PackageDir" `
    "/DOutputDir=$DistRoot" `
    $InnoScript

  if (!(Test-Path $InstallerPath)) {
    throw "Installer build completed, but $InstallerPath was not created"
  }

  Write-Host "Wrote $InstallerPath"
}
