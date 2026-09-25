$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
if(Test-Path "$root\release-hold.json"){ $hold=Get-Content "$root\release-hold.json" -Raw | ConvertFrom-Json; if($hold.Active){throw $hold.Reason} }
$msbuild=& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -find MSBuild\**\Bin\MSBuild.exe
& $msbuild "$root\MWCriterionDrift.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
if($LASTEXITCODE){throw 'ASI build failed'}
& $msbuild "$root\MWCriterionDrift.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:TestHarness=true /v:minimal /nologo
if($LASTEXITCODE){throw 'Test build failed'}
Push-Location (Split-Path $root)
try { & "$root\bin\ControllerTests.exe" } finally { Pop-Location }
if($LASTEXITCODE){throw 'Controller tests failed'}

foreach($project in @('HudTests','ConfigCheck','PresentTests')){
 & $msbuild "$root\$project.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
 if($LASTEXITCODE){throw "$project build failed"}
}
& "$root\bin\ConfigCheck.exe" "$root\config"
if($LASTEXITCODE){throw 'Configuration validation failed'}
New-Item -ItemType Directory -Force "$root\evidence\config-hud" | Out-Null
& "$root\bin\HudTests.exe" "$root\evidence\config-hud"
if($LASTEXITCODE){throw 'HUD validation failed'}

& "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe" /nologo /target:winexe "/out:$root\bin\Configurator.exe" /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll /r:System.Core.dll "$root\editor\Configurator.cs"
if($LASTEXITCODE){throw 'Editor build failed'}
Copy-Item "$root\editor\fields.json" "$root\bin\fields.json" -Force
$fixture=Join-Path $root ('evidence\editor-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path "$fixture\vehicles\SVJ" -Force | Out-Null
Copy-Item "$root\config\default_*.json" $fixture
Copy-Item "$root\config\settings.json" $fixture
Copy-Item "$root\config\default_drift.json" "$fixture\vehicles\SVJ\drift.json"
Copy-Item "$root\config\default_camera.json" "$fixture\vehicles\SVJ\camera.json"
$p=Start-Process "$root\bin\Configurator.exe" -ArgumentList @(('"'+$fixture+'"'),'--self-test') -WindowStyle Hidden -PassThru
if(!$p.WaitForExit(30000)){if((Get-Process -Id $p.Id).Path -eq "$root\bin\Configurator.exe"){Stop-Process -Id $p.Id};throw 'Editor test timeout'}
if($p.ExitCode){Get-Content "$fixture\editor-error.txt";throw 'Editor tests failed'}
Get-Content "$fixture\editor-test-result.txt"
Set-Content "$root\evidence\editor-fixture-path.txt" $fixture

& "$root\bin\PresentTests.exe"
if($LASTEXITCODE){throw 'HUD dispatch tests failed'}
