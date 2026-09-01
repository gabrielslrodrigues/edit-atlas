$ErrorActionPreference = "Stop"

[Console]::Error.WriteLine(@"
Windows packaged E2E provisioning requires the interactive desktop virtual
machine tracked by issue #165. Windows containers cannot provide the UI
Automation desktop session. No package was installed.

See tests/e2e/README.md#provisioned-environments for the provisioning contract.
"@)
exit 2
