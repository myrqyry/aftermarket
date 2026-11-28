import importlib.util
import os
from pathlib import Path
import plistlib
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,os.environ["OBS_CI_SCRIPTS"])
spec=importlib.util.spec_from_file_location("hook",Path(__file__).with_name("ci-native.py"))
hook=importlib.util.module_from_spec(spec); spec.loader.exec_module(hook)
class Test(unittest.TestCase):
    def test_restart_helper_signing_order(self):
        helper=Path("aftermarket.plugin/Contents/MacOS/aftermarket-restart-helper")
        for target in ("macos-arm64", "macos-x86_64"):
            with self.subTest(target=target):
                commands=[]
                hook.sign_restart_helper(helper,target,run=lambda *args: commands.append(args))
                self.assertEqual(commands, [
                    tuple(hook.sdk.mac_arch_command(helper,target)),
                    ("otool", "-L", helper),
                    ("codesign", "--force", "--sign", "-", helper),
                    ("codesign", "--verify", "--strict", helper)])
                commands.clear()
                def reject(*args):
                    commands.append(args)
                    raise RuntimeError("architecture failed")
                with self.assertRaisesRegex(RuntimeError,"architecture failed"):
                    hook.sign_restart_helper(helper,target,run=reject)
                self.assertEqual(len(commands),1)

    def test_package_layouts(self):
        for target in ("windows-x64","macos-arm64","macos-x86_64"):
            for version in ("32.2.0","33.0.3"):
                with self.subTest(target=target,version=version), tempfile.TemporaryDirectory() as tmp:
                    work=Path(tmp)
                    installed=work/"installed"
                    if target.startswith("macos"):
                        binary=installed/"aftermarket.plugin/Contents/MacOS/aftermarket"
                        data=binary.parent.parent/"Resources"
                        binary.parent.mkdir(parents=True)
                        (binary.parent.parent/"Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable":"aftermarket","CFBundlePackageType":"BNDL"}))
                    else:
                        binary=installed/("bin/64bit" if version=="32.2.0" else "")/"aftermarket.dll"
                        data=installed/"data"
                        binary.parent.mkdir(parents=True)
                    binary.write_bytes(b"module")
                    helper=binary.with_name("aftermarket-restart-helper" + (".exe" if target=="windows-x64" else ""))
                    helper.write_bytes(b"helper")
                    helper.chmod(0o755)
                    data.mkdir()
                    (data/"LICENSE").write_bytes((ROOT/"LICENSE").read_bytes())
                    notice=data/"THIRD_PARTY_NOTICES.txt"
                    notice.write_bytes((ROOT/"third_party/libebur128/THIRD_PARTY_NOTICES.txt").read_bytes())
                    signing=[]
                    out=hook.stage(ROOT,installed,work/"package",target,version,
                                   sign_helper=lambda *args: signing.append(args))
                    self.assertEqual(signing, [(out.with_name(helper.name), target)]
                                     if target.startswith("macos-") else [])
                    self.assertEqual(out.read_bytes(),b"module")
                    self.assertTrue(out.is_file())
                    packaged=out.with_name(helper.name)
                    self.assertEqual(packaged.read_bytes(),b"helper")
                    helper.unlink()
                    with self.assertRaisesRegex(ValueError,"Missing or invalid restart helper"):
                        hook.stage(ROOT,installed,work/"missing-helper",target,version)
                    helper.write_bytes(b"helper")
                    helper.chmod(0o755)
                    extra=helper.with_name("unexpected")
                    extra.write_bytes(b"extra")
                    with self.assertRaisesRegex(ValueError,"Unexpected native package files"):
                        hook.stage(ROOT,installed,work/"extra-file",target,version)
                    extra.unlink()
                    (data/"LICENSE").unlink()
                    with self.assertRaisesRegex(ValueError,"Missing or changed asset: LICENSE"):
                        hook.stage(ROOT,installed,work/"missing-license",target,version)
if __name__=="__main__":unittest.main()
