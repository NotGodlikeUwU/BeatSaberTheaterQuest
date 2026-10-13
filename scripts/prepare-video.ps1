param([Parameter(Mandatory)][string]$Video,[Parameter(Mandatory)][string]$MapFolder,[Nullable[int]]$Offset,[switch]$Transcode,[string]$Ffmpeg='ffmpeg')
$ErrorActionPreference='Stop'
$taskMap=(Resolve-Path -LiteralPath $MapFolder).Path
$taskVideo=(Resolve-Path -LiteralPath $Video).Path
if (-not (Test-Path (Join-Path $taskMap 'Info.dat')) -and -not (Test-Path (Join-Path $taskMap 'info.dat'))) { throw 'Map folder must contain Info.dat' }
$taskOutput=Join-Path $taskMap 'video.mp4'
if ($Transcode) {
    & $Ffmpeg -n -i $taskVideo -an -vf 'scale=w=1280:h=720:force_original_aspect_ratio=decrease:force_divisible_by=2,fps=30' -c:v libx264 -profile:v main -pix_fmt yuv420p -crf 22 -movflags +faststart $taskOutput
    if ($LASTEXITCODE -ne 0) { throw 'ffmpeg failed; existing output is never overwritten' }
} else {
    if ([IO.Path]::GetExtension($taskVideo).ToLowerInvariant() -ne '.mp4') { throw 'Use MP4 or -Transcode with ffmpeg' }
    if ($taskVideo -ne $taskOutput) {
        if (Test-Path $taskOutput) { throw 'video.mp4 already exists; rename it before replacing' }
        Copy-Item -LiteralPath $taskVideo -Destination $taskOutput
    }
}
$taskConfigPath=Join-Path $taskMap 'theater-video.json'
if (Test-Path $taskConfigPath) {
    $taskConfig=Get-Content -LiteralPath $taskConfigPath -Raw | ConvertFrom-Json
    Copy-Item -LiteralPath $taskConfigPath -Destination ($taskConfigPath+'.backup-'+(Get-Date -Format 'yyyyMMdd-HHmmssfff'))
} else { $taskConfig=[pscustomobject]@{offset=0;playbackSpeed=1;loop=$false} }
$taskConfig | Add-Member -NotePropertyName videoFile -NotePropertyValue video.mp4 -Force
if ($PSBoundParameters.ContainsKey('Offset')) { $taskConfig | Add-Member -NotePropertyName offset -NotePropertyValue $Offset -Force }
$taskConfig | ConvertTo-Json -Depth 50 | Set-Content -LiteralPath $taskConfigPath -Encoding utf8
Write-Output "Prepared $taskOutput and $taskConfigPath. Copy both to the same map folder on Quest."

