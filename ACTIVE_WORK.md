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
