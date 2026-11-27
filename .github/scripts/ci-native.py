import shutil
from native_caller import layout, verify_payload

MODULE = "aftermarket"

def configure(root, work, target, version):
    # Build the complete plugin. Linux remains the native UI runtime test
    # gate; POSIX-specific installer and GUI tests need separate native ports.
    return ["-DBUILD_TESTING=OFF",
            "-DAFTERMARKET_PACKAGE_LAYOUT=" + ("legacy" if version=="32.2.0" else "modern")]

def stage(root, installed, payload, target, version):
    binary, data, notice = layout(payload, MODULE, target, version)
    if target.startswith("macos-"):
        shutil.copytree(installed/"aftermarket.plugin", payload/"aftermarket.plugin")
    else:
        shutil.copytree(installed, payload/MODULE)
    expected = (root/"third_party/libebur128/THIRD_PARTY_NOTICES.txt").read_bytes()
    return verify_payload(payload,MODULE,target,version,{"THIRD_PARTY_NOTICES.txt":expected,"LICENSE":(root/"LICENSE").read_bytes()},None)
