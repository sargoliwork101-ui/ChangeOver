#!/usr/bin/env python3
"""Generate the real panel with its embedded font, without simulation or fetch overrides.
The standalone file is a layout copy. It needs the preview HTTP server or an ESP
for live data; it must never invent successful board replies or calibration data.
"""
import json
import re
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "ESP_Calibration" / "plink_panel.h"
FONT = ROOT / "ESP_Calibration" / "plink_font.h"
OUT = ROOT / "ESP_Calibration" / "panel_preview.html"
def panel_html():
    """Extract the embedded HTML verbatim."""
    source = SRC.read_text(encoding="utf-8")
    start = source.index('R"HTML(') + len('R"HTML(')
    return source[start:source.rindex(')HTML"')]
def font_css():
    """Decode the exact font CSS served by the ESP."""
    source = FONT.read_text(encoding="utf-8")
    start = source.index("static const char ESP_PANEL_FONT_CSS")
    eq = source.index("=", start) + 1
    end = source.index(";\n", eq)
    literals = re.findall(r'"(?:\\.|[^"\\])*"', source[eq:end])
    return "".join(json.loads(literal) for literal in literals)
def main():
    html = panel_html()
    link = '<link rel="stylesheet" href="/f.css?v=2">'
    if link not in html:
        sys.exit("panel markup has no /f.css link")
    html = html.replace(link, '<style>' + font_css() + '</style>', 1)
    OUT.write_text(html, encoding="utf-8")
    print("wrote %s (%.0f KB)" % (OUT.relative_to(ROOT), OUT.stat().st_size / 1024))
    print("no simulated data embedded; use the preview server or a real ESP")
if __name__ == "__main__":
    main()
