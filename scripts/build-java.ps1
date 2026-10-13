param()
$ErrorActionPreference='Stop'
$taskProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskJava=Join-Path $taskProject 'java'
$taskTools=[IO.Path]::GetFullPath((Join-Path $taskProject '..\tools\r8'))
$taskAndroid=Join-Path $taskTools 'platform-33\android-13\android.jar'
$taskJars=@(Get-ChildItem (Join-Path $taskJava 'libs') -Filter '*.jar' | Sort-Object Name | ForEach-Object FullName)
$taskClasses=Join-Path $taskJava 'classes'
New-Item -ItemType Directory -Force $taskClasses,(Join-Path $taskJava 'dex'),(Join-Path $taskJava 'desugar'),(Join-Path $taskJava 'merged') | Out-Null
$taskSources=@(Get-ChildItem (Join-Path $taskJava 'src') -Recurse -Filter '*.java' | ForEach-Object FullName)
& javac --release 11 -encoding UTF-8 -cp ((@($taskAndroid)+$taskJars) -join ';') -d $taskClasses @taskSources
if ($LASTEXITCODE -ne 0) { throw 'Java downloader compilation failed' }
& jar --create --file (Join-Path $taskJava 'downloader.jar') -C $taskClasses .
if ($LASTEXITCODE -ne 0) { throw 'Java archive failed' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskConfigZip=[IO.Compression.ZipFile]::OpenRead((Join-Path $taskTools 'desugar-config.jar'))
try { [IO.Compression.ZipFileExtensions]::ExtractToFile($taskConfigZip.GetEntry('META-INF/desugar/d8/desugar.json'),(Join-Path $taskTools 'desugar.json'),$true) } finally { $taskConfigZip.Dispose() }
$taskCommon=@('--release','--min-api','32','--lib',$taskAndroid,'--desugared-lib',(Join-Path $taskTools 'desugar.json'))
& java -Xmx2g -cp (Join-Path $taskTools 'r8.jar') com.android.tools.r8.D8 @taskCommon --output (Join-Path $taskJava 'dex') (Join-Path $taskJava 'downloader.jar') @taskJars
if ($LASTEXITCODE -ne 0) { throw 'D8 compilation failed' }
& java -Xmx2g -cp (Join-Path $taskTools 'r8.jar') com.android.tools.r8.L8 @taskCommon --output (Join-Path $taskJava 'desugar') (Join-Path $taskTools 'desugar-jdk.jar') (Join-Path $taskTools 'desugar-config.jar')
if ($LASTEXITCODE -ne 0) { throw 'Android library desugaring failed' }
foreach ($taskKind in @('dex','desugar')) {
    $taskDexFiles=@(Get-ChildItem (Join-Path $taskJava $taskKind) -Filter '*.dex')
    if ($taskDexFiles.Count -ne 1) { throw "Unexpected multiple DEX files in $taskKind" }
    $taskName=if($taskKind -eq 'dex') {'bstheater-download.dex'} else {'bstheater-desugar.dex'}
    Copy-Item -LiteralPath $taskDexFiles[0].FullName -Destination (Join-Path $taskJava $taskName) -Force
}
