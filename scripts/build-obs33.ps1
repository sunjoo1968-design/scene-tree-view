param(
 [Parameter(Mandatory=$true)][string]$ObsPath,
 [Parameter(Mandatory=$true)][string]$ObsSourceDir,
 [string]$QtPath = '',
 [switch]$BuildHub
)
$ErrorActionPreference='Stop'
$linkRoot=Split-Path $PSScriptRoot -Parent
$linkImports=Join-Path $linkRoot '.local/obs33/imports'
New-Item -ItemType Directory -Force $linkImports | Out-Null
$linkVersion=(Get-Item -LiteralPath "$ObsPath/bin/64bit/obs.dll").VersionInfo.FileVersion
if($linkVersion -notmatch '^33\.'){throw 'OBS 33 Windows x64 required'}
if(-not (Test-Path -LiteralPath "$ObsSourceDir/libobs/obs.h")){throw 'Official OBS33 source SDK required'}
$linkVs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$linkToolset=Get-ChildItem "$linkVs/VC/Tools/MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$linkTools=Join-Path $linkToolset.FullName 'bin/Hostx64/x64'
foreach($linkModule in @('obs','obs-frontend-api')){
 $linkExports=& "$linkTools/dumpbin.exe" /nologo /exports "$ObsPath/bin/64bit/$linkModule.dll"
 if($LASTEXITCODE -ne 0){throw 'Export inspection failed'}
 $linkNames=@($linkExports | ForEach-Object {if($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)'){$Matches[1]}})
 if($linkNames.Count -lt 10){throw 'Invalid exports'}
 @("LIBRARY $linkModule.dll",'EXPORTS')+$linkNames | Set-Content "$linkImports/$linkModule.def" -Encoding ascii
 & "$linkTools/lib.exe" /nologo "/def:$linkImports/$linkModule.def" /machine:x64 "/out:$linkImports/$linkModule.lib"
 if($LASTEXITCODE -ne 0){throw 'Import library failed'}
}
if(Test-Path "$linkRoot/scripts/camera-mix-hybrid.lua"){$linkSource="$linkRoot/native/camera-mix-hybrid";$linkBuild="$linkRoot/.local/obs33/controller-build"}
elseif(Test-Path "$linkRoot/source/hub/package.json"){$linkSource="$linkRoot/source/obs-tally-bridge";$linkBuild="$linkRoot/.build-tools/obs33-bridge"}
elseif(Test-Path "$linkRoot/buildspec.json"){$linkSource="$linkRoot/cmake/obs33";$linkBuild="$linkRoot/build-obs33"}
else{$linkSource=$linkRoot;$linkBuild="$linkRoot/build-obs33"}
$linkArgs=@('-S',$linkSource,'-B',$linkBuild,'-G','Visual Studio 17 2022','-A','x64',"-DOBS_SOURCE_DIR=$ObsSourceDir","-DOBS_IMPORT_DIR=$linkImports")
if($QtPath){$linkArgs+="-DCMAKE_PREFIX_PATH=$QtPath"}
cmake @linkArgs
if($LASTEXITCODE -ne 0){throw 'OBS33 configure failed'}
cmake --build $linkBuild --config Release --parallel 2
if($LASTEXITCODE -ne 0){throw 'OBS33 build failed'}
if($BuildHub -and (Test-Path "$linkRoot/source/hub/package.json")){
 & "$linkRoot/scripts/build-obs-compat.ps1" -ObsPath $ObsPath -ObsSourceDir $ObsSourceDir -ObsBuildDir $linkBuild -SkipDependencies
}
