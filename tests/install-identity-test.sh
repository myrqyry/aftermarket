#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
export XDG_CONFIG_HOME="$tmp/config"
export XDG_DATA_HOME="$tmp/data"
build="$tmp/build"
cfg="$XDG_CONFIG_HOME/obs-studio"
old="$cfg/plugins/obs-ui-augment"
new="$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
mkdir -p "$build" "$old/bin/64bit" "$cfg/plugin_config/obs-ui-augment"
printf 'old module\n' > "$old/bin/64bit/obs-ui-augment.so"
printf 'new module\n' > "$build/aftermarket.so"
printf 'settings\n' > "$cfg/plugin_config/obs-ui-augment/rules.json"
bash "$root/scripts/install.sh" "$build"
cmp "$build/aftermarket.so" "$new"
cmp "$root/third_party/libebur128/THIRD_PARTY_NOTICES.txt" "$cfg/plugins/aftermarket/data/THIRD_PARTY_NOTICES.txt"
test ! -e "$old"
test "$(cat "$cfg/plugin_config/obs-ui-augment/rules.json")" = settings
test ! -e "$cfg/plugin_config/aftermarket"
find "$XDG_DATA_HOME/aftermarket/rollback" -name obs-ui-augment.so -exec grep -Fx 'old module' {} \; | grep -Fx 'old module'
# Reinstall is idempotent, and does not need a legacy installation.
bash "$root/scripts/install.sh" "$build"
cmp "$build/aftermarket.so" "$new"
cmp "$root/third_party/libebur128/THIRD_PARTY_NOTICES.txt" "$cfg/plugins/aftermarket/data/THIRD_PARTY_NOTICES.txt"
# Missing new binary cannot remove the working installation.
mv "$build/aftermarket.so" "$build/hold"
if bash "$root/scripts/install.sh" "$build"; then exit 1; fi
cmp "$build/hold" "$new"
echo 'Installation identity migration verified'
