param(
  [Parameter(Mandatory = $true)]
  [string]$CacheDirectory,
  [string]$BeforeFingerprint = ""
)

$ErrorActionPreference = "Stop"

function Get-TextHash {
  param(
    [Parameter(Mandatory = $true)]
    [AllowEmptyString()]
    [string]$Text
  )

  $algorithm = [System.Security.Cryptography.SHA256]::Create()
  try {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $hash = $algorithm.ComputeHash($bytes)
    return [Convert]::ToHexString($hash).ToLowerInvariant()
  } finally {
    $algorithm.Dispose()
  }
}

$resolvedCacheDirectory = [System.IO.Path]::GetFullPath($CacheDirectory)
$archives = @()
if (Test-Path -LiteralPath $resolvedCacheDirectory) {
  $archives = @(
    Get-ChildItem -LiteralPath $resolvedCacheDirectory -File -Recurse |
      ForEach-Object {
        [System.IO.Path]::GetRelativePath(
          $resolvedCacheDirectory,
          $_.FullName
        ).Replace("\", "/")
      } |
      Sort-Object
  )
}

$fingerprint = Get-TextHash ($archives -join "`n")
[bool]$changed = $BeforeFingerprint -and $fingerprint -ne $BeforeFingerprint

Write-Host "vcpkg binary archive inventory: $($archives.Count) file(s), $fingerprint"
if ($BeforeFingerprint) {
  Write-Host "vcpkg binary archive inventory changed: $($changed.ToString().ToLowerInvariant())"
}

if ($env:GITHUB_OUTPUT) {
  Add-Content -Path $env:GITHUB_OUTPUT -Value "fingerprint=$fingerprint"
  Add-Content -Path $env:GITHUB_OUTPUT -Value "archive-count=$($archives.Count)"
  Add-Content -Path $env:GITHUB_OUTPUT -Value "changed=$($changed.ToString().ToLowerInvariant())"
}
