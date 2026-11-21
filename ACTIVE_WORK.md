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

## Slice 8 — integrated rotary peak meter (2026-11-19)

Outcome: eliminate the separate tall native meter for knob-enabled sources by
drawing a separate outer peak ring around the existing rotary gain control.
Reads max channel peak (dB) and sample/true peak mode via public libobs API,
uses native nominal/warning/error color properties, -60..0 dB scale,
-20/-9 thresholds, 1s peak/clip hold and 30 dB/s fall. Independent of decorative
motion. Default on for knob-enabled channels, opt out via gradient.knobMeters.
Only paired VolumeMeter in volMeterFrame is hidden and restored; if binding
fails use native meter. Tests verify fallback, real paint differences, warning
color, slider value/size preservation and motion independence. Real isolated
OBS 32.2.0 Colorway verified selected knob + original adjacent channel,
mute changed 567 pixels in knob region with unchanged -1.9 dB gain, native
meter restored live, both integrated knobs, drag changed native gain to 0 dB.
Combined peak is not full independent stereo/magnitude/input peak, fixed
thresholds; horizontal/rebuild/rename/other versions and performance unverified.
Earlier named tokens and leak attribution still open.

## Slice 9 — Inspector workspace (2026-11-19)

Outcome: consolidated existing functionality into four native tabs: Menus &
widgets, Mixer, Gradients, Panels. Adjustable capture delay, action search,
existing rule icon previews, optional technical details, source checklist
discovered from actual mixer widgets, exact offline names, capture routes to
the selected source, palette preview and native color picker, explicit
shared Apply/Reload with unsaved indicator and conflict rejection. Panels
entry routes into the same inspector. Existing rendering engines and rule
formats retained; unknown gradient fields preserved. Real isolated OBS
32.2.0 Colorway: source selection changed both channels to integrated knobs,
custom palette persisted and recolored the controls, panel collapse/restore,
source and menu captures. All 8 CTests pass including stale write and
empty selection regressions. No named-token resolver or arbitrary widget
editor added; earlier renderer limitations remain. Source list discovery
only covers current mixer widgets, with manual offline-name fallback.

## Slice 10 — rotary meter visibility repair (2026-11-19)

User reported missing meter rings. Inspected current running OBS 32.2.0,
Solarized Dark+, with knobs, knobMeters, audioReactive and Motion enabled.
Correct plugin installed; probes attached to Media Source and Live Latent:
TTS. Actual mixer screenshot showed extremely dark idle outer tracks;
Media Source muted, TTS signal not established at capture. Root visual
defect reproduced with dark palette: idle scale lacked recognizable
warning-zone pixels. Fixed by drawing distinct dimmed nominal/warning/error
segments and thicker outer ring; active fill retains native colors.
Hidden native meters no longer re-register as decorative pulse targets.
Additionally found horizontal mixer repolish could shrink knob; regression
reproduced native minimum reset, repair reasserts integrated size while
preserving newer native constraints for restoration.
Real isolated OBS tone/silence channels verified visible idle zones, active
peak and muted idle, full-size horizontal knobs and live switch to vertical.
Isolated theme remained Rosé Pine despite setting Solarized Dark ID; exact
dark palette covered by Qt regression, not verified on live user session
with audio. Do not claim user TTS signal path repaired until exercised.
Current user config untouched. Open: full stereo meter, named OBS theme
tokens, rename/rebuild stress, generic widget targeting and profiling.

## Slice 11 — slider thumb/gradient seam (2026-11-19)

User screenshot of Synthwave84 filter property sliders showed cyan native
sub-page wedges between the animated rounded cap and white thumb. Reproduced
24 native cyan seam pixels with Colorway-like Qt stylesheet. The old renderer
ended before the thumb bounding box, then rounded both ends; Qt stylesheet
SC_SliderHandle alone also redraws the native page (confirmed Qt source).
Fixed by clipping a full-groove gradient at the thumb center, comparing
native style paint with the handle moved away in a small cached patch,
preserving the genuine handle silhouette and recompositing identifiable
solid-color antialias edge pixels. Complex ambiguous pixels remain native.
Regression: zero cyan seams at values 5, 50, 95, 100, inverted/RTL and
vertical, white thumb center retained. Full 8 CTests passed. Actual isolated
OBS 32.2.0 color correction filter property sliders with contrasting orange
gradient showed clean joins, native saturation drag 0.25 to 3.40; vertical
mixer faders also visible. The isolated instance still used its existing
Colorway palette despite requested Synthwave84 ID, so exact user variant
and plugin filter dialog not directly exercised. No changes to live settings,
theme or user OBS session. Higher-DPI and complex image/shaded thumb styles
need broader verification. Earlier gaps remain tracked above.

## Project identity — Aftermarket (2026-11-19)

User selected Aftermarket, inspired by automotive customization culture, and
requested a new GitHub repository with existing work and history. Project-facing
name updated; retain existing module/config identity and local checkout to avoid
breaking installations. Remote target: myrqyry/aftermarket. Private initially
pending public visibility/licensing decisions. No release or binary packaging
implied by source publication.

## Slice 12 — compact native menus (2026-11-19)

User disliked context menus spanning the screen. Qt 6.4.2 source confirms
clamping QWidget height alone does not establish native scroll flags. Added
reversible compact stylesheet and native overflow pages with original
QActions; logical rule labels/IDs preserved across pages. Keep the last four
commands on the top-level menu when feasible; no arbitrary semantic reordering.
Default enabled, inspector toolbar checkbox and compactMenus JSON opt-out.
Skip menus with embedded QWidgetActions or tear-off behavior.
RED regression: long menu exceeded the budget. GREEN: original actions
reachable, keyboard native exec return and root triggered signal exactly
once, scoped icon resolution, restoration, final commands kept, embedded
widget exclusion, toggle and inspector persistence.
Isolated OBS 32.2.0/Colorway: source context popup 277x720 before, 254x436
final, overflow 240x203. Earlier intermediate overflow Rename clicked and
native inline editor opened; final screenshot shows direct Rename, Remove,
Filters, Properties and middle overflow with existing native submenu arrows.
Multi-monitor, other DPI/platforms, very large fonts and third-party
dynamic menus require wider runtime validation. No theme or live config
changes. Remaining unrelated gaps stay open.

## Slice 13 — browse-first menus and explicit overflow (2026-11-19)

User corrected the earlier automatic overflow behavior: they want to view
all choices without an extra click, especially filter and theme lists.
Replaced automatic overflow with native columns on the local Qt 6.4.2 ABI,
retaining compact row spacing and original QAction objects. Explicit scoped
menuOverflow rules alone can move an action; inspector checkbox independent
of icon edits. Native source menu measured 554x312, effect filter picker
500x204 with all 18 choices visible; selecting an item in the second column
opened OBS's original Filter name dialog. Native context placement corrected
near the pointer. The Appearance theme/themeVariant QComboBoxes use a
separate searchable grid for long lists. Qt test with 120 entries verifies
wrapping, search reset, actual mouse selection and native activation.
Isolated OBS exposed just one Colorway style despite copied variant files,
so real populated theme selection remains unverified. Qt private columns
are version-gated; other versions use compact native rows. Large lists
may need scrolling, never automatic hiding. All 8 regression tests passed
on the local Qt version. User rules and themes unchanged; no automatic
overflow migration needed because old overflow was generated, not saved.

## Slice 14 — preserve Source context menu's single column (2026-11-20)

User disliked two columns on the ordinary Source context menu, specifically
not the multi-column choice pickers. Added source-command-group recognition
(Order, Transform, Filters, Properties) to exempt that task-oriented menu
from the native column reflow while retaining its compact styling and
explicit placement rules. RED regression: source menu reflowed into multiple
columns. GREEN: all original action rectangles share a single x position;
existing long choice-list test still verifies multiple columns.
Actual isolated OBS 32.2.0/Colorway: Source context 275x611 in one column;
original Filters action opened actual filter dialog, whose Add Filter list
remained 500x204 with two columns. Theme-picker adapter unchanged.
No change to user rules, theme files or the live OBS process. The signature
uses OBS's English Source action labels; localized or unconventional third-
party Source menus may require an improved contextual locator.

## Slice 15 — native menu content editing (2026-11-20)

User requested Linux Mint Nemo Actions-style customization: remove selected
defaults and add user-defined entries. Inspected installed actual Nemo
.nemo_action format for Name, Exec, icon and conditional selection inspiration.
Added menuHidden (independent scoped, reversible removal of QAction from
QMenu, without mutating or deleting the action) and menuCustom (stable ID,
label, executable, argv array, optional known icon, explicit menu scope).
Actual execution uses QProcess::startDetached, never an implicit shell.
Icon and motion rules remain independent. Native action ordering and state
restore when menu closes. Hidden beats conflicting overflow placement.
MenuLayout preserves Source context's single-column identity before hiding
sentinel actions. Custom entries are popup-lifetime objects, deleted on close.
A new Inspector checkbox hides the selected default; Add custom... opens a
label/program/arguments/icon editor; Manage entries... restores hidden
defaults, edits/deletes custom entries, and has a Show all view reachable
without menu capture. Atomic file editing, schema validation and unknown
settings preservation retained. Only append custom actions; no reordering,
grouping, native binding, selection placeholders or Nemo file import yet.

TDD: RED missing APIs; GREEN QAction restoration, untouched signal
connection and checkable state, exact program argv to temp test file,
menu scoping, conflicting hidden/overflow, user config validation, model
and Inspector UI actions, global recovery without capture. Actual isolated
OBS 32.2.0/Colorway Sources menu with Remove hidden, Test custom entry
at bottom, physically clicked entry and verified generated test file.
Native menu and global management dialog captured from isolated OBS.
Do not assert the user's live source-specific command behavior was tested
or that arbitrary external programs succeed in all environments.
Existing live OBS and user's rules unchanged; local commit/install only,
no automatic GitHub push. Further hardening: dynamic menu rebuild/reuse,
internationalized anonymous selectors, program launch failure feedback,
custom sorting/submenus, and selection-aware arguments.
