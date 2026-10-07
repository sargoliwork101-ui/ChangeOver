#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_panel_header.py - embed web/ into up_web.h (PROGMEM raw strings).

[EN] WHY
     The panel's HTML, CSS and JS live as real editable files under web/, not
     as a wall of escaped C next to the firmware. Hand-writing them into a
     header is how the engineering panel started, and every later edit then
     needed the backslashes counted by hand: one missing quote and the whole
     sketch stops compiling, with an error pointing at a line that nobody
     reads. This generator keeps ONE copy of each asset - the file in web/ -
     and writes the C++ that embeds it.

     It is the opposite direction to tools/make_panel_preview.py: that one
     pulls the markup OUT of the firmware so a human can look at it; this one
     pushes it IN so the board can serve it. Both read the same source of
     truth, so the preview and the served page cannot disagree.

     Output is deterministic: no timestamps, only content hashes. Regenerating
     without changing web/ must produce a byte-identical up_web.h, which is
     exactly what tools/check_user_panel.sh asserts - a stale embedded copy is
     a panel that silently serves yesterday's page.

[FA] چرا
     HTML و CSS و JS پنل به‌صورت فایل‌های واقعی و قابل ویرایش در web/ هستند،
     نه دیوار C فرار‌داده‌شده کنار فرم‌ور. دست‌نویس کردنشان در هدر همان کاری
     است که پنل مهندسی با آن شروع شد و بعد هر ویرایش یعنی شمردن دستی بک‌اسلش:
     یک کوتیشن فراموش‌شده و کل اسکچ کامپایل نمی‌شود، با خطایی که به خطی اشاره
     می‌کند که کسی نمی‌خواند. این تولیدکننده یک نسخه از هر دارایی (فایل داخل
     web/) را نگه می‌دارد و کد ++C جاسازی‌اش را می‌نویسد.

     جهت کار عکس tools/make_panel_preview.py است: آن یکی مارک‌آپ را از
     فرم‌ور بیرون می‌کشد تا انسان ببیندش، این یکی داخل می‌فرستد تا برد
     سروش کند. هر دو از یک منبع حقیقت می‌خوانند، پس پیش‌نمایش و صفحهٔ سروشده
     نمی‌توانند با هم اختلاف پیدا کنند.

     خروجی قطعی است: بدون زمان، فقط هش محتوا. تولید دوباره بدون تغییر web/
     باید عیناً همان up_web.h را بسازد و tools/check_user_panel.sh همین را
     تأیید می‌کند - نسخهٔ جاسازی‌شدهٔ کهنه یعنی پنلی که بی‌صدا صفحهٔ دیروز را
     سرو می‌کند.

Run: python3 user_panel/tools/build_panel_header.py
Out: user_panel/up_web.h
"""

import hashlib
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PANEL = HERE.parent
OUT = PANEL / "up_web.h"

# [EN] (source file, C array name, raw-string delimiter, description)
# [FA] (فایل منبع، نام آرایهٔ C، جداکنندهٔ رشتهٔ خام، توضیح)
ASSETS = [
    ("index.html", "UP_INDEX_HTML", "UPH", "the shell / پوسته"),
    ("app.css", "UP_APP_CSS", "UPC", "the theme / پوستهٔ ظاهری"),
    ("app.js", "UP_APP_JS", "UPJ", "the application / برنامه"),
]

LEGAL_ENDINGS = (".html", ".css", ".js")


def read_asset(name):
    """[EN] One web asset as text. Dies loudly rather than embedding a missing
       file as an empty string.
       [FA] یک دارایی وب به‌صورت متن. اگر فایل نباشد با صدای بلند می‌میرد، نه
       اینکه جای آن رشتهٔ خالی جاسازی کند."""
    path = PANEL / "web" / name
    if not path.is_file():
        sys.exit("missing asset: %s" % path)
    text = path.read_text(encoding="utf-8")
    if '"""' in text:
        pass  # harmless for raw strings, kept as a reminder that quotes matter
    if not text.strip():
        sys.exit("asset is empty: %s" % path)
    return text


def check_raw_delimiter(text, delim, name):
    """[EN] A raw string ends at )DELIM" - so the asset must never contain it,
       and must not end with the escaping edge cases either.
       [FA] رشتهٔ خام در )DELIM" تمام می‌شود، پس دارایی هرگز نباید آن را داشته
       باشد و نباید با حالت‌های مرزی فرار تمام شود."""
    closer = ")" + delim + '"'
    if closer in text:
        sys.exit("asset %s contains the raw-string terminator %s" % (name, closer))


def fingerprint(text):
    """[EN] First 12 hex digits of SHA-256 - what a human compares to see
       whether the embedded copy is the current one.
       [FA] ۱۲ رقم اول SHA-256 - چیزی که انسان مقایسه می‌کند تا ببیند نسخهٔ
       جاسازی‌شده همان نسخهٔ فعلی است."""
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:12]


def c_string(text, delim, name):
    """[EN] The text as one raw string literal. Multi-byte characters are fine:
       they travel as UTF-8 bytes and the page declares charset=utf-8.
       [FA] متن به‌صورت یک رشتهٔ خام. کاراکترهای چندبایتی مشکلی ندارند: به‌شکل
       بایت‌های UTF-8 می‌روند و صفحه charset=utf-8 را اعلام می‌کند."""
    check_raw_delimiter(text, delim, name)
    return 'R"%s(%s)%s"' % (delim, text, delim)


def main():
    # [EN] `--output PATH` writes somewhere else - tools/check_user_panel.sh
    #      regenerates into a temporary file and compares, so "the embedded copy
    #      is stale" is a machine answer instead of a code review.
    # [FA] `--output PATH` جای دیگری می‌نویسد - tools/check_user_panel.sh داخل
    #      فایل موقت از نو می‌سازد و مقایسه می‌کند تا «نسخهٔ جاسازی‌شده کهنه
    #      است» جواب ماشین باشد نه بازبینی چشمی.
    out_path = OUT
    if "--output" in sys.argv:
        out_path = Path(sys.argv[sys.argv.index("--output") + 1]).resolve()

    parts = []
    for name, array, delim, what in ASSETS:
        if not name.endswith(LEGAL_ENDINGS):
            sys.exit("unexpected asset type: %s" % name)
        parts.append((name, array, delim, what, read_asset(name)))

    total = sum(len(p[4].encode("utf-8")) for p in parts)

    lines = []
    lines.append("/**")
    lines.append(" * @file    up_web.h")
    lines.append(" * @brief   [EN] The web assets of the user panel, embedded in PROGMEM.")
    lines.append(" *")
    lines.append(" *          GENERATED FILE - DO NOT EDIT. It is written by")
    lines.append(" *          user_panel/tools/build_panel_header.py from the three files in")
    lines.append(" *          user_panel/web/. Edit those, then re-run the generator; the check")
    lines.append(" *          script re-runs it and fails if this file is stale, so the panel can")
    lines.append(" *          never serve a page that no longer matches its own sources.")
    lines.append(" *")
    lines.append(" *          Each asset is one raw string literal (R\"...\"), so nothing in the")
    lines.append(" *          CSS or JS needs escaping: what the file holds byte-for-byte is what")
    lines.append(" *          the board sends. The engineering panel uses the same pattern, which")
    lines.append(" *          is why both can be read as web pages instead of as C.")
    lines.append(" *")
    lines.append(" * @brief   [FA] دارایی‌های وب پنل کاربر، جاسازی‌شده در PROGMEM.")
    lines.append(" *")
    lines.append(" *          فایل تولیدشده - ویرایش نکنید. این فایل توسط")
    lines.append(" *          user_panel/tools/build_panel_header.py از سه فایل داخل")
    lines.append(" *          user_panel/web/ نوشته می‌شود. آن‌ها را ویرایش کنید و بعد تولیدکننده")
    lines.append(" *          را دوباره اجرا کنید؛ اسکریپت بررسی دوباره اجرایش می‌کند و اگر این")
    lines.append(" *          فایل کهنه باشد شکست می‌خورد، پس پنل هرگز صفحه‌ای را سرو نمی‌کند که با")
    lines.append(" *          منبع خودش نمی‌خواند.")
    lines.append(" *")
    lines.append(" *          هر دارایی یک رشتهٔ خام (R\"...\") است، پس هیچ‌چیز در CSS یا JS نیاز")
    lines.append(" *          به فرار‌دادن ندارد: همان بایتی که فایل دارد، همان چیزی است که برد")
    lines.append(" *          می‌فرستد. پنل مهندسی هم همین الگو را دارد و همین است که هر دو را")
    lines.append(" *          می‌توان به‌جای C، به‌شکل صفحهٔ وب خواند.")
    lines.append(" */")
    lines.append("")
    lines.append("#ifndef UP_WEB_H")
    lines.append("#define UP_WEB_H")
    lines.append("")
    lines.append("/* ==================== Assets / دارایی‌ها ==================== */")
    for name, array, delim, what, text in parts:
        lines.append("/**")
        lines.append(" * @brief [EN] web/%s - %s" % (name, what.split(" / ")[0]))
        lines.append(" *        [FA] web/%s - %s" % (name, what.split(" / ")[1]))
        lines.append(" *        [EN] %d bytes, sha256[:12] = %s" % (len(text.encode("utf-8")), fingerprint(text)))
        lines.append(" *        [FA] %d بایت، sha256[:12] = %s" % (len(text.encode("utf-8")), fingerprint(text)))
        lines.append(" */")
        lines.append("static const char %s[] PROGMEM = %s;" % (array, c_string(text, delim, name)))
        lines.append("")
    lines.append("/* ==================== Embedded-size guards / نگهبان اندازه =====================")
    lines.append("   [EN] A build-time promise that nothing was embedded empty or truncated.")
    lines.append("        If an asset is ever replaced by a placeholder, the sketch stops here")
    lines.append("        instead of serving a blank page from a device in a cabinet.")
    lines.append("   [FA] قول زمان ساخت که هیچ‌چیز خالی یا بریده جاسازی نشده است. اگر روزی")
    lines.append("        دارایی‌ای با یک جانگهدار عوض شود، اسکچ همین‌جا متوقف می‌شود نه اینکه")
    lines.append("        صفحهٔ سفید از دستگاهی داخل تابلو سرو کند. */")
    for name, array, delim, what, text in parts:
        lines.append('static_assert(sizeof(%s) == %du, "%s: expected %d bytes");'
                     % (array, len(text.encode("utf-8")) + 1, name, len(text.encode("utf-8"))))
    lines.append("")
    lines.append("/* [EN] Total payload the browser has to pull over the panel's own AP. */")
    lines.append("/* [FA] کل داده‌ای که مرورگر باید روی AP خود پنل بگیرد. */")
    lines.append("#define UP_WEB_TOTAL_BYTES %du" % total)
    lines.append("")
    lines.append("#endif /* UP_WEB_H */")

    text_out = "\n".join(lines) + "\n"
    out_path.write_text(text_out, encoding="utf-8")
    print("wrote %s (%.1f KB) from %d asset(s), %.1f KB embedded"
          % (out_path, out_path.stat().st_size / 1024.0, len(parts), total / 1024.0))


if __name__ == "__main__":
    main()
