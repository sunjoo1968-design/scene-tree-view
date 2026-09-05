[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
            "${ProjectRoot}/release/${ProductName}-*-windows-*.exe"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    Log-Group "Building installer for ${ProductName}..."

    $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"

    if ( ! ( Test-Path -Path $IsccFile ) ) {
        throw "Inno Setup script not found: ${IsccFile}. Configure the project before packaging."
    }

    $PackageRoot = "${ProjectRoot}/release/Package"
    $Staged = "${ProjectRoot}/release/${Configuration}/${ProductName}"

    Remove-Item -Path $PackageRoot -Recurse -Force -ErrorAction SilentlyContinue

    New-Item -Path "${PackageRoot}/recommended" -ItemType Directory -Force | Out-Null
    Copy-Item -Path $Staged -Destination "${PackageRoot}/recommended/${ProductName}" -Recurse -Force

    New-Item -Path "${PackageRoot}/portable/obs-plugins" -ItemType Directory -Force | Out-Null
    New-Item -Path "${PackageRoot}/portable/data/obs-plugins" -ItemType Directory -Force | Out-Null
    Copy-Item -Path "${Staged}/bin/64bit" -Destination "${PackageRoot}/portable/obs-plugins/64bit" -Recurse -Force
    Copy-Item -Path "${Staged}/data" -Destination "${PackageRoot}/portable/data/obs-plugins/${ProductName}" -Recurse -Force

    #
    $IsccArgs = @(
        "${IsccFile}"
        "/O${ProjectRoot}/release"
        "/F${OutputName}-Installer"
        "/DPackageDir=${PackageRoot}"
    )

    Invoke-External iscc @IsccArgs
    Log-Group
}

Package
