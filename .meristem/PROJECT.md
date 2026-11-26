# Aftermarket

Purpose: a plugin-owned, rule-driven runtime UI modification engine for native
interfaces, currently supporting OBS Studio. Preserve native actions and source
state; extend presentation, animation, control forms and layout. Complement
Colorway without requiring it. Keep the core independent of OBS where practical,
without premature rewrites.

Identity: module and config directory use aftermarket; migration preserves the
previous obs-ui-augment configuration and package. Local checkout remains
obs-ui-augment to preserve existing workspace references. No OBS fork or video effects.
Current capabilities include native mixer/AudioBus faces, responsive
property forms, source/filter presets, native command/search surfaces,
theme-aware plugin icons and private contextual notes on scene/source rows.
The local runtime target is OBS 32.2.0 / Qt 6.4.2 / Linux X11.
See README.md and docs/user-guide.md for current behavior, docs/ci.md
for qualification boundaries, and ACTIVE_WORK.md for historical receipts.
Local success does not imply hosted CI or cross-platform qualification.
