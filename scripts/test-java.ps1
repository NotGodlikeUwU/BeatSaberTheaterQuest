param()
$ErrorActionPreference='Stop'
$taskJava=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\java'))
$taskClasses=Join-Path $taskJava 'test-classes'
New-Item -ItemType Directory -Force $taskClasses | Out-Null
$taskJars=@(Get-ChildItem (Join-Path $taskJava 'libs') -Filter '*.jar' | Sort-Object Name | ForEach-Object FullName)
$taskAndroid=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\tools\r8\platform-33\android-13\android.jar'))
$taskSources=@(Get-ChildItem (Join-Path $taskJava 'src') -Recurse -Filter '*.java' | ForEach-Object FullName)+@(Get-ChildItem (Join-Path $taskJava 'tests') -Filter '*.java' | ForEach-Object FullName)
& javac --release 11 -encoding UTF-8 -cp ((@($taskAndroid)+$taskJars) -join ';') -d $taskClasses @taskSources
if ($LASTEXITCODE -ne 0) { throw 'Java test compilation failed' }
$taskScratch=Join-Path $taskJava ('scratch-'+[Guid]::NewGuid().ToString('N'))
& java -cp ((@($taskClasses)+$taskJars) -join ';') org.bstheater.quest.VideoDownloaderTest $taskScratch
if ($LASTEXITCODE -ne 0) { throw 'Java download tests failed' }
& java -cp ((@($taskClasses)+$taskJars) -join ';') org.bstheater.quest.YoutubeAuthTest $taskScratch
if ($LASTEXITCODE -ne 0) { throw 'YouTube session tests failed' }
& java -cp ((@($taskClasses,$taskAndroid)+$taskJars) -join ';') org.bstheater.quest.HlsManifestTest
if ($LASTEXITCODE -ne 0) { throw 'HLS manifest tests failed' }
