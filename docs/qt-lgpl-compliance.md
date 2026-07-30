# Qt LGPL compliance policy

Edit Atlas uses the open source edition of Qt under the GNU Lesser General
Public License version 3. This document records the project's technical policy;
it is not legal advice.

## Linkage policy

All supported desktop targets dynamically link Qt. The inherited platform and
architecture presets explicitly select project-owned dynamic target triplets,
while vcpkg detects the native host triplet. CMake rejects Qt Core, GUI,
Widgets, or Concurrent imported as static libraries.

The supported presets are:

| Target | Debug | Release |
| --- | --- | --- |
| Linux x64 | `debug-x64-linux` | `release-x64-linux` |
| macOS ARM64 | `debug-arm64-osx` | `release-arm64-osx` |
| macOS x64 | `debug-x64-osx` | `release-x64-osx` |
| macOS Universal | `debug-universal-osx` | `release-universal-osx` |
| Windows x64 | `debug-x64-windows` | `release-x64-windows` |

Qt platform plugins must remain dynamically loaded. The install-time Qt
deployment script copies the required Qt libraries and plugins into the staged
application.

## Binary distribution checklist

Every binary release must:

1. Include `THIRD_PARTY_NOTICES.md` and the `qtbase` copyright file installed
   from the resolved vcpkg package.
2. Include the LGPL version 3 terms contained in that copyright material and
   prominently identify Qt as LGPL software.
3. Publish the complete corresponding source for the exact Qt build,
   including all vcpkg patches, as an Edit Atlas-controlled release asset. A
   link to an upstream Qt download is not sufficient.
4. Keep Qt libraries and plugins replaceable by interface-compatible modified
   builds. Packaging, signing, or access controls must not prevent users from
   installing and running such replacements.
5. Preserve all license and attribution notices for Qt's bundled third-party
   components.
6. Re-run linkage verification on every supported release artifact.

The packaging and release workflows must fail rather than publish an artifact
that is missing any required compliance material.

## Verification

CMake configuration is the first enforcement layer:

```sh
cmake --preset release-x64-linux
```

Configuration fails if `Qt6::Core`, `Qt6::Gui`, `Qt6::Widgets`, or
`Qt6::Concurrent` is not a shared library target. Release CI must additionally
inspect the final packaged binary:

- Linux: `readelf -d` or `ldd` must report `libQt6Widgets.so`.
- macOS: `otool -L` must report a dynamic Qt Widgets library or framework.
- Windows: `dumpbin /dependents` must report `Qt6Widgets.dll`.

CI performs these checks on build-tree binaries, staged installations, and
the extracted or installed contents of every generated package. It also
verifies that every Mach-O file installed by the macOS package contains both
x86_64 and ARM64 slices.

## References

- [Qt open source licensing FAQ](https://www.qt.io/faq/qt-open-source-licensing)
- [Qt GPL and LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations)
- [Qt application deployment with CMake](https://doc.qt.io/qt-6/qt-generate-deploy-app-script.html)
