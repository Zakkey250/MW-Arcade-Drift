param(
    [string]$GameRoot = 'C:\Program Files (x86)\EA GAMES\Need For Speed Most Wanted REDUX 3.04 - Main'
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot
$game = (Resolve-Path -LiteralPath $GameRoot).Path
$exe = Join-Path $game 'speed.exe'
$scripts = Join-Path $game 'scripts'
$expected = '0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf) -or -not (Test-Path -LiteralPath $scripts -PathType Container)) { throw 'Redux game layout not found.' }
if ((Get-Item -LiteralPath $exe).Length -ne 5926912 -or (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $expected) { throw 'Redux executable identity mismatch.' }
if (-not (Test-Path -LiteralPath (Join-Path $game 'dinput8.dll') -PathType Leaf)) { throw 'Existing ASI loader not found.' }
$running = @(Get-CimInstance Win32_Process -Filter "name = 'speed.exe'" | Where-Object { $_.ExecutablePath -and ([IO.Path]::GetFullPath($_.ExecutablePath) -ieq $exe) })
if ($running.Count) { throw 'Redux is running; exit before installing.' }
$sourceAsi = Join-Path $project 'bin\MWArcadeDrift.asi'
$sourceIni = Join-Path $project 'MWCriterionDrift.ini'
$config = Join-Path $project 'config'
foreach ($file in @($sourceAsi, $sourceIni, (Join-Path $config 'default_drift.json'), (Join-Path $config 'default_camera.json'), (Join-Path $config 'settings.json'))) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing source: $file" }
}
$evidence = Join-Path $project ('evidence\redux-install-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$stage = Join-Path $evidence 'stage\scripts'
$stageConfig = Join-Path $stage 'MWArcadeDrift'
New-Item -ItemType Directory -Path $stageConfig -Force | Out-Null
Copy-Item -LiteralPath $sourceAsi -Destination (Join-Path $stage 'MWArcadeDrift.asi')
$iniText = Get-Content -LiteralPath $sourceIni -Raw
if ($iniText -notmatch '(?ms)^\[Camera\]\s*\r?\nEnabled=1\s*$') { throw 'Expected camera setting missing; refusing ambiguous install.' }
$iniText = $iniText -replace '(?ms)(^\[Camera\]\s*\r?\n)Enabled=1', '${1}Enabled=0'
$iniText = $iniText -replace '(?m)^; Smooth drift chase camera\.', '; Disabled for Redux while NFSMWOrbitCamera.asi is installed. Smooth drift chase camera.'
[IO.File]::WriteAllText((Join-Path $stage 'MWArcadeDrift.ini'), $iniText, (New-Object System.Text.UTF8Encoding($false)))
foreach ($name in @('default_drift.json', 'default_camera.json', 'settings.json')) { Copy-Item -LiteralPath (Join-Path $config $name) -Destination (Join-Path $stageConfig $name) }
& (Join-Path $project 'bin\ConfigCheck.exe') $stageConfig
if ($LASTEXITCODE) { throw 'Staged configuration invalid.' }

# Record the existing loader, camera mods, executable and saves without editing them.
$protected = @($exe, (Join-Path $game 'dinput8.dll'))
$protected += @(Get-ChildItem -LiteralPath $scripts -File | Where-Object { $_.Name -match '^(NFSMWOrbitCamera|NFSMostWanted\.WidescreenFix)\.(asi|ini)$' } | ForEach-Object FullName)
$save = Join-Path $game 'SAVE'
if (Test-Path -LiteralPath $save) { $protected += @(Get-ChildItem -LiteralPath $save -Recurse -File | ForEach-Object FullName) }
function Snapshot([string[]]$paths) {
    @($paths | ForEach-Object { [pscustomobject]@{ Path = $_; Length = (Get-Item -LiteralPath $_).Length; SHA256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash } })
}
$before = @(Snapshot $protected)
$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'protected-before.json') -Encoding UTF8

$destFiles = @(
    @('MWArcadeDrift.asi', 'MWArcadeDrift.asi'),
    @('MWArcadeDrift.ini', 'MWArcadeDrift.ini'),
    @('MWArcadeDrift\default_drift.json', 'MWArcadeDrift\default_drift.json'),
    @('MWArcadeDrift\default_camera.json', 'MWArcadeDrift\default_camera.json'),
    @('MWArcadeDrift\settings.json', 'MWArcadeDrift\settings.json')
)
$backup = Join-Path $evidence 'previous'
foreach ($pair in $destFiles) {
    $dest = Join-Path $scripts $pair[1]
    if (Test-Path -LiteralPath $dest) {
        $saved = Join-Path $backup $pair[1]
        New-Item -ItemType Directory -Path (Split-Path $saved) -Force | Out-Null
        Copy-Item -LiteralPath $dest -Destination $saved
    }
}
foreach ($pair in $destFiles) {
    $from = Join-Path $stage $pair[0]
    $dest = Join-Path $scripts $pair[1]
    New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
    Copy-Item -LiteralPath $from -Destination $dest -Force
    if ((Get-FileHash -LiteralPath $from -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash) { throw "Installed file failed hash check: $dest" }
}
$after = @(Snapshot $protected)
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'protected-after.json') -Encoding UTF8
if (($before | ConvertTo-Json -Compress -Depth 4) -cne ($after | ConvertTo-Json -Compress -Depth 4)) { throw 'A protected game, mod or save file changed.' }
& (Join-Path $project 'bin\ConfigCheck.exe') (Join-Path $scripts 'MWArcadeDrift')
if ($LASTEXITCODE) { throw 'Installed configuration invalid.' }
Write-Output "REDUX_READY game=$game evidence=$evidence camera=off"
