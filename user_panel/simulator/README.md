# شبیه‌ساز پنل کاربر — User Panel Simulator

**ماژول:** `user_panel/simulator/` · **پورت:** ۸۰۹۰ · **شاخه:** `arena/00fd6b0e-changeover`

---

## ۱) این چیست — What it is

**FA** این پوشه، «خودِ» کد پنل را روی کامپیوتر اجرا می‌کند، نه یک ماکت از آن.
`sim_main.cpp` همان `user_panel.ino` و همان سرصفحه‌های `up_*.h` را در یک واحد
ترجمه، با همان ترتیب include‌ای که برد دارد، کامپایل می‌کند؛ فقط برد ESP و خودِ
ماشین جعلی‌اند:

- **ESP جعلی:** `../tools/stub_esp.h` (همان استاب‌هایی که تست میزبان استفاده می‌کند) -
  سوکت، فایل‌سیستم، سرور وب و ساعت.
- **ماشین جعلی:** `sim_machine.h` - پک ۲۴ ولت، دو نیمهٔ باتری، دو شارژر با حالت‌های
  واقعی، ماسک خطا، قفل عدم‌توازن، بودجهٔ ۲۰ سیکل، تعویض باتری و قاعدهٔ بار سوم جهش.
- **مسیر نوشتن:** هر چیزی که پنل به `/s` می‌نویسد، شبیه‌ساز می‌گیرد و به مدل
  می‌دهد - پس مسیر فرمان پنل واقعاً آزمایش می‌شود و نه فقط لاگ آن.

**EN** This folder runs the panel's OWN code on a computer, not a mock of it.
`sim_main.cpp` compiles the same `user_panel.ino` and the same `up_*.h` headers in
one translation unit, with the same include order the board uses; only the ESP and
the machine are fake. The fake ESP is the same stub set the host tests use; the fake
machine is `sim_machine.h`; and everything the panel writes to `/s` is captured and
handed to the model, so the panel's command path is exercised and not merely logged.

---

## ۲) چطور اجرا می‌شود — How to run it

```bash
# صفحهٔ کنترل و خود پنل روی http://localhost:8090
bash user_panel/simulator/run_sim.sh

# بدون برد و بدون سوکت: ۱۴ بررسی، کد خروج صفر یعنی سبز
bash user_panel/simulator/run_sim.sh --selftest

# پورت و سرعت دیگر
bash user_panel/simulator/run_sim.sh --port 9000 --speed 30
```

| Address | What |
|---|---|
| `http://localhost:8090/sim` | صفحهٔ کنترل: تزریق خطا، باتری، شبکه، زمان، و قابِ خود پنل |
| `http://localhost:8090/` | خود پنل — همان صفحهٔ واقعی، از همان کدی که روی برد می‌رود |
| `http://localhost:8090/api/...` | همان API واقعی، با همان بررسی نشست و نقش‌ها |

ورود پیش‌فرض: `admin` / `admin` (و `user` / `user` برای نقش بیننده).

---

## ۳) چه چیزی را می‌گیرد — What it catches before a board does

**FA**
- صفحه‌ای که کلمهٔ تلمتری اشتباه را می‌خواند: اعداد از دل پارسر واقعی و نقشهٔ
  واقعی اندیس‌ها می‌آیند (ستون «پک باتری» روی صفحه با مقدار مدل مقایسه می‌شود).
- دروازه‌های نقش: مدیر، اپراتور و بیننده روی همان مسیرها آزمایش می‌شوند.
- مسیر فرمان: «قطع شارژر ۱» باید به‌صورت `id=11&v=0` روی `/s` بنشیند و مدل هم
  اطاعت کند - شبیه‌ساز هر دو را جدا بررسی می‌کند.
- گزارش اکسل جریانی، بدون بازکردن اکسل: ابتدای فایل باید `PK` باشد.
- قفل عدم‌توازن و `FINAL_FAULT`: با تعویض باتری آزاد می‌شود و با بار سوم جهش فقط
  خاموش‌روشن برد پاکش می‌کند.
- زمان: با «۱ روز جلو» نمونه‌برداری، جمع‌های روزانه و نقشهٔ حرارتی پر می‌شوند، پس
  صفحه‌های تاریخ‌محور بدون انتظار یک روز آزمایش می‌شوند.

**EN** A screen reading the wrong telemetry word (the numbers travel through the
real parser and the real index map); the role gates on the real routes; the command
path (a charger cut must arrive as `id=11&v=0` AND the model must obey); the streamed
Excel report; the imbalance latch and `FINAL_FAULT` conversations; and time itself -
"advance one day" fills sampling, daily rollups and the heat map, so the date-driven
screens can be tested without waiting a day.

---

## ۴) چه چیزی را نمی‌گیرد — What it cannot catch

**FA** پایهٔ اشتباه، تقسیم‌کنندهٔ اشتباه، آنتن ضعیف، فلش کند، جریان نشتی، گرمای
برد، و هر زمان‌بندی‌ای که فقط روی سیلیکون واقعی وجود دارد. شبیه‌ساز جای میز آزمایش
را نمی‌گیرد؛ کارش این است که پیش از میز، ایرادهای منطقی و صفحه‌ای را بگیرد.

**EN** A wrong pin, a wrong divider, a weak antenna, a slow flash chip, leakage
current, heat - and any timing that only exists on real silicon. The simulator does
not replace the bench; its job is to remove the logic and screen defects before the
bench.

---

## ۵) فایل‌ها — Files

| File | [EN] | [FA] |
|---|---|---|
| `sim_main.cpp` | the real sketch + the HTTP server + the control commands + `--selftest` | اسکچ واقعی + سرور HTTP + فرمان‌ها + خودآزمایی |
| `sim_machine.h` | the model: pack, halves, chargers, faults, latch, cycles, swap, board reset | مدل ماشین |
| `sim.html` | the control page (Persian, RTL, live state, an iframe of the real panel) | صفحهٔ کنترل |
| `run_sim.sh` | always rebuilds, then runs or self-tests | ساخت دوباره و اجرا |
| `build/` | build output — **ignored by git** | خروجی ساخت، نادیده‌گرفته‌شده |

---

## ۶) نکته‌های مدل — Model notes (deliberately honest)

**FA**
- **دو نیمهٔ باتری با هم حرکت می‌کنند:** نیمهٔ پایینی نزدیک نصف پک می‌نشیند و با
  آن بالا می‌رود. مدلی که این کار را نکند، روی دستگاه سالم «عدم‌توازن» گزارش می‌کند
  و شبیه‌سازی که بی‌دلیل هشدار بدهد از نبودنش بدتر است.
- **«سیکل» یک سیکل شارژ است، نه یک تیک:** بودجهٔ ۲۰ سیکلی با شروع دوبارهٔ شارژ در
  حالت قفل خرج می‌شود، همان‌طور که فرم‌ور می‌شمارد.
- **قفل با تعویض باتری آزاد می‌شود:** همان قاعدهٔ سه‌ثانیه‌ای ماژول عدم‌توازن.
- **`/s` شناسه‌های بالای ۱۱۸ را با ۴۰۰ رد می‌کند:** عمداً مدل شده، چون همین دلیلِ
  نبودِ دکمهٔ «پاک‌کردن قفل» در پنل است.

**EN** The two halves move together (a model that reports an imbalance on a healthy
machine is worse than no model); a "cycle" is a charge session and not a tick; the
latch is released by the three-second battery-swap rule; and `/s` really does refuse
ids past 118, because that refusal is the reason the panel ships no "clear the latch"
button.
