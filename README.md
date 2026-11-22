# Aftermarket

**Beyond factory spec.**

A native runtime UI modification engine for OBS Studio. Inspect, customize,
animate and transform existing controls without forking OBS or replacing its
theme system. Built to complement Colorway: the theme defines the visual
identity, while Aftermarket extends presentation and interaction at runtime.

Current capabilities include context-menu icons, hiding default commands,
custom executable actions, a native inspector, animated gradients, audio-reactive mixer effects, rotary gain controls with
integrated peak meters, collapsible docks, a native Audio Mixer patchbay backside,
and a reversible dockable preview.

Formerly OBS UI Augment. The installed module, configuration directory and
Tools menu entries retain their existing names for compatibility.

| Gradient faders | Integrated rotary meters |
| --- | --- |
| ![Native mixer faders with animated tracks](docs/screenshots/readme/mixer-faders.png) | ![Rotary gain controls and peak rings](docs/screenshots/readme/mixer-rings.png) |

| Native menu | Augmented menu |
| --- | --- |
| ![Native source context menu](docs/screenshots/readme/menu-disabled.png) | ![Augmented source context menu](docs/screenshots/readme/menu-enabled.png) |

Actual OBS 32.2.0 with Colorway, captured at native resolution in an isolated X11 test profile. The two mixer channels use a real test tone and silence.

## Install locally (Linux)

Requires the matching libobs and OBS frontend API development packages, Qt 6 Widgets and SVG, CMake and a C++17 compiler. Use the CMake package for the libobs actually loaded by the installed OBS executable.

```sh
cmake -S . -B build -G Ninja -Dlibobs_DIR=/usr/local/lib/cmake/libobs -Dobs-frontend-api_DIR=/usr/local/lib/cmake/obs-frontend-api
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/install.sh
```

Restart OBS (or open it if closed). The module creates its rules file at
`~/.config/obs-studio/plugin_config/obs-ui-augment/rules.json` by default.
If XDG_CONFIG_HOME is set, use that configuration root.

## Inspector workspace

Open **Tools → UI Augment Inspector**. All existing effects are now accessible
from four pages in one native, theme-aware window:

- **Menus & widgets:** Pick target with an adjustable 2–10 second delay,
  open a context menu and hover an action. Filter captured labels, see saved
  icon previews, choose an icon/color/animation and save a rule. Technical
  geometry, ancestry and generated JSON are available through Show technical
  details; ordinary widget capture remains read-only.
- **Mixer:** choose all current/future mixer sources or check individual
  live source names. Refresh the actual mixer list or add an exact offline
  name, including names containing semicolons. Select knobs, integrated
  peak meters, per-source reactive gradients or native meter color pulses.
  Picking a mixer widget opens this page at its source, without changing it.
- **Gradients:** enable track categories, select theme palette or enter
  custom colors, add colors with the native color picker, preview the
  sequence and adjust loop duration.
- **Panels:** collapse/expand eligible native docks, refresh the list or
  expand all. Optionally move the existing OBS preview and its source toolbar
  into a movable, floatable, closable native dock, or restore it to the center.
  Enable **Collapsible source / filter property groups** to add independent
  fold arrows to ordinary grouped OBS settings, preserving their live controls.
  Toggle **Audio Mixer backside (native routing)** to add a flip button to
  OBS's real Audio Mixer dock. **Alternating list backgrounds** adds separate
  Scenes and Sources switches with native-theme or custom row colors. The
  Tools → UI Augment Panels shortcut opens this page.

### Compact hamburger menu bar

**Tools → UI Augment Inspector → Menus & widgets → ☰ Menu bar** replaces OBS's
top-level File/Edit/View/Docks/Profile/Scene Collection/Tools/Help labels
with one hamburger entry, using **the same native Qt menus and actions**.
Click it to access every original submenu, including items registered by other
OBS plugins. The longer category names have room for their submenu arrows.
The existing **Compact menus** checkbox is separate: it controls long popup
layouts, not the top menu bar.

The hamburger mode is **off by default** and saved under
`"hamburgerMenuBar": true` in `rules.json`. Switch it off to restore the
classic menu bar immediately, without restarting OBS. Qt provides an
**Alt+M** shortcut, with the regular native menu/navigation behavior retained;
the shortcut was checked in the Qt regression harness, not in every desktop
window manager. Late plugin-added top-level menus are placed in the hamburger,
and enabling/disabling never duplicates or replaces OBS's actions or submenus.
The menu bar's remaining space is intentionally left available for later
status/quick-control slots—those controls are **not** part of this slice.

![Native OBS menus inside Aftermarket's compact hamburger bar](docs/screenshots/readme/hamburger-menu-bar.webp)

### Alternating Scenes and Sources backgrounds

In **Tools → UI Augment Inspector → Panels → Alternating list backgrounds**,
turn on **Scenes** and/or **Sources** independently. By default both are off.
**Normal rows** and **Alternating rows** can each use a chosen solid color
or **Use theme** to return to native Colorway/OBS styling. The settings
are stored atomically as `listRows` in `rules.json`; changes apply immediately
and disabling the feature restores the original palette and stylesheets.

This augments the **existing OBS widgets**. The built-in scene selection and
hover colors remain native; source-specific row colors assigned through OBS
are preserved. Scene *grid mode* stays untouched. Other OBS/third-party lists
are deliberately excluded until their native behavior can be checked.

![Alternating row colors on native Scenes and Sources lists](docs/screenshots/readme/alternating-scenes-sources.webp)

### Native Audio Mixer backside

This feature is **off by default** and stored as `"mixerBackside": true` in
`rules.json` when enabled. The original Audio Mixer remains unchanged behind
the flip button; floating and tabified docks retain OBS's own controls. The
backside draws actual source-to-track connections and exposes six track
toggles and the three OBS monitoring modes. Selecting a source uses its OBS
UUID (not its display name). The interface refreshes from OBS while visible,
so changes in Advanced Audio Properties appear here, and vice versa.

Tracks **1–6** are native OBS assignments, not newly invented buses or six
independent hardware outputs. Stream/record output-track selection and detailed
audio options stay in OBS's own Settings and Advanced Audio Properties. Source
routing changes save through OBS's usual scene collection, not a separate
patchbay configuration. The graphic is scrollable in narrow docks; custom
backside editors, size presets and arbitrary audio patching are not yet built.

![Real OBS/Colorway Audio Mixer backside showing native audio tracks 1, 4 and 6](docs/screenshots/readme/mixer-patchbay.webp)

![Mixer page in OBS](docs/screenshots/readme/workspace-mixer.png)

![Gradient page in OBS](docs/screenshots/readme/workspace-gradients.png)

![Panels page in OBS](docs/screenshots/readme/workspace-panels.png)

Property-group folding is off by default and saved as `collapsiblePropertyGroups` in
`rules.json`. It affects native `OBSPropertiesView` **ordinary QGroupBox sections**
only, not ungrouped filter controls or checkable enable/disable groups. Expanded
content is the original OBS form, and disabling the feature restores its
original row visibility. The folded state is session-only; groups rebuilt by
OBS begin expanded. No settings values are changed by folding.

Mixer and gradient edits share an explicit Apply changes button; a pending
indicator and Reload saved settings distinguish the draft from applied
settings. No separate editor dialog is needed. Apply preserves unknown
settings and rejects conflicting external changes rather than overwriting
them; an empty individual selection never silently selects all sources.
Motion remains a global immediate toggle. Menu rules still save individually.

Scope a menu rule to its captured menu (default), or uncheck scope to target
the action across menus. Reopen the menu after saving. Existing native icons
require explicit replacement. Remove exact rule deletes only the identical
selector and leaves broader rules intact. The menu matcher's previous
heuristic and rule precedence are unchanged.

![Captured native menu](docs/screenshots/readme/workspace-menus.png)

## Theme-aware transform selection

OBS's native selection frame normally uses red borders with square resize
handles. Aftermarket now offers **Modern transform frame and handles** in
**Tools → UI Augment Inspector → Gradients → Preview selection**.
This option is **off by default**, applies immediately, and is independent
of gradient tracks and the Motion toggle.

- Uses the active Qt/Colorway theme's link/accent color for **native selection
  borders**, highlight for crop, and a brighter accent for hover. This uses
  OBS's own accessibility color override rather than coloring rendered video.
- Draws a thin, contrast-backed frame with **rounded corner handles** and
  smaller circular edge handles. Hover gives a stronger fill. It follows live
  source transforms, including ordinary resize and rotation.
- The decoration is a **transparent mouse-through companion window**, not
  another scene renderer and not a replacement for OBS transform events.
  Original native handles still receive every click and drag.
- **Scope:** OBS 32.2.0 on composited Linux/X11 was tested. The rounded
  overlay is deliberately hidden at custom zoom/pan levels when its
  approximate projection cannot be trusted, or when the OBS window is not
  active. OBS's native selection remains usable. Group transforms and
  other platforms need separate validation.

<details><summary>Important behavior and limitations</summary>

The visible handles are larger, but **OBS's actual mouse hit targets are not
enlarged**. Enlarging them would require changing OBS native selection
hit-testing and is not part of this visual pass. Custom zoom and grouped
sources retain native handles rather than showing misplaced decorations.

The option persists as root-level `"modernTransformFrame": true` in
`rules.json`. While active, the plugin keeps a snapshot of the previous
OBS accessibility colors and restores them when disabled or unloaded,
preserving external edits rather than overwriting them. Existing user
settings are not changed by installation.

</details>

![Actual OBS/Colorway transform frame with a thin accent outline and rounded handles](docs/screenshots/readme/transform-modern.png)

## Dockable video preview

The preview normally stays in the center of OBS. To move it into the native
dock system, open **Tools → UI Augment Panels** (or the **Panels** tab of the
Inspector) and press **Make preview dockable**. The Preview dock can be
dragged between dock areas, floated, resized, or closed just like the other
native OBS docks.

- **Show preview** reopens the dock after you close its window.
- **Restore fixed preview** returns the **same existing** video preview and
  source-control toolbar to OBS's original central area. No duplicate libobs
  display or scene renderer is created.
- If Studio Mode is on, its existing Preview and Program views stay together.
  In narrow docks, they use OBS's portrait stack rather than extending off
  screen. The original direction returns when docking is disabled.
- Floating begins near the previous dock position at a usable size and
  remains freely resizable. Restoring the original preview is also performed
  on normal OBS window close and plugin unload.

| Docked Preview/Program | Floating Preview/Program |
| --- | --- |
| ![Native OBS Preview and Program workspace inside a movable dock](docs/screenshots/readme/preview-docked.png) | ![Same OBS preview workspace floating as an independent window](docs/screenshots/readme/preview-floating.png) |

### Responsive Studio Mode

Once the preview is docked, a small **Studio** toolbar appears whenever
OBS Studio Mode is active. It provides four session-only display choices:

- **Auto:** side-by-side Preview/Program in wide docks (at least ~900 logical
  pixels), stacked in medium docks, and Program focus in very small docks
  (under ~430 pixels wide or 380 pixels high).
- **Both:** keep the real Preview and Program surfaces visible; in a narrow
  dock, Program receives slightly more of the available height.
- **Program / Preview:** focus that one original OBS display without replacing
  its renderer. Switch back to Both or Auto any time.
- **Transition strip:** when stacked, the *original OBS control widget* becomes
  a compact horizontal, scrollable bar. The chevrons reveal the remaining
  native quick-transition buttons and T-bar; their original click handlers
  and sliders are not proxied or replaced.

Program and Preview titles are reduced in the compact layout and their
original font settings return with the fixed preview. Quick transitions
added or removed by OBS update the scrollable width. Switching Studio Mode
off and back on also safely reconstructs the strip.

| Compact dock | Wide floating dock |
| --- | --- |
| ![Aftermarket compact Studio Mode with stacked live displays and horizontally scrollable native transition controls](docs/screenshots/readme/studio-compact.png) | ![Aftermarket side-by-side Studio Mode with native centered transitions](docs/screenshots/readme/studio-wide.png) |

**Session-only and opt-in.** It does not change existing theme files or
`rules.json`. The normal OBS File → Exit path was checked in an isolated
test: the temporary dock was absent from the saved DockState. Native display
rendering, Studio Mode toggling and docking/undocking were exercised in
OBS 32.2.0 on X11. Actual streaming or recording, Wayland, Windows, macOS,
and long-running native-display stress remain unverified. The placement of
other docks may shift while the preview is docked; their identities and
contents are preserved, not their exact pixel geometry.

## Compact menus

Long *choice lists*, such as the Add Filter menu, use tighter rows and
balanced, visible columns, typically aiming for about 340 logical pixels in
height. The regular OBS **Source context menu stays a single column** with
compact spacing, preserving its familiar command layout. No commands are
automatically moved behind another menu. If a list cannot fit within the
available screen space, native navigation remains available.

| Source menu | Filter picker |
| --- | --- |
| ![Compact single-column source context menu](docs/screenshots/readme/menu-source-single.png) | ![All 18 effect filters in two columns](docs/screenshots/readme/filter-columns.png) |

Use **Tools → UI Augment Inspector → Compact menus** to enable or disable
the layout (enabled by default). Pick a particular action and check
**Move to More actions** only if you want that specific command tucked
away. Placement rules are stored independently in the root-level
`menuOverflow` array; existing icon rules are unaffected. Capture the action
again in More actions and uncheck it to restore its normal placement.

For OBS Appearance settings, the Theme and Style combo boxes with at least
16 choices open a searchable, scrollable grid; all choices remain present
until the user deliberately filters them. Long theme names use the full
allocated grid-cell width rather than being clipped to their intrinsic label
size; titles exceeding screen-constrained cells still have tooltips. Selection
commits to the original combo box's model. Unrelated and editable combo boxes
are left unchanged.

The native column adapter is built for Qt 6.4.2 with its private development
headers (on Debian/Ubuntu, qt6-base-private-dev) and requires a matching
runtime version. Other versions fall back to compact native rows rather
than accessing mismatched private layout data. Short menus, embedded
widget actions and tear-off menus keep their native layout. The active
theme supplies colors and type.

## Context-menu actions

Aftermarket can now customize the **contents** of a native context menu,
rather than only its appearance. Open **Tools → UI Augment Inspector**, pick
the target menu, then use the **Menus & widgets** page:

- **Hide this default entry** removes that selected command only from
  matching popup menus. The actual OBS QAction, its signals, and its original
  state are preserved. The entry returns when the rule is removed.
- **Add custom…** creates a labelled entry with an optional icon, an
  executable, and an argument list. These entries appear at the bottom of
  the captured menu and execute only when selected.
- **Manage entries…** edits or removes custom entries and restores hidden
  defaults. **Show all menu changes** works even with no menu captured, so
  it remains a recovery path if every entry was hidden.

The editor scopes changes to the captured menu by its native object name
when available, or by observed action labels for anonymous menus.
Unchecking **Scope to observed menu** while hiding a native entry permits
an explicitly broader match. No original OBS action is deleted.

Custom commands invoke the specified executable directly with its argument
list, **without automatically launching a shell or substituting selection
values**. Enter trusted commands only. Example excerpt (merge into your
existing `rules.json`; do not replace the entire file):

```json
{
  "menuHidden": [
    {"text": "Remove", "menuContains": ["Add Source", "Open Source Projector"]}
  ],
  "menuCustom": [
    {
      "id": "open-my-tools",
      "label": "Open my tools",
      "program": "xdg-open",
      "args": ["/home/me/tools"],
      "menuContains": ["Add Source", "Open Source Projector"],
      "icon": "monitor"
    }
  ]
}
```

These keys are added to the existing `rules.json`; they do not replace the
original `rules`, `menuOverflow`, or gradient settings. Add commands from
the inspector to generate unique IDs automatically.

| Customized native menu | Recover or edit saved entries |
| --- | --- |
| ![OBS source menu with a custom command and a hidden default](docs/screenshots/readme/menu-custom-actions.png) | ![Manage hidden native actions and custom entries](docs/screenshots/readme/menu-actions-manager.png) |

**Current scope:** native QMenu actions, scoped hiding, and direct executable
custom entries. The inspector can browse nested native submenus, but custom
entries are appended to the captured menu rather than placed into new nested
groups. Ordering custom commands, selection/source argument placeholders, and
importing external menu-action definitions are not yet implemented. Native
menus containing embedded widget actions are excluded from the default removal
layout for safety.

## Submenu inspection and selected choices

In **Tools → UI Augment Inspector → Menus & widgets**, check **List submenu
items** to expand a captured menu into a browsable tree. It includes
currently available nested actions, checked states, disabled rows,
separators, and OBS's color-preset buttons. Leave it unchecked for the
original concise top-level list. Selecting a nested action retains that
action's own submenu scope when inspecting or saving supported rules.

Enable **Show selected submenu choices** to paint the one explicitly
selected choice in a **right-aligned trailing column**, using the current
theme's link/accent color and parentheses, for example:

- **Scale Filtering** — *(Lanczos)* aligned near the submenu arrow
- **Blending Mode** — *(Normal)* aligned at the same right edge
- **Set Color** — *(Red)* (also supports OBS's eight selected preset swatches)

The toggle is off by default and persists as the root-level
`"showSubmenuSelections": true` option in `rules.json`. The main
QAction labels are **never rewritten** to display values. A transparent
drawing layer adds the current choice independently; native arrows and hit
targets stay functional, and the layer disappears when the menu closes.
Long values are elided as space requires. If a submenu exposes no
unambiguous checked choice, the parent remains unchanged. Dynamically populated submenus
may need to be opened before capturing their full list.

| Selected choices in OBS | Expanded inspector menu tree |
| --- | --- |
| ![Native OBS source menu showing selected color and Lanczos filtering](docs/screenshots/readme/menu-selected-choices.png) | ![Inspector listing nested native menu items and selected color preset](docs/screenshots/readme/inspector-menu-tree.png) |

## Motion

Open **Tools → UI Augment Inspector** and enable **Motion** in the toolbar.
This applies the default soft effect to matched icons the next time a menu opens.
Motion is off by default, so existing configurations retain their static appearance.
For individual actions, pick a preset and duration in the inspector and save the rule.

- **soft**: fade/slide entrance, modest icon growth on hover.
- **pop**: fade/slide with a more pronounced scale transition.
- **spin**: fade/slide with rotation on hover.
- **none**: static icon even when global motion is enabled.

The duration is 60–600 ms (default 160 ms); a capped 9 ms stagger separates successive entrances.
Uncheck Motion for reduced motion. The master switch persists in the rules file; it is applied when a menu opens.

```json
{
  "enabled": true,
  "motion": true,
  "rules": [
    {
      "text": "Filters",
      "icon": "sliders",
      "animation": {"preset": "spin", "duration": 220}
    }
  ]
}
```

Icon motion does not move the native menu hit targets. Only the independently drawn icon layer
changes. The renderer caches rasterized SVGs at the display's device pixel ratio, clips
motion to the icon gutter, uses monotonic elapsed time with cubic easing, and stops its
16 ms timer when transitions finish or the menu closes. Hover retargets from its current
value rather than snapping to an endpoint.

[Real OBS icon reveal frames](docs/screenshots/motion-frames.png).

Verified in isolated OBS 32.2.0 with the actual Colorway Rosé Pine theme: GUI enabled
motion, selected spin and a 600 ms duration for Scale Filtering, persisted the scoped
rule, and captured changing icon pixels during both entrance and hover.

## Animated gradient tracks

Open **Tools → UI Augment Inspector → Gradients**. Turn on **Enable gradient tracks**, select sliders and/or progress bars, and apply. Enable the inspector's global **Motion** toggle for flowing gradients. The defaults leave existing configurations untouched.

- Sliders and media seek bars: gradient follows native slider position, including vertical faders, inverted appearance and right-to-left geometry. The native thumb and input stay intact.
- Determinate progress bars: fill follows the widget's actual range and value; text-bearing bars use a thin accent strip to preserve native text. Indeterminate, empty, hidden and disabled tracks are not painted.
- Colors: leave blank for the native palette, or specify 2–6 opaque colors separated by commas. Loop duration is adjustable from 400–12000 ms.

```json
{
  "enabled": true,
  "motion": true,
  "gradient": {
    "enabled": true,
    "sliders": true,
    "progress": true,
    "period": 1800,
    "colors": ["#31748f", "#c4a7e7", "#9ccfd8"]
  },
  "rules": []
}
```

Edit just the gradient and motion fields in an existing file to preserve your menu rules. The inspector merges settings into the existing file, atomically. Changes apply without a theme reload or restart; the rules watcher also reloads valid external edits. Motion off freezes the gradient; gradient off removes the decoration entirely.

The overlay derives groove, thumb and progress content rectangles from Qt's actual style geometry. Slider gradients extend to the native thumb center, preserving the actual handle silhouette instead of leaving a rounded gap before it. A cached native-paint comparison covers the exposed page around the thumb; solid-color antialiased edges are recomposited over the animated fill. Ambiguous custom handle pixels remain native. It uses a seamless repeated gradient and one shared 30 Hz timer, which stops if there are no visible, nonempty animated tracks. It never writes slider values or changes the stream compositor.

![Gradient tracks on actual OBS color correction filter sliders](docs/screenshots/readme/gradient-thumb-join.png)

[Earlier media seek and fader verification](docs/screenshots/gradient-tracks.png).

Verified in an isolated OBS 32.2.0 session with Colorway Rosé Pine, native media playback and mixer fader. Both tracks' pixels changed while paused elapsed text stayed fixed; native seeking updated playback position. Live motion-disable made the pixels stable, and live gradient-disable restored the native appearance. Qt regression tests cover progress bars, vertical and inverted sliders, stylesheet geometry, invalid configuration and reduced motion. Progress bars were tested in Qt, not a specific OBS progress dialog. Custom-drawn controls that are not QSlider/QProgressBar are outside this slice.

## Audio-reactive mixer effects

In **Tools → UI Augment Inspector**, enable gradient tracks on Gradients, then open Mixer and enable
**Audio-reactive mixer gradients**, and optionally **Pulse native meter colors**.
Enable Motion in the inspector. The effects are opt-in and can be toggled live.

- Each mixer fader follows its own source's libobs magnitude, not the global
  clock or another channel. Audio energy accelerates gradient flow and raises
  brightness; silence or mute brings the effect to rest. The native slider
  position and volume never change.
- The native meter's nominal foreground color responds to the same envelope
  by brightening its current theme color. This is a color modulation, not a
  replacement meter renderer. Warning, clipping, disabled colors, magnitude
  and peak-hold geometry remain under OBS control.
- Non-mixer sliders and progress bars retain the time-driven gradient behavior.

The exponential envelope uses 55 ms attack and 220 ms release.
Audio callbacks publish only a numeric magnitude and timestamp; no waveforms
are retained and no audio is sent anywhere. Source identity is matched to the
full current VolumeName button label; weak source references are revalidated.
No private OBS object offsets or ABI casts are used. Probes are released after
the associated controls stop requesting levels, or when the effects are disabled.

Add these keys to an existing gradient object:

```json
{
  "audioReactive": true,
  "meters": true
}
```

[Reactive mixer verification: tone and silent sources](docs/screenshots/audio-reactive.png).

Verified with real OBS 32.2.0, Colorway Rosé Pine, two isolated media sources
and no active recording or streaming: 1218 changed pixels on the tone fader
versus zero on the silent fader. Turning Motion off restored the native meter
nominal color (325 changed pixels); sampled warning/clip and silent-meter regions
were unchanged. Native mute stopped the tone fader effect after release.
The Qt tests also verify independent envelopes, unchanged slider values, theme
base updates and configuration persistence. This first slice is magnitude-driven,
not beat detection or FFT. OBS named-token resolution and full gradient painting
inside meter bars remain future work.

## Rotary mixer knobs

Open **Tools → UI Augment Inspector → Mixer** and check
**Replace mixer faders with knobs**. Enable All mixer sources to replace every
mixer fader, or select specific sources in the checklist (e.g.
`Mic; Music`). Apply without restarting OBS. For now this is limited to mixer
sliders, not seek bars or arbitrary settings controls.

Click and drag a knob up/right to increase volume, down/left to decrease.
There is no value jump on initial press. The original QSlider stays in the
layout and owns range, tracking, value, slider signals, focus, and existing
keyboard/wheel behavior. Its native paint is suppressed while a mouse-transparent
child draws a rotary ring and pointer. Enough minimum size is requested to
fit the knob, subject to the native maximum and surrounding layout constraints.

Knobs work independently of Enable gradient tracks, using native palette colors.
With Sliders and gradient tracks enabled, the ring uses the same configured
palette, motion and optional per-source audio-reactivity as faders.
Toggle knobs off to restore the native slider and its original size constraints
where those have not been changed by another owner. The current volume is preserved.

```json
{
  "knobs": true,
  "knobSources": ["Mic", "Music"]
}
```

Merge these keys into the existing gradient object. Source names are scoped
exactly, not durable source UUIDs; the text UI uses semicolons as delimiters.

[Earlier mixed knob/fader verification](docs/screenshots/knob-mixer.png).

Verified in isolated OBS 32.2.0 with Colorway Rosé Pine: Tone test switched to
a knob, Silent test remained a fader; the reactive ring animated (242 changed
pixels, silent fader zero); native volume moved from -12.0 dB to -3.5 dB after
a drag; disabling knobs restored the slider at -3.5 dB. Qt regressions cover
target selection, slider signals, programmatic updates, native key handling,
invalid config and restoration. Horizontal mixer layouts, live source rename
and theme changes under knob sizing need further real runtime testing.

## Integrated knob meters

Knob-enabled mixer channels now default to a combined rotary control rather
than a knob alongside a tall native meter. The inner ring shows gain, the
outer ring shows actual peak dB from the source's libobs volmeter. Metering
continues even with decorative Motion disabled. Native nominal, warning and
error colors are reused, with -60 to 0 dB scale, -20/-9 dB thresholds, a
one-second peak marker, clip flash and 30 dB/s release.

![Integrated peak rings on the actual OBS mixer](docs/screenshots/readme/mixer-rings.png)

The native meter is hidden only when paired with the supported knob and
source can be resolved. It is restored if the binding is unavailable or
the effect is disabled. Disable **Integrate peak meter into knob** in
Tools → UI Augment Inspector → Mixer to retain the full native meter.
The corresponding JSON key is `gradient.knobMeters` (default true).
The compact control requests a 68-pixel minimum and 76-pixel maximum height
subject to native constraints. The OBS setting for sample/true peak is used.

This is a combined maximum-channel peak display, not a reproduction of the
full multichannel OBS meter, magnitude and input-peak indicators. Use the
native meter for detailed stereo/monitoring work, particularly when checking
levels against the scale or custom threshold settings. The ring does not
modify source gain; the existing inner gain control continues to operate the
original slider. Real vertical mixer verification: one selected knob
replaced only its paired meter, mute cleared the outer ring without moving
gain, both knobs could be selected, and dragging changed the real dB label
from -1.9 to 0.0 dB. Live disable restored the original meter.
The meter scale remains visible at silence and mute, using dimmed native zone colors. Horizontal and vertical layouts have been exercised in isolated OBS, including switching orientation; theme-driven slider size changes are reconciled. Source renaming/rebuilding remain open.

## Collapsible panels

Middle-click a visible dock's title bar to collapse it to its header. Middle-click
again to expand. Alternatively open **Tools → UI Augment Panels**, select a
panel, and choose **Collapse / expand**. **Expand all** restores all rolled-up
panels. The original dock and its contents stay alive; this does not close the
dock or delete OBS sources.

| Expanded Sources dock | Collapsed Sources dock |
| --- | --- |
| ![Expanded native Sources dock](docs/screenshots/readme/panel-expanded.png) | ![Sources rolled up to its header](docs/screenshots/readme/panel-collapsed.png) |

The controller temporarily hides dock content and constrains the dock height
to the native header, preserving its width. It asks QMainWindow to redistribute
space and restores the original size constraints and requested dimensions
when expanded. Neighboring dock sizes remain subject to Qt layout negotiation;
pixel-perfect restoration is not guaranteed. Native close/float gestures
are untouched. Floating, tabified and vertical-title docks are excluded from
collapse in this slice. Closing or relocating a collapsed dock restores it.
The collapse state is session-only; no separate collapse settings are persisted.
The controller restores on the main window close event or unload. Smooth
height transitions are not implemented yet.

Verified in isolated OBS 32.2.0 using Colorway Rosé Pine: Sources collapsed
to a readable title bar at its original column width and expanded to show
the original source list; panel manager collapsed Scenes and Expand all
restored it. Also verified by Qt dock-layout regression tests.

## Live rules

The file is re-read just before a menu opens. Close and reopen the menu after editing it; there is no OBS restart or theme replacement. Invalid or partially written JSON keeps the last valid rules.

Example of a complete replacement rules file:

```json
{
  "enabled": true,
  "inspect": false,
  "rules": [
    {"text": "Add Source", "icon": "plus", "color": "#90d4a6"},
    {"text": "Filters", "icon": "sliders", "color": "#9cc4f5"},
    {"objectName": "actionRemoveSource", "icon": "trash", "color": "#f08b8b", "replaceExisting": true}
  ]
}
```

- `text`: exact action label, ignoring mnemonic ampersands, trailing ellipses, case, and shortcut suffixes.
- `objectName`: exact Qt action identifier. Prefer it when available; many OBS menu actions are created without one.
- `menuObjectName`: optional exact menu identifier. If both action selectors are provided, both must match. First matching rule wins.
- `menuContains`: optional array of neighboring action labels that must all be visible in the target menu. Used to scope otherwise anonymous popups. Matches normalized labels, not order. This is a contextual heuristic, not a unique persistent menu ID; it may not match after menu layout or language changes.
- `icon`: one of plus, pencil, trash, copy, clipboard, move, order, group, sliders, gear, pointer, monitor, camera, palette, layers, arrow-in, arrow-out, resize.
- `color`: optional literal color; without it the icon uses the menu text palette. Disabled entries are dimmed.
- `replaceExisting`: opt in to covering a pre-existing icon with an invisible slot and our own drawing, then restoring it on close. The stock Remove rule uses this to give destructive actions their own color.
- `inspect`: logs the discovered menu labels and action object names to the ordinary OBS log; turn it off again afterward. `enabled:false` disables painting but inspection can remain enabled.

## Why this isn't another QSS theme

A global, non-consuming event filter discovers QMenus as they are polished or shown.
Before each popup, matched, ordinary actions receive a transparent icon as a temporary
layout reservation. The actual icon artwork is owned by a mouse-transparent child
widget, painted against the menu's real action geometry. It does not become a native
QAction icon. OBS/Qt still control keyboard navigation, shortcuts, selection, submenus,
and action execution.

On close or unload, original icons and visibility are restored if the reservation
still belongs to us. Existing icons are left alone by default. Separators,
checkable actions and QWidgetActions are not decorated. Unknown actions are untouched.
The extension is local to UI rendering, not the OBS video compositor.

Rules remain declarative JSON; they are not evaluated as scripts. Explicit
`menuCustom` actions are an exception to the original visual-only scope: when
clicked, they launch the configured executable and argument array directly,
without invoking a shell automatically. Only add trusted commands. Aftermarket
does not implement a complete custom menu renderer, custom icon file imports,
general-purpose widget styling, or runtime scripting.

## Verification

- Qt regression test: actual magenta pixels in an offscreen QMenu grab; native icon pixels are transparent; existing and checkable icons preserved; rule hot reload, disabled state, invalid JSON fallback, explicit icon override and restoration, cleanup on unload.
- OBS 32.2.0 isolated Xvfb profile: 22 source context-menu actions augmented, then disabled and re-enabled without restarting OBS. The menu comparison above was recaptured from the actual enabled and disabled popup in a later isolated session.
- Clicked rendered Rename item in the real popup and verified native inline editing. See [native rename receipt](docs/screenshots/native-rename.png).
- Isolated OBS exited via Ctrl+Q. No crash observed. The OBS log reported 3 memory leaks at shutdown; their ownership was not established. No claim of leak-free operation.

Inspector slice verification (same isolated OBS configuration):
- Captured an actual source widget and displayed its ancestry.
- Captured Sources context menu from pointer position, selected Scale Filtering, saved a scoped color override from the GUI, reopened the menu and programmatically counted 12 magenta pixels in the actual popup.
- Removed that exact override from the GUI and confirmed the original broader rule remained.
- Added regression tests for scoped matching, escaped ampersands, snapshot lifetime, preserving unknown root fields and unrelated rules, refusing invalid JSON, duplicate override replacement, real rendered pixels, and GUI rule saves.

Only Linux/X11, this OBS/Qt version and this theme have been tested in real OBS.
Fixed 16-pixel artwork and a six-pixel inset suit the tested menu, but additional themes,
DPI scaling, long scrolling menus, RTL, other platforms and other widgets need validation.

## Disable/remove

Set `"enabled": false` in rules.json and reopen menus. To completely unload,
close OBS and remove just `plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so`
from its user config directory. The rules file can be preserved.
