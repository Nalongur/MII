[CmdletBinding()]
param([string]$IdfPath)
$ErrorActionPreference = 'Stop'
$miiRepository = Split-Path -Parent $PSScriptRoot
$miiVersions = Get-Content (Join-Path $miiRepository 'versions.json') -Raw | ConvertFrom-Json
if (-not $IdfPath) {
    $IdfPath = Join-Path $env:USERPROFILE ".espressif\frameworks\esp-idf-v$($miiVersions.espIdfVersion)"
}
$miiMbed = Join-Path $IdfPath 'components\mbedtls\mbedtls'
$miiIdfVersion = Get-Content (Join-Path $IdfPath 'tools\cmake\version.cmake') -Raw
$miiActualVersion = (@('MAJOR', 'MINOR', 'PATCH') | ForEach-Object {
    $miiMatch = [regex]::Match($miiIdfVersion, "set\(IDF_VERSION_$_ ([0-9]+)\)")
    if (-not $miiMatch.Success) { throw 'ESP-IDF version metadata is malformed.' }
    $miiMatch.Groups[1].Value
}) -join '.'
if ($miiActualVersion -ne $miiVersions.espIdfVersion) {
    throw "Expected ESP-IDF $($miiVersions.espIdfVersion), got $miiActualVersion."
}
if (-not (Test-Path -LiteralPath (Join-Path $miiMbed 'library\md.c'))) {
    throw 'Pinned ESP-IDF mbedTLS sources are required for the auth platform host tests.'
}
$miiZig = (Get-Command zig -ErrorAction Stop).Source
$miiBuild = Join-Path $miiRepository 'build\host\mbedtls-auth'
New-Item -ItemType Directory -Force $miiBuild | Out-Null
function Assert-MiiExit([string]$step) {
    if ($LASTEXITCODE -ne 0) { throw "$step failed: exit $LASTEXITCODE" }
}
Push-Location $miiRepository
try {
    $miiIncludes = @('-Iprotocol/host-tests/auth/idf-stubs',
        '-Ifirmware/components/mii_auth/include', '-Ifirmware/components/mii_protocol/include')
    $miiCommon = @('-O1', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=undefined')
    $miiCpp = @('-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Wno-nullability-completeness')
    $miiSources = @('firmware/components/mii_auth/owner_session.cpp',
        'firmware/components/mii_protocol/session_state.cpp')
    $miiNumericExe = Join-Path $miiBuild 'numeric_comparison_test.exe'
    & $miiZig c++ @miiCpp @miiCommon @miiIncludes protocol/host-tests/auth/numeric_comparison_test.cpp `
        firmware/components/mii_auth/numeric_comparison.cpp -o $miiNumericExe
    Assert-MiiExit 'Numeric comparison compilation'
    & $miiNumericExe
    Assert-MiiExit 'Numeric comparison host test'
    $miiStoreExe = Join-Path $miiBuild 'owner_store_test.exe'
    & $miiZig c++ @miiCpp @miiCommon @miiIncludes @miiSources `
        protocol/host-tests/auth/owner_store_test.cpp `
        firmware/components/mii_auth/owner_record_codec.cpp `
        firmware/components/mii_auth/idf_owner_store.cpp -o $miiStoreExe
    Assert-MiiExit 'Owner store host compilation'
    & $miiStoreExe protocol/test-vectors/authentication_v1.json
    Assert-MiiExit 'Owner store host test'

    $miiObjects = @()
    foreach ($miiSource in @('md.c', 'sha256.c', 'platform.c', 'platform_util.c', 'constant_time.c')) {
        $miiObject = Join-Path $miiBuild ($miiSource + '.obj')
        & $miiZig cc -std=c11 @miiCommon @miiIncludes "-I$miiMbed/include" "-I$miiMbed/library" `
            -c (Join-Path $miiMbed "library\$miiSource") -o $miiObject
        Assert-MiiExit "mbedTLS host compilation $miiSource"
        $miiObjects += $miiObject
    }
    $miiCryptoExe = Join-Path $miiBuild 'idf_auth_crypto_test.exe'
    & $miiZig c++ @miiCpp @miiCommon @miiIncludes "-I$miiMbed/include" @miiSources `
        protocol/host-tests/auth/idf_auth_crypto_test.cpp `
        firmware/components/mii_auth/idf_auth_crypto.cpp @miiObjects -o $miiCryptoExe
    Assert-MiiExit 'IDF crypto host compilation'
    & $miiCryptoExe protocol/test-vectors/authentication_v1.json
    Assert-MiiExit 'IDF crypto host test'

    $miiWireIncludes = @('-Ithird_party/nanopb', '-Ifirmware/components/mii_protocol/generated')
    $miiWireObjects = @()
    foreach ($miiSource in @('third_party/nanopb/pb_common.c', 'third_party/nanopb/pb_encode.c',
        'third_party/nanopb/pb_decode.c', 'firmware/components/mii_protocol/generated/mii.pb.c')) {
        $miiObject = Join-Path $miiBuild ((Split-Path $miiSource -Leaf) + '.obj')
        & $miiZig cc -std=c11 @miiCommon @miiWireIncludes -c $miiSource -o $miiObject
        Assert-MiiExit "Authentication nanopb compilation $miiSource"
        $miiWireObjects += $miiObject
    }
    $miiWireExe = Join-Path $miiBuild 'auth_wire_test.exe'
    & $miiZig c++ @miiCpp @miiCommon @miiIncludes @miiWireIncludes "-I$miiMbed/include" @miiSources `
        protocol/host-tests/auth/auth_wire_test.cpp firmware/components/mii_auth/auth_wire.cpp `
        firmware/components/mii_auth/idf_auth_crypto.cpp firmware/components/mii_protocol/protocol_codec.cpp `
        @miiObjects @miiWireObjects -o $miiWireExe
    Assert-MiiExit 'Authentication wire host compilation'
    & $miiWireExe protocol/test-vectors/authentication_v1.json
    Assert-MiiExit 'Authentication wire host test'
} finally { Pop-Location }
Write-Host 'AUTH_PLATFORM_HOST_PASS: real pinned mbedTLS, instrumented allocation and stubbed NVS/RNG platform plumbing with UBSan'
Write-Host 'Boundary: no S3 hardware crypto/entropy, eFuse provisioning, encrypted-flash/power-cut, or radio evidence.'
