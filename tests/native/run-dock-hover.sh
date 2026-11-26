#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
module="$(realpath "${1:-$repo/build/aftermarket.so}")"
root=$(mktemp -d /tmp/am-dock-hover.XXXXXX)
echo "Evidence: $root"
cfg="$root/home/.config/obs-studio"
mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/dock-hover/bin/64bit" "$cfg/plugin_config/aftermarket"
cp "$module" "$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
cp -a "$HOME/.config/obs-studio/themes" "$cfg/"
printf '[General]\nFirstRun=true\nConfirmOnExit=false\n[Appearance]\nTheme=com.myrqyry.Colorway.DarkHorizon\nFontScale=10\nDensity=-2\n' >"$cfg/user.ini"
printf '{"enabled":true,"motion":false,"rules":[],"mixerBackside":true}\n' >"$cfg/plugin_config/aftermarket/rules.json"
g++ -std=c++17 -fPIC -shared "$repo/tests/native/dock-hover-probe.cpp" -o "$cfg/plugins/dock-hover/bin/64bit/dock-hover.so" $(pkg-config --cflags --libs Qt6Widgets) -I/usr/include/obs -lobs -lobs-frontend-api
set +e
timeout 35s xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config" XDG_CACHE_HOME="$root/home/.cache" XDG_DATA_HOME="$root/home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 PRESET_ROOT="$root" obs --multi --disable-shutdown-check --no-updater >"$root/obs.log" 2>&1
rc=$?
set -e
echo "OBS_EXIT=$rc"
grep HOVERCHECK "$root/obs.log" || true
[[ "$rc" == 0 ]]
[[ $(grep -c 'HOVERCHECK .*=[1]$' "$root/obs.log") == 16 ]]
! grep -q 'HOVERCHECK .*=[0]$' "$root/obs.log"
