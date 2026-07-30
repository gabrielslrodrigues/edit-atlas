# Diagnostic logging and support bundles

Edit Atlas writes application logs to a private `logs` directory below the
platform-provided per-user application-data directory. Logging initialization
never blocks application startup: if the persistent file sink cannot be
created, terminal logging remains available.

Each log file is limited to 1 MiB. The active file and its rotations are capped
at five files in total, and recognized log files older than 14 days are removed
at startup. Only files named `edit-atlas.log` or
`edit-atlas.<rotation>.log` participate in retention and support-bundle export.

The application records operational events and these fixed diagnostic fields:

- Edit Atlas version
- operating-system description
- CPU architecture
- Qt version
- selected Qt platform plugin
- registered import and export format identifiers

Each entry includes the source filename and line number. Only the filename is
recorded; full source paths and build-directory paths are not included.

Logs must not contain imported document contents, spreadsheet contents, media
contents, environment-variable values, secrets, or telemetry.

## Exported support bundles

**Help → Export Diagnostic Logs** explains the bundle contents before asking
for a local destination. The generated ZIP contains only:

- `environment.txt`, a human-readable summary of the fixed fields above
- recent recognized logs under `logs/`

Files with any other name are ignored even if they are placed in the log
directory. Timelines, spreadsheets, source media, environment variables, and
crash dumps are never collected automatically. Bundle creation is entirely
offline and does not upload or submit anything.
