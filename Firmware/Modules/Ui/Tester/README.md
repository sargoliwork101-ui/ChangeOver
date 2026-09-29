/**
 * @file    Firmware/Modules/Ui/Tester/README.md
 * @brief   [EN] What the Ui module's test tool is, why it exists and how to run it.
 *          [FA] ابزار تست ماژول UI: چیست، چرا نوشته شد و چطور اجرا می‌شود.
 */

# Ui — Tester / پوشهٔ تست ماژول UI

[EN] Everything in this folder tests **the Ui module only** (LED patterns and
the buzzer). It is kept next to the code it tests so a change to `ui_led.*` /
`ui_buzzer.*` and the test that guards it are never separated.

[FA] هرچه در این پوشه است فقط **ماژول UI** را تست می‌کند (الگوهای LED و بازر).
کنار همان کدی گذاشته شده که تستش می‌کند تا تغییر `ui_led.*`/`ui_buzzer.*` و
تستی که از آن محافظت می‌کند از هم جدا نیفتند.

---

## 1. Files / فایل‌ها

| File | What it is |
|---|---|
| `host_test_ui.py` | Host test for the buzzer duty/period rules and the LED percentage bands |
| `README.md` | This file |

---

## 2. Running it / اجرا

```bash
python3 Firmware/Modules/Ui/Tester/host_test_ui.py
# expected: ALL HOST TESTS PASSED (buzzer + BatteryRun 2%+0/1 + Charging 5% + Full 100/95 + phase)
```

[EN] It runs on a PC with no board attached. It reads the real constants out of
`../ui_buzzer.h` and `../ui_led.h` (one level up, in the module root) rather
than hard-coding them, so the test follows the firmware when a threshold moves.

[FA] روی PC و بدون برد اجرا می‌شود. ثابت‌ها را از `../ui_buzzer.h` و
`../ui_led.h` (یک سطح بالاتر، در ریشهٔ ماژول) می‌خواند و hard-code نمی‌کند، پس
با جابه‌جا شدن یک آستانه در فرم‌ور، تست هم با آن جابه‌جا می‌شود.

---

## 3. What it covers / چه چیزی را پوشش می‌دهد

| Area | Rule under test |
|---|---|
| Buzzer | duty / period / gap validity, the off and invalid results |
| BatteryRun | the 2 % step, and the 0 % / 1 % edge cases |
| Charging | the 5 % step across the 53..61 band |
| Full | the 100 / 95 latch pair (enter at 100, leave below 95) |
| Phase | LED phase relationship |

[EN] The 100/95 pair is the one worth knowing about: **entering** the full face
needs 100 %, **leaving** it needs to drop below 95 %. That gap is deliberate
hysteresis — without it the display flickers between full and not-full on a
pack sitting right at the boundary.

[FA] جفت ۱۰۰/۹۵ ارزش دانستن دارد: **ورود** به حالت پر ۱۰۰٪ می‌خواهد و **خروج**
افت به زیر ۹۵٪. این فاصله عمداً هیسترزیس است — بدون آن، پکی که دقیقاً روی مرز
نشسته باعث چشمک‌زدن نمایش بین پر و ناپر می‌شود.

---

## 4. Limits / محدودیت‌ها

[EN] This is a host test: it checks the **rules**, not the hardware. It cannot
see LED brightness, real buzzer loudness, or RTOS timing. Board behaviour still
has to be confirmed on the bench.

[FA] این تست host است: **قواعد** را می‌سنجد نه سخت‌افزار را. روشنایی واقعی LED،
بلندی واقعی بازر و زمان‌بندی RTOS را نمی‌بیند. رفتار برد باید سر بنچ تأیید شود.
