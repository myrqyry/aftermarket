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

### Slice 29 verified native installation (2026-11-22)

- Committed as `c94b19e` on current `main`; did not push. Working tree clean.
- Updated the installed native OBS 32 module via the existing atomic
  `scripts/install.sh` without terminating or restarting the user's live
  `obs --verbose` session (PID 2908069 at install).
- Preserved the previous binary at
  `~/.local/share/aftermarket/rollback/obs-ui-augment.pre-routing-health.11ef6232c9e9.so`.
- Installed file and tested build SHA-256 match exactly:
  `65666a050f4abdcc480d5966722f1c9da092786c3a483c24dceb7d8c192eaf50`.
- Existing `rules.json` SHA-256 remains
  `b0286e2ec9e01ae6fc7d1ba29b688382ed0e41313c1bda70c3ad5edcbb0c0fbf`.
- Current running OBS continues using its already-loaded module. The new
  behavior becomes active on a voluntary next restart; no scene or audio
  changes were made during this installation.

## Slice 30 — native Action Strip, status, owner-aware hotkeys, bounded dock retry (2026-11-22)

User accepted ideas from StreamUP's actual code review and reminded us of
Timeline's existing Macro Strip. Inspected CURRENT ~/loam/timeline
macro-strip-dock.cpp, macro-button-model.cpp and hotkey-catalog.cpp and
the relevant StreamUP source. Reimplemented the design patterns in Aftermarket
without importing code, persisting into Timeline files, or creating a
cross-plugin runtime dependency.

Features:
- Opt-in ActionStrip at native QMenuBar top-right corner, working with
  hamburger or classic menus. Versioned, atomic, validated
  plugin_config/obs-ui-augment/action-strip.json; defaults to empty/off;
  refuses bad entries rather than dropping old valid items. Keeps the
  original OBS-owned corner widget identity, and restores that widget
  on teardown.
- Live status items: recording/streaming flags, elapsed recording/streaming
  clocks, CPU, FPS, stream output dropped frames, distinct render-missed
  frames, stream and recording bitrates. One 1Hz shared refresh; 5-second
  cached native hotkey discovery. Elapsed time uses monotonic clocks and
  native frontend start/pause/unpause/stop events; unknown time for
  pre-existing outputs remains --:-- rather than invented duration.
- Searchable owner-aware native hotkey picker (frontend, source by UUID,
  output, encoder, service). Persisted hotkey:v1 identity includes
  registering owner. Exact unique matching prevents ambiguous or missing
  hotkeys from operating on other sources. OBS routed press/release executes
  the original registered callback.
- Actual native dock visibility toggles (not fake panels), checkable theme
  outline, inline mouse drag-to-reorder, right-click rename/move/remove,
  separator, stretch spacer and editor dialog. Flexible overflow menu
  activates on constrained bars. Theme-derived Qt palette and font;
  natural minimum item widths; dynamic corner repositioning prevents
  clipping when a longer hotkey is added.
- Bounded cancelable late-dock retry (8 attempts, 2s interval) specifically
  for the already-existing native Audio Mixer backside. No secondary
  dock manager or native workspace migration.
- New Inspector Action strip tab and native Tools -> Aftermarket: Action
  Strip editor. Emergency key hides customizations, retains desired
  enabled preference and saved items; explicit Resume reattaches the
  strip while restoring original native menus and dock identities.

TDD: three new tests action-strip, native-action, dock-attach-retry,
updated inspector test. Covers QMenuBar original corner recovery, safe
teardown/recreate, model persistence/reorder, Qt actual drop events,
narrow-to-wide overflow action routing, source registerer uniqueness,
record/stream clock with pause, exact native dock toggle, retries/cancel.
Full CTest: 20/20 PASS, existing unrelated knob-test AutoMoc warning.

Real isolated OBS 32.2.0/Colorway using disposable Xvfb :114 profile:
displayed native hamburger and live status on same top bar. Toggle Mixer
hid/revealed original OBS dock. Opened editor via original Tools submenu.
Hotkey chooser created owner-aware Disable Preview binding; activating
it produced OBS's real 'Preview is currently disabled' screen.
12-item crowded bar at 860px width displayed a functioning overflow
menu with extra native OBS status values; no crushed last button.
Emergency Ctrl+Alt+Shift+F12 restored classic native menus and mixer
while writing marker and preserving action-strip enabled JSON; a
separate isolated OBS restart stayed in safe mode. Resume restored
hamburger and action strip with the saved items. Previously found Qt
hidden menu-bar corner on resume reproduced in a failing test and
fixed in both Qt and the live OBS copy. Screenshot saved as
docs/screenshots/readme/action-strip.webp, other test screenshots
remain under build/action-strip-smoke/.

No replacement Scenes list, new mixing engine, arbitrary dock relocation,
or StreamUP/Timeline state modification. Output clocks cannot reconstruct
unknown elapsed time before the plugin observed the start event.
Source/plugin uniqueness is enforced, so indistinguishable hotkeys remain
disabled. Broader cross-platform/theme/DPI validation remains open.
No user OBS scene collection was changed and no upstream push.

### Slice 30 verified local deployment (2026-11-22)

- Feature commit: ef2dc5e. Full CTest 20/20 passed, and real isolated
  OBS 32.2.0/Colorway checks covered strip display, overflow in an 860px
  window, native Mixer hide/show, owner-aware Disable Preview hotkey,
  Tools/Inspector editor and emergency/restart/resume behavior.
- With no user OBS running, installed the tested binary via the repository's
  atomic scripts/install.sh; destination bytes match build/obs-ui-augment.so.
  Installed SHA-256: e14f75bff1c8944576136881301341a27b2d22606dbc1cb6a167e94aa1150ba7.
- Preserved the previous installed plugin, SHA-256 prefix 65666a050f4a, at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-action-strip.65666a050f4a.so.
- Confirmed the user's rules.json SHA-256 remained exactly
  b0286e2ec9e01ae6fc7d1ba29b688382ed0e41313c1bda70c3ad5edcbb0c0fbf.
  Their action-strip.json was absent both before and after installation:
  the feature is opt-in, with an empty item list when first opened.
  No user OBS scene/recording/route changes, no user OBS process changes,
  no external push. It will load on the next ordinary OBS launch.

## Slice 31 — native AudioHub Monitor output endpoint (2026-11-22)

Intent: Aftermarket's native Audio Mixer backside/AudioHub already supported
three OBS source monitoring modes through a dropdown, but its visual patchbay
only represented six native output-track assignments. Add a distinct Monitor
destination, not a fabricated track 7 or a secondary audio engine.

Behavior: a pinned MON output jack in the graph header (visible in the short
stock OBS mixer dock), plus a theme-aware checkable MON output button alongside
tracks 1–6. The six numbered endpoints and native mixer masks retain their
previous positions and semantics. Graph cables now render the native
monitoring route when per-source OBS monitoring mode is not Off. Hover
focus and Show all wires include the monitor route; MON hover reports source
counts and explicitly distinguishes it from Track 7. Click graph MON or the
button to toggle the selected source between Off and Monitor and Output;
drag any source to MON to connect idempotently without changing existing
Monitor Only or any of its assigned track bits. The existing 3-mode dropdown
remains the exact control for Off, Monitor Only and Monitor and Output,
synchronized from real OBS state. All routing uses the existing
ObsRouting::setMonitoring(uuid,mode) with UUID checks.

Safety: connecting to MON defaults to Monitor and Output rather than Monitor
Only, so the operation does not silently suppress ordinary output. OBS
Settings > Audio > Advanced owns the Monitoring Device. Monitoring can
create echo/feedback when monitored device is captured; tooltip and README
explain that. Monitor Only suppresses normal program output even if track
assignments remain visible as wires. No separate destination device, internal
mixing engine or libobs source was created.

TDD: RED new connectMonitor method failed compilation; initial UI regression
failed under original 170px dock minimum when MON was introduced as a
seventh scrolling row; GREEN after pinning MON into the graph header and
preserving six-track row geometry. Updated Qt test covers real QMouseEvent
source-to-MON drag, idempotent re-drag, MON node click twice, checked
indicator sync, native dropdown and Monitor Only preservation, unknown UUID
rejection, unchanged mixer masks, responsive graph minimum and actual cable
render pixel changes. Full CTest: 20/20 PASS; no unrelated change.

Real isolated OBS 32.2.0 (Colorway theme), disposable Xvfb display :115 and
copied XDG/OBS profile, exact compiled plugin SHA-256
ba12c4f507c8b29247e82f7e43ab2e9e57377d16e674d8854a592c4084cb6691.
Native Audio Mixer backside visibly displayed MON at graph header even with
short graph viewport. Clicking MON on the selected Silent test source changed
real OBS monitoring mode Monitor Only -> Off -> Monitor and Output in the
source dropdown and diagram, without changing tracks 1,2,4,6. That test
source was muted; audible playback to a physical device is NOT verified.
Screenshot: docs/screenshots/readme/audio-monitor-output.webp. Full isolated
screenshot trail under build/monitor-output-smoke. User's running OBS scene
collection and audio devices were not touched. No upstream publication.

### Slice 31 verified local deployment (2026-11-22)

- Source feature commit: 819f676. Full CTest 20/20 passed.
- Exact tested and installed binary SHA-256:
  ba12c4f507c8b29247e82f7e43ab2e9e57377d16e674d8854a592c4084cb6691.
  Isolated OBS 32.2.0 loaded that exact binary; clicked live MON output
  and observed native monitoring state update in its disposable profile.
- Installed using repository's atomic scripts/install.sh, not touching the
  user's ongoing OBS PID 3118186, which stays on its previously loaded
  module until the next voluntary restart.
- Previous installed binary preserved unchanged at:
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-monitor-output.e14f75bff1c8.so.
- Existing user rules.json SHA-256 unchanged:
  b0286e2ec9e01ae6fc7d1ba29b688382ed0e41313c1bda70c3ad5edcbb0c0fbf.
  Existing action-strip.json SHA-256 unchanged:
  0585e96a5a4f4b702372062a1a78b245a59424350a0a86d4b6c4a803fa192ced.
- Temporary Xvfb :115 and isolated OBS stopped. No live user OBS routing,
  scene collection, monitoring devices, recording or process was changed;
  no push. Physical monitoring-device audibility was not tested.

## Slice 32 — AudioHub graph simplification and native Audio Mixer front controls (2026-11-22)

User screenshot showed a misaligned Tracks header, duplicated source selector
and output button row, MON text, verbose diagnostics, and a desire for real
activity dots plus per-source pan/sync offset with optional knobs. This
checkpoint covers the graph AND mixer front, not only the last changed widget.

- Backside AudioPatchbay retains UUID-authoritative native OBS route and
  monitoring callbacks and hover/drag interactions. Removed the redundant
  source dropdown, lower 1–6/MON button row and text diagnostics/status
  messages. Graph source nodes themselves select; the native 3-mode
  monitoring dropdown remains a selected-source control, now labelled
  Monitor, and the right heading is Outputs. Headphone output is drawn as
  a Qt vector, not a fragile font glyph. No hardcoded widget colors.
  Measured source and output status dots now use a scoped libobs volmeter
  per UUID (ObsSignalLevels), storing atomic peak magnitude and sample
  timestamps and ignoring stale/unknown signals; output dots aggregate
  active unmuted, enabled, routed sources and respect Monitor Only.
  The graph never represents source-enabled state as proof of sound.
- Original OBS Audio Mixer front preserves VolumeControl, native fader,
  mute and monitoring controls. Added per-source Pan (OBS balance [0,1])
  and precise Sync offset (millisecond field mapped to native OBS
  nanoseconds) into each native vertical VolumeControl layout. Optional
  Knobs presentation replaces horizontal pan slider with themed QDial
  and adds a coarse ±2000ms sync dial while retaining exact sync input.
  Compact two-row layout was revised after the **real isolated OBS**
  mixer showed 81–88px-wide VolumeControl columns; first attempt had
  a 0px slider height, fixed by explicit 20px slider and 23px spinbox
  hit targets with reversible 48/57px row heights.
  NativeAudioSettings only matches valid audio UUIDs, rejects invalid
  edits and disables pan for mono. FrontAudioControls matches the native
  VolumeName labels to UUID-backed sources, rejects ambiguous names,
  readbacks external OBS edits, writes through native libobs setters,
  and removes every plugin-owned addition on detach.
- Knob presentation stored separately via QSaveFile in
  plugin_config/obs-ui-augment/front-audio-controls.json, not in OBS
  scene/routing data. Successful native changes schedule a **coalesced
  320ms obs_frontend_save()** request; native readbacks never save.
  The backing OBS project persistence path is source-of-truth and is
  not replaced. NOTE: In isolated Xvfb OBS, UI wheel changes were
  displayed (0ms->10ms->30ms), and setter save-request callback behavior
  is test-covered, but the inspected scene JSON did **not** change.
  This is an explicit unresolved **end-to-end on-disk persistence
  verification gap**, not evidence of verified persistence in the user's
  normal OBS session. The local test profile appears to use a different
  source file than the direct edited JSON (log still referenced the
  original tone.wav after JSON was changed). Do not claim real saved
  pan/offset values until normal-profile validation proves it.
- DockFaces now supports a removable front-only header accessory Knobs
  toggle, shown only while the front face is active. FrontAudioControls
  resets before DockFaces restores the exact native mixer widgets in
  emergency or unload. The existing alternate AudioHub face remains
  fully reversible.

TDD: a complete replacement AudioPatchbay regression was RED until old
controls were removed and SignalProvider/graph UI implemented; focused
Qt test covers no duplicate widgets, active and inactive signal
provider readings, native headphone routing, hover/selection, missing UUIDs.
New FrontAudioControls Qt test was RED until controls, external sync,
both presentation modes, model writeback, exact native preservation,
narrow-90px layout and coalesced save-request hook were implemented.
Native libobs obs-routing regression verifies balance and nanosecond
sync-offset setters and readback, invalid UUIDs, source rename.
Updated DockFaces regression verifies reversible header accessory and
front/back visibility.

Real isolated OBS 32.2.0, Colorway theme, copied disposable XDG profile on
Xvfb :116. Captured graph routing screenshot with actual Tone test
metered activity: a source and routed output dot lit after unmuting
the tone, while Silent test stayed dim. Front screenshots demonstrate
usable pan and sync sliders and Knobs mode in 81–88px native mixer
columns. Tested emergency Ctrl+Alt+Shift+F12 from the front: the entire
custom UI and source adjustments disappeared and the original mixer
returned while OBS remained running. Byte-for-byte SHA256 of copied
rules.json, action-strip.json and front-audio-controls.json were
identical before and after emergency; safe-mode.flag was written.
Screenshots: docs/screenshots/readme/mixer-patchbay.webp and
docs/screenshots/readme/audiohub-front-knobs.webp.

No user OBS scene, settings, recordings or audio routes touched, no
physical audio output proof and no upstream push.

### Slice 32 verified local installation (2026-11-22)

- Source commit: ddd3f8a. Full CTest: **21/21 pass**, zero new warnings.
- Installed tested binary via repository atomic scripts/install.sh; installed
  /home/myrqyry/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so
  matches built SHA256 00be17a62ce0243a94b0de005ad54dcfc801c0a1745895d61ab1ea6e5f1975dd.
  Prior version backed up byte-for-byte to
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-audiohub-redesign.ba12c4f507c8.so.
- Actual user OBS pid **3420671** (obs --verbose) was NOT restarted or
  modified; it continues running its previously loaded module until the
  user voluntarily closes/reopens OBS. Old vs new compiled module SHA
  differ as expected.
- User rules.json SHA unchanged:
  b0286e2ec9e01ae6fc7d1ba29b688382ed0e41313c1bda70c3ad5edcbb0c0fbf.
  User action-strip.json SHA unchanged:
  0585e96a5a4f4b702372062a1a78b245a59424350a0a86d4b6c4a803fa192ced.
  User front-audio-controls.json was absent before and after install;
  it is created only when the user changes knob presentation.
- Disposable test OBS / Xvfb :116 terminated. No user OBS scenes, routing,
  levels, recordings or monitoring devices touched; no upstream push.
- Still open: end-to-end verification that a pan/sync edit is durably
  persisted to OBS's actual scene JSON. Native libobs setter/readback,
  debounced frontend save request and UI redraw tested; isolated profile's
  collection file did not reflect the test adjustment. Do not silently
  promote this to a verified live persistence claim.

## Slice 34 — AudioHub circular destinations and source icons (2026-11-22)

Follow-up from user's annotated screenshot of the narrow native Audio Mixer
backside. Requested four visual/interaction corrections to the previously
implemented AudioHub: move measured activity dots to the right edge of source
cards, add source icons, remove the Outputs text label and bottom Monitor
dropdown, and make all six track and headphones destinations circular.

Implementation:
- Existing libobs ObsRouting::list now carries each source's actual registered
  unversioned OBS source ID into RouteSource::sourceType; the ID is included
  in RouteSource equality so native source-type changes repaint. Qt-palette
  pictograms classify registered IDs for browser, media/FFmpeg/VLC, mic/input
  capture, output/desktop capture, and an audio waveform fallback for plugin
  sources including TTS. No source names are parsed, and no fixed icon colors.
- Source activity dots move to the RIGHT of the source card (left glyph,
  centered/elided label, right measured-signal dot); the original cable
  connection pin remains outside the right edge and distinct. Each output
  activity dot also sits outside its circle on the RIGHT.
- Outputs 1-6 and the headphones monitor are drawn as real 28px diameter
  circles; hit-testing checks radial distance instead of a bounding square.
  Idle rings blend QPalette::Mid with QPalette::Text for visibility across
  dark/light themes rather than hardcoded Colorway colors. Hover/focus,
  selected-route drawing, click-to-toggle, source drag-to-connect and Show
  all wires remain unchanged.
- Removed the in-canvas Outputs heading and the entire bottom Monitoring
  row/dropdown. The original OBS three monitoring modes remain reachable
  by RIGHT-clicking the headphones circle: Off, Monitor Only, Monitor and
  Output, with the current OBS mode checked. Normal LEFT-click still toggles
  Off / Monitor and Output. Mode state is synchronized on each source
  selection/OBS refresh.
- Important real-OBS regression: an initial right-click implementation on
  QMouseEvent showed OBS's unrelated native Audio Mixer context menu because
  Qt separately emits QContextMenuEvent. Reproduced as RED test with an
  unaccepted QContextMenuEvent, then GREEN by overriding the canvas
  contextMenuEvent and accepting it only for the headphones circle. Other
  right clicks remain native.

Verification:
- Updated Qt AudioPatchbay test covers absence of old dropdown/labels,
  source-ID icon classification, live glyph pixel change, right-side signal
  pixel changes, truly round node pixels and corner click rejection,
  all native drag/click/hover routes and meters, monitor mode action
  checked state, explicit Qt QContextMenuEvent acceptance, stale-source
  rejection. Updated libobs routing test checks sourceType returned from
  native unversioned source ID.
- Full CMake build and CTest 21/21 PASS, no build warnings. Source diff check clean.
- Isolated OBS 32.2.0 on disposable Colorway profile and Xvfb :117
  (build/audiohub-circle-smoke), user OBS untouched. Real screenshot shows
  source icons at left, measured dots at right, clearly ringed circular
  destination nodes, no Outputs heading/dropdown. Clicked Tone test source,
  highlighted its original native Track 1 route. Right-click on the native
  headphones circle opened the correct 3-mode context menu (not OBS generic
  mixer menu); clicked Monitor Only and verified it was checked on reopen.
  Ctrl+Alt+Shift+F12 emergency restored the real native mixer with
  original strips and left the disposable OBS process running.
- README descriptions updated and screenshots:
  docs/screenshots/readme/mixer-patchbay.webp,
  docs/screenshots/readme/audiohub-monitor-menu.webp.

Boundary: No new monitoring device or track, no changes to OBS audio source
values, no live user OBS UI/scene/routing edits, no publication. Physical
audio delivery is not inferred from UI signal dots. Pan/sync persistence
gap from Slice 32 is unrelated and remains open.

### Slice 34 — verified local installation (2026-11-22)

- Implementation commit: 0b63e67; full CTest 21/21, no build warnings.
- Atomic scripts/install.sh updated the installed plugin with binary SHA256
  0b6af0a9b9c49b5c154822d7cf9c3f413f5788de85d8edf463d8457ba2e35f26,
  byte-for-byte matching the tested build.
- Previous plugin backed up unchanged at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-audiohub-circle-icons.00be17a62ce0.so
  (previous SHA256 00be17a62ce0243a94b0de005ad54dcfc801c0a1745895d61ab1ea6e5f1975dd).
- User's actual OBS PID 3524994 stayed running throughout and is still using
  its already-loaded old module until a voluntary OBS restart. All three
  settings files (rules.json, action-strip.json and front-audio-controls.json)
  had unchanged SHA256 values before and after installation. No user's
  scene, audio, status, mixer output, recording or monitor routing was changed.
- Isolated OBS and Xvfb :117 stopped. Repository remains local only; no push.

## Slice 35 — controls IN the native dock title bar, and truthful signal dots (2026-11-22)

User requested work on dock header buttons and status dots, then corrected
an important ambiguity: buttons belong in the **actual QDockWidget title bar**
alongside OBS's existing undock action, *not* an Aftermarket-owned second
"Front / Routing" header row. Preserve this requirement for future passes.

- DockFaces now hosts **direct child DockHeaderButton** controls in the real
  Audio Mixer dock title region: presentation Knobs toggle, reversible
  front/back face switch, and optional previous/next tab buttons. The
  extra internal content-area header was removed. OBS's true dock title,
  QDockWidget::titleBarWidget identity, native paint/drag region, native
  float/close hit areas, original fader widget, QDockWidget features,
  toggleViewAction and tab management remain owned by OBS.
- Custom 20x18 glyph controls have palette-derived normal/hover/checked
  states, accessible names and tooltips. Qt's SE_DockWidgetTitleBarText
  informs horizontal placement and real native content top defines
  vertical placement; buttons yield to long titles and narrow docks.
  The face switch has highest priority, then front-only Knobs, then
  tab arrows as an optional pair if space permits. On floating or
  vertical-title docks, no extra overlay is shown; platform decoration
  takes precedence. Dock resize, title changes, palette/style changes,
  docking and tab changes schedule repositioning.
- The first attempt put a translucent QWidget **over** the title bar,
  which rendered a black rectangular gap in the real Colorway/Xvfb
  screenshot, despite passing the Qt unit test. Removed the wrapper
  entirely: buttons are direct dock children with no background surface.
  Confirmed a screenshot with the real "Audio Mixer" title visible,
  the two Aftermarket buttons, and the original native undock icon
  side-by-side. Real pointer clicks on the FACE icon opened AudioHub
  and returned to the native mixer. Pointer clicks on the Knobs icon
  toggled the isolated front-audio-controls.json preference true/false.
  No custom "Mixer" or "Routing" row exists inside dock content.
- Source/output activity dots now distinguish live measured signal
  (filled highlight), measured silence (ring with center point),
  muted/disabled (slashed ring) and unknown/stale/inactive meter
  (unfilled ring). Each uses active Qt palette colors; source and
  output dots are separated from connection ports. Output aggregation
  respects source monitor-only routing and prioritizes Active, then
  Unknown (if any unmeasured routed source), then known Silent,
  then Muted. This avoids falsely labeling a destination silent or
  entirely muted when it contains an unknown source.

TDD:
- RED dock-faces-test on missing direct-title-area controls; GREEN after
  moving from owned internal header to actual QDockWidget title.
  Tests assert direct dock parent, in-title geometry, no second
  internal row, QDockWidget original title widget/features/toggleView
  identity, front/back check state, color response, tab navigation,
  long-title safety, floating-dock hiding/redocking and exact detach.
- RED AudioPatchbay tests on missing ActivityState, unknown/inactive
  falsely displayed as silence, and mixed unknown+muted output
  incorrectly displayed as muted; GREEN with state semantics.
  Additional test verifies drawing follows changed highlight palette.
- Full CMake build and CTest **21/21 PASS** with no compiler warnings,
  git diff --check clean.
- Real isolated OBS 32.2.0 with Colorway theme on disposable Xvfb :118:
  tested true dock titlebar placement beside original undock action,
  pointer face flip to/back from AudioHub, pointer knob toggle
  updating disposable configuration and original native controls,
  source/output activity states with test tone, and no black rectangle.
  Screenshots refreshed:
  docs/screenshots/readme/mixer-patchbay.webp and
  docs/screenshots/readme/audiohub-front-knobs.webp.
  Test instance was later stopped while the user's own OBS session
  was independently restarted; do not claim an end-to-end emergency
  key result from that interrupted attempt. Qt emergency-recovery
  test still passed, and DockFaces detach restores native identity.
- No user OBS scene/routing/settings changed, no production process
  restarted by our test commands, no upstream push. The separate
  native pan/sync on-disk save verification gap remains unresolved.

### Slice 35 — verified native-title installation (2026-11-22)

- Feature checkpoint: 084f135. Full CTest **21/21 pass**, no compiler
  warnings. Original OBS 32.2.0/Colorway checked by real GUI clicks and
  screenshot; exact final built SHA256
  41920d1b9165bc5546f524952a79a74dd96abe1b1fced7c50cb7649ffdbded20
  was subsequently loaded successfully in isolated OBS on Xvfb :118.
- Installed through the repository's atomic scripts/install.sh.
  Installed binary byte-for-byte matches the final built SHA256 above.
  Previous binary SHA256
  0b6af0a9b9c49b5c154822d7cf9c3f413f5788de85d8edf463d8457ba2e35f26
  preserved in
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-native-dock-title.0b6af0a9b9c4.so.
- All user configuration SHA256 hashes for rules.json, action-strip.json and
  front-audio-controls.json remained unchanged. User's live OBS PID
  3601543 was not restarted or changed by installation; it retains its
  previously loaded code until the user voluntarily restarts OBS.
  Disposable OBS and Xvfb :118 terminated. No upstream push.

## Slice 36 — passive, theme-aware icons beside native OBS dock titles (2026-11-22)

User request: add icons to the titles of **each** dock, following earlier
placement of Mixer/Routing and Knobs buttons directly in the real OBS dock
title bars. Do not create another internal header, change dock titles, replace
Qt title painting, interfere with native undock/close hit targets, or disturb
the user's scene/layout.

Implemented new DockTitleIcons manager (src/dock-title-icons.{h,cpp}),
initialized in startCustomUi before DockFaces and removed on stopCustomUi
after DockFaces. It attaches transparent-for-mouse, nonfocusable,
Qt-palette-colored 14px vector glyphs directly to native QDockWidget
children. Icons are drawn immediately **after** the native title text.
QDockWidget titleBarWidget, windowTitle, features, toggleViewAction,
native title painter and drag/float/close semantics remain unchanged.
Icons classify actual dock object IDs where available, then displayed titles:
Scenes (stacked frames), Sources (linked nodes), Audio Mixer (faders),
Transitions (bidirectional arrows), Controls (toggle), Preview (monitor),
Chat (speech bubble), Browser (globe), and a generic panel fallback.
This supports third-party docks without assuming their identity.

Automatic main ChildAdded scanning discovers newly created plugin docks.
A dock name change updates the glyph classification; font, theme, style,
title, parent dock resizing and visibility trigger refresh. If the Qt
native title region has 21 or more free pixels, use a 14px icon; 14-20
free pixels use 10px compact artwork; otherwise hide rather than cover
native text/float/close actions. Floating, vertical-title, no-content,
hidden/tabbed, and custom title-bar docks are skipped. The existing
DockFaces mixer title controls reserve space after any visible title
icon, avoiding icon/button overlap.

TDD / verification:
- RED test for missing feature and classifier, then GREEN. Additional RED
  when plugin dock was renamed from generic to Twitch Chat but kept the
  previous generic glyph; GREEN after automatic glyph kind refresh.
  Additional RED/GREEN for 10px adaptive icon availability.
- New dock-title-icons-test covers each native dock role/fallback,
  distinct glyph pixels, palette-based repaint, true native title
  identity, transparent mouse behavior, title/text/controls geometry,
  existing Audio Mixer face-toggle coexistence, auto discovery of
  late-added plugin docks, rename classification, long-title overflow,
  floating/redocking, custom title bar precedence, and exact teardown.
- Full CMake build + **22/22 CTest PASS**, no new compiler warnings,
  git diff --check clean. The previous 21 test suite grew by one test.
- Loaded the EXACT SHA256
  886fc1295e87c1ca32cec87ed47b01ba684a136855482c9ccff1706ebb318d14
  in a separate real OBS Studio 32.2.0 instance on disposable Xvfb :119,
  copied Colorway profile with no user settings mutations. Screenshots
  show distinct icons beside native Scenes, Sources, Audio Mixer, Controls
  and show AudioHub face switch into and out of the same titled dock.
  Scene Transitions was too narrow in that OBS dock arrangement to show
  its icon without colliding with native undock control, so it hides;
  wider docks can show it. Screenshot:
  docs/screenshots/readme/dock-title-icons.webp.

No user OBS scene, audio state, saved layout, recording, or settings altered.
The feature is default-on as requested while Aftermarket UI is active,
and its entire decorator is removed on emergency stop/unload.
No independent user toggle or per-dock customization was added, and
no upstream push is implied.

### Slice 36 — verified local deployment (2026-11-22)

- Feature commit: 89905a0; all **22/22 CTest** passed after the adaptive
  14/10px title icon adjustment; no compiler warnings on final build.
- Verified exact SHA256 886fc1295e87c1ca32cec87ed47b01ba684a136855482c9ccff1706ebb318d14
  loaded in isolated OBS 32.2.0, with native Audio Mixer title and face
  switch still working. Installed this exact binary by repository's atomic
  scripts/install.sh.
- Previous installed binary SHA256
  41920d1b9165bc5546f524952a79a74dd96abe1b1fced7c50cb7649ffdbded20
  preserved byte-for-byte at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-dock-title-icons.41920d1b9165.so.
- All three user config file hashes were identical before/after installation:
  rules.json, action-strip.json, front-audio-controls.json. No user OBS
  process was running during this install; none was started or stopped.
  Disposable OBS and Xvfb :119 terminated after verification. No push.

## Slice 37 — dock icons BEFORE native title text (2026-11-22)

User correction: icons must appear **left of each dock title**, in the
familiar icon–label order of OBS context menus, not appended after titles.
This is a correction of Slice 36 placement, not an invitation to replace
native title bars or change persistent dock names.

Findings: native Colorway/QSS QDockWidget title painting ignores
QProxyStyle::drawControl edits to QStyleOptionDockWidget.title and .rect,
and ignores a direct per-dock CSS padding-left override. Those paths were
tested with real OBS 32.2.0 / Colorway in a separate Xvfb :120 disposable
profile and visually FAILED: the text still overlapped the left glyph.
The reliable Qt native painter path is to add *temporary visual leading
spaces* to its on-screen windowTitle. Preserve the original verbatim in
DockTitleIcons (originalTitles_), track exact applied text
(displayedTitles_) and preserve original toggleViewAction labels
(originalActionLabels_). The source objectName, titleBarWidget, close/
float controls, state, font, palette and native painter are untouched.

Current behavior: 14px/10px palette-based vector glyph drawn at the left
of the Qt title text region; QFontMetrics computes enough leading display
spaces for icon+gap; the actual native title text then starts after the icon.
The OBS Docks menu QAction stays the **original unpadded label** (verified
from a real menu screenshot, not just a fake test). External source/plugin
renames update the original-title record without accidental prefix
accumulation; title change notifications generated by Aftermarket itself
are ignored even if delivered later. Layout refreshes cannot double-prefix.
When there is no available room, or on hidden/floating/custom title bar,
the glyph hides and original title is restored. On plugin unload or
emergency Ctrl+Alt+Shift+F12 the exact original titles are restored and
glyphs removed; user configs and existing OBS dock settings are unchanged.
This transient display-title spacing is a deliberate, reversible
compatibility mechanism, not a lasting rename. Keep it explicit in docs.

Tests: TDD RED for left-of-text placement/text reservation, then GREEN
after replacing ineffective QProxyStyle. RED regression on repeated
refresh/external rename and teardown lost original title; fixed event
filter recognition of delayed self-generated QEvent::WindowTitleChange.
New tests ensure original Docks toggle labels, external plugin renames,
no accumulated prefix after eight refreshes, long-title safety,
floating/custom-title precedence, distinct glyphs/palette changes,
native titleBarWidget identity and precise restoration on teardown.
Real Colorway/Xvfb :120 screenshots compare before/after; final shows
icon → space → native title, with genuine color/theme rendering.
Native Docks menu screenshot shows unpadded Scenes/Sources/Audio Mixer.
Real isolated OBS emergency recovery restored default titles and removed
all icons while OBS remained running; safe-mode.flag written, three
copied config SHA256 values unchanged before/after.
Screenshot to README: docs/screenshots/readme/dock-title-icons.webp.

User OBS process PID 3638459 was not touched during isolated tests.
No frontend native menu, dock objectName, saved layout, scene, stream,
source audio, or program settings were edited. No push. Prior separate
AudioHub pan/sync disk persistence gap remains unrelated and open.

### Slice 37 — verified local installation (2026-11-22)

- Feature commit: ad3a0f2. Full build and **22/22 CTest PASS**,
  no compiler warnings. The exact tested binary SHA256
  7ae1af78344da322cb181f5d0a65f30255bd0c6170776f7ed06d54bc9ccd7516
  was loaded in isolated OBS 32.2.0/Colorway and installed via the
  repository's atomic scripts/install.sh. Installed and built bytes match.
- Prior installed SHA256
  886fc1295e87c1ca32cec87ed47b01ba684a136855482c9ccff1706ebb318d14
  retained byte-for-byte at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-left-title-icons.886fc1295e87.so.
- User's live OBS process PID **3638459** remained unchanged and was not
  interrupted; it still holds the previously loaded plugin until the
  user voluntarily restarts. User rules.json, action-strip.json and
  front-audio-controls.json SHA256 all unchanged. No git push.
- Disposable Xvfb :120 and OBS were terminated after test. Working tree
  clean at feature checkpoint. The original dock title strings are
  restored on unload/emergency and Docks menu labels stay unpadded.

## Slice 38 — AudioHub below the native mixer, title overflow and compact two-orientation controls (2026-11-22)

User annotated a cramped real Audio Mixer screenshot: place AudioBus/AudioHub
at the **bottom**, move the hidden-channel/footer settings into a native-title
vertical-ellipsis menu, and refine both OBS mixer orientations with horizontally
aligned controls. This work remains inside the existing Aftermarket project,
`obs-ui-augment`; it does not belong to OverlayKit.

Implementation:
- DockFaces now preserves the exact original OBS AudioMixer widget as the
  **upper** half of a vertically draggable splitter and places the existing
  AudioPatchbay underneath, visible by default. The original title-region
  face glyph collapses/expands the bottom panel rather than replacing the mixer.
  The native widget and all of its signal, source and routing identities remain.
- DockHeaderGlyph::More provides a palette-aware vertical three-dot control
  in the real QDockWidget title. Its menu exposes AudioHub visibility, the
  pan/sync knob presentation toggle, the native **0 hidden** channel entry,
  mixer orientation, Advanced Audio Properties, Mixer options, and an editable
  per-source Pan/Sync submenu. Original OBS button signals and the native
  Options menu are reused where available; nothing invents a second mixer.
- FrontAudioControls preserves the native footer QToolBar identity and hides
  it only when the three-dot control is actually visible. The footer returns
  automatically when the title button disappears (floating, cramped or
  unsupported title geometry), and restores on detach/emergency unload.
  Unknown toolbars are not hidden. Narrow vertical VolumeControl strips use
  pan dial + precise milliseconds on one 30px row; wider and horizontal
  strips use one aligned horizontal row, with the presentation toggle in the
  overflow. Existing native faders, mutes, settings APIs and source UUIDs stay.
- The native vertical volMeterFrame, VolumeMeter and fader height constraints
  are **temporarily** compacted while AudioHub is open to reclaim unused
  tall-slider space (especially when the painted fader is a small rotary).
  Their captured original bounds restore on bottom-section collapse, switch to
  native horizontal mixer or teardown. No gain/monitor/track/scene values change.
  The AudioHub canvas remains scrollable when a dock is shorter than the whole
  six-track graph. No OBS fork or external DSP bus.

Verification:
- RED tests first for missing split/overflow and native-footer delegation,
  narrow 30px row, native vertical constraint compression/restoration; GREEN
  regressions cover exact widget identity, native controls, title placement,
  menu proxies, floating/narrow footer fallback, knob setting and source edits,
  vertical/horizontal mixer reflow, mouse hit regions, and teardown.
- Final CMake build clean (no compiler warnings), **22/22 CTests passed**,
  git diff --check clean.
- Real isolated OBS **32.2.0** / Colorway under separate Xvfb and disposable
  config: initial clipping of lower patchbay was observed and corrected.
  The resulting vertical view shows native rotary mixer, compact pan/sync
  strip, and bottom routing nodes; the patchbay scrolls on short docks.
  Real XTest pointer click at native title ellipsis opened the complete
  settings popup; a separate real click triggered OBS's native horizontal
  orientation and displayed horizontally aligned controls and bottom
  AudioHub. Two checked-in cropped optimized WebP screenshots:
  `docs/screenshots/readme/audiohub-bottom-menu.webp` and
  `docs/screenshots/readme/audiohub-horizontal.webp`.
- Local build SHA256
  `0d63bbad9ba0050d670e4d7dfc462f63a0d4da39454492323ac125e860ddef86`
  installed atomically at
  `~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so`;
  installed file byte-for-byte matches the tested build. Old binary preserved
  unchanged at
  `~/.local/share/aftermarket/rollback/obs-ui-augment.pre-bottom-audiohub.7ae1af78344d.so`.
  The user's actual OBS **PID 27607** stayed running and was not restarted by
  this installation, so it retains its previously loaded plugin until a future
  OBS restart. All three live config files (rules.json, action-strip.json,
  front-audio-controls.json) retained exact pre/post SHA256 hashes.

Boundaries: Native OBS options-submenu *activation*, per-source change
persisted through an actual user's OBS restart, source-add/rebuild geometry,
other platforms/themes, and physical audio delivery were not claimed as
end-to-end proven. The pre-existing pan/sync native disk-persistence gap
remains separate and open. No scene/audio routing or user's current profile
was edited, no user process restarted, and no GitHub push requested.

## Slice 39 — correction: bottom AudioBus *control*, not bottom AudioHub pane (2026-11-22)

User clarified that "move audiobus to bottom" meant moving the **interactive
AudioBus view-switch element** to the bottom of the mixer dock, **not** putting
the entire routing interface beneath the native mixer. The prior Slice 38
split-panel interpretation was incorrect. This entry supersedes that layout;
Slice 38 remains in history as evidence of the misinterpretation.

Implemented and checked in the same checkout:
- Restored the native mixer/AudioHub **exclusive two-page QStackedWidget**.
  Front is the existing original OBS AudioMixer widget, back is the existing
  AudioPatchbay. Only one occupies the dock content at a time.
- Added a *single compact*, native themed, checkable **AudioBus** QToolButton
  at the bottom edge of the dock. Clicking opens the AudioHub routing page
  and its text changes to **Audio Mixer**; clicking again restores the mixer.
  It remains usable when the dock floats or its title buttons are hidden.
  No routing graph, second header, or native toolbar is duplicated.
- Removed the old title-bar flip glyph; the title keeps the **vertical ellipsis**
  menu for native Hidden, Options, orientation and Advanced Audio Properties,
  knob mode and the channel pan/sync submenu. The native footer restoration
  fallback and the one-row horizontal pan/sync controls remain. Reverted the
  split-only tall fader/meter compression; real native sizes no longer need
  to shrink to make space for a second pane.
- Corrected README and actual cropped 8–16 KB WebP views: current native
  mixer with bottom switch, alternate AudioHub routing, and title settings
  dropdown. Removed misleading Slice 38 screenshots from the working tree;
  their original bytes remain recoverable from Git history.

Verification:
- Wrote the failing regression first (old layout still had splitter and
  no bottom AudioBus button). Focused green for dock faces/dock title icons;
  full **22/22 CTest passed**, no compiler warnings or diff whitespace errors.
  Tests check exclusive pages, bottom position, real button click, title
  accessory independence, floating and long-title fallback, restore and
  teardown.
- Real isolated OBS 32.2.0/Colorway, disposable Xvfb and config, observed
  full native mixer on first load with just the bottom AudioBus button,
  clicked bottom switch into full routing view, clicked **Audio Mixer** to
  return, then clicked vertical ellipsis to show original hidden controls
  and channel settings. Preserved native theme and source readouts in the
  screenshots. The isolated OBS instance shut down.
- Installed the exact final built binary SHA256
  `370d4baf126dfe11514843a88d660c9d745eb59e38cd8c40553f627e8d2ef961`
  through scripts/install.sh; installed file matched build byte for byte.
  Rollback backed up at
  `~/.local/share/aftermarket/rollback/obs-ui-augment.pre-toggle-only.0d63bbad9ba0.so`.
  Live user OBS PID **73590** was unchanged and not restarted; it may still
  have the previous module loaded until the user's next restart. The three
  live config file hashes (rules, action strip, front-audio) were identical
  before and after install. No user scene/audio/routing values changed.
- No push or external publication; source is local to the user's loam.

Remaining: a subsequent restart of the user's live OBS is needed to see this
installed binary in that running process; isolated tests are not evidence of
the user's active audio delivery or on-disk pan/sync source persistence.

## Slice 40 — global dock three-dot menus and Collapse/Expand first (2026-11-22)

User asked for the three-dot overflow on every OBS dock with Collapse/Expand to
start. The corrected Slice 39 bottom AudioBus switch and exclusive mixer/AudioHub
faces remain unchanged. The superseded split-panel version was considered
visually acceptable but not requested back in this slice.

Implementation:
- Extended existing DockTitleIcons to install a theme-aware more button on
  native-titled OBS and newly added plugin docks. One deferred update covers
  native Qt layout assignment after the plugin dock is created.
- The first menu item uses existing Panels canCollapse/collapsed/toggle to
  Collapse or Expand. Unsupported native/tabbed layout disables that item;
  floating, vertical and custom title bars retain native ownership and are
  not overlaid.
- The mixer keeps its richer audio-specific overflow with the same collapse
  action first, without introducing a second three-dot icon. A generic
  fallback appears if the specialized mixer icon is unavailable.
- Narrow headers elide only the visible title text to leave the menu a click
  target, preserving exact dock name in OBS Docks menu, source widget
  identity, native float/close buttons and reversible title restoration.
- README has new optimized screenshots; Panels UI explains the title menu.

Verification:
- New tests first failed for missing generic menus and action. Updated tests
  cover dock discovery, menu position, action state/trigger/restore, native
  source widget identity, floating/tabbed/custom-title fallback, long-title
  elision and single mixer overflow.
- Clean compiled module; full serial Qt CTest 22/22 pass; diff check clean.
  One earlier parallel run encountered an intermittent existing OBS routing
  teardown segfault, but the focused test and serial suite passed. It was not
  fixed or claimed resolved.
- Real isolated OBS 32.2.0 under Xvfb: Scenes, Sources, Audio Mixer,
  Scene Transitions and Controls displayed menus. Clicked Scenes Collapse,
  observed native contents hidden and Sources move up, then clicked Expand
  and observed exact native contents return. Clicked mixer title overflow;
  Collapse appeared first, followed by existing pan/sync, hidden, orientation,
  advanced-audio and per-channel options. Native AudioBus bottom switch intact.
- Installed local build with SHA256
  ea8daeb442a234924820670b63c0e14b622b58968205ce7e3103df6dd3430adb
  bytewise identical to installed module. Previous installed module backed up
  at ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-global-overflow.370d4baf126d.so.
  User OBS was not running; config hashes before and after installation matched.
  No external push, repo release or user scene/audio/routing modification.

Open: custom/floating title bars deliberately retain their own controls.
Verification was in isolated OBS, not a subsequent personal-profile session.

## Slice 41 — selectable middle ellipsis for long dock titles (2026-11-22)

User asked for an option to place an ellipsis in the middle instead of
at the end when a dock name is too long. Continues Slice 40.

Implementation:
- Every generic dock three-dot menu and the specialized Audio Mixer menu
  now include a Long titles submenu with mutually exclusive Ellipsis at end
  and Ellipsis in middle actions. Collapse/Expand remains the first action.
- Reuses Qt QFontMetrics::elidedText with ElideRight or ElideMiddle,
  preserving native full names in OBS Docks menu, click targets and teardown.
- One shared preference is stored in
  ~/.config/obs-studio/plugin_config/obs-ui-augment/dock-title-settings.ini
  via QSettings atomic sync. Default remains End when not configured.
  No source routing or existing user configuration is changed.

Verification:
- TDD red first showed missing settings-aware constructor and menu API.
  Green assertions cover both title modes, live update, checked menu actions,
  full native Docks menu name, persistence, reload, and restored titles.
- CMake build and serial 22/22 CTests passed; git diff --check clean.
- Isolated real OBS 32.2.0 / Colorway / Xvfb: open Scene Transitions
  three-dot overflow, Long titles submenu, click Ellipsis in middle,
  observe displayed Scene…itions, persist [title] ellipsis=middle to
  isolated config. Restart isolated OBS with the same config confirmed
  middle choice persisted; Audio Mixer retained its original options and
  exposed the shared Long titles submenu.
- README and optimized screenshots updated in
  docs/screenshots/readme/dock-title-ellipsis-choice.webp and
  docs/screenshots/readme/dock-title-middle.webp.
- Installed matching plugin binary SHA256
  bbeb95d87a3f90637c48edbf1dcc9f5ad33a822ef2389ea200b224dd28f718f0
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so,
  previous version backed up at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-middle-ellipsis.ea8daeb442a2.so.
  User OBS PID 147889 stayed running and was not restarted. Four tracked
  user configuration paths had identical before/after hashes.

Boundary: personal OBS session must restart to load the new build, and
the user's preference remains unchanged until they select Middle.
No GitHub push, release or external publication.

## Slice 42 — app-wide middle ellipsis, beyond dock titles (2026-11-22)

User clarified middle ellipsis should apply across OBS, not only dock
headers. The previously installed dock-title preference is reused as one
shared app-wide setting, available through Long text · Entire app on all
native dock three-dot menus.

Implementation:
- New AppTextElision attaches to the Qt application from the existing
  DockTitleIcons lifecycle. Middle mode applies native textElideMode to item
  views and QTabBar; handles plain one-line non-interactive QLabels with
  reversible visible text and original-name tooltip; paints narrow
  QComboBox selection, QPushButton, QCheckBox and QRadioButton display text
  via existing native QStyle without altering actual values or action state.
- Newly created widgets/dialogs are handled. Custom Aftermarket AudioHub
  source names and submenu value captions obey the same mode.
- End remains the existing default and restores each native widget's
  former elision behavior (which may clip instead of draw a literal ending
  ellipsis), original source names and label text. Underlying OBS source
  IDs, model values, editable fields, source buttons, menu identities and
  rich/word-wrapped content are not changed. Fully custom-drawn third-party
  widgets are outside universal support; no claim of overriding them.
- The persisted preference stays in dock-title-settings.ini for backwards
  compatibility; no new user config format. Added app-text-elision-test
  and current README screenshots from the real OBS Sources list.

Verification:
- Test-first: initial missing app adapter fails, then full serial CTest
  23/23 passes; zero compiler warnings and clean whitespace check.
  Regression covers default native state, middle mode and return to end,
  model/name immutability, dynamic widgets, external label changes,
  Qt-list and tab modes, native-styled button/combo/checkbox painted pixels,
  original values and checked state, restoration and repaint lifecycle.
- Real OBS 32.2.0 with isolated Xvfb profiles loaded two synthetic sources,
  including exact full name:
  Extremely Long Microphone Input With A Distinct Ending
  In Middle mode OBS Sources list visibly paints the start and ending:
  Extremely Long M… Distinct Ending
  OBS logs confirm the complete original source name is still used. Audio
  Mixer labels, dock titles and non-interactive labels follow the mode.
  End mode preserves original native display behavior with same scene.
- Important smoke correction: old disposable profile had a stale external
  scene collection path AND an older local copied plugin binary. Corrected
  both under the ignored smoke directories, then verified loaded module
  path through /proc/PID/maps and binary hashes. A final clean binary
  was run and visually confirmed, after removing all diagnostics.
- Installed SHA256 of the clean plugin:
  d498c36337779e65038b218b0c195bbe948cb3231771d36606353eac04182700
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Rollback of previously installed clean version:
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-global-text-ellipsis.bbeb95d87a3f.so
  Live OBS PID 147889 was NOT restarted. The existing user rules,
  action strip, front-audio and title setting hashes were identical pre/post.
  Only ignored disposable scene copies were renamed, not real user scenes.

Outstanding: next personal OBS restart loads the new binary. Third-party
custom drawing requires per-control adapters; editable text intentionally
is not modified. No GitHub push or publishing requested.

## Slice 43 — Aftermarket category inside native OBS Settings (2026-11-22)

User corrected the global ellipsis preference location and reminded us of the
previous plan to move Aftermarket settings into an OWN entry in the main OBS
Settings sidebar. Prior assistant research had deferred this for native
dialog lifecycle and Apply/Cancel behavior. The earlier interim attempt at
placing a global Appearance tab inside the Inspector was not committed and
was replaced with this native integration.

Implementation:
- Added ObsSettingsPage, a reversible Qt adapter for OBS 32.2.0's actual
  OBSBasicSettings QDialog. It discovers the native listWidget/settingsPages
  pair on dialog creation and inserts ONE Aftermarket sidebar item and
  corresponding native QStackedWidget page at the end (after Advanced),
  preserving all nine original category indices, layout, labels and widgets.
  Native OBS remains the owner of the dialog.
- The Aftermarket page uses normal OBS/Colorway palette and groups the
  regular UI preferences as Appearance, Menus, Mixer, Panels, Action strip.
  Includes app-wide Long text End/Middle; theme transform styling, Scenes
  and Sources stripe toggles; compact/hamburger menus, motion and submenu
  selected choices; AudioBus view, gain knobs and integrated meters, gradient
  tracks/reactive/native meters, loop and color values, source selection;
  property-group collapse; Action strip enabled and its specialized editor.
- Dock header three-dot menus now hold dock-local Collapse/Expand (plus the
  existing native mixer-specific actions). The global ellipsis selector is
  removed from ALL dock menus. Existing persisted selection in
  dock-title-settings.ini migrates without changing the file format.
- Aftermarket edits stage locally until the EXISTING OBS Settings Apply/OK
  button is clicked. Plugin-owned changes enable the otherwise disabled
  native Apply button; Cancel discards unapplied Aftermarket edits. Saves
  use existing controller/runtime setters with rollback on an operation
  failure and preserve concurrent unrelated settings. The page is removed
  on plugin emergency teardown without deleting any OBS settings page.
  The native inspector remains for specialized per-target rule editing and
  detailed color/source tools, not as the primary global settings location.

Verification:
- RED test first showed absent native settings integration. Then tested fake
  OBSBasicSettings QWidget tree with native nine-row list/stack; Aftermarket
  insertion once; categories/indices/ownership unchanged; five-tab page;
  Apply initially disabled, plugin edit enables it; Cancel discards; Apply
  persists app-wide mode and mixer/stripes/property/knobs/action-strip;
  later reopen reads saved state; native OK also saves; invalid gradients
  do not modify gradient config; adapter destruction removes only plugin
  page and restores a valid native selection.
- Full clean CMake build with no compiler warnings and serial CTest
  24/24 passed; diff check clean.
- Real isolated OBS Studio 32.2.0 / Colorway with copied sandbox-only
  plugin/config: OBS Settings opens showing ninth native categories plus
  Aftermarket as last sidebar entry. Clicking it shows native-themed
  Appearance / Menus / Mixer / Panels / Action strip content. In live XTest,
  changing the global middle setting to End enabled native Apply without
  immediately changing source names. Clicking Cancel closed Settings and
  retained middle-truncated Sources text. Reopening, choosing End, and
  clicking native Apply persisted ellipsis=end in disposable QSettings and
  visibly restored normal OBS Sources label presentation. Main OBS Settings
  buttons, native pages, and OBS scene/source identities remain.
- Updated README documents OBS Settings -> Aftermarket, native Apply/OK/
  Cancel, screenshot of its Appearance and Mixer pages, and the continuing
  specialized role of the Inspector. Earlier misleading docs pointing to
  the global option in every dock three-dot menu were corrected.
- Installed clean module SHA256
  2516150b0d904b3b00985c4b598e2e6a6d6ea2127c7b96c8cfe103b9659e17b9
  in ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so,
  matching the compiled binary byte-for-byte. Previous installed build
  preserved at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-native-settings.d498c3633777.so.
  User OBS PID 147889 was not restarted; four tracked user config files had
  identical checksums before/after installation. All configuration changes
  made in the runtime tests used an ignored disposable OBS profile.

Boundaries:
- The plugin does not replace OBS itself or its native Settings class;
  integration is guarded for the OBS 32.x Qt widget names/structure checked
  at runtime. Unrecognized future layouts are not altered.
- Specialized menu-rule authoring and extra advanced color pickers remain
  in Tools -> UI Augment Inspector until those editors are deliberately
  moved; the native sidebar contains normal global preferences now.
- Actual running user OBS must restart to load the newly installed module.
  No changes to scenes/audio routing or GitHub publication requested.

## Slice 44 — Pop out and Close moved into dock three-dot menus (2026-11-22)

Requested outcome: Every ordinary native-titled dock overflow should contain
the native window controls, so floating/close buttons do not crowd the header;
the specialized Audio Mixer overflow must remain a single menu.

Implementation:
- DockTitleIcons now hides the separate Qt title-bar Pop out and Close
  buttons only while a docked, horizontal native title has a usable
  Aftermarket three-dot overflow. This is done by temporarily masking
  QDockWidgetFloatable and QDockWidgetClosable, leaving DockWidgetMovable and
  all source/control widgets alone. This also recovers horizontal title width.
- The ordinary menu now contains Collapse/Expand, a separator, Pop out
  (native setFloating(true)) and Close (native close()). Both have native
  Qt-style icons. The specialized Audio Mixer menu uses the same shared
  menu-actions helper before its existing knobs/hidden-channel/advanced
  options, without creating a second ellipsis button.
- Each action is deferred until QMenu dispatch returns and uses QPointer
  guards to avoid a dangling QObject if a dock is destroyed mid-action.
  Before floating/closing, the dock's original Qt feature flags are restored.
  Original Qt controls are also restored for a floating, hidden, custom-
  titled, vertical-titled, or overflow-inaccessible dock, and on adapter
  destruction/emergency restore. Unrelated feature changes (e.g. vertical
  title/movable) are carried through; a real OBS Lock Docks (all flags off)
  is honored instead of re-enabling the window buttons. Dynamic plugin docks
  are discovered by the existing manager.
- Native OBS protected core docks intentionally omit DockWidgetClosable in
  OBSBasic_Docks.cpp; the Close action is disabled for them. Their normal
  Pop out remains available when OBS allows it. Explicitly restricted
  third-party dock features are also honored: this is not a new capability
  granting unrestricted closing or floating.
- README updated with two short optimized WebP captures from real OBS 32.2
  / Colorway: dock-window-actions.webp and mixer-dock-window-actions.webp.
  Previous statements claiming the separate native buttons were untouched
  were corrected.

Verification:
- TDD red before implementation: Qt dock-title regression expected native
  float/close flags suppressed and a working overflow and failed at the
  unsuppressed floatability assertion.
- Green test verifies five native docks, action labels/enablement, Collapse/
  Expand surviving, generic third-party dock creation, a real Pop out
  action on a dock followed by native floating features restored, redock
  re-suppression, custom title restoration, Qt Lock Docks disabling actions,
  explicitly restricted dock, real Close action on a closable plugin dock,
  native Qt features and exact full title/Docks-action names on teardown.
- Real isolated OBS 32.2 / Colorway runtime loaded matching new module
  from a disposable XDG profile; screenshots verified the Scenes and
  Audio Mixer menus show Pop out and Close with original controls removed
  from dock headers. XTest clicked the Scenes Pop out entry and OBS stayed
  alive; the dock detached and its native floating title handling returned.
- Full CMake build clean, serial CTest **24/24 passed**, no compiler warnings,
  git diff --check clean.
- Installed exact built .so with SHA256
  554851c6e94a41a87e521e866283f3a0588d77387cb5b8b4454d6a775208d679
  to ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Previous known-good binary preserved at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-dock-overflow.2516150b0d90.so.
  OBS was not running at install time; no user process restarted or modified.
  Four monitored user config files had identical pre/post checksums.

Boundaries: Floating OS/WM window titlebars and third-party custom title
bars retain their native controls; the plugin does not replace those title
bars. Protected OBS built-in docks remain nonclosable by design. No push
or external publication requested.

## Slice 45 — replace AudioBus activity dots with live node outlines (2026-11-22)

User clarified the AudioBus visual treatment: do not introduce another status
indicator. REMOVE the current separate activity dots entirely and use the
OUTLINE of each existing source card and output circle to show active audio.

Implementation:
- Removed the paintActivityDot renderer and both source/output dot draw calls.
  The activityForSource / outputActivity logic, including real recent measured
  signal, OBS enabled/muted state, Monitor Only handling and source aggregation,
  remains unchanged and is now the single authority for node borders.
- Measured Active source card borders and numbered/headphones output circle
  borders draw a slightly stronger line using the active QPalette Highlight.
  Silent, muted, disabled, unknown and stale signal states use a neutral outline
  rather than falsely claiming activity. Tooltip descriptions retain more
  granular signal state.
- Selected source / hovered source / connected destination remain separate:
  subtle selected-route background fills, hover-neutral emphasis and cables,
  WITHOUT granting the active accent border merely because they are selected
  or connected. Output node hit geometry, transparent corners, native source
  routing and monitor operations are not changed.
- Reclaimed former in-card dot space for a wider source label. The cable port
  and registered-source-type icons remain intact. No additional widgets,
  overlays, polling paths or settings were introduced.
- Updated the README and replaced the outdated right-side-dot screenshot with
  a 20 KB cropped WebP from a REAL OBS routing view:
  docs/screenshots/readme/audiohub-activity-outlines.webp. Old screenshot
  docs/screenshots/readme/mixer-patchbay.webp was removed from HEAD but remains
  recoverable in Git history.

Verification:
- TDD: new visual regression initially failed on sourceOutlinePixels>0 with
  the old dot renderer. Green regression requires source card and output circle
  border changes from measured input, zero changes at both old dot locations,
  source-type icon pixels untouched, and new outline recoloring when the
  live Qt Highlight palette changes. Muting a previously active source removes
  both the source's and eligible output's highlighted outlines without
  changing former dot regions. Previous signal aggregation, transparent
  circle hit testing and native drag/toggle tests remain.
- Clean build without warnings, serial full CTest **24/24 passed**,
  git diff --check passed.
- Verified in real isolated OBS Studio 32.2.0 under Xvfb: copied profile
  with actual looping tone and silence media sources. An old safe-mode.flag
  initially disabled the UI; moved it out of ONLY the disposable profile,
  never the real user's config. Confirmed the exact new module loaded from
  the disposable XDG plugin path in /proc/PID/maps and that both OBS audio
  envelope sources attached. Clicked the native AudioBus bottom switch and
  inspected graph screenshot: Tone test has active highlighted card outline,
  its eligible output circles have accent outlines, Silent test remains
  neutral, and neither card nor circle has a separate activity dot.
- Installed final compiled module SHA256
  0bc93f608aa2fe642f943f690d014169df0060ed59b4e72ef48ab124aea2dcce
  to ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so
  and confirmed installed bytes exactly match build. Prior installed build
  preserved at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-audiobus-outlines.554851c6e94a.so.
  User OBS PID 270746 was left running and was not restarted. All four
  tracked real user config file hashes were identical before and after
  installation; OBS scene/audio/routing values untouched.

Boundary: The user OBS instance must restart before it can load the newly
installed module. Activity still means measured signal in OBS, not proof of
audible physical playback. No GitHub push/release requested.

## Slice 46 — theme-aware AudioBus title-menu routing controls (2026-11-22)

User requested explicit Qt/OBS theme awareness and relocation of Show all
wires and Refresh from the AudioBus graph header into the existing three-dot
dock title menu.

Implementation:
- Removed Show all wires QCheckBox and literal Unicode refresh QToolButton
  from AudioPatchbay header, leaving the native Audio routes QLabel only.
- Added AudioPatchbay::populateRoutingMenu to append native, checkable
  Show all wires and Refresh audio routes QActions to caller-owned QMenu.
  The check is rebuilt from actual graph state each time; toggling changes
  only cable visibility, never OBS audio tracks or monitoring. Refresh calls
  the authoritative OBS route query and leaves periodic refresh intact.
  QAction callbacks guard the AudioPatchbay with QPointer for teardown.
- Added the two actions to the existing Audio Mixer dock title's only
  three-dot menu, only when its AudioBus routing page is visible.
  They are absent on the original native mixer page. Existing dock actions
  and advanced mixer controls remain intact. No second menu button, footer,
  or graph toolbar was added.
- The menu and check/hover/focus use native Qt widgets and palette. The
  Refresh icon uses the current Qt QStyle BrowserReload shape tinted to the
  current OBS QMenu Text palette role at menu creation; a system desktop icon
  initially rendered blue under dark OBS and was corrected after visual
  review. Existing graph sources, cables and active outlines are already
  Qt-palette-aware.
- Updated README and added a cropped real-OBS WebP screenshot at
  docs/screenshots/readme/audiobus-title-routing-options.webp.

Verification:
- TDD RED: test initially failed for missing menu actions and state setter.
  GREEN: checks no redundant header controls, expected QAction labels,
  object names and checkability; wire toggle and menu rebuild state; actual
  cable counts, authoritative refresh queries, unchanged OBS track and
  monitor writers, and existing monitor context-menu behavior.
- Theme RED: system icon produced identical pixmaps for different menu
  palettes. GREEN: native Qt icon tint differs with light/dark text palette,
  and light/dark QPalette changes source card/output outlines, fills and
  captions without fixed colors.
- Full CMake build without warnings, 24/24 serial CTests passed, diff
  whitespace check passed.
- Real isolated OBS 32.2.0 loaded correct sandbox .so and scene collection
  with actual tone and silent media sources. Visual check: AudioBus header
  has no Show all wires or Refresh control; both appear in mixer title menu;
  clicking Show all wires adds cable paths, menu reopens with checkmark;
  Refresh leaves routing face intact; switching to native mixer hides those
  routing-only menu actions. The icon is palette-colored, not system blue.
- Installed exact build SHA256
  a4e50200700422be1bfe26517efbca83472ebeb60fb2fa76d92a49c831cac977
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so,
  verified by bytewise comparison. Previous installed module backed up at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-audiobus-title-options.0bc93f608aa2.so.
  User OBS PID 294527 remained running and was not restarted, and all four
  tracked real user settings file hashes were unchanged before and after.

Boundary: the routing actions are in the title menu on docked AudioBus.
On floating/custom-title docks the pre-existing native title accessory can
be absent; this slice did not add another title bar. No GitHub push,
external release, or change to user scenes/audio routing requested.

## Slice 47 — audio channel widths independent of source names (2026-11-22)

User asked that native Audio Mixer channels NOT grow wider merely because an
OBS source has a long name. Keep complete OBS names, existing mixer geometry,
and reversible native widget ownership.

Implementation:
- FrontAudioControls now recognizes native OBS 32.x VolControl widgets
  (also retaining older VolumeControl integrations) and locates only their
  OBSSourceLabel QLabel or older VolumeName QAbstractButton. The label gets
  QSizePolicy::Ignored horizontally, so its text/sizeHint cannot dictate
  channel width; its original vertical policy, font, text, tooltip, source
  identity, events and paint remain OBS-owned.
- A compact 68 Qt-pixel name-independent MINIMUM label allowance prevents
  OBS's centered vertical QLabel layout from collapsing names to zero width
  when size hints are ignored. Labels can use more room in wider layouts;
  no fixed width is applied to the entire channel or faders. Both vertical
  columns and horizontal rows are supported without deriving width from
  each source's name. The existing global middle-ellipsis feature may
  shorten displayed names; full source names remain in OBS.
- Only known native mixer source-name widgets are affected. Each original
  QSizePolicy and minimum width are saved using QPointer-managed entries,
  and are restored during detach/emergency cleanup (without clobbering an
  unrelated vertical policy change). New source labels are discovered on
  ordinary existing refresh/poll, including name changes.
- README now documents this explicitly with a small native OBS screenshot:
  docs/screenshots/readme/mixer-name-independent-width.webp.

Verification:
- TDD RED: actual OBS-like VolControl + OBSSourceLabel QLabel layout
  initially failed because names still had Preferred horizontal size
  policy. Initial naive Ignored policy passed sizeHint but a REAL OBS
  screenshot revealed names became invisible. Added another RED test for
  visible label width and full rollback, then introduced constant minimum
  readable label allowance; both regressions now pass. Older VolumeName
  QToolButton compatibility and native-style horizontal row tests pass.
- Real OBS Studio 32.2.0 side-by-side before/after through separate
  disposable XDG profiles. Both scene copies contained the exact long
  synthetic source name "A Very Long Microphone Name For Testing Channel
  Width Independent Of This Entire Name" and the short "Tone test" source.
  Confirmed both module versions loaded at their sandbox paths in
  /proc/PID/maps and complete long name in OBS startup logs. Before:
  native vertical mixer channel for long source noticeably wider than
  short source. Final after: similarly compact channel widths, names
  visible, long one truncated and short one readable, gain/mute/monitor and
  per-channel pan/sync controls still present. These screenshots were
  taken in isolated OBS; user scene and layout were never changed.
- Final full CMake build clean/no compiler warnings, 24/24 serial CTests
  passed, git diff --check passed.
- Installed final binary SHA256
  5eb75d76004c0d1c7b49fd233b3fc73ea85d25ba687246c82631195ad8d223e2
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so;
  bytewise match verified. Previous installed binary preserved at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-name-independent-channels.a4e502007004.so.
  User OBS PID 294527 was not restarted. All four tracked real user
  configuration file hashes stayed identical before/after.

Boundary: Native OBS needs restart to load the installed library. No
GitHub push or release requested. Fully custom third-party mixer widgets
are outside this size policy adapter.

## Slice 48 — correct needless audio-source label truncation (2026-11-22)

User reported that the previous name-independent mixer sizing pass had
started shortening names even when the mixer had space. Root cause was
the fixed 68-pixel fallback assigned to *all* native OBS label widgets.
Although horizontal sizeHint was ignored correctly, this small minimum
unnecessarily collapsed the vertical source labels despite the native
VolControl's 110-pixel maximum channel width.

Implementation:
- Keep the name-independent QSizePolicy::Ignored horizontal policy, so
  source text length cannot expand the mixer channel.
- For native OBS VolControl with an explicit maximum width, use the
  existing channel width minus its layout margins as the NAME-INDEPENDENT
  readable label minimum (OBS vertical VolControl max 110). Thus the
  label can use the space OBS already allows, with no extra channel growth.
  The small fallback applies only to uncapped third-party/horizontal
  layouts, which retain expansion to available container space.
- Complete OBS source text, native tooltip/paint path, meter and gain,
  pan/sync controls, and the global user-selected ellipsis mode remain
  unchanged. The exact original label policy/minimum are still restored
  on detach and emergency recovery.
- README now clarifies when elision is justified and shows an updated,
  cropped real OBS screenshot at
  docs/screenshots/readme/mixer-name-independent-width.webp.

Verification:
- RED TDD: regression expecting native channel-capacity label width failed
  on the previous 68px implementation. GREEN test verifies both long and
  normal label have name-independent width equal to OBS's 110px cap,
  that the normal Live Latent: TTS label fits when its text measures
  within that cap, full name survives rename, and original minimum and
  sizing policy are restored. Horizontal real-style layout test requires
  substantial readable width (>=120px); prior tests still pass.
- REAL OBS 32.2.0 side-by-side with two disposable profiles using the
  same scene sources: Live Latent: TTS and A Very Long Microphone Name
  For Testing Channel Width Independent Of This Entire Name. Verified
  each OBS process loaded the specific old/new module from its sandbox
  XDG path and full source names in OBS logs. Old screenshot shows
  Live…TTS needlessly shortened; corrected screenshot shows the full
  Live Latent: TTS, with the very long source still appropriately
  truncated. Both channels stay equal-width; gain, monitor, pan/sync,
  and meter controls remain present. No actual user's scenes modified.
- Clean full CMake build, 24/24 serial CTests passed, no compiler
  warnings, git diff --check clean.
- Installed SHA256:
  b668f0bea95d5444f756d29602cc81198e474fa9c84bf5e25b152900e518d580
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so,
  verified bytewise. Previous binary backed up at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-name-label-width-fix.5eb75d76004c.so.
  User OBS PID 294527 remains running and was not restarted; the
  four monitored real user config files had unchanged hashes.

Boundary: a real OBS restart is required to load the installed plugin.
No GitHub push/release or audio scene/routing change requested.

## Slice 49 — restore native Qt text ownership (2026-11-22)

Reason: User reported app-wide nonsensical middle-ellipsis of ordinary labels,
including Filters and Select color, after a sequence of narrower mixer-width
fixes. This reflected architectural drift from Aftermarket's Tampermonkey-
for-OBS purpose into global replacement of native QWidget paint paths.

Current root-cause evidence:
- The installed module's AppTextElision intercepted all QEvent::Paint
  events for QPushButton/QCheckBox/QRadioButton/QComboBox, built incomplete
  QStyle options, subtracted extra pixels from already style-managed content
  rects and substituted shortened strings. Separately, it rewrote QLabel
  text to an elided copy, which also changed QLabel sizeHint and could cause
  Qt layouts to shrink around the abbreviation. Native buttons must retain
  Qt/Colorway painting and layout authority.
- Two isolated real OBS 32.2.0 copies with the *same* scene, window geometry,
  theme and stored middle-ellipsis preference showed independent failures
  in the installed module: Select color appeared as Sele...olor and a third-
  party Free upgrade to Stream Suite button became needlessly abbreviated.
  The corrected module shows both complete. Scene and source names in OBS
  logs and data remained unchanged. Side-by-side evidence is in
  docs/screenshots/readme/native-control-paint-comparison.webp.

Implemented:
- Removed app-wide custom painting of standard Qt pushbuttons, checkboxes,
  radio buttons and combo boxes. Their QStyle/style-sheet rendering,
  native geometry, focus/disabled/checked semantics, original text and
  QSS stay owned by OBS/Colorway and respective plugins.
- Eliminated all QLabel::setText-based elision. Normal QLabel widgets are
  untouched. A deliberately narrow known OBSSourceLabel display adapter
  paints only a *visually overflowing* source name in the selected middle
  mode and exposes the original name as a tooltip. Original QLabel::text,
  sizeHint, source IDs and rename notifications stay intact; tooltip is
  restored only when it was injected by this adapter.
- Retained native QAbstractItemView::textElideMode and QTabBar::elideMode,
  with prior preferences restored when switching off or unloading. The
  stored user preference remains in the existing Aftermarket Settings
  Appearance page and requires no migration. Aftermarket-owned dock title,
  AudioBus, and menu-choice text continues using its own existing bounded
  font-metric calculations.
- Eliminated global whole-widget-tree scans on every resize/layout request
  and child-add event. Newly shown/polished native item views and tabs pick
  up the current mode locally, with a single full scan on explicit mode
  transition. Removed dead public text helper methods.
- README corrected: the preference applies where supported by native Qt
  elide APIs or a specific Aftermarket adapter, not an unconditional right
  to repaint every native or third-party widget. This is a deliberate
  compatibility boundary rather than an attempt to silently emulate Qt.

Verification:
- Focused Qt regression covers native style-sheet Filters button retaining
  both native paint and full text; native buttons, checkboxes and combo boxes
  paint unchanged with the global middle preference; ordinary QLabel text
  and sizeHint unaffected; OBS source-name QLabel elides only in paint, never
  rewrites source text or layout; dynamic source rename, child item views and
  tab bars; settings transition and teardown restore full native behavior.
- Full CMake build clean, serial CTest **24/24 passed**, no compiler warnings,
  and whitespace check clean.
- Final exact module SHA256:
  dd67f6ce0eb65e512472adf317fe0c3dd475cd438a1d01227f94a8d1558b6c58.
  Verified in isolated real OBS with matching sandbox module from proc maps;
  same source names load fully, selected native Color source displays
  "Select color" in full, while third-party "Free upgrade to Stream Suite"
  also remains full. No OBS log errors attributed to Aftermarket.
- Installed byte-exact module at
  ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Saved prior installed module at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-native-text-restoration.b668f0bea95d.so.
  User OBS PID 294527 left running; tracked user config files
  rules.json, action-strip.json, front-audio-controls.json, and
  dock-title-settings.ini had identical hashes before/after install.

Boundaries and remaining architecture audit:
- This slice closes a verified shared rendering regression, not a blanket
  certification of all Aftermarket features. Other architecture-sensitive
  interceptors (gradient paint hooks, dock-title feature changes, mixer
  label sizing, panel layout changes, and menu QSS) remain candidates for
  targeted review against real OBS behavior; no unverified broad refactor.
- Qt/third-party custom-painted widgets do not automatically support
  forced middle ellipsis. The default native behavior is intentional;
  future adapters require concrete user value and real runtime tests.
- User OBS needs restart to load installed library. No GitHub push or release
  requested. No scene/audio data was touched.

## Slice 50 — independent selected AudioBus outline; all wires on by default (2026-11-23)

User requested two focused changes to the existing AudioBus canvas:
1. Selected audio source nodes must retain a distinct outline, not just a
   filled background, even when their measured audio is not active.
2. Show all wires must be enabled when the routing view first opens.

Implementation:
- The selected source uses the current Qt QPalette::Link color for its
  outer border, with a palette-derived fallback if it matches the signal
  Highlight or node background. The existing measured Active signal keeps
  the Qt QPalette::Highlight color. A selected+active node has both:
  selection outer perimeter, live signal inner line. Unselected active
  sources retain their signal-color perimeter, and inactive unselected
  nodes remain subdued. No dots or hardcoded colors were introduced.
  The selected route's subtle fill, source icon, ports and cables remain.
- Canvas::showAll defaults true; each new AudioBus view immediately draws
  all connected source/target wires, and the native dock menu Show all
  wires QAction opens checked. The option remains reversible at runtime.
  Hovering a destination no longer filters unrelated wires while all-wires
  is enabled; focused-only hover behavior remains in the off mode.
  No change to OBS routing, monitoring, or source names/settings.

Verification:
- TDD red: initial default-check assertion failed with pre-change code;
  green test covers default all-wire cable count (10 for fixture), native
  checked menu state, hover keeping all 10 cables, selection changes
  retaining all wires, toggling to focused-only (7 cables), and full
  rebuild of a fresh QAction reflecting the current graph state.
- Focused Qt image tests verify selection outer pixels change on selection,
  unselected nodes stay subdued, Link palette color changes selected
  border only, live audio adds an inner activity stroke without losing
  selected border, and prior mute/live/theme/output signal tests still pass.
- Actual OBS Studio 32.2.0 isolated Xvfb profile using two media sources
  (Tone test and Silent test), with exact compiled module loaded at its
  sandbox path. Screenshots show selected Silent test node outlined and
  wires from both sources visible at startup, while the Audio Mixer dock
  title menu opens with Show all wires checked. README routing screenshots
  updated to this new behavior.
- Full build with no warnings, serial CTest **24/24 passed**,
  git diff --check clean.
- Installed byte-exact module SHA256
  ea5984764cc9fb8d6ca27fd1095d5e02df76fff9e9628542929a699e02ba0857
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Backed up previous binary to
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-selected-outline-all-wires.dd67f6ce0eb6.so.
  Real user OBS PID 405867 was not restarted; four tracked user settings
  files were byte-identical before and after installation.

Boundary: existing live OBS needs restarting for the installed module to
load. No remote push, release, source routing or audio data mutation.

## Slice 51 — one clearly distinct outline color for selection vs audio activity (2026-11-23)

Correction: the preceding AudioBus slice drew both an outer selected border
and an inner active ring on selected+active sources, and the Colorway theme
resolved Link and Highlight to visually similar shades of blue. The user
wanted only two clearly different outline colors, not layered decorations.

Implementation:
- Each source still has exactly ONE outline. Selected wins and uses
  QPalette::Link (blue in current OBS theme). Unselected sources with a
  measured active signal use an activity color; outputs with live routed
  audio use that same color. Inactive unselected sources/outputs stay neutral.
- A small palette-aware helper keeps the normal QPalette::Highlight color
  when its hue is already sufficiently distinct from Link. When Link and
  Highlight resolve to nearly the same hue, derive a contrasting active
  hue from Link by rotating the hue; retain theme-derived brightness/alpha.
  In Colorway this is blue for selection and green for audio activity.
- Removed the separate inner active ring on selected+active sources.
  The selected border remains stable regardless of measured signal. Did
  not alter cable visibility, target routes, status readings, or Show all
  wires default (still on). No separate dots or new UI elements.

Verification:
- TDD RED: updated test rejects the inner stroke, requires stable selected
  source pixels after its signal becomes active, verifies mute does not
  alter selected border, verifies unselected activity changes its outline,
  and requires a minimum hue separation of 65 degrees, not just a change
  of luminance between two blues. GREEN after one-outline change and hue
  contrast fallback. Existing Qt palette and routing tests still pass.
- Real isolated OBS 32.2.0 with Tone test and Silent test, compiled module
  confirmed loaded at the sandbox path. Selected Silent test outline RGB
  (68,100,199) is blue; active Tone test outline RGB (68,158,39)
  is green; active Track 1 circle uses green. Screenshot refreshed at
  docs/screenshots/readme/audiohub-activity-outlines.webp.
- Full build clean (no warnings), 24/24 serial CTests pass,
  git diff --check clean. Byte-identical module installed with SHA256
  84271c6c7191d06def6b29ce5750f0b458a9b2b9ab258992acb794b6c909d92a
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Old installed .so backed up to
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-single-colored-outlines.ea5984764cc9.so.
  User OBS was not running at install. Four tracked user config files
  unchanged. No GitHub push or unrelated changes.

## Slice 52 — native dock and AudioBus UI animations (2026-11-23)

Goal: Animate Aftermarket's existing UI collapse/expand and view transitions
without replacing OBS native widgets, repurposing OBS scene transitions,
adding global paint hacks, or changing saved scenes/audio routing.

Implementation:
- Reused the existing global motion preference in rules.json, exposed at
  OBS Settings → Aftermarket → Menus and Tools → UI Augment Inspector. Wired
  it as a live callback into Panels and DockFaces, so Motion off retains the
  immediate behavior and Motion on applies to future UI interactions.
- Panels: 165-ms InOutCubic height animation for the existing native
  QDockWidget collapse/expand path. The original content and dock remain
  OBS-owned. To prevent QMainWindow from snapping directly to the endpoint
  when content is hidden, pin both minimumHeight and maximumHeight to the
  intermediate animated height and request native resizeDocks each frame.
  Track and restore original min/max heights and width on normal expand,
  interrupted expansion, dock closure, emergency restore and plugin cleanup.
  Existing restrictions for floating/tabbed docks remain. Use Qt animation
  style hint SH_Widget_Animate and the global Motion preference.
- DockFaces: 155-ms OutCubic crossfade between real Audio Mixer and
  AudioBus pages, using a temporary click-through, nonfocusable QLabel
  screenshot of the outgoing page over the newly selected true widget.
  Neither OBS mixer nor routing widgets receive an opacity effect or are
  cloned/replaced. Overlay self-deletes at finish and is cleared on rapid
  reversal or detach. Motion off skips overlay and switches immediately.
- Updated the native global Motion checkbox caption, Inspector tooltip and
  status messages; README now distinguishes UI animations from actual
  OBS streaming/video scene-transition effects and includes a genuine OBS
  3-frame collapse + 3-frame AudioBus crossfade image at
  docs/screenshots/readme/ui-motion-docks-and-audiobus.webp.

Verification:
- TDD RED: focused tests initially failed on constructors missing Motion
  callback; introduced implementation, then found a further RED on actual
  dock geometry (animated maximumHeight while Qt still snapped on hide).
  Fixed by pinning min/max to animated height, confirmed native intermediate
  geometry in Qt test and real OBS screenshots. A RED emergency-restore test
  caught pending expand bounds remaining constrained; repaired with saved
  original constraints and cleanup. Regression now exercises reverse
  mid-motion, reduced Motion mode, exact restored bounds, and native dock
  deletion while a collapse animation is in flight.
- DockFaces focused regression verifies an overlay and QPropertyAnimation
  only when Motion is enabled, unchanged real OBS QWidget effects and
  identities, click-through behavior, rapid reversal cancellation, cleanup
  after 250 ms, and direct switching with Motion off.
- Real isolated OBS Studio 32.2.0 in Xvfb with sandbox-only Motion enabled,
  exact module loaded at the sandbox profile plugin path, actual media
  sources and routing page. Captured fade-start/mid/end showing native
  mixer giving way to AudioBus; captured pinned-0/20/50/90 frame series
  showing Scenes native dock gradually shrinking and neighboring docks
  naturally repositioning rather than snapping. No relevant OBS log errors.
  Screenshot montage uses these captured frames, not simulated UI.
- Full CMake build with no compiler warnings; 24/24 serial CTests passed,
  git diff --check clean. Compiled SHA256
  8b4d920fb2d0da3ed431afee433b4a34cf3a865d810fa31bafe6a783352dc136.
- Installed binary byte-for-byte to
  ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so,
  with rollback of prior installed module at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-ui-motion.84271c6c7191.so.
  User OBS was not running during installation; all four monitored user
  config file hashes stayed identical before/after.

Boundary: no animations of actual OBS scene transitions, stream actions,
or arbitrary third-party widgets; this slice covers native-supported dock
collapse/expand and the current two-page Audio Mixer/AudioBus switch.
No GitHub push or release requested. Motion is already on in user's saved
rules.json, so these UI transitions become visible next OBS launch.

## Slice 53 — compact popup placement follows its opening point (2026-11-23)

User showed an actual OBS Filters Add effect menu with compact columns
opening at the wrong position relative to its trigger. Root cause in the
Qt 6.4.2-specific MenuLayout::present: it resized a menu from the native
narrow/tall dimensions to wide/multi-column dimensions but reused the old
horizontal position, clamping only to the screen edge. A concrete test at
1920x1080 reproduced the bug: a 58-entry menu opened at x=1499 rendered
with its right edge at x=1916, 417 px away from its trigger.

Implementation:
- Capture the opening mouse position during QMenu::aboutToShow (before Qt
  assigns the initial native geometry), then validate its horizontal
  proximity to the original Qt popup geometry on first presentation.
  Never assume an unrelated cursor position from a programmatic popup.
- Reflow large root menus using that stable opening point in BOTH axes.
  Prefer opening right/down when there is space, otherwise flip left/up;
  clamp to the available geometry of the screen containing the opening
  point. Do not recalculate the anchor from later mouse hover events.
- Parent QMenu/QMenuBar submenu popups retain their native Qt positioning.
  Unrelated small menus and Qt versions without the exact private-layout
  ABI continue to use the existing paths. No action text, model data,
  QAction ownership, hover target or item rectangle was changed.
- README compact-menu section now explains opening-point anchoring.

Verification:
- TDD RED on real XCB 1920x1080: for an Add-filter-like popup of 58
  actions, opening at (1499,879) produced a 954x319 menu at (962,560),
  with right edge at 1916 rather than 1499. GREEN after change: the same
  menu is at (546,561), right edge 1499, while every action is present
  and actionAt(actionGeometry.center()) identifies the original QAction.
- Rechecked other corners and bottom placements at XCB 1920x1080, XCB
  3840x2160 and Qt offscreen. In 4K, opening at (3419,1959) now gives
  a four-column menu at (2466,1641) with the right edge at 3419. A
  delayed-reflow test checks a cursor move between opening and presentation.
- Actual OBS 32.2.0 Filters dialog and Add effect list tested via separate
  Xvfb/XDG profile and exact new module loaded from its sandbox path.
  The available Color-source effect popup (two columns in this profile)
  still opens directly at the Add button (222,799); its actions remain
  intact. The full 58-item long menu behavior is covered by actual Qt/XCB
  integration, not claimed as a matching real OBS source-specific list.
- Full CMake build clean with no compiler warnings; 24/24 serial CTests
  pass, and git diff --check clean. Installed new binary SHA256
  9801994460ee4acfd07fa8dcb4cd37379d4cae62a11976981abd236a0aaec7f3
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Previous installed module preserved at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-menu-placement.8b4d920fb2d0.so.
  The user's running OBS PID 24070 and four tracked real user plugin
  configuration files were unchanged by the install.

Boundary: User's OBS must restart to load the new plugin. No external push
or release requested in this turn. The isolated test OBS and Xvfb may be
closed after verification without affecting the user's real OBS process.

## Slice 54 — alternating Scenes/Sources row background strength (2026-11-23)

Requested: adjustable visual strength for the existing alternating
background stripes in the native OBS Scenes and Sources list widgets.

Implementation:
- Added one shared 0–200% Strength slider to OBS Settings → Aftermarket →
  Appearance and the detailed Tools → UI Augment Inspector → Panels →
  Alternating list backgrounds. Live percent readout in both places.
- 100% preserves previous chosen/theme normal and alternate row colors
  exactly, including existing saved configurations without the strength key.
  0% blends alternating into normal background; 200% doubles the native or
  authored RGB difference (channels clamped 0–255). Any intermediate
  strength uses the linear mix. Both lists share the same setting.
- Reuses RowStripes native QPalette Base/AlternateBase approach. Colorway
  SceneTree QSS alternate painting uses a narrowly scoped idle-row override
  when strength differs from default, preserving native selected/hover
  states, source-specific rows, scene grid layout, widget identities and
  other third-party list views. Existing override cleanup on disable
  remains unchanged.
- Persisted in the already-supported listRows JSON object as integer
  strength, strictly validated 0–200 (no strings, fractions or out-of-range
  values). No migration required and no change to old settings unless the
  slider is moved.
- Inspector previews through RowStripes.configure while dragging, saving
  only on slider release/keyboard value change. Native OBS Settings uses
  existing Apply/OK/Cancel staging and undo semantics.
- README documents slider semantics and both entry points; added native
  isolated OBS settings screenshot at
  docs/screenshots/readme/alternating-row-strength.webp.

Verification:
- TDD RED: row-stripes contrast checks at 0/50/200, actual SceneTree
  painted pixels, native-theme QSS without custom colors, Controller
  rejection of invalid values, native settings slider Apply/Cancel and
  Inspector persistence all failed prior to implementation. GREEN after
  changes. Inspector regression additionally proves dragging previews
  RowStripes without saving, then final valueChange persists.
- Actual OBS Studio 32.2.0 sandbox with isolated XDG profile and real
  installed-in-sandbox compiled module shows Aftermarket Appearance
  Strength slider at 180%, dark theme preserved. Changing to 0% in OBS
  Settings did not mutate its sandbox rules.json before Apply; Apply
  saved 0%. Moving to 200% and pressing Cancel kept saved 0%.
  Never modified the user's current real rules.json during this test.
- Full CMake build clean, 24/24 serial CTests pass without warnings,
  git diff --check clean.
- Installed byte-identical module SHA256
  968b659c716fecda921a51fcbf3df763f9765dbc3edcbddbe4b51ffcdb682129
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Previous binary saved at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-row-stripe-strength.9801994460ee.so.
  User OBS PID 24070 remained running; all four tracked real user
  plugin settings files were hash-identical before/after install.
- No push requested. User needs an OBS restart to load the new module.

## Slice 55 — readable Action Strip dock show/hide labels (2026-11-23)

User wanted the text for dock show/hide buttons cleaned up. Current real
action-strip.json holds one button with empty label and target controlsDock,
which ActionStrip previously rendered literally as the technical object ID.

Implementation:
- For dock toggles without an explicit custom label, resolve the native
  QDockWidget via its saved stable objectName, then render its full
  toggleViewAction text. Thus controlsDock displays "Controls", not the
  internal ID. The Docks menu action remains the authority even when
  Aftermarket abbreviates/pads the visible dock header for limited space.
  Explicit user-authored labels remain authoritative; missing dock IDs
  remain diagnosable, and the button remains disabled.
- A checkable dock button has a stable plain title regardless of state,
  with dynamic tooltip and accessible name "Hide Controls" when visible
  and "Show Controls" when hidden. The checkmark/pressed state still shows
  visibility; no extra on-button wording or width churn was added.
- Overflow menu entries for dock toggles are also checkable, show the
  same full native name and track enabled/checked state and tooltip. The
  Action Strip editor's dock list and native dock chooser now display
  full names instead of the technical objectName where possible (ID
  remains available in the chooser to disambiguate same-name docks).
  Original docking, content, action targets, hotkeys and saved configuration
  are unchanged.

Verification:
- TDD red with actual user-equivalent empty dock label: initial test failed
  because mixerDock rendered as a raw ID. Green checks native "Audio Mixer"
  label, checked state and Show/Hide tooltips/accessibility, native action
  title remaining full when window title is shortened, dock rename updating
  the text, custom labels overriding, and persistence of blank original
  labels. Narrow bar regression exercises checked/unchecked overflow
  actions including the actual dock visibility toggle and restored state.
- Real OBS 32.2.0 isolated Xvfb profile with a copy of the user's existing
  single controlsDock Action Strip item (no real user config mutations)
  and the exact compiled module loaded from sandbox plugin path. Top-right
  button visibly reads "Controls"; clicked once to hide actual Controls
  panel while button remains, clicked again to restore it. Screenshots
  in build/dock-text-smoke/controls-toggle.png,
  controls-hidden.png and controls-restored.png. No relevant OBS log errors.
- Full CMake build clean; all 24/24 CTests pass, no compiler warnings,
  git diff --check clean. Byte-matched installed module SHA256
  bac4894eec30e32c1eb25898af6b5eb2f27fa2a1f9e6ad691b4d7e8bfb6291cb
  at ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Previous binary backed up to
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-dock-toggle-labels.968b659c716f.so.
  User OBS PID 99268 was not restarted; rules.json, action-strip.json,
  front-audio-controls.json and dock-title-settings.ini hashes are identical
  before and after installation. No GitHub push requested.

Boundary: the actual user's running OBS must restart before displaying the
updated label. No native dock action data or streaming settings are altered.

## Slice 56 — two-sided dock backside flip (2026-11-23)

Requested exploration: whether a card-flip animation between the front and
back sides of an OBS dock would be feasible or excessive. Chosen a restrained
210 ms half-turn in the existing DockFaces two-page Audio Mixer/AudioBus path,
replacing (not layering over) the older 155 ms crossfade.

Implementation:
- Retained existing global Motion toggle, Qt animation style hint, bottom
  AudioBus/Audio Mixer button and actual QStackedWidget page identities.
  Motion disabled still switches immediately with no temporary widget.
- New internal FlipFrame paints two QPixmap snapshots, captured once from
  the outgoing and newly selected real native pages. A 210 ms InOutSine
  QVariantAnimation drives a horizontal pseudo-perspective half-turn:
  outgoing face folds to the center; incoming face unfolds. The temporary
  overlay has no focus or mouse input, uses the current Qt palette for its
  background and is deleted on completion, interruption, or dock detach.
  The actual OBS mixer, AudioBus widgets, routing, and streams are never
  rotated, duplicated, replaced, or assigned a graphics effect.
- README Motion explanation and existing real-OBS screenshot montage updated
  from crossfade to actual captured flip frames. No new preferences or
  unrelated UI changes.

Verification:
- TDD red against prior crossfade: required temporary QWidget named
  aftermarket-view-flip, bounded 175–260 ms QVariantAnimation, real pixel
  changes after an intermediate frame, QStackedWidget identity preserved,
  quick reversal clearing the earlier frame, no native QWidget effects,
  and no overlay when Motion is disabled. Green with flip implementation.
- Real isolated OBS 32.2.0 used native Audio Mixer and actual AudioBus
  audio-route page in the same dock, with the expected module loaded from
  the sandbox plugin path. Before, mid-turn and settled screenshots showed
  visible intermediate geometry; the correct final routing page was active,
  with no relevant OBS errors. Repeated using an isolated build exact binary
  SHA256 aa0ada16bdf7eb89d7ed01cc8b6208e5ecf97e3a2c8f0c3981b7b517acdd2e9e,
  with both frame change and destination-page change verified.
- Concurrent unrelated dock-layout work appeared uncommitted in the shared
  checkout while verifying. Preserved that work entirely and built the flip
  separately from the checked-in HEAD plus only the modified source/test
  files, in an ephemeral temporary directory outside Loam. Clean 90-target
  CMake build without warnings. Original 24 tests each pass under appropriate
  display backend: 23/24 in one XCB suite, and menu-bar-compact passes offscreen;
  panels and obs-routing pass under XCB but fail under unavailable/flaky
  offscreen conditions. Thus no single 24/24 CTest invocation is claimed.
- Existing user's running OBS session and actual plugin configuration were
  not used for development; separate Xvfb/XDG config profiles only.

Boundaries: only docks already managed by DockFaces get the flip (currently
the Audio Mixer/AudioBus backside). No new global widget interception or
animation engine, no forced motion, no remote push requested. Other in-flight
dock-layout files are outside this change.

## Named dock-layout macros — 2026-11-23

- Native Qt DockLayouts implementation with versioned, integrity-checked JSON, stable snapshot IDs, required native dock IDs, QSaveFile persistence, explicit saves only, one-level Undo and missing-dock refusal.
- Tools menu entry and dialog for Save, Switch, Undo, Rename, Update and Delete. Action Strip now has a layout action type, enabled by stable identity, disabled if its saved layout disappears.
- Tests: TDD red/green for absent dock-layout header and for missing Action Strip integration. Qt tests exercise real QMainWindow saves and restores, case-insensitive duplicate names, corrupt file handling, missing docks and Undo. 26/26 full CTest passed. Isolated OBS 32.2 loaded the new plugin and registered the new Tools layout action with marker AFTERMARKET_LAYOUT_TOOLS_20261123.
- Preexisting/concurrent changes to README.md, dock-faces.cpp, dock-faces-test.cpp, and dock animation screenshot are not part of this change. Stage only focused layout feature files. No live OBS config touched, no push.

### Real OBS layout GUI regression (2026-11-23)

- Isolated OBS 32.2.0 Xvfb instance with temporary HOME and temporary native
  probe plugin loaded exact build/obs-ui-augment.so. Tools action found and
  enabled. Native Aftermarket manager opened; Save current created OBS Layout
  E2E; mixerDock was moved to a different QMainWindow dock area; Switch to
  layout restored the original area; Undo returned mixerDock to the moved area.
- Probe log at /tmp/aftermarket-layout-obs-e2e-20261123/obs.log:
  AFTERMARKET_REAL_GUI_TOOLS_ACTION=PASS,
  AFTERMARKET_REAL_GUI_SAVED_NAME=OBS Layout E2E,
  AFTERMARKET_REAL_GUI_SAVE_SWITCH_UNDO=PASS.
  OBS exited 0. No real user OBS config, dock state or profile touched.

### Local installation — 2026-11-23

- Explicit user accepted implementation of JR Dockie-style layout switching
  inside Aftermarket; native module installed to
  ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so
  using existing atomic scripts/install.sh. Verified installed bytes equal
  the passing build: SHA-256
  a30c230b005f9c417cff21f169d5240e05b46e69a881a09ce3d5351f32e9a448.
- Previous installed binary backed up (and compared) at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-dock-layouts.aa0ada16bdf7eb89.so.
- An existing OBS process (PID 166750) remained running throughout the
  atomic module replacement. The new module activates only on the next
  voluntary OBS restart; no user session, scene, profile, rules, dock layout,
  stream, or source state was changed. Normal OBS GUI after restart still
  requires user confirmation. Do not claim it was hot-reloaded.
- Last verification: post-commit build and 26/26 CTests PASS; disposable
  OBS 32.2 end-to-end GUI test saved, switched, undid native mixerDock
  layouts and exited 0. No remote push.

## Slice 58 — embed OBS 32.2 native Add Source picker beside Sources (2026-11-23)

User supplied an image of the new OBS 32.2 native Add Source chooser and
described its 1040x620 standalone window as a UX regression, preferring to
incorporate it into OBS without replacing OBS's source-creation workflow.

Implementation:
- SourcePicker is a tightly scoped QApplication event adapter for only the
  native dialog named OBSBasicSourceSelect with required sourceTypeList,
  existingScrollArea, sourceVisible and addExistingButton children. It acts
  on Qt's pre-show Polish event, BEFORE the dialog enters its exec loop.
  Reparenting after Show was experimentally proven to terminate QDialog::exec
  prematurely, so that unsafe path is deliberately avoided.
- Instead of any new source implementation, reparent the existing QDialog
  as a bounded child QWidget of the main OBS window, beside the native
  sourcesDock. A responsive ~700–820 wide and 460–540 tall overlay preserves
  source-type list, preview cards, recent items, native signals, create/add
  logic and subsequent Properties dialog. Reduces native type-list minimum
  from 460 to 240 and keeps it scrollable; long type labels expose their
  exact full text via tooltips. Preserves OBS palette/theme.
- Adds a compact Add Source title and accessible close button because the
  original desktop titlebar no longer exists. Uses the existing native
  dialog result and modality. When Sources dock is hidden, the main window
  is too small, or the expected native controls are missing, leave OBS's
  original standalone source dialog unchanged.
- Native OBS Settings → Aftermarket → Appearance now exposes "Embed Add
  Source picker in OBS", enabled by default. Apply/OK persists explicitly
  in plugin_config/obs-ui-augment/source-picker.ini; Cancel discards staged
  changes. Opt-out restores the standalone OBS picker on its next opening.
  No changes to OBS scene, sources, or configuration files by this setting.
  Emergency recovery disables the hook without forcibly closing any
  active source dialog.
- README added behavior explanation and real isolated OBS screenshot
  docs/screenshots/readme/embedded-source-picker.webp.

Verification:
- TDD RED: missing SourcePicker module; GREEN unit tests for native dialog
  identity and required control contract, child-widget embedding, bounded
  geometry, in-window source dock anchoring, modal exec accept, accessible
  close title, scroll height and full-name tooltips, disabled fallback,
  incomplete dialog fallback, and QSettings persistence/reversibility.
  Native Aftermarket settings test checks staged Apply/Cancel checkbox state
  and commit via the existing OBS Settings integration.
- Actual OBS Studio 32.2.0 under disposable Xvfb and isolated XDG profile
  with byte-matched test plugin: clicking the true Sources dock plus opened
  the ORIGINAL source selector in the OBS main window with a new compact
  Add Source header; selecting Image and pressing Add a new Image created
  a real native Image source and opened its ordinary OBS Properties dialog.
  Clicking the added close icon dismissed the picker; clicking Sources plus
  reopened it. No relevant OBS log errors. User OBS was never touched.
- Clean isolated source tree was built from checked-in HEAD 0b85cda plus
  only these source-picker changes, to avoid unrelated concurrent QuickFind
  edits in the shared working tree. Complete CMake build with no compiler
  warnings and serial CTest **27/27 passed**.
- Shared checkout contains in-progress unrelated QuickFind work (CMake,
  plugin, Action Strip, new files). Transplanted the source-picker changes
  surgically and staged only their hunks for plugin.cpp/CMakeLists.txt,
  preserving QuickFind work in the unstaged working tree. No push requested.

Boundary: the source-creation Properties editor remains OBS's existing
dialog; this slice embeds the initial Add Source picker only. No OBS fork,
no reimplementation of scene/undo/source creation, no global dialog changes.


## Nutty-inspired Quick Find — 2026-11-23

- User asked for the next Nutty video improvement after successful JR Dockie
  layout macros. Built a lightweight native Aftermarket Quick Find palette
  rather than a permanent QAU-style search dock: source/scene/filter names,
  type names, common path/URL settings, parent-scene context and ranked
  multi-token searches. Native actions open Properties or the owning source's
  Filters; explicit Go to scene uses preview scene in Studio Mode.
- Native Tools menu entry, Action Strip search item and configurable OBS
  frontend hotkey with scene-collection save/load callback; no default
  shortcut, no new global keyboard interception.
- Runtime lookup by source/filter UUID; stale sources and removed filters
  fail closed. Catalog is built on palette activation and explicit Refresh,
  not continuously on the render thread. 5,000-entry and recursion caps.
- TDD: quick-find-test, quick-find-obs-test and quick-find-strip-test were
  created RED, then passed with real libobs sources and Qt widgets. A
  stale-filter regression failed before the ownership check and then passed.
- Initial disposable OBS harness GUI checks passed but process crashed at
  exit due to the temporary fixture retaining source refs. Fixed fixture
  cleanup and reran in fresh isolated profile: real OBS 32.2.0 GUI Tools
  launch, native source search, URL, filter and parent-scene search, scene
  navigation, native registered hotkey callback and cleanup all passed;
  OBS exited 0. Verified log:
  /tmp/aftermarket-quick-find-hotkey-isolated-20261123/obs.log.
- No changes to user's real OBS scene collection, no new dock, no push.

### Local installation verified — 2026-11-23

- User's Linux Mint Aftermarket user plugin updated via atomic scripts/install.sh;
  installed path ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Installed SHA-256 matches the exact source build exercised by an isolated
  real OBS 32.2 GUI test, including Tools Find Sources, source/URL/filter/scene
  lookup, scene jump, keyboard hotkey routed callback, fixture cleanup and
  normal process exit:
  9343fc50d5e3e8e51a256a356ba831cef1df3843c6217a4347ec7f802e1f4437.
- Preserved previous installed module at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-quick-find.1409118e24fc47b1.so.
  Verified byte-for-byte backup and destination match. OBS was not running
  during the swap. No scene collection, layout, rules, streaming settings or
  saved hotkey bindings changed.
- Original post-feature regression suite passed 30/30. A later post-commit
  rerun exposed an intermittent preexisting Panels test (assertion at
  tests/panels-test.cpp:78, dock height after only 65 ms): three identical
  direct reruns passed twice and failed once. Search-focused tests and real
  OBS GUI passed. No unrelated dock animation changes made in this task.
- src/plugin.cpp carries a separate, unrelated in-progress SourcePicker
  comment difference after the feature commit; left it unstaged intentionally.
  No push performed.

## Cue Deck interoperability direction — 2026-11-23 (design decision; not implemented)

User specifically wants Aftermarket Cue Deck to orchestrate existing OBS plugins while
we develop first-party versions; existing-plugin users must not be forced to migrate
to use cues. Do not embed copies of third-party plugins or make their installation
mandatory. Design cue steps as semantic capabilities (switch layout, animate source,
start named macro, change scene, activate effect), resolving to independent adapters
with feature/capability discovery and user-pinned provider selection where multiple
providers exist. Support stable native OBS frontend hotkeys and documented obs-websocket
vendor requests before considering plugin-specific internal hooks. This machine has
move-transition, advanced-scene-switcher, emulens, hyperShaderFX installed. Exeldro
Move supports OBS filter/hotkey triggering; Advanced Scene Switcher documents
AdvancedSceneSwitcherRunMacro websocket vendor request. JR Dockie can be a
supported optional external-provider target where actually present, not a Linux
dependency. Our native Aftermarket dock-layout backend is already implemented.

Guardrails: missing/unsupported providers must be visible as unavailable rather
than silently substituting an action with different semantics; on-air and
irreversible actions must be explicit; do not modify third-party configuration;
do not confuse an action being dispatched with the action completing. Keep
provider-specific fields isolated, user cue definitions provider-independent,
and native implementations incremental rather than a forced replacement.
No cue engine or adapter implementation is claimed here. Next focused slice:
one vertical capability across native OBS and one verified external plugin,
with runtime discovery and a full real-OBS test; only then generalize.

### Cue Deck direction refinement — 2026-11-23 (supersedes adapter-first priority)

User clarified that well-designed OBS plugins already expose hotkeys and does NOT
want separate third-party adapters unless they make Aftermarket materially
better. Build Cue Deck atop the existing NativeActionBridge stable hotkey
enumeration/triggering, native dock-layout switching, and documented OBS
frontend actions first. Avoid an Exeldro-specific adapter as the first slice.
Add narrow provider-specific integration only when hotkeys cannot express
parameters, query state, signal completion/error, or guarantee intended
on/off semantics. A toggle hotkey alone cannot serve as a deterministic
"enable" or "disable" operation without confirmed state. Preserve plugin
independence, no forced migration, no reimplementation for compatibility's
sake. Next viable engineering slice is a small sequencer of existing OBS
actions/layouts/hotkeys with explicit safety and error semantics. This is
a design correction, NOT implemented code.

## Compact Add Source split — 2026-11-23

User reported the embedded stock picker was cropped and requested a substantially smaller interface, independent Add existing filtering, and single-click creation from source types. Replaced the oversized native thumbnail layout in the embedded presentation with a 380x400 bounded panel, compact native source-type rows (click/Enter creates via OBS's original create button), and a separate existing-source list with native type selection and case-insensitive name search. Existing rows delegate selection to the actual OBS card buttons; the native Add existing button retains the real source identity and addition logic. Recent uses OBS's limited recent list; the type selector exposes all existing sources of the selected type. Shared native visibility checkbox. Native fallback and persisted opt-out retained. No new source creation implementation, or scene/undo data changes.

Verification: focused RED test reproduced missing split, old oversized/fallback geometry and missing direct creation; GREEN verifies 680x440 main window, bounded panel, new/existing separation, hidden native cards, type and name filtering, correct native callback identity, disabled/incomplete fallback, and preference persistence. Real isolated OBS 32.2.0 Xvfb :253 loaded exact module, showed both compact modes (380x400 as a child window), filtered 'col' to Color, enabled native 'Add 1 existing', added Color to Scene, then one click on Image created Image 2 and opened native Properties. Logs and screenshots in build/compact-picker-v2-smoke. Full build succeeded without warnings; CTest 29/30, known offscreen panels geometry assertion line 78 failed, same panels test passed under XCB on :253. All 30 tests therefore passed across compatible environments, not one combined CTest run. README screenshot replaced with real compact views. Preserve unrelated plugin.cpp comment edit. No push.

## Compact menu monitor anchoring — 2026-11-23

Reported long compact context menus appearing on another monitor. Actual desktop: HDMI-0 3840x2160+0+0, primary DP-2 1920x1080+905+2160. Isolated dummy Xorg :255 reproduced these exact monitor rectangles without touching user desktop. Regression before fix: root popup requested at (1005,2260) on lower monitor, pointer at (1005,1079) on upper, compact geometry moved to (1005,1079) 522x298 because old anchor validation compared x only. Resolve monitor once from native global popup geometry or native causedPopup triggering action rectangle; reject cross-monitor pointer anchor, don't trust lagging QWindow::screen. Submenus position against parent row using final column size; menu-bar popups similarly anchor to action. Clamp to one monitor's available geometry; existing column sizing/hit testing and single-column source context behavior preserved. Multi-head regression verifies both screens, stale cross-monitor cursor, actual Qt keyboard submenu, parent side placement, every action rect/hit target. RED reproduced exact jump; GREEN both heads. Full build clean; 29/30 offscreen CTest, preexisting panels geometry failure passed single-head Xvfb :256 XCB (dual-head panels test has separate frame intersection assumption, not addressed). Real OBS 32.2.0 isolated dual-head :255, exact module loaded: Filters Color right-click Add submenu on upper (466x238 at 443,450), moved dialog to lower monitor (1050,2210), reopened actual context Add submenu (466x238 at 1293,2583); both entirely on triggering monitor, aligned beside parent, screenshots in build/menu-dual-monitor-smoke. No relevant log errors. Preserve unrelated plugin.cpp comment edit. No push.

## README organization and fresh captures — 2026-11-23

Reorganized the original 957-line README into a 201-line visual product overview by feature family, with detailed instructions and configuration examples preserved in docs/user-guide.md (linked table of contents). Added Dock Layouts and Quick Find discoverability, consistent AudioBus naming, corrected old collapse-motion, audio-activity, menu-monitor and action-strip descriptions. New real OBS 32.2.0 captures from isolated Xvfb :257 with the same installed Aftermarket binary: Quick Find filtered source/filter, actual saved dock layout, split Add Source picker with name filter, measured activity in AudioBus. Added aligned nested Filters Add capture from preceding exact dual-monitor runtime check. Five focused optimized WebP assets total ~69 KiB; visually reviewed. Relative file links, image decode, heading anchors, balanced code fences, git diff --check verified. No production code or user settings changed; unrelated plugin.cpp change excluded. No push.

### Cue Deck provider choice clarified — 2026-11-23 (supersedes hotkeys-only reading)

User explicitly corrected the overcorrection: existing good OBS plugins, especially
Exeldro's Move, should be USED directly and deeply when they offer a good solution.
Hotkey registration is a useful generic integration channel, not a ceiling or
a reason to exclude richer integrations. Build Cue Deck around the best
existing capability: use native OBS hotkey for simple triggering, OBS source
and filter API where appropriate for discovery/parameters/state, and supported
plugin/vendor workflows when they offer materially better orchestration.
Keep Exeldro Move as a first-class eligible provider, not merely a disposable
compatibility shim or something we must rewrite. Build Aftermarket-native
capabilities alongside excellent existing solutions only where our implementation
adds distinct value; no forced migration, no automatic replacement of effects.
Preserve user-selected provider and semantics of cues, discover capabilities
before claiming support, and don't confuse triggered with completed.
This is a DESIGN DECISION only; no Cue Deck runtime or Move adapter implemented.


## Cue Deck — first working native slice (2026-11-23)

- Scoped implementation: named ordered cues of Aftermarket native dock-layout
  snapshots, stable OBS/native/plugin hotkey identities, and explicit waits.
  No forced replacement of Exeldro Move or other existing OBS plugins.
  Tools menu manager supports new/edit/delete/run/stop, add/reorder/remove
  layout/hotkey/wait steps, and readable action grouping. Native Action Strip
  cue button follows stable cue ID, updates caption on rename, disables when
  cue deleted; overflow preserves availability.
- Versioned atomic separate cue-deck.json config. Caps 32 cues, 48 steps,
  30 seconds per wait and 120 seconds cumulative wait. Invalid/corrupt loads
  do not replace current parsed cues. All actions preflight against live
  layout and hotkey choices before any dispatch. Runtime failure halts the
  remaining sequence; cancel stops pending timers without claiming undo.
  No cue runs automatically at OBS startup. Dispatch status deliberately
  distinguishes action invocation from effect completion.
- Qt RED/GREEN regression: new cue-deck-test and cue-strip-test cover
  persistence, malformed config recovery, name uniqueness, invalid step
  types/durations, missing dependencies/preflight no partial execution,
  ordered actions, wait, single flight, cancel, dispatch failure, manager
  widget availability, stable action-strip target and disabled stale button.
  Existing action-strip and dock-layout tests stayed green. Full build and
  32/32 CTests passed.
- Isolated real OBS 32.2 GUI test (no user scene collection): opened Tools
  Cue Deck, authored a cue in actual nested dialogs, picked a registered
  test-plugin frontend hotkey, persisted cue-deck.json, clicked Run and
  verified routed hotkey callback and exact dispatch-only status; exited OBS
  with code 0. Log /tmp/aftermarket-cue-obs-gui3-20261123/obs.log.
- Exeldro Move 3.2.1 was loaded in a separate isolated real OBS GUI session;
  created and attached an actual move_source_filter to a disposable source.
  In one run its source-scoped Start/Hold hotkeys registered; in others
  they were not present until source properties/ticks. This slice therefore
  supports Move by its *actually registered* native hotkeys and does NOT
  claim automatic Move-filter discovery or direct start/finish integration.
  Source test sessions exited 0. Specific Move completion has not been
  measured. Deeper Move integration should be implemented only once stable
  trigger semantics are verified against this installed version.
- Local installation verified using the exact final OBS-GUI-tested module
  (SHA-256 15fe75f65b77c2986e38acca2379b27203f2686777f7fdc61016a7a4c70c8c8a).
  Final isolated OBS GUI also loaded Exeldro Move 3.2.1 alongside the actual
  module and passed all authoring/save/hotkey-dispatch/status checks, exited 0:
  /tmp/aftermarket-cue-final-obs-gui-20261123/obs.log.
  Previous installed module backed up and byte-verified at
  ~/.local/share/aftermarket/rollback/obs-ui-augment.pre-cue-deck.9ca21fdae63d6ed7.so.
  scripts/install.sh installed byte-identical binary at
  ~/.config/obs-studio/plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so.
  Active OBS process PID 285986 was left running; feature activates after
  normal OBS restart. All existing plugin config file SHA-256s verified
  unchanged before/after installation. Postcommit focused 6/6 tests passed.
  Unrelated plugin.cpp comment change preserved unstaged. No push.

## Linux OBS32 CI and README consolidation (2026-11-24)

- Reuse the source-pinned SDK approach from Framerot, strict native testing
  and install-layout verification from Timeline, and product-first README
  conventions established across the OBS plugin portfolio. The initial
  Aftermarket and Streaming Media pilots share identical self-contained
  verification helpers; do not introduce a remote shared-workflow dependency
  until the hosted jobs qualify.
- Downloaded and SHA-256-verified OBS 32.2.0, built its SDK locally in an
  isolated directory, and clean-built committed source 3f9aa35 plus the
  scoped CI changes. All 32 native CTest cases passed; no skips.
  Staged package contained only the expected native module; no embedded
  RPATH/RUNPATH or unresolved shared libraries, and both libobs libraries
  resolved to the freshly built SDK. Checksum and extracted bytes verified.
  Archive SHA-256: a2b6fc5f5d6b197bfc8cca25d9dc22e178cb80596d1b886e23e3fe4e10e42509.
  Evidence: /tmp/obs-ci-pilots.Kk7s56/obs-ui-augment-ci.log.
- README relative link regression, shell syntax and YAML structure passed.
  Hosted GitHub Actions runs are not yet established; no push, release,
  source export, production installation, or OBS restart. Streaming Media's
  CI deliberately excludes bundled yt-dlp/Deno and live network playback;
  release licensing and wider platform qualification remain separate gates.

## Source picker native surface layering — 2026-11-24

During the gallery refresh, the embedded Add Source child was visibly occluded by native preview/transform surfaces. Root fix: keep the original OBS QDialog as a frameless window-modal transient owned by the main window instead of reparenting it as an ordinary child. Position in global coordinates within the owner's client area and relevant monitor; maintain dock-relative compact bounds, no always-on-top flag. Preserve native creation/add-existing/Properties, opt-out and dismissal. Stop placement timer on completion; reject if owner/dock disappears or owner minimizes. Transform visual refresh now yields to active modal dialogs in addition to popup/inactive window guards.

RED: source-picker regression with native preview widget failed child/window/transient-parent assertions on prior implementation. GREEN: offscreen and XCB focused tests pass, including native surface pixel check, modal ownership and bounds. Full build clean; 32/32 CTests pass. Actual OBS 32.2.0 isolated Xvfb :259, Rosé Pine, main moved to (120,70), exact built module loaded: selected Color with visible custom transform overlay, picker at (392,361) 380x400 fully painted above preview; transform overlay not visible during modal picker. Filtered existing Color and added it through native Add 1 existing, then single-click Image created Image 3 and native Properties. Transform overlay returned after clicking back into OBS (Xvfb without window manager does not automatically restore owner focus). Screenshots/logs in build/picker-layer-smoke.euQyAN; picker README asset recaptured from these verified views. No broad UI refactor, user settings changes or push. Earlier gallery refresh remains separate; unrelated plugin.cpp comment excluded from commit.

## Aftermarket internal identity migration — 2026-11-24

Renamed module/build/staged package to aftermarket.so, OBS module display name to Aftermarket, runtime logging and legacy Inspector/Panels/widget identifiers to Aftermarket names, docs and CI package validation updated. Existing checkout remains obs-ui-augment to avoid breaking workspace references. Existing stable hotkey and dock-layout IDs preserved. Gradient exclusions remain limited to the renamed original four owned widget IDs, not all aftermarket-* descendants (native mixer controls still receive gradients).

Linux installer stages the new binary and retires the entire old plugin package into an external rollback directory before publishing the replacement. It restores the old package if publishing fails. Configuration is not migrated while the existing OBS instance is running: the new module moves the complete legacy config directory at startup, before constructing persistent services, including unknown files and safe-mode.flag. Existing populated destinations, invalid paths and symlinks fail closed rather than overwrite configuration. Manual side-by-side user installation is rejected; system-wide old modules must also be removed. Docs include upgrade and rollback instructions.

RED installer test failed because old installer expected obs-ui-augment.so; new config migration test initially failed to compile without its implementation. GREEN full native build and 34/34 CTests passed. Isolated OBS 32.2.0 Xvfb :260 upgraded a copy of the previous installation using scripts/install.sh, loaded aftermarket.so from the new plugin directory, logged [aftermarket] and registered layout/Quick Find/Cue Deck tools. Complete previous config files matched SHA256 checks after migration (rules, strip, layouts, picker, front audio and unknown receipt); native mixer backside and gradient widgets visible with renamed IDs. Local CMake install payload is exactly aftermarket/bin/64bit/aftermarket.so; no RPATH/RUNPATH or unresolved ldd dependencies. CMake strips build RPATH, so staged binary is not byte-identical to build binary. README link validator passed. Build emitted existing Qt AUTOMOC diagnostic for knob-test; no claim of hosted CI execution or other platforms. Local runtime receipts: build/identity-smoke.C7HuoE. No checkout rename, no unrelated changes or push.

## Compact Add Existing thumbnails — 2026-11-24

User requested thumbnails in Add Existing without expanding the compact picker. ExistingRows mirrors each native OBS SourceSelectButton's thumbnail QLabel pixmap into a 64x36 aspect-preserving decoration, 44px row height, retaining the 380x400 frameless transient and separate unchanged Add New type list. Uses QPixmap cache keys to avoid repeated scaling; asynchronous native updates are mirrored on the existing refresh timer, only for name-filter-matching rows. No independent source rendering or changes to native selection, source UUIDs, filtering, or creation. Thumbnail availability and cadence remain governed by OBS's manager, including native type-icon placeholders for non-video/unconfigured sources or pending previews; offscreen native thumbnail scheduling is not overridden.

RED source-picker test failed for missing icon and size/late-update behavior. GREEN tests verify native pixmap pixels, asynchronous change without replacing card, portrait aspect preservation with transparent padding, null pixmap handling, filtering and native add identity. Full 34/34 CTests pass. Exact aftermarket.so loaded in isolated OBS 32.2.0 Xvfb :261 (build/picker-thumbs-smoke.Kl6pg7): verified native Color thumbnail, Add 1 existing action, and configured Image 3 with a disposable geometric test image through native Properties. Its actual image appeared in both Recent Sources and Image-filtered list, with native icons for empty images/audio. README gallery updated from those runtime shots. No claim of overriding native thumbnail scheduling for arbitrarily long or scrolled lists. No push; preserve unrelated plugin.cpp comment edit.
