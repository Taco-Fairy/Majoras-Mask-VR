param([ValidateSet('Configure','Native','Apk','All')][string]$Step='All',[ValidateRange(1,16)][int]$Jobs=8)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
# Private tools are available only for explicitly tagged preview builds.
$releaseInfo=Get-Content -LiteralPath (Join-Path $root 'platform/version.json') -Raw | ConvertFrom-Json
$localTestTools=if($releaseInfo.channel -eq 'preview'){'ON'}else{'OFF'}
Set-Location -LiteralPath $root
$env:TEMP=Join-Path $root 'cache/tmp';$env:TMP=$env:TEMP
$env:ANDROID_USER_HOME=Join-Path $root 'cache/android-user'
$env:GRADLE_USER_HOME=Join-Path $root 'cache/gradle'
if(!$env:JAVA_HOME){throw 'Set JAVA_HOME to your JDK 17 directory'}
if(!$env:ANDROID_HOME){throw 'Set ANDROID_HOME to your Android SDK directory'}
$ndk=Join-Path $env:ANDROID_HOME 'ndk/27.0.12077973'
if($Step -in 'Configure','All'){
 $args=@('-S','src/2ship2harkinian','-B','build/android-quest','-G','Ninja',"-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake",'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DANDROID_STL=c++_shared','-DCMAKE_BUILD_TYPE=Release','-DMMVR_ENABLE=ON',"-DMMVR_LOCAL_TEST_TOOLS=$localTestTools",'-DMMVR_STATE_NATIVE_BACKEND=ON','-DCMAKE_POLICY_VERSION_MINIMUM=3.5')
 & cmake @args
 if($LASTEXITCODE -ne 0){throw 'Quest CMake configure failed'}
}
if($Step -in 'Native','All'){
 & cmake --build build/android-quest --target 2ship --parallel $Jobs
 if($LASTEXITCODE -ne 0){throw 'Quest native build failed'}
}
if($Step -in 'Apk','All'){
 & (Join-Path $PSScriptRoot 'package-quest.ps1')
 if($LASTEXITCODE -ne 0){throw 'Quest APK packaging failed'}
}
