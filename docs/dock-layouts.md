# Aftermarket dock layouts

Save and restore native OBS dock arrangements using Qt dock state. Aftermarket does not depend on JR Dockie, replace existing docks, or alter scene, audio, or recording settings.

## Use

1. In OBS, open Tools > Aftermarket: Dock Layouts... after the application finishes loading.
2. Arrange the native OBS docks and choose Save current... Give the arrangement a unique name, such as Streaming, Editing, or Audio.
3. Select a saved snapshot and choose Switch to layout (or double-click it). Undo switch restores the previous dock arrangement, once.
4. Use Rename, Update from current (confirmed replacement), and Delete (confirmed removal) from the same dialog.
5. For a one-click layout macro, open Aftermarket: Action Strip..., choose Add... > layout, and select a saved arrangement. Turn on the Action Strip if you want the button in the OBS menu bar. Saved layout actions can be reordered and overflow like existing hotkey or dock buttons.

## Safety and scope

- Qt QMainWindow::saveState() captures dock placement, tabs, floating states, sizes, visibility, and window-owned toolbars as supported by Qt. It does not save scenes, sources, stream settings, or audio routing.
- Snapshots live in a separate versioned plugin_config/aftermarket/dock-layouts.json in the current OBS profile. Their UUIDs remain stable when renamed. The file is atomically saved with Qt QSaveFile, capped in size, and validated with a SHA-256 checksum per snapshot.
- Layout switching refuses a snapshot if one of its required dock IDs is missing. Failed Qt restoration triggers a best-effort rollback to the previous arrangement. Invalid or corrupt files cannot replace already loaded valid snapshots.
- One successful layout switch creates a single in-memory Undo state. No snapshot is automatically restored at startup. Emergency safe mode disables layout management alongside the other Aftermarket UI customizations.
- These files are tied to the available docks and Qt/OBS version, not guaranteed to transfer identically between systems. A dock supplied by another plugin must already be registered before its saved layout can load.

## Verification

The dock-layouts CTest covers native Qt dock positioning, persistence, rename, update, delete, missing-dock refusal, corrupted saves, and Undo. The layout-strip CTest covers visible one-click routing to a real Qt window, live rename, stale-target disable and persistent action identities. The complete test suite and a disposable real OBS test are required before installation. A native ephemeral OBS test plugin exercised the actual Tools action, Save current dialog, dock relocation, Switch to layout and Undo through the running Qt frontend; each passed and the isolated OBS process exited normally.

This is independent Aftermarket code built on Qt's public API, not an embedded copy of JR Dockie.
