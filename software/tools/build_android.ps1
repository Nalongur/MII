[CmdletBinding()]
param(
    [string]$AndroidSdkPath = (Join-Path $env:LOCALAPPDATA 'Android\Sdk'),
    [string]$JdkPath = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot',
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
$androidRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'android'
foreach ($requiredFile in @('platforms\android-36\android.jar','build-tools\36.0.0\aapt2.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $AndroidSdkPath $requiredFile) -PathType Leaf)) {
        throw "Required pinned Android SDK file missing: $requiredFile"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $JdkPath 'bin\java.exe'))) { throw 'Configured JDK is missing.' }
$previousJavaHome = $env:JAVA_HOME
$previousAndroidHome = $env:ANDROID_HOME
try {
    $env:JAVA_HOME = $JdkPath
    $env:ANDROID_HOME = $AndroidSdkPath
    Push-Location $androidRoot
    try {
        $gradleArgs = @('--no-daemon','--console=plain','testDebugUnitTest','lintDebug','assembleDebug','assembleRelease')
        if ($Offline) { $gradleArgs += '--offline' }
        & .\gradlew.bat @gradleArgs
        if ($LASTEXITCODE -ne 0) { throw "Android build failed with exit code $LASTEXITCODE" }
        foreach ($apk in @('app\build\outputs\apk\debug\app-debug.apk','app\build\outputs\apk\release\app-release-unsigned.apk')) {
            if (-not (Test-Path -LiteralPath $apk -PathType Leaf)) { throw "Expected APK missing: $apk" }
            $artifact = Get-Item -LiteralPath $apk
            $sha256 = [System.Security.Cryptography.SHA256]::Create()
            try { $hash = [BitConverter]::ToString($sha256.ComputeHash([System.IO.File]::ReadAllBytes($artifact.FullName))).Replace('-','').ToLowerInvariant() }
            finally { $sha256.Dispose() }
            Write-Host "APK: $($artifact.FullName); bytes=$($artifact.Length); sha256=$hash"
        }
    } finally { Pop-Location }
} finally {
    $env:JAVA_HOME = $previousJavaHome
    $env:ANDROID_HOME = $previousAndroidHome
}
Write-Host 'ANDROID_BUILD_PASS: unit vectors, strict lint, Debug and unsigned shrunk Release'
Write-Host 'Boundary: no install, emulator, phone, radio, binding, or release signing evidence.'
