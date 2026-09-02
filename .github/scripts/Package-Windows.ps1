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

    # 安装包不设开关，每次打包都出。同类插件（advanced-scene-switcher / waveform）是用
    # -BuildInstaller 或 tag 条件挡住的，这里故意不挡：.iss 写错只有编译才会发现，挡起来就要
    # 等到发版当天才第一次执行。编译实测 0.5 秒、产物 2 MB，比"发版时才炸"便宜得多。
    # 附带好处是不用碰 package-plugin/action.yaml——那也是上游模板文件，少改一个少一处冲突。
    Log-Group "Building installer for ${ProductName}..."

    $IsccFile = "${ProjectRoot}/build_${Target}/installer-Windows.generated.iss"

    if ( ! ( Test-Path -Path $IsccFile ) ) {
        throw "Inno Setup script not found: ${IsccFile}. Configure the project before packaging."
    }

    # 安装包要带两套布局，让用户既能装进 %ProgramData%\obs-studio\plugins（推荐），也能装进
    # OBS 自己的目录（便携版只认这一套）。两套目录结构不同，由 .iss 的 Check: 按用户选的
    # 目录挑一套装。为什么是这两套、依据哪段 OBS 源码，都写在 installer-Windows.iss.in 顶部。
    $PackageRoot = "${ProjectRoot}/release/Package"
    $Staged = "${ProjectRoot}/release/${Configuration}/${ProductName}"

    Remove-Item -Path $PackageRoot -Recurse -Force -ErrorAction SilentlyContinue

    # 推荐布局：DestDir 是 ...\plugins，所以载荷要自带 <ProductName>\ 这一层。
    New-Item -Path "${PackageRoot}/recommended" -ItemType Directory -Force | Out-Null
    Copy-Item -Path $Staged -Destination "${PackageRoot}/recommended/${ProductName}" -Recurse -Force

    # OBS 目录布局：DestDir 是 OBS 根目录，载荷自带 obs-plugins\ 与 data\ 两层。
    New-Item -Path "${PackageRoot}/portable/obs-plugins" -ItemType Directory -Force | Out-Null
    New-Item -Path "${PackageRoot}/portable/data/obs-plugins" -ItemType Directory -Force | Out-Null
    Copy-Item -Path "${Staged}/bin/64bit" -Destination "${PackageRoot}/portable/obs-plugins/64bit" -Recurse -Force
    Copy-Item -Path "${Staged}/data" -Destination "${PackageRoot}/portable/data/obs-plugins/${ProductName}" -Recurse -Force

    # 裸调 iscc：GitHub 的 windows runner 预装了 Inno Setup 并且在 PATH 上。这不是推断——
    # advanced-scene-switcher 与 waveform 都在 runner 上这么调，且都在持续发出 Installer.exe。
    # 缺失时 PowerShell 自己会报 "The term 'iscc' is not recognized"，足够清楚，不必再包一层。
    #
    # PackageDir 覆盖 .iss 里的默认值：CMake 在配置期不知道多配置生成器最终用的是哪个
    # config，只有这里知道。/F 同理——安装包名跟 zip 名同源，都从 buildspec 推出来。
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
