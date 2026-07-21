$ErrorActionPreference = 'Stop'

$ProjectDir = $PSScriptRoot
$RunningOnWindows = $env:OS -eq 'Windows_NT'
$Clean = $false
$Platform = 'host'
$RunTests = $false
$RunAfter = $false
$Release = $false
$Developer = $false
$WindowsBuild = $false

function Show-Banner {
    Write-Host @'

 /$$      /$$ /$$$$$$       /$$$$$$   /$$$$$$
| $$$    /$$$|_  $$_/      /$$__  $$ /$$__  $$
| $$$$  /$$$$  | $$       |__/  \ $$|__/  \ $$
| $$ $$/$$ $$  | $$ /$$$$$$ /$$$$$$/   /$$$$$/
| $$  $$$| $$  | $$|______//$$____/   |___  $$
| $$\  $ | $$  | $$       | $$       /$$  \ $$
| $$ \/  | $$ /$$$$$$     | $$$$$$$$|  $$$$$$/
|__/     |__/|______/     |________/ \______/

             MI-23 BUILD SYSTEM

'@
}

function Show-Help {
    Write-Host 'Usage: .\build.ps1 [options]'
    Write-Host ''
    Write-Host 'Run .\build.ps1 with no options for interactive mode.'
    Write-Host ''
    Write-Host 'Options:'
    Write-Host '  --clean'
    Write-Host '  --platform=host'
    Write-Host '  --platform=windows'
    Write-Host '  --platform=rp2350'
    Write-Host '  --test'
    Write-Host '  --run'
    Write-Host '  --release'
    Write-Host '  --developer'
    Write-Host '  --dev'
    Write-Host '  --help'
}

function Show-InteractiveMenu {
    Write-Host 'What do you want to build?'
    Write-Host '  1) Host simulator developer build'
    Write-Host '  2) Host simulator developer build and run'
    Write-Host '  3) Run unit tests'
    Write-Host '  4) RP2350 developer firmware'
    Write-Host '  5) Host simulator release build'
    Write-Host '  6) RP2350 release firmware'
    Write-Host '  7) Windows simulator'
    Write-Host ''

    $choice = Read-Host 'Choose an option [1-7]'
    switch ($choice) {
        '1' { $script:Platform = 'host'; $script:Developer = $true }
        '2' { $script:Platform = 'host'; $script:Developer = $true; $script:RunAfter = $true }
        '3' { $script:Platform = 'host'; $script:RunTests = $true }
        '4' { $script:Platform = 'rp2350'; $script:Developer = $true }
        '5' { $script:Platform = 'host'; $script:Release = $true }
        '6' { $script:Platform = 'rp2350'; $script:Release = $true }
        '7' { $script:Platform = 'windows'; $script:WindowsBuild = $true }
        default { throw 'Invalid option.' }
    }

    $cleanChoice = Read-Host 'Clean build folder first? [y/N]'
    if ($cleanChoice -match '^(?i:y|yes)$') {
        $script:Clean = $true
    }
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory)]
        [string] $Executable,

        [Parameter(ValueFromRemainingArguments)]
        [string[]] $Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Executable $($Arguments -join ' ')"
    }
}

function Find-RequiredCommand {
    param([Parameter(Mandatory)][string] $Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command '$Name' was not found in PATH."
    }
    return $command.Source
}

function Find-NinjaCommand {
    $command = Get-Command 'ninja' -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs/CLion/bin/ninja/win/x64/ninja.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'),
        (Join-Path $env:ProgramFiles 'CMake/bin/ninja.exe')
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $wingetNinja = Get-ChildItem (Join-Path $env:LOCALAPPDATA 'Microsoft/WinGet/Packages') `
        -Recurse -Filter 'ninja.exe' -File -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($wingetNinja) {
        return $wingetNinja.FullName
    }

    throw "Ninja is required for RP2350 builds but was not found. Install it with 'winget install Ninja-build.Ninja' and reopen PowerShell."
}

function Find-PicoSdkPath {
    $candidates = @(
        $env:PICO_SDK_PATH,
        'C:\Pico\pico-sdk',
        (Join-Path $HOME 'pico-sdk')
    ) | Where-Object { $_ }

    foreach ($candidate in $candidates) {
        $resolvedCandidate = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path -LiteralPath (Join-Path $resolvedCandidate 'external/pico_sdk_import.cmake')) {
            return $resolvedCandidate
        }
    }

    throw "PICO_SDK_PATH is not set and the Pico SDK was not found in a standard location. Set `$env:PICO_SDK_PATH to your pico-sdk directory."
}

function Find-PicoToolchainPath {
    $candidates = @($env:PICO_TOOLCHAIN_PATH)
    $armToolchains = Get-ChildItem ${env:ProgramFiles(x86)} `
        -Directory -Filter 'Arm GNU Toolchain arm-none-eabi' -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue } |
        Sort-Object Name -Descending
    $candidates += $armToolchains.FullName

    foreach ($candidate in ($candidates | Where-Object { $_ })) {
        $resolvedCandidate = [System.IO.Path]::GetFullPath($candidate)
        if ((Test-Path -LiteralPath (Join-Path $resolvedCandidate 'bin/arm-none-eabi-gcc.exe')) -or
            (Test-Path -LiteralPath (Join-Path $resolvedCandidate 'arm-none-eabi-gcc.exe'))) {
            return $resolvedCandidate
        }
    }

    throw "The Arm GNU embedded toolchain was not found. Install it with 'winget install Arm.GnuArmEmbeddedToolchain' and reopen PowerShell."
}

function Initialize-VisualStudioEnvironment {
    if (-not $RunningOnWindows -or (Get-Command 'cl.exe' -ErrorAction SilentlyContinue)) {
        return
    }

    $candidates = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio/2022/Community/Common7/Tools/VsDevCmd.bat')
    )
    $vsDevCmd = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $vsDevCmd) {
        throw 'Visual Studio C++ build tools are required to build the Pico SDK host-side picotool helper.'
    }

    Write-Host "Initializing Visual Studio build environment: $vsDevCmd"
    $commandLine = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    $environmentLines = & $env:ComSpec /d /c $commandLine
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio environment initialization failed with exit code $LASTEXITCODE."
    }

    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

Show-Banner

if ($args.Count -eq 0) {
    Show-InteractiveMenu
} else {
    foreach ($argument in $args) {
        switch -Regex ($argument) {
            '^--clean$' { $Clean = $true; continue }
            '^--platform=(.+)$' { $Platform = $Matches[1]; continue }
            '^--test$' { $RunTests = $true; $Platform = 'host'; continue }
            '^--run$' { $RunAfter = $true; continue }
            '^--release$' { $Release = $true; continue }
            '^--(?:developer|dev)$' { $Developer = $true; continue }
            '^--help$' { Show-Help; exit 0 }
            default {
                Write-Error "Unknown option: $argument" -ErrorAction Continue
                Show-Help
                exit 1
            }
        }
    }
}

if ($Platform -eq 'win') {
    $Platform = 'windows'
}
if ($Platform -eq 'windows') {
    $WindowsBuild = $true
}

if ($Release -and $Developer) {
    throw '--release and --developer cannot be used together.'
}
if ($RunTests -and $WindowsBuild) {
    throw 'Tests only work for native host builds.'
}
if ($RunTests -and $Release) {
    throw '--test uses a host test build, not a release build.'
}

if ($WindowsBuild) {
    $BuildDir = Join-Path $ProjectDir 'build-win'
} elseif ($Release -and $Platform -eq 'host') {
    $BuildDir = Join-Path $ProjectDir 'build-host-release'
} elseif ($Release -and $Platform -eq 'rp2350') {
    $BuildDir = Join-Path $ProjectDir 'build-rp2350-release'
} elseif ($Developer) {
    $BuildDir = Join-Path $ProjectDir "build-$Platform-dev"
} else {
    $BuildDir = Join-Path $ProjectDir "build-$Platform"
}

$DeveloperOptions = if ($Developer) { 'ON' } else { 'OFF' }
$BuildType = if ($Release) { 'Release' } else { 'Debug' }
$BuildLabel = if ($Developer) { 'developer' } elseif ($Release) { 'release' } else { 'standard' }
$ReleaseOption = if ($Release) { 'ON' } else { 'OFF' }

$HostExecutableName = if ($RunningOnWindows) { 'mi23.exe' } else { 'mi23' }
$HostBinary = Join-Path $BuildDir "firmware/platform/host/sdl_simulator/$HostExecutableName"
$WindowsBinary = Join-Path $BuildDir 'firmware/platform/host/sdl_simulator/mi23.exe'
$HostTestDir = Join-Path $BuildDir 'tests'
$Rp2350OutputDir = Join-Path $BuildDir 'firmware/platform/rp2350'
$Rp2350Uf2 = Join-Path $Rp2350OutputDir 'mi23.uf2'
$Rp2350Elf = Join-Path $Rp2350OutputDir 'mi23.elf'
$Rp2350Bin = Join-Path $Rp2350OutputDir 'mi23.bin'

if ($Clean) {
    $resolvedProjectDir = [System.IO.Path]::GetFullPath($ProjectDir).TrimEnd('\', '/')
    $resolvedBuildDir = [System.IO.Path]::GetFullPath($BuildDir).TrimEnd('\', '/')
    $expectedPrefix = $resolvedProjectDir + [System.IO.Path]::DirectorySeparatorChar + 'build-'
    if (-not $resolvedBuildDir.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean unexpected path: $resolvedBuildDir"
    }

    if (Test-Path -LiteralPath $resolvedBuildDir) {
        Write-Host "Cleaning $resolvedBuildDir..."
        Remove-Item -LiteralPath $resolvedBuildDir -Recurse -Force
    }
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

$Cmake = Find-RequiredCommand 'cmake'
$cachePath = Join-Path $BuildDir 'CMakeCache.txt'
if ($Platform -eq 'rp2350' -and (Test-Path -LiteralPath $cachePath)) {
    $generatorEntry = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_GENERATOR:INTERNAL=' |
        Select-Object -First 1
    if ($generatorEntry) {
        $cachedGenerator = ($generatorEntry.Line -split '=', 2)[1]
        if ($cachedGenerator -ne 'Ninja') {
            throw "The existing RP2350 build directory uses the incompatible '$cachedGenerator' generator. Rerun with --clean so it can be configured with Ninja."
        }
    }
}

$hasConfiguredGenerator = (Test-Path -LiteralPath (Join-Path $BuildDir 'Makefile')) -or
                          (Test-Path -LiteralPath (Join-Path $BuildDir 'build.ninja')) -or
                          (@(Get-ChildItem -LiteralPath $BuildDir -Include '*.sln', '*.slnx' -File -ErrorAction SilentlyContinue).Count -gt 0)
if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt')) -or -not $hasConfiguredGenerator) {
    Write-Host "Configuring CMake for platform: $Platform"

    if ($WindowsBuild) {
        Invoke-NativeCommand $Cmake `
            '-S' $ProjectDir `
            '-B' $BuildDir `
            '-DPLATFORM=host' `
            "-DCMAKE_TOOLCHAIN_FILE=$ProjectDir/cmake/toolchains/mingw64.cmake" `
            '-DCMAKE_BUILD_TYPE=Release' `
            '-DBUILD_TESTING=OFF' `
            '-DMI23_ENABLE_DEVELOPER_OPTIONS=OFF'
    } elseif ($Platform -eq 'host') {
        $configureArguments = @(
            '-S', $ProjectDir,
            '-B', $BuildDir,
            '-DPLATFORM=host',
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DBUILD_RELEASE=$ReleaseOption",
            "-DMI23_ENABLE_DEVELOPER_OPTIONS=$DeveloperOptions"
        )

        if ($RunningOnWindows) {
            $toolchainFile = $env:CMAKE_TOOLCHAIN_FILE
            if (-not $toolchainFile -and (Test-Path -LiteralPath 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake')) {
                $toolchainFile = 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
            }
            if (-not $toolchainFile -and $env:VCPKG_ROOT) {
                $toolchainFile = Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
            }
            if ($toolchainFile -and (Test-Path -LiteralPath $toolchainFile)) {
                Write-Host "Using vcpkg toolchain: $toolchainFile"
                $configureArguments += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
            }
        }

        Invoke-NativeCommand $Cmake @configureArguments
    } else {
        $configureArguments = @(
            '-S', $ProjectDir,
            '-B', $BuildDir,
            "-DPLATFORM=$Platform",
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DMI23_ENABLE_DEVELOPER_OPTIONS=$DeveloperOptions"
        )
        if ($Platform -eq 'rp2350') {
            Initialize-VisualStudioEnvironment
            $ninja = Find-NinjaCommand
            $picoSdkPath = Find-PicoSdkPath
            $picoToolchainPath = Find-PicoToolchainPath
            Write-Host "Using Ninja for the RP2350 cross-build: $ninja"
            Write-Host "Using Pico SDK: $picoSdkPath"
            Write-Host "Using Arm toolchain: $picoToolchainPath"
            $configureArguments += @(
                '-G', 'Ninja',
                "-DCMAKE_MAKE_PROGRAM=$ninja",
                "-DPICO_SDK_PATH=$picoSdkPath",
                "-DPICO_TOOLCHAIN_PATH=$picoToolchainPath"
            )
        }
        Invoke-NativeCommand $Cmake @configureArguments
    }
}

$multiConfig = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_CONFIGURATION_TYPES:' -Quiet
if ($multiConfig -and $Platform -eq 'host') {
    $HostBinary = Join-Path $BuildDir "firmware/platform/host/sdl_simulator/$BuildType/$HostExecutableName"
}

Write-Host "Building MI-23 ($Platform, $BuildLabel)..."
$buildArguments = @('--build', $BuildDir, '--parallel', [Environment]::ProcessorCount.ToString())
if ($multiConfig) {
    $buildArguments += @('--config', $BuildType)
}
Invoke-NativeCommand $Cmake @buildArguments

if ($WindowsBuild) {
    if (-not (Test-Path -LiteralPath $WindowsBinary)) {
        throw "Windows binary not found at $WindowsBinary"
    }

    $stripCommand = Get-Command 'x86_64-w64-mingw32-strip' -ErrorAction SilentlyContinue
    if ($stripCommand) {
        Invoke-NativeCommand $stripCommand.Source $WindowsBinary
    }

    Write-Host ''
    Write-Host 'Windows build successful:'
    Write-Host "  $WindowsBinary"
    exit 0
}

if ($Release) {
    if ($Platform -eq 'host') {
        if (-not (Test-Path -LiteralPath $HostBinary)) {
            throw "Release binary not found at $HostBinary"
        }

        $stripCommand = Get-Command 'strip' -ErrorAction SilentlyContinue
        if ($stripCommand) {
            Invoke-NativeCommand $stripCommand.Source $HostBinary
        }

        $releaseName = if ($RunningOnWindows) {
            'mi23-windows-x86_64.exe'
        } else {
            'mi23-linux-x86_64'
        }
        $releasePath = Join-Path $ProjectDir $releaseName
        Copy-Item -LiteralPath $HostBinary -Destination $releasePath -Force

        Write-Host ''
        Write-Host 'Host release binary ready:'
        Write-Host "  $releasePath"
        Write-Host 'Developer Options: disabled'
    } elseif ($Platform -eq 'rp2350') {
        if (-not (Test-Path -LiteralPath $Rp2350Uf2)) {
            throw "RP2350 release firmware not found at $Rp2350Uf2"
        }

        Write-Host ''
        Write-Host 'RP2350 release firmware ready:'
        Write-Host "  UF2: $Rp2350Uf2"
        Write-Host "  ELF: $Rp2350Elf"
        Write-Host "  BIN: $Rp2350Bin"
        Write-Host 'Developer Options: disabled'
    }
    exit 0
}

Write-Host ''
Write-Host 'Build successful.'
$developerStatus = if ($Developer) { 'enabled' } else { 'disabled' }
Write-Host "Developer Options: $developerStatus"

if ($Platform -eq 'rp2350') {
    Write-Host ''
    Write-Host "RP2350 $BuildLabel firmware outputs:"
    Write-Host "  UF2: $Rp2350Uf2"
    Write-Host "  ELF: $Rp2350Elf"
    Write-Host "  BIN: $Rp2350Bin"
}

if ($RunTests) {
    Write-Host ''
    Write-Host 'Running unit tests...'
    $Ctest = Find-RequiredCommand 'ctest'
    $testArguments = @('--test-dir', $HostTestDir, '--output-on-failure')
    if ($multiConfig) {
        $testArguments += @('-C', $BuildType)
    }
    Invoke-NativeCommand $Ctest @testArguments
}

if ($RunAfter) {
    if ($Platform -ne 'host') {
        Write-Warning '--run only works with host builds.'
    } else {
        Invoke-NativeCommand $HostBinary
    }
}
