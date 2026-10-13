param([Parameter(Mandatory)][string]$CookieFile, [string]$Serial)
$ErrorActionPreference='Stop'
$taskCookie=Get-Item -LiteralPath $CookieFile
if ($taskCookie.PSIsContainer -or $taskCookie.Length -gt 256*1024) { throw 'Expected a Netscape cookie export no larger than 256 KiB' }
$taskHeader=Get-Content -LiteralPath $taskCookie.FullName -TotalCount 1
if ($taskHeader -notmatch '^# (Netscape HTTP Cookie File|HTTP Cookie File)') { throw 'Expected a Netscape cookie export' }
$taskAdb=(Get-Command adb -ErrorAction SilentlyContinue).Source
if (-not $taskAdb) { $taskAdb='C:\Program Files\platform-tools\adb.exe' }
if (-not (Test-Path -LiteralPath $taskAdb)) { throw 'Android platform-tools / adb required' }
if (-not $Serial) {
    $taskDevices=@(& $taskAdb devices | ForEach-Object { if ($_ -match '^(\S+)\s+device$') {$Matches[1]} })
    if ($taskDevices.Count -ne 1) { throw 'Connect and authorize one Quest, or specify -Serial' }
    $Serial=$taskDevices[0]
}
$taskRemoteDir='/sdcard/ModData/com.beatgames.beatsaber/Mods/bstheater'
$taskRemote="$taskRemoteDir/youtube-cookies.txt"
& $taskAdb -s $Serial shell mkdir -p $taskRemoteDir
if ($LASTEXITCODE -ne 0) { throw 'Cannot create Theater data directory' }
& $taskAdb -s $Serial push $taskCookie.FullName "$taskRemote.part"
if ($LASTEXITCODE -ne 0) { throw 'Cookie export transfer failed' }
$taskExpected=(Get-FileHash -LiteralPath $taskCookie.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$taskReceived=& $taskAdb -s $Serial shell sha256sum "$taskRemote.part"
if ($LASTEXITCODE -ne 0 -or $taskReceived -notmatch "^$taskExpected\s") { throw 'Transferred cookie export hash mismatch' }
& $taskAdb -s $Serial shell mv "$taskRemote.part" $taskRemote
if ($LASTEXITCODE -ne 0) { throw 'Cannot publish cookie export' }
Write-Output 'Cookie export transferred. In Theater select a configured map and press Download Video / Retry.'
