param(
  [Parameter(Mandatory = $true)]
  [string] $Msi,

  [string] $MediaFixtureDir,

  [string] $FixtureGenerator,

  [string] $ArtifactDir,

  [switch] $AllowHostInstall,

  [int] $TimeoutMinutes = 60,

  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]] $PytestArguments
)

# Runs the packaged Windows suite in a Windows Sandbox, so a local run installs
# no Edit Atlas package and no packaged E2E dependency on the developer host.
#
# The sandbox executes tests/e2e/windows/run-provisioned.ps1, the same harness
# the CI job calls. Windows Sandbox and the hosted runner cannot share an OS
# image, since the sandbox is built from the host's own build; they share this
# repository's provisioning, execution, artifact, and cleanup behavior instead.
#
# Windows Sandbox is absent from Home editions and cannot be enabled there, and
# those hosts have no Hyper-V either, so they have no isolated interactive
# desktop at all. -AllowHostInstall runs the same harness on the host instead,
# trading isolation for the ability to run locally what CI runs. It is never
# selected automatically: installing on the host has to be asked for.

$ErrorActionPreference = "Stop"
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepositoryRoot = [IO.Path]::GetFullPath(
  (Resolve-Path (Join-Path $ScriptDirectory "../..")).ProviderPath
)

if ($env:OS -ne "Windows_NT") {
  throw "Windows Sandbox provisioning requires Windows"
}

$SandboxExecutable = Join-Path $env:SystemRoot "System32/WindowsSandbox.exe"
if (-not $AllowHostInstall) {
  if (-not (Test-Path -LiteralPath $SandboxExecutable)) {
    throw @"
Windows Sandbox is not available on this host.
It requires Windows 10 1903 or newer, Pro, Enterprise, or Education, with
hardware virtualization enabled in firmware. Enable the feature with:
  Enable-WindowsOptionalFeature -Online `
    -FeatureName Containers-DisposableClientVM
A restart is required, and the command-line launcher is present from Windows 11
24H2 onward.

A Home edition cannot enable it and has no Hyper-V either. Pass
-AllowHostInstall to run the same harness on this host without isolation.
"@
  }

  if (Get-Process -Name WindowsSandbox, WindowsSandboxClient `
      -ErrorAction SilentlyContinue) {
    throw @"
A Windows Sandbox instance is already running, and Windows permits only one.
Close it and run this script again.
"@
  }
}

# The package installs into the artifact root, and msiexec cannot install to a
# UNC target. A sandbox additionally cannot map one, so every mapped folder
# must be local there. A checkout reached over \\wsl.localhost is the common
# way to hit this.
function Assert-LocalPath {
  param([string] $Path, [string] $Description)

  if ($Path.StartsWith("\\")) {
    throw @"
$Description must be a local path, not a network path: $Path
Windows Installer cannot install to one, and Windows Sandbox cannot map one.
"@
  }
}

if (-not (Test-Path -LiteralPath $Msi)) {
  throw "The MSI package does not exist: $Msi"
}
if ([bool] $MediaFixtureDir -eq [bool] $FixtureGenerator) {
  throw "Supply exactly one of -MediaFixtureDir and -FixtureGenerator"
}

$Msi = (Resolve-Path -LiteralPath $Msi).ProviderPath
if (-not $ArtifactDir) {
  $ArtifactDir = Join-Path $RepositoryRoot "build/e2e"
}
New-Item -ItemType Directory -Force $ArtifactDir | Out-Null
$ArtifactDir = (Resolve-Path -LiteralPath $ArtifactDir).ProviderPath

Assert-LocalPath $ArtifactDir "The artifact directory"
if (-not $AllowHostInstall) {
  Assert-LocalPath $RepositoryRoot "The repository"
  Assert-LocalPath (Split-Path -Parent $Msi) "The MSI's directory"
}

if ($FixtureGenerator) {
  # Generated on the host, as on Linux: the generator is a build-tree binary
  # linked against the host's toolchain, which the sandbox image does not have.
  if (-not (Test-Path -LiteralPath $FixtureGenerator -PathType Leaf)) {
    throw "The fixture generator does not exist: $FixtureGenerator"
  }
  $MediaFixtureDir = Join-Path $ArtifactDir "media-fixtures"
  & (Join-Path $ScriptDirectory "generate-media-fixtures.ps1") `
    -Generator $FixtureGenerator `
    -FixtureDirectory $MediaFixtureDir
} elseif (-not (Test-Path -LiteralPath $MediaFixtureDir)) {
  throw "The media-fixture directory does not exist: $MediaFixtureDir"
}
$MediaFixtureDir = (Resolve-Path -LiteralPath $MediaFixtureDir).ProviderPath
if (-not $AllowHostInstall) {
  Assert-LocalPath $MediaFixtureDir "The media-fixture directory"
}

$Harness = Join-Path $ScriptDirectory "windows/run-provisioned.ps1"

if ($AllowHostInstall) {
  # The package installs per-machine, so a silent install needs elevation.
  $principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
  if (-not $principal.IsInRole(
      [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw @"
-AllowHostInstall requires an elevated session. The package installs
per-machine, so msiexec cannot install it silently otherwise.
"@
  }

  # An installed Edit Atlas shares this package's upgrade code, so installing
  # here would be treated as an upgrade of it, and the harness's cleanup would
  # then uninstall what it upgraded. Refusing leaves that decision with the
  # person who owns the installation.
  $uninstallKeys = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
    "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*"
  )
  $installed = Get-ItemProperty $uninstallKeys -ErrorAction SilentlyContinue |
    Where-Object { $_.DisplayName -like "Edit Atlas*" } |
    Select-Object -First 1
  if ($installed) {
    throw @"
Edit Atlas $($installed.DisplayVersion) is already installed on this host.
This package shares its upgrade code, so installing it here would replace that
installation and the run's cleanup would then remove it. Uninstall it first,
and reinstall it afterwards if you still want it.
"@
  }

  Write-Warning @"
Running without isolation. This installs uv and the package under test on this
host. The package and its crash-dump settings are removed afterwards; uv and
the artifacts under $ArtifactDir remain, and no Edit Atlas stays installed.
"@

  & $Harness `
    -RepositoryRoot $RepositoryRoot `
    -ArtifactRoot $ArtifactDir `
    -Msi $Msi `
    -MediaFixtureDir $MediaFixtureDir `
    @PytestArguments
  exit $LASTEXITCODE
}

# Mapped folders are directories, so the MSI travels as its parent directory
# and is named again inside the sandbox.
$PackageDirectory = Split-Path -Parent $Msi
$MsiName = Split-Path -Leaf $Msi

$GuestRoot = "C:\edit-atlas"
$GuestSource = "$GuestRoot\source"
$GuestPackage = "$GuestRoot\package"
$GuestFixtures = "$GuestRoot\media-fixtures"
$GuestResults = "$GuestRoot\results"

# The sandbox reports no exit status to the host, so the bootstrap records the
# suite's status in the mapped result directory and the host waits for it.
$StatusFile = Join-Path $ArtifactDir "sandbox-exit-code.txt"
$TranscriptFile = Join-Path $ArtifactDir "sandbox-harness.log"
Remove-Item -LiteralPath $StatusFile -Force -ErrorAction SilentlyContinue

$EncodedPytestArguments = ""
if ($PytestArguments) {
  $quoted = $PytestArguments | ForEach-Object {
    "'" + ($_ -replace "'", "''") + "'"
  }
  $EncodedPytestArguments = " " + ($quoted -join " ")
}

$BootstrapFile = Join-Path $ArtifactDir "sandbox-bootstrap.ps1"
@"
`$ErrorActionPreference = 'Continue'
Start-Transcript -Path '$GuestResults\sandbox-harness.log' -Force | Out-Null
try {
  & '$GuestSource\tests\e2e\windows\run-provisioned.ps1' ``
    -RepositoryRoot '$GuestSource' ``
    -ArtifactRoot '$GuestResults' ``
    -Msi '$GuestPackage\$MsiName' ``
    -MediaFixtureDir '$GuestFixtures'$EncodedPytestArguments
  `$code = `$LASTEXITCODE
} catch {
  Write-Output `$_
  `$code = 1
}
Stop-Transcript | Out-Null
Set-Content -Path '$GuestResults\sandbox-exit-code.txt' -Value `$code
"@ | Set-Content -LiteralPath $BootstrapFile -Encoding UTF8

$BootstrapCommand =
  "powershell.exe -ExecutionPolicy Bypass -NoProfile " +
  "-File $GuestResults\sandbox-bootstrap.ps1"

$ConfigurationFile = Join-Path $ArtifactDir "sandbox.wsb"
@"
<Configuration>
  <VGpu>Disable</VGpu>
  <Networking>Enable</Networking>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$RepositoryRoot</HostFolder>
      <SandboxFolder>$GuestSource</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$PackageDirectory</HostFolder>
      <SandboxFolder>$GuestPackage</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$MediaFixtureDir</HostFolder>
      <SandboxFolder>$GuestFixtures</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$ArtifactDir</HostFolder>
      <SandboxFolder>$GuestResults</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>$BootstrapCommand</Command>
  </LogonCommand>
</Configuration>
"@ | Set-Content -LiteralPath $ConfigurationFile -Encoding UTF8

Write-Host "Starting Windows Sandbox for the packaged E2E suite."
Start-Process -FilePath $SandboxExecutable `
  -ArgumentList $ConfigurationFile | Out-Null

$Deadline = (Get-Date).AddMinutes($TimeoutMinutes)
while (-not (Test-Path -LiteralPath $StatusFile)) {
  if ((Get-Date) -gt $Deadline) {
    throw @"
The sandboxed suite did not report a status within $TimeoutMinutes minutes.
The sandbox window is still open; its transcript is at $TranscriptFile.
"@
  }
  Start-Sleep -Seconds 5
}

$SuiteExitCode = [int](
  (Get-Content -LiteralPath $StatusFile -Raw).Trim()
)
Write-Host "The sandboxed suite exited with status $SuiteExitCode."
Write-Host "Reports and artifacts are under $ArtifactDir."
exit $SuiteExitCode
