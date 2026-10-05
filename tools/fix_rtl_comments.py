#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] RTL comment hygiene.

A Persian comment is a right-to-left paragraph. When a formula, an id range
or any other latin/arithmetic run is dropped inside it with no direction
mark, the bidi algorithm re-orders the neighbouring punctuation and the
reader sees things like "(100 / dur * count" instead of
"(dur * count / 100)". The fix is one character: wrap every latin/arithmetic
run that contains an operator in U+200E LEFT-TO-RIGHT MARK, so it is laid
out as one left-to-right island inside the Persian text.

Only comment lines that actually contain Persian letters are touched. Code,
string literals and pure-English comments are left exactly as they are.

Usage:
    python3 tools/fix_rtl_comments.py            # rewrite in place
    python3 tools/fix_rtl_comments.py --check    # report, exit 1 on findings

[FA] بهداشت کامنت‌های راست‌به‌چپ.

کامنت فارسی یک پاراگراف راست‌به‌چپ است. اگر فرمول یا بازهٔ عددی بدون علامت
جهت وسط آن بیفتد، الگوریتم bidi پرانتزها و علامت‌ها را جابه‌جا نشان می‌دهد و
فرمول به‌هم می‌ریزد. راه‌حل یک کاراکتر است: هر رشتهٔ لاتین/ریاضی که عملگر
دارد بین دو LRM بسته می‌شود تا مثل یک جزیرهٔ چپ‌به‌راست دیده شود.

فقط خط‌های کامنتی که حرف فارسی دارند دست می‌خورند.
"""

import re
import sys
import glob

LRM = "\u200e"

# [EN] Persian/Arabic letters. [FA] حروف فارسی.
FA = re.compile(r"[\u0600-\u06FF]")

# [EN] A latin/arithmetic run: letters, digits, brackets, operators, dots.
# [FA] رشتهٔ لاتین/ریاضی.
RUN = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_ \t()\[\]{}<>=+\-*/%.,:^|&!~']*"
                 r"[A-Za-z0-9_()\[\]}>%]")

# [EN] Only runs that really are a formula or a range need the mark.
# [FA] فقط رشته‌ای که واقعاً فرمول یا بازه است علامت می‌خواهد.
OP = re.compile(r"[A-Za-z0-9_)\]]\s*(?:<=|>=|==|!=|->|=>|[-+*/%=<>^])\s*"
                r"[A-Za-z0-9_(\[]|\.\.")


def _wrap(line):
    """[EN] Wrap formula runs of one comment line. [FA] بستن فرمول‌های یک خط."""
    head = ""
    body = line.rstrip("\n")
    tail = line[len(body):]

    # [EN] Keep the comment opener/leader out of the rewrite.
    # [FA] آغازگر کامنت دست‌نخورده می‌ماند.
    lead = re.match(r"^(\s*(?:/\*+|\*+|//+|)\s*)", body)
    if lead:
        head = lead.group(1)
        body = body[len(head):]

    closer = ""
    if body.endswith("*/"):
        closer = "*/"
        body = body[:-2]

    out, pos = [], 0
    for m in RUN.finditer(body):
        run = m.group(0)
        if not OP.search(run):
            continue
        before = body[pos:m.start()]
        # [EN] Already marked? leave it. [FA] اگر علامت دارد، رها کن.
        if before.endswith(LRM):
            continue
        out.append(before)
        out.append(LRM + run + LRM)
        pos = m.end()
    out.append(body[pos:])
    return head + "".join(out) + closer + tail


def _comment_lines(text):
    """[EN] Indices of comment lines. [FA] شمارهٔ خط‌های کامنت."""
    inblock = False
    for i, line in enumerate(text.splitlines(keepends=True)):
        stripped = line.lstrip()
        is_comment = (inblock or stripped.startswith("/*")
                      or stripped.startswith("*") or stripped.startswith("//"))
        if "/*" in line and "*/" not in line.split("/*", 1)[1]:
            inblock = True
        if "*/" in line:
            inblock = False
        yield i, line, is_comment


def process(path, check):
    text = open(path, encoding="utf-8").read()
    lines = text.splitlines(keepends=True)
    hits = []
    for i, line, is_comment in _comment_lines(text):
        # [EN] Section banners (==== Name ====) are grep anchors for the
        #      rule checker and must stay byte-for-byte as they are.
        # [FA] خط‌های جداکنندهٔ بخش‌ها لنگر grep هستند و دست نمی‌خورند.
        if (not is_comment or not FA.search(line) or LRM in line
                or "====" in line):
            continue
        new = _wrap(line)
        if new != line:
            hits.append(i + 1)
            lines[i] = new
    if hits and not check:
        open(path, "w", encoding="utf-8").write("".join(lines))
    return hits


def main():
    check = "--check" in sys.argv
    targets = sorted(
        glob.glob("Firmware/**/*.[ch]", recursive=True)
        + glob.glob("esp_link_panel/*.h")
        + glob.glob("esp_link_panel/*.ino")
        + glob.glob("tools/*.js"))
    total, files = 0, 0
    for path in targets:
        hits = process(path, check)
        if hits:
            files += 1
            total += len(hits)
            print(f"{path}: {len(hits)} line(s) "
                  f"{'need' if check else 'got'} a direction mark "
                  f"{hits[:8]}{' ...' if len(hits) > 8 else ''}")
    if check:
        if total:
            print(f"\nRTL COMMENT CHECK FAILED: {total} line(s) in {files} "
                  f"file(s) - run tools/fix_rtl_comments.py")
            return 1
        print("RTL comment check passed / کامنت‌های راست‌به‌چپ سالم‌اند")
        return 0
    print(f"\n{total} line(s) in {files} file(s) rewritten")
    return 0


if __name__ == "__main__":
    sys.exit(main())
