#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
module="$(realpath "${1:-$repo/build/aftermarket.so}")"
root=$(mktemp -d /tmp/am-workspace-polish.XXXXXX)
echo "Evidence: $root"
cfg="$root/home/.config/obs-studio"
mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/workspace-probe/bin/64bit" "$cfg/plugin_config/aftermarket"
cp "$module" "$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
cp -a "$HOME/.config/obs-studio/themes" "$cfg/"
printf '[General]\nFirstRun=true\nConfirmOnExit=false\n[Appearance]\nTheme=%s\nFontScale=10\nDensity=-2\n' "${WORKSPACE_TEST_THEME:-com.myrqyry.Colorway.FluffyDark}" >"$cfg/user.ini"
printf '{"enabled":true,"motion":false,"rules":[]}\n' >"$cfg/plugin_config/aftermarket/rules.json"
g++ -std=c++17 -fPIC -shared "$repo/tests/native/workspace-polish-probe.cpp" -o "$cfg/plugins/workspace-probe/bin/64bit/workspace-probe.so" $(pkg-config --cflags --libs Qt6Widgets) -I/usr/include/obs -lobs -lobs-frontend-api
set +e
timeout 35s xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config" XDG_CACHE_HOME="$root/home/.cache" XDG_DATA_HOME="$root/home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 PRESET_ROOT="$root" obs --multi --disable-shutdown-check --no-updater >"$root/obs.log" 2>&1
rc=$?
set -e
echo "OBS_EXIT=$rc"
grep -E 'WORKCHECK|WORKDETAIL' "$root/obs.log" || true
[[ "$rc" == 0 ]]
[[ $(grep -c 'WORKCHECK .*=[1]$' "$root/obs.log") == 7 ]]
! grep -q 'WORKCHECK .*=[0]$' "$root/obs.log"
