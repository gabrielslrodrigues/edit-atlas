# Edit Atlas

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files and exporting structured reports. The first
supported input format will be CMX 3600 EDL.

The project is in its initial scaffolding phase.

## Requirements

- CMake 4.0 or newer
- A C++23 compiler
- Ninja

## Configure and build

Configure a debug build:

```sh
cmake --preset debug
```

Build it:

```sh
cmake --build --preset debug
```

Release builds use the corresponding `release` presets.

To treat project warnings as errors, configure with:

```sh
cmake --preset debug -DEDIT_ATLAS_WARNINGS_AS_ERRORS=ON
```

## License

Edit Atlas is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).
