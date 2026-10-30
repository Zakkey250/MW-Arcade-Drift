$ErrorActionPreference='Stop'
$msbuild=& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -find MSBuild\**\Bin\MSBuild.exe
& $msbuild "$PSScriptRoot\MWArcadeDriftAI.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
if($LASTEXITCODE){throw 'AI addon build failed'}
& $msbuild "$PSScriptRoot\MWArcadeDriftAI.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:AITests=true /v:minimal /nologo
if($LASTEXITCODE){throw 'AI tests build failed'}
& "$PSScriptRoot\bin\AITests.exe"
if($LASTEXITCODE){throw 'AI tests failed'}
$e=Join-Path (Split-Path $PSScriptRoot) ('evidence\ai-abi-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $e | Out-Null
& "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe" /nologo /platform:x86 /target:exe "/out:$e\NativeSmoke.exe" "$PSScriptRoot\NativeSmoke.cs"
if($LASTEXITCODE){throw 'AI smoke host build failed'}
foreach($setting in @('off','missing','invalid','on')){
 $case=Join-Path $e $setting;New-Item -ItemType Directory -Path $case | Out-Null
 Copy-Item -LiteralPath "$PSScriptRoot\bin\MWArcadeDriftAI.asi" -Destination $case
 $value=if($setting -eq 'missing'){''}elseif($setting -eq 'invalid'){"Logging=invalid`n"}elseif($setting -eq 'on'){"Logging=1`n"}else{"Logging=0`n"}
 [IO.File]::WriteAllText((Join-Path $case 'MWArcadeDriftAI.ini'),"[Diagnostics]`n$value",[Text.UTF8Encoding]::new($false))
 $p=Start-Process -FilePath "$e\NativeSmoke.exe" -ArgumentList @(('"'+$case+'\MWArcadeDriftAI.asi"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$case\result.txt" -RedirectStandardError "$case\error.txt"
 if(!$p.WaitForExit(10000)){if((Get-Process -Id $p.Id).Path -eq "$e\NativeSmoke.exe"){Stop-Process -Id $p.Id};throw 'AI ABI test timeout'}
 if($p.ExitCode){Get-Content "$case\error.txt";throw 'AI ABI test failed'}
 if((Test-Path "$case\MWArcadeDriftAI.log") -ne ($setting -eq 'on')){throw 'AI diagnostic OFF/ON failure'}
 Get-Content "$case\result.txt";Write-Output "PASS AI logging=$setting"
}
Set-Content (Join-Path (Split-Path $PSScriptRoot) 'evidence/latest-ai-abi.txt') $e
