# Aftermarket

**Beyond factory spec.**

A native runtime UI modification engine for OBS Studio. Customize how OBS
looks, moves, and feels without replacing its native controls, forking OBS,
or changing the video compositor. Aftermarket complements Colorway:
the theme supplies the visual identity, and Aftermarket extends interaction,
layout, and presentation.

| Native mixer, reimagined | AudioBus routing |
| :---: | :---: |
| ![Native gain controls with integrated peak rings](docs/screenshots/readme/mixer-rings.png) | ![The alternate AudioBus patchbay inside the Audio Mixer dock](docs/screenshots/readme/audiohub-activity-outlines.webp) |

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
- Persistent channel order: unlock in the mixer title menu, drag source names,
  then lock again. AudioBus follows the same saved order; reset restores native order.

![Native OBS faders with gradient tracks](docs/screenshots/readme/mixer-faders.png)

**Optional independent device sends:** install [Exeldro's Audio Monitor](https://obsproject.com/forum/resources/audio-monitor.1186/) to expose playback devices below the native track nodes. Drag a source to a device; right-click its wire for independent mute, gain, pan and delay. Choose visible destinations from the mixer title menu. Existing Audio Monitor filters are not taken over by Aftermarket. Without the plugin, the native graph works normally.

The mixer still belongs to OBS. AudioBus does not implement a second audio engine or DSP graph; optional device sends use the Audio Monitor plugin's own filters. [Detailed mixer guide](docs/user-guide.md#mixer-and-audiobus).

## Workspace

### Docks and layouts

Native dock titles get theme-aware icons and compact three-dot menus for
supported collapse, float, and close operations. Roll up a dock without
destroying its contents. Restore the original preview or make it a movable
native dock, including responsive Studio Mode.

Save complete dock arrangements with **Tools → Aftermarket: Dock Layouts…**.
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
Optional [Frame Interleave Filter](https://obsproject.com/forum/resources/frame-interleave-filter.1618/)
adds **Off-air frame limit** to eligible asynchronous video sources in the Quick Find
context menu; the original filter handles activity-aware frame dropping.
Workspace checkpoints capture their scene-collection configuration without replacing
Source Dock's rendering or controls.

| Native dock headers | Saved dock arrangements |
| :---: | :---: |
| ![Native OBS docks with compact title controls](docs/screenshots/readme/dock-all-overflows.webp) | ![Dock Layouts manager with a saved arrangement](docs/screenshots/readme/dock-layouts.webp) |

| Compact Studio Mode | Wide floating Studio Mode |
| :---: | :---: |
| ![Stacked native Preview and Program](docs/screenshots/readme/studio-compact.png) | ![Side-by-side native Preview and Program with horizontal transition strip below](docs/screenshots/readme/studio-wide.png) |

### Action Strip

Put useful controls in the existing menu bar: OBS and plugin hotkeys,
dock show/hide buttons, live status, saved dock layouts, and Quick Find.
Reorder them directly or use the editor. Extra items move to overflow instead
of being clipped. The optional hamburger menu frees additional space.

![Configurable native OBS Action Strip](docs/screenshots/readme/action-strip.webp)

[Workspace guide](docs/user-guide.md#workspace-and-dock-controls) ·
[Dock layouts reference](docs/dock-layouts.md)

### Cue Deck

Use **Tools → Aftermarket: Cue Deck…** to create named sequences of saved
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

### Quick Find

Open **Tools → Aftermarket: Find Sources…** to search the current scene
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

Customize specific native menus through the Inspector: add icons, hide
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
- Collapsible native property groups.
- Animated gradient tracks, menu artwork, dock collapse, and a brief
  AudioBus half-turn. Disable interface animations for immediate transitions.

| List backgrounds | Transform selection |
| :---: | :---: |
| ![Alternating rows in OBS lists](docs/screenshots/readme/alternating-scenes-sources.webp) | ![Theme-colored source selection and transform handles](docs/screenshots/readme/transform-modern.png) |

![Real OBS dock collapse and AudioBus flip animation frames](docs/screenshots/readme/ui-motion-docks-and-audiobus.webp)

[Appearance and motion guide](docs/user-guide.md#appearance-and-motion)

## Settings and inspector

Open **OBS → Settings → Aftermarket** for normal preferences.
Use the native Apply, OK, and Cancel buttons. The five pages are
Appearance, Menus, Mixer, Panels, and Action strip.

![Aftermarket's native OBS Settings page](docs/screenshots/readme/aftermarket-obs-settings.webp)

For advanced per-target rules, open **Tools → Aftermarket: Inspector**.
Capture an actual menu or widget, browse its native identity, and customize
supported targets. Detailed gradient and mixer editors are available there too.

## Install locally (Linux)

Requires matching libobs and OBS frontend API development packages,
Qt 6 Widgets and SVG, CMake, Ninja, and a C++17 compiler. Point CMake to
the development libraries matching your installed OBS.

```sh
cmake -S . -B build -G Ninja -Dlibobs_DIR=/usr/local/lib/cmake/libobs -Dobs-frontend-api_DIR=/usr/local/lib/cmake/obs-frontend-api
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/install.sh
```

Restart OBS. Existing themes and configuration are preserved by the installer.
The module and configuration directory are now named aftermarket. The Linux installer backs up the previous plugin outside OBS’s search path; existing settings migrate as a complete directory on the next launch. See the [upgrade guide](docs/user-guide.md#upgrading-from-obs-ui-augment).

**Compatibility:** Runtime verification currently covers Linux/X11,
OBS 32.2.0 and Qt 6.4.2. The native column layout adapter requires the
matching Qt private development headers (qt6-base-private-dev on Debian/Ubuntu)
and runtime; other versions retain compact native rows. Other platforms,
DPI configurations, and OBS versions need additional verification.

## Build and verification

The [Linux CI workflow](.github/workflows/ci.yml) targets Ubuntu 24.04 and
a SHA-256-pinned OBS 32.2.0 SDK. It checks local README links, builds the
module, runs the native Qt/libobs CTest suite under an isolated X display
(at least 32 tests, no skips), stages the plugin, checks linked libraries
and the exact package contents, and verifies the archive checksum.
It does not install into a user's profile, publish release assets, or
claim Windows, macOS, or OBS 33 support.

| Target | Verification boundary |
| --- | --- |
| Linux x86_64 / OBS 32.2 / Qt 6.4.2 | Local native tests and isolated OBS GUI exercises |
| Ubuntu 24.04 / pinned OBS 32.2 SDK | Local CI driver passed; hosted result pending |
| Windows, macOS, OBS 31/33 | Not qualified |

For a local build using matching development headers, use the commands
above. The same CI validation driver can also run against an explicit
installed SDK (see [CI guide](docs/ci.md)). The temporary module-only
archive is for verification, not an authorized distributable release.

## Recovery and documentation

**Ctrl+Alt+Shift+F12** or **Tools → Aftermarket: Emergency restore** restores
native UI presentation without deleting your Aftermarket settings, scenes,
or routing. The safe-mode marker persists across restart. Resume explicitly
when ready.

- [User guide](docs/user-guide.md) — detailed controls, configuration, runtime behavior, and limitations.
- [Dock layouts](docs/dock-layouts.md) — saving, switching, rollback, and compatibility.
- [Verification and development history](ACTIVE_WORK.md) — implementation checkpoints and real runtime evidence.

Screenshots are captured from real, isolated OBS sessions with native themes
and Colorway. The project is independent of OBS Studio and does not replace its
theme system or streaming compositor.
