param([string]$Version='0.1.1')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
if($Version -notmatch '^[0-9a-z.-]+$'){throw 'Invalid version'}
$release=Join-Path $root ('dist\editor-'+$Version+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$stage=Join-Path $release 'staging'
$app=Join-Path $stage 'scripts\MWArcadeDrift'
New-Item -ItemType Directory -Path $app,(Join-Path $stage 'licenses') -Force | Out-Null
foreach($name in @('Configurator.exe','ConfigCheck.exe','fields.json')){Copy-Item -LiteralPath (Join-Path $root "bin\$name") -Destination (Join-Path $app $name)}
foreach($lang in @('EN','JA')){Copy-Item -LiteralPath (Join-Path $root "CONFIGURATION_$lang.md") -Destination (Join-Path $stage "EDITOR_CONFIGURATION_$lang.md")}
Copy-Item -LiteralPath (Join-Path $root 'packaging\EDITOR.md') -Destination (Join-Path $stage 'EDITOR_README.md')
Copy-Item -LiteralPath (Join-Path $root 'third_party\nlohmann\LICENSE.MIT') -Destination (Join-Path $stage 'licenses\nlohmann-json.txt')
if(Get-ChildItem $stage -Recurse -File | Where-Object {$_.Extension -in @('.asi','.dll','.ini')}){throw 'Editor package contains core files'}
$zip=Join-Path $release "MWArcadeDrift-Editor-$Version.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
$verify=Join-Path $release 'verification';Expand-Archive -LiteralPath $zip -DestinationPath $verify
$fixture=Join-Path $release 'fixture';$car=Join-Path $fixture 'vehicles\SVJ';New-Item -ItemType Directory -Path $car -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'config\default_drift.json') -Destination $fixture
Copy-Item -LiteralPath (Join-Path $root 'config\default_camera.json') -Destination $fixture
Copy-Item -LiteralPath (Join-Path $root 'config\settings.json') -Destination $fixture
Copy-Item -LiteralPath (Join-Path $root 'config\default_drift.json') -Destination (Join-Path $car 'drift.json')
Copy-Item -LiteralPath (Join-Path $root 'config\default_camera.json') -Destination (Join-Path $car 'camera.json')
Copy-Item -LiteralPath (Join-Path $verify 'scripts\MWArcadeDrift\fields.json') -Destination (Join-Path $fixture 'fields.json')
$editor=Join-Path $verify 'scripts\MWArcadeDrift\Configurator.exe'
$test=Start-Process -FilePath $editor -ArgumentList @(('"'+$fixture+'"'),'--self-test') -WindowStyle Hidden -PassThru
if(!$test.WaitForExit(30000)){if((Get-Process -Id $test.Id).Path -eq $editor){Stop-Process -Id $test.Id};throw 'Editor test timeout'}
if($test.ExitCode){Get-Content (Join-Path $fixture 'editor-error.txt');throw 'Packaged editor test failed'}
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $release 'SHA256SUMS-editor.txt') -Value "$hash  $(Split-Path $zip -Leaf)" -Encoding ascii
Get-Content (Join-Path $fixture 'editor-test-result.txt')
Write-Output "RELEASE=$release"
