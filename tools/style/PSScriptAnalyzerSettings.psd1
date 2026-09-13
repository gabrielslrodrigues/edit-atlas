@{
  IncludeDefaultRules = $true
  # These scripts write CI/console progress text, not pipeline data; Write-Host
  # is the correct choice for that, not a workaround.
  ExcludeRules = @('PSAvoidUsingWriteHost')
  Rules = @{
    PSUseConsistentIndentation = @{
      Enable = $true
      Kind = 'space'
      IndentationSize = 2
      PipelineIndentation = 'IncreaseIndentationForFirstPipeline'
    }
    PSPlaceOpenBrace = @{
      Enable = $true
      OnSameLine = $true
      NewLineAfter = $true
      IgnoreOneLineBlock = $true
    }
    PSPlaceCloseBrace = @{
      Enable = $true
      NewLineAfter = $false
      IgnoreOneLineBlock = $true
      NoEmptyLineBefore = $true
    }
    PSUseConsistentWhitespace = @{
      Enable = $true
      CheckOpenBrace = $true
      CheckInnerBrace = $true
      CheckPipe = $true
      CheckSeparator = $true
      CheckOperator = $true
      CheckParameter = $false
    }
  }
}
