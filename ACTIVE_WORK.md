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

## Slice 16 — full native menu inspection and selected submenu choices (2026-11-20)

User requested an optional complete listing of available actions from a
captured context menu, including nested submenu choices, and opt-in selected
choice summaries on parent menu entries (such as Lanczos and selected source
colors). No external file-manager naming used for the feature.

Implemented Inspector's List submenu items local toggle. Recursive native
QMenu snapshot preserves original action text, each submenu's object name,
sibling labels, checked state, disabled state and separators. It bounds
traversal to eight levels and 500 entries and guards cycles. The OBS
ColorSelect QWidgetAction's eight preset buttons are individually listed
read-only, including checked preset from its native 2px selection border.
Nested checkable choices can be scoped for hide/overflow edits, but the icon
painter still correctly excludes them. Default compact inspector view stays
unchanged unless the checkbox is selected.

Root-level showSubmenuSelections boolean persists opt-in (default false).
For menus that expose a single checked QAction, parent text temporarily
includes that choice. For OBS ColorSelect's custom widget, the selected
preset appears as Red/Yellow/Green/Cyan/Blue/Magenta/Dark gray/White.
Other color selections use the actual native label if available; no guessed
custom color hex. Qt QAction text is restored after menu dismissal, and
matching uses original text while menus are open. Submenu aboutToShow
refreshes parent status on the next event-loop turn for lazily updated checks.
Ambiguous multi-check groups do not claim a single selection.

RED/GREEN Qt tests: nested tree and child scope, UI toggle and persistence,
native checked/unchecked states, label restoration, ambiguous multi-check,
custom QColor fallback, OBS ColorSelect preset selection, and submenu
aboutToShow refresh. 8/8 full Qt CTests pass. Real isolated OBS Studio
32.2.0 Colorway test: clicked Lanczos, selected source color Red, reopened
native popup and observed Scale Filtering · Lanczos and Set Color · Red.
Real inspector lists child projector items, separators, filter choices,
the source color presets and selected Red, with a verified screenshot.
Saved readable screenshots in docs/screenshots/readme. Live user rules,
source state, themes and running OBS left untouched. Installed v0.13.0
locally, verified identical module and live rules SHA. No push performed.

Remaining: dynamically generated child actions unavailable until their
submenus populate, arbitrary non-OBS QWidgetAction introspection, custom
color hex resolution, broader DPI/platform/RTL and localization testing,
and the unrelated previously tracked work.

## Slice 17 — dockable native Preview workspace (2026-11-20)

User asked whether OBS's video preview could operate as a dock. Inspected
OBS 32.2.0's actual OBSBasic.ui, OBSBasicPreview lifecycle and Studio Mode
handlers. The preview belongs to canvasEditor inside centralwidget, with
the separate contextContainer source toolbar also in centralwidget; Studio
Mode adds programOptions/programWidget into the previewLayout. An early
canvasEditor-only dock worked but left a huge empty center and opened a
tiny video tile, so it was rejected.

Implemented an opt-in native QDockWidget in Panels, moving the ORIGINAL
QMainWindow::takeCentralWidget() (preview, preview-disabled display, scene
source controls and Studio program) into Preview dock, not duplicating a
renderer. No original widgets or QActions deleted. Supports native dock
rearrangement, floating, closing/reopening through Panels, and reversible
setCentralWidget restore. Native QWidget tree identity survives transitions.
The feature is session-only; default is the original fixed preview.
Qt tests cover identity, source context contents, dock close/show, floating,
exact reparent/recover, invalid host, restored on main Close, and deleting the
temporary QDockWidget rather than leaving a serialized reference.

In narrow docks (<760 logical pixels), the native previewLayout is switched
to BottomToTop in Studio Mode, otherwise retains the original Qt direction.
Handles program widgets added/removed after docking, and Studio Mode already
on at the time of conversion. Restores the exact pre-conversion direction
on undock. Floating opens at the last dock size and origin, clamped to the
screen so a native minimum/preferred display size cannot swallow the entire
desktop. The user can resize the floating window afterward. Reflow does not
change user OBS profile settings.

Actual isolated OBS Studio 32.2.0 / Colorway X11 test: native video continued
to render; studio preview/program stacked within the right-side dock; native
Studio Mode disabled and enabled while docked correctly switched between
one preview and Preview/Program; native double-click floated the Preview,
final 727x424 at +705,+522 fully on 1440x1080 display, and Panels restored
the existing Studio central view. Actual File → Exit closed gracefully after
docking. Saved user.ini DockState (isolated profile) did not contain the
Aftermarket preview dock identifier. No user's live OBS process, real rules,
theme or scene collection modified. README native captures recorded.
Any current dock redistribution is not guaranteed to restore pixel-exact.
Not verified: live stream/record output, Wayland, non-Linux, long-term native
rendering behavior or exotic dock/tab changes.

## Slice 18 — responsive Studio Mode shell (2026-11-20)

User showed how OBS's original Studio Mode looked cramped and disorganized
inside the new dock and asked for genuinely dynamic responsive behavior.
Revisited actual OBS 32.2.0 source: canvasEditor contains the native Preview
and Studio Mode adds programOptions and programWidget. OBS QuickTransition
code explicitly reinterpret_casts programOptions->layout() to QVBoxLayout,
inserts buttons at index 3, and reuses QPushButton signals. Therefore no
replacement layout class, clone renderer, duplicate transition buttons or
rewiring was acceptable.

Implemented a small dock-only Studio toolbar with Auto/Both/Program/Preview
buttons, retaining native OBS output/preview surfaces. Auto chooses native
side-by-side at >=900px, stacked below, and Program focus below 430px width
or 380px height. Both keeps both videos, with a 3:2 Program preference in
stacked mode; Program/Preview focus selectively hides only the other video,
and restores it on mode change or undocking. The compact labels use 12px
type and restore their original Qt fonts. Original layout direction,
alignment, stretch factors and native central widget identity are restored
on undock/OBS close. Modes are session-only, not new persistent settings.

In compact mode, preserve OBS's original QVBoxLayout object and every
quick-transition QPushButton, slider, menu and signal. Reorient it horizontally
inside a bounded, native QScrollArea in place of the original layout item.
Chevron buttons browse its horizontal scroll range, including the native
T-bar. This prevents the original quick-transition buttons from forcing
the dock/window beyond available screen width. The scroll area uses QWidget
ownership transfer with takeWidget() on wide reflow, Studio Mode teardown
and undocking; native programOptions is never deleted by Aftermarket. A
QPointer-based deferred cleanup handles OBS deleting its controls during
Studio Mode exit, and listens for options layout changes to update widths
after dynamically adding/removing native quick transitions. Qt scrollbar
signals update arrow states without reentering Qt layout geometry.

TDD: RED failures for missing responsive modes/toolbar, focus distribution,
bounded scrolling, and dynamic quick transitions. GREEN: mock OBS canvas
with 12 native quick transitions, arrow scrolling, button signal identity,
dynamic button insertion/removal, automatic focus, wide-to-narrow resizing,
preexisting Studio Mode and Studio Mode teardown/recreation, exact fixed
preview restoration, previous panels behavior. All eight CTests passed.
Real isolated OBS 32.2.0/Colorway X11: narrow 580px floating dock stacked
Program and Preview with native quick controls in bounded scroll bar; right
arrow physically clicked and verified translated the button strip so Fade to
Black and the T-bar were accessible. Actual Program/Preview/Both/Auto
buttons tested; native Studio Mode switched off and back on without a crash.
Floating resized to 1080px side-by-side with original native vertical
transition controls; resized back narrow; restored fixed central preview
using Panels UI with original video and transitions visible. Native screenshots
saved for README. Isolated scenes contained no live user media and did not
start streaming/recording. Existing live OBS, user rules and Colorway untouched.

Remaining ideas, not implemented: animated transitions, PiP/focus inset,
persistent named studio presets, arbitrary drag arrangement of the two
native video panels, verification of actual broadcast/record output, Wayland,
Windows/macOS, RTL/multimonitor stress, and support beyond verified OBS 32.2.0
Qt 6.4.2.

## Slice 19 — trailing selected submenu values (2026-11-20)

User wanted Show selected submenu choices to stop appending plain text:
right-align the active value, use a distinct secondary/accent color, and
enclose it in parentheses instead of separating it with a dot/dash.
Preserved the existing setting (opt-in). Replaced temporary QAction text
rewriting with a painter-owned, mouse-transparent overlay row for choices.
Original QAction text remains unmodified for native shortcuts, menus,
inspector matching, and accessibility label continuity. Values are right-
aligned before the native submenu arrow (mirrored in RTL), colored from
the active Qt theme's QPalette::Link, and elided inside parentheses when
space is constrained. The menu's minimum width is increased only as
needed for non-overlapping labels and trailing values and restored on hide
unless an external actor modified it. The transient choice property is
also cleared on hide, and asynchronous submenu updates repaint without
changing native text. UI tooltip/status and README wording now match.

TDD RED on the old QAction title mutation, then GREEN. Tests assert
unchanged native labels, trailing theme-aware nontext pixels, native hit
targets, animated/recomputed choices (first/Lanczos/Bilinear), color preset,
state/property cleanup, minimum width restoration, RTL and setting off.
Actual isolated OBS 32.2.0 / Colorway screenshot verified source menu's
parent labels at left; colored, parenthesized choices aligned at right;
original arrow remains at far edge. Clicked the real Scale Filtering submenu
and selected Lanczos; reopened menu showed (Lanczos) aligned with other
value labels. Replaced README's prior captured screenshot with verified
native menu pixels. No modification to user's real OBS state or rules,
no GitHub push. Further validation: other platforms/themes, very narrow
screens and custom QStyle arrangements.

## Slice 20 — theme-aware native transform frame and rounded visual handles (2026-11-20)

User supplied actual OBS preview screenshot of red square transformation
handles and asked for more modern, Colorway/theme-aware frame and points.
Inspected OBS 32.2.0 upstream OBSBasicPreview.cpp and OBSBasic_Preview.cpp.
OBS draws selected/crop/hover transforms with libobs graphics in RenderMain,
not Qt widgets. Native GetSelectionColor/GetCropColor/GetHoverColor read
Accessibility OverrideColors and SelectRed/SelectGreen/SelectBlue from OBS
user config; native grip picking uses HANDLE_RADIUS 4 and selection radius 6.
Do not falsely claim that styling enlarged native hit regions.

Added opt-in root modernTransformFrame false default, atomic Controller
getter/setter and immediate Inspector Gradients tab toggle. Runtime
NativeTransformColors uses OBS frontend's user config; saves prior presence
and values of the four affected keys, sets theme palette Link/Highlight/
bright Link (packed 0xAABBGGRR, alpha=255), and restores on disable or
frontend exit/unload. If an external settings editor modifies any applied
value, it relinquishes the changed key, restores unaffected values and
does not immediately overwrite the external edit. No user ini saves from
the plugin. A theme-dependent state change updates the in-memory colors.

A Qt child drawing overlay was tested and observed BEHIND OBS's native
graphics. A native X11 child did not fix z-order. Using a transparent
mouse-through Qt Tool transient window with a compositor did: actual
Colorway/OBS 32.2.0 preview showed a contrast halo, thin accent border,
round dark-filled 6.4px corner and 5.1px edge handles, with accented hover
states. Original OBS GPU display remains untouched. XTest physically dragged
a real native source corner (selected scene quad moved from 269.6,10.0 to
245.6,-3.5); overlay followed and input still passed to OBS. An explicit
disable/re-enable in a large fitted preview showed square native handles
and rounded styled handles respectively with measured changed pixels.
Documentation uses the actual larger preview capture.

Quad coordinates derived from selected unlocked visible video scene items'
libobs box transform, mapped to current fitted preview with OBS's 10px
preview edge inset. The percentage label is truncated from actual scaling;
decorations only show when its current value matches the fitted transform.
Custom fixed zoom (including tested 12%) hides the floating art rather than
risk incorrect positions, while native transform handles still work. Groups
are skipped as their child-space transforms need separate handling.
Tool window hides when OBS is minimized/inactive or a popup takes focus;
no extra input capture or render output modification.

TDD tests: transform-visual pixel shape, empty state, transparent mouse
input, tool-window flags, fitted-zoom guard; transform-color preserves
existing and missing config keys, follows theme updates, avoids clobbering
external edits; Inspector controller toggle/default. Real isolated OBS
Colorway X11, compositor and Xvfb GUI checked selection, hover/resize,
custom zoom hide, live opt-out, restored native visuals. Full suite includes
these two new tests. Real user profile, scenes and live OBS process not
changed. No push unless explicitly requested. Current limits: native hit
radius unchanged, groups skipped, graphics-layer compositor required for
round art, no Wayland/Windows/macOS validation, custom zoom not covered.

## Slice 21 — optional native property group folding (2026-11-21)

Scope: safe first part of the dock/settings/property brainstorming. Built an
opt-in `PropertySections` observer that finds *ordinary* `QGroupBox` sections
inside genuine `OBSPropertiesView → PropertiesContainer` widgets. Adds a small
arrow at each section header and uses Qt 6.4 `QFormLayout::setRowVisible`
on existing rows. No native widget is reparented, cloned, or disconnected;
filter values, original row visibility, and native enable/check state are
preserved. Disabling the feature restores original row visibility and removes
arrows; destroying a dialog tears down the observer's entries.

Inspector → Panels has an immediate checkbox. Its `collapsiblePropertyGroups`
bool persists atomically in the existing rules JSON; default false. Native
checkable groups (whose checkbox controls property enablement), unrelated
QGroupBox widgets, and ungrouped property rows are deliberately not modified.
Group fold positions are not persisted across OBS rebuilds.

TDD: `property-sections-test` covers source ownership matching, fold/restore,
preserving native values, initially hidden rows, dynamic new group attachment,
non-OBS exclusion, checkable group exclusion, and teardown. Inspector test covers
rules persistence and immediate toggle behavior. Full CTest: 11/11 green.
Isolated OBS 32.2.0 Xvfb with a copied scene/profile and freshly built module
loaded successfully and opened an existing color-correction filter without a
crash; that filter has no native group, so the *real OBS group-arrow interaction*
is not yet verified. No live user OBS process or profile modified. Native
property-group testing across plugins and themes remains needed.

## Slice 22 — reversible Audio Mixer backside + native routing (2026-11-21)

Outcome: the first dock-backside implementation is usable without replacing
OBS's native Audio Mixer. `DockFaces` temporarily hosts the **same native
QDockWidget content** on its front and a plugin-owned `AudioPatchbay` on the
back; a compact in-content flip button remains present in tabbed docks.
Optional native-tab prev/next buttons raise OBS-owned sibling docks.
Enabling/disabling is immediate via Inspector → Panels and persists as root
`mixerBackside` (default false). No arbitrary user-defined editor, dock
size presets, extra DSP, virtual audio routing, or OBS Settings-page injection.

The patchbay draws source-to-track cables, and has both clickable endpoints
and accessible track buttons. It also provides the native three-state
monitoring selector. The bridge enumerates live libobs audio sources with
stable UUIDs; edits reacquire an owned source reference by UUID and release it
after reading/updating the existing mix mask or monitor mode. Source rename or
deletion cannot retarget an edit by display label. It refreshes from OBS while
visible; reads/flip actions alone don't modify scene audio.

TDD: new `dock-faces-test` covers original widget identity/faders, flip,
tab sibling selection, detach, repeated attach, close and destruction. New
`audio-patchbay-test` verifies backend-authoritative updates, unaffected
tracks, source rename and removal, monitoring and empty state. A queued dock
location event originally segfaulted after detachment; regression reproduced
it, and guarded QPointers plus a receiver-owned timer fixed it. Inspector
tests verify opt-in persistence and toggle. Full CTest 13/13 passed.

Real OBS 32.2.0 in isolated Xvfb + copied scene/config with Colorway loaded
the plugin, displayed the normal mixer with added header, flipped to the
patchbay, and showed two actual audio sources. A click to enable Silent test
track 4 reflected in the **original Advanced Audio Properties** window,
and enabling track 6 in that native window reflected in the patchbay.
Changing monitoring to Monitor Only on the patchbay succeeded.
On normal File → Exit, copied scene collection persisted Silent test mixers
mask 41 (tracks 1+4+6) and monitoring_type 1, while Tone test stayed mask 1,
monitoring_type 0. Original faders rendered and remained interactive after
flipping back. Restarting isolated OBS reloaded mask 41 / monitoring 1 and
showed the updated face header; normal exit succeeded again. All changes
to copied disposable OBS state; the user's running instance and actual
profile were untouched. Image:
`docs/screenshots/readme/mixer-patchbay.webp`.

Remaining: varied OBS versions/platforms, live runtime source churn,
more complex tab groups, native UI update during dock relayout, monitoring
on multiple audio devices, user-defined back providers, size presets, and
long-running performance/teardown stress. No remote push.

## Slice 23 — Theme/Style picker text clipping regression (2026-11-21)

User supplied a photo of OBS Settings → Appearance → Style, with 166 Colorway
variants spread across four spaced columns. Labels were rendered as repeated
`Catpp...`/`EarthB...` fragments despite abundant blank width per column.
Inspected current Qt picker and actual Colorway metadata: the source strings
are intact; `QListWidget::setGridSize` spaces items but does not size their
delegate paint rectangles. A Qt offscreen probe showed 180px grid step with
only 83px visual item width. This was presentation clipping, not bad data.

TDD: extended the actual popup test to 166 items with 52 long, similarly
prefixed Catppuccin names. Required `visualItemRect.width` to match each
allocated grid cell; observed a failing assertion on the old implementation.
Fixed with explicit `QListWidgetItem::setSizeHint(gridSize)` for all items
after determining grid width. Focused menu test then passed, including text
filtering, scroll/selection of item 83, original combo activation, and native
model preservation. The guard preserves screen-based column bounds and
tooltips for genuinely overlong labels.

Isolated OBS 32.2.0/Colorway with copied user theme files launched and showed
native Appearance controls without a crash; however the isolated OBS profile
exposes only one selectable Style despite 332 copied .ovt files (pre-existing
test-profile limitation). Thus the full 166-entry *real OBS* popup was not
visually validated in that instance. The changed code is also subject to the
existing theme-picker opt-in gating via Compact menus. User's OBS instance and
settings were not changed. No push.

## Slice 24 — opt-in alternating backgrounds for native Scenes and Sources (2026-11-21)

Added independently toggleable native SceneTree and SourceTree row striping.
Inspector → Panels → Alternating list backgrounds offers Scenes/Sources switches
and two separately configurable opaque colors, Normal and Alternating; both
color slots may instead inherit the OBS theme (the default). Settings persist
atomically as a validated optional `listRows` root object in rules.json,
with controls updating live and failures rolled back.

The presentation adapter attaches only to the actual OBS widgets by Qt
class identity + objectName. It preserves native models, index widgets,
source-specific color presets, selection, hover and drag/drop. Sources use
`QListView::setAlternatingRowColors` with the two native palette roles.
Colorway's Scenes list has a global QSS `QListWidget::item:alternate` rule
that overrides Qt palette colors; for custom colors the adapter adds an
in-view `SceneTree::item` color rule ONLY for unselected, non-hovered list
rows, preserving the original native stylesheet. Scene tile/grid mode
temporarily suspends row striping and then restores it on return to ListMode.
On toggle-off, teardown or plugin unload, original flags, palette and
stylesheets are restored; Qt theme/style changes are reconciled rather than
retaining our colors as the next "native" palette.

TDD: 14/14 full CTests passed. New `row-stripes-test` exercises both target widgets, excludes
generic/third-party lists, preserves original source index widgets,
selection highlights and source styles, applies custom normal/alternate
colors, restores edited themes, and guards a reproduced grid-mode QSS
restoration failure. A second red test reproduces Colorway-like
QListWidget::item:alternate stylesheet precedence with rendered-pixel
comparison; a scoped SceneTree override makes it pass. Inspector tests
cover atomic storage, invalid settings, two immediate toggles and
theme-reset. Screenshot: docs/screenshots/readme/alternating-scenes-sources.webp.

Real isolated OBS 32.2.0 with the actual Colorway theme, copied profile and
three scenes: Sources row 2 changed from configured #606088 to native
#191724 when disabled (pixel at x199,y556), while Scenes remained enabled;
Scene row 2 rendered #606088 after the theme-specific scoped override,
and selected Scene row 3 remained OBS's native #31748f. Disabling Scenes
returned row 2 to native #191724 while Sources remained #606088; re-enabling
returned it to #606088. Scoped UI toggles persisted only to the disposable
OBS profile. User's live OBS, themes and source settings untouched.

Limits: other widgets and third-party list implementations are excluded,
and no claim is made about all Qt/OBS platforms or custom themes. No push.

## Slice 25 — native OBS menu bar as one hamburger (2026-11-21)

Accepted direction: replace OBS's top-level File, Edit, View, Docks, Profile,
Scene Collection, Tools and Help labels with one `☰` control, not a second
toolbar squeezed alongside them. The freed area may later host shared action
strips/status icons, but none are invented in this slice.

Implemented `MenuBarCompact`, a small, opt-in runtime Qt adapter for
non-native `QMenuBar`. It moves **existing QAction identities** from the
bar to a submenu of a new hamburger `QMenu`; original QMenu objects,
action callbacks, shortcuts, disabled/checkable states and plugin-owned
Tools entries stay intact. An action-event observer captures top-level menus
added later by other plugins. The class restores the original action order
on disable/unload and destroys its plugin-owned popup and shortcut on
teardown. Width reserves room for long names and submenu arrows.
The native corner widget (if any) remains untouched. MacOS-native menu bars
are excluded rather than hacked.

Inspector → Menus & widgets adds a separate `☰ Menu bar` toggle. It writes
`hamburgerMenuBar` (default false) atomically to rules.json. Startup
reapplies the setting after native Tools actions exist; disabling restores
the classic bar immediately. Alt+M is registered as a Qt window shortcut.
`Compact menus` remains a distinct preference for popup density.

TDD: full CTest suite 15/15 passed. New `menu-bar-compact-test` first failed without its adapter; now
checks exact pointer identity of original menus/commands, top-level
consolidation, preservation of existing menu-bar corner widget, dynamic
plugin menu additions, native action callbacks and accelerators, repeated
toggles, native restore order, deleted plugin menus, synthetic Qt Alt+M
press/release opening the menu, shortcut removal on disable, full
plugin-owned popup removal on teardown, and empty/unrelated menus.
Inspector test covers immediate toggle, original menu restoration and
persistent config bool.

Isolated real OBS 32.2.0/Colorway under Xvfb with copied disposable profile
and rebuilt module loaded with opt-in persisted: menu bar showed a single
hamburger. Clicking displayed all eight original OBS submenus. Hovering
Tools opened the original menu with OBS and third-party plugin actions,
including UI Augment Inspector and Panels. Selecting Inspector from that
submenu launched the original settings window. Toggling `☰ Menu bar` off
restored the original File/Edit/View/... labels immediately; the copied
rules.json persisted false. Restart after rebuild with true showed the
hamburger again, and the updated menu width kept Scene Collection clear
of its submenu arrow. Screenshot:
`docs/screenshots/readme/hamburger-menu-bar.webp`.

Limitations: synthetic Xvfb Alt+M key injection did not visibly open the
popup even though Qt key-event regression passed; actual keyboard shortcut
needs validation with a window manager. When new top-level plugin menu
entries are created while compact mode is active, they are appended in
observed order; incompatible plugin code that requires inserting before
an action removed from the menubar may need an adapter extension.
No additional user-customizable status controls or menu-bar styling
framework added. User's running OBS process, configuration and installed
plugin remain unchanged. No push.

## Slice 26 — persistent emergency UI recovery hotkey (2026-11-21)

User request: an emergency-mode hotkey to recover the UI when an Aftermarket
customization traps native OBS controls. Implemented Ctrl+Alt+Shift+F12
(in-app/Qt application shortcut, with application-wide KeyPress filter that
can intercept even an open QMenu). Also added a native Tools menu action
for emergency restore and, when safe, a separate *Resume UI customizations*
action. The emergency keystroke only engages safe mode; it cannot toggle
customizations back on.

Safe mode persists via an independent `safe-mode.flag` marker adjacent to
`rules.json`, committed via QSaveFile *before* live restoration. It never
changes the user's rules, scene collection, audio routing, theme or recordings.
Marker presence makes the plugin fail closed during startup even if the
rules file is broken or unreadable. Resume is explicit through the native
Tools menu and clears the marker only after the normal UI adapters are
successfully reinstated.

Controller gained `setRuntimeSafeMode` and an optional initially
suspended constructor path: menu overlays/theme picker and gradient/audio/
rotary effects stop and restore original Qt controls, and the file watcher
cannot re-enable them. Initial safe startup does not attach menu overlays.
Plugin UI teardown restores collapsed panels and native preview, unwraps
Audio Mixer dock backsides, returns original top-level OBS QActions,
restores Scene/Source palettes/stylesheets, removes property collapse
controls and transform overlay; it keeps OBS itself running.
The runtime activation callback is queued so that removing an active
hamburger QMenu or Inspector cannot destroy a widget mid-signal. Restoring
the UI recreates the adapters with saved preferences. The emergency
key and tool actions remain available independently of the main inspector.
Backend availability is unchanged: no service/recording actions are invoked.

TDD: full CTest 16/16 passed. New `emergency-recovery-test` verifies the full chord (including
keystroke while a popup is active), partial chord/repeat suppression,
independent marker persistence, repeat safety, failed resume preserving the
marker, successful explicit resume and unchanged rules file. `menu-test`
includes a regression for live suspend/resume of original native QAction
artwork/layout and a RED→GREEN startup test showing that an initially
suspended Controller attaches no menu decoration until resumed.

Real isolated OBS 32.2.0 with copied Colorway profile and hamburger,
mixer backside, collapsible sections and alternating list colors enabled:
XTest Ctrl+Alt+Shift+F12 sent with hamburger popup open wrote marker
and restored original File/Edit/View/Tools top-level bar, native Audio
Mixer and original list colors while OBS remained running. Compared SHA256
of copied rules.json before/after: identical
(`9874f7ade496183fb5d79a07f0302c22e4df161d416a0df88124a79c3a2023c7`).
Terminated isolated OBS, restarted with marker present: native UI loaded,
plugin log explicitly reported recovery mode, and native Tools menu had
Inspector/Panels disabled and *Resume UI customizations* enabled.
Clicking Resume cleared marker and immediately restored the hamburger,
mixer backside and list colors, still without changing rules.json.
A second XTest emergency chord while the Inspector window was open closed
that window, restored the native UI, wrote the marker and left OBS running.
The latest rebuilt module also restarted into safe mode with zero menu
augmentations and the same unchanged preference SHA256.

Limitations: the shortcut is scoped to OBS Qt focus (not global across
other applications); OS shortcut managers may intercept it. The built-in
Tools entry or marker-file fallback is supported. Emergency recovery does
not rewrite or reset native OBS workspace arrangements or external plugin
state. Stress across other platforms, complex preview tab configurations,
and runtime failure injection is unverified. The user's running OBS is
untouched and there is no push.

## Deployment checkpoint — native Audio Mixer backside installed (2026-11-21)

User requested Remote Desktop Commander after asking if the audio bus was done.
Confirmed the agreed native OBS patchbay existed in commit `0bdeeba`, but
installed OBS plugin remained a stale older binary from Nov 20. On yuzu-mint,
OBS 32.2.0 was closed, the current plugin rebuilt cleanly (`ninja: no work to
do`) and all 16 CTests passed. Preserved the installed binary at
`~/.local/share/aftermarket/rollback/obs-ui-augment.pre-audio-backside.4abebcf35892.so`
(sha256 beginning `4abebcf35892`). Used the repository's atomic
`scripts/install.sh` to install the current tested `build/obs-ui-augment.so`.
Verified destination bytes match the build (sha256
`b21716ac7a8f2cea275fe3448b96f8ef33616bc882d7873c4bfc31848879496b`).
Verified actual `rules.json` SHA256 was unchanged
(`ff8f1f55f824d437df1c9a17cfa178bad088d9e4f17dd1c1b61a11ec1ad54623`).
No OBS scene collection, preferences or active process was changed; plugin
will load when OBS is next opened. `mixerBackside` remains off by default:
enable through Tools → UI Augment Inspector → Panels, then flip Audio Mixer.
No push. This is native tracks 1–6 + monitoring, NOT an independent arbitrary
DSP/audio bus engine.

## Slice 27 — compact, draggable native Audio Mixer patchbay (2026-11-21)

Mat's real dock feedback: patchbay graph was much too large; it needed
drag-and-drop and an option to see all source connections. Inspected actual
`AudioPatchbay::Canvas`: fixed 54px vertical row spacing, 365px minimum
graph height, 310px minimum width, and click-only input.

TDD first: added failing test requiring graph minimum height <=170px instead
of 365px, source->track pointer drag independent of the currently selected
source, idempotent repeat drops, rejection of drops outside valid nodes,
and a visible-cable count / pixel-rendering test for Show all wires. Added
`ObsRouting::setTrack(uuid,track,connected)` using existing owned OBS source
refs and current native mixer mask, calling the public setter only if the
bit actually changes. The original track buttons still toggle and monitoring
remains untouched. Dragging to an already connected track is not a disconnect.
Backend is authoritative on every drop, so stale source UUIDs cannot redirect
edits. Qt tests passed with an injected set-track callback and precise
mouse press/move/release events.

Reworked the Canvas to a 240px minimum width, 18-32px adaptive vertical
step, compact node rectangles, <=166px minimum height for two sources, drag
preview and track hover feedback; source clicks still focus. Native Qt
QPalette colors drive all artwork. The optional Show all wires checkbox is
off by default and purely visual. Other source routes are drawn behind the
selected route with a legible theme-derived tint; selected routes render
last. Existing selection-specific track buttons and monitoring controls stay
accessible. Show all is session-only (not a new persistent rules key).

Real OBS 32.2.0/Colorway under an isolated Xvfb :112 copied profile:
after changing the patchbay in the rebuilt module, all six track targets
were visible in the short Audio Mixer dock; dragged Silent test source to
track 2 using actual XTest pointer press, move, release. The new cable
appeared and track 2 checkbox became checked. A normal OBS File → Exit
persisted Silent test `mixers=43` (tracks 1,2,4,6) from initial 41
(1,4,6); Tone test stayed `mixers=1`. Checked Show all wires and verified
that Tone test's route became visible alongside selected Silent test.
Updated screenshot `docs/screenshots/readme/mixer-patchbay.webp` from
the actual isolated OBS application. No external audio bus/engine added.
User OBS process remained running independently; no scene/recording changes
were made to the user's profile.

### Slice 27 deployment checkpoint (2026-11-21)

Full CTest suite: **16/16 passed**. Commit `386dca4`.
After real XTest drag in disposable OBS, normal OBS File → Exit saved
Silent test audio mixers mask **43**, confirming source-to-track 2 changed
the libobs state rather than only painting a connection. Visual inspection
showed all six destinations within the short mixer dock; the Show all
toggle revealed the second source's track 1 wire. Replaced screenshot in README.

The user's OBS `obs --verbose` remained running (PID 2851624); no session
was restarted, no scene collection or recording modified. Preserved prior
installed plugin at
`~/.local/share/aftermarket/rollback/obs-ui-augment.pre-compact-graph.b21716ac7a8f.so`.
Used atomic `./scripts/install.sh` to install verified module
SHA256 `4a7039100dc45f26919516915c31ac2cfcfbf22138453260ec98763bbea26408`
into the normal per-user OBS plugin path; installed module `cmp` matched
the tested build. Existing `rules.json` SHA remained
`b0286e2ec9e01ae6fc7d1ba29b688382ed0e41313c1bda70c3ad5edcbb0c0fbf`.
New graph will appear **after the user next restarts OBS**; running process
continues using its old, already-loaded module. No push.

## Slice 28 — coordinated theme-aware typography and selected-state polish (2026-11-21)

User requested a typography review with thinner set/selected states, then
corrected an overly narrow Audio Patchbay-only focus. This checkpoint
deliberately covers **several Aftermarket surfaces** rather than only the
most recently implemented widget.

Reviewed in the real Colorway theme and patched:
- Audio patchbay: selected source tint now subtle rather than opaque accent
  fill; 0.95px selected source outlines, 1.25px focused wires, 0.8px
  unfocused wires, 1.25px drag guide, finer target ports. Native theme
  button/text/highlight colors are blended rather than using fixed colors.
  Captions gain legibility, labels use differentiated native-font weights.
- Shared header-only `visual-polish.h`: a strictly Aftermarket-owned
  `FineStateToolButton` with a 1px highlight hairline in the selected
  state, restrained native-palette tint, normal weight text, and
  QStyle-provided focus rectangle. Used by patchbay track controls *and*
  native Studio Mode's Auto/Both/Program/Preview toolbar selections.
  Leaves source, panel and OBS-owned widgets/behavior untouched.
- Inspector: Mixer and Gradient headings are semibold, theme-relative
  size (+0.75pt where available) rather than hardcoded bold +2pt;
  target heading semibold. A dialog-scoped font-weight stylesheet prevents
  inherited all-bold themes from flattening the hierarchy; only font
  weight is specified, never font family, palette, background or border.
  This does not restyle application-level native menus.
- Studio Mode: title semibold, derived from theme size instead of 12px.
  Compact native Preview and Program title sizes derive from each original
  native QFont; use Medium weight rather than fixed DemiBold 12px.
- Dock face labels: Medium font weight from original theme font.
- Compact menu density: minimum row size tracks native QFontMetrics
  instead of fixed 16px, preserving the active font and native action
  handlers.

TDD red/green tests added for Inspector typography and scoped styling,
studio selection style and font-relative labels, dock label weight, font-
metric-sensitive compact menu height with restoration, and patchbay selected
state/tint/fonts. All 16 CTests passed (no new build warnings). A fresh
isolated OBS 32.2.0 Colorway runtime under Xvfb :113 loaded the module and
visually inspected the full mixer patchbay and Mixer Inspector page.
Screenshots updated/created:
`docs/screenshots/readme/mixer-patchbay.webp`,
`docs/screenshots/readme/inspector-typography.webp`.

Boundary: explicit per-widget theme QSS may still determine exact
font-face appearance; this pass intentionally does not override Colorway
typefaces or all OBS-owned checked/list states. Verification is OBS 32.2.0
on the isolated Colorway profile, not every theme, device or DPI.
Native audio routing, scene collections and preferences are unchanged.
The user's existing OBS process stayed running.

### Slice 28 verified local install (2026-11-21)

Checkpoint commit `40466b7`; full 16/16 CTests passed, isolated OBS
Colorway Inspector and Audio Mixer visually inspected. The prior local
module was saved as
`~/.local/share/aftermarket/rollback/obs-ui-augment.pre-typography-polish.4a7039100dc4.so`
before running the existing atomic `scripts/install.sh`.
Installed module matches the tested build byte-for-byte, sha256
`11ef6232c9e9a78e15938947948e8ae0cd26fa2a86dbba7f7ff159e5bf53adf9`.
The user `rules.json` SHA256 was unchanged
(`b0286e2ec9e01ae6fc7d1ba29b688382ed0e41313c1bda70c3ad5edcbb0c0fbf`).
The actual user OBS process stayed running as PID 2851624, with its existing
in-memory plugin; it needs a voluntary OBS restart to load this update.
No repo push or scene collection changes.

## Slice 29 — OBS routing diagnostics and hover signal trace (2026-11-22)

Outcome: add read-only source-health visibility and temporary signal-path
inspection to the existing compact Audio Mixer backside. The native route
model now includes OBS source muted, enabled and audio-active flags, read
using `obs_source_muted`, `obs_source_enabled`, and
`obs_source_audio_active` inside the existing libobs enumeration. These
flags are not signal-level meters. Source cards show small M/Off markers,
muted/disabled existing cables render in the native palette as subtler
dashes without removing their tracks, and a selected-source diagnostics
line reports tracks 1–6, monitoring, mute/disabled and OBS audio-active
state. Hover a source to focus its routes, or hover a destination track
to reveal its actual incoming wires even with Show all wires off.
Hover clears on leave; focus, selection, stored Show all setting, and
live scene audio remain unchanged until explicit existing track/monitor
actions occur.

Verification: RED source contract failed to compile without new flags,
then RED UI regression failed on missing diagnostics label. GREEN
component regression covers source and track hover including real rendered
pixel differences, tooltip state, clearing on leave, real-time state
refresh, and zero backend writes on inspection. New native libobs test
creates a real dummy OBS audio source in its own process; verifies
muted/disabled/audio-active state, monitor-only, persisted track mask,
idempotent link creation, rename stability by UUID, and stale-target
rejection. Full Qt/native CTest: **17/17 passed**. Isolated OBS 32.2.0
launch in a temporary XDG profile loaded and initialized the rebuilt
module successfully while the user's real OBS process continued
uninterrupted. Did not exercise a real populated Audio Mixer backside
during that clean-profile smoke test; broad theme/Qt version and live
signal-level behavior not claimed. No atkAudio plugin DSP or private
user scenes/configuration were changed.

Scope boundary: this is OBS's ordinary six output tracks and native source
flags, not atkAudio PluginHost2's internal VST/MIDI graph or a second
audio engine. Review and licensing note: atkAudio v0.36.3 source is
AGPL-3.0; conceptual behavior was assessed without copying code.
