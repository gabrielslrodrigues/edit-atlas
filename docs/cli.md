# Command-line interface

`edit-atlas-cli` is the Qt-free terminal frontend for the same local CMX 3600
EDL to XLSX conversion workflow provided by the desktop application. It invokes
the shared document import and export services and performs no network
operations.

## Convert an EDL

A non-drop-frame EDL requires its exact frame rate:

```sh
edit-atlas-cli convert --fps 24 timeline.edl report.xlsx
edit-atlas-cli convert --fps 30000/1001 timeline.edl report.xlsx
```

Drop-frame EDLs default to `30000/1001` when `--fps` is omitted:

```sh
edit-atlas-cli convert broadcast.edl report.xlsx
```

The input format is detected through the built-in format registry. The output
uses the registered XLSX exporter. The CLI does not implement either format
itself.

Run `edit-atlas-cli --help` for the command summary,
`edit-atlas-cli convert --help` for conversion options, and
`edit-atlas-cli --version` for the installed version.

Use `--` to end option processing when an input or output filename begins with
a hyphen:

```sh
edit-atlas-cli convert --fps 24 -- -input.edl -report.xlsx
```

## Workbook options

Brazilian Portuguese is the default workbook language, matching the desktop
application's initial language. Select English with:

```sh
edit-atlas-cli convert \
  --fps 24 \
  --language en \
  timeline.edl \
  report.xlsx
```

Accepted language tags are `en` and `pt-BR`. Imported titles, identifiers,
comments, paths, timecodes, metadata, and diagnostic messages remain unchanged.

The XLSX exporter includes its timeline summary and diagnostics sheets by
default. Either can be omitted:

```sh
edit-atlas-cli convert \
  --fps 24 \
  --no-timeline-sheet \
  --no-diagnostics-sheet \
  timeline.edl \
  report.xlsx
```

These flags affect the optional summary and diagnostics sheets; the event
report remains present.

Choose an ordered subset of event columns with their stable identifiers:

```sh
edit-atlas-cli convert \
  --fps 24 \
  --columns event,reel,clip-name,comments \
  timeline.edl \
  report.xlsx
```

Available identifiers are `event`, `reel`, `track-kind`, `track`, `edit-type`,
`transition`, `transition-frames`, `source-in`, `source-out`, `record-in`,
`record-out`, `duration-frames`, `clip-name`, `source-file`, `comments`, and
`source-line`. The default includes all columns in that order. Identifiers are
always English, lowercase integration values regardless of workbook language.
Each identifier may occur only once.

## Existing destinations

Conversion never overwrites an existing destination implicitly. If
`report.xlsx` already exists, the command exits with code `4` and preserves
the file. Authorize atomic replacement explicitly with:

```sh
edit-atlas-cli convert --force --fps 24 timeline.edl report.xlsx
```

## Diagnostics and exit codes

Successful conversion writes a one-line summary to standard output. Warnings
and errors are written to standard error with their severity, stable code, and
`source:line:column` location when available.

| Code | Meaning |
| ---: | --- |
| `0` | Conversion completed without warnings or errors |
| `1` | Conversion completed and created the workbook, but reported warnings |
| `2` | The command line is invalid |
| `3` | The readable input contains import errors; no workbook was created |
| `4` | The destination exists and replacement was not authorized |
| `5` | A filesystem or application operation failed |

Use diagnostic codes rather than matching their human-readable messages in
automation.

## Executable locations

The build-tree executable is located at `src/cli/edit-atlas-cli` below the
selected preset directory, with `.exe` appended on Windows. Installed Linux
packages place it in `/usr/bin`. The Windows MSI places it beside
`edit-atlas.exe`. The macOS package places it inside
`/Applications/edit-atlas.app/Contents/MacOS` so it remains part of the
application bundle.

## Privacy and encoding

Input and output files remain local and are never uploaded. Paths and imported
text are preserved as UTF-8. On Windows, the executable obtains arguments
through the native UTF-16 command line and converts them explicitly rather than
using the active legacy code page.
