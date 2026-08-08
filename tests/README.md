# Test suite

Edit Atlas separates tests by the boundary each test crosses:

- `unit/` contains isolated domain, format, storage, support, widget/model, and
  utility tests. Test doubles and temporary files may be used to exercise one
  component without assembling a production workflow.
- `integration/` contains tests that connect real services, registered formats,
  filesystem behavior, or an in-process frontend.
- `e2e/` is reserved for pytest scenarios that drive installed packages through
  their public GUI and command-line interfaces.
- `fixtures/` owns immutable synthetic inputs shared by multiple test layers.
  A fixture belongs in a format-specific subdirectory and must document its
  provenance. Tests must copy a fixture before modifying it.

Every test discovered by CTest has exactly one `unit` or `integration` label.
The normal test presets continue to run both layers. Select one layer from an
existing configured build tree with:

```sh
ctest --preset debug-x64-linux --label-regex '^unit$'
ctest --preset debug-x64-linux --label-regex '^integration$'
```

Use the preset matching the configured platform and build type. The packaged
E2E suite is intentionally outside CTest and has its own reproducible entry
points.

The former CMake-driven separate-process CLI check is intentionally not
registered here. Packaged CLI process behavior belongs to the pytest E2E suite
tracked by issue #88; the integration suite invokes the CLI application in
process.
