param(
    [Parameter(Mandatory = $true)]
    [string] $Cli,

    [string] $Locale = "C.UTF-8",

    [double] $OperationTimeout = 15.0,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $PytestArguments
)

$ErrorActionPreference = "Stop"
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepositoryRoot = (Resolve-Path (Join-Path $ScriptDirectory "../..")).Path
$E2eRoot = Join-Path $RepositoryRoot "build/e2e"
$VirtualEnvironment = Join-Path $E2eRoot "venv"

if (-not (Get-Command uv -ErrorAction SilentlyContinue)) {
    throw "uv is required to run the E2E suite"
}

$Reports = Join-Path $E2eRoot "reports"
$Output = Join-Path $E2eRoot "output"
$Artifacts = Join-Path $E2eRoot "artifacts"
New-Item -ItemType Directory -Force $Reports, $Output, $Artifacts | Out-Null

$Arguments = @(
    "-m", "pytest",
    "-c", (Join-Path $ScriptDirectory "pytest.ini"),
    (Join-Path $ScriptDirectory "tests/test_packaged_cli.py"),
    "--cli", $Cli,
    "--fixture-dir", (Join-Path $RepositoryRoot "tests/fixtures/cmx3600"),
    "--output-dir", $Output,
    "--state-root", (Join-Path $E2eRoot "state"),
    "--artifact-dir", $Artifacts,
    "--locale", $Locale,
    "--operation-timeout", $OperationTimeout,
    "--junitxml", (Join-Path $Reports "junit.xml"),
    "--html", (Join-Path $Reports "report.html"),
    "--self-contained-html"
) + $PytestArguments

$env:UV_PROJECT_ENVIRONMENT = $VirtualEnvironment
& uv run --locked --project $ScriptDirectory python @Arguments
exit $LASTEXITCODE
