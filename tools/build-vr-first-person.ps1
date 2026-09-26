param([ValidateSet('Configure','Build','Install','All')][string]$Step='All')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
# Private tools are available only for explicitly tagged preview builds.
$releaseInfo=Get-Content -LiteralPath (Join-Path $root 'platform/version.json') -Raw | ConvertFrom-Json
$localTestTools=if($releaseInfo.channel -eq 'preview'){'ON'}else{'OFF'}
$source=Join-Path $root 'src\2ship2harkinian'
$build=Join-Path $root 'build\windows-vr-first-person'
$env:TEMP=Join-Path $root 'cache\tmp'
$env:TMP=$env:TEMP
$env:VCPKG_ROOT=Join-Path $root 'toolchains\vcpkg'
$env:VCPKG_DOWNLOADS=Join-Path $root 'downloads\vcpkg'
$env:VCPKG_DEFAULT_BINARY_CACHE=Join-Path $root 'cache\vcpkg-binaries'
$env:VCPKG_DISABLE_METRICS='1'
$env:VCPKG_MAX_CONCURRENCY='8'
foreach($dir in @($env:TEMP,$env:VCPKG_DOWNLOADS,$env:VCPKG_DEFAULT_BINARY_CACHE)){New-Item -ItemType Directory -Path $dir -Force | Out-Null}
$log=Join-Path $root ('logs\phase4a-'+$Step+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
Start-Transcript -LiteralPath $log
try {
 if($Step -in @('All','Configure')) {
  cmake -S $source -B $build -G 'Visual Studio 17 2022' -T v143 -A x64 '-DCMAKE_BUILD_TYPE=Release' '-DNON_PORTABLE=OFF' '-DMMVR_ENABLE=ON' "-DMMVR_LOCAL_TEST_TOOLS=$localTestTools" '-DMMVR_STATE_NATIVE_BACKEND=ON' "-DOpenXR_DIR=$(Join-Path $root 'toolchains\vcpkg\installed\x64-windows-static\share\openxr')" "-DVCPKG_ROOT=$env:VCPKG_ROOT" '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' "-DCMAKE_INSTALL_PREFIX=$(Join-Path $root 'run\vr-first-person')"
  if($LASTEXITCODE -ne 0){throw "Configure failed: $LASTEXITCODE"}
 }
 if($Step -in @('All','Build')) {
  cmake --build $build --config Release --parallel 8
  if($LASTEXITCODE -ne 0){throw "Build failed: $LASTEXITCODE"}
  cmake --build $build --config Release --target Generate2ShipOtr --parallel 8
  if($LASTEXITCODE -ne 0){throw "Support archive failed: $LASTEXITCODE"}
 }
 if($Step -in @('All','Install')) {
  cmake --install $build --config Release --component 2s2h
  if($LASTEXITCODE -ne 0){throw "Install failed: $LASTEXITCODE"}
 }
} finally {Stop-Transcript}

