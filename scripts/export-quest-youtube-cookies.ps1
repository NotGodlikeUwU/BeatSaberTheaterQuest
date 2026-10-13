param(
    [string]$Serial,
    [string]$DebugSocket,
    [string]$OutputFile=(Join-Path $PSScriptRoot '..\private\youtube-cookies.txt'),
    [switch]$NoCopy,
    [int]$DevToolsPort=0
)
$ErrorActionPreference='Stop'
# Use the browser's documented DevTools interface via authorized USB ADB.
# No browser database is opened, and cookie values are never written to stdout.
$taskAdb=(Get-Command adb -ErrorAction SilentlyContinue).Source
if (-not $taskAdb) { $taskAdb='C:\Program Files\platform-tools\adb.exe' }
$taskForwarded=$false
$taskWebSocket=$null
$taskCancellation=[Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds(20))
try {
    if ($DevToolsPort -eq 0) {
        if (-not (Test-Path -LiteralPath $taskAdb)) { throw 'Android platform-tools / adb required' }
        if (-not $Serial) {
            $taskDevices=@(& $taskAdb devices | ForEach-Object { if ($_ -match '^(\S+)\s+device$') {$Matches[1]} })
            if ($taskDevices.Count -ne 1) { throw 'Connect and authorize one Quest, or specify -Serial' }
            $Serial=$taskDevices[0]
        }
        if (-not $DebugSocket) {
            $taskSockets=@(& $taskAdb -s $Serial shell cat /proc/net/unix | ForEach-Object {
                if ($_ -match '@([A-Za-z0-9_.-]*(?:chrome|oculus|browser)[A-Za-z0-9_.-]*devtools_remote[A-Za-z0-9_.-]*)\s*$') {$Matches[1]}
            } | Sort-Object -Unique)
            if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect browser debugging sockets' }
            if ($taskSockets.Count -ne 1) { throw 'Open YouTube in Meta Quest Browser. If several browser debugging sockets exist, specify -DebugSocket.' }
            $DebugSocket=$taskSockets[0]
        }
        if ($DebugSocket -notmatch '^[A-Za-z0-9_.-]+$' -or $DebugSocket -notlike '*devtools_remote*') { throw 'Invalid browser debugging socket' }
        $taskPortOutput=& $taskAdb -s $Serial forward tcp:0 "localabstract:$DebugSocket"
        if ($LASTEXITCODE -ne 0 -or $taskPortOutput -notmatch '^\d+$') { throw 'Cannot forward browser DevTools' }
        $DevToolsPort=[int]$taskPortOutput
        $taskForwarded=$true
    } elseif (-not $NoCopy) { throw '-DevToolsPort requires -NoCopy; normal Quest import uses USB ADB' }
    if ($DevToolsPort -lt 1 -or $DevToolsPort -gt 65535) { throw 'Invalid DevTools port' }
    $taskTabs=Invoke-RestMethod "http://127.0.0.1:$DevToolsPort/json/list" -TimeoutSec 10 -NoProxy
    $taskTab=$taskTabs | Where-Object {
        $taskUri=$null
        [uri]::TryCreate($_.url,[UriKind]::Absolute,[ref]$taskUri) -and $_.type -eq 'page' -and
            $taskUri.Scheme -eq 'https' -and $taskUri.Host -in @('www.youtube.com','youtube.com','m.youtube.com')
    } | Select-Object -First 1
    if (-not $taskTab) { throw 'Open a signed-in YouTube tab in Meta Quest Browser, then run this script again' }
    $taskDebugger=[uri]$taskTab.webSocketDebuggerUrl
    if ($taskDebugger.Scheme -ne 'ws' -or $taskDebugger.AbsolutePath -notlike '/devtools/page/*') { throw 'Unexpected browser debugging endpoint' }
    $taskWebSocket=[Net.WebSockets.ClientWebSocket]::new()
    $taskWebSocket.ConnectAsync([uri]"ws://127.0.0.1:$DevToolsPort$($taskDebugger.AbsolutePath)",$taskCancellation.Token).GetAwaiter().GetResult() | Out-Null
    $taskRequest=[Text.Encoding]::UTF8.GetBytes('{"id":1,"method":"Network.getCookies","params":{"urls":["https://www.youtube.com/"]}}')
    $taskWebSocket.SendAsync([ArraySegment[byte]]::new($taskRequest),[Net.WebSockets.WebSocketMessageType]::Text,$true,$taskCancellation.Token).GetAwaiter().GetResult() | Out-Null
    $taskMessage=$null
    while (-not $taskMessage) {
        $taskBuffer=[byte[]]::new(16384)
        $taskBytes=[IO.MemoryStream]::new()
        try {
            do {
                $taskRead=$taskWebSocket.ReceiveAsync([ArraySegment[byte]]::new($taskBuffer),$taskCancellation.Token).GetAwaiter().GetResult()
                if ($taskRead.MessageType -eq [Net.WebSockets.WebSocketMessageType]::Close) { throw 'Browser closed the debugging connection' }
                if ($taskBytes.Length+$taskRead.Count -gt 1024*1024) { throw 'Browser debugging response too large' }
                $taskBytes.Write($taskBuffer,0,$taskRead.Count)
            } while (-not $taskRead.EndOfMessage)
            $taskParsed=[Text.Encoding]::UTF8.GetString($taskBytes.ToArray()) | ConvertFrom-Json
            if ($taskParsed.id -eq 1) { $taskMessage=$taskParsed }
        } finally { $taskBytes.Dispose() }
    }
    if ($taskMessage.error) { throw 'Browser rejected the YouTube cookie export request' }
    $taskCookies=@($taskMessage.result.cookies | Where-Object {
        $_.domain -match '^\.?([A-Za-z0-9-]+\.)*youtube\.com$' -and ($_.session -or $_.expires -gt [DateTimeOffset]::UtcNow.ToUnixTimeSeconds())
    })
    if (-not ($taskCookies | Where-Object name -EQ 'LOGIN_INFO') -or -not ($taskCookies | Where-Object { $_.name -in @('SAPISID','__Secure-1PAPISID','__Secure-3PAPISID') })) {
        throw 'YouTube login cookies not present. Sign in and open a video in Meta Quest Browser.'
    }
    $taskLines=[Collections.Generic.List[string]]::new()
    $taskLines.Add('# Netscape HTTP Cookie File')
    foreach ($taskCookie in $taskCookies) {
        if ($taskCookie.name -notmatch '^[!#$%&''*+.^_`|~0-9A-Za-z-]+$' -or $taskCookie.value -match '[\x00-\x20\x7f;]' -or $taskCookie.path -match '[\r\n\t]') { throw 'Invalid cookie data returned by browser' }
        $taskDomain=$taskCookie.domain
        $taskSubdomains=if($taskDomain.StartsWith('.')) {'TRUE'} else {'FALSE'}
        if ($taskCookie.httpOnly) {$taskDomain='#HttpOnly_'+$taskDomain}
        $taskSecure=if($taskCookie.secure) {'TRUE'} else {'FALSE'}
        $taskExpiry=if($taskCookie.session -or $taskCookie.expires -lt 0) {0} else {[long][Math]::Floor($taskCookie.expires)}
        $taskLines.Add((@($taskDomain,$taskSubdomains,$taskCookie.path,$taskSecure,$taskExpiry,$taskCookie.name,$taskCookie.value) -join "`t"))
    }
    $taskOut=[IO.Path]::GetFullPath($OutputFile)
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($taskOut)) | Out-Null
    [IO.File]::WriteAllLines($taskOut,$taskLines,[Text.UTF8Encoding]::new($false))
    Write-Output "Exported $($taskCookies.Count) YouTube cookies through Meta Quest Browser DevTools. Values omitted."
    if (-not $NoCopy) { & (Join-Path $PSScriptRoot 'import-youtube-cookies.ps1') -CookieFile $taskOut -Serial $Serial }
} finally {
    if ($taskWebSocket) { $taskWebSocket.Dispose() }
    $taskCancellation.Dispose()
    if ($taskForwarded) { & $taskAdb -s $Serial forward --remove "tcp:$DevToolsPort" | Out-Null }
}
