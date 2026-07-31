param(
  [Parameter(Mandatory = $true)]
  [string]$RequestedKey,
  [string]$MatchedKey = ""
)

$ErrorActionPreference = "Stop"

if ($MatchedKey) {
  $status = "restored generation"
  $detail = "$MatchedKey (next generation $RequestedKey)"
} else {
  $status = "miss"
  $detail = $RequestedKey
}

$message = "vcpkg archive cache: $status - $detail"
Write-Host $message

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- $message"
}
