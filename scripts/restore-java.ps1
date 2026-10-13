param()
$ErrorActionPreference='Stop'
$taskJava=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\java'))
$taskUrls=Get-Content (Join-Path $taskJava 'dependencies.json') -Raw | ConvertFrom-Json
$taskHashes=Get-Content (Join-Path $taskJava 'dependencies.sha256.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force (Join-Path $taskJava 'libs') | Out-Null
foreach ($taskHash in $taskHashes) {
    if ([IO.Path]::GetFileName($taskHash.Name) -ne $taskHash.Name) { throw 'Unexpected dependency filename' }
    $taskTarget=Join-Path $taskJava ('libs\'+$taskHash.Name)
    if (-not (Test-Path -LiteralPath $taskTarget)) { Invoke-WebRequest -Uri $taskUrls.($taskHash.Name) -OutFile $taskTarget }
    if ((Get-FileHash -LiteralPath $taskTarget -Algorithm SHA256).Hash -ne $taskHash.SHA256) { throw ('Dependency checksum mismatch: '+$taskHash.Name) }
}
Write-Output 'Java dependency checksums verified'
