#!/usr/bin/env python3
"""
[EN] Stamp the web panel with a short build id derived from its own markup.

     Three times this session the panel was reported "broken" while every
     delivered artifact rendered correctly, because nobody - not the user and
     not the agent - could tell WHICH build was on screen. The preview URL
     changes whenever the sandbox restarts, a browser tab survives that
     change, and an ESP keeps serving whatever was last flashed onto it. A
     stale page is indistinguishable from a broken one.

     So the header now carries seven hex characters taken from the panel
     source itself. Read them off the page and you know exactly which build
     you are looking at. The value is DERIVED, never typed: this script
     recomputes it and audit_consistency.py fails if the file disagrees, so
     the stamp cannot go stale the way a hand-written version string does.

[FA] مهر بیلد روی پنل وب، مشتق‌شده از خود مارک‌آپ.

     سه بار در این جلسه پنل «خراب» گزارش شد در حالی که هر سه نسخهٔ تحویلی
     درست رندر می‌شدند، چون نه کاربر و نه عامل نمی‌توانست بفهمد کدام بیلد
     روی صفحه است. نشانی پیش‌نمایش با هر ری‌ست عوض می‌شود، تب مرورگر باقی
     می‌ماند، و ESP هر چه آخرین بار فلش شده را سرو می‌کند. صفحهٔ کهنه از
     صفحهٔ خراب قابل تشخیص نیست.

     حالا سربرگ هفت نویسهٔ hex از خود سورس پنل دارد. از روی صفحه بخوانید و
     بدانید کدام بیلد را می‌بینید. مقدار «مشتق» است نه دستی: این اسکریپت
     دوباره حسابش می‌کند و ممیزی اگر فایل نخواند شکست می‌دهد.
"""
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PANEL = ROOT / "esp_link_panel" / "plink_panel.h"

STAMP_RE = re.compile(r'(<span class="bs" id="bs">build )([0-9a-f]{7}|_______)(</span>)')
BLANK = "_______"


def literal(src: str) -> str:
    """[EN] The HTML the board serves / [FA] همان HTML که برد می‌فرستد."""
    return src.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]


def expected(src: str) -> str:
    """[EN] Hash the markup with the stamp blanked, so it cannot chase itself.
       [FA] هش مارک‌آپ با مهرِ خالی‌شده تا خودش را دنبال نکند."""
    body = STAMP_RE.sub(lambda m: m.group(1) + BLANK + m.group(3), literal(src))
    return hashlib.sha1(body.encode("utf-8")).hexdigest()[:7]


def main() -> int:
    src = PANEL.read_text(encoding="utf-8")
    m = STAMP_RE.search(src)
    if not m:
        print("no build stamp found in the panel header", file=sys.stderr)
        return 2
    want = expected(src)
    if m.group(2) == want:
        print(f"panel build stamp already correct: {want}")
        return 0
    PANEL.write_text(STAMP_RE.sub(lambda x: x.group(1) + want + x.group(3), src, count=1),
                     encoding="utf-8")
    print(f"panel build stamp {m.group(2)} -> {want}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
