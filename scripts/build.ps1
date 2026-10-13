param([switch]$clean)
$ErrorActionPreference = 'Stop'
$taskProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskBuild = [IO.Path]::GetFullPath((Join-Path $taskProject 'build'))
if ($taskBuild -ne "$taskProject\build") { throw 'Unexpected build path' }
if ($clean -and (Test-Path -LiteralPath $taskBuild)) { Remove-Item -LiteralPath $taskBuild -Recurse -Force }
$taskTools = Join-Path $taskProject '..\tools\qpm'
if (Test-Path (Join-Path $taskTools 'ninja.exe')) { $env:Path = "$taskTools;" + $env:Path }
$taskDexPaths = @('bstheater-download.dex','bstheater-desugar.dex') | ForEach-Object { Join-Path $taskProject "java\$_" }
$taskJavaInputs = @(Get-ChildItem (Join-Path $taskProject 'java\src') -Recurse -Filter '*.java') + @(Get-Item (Join-Path $PSScriptRoot 'build-java.ps1')) + @(Get-ChildItem (Join-Path $taskProject 'java\libs') -Filter '*.jar')
$taskRebuildJava = @($taskDexPaths | Where-Object { -not (Test-Path -LiteralPath $_) }).Count -gt 0
if (-not $taskRebuildJava) {
    $taskOldestDex = ($taskDexPaths | ForEach-Object { (Get-Item -LiteralPath $_).LastWriteTimeUtc } | Sort-Object | Select-Object -First 1)
    $taskRebuildJava = @($taskJavaInputs | Where-Object LastWriteTimeUtc -GT $taskOldestDex).Count -gt 0
}
if ($taskRebuildJava) { & (Join-Path $PSScriptRoot 'build-java.ps1') }
& cmake -S $taskProject -B $taskBuild -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& cmake --build $taskBuild -j 4
if ($LASTEXITCODE -ne 0) { throw 'Android build failed' }
