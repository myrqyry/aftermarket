import importlib.util
import os
from pathlib import Path
import plistlib
import sys
import tempfile
import unittest
from unittest.mock import patch, call
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,os.environ["OBS_CI_SCRIPTS"])
spec=importlib.util.spec_from_file_location("hook",Path(__file__).with_name("ci-native.py"))
hook=importlib.util.module_from_spec(spec); spec.loader.exec_module(hook)
class Test(unittest.TestCase):
    def test_restart_helper_signing_order(self):
        helper=Path("aftermarket.plugin/Contents/MacOS/aftermarket-restart-helper")
        for target in ("macos-arm64", "macos-x86_64"):
            with self.subTest(target=target), patch.object(hook.sdk, "run") as run:
                hook.sign_restart_helper(helper,target)
                self.assertEqual(run.call_args_list, [
                    call(*hook.sdk.mac_arch_command(helper,target)),
                    call("otool", "-L", helper),
                    call("codesign", "--force", "--sign", "-", helper),
                    call("codesign", "--verify", "--strict", helper)])
                run.reset_mock()
                run.side_effect=RuntimeError("architecture failed")
                with self.assertRaisesRegex(RuntimeError,"architecture failed"):
                    hook.sign_restart_helper(helper,target)
                self.assertEqual(run.call_count,1)

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
                    with patch.object(hook, "sign_restart_helper") as sign:
                        out=hook.stage(ROOT,installed,work/"package",target,version)
                        if target.startswith("macos-"):
                            sign.assert_called_once_with(out.with_name(helper.name), target)
                        else:
                            sign.assert_not_called()
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
