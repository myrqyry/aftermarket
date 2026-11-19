# OBS UI Augment

An experimental runtime augmentation layer for OBS Studio. The first slice adds independently painted icons to the existing menus without changing OBS themes or source objects.

| Native | Augmented |
| --- | --- |
| ![Native source context menu](docs/screenshots/menu-disabled.png) | ![Augmented source context menu](docs/screenshots/menu-enabled.png) |

Real OBS 32.2.0, Qt 6.4.2, Colorway Rosé Pine — Pine, isolated X11 test profile.

## Install locally (Linux)

Requires OBS's matching libobs development package, Qt 6 Widgets and SVG, CMake and a C++17 compiler. Use the CMake package for the libobs actually loaded by the installed OBS executable.

```sh
cmake -S . -B build -G Ninja -Dlibobs_DIR=/usr/local/lib/cmake/libobs
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/install.sh
```

Restart OBS (or open it if closed). The module creates its rules file at
`~/.config/obs-studio/plugin_config/obs-ui-augment/rules.json` by default.
If XDG_CONFIG_HOME is set, use that configuration root.

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

The first slice is intentionally declarative JSON, not arbitrary code execution.
It does not implement a complete custom menu renderer, custom icon file imports,
a graphical rule editor, general widget styling, or runtime scripts.

## Verification

- Qt regression test: actual magenta pixels in an offscreen QMenu grab; native icon pixels are transparent; existing and checkable icons preserved; rule hot reload, disabled state, invalid JSON fallback, explicit icon override and restoration, cleanup on unload.
- OBS 32.2.0 isolated Xvfb profile: 22 source context-menu actions augmented, then disabled and re-enabled without restarting OBS. Screenshots above were captured from the actual enabled and disabled popup in the same session.
- Clicked rendered Rename item in the real popup and verified native inline editing. See [native rename receipt](docs/screenshots/native-rename.png).
- Isolated OBS exited via Ctrl+Q. No crash observed. The OBS log reported 3 memory leaks at shutdown; their ownership was not established. No claim of leak-free operation.

Only Linux/X11, this OBS/Qt version and this theme have been tested in real OBS.
Fixed 16-pixel artwork and a six-pixel inset suit the tested menu, but additional themes,
DPI scaling, long scrolling menus, RTL, other platforms and other widgets need validation.

## Disable/remove

Set `"enabled": false` in rules.json and reopen menus. To completely unload,
close OBS and remove just `plugins/obs-ui-augment/bin/64bit/obs-ui-augment.so`
from its user config directory. The rules file can be preserved.
