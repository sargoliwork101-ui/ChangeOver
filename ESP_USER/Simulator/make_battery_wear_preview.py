#!/usr/bin/env python3
"""Build a standalone, preview-only battery review mockup; never alter firmware."""
import argparse
from pathlib import Path
# ==================== Preview generator ====================
def main():
    """Embed the demo asset in an already-generated offline panel; output an HTML path."""
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=here.parents[1] / "deliverables" / "battery-wear-preview.html")
    output = parser.parse_args().output
    panel = (here.parent / "simple_preview.html").read_text(encoding="utf-8")
    script = (here / "battery_wear_preview.js").read_text(encoding="utf-8")
    mock_anchor = "var mockActive = (location.protocol === 'file:');"
    assert panel.count(mock_anchor) == 1
    panel = panel.replace(mock_anchor, "var mockActive = true; /* Standalone preview: never send device commands. */")
    assert panel.count("</body>") == 1
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(panel.replace("</body>", '<script data-battery-wear-demo>\n' + script + '\n</script></body>'), encoding="utf-8")
    print(output)
# ==================== Entry point ====================
if __name__ == "__main__":
    main()
