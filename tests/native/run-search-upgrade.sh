#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
am="$(realpath "$1")"
unscene="$(realpath "$2")"
fixture="$(realpath "$3")"
root=$(mktemp -d /tmp/am-search-upgrade.XXXXXX)
echo "Evidence: $root"
cfg="$root/home/.config/obs-studio"
mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/unscene/bin/64bit" "$cfg/plugins/search-probe/bin/64bit" "$cfg/plugin_config/aftermarket"
cp "$am" "$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
cp "$unscene" "$cfg/plugins/unscene/bin/64bit/unscene.so"
cp -a "$fixture/basic" "$cfg/"
printf '[General]\nFirstRun=true\nConfirmOnExit=false\n' >"$cfg/user.ini"
printf '{"enabled":true,"motion":false,"rules":[]}\n' >"$cfg/plugin_config/aftermarket/rules.json"
g++ -std=c++17 -fPIC -shared "$repo/tests/native/search-upgrade-probe.cpp" -o "$cfg/plugins/search-probe/bin/64bit/search-probe.so" $(pkg-config --cflags --libs Qt6Widgets) -I/usr/include/obs -lobs -lobs-frontend-api
set +e
timeout 45s xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config" XDG_CACHE_HOME="$root/home/.cache" XDG_DATA_HOME="$root/home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 PRESET_ROOT="$root" obs --multi --disable-shutdown-check --no-updater >"$root/obs.log" 2>&1
rc=$?
set -e
grep 'SEARCHCHECK' "$root/obs.log" || true
echo "OBS_EXIT=$rc"
[[ "$rc" == 0 ]]
[[ $(grep -c 'SEARCHCHECK .*=[1]$' "$root/obs.log") == 13 ]]
! grep -q 'SEARCHCHECK .*=[0]$' "$root/obs.log"
