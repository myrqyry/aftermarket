# CI and package verification

The initial Linux CI pilot adapts the pinned OBS SDK build from Framerot
(commit 16d1ece8), the strict test gate and version-specific build approach
from Timeline, and the package/content integrity checks from both.
The two pilot repositories carry self-contained copies so neither needs
access to another private repository at build time. Promote a shared,
versioned workflow only after the pilots qualify.

## Verification stages

1. Check common relative Markdown links and HTML image sources in README.
   These checks do not validate remote URLs or heading anchors.
2. Download OBS 32.2.0 source and verify its pinned SHA-256 before
   building and installing the SDK into runner-local storage.
3. Configure a clean Release build with explicit SDK CMake package paths.
4. Require the native CTest suite to register at least the baseline number
   of tests and pass under Xvfb/software rendering without skipped tests.
5. Stage the module via CMake install, enforce the exact plugin module and
   required third-party notice payload, reject embedded runtime search paths,
   and check shared libraries.
6. Create a temporary archive, verify its SHA-256, extract it separately,
   and compare the extracted module and notice against their expected contents.

The workflow runs on pull requests, pushes to main/master, and manual
dispatch. It has read-only repository permissions and no automatic
release or binary upload. The checksum is recorded in the run summary.
A passing job establishes the tested Linux SDK/build/package path, not
interactive OBS behavior, GPU-driver coverage, or cross-platform support.
No user OBS profile is modified by the verifier.

## Reproduce on a developer machine

Run from the project root with build dependencies installed. For the
exact CI SDK, use an empty temporary directory and run:

    bash .github/scripts/prepare-obs32-sdk.sh /tmp/plugin-obs32-sdk

Then run the project-specific verifier (see the workflow for the module
name and baseline test count) against /tmp/plugin-obs32-sdk/sdk and a
new empty work directory. You may instead pass a matching local SDK prefix
such as /usr/local; that result qualifies the installed local SDK only.
Keep the live OBS application and profile separate from build staging.

## Local qualification — 2026-11-24

The pinned OBS 32.2.0 SDK was downloaded, checksum-verified and built
from source. A clean Release build of the committed source plus this
CI change passed all 32 CTest cases, the module-only package allowlist,
shared-library checks, SHA-256 verification and extraction comparison.
The original 32-test SDK/package qualification above is historical, not a
claim that the latest package was requalified against that SDK.

## Current qualification — 2026-11-25

The local build at `f111fbe` passed all **44/44 CTests** using the installed
OBS 32.2.0 SDK and Qt 6.4.2. The CI minimum now follows the expanded
46-test baseline; the gate runs every registered test and rejects skipped
or failing tests.

The latest completed hosted run before this documentation refresh,
[45740037866](https://github.com/myrqyry/aftermarket/actions/runs/45740037866)
at `58df432`, successfully prepared the pinned SDK and built the module,
but failed two of 44 tests:

- `command-center`: line 31, launcher visibility assertion.
- `menu-augmentation`: line 363, popup anchor-distance assertion.

The package/checksum stages did not complete. Neither test is waived.
The launcher test reproduced the same assertion locally with Qt font DPI 72:
its fixture had not guaranteed the 20px minimum menu-bar height. It now
provides a suitable height and explicitly checks hiding and restoring the
launcher on short menu bars, including a separate 72-DPI CTest case. The popup test runs on Qt XCB inside a dedicated
1280x1024 Xvfb display (requires `xvfb` and `xauth`), rather than forcing
Qt offscreen, and checks anchoring against the actual space on each side.
Other offscreen widget tests keep their existing backend.
A local Release verification of the CI fix on the `1011155` baseline
passed all 45 CTests, the installed local SDK dependency checks, and the
module-only archive checksum/extraction checks. This was not a hosted
verification or a release.
Hosted run [45762058216](https://github.com/myrqyry/aftermarket/actions/runs/45762058216)
passed at `2a56611` (45 tests, before loudness integration). The combined
loudness package has a separate 46-test qualification baseline and requires
the bundled libebur128 notice at `aftermarket/data/THIRD_PARTY_NOTICES.txt`.
Local Release verification against the installed SDK passed all 46 CTests
and both staged/extracted file checks, including the notice. Its hosted
qualification is separate.
A new push to main/master triggers the existing workflow, not a release.

Run local relative-link checks for the maintained documentation with:

    python3 .github/scripts/test-readme-links.py
    for doc in README.md docs/*.md .meristem/PROJECT.md ACTIVE_WORK.md; do
      python3 .github/scripts/check_readme_links.py "$doc"
    done

These checks cover relative file targets, not remote sites or every
heading anchor.

The temporary archives are intentionally not release packages. Public
redistribution requires the owner to establish licensing, provenance and
platform qualification first. Never publish local logs, scenes, models,
private development notes, or third-party executables by default.
