param([string]$Directory='C:\Program Files (x86)\EA GAMES\Need for Speed Most Wanted Main\scripts\MWArcadeDrift')
$ErrorActionPreference='Stop'
& (Join-Path (Split-Path $PSScriptRoot) 'bin\ConfigCheck.exe') $Directory
if($LASTEXITCODE){throw 'Settings validation failed'}
