# Scene Tree View 1.0.10

## Plan / Design

- Mark collapsed ancestors of the current program scene with the existing muted red on-air highlight and scene-name tooltip.
- Find program expands ancestors and centers the row without selecting or switching a scene; clears search to reveal hidden results.
- Export named JSON backups using QSaveFile atomic replacement. Save structure, aliases, colors, order and expanded state, not OBS sources.
- Restore only into the original scene collection after explicit confirmation. Layout lock blocks restore, but allows backup and find.
- Validate size (8 MiB), schema, version, depth (64) and node count (50,000) before parsing tree nodes. Restore through a temporary store so failure does not mutate layout.
- Match existing scenes, prune absent ones and append new scenes at root. Do not create, remove, rename or switch OBS scenes.
- Parent owns UI integration; subagent owns import validation and regression tests.

## Check / Act

- Windows x64 RelWithDebInfo build passed; final incremental build verified.
- CTest passed, including successful restore, aliases/colors/expansion, absent scenes, invalid schema/version/canvas, depth/node/byte limits and preservation of original state on failure.
- Locale validation passed: 10 locales, 52 keys, zero hard issues.
- Subagent static review found no concrete safety defects in program navigation, ancestor markers or backup/restore modal handling.
- Plugin-only ZIP structure and archived DLL hash verified against final build.
- Installed OBS was not modified. Actual OBS UI interactions and broadcast behavior have not been tested; verify in an offline session before broadcast.
