$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
$env:TEMP=Join-Path $root 'cache/tmp';$env:TMP=$env:TEMP
$env:ANDROID_USER_HOME=Join-Path $root 'cache/android-user'
$env:GRADLE_USER_HOME=Join-Path $root 'cache/gradle'
if(!$env:JAVA_HOME){throw 'Set JAVA_HOME to your JDK 17 directory'}
if(!$env:ANDROID_HOME){throw 'Set ANDROID_HOME to your Android SDK directory'}
$ndk=Join-Path $env:ANDROID_HOME 'ndk/27.0.12077973'
$stage=Join-Path $root 'build/quest-package'
$libs=Join-Path $stage 'jniLibs/arm64-v8a';$assets=Join-Path $stage 'assets'
New-Item -ItemType Directory -Path $libs,$assets -Force | Out-Null
foreach($name in @('lib2ship.so','libSDL2.so','libopenxr_loader.so','libmmvr_extract.so')){
 $matches=@(Get-ChildItem -LiteralPath (Join-Path $root 'build/android-quest') -Filter $name -File -Recurse)
 if($matches.Count -ne 1){throw "Expected exactly one native library: $name (found $($matches.Count))"}
 Copy-Item -LiteralPath $matches[0].FullName -Destination (Join-Path $libs $name) -Force
 & "$ndk/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe" --strip-unneeded (Join-Path $libs $name)
 if($LASTEXITCODE -ne 0){throw "Failed to strip packaged copy of $name"}
}
Copy-Item -LiteralPath "$ndk/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" -Destination $libs -Force
& "$ndk/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe" --strip-unneeded (Join-Path $libs 'libc++_shared.so')
if($LASTEXITCODE -ne 0){throw 'Failed to strip packaged C++ runtime'}
$supportArchive=python tools/release_support.py
if($LASTEXITCODE -ne 0){throw 'Support archive preparation failed'}
$assetFiles=@{'2ship.o2r'=[System.IO.Path]::GetRelativePath($root,$supportArchive.Trim());'gamecontrollerdb.txt'='build/android-quest/gamecontrollerdb.txt';'version.json'='platform/version.json';'update-feed.json'='platform/updater/update-feed.json'}
python tools/release_notices.py
if($LASTEXITCODE -ne 0){throw 'Dependency notice packaging failed'}
python tools/package-extractor.py
if($LASTEXITCODE -ne 0){throw 'Extractor packaging failed'}
$hashes=@{'extractor.zip'=(Get-FileHash (Join-Path $assets 'extractor.zip')).Hash.ToLowerInvariant()}
foreach($entry in $assetFiles.GetEnumerator()){
 Copy-Item -LiteralPath (Join-Path $root $entry.Value) -Destination (Join-Path $assets $entry.Key) -Force
 $hashes[$entry.Key]=(Get-FileHash -LiteralPath (Join-Path $assets $entry.Key) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $assets 'bundled-assets.json') -Encoding utf8NoBOM
$keys=Join-Path $root 'toolchains/quest-signing';New-Item -ItemType Directory -Path $keys -Force | Out-Null
$store=Join-Path $keys 'mmvr.jks';$properties=Join-Path $keys 'signing.properties'
if(!(Test-Path -LiteralPath $store)){
 if(Test-Path -LiteralPath $properties){throw 'Signing properties exist without key; recover the key instead of changing update identity'}
 $secretBytes=New-Object byte[] 32
 [System.Security.Cryptography.RandomNumberGenerator]::Fill($secretBytes)
 $env:MMVR_SIGNING_PASSWORD=[Convert]::ToBase64String($secretBytes)
 try{
  & "$env:JAVA_HOME/bin/keytool.exe" -genkeypair -keystore $store -storetype JKS -storepass:env MMVR_SIGNING_PASSWORD -keypass:env MMVR_SIGNING_PASSWORD -alias mmvr -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=FullDiveGames MMVR, O=FullDiveGames' *> (Join-Path $root 'logs/quest-signing-create.log')
  if($LASTEXITCODE -ne 0){throw 'Quest signing identity creation failed'}
  [System.IO.File]::WriteAllText($properties,"storePassword=$env:MMVR_SIGNING_PASSWORD`n",[System.Text.UTF8Encoding]::new($false))
 }finally{Remove-Item Env:MMVR_SIGNING_PASSWORD -ErrorAction SilentlyContinue}
}
if(!(Test-Path -LiteralPath $properties)){throw 'Signing properties missing; restore them to retain APK update identity'}
$gradleProperties=Join-Path $root 'platform/android/gradle/wrapper/gradle-wrapper.properties'
if(!(Select-String -LiteralPath $gradleProperties -Pattern '^distributionSha256Sum=' -Quiet)){
 $digest=(Invoke-RestMethod -Uri 'https://services.gradle.org/distributions/gradle-8.9-bin.zip.sha256').Trim()
 if($digest -notmatch '^[0-9a-f]{64}$'){throw 'Invalid official Gradle digest'}
 Add-Content -LiteralPath $gradleProperties -Value "distributionSha256Sum=$digest"
}
Push-Location -LiteralPath (Join-Path $root 'platform/android')
try{& ./gradlew.bat --no-daemon --console plain :app:assembleRelease; if($LASTEXITCODE -ne 0){throw 'APK build failed'}}finally{Pop-Location}
$out=Join-Path $root 'releases/quest-preview';New-Item -ItemType Directory -Path $out -Force | Out-Null
$apk=Join-Path $out 'MMVR-Quest-preview.apk'
Copy-Item -LiteralPath (Join-Path $root 'platform/android/app/build/outputs/apk/release/app-release.apk') -Destination $apk -Force
& "$env:ANDROID_HOME/build-tools/35.0.0/apksigner.bat" verify --verbose --print-certs $apk
if($LASTEXITCODE -ne 0){throw 'Packaged APK signature invalid'}
Get-FileHash -LiteralPath $apk -Algorithm SHA256
