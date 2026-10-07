# Build gimp-raw-heic from scratch with MSYS2 CLANG64 (the toolchain GIMP 3.x
# for Windows is built with).  Usage:
#   .\build.ps1                 # configure + build
#   .\build.ps1 -Install        # also copy to %APPDATA%\GIMP\<ver>\plug-ins
#   .\build.ps1 -Package        # also build dist\gimp-raw-heic-<ver>.zip
#   .\build.ps1 -Setup          # install the needed MSYS2 packages first
param(
  [switch]$Setup,
  [switch]$Install,
  [switch]$Package,
  [switch]$Clean,
  [switch]$UpdatePo,
  [string]$Msys = "C:\msys64",
  [string]$GimpConfigVersion = ""
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$env:MSYSTEM = "CLANG64"
$env:PATH = "$Msys\clang64\bin;$Msys\usr\bin;" + $env:PATH

if ($Setup) {
  $pkgs = "clang","meson","ninja","pkgconf","gimp","libraw","libheif","lcms2",
          "gexiv2","gettext-tools" | ForEach-Object { "mingw-w64-clang-x86_64-$_" }
  & "$Msys\usr\bin\pacman.exe" -S --needed --noconfirm @pkgs
  if ($LASTEXITCODE) { throw "pacman failed" }
}

if ($UpdatePo) {
  Push-Location $root
  $srcs = Get-ChildItem src\*.c | ForEach-Object { "src/" + $_.Name }
  $srcs | Set-Content -Encoding ascii po\POTFILES.in
  xgettext --from-code=UTF-8 -k_ -kN_ -kngettext:1,2 --package-name=gimp-raw-heic `
    -o po/gimp-raw-heic.pot -f po/POTFILES.in
  if ($LASTEXITCODE) { Pop-Location; throw "xgettext failed" }
  python3 po/make_pl.py
  $missing = $LASTEXITCODE
  Pop-Location
  if ($missing) { Write-Warning "po/pl.po has untranslated strings (see above)" }
}

$build = Join-Path $root "build"
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }
$env:CC = "clang"; $env:CXX = "clang++"
if (-not (Test-Path (Join-Path $build "build.ninja"))) {
  meson setup $build $root
  if ($LASTEXITCODE) { throw "meson setup failed" }
}
meson compile -C $build
if ($LASTEXITCODE) { throw "build failed" }

# Stage the installable folder: build\stage\gimp-raw-heic\
$stage = Join-Path $build "stage\gimp-raw-heic"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item (Join-Path $build "gimp-raw-heic.exe") $stage
New-Item -ItemType Directory -Force (Join-Path $stage "locale") | Out-Null
foreach ($mo in Get-ChildItem (Join-Path $build "po") -Filter *.mo -ErrorAction SilentlyContinue) {
  $dir = Join-Path $stage ("locale\" + $mo.BaseName + "\LC_MESSAGES")
  New-Item -ItemType Directory -Force $dir | Out-Null
  Copy-Item $mo.FullName (Join-Path $dir "gimp-raw-heic.mo")
}
Write-Host "Staged: $stage"

if ($Install) {
  if (-not $GimpConfigVersion) {
    $GimpConfigVersion = (Get-ChildItem "$env:APPDATA\GIMP" -Directory |
      Where-Object Name -match '^3\.\d+$' | Sort-Object { [version]$_.Name } |
      Select-Object -Last 1).Name
  }
  $dest = "$env:APPDATA\GIMP\$GimpConfigVersion\plug-ins\gimp-raw-heic"
  if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
  Copy-Item -Recurse $stage $dest
  Write-Host "Installed to $dest"
}

if ($Package) {
  $ver = (Select-String -Path (Join-Path $root "meson.build") -Pattern "version : '([^']+)'").Matches[0].Groups[1].Value
  $dist = Join-Path $root "dist"
  New-Item -ItemType Directory -Force $dist | Out-Null

  # documents and licence texts next to the plug-in
  foreach ($f in "README.md","LICENSES.md","CHANGELOG.md") {
    Copy-Item (Join-Path $root $f) $stage
  }
  Copy-Item "$Msys\clang64\share\licenses\gimp\COPYING" (Join-Path $stage "COPYING")
  $lic = New-Item -ItemType Directory -Force (Join-Path $stage "licenses")
  Copy-Item "$Msys\clang64\share\licenses\libraw\LICENSE.LGPL" (Join-Path $lic "LibRaw-LGPL-2.1.txt")
  Copy-Item "$Msys\clang64\share\licenses\libraw\LICENSE.CDDL" (Join-Path $lic "LibRaw-CDDL-1.0.txt")
  Copy-Item "$Msys\clang64\share\licenses\gimp\COPYING" (Join-Path $root "COPYING") -ErrorAction SilentlyContinue

  $zip = Join-Path $dist "gimp-raw-heic-$ver-win64.zip"
  if (Test-Path $zip) { Remove-Item $zip }
  Compress-Archive -Path $stage -DestinationPath $zip
  Write-Host "Package: $zip"

  # corresponding source (GPL / LGPL relinking)
  $srcStage = Join-Path $build "stage-src\gimp-raw-heic-$ver"
  if (Test-Path $srcStage) { Remove-Item -Recurse -Force $srcStage }
  New-Item -ItemType Directory -Force $srcStage | Out-Null
  foreach ($item in "src","po","tests","meson.build","meson_options.txt","build.ps1",
                    "README.md","LICENSES.md","CHANGELOG.md","COPYING") {
    $p = Join-Path $root $item
    if (Test-Path $p) { Copy-Item -Recurse $p $srcStage }
  }
  Get-ChildItem -Recurse $srcStage -Include "__pycache__" -Directory | Remove-Item -Recurse -Force
  $szip = Join-Path $dist "gimp-raw-heic-$ver-source.zip"
  if (Test-Path $szip) { Remove-Item $szip }
  Compress-Archive -Path $srcStage -DestinationPath $szip
  Write-Host "Source:  $szip"
}
