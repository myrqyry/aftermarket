#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build}"
src="$build/obs-ui-augment.so"
[[ -f "$src" ]] || { echo "Build the plugin first: $src" >&2; exit 1; }
config="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio"
dest="$config/plugins/obs-ui-augment/bin/64bit"
mkdir -p "$dest"
temp="$(mktemp "$dest/.obs-ui-augment.XXXXXX")"
trap 'rm -f -- "$temp"' EXIT
install -m 755 "$src" "$temp"
mv -f -- "$temp" "$dest/obs-ui-augment.so"
echo "Installed $dest/obs-ui-augment.so"
echo "Open/restart OBS. Existing rules and themes have not been modified."
