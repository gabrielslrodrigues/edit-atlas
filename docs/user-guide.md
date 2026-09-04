# Desktop user guide

Edit Atlas reads CMX 3600 edit decision lists and exports XLSX reports. All
timeline, video, settings, and diagnostic operations remain on the local
computer.

## Open and inspect a timeline

Open an EDL with **File → Open Timeline**, the platform open shortcut, or by
dropping it onto the window. A non-drop-frame EDL that does not declare its
frame rate prompts for one before import. Import warnings and errors retain
their source line numbers.

Select a column heading to sort the parsed events. Filters above the table can
match all or any configured conditions:

- text fields support literal or RE2 regular-expression matching, optional
  case matching, and whole-word matching;
- track kind and edit type use categorical values; and
- timecode and duration fields use exact typed values.

## Save reusable templates

The template controls save a named combination of filter conditions and
ordered export columns. Apply a template to later documents, or rename,
update, duplicate, and delete it. The interface marks the active template as
modified when its current filters or columns differ from the saved version.

Templates are stored only in the current user's application-data directory.

## Export a spreadsheet

Choose **Export Spreadsheet** and select the workbook language, optional
summary and diagnostic sheets, and event columns. Columns can be included,
excluded, and reordered. The standard timeline columns are selected by
default; **Initial frame** is optional.

The workbook language controls generated sheet names, headings, labels, and
document properties. Imported titles, identifiers, comments, paths,
timecodes, metadata keys, and diagnostic details remain unchanged. Numeric
values remain numeric cells.

An existing destination is never replaced without confirmation.

### Include initial frames

Selecting **Initial frame** requires a matching rendered video in MOV, MP4, or
MXF format. The video must have a constant frame rate and readable embedded
starting timecode. Its timecode mode, frame rate, start, and duration must
match the EDL record timeline.

Edit Atlas validates the video before export, extracts the frame at each
exported event's Record In timecode, and embeds the images in the workbook.
Extraction can be cancelled. Ordinary exports do not require a video.

## Language and appearance

English and Brazilian Portuguese are available from the **Language** menu.
Brazilian Portuguese is selected for a new profile, and the application
remembers later choices.

The **Appearance** menu offers System, Light, and Dark. System follows changes
to the operating-system appearance without restarting. Some desktop
environments control window decorations and native dialogs independently, so
their chrome may not change with the application content.

## Recent files and diagnostics

Recent-file history is disabled by default. Enabling **Remember Recent Files**
stores local paths in the platform settings. Disabling it clears the saved
history.

**Help → Export Diagnostic Logs** explains the contents of an offline support
bundle before asking for a destination. See [Diagnostic logging and support
bundles](diagnostic-support.md) for its exact contents and privacy limits.
