param(
    [string]$Package=(Join-Path $PSScriptRoot '..\BeatSaberTheaterQuest-0.1.0.qmod'),
    [Parameter(Mandatory)][string]$SchemaPath
)
$ErrorActionPreference='Stop'
# Use MBF's actual schema: https://github.com/Lauriethefish/ModsBeforeFriday/blob/main/mbf-agent/src/mod_man/qmod_schema.json
$taskSchema=Get-Content -LiteralPath $SchemaPath -Raw
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskArchive=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Package))
try {
    $taskEntry=$taskArchive.GetEntry('mod.json')
    if(-not $taskEntry){throw 'QMOD is missing mod.json'}
    $taskReader=[IO.StreamReader]::new($taskEntry.Open())
    try {$taskJson=$taskReader.ReadToEnd()} finally {$taskReader.Dispose()}
} finally {$taskArchive.Dispose()}
if(-not ($taskJson | Test-Json -Schema $taskSchema)){throw 'Packaged manifest does not match the MBF QMOD schema'}
$taskManifest=$taskJson | ConvertFrom-Json
foreach($taskDependency in $taskManifest.dependencies){
    if(-not $taskDependency.version.StartsWith('^')){throw ('Dependency must allow compatible updates: '+$taskDependency.id)}
}
# Regression: the shortened version reported by the tester must be rejected.
$taskManifest.version='0.1'
$taskInvalid=$taskManifest | ConvertTo-Json -Depth 30
if($taskInvalid | Test-Json -Schema $taskSchema -ErrorAction SilentlyContinue){throw 'Schema accepted invalid version 0.1'}
Write-Output 'Packaged MBF schema validation, caret dependencies, and invalid-version regression passed'
