param([string]$VcVars)
$ErrorActionPreference='Stop'
$taskProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $VcVars) {
    $taskVsWhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    $taskVs=& $taskVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $VcVars=Join-Path $taskVs 'VC\Auxiliary\Build\vcvars64.bat'
}
if (-not (Test-Path $VcVars)) { throw 'MSVC C++ tools required for host tests' }
$taskTests=Join-Path $taskProject 'tests'
$taskCmd=Join-Path $taskTests 'build-tests.cmd'
@"
@echo off
call "$VcVars" >nul
cl /nologo /std:c++20 /EHsc /W3 /I "$taskProject\include" /I "$taskProject\extern\includes" "$taskProject\src\video_config.cpp" "$taskTests\config_tests.cpp" /Fe:"$taskTests\config_tests.exe" /Fo:"$taskTests\\"
exit /b %errorlevel%
"@ | Set-Content $taskCmd
& cmd /c $taskCmd
if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
$taskScratch=Join-Path $taskTests ('scratch-'+[guid]::NewGuid().ToString('N'))
& (Join-Path $taskTests 'config_tests.exe') $taskScratch (Join-Path $taskTests 'fixtures\cinema-video-50261.json')
if ($LASTEXITCODE -ne 0) { throw 'Config tests failed' }
