#!/usr/bin/env python3
"""Remove Persian comment lines and blank lines from source files.

What is removed:
  * any comment line that contains Persian characters,
  * blank lines that are not inside a string literal.

What is kept byte for byte:
  * every code line,
  * every string literal (including raw strings that carry HTML),
  * every comment that has no Persian in it.

Usage:
  python3 tools/strip_fa_comments.py --check [paths...]
  python3 tools/strip_fa_comments.py --apply [paths...]
"""
import os
import re
import sys
PERSIAN = re.compile('[\u0600-\u06FF\u200e\u200f\u202a-\u202e\ufb50-\ufdff\ufe70-\ufeff]')
C_EXTS = ('.c', '.h', '.ino')
PY_EXTS = ('.py',)
JS_EXTS = ('.js',)
SH_EXTS = ('.sh',)
DEFAULT_TARGETS = [
    'Firmware',
    'CubeIDE/Core',
    'ESP_USER',
    'ESP_Calibration',
    'tools',
    'Programmer',
]
SKIP_DIRS = {
    'node_modules', '__pycache__', 'build', 'Drivers', 'Middlewares',
    'STM32CubeIDE', 'Debug', 'Release',
}
SKIP_FILES = {
    'panel_preview.html', 'index.html', 'simple_preview.html',
    'simple_panel_clean.html', 'scenario5_simulator.html', 'chart_now.svg',
}
def has_persian(text):
    return PERSIAN.search(text) is not None
def is_decoration(line):
    return line.strip(' \t*/') == ''
SEP_LINE = re.compile(r'^(?P<head>\s*(?:/\*)?\s*=+)\s*(?P<mid>.*?)\s*(?P<tail>=+\s*(?:\*/)?)\s*$')
def clean_line(line):
    """Salvage the ASCII part of one comment line, '' when nothing is left."""
    no_fa = PERSIAN.sub('', line).replace('\u200c', '')
    sep = SEP_LINE.match(no_fa)
    if sep is not None:
        mid = re.sub(r'\s*/\s*$', '', sep.group('mid').strip())
        mid = re.sub(r'\s{2,}', ' ', mid).strip()
        if not re.search('[A-Za-z]', mid):
            return ''
        return sep.group('head').strip() + ' ' + mid + ' ' + sep.group('tail').strip()
    text = re.sub(r'\[(?:FA|EN)\]', '', no_fa)
    text = re.sub(r'\s{2,}', ' ', text).strip()
    if not re.search('[A-Za-z]', text):
        return ''
    # only a Doxygen tag line keeps its ASCII payload (name, units, type)
    if re.match(r'(?:/\*|\*|//|#)?\s*@(param|retval|return|pre|post)\b', text):
        return text
    return ''
def strip_comment(comment):
    """Return the replacement text for one comment token (None = delete)."""
    if not has_persian(comment):
        return comment
    if not comment.startswith('/*'):
        return clean_line(comment) or None
    multiline = '\n' in comment
    body = comment[2:-2] if comment.endswith('*/') else comment[2:]
    kept = []
    for ln in body.split('\n'):
        if not has_persian(ln):
            if multiline and is_decoration(ln):
                continue
            if ln.strip():
                kept.append(ln.rstrip())
            continue
        salvaged = clean_line(ln)
        if salvaged:
            kept.append(salvaged)
    if not kept:
        return None
    if not multiline:
        return '/* ' + kept[0].strip().lstrip('*').strip() + ' */'
    out = ['/**'] if comment.startswith('/**') else ['/*']
    for ln in kept:
        out.append(ln if ln.lstrip().startswith('*') else ' * ' + ln.strip())
    out.append(' */')
    return '\n'.join(out)
def scan_c(text):
    i, n = 0, len(text)
    buf = []
    while i < n:
        c = text[i]
        raw_at = i
        if text.startswith('u8R"', i):
            raw_at = i + 2
        if text.startswith('R"', raw_at):
            start = i
            j = text.index('"', raw_at) + 1
            open_paren = text.index('(', j)
            delim = text[j:open_paren]
            close = ')' + delim + '"'
            k = text.find(close, open_paren)
            end = n if k == -1 else k + len(close)
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('str', text[start:end])
            i = end
            continue
        if c in '"\'':
            quote = c
            start = i
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == quote or text[i] == '\n':
                    i += 1
                    break
                i += 1
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('str', text[start:i])
            continue
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j == -1 else j
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('comment', text[i:j])
            i = j
            continue
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j == -1 else j + 2
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('comment', text[i:j])
            i = j
            continue
        buf.append(c)
        i += 1
    if buf:
        yield ('code', ''.join(buf))
def scan_py(text):
    i, n = 0, len(text)
    buf = []
    while i < n:
        c = text[i]
        if c == '#':
            j = text.find('\n', i)
            j = n if j == -1 else j
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('comment', text[i:j])
            i = j
            continue
        triple = None
        for q in ('"""', "'''"):
            if text.startswith(q, i):
                triple = q
                break
        if triple is not None:
            j = text.find(triple, i + 3)
            j = n if j == -1 else j + 3
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('str', text[i:j])
            i = j
            continue
        if c in '"\'':
            q = c
            start = i
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == q or text[i] == '\n':
                    i += 1
                    break
                i += 1
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('str', text[start:i])
            continue
        buf.append(c)
        i += 1
    if buf:
        yield ('code', ''.join(buf))
def scan_js(text):
    i, n = 0, len(text)
    buf = []
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j == -1 else j
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('comment', text[i:j])
            i = j
            continue
        if text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j == -1 else j + 2
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('comment', text[i:j])
            i = j
            continue
        if c in '"\'`':
            q = c
            start = i
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('str', text[start:i])
            continue
        buf.append(c)
        i += 1
    if buf:
        yield ('code', ''.join(buf))
def scan_sh(text):
    i, n = 0, len(text)
    buf = []
    while i < n:
        if text.startswith('<<', i):
            m = re.match(r"<<-?\s*'?([A-Za-z_][A-Za-z0-9_]*)'?", text[i:])
            if m:
                tag = m.group(1)
                j = text.find('\n', i)
                j = n if j == -1 else j
                end = re.search(r'^' + re.escape(tag) + r'\s*$', text[j + 1:], re.M)
                stop = n if end is None else j + 1 + end.end()
                if buf:
                    yield ('code', ''.join(buf))
                    buf = []
                yield ('str', text[i:stop])
                i = stop
                continue
        c = text[i]
        if c == '#' and (i == 0 or text[i - 1] in ' \t\n'):
            j = text.find('\n', i)
            j = n if j == -1 else j
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('comment', text[i:j])
            i = j
            continue
        if c in '"\'':
            q = c
            start = i
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
            if buf:
                yield ('code', ''.join(buf))
                buf = []
            yield ('str', text[start:i])
            continue
        buf.append(c)
        i += 1
    if buf:
        yield ('code', ''.join(buf))
SCANNERS = {'c': scan_c, 'py': scan_py, 'js': scan_js, 'sh': scan_sh}
def kind_of(path):
    ext = os.path.splitext(path)[1].lower()
    if ext in C_EXTS:
        return 'c'
    if ext in PY_EXTS:
        return 'py'
    if ext in JS_EXTS:
        return 'js'
    if ext in SH_EXTS:
        return 'sh'
    return None
def rebuild(text, kind):
    pieces = []
    for tok_kind, value in SCANNERS[kind](text):
        if tok_kind == 'comment':
            replacement = strip_comment(value)
            if replacement:
                pieces.append(('comment', replacement))
            continue
        pieces.append((tok_kind, value))
    chars = []
    prot = []
    for tok_kind, value in pieces:
        is_str = (tok_kind == 'str')
        for ch in value:
            chars.append(ch)
            prot.append(is_str)
    raw = ''.join(chars)
    out_lines = []
    cur = []
    cur_prot_at = []
    cur_prot = False
    prev_continuation = False
    prev_nl_prot = False
    idx = 0
    n = len(raw)
    while idx <= n:
        if idx == n or raw[idx] == '\n':
            line = ''.join(cur)
            trail = line.rstrip(' \t')
            if trail != line:
                removed = cur_prot_at[len(trail):len(line)]
                if not any(removed):
                    line = trail
            drop = (line.strip() == '' and not cur_prot and not prev_continuation
                    and not prev_nl_prot)
            if not drop:
                out_lines.append(line)
                prev_continuation = line.rstrip().endswith('\\')
            prev_nl_prot = prot[idx] if idx < n else False
            cur = []
            cur_prot_at = []
            cur_prot = False
            idx += 1
            continue
        cur.append(raw[idx])
        cur_prot_at.append(prot[idx])
        cur_prot = cur_prot or prot[idx]
        idx += 1
    new_text = '\n'.join(out_lines)
    if new_text and not new_text.endswith('\n'):
        new_text += '\n'
    return new_text
def iter_files(targets):
    for target in targets:
        if os.path.isfile(target):
            yield target
            continue
        for root, dirs, files in os.walk(target):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for name in sorted(files):
                if name in SKIP_FILES:
                    continue
                path = os.path.join(root, name)
                if kind_of(path) is not None:
                    yield path
def main(argv):
    apply = '--apply' in argv
    args = [a for a in argv if not a.startswith('--')]
    targets = args if args else DEFAULT_TARGETS
    changed = 0
    saved = 0
    for path in sorted(iter_files(targets)):
        kind = kind_of(path)
        if kind is None:
            continue
        with open(path, 'r', encoding='utf-8', newline='') as fh:
            text = fh.read()
        text = text.replace('\r\n', '\n')
        new_text = rebuild(text, kind)
        if new_text != text:
            changed += 1
            delta = text.count('\n') - new_text.count('\n')
            saved += delta
            print('%-62s -%d' % (path, delta))
            if apply:
                with open(path, 'w', encoding='utf-8', newline='\n') as fh:
                    fh.write(new_text)
    print('files: %d, lines removed: %d%s' % (changed, saved,
                                              '' if apply else ' (dry run)'))
    return 0
if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
