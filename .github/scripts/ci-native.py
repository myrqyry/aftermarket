import os
import shutil
from pathlib import Path
from native_caller import layout, verify_payload
import native_sdk as sdk

MODULE = "aftermarket"

def configure(root, work, target, version):
    # Build the complete plugin. Linux remains the native UI runtime test
    # gate; POSIX-specific installer and GUI tests need separate native ports.
    return ["-DBUILD_TESTING=OFF",
            "-DAFTERMARKET_PACKAGE_LAYOUT=" + ("legacy" if version=="32.2.0" else "modern")]

def sign_restart_helper(helper, target):
    # Intel executables are not automatically ad-hoc signed like arm64.
    # Sign the explicit nested executable before the shared driver signs
    # the enclosing plugin, never use blanket codesign --deep signing.
    sdk.run(*sdk.mac_arch_command(helper, target))
    sdk.run("otool", "-L", helper)
    sdk.run("codesign", "--force", "--sign", "-", helper)
    sdk.run("codesign", "--verify", "--strict", helper)

def stage(root, installed, payload, target, version):
    binary, data, notice = layout(payload, MODULE, target, version)
    # The shared verifier checks the exact original module/assets allowlist.
    # Validate the known auxiliary executable separately.
    mac = target.startswith("macos-")
    source_root = installed / "aftermarket.plugin" if mac else installed
    destination_root = payload / ("aftermarket.plugin" if mac else MODULE)
    helper_name = "aftermarket-restart-helper" + (".exe" if target == "windows-x64" else "")
    helper = binary.with_name(helper_name)
    source_helper = source_root / helper.relative_to(destination_root)
    if (source_helper.is_symlink() or not source_helper.is_file()
            or source_helper.stat().st_size == 0
            or (mac and not os.access(source_helper, os.X_OK))):
        raise ValueError("Missing or invalid restart helper")
    if source_root.is_symlink() or any(p.is_symlink() for p in source_root.rglob("*")):
        raise ValueError("Unexpected symlink")
    def ignore_helper(directory, names):
        return [helper_name] if Path(directory) == source_helper.parent and helper_name in names else []
    shutil.copytree(source_root, destination_root, ignore=ignore_helper)
    expected = (root/"third_party/libebur128/THIRD_PARTY_NOTICES.txt").read_bytes()
    verified = verify_payload(payload, MODULE, target, version,
        {"THIRD_PARTY_NOTICES.txt": expected, "LICENSE": (root/"LICENSE").read_bytes()}, None)
    shutil.copy2(source_helper, helper)
    if helper.read_bytes() != source_helper.read_bytes():
        raise ValueError("Restart helper changed during staging")
    if mac:
        sign_restart_helper(helper, target)
    return verified
