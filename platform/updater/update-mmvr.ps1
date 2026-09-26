param(
 [string]$Root=$PSScriptRoot,[string]$FeedUri='',
 [switch]$CheckOnly,[int]$WaitForPid=0,[switch]$NoRestart,[switch]$LocalTest
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Net.Http
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Root=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
function Inside([string]$path){
 $full=[IO.Path]::GetFullPath($path)
 if(!$full.StartsWith($Root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Updater path escapes installation'}
 $part=$Root
 $segments=@('')+@($full.Substring($Root.Length+1).Split([IO.Path]::DirectorySeparatorChar))
 foreach($segment in $segments){
  if($segment){$part=Join-Path $part $segment}
  if(Test-Path -LiteralPath $part){if((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Updater refuses redirected paths'}}
 }
 return $full
}
function Child([string]$relative){return Inside (Join-Path $Root $relative)}
$updates=Child 'updates';[IO.Directory]::CreateDirectory($updates)|Out-Null
$env:TEMP=Child 'updates/tmp';$env:TMP=$env:TEMP;[IO.Directory]::CreateDirectory($env:TEMP)|Out-Null
$status=Child 'updates/status.json';$mutex=$null;$client=$null;$rollback=$null;$applied=New-Object 'System.Collections.Generic.List[object]'
function AtomicCopy([string]$from,[string]$to){
 $to=Inside $to;$temp=Inside ($to+'.pending-'+[Guid]::NewGuid().ToString('N'))
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($to))|Out-Null
 [IO.File]::Copy($from,$temp,$false)
 if([IO.File]::Exists($to)){[IO.File]::Replace($temp,$to,[NullString]::Value)}else{[IO.File]::Move($temp,$to)}
}
function State([string]$state,[string]$message,[double]$progress=0){
 $json=@{state=$state;message=$message;progress=$progress;updatedUtc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Compress
 $temp=Child 'updates/status.pending';[IO.File]::WriteAllText($temp,$json,[Text.UTF8Encoding]::new($false))
 if([IO.File]::Exists($status)){[IO.File]::Replace($temp,$status,[NullString]::Value)}else{[IO.File]::Move($temp,$status)}
}
function Url([string]$text){
 $uri=[Uri]$text
 if(!$uri.IsAbsoluteUri -or $uri.UserInfo){throw 'Invalid update URL'}
 if($uri.Scheme -ne 'https' -and !($LocalTest -and $uri.Scheme -eq 'http' -and $uri.IsLoopback)){throw 'Updates require HTTPS'}
 return $uri
}
function Download([string]$url,[string]$to,[long]$expected,[long]$limit,[string]$label){
 $uri=Url $url;$response=$null
 for($redirect=0;$redirect -lt 6;$redirect++){
  $response=$client.GetAsync($uri,[Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
  $code=[int]$response.StatusCode
  if($code -ge 300 -and $code -lt 400){
   $location=$response.Headers.Location;$response.Dispose();$response=$null
   if(!$location){throw 'Missing update redirect'}
   if(!$location.IsAbsoluteUri){$location=[Uri]::new($uri,$location)}
   $uri=Url $location.AbsoluteUri
  }else{break}
 }
 if(!$response){throw 'Too many update redirects'}
 try{
  $response.EnsureSuccessStatusCode()|Out-Null;Url $response.RequestMessage.RequestUri.AbsoluteUri|Out-Null
  $length=$response.Content.Headers.ContentLength
  if($length -and ($length -gt $limit -or ($expected -gt 0 -and $length -ne $expected))){throw 'Unexpected download size'}
  $stream=$response.Content.ReadAsStreamAsync().GetAwaiter().GetResult();$out=[IO.File]::Create((Inside $to))
  try{$bytes=New-Object byte[] 65536;$total=0L;$last=[DateTime]::MinValue
   while(($n=$stream.Read($bytes,0,$bytes.Length)) -gt 0){
    $total+=$n;if($total -gt $limit -or ($expected -gt 0 -and $total -gt $expected)){throw 'Download exceeded declared size'}
    $out.Write($bytes,0,$n)
    if($expected -gt 0 -and ([DateTime]::UtcNow-$last).TotalSeconds -gt 1){State 'downloading' $label ($total/[double]$expected);$last=[DateTime]::UtcNow}
   }
   $out.Flush($true);if($expected -gt 0 -and $total -ne $expected){throw 'Download was truncated'}
  }finally{$out.Dispose();$stream.Dispose()}
 }finally{$response.Dispose()}
}
function Digest([string]$path,[string]$expected){
 if($expected -notmatch '^[a-fA-F0-9]{64}$'){throw 'Invalid update SHA-256'}
 $sha=[Security.Cryptography.SHA256]::Create();$stream=[IO.File]::OpenRead($path)
 try{$actual=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}
 if($actual -ne $expected){throw 'Update SHA-256 verification failed'}
}
function PackagePath([string]$name){
 if(!$name -or $name.Contains('\') -or $name.StartsWith('/') -or $name -match '(^|/)\.{1,2}(/|$)' -or $name -match '[:\x00-\x1f]' -or $name -match '[ .](/|$)'){throw 'Unsafe package path'}
 if($name.Contains('//') -or $name -match '(^|/)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|/|$)'){throw 'Reserved package path'}
 if($name -cnotmatch '^(2ship\.exe|2ship\.o2r|openxr_loader\.dll|gamecontrollerdb\.txt|update-mmvr\.ps1|launch-mmvr\.ps1|mmvr-runtime-probe\.exe|README\.md|update-feed\.json|version\.json|Play Majoras Mask VR\.cmd|readme\.txt|licenses/[A-Za-z0-9_./-]+|assets/[A-Za-z0-9_./-]+)$'){throw "Package may not replace user data: $name"}
 return $name
}
try{
 $mutex=[IO.File]::Open((Child 'updates/update.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 State 'checking' 'Checking for updates…'
 if(!$FeedUri -and (Test-Path -LiteralPath (Child 'update-feed.json'))){$config=Get-Content -LiteralPath (Child 'update-feed.json') -Encoding UTF8 -Raw|ConvertFrom-Json;$FeedUri=[string]$config.url}
 $current=Get-Content -LiteralPath (Child 'version.json') -Encoding UTF8 -Raw|ConvertFrom-Json
 if($current.product -ne 'mmvr' -or $current.build -lt 1){throw 'Invalid installed build identity'}
 if(!$FeedUri){$FeedUri=[string]$current.updateFeedUrl}
 if(!$FeedUri){State 'unconfigured' 'No release feed is configured.';exit 2}
 $handler=[Net.Http.HttpClientHandler]::new();$handler.AllowAutoRedirect=$false;$client=[Net.Http.HttpClient]::new($handler);$client.Timeout=[TimeSpan]::FromMinutes(15);$client.DefaultRequestHeaders.UserAgent.ParseAdd('FullDiveGames-MMVR-Updater/1')
 $manifestPath=Child 'updates/latest.json';Download $FeedUri $manifestPath 0 1048576 'Reading release information'
 $manifest=Get-Content -LiteralPath $manifestPath -Encoding UTF8 -Raw|ConvertFrom-Json
 if($manifest.schema -ne 1 -or $manifest.product -ne 'mmvr' -or $manifest.build -lt 1 -or !$manifest.version){throw 'Invalid MMVR update manifest'}
 if($manifest.build -le $current.build){State 'current' "Already up to date ($($current.version)).";exit 0}
 if(!$manifest.windows){throw 'This release does not contain a Windows build'}
 $package=$manifest.windows
 if($package.size -lt 1 -or $package.size -gt 2147483648 -or $package.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw 'Invalid Windows package metadata'}
 Url $package.url|Out-Null
 if($CheckOnly){State 'available' "Update $($manifest.version) is available. Select Install update.";exit 0}
 $transaction=[Guid]::NewGuid().ToString('N');$stage=Child "updates/staging/$transaction";[IO.Directory]::CreateDirectory($stage)|Out-Null
 $zip=Inside (Join-Path $stage 'payload.zip');Download $package.url $zip $package.size $package.size "Downloading $($manifest.version)…";Digest $zip $package.sha256
 $fileMap=New-Object 'System.Collections.Generic.Dictionary[string,object]' ([StringComparer]::OrdinalIgnoreCase)
 $sum=0L
 foreach($file in $package.files){
  $name=PackagePath ([string]$file.path)
  if($fileMap.ContainsKey($name) -or $file.size -lt 0 -or $file.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw 'Invalid package file manifest'}
  $sum+=[long]$file.size;if($sum -gt 4294967296){throw 'Package expansion exceeds limit'};$fileMap.Add($name,$file)
 }
 if(!$fileMap.ContainsKey('2ship.exe') -or !$fileMap.ContainsKey('version.json')){throw 'Package lacks executable or version identity'}
 State 'verifying' 'Verifying update files…'
 $archive=[IO.Compression.ZipFile]::OpenRead($zip);$seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
 try{foreach($entry in $archive.Entries){
   $name=PackagePath $entry.FullName
   if(!$seen.Add($name) -or !$fileMap.ContainsKey($name) -or $entry.Length -ne $fileMap[$name].size -or (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000){throw 'Unexpected or redirected ZIP entry'}
   $target=Inside (Join-Path $stage ('files/'+$name));[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null
   $entryStream=$entry.Open();$output=[IO.File]::Create($target)
   try{$chunk=New-Object byte[] 65536;$expanded=0L;while(($n=$entryStream.Read($chunk,0,$chunk.Length)) -gt 0){$expanded+=$n;if($expanded -gt $entry.Length){throw 'ZIP expansion exceeded declared size'};$output.Write($chunk,0,$n)};if($expanded -ne $entry.Length){throw 'Truncated ZIP entry'};$output.Flush($true)}finally{$output.Dispose();$entryStream.Dispose()}
   Digest $target $fileMap[$name].sha256
  }}finally{$archive.Dispose()}
 if($seen.Count -ne $fileMap.Count){throw 'Package files missing'}
 $identity=Get-Content -LiteralPath (Join-Path $stage 'files/version.json') -Encoding UTF8 -Raw|ConvertFrom-Json
 if($identity.product -ne 'mmvr' -or $identity.build -ne $manifest.build -or $identity.version -ne $manifest.version){throw 'Package build identity mismatch'}
 if($WaitForPid -gt 0){
  $game=Get-Process -Id $WaitForPid -ErrorAction SilentlyContinue
  if($game){
   if($game.Path -ne (Child '2ship.exe')){throw 'Updater process does not match this game installation'}
   State 'waiting' 'Download verified. Closing the game to install…'
   if(!$game.WaitForExit(120000)){throw 'Game did not close; no files were replaced'}
  }
 }
 if(Get-Process -Name 2ship -ErrorAction SilentlyContinue|Where-Object {$_.Path -eq (Child '2ship.exe')}){throw 'Close this game installation before updating'}
 $rollback=Child "updates/rollback/$transaction";[IO.Directory]::CreateDirectory($rollback)|Out-Null
 State 'installing' 'Installing verified update…'
 foreach($name in $fileMap.Keys){
  $target=Child $name;$backup=Inside (Join-Path $rollback $name);$existed=[IO.File]::Exists($target)
  if($existed){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup))|Out-Null;[IO.File]::Copy($target,$backup,$false)}
  $record=@{path=$name;existed=$existed};$applied.Add($record)
  $applied|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $rollback 'rollback.json') -Encoding UTF8
  AtomicCopy (Join-Path $stage ('files/'+$name)) $target
 }
 State 'installed' "Installed $($manifest.version). Saves, mods and settings were preserved."
 # Preserve runtime discovery after updating instead of bypassing the VR launcher.
 if(!$NoRestart){
  $launcher=Child 'launch-mmvr.ps1'
  if(Test-Path -LiteralPath $launcher -PathType Leaf){
   $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$launcher+'"'
   Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -WorkingDirectory $Root -WindowStyle Hidden|Out-Null
  }else{$env:MMVR_ENABLE='1';Start-Process -FilePath (Child '2ship.exe') -WorkingDirectory $Root -WindowStyle Hidden|Out-Null}
 }
 exit 0
}catch{
 $errorText=$_.Exception.Message;$rollbackErrors=@()
 for($i=$applied.Count-1;$i -ge 0;$i--){try{$item=$applied[$i];$to=Child $item.path;if($item.existed){AtomicCopy (Join-Path $rollback $item.path) $to}else{if(Test-Path -LiteralPath $to){Remove-Item -LiteralPath $to -Force}}}catch{$rollbackErrors+=$_.Exception.Message}}
 if($rollbackErrors.Count){$errorText+='; rollback needs attention: '+($rollbackErrors -join '; ')}
 if($mutex){State 'error' $errorText}
 Write-Error $errorText -ErrorAction Continue
 exit 1
}finally{if($client){$client.Dispose()};if($mutex){$mutex.Dispose()}}
