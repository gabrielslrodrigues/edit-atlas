param(
  [Parameter(Mandatory = $true)]
  [string]$PrimaryKey,
  [string]$MatchedKey = "",
  [string]$ExactHit = ""
)

$ErrorActionPreference = "Stop"

if ($ExactHit -eq "true") {
  $status = "exact hit"
  $detail = $PrimaryKey
} elseif ($MatchedKey) {
  $status = "fallback hit"
  $detail = "$MatchedKey (requested $PrimaryKey)"
} else {
  $status = "miss"
  $detail = $PrimaryKey
}

$message = "vcpkg binary cache: $status - $detail"
Write-Host $message

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- $message"
}
