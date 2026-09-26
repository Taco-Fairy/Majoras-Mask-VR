param(
 [ValidateSet('Auto','SteamVR','VDXR')][string]$Runtime='Auto',
 [string]$GameDirectory=$PSScriptRoot,
 [switch]$DetectOnly,
 [switch]$FunctionsOnly
)
$ErrorActionPreference='Stop'

function Select-MMVRRuntime {
 param([string]$Explicit,[string]$DefaultManifest,[array]$Candidates,[scriptblock]$Probe)
 $attempts=@()
 if($Explicit){
  $result=& $Probe $Explicit
  return [pscustomobject]@{manifest=$Explicit;reason='Explicit runtime override';available=$result.available;attempts=@($result)}
 }
 # A running SteamVR with a connected HMD is the user's active PCVR route
 # (including Steam Link/ALVR). Explicit choices above always retain priority.
 foreach($candidate in $Candidates){
  if($candidate.name -ne 'SteamVR' -or !$candidate.running -or !$candidate.manifest){continue}
  $result=& $Probe $candidate.manifest;$attempts+=@($result)
  if($result.available){return [pscustomobject]@{manifest=$candidate.manifest;reason='Running SteamVR has a headset';available=$true;attempts=$attempts}}
 }
 $result=& $Probe $null;$attempts+=@($result)
 if($result.available){return [pscustomobject]@{manifest=$null;reason='OpenXR default runtime has a headset';available=$true;attempts=$attempts}}
 foreach($candidate in $Candidates){
  if($candidate.name -eq 'SteamVR' -or !$candidate.running -or !$candidate.manifest -or $candidate.manifest -eq $DefaultManifest){continue}
  $result=& $Probe $candidate.manifest;$attempts+=@($result)
  if($result.available){return [pscustomobject]@{manifest=$candidate.manifest;reason=($candidate.name+' fallback has a headset');available=$true;attempts=$attempts}}
 }
 return [pscustomobject]@{manifest=$null;reason='No available headset found; preserving OpenXR default';available=$false;attempts=$attempts}
}
function Invoke-MMVRProbe {
 param([string]$Manifest)
 $info=New-Object System.Diagnostics.ProcessStartInfo
 $info.FileName=Join-Path $GameDirectory 'mmvr-runtime-probe.exe';$info.Arguments='--headset-only'
 $info.WorkingDirectory=$GameDirectory;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
 $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
 if($Manifest){$info.EnvironmentVariables['XR_RUNTIME_JSON']=$Manifest}else{$info.EnvironmentVariables.Remove('XR_RUNTIME_JSON')}
 $p=New-Object System.Diagnostics.Process;$p.StartInfo=$info
 try {
  $null=$p.Start();$stdout=$p.StandardOutput.ReadToEndAsync();$stderr=$p.StandardError.ReadToEndAsync()
  $timedOut=!$p.WaitForExit(8000)
  if($timedOut){$p.Kill();$p.WaitForExit()}
  return [pscustomobject]@{manifest=$Manifest;available=(!$timedOut -and $p.ExitCode -eq 0);timeout=$timedOut;exitCode=$p.ExitCode;output=($stdout.Result+$stderr.Result)}
 } catch {
  return [pscustomobject]@{manifest=$Manifest;available=$false;timeout=$false;error=$_.Exception.Message}
 } finally {$p.Dispose()}
}
if($FunctionsOnly){return}
$GameDirectory=[IO.Path]::GetFullPath($GameDirectory)
$exe=Join-Path $GameDirectory '2ship.exe'
if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'MMVR game executable is missing'}
$defaultManifest=(Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction SilentlyContinue).ActiveRuntime
$steamRoot=(Get-ItemProperty -LiteralPath 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
$steamManifest=$null
# SteamVR may be installed in a secondary Steam library. Read Steam's registered OpenVR paths first.
$vrPaths=Join-Path $env:LOCALAPPDATA 'openvr/openvrpaths.vrpath'
if(Test-Path -LiteralPath $vrPaths){
 try {foreach($path in (Get-Content -LiteralPath $vrPaths -Raw|ConvertFrom-Json).runtime){$candidate=Join-Path $path 'steamxr_win64.json';if(Test-Path -LiteralPath $candidate -PathType Leaf){$steamManifest=$candidate;break}}}catch{}
}
if(!$steamManifest -and $steamRoot){$candidate=Join-Path $steamRoot 'steamapps/common/SteamVR/steamxr_win64.json';if(Test-Path -LiteralPath $candidate -PathType Leaf){$steamManifest=$candidate}}
$vdManifest=$null
$vdProcess=@(Get-Process -Name 'VirtualDesktop.Streamer' -ErrorAction SilentlyContinue)
foreach($process in $vdProcess){if($process.Path){$candidate=Join-Path (Split-Path $process.Path -Parent) 'OpenXR/virtualdesktop-openxr.json';if(Test-Path -LiteralPath $candidate -PathType Leaf){$vdManifest=$candidate;break}}}
if(!$vdManifest){$candidate=Join-Path $env:ProgramFiles 'Virtual Desktop Streamer/OpenXR/virtualdesktop-openxr.json';if(Test-Path -LiteralPath $candidate -PathType Leaf){$vdManifest=$candidate}}
$candidates=@(
 [pscustomobject]@{name='SteamVR';running=[bool](Get-Process -Name vrserver -ErrorAction SilentlyContinue);manifest=$steamManifest},
 [pscustomobject]@{name='VDXR';running=($vdProcess.Count -gt 0);manifest=$vdManifest}
)
$explicit=$env:XR_RUNTIME_JSON
if($Runtime -ne 'Auto'){
 $explicit=if($Runtime -eq 'SteamVR'){$steamManifest}else{$vdManifest}
 if(!$explicit){throw "$Runtime runtime manifest could not be located"}
}
$selected=Select-MMVRRuntime -Explicit $explicit -DefaultManifest $defaultManifest -Candidates $candidates -Probe {param($m) Invoke-MMVRProbe $m}
$logDir=Join-Path $GameDirectory 'logs';New-Item -ItemType Directory -Path $logDir -Force|Out-Null
$record=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');requested=$Runtime;defaultManifest=$defaultManifest;steamVrRunning=$candidates[0].running;virtualDesktopRunning=$candidates[1].running;selection=$selected}
$record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $logDir ('runtime-launch-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.json')) -Encoding UTF8
Write-Host $selected.reason
foreach($attempt in $selected.attempts){if($attempt.output){Write-Host $attempt.output.Trim()}}
if($DetectOnly){return}
$oldRuntime=$env:XR_RUNTIME_JSON
try {
 if($selected.manifest){$env:XR_RUNTIME_JSON=$selected.manifest}else{Remove-Item Env:XR_RUNTIME_JSON -ErrorAction SilentlyContinue}
 $env:MMVR_ENABLE='1';$env:MMVR_CREATE_COMPLETE_SLOT3='0'
 foreach($name in @('MMVR_SMOKE_FRAMES','MMVR_NATIVE_TEST','MMVR_DEBUG_TEST','MMVR_LIFECYCLE_TEST','MMVR_TOWN_TEST','MMVR_ARENA_EXPANSION_TEST')){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
 $process=Start-Process -FilePath $exe -WorkingDirectory $GameDirectory -PassThru
 $process.WaitForExit();exit $process.ExitCode
} finally {$env:XR_RUNTIME_JSON=$oldRuntime}
