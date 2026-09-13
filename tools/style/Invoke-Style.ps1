[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [ValidateSet('check', 'format')]
  [string]$Mode,
  [Parameter(Mandatory)]
  [string]$SourcePath
)

$ErrorActionPreference = 'Stop'
if ($args.Count -ne 0) {
  throw 'Usage: Invoke-Style.ps1 -Mode check|format -SourcePath file'
}

$modulePath = Join-Path $PSScriptRoot `
  'modules/PSScriptAnalyzer/1.24.0/PSScriptAnalyzer.psd1'
Import-Module $modulePath -RequiredVersion '1.24.0' -ErrorAction Stop
$settings = Join-Path $PSScriptRoot 'PSScriptAnalyzerSettings.psd1'
$content = [IO.File]::ReadAllText($SourcePath)
$formatted = Invoke-Formatter -ScriptDefinition $content -Settings $settings
$formatted = $formatted.Replace("`r`n", "`n").TrimEnd() + "`n"
if ($Mode -eq 'format') {
  [IO.File]::WriteAllText(
    $SourcePath, $formatted, [Text.UTF8Encoding]::new($false))
  exit 0
}
if ($content -cne $formatted) {
  Write-Error "PowerShell formatting differs: $SourcePath"
}
$diagnostics = @(Invoke-ScriptAnalyzer -Path $SourcePath -Settings $settings)
if ($diagnostics.Count -gt 0) {
  $diagnostics | Format-Table -AutoSize | Out-Host
  exit 1
}
