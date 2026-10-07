#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_preview.py - build a single HTML file that shows the panel WITHOUT a board.

[EN] WHY
     The panel is a page that lives inside an ESP8266 and only ever speaks to a
     real machine. Nobody can review a layout that way: the board has to be on,
     the Wi-Fi has to reach it, and the machine has to be doing something worth
     looking at. This script produces preview/user_panel_preview.html: the SAME
     index.html, the SAME app.css, the SAME app.js and the SAME embedded font
     the board serves - with the network replaced by a script that answers every
     /api/... call with a fixed, believable dataset.

     It is a preview, not a second implementation: not one line of the page is
     rewritten. If the preview disagrees with the board, the preview is what is
     wrong - unless it is the data, which is deliberately synthetic.
     (tools/panel_preview_server.py can serve it on a port when a local file is
     not enough; the file is self-contained either way.)

     What it does NOT do: logins (the preview is always logged in as an admin),
     downloads (the export links would hit the filesystem), and writes (an
     action answers ok, nothing happens).

[FA] چرا
     پنل صفحه‌ای است که داخل یک ESP8266 زندگی می‌کند و فقط با یک ماشین واقعی
     حرف می‌زند. هیچ‌کس نمی‌تواند چیدمان را این‌طور بازبینی کند: برد باید روشن
     باشد، وای‌فای باید به آن برسد و ماشین باید مشغول کاری باشد که ارزش نگاه
     کردن داشته باشد. این اسکریپت preview/user_panel_preview.html را می‌سازد:
     همان index.html، همان app.css، همان app.js و همان قلم جاسازی‌شده‌ای که برد
     سرو می‌کند - با شبکه‌ای که جایش را اسکریپتی گرفته که به هر فراخوانی
     /api/... با یک دادهٔ ثابت و باورپذیر جواب می‌دهد.

     این پیش‌نمایش است، نه پیاده‌سازی دوم: حتی یک خط از صفحه بازنویسی نمی‌شود.
     اگر پیش‌نمایش با برد اختلاف داشت، اشکال از پیش‌نمایش است - مگر داده باشد
     که عمداً ساختگی است. (tools/panel_preview_server.py می‌تواند آن را روی یک
     پورت سرو کند؛ در هر حال فایل خودبسنده است.)

     چه کاری نمی‌کند: ورود (پیش‌نمایش همیشه با نقش مدیر وارد است)، دانلود
     (پیوندهای برون‌بری به فایل‌سیستم می‌خورند) و نوشتن (هر اقدام ok می‌دهد و
     هیچ اتفاقی نمی‌افتد).

Run: python3 user_panel/tools/build_preview.py
Out: user_panel/preview/user_panel_preview.html
"""

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PANEL = HERE.parent
WEB = PANEL / "web"
OUT = PANEL / "preview" / "user_panel_preview.html"

# ---------------------------------------------------------------- the mock --
# [EN] The dataset. Written as JavaScript so it can be generated (a 24 hour
#      voltage profile, seven heat-map rows, thirty daily rows) instead of being
#      typed out as a wall of numbers.
# [FA] داده. به‌صورت جاوااسکریپت نوشته شده تا ساخته شود (پروفایل ۲۴ ساعتهٔ ولتاژ،
#      هفت ردیف نقشهٔ حرارتی، سی ردیف روزانه) نه اینکه دیواری از عدد تایپ شود.
MOCK_JS = r"""
/* ============================================================================
 * PREVIEW DATA - this script exists only in the preview file.
 * دادهٔ پیش‌نمایش - این اسکریپت فقط در فایل پیش‌نمایش وجود دارد.
 * ==========================================================================*/
(function () {
  const NOW = Math.floor(Date.now() / 1000);
  const DAY = 86400;
  const HOUR = 3600;

  /* [EN] A deterministic generator: the same picture every time the page is
     opened, so a change in the layout is a change in the layout.
     [FA] یک تولیدکنندهٔ قطعی: هر بار باز شدن صفحه همان تصویر، تا تغییر در
     چیدمان، تغییر در چیدمان باشد. */
  function rng(seed) {
    let s = seed >>> 0;
    return function () { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; };
  }

  /* ---- the machine right now: input cut two hours ago, running on battery,
          channel 1 waiting to retry after a jitter trip --------------------- */
  const FL_SNAPSHOT = 0x01, FL_INPUT = 0x02, FL_MEAS = 0x04, FL_CH1 = 0x08, FL_CH2 = 0x10;
  const fl = FL_SNAPSHOT | FL_MEAS;                    /* no input bit: on battery */
  const fl2 = 0x00;
  const states = { off: 0, bulk: 1, absorb: 2, float: 3, bringup: 4, jit: 5, inputWait: 6, final: 7, batLost: 8, manual: 9 };
  const t = [];
  t[0] = 4015; t[1] = 25000; t[2] = 0; t[3] = 0; t[4] = 0; t[5] = 0; t[6] = states.jit;
  t[7] = 64; t[8] = 320; t[9] = 0; t[10] = 0; t[11] = 0; t[12] = 0; t[13] = states.off;
  t[14] = 0;             /* input voltage: gone   */
  t[15] = 24820;         /* pack                  */
  t[16] = 12560;         /* lower battery         */
  t[17] = 12560;
  t[18] = 12260;
  t[19] = (1 << 4);      /* fault mask: jitter on channel 1 */
  t[20] = 0; t[21] = 2100; t[22] = 1105; t[23] = 1210; t[24] = 3295;
  t[25] = 300;           /* imbalance mV          */
  t[26] = 2;             /* imbalance episodes    */
  t[27] = 1;

  const live = {
    t: t,
    fl: fl,
    fl2: fl2,
    flags: {
      snapshot: 1, input: 0, measValid: 1,
      ch1: (fl & FL_CH1) ? 1 : 0, ch2: (fl & FL_CH2) ? 1 : 0, manual: 0
    },
    link: { online: 1, ageMs: 640, frames: 918233, vm: 0, ce: 0, err: 0, sta: 1 },
    now: { wallValid: 1, wall: NOW, uptimeS: 918233, absS: 1219333 },
    today: { charges: 3, incomplete: 1, runS: 7350, chargeS: 9640, energyWh10: 4120 },
    imb: { mv: 300, events: 2, cycles: 1, latched: 0, blocked: 0 },
    store: {
      usedBytes: 118784, totalBytes: 524288, samples: 12480, events: 462,
      days: 21, purges: 2, purging: 0
    },
    faultBits: [4],
    faultCodes: ['E05', 'E14', 'E17'],
    par: { '74': 21000, '75': 28500 },
    tot: {
      charges: 148, incomplete: 9, sumChargeS: 512400, runS: 26400, inputS: 1290000,
      coverageS: 1814400, boots: 6, outages: 11, peakI1: 5820, peakI2: 5640,
      minV: 23180, maxV: 27460
    }
  };

  /* ---- 24 hours of samples, one per 12 minutes: charging in the morning and
          an input cut in the evening -------------------------------------- */
  function series() {
    const n = 120, r = rng(20261007);
    const v24 = [], v12 = [], vin = [], ageMin = [], label = [];
    const bucket = Math.round((24 * 3600) / n);
    for (let i = 0; i < n; i++) {
      const hoursAgo = (24 * 3600 - i * bucket) / 3600;         /* i = oldest */
      const cut = (hoursAgo > 1.4 && hoursAgo < 3.6);           /* the outage  */
      const charging = (hoursAgo > 12 && hoursAgo < 16);
      const pack = charging ? 25600 + 420 * Math.sin(i / 7) : 24820 - 260 * Math.cos(i / 11);
      const sag = cut ? 420 * Math.min(1, (3.6 - hoursAgo)) : 0;
      v24.push(Math.round(pack - sag + (r() - 0.5) * 40));
      v12.push(Math.round(12560 + 220 * Math.sin(i / 9) + (r() - 0.5) * 24));
      vin.push(cut ? 0 : Math.round(24550 + 180 * Math.sin(i / 5) + (r() - 0.5) * 60));
      ageMin.push(Math.round(hoursAgo * 60));
      if (i % Math.round(n / 4) === 0) {
        const h = new Date((NOW - Math.round(hoursAgo * 3600)) * 1000).getHours();
        label.push(pad2fa(h) + ':' + '۰۰');
      }
    }
    return { n: n, hours: 24, bucketS: bucket, now: live.now.absS, first: live.now.absS - 24 * 3600,
             v24: v24, v12: v12, vin: vin, ageMin: ageMin, label: label };
  }

  function pad2fa(v) {
    const d = '۰۱۲۳۴۵۶۷۸۹';
    return String(v).padStart(2, '0').replace(/[0-9]/g, (x) => d[+x]);
  }

  /* ---- the event journal, newest first --------------------------------- */
  const events = [
    { code: 8,  ch: 1, sev: 1, dur: 0,   t: NOW - 900,    a: 0,    b: 0,   ageS: 900 },
    { code: 12, ch: 1, sev: 1, dur: 0,   t: NOW - 1500,   a: 0,    b: 0,   ageS: 1500 },
    { code: 4,  ch: 0, sev: 1, dur: 0,   t: NOW - 5400,   a: 0,    b: 0,   ageS: 5400 },
    { code: 6,  ch: 0, sev: 1, dur: 0,   t: NOW - 5400,   a: 0,    b: 0,   ageS: 5400 },
    { code: 3,  ch: 2, sev: 1, dur: 940, t: NOW - 14400,  a: 940,  b: 118, ageS: 14400 },
    { code: 2,  ch: 1, sev: 0, dur: 7320, t: NOW - 16200, a: 7320, b: 512, ageS: 16200 },
    { code: 1,  ch: 1, sev: 0, dur: 0,   t: NOW - 23520,  a: 0,    b: 0,   ageS: 23520 },
    { code: 14, ch: 0, sev: 1, dur: 0,   t: NOW - 26000,  a: 1,    b: 340, ageS: 26000 },
    { code: 9,  ch: 0, sev: 0, dur: 0,   t: NOW - 28800,  a: 0,    b: 0,   ageS: 28800 },
    { code: 16, ch: 0, sev: 1, dur: 0,   t: NOW - 43200,  a: 0,    b: 0,   ageS: 43200 },
    { code: 10, ch: 0, sev: 2, dur: 0,   t: NOW - 72000,  a: 0,    b: 0,   ageS: 72000 },
    { code: 11, ch: 0, sev: 2, dur: 0,   t: NOW - 73000,  a: 0,    b: 0,   ageS: 73000 }
  ];

  /* ---- statistics ------------------------------------------------------ */
  function sessions() {
    const r = rng(7717), out = [];
    for (let i = 11; i >= 0; i--) {
      const done = (i % 5 !== 0) ? 1 : 0;
      const dur = Math.round(2400 + r() * 5200 + (done ? 900 : -400));
      out.push([NOW - (i * 6 + 1) * HOUR, done, dur]);
    }
    return out;
  }

  function daily() {
    const r = rng(31337), out = [];
    for (let i = 29; i >= 0; i--) {
      const day = NOW - i * DAY;
      const start = Math.floor(day / DAY) * DAY;       /* first second of that day */
      out.push([start, Math.round(2 + r() * 6), 0, Math.round(1800 + r() * 9000)]);
    }
    return out;
  }

  function heat() {
    const r = rng(90210), rows = [], days = [];
    for (let d = 6; d >= 0; d--) {
      const row = new Array(24).fill(0);
      for (let h = 0; h < 24; h++) {
        const evening = (h >= 17 && h <= 22), morning = (h >= 6 && h <= 9);
        row[h] = (evening || morning) ? Math.round(r() * 3) : (r() > 0.86 ? 1 : 0);
      }
      rows.push(row);
      days.push(Math.floor((NOW - d * DAY) / DAY) * DAY);
    }
    return { rows: rows, days: days };
  }

  const H = heat(), D = daily(), SES = sessions();
  const stats = {
    kpi: {
      charges: 148, incomplete: 9, sumChargeS: 512400, avgChargeS: 3462, minChargeS: 612,
      maxChargeS: 8410, medianChargeS: 3260, outages: 11, outageS: 26400, runS: 26400,
      maxRunS: 9120, inputS: 1290000, chargeWh10: 51240, runWh10: 3180,
      peakI1: 5820, peakI2: 5640, minV: 23180, maxV: 27460, imbEvents: 2, boots: 6,
      coverageS: 1814400, days: 21, sessions: 12, maxChargesPerHour: 3, clock: 1
    },
    sessions: SES,
    daily: D,
    heatDays: H.days,
    heat: H.rows
  };

  /* ---- administration -------------------------------------------------- */
  const users = {
    u: [
      ['admin', 'admin', NOW - 1814400, NOW - 120, 0],
      ['operator', 'operator', NOW - 1728000, NOW - 86400, 0],
      ['viewer', 'viewer', NOW - 900000, NOW - 400000, 1]
    ]
  };
  const audit = {
    n: 8,
    a: [
      [NOW - 900,    'operator', 'charger1_off'],
      [NOW - 1500,   'operator', 'charger1_on'],
      [NOW - 5400,   'system',   'input_lost'],
      [NOW - 14400,  'admin',    'user_add'],
      [NOW - 28800,  'admin',    'password_change'],
      [NOW - 43200,  'operator', 'charger2_off'],
      [NOW - 72000,  'admin',    'export_csv'],
      [NOW - 1814400, 'system',  'boot']
    ]
  };
  const version = {
    name: 'user panel', fw: '1.0', build: 'PREVIEW-2026-10-07',
    board: 'ESP8266', ap: 'ChangeOver-User', source: '192.168.4.1', router: '192.168.5.1'
  };

  /* ---- the shim: answer every route the page knows --------------------- */
  const routes = {
    '/version': version,
    '/api/me': { ok: 1, user: 'admin', role: 'admin', must: 0 },
    '/api/login': { ok: 1, user: 'admin', role: 'admin', must: 0 },
    '/api/logout': { ok: 1 },
    '/api/pass': { ok: 1 },
    '/api/clock': { ok: 1, epoch: NOW },
    '/api/live': live,
    '/api/series': series(),
    '/api/events': { n: events.length, limit: 60, clock: 1, e: events },
    '/api/stats': stats,
    '/api/admin/users': users,
    '/api/admin/user': { ok: 1 },
    '/api/admin/action': { ok: 1 },
    '/api/admin/audit': audit,
    '/api/export': { ok: 0, err: 'preview has no history to export' }
  };

  const realFetch = window.fetch ? window.fetch.bind(window) : null;

  window.fetch = function (url, options) {
    const path = String(url).split('?')[0];
    if (Object.prototype.hasOwnProperty.call(routes, path)) {
      const payload = routes[path];
      return Promise.resolve({
        ok: true,
        status: 200,
        headers: { get: () => 'application/json' },
        json: () => Promise.resolve(payload),
        text: () => Promise.resolve(JSON.stringify(payload))
      });
    }
    if (realFetch) { return realFetch(url, options); }
    return Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve({ ok: 0, err: 'not in preview' }) });
  };
})();
"""

PREVIEW_BANNER = """
<div style="background:#3a2a06;border-bottom:1px solid #6b4e0d;color:#f7c33c;
            font:13px/1.6 Vazirmatn,system-ui,sans-serif;padding:8px 14px;text-align:center">
  <b>پیش‌نمایش طراحی</b> — داده‌ها ساختگی‌اند و از برد نمی‌آیند.
  <span style="opacity:.75">Design preview — the data below is synthetic, not from a board.</span>
</div>
"""


def read(path):
    return path.read_text(encoding="utf-8")


def font_css():
    """[EN] Rebuild the @font-face text that lives in up_font.h as C string
       chunks, so the preview uses the very same embedded font as the board.
       [FA] بازسازی متن @font-face که در up_font.h به‌صورت تکه‌های رشتهٔ C است،
       تا پیش‌نمایش همان قلم جاسازی‌شدهٔ برد را به کار ببرد."""
    source = read(PANEL / "up_font.h")
    lines = source.splitlines()
    start = next(i for i, line in enumerate(lines) if "UP_PANEL_FONT_CSS[]" in line)
    chunks = []
    # [EN] The CSS is written as one raw literal per line and the lines are
    #      concatenated by the compiler. The first semicolon is NOT the end:
    #      the CSS text itself is full of them.
    # [FA] CSS به‌صورت یک رشتهٔ خام در هر خط نوشته شده و کامپایلر خطوط را به هم
    #      می‌چسباند. اولین نقطه‌ویرگول پایان نیست: خود متن CSS پر از آن است.
    for line in lines[start:]:
        chunks.extend(re.findall(r'"((?:[^"\\]|\\.)*)"', line))
        if line.rstrip().endswith(";"):
            break
    return "".join(chunks).replace('\\"', '"')


def build():
    # [EN] `--output PATH` writes somewhere else - the gate regenerates into a
    #      temporary file and compares, so "the preview is stale" is a machine
    #      answer rather than a review note.
    # [FA] `--output PATH` جای دیگری می‌نویسد - گیت داخل فایل موقت از نو می‌سازد و
    #      مقایسه می‌کند تا «پیش‌نمایش کهنه است» جواب ماشین باشد نه یادداشت بازبین.
    out_path = OUT
    if "--output" in sys.argv:
        out_path = Path(sys.argv[sys.argv.index("--output") + 1]).resolve()

    html = read(WEB / "index.html")
    css = read(WEB / "app.css")
    js = read(WEB / "app.js")

    html = html.replace('<link rel="stylesheet" href="/f.css">',
                        "<style>\n" + font_css() + "\n</style>")
    html = html.replace('<link rel="stylesheet" href="/app.css">',
                        "<style>\n" + css + "\n</style>")
    html = html.replace('<script src="/app.js"></script>',
                        "<script>\n" + MOCK_JS + "\n</script>\n<script>\n" + js + "\n</script>")
    html = html.replace("<body>", "<body>\n" + PREVIEW_BANNER, 1)
    html = html.replace("<title>پنل کاربر — ChangeOver</title>",
                        "<title>پیش‌نمایش پنل کاربر — ChangeOver</title>")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(html, encoding="utf-8")
    print("wrote %s (%.1f KB)" % (out_path, out_path.stat().st_size / 1024.0))
    return 0


if __name__ == "__main__":
    sys.exit(build())
