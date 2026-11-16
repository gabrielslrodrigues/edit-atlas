param(
  [string]$LogDirectory = 'build/ci-logs'
)

$ErrorActionPreference = 'Stop'
$logOptions = @{
  LiteralPath = $LogDirectory
  Filter = 'configure-*.log'
  File = $true
  ErrorAction = 'SilentlyContinue'
}
$logs = @(Get-ChildItem @logOptions)
if ($logs.Count -eq 0) {
  Write-Warning 'No configure log was captured; cache activity is unknown.'
  exit 0
}

foreach ($log in $logs) {
  $restoredCount = 0
  $started = [System.Collections.Generic.HashSet[string]]::new()
  $completed = [System.Collections.Generic.HashSet[string]]::new()
  $cacheErrors = 0
  foreach ($line in Get-Content -LiteralPath $log.FullName) {
    if ($line -match 'Restored ([0-9]+) package') {
      $restoredCount += [int]$Matches[1]
    }
    if ($line -match '^Building ([^\[:]+)(?:\[[^]]*\])?:([^@]+)@') {
      [void]$started.Add("$($Matches[1]):$($Matches[2])")
    }
    if ($line -match '^Elapsed time to handle (.+?): ') {
      [void]$completed.Add($Matches[1])
    }
    if ($line -match 'Unable to load the service index|Failed to (upload|push)|error:.*nuget') {
      $cacheErrors++
    }
  }
  $finished = @($started | Where-Object { $completed.Contains($_) })
  $message = @(
    "$($log.BaseName): $restoredCount restored; "
    "$($started.Count) builds started; $($finished.Count) completed; "
    "$cacheErrors cache-error log lines"
  ) -join ''
  Write-Host $message
  if ($env:GITHUB_STEP_SUMMARY) {
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- $message"
  }
}
