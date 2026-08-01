param(
  [Parameter(Mandatory = $true)]
  [string]$PackageDirectory,
  [string]$InstallDirectory = ""
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedProcess {
  param(
    [Parameter(Mandatory = $true)]
    [string]$FilePath,
    [Parameter(Mandatory = $true)]
    [string[]]$ArgumentList,
    [Parameter(Mandatory = $true)]
    [string]$Description,
    [int]$TimeoutSeconds = 300
  )

  Write-Host "$Description..."
  $process = Start-Process `
    -FilePath $FilePath `
    -ArgumentList $ArgumentList `
    -PassThru `
    -NoNewWindow
  if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw "$Description timed out after $TimeoutSeconds seconds."
  }

  if ($process.ExitCode -notin @(0, 3010)) {
    throw "$Description failed with exit code $($process.ExitCode)."
  }
  Write-Host "$Description completed with exit code $($process.ExitCode)."
}

function Get-EditAtlasUninstallEntry {
  Get-ChildItem `
    "HKLM:/Software/Microsoft/Windows/CurrentVersion/Uninstall", `
    "HKLM:/Software/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall" `
    -ErrorAction SilentlyContinue |
    Get-ItemProperty |
    Where-Object { $_.DisplayName -eq "Edit Atlas" } |
    Select-Object -First 1
}

Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;

public static class EditAtlasNativeMethods {
  [DllImport("kernel32.dll")]
  public static extern uint SetErrorMode(uint errorMode);
}
"@

$sourceDirectory = (
  Resolve-Path (Join-Path $PSScriptRoot "../..")
).Path
$packageDirectoryPath = (Resolve-Path $PackageDirectory).Path
$installers = @(
  Get-ChildItem -Path $packageDirectoryPath -File -Filter "*.msi"
)
if ($installers.Count -ne 1) {
  throw "Expected exactly one MSI installer in $packageDirectoryPath; found $($installers.Count)."
}
$installer = $installers[0].FullName

if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
  $InstallDirectory = Join-Path `
    $sourceDirectory `
    "build/package-check-windows/install"
}
$InstallDirectory = [System.IO.Path]::GetFullPath($InstallDirectory)
if (Test-Path $InstallDirectory) {
  throw "The package verification target already exists: $InstallDirectory"
}
$verificationDirectory = Split-Path $InstallDirectory -Parent
$null = New-Item `
  -ItemType Directory `
  -Path $verificationDirectory `
  -Force
$installLog = Join-Path $verificationDirectory "msi-install.log"
$uninstallLog = Join-Path $verificationDirectory "msi-uninstall.log"
$cleanupLog = Join-Path $verificationDirectory "msi-cleanup.log"

$installed = $false
try {
  Invoke-CheckedProcess `
    -FilePath "msiexec.exe" `
    -ArgumentList @(
      "/i",
      "`"$installer`"",
      "/qn",
      "/norestart",
      "/L*V",
      "`"$installLog`"",
      "INSTALL_ROOT=`"$InstallDirectory`""
    ) `
    -Description "Installing the MSI package"
  $installed = $true

  $executable = Join-Path $InstallDirectory "bin/edit-atlas.exe"
  if (-not (Test-Path $executable -PathType Leaf)) {
    throw "The installed application executable is missing: $executable"
  }

  & cmake `
    "-DEDIT_ATLAS_DEPLOYMENT_ROOT=$InstallDirectory" `
    "-DEDIT_ATLAS_EXECUTABLE=$executable" `
    -P "$sourceDirectory/cmake/VerifyApplicationDeployment.cmake"
  if ($LASTEXITCODE -ne 0) {
    throw "The installed Qt deployment verification failed."
  }

  if ($null -eq (Get-EditAtlasUninstallEntry)) {
    throw "The Edit Atlas uninstall registry entry was not found."
  }

  $previousErrorMode = [EditAtlasNativeMethods]::SetErrorMode(0x8003)
  try {
    $process = Start-Process -FilePath $executable -PassThru
  } finally {
    $null = [EditAtlasNativeMethods]::SetErrorMode($previousErrorMode)
  }
  if ($process.WaitForExit(5000)) {
    throw (
      "The installed application exited during its launch smoke test " +
      "with code $($process.ExitCode)."
    )
  }
  Stop-Process -Id $process.Id -Force
  if (-not $process.WaitForExit(30000)) {
    throw "The application did not stop within 30 seconds."
  }

  Invoke-CheckedProcess `
    -FilePath "msiexec.exe" `
    -ArgumentList @(
      "/x",
      "`"$installer`"",
      "/qn",
      "/norestart",
      "/L*V",
      "`"$uninstallLog`""
    ) `
    -Description "Uninstalling the MSI package"
  $installed = $false

  if (Test-Path $executable -PathType Leaf) {
    throw "The application executable remains after uninstall: $executable"
  }
  if ($null -ne (Get-EditAtlasUninstallEntry)) {
    throw "The Edit Atlas uninstall registry entry remains after uninstall."
  }
} finally {
  if ($installed) {
    try {
      Invoke-CheckedProcess `
        -FilePath "msiexec.exe" `
        -ArgumentList @(
          "/x",
          "`"$installer`"",
          "/qn",
          "/norestart",
          "/L*V",
          "`"$cleanupLog`""
        ) `
        -Description "Cleaning up the MSI package" `
        -TimeoutSeconds 120
    } catch {
      Write-Warning "MSI cleanup failed: $_"
    }
  }
}

Write-Host "Verified Windows package: $installer"
