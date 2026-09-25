param([string]$LibraryPath)
$ErrorActionPreference='Stop'
if([IntPtr]::Size -ne 4){throw 'Run using Windows SysWOW64 WindowsPowerShell (32-bit).'}
$root=Split-Path $PSScriptRoot
$dll=if($LibraryPath){(Resolve-Path -LiteralPath $LibraryPath).Path}else{Join-Path $root 'bin\MWArcadeDrift.asi'}
$log=[IO.Path]::ChangeExtension($dll,'.log')
$before=if(Test-Path $log){(Get-Item $log).Length}else{0}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DriftLoadProbe {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryW(string name);
}
'@
$module=[DriftLoadProbe]::LoadLibraryW($dll)
if($module -eq [IntPtr]::Zero){throw "Load failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"}
$success=$false
for($n=0;$n -lt 50;$n++){
 Start-Sleep -Milliseconds 100
 if((Test-Path $log) -and (Get-Item $log).Length -gt $before){
  $tail=(Get-Content $log -Raw).Substring([int]$before)
  if($tail -match 'REJECTED exe='){$success=$true;break}
 }
}
if(-not $success){throw 'Expected wrong-host rejection not observed.'}
Write-Output 'PASS x86 ASI loads and rejects unsupported host before physics hook installation.'
