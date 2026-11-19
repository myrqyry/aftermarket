# Active work

Outcome: first real slice of Tampermonkey for OBS — independent artwork applied to real context-menu actions.
Verified: CMake module build against local libobs and Qt 6.4.2; Qt regression suite; OBS 32.2.0 isolated Xvfb profile with actual Colorway Rosé Pine — Pine theme and assets; 22 decorated actions, on/off/on rules hot reload, real pointer-click Rename opened native inline editor, isolated process graceful shutdown. Before/after and rename screenshots in docs/screenshots.
Locked: OBS receives only the transparent layout reservation; actual SVG rendered by our plugin; no OBS data/stream changes; no fork. Preserve original action state and existing icons unless rule opts in.
Remaining (exploratory, not accepted MVP scope): external SVG registry, rule inspector GUI, non-menu widgets, richer layout/painting hooks, portability and varied DPI/RTL/scrolling validation. OBS shutdown reported 3 unassigned memory leaks, not yet attributed.

## Slice 2 — native inspector (2026-11-18)

Outcome: capture native OBS widget identities and author contextual menu augmentation rules through a real GUI. Verified: built and both Qt CTests passed; real OBS 32.2.0 isolated profile captured OBSSourceLabel ancestry, captured live context menu, saved Scale Filtering scoped override, reopened popup (12 magenta pixels observed), removed exact override while preserving broader default. Screenshots in docs/screenshots. Current user's running OBS session and rules file remain untouched until they restart to load the new module.
Selector design: use QAction objectName when available, otherwise normalized text; scope by QMenu objectName or up to two neighboring action labels. Anonymous-menu scoping is heuristic and locale dependent. Rule updates are atomic and reject invalid JSON.
Remaining: widget decoration (read-only widget inspector currently), user SVG imports, richer property bindings, true unique scope for anonymous menus, accessibility/DPI/platform validation. Prior 3 unattributed OBS shutdown leaks remain open.

## Slice 3 — motion (2026-11-18)

Outcome: independent animation of injected menu artwork, keeping native OBS behavior and geometry. Implemented opt-in global motion, per-action soft/pop/spin/none presets, duration 60–600 ms, capped stagger, cached DPR-aware SVG rendering, continuous hover retargeting, dirty icon-region repaints, and a timer that stops at rest/on close. Inspector controls use existing atomic JSON writer. Verified 3/3 Qt CTests, including transition continuity, pixel changes, idle/cancel behavior, GUI serialization and invalid duration rejection. Real OBS 32.2.0 isolated profile: enabled motion using inspector, saved scoped spin/600 ms rule, captured four entrance frames and changing hover icon pixels. No change to user rules file. Scope deliberately remains menu artwork, not animations of whole windows, native row highlights, source widgets, or the entire theme. No wider platform/DPI validation or measured performance claim.

## Slice 4 — animated gradient tracks (2026-11-18)

Outcome: independent gradient decoration for native QSlider (including media elapsed and mixer faders) and QProgressBar. Opt-in global gradient settings, native palette or 2–6 colors, 400–12000 ms loop, existing global motion switch. Native style geometry including State_Horizontal for QSS, thumb keeps native hit target, supports inversion and RTL, progress text preserved via a narrow accent stripe. Shared 30 Hz clock driven only by visible nonempty tracks, no animation when motion off. QFileSystemWatcher reloads valid atomic/external edits. Verified: Qt CTests 4/4 (including regression for QSS horizontal groove), isolated OBS 32.2.0 Colorway Rosé Pine: paused media elapsed text unchanged, seek track 1246 changed pixels, mixer fader 1222 changed pixels, native seeking changed elapsed label, live motion disable stable, live gradient disable restored native appearance. Still open: real OBS determinate progress dialog, custom-drawn non-QSlider/QProgressBar controls, broader platforms/DPI/RTL real runtime, performance profiling and earlier unattributed shutdown leaks. No live user rules changes intended.

## Slice 5 — per-source audio reactivity (2026-11-19)

Outcome: opt-in audio-reactive mixer fader gradients and native meter nominal-color modulation. Verified native OBS 32.2.0 VolumeControl/VolumeName/VolumeMeter source and installed SDK; source binding uses the full current VolumeName text, weak-source identity checks and a public libobs volmeter callback that only publishes numeric envelopes. Same 30 Hz UI clock; 55 ms attack, 220 ms release; muted, stale, and absent sources decay to idle. Meter uses current native theme nominal color, restores it on disable, leaves warning/error and disabled color properties untouched. No native volume/seek or video output edits. Verified Qt CTests 5/5 and real isolated OBS 32.2.0 Rosé Pine with tone/silence sources: independent fader pixels (1218 vs 0), meter nominal tint changed (325 pixels), sampled warning/clip and silent meter regions unchanged, native mute stops flow. GUI persists both flags; user settings preserved. Scope remains magnitude envelope, not FFT/beat sync, full gradient meter paint, or support for other OBS UI layouts/versions. Named OBS/Colorway var(--token) resolution discussed in preceding slice remains unimplemented; palette-derived gradients and native meter theme color remain supported. Performance profiling, rename/rebuild runtime stress and prior shutdown leak attribution remain open.

## Slice 6 — rotary mixer controls (2026-11-19)

Outcome: reversible rotary presentation for selected or all mixer faders. Original slider stays in native layout and handles values, focus, wheel/key behavior and slider signals; relative drag and independent rotary paint; minimum size restored if not externally modified. Knob source scope uses exact full VolumeName labels; empty list means all mixer sources. Independent enable flag, theme palette default and existing gradient/audio modulation available. Verified 6/6 Qt CTests and isolated OBS 32.2.0 Colorway Rosé Pine: selected Tone test knob only, silent fader unchanged, 242 animated ring pixels, native dB label -12.0 to -3.5 after drag, live disable restored slider at -3.5. Remaining: generic widget-targeted knob rules, actual horizontal mixer testing, rename/rebuild/theme geometry stress, compact custom layout, named theme tokens, full meter gradients, performance profiling and previous leak attribution. No live user rules changed.

## Slice 7 — collapsible dock panels (2026-11-19)

Outcome: session-only dock roll-up controlled by middle-click on the native
title bar and Tools → UI Augment Panels. Original contents retain identity,
temporary height constraint rolls dock up; width minimum avoids Qt shrinking
the whole column when contents are hidden. Expand restores original constraints
and requests original dimensions; exact sibling geometry is not guaranteed.
Guard floating/tabified/vertical-title docks; restore on close, relocation,
main close or plugin unload. Verified real isolated OBS 32.2.0 Colorway:
Sources header collapse/expand and same column width, Scenes manager collapse
and Expand all. Qt panel regression covers width, height, original content,
gesture, tab guard and close restore. No rule config changes. Remaining:
animated height transitions, header affordance, collapse persistence if desired,
more complicated layouts, horizontal sibling behavior and broader platform tests.
