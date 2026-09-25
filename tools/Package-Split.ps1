param([string]$Version='0.1.0')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
if(Test-Path "$root\release-hold.json"){ $hold=Get-Content "$root\release-hold.json" -Raw | ConvertFrom-Json; if($hold.Active){throw $hold.Reason} }
if($Version -notmatch '^[0-9a-z.-]+$'){throw 'Invalid version'}
if((Get-Content "$root\src\Plugin.cpp" -Raw) -notmatch ([regex]::Escape("MW Arcade Drift $Version pid="))){throw 'Version does not match core source'}
$msbuild=& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -find MSBuild\**\Bin\MSBuild.exe
& $msbuild "$root\CorePackageTests.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
if($LASTEXITCODE){throw 'Package test build failed'}
$release=Join-Path $root ('dist\'+$Version+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if(Test-Path $release){throw 'Output already exists'}
$core=Join-Path $release 'staging\Core';$editor=Join-Path $release 'staging\Editor'
New-Item -ItemType Directory -Path $core,$editor -Force | Out-Null
function Copy-ReleaseFile($source,$dest,$relative){
 $target=Join-Path $dest $relative
 New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
 Copy-Item -LiteralPath (Join-Path $root $source) -Destination $target
}
Copy-ReleaseFile 'bin\MWArcadeDrift.asi' $core 'scripts\MWArcadeDrift.asi'
Copy-ReleaseFile 'MWCriterionDrift.ini' $core 'scripts\MWArcadeDrift.ini'
foreach($n in @('default_drift.json','default_camera.json','settings.json')){Copy-ReleaseFile "config\$n" $core "scripts\MWArcadeDrift\$n"}
foreach($lang in @('EN','JA')){
 Copy-ReleaseFile "packaging\CORE_$lang.md" $core "CORE_$lang.md"
 Copy-ReleaseFile "CONFIGURATION_$lang.md" $core "CONFIGURATION_$lang.md"
 Copy-ReleaseFile "CONFIGURATION_$lang.md" $editor "EDITOR_CONFIGURATION_$lang.md"
}
foreach($n in @('Configurator.exe','ConfigCheck.exe','fields.json')){Copy-ReleaseFile "bin\$n" $editor "scripts\MWArcadeDrift\$n"}
Copy-ReleaseFile 'packaging\EDITOR.md' $editor 'EDITOR_README.md'
Copy-ReleaseFile 'third_party\minhook\LICENSE.txt' $core 'licenses\MinHook.txt'
foreach($dir in @($core,$editor)){Copy-ReleaseFile 'third_party\nlohmann\LICENSE.MIT' $dir 'licenses\nlohmann-json.txt'}
# Allow-listed inputs only. No recursive bin/production folder copying.
if(Get-ChildItem $core -File -Recurse | Where-Object {$_.Extension -notin @('.asi','.ini','.json','.md','.txt')}){throw 'Unexpected core payload'}
if(Get-ChildItem $core -Filter '*.exe' -Recurse){throw 'Core must contain no EXE'}
$collision=@(Get-ChildItem $editor -Recurse -File | Where-Object {Test-Path (Join-Path $core $_.FullName.Substring($editor.Length+1))})
# A shared license is harmless; runtime and configuration payloads may not overlap.
if($collision | Where-Object {$_.FullName.Substring($editor.Length+1) -notlike 'licenses\*'}){throw 'Editor could overwrite core files'}
foreach($pair in @(@{Kind='Core';Directory=$core},@{Kind='Editor';Directory=$editor})){
 $manifest=@(Get-ChildItem $pair.Directory -File -Recurse | Sort-Object FullName | ForEach-Object {[ordered]@{Path=$_.FullName.Substring($pair.Directory.Length+1).Replace('\','/');SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 $manifest | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $release ($pair.Kind+'-manifest.json')) -Encoding utf8
 Compress-Archive -Path (Join-Path $pair.Directory '*') -DestinationPath (Join-Path $release "MWArcadeDrift-$($pair.Kind)-$Version.zip")
}
# Tests run against freshly extracted copies, never against production or release staging.
$test=Join-Path $release 'verification';New-Item -ItemType Directory -Path $test | Out-Null
Expand-Archive -LiteralPath "$release\MWArcadeDrift-Core-$Version.zip" -DestinationPath $test
& "$root\bin\CorePackageTests.exe" $test
if($LASTEXITCODE){throw 'Core-only profile test failed'}
& "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\Smoke-Reject.ps1" -LibraryPath "$test\scripts\MWArcadeDrift.asi"
if($LASTEXITCODE){throw 'Extracted core load/reject test failed'}
$profileRoot=Join-Path $test 'scripts\MWArcadeDrift'
& "$root\bin\ConfigCheck.exe" $profileRoot
if($LASTEXITCODE){throw 'Extracted core configuration invalid'}
$before=@(Get-ChildItem "$test\scripts" -File -Recurse | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Hash=(Get-FileHash $_.FullName).Hash}})
Expand-Archive -LiteralPath "$release\MWArcadeDrift-Editor-$Version.zip" -DestinationPath $test -Force
foreach($f in $before){if((Get-FileHash $f.Path).Hash -ne $f.Hash){throw 'Optional editor install changed core/settings'}}
# Run the editor's behavioral tests on a second copy; those tests intentionally edit JSON.
$editTest=Join-Path $release 'editor-verification';Copy-Item -LiteralPath $test -Destination $editTest -Recurse
$editRoot=Join-Path $editTest 'scripts\MWArcadeDrift'
$p=Start-Process "$editRoot\Configurator.exe" -ArgumentList @(('"'+$editRoot+'"'),'--self-test') -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(30000)){if((Get-Process -Id $p.Id).Path -eq "$editRoot\Configurator.exe"){Stop-Process -Id $p.Id};throw 'Packaged editor test timeout'}
if($p.ExitCode){Get-Content "$editRoot\editor-error.txt";throw 'Packaged editor test failed'}
Get-Content "$editRoot\editor-test-result.txt"
# Remove precisely the three optional files, then re-check unchanged core settings.
foreach($n in @('Configurator.exe','ConfigCheck.exe','fields.json')){Remove-Item -LiteralPath (Join-Path $profileRoot $n)}
foreach($f in $before){if((Get-FileHash $f.Path).Hash -ne $f.Hash){throw 'Optional editor removal changed core/settings'}}
& "$root\bin\CorePackageTests.exe" $test
if($LASTEXITCODE){throw 'Core after editor removal failed'}
Get-FileHash "$release\*.zip" | Select-Object @{n='File';e={Split-Path $_.Path -Leaf}},Hash | ConvertTo-Json | Set-Content "$release\SHA256.json" -Encoding utf8
'PASS core has zero EXEs; editor overlay/removal preserves core/settings; native core profile tests and packaged editor safety tests pass. In-game core-only acceptance not performed.' | Set-Content "$release\verification.txt" -Encoding utf8
Set-Content "$root\evidence\latest-split-package.txt" $release
Get-Content "$release\verification.txt"
Write-Output "RELEASE=$release"
