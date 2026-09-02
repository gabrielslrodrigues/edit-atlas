<#
.SYNOPSIS
    Installs the project-owned Windows dependencies for packaged E2E.

.DESCRIPTION
    Windows cannot share the Linux runner image, so it shares a dependency
    contract instead. This installs what the repository owns and states what
    the environment must already provide, so a hosted runner and the local
    disposable environment prepare themselves the same way.

    Only what is needed to install the generated MSI and execute the packaged
    suite is installed. Build tooling is deliberately absent: the compiler,
    the Windows SDK, and CMake come from the selected image or from the build
    workflow's own actions.

    The environment must additionally provide Windows with PowerShell 7 or
    later and an interactive desktop session. UI Automation drives a real
    desktop, so a service or session-isolated context cannot run the suite;
    that requirement is verified rather than installed.

.PARAMETER Force
    Reinstall uv even when a satisfying version is already present.
#>
[CmdletBinding()]
param(
  [switch]$Force
)

$ErrorActionPreference = "Stop"

# Kept in step with `required-version` in tests/e2e/pyproject.toml, which is
# what the suite enforces at run time, and with the pinned action version used
# by the hosted workflows.
$RequiredUvVersion = "0.12.3"

function Get-InstalledUvVersion {
  $command = Get-Command uv -ErrorAction SilentlyContinue
  if ($null -eq $command) {
    return $null
  }
  $output = & uv --version 2>$null
  if ($LASTEXITCODE -ne 0 -or -not $output) {
    return $null
  }
  # `uv --version` reports "uv 0.12.3 (abcdef 2026-01-01)".
  $match = [regex]::Match($output, '(\d+\.\d+\.\d+)')
  if (-not $match.Success) {
    return $null
  }
  return $match.Groups[1].Value
}

function Install-Uv {
  param([string]$Version)

  # The standalone installer is used rather than a package manager so the
  # exact version is selected identically wherever this runs.
  $installer = Join-Path $env:TEMP "install-uv.ps1"
  $uri = "https://astral.sh/uv/$Version/install.ps1"
  Write-Host "Installing uv $Version"
  Invoke-WebRequest -Uri $uri -OutFile $installer -UseBasicParsing
  try {
    & powershell -ExecutionPolicy Bypass -File $installer
    if ($LASTEXITCODE -ne 0) {
      throw "The uv installer failed with exit code $LASTEXITCODE"
    }
  } finally {
    Remove-Item $installer -Force -ErrorAction SilentlyContinue
  }

  # The installer extends PATH for later sessions; make it usable in this one.
  $uvDirectory = Join-Path $env:USERPROFILE ".local\bin"
  if (Test-Path $uvDirectory) {
    $env:PATH = "$uvDirectory;$env:PATH"
  }
}

$installed = Get-InstalledUvVersion
if ($Force -or $installed -ne $RequiredUvVersion) {
  if ($installed) {
    Write-Host "Found uv $installed; the suite requires $RequiredUvVersion"
  }
  Install-Uv -Version $RequiredUvVersion
  $installed = Get-InstalledUvVersion
}

if ($installed -ne $RequiredUvVersion) {
  throw "uv $RequiredUvVersion is required but '$installed' is available"
}

Write-Host "uv $installed is installed"

# Reported rather than installed. These belong to the environment, and the
# packaged suite fails clearly on its own when a desktop is unavailable.
if ([Environment]::UserInteractive) {
  Write-Host "Interactive desktop session: available"
} else {
  Write-Warning (
    "No interactive desktop session was detected. Packaged Windows E2E " +
    "drives the application through UI Automation and requires one."
  )
}
Write-Host "PowerShell: $($PSVersionTable.PSVersion)"
