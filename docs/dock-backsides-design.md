# Aftermarket: dock faces, header controls, and native audio patchbay

Status: **historical design proposal** (2026-11-21). Retained for provenance, not current product instructions. AudioBus and native dock headers are implemented; the [current user guide](user-guide.md#mixer-and-audiobus) supersedes this proposal, including optional Audio Monitor device sends.
Owner: `obs-ui-augment/`, project-facing name Aftermarket.

## Intent and accepted direction

Mat wants existing OBS docks to have a reversible *front* (their original native content) and
a *back* (specialized or user-defined workspace), operated from concise dock header icons.
Header controls may also display meaningful status, switch between tabified docks, and change
dock sizes. The first specialized back is a **single visual Audio Mixer patchbay** for
native OBS source-to-track (1-6) and monitoring assignments. OBS already provides the
Advanced Audio Properties matrix, so Aftermarket should **not duplicate it**; keep that
native dialog accessible for detailed settings. Reuse OBS data and controls, not its
window's QWidget instances. **No new audio routing backend, signal graph, or
external patching** is wanted.

Preserve OBS's original ownership, controls, audio engine, tab bars, dock moves and layout.
This is an opt-in Aftermarket presentation/interaction layer, not an OBS fork.

## Existing relevant seams (inspected 2026-11-21)

- `src/panels.cpp` manages session-only dock collapse/restore and a movable native Preview
  dock; `Panels::canCollapse` already avoids tabified/floating docks.
- `src/obs-audio.cpp` obtains live OBS audio sources and reads meters; it does not
  currently modify routing.
- The installed libobs SDK exposes `obs_source_get_audio_mixers`,
  `obs_source_set_audio_mixers`, `obs_source_get_monitoring_type` and
  `obs_source_set_monitoring_type`.
- Native **Advanced Audio Properties** already lays out per-source track assignments,
  monitoring, levels and further audio controls. Keep that OBS-owned dialog intact; its
  UI is a behavior/reference seam rather than movable content to reparent into a dock.
- Existing `Panels` and `tests/panels-test.cpp` are suitable regression boundaries.
- Working branch `main` was clean and one local commit ahead of origin on inspection.
  The design has not been runtime-tested.

## Approaches considered

1. **Replace QDockWidget native title bars with custom icon-rich title bars.**
   Compact when standalone, but tabified layouts may hide the title bar and native
   dragging/floating/double-click semantics need special handling. Not recommended for v1.
2. **Embed a slim Aftermarket control strip at the top of the dock's content.**
   The native title/tab bar remains untouched. The strip stays available within an
   active tab, floating dock or ordinary dock. Costs a small amount of vertical space.
   **Recommended first version.**
3. **Customize QMainWindow's tab bar or overlay dock chrome.** Most visually integrated,
   but couples to Qt internals, heterogeneous dock groups and style/geometry lifecycles.
   Consider only after a proven stable public-API foundation.

## Behavior proposal

A `DockFaces` coordinator registers eligible native QDockWidgets by stable object identity
(not translated display title), remembers original widget and geometry constraints, and
hosts a compact header plus a front/back page selection. The native front widget is moved,
never cloned, and remains the real OBS control surface. The back page belongs to Aftermarket.
A dock can register a built-in back provider or a user-defined composition. Flipping does
not change OBS source state and does not itself resize the dock.

Header actions, compact and consistent:
- **Face toggle:** flip to back/front, with clear state and keyboard/tooltip equivalent.
- **Tab previous/next:** if in a tabbed group, raise an existing sibling dock rather than
  manufacturing a new tab system. Native tabs stay available; omit controls when ungrouped.
- **Size preset:** compact / normal / expanded; on docked windows use Qt
  `QMainWindow::resizeDocks`, understanding that a tab group/neighbor geometry moves with it.
  On floating docks resize the floating window within screen bounds. Retain a per-session
  previous size for return, but do not promise pixel-exact restoration.
- **Status slots:** only for real source/dock state (such as source muted, meter activity or
  route count); icons use tooltips and noncolor cues, and need not be present on all docks.
- **Overflow:** configuration, restore and lower-priority actions; keep normal native close,
  float, move and tab controls, and do not duplicate them needlessly.

Use theme palette roles, compact circular click targets, and restrained optional flip motion.
Respect reduced motion and Aftermarket's existing motion-off behavior. Explicit keyboard
access and accessible names are required. Dock collapse and face switching must not compete;
expand before flip when needed. Tabs keep Qt's native behavior and ownership.

## Backside providers

Built-in back pages are identified by provider IDs, can attach only where supported, and
read/write source truth through public OBS/libobs APIs. Start with `audio-patchbay`.

Keep back providers narrowly registered so a later user-defined backside can supply
safe controls or a supported view without rewriting the dock host. Do not start by building
a generic visual page editor, arbitrary widget reparenting, or a user C++ plugin loader.
Add a real composition UI only once specific user-defined content demands it. Future
providers must obey the same native-state and reversible-lifecycle rules.

Provider activation/deactivation cannot destroy native dock content. On unload, OBS close,
dock destruction, widget replacement or unsupported layout transitions, Aftermarket must
cleanly detach its own page/header and restore the original native widget ownership,
visibility and applicable constraints without overriding later external changes.

## Audio Mixer: visual patchbay over native OBS state

Build **one graphical patchbay**, not a redundant matrix: source nodes connect to the six
track destinations, with a native three-state monitoring selector. The patchbay adapts its
canvas/scrolling to narrow docks rather than silently switching into a second UI. Keep
Advanced Audio Properties available for its existing matrix and details such as sync
offset, mono and balance. Expose a shortcut to OBS's original dialog when its existing
action is reliably found; do not copy/reparent that dialog's QWidgets or duplicate its
state. If public OBS APIs or lifecycle constraints cannot support an interaction safely,
leave it in the native dialog rather than fabricate a workaround.

- Enumerate OBS audio-capable sources using appropriate public source enumeration/ref
  management; show track connectors 1–6. Actual cables reflect each source's
  `obs_source_get_audio_mixers` bit mask.
- Clicking or dragging a source-to-track link toggles only that track's bit using
  `obs_source_set_audio_mixers`; unrelated bits must be preserved. Expose a plain
  checked-state/keyboard alternative to drag-only patching.
- Show monitoring as a **single three-way choice**: Off, Monitor Only, Monitor and Output,
  mapped to `obs_source_get/set_monitoring_type`. Do not depict it as arbitrary independent
  routing cables or imply Monitoring Only is sent to an output track.
- Track labels are 1–6, not 'Stream' or 'Record' unless explicitly derived from actual
  output settings; the patchbay does not modify output settings or choose stream track.
- Refresh while visible and after OBS source changes. Treat OBS as authoritative:
  changes made in Advanced Audio Properties must appear when the patchbay refreshes;
  source rename/removal must not retarget to another source with the same display name.
- Only a deliberate user edit may mutate audio routing. A view open/close/flip must not.
  Persist through OBS's ordinary scene collection behavior, not a separate speculative
  Aftermarket routing store.

## Non-goals of this slice

No PipeWire/JACK/virtual-audio backend, generic bus creation, filter-chain rewiring,
audio DSP, OBS tab-bar replacement, hijacking native title drag, fully arbitrary widget
reparenting, or broad theme redesign.

## Verification and rollout

Implement in small TDD slices after design review:
1. Reversible face host and compact header; prove native widget identity, action behavior,
   restore on unload/close, tab group navigation, collapse interaction, and floating.
2. Size presets and per-session restore, including group and screen edge cases.
3. Native OBS Patchbay read-only view matched against real Advanced Audio Properties;
   then add deliberate track and monitoring edits using public APIs. Test preserved
   unrelated bits and source identity with regressions, and verify both directions
   of native-dialog/patchbay synchronization in isolated OBS.
4. Add the smallest provider registration seam for specialized and user-defined backs.
   Build a user-facing editor only when the first custom use case requires one; retain
   atomic reversible configuration and an explicit recovery path.

Validate with Qt offscreen regressions **and** an isolated real OBS profile including tabs,
floating docks, audio sources, Advanced Audio Properties synchronization, OBS restart
and teardown. Do not treat a compiled plugin or fake UI test as evidence of live routing.
Never mutate the user's running OBS/profile as part of verification.

## Unresolved product choices

- Whether size presets are generic or editable per dock. Proposed default: three presets
  with session-only dimensions first.
- Minimum set of primitives in a user-defined back. Proposed default: registered action
  buttons, status indicators, labels and read-only value widgets, expanded by evidence.
- Whether face selection persists across OBS restarts. Proposed default: enabled dock
  providers persist, active face remains session-only initially.
