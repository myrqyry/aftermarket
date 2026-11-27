#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
module="$(realpath "${1:-$repo/build/aftermarket.so}")"
root=$(mktemp -d /tmp/am-title-gradient.XXXXXX)
echo "Evidence: $root"
cfg="$root/home/.config/obs-studio"
mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/title-gradient-probe/bin/64bit" "$cfg/plugin_config/aftermarket"
cp "$module" "$cfg/plugins/aftermarket/bin/64bit/aftermarket.so"
printf '[General]\nFirstRun=true\n' >"$cfg/user.ini"
pattern="${GRADIENT_PATTERN:-shimmer}"
case "$pattern" in flow|reverse|bounce|breathe|shimmer|drift|aurora|tide|bloom|orbit|nebula|vortex|ripple) ;; *) echo "Unknown pattern" >&2; exit 1;; esac
scale="${GRADIENT_SCALE:-100}"
[[ "$scale" =~ ^[0-9]{2,3}$ ]] && ((scale>=50&&scale<=400)) || exit 1
printf '{"enabled":true,"motion":true,"rules":[],"gradient":{"enabled":true,"sliders":false,"progress":false,"titlebars":true,"pattern":"%s","period":1200,"scale":%s,"colors":["#31748f","#c4a7e7"]}}\n' "$pattern" "$scale" >"$cfg/plugin_config/aftermarket/rules.json"
if [[ "${FULL_HEADER:-0}" == 1 ]]; then
 cp -a "$HOME/.config/obs-studio/themes" "$cfg/"
 printf '\n[Appearance]\nTheme=com.myrqyry.Colorway.FluffyDark\nFontScale=10\nDensity=-2\n' >>"$cfg/user.ini"
 sed -i 's/"titlebars":true/"titlebars":false,"titlebarFill":true/' "$cfg/plugin_config/aftermarket/rules.json"
fi
g++ -std=c++17 -fPIC -shared "$repo/tests/native/title-gradient-probe.cpp" -o "$cfg/plugins/title-gradient-probe/bin/64bit/title-gradient-probe.so" $(pkg-config --cflags --libs Qt6Widgets) -I/usr/include/obs -lobs -lobs-frontend-api
set +e
timeout 50s nice -n 10 xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config" XDG_CACHE_HOME="$root/home/.cache" XDG_DATA_HOME="$root/home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 PRESET_ROOT="$root" obs --multi --disable-shutdown-check --no-updater >"$root/obs.log" 2>&1
rc=$?
set -e
echo "OBS_EXIT=$rc"
grep -E "GRADIENTCHECK" "$root/obs.log" || true
[[ "$rc" == 0 ]]
[[ $(grep -c 'GRADIENTCHECK .*=[1]$' "$root/obs.log") == 8 ]]
! grep -q 'GRADIENTCHECK .*=[0]$' "$root/obs.log"
