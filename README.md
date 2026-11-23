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

## Settings inside OBS

Open **OBS → Settings → Aftermarket** from OBS's normal Settings dialog.
Aftermarket has its own **native Settings sidebar entry** after Advanced,
not an extra dock menu, a Tools-menu shortcut, or another floating Settings
window. It groups interface preferences into **Appearance, Menus, Mixer,
Panels, and Action strip**. The app-wide **Long text** option lives under
**Appearance → At end / In middle**; it no longer appears in every dock's
three-dot menu.

The options are staged until you click OBS's existing **Apply** or **OK**
button. **Cancel discards unapplied Aftermarket changes**, using the same
native Settings controls as other OBS pages. Source names, mixer routing,
and unrelated OBS settings are preserved. Advanced target-specific tools
remain in the Inspector; the regular preferences now have their own home.

![Aftermarket entry in the real OBS Settings sidebar, with global Appearance preferences](docs/screenshots/readme/aftermarket-obs-settings.webp)

![Mixer and gradient preferences inside OBS Settings Aftermarket](docs/screenshots/readme/aftermarket-settings-mixer.webp)

## Inspector workspace

Open **Tools → UI Augment Inspector** for advanced per-target editing and
richer visual editors. Its specialized pages remain available for:

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

### Patchbay routing diagnostics

The **Audio Mixer backside** uses native OBS tracks 1–6, not a second audio
mixing engine. Each source now exposes its OBS track assignments and monitoring
mode alongside read-only **Muted**, **Disabled**, and **Audio inactive** state.
The small `M`/`Off` source markers and dashed inactive/muted cables make
routing easier to inspect without hiding still-connected wires. The panel
does not try to infer sound level: **Audio active** is OBS's own source flag,
not proof that audio is audible.

Hover a source to trace its cables, or hover a track to see the sources
feeding that track. These temporary paths work even with **Show all wires**
off. Mouse movement, refresh and inspection never change routing; existing
source-to-track drag gestures, native track buttons, and monitoring selectors
are the only edit paths. Aftermarket doesn't inspect or modify atkAudio
PluginHost2 graphs, device routing or VST chains.

### Emergency recovery mode

**Ctrl+Alt+Shift+F12**, while OBS is focused, immediately activates
**Aftermarket safe mode**. This shortcut is captured even when an Aftermarket
popup has keyboard focus. A second path is always available under
**Tools → Aftermarket: Emergency restore** (including inside the hamburger).
Recovery disables Aftermarket's menu decorations, hamburger, gradient/rotary
effects, audio meter tinting, source/scene row colors, collapsible property
groups, dock backsides, and dock/preview layout enhancements. Original OBS
widgets and actions are restored **without changing scenes, routing or
Aftermarket preferences**. It is not OBS's own *Reset UI* command and does
not reset OBS dock layouts or close the application.

Before changing the UI, recovery writes a separate
`plugin_config/obs-ui-augment/safe-mode.flag` alongside `rules.json`.
On the next OBS startup, the plugin checks this marker and does not apply
customizations, **even if** the settings file is unreadable. Repeated use of
the emergency shortcut cannot accidentally re-enable a broken interface.
After the issue is corrected, choose **Tools → Aftermarket: Resume UI
customizations**. Only successful explicit resume removes the marker; the
original `rules.json` is never rewritten by emergency mode.

If OBS is unable to reach the shortcut because startup fails, create
`safe-mode.flag` in the same plugin configuration folder as `rules.json`
while OBS is closed, then reopen OBS. Remove it or use **Resume** only
when ready to try the customizations again.

The shortcut works within OBS/Qt, not as a system-wide hotkey when another
application is focused. Desktop environments may intercept key combinations;
the Tools action and on-disk marker are alternatives.

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
The remaining menu-bar space can hold the optional Action Strip described below.

![Native OBS menus inside Aftermarket's compact hamburger bar](docs/screenshots/readme/hamburger-menu-bar.webp)

### Configurable Action Strip: live status, native hotkeys and docks

**OBS Settings → Aftermarket → Action strip** controls visibility and
opens the button editor. The **Tools → Aftermarket: Action Strip…** shortcut
is still available for editing. Choose
**Show action strip in OBS menu bar** to place your configured items at the
right of the *existing* native menu bar, including when the hamburger is off.
It is **off by default** and does not replace OBS's Controls dock, native
menus, hotkeys, scene collection or status bar.

Available items:

- **Status:** REC/stream activity, recording or stream elapsed time, FPS,
  CPU, dropped *stream* frames, missed *render* frames, live recording and
  streaming bitrates. Metrics share one periodic refresh; the stream/record
  clocks follow OBS's frontend start/pause/resume/stop events. If the plugin
  attaches to a session that already started, elapsed time displays
  **--:--** instead of inventing a start time.
- **OBS hotkeys:** Search a grouped list of actual frontend, plugin, source,
  output, encoder and service hotkeys. The persisted identity includes the
  registering owner (source UUID where available), not just the displayed
  name. Missing/ambiguous hotkeys become inactive; they are never reassigned
  to an unrelated source.
- **Native dock buttons:** Show/hide any dock with an identifiable OBS dock ID.
  Buttons reflect real dock visibility without replacing the dock.
- **Flexible spacers and separators** for arranging groups.

Drag controls to reorder them directly on the strip, or use the editor's
move buttons. Right-click an existing control to rename, move or remove it.
The bar uses the active OBS Qt palette and existing typefaces, with the same
subtle checkable-button style as other Aftermarket controls. When there is
not enough horizontal room, excess items move to a working **⋯** overflow
menu instead of becoming tiny or cut off.

Its configuration lives in the independently and atomically saved
`plugin_config/obs-ui-augment/action-strip.json`. The emergency shortcut
**Ctrl+Alt+Shift+F12** hides the strip and restores OBS's original menu
corner widget; **Resume UI customizations** recreates it with the same saved
items and enabled preference. Aftermarket also makes bounded attempts to
attach the *existing* optional Audio Mixer backside when OBS creates its
dock late, and stops retrying on emergency recovery. It does not create a
new multi-dock manager or copy Timeline's macro data.

![Native OBS Action Strip in Colorway](docs/screenshots/readme/action-strip.webp)

### Alternating Scenes and Sources backgrounds

In **OBS Settings → Aftermarket → Appearance**, turn on **Scenes** and/or
**Sources** alternate rows independently. The Inspector's detailed
**Panels → Alternating list backgrounds** editor remains available for
choosing custom row colors. By default both are off.
**Normal rows** and **Alternating rows** can each use a chosen solid color
or **Use theme** to return to native Colorway/OBS styling. The settings
are stored atomically as `listRows` in `rules.json`; changes apply immediately
and disabling the feature restores the original palette and stylesheets.

This augments the **existing OBS widgets**. The built-in scene selection and
hover colors remain native; source-specific row colors assigned through OBS
are preserved. Scene *grid mode* stays untouched. Other OBS/third-party lists
are deliberately excluded until their native behavior can be checked.

![Alternating row colors on native Scenes and Sources lists](docs/screenshots/readme/alternating-scenes-sources.webp)

### Theme-aware typography and selected states

Aftermarket's own controls use a restrained visual hierarchy without
changing the current OBS theme's font family or color palette. The Inspector
uses normal-weight body text and semibold section headings; Studio Mode
labels scale relative to the active font rather than forcing 12px.
Aftermarket-owned selected Studio view buttons and patchbay track switches
use a fine theme-color outline rather than the stronger generic checked
state. Audio cables, selected source borders and drag previews are also
thinner, while their source/track routes remain unchanged.

Compact menus honor the active font's actual line height. **Native OBS
widgets, menus and source list selection remain owned by OBS and its theme.**

![Aftermarket Inspector typography with Colorway](docs/screenshots/readme/inspector-typography.webp)

### Native Audio Mixer and AudioHub

Enable **AudioBus alternate routing view** in **OBS Settings → Aftermarket → Mixer**. The native Audio Mixer and AudioHub remain **two alternate views inside the same dock**, not two vertically stacked panels. Only the **interactive AudioBus switch** has moved to a small bottom-edge control: click **AudioBus** to open the routing view, then **Audio Mixer** to return. The original OBS audio controls are preserved, and emergency recovery **Ctrl+Alt+Shift+F12** restores the native mixer without changing scene or audio routing.

**AudioHub (alternate routing view)** is a compact source-to-output patchbay:

- **Click a source node to select it**, then click a numbered destination to toggle its route or drag the source to connect. Clicking an already-connected track toggles it off; repeating a drag never disconnects it. **Show all wires** reveals every source's routing. Hovering a source or output temporarily highlights related paths without modifying state.
- The six **circular output nodes** represent OBS's actual tracks 1–6. The separate circular **headphones node** represents OBS monitoring, not Track 7. **Left-click** the headphones to toggle Off / Monitor and Output for the selected source; **right-click** it for the three-mode menu (**Off / Monitor Only / Monitor and Output**). There is no bottom monitoring dropdown. Dragging to headphones preserves an existing Monitor Only connection and never rewrites track assignments. The selected source is identified by OBS UUID, not its potentially duplicated or renamed display name.
- **Status dots** sit on the right of each source and output circle, separate from cable ports. A filled accent dot means a recent measured signal; a ring with center mark means measured silence; a slashed ring means muted or disabled; a hollow ring means unknown or inactive audio. Output dots aggregate eligible routed sources and respect Monitor Only. Icons on the left follow registered OBS source IDs, not editable names. All rings, fills, and glyphs use the active theme palette. A lit output dot **does not prove a physical playback device is audible**.
- The graph owns source selection and routing, so the **Outputs** heading, duplicate source selector, lower output-button row, bottom Monitor dropdown, and verbose stats are gone. Clicking or dragging to the circles keeps native OBS routing behavior; clicks on transparent square corners do not toggle a circle. In narrow docks the headphones stay pinned at the top. Hover previews and **Show all wires** continue working.

![Compact AudioHub routing with source icons, right-side activity and circular outputs](docs/screenshots/readme/mixer-patchbay.webp)

![Right-click the headphones for the native OBS monitoring modes](docs/screenshots/readme/audiohub-monitor-menu.webp)

**Dock title icons:** Aftermarket draws small, theme-colored vector symbols **before (to the left of)** the real titles of OBS docks, like icon-and-label entries in OBS's context menus: layered frames for Scenes, linked nodes for Sources, mixer sliders for Audio Mixer, arrows for Transitions, a toggle for Controls, a monitor for Preview, and chat/browser symbols for matching plugin docks. Unrecognized docks get a neutral panel icon. Newly added plugin docks and title changes are picked up automatically. The icons are passive and mouse-transparent; they never replace OBS's native dock buttons or drag behavior. To make left-hand space under Qt themes that ignore title styling offsets, Aftermarket temporarily adds display spacing to the on-screen title, while keeping **Docks menu labels unpadded**. The exact original titles are restored on unload or emergency recovery. Icons use 14px or compact 10px artwork when space allows. In cramped native titles, the visible label can elide so the menu remains clickable while the **full original name stays in OBS's Docks menu**. Floating, vertical-title or custom-title-bar docks retain their own window/title controls rather than being overlaid.

![OBS dock titles with theme-aware icons](docs/screenshots/readme/dock-title-icons.webp)

**Three-dot menus on native dock titles:** Every native-titled OBS or plugin dock gets a theme-colored vertical ellipsis. Its **first action** is **Collapse**, changing to **Expand** when the dock is rolled up; it uses the existing Aftermarket Panels action, preserves the original widget and restores its dimensions on expansion. Tabbed or otherwise non-collapsible native docks show a disabled action instead of altering native ownership. Floating docks retain their window manager title, so Aftermarket does not overlay its title buttons. Newly created plugin docks are discovered automatically. The Audio Mixer keeps its more detailed menu, with the same Collapse/Expand action **above** its existing mixer-only entries, and no second ellipsis. The original dock title, close/float buttons, native Docks menu names and menu interactions stay under OBS ownership. There is no new dock or floating toolbar.

![Overflow buttons on OBS native docks, including a shortened Scene Transitions title](docs/screenshots/readme/dock-all-overflows.webp)

![Scenes dock three-dot menu: Collapse](docs/screenshots/readme/dock-collapse-menu.webp)

![Scenes after Collapse; native source contents remain intact](docs/screenshots/readme/dock-collapsed.webp)

![Audio Mixer retains its options under the first Collapse action](docs/screenshots/readme/dock-mixer-overflow.webp)

**App-wide long-text truncation:** Choose **OBS Settings → Aftermarket → Appearance → Long text → At end / In middle**. The same preference controls truncation in dock titles, native Qt source/scene lists and other item views, tabs, non-editable dropdowns, pushbuttons, checkboxes/radio buttons, ordinary non-wrapping display labels, and Aftermarket's custom AudioHub source labels and menu selection summaries. Middle truncation preserves both ends (for example, Scene…itions) and applies to newly created windows and plugin docks. Existing installations default to End, which **leaves other OBS widget types' existing presentation untouched**. Turning middle mode off restores previous Qt view/tab preferences and full label text. The choice is saved in plugin_config/obs-ui-augment/dock-title-settings.ini.

The presentation layer does **not** rewrite OBS source/model names, editable text fields, actual button or dropdown values, rich/word-wrapped labels, or native Docks menu names. Native Qt elision is used for item views and tabs; buttons/dropdowns use Qt-style drawing; plain labels are reversibly shortened. Third-party widgets with completely custom painting may need their own adapter.

**Same real OBS Sources list, same full names—only the display changes:**

![OBS Sources list before middle truncation](docs/screenshots/readme/app-elision-end.webp)

![OBS Sources list with global middle ellipsis selected](docs/screenshots/readme/app-elision-middle.webp)

![The Scene Transitions dock with a middle ellipsis](docs/screenshots/readme/dock-title-middle.webp)

**Native mixer title and bottom control:** After **Collapse/Expand**, the **vertical three-dot button in the genuine OBS dock title** opens the pan/sync presentation setting and the original **hidden sources, mixer orientation, Advanced Audio Properties and Mixer options** actions, plus a **Channel settings** submenu with per-source native Pan and Sync editors. The old OBS footer is hidden only when the header menu is actually available; floating or cramped docks restore it, so controls remain accessible. The interactive **AudioBus** button lives by itself at the dock's bottom edge and switches between mixer and routing pages, never stacking the entire routing graph below OBS. Dock icons, native titles, native undock controls, and drag behavior remain untouched.

![Mixer with only the AudioBus switch at the bottom](docs/screenshots/readme/audiohub-bottom-switch.webp)

![Alternate routing view opened by the bottom switch](docs/screenshots/readme/audiohub-routing-view.webp)

![Native title-bar three-dot settings menu](docs/screenshots/readme/audiohub-title-settings.webp)

**Controls within the mixer:** Pan and Sync offset remain horizontally aligned in one compact row per OBS source, rather than two vertically stacked rows. Narrow vertical strips use a pan dial and a precise millisecond spinner; horizontal mixer strips use the same one-row layout. Pan maps to native OBS balance (0 = left, 50 = center, 100 = right), with mono pan disabled; the coarse sync dial spans ±2000 ms and the precise editor supports OBS's wider range. The vertical fader/meter geometry stays native — no compression is needed when the patchbay uses a separate page. No fader, mute button, source identity or monitoring state is replaced.

Pan and sync are read from and written to the same source properties shown in **Advanced Audio Properties**. Adjustments request a debounced OBS project save; the separate atomic `plugin_config/obs-ui-augment/front-audio-controls.json` holds only the slider/knob presentation preference. External source changes are read back while the dock is visible, and all additional widgets detach cleanly.

**Monitoring note:** the physical Monitor Device is selected under **OBS Settings → Audio → Advanced**. Routing a source to an already-captured monitor device can cause echo/feedback. Monitor Only suppresses normal program output even if track assignments remain enabled. The plugin neither chooses a hardware output nor performs arbitrary DSP/bus routing. With many sources, the diagram can still scroll.

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
