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
| Version | 1.1 (build stamp in `up_config.h`) | ۱٫۱ |
| Target | ESP8266 (Arduino core 3.x), 4 MB flash | ESP8266 با فلش ۴ مگابایت |
| Language | C++17, static allocation only | ++C۱۷، فقط تخصیص ثابت |
| Firmware touched | **none** — the STM32 side is untouched | **هیچ** — سمت STM32 دست‌نخورده |
| Host tests | 237 checks, 0 failures (ASAN + UBSAN) | ۲۳۷ بررسی، ۰ خطا |
| Gate | `bash user_panel/tools/check_user_panel.sh` → 29 checks, 0 failures | گیت همین اسکریپت |
| Workbook, read by Python | `verify_report_xlsx.py` → 24 checks, 0 failures (zipfile + openpyxl + jdatetime) | ۲۴ بررسی، ۰ خطا |
| Simulator | `bash user_panel/simulator/run_sim.sh --selftest` → 14 checks, 0 failures | ۱۴ بررسی، ۰ خطا |
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
| 2026-10-07 | v1.1: real `.xlsx` report (`up_xlsx.h` + `up_report.h`), five sheets, range selector (all/7/30/90), streamed from the panel | گزارش اکسل واقعی با پنج برگه |
| 2026-10-07 | v1.1: the clock is **admin only** and set by hand; no browser ever pushes the time | ساعت فقط دستی و فقط مدیر |
| 2026-10-07 | v1.1: 17 dated anchors found a 32-bit time overflow - every epoch past 2038 printed a date in 1902 | لنگرهای تاریخی، سرریز ۲۰۳۸ را گرفتند |
| 2026-10-07 | v1.1: the simulator (`simulator/`) runs the real sketch against a model of the machine | شبیه‌ساز با کد واقعی |
| 2026-10-07 | v1.1: checked whether a "clear the imbalance latch" command is possible at all - it is not, and this README says why instead of shipping a dead button | امکان‌سنجی پاک‌کردن قفل عدم‌توازن |

---

## ۳) فایل‌ها — Files

| File | [EN] | Size |
|---|---|---|
| `user_panel.ino` | the sketch entry point: `setup()` / `loop()` and the status LED | 17 KB |
| `up_config.h` | every constant, in one place | 12 KB |
| `up_calendar.h` | Jalali + Gregorian conversion, pure integer, verified against 17 dated anchors | 29 KB |
| `up_xlsx.h` | the streaming `.xlsx` writer: stored ZIP, own CRC32, RTL sheets, frozen headers | 66 KB |
| `up_report.h` | the workbook itself: summary, daily, charges, events, samples + the range filter | 85 KB |
| `up_state.h` | RAM model, link health, clock, battery-percent mapping | 35 KB |
| `up_store.h` | LittleFS rings, "drop the oldest", daily rows, lifetime totals, CSV row | 44 KB |
| `up_auth.h` | salted+iterated SHA-256 users, sessions, cookie, action log | 34 KB |
| `up_history.h` | telemetry → charge sessions, outages, runs, events, counters | 40 KB |
| `up_link.h` | Wi-Fi (AP+STA) and the two reads plus the only write | 30 KB |
| `up_http.h` | the web server: routes, JSON writer, role gates, the Excel report route | 103 KB |
| `up_web.h` | **generated** page (HTML+CSS+JS) in PROGMEM | 130 KB |
| `up_font.h` | **generated** Vazirmatn (SIL OFL 1.1) as base64 woff2, served as `/f.css` | 51 KB |
| `web/` | the real, editable page: `index.html`, `app.css` (23 KB), `app.js` (96 KB) | |
| `tools/` | generator, preview builder, preview server, host stubs, test suite, gate script, `verify_report_xlsx.py` | |
| `simulator/` | the real sketch on a laptop: `sim_main.cpp`, `sim_machine.h`, `sim.html`, `run_sim.sh` | |
| `PLAN.md` | the work plan this module was built to | 8 KB |
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
| `POST /api/clock` | **admin only** | the admin sets the panel's date, time and offset by hand (plausibility-checked) |
| `GET /api/live` | viewer+ | 28 telemetry words, decoded flags, link health, today, imbalance, storage, active fault codes |
| `GET /api/series?hours=&points=` | viewer+ | stored samples decimated into ≤120 buckets + axis labels |
| `GET /api/events?limit=` | viewer+ | the newest events, newest first |
| `GET /api/stats?days=` | viewer+ | KPI block, charge sessions, daily rows, 7×24 hour heat map |
| `GET /api/admin/users` | admin | accounts (never a password or a digest) |
| `POST /api/admin/user` | admin | add / delete / re-role / re-password, with last-admin and no-self-delete guards |
| `POST /api/admin/action` | operator+ | **the only write**: `charger1_off/on`, `charger2_off/on` (params 11/12), `purge` (admin) |
| `GET /api/admin/audit?limit=` | admin | who did what, newest first |
| `GET /api/admin/clock` | admin | what the panel believes the time is, so the admin's form opens on the truth |
| `GET /api/admin/report.xlsx?days=N` | admin | **the Excel report**: streamed `.xlsx`, five sheets, `days=0`/absent = everything |
| `GET /api/export?what=csv\|json` | admin | streamed history, `user_panel_history.csv\|json` |

**FA** تنها مسیری که روی سیم برد چیزی می‌نویسد `POST /api/admin/action` است و فقط
پارامترهای ۱۱ و ۱۲ (فعال‌ساز شارژرها) را می‌فرستد. هیچ مسیری آستانه، کالیبراسیون،
محدودهٔ duty یا اقدام خطا را عوض نمی‌کند.

### چرا دکمهٔ «پاک‌کردن خطا» وجود ندارد — Why there is no "clear fault" button

[EN] This was asked and then measured against the protocol, not guessed: the
engineering panel's ESP answers `/s` for ids **0..118 only** and refuses anything
past that with `400`; the imbalance latch lives in NVM slots **200..202**, which
that ESP never accepts from a client ("NVM boot replay only"). A write of 202 = 0
would clear the latch on the STM32 - but the write cannot leave the panel, so an
admin button would be a lie. What the panel does instead is tell the truth on the
diagnostics page: the latch is released by (a) a real battery swap - the pack
absent for three seconds, which the imbalance module counts as "the memory leaves
with the battery" - or (b) a power cycle, which is also the only thing that
releases `FINAL_FAULT`. The simulator can demonstrate both in ten seconds
(`swap` and `board` on the control page).

[FA] این پرسیده و بعد با خود پروتکل سنجیده شد، نه حدس زده: ESP پنل مهندسی به
`/s` فقط برای شناسه‌های **۰ تا ۱۱۸** جواب می‌دهد و بقیه را با `400` رد می‌کند؛
قفل عدم‌توازن در اسلات‌های NVM **۲۰۰ تا ۲۰۲** است که آن ESP هرگز از یک کلاینت
نمی‌پذیرد («فقط پخش NVM هنگام بوت»). نوشتن ۲۰۲ = ۰ قفل را روی STM32 پاک می‌کرد -
ولی این نوشتن هرگز از پنل بیرون نمی‌رود، پس دکمهٔ مدیر یک دروغ می‌شد. کاری که پنل
می‌کند این است که در صفحهٔ دیاگ راستش را بگوید: قفل با (الف) تعویض واقعی باتری -
سه ثانیه جدا بودن پک، که ماژول عدم‌توازن آن را «رفتن حافظه با باتری» می‌شمارد - یا
(ب) خاموش‌روشن برد آزاد می‌شود؛ که تنها راه آزادکردن `FINAL_FAULT` هم هست.
شبیه‌ساز هر دو را در ده ثانیه نشان می‌دهد (`swap` و `board` در صفحهٔ کنترل).

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

---

## ۸) گزارش اکسل — The Excel report

[EN] A dedicated section of the stats view, admin only, with a range selector
(all / 7 / 30 / 90 days). The file is a **real** `.xlsx`: a stored (uncompressed)
ZIP with its own CRC32 and a hand-written SpreadsheetML part per sheet, streamed to
the browser as it is built, so a 4000-row workbook never has to fit in the ESP's RAM.

| Sheet | What is in it |
|---|---|
| `خلاصه` | report identity, generation time in both calendars, range, storage, range totals, lifetime totals, charge-duration histogram |
| `روزانه` | one row per stored day: charges, incomplete, charge seconds, energy, battery/input seconds, outages, boots, min/max pack, peak hour |
| `شارژها` | every charge session: start/complete/incomplete, channel, seconds, minutes, energy |
| `رویدادها` | the event log with the operator's words, severity, duration, the fault bit that caused it |
| `نمونهها` | the newest 4000 samples of the range: input, pack, lower battery, currents, duty, states, flags |

Numbers are numbers (so Excel can sum and chart them), dates are Jalali text so they
sort as text in any tool, every sheet is right-to-left with a frozen header, and the
range filter cuts records - which the gate proves by reading the workbook back with
Python's `zipfile`, `openpyxl` and `jdatetime` and checking that the one-day file is
exactly the newest rows of the full one.

**FA** یک بخش جداگانه در نمای آمار، فقط مدیر، با انتخاب بازه (همه / ۷ / ۳۰ / ۹۰ روز).
فایل، `xlsx` «واقعی» است: زیپ ذخیره‌ای با CRC32 خودش و اجزای SpreadsheetML دست‌نویس،
همان‌طور که ساخته می‌شود به مرورگر می‌رود، پس کتاب ۴۰۰۰ ردیفی هرگز لازم نیست در رم
ESP جا شود. اعداد، عدد می‌مانند (اکسل می‌تواند جمع و نمودار بگیرد)، تاریخ‌ها متن شمسی
هستند تا در هر ابزاری مرتب شوند، هر برگه راست‌به‌چپ با سرسطر ثابت است، و فیلتر بازه
واقعاً رکورد می‌اندازد - گیتی که کتاب را با `zipfile` و `openpyxl` و `jdatetime`
پایتون بازمی‌خواند و بررسی می‌کند کتاب یک‌روزه دقیقاً جدیدترین ردیف‌های کتاب کامل است.

---

## ۹) شبیه‌ساز — The simulator

[EN] `simulator/` runs the panel's own sketch — the same `user_panel.ino`, the same
single translation unit — against a model of the machine and the host stubs, and
serves it on **http://localhost:8090** with a control page at `/sim`: inject a fault,
trip the jitter three times, start an imbalance episode, swap the battery, cut the
network, jump an hour or a day, and watch the real page react. `--selftest` runs
fourteen checks without opening a socket; the gate runs it on every pass.

**FA** `simulator/` خودِ اسکچ پنل را - همان `user_panel.ino` و همان یک واحد ترجمه -
در کنار مدلی از ماشین و استاب‌های میزبان اجرا می‌کند و روی
**http://localhost:8090** با صفحهٔ کنترل در `/sim` سرو می‌کند: تزریق خطا، سه بار
جهش، شروع رخداد عدم‌توازن، تعویض باتری، قطع شبکه، یک ساعت یا یک روز جلو، و تماشای
واکنش صفحهٔ واقعی. `--selftest` چهارده بررسی را بدون بازکردن سوکت اجرا می‌کند و
گیت در هر گذر آن را می‌دواند.
