param([string]$LibraryPath)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$dll=if($LibraryPath){(Resolve-Path -LiteralPath $LibraryPath).ProviderPath}else{Join-Path $root 'bin\MWArcadeDrift.asi'}
$evidence=Join-Path $root ('evidence\mode-abi-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$probe=Join-Path $evidence 'NativeModeSmoke.exe'
& "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe" /nologo /platform:x86 /target:exe "/out:$probe" "$root\tests\NativeModeSmoke.cs"
if($LASTEXITCODE){throw 'Native mode ABI probe build failed'}
foreach($setting in @('off','missing','invalid','on')){
 $directory=Join-Path $evidence $setting;New-Item -ItemType Directory -Path $directory | Out-Null
 $copy=Join-Path $directory 'MWArcadeDrift.asi';Copy-Item -LiteralPath $dll -Destination $copy
 $logging=if($setting -eq 'missing'){''}elseif($setting -eq 'invalid'){"Logging=invalid`r`n"}elseif($setting -eq 'on'){"Logging=1`r`n"}else{"Logging=0`r`n"}
 [IO.File]::WriteAllText((Join-Path $directory 'MWArcadeDrift.ini'),"[Diagnostics]`r`n${logging}[Drift]`r`nEnabled=1`r`nTelemetry=1`r`n",[Text.UTF8Encoding]::new($false))
 $stdout=Join-Path $directory 'result.txt';$stderr=Join-Path $directory 'error.txt'
 $process=Start-Process -FilePath $probe -ArgumentList @(('"'+$copy+'"'),$setting) -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
 if(!$process.WaitForExit(15000)){
  $owned=Get-Process -Id $process.Id -ErrorAction SilentlyContinue
  if($owned -and $owned.Path -eq $probe -and $owned.StartTime -eq $process.StartTime){Stop-Process -Id $owned.Id}
  throw 'ABI test timeout'
 }
 Get-Content -LiteralPath $stdout
 Write-Output "PASS diagnostics configuration case=$setting"
 if($process.ExitCode){Get-Content -LiteralPath $stderr;throw 'Native mode ABI test failed'}
}
Set-Content -LiteralPath "$root\evidence\latest-mode-abi.txt" $evidence
Write-Output "PASS no game launch or runtime installation; evidence=$evidence"
