param(
  [Parameter(Mandatory = $true)]
  [string] $RepositoryRoot,

  [Parameter(Mandatory = $true)]
  [string] $ArtifactRoot,

  [Parameter(Mandatory = $true)]
  [string] $Msi,

  [Parameter(Mandatory = $true)]
  [string] $MediaFixtureDir,

  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]] $PytestArguments
)

# Runs the packaged Windows suite inside a disposable environment: the hosted
# CI runner or a Windows Sandbox. Both call this script, so dependency
# provisioning, MSI installation, crash configuration, suite invocation,
# artifact layout, cleanup, and exit status are identical on either side.
#
# The environment must already be disposable. This script installs Edit Atlas
# and writes machine state, and never undoes that on a host it does not own.

$ErrorActionPreference = "Stop"

function Resolve-Existing {
  param([string] $Path, [string] $Description)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw "$Description is unavailable: $Path"
  }
  return (Resolve-Path -LiteralPath $Path).ProviderPath
}

$RepositoryRoot = Resolve-Existing $RepositoryRoot "The repository"
$Msi = Resolve-Existing $Msi "The MSI package"
$MediaFixtureDir = Resolve-Existing `
  $MediaFixtureDir "The media-fixture directory"

New-Item -ItemType Directory -Force $ArtifactRoot | Out-Null
$ArtifactRoot = (Resolve-Path -LiteralPath $ArtifactRoot).ProviderPath

$InstallRoot = Join-Path $ArtifactRoot "install"
$InstallLog = Join-Path $ArtifactRoot "msi-install.log"
$UninstallLog = Join-Path $ArtifactRoot "msi-uninstall.log"
$DumpDirectory = Join-Path $ArtifactRoot "crash-dumps"
$WerRoot =
  "HKCU:\Software\Microsoft\Windows\Windows Error Reporting\LocalDumps"
$Executables = @("edit-atlas.exe", "edit-atlas-cli.exe")

if (-not [Environment]::UserInteractive) {
  throw "Windows packaged E2E requires an interactive desktop session"
}

# The MSI is an input from the caller, so it is identified before being
# installed rather than trusted by filename.
$Installer = New-Object -ComObject WindowsInstaller.Installer
$Database = $Installer.GetType().InvokeMember(
  "OpenDatabase", "InvokeMethod", $null, $Installer, @($Msi, 0))
function Get-MsiProperty {
  param([string] $Name)

  $view = $Database.GetType().InvokeMember(
    "OpenView", "InvokeMethod", $null, $Database,
    @("SELECT Value FROM Property WHERE Property = '$Name'"))
  $view.GetType().InvokeMember("Execute", "InvokeMethod", $null, $view, $null)
  $record = $view.GetType().InvokeMember(
    "Fetch", "InvokeMethod", $null, $view, $null)
  if ($null -eq $record) {
    return $null
  }
  return $record.GetType().InvokeMember(
    "StringData", "GetProperty", $null, $record, @(1))
}

$ProductName = Get-MsiProperty "ProductName"
if ($ProductName -notlike "Edit Atlas*") {
  throw "The supplied MSI is not an Edit Atlas package: $ProductName"
}
# The Template property carries the target platform, as in "x64;1033".
$Template = Get-MsiProperty "Template"
if ($Template -notlike "x64*") {
  throw "The supplied MSI does not target x64: $Template"
}

foreach ($executable in $Executables) {
  Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($executable)) `
    -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
}

$SuiteExitCode = 1
try {
  & (Join-Path $RepositoryRoot "scripts/ci/install-windows-dependencies.ps1")

  $process = Start-Process `
    -FilePath msiexec.exe `
    -ArgumentList @(
      "/i", "`"$Msi`"", "/qn", "/norestart", "/L*V", "`"$InstallLog`"",
      "INSTALL_ROOT=`"$InstallRoot`""
    ) `
    -PassThru `
    -Wait
  if ($process.ExitCode -notin @(0, 3010)) {
    throw "MSI installation failed with exit code $($process.ExitCode)."
  }

  $App = Join-Path $InstallRoot "bin/edit-atlas.exe"
  $Cli = Join-Path $InstallRoot "bin/edit-atlas-cli.exe"
  foreach ($path in @($App, $Cli)) {
    if (-not (Test-Path -LiteralPath $path)) {
      throw "The MSI did not install an expected executable: $path"
    }
  }

  # The package installs whenever the installer's own conditions are met, but
  # its bundled Qt and FFmpeg libraries may still fail to load. Reporting that
  # here names it as an incompatible input instead of letting it surface as
  # unexplained failures across the suite.
  $smokeLog = Join-Path $ArtifactRoot "package-smoke.log"
  & $Cli --version *> $smokeLog
  if ($LASTEXITCODE -ne 0) {
    Get-Content -LiteralPath $smokeLog |
      ForEach-Object { Write-Host "  $_" }
    throw @"
The installed package cannot run in this environment.
Supply a package built for a supported Windows target, such as the one CI
produces.
"@
  }

  New-Item -ItemType Directory -Force $DumpDirectory | Out-Null
  foreach ($executable in $Executables) {
    $registryPath = Join-Path $WerRoot $executable
    New-Item -Path $registryPath -Force | Out-Null
    New-ItemProperty -Path $registryPath -Name DumpFolder `
      -PropertyType ExpandString -Value $DumpDirectory -Force | Out-Null
    New-ItemProperty -Path $registryPath -Name DumpType `
      -PropertyType DWord -Value 2 -Force | Out-Null
    New-ItemProperty -Path $registryPath -Name DumpCount `
      -PropertyType DWord -Value 5 -Force | Out-Null
  }

  $env:EDIT_ATLAS_E2E_ROOT = $ArtifactRoot
  $env:EDIT_ATLAS_E2E_MEDIA_FIXTURE_DIR = $MediaFixtureDir
  $env:EDIT_ATLAS_E2E_VIRTUAL_ENVIRONMENT = Join-Path $ArtifactRoot "venv"

  & (Join-Path $RepositoryRoot "tests/e2e/run-windows.ps1") `
    -App $App `
    -Cli $Cli `
    @PytestArguments
  $SuiteExitCode = $LASTEXITCODE
} finally {
  foreach ($executable in $Executables) {
    Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($executable)) `
      -ErrorAction SilentlyContinue |
      Stop-Process -Force -ErrorAction SilentlyContinue
  }

  if (Test-Path -LiteralPath $InstallLog) {
    $process = Start-Process `
      -FilePath msiexec.exe `
      -ArgumentList @(
        "/x", "`"$Msi`"", "/qn", "/norestart", "/L*V", "`"$UninstallLog`""
      ) `
      -PassThru `
      -Wait
    if ($process.ExitCode -notin @(0, 1605, 3010)) {
      Write-Warning "MSI uninstall failed with exit code $($process.ExitCode)."
    }
  }

  foreach ($executable in $Executables) {
    Remove-Item (Join-Path $WerRoot $executable) -Recurse -Force `
      -ErrorAction SilentlyContinue
  }
  Remove-Item (Join-Path $ArtifactRoot "state") -Recurse -Force `
    -ErrorAction SilentlyContinue
}

exit $SuiteExitCode
