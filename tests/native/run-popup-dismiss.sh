#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
module="$(realpath "${1:-$repo/build/aftermarket.so}")"
root=$(mktemp -d /tmp/am-popup-native.XXXXXX)
echo "Evidence: $root"
cfg="$root/home/.config/obs-studio"
mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/popup-probe/bin/64bit" "$cfg/plugin_config/aftermarket"
cp "$module" "$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
printf '[General]\nFirstRun=true\n' >"$cfg/user.ini"
g++ -std=c++17 -fPIC -shared "$repo/tests/native/popup-dismiss-probe.cpp" -o "$cfg/plugins/popup-probe/bin/64bit/popup-probe.so" $(pkg-config --cflags --libs Qt6Widgets) -I/usr/include/obs -lobs -lobs-frontend-api -lXtst -lX11
set +e
timeout 28s xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config" XDG_CACHE_HOME="$root/home/.cache" XDG_DATA_HOME="$root/home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 PRESET_ROOT="$root" obs --multi --disable-shutdown-check --no-updater >"$root/obs.log" 2>&1
rc=$?
set -e
echo "OBS_EXIT=$rc"
grep POPUPPROBE "$root/obs.log" || true
[[ "$rc" == 0 ]]
[[ $(grep -c 'POPUPPROBE .*=[1]$' "$root/obs.log") == 11 ]]
! grep -q 'POPUPPROBE .*=[0]$' "$root/obs.log"
