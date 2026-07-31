param(
  [Parameter(Mandatory = $true)]
  [string]$BuildDirectory
)

$ErrorActionPreference = "Stop"

$logs = @(
  Get-ChildItem -LiteralPath $BuildDirectory `
    -Filter "vcpkg-manifest-install.log" `
    -File `
    -Recurse `
    -ErrorAction SilentlyContinue
)

$restoredCount = 0
$builtPackages = [System.Collections.Generic.HashSet[string]]::new()
foreach ($log in $logs) {
  foreach ($line in Get-Content -LiteralPath $log.FullName) {
    if ($line -match "Restored ([0-9]+) package") {
      $restoredCount += [int]$Matches[1]
    }
    if ($line -match "^Building (.+)\.\.\.$") {
      [void]$builtPackages.Add($Matches[1])
    }
  }
}

$built = @($builtPackages | Sort-Object)
$message = "vcpkg binary packages: $restoredCount restored, $($built.Count) built"
Write-Host $message
if ($built.Count -gt 0) {
  Write-Warning "vcpkg built: $($built -join ', ')"
}

if ($env:GITHUB_OUTPUT) {
  Add-Content -Path $env:GITHUB_OUTPUT -Value "restored-package-count=$restoredCount"
  Add-Content -Path $env:GITHUB_OUTPUT -Value "built-package-count=$($built.Count)"
}

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- $message"
  if ($built.Count -gt 0) {
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "  - built: ``$($built -join '`, `')``"
  }
}
