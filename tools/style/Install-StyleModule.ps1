[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
if ($args.Count -ne 0) {
  throw 'Usage: Install-StyleModule.ps1'
}

Save-Module -Name PSScriptAnalyzer -RequiredVersion '1.24.0' `
  -Repository PSGallery -Path (Join-Path $PSScriptRoot 'modules')
