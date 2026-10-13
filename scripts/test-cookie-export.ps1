param()
$ErrorActionPreference='Stop'
$taskProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskClasses=Join-Path $taskProject 'java\test-classes'
$taskScratch=Join-Path $taskProject ('java\scratch-export-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $taskScratch | Out-Null
$taskPortFile=Join-Path $taskScratch 'port.txt'
$taskOutput=Join-Path $taskScratch 'youtube-cookies.txt'
$taskJava=(Get-Command java).Source
$taskFixture=Start-Process -FilePath $taskJava -ArgumentList @('-cp',"`"$taskClasses`"",'org.bstheater.quest.BrowserCookieFixture',"`"$taskPortFile`"") -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $taskScratch 'fixture.log') -RedirectStandardError (Join-Path $taskScratch 'fixture-error.log')
try {
    $taskWait=[Diagnostics.Stopwatch]::StartNew()
    while (-not (Test-Path -LiteralPath $taskPortFile)) {
        if ($taskWait.Elapsed.TotalSeconds -gt 10 -or $taskFixture.HasExited) { throw 'Cookie protocol fixture failed to start' }
        Start-Sleep -Milliseconds 100
    }
    $taskPort=[int](Get-Content -LiteralPath $taskPortFile -Raw)
    & (Join-Path $PSScriptRoot 'export-quest-youtube-cookies.ps1') -DevToolsPort $taskPort -NoCopy -OutputFile $taskOutput
    if (-not $taskFixture.WaitForExit(10000) -or $taskFixture.ExitCode -ne 0) { throw 'Cookie protocol fixture failed' }
    $taskCookies=Get-Content -LiteralPath $taskOutput -Raw
    if ($taskCookies -notmatch '#HttpOnly_\.youtube\.com' -or $taskCookies -notmatch 'LOGIN_INFO' -or $taskCookies -match 'OTHER_ACCOUNT|never-export|SPOOF') { throw 'Export omitted login cookies or included another domain' }
    Write-Output 'Cookie export integration passed (synthetic session only)'
} finally {
    if (-not $taskFixture.HasExited) {$taskFixture.Kill()}
    $taskFixture.Dispose()
}
