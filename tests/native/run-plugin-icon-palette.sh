#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
module="$(realpath "${1:-$repo/build/aftermarket.so}")"
installed="${2:-$HOME/.config/obs-studio}"
root=$(mktemp -d /tmp/am-palette-native.XXXXXX)
echo "Evidence: $root"
g++ -std=c++17 -fPIC -shared "$repo/tests/native/plugin-icon-palette-probe.cpp" -o "$root/probe.so" $(pkg-config --cflags --libs Qt6Widgets Qt6Svg) -I/usr/include/obs -lobs -lobs-frontend-api
for theme in Gruvbox RosePineDawn.Pine; do
 home="$root/$theme";cfg="$home/.config/obs-studio"
 mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/icon-probe/bin/64bit" "$cfg/plugin_config/aftermarket"
 cp "$module" "$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
 cp "$root/probe.so" "$cfg/plugins/icon-probe/bin/64bit/icon-probe.so"
 cp -a "$installed/plugins/source-clone" "$installed/plugins/emulens" "$cfg/plugins/"
 cp -a "$installed/themes" "$cfg/"
 printf '[General]\nFirstRun=true\nConfirmOnExit=false\nEnableAutoUpdates=false\n[Appearance]\nTheme=com.myrqyry.Colorway.%s\n[BasicWindow]\nShowSourceIcons=true\n' "$theme" >"$cfg/user.ini"
 printf '{"enabled":true,"motion":false,"rules":[]}\n' >"$cfg/plugin_config/aftermarket/rules.json"
 set +e
 timeout 35s xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$home" XDG_CONFIG_HOME="$home/.config" XDG_CACHE_HOME="$home/.cache" XDG_DATA_HOME="$home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 PROBE_DIR="$home" obs --multi --disable-shutdown-check --no-updater >"$home/obs.log" 2>&1
 rc=$?
 set -e
 echo "$theme OBS_EXIT=$rc"
 grep PALETTECHECK "$home/obs.log" || true
 [[ "$rc" == 0 ]]
 grep -q 'PALETTECHECK initial source=1 filter=1' "$home/obs.log"
 grep -q 'PALETTECHECK live source=1 filter=1' "$home/obs.log"
done
