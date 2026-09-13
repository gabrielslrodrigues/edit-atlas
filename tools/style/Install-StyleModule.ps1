[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
if ($args.Count -ne 0) {
  throw 'Usage: Install-StyleModule.ps1'
}

$modulesPath = Join-Path $PSScriptRoot 'modules'
New-Item -ItemType Directory -Force -Path $modulesPath | Out-Null
Save-Module -Name PSScriptAnalyzer -RequiredVersion '1.24.0' `
  -Repository PSGallery -Path $modulesPath
