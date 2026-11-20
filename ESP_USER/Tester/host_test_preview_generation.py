#!/usr/bin/env python3
"""Exercise preview generation without HTML mirrors or changes to production inputs."""
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
ESP = ROOT / "ESP_USER"
MIRRORS = ("index.html", "simple_panel_clean.html")
class PreviewGeneration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = tempfile.TemporaryDirectory(prefix="esp-preview-generation-")
        cls.addClassCleanup(cls.work.cleanup)
        cls.root = Path(cls.work.name)
        cls.esp = cls.root / "ESP_USER"
        cls.esp.mkdir()
        inputs = list(ESP.glob("*.h")) + list(ESP.glob("*.ino"))
        inputs += [ESP / "make_simple_preview.py", ROOT / "ESP_AGENT_SPEC.md",
                   ROOT / "Firmware/Bsp/Src/bsp_measurement.c"]
        cls.before = {}
        for source in inputs:
            target = cls.root / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            cls.before[target] = source.read_bytes()
        cls.generate()
        cls.first = (cls.esp / "simple_preview.html").read_bytes()
        cls.generate()
        cls.second = (cls.esp / "simple_preview.html").read_bytes()
    @classmethod
    def generate(cls):
        subprocess.run([sys.executable, str(cls.esp / "make_simple_preview.py")],
                       cwd=cls.root, check=True, capture_output=True, timeout=30)
    def test_repeat_generation_is_identical(self):
        self.assertEqual(self.first, self.second)
    def test_checked_in_preview_is_current(self):
        self.assertEqual(self.first, (ESP / "simple_preview.html").read_bytes())
    def test_production_inputs_are_unchanged(self):
        for target, expected in self.before.items():
            with self.subTest(file=target.name):
                self.assertEqual(target.read_bytes(), expected)
    def test_generator_does_not_recreate_mirrors(self):
        for name in MIRRORS:
            with self.subTest(file=name):
                self.assertFalse((self.esp / name).exists())
    def test_repository_has_no_mirrors(self):
        for name in MIRRORS:
            with self.subTest(file=name):
                self.assertFalse((ESP / name).exists())
    def test_production_markup_is_preserved(self):
        source = (ESP / "simple_panel.h").read_text(encoding="utf-8")
        page = source.split('R"rawliteral(', 1)[1].rsplit(')rawliteral"', 1)[0]
        for url in ("calendar", "about", "pack"):
            page = page.replace('<script src="/' + url + '.js" defer></script>', "")
        generated = self.first.decode("utf-8")
        self.assertEqual(generated.count("<!-- SIM_PREVIEW_SHIM_BEGIN"), 1)
        without_shim = re.sub(r"<!-- SIM_PREVIEW_SHIM_BEGIN.*?(?=</head>)", "",
                              generated, count=1, flags=re.S)
        self.assertEqual(without_shim, page)
    def test_shared_assets_are_embedded_exactly_once(self):
        generated = self.first.decode("utf-8")
        for file, tag, attribute in (("charge_calendar.h", "calendar", "charge-calendar"),
                                     ("about_footer.h", "footer", "company-footer"),
                                     ("pack_review_panel.h", "packjs", "pack-review")):
            with self.subTest(file=file):
                source = (ESP / file).read_text(encoding="utf-8")
                asset = source.split('R"' + tag + '(', 1)[1].split(')' + tag + '"', 1)[0]
                self.assertEqual(generated.count('<script data-' + attribute + '>'), 1)
                self.assertIn('<script data-' + attribute + '>\n' + asset + '\n</script>', generated)
if __name__ == "__main__":
    unittest.main(verbosity=2)
