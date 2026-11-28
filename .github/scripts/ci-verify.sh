#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
[[ "$MODULE" == aftermarket && "$PUBLIC_NAME" == aftermarket ]]
# Use the revision-verified executable restored by the shared workflow.
engine="${OBS_CI_ENGINE_ROOT:?Missing verified shared engine}"
pin="${ENGINE_REVISION:?Missing immutable shared engine revision}"
[[ "$(git -C "$engine" rev-parse HEAD)" == "$pin" ]]
export OBS_CI_SCRIPTS="$engine/scripts"
python "$root/.github/scripts/test-native-caller.py"
if [[ "$TARGET" != linux-x86_64 ]]; then
    exec python "$engine/scripts/native_caller.py" "$root"
fi
# Shared artifact identity is versioned. The standalone verifier also
# retains its historical local OBS32 filename for backward compatibility.
[[ "$ARCHIVE" == "aftermarket-linux-x86_64-obs$OBS_VERSION-test.tar.gz" ]] || {
    echo "Expected aftermarket-linux-x86_64-obs$OBS_VERSION-test.tar.gz, got $ARCHIVE" >&2
    exit 1
}
if [[ "${OBS_CI_LINUX_SDK_CACHE:-false}" == true ]]; then
    [[ -z "${AFTERMARKET_LOCAL_SDK:-}" ]]
    bash "$engine/scripts/run-linux-caller.sh" "$root" --sdk-only
    sdk="$RUNNER_TEMP/caller-work/obs/sdk"
else
    sdk="${AFTERMARKET_LOCAL_SDK:-$RUNNER_TEMP/obs-sdk/sdk}"
    if [[ -z "${AFTERMARKET_LOCAL_SDK:-}" ]]; then
        bash "$engine/scripts/prepare-linux-sdk.sh" "$RUNNER_TEMP/obs-sdk" "$OBS_VERSION"
    else
        [[ "${GITHUB_ACTIONS:-}" != true ]]
    fi
fi
bash "$root/.github/scripts/verify-linux.sh" "$sdk" "$RUNNER_TEMP/plugin-ci" "$MODULE" "$MINIMUM_TESTS"
"$engine/target/release/obs-ci" inspect-module "$RUNNER_TEMP/plugin-ci/$ARCHIVE" --module "$MODULE" --target "$TARGET" --obs-version "$OBS_VERSION"
