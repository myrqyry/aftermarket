#!/usr/bin/env bash
# Preserve the existing verified 46-test, notice and extracted-package gate.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
[[ "$MODULE" == aftermarket && "$PUBLIC_NAME" == aftermarket && "$TARGET" == linux-x86_64 && "$OBS_VERSION" == 32.2.0 ]]
[[ "$ARCHIVE" == aftermarket-linux-x86_64-obs32.2.0-test.tar.gz ]]
sdk="${AFTERMARKET_LOCAL_SDK:-$RUNNER_TEMP/obs-sdk/sdk}"
if [[ -z "${AFTERMARKET_LOCAL_SDK:-}" ]]; then
    bash "$root/.github/scripts/prepare-obs32-sdk.sh" "$RUNNER_TEMP/obs-sdk"
else
    [[ "${GITHUB_ACTIONS:-}" != true ]]
fi
exec bash "$root/.github/scripts/verify-linux.sh" "$sdk" "$RUNNER_TEMP/plugin-ci" "$MODULE" "$MINIMUM_TESTS"
