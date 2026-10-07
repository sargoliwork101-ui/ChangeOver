# پنل کاربر ChangeOver — User Panel

**EN** A read-only dashboard for the owner of the machine: live state, statistics, a Persian
fault diagnosis page, and an administration area for users and the two charger enables.
It runs on its **own ESP8266 board** and reads the existing engineering panel's API.
**FA** داشبورد فقط-خواندنی برای صاحب دستگاه: وضعیت زنده، آمار، صفحهٔ دیاگ خطا به فارسی و
بخش مدیریت برای کاربران و دو فعال‌ساز شارژر. روی **برد ESP8266 مستقل خودش** اجرا
می‌شود و API همان پنل مهندسی موجود را می‌خواند.

---

## ۱) وضعیت — Status

| | [EN] | [FA] |
|---|---|---|
| Version | 1.0 (build stamp in `up_config.h`) | ۱٫۰ |
| Target | ESP8266 (Arduino core 3.x), 4 MB flash | ESP8266 با فلش ۴ مگابایت |
| Language | C++17, static allocation only | ++C۱۷، فقط تخصیص ثابت |
| Firmware touched | **none** — the STM32 side is untouched | **هیچ** — سمت STM32 دست‌نخورده |
| Host tests | 126 checks, 0 failures (ASAN + UBSAN) | ۱۲۶ بررسی، ۰ خطا |
| Gate | `bash user_panel/tools/check_user_panel.sh` → 23 checks, 0 failures | گیت همین اسکریپت |
| Board tests | **not run yet** — no ESP8266 was flashed for this deliverable | هنوز اجرا نشده |

[EN] The headers are larger than their code suggests: every file carries the bilingual
file header, the per-function documentation and the "why" notes this project's rules
require, and those are counted in the sizes above.
[FA] هدرها از آنچه کدشان نشان می‌دهد بزرگ‌ترند: هر فایل سرصفحهٔ دوزبانه، مستندات
هر تابع و توضیح «چرا» را دارد که قواعد پروژه خواسته و در اندازه‌های بالا شمرده شده‌اند.

Hierarchy decision (recorded so nobody has to re-derive it): the raw figure for the
changeover panel already is the machine's own AP at `192.168.4.1`, so this panel's soft-AP
would collide with it if left at the ESP8266 default. The panel therefore serves
**192.168.5.1**, documented in `up_config.h` and asserted by a host test.
**FA** جایگاه در شبکه: AP خودِ ماشین `192.168.4.1` است، پس AP این پنل روی **192.168.5.1**
می‌آید تا دو زیرشبکهٔ پیش‌فرض روی یک دستگاه جمع نشوند.

---

## ۲) تاریخچه — History

| Date | [EN] | [FA] |
|---|---|---|
| 2026-10-07 | New module: page (`web/`), sketch (`user_panel.ino`), 9 headers, host stubs, tests, preview | ماژول تازه |
| 2026-10-07 | Host tests found and fixed 4 real defects (see `tools/host_test_user_panel.cpp` header) | چهار نقص واقعی |
| 2026-10-07 | Daily rows: the file grows before it is written at an offset (a filesystem that refuses seek-past-end would silently overwrite day one with today) | رشد فایل روزانه پیش از نوشتن |

---

## ۳) فایل‌ها — Files

| File | [EN] | Size |
|---|---|---|
| `user_panel.ino` | the sketch entry point: `setup()` / `loop()` and the status LED | 17 KB |
| `up_config.h` | every constant, in one place | 12 KB |
| `up_state.h` | RAM model, link health, clock, battery-percent mapping | 35 KB |
| `up_store.h` | LittleFS rings, "drop the oldest", daily rows, lifetime totals, CSV row | 44 KB |
| `up_auth.h` | salted+iterated SHA-256 users, sessions, cookie, action log | 34 KB |
| `up_history.h` | telemetry → charge sessions, outages, runs, events, counters | 40 KB |
| `up_link.h` | Wi-Fi (AP+STA) and the two reads plus the only write | 30 KB |
| `up_http.h` | the web server: routes, JSON writer, role gates | 92 KB |
| `up_web.h` | **generated** page (HTML+CSS+JS) in PROGMEM | 130 KB |
| `up_font.h` | **generated** Vazirmatn (SIL OFL 1.1) as base64 woff2, served as `/f.css` | 51 KB |
| `web/` | the real, editable page: `index.html`, `app.css` (23 KB), `app.js` (96 KB) | |
| `tools/` | generator, preview builder, preview server, host stubs, test suite, gate script | |
| `preview/` | **generated** single-file preview with synthetic data | 180 KB |

**FA** `up_web.h` و `up_font.h` ساخته‌شده‌اند و ویرایش دستی‌شان ممنوع است؛ گیت اگر
کهنه باشند شکست می‌دهد. صفحهٔ واقعی و قابل ویرایش زیر `web/` است.

---

## ۴) توابع و مسیرها — Functions and routes

| Route | Role | Purpose |
|---|---|---|
| `GET /` `/app.css` `/app.js` `/f.css` | — | the page itself (PROGMEM, cached lifetimes set per asset) |
| `GET /version` | — | name, firmware, build stamp, board, AP, addresses |
| `GET /api/me` | — | who am I, my role, must I change my password |
| `POST /api/login` `/api/logout` `/api/pass` | — / viewer+ | session cookie (HttpOnly, 8 h idle), forced first change |
| `POST /api/clock` | viewer+ | the browser hands the panel its time (plausibility-checked) |
| `GET /api/live` | viewer+ | 28 telemetry words, decoded flags, link health, today, imbalance, storage, active fault codes |
| `GET /api/series?hours=&points=` | viewer+ | stored samples decimated into ≤120 buckets + axis labels |
| `GET /api/events?limit=` | viewer+ | the newest events, newest first |
| `GET /api/stats?days=` | viewer+ | KPI block, charge sessions, daily rows, 7×24 hour heat map |
| `GET /api/admin/users` | admin | accounts (never a password or a digest) |
| `POST /api/admin/user` | admin | add / delete / re-role / re-password, with last-admin and no-self-delete guards |
| `POST /api/admin/action` | operator+ | **the only write**: `charger1_off/on`, `charger2_off/on` (params 11/12), `purge` (admin) |
| `GET /api/admin/audit?limit=` | admin | who did what, newest first |
| `GET /api/export?what=csv\|json` | admin | streamed history, `user_panel_history.csv\|json` |

**FA** تنها مسیری که روی سیم برد چیزی می‌نویسد `POST /api/admin/action` است و فقط
پارامترهای ۱۱ و ۱۲ (فعال‌ساز شارژرها) را می‌فرستد. هیچ مسیری آستانه، کالیبراسیون،
محدودهٔ duty یا اقدام خطا را عوض نمی‌کند.

---

## ۵) ورودی و خروجی — What it reads and what it sends

Reads (never writes) from the engineering panel on the machine's own AP:

* `GET /t` every 1000 ms — `on,age,seq,fl,fl2,n,q,q2,q3,q4,ka,vm,ce,t[28],p[119]`
* `GET /m` every 15 000 ms — the peaks window (`hi[]`, `lo[]`, `n`, `or`)

Writes, only when an operator or admin confirms an action: `POST /s?id=11|12&v=0|1`.

Storage on the panel's own flash (auto-drop of the oldest is the user's requirement):

| File | Content | Share of the partition |
|---|---|---|
| `/up_samples.bin` | one sample per second (13 columns on export) | 42 % |
| `/up_events.bin` | charge sessions, outages, faults, boots, admin actions | 22 % |
| `/up_daily.bin` | one row per day: charges, runtime, energy, hours histogram | 10 % |
| `/up_tot.bin` | lifetime counters + the clock pair (monotonic ↔ wall) | — |
| `/up_users.bin` `/up_audit.bin` | accounts and the action log | never purged |

CSV columns: `epoch,abs_s,vin_mv,v24_mv,v12_mv,i1_ma,i2_ma,duty1,duty2,state1,state2,flags,flags2`.

**FA** وقتی حافظه پر شود، **قدیمی‌ترین** داده دور ریخته می‌شود (نصف حلقه، با فایل
موقت و جای‌گذاری اتمی) و صفحه صادقانه می‌گوید چند روز را پوشش می‌دهد.

---

## ۶) پیش‌فرض امن — Safe defaults

* **Display-only for users.** A viewer can read every page and change nothing.
* **Two writes in the whole program**, both behind a confirmation dialog with a
  single-use stamp, so a double tap cannot cut a charger twice.
* **No fault can be cleared** — the protocol has no such command. The diagnostics page
  marks `FINAL_FAULT` channels as *waiting for a board reset* and shows the human
  procedure instead of pretending an API exists.
* **No TLS is claimed.** The login protects against a curious neighbour on the same
  Wi-Fi, not against someone who can read the wire; the README of the engineering panel
  says the same about its own password.
* Forced password change on first login; the seeded `admin/admin` closes every route
  except the change itself.
* `UP_BUILD_STAMP` is compiled into `/version`, so a cabinet can always say which build
  it is running.

**FA** پنل برای کاربر فقط نمایشگر است؛ تنها نوشتنی‌ها دو فعال‌ساز شارژرند، با تأیید و
مهر یک‌بارمصرف. پاک‌کردن خطا از API ممکن نیست و صفحه به‌جای ادعای دروغ، روش دستی
ری‌استارت برد را نشان می‌دهد. هیچ ادعایی دربارهٔ TLS نمی‌شود.

---

## ۷) درخت اتصال و راه‌اندازی — Wiring and deployment

```
   mains ──► machine (24 V changeover) ── USART1 ──► engineering ESP
                                                      192.168.4.1   AP: ChangeOver-ESP
                                                            ▲
                                                            │ STA (client) + reads /t, /m
                                                            │
                                          user panel ESP8266 ┘
                                          AP: ChangeOver-User / 123456789 / ch 6
                                          192.168.5.1   ← phone, tablet, laptop
```

1. Arduino IDE → board **Generic ESP8266 Module** (or LOLIN/Wemos D1 mini), 80 MHz,
   4 MB flash (1 MB SPIFFS + 1 MB OTA), 115200 upload.
2. Adjust `UP_STA_SSID` / `UP_STA_PASS` in `up_config.h` if the machine's network is not
   the default; adjust the panel's own AP credentials there too.
3. Upload the sketch. Status LED: slow blink = link healthy, fast blink = link down.
4. Join **ChangeOver-User**, open `http://192.168.5.1`, log in as `admin` / `admin`,
   change the password (forced), then create one account per person under *مدیریت*.
5. Before trusting it, run the module gate:
   `bash user_panel/tools/check_user_panel.sh`
   and look at the design offline:
   `python3 user_panel/tools/build_preview.py` → `preview/user_panel_preview.html`
   (or `python3 user_panel/tools/panel_preview_server.py 8080`).

**FA** ترتیب راه‌اندازی: فلش اسکچ، اتصال به شبکهٔ `ChangeOver-User`، ورود با
`admin/admin` و تغییر اجباری گذرواژه، سپس ساخت یک حساب برای هر شخص. پیش از اعتماد،
گیت ماژول و پیش‌نمایش آفلاین را اجرا کنید. فلش‌کردن و آزمایش روی برد هنوز انجام نشده
است و این README ادعای خلافش نمی‌کند.
