# Scene Tree View 1.0.9

## Plan / Design

- Bulk folder colors and bulk scene icon colors, scoped to selected rows of the context target's kind.
- Shared folder removal confirmation for toolbar, Delete key and context menu. Cancel is the default.
- Keep contents promotes immediate children; remove nested layout discards the tree subtree only. OBS scenes reappear at root, never removed from OBS.
- Sequential, collision-free default folder names across a canvas, including nested folders.
- Preserve layout lock, preview/program behavior, native OBS scene order and folder double-click expansion.
- Delegate store naming and regression tests; integrate UI locally.

## Safety / Check Plan

- Capture paths by value and compare store snapshot after modal dialogs before applying operations.
- Normalize ancestor/descendant selections and delete in reverse path order.
- Restrict Delete shortcut to the tree widget, excluding inline name editors and search.
- Build plugin, run store regression tests, check locale keys and ZIP layout.
- Do not install into or automate the broadcasting OBS instance.

## Result

- Windows x64 RelWithDebInfo plugin build passed (OBS 32.0.2 dependencies).
- CTest passed: 1 executable, including numbering, bulk color persistence, subtree removal and dissolution safety regressions.
- Locale validation: 10 locales, 43 baseline keys, zero hard issues.
- Subagent read-only UI safety review found no concrete safety defects in selection normalization, modal snapshot checks or bulk color application.
- Plugin-only ZIP verified; archived DLL SHA-256 matches final build.
- Installed OBS was not modified. Live OBS mouse interactions and on-air behavior were not tested; verify offline before broadcast.
