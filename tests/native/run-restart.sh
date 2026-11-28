#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
module="$(realpath "${1:-$repo/build/aftermarket.so}")"
helper="$(dirname "$module")/aftermarket-restart-helper"
test -x "$helper"
root=$(mktemp -d /tmp/am-restart.XXXXXX)
echo "Evidence: $root"
cfg="$root/home/.config/obs-studio"
mkdir -p "$cfg/plugins/aftermarket/bin/64bit" "$cfg/plugins/restart-probe/bin/64bit" "$cfg/plugin_config/aftermarket"
cp "$module" "$helper" "$cfg/plugins/aftermarket/bin/64bit/"
printf '[General]\nFirstRun=true\nConfirmOnExit=false\n' >"$cfg/user.ini"
printf '{"enabled":true,"motion":false,"rules":[]}\n' >"$cfg/plugin_config/aftermarket/rules.json"
g++ -std=c++17 -fPIC -shared "$repo/tests/native/restart-probe.cpp" -o "$cfg/plugins/restart-probe/bin/64bit/restart-probe.so" $(pkg-config --cflags --libs Qt6Widgets) -I/usr/include/obs -lobs -lobs-frontend-api
timeout 55s xvfb-run -a -s '-screen 0 1600x1000x24' env HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config" XDG_CACHE_HOME="$root/home/.cache" XDG_DATA_HOME="$root/home/.local/share" LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 PRESET_ROOT="$root" bash -c '
 set -e
 obs --multi --disable-shutdown-check --no-updater >"$PRESET_ROOT/obs.log" 2>&1
 for i in $(seq 1 100); do [[ -s "$PRESET_ROOT/second.pid" ]] && break; sleep .3; done
 test -s "$PRESET_ROOT/second.pid"
 first=$(cat "$PRESET_ROOT/first.pid"); second=$(cat "$PRESET_ROOT/second.pid")
 [[ "$first" != "$second" ]]
 ! kill -0 "$first" 2>/dev/null
 # Keep Xvfb alive until the new process closes.
 for i in $(seq 1 50); do
  if ! kill -0 "$second" 2>/dev/null || [[ $(ps -o stat= -p "$second") == Z* ]]; then break; fi
  sleep .2
 done
'
test -s "$root/registered"
test -s "$root/cancelled"
test ! -e "$root/failure"
grep -F 'Restart helper launched' "$root/obs.log"
echo "Restart verified: $(cat "$root/first.pid") -> $(cat "$root/second.pid"); cancel and hotkey confirmed"
