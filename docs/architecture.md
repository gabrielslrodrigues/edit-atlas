# Architecture

Edit Atlas separates format processing from application workflows and user
interfaces:

```text
Qt desktop frontend ─┐
future CLI frontend ─┴─> application services ─┬─> core
                                               └─> built-in formats ─> core
```

## Core

`EditAtlas::Core` owns the format-independent timeline model, handler registry,
and in-memory import and export pipeline. It has no filesystem or user-interface
dependency.

## Formats

Format targets implement importers and exporters against the core interfaces.
They receive byte spans and return domain objects or export artifacts. They do
not open files, display messages, or schedule work.

## Application services

`EditAtlas::Services` is the reusable application boundary. It composes the
built-in format registry and coordinates filesystem operations with the core
pipeline using standard C++ types. A successful document-opening request
returns a `DocumentSession`; failures preserve their stage, path, filesystem
error, and import diagnostics. Filesystem failures retain their native
`std::error_code` instead of frontend-specific text.

The services target has no Qt dependency. Any executable can use document
opening and built-in format composition by linking `EditAtlas::Services`.

## Frontends

Frontends adapt platform-specific values at the application-services boundary.
The Qt desktop frontend owns dialogs, background scheduling, models, widgets,
translations, recent-file settings, and other desktop integration. A future
CLI can provide terminal input and output while invoking the same services.

Dependencies point inward: frontends may depend on application services, which
may depend on core and format targets. Core and format targets never depend on
application services or a frontend.
