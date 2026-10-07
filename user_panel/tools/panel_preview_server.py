#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
panel_preview_server.py - serve the built preview on a port.

[EN] WHY
     preview/user_panel_preview.html opens from the filesystem, and that is
     usually enough. It is not enough when the file is opened on a phone, or
     when a font has to load, or when somebody wants to show the panel to a
     colleague on the same network. This serves that one file at every path, on
     every interface (0.0.0.0), with caching switched off so a rebuild is seen
     on the next refresh.

     It serves the PREVIEW, never the panel: nothing here talks to a machine.

[FA] چرا
     preview/user_panel_preview.html از روی فایل‌سیستم باز می‌شود و این معمولاً
     کافی است. وقتی فایل روی گوشی باز شود، یا قلمی باید بارگذاری شود، یا کسی
     بخواهد پنل را روی شبکه برای همکاری نشان دهد، کافی نیست. این سرو، همان یک
     فایل را روی هر مسیر و روی همهٔ رابط‌ها (0.0.0.0) با کش خاموش تحویل می‌دهد
     تا ساخت دوباره با یک تازه‌سازی دیده شود.

     این «پیش‌نمایش» را سرو می‌کند، هرگز خود پنل را: هیچ‌چیز اینجا با ماشینی
     حرف نمی‌زند.

Run: python3 user_panel/tools/panel_preview_server.py [port]   (default 8080)
"""

import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREVIEW = HERE.parent / "preview" / "user_panel_preview.html"


class Handler(BaseHTTPRequestHandler):
    def _send(self):                                            # noqa: N802
        if not PREVIEW.is_file():
            self.send_error(404, "run tools/build_preview.py first")
            return
        body = PREVIEW.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    do_GET = _send
    do_HEAD = _send

    def log_message(self, fmt, *args):                          # noqa: A003
        sys.stderr.write("preview %s\n" % (fmt % args))


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print("user panel preview on http://0.0.0.0:%d/ (Ctrl+C to stop)" % port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.server_close()


if __name__ == "__main__":
    main()
