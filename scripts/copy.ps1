param([Parameter(Mandatory)][string]$Serial)
$ErrorActionPreference='Stop'
$taskProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskBinary=Join-Path $taskProject 'build\libbstheater.so'
$taskSolver=Join-Path $taskProject 'build\libbstheaterjs.so'
if (-not (Test-Path $taskBinary) -or -not (Test-Path $taskSolver)) { throw 'Build both mod libraries first' }
$taskVersion=(& adb -s $Serial shell dumpsys package com.beatgames.beatsaber | Select-String 'versionName=').ToString()
if ($taskVersion -notmatch 'versionName=1\.40\.8(?:_|\s|$)') { throw "Wrong Beat Saber version: $taskVersion" }
$taskRemote='/sdcard/ModData/com.beatgames.beatsaber/Modloader/early_mods/libbstheater.so'
$taskSolverRemote='/sdcard/ModData/com.beatgames.beatsaber/Modloader/libs/libbstheaterjs.so'
& adb -s $Serial shell test -f $taskRemote
if ($LASTEXITCODE -ne 0) { throw 'Install the QMOD with MBF first; this script only updates an existing dev installation' }
$taskBackup=Join-Path $taskProject ('backups\'+(Get-Date -Format 'yyyyMMdd-HHmmssfff'))
New-Item -ItemType Directory -Force $taskBackup | Out-Null
& adb -s $Serial pull $taskRemote $taskBackup
if ($LASTEXITCODE -ne 0) { throw 'Backup failed' }
& adb -s $Serial shell test -f $taskSolverRemote
$taskHadSolver=$LASTEXITCODE -eq 0
if ($taskHadSolver) {
    & adb -s $Serial pull $taskSolverRemote $taskBackup
    if ($LASTEXITCODE -ne 0) { throw 'Solver backup failed' }
}
& adb -s $Serial shell am force-stop com.beatgames.beatsaber
if ($LASTEXITCODE -ne 0) { throw 'Stop failed' }
try {
    foreach ($taskPair in @(@($taskSolver,$taskSolverRemote),@($taskBinary,$taskRemote))) {
        $taskPart=$taskPair[1]+'.part'
        & adb -s $Serial push $taskPair[0] $taskPart
        if ($LASTEXITCODE -ne 0) { throw 'Upload failed' }
        $taskHash=(Get-FileHash $taskPair[0] -Algorithm SHA256).Hash.ToLowerInvariant()
        $taskRemoteHash=(& adb -s $Serial shell sha256sum $taskPart)
        if ($LASTEXITCODE -ne 0 -or $taskRemoteHash -notmatch "^$taskHash\s") { throw 'Installed hash mismatch' }
        & adb -s $Serial shell mv $taskPart $taskPair[1]
        if ($LASTEXITCODE -ne 0) { throw 'Publish failed' }
        Write-Output "$($taskPair[1]): SHA256 $taskHash"
    }
} catch {
    & adb -s $Serial push (Join-Path $taskBackup 'libbstheater.so') $taskRemote
    if ($taskHadSolver) { & adb -s $Serial push (Join-Path $taskBackup 'libbstheaterjs.so') $taskSolverRemote }
    throw "Update failed; rollback files: $taskBackup. $($_.Exception.Message)"
}
& adb -s $Serial shell am start -n com.beatgames.beatsaber/com.unity3d.player.UnityPlayerActivity
if ($LASTEXITCODE -ne 0) { throw 'Launch failed' }
Write-Output "Updated both Theater libraries. Rollback: $taskBackup"

