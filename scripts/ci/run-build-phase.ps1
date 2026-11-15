param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('configure-debug', 'configure-release', 'build-debug',
    'build-release')]
  [string]$Phase,
  [Parameter(Mandatory = $true)]
  [string[]]$CMakeArguments
)

$ErrorActionPreference = 'Stop'
$directory = Join-Path $PWD 'build/ci-logs'
New-Item -ItemType Directory -Force -Path $directory | Out-Null
$log = Join-Path $directory "$Phase.log"
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$status = 1
try {
  & cmake @CMakeArguments 2>&1 | Tee-Object -FilePath $log
  $status = $LASTEXITCODE
} finally {
  $timer.Stop()
  $message = "$Phase`: $($timer.Elapsed.TotalSeconds.ToString('F1')) " `
    + "seconds; exit $status"
  Write-Host $message
  if ($env:GITHUB_STEP_SUMMARY) {
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- $message"
  }
}
exit $status
