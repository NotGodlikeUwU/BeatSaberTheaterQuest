param([Parameter(Mandatory)][string]$Serial,[string]$Video='/data/local/tmp/bstheater-check/video.mp4')
$ErrorActionPreference='Stop'
$taskProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskTools=[IO.Path]::GetFullPath((Join-Path $taskProject '..\tools\r8'))
$taskScratch=Join-Path $taskProject 'tests\scratch-device-video'
New-Item -ItemType Directory -Force (Join-Path $taskScratch 'classes'),(Join-Path $taskScratch 'dex') | Out-Null
& javac --release 11 -cp (Join-Path $taskTools 'platform-33\android-13\android.jar') -d (Join-Path $taskScratch 'classes') (Join-Path $taskProject 'tests\android\AndroidVideoCheck.java')
if ($LASTEXITCODE -ne 0) { throw 'Device check compilation failed' }
& jar --create --file (Join-Path $taskScratch 'check.jar') -C (Join-Path $taskScratch 'classes') .
if ($LASTEXITCODE -ne 0) { throw 'Device check archive failed' }
& java -cp (Join-Path $taskTools 'r8.jar') com.android.tools.r8.D8 --release --min-api 32 --lib (Join-Path $taskTools 'platform-33\android-13\android.jar') --output (Join-Path $taskScratch 'dex') (Join-Path $taskScratch 'check.jar')
if ($LASTEXITCODE -ne 0) { throw 'Device check DEX failed' }
& adb -s $Serial push (Join-Path $taskScratch 'dex\classes.dex') /data/local/tmp/bstheater-check/codec-check.dex
if ($LASTEXITCODE -ne 0) { throw 'Device check push failed' }
$taskQuotedVideo="'"+$Video.Replace("'","'\''")+"'"
& adb -s $Serial shell env CLASSPATH=/data/local/tmp/bstheater-check/codec-check.dex app_process /system/bin AndroidVideoCheck $taskQuotedVideo
if ($LASTEXITCODE -ne 0) { throw 'Quest codec check failed' }
