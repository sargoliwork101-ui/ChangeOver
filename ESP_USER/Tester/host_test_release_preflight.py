#!/usr/bin/env python3
"""Ensure the mandatory gate cannot silently skip unavailable or unpinned dependencies."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
RUNNER = Path(__file__).with_name("run_release_tests.sh")
class ReleasePreflight(unittest.TestCase):
    def invoke(self, **changes):
        env = dict(os.environ)
        env.update(changes)
        return subprocess.run(["bash", str(RUNNER), "--preflight"], env=env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
    def test_valid_dependencies_only_mean_preflight(self):
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("tests not run", result.stdout)
        self.assertNotIn("ALL MANDATORY", result.stdout)
    def test_missing_browser_is_not_success(self):
        result = self.invoke(CHROMIUM_PATH="/nonexistent/changeover-chromium")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("CHROMIUM_PATH", result.stdout)
        self.assertNotIn("PASS", result.stdout)
    def test_missing_flash_source_is_not_success(self):
        result = self.invoke(LITTLEFS_SOURCE="/nonexistent/changeover-littlefs")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("LITTLEFS_SOURCE", result.stdout)
        self.assertNotIn("PASS", result.stdout)
    def test_different_flash_source_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="pack-preflight-") as temp:
            for name in ("lfs.c", "lfs.h", "lfs_util.c", "lfs_util.h"):
                shutil.copyfile(Path(os.environ["LITTLEFS_SOURCE"]) / name, Path(temp) / name)
            with (Path(temp) / "lfs.c").open("a") as out:
                out.write("\n/* Modified test input. */\n")
            result = self.invoke(LITTLEFS_SOURCE=temp)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("pinned version", result.stdout)
        self.assertNotIn("PASS", result.stdout)
if __name__ == "__main__":
    unittest.main()
