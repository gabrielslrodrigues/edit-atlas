param(
  [Parameter(Mandatory = $true)]
  [string]$Repository,
  [Parameter(Mandatory = $true)]
  [string]$Ref,
  [Parameter(Mandatory = $true)]
  [string]$CurrentKey,
  [Parameter(Mandatory = $true)]
  [string[]]$KeyPrefix
)

$ErrorActionPreference = "Stop"

$cacheJson = gh cache list `
  --repo $Repository `
  --ref $Ref `
  --limit 100 `
  --json id,key
if ($LASTEXITCODE -ne 0) {
  throw "Could not list superseded vcpkg caches."
}

$listedCaches = @($cacheJson | ConvertFrom-Json)
if ($listedCaches.key -notcontains $CurrentKey) {
  Write-Warning (
    "The replacement vcpkg cache is not visible; " +
    "superseded caches will not be deleted."
  )
  return
}

$caches = @(
  $listedCaches |
    Where-Object {
      $key = $_.key
      $matchesPrefix = $KeyPrefix.Where({ $key.StartsWith($_) }).Count -gt 0
      $key -ne $CurrentKey -and $matchesPrefix
    }
)

foreach ($cache in $caches) {
  gh cache delete "$($cache.id)" --repo $Repository
  if ($LASTEXITCODE -ne 0) {
    throw "Could not delete vcpkg cache $($cache.id)."
  }
  Write-Host "Deleted superseded vcpkg cache $($cache.id)."
}
