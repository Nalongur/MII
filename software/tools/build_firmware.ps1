[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Profile = 'All',

    [string]$IdfPath,

    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $repositoryRoot 'firmware'
$versionsPath = Join-Path $repositoryRoot 'versions.json'
$versions = Get-Content -LiteralPath $versionsPath -Raw | ConvertFrom-Json
$requiredVersion = [string]$versions.espIdfVersion

if (-not $requiredVersion) {
    throw 'versions.json does not define espIdfVersion.'
}

if (-not $IdfPath) {
    $IdfPath = Join-Path $env:USERPROFILE ".espressif\frameworks\esp-idf-v$requiredVersion"
}

$exportScript = Join-Path $IdfPath 'export.ps1'
if (-not (Test-Path -LiteralPath $exportScript -PathType Leaf)) {
    throw "Pinned ESP-IDF v$requiredVersion was not found at $IdfPath"
}

. $exportScript

$reportedVersion = (& idf.py --version | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "idf.py version query failed with exit code $LASTEXITCODE"
}
if ($reportedVersion -ne "ESP-IDF v$requiredVersion") {
    throw "Expected ESP-IDF v$requiredVersion, but idf.py reported '$reportedVersion'."
}

$profiles = if ($Profile -eq 'All') { @('Debug', 'Release') } else { @($Profile) }

Push-Location $firmwareRoot
try {
    foreach ($currentProfile in $profiles) {
        $profileName = $currentProfile.ToLowerInvariant()
        $buildDirectory = "build-$profileName"
        $defaults = "sdkconfig.defaults.v7_n8;sdkconfig.defaults.$profileName"

        Write-Host "== ESP-IDF $requiredVersion / $currentProfile =="
        if ($Fresh -and (Test-Path -LiteralPath $buildDirectory -PathType Container)) {
            & idf.py -B $buildDirectory fullclean
            if ($LASTEXITCODE -ne 0) {
                throw "$currentProfile firmware fullclean failed with exit code $LASTEXITCODE"
            }
        }
        & idf.py -B $buildDirectory "-DSDKCONFIG_DEFAULTS=$defaults" build
        if ($LASTEXITCODE -ne 0) {
            throw "$currentProfile firmware build failed with exit code $LASTEXITCODE"
        }

        $applicationImage = Join-Path $buildDirectory 'mii_firmware.bin'
        $bootloaderImage = Join-Path $buildDirectory 'bootloader\bootloader.bin'
        if (-not (Test-Path -LiteralPath $applicationImage -PathType Leaf)) {
            throw "$currentProfile application image is missing: $applicationImage"
        }
        if (-not (Test-Path -LiteralPath $bootloaderImage -PathType Leaf)) {
            throw "$currentProfile bootloader image is missing: $bootloaderImage"
        }

        $generatedSdkconfig = Join-Path $buildDirectory 'sdkconfig'
        $sdkconfigText = Get-Content -LiteralPath $generatedSdkconfig -Raw
        $selfTestEnabled = $sdkconfigText -match '(?m)^CONFIG_MII_PROTOCOL_STARTUP_SELF_TEST=y\s*$'
        $bleServiceEnabled = $sdkconfigText -match '(?m)^CONFIG_MII_BLE_CONTROL_SERVICE=y\s*$'
        if ($currentProfile -eq 'Debug' -and -not $selfTestEnabled) {
            throw 'Debug sdkconfig is stale: protocol startup self-test is not enabled. Re-run with -Fresh.'
        }
        if ($currentProfile -eq 'Release' -and $selfTestEnabled) {
            throw 'Release sdkconfig incorrectly enables the protocol startup self-test. Re-run with -Fresh.'
        }
        if ($bleServiceEnabled) {
            throw "$currentProfile sdkconfig unexpectedly enables the unauthenticated experimental BLE service."
        }

        $applicationBytes = (Get-Item -LiteralPath $applicationImage).Length
        $bootloaderBytes = (Get-Item -LiteralPath $bootloaderImage).Length
        Write-Host "$currentProfile firmware build passed: app=$applicationBytes bytes, bootloader=$bootloaderBytes bytes"
    }
} finally {
    Pop-Location
}

Write-Host 'FIRMWARE_BUILD_PASS'
Write-Host 'Boundary: images were built only; no device was flashed or physically tested.'
