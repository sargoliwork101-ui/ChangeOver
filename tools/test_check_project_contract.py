#!/usr/bin/env python3
"""Regression tests for project contract checker coverage and failures."""
import contextlib
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path
from tools import strip_fa_comments as STRIPPER
CHECKER_PATH = Path(__file__).with_name("check_project_contract.py")
SPEC = importlib.util.spec_from_file_location("check_project_contract", CHECKER_PATH)
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)
class ProjectContractCheckerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.old_root = CHECKER.ROOT
        CHECKER.ROOT = str(self.root)
        (self.root / "Board").mkdir()
        (self.root / "support").mkdir()
        (self.root / "README.md").write_text("support docs\n", encoding="utf-8")
        (self.root / "Board" / "CONTRACT.md").write_text(
            '```contract json\n{"version":1,"project":"B","root":"Board",'
            '"http_routes":["GET /"]}\n```\n', encoding="utf-8")
        (self.root / "Board" / "app.ino").write_text(
            'server.on("/", HTTP_GET, handler);\n', encoding="utf-8")
        (self.root / "Board" / "interface.h").write_text(
            "#define RATE 5u\n", encoding="utf-8")
        self.manifest = {
            "version": 1,
            "projects": {"B": {"root": "Board", "contract": "Board/CONTRACT.md",
                                 "route_source": "Board/app.ino", "locked_symbols": [
                                     {"file": "Board/interface.h", "defines": {"RATE": "5u"}}]}},
            "outside_directories": ["support"],
            "outside_files": ["README.md"],
            "ignored_root_entries": [],
        }
    def tearDown(self):
        CHECKER.ROOT = self.old_root
        self.temp.cleanup()
    def run_check(self, manifest=None):
        with contextlib.redirect_stdout(io.StringIO()):
            return CHECKER.check(manifest or self.manifest)
    def test_matching_contract_and_source_pass(self):
        self.assertEqual(self.run_check(), 0)
    def test_unclassified_root_entry_fails(self):
        (self.root / "unlisted").mkdir()
        self.assertEqual(self.run_check(), 1)
    def test_http_route_mismatch_fails(self):
        (self.root / "Board" / "app.ino").write_text(
            'server.on("/different", HTTP_GET, handler);\n', encoding="utf-8")
        self.assertEqual(self.run_check(), 1)
    def test_locked_constant_mismatch_fails(self):
        (self.root / "Board" / "interface.h").write_text(
            "#define RATE 6u\n", encoding="utf-8")
        self.assertEqual(self.run_check(), 1)
    def test_unsafe_project_root_fails(self):
        bad = json.loads(json.dumps(self.manifest))
        bad["projects"]["B"]["root"] = "../outside"
        self.assertEqual(self.run_check(bad), 1)
class StripFaCommentsExitCodeTests(unittest.TestCase):
    def test_check_mode_fails_when_rewrite_is_needed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.py"
            path.write_text("value = 1\n\n", encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                result = STRIPPER.main([str(path)])
            self.assertEqual(result, 1)
    def test_check_mode_passes_after_apply(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.py"
            path.write_text("value = 1\n\n", encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(STRIPPER.main(["--apply", str(path)]), 0)
                self.assertEqual(STRIPPER.main([str(path)]), 0)
if __name__ == "__main__":
    unittest.main()
