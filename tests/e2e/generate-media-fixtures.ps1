param(
    [Parameter(Mandatory = $true)]
    [string] $Generator,

    [Parameter(Mandatory = $true)]
    [string] $FixtureDirectory
)

# Generates the rendered-video E2E fixtures and records the generator identity
# that produced them. A desktop suite rejects fixtures without a matching
# record, so fixtures must be produced through this entry point rather than by
# invoking the generator directly.

$ErrorActionPreference = "Stop"
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepositoryRoot = (Resolve-Path (Join-Path $ScriptDirectory "../..")).Path

if (-not (Test-Path -LiteralPath $Generator -PathType Leaf)) {
    throw "generator does not exist: $Generator"
}

New-Item -ItemType Directory -Force -Path $FixtureDirectory | Out-Null

& $Generator $FixtureDirectory
if ($LASTEXITCODE -ne 0) {
    throw "the fixture generator failed with exit code $LASTEXITCODE"
}

$Python = if (Get-Command python3 -ErrorAction SilentlyContinue) {
    "python3"
} else {
    "python"
}

& $Python (Join-Path $ScriptDirectory "application/media_fixtures.py") `
    --repository-root $RepositoryRoot `
    $FixtureDirectory
if ($LASTEXITCODE -ne 0) {
    throw "recording the generator identity failed with exit code $LASTEXITCODE"
}
