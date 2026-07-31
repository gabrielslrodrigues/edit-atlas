param(
  [Parameter(Mandatory = $true)]
  [string]$Triplet,
  [Parameter(Mandatory = $true)]
  [string]$DependencyHash
)

$ErrorActionPreference = "Stop"

function Get-TextHash {
  param(
    [Parameter(Mandatory = $true)]
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

function Get-CommandLine {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Command,
    [string[]]$Arguments = @()
  )

  $output = & $Command @Arguments 2>&1
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed while calculating the cache context: $Command"
  }
  return ($output | ForEach-Object { $_.ToString() }) -join "`n"
}

$compilerName = if ($IsWindows) { "cl.exe" } else { "c++" }
$compiler = Get-Command $compilerName -ErrorAction Stop
$compilerHash = (Get-FileHash $compiler.Source -Algorithm SHA256).Hash.ToLowerInvariant()
$vcpkgRevision = (Get-CommandLine "git" @("-C", "vcpkg", "rev-parse", "HEAD")).Trim()

$context = @(
  "triplet=$Triplet"
  "image-os=$($env:ImageOS)"
  "image-version=$($env:ImageVersion)"
  "compiler=$($compiler.Source)"
  "compiler-sha256=$compilerHash"
  "cmake=$((Get-CommandLine "cmake" @("--version")).Split("`n")[0])"
  "ninja=$(Get-CommandLine "ninja" @("--version"))"
)

if ($IsWindows) {
  $context += @(
    "vc-tools-version=$($env:VCToolsVersion)"
    "windows-sdk-version=$($env:WindowsSDKVersion)"
  )
} elseif ($IsMacOS) {
  $context += @(
    "xcode=$(Get-CommandLine "xcodebuild" @("-version"))"
    "macos-sdk=$(Get-CommandLine "xcrun" @("--sdk", "macosx", "--show-sdk-version"))"
  )
} else {
  $context += "linker=$((Get-CommandLine "ld" @("--version")).Split("`n")[0])"
}

$contextText = $context -join "`n"
$contextHash = Get-TextHash $contextText
$compatibilityPrefix = "vcpkg-v2-$($env:RUNNER_OS)-$($env:RUNNER_ARCH)-$Triplet-$DependencyHash-$vcpkgRevision"
$keyPrefix = "$compatibilityPrefix-$contextHash"

Write-Host "vcpkg cache context:"
$context | ForEach-Object { Write-Host "  $_" }
Write-Host "  vcpkg-revision=$vcpkgRevision"
Write-Host "  context-sha256=$contextHash"
Write-Host "  compatibility-prefix=$compatibilityPrefix"
Write-Host "  key-prefix=$keyPrefix"

if ($env:GITHUB_OUTPUT) {
  Add-Content -Path $env:GITHUB_OUTPUT -Value "context-hash=$contextHash"
  Add-Content -Path $env:GITHUB_OUTPUT -Value "vcpkg-revision=$vcpkgRevision"
  Add-Content -Path $env:GITHUB_OUTPUT -Value "compatibility-prefix=$compatibilityPrefix"
  Add-Content -Path $env:GITHUB_OUTPUT -Value "key-prefix=$keyPrefix"
}

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "- vcpkg toolchain context: ``$contextHash``"
}
