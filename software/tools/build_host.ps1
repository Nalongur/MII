[CmdletBinding()]
param(
    [switch]$Offline
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vectorPath = Join-Path $repositoryRoot 'protocol\test-vectors\device_status_request_v1.json'
$cppSource = Join-Path $repositoryRoot 'protocol\host-tests\cpp\protocol_vector_test.cpp'
$hostBuildDirectory = Join-Path $repositoryRoot 'build\host\cpp'
$cppExecutable = Join-Path $hostBuildDirectory 'protocol_vector_test.exe'
$nanopbGeneratorScript = Join-Path $repositoryRoot 'tools\generate_nanopb.ps1'
$nanopbTestSource = Join-Path $repositoryRoot 'protocol\host-tests\nanopb\nanopb_vector_test.cpp'
$nanopbRuntimeDirectory = Join-Path $repositoryRoot 'third_party\nanopb'
$nanopbGeneratedDirectory = Join-Path $repositoryRoot 'build\host\nanopb'
$nanopbCommittedDirectory = Join-Path $repositoryRoot 'firmware\components\mii_protocol\generated'
$protocolComponentDirectory = Join-Path $repositoryRoot 'firmware\components\mii_protocol'
$protocolIncludeDirectory = Join-Path $protocolComponentDirectory 'include'
$sessionTestSource = Join-Path $repositoryRoot 'protocol\host-tests\session\session_state_test.cpp'
$sessionExecutable = Join-Path $repositoryRoot 'build\host\session_state_test.exe'
$frameVectorPath = Join-Path $repositoryRoot 'protocol\test-vectors\framed_device_status_request_v1.json'
$frameTestSource = Join-Path $repositoryRoot 'protocol\host-tests\frame\frame_codec_test.cpp'
$frameExecutable = Join-Path $repositoryRoot 'build\host\frame_codec_test.exe'
$ingressTestSource = Join-Path $repositoryRoot 'protocol\host-tests\frame\frame_ingress_test.cpp'
$ingressExecutable = Join-Path $repositoryRoot 'build\host\frame_ingress_test.exe'
$bleComponentDirectory = Join-Path $repositoryRoot 'firmware\components\mii_ble'
$bleIncludeDirectory = Join-Path $bleComponentDirectory 'include'
$bleRxTestSource = Join-Path $repositoryRoot 'protocol\host-tests\ble\ble_rx_session_test.cpp'
$bleRxExecutable = Join-Path $repositoryRoot 'build\host\ble_rx_session_test.exe'
$bleTxTestSource = Join-Path $repositoryRoot 'protocol\host-tests\ble\ble_tx_session_test.cpp'
$bleTxExecutable = Join-Path $repositoryRoot 'build\host\ble_tx_session_test.exe'
$nanopbObjectDirectory = Join-Path $repositoryRoot 'build\host\nanopb-objects'
$nanopbExecutable = Join-Path $repositoryRoot 'build\host\nanopb_vector_test.exe'
$kotlinProject = Join-Path $repositoryRoot 'protocol\host-tests\kotlin'
$authComponentDirectory = Join-Path $repositoryRoot 'firmware\components\mii_auth'
$authTestSource = Join-Path $repositoryRoot 'protocol\host-tests\auth\owner_session_test.cpp'
$authExecutable = Join-Path $repositoryRoot 'build\host\owner_session_test.exe'
$authVectorPath = Join-Path $repositoryRoot 'protocol\test-vectors\authentication_v1.json'

function Assert-LastExitCode {
    param([Parameter(Mandatory)][string]$Step)
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE"
    }
}

Write-Host '== Static repository baseline =='
& python (Join-Path $repositoryRoot 'tools\validate_baseline.py')
Assert-LastExitCode 'Python baseline validation'

$zigCommand = Get-Command zig -ErrorAction SilentlyContinue
if ($null -ne $zigCommand) {
    $zigExecutable = $zigCommand.Source
} else {
    $wingetPackageRoot = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
    $zigPackage = Get-ChildItem -LiteralPath $wingetPackageRoot -Directory -Filter 'zig.zig_*' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -ne $zigPackage) {
        $zigExecutable = Get-ChildItem -LiteralPath $zigPackage.FullName `
            -Filter zig.exe -File -Recurse |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $zigExecutable) {
    throw 'Zig was not found in PATH or the current-user WinGet package directory.'
}

New-Item -ItemType Directory -Force -Path $hostBuildDirectory | Out-Null
Write-Host '== C++ protocol vector build =='
& $zigExecutable version
Assert-LastExitCode 'Zig version query'
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    -o $cppExecutable $cppSource
Assert-LastExitCode 'C++ compilation'
& $cppExecutable $vectorPath
Assert-LastExitCode 'C++ protocol vector execution'

Write-Host '== Firmware session state build =='
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$protocolIncludeDirectory" `
    -o $sessionExecutable $sessionTestSource `
    (Join-Path $protocolComponentDirectory 'session_state.cpp')
Assert-LastExitCode 'firmware session state compilation'
& $sessionExecutable
Assert-LastExitCode 'firmware session state execution'

Write-Host '== Firmware streaming frame build =='
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$protocolIncludeDirectory" `
    -o $frameExecutable $frameTestSource `
    (Join-Path $protocolComponentDirectory 'frame_codec.cpp')
Assert-LastExitCode 'firmware streaming frame compilation'
& $frameExecutable $frameVectorPath
Assert-LastExitCode 'firmware streaming frame execution'

Write-Host '== Firmware transport ingress build =='
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$protocolIncludeDirectory" `
    -o $ingressExecutable $ingressTestSource `
    (Join-Path $protocolComponentDirectory 'frame_codec.cpp') `
    (Join-Path $protocolComponentDirectory 'frame_ingress.cpp')
Assert-LastExitCode 'firmware transport ingress compilation'
& $ingressExecutable
Assert-LastExitCode 'firmware transport ingress execution'

Write-Host '== BLE receive session build =='
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$protocolIncludeDirectory" "-I$bleIncludeDirectory" `
    -o $bleRxExecutable $bleRxTestSource `
    (Join-Path $protocolComponentDirectory 'frame_codec.cpp') `
    (Join-Path $protocolComponentDirectory 'frame_ingress.cpp') `
    (Join-Path $bleComponentDirectory 'ble_rx_session.cpp')
Assert-LastExitCode 'BLE receive session compilation'
& $bleRxExecutable
Assert-LastExitCode 'BLE receive session execution'

Write-Host '== BLE notification transmit session build =='
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$protocolIncludeDirectory" "-I$bleIncludeDirectory" `
    -o $bleTxExecutable $bleTxTestSource `
    (Join-Path $bleComponentDirectory 'ble_tx_session.cpp')
Assert-LastExitCode 'BLE notification transmit session compilation'
& $bleTxExecutable
Assert-LastExitCode 'BLE notification transmit session execution'

Write-Host '== Owner binding and authenticated session build =='
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$(Join-Path $authComponentDirectory 'include')" "-I$protocolIncludeDirectory" `
    -o $authExecutable $authTestSource `
    (Join-Path $authComponentDirectory 'owner_session.cpp') `
    (Join-Path $protocolComponentDirectory 'session_state.cpp') -lbcrypt
Assert-LastExitCode 'owner session compilation'
& $authExecutable $authVectorPath
Assert-LastExitCode 'owner session execution'
& powershell -ExecutionPolicy Bypass -File (Join-Path $repositoryRoot 'tools\test_auth_platform.ps1')
Assert-LastExitCode 'auth platform host tests'

Write-Host '== nanopb generated protocol vector build =='
& powershell -ExecutionPolicy Bypass -File $nanopbGeneratorScript
Assert-LastExitCode 'nanopb source generation'
New-Item -ItemType Directory -Force -Path $nanopbObjectDirectory | Out-Null
$nanopbCSources = @(
    (Join-Path $nanopbRuntimeDirectory 'pb_common.c'),
    (Join-Path $nanopbRuntimeDirectory 'pb_encode.c'),
    (Join-Path $nanopbRuntimeDirectory 'pb_decode.c'),
    (Join-Path $nanopbCommittedDirectory 'mii.pb.c')
)
$nanopbObjects = @()
foreach ($source in $nanopbCSources) {
    $object = Join-Path $nanopbObjectDirectory ((Split-Path $source -Leaf) + '.obj')
    & $zigExecutable cc -std=c11 -Wall -Wextra -Wpedantic `
        "-I$nanopbRuntimeDirectory" "-I$nanopbGeneratedDirectory" `
        -c $source -o $object
    Assert-LastExitCode "nanopb C compilation: $source"
    $nanopbObjects += $object
}
$protocolCppSources = @(
    (Join-Path $protocolComponentDirectory 'frame_codec.cpp'),
    (Join-Path $protocolComponentDirectory 'protocol_codec.cpp'),
    (Join-Path $protocolComponentDirectory 'session_state.cpp'),
    (Join-Path $protocolComponentDirectory 'protocol_self_test.cpp')
)
foreach ($source in $protocolCppSources) {
    $object = Join-Path $nanopbObjectDirectory ((Split-Path $source -Leaf) + '.obj')
    & $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
        -Wno-nullability-completeness -O0 -g `
        "-I$protocolIncludeDirectory" "-I$nanopbRuntimeDirectory" `
        "-I$nanopbCommittedDirectory" -c $source -o $object
    Assert-LastExitCode "firmware protocol C++ compilation: $source"
    $nanopbObjects += $object
}
& $zigExecutable c++ -std=c++17 -Wall -Wextra -Wpedantic `
    -Wno-nullability-completeness -O0 -g `
    "-I$protocolIncludeDirectory" "-I$nanopbRuntimeDirectory" `
    "-I$nanopbCommittedDirectory" `
    -o $nanopbExecutable $nanopbTestSource @nanopbObjects
Assert-LastExitCode 'nanopb C++ test compilation'
& $nanopbExecutable $vectorPath
Assert-LastExitCode 'nanopb generated protocol vector execution'

$localGradleWrapper = Join-Path $kotlinProject 'gradlew.bat'
if (Test-Path -LiteralPath $localGradleWrapper) {
    $gradleExecutable = $localGradleWrapper
} else {
    $gradleCommand = Get-Command gradle -ErrorAction SilentlyContinue
    if ($null -ne $gradleCommand) {
        $gradleExecutable = $gradleCommand.Source
    } else {
        $gradleDistributionRoot = Join-Path $env:USERPROFILE '.gradle\wrapper\dists'
        $gradleExecutable = Get-ChildItem -LiteralPath $gradleDistributionRoot `
            -Filter gradle.bat -File -Recurse |
            Sort-Object FullName -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }
}
if (-not $gradleExecutable) {
    throw 'Gradle was not found in PATH, the project wrapper, or the user wrapper cache.'
}

Write-Host '== Kotlin/generated-protobuf protocol vector build =='
$gradleArguments = @('--no-daemon', '-p', $kotlinProject)
if ($Offline) {
    $gradleArguments += '--offline'
}
$gradleArguments += 'check'
& $gradleExecutable @gradleArguments
Assert-LastExitCode 'Kotlin Gradle build'

Write-Host 'HOST_BUILD_PASS: static baseline, session state, streaming framing, transport ingress, BLE receive/transmit, owner authentication, independent C++, nanopb, and Kotlin generated vectors'
Write-Host 'Boundary: this is not ESP-IDF firmware, Android APK, flash, or physical-board validation.'
