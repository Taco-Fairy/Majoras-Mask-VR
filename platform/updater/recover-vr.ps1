$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$game=Join-Path $root '2ship.exe'
if(Get-Process 2ship -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $game}) {throw 'Close Majora''s Mask VR first.'}
[IO.File]::WriteAllText((Join-Path $root 'reset-vr-settings.request'),'Reset registered VR preferences on next launch.')
# Process-local overrides only: do not change the user/system OpenXR registration.
Remove-Item Env:XR_RUNTIME_JSON -ErrorAction SilentlyContinue
& (Join-Path $root 'launch-mmvr.ps1') -Runtime Auto -GameDirectory $root
