[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $repositoryRoot 'build\host\nanopb'
$committedDirectory = Join-Path $repositoryRoot 'firmware\components\mii_protocol\generated'

function Assert-LastExitCode {
    param([Parameter(Mandatory)][string]$Step)
    if ($LASTEXITCODE -ne 0) {
        throw "$Step failed with exit code $LASTEXITCODE"
    }
}

$versionProbe = @'
import importlib.metadata as metadata

expected = {
    'nanopb': '0.4.9.1',
    'grpcio-tools': '1.62.3',
    'protobuf': '4.25.6',
}
actual = {name: metadata.version(name) for name in expected}
if actual != expected:
    raise SystemExit(f'protocol generator dependency mismatch: expected {expected}, got {actual}')
print('nanopb generator dependencies:', ', '.join(f'{name}={version}' for name, version in actual.items()))
'@

& python -c $versionProbe
Assert-LastExitCode 'nanopb Python dependency check'

$pluginProbe = @'
import importlib.metadata as metadata

distribution = metadata.distribution('nanopb')
matches = [
    distribution.locate_file(path)
    for path in distribution.files or []
    if path.name.lower() == 'protoc-gen-nanopb.exe'
]
if len(matches) != 1:
    raise SystemExit(f'expected one protoc-gen-nanopb.exe, found {len(matches)}')
print(matches[0])
'@

$pluginExecutable = (& python -c $pluginProbe | Select-Object -Last 1).Trim()
Assert-LastExitCode 'nanopb plugin discovery'
if (-not (Test-Path -LiteralPath $pluginExecutable -PathType Leaf)) {
    throw "nanopb plugin was not found at $pluginExecutable"
}

$nanopbProtoDirectory = (& python -c `
    "import nanopb, pathlib; print(pathlib.Path(nanopb.__file__).resolve().parent / 'generator' / 'proto')" |
    Select-Object -Last 1).Trim()
Assert-LastExitCode 'nanopb include discovery'

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
Push-Location $repositoryRoot
try {
    & python -m grpc_tools.protoc `
        '-Iprotocol' `
        "-I$nanopbProtoDirectory" `
        "--plugin=protoc-gen-nanopb=$pluginExecutable" `
        '--nanopb_out=-fprotocol/mii.options,--error-on-unmatched:build/host/nanopb' `
        'protocol/mii.proto'
    Assert-LastExitCode 'nanopb source generation'
} finally {
    Pop-Location
}

foreach ($name in ('mii.pb.c', 'mii.pb.h')) {
    $generatedFile = Join-Path $outputDirectory $name
    if (-not (Test-Path -LiteralPath $generatedFile -PathType Leaf)) {
        throw "nanopb generator did not create $generatedFile"
    }
    $committedFile = Join-Path $committedDirectory $name
    if (-not (Test-Path -LiteralPath $committedFile -PathType Leaf)) {
        throw "committed nanopb source is missing: $committedFile"
    }
    $generatedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $generatedFile).Hash
    $committedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $committedFile).Hash
    if ($generatedHash -ne $committedHash) {
        throw "committed nanopb source is stale: $committedFile"
    }
}

Write-Host 'NANOPB_GENERATION_PASS: mii.pb.c and mii.pb.h'
Write-Host 'NANOPB_COMMITTED_SOURCES_PASS: firmware component matches canonical schema'
