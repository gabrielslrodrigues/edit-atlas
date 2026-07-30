param(
    [Parameter(Mandatory = $true)]
    [string]$Installer,

    [Parameter(Mandatory = $true)]
    [string]$InstallDirectory,

    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory
)

$ErrorActionPreference = "Stop"

function Invoke-CheckedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList
    )

    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $ArgumentList `
        -Wait `
        -PassThru
    if ($process.ExitCode -notin @(0, 3010)) {
        throw "$FilePath exited with code $($process.ExitCode)."
    }
}

Invoke-CheckedProcess `
    -FilePath "msiexec.exe" `
    -ArgumentList @(
        "/install",
        $Installer,
        "/quiet",
        "/norestart",
        "INSTALL_ROOT=$InstallDirectory"
    )

$executable = Join-Path $InstallDirectory "bin/edit-atlas.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "The installed Edit Atlas executable was not found."
}

& cmake `
    "-DEDIT_ATLAS_DEPLOYMENT_ROOT=$InstallDirectory" `
    "-DEDIT_ATLAS_EXECUTABLE=$executable" `
    -P (Join-Path $SourceDirectory "cmake/VerifyQtDeployment.cmake")
if ($LASTEXITCODE -ne 0) {
    throw "The installed Qt deployment verification failed."
}

$uninstallEntry = Get-ChildItem `
    "HKLM:/Software/Microsoft/Windows/CurrentVersion/Uninstall", `
    "HKLM:/Software/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall" `
    -ErrorAction SilentlyContinue |
    Get-ItemProperty |
    Where-Object { $_.DisplayName -eq "Edit Atlas" } |
    Select-Object -First 1
if ($null -eq $uninstallEntry) {
    throw "The Edit Atlas uninstall registry entry was not found."
}

Invoke-CheckedProcess `
    -FilePath "msiexec.exe" `
    -ArgumentList @("/uninstall", $Installer, "/quiet", "/norestart")

if (Test-Path -LiteralPath $executable -PathType Leaf) {
    throw "The Edit Atlas executable remains after uninstalling."
}

$remainingUninstallEntry = Get-ChildItem `
    "HKLM:/Software/Microsoft/Windows/CurrentVersion/Uninstall", `
    "HKLM:/Software/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall" `
    -ErrorAction SilentlyContinue |
    Get-ItemProperty |
    Where-Object { $_.DisplayName -eq "Edit Atlas" } |
    Select-Object -First 1
if ($null -ne $remainingUninstallEntry) {
    throw "The Edit Atlas uninstall entry remains after uninstalling."
}

Write-Host "Verified MSI installation, Qt deployment, and uninstall."
