/**
 * @file    Firmware/Modules/Charger/Tester/README.md
 * @brief   [EN] What the Charger module's test tools are, why they exist and how to run them.
 *          [FA] ابزارهای تست ماژول شارژر: چه هستند، چرا نوشته شدند و چطور اجرا می‌شوند.
 */

# Charger — Tester / پوشهٔ تست ماژول شارژر

[EN] Everything in this folder tests **the Charger module only**. It is kept
next to the code it tests so that a change to `charger.c` / `charger.h` and the
test that guards it are never separated.

[FA] هرچه در این پوشه است فقط **ماژول شارژر** را تست می‌کند. کنار همان کدی
گذاشته شده که تستش می‌کند تا تغییر `charger.c`/`charger.h` و تستی که از آن
محافظت می‌کند هرگز از هم جدا نیفتند.

---

## 1. Files / فایل‌ها

| File | What it is | Why it exists |
|---|---|---|
| `host_test_charger.py` | 40 host tests | The safety + contract gate. Runs on a PC, no board needed. |
| `pid_tuning_sim.py` | Charge simulator | Produces the *numbers* the PID design is justified with. |
| `README.md` | This file | — |

---

## 2. `host_test_charger.py` — the gate / دروازه

```bash
python3 Firmware/Modules/Charger/Tester/host_test_charger.py
# expected: ALL 40 CHARGER HOST TESTS PASSED
```

**[EN] What it actually checks.** Most tests are *static*: they read
`charger.c`, `charger.h`, `esp_link*`, the ESP panel sources and the CubeMX
`.ioc`, and assert the safety contract still holds — the 500 permille DCM duty
ceiling, the 950 mA hard fault, the 15.0 V over-voltage cut, the JIT sequence,
the id map agreeing on both sides of the wire, the NVM record version, and so
on. Three tests are *behavioural*: they drive `pid_tuning_sim.py` and assert
how the loop responds.

**[FA] واقعاً چه چیزی را چک می‌کند.** بیشتر تست‌ها *ایستا*ند: سورس‌ها را
می‌خوانند و قرارداد ایمنی را می‌سنجند — سقف دیوتی ۵۰۰ پرمیل، خطای سخت
۹۵۰ میلی‌آمپر، قطع اضافه‌ولتاژ ۱۵٫۰ ولت، توالی JIT، یکی‌بودن نقشهٔ شناسه‌ها در
دو سر سیم، نسخهٔ رکورد NVM و… . سه تست *رفتاری*اند و شبیه‌ساز را می‌رانند.

> **[EN] It does NOT prove the firmware is correct on hardware.** There is no
> ARM build here and no battery attached. It proves the *sources* still satisfy
> the written contract.
> **[FA] این تست ثابت نمی‌کند فرم‌ور روی سخت‌افزار درست است.** نه بیلد ARM
> هست نه باتری. ثابت می‌کند *سورس‌ها* هنوز قرارداد نوشته‌شده را رعایت می‌کنند.

### Why a *static* test suite at all? / چرا اصلاً تست ایستا؟

[EN] Because the dangerous regressions on this product are not logic slips,
they are **silent constant drift**: someone widens a limit, renumbers an id on
one side of the link only, or removes a margin. A compiler is happy with all
three. These tests are the thing that is not.

[FA] چون خطرناک‌ترین پس‌رفت‌های این محصول، اشتباه منطقی نیستند بلکه
**جابه‌جایی بی‌صدای ثابت‌ها**ست: کسی حدی را باز می‌کند، شناسه‌ای را فقط در یک سر
لینک عوض می‌کند، یا حاشیه‌ای را برمی‌دارد. کامپایلر با هر سه راحت است؛ این
تست‌ها نیستند.

---

## 3. `pid_tuning_sim.py` — the simulator / شبیه‌ساز

[EN] A **faithful transcript** of `func__Charger_PidStep` plus a plant model of
the flyback + lead-acid pack. Its single most important property:

> It **reads the constants straight out of `charger.h` with a regex** instead of
> copying them. Change a gain in the firmware and the simulator changes with
> it — it can never quietly describe a version of the code that no longer
> exists.

[FA] یک **رونوشت وفادار** از `func__Charger_PidStep` به‌اضافهٔ مدل فلای‌بک و پک
سرب-اسید. مهم‌ترین ویژگی‌اش: **ثابت‌ها را با regex مستقیم از `charger.h`
می‌خواند** و کپی نمی‌کند؛ پس هرگز نسخه‌ای از کد را توصیف نمی‌کند که دیگر وجود
ندارد.

```bash
cd Firmware/Modules/Charger/Tester

python3 pid_tuning_sim.py                      # 10 scenarios x 10 h, the factory tune
python3 pid_tuning_sim.py --trace              # which loop drives the ONE duty, pass by pass
python3 pid_tuning_sim.py --stress             # setpoint / duty-ceiling / pack disturbances
python3 pid_tuning_sim.py --rows '12,1600,0,1000,1000;50,18000,0,10,1000'
python3 pid_tuning_sim.py --compare            # architecture comparison
```

### The plant / مدل

```text
i(mA)    = 230.4 x (D/1000)^2 / (v_term/1000) x 1000      # flyback in DCM
v_term   = voc + i x R                                     # R = 0.15 ohm default
i_gas    = I0 x exp((v_term - 14400) / 150)                # gassing sink, ends the charge
voc     += ((i - i_gas)/1000) x dt / C x 1000              # C = 2000 mAh default
```

### Sensor noise / نویز

[EN] **Seeded PRNG, broadband — never a sine.** An earlier version used a
deterministic tone and its duty-reversal counts were chaotic enough to *reverse
a design decision*: it first argued the third gain row was needed for noise,
and the PRNG showed the opposite. Any noise claim must average **≥ 4 seeds**.

[FA] **PRNG با seed و پهن‌باند — هرگز سینوسی.** نسخهٔ قبلی از تن قطعی استفاده
می‌کرد و شمارش تغییر جهتش آن‌قدر آشوبناک بود که **یک تصمیم طراحی را برعکس
کرد**. هر ادعای نویز باید میانگین **حداقل ۴ seed** باشد.

---

## 4. What the simulator CANNOT tell you / آنچه شبیه‌ساز نمی‌گوید

[EN] Being explicit about this matters more than the results themselves:

- real ADC behaviour, component tolerance, temperature drift
- RTOS timing jitter and task interaction
- the **15.0 V OV cut and the 950 mA hard fault**, which live in the state
  machine *above* `PidStep` and are not modelled here
- the bumpless re-seed path (nothing else writes the duty in the model)
- anything about a real battery's chemistry beyond the gassing term

[FA] صراحت دربارهٔ این‌ها از خودِ نتایج مهم‌تر است: رفتار واقعی ADC، تلرانس
قطعات، دریفت دما، جیتر زمان‌بندی RTOS، **قطع ۱۵٫۰ ولت و خطای سخت
۹۵۰ میلی‌آمپر** (که *بالای* `PidStep` در ماشین حالت‌اند و مدل نشده‌اند)، مسیر
re-seed، و هر چیزی از شیمی واقعی باتری فراتر از جملهٔ گازدهی.

---

## 5. Rules for changing anything here / قواعد تغییر

1. **[EN] Never copy a firmware constant into the simulator.** Read it from the
   header. / **[FA] هرگز ثابت فرم‌ور را در شبیه‌ساز کپی نکنید؛** از هدر بخوانید.
2. **[EN] Every number published in a doc must be regenerable by a command in
   this folder.** / **[FA] هر عددی که در سند منتشر می‌شود باید با یک دستور در
   همین پوشه بازتولید شود.**
3. **[EN] After adding a test, mutation-test it:** break the thing on purpose
   and confirm the test fails. A test that never fails guards nothing. /
   **[FA] بعد از افزودن تست، موتیشن‌تست کنید:** عمداً خرابش کنید و ببینید تست
   می‌افتد. تستی که هرگز نمی‌افتد از چیزی محافظت نمی‌کند.
4. **[EN] Keep it runnable from anywhere** — both from the repo root and from
   inside this folder. / **[FA] از هر جا قابل اجرا بماند** — هم از ریشهٔ مخزن
   هم از داخل همین پوشه.
