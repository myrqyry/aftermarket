# Aftermarket

**Beyond factory spec.**

A native runtime UI modification engine for OBS Studio. Customize how OBS
looks, moves, and feels without replacing its native controls, forking OBS,
or changing the video compositor. Aftermarket complements Colorway:
the theme supplies the visual identity, and Aftermarket extends interaction,
layout, and presentation.

| Native mixer, reimagined | AudioBus routing |
| :---: | :---: |
| ![Compact rotary native mixer with dB readouts, side controls](docs/screenshots/readme/mixer-rotary-current.png) | ![The alternate AudioBus patchbay inside the Audio Mixer dock](docs/screenshots/readme/audiohub-activity-outlines.webp) |

[Audio and routing](#audio-and-routing) · [Workspace](#workspace) · [Sources and navigation](#sources-and-navigation) · [Menus](#menus) · [Appearance and motion](#appearance-and-motion) · [Install](#install-locally-linux) · [User guide](docs/user-guide.md)

> **Development preview.** Linux/X11, OBS 32.2.0 and Qt 6.4.2 are the
> locally exercised runtime target. No public binary release is established.
> [Build checks](https://github.com/myrqyry/aftermarket/actions) · [Install locally](#install-locally-linux)

## Audio and routing

### AudioBus

Turn the Audio Mixer into a two-sided dock. Click **AudioBus** at the bottom
to reveal a patchbay for OBS's native tracks 1–6 and monitoring; click
**Audio Mixer** to return to the original controls.

- Select a source and click a destination to toggle its native route, or drag
  a connection. All wires are shown by default.
- The headphones node controls the source's native monitoring mode.
- Distinct theme-derived outlines show selection and measured audio activity,
  without modifying signal flow.
- Five compact output loudness dots beside AudioBus: momentary, short-term, integrated, LRA and true peak, with track selection, target and reset in the mixer menu. Real EBU R128 measurements from bundled [libebur128](https://github.com/jiixyj/libebur128), not channel-peak estimates. Inspired by [Loudness Dock](https://github.com/norihiro/obs-loudness-dock/).
- The dock's three-dot menu contains routing options, refresh, and the
  original mixer settings.

| AudioBus in the current OBS build | Native monitoring menu |
| :---: | :---: |
| ![Live source and track activity in AudioBus](docs/screenshots/readme/audiobus-current.webp) | ![Native OBS monitoring modes](docs/screenshots/readme/audiohub-monitor-menu.webp) |

### Mixer controls

Keep the familiar native mixer or customize selected channels:

- Rotary gain controls with optional integrated peak rings.
- Per-source audio-reactive gradient faders and native meter color modulation.
- Compact Pan and Sync controls that operate native OBS source properties.
- Channel widths independent of source-name length, with readable labels.
- Compact rotary mixer channels in both orientations: native dB value below gain, orientation-aware control pairs spaced to the gain face (left/right in stacked channels, above/below in side-by-side channels), title state styling and a right-aligned three-dot source menu in the header. Pan and sync use 32px theme-aware single-scale faces (no center pointer on sync) and the gain knob’s relative drag direction. Endless relative sync encoder (Shift for fine adjustment); right-click for exact milliseconds. Double-click any mixer knob to reset (gain 0 dB, pan center, sync 0 ms); reset actions are also available in the knob context menus.
- Responsive mixer grid: up to four columns for horizontal-fader rows, and automatic wrapping of compact side-by-side rotary strips when all rows fit. Auto mixer grid toggle in the title menu.
- Persistent channel order: unlock in the mixer title menu, drag source names,
  then lock again. AudioBus follows the same saved order; reset restores native order.

| Responsive rotary mixer (wide dock) | Native gradient faders |
| :---: | :---: |
| ![Four-column rotary mixer with native channels](docs/screenshots/readme/mixer-rotary-grid.png) | ![Native OBS faders with gradient tracks](docs/screenshots/readme/mixer-faders.png) |

![Side-by-side rotary channels wrap into rows when dock height allows](docs/screenshots/readme/mixer-rotary-columns.png)

The rotary screenshots show the revised single-scale side controls, reclaimed channel header, and compact grid in isolated OBS 32.2.0 sessions using the native dark theme and Colorway Fluffy Dark. To adjust sync, drag up/right or down/left, or scroll (10 ms per pixel of combined drag or wheel detent), hold Shift for 1 ms, or right-click for exact entry. The indicator turns continuously; the actual offset is clamped, not wrapped.

**Optional independent device sends:** install [Exeldro's Audio Monitor](https://obsproject.com/forum/resources/audio-monitor.1186/) to expose playback devices below the native track nodes. Drag a source to a device; right-click its wire for independent mute, gain, pan and delay. Choose visible destinations from the mixer title menu. Existing Audio Monitor filters are not taken over by Aftermarket. Without the plugin, the native graph works normally.

The mixer still belongs to OBS. AudioBus does not implement a second audio engine or DSP graph; optional device sends use the Audio Monitor plugin's own filters. [Detailed mixer guide](docs/user-guide.md#mixer-and-audiobus).

## Workspace

The main **Tools → Aftermarket → Workspace** window contains seven tabs:
Menus & widgets, Mixer, Gradients, Panels, Action strip, Dock layouts and
Cue Deck. Quick Find, Command Center and Emergency restore are the other Tools
commands. Routine preferences remain in OBS Settings. The Workspace uses compact native sections, a plain theme-palette surface, context-specific controls, searchable mixer sources, and a two-pane cue-step preview.

### Docks and layouts

Native dock titles get theme-aware icons and compact three-dot menus for
supported collapse, float, and close operations. Roll up a dock without
destroying its contents. Restore the original preview or make it a movable
native dock, including responsive Studio Mode.

When the Colorway theme package provides its plugin-icon registry, Aftermarket
uses the registered source/filter types and exact dock IDs (or the verified SourceDock and QuickAccessDock widget classes for dynamically named docks) to display the custom
glyphs in its source picker, Source Inspector, Quick Find, dock title bars,
and the native Sources list and Filters dialog. Native Sources rows reuse
the existing icon slot; filter rows receive a mouse-transparent glyph beside
their native name. The exact native source model name resolves to a libobs
source, and native filter item data resolves directly to a libobs filter.
Selection changes follow the view palette; emergency recovery restores
original pixmaps and removes only Aftermarket-owned filter glyphs.
The registered monochrome artwork is tinted at runtime to the actual Qt
foreground, selected text or disabled text color, preserving its alpha and
scaling. Palette changes retint existing decorations without altering list
selection or filter settings. Colorway still ships both fixed-ink polarities
for standalone use. See [Colorway plugin icons](https://github.com/myrqyry/colorway/blob/main/docs/PLUGIN_ICONS.md) for the artwork and exact registration coverage. Unknown IDs and missing assets retain the original or
generic icon. No external plugin is required
or patched, and the stock OBS source tree is not rewritten.

Save complete dock arrangements with **Tools → Aftermarket → Workspace → Dock layouts**.
Switch between saved arrangements, or undo the last switch. Layout snapshots
preserve native dock placement, not scene or audio settings.

**Workspace checkpoints** extend the layout manager with named local recovery bundles:
the freshly saved OBS scene collection, native dock state, and selected Aftermarket settings.
Restore docks independently or export scene/settings files for deliberate recovery.
No live scene collection or settings are overwritten.
If [Scene Collection Manager](https://obsproject.com/forum/resources/scene-collection-manager.1434/)
is loaded, optionally create a verified companion backup in its native backup library and open
its manager directly from Dock Layouts.

Optional [Source Dock](https://obsproject.com/forum/resources/source-dock.1317/) panels
receive the native Aftermarket header controls and a shortcut to Source Docks settings.
Use **Pin to workspace** in Quick Find to create an actual Source Dock without switching scenes.
The optional Source Dock workspace-api fork supplies a native pinning API,
independent SDR preview refresh (Live, 30, 15, 10, 5, 2, 1 FPS or Freeze),
controls-only mode and cached resolution (2048, 1280, 640 or 320 px).
Hidden previews release their cache and showing reference. Stock 0.5.1
supports the guarded pinning adapter but not these new preview options.
The fork retains upstream module identity and license; no public fork
release is established.
Optional [Frame Interleave Filter](https://obsproject.com/forum/resources/frame-interleave-filter.1618/)
adds **Off-air frame limit** to eligible asynchronous video sources in the Quick Find
context menu; the original filter handles activity-aware frame dropping.
Workspace checkpoints capture Source Dock settings and Frame Interleave filters through the native scene collection. Neither integration replaces the underlying source or Program renderer.

| Native dock headers | Saved dock arrangements |
| :---: | :---: |
| ![Native OBS docks with compact title controls](docs/screenshots/readme/dock-all-overflows.webp) | ![Workspace Dock layouts tab with saved arrangements](docs/screenshots/readme/dock-layouts.webp) |

| Compact Studio Mode | Wide floating Studio Mode |
| :---: | :---: |
| ![Stacked native Preview and Program](docs/screenshots/readme/studio-compact.png) | ![Side-by-side native Preview and Program with horizontal transition strip below](docs/screenshots/readme/studio-wide.png) |

### Action Strip

Put useful controls in the existing menu bar: OBS and plugin hotkeys,
dock show/hide buttons, live status, saved dock layouts, Cue Deck sequences, and Quick Find.
Reorder them directly or use the editor. Extra items move to overflow instead
of being clipped. The optional hamburger menu frees additional space.

![Configurable native OBS Action Strip](docs/screenshots/readme/action-strip.webp)

[Workspace guide](docs/user-guide.md#workspace-and-dock-controls) ·
[Dock layouts reference](docs/dock-layouts.md)

### Cue Deck

Use **Tools → Aftermarket → Workspace → Cue Deck** to create named sequences of saved
dock-layout changes, native OBS or plugin hotkeys, and timed waits. Reorder
steps, run or stop a cue, and optionally pin it to the Action Strip.
Existing plugin hotkeys—including Exeldro Move actions when registered by
OBS—can be used without replacing their effects. Cue Deck checks for missing
actions before running, and runs only when you explicitly invoke a cue. Hotkey
actions themselves can change what's on air.

[See Cue Deck instructions](docs/user-guide.md#cue-deck)

## Sources and navigation

### Compact Add Source

On compatible OBS 32.2 installations, replace the oversized source chooser's
layout with a compact panel beside Sources.

- **Add new:** click a source type once to create it and open OBS Properties.
- **Add existing:** browse compact source thumbnails, filter by type and
  source name, select a result, and use OBS's native Add existing action.

Both modes share the native visibility option. OBS retains source identity,
creation, selection, undo, and Properties. The original standalone chooser
remains available in Settings.

![Compact Add new and filtered Add existing views](docs/screenshots/readme/embedded-source-picker.webp)

### Command Center and Source Inspector

The centered menu-strip field opens a keyboard-navigable command palette.
Use `@` to find sources, scenes and filters, or `/` for Aftermarket commands
and registered OBS hotkeys. Hotkeys require explicit confirmation. Source
results pin the optional native Source Inspector dock without changing scenes.
Open **Docks → Source Inspector** to follow source selection, see the scene-item
transform, save/load named transform presets, add/search/toggle/reorder/remove
filters and check scene references, or open native Properties and Filters.
Transform loads and filter edits participate in OBS Edit → Undo/Redo. [Command center guide](docs/user-guide.md#command-center-and-source-inspector).

### Contextual notes

Private notes attach to scenes, shared sources, filters or specific scene
items. Right-click a native Scenes or Sources row and choose Add note; its
small information glyph appears on the row when a note exists. In Quick
Find, right-click a result and choose Add note; in the
command center, right-click an @ result. The Source Inspector supports notes
on its source heading, transform context and filter rows. A small circled
information symbol appears only when a note exists: hover to read the native
tooltip, click to edit. Search scene, source and filter notes through @.
No notes dock or broadcast source is required.

| Native Sources note marker | Native Scenes note marker |
| :---: | :---: |
| ![Source note marker beside visibility and lock](docs/screenshots/readme/source-notes.png) | ![Scene note marker at the trailing edge](docs/screenshots/readme/scene-notes.png) |

![Native note tooltip](docs/screenshots/readme/note-tooltip.png)

Notes live in a local, owner-only `context-notes.json` file, separate from
scene exports and workspace checkpoints. Back it up independently.
Scene grid mode and scene rows without enough spare width retain the
context-menu action without a marker. [Notes guide](docs/user-guide.md#contextual-notes).

### Quick Find

Click outside either the compact Add Source picker or the Quick Find popup to dismiss it.

Open **Tools → Aftermarket → Find Sources…** to search the current scene
collection by source, scene, filter, type, file path, or URL. Open native
Properties or Filters, or jump to the scene. Studio Mode navigation targets
the preview rather than the on-air program. Assign an OBS hotkey or add the
palette to the Action Strip for faster access.

![Quick Find returns a source and its existing filter](docs/screenshots/readme/quick-find.webp)

[Source and navigation guide](docs/user-guide.md#sources-and-navigation)

## Menus

Long choice menus use balanced compact columns while keeping every native
action available. The ordinary Source context menu stays a familiar single
column. Popups remain on their invocation monitor; nested menus align to
their triggering row, including on offset dual-monitor layouts.

| Native source menu | Augmented source menu |
| :---: | :---: |
| ![Default OBS source context menu](docs/screenshots/readme/menu-disabled.png) | ![Aftermarket menu icons](docs/screenshots/readme/menu-enabled.png) |

![A compact Filters Add submenu attached to its actual parent menu](docs/screenshots/readme/filter-submenu-anchored.webp)

Native Qt tooltips have a configurable maximum width (360 logical px by default), with automatic wrapping and a native unlimited option.

Compact columns are reserved for flat choice lists. Menus containing visible submenus (including More actions) retain native layout and compact row spacing.

Customize specific native menus in Workspace → Menus & widgets: add icons, hide
unwanted commands, explicitly move entries into More actions, add trusted
executable commands, and display checked submenu values. The searchable
theme/style picker makes long Appearance lists easier to navigate.

[Menu customization and live rules](docs/user-guide.md#menus-and-rules)

## Appearance and motion

Aftermarket works with the existing OBS theme rather than imposing a new
palette. Optional refinements include:

- Alternating Scenes and Sources row backgrounds with adjustable strength.
- Theme-colored transform selection frames and handles.
- End or middle ellipses for long labels, while preserving actual names.
- Hover-revealed dock overflow buttons, including the Audio Mixer; keyboard access and open-menu feedback remain available without shifting titles.
- Theme-aware Settings gear at the far-right edge of the native menu bar, independent of the optional Action Strip.
- Responsive source/filter property columns (Auto, 1, up to 2, or up to 3), including independently reflowing collapsible native property groups.
- Named source/filter property and whole filter-chain presets directly in the native Defaults/OK/Close footer, with portable JSON import/export.
- Gradient tracks and optional native dock titlebar accents or full-header gradients: Flow, Reverse, Ping-pong, Breathing, Shimmer and Static, using one shared palette, loop and clock.
- Animated menu artwork (soft, pop, spin, fade and lift), dock collapse, and a two-sided perspective flip between Audio Mixer and AudioBus. Aftermarket-owned dock, Studio and Action Strip toggle controls have subtle animated state feedback. Bounded elapsed-time steps, partial repaint regions and idle timers reduce GUI work. Disable Motion for immediate transitions.

| List backgrounds | Transform selection |
| :---: | :---: |
| ![Alternating rows in OBS lists](docs/screenshots/readme/alternating-scenes-sources.webp) | ![Theme-colored source selection and transform handles](docs/screenshots/readme/transform-modern.png) |

![Native Color Correction filter settings reflowed into three columns](docs/screenshots/readme/responsive-filter-properties.webp)

![Aligned label and editor columns inside a native property section](docs/screenshots/readme/responsive-section-controls.png)

The section detail is cropped from a real isolated native properties
dialog using a synthetic Camera Controls group, not a physical camera.

**Property presets:** choose a saved preset, Load, Save a new preset, or use
the overflow menu to update, rename, delete, or undo the last load. Presets
are scoped to the exact source/filter type. In Filters, toggle **Whole filter chain** in the overflow menu to save or replace an ordered chain, including settings and enabled state. Import/export supports both preset scopes. Native Defaults and source
OK/Cancel remain intact. See [property presets](docs/user-guide.md#named-source-and-filter-presets).

[Appearance and motion guide](docs/user-guide.md#appearance-and-motion)

## Settings and Workspace

Open **OBS → Settings → Aftermarket** for normal preferences.
Use the native Apply, OK, and Cancel buttons. The five pages are
Appearance, Menus, Mixer, Gradients, Panels, and Action strip. Compact native sections keep related controls together; Gradients includes an editable theme-aware palette preview and color picker.

![Aftermarket's native OBS Settings page](docs/screenshots/readme/aftermarket-obs-settings.webp)

All Aftermarket tools are grouped in a single **Tools → Aftermarket** submenu. For advanced per-target rules, open **Tools → Aftermarket → Workspace**.
Capture an actual menu or widget, browse its native identity, and customize
supported targets. Detailed gradient and mixer editors are available there too.

## Install locally (Linux)

Requires matching libobs and OBS frontend API development packages,
Qt 6 Widgets and SVG, CMake, Ninja, and a C++17 compiler. Linux tests also
require Xvfb and xauth for isolated native popup geometry checks. Point CMake
to the development libraries matching your installed OBS.

```sh
cmake -S . -B build -G Ninja -Dlibobs_DIR=/usr/local/lib/cmake/libobs -Dobs-frontend-api_DIR=/usr/local/lib/cmake/obs-frontend-api
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/install.sh
```

Restart OBS. Existing themes and configuration are preserved by the installer. For same-name upgrades, back up the previous module outside the plugin directory before installing. Optional companion plugins are installed separately.
The module and configuration directory are now named aftermarket. The Linux installer moves the legacy obs-ui-augment package outside OBS’s search path; existing settings migrate as a complete directory on the next launch. See the [upgrade guide](docs/user-guide.md#upgrading-from-obs-ui-augment).

**Compatibility:** Runtime verification currently covers Linux/X11,
OBS 32.2.0 and Qt 6.4.2. The native column layout adapter targets Qt
6.4.2 and 6.10.2, requiring the
matching Qt private development headers (qt6-base-private-dev on Debian/Ubuntu)
and runtime; other versions retain compact native rows. Other platforms,
DPI configurations, and OBS versions need additional verification.

## Build and verification

The [shared CI workflow](.github/workflows/ci.yml) selects eight OBS
32.2.0 and 33.0.3 build targets: Linux x86-64, Windows x64, macOS Intel
and macOS Apple Silicon. Both Linux legs require all 46 native tests
without skips. Windows and macOS build and inspect production packages;
native interactive runtime qualification remains separate.
Packages are downloadable CI artifacts, not automatic releases.

At commit `77a6d4a`, [hosted run 45929248597](https://github.com/myrqyry/aftermarket/actions/runs/45929248597)
produced seven successful native binary packages. Linux OBS33 compiled
but failed four runtime regression tests. The follow-up addresses
the OBS33 monitoring API change, compact native dock geometry and
Qt 6.10.2 menu adapter. The complete local OBS32/Qt6.4.2 suite passed
46/46; the new hosted OBS33 result remains to be verified.

For a local build using matching development headers, use the commands
above. The same CI validation driver can also run against an explicit
installed SDK (see [CI guide](docs/ci.md)). The temporary module-only
archive is for verification and testing, not an automatic release.

## Recovery and documentation

**Ctrl+Alt+Shift+F12** or **Tools → Aftermarket → Emergency restore** restores
native UI presentation without deleting your Aftermarket settings, scenes,
or routing. The safe-mode marker persists across restart. Resume explicitly
when ready.

- [User guide](docs/user-guide.md) — detailed controls, configuration, runtime behavior, and limitations.
- [Dock layouts](docs/dock-layouts.md) — saving, switching, rollback, and compatibility.
- [Verification and development history](ACTIVE_WORK.md) — implementation checkpoints and real runtime evidence.

Screenshots are captured from real, isolated OBS sessions with native themes
and Colorway. The rotary mixer, contextual notes and responsive section detail were refreshed on November 25, 2026. Other captures illustrate their verified feature state and may predate subsequent UI refinements. The project is independent of OBS Studio and does not replace its
theme system or streaming compositor.
