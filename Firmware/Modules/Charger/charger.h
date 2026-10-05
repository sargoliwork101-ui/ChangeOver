/**
 * @file    charger.h
 * @brief   [EN] Two independent 12 V flyback charger channels. CHG_MASTER_ENABLE
 *          is the single overall activation gate; when it is 0 the whole
 *          Charger must remain safe-off regardless of runtime flags. All
 *          numerical protections (voltage, current, JIT and missing-battery
 *          thresholds) remain enforced whenever control is allowed.
 *          [FA] دو کانال مستقل شارژر فلای‌بک ۱۲ ولت. CHG_MASTER_ENABLE تنها
 *          کلید فعال‌سازی کلی است؛ با مقدار ۰ کل Charger صرف‌نظر از پرچم‌های
 *          زمان اجرا ‎safe-off‎ می‌ماند. با فعال‌بودن کنترل، همه حفاظت‌های عددی
 *          (ولتاژ، جریان، JIT و آستانه باتری) همچنان اجرا می‌شوند.
 */

#ifndef CHARGER_H
#define CHARGER_H

/* ==================== Includes / شامل‌ها ==================== */
#include "app_types.h"
#include <stdint.h>

/* ==================== Master enable / فعال‌سازی کلی ==================== */
/*
 * [EN] Single master switch for Charger control.
 *   0 = safe-off skeleton only: all PWM stopped, the transformer input
 *       relay keeps its NC contact closed (coil off, safe-idle), JIT and
 *       relay disconnect policy inactive. Runtime flags such as
 *       power_stage_enabled/pwm_max_duty are NOT hidden hard gates for
 *       Charger; numeric protections are never bypassed.
 *   1 = the per-channel control loop runs only when every explicit
 *       hardware condition is also satisfied (valid snapshot, installed
 *       channel, Vin ADC >= 22000 mV, valid battery sense, current
 *       limits, JIT policy).
 * [FA] تنها کلید فعال‌سازی کلی Charger.
 *   ۰ = فقط اسکلت ‎safe-off: PWM‎ متوقف، رله NC بسته (‎safe-idle)‎، سیاست
 *       JIT/رله غیرفعال. پرچم‌های زمان اجرا دروازهٔ پنهان نیستند؛
 *       حفاظت‌های عددی هیچ‌وقت bypass نمی‌شوند.
 *   ۱ = حلقهٔ کنترل هر کانال فقط با برقراری همهٔ شرایط صریح سخت‌افزاری
 *       (snapshot معتبر، کانال نصب‌شده، Vin >= ۲۲۰۰۰mV، sense باتری
 *       معتبر، حدهای جریان، سیاست JIT) اجرا می‌شود.
 */
#define CHG_MASTER_ENABLE             1u

/* ==================== Board/test selection constants / ثابت‌های انتخاب برد و تست ==================== */
/*
 * [EN] The only two assembly-selection constants to change when the
 * installed transformer changes. Both are installed now:
 *   Channel 1 = 1 (user bring-up order 2026-09-20) -> PWM1 PA0 / Current1
 *               / JIT1 active (bench verification stays on the board
 *               checklist).
 *   Channel 2 = 1 -> PWM2 PA6 / Current2 PA7 / JIT2 PB6. Trans2 feeds an
 *               independent 12 V battery on VLOW = MID - GND; the 24 V
 *               pack measurement is monitor-only, never a charge setpoint
 *               or missing-battery condition for CH2.
 * [FA] تنها دو ثابتی که با عوض‌شدن ترانس مونتاژشده تغییر می‌کنند؛ هر دو
 *      نصب‌اند: کانال ۱ = ۱ (دستور راه‌اندازی ۲۰۲۶-۰۹-۲۰) → PWM1 PA0 /
 *      ‎Current1 / JIT1 (‎تأیید بنچ در چک‌لیست برد)؛ کانال ۲ = ۱ → PWM2 PA6
 *      / ‎Current2 PA7 / JIT2 PB6. Trans2‎ به باتری ۱۲V مستقل روی VLOW وصل
 *      است؛ پک ۲۴V فقط مانیتور است و هیچ‌گاه setpoint شارژ یا شرط
 *      ‎battery-missing‎ برای CH2 نیست.
 */
#define CHG_CHANNEL_1_INSTALLED       1u
#define CHG_CHANNEL_2_INSTALLED       1u

#define CHG_CHANNEL_1_MASK            (1u << 0)
#define CHG_CHANNEL_2_MASK            (1u << 1)
#define CHG_INSTALLED_CHANNEL_MASK    \
    (((CHG_CHANNEL_1_INSTALLED != 0u) ? CHG_CHANNEL_1_MASK : 0u) | \
     ((CHG_CHANNEL_2_INSTALLED != 0u) ? CHG_CHANNEL_2_MASK : 0u))

/* ==================== Conservative bring-up gate / دروازه امن راه‌اندازی ==================== */
/* [EN] CHG_TRANSFORMER_KNOWN = 1: transformer data and the current-sense
 * chain are board-verified (gate/shunt waveforms at the 10% stage, R41/R42
 * divider compensated in the BSP, zero offset 8 counts, R77 confirmed 100k,
 * shunt reading physically consistent with a 23.5 V / 12.55 V energy audit).
 * Normal Bulk/Absorb/Float control below now runs. It is still a separate
 * compile-time safety gate from CHG_MASTER_ENABLE.
 * [‎FA] CHG_TRANSFORMER_KNOWN = 1‎: داده ترانس و زنجیره سنجش جریان روی برد
 * تأیید شده‌اند. کنترل عادی ‎Bulk/Absorb/Float‎ حالا اجرا می‌شود. این دروازه
 * همچنان جدا از CHG_MASTER_ENABLE است. */
#define CHG_TRANSFORMER_KNOWN         1u

/* ==================== Explicit limited bring-up test mode / حالت صریح تست bring-up ==================== */
/*
 * [EN] Explicit bring-up-only mode and the ONLY way to run switching when
 * CHG_TRANSFORMER_KNOWN=0: only Trans2 (CH2), NO real battery, external
 * source current-limited, waveform validation only - normal Bulk/Absorb/
 * Float control does NOT run here. Limits: only installed CH2 is allowed
 * (CH1 forced 0); start duty 1%, duty step as usual but clamped to
 * CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE - current stage max 10% duty with a
 * 100 mA external source limit; do not raise until the transformer data
 * is measured and CHG_TRANSFORMER_KNOWN flips to 1. Never enters Absorb/
 * Float, never uses a 24 V pack setpoint, still gated by
 * CHG_MASTER_ENABLE=1, Vin >= 22000 mV and all numeric protections.
 * (With CHG_BRINGUP_TEST_ENABLE=1: gate/shunt waveforms scope-verified at
 * the 10% stage with a calibrated current path.)
 * [FA] مود صریح فقط-‎bring-up‎ و تنها راه سوئیچینگ با ‎CHG_TRANSFORMER_KNOWN=0‎:
 *      فقط Trans2 (CH2)، بدون باتری واقعی، منبع خارجی محدود، فقط
 *      اعتبارسنجی شکل‌موج - کنترل عادی اجرا نمی‌شود. حدود: فقط CH2 (CH1
 *      صفر)؛ شروع ۱٪، گام معمول ولی گیرهٔ CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE
 *      - مرحلهٔ فعلی حداکثر ۱۰٪ duty با حد ۱۰۰mA منبع خارجی؛ تا اندازه‌گیری
 *      ترانس و یک‌شدن CHG_TRANSFORMER_KNOWN بالاتر نرو. هرگز ‎Absorb/Float‎ و
 *      setpoint پک ۲۴V نه؛ همچنان گیت ‎CHG_MASTER_ENABLE=1‎، Vin >= ۲۲۰۰۰mV و
 *      همهٔ حفاظت‌های عددی. (شکل‌موج‌های ‎gate/shunt‎ در مرحلهٔ ۱۰٪ با اسکوپ
 *      تأیید و مسیر جریان کالیبره شده است.)
 */
#define CHG_BRINGUP_TEST_ENABLE                0u   /* [EN] bring-up finished; normal charge active / bring-up تمام شد، شارژ نرمال فعال است */
#define CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE    100u  /* [EN] dormant: 10% stage verified on board / غیرفعال: مرحله ۱۰٪ روی برد تأیید شده */
#define CHG_BRINGUP_TEST_SOURCE_LIMIT_MA      150u  /* [EN] dormant: 150 mA source limit / غیرفعال: حد منبع ۱۵۰mA */
#define CHG_BRINGUP_TEST_FULL_STAGE_MAX_DUTY  100u  /* [EN] 10% after waveform confirmation / ۱۰٪ بعد از تأیید شکل‌موج */
#define CHG_BRINGUP_TEST_FULL_STAGE_LIMIT_MA  100u  /* [EN] 100 mA after waveform confirmation / ۱۰۰mA بعد از تأیید شکل‌موج */

/* ==================== Electrical policy / سیاست الکتریکی ==================== */
#define CHG_PWM_FREQUENCY_HZ          50000u
#define CHG_PWM_TIMER_CLOCK_HZ        72000000u
#define CHG_PWM_PRESCALER             0u
#define CHG_PWM_AUTO_RELOAD           1439u
#define CHG_ABSORB_MV                 14400u
/* [EN] Absorb as a voltage-hold window (user directive 2026-09-19): enter the
 *      window at CHG_ABSORB_ENTER_MV, hold the 14.4 V setpoint with fine
 *      0.1% duty steps (CHG_DUTY_STEP_FINE_PERMILLE) instead of the coarse
 *      0.5% steps so the voltage stays put; the 10-minute soak counts while
 *      the WHOLE ABSORB state lasts (his final directive), a dip below 14.3 V
 *      returns to BULK and RESETS the soak, and overshoot above
 *      CHG_ABSORB_OVER_MV (14.6 V) gets coarse 0.5% down-steps to come back
 *      fast. Still safely below FAULT_BAT_DISCONNECT_MV (14.8 V).
 * [FA] ابزورب به‌صورت پنجره تثبیت ولتاژ: ورود ۱۴٫۳V، تثبیت ۱۴٫۴V با پلهٔ
 *      ریز ۰٫۱٪ به‌جای ۰٫۵٪؛ شستشوی ۱۰ دقیقه‌ای کل مدتِ حالت ابزورب
 *      جمع می‌شود (دستور نهایی)، زیر ۱۴٫۳V برگشت به بالک + ریست، بالای
 *      ۱۴٫۶V کاهش سریع ۰٫۵٪. */
#define CHG_ABSORB_ENTER_MV           14300u
#define CHG_ABSORB_OVER_MV            14600u
#define CHG_DUTY_STEP_FINE_PERMILLE       1u
#define CHG_FLOAT_MV                  13500u
#define CHG_REENTRY_MV               12800u
/* [EN] How long a channel must CONTINUOUSLY SEE battery voltage before
 *      a bulk start is allowed (user 2026-09-19: "10..20 s to settle, then
 *      start"; the count keys on BATTERY voltage only - input validity
 *      gates the bulk start separately). 15 s = middle of the window.
 *      Mid-cycle paths (JIT resume, 12.8 V reentry) are exempt (their
 *      presence stamp is already live). This gate also kills the bat-lost
 *      FLAP (clear -> BULK -> re-pump -> set again every 30 s, with the
 *      yellow blink): with the cable out the voltage is invalid, the
 *      stamp stays 0, BULK never re-arms.
 * [FA] چند ثانیه «دیده‌شدن پیوستهٔ ولتاژ باتری» برای مجوز شروع بالک (دستور
 *      کاربر: ۱۰..۲۰ ثانیه ثبات، بعد شارژ؛ شمارش فقط با ولتاژ باتری -
 *      ورودی گیت جداگانه دارد). ۱۵ ثانیه وسط پنجره. مسیرهای میان‌چرخه
 *      معاف‌اند. همین گیت چرخهٔ پینگ‌پنگ قطع‌باتری و چشمک زردش را هم
 *      می‌کشد: با کابل بیرون ولتاژ نامعتبر است، مهر صفر می‌ماند و بالک
 *      مسلح نمی‌شود.
 */
#define CHG_CONNECT_SETTLE_MS        15000u
#define CHG_BULK_CURRENT_MAX_MA       650u
/* [EN] TEMPORARY bench diagnostic (2026-09-18): fixed 15% duty, NO
 *      ramp, NO band regulation - one stable operating point to calibrate
 *      the measurement coefficients (scope MEAN at LM358 out, bench V/I
 *      vs. the firmware readings). All usual gates stay (snapshot,
 *      Vin >= 22000 mV, battery sense, JIT, >950 mA hard fault) and
 *      switching stops while Vbat >= 14.4 V (no overcharge with regulation
 *      off). Set 0 to return to normal charge control.
 * [FA] تست موقت بنچ: دیوتی ثابت ۱۵٪، بدون رمپ و باند - یک نقطهٔ پایدار
 *      برای کالیبره‌کردن ضرایب اندازه‌گیری. همهٔ گیت‌ها فعال و روی ۱۴٫۴V
 *      سوئیچینگ می‌ایستد. برای شارژ نرمال مقدار را ۰ کن.
 */
#define CHG_FIXED_DUTY_TEST_ENABLE              0u  /* [EN] 1=fixed 15% duty diagnostic (DONE, coefficients locked); 0=normal charge / تست تمام شد، شارژ نرمال فعال */
#define CHG_FIXED_DUTY_TEST_DUTY_PERMILLE     150u
/* [EN] CHG_CURRENT_LIMIT_MA (675) was deleted on 2026-10-03: a grep of the
 *      whole firmware found zero readers, so it had been describing a limit
 *      that did not exist since the step chain was removed in v1.23. It was
 *      a candidate for the new panel-settable limit block until that grep -
 *      publishing it would have given the panel a control wired to nothing,
 *      which is worse than leaving it out.
 * [FA] این ثابت در ۲۰۲۶-۱۰-۰۳ حذف شد: جست‌وجو در کل فرم‌ور هیچ خواننده‌ای
 *      پیدا نکرد، یعنی از حذف زنجیرهٔ پله‌ای در v1.23 حدی را توصیف می‌کرد که
 *      وجود نداشت. تا پیش از آن جست‌وجو نامزد بلوک حدهای تنظیم‌شدنی بود -
 *      منتشر کردنش یعنی دادن کنترلی به پنل که به هیچ‌چیز وصل نیست، و این از
 *      نگذاشتنش بدتر است. */
/* [EN] The 630..650 mA hysteresis band that the deleted step chain used is
 *      gone with it (v1.23): a PID has no band, it has a setpoint
 *      (CHG_BULK_CURRENT_MAX_MA minus CHG_PID_CURRENT_MARGIN_MA = 640 mA)
 *      and it sits on it. CHG_BULK_CURRENT_MAX_MA now has a second job as
 *      the hard over-current BACKSTOP inside the PID (user order
 *      2026-09-29), while the hard-fault trip stays what it always was:
 *      the trip that cuts the channel and raises the fault. Its factory
 *      value is still 950 mA; only its settable ceiling moved.
 * [FA] باند هیسترزیس ۶۳۰..۶۵۰ میلی‌آمپر که زنجیرهٔ پله‌ای حذف‌شده استفاده
 *      می‌کرد با خودش رفت (v1.23): PID باند ندارد، ست‌پوینت دارد
 *      (CHG_BULK_CURRENT_MAX_MA منهای CHG_PID_CURRENT_MARGIN_MA = ۶۴۰
 *      میلی‌آمپر) و روی همان می‌نشیند. حالا CHG_BULK_CURRENT_MAX_MA کار
 *      دومی هم دارد: پشتیبان سخت اضافه‌جریان داخل PID (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۹)، و تریپ خطای سخت همان چیزی می‌ماند که بود: تریپی که
 *      کانال را قطع و خطا را بلند می‌کند. مقدار کارخانه‌اش هنوز ۹۵۰
 *      میلی‌آمپر است؛ فقط سقف تنظیم‌شدنی‌اش جابه‌جا شد. */
/* [EN] ===== Settable-current envelope (USER-ORDERED LOGIC CHANGE
 *      2026-10-03: "why do the numbers have limits? I cannot raise the hard
 *      fault current - my hand has to be free; my batteries may be in
 *      parallel and I may want more current") =====
 *
 *      The old 950 mA was NOT a hardware limit. It was a compile-time
 *      preference, written when a single 7 Ah battery per channel was the
 *      only case, and it silently capped the panel. Three separate things
 *      were being conflated, and only one of them is physics:
 *
 *        1. MAGNETICS. Protected by CHG_DUTY_MAX_PERMILLE (50 %), which is
 *           a property of the board's transformer, not a taste. UNCHANGED
 *           and still not settable - raising the trip level cannot make the
 *           converter push more through the core than the duty cap allows.
 *        2. MEASURABILITY. A trip the ADC can never reach is not a
 *           protection, it is decoration. The chain saturates at 4095
 *           counts = 3594 mA chain current (bsp_measurement.c
 *           ConvertCurrent: counts x 24200 / 27573). Through the channel
 *           LUTs and divided by the HIGHEST valid battery voltage - the
 *           worst case, because battery current = power / voltage - that
 *           is 3534 mA on channel 1 and 4124 mA on channel 2. So any
 *           ceiling at or below 3534 mA is a number the firmware can
 *           actually observe. That is the real bound, and the one used.
 *        3. POLICY. Where the user wants the charger to shout. That is the
 *           user's call, not the firmware's, and it is now settable across
 *           the whole measurable range.
 *
 *      CALIBRATED vs MEASURABLE - the honest caveat. The LUTs are fitted
 *      from bench data only up to chain 640 mA (ch1) and 707 mA (ch2),
 *      which at 14.4 V is 631 mA and 749 mA of battery current. Above the
 *      last anchor calibration.h extends the last slope, so readings there
 *      are extrapolated, not measured. Extrapolation is fine for a trip
 *      level - it only has to be monotonic to fire - but the panel marks
 *      where calibration ends so a current set above it is an informed
 *      choice rather than a hidden one.
 *
 *      Parallel batteries, which is what prompted this, need more CURRENT
 *      at the SAME voltage. The voltage ceilings (OV cutoff, absorb, the
 *      15 V valid-range limit) are therefore deliberately NOT raised:
 *      paralleling does not change battery chemistry, and a 12 V lead-acid
 *      string damaged above 15 V is damaged whether there is one of them
 *      or four.
 *
 * [FA] ===== پوشش جریان تنظیم‌شدنی (تغییر منطق به دستور کاربر ۲۰۲۶-۱۰-۰۳:
 *      «چرا اعداد محدودیت دارن؟ نمی‌تونم جریان خطای سخت رو ببرم بالاتر؟
 *      باید دستم باز باشه؛ ممکنه باتری‌هام موازی باشن») =====
 *
 *      عدد ۹۵۰ قدیمی «حد سخت‌افزار» نبود؛ یک ترجیح زمان کامپایل بود که
 *      وقتی نوشته شد تنها حالت ممکن یک باتری ۷ آمپرساعتی در هر کانال بود،
 *      و بی‌صدا پنل را سقف می‌زد. سه چیز جداگانه با هم قاطی شده بودند و
 *      فقط یکی‌شان فیزیک است:
 *        ۱) مغناطیس: با سقف duty پنجاه درصد محافظت می‌شود که مشخصهٔ ترانس
 *           برد است نه سلیقه. دست‌نخورده و همچنان غیرقابل تنظیم.
 *        ۲) قابلیت اندازه‌گیری: تریپی که ADC هرگز به آن نمی‌رسد محافظت
 *           نیست، تزئین است. زنجیره در ۴۰۹۵ شمارش اشباع می‌شود و از مسیر
 *           جدول‌ها و تقسیم بر بالاترین ولتاژ معتبر (بدترین حالت) به
 *           ۳۵۳۴ میلی‌آمپر در کانال ۱ می‌رسد. پس هر سقفی تا ۳۵۳۴ عددی است
 *           که فرم‌ور واقعاً می‌تواند ببیند. همین حد واقعی است.
 *        ۳) سیاست: اینکه کاربر کجا بخواهد شارژر فریاد بزند. این تصمیم
 *           کاربر است نه فرم‌ور، و حالا در تمام بازهٔ قابل اندازه‌گیری
 *           تنظیم‌شدنی است.
 *      نکتهٔ صادقانه: جدول‌ها فقط تا ۶۳۱ و ۷۴۹ میلی‌آمپر (در ۱۴٫۴ ولت)
 *      برازش شده‌اند و بالاتر از آن شیب آخر ادامه می‌یابد، یعنی برون‌یابی.
 *      برای یک سطح تریپ کافی است، ولی پنل لبهٔ کالیبراسیون را علامت می‌زند
 *      تا انتخاب بالاتر از آن آگاهانه باشد.
 *      باتری موازی جریان بیشتر در همان ولتاژ می‌خواهد، پس سقف‌های ولتاژ
 *      عمداً بالا نرفتند: موازی‌بستن شیمی باتری را عوض نمی‌کند. */

/* [EN] 4095 counts x 24200 / 27573 - the chain's full-scale current.
 * [FA] جریان تمام‌مقیاس زنجیره. */
#define CHG_CHAIN_FULL_SCALE_MA        ((4095u * 24200u) / 27573u)
/* [EN] Worst-case (channel 1, at the highest valid battery voltage)
 *      battery current the chain can still represent. Derived above.
 * [FA] بدترین‌حالتِ جریان باتری که زنجیره هنوز می‌تواند نمایش دهد. */
#define CHG_CURRENT_MEASURABLE_MAX_MA  3534u
/* [EN] The settable ceiling. Below the measurable maximum so every value
 *      the panel can ask for is one the firmware can actually observe.
 * [FA] سقف تنظیم‌شدنی. زیر بیشینهٔ قابل اندازه‌گیری. */
#define CHG_CURRENT_HARD_FAULT_MAX_MA  3000u
/* [EN] Where the LUT fit stops and extrapolation begins, at 14.4 V. The
 *      panel draws this so the user can see the edge of calibrated data.
 * [FA] جایی که برازش جدول تمام و برون‌یابی شروع می‌شود (در ۱۴٫۴ ولت). */
#define CHG_CURRENT_CALIBRATED_MA      631u
/* [EN] The old single name CHG_CURRENT_HARD_FAULT_MA is deliberately NOT
 *      kept as an alias. It was used for two different jobs - the power-on
 *      value of the trip AND the ceiling the panel is clamped to - which
 *      was harmless only while they were the same number. Opening the
 *      ceiling makes them different, and an alias would have silently
 *      shipped a 3000 mA factory default. Every use site now says which
 *      one it means.
 * [FA] نام قدیمی عمداً به‌صورت alias نگه داشته نشد. آن یک نام دو کار
 *      می‌کرد - مقدار روشن‌شدن تریپ و سقفی که پنل به آن گیره می‌خورد - و
 *      این فقط تا وقتی بی‌خطر بود که هر دو یک عدد بودند. بازکردن سقف
 *      آن‌ها را جدا می‌کند و alias بی‌صدا پیش‌فرض کارخانه را ۳۰۰۰ می‌کرد. */
/* [EN] Factory default stays where it was: opening a range must not move
 *      anyone's working setup. A board that is flashed and never touched
 *      behaves exactly as before.
 * [FA] پیش‌فرض کارخانه همان‌جا می‌ماند: بازکردن یک بازه نباید تنظیمات کاری
 *      کسی را جابه‌جا کند. */
#define CHG_CURRENT_HARD_FAULT_DEFAULT_MA  950u
_Static_assert(CHG_CURRENT_HARD_FAULT_MAX_MA <= CHG_CURRENT_MEASURABLE_MAX_MA,
               "hard-fault ceiling above what the ADC chain can represent");
_Static_assert(CHG_CURRENT_HARD_FAULT_DEFAULT_MA <= CHG_CURRENT_HARD_FAULT_MAX_MA,
               "factory default above its own ceiling");
_Static_assert(CHG_CURRENT_CALIBRATED_MA < CHG_CURRENT_HARD_FAULT_DEFAULT_MA,
               "calibrated edge must sit below the default trip");
/* [EN] Battery-current estimate architecture v1.3 (user order
 *      2026-09-24). The sense chain turned out to be battery-side, so
 *      with the 2026-09-24 battery-calibrated gains the filtered reading
 *      already IS the battery current. Instead of deleting the old
 *      primary->output conversion, it stays as an EXPLICIT,
 *      panel-calibratable stage: ETA1/ETA2 (ESP params 9/10) are
 *      per-channel factors, DEFAULT 0 = identity (a reflash changes no
 *      number until the user calibrates). Non-zero: iest = I_filtered x
 *      Vin x eta / (1000 x Vbat) with LIVE voltages, so the reading stays
 *      true while Vbat moves during a charge (identity drifts by
 *      Vbat_cal/Vbat). Calibration is ONE ESP command (CAL_REFERENCE,
 *      protocol v1.3): the user types the battery-side DMM mA and the
 *      firmware computes eta from its own live snapshot; with the
 *      battery-calibrated gains it lands near 1000 x Vbat / Vin (~537).
 *      The legacy 758/242 permille (bench 2026-09-22, wrong
 *      shunt-in-MOSFET-source assumption) feed no calculation - kept only
 *      as inert defaults for the eta param read-back; ch2's 242 also
 *      absorbs the ch2 sense chain over-reading (~2.9x), do not reuse it
 *      as a physical constant.
 * [FA] معماری تخمین جریان باتری v1.3 (دستور ۲۰۲۶-۰۹-۲۴): زنجیرهٔ sense سمت
 *      باتری است، پس با گین‌های کالیبره-باتری عدد فیلترشده خودش جریان
 *      باتری است. تبدیل قدیمی اولیه→خروجی به‌جای حذف، مرحلهٔ صریحِ
 *      قابل‌کالیبره ماند: ‎ETA1/ETA2 (‎پارامتر ۹/۱۰) ضریب هر کانال با
 *      پیش‌فرض ۰ = همانی (ریفلش عددی را عوض نمی‌کند). غیرصفر: ‎iest = I‎ ×
 *      Vin × η ÷ (۱۰۰۰ × Vbat) با ولتاژهای زنده تا خوانش با حرکت Vbat
 *      درست بماند. کالیبراسیون یک فرمان ESP (CAL_REFERENCE): کاربر عدد
 *      مولتی‌متر سمت باتری را می‌دهد و فریم‌ور η را از snapshot خودش
 *      می‌سازد؛ با گین‌های جدید نزدیک ۱۰۰۰×Vbat÷Vin (~۵۳۷) می‌افتد.
 *      مقادیر قدیمی ۷۵۸/۲۴۲ (فرض اشتباه شانت-در-سورس) در هیچ محاسبه‌ای
 *      نیستند - فقط پیش‌فرض بی‌اثر برای خوانده‌شدن پارامتر؛ ۲۴۲ کانال ۲
 *      خطای ‎over-read‎ زنجیرهٔ sense (~۲٫۹×) را هم جذب کرده - ثابت فیزیکی
 *      نیست. */
#define CHG_FLYBACK_ETA1_PERMILLE            0u
#define CHG_FLYBACK_ETA2_PERMILLE            0u

/* [EN] Clamp limits of the ESP-adjustable runtime conversion factor
 *      (v1.3): 0 = identity bypass (compiled default), 1..999 = the live
 *      Vin/Vbat conversion. The panel may zero it to return to the raw
 *      filtered reading at any time.
 * [FA] حدود گیرهٔ ضریب تبدیل زمان اجرای قابل‌تنظیم از ESP (v1.3): صفر =
 *      همانی/گذر (پیش‌فرض کامپایل)، ۱..۹۹۹ = تبدیل زندهٔ ‎Vin/Vbat‎. پنل
 *      می‌تواند هر وقت خواست صفرش کند تا به خوانش فیلترشدهٔ خام برگردد. */
/* [EN] ETA vs the ch2 bench LUT (full-program audit 2026-09-27): ch2
 *      already yields true battery current through the power LUT in
 *      measurement.c - keep ETA2 at 0 or the current converts twice
 *      (LUT shape x power factor). ETA calibration is for channels
 *      without a bench table (ch1 until its SOLO1 data arrives).
 * [FA] نسبت ETA با جدول بنچ کانال ۲ (ممیزی کل برنامه): کانال ۲ با
 *      خروجی جدول توانی measurement.c همان جریان واقعی باتری است -
 *      ETA آن صفر بماند تا جریان دو بار تبدیل نشود.
 *      کالیبرهٔ ETA برای کانال بدون جدول بنچ است. */
#define CHG_ETA_MIN_PERMILLE                  0u
#define CHG_ETA_MAX_PERMILLE                  999u

/* [EN] Live-voltage sanity floors for the conversion (v1.3): below these
 *      the estimate falls back to the identity instead of dividing a
 *      garbage snapshot. Also used by the ESP CAL_REFERENCE handler.
 * [FA] کف‌های سلامت ولتاژ زنده برای تبدیل (v1.3): زیر این مقادیر تخمین
 *      به‌جای تقسیم snapshot بی‌معنی به همانی برمی‌گردد. هندلر
 *      CAL_REFERENCE ‌ی ESP هم از همین‌ها استفاده می‌کند. */
#define CHG_ETA_MIN_VIN_MV                10000u
#define CHG_ETA_MIN_VBAT_MV                5000u
#define CHG_INPUT_VALID_MV           22000u
#define CHG_DUTY_START_PERMILLE        10u
#define CHG_DUTY_STEP_PERMILLE          5u
/* [EN] The only fixed-step cadence left after v1.23. The BULK/ABSORB step
 *      chain and its four other intervals were deleted with the legacy
 *      regulator (user order 2026-09-29: "remove the previous logic, the
 *      charger must be PID only"); what survives is the FLOAT park-down,
 *      which is not regulation - it walks the duty to zero at one 0.5%
 *      step per CHG_DUTY_RAMP_DOWN_INTERVAL_MS and leaves it there. The PID
 *      is explicitly invalidated while that runs, so the next reentry seeds
 *      from the real hardware duty rather than a stale integral.
 * [FA] تنها ضرب‌آهنگ پله‌ای باقی‌مانده بعد از v1.23. زنجیرهٔ پله‌ای
 *      بالک/ابزورب و چهار بازهٔ دیگرش با تنظیم‌کنندهٔ قدیمی حذف شدند (دستور
 *      کاربر ۲۰۲۶-۰۹-۲۹: «منطق قبلی را بردار، شارژر فقط PID باشد»)؛ آنچه
 *      مانده پارک‌کردن در فلوت است که تنظیم نیست - دیوتی را با پلهٔ ۰٫۵٪ در
 *      هر CHG_DUTY_RAMP_DOWN_INTERVAL_MS تا صفر می‌برد و همان‌جا رها می‌کند.
 *      PID در طول آن عمداً باطل می‌شود تا بازگشت بعدی از دیوتی واقعی
 *      سخت‌افزار بذر بگیرد نه از انتگرال کهنه. */
#define CHG_DUTY_RAMP_DOWN_INTERVAL_MS  500u

/* ==================== Two-loop CC/CV PID regulator (v1.22) ====================
 * [EN] USER ORDER 2026-09-28: "slow the absorb duty rise down, and instead of
 *      all that complexity write a two-loop CC/CV PID - one gain set at the
 *      start, one in the middle, one in the last region - and expose it on
 *      the ESP panel." This block replaces the fixed-step bang-bang chain
 *      (0.5%/0.1% steps on 500/1000/2000 ms timers) with ONE positional PID
 *      core running a textbook CC/CV min-select:
 *
 *        branch C (current) : error = current setpoint - measured current,
 *                             ALWAYS uses the stage-1 gain row.
 *        branch V (voltage) : error = absorb setpoint - measured voltage,
 *                             uses the stage-2 row while Vbat is BELOW the
 *                             setpoint and the stage-3 row once it is at or
 *                             above it.
 *
 *      Both branches are evaluated every update and the one asking for the
 *      SMALLER duty wins; the winner alone drives the shared integrator and
 *      its own slew limits. That is why each loop needs its own gain row:
 *      a volt of voltage error and an amp of current error are different
 *      physical quantities, so one shared Kp would make the comparison
 *      meaningless and the CC->CV knee would never happen (simulated: the
 *      pack sails to 14.6 V before the voltage branch ever wins).
 *
 *      So the three user-visible "stages" are, in charging order:
 *        stage 1 "start"  = the constant-current / bulk loop (branch C)
 *        stage 2 "middle" = the voltage loop climbing to the setpoint - the
 *                           region whose rise the user asked to slow down
 *        stage 3 "last"   = the voltage loop holding at / backing off from
 *                           the setpoint
 *      The state machine (BULK/ABSORB/FLOAT, soak, taper, dip, reentry) and
 *      EVERY protection (OV cutoff, hard current limit, JIT, battery-valid,
 *      duty ceiling, FLOAT parks at zero) are untouched: the PID only
 *      decides the duty number inside the window they already allow.
 *      Setting id 83 to 0 restores the legacy step regulator unchanged.
 * [FA] دستور کاربر ۲۰۲۶-۰۹-۲۸: «سرعت رشد دیوتی در ابزورب کمتر شود و به‌جای
 *      این همه پیچیدگی یک PID دوحلقه‌ای بنویس - اولش یک سری ضریب، وسطش
 *      یک سری، ناحیهٔ آخرش هم یک سری دیگر - و این تنظیمات در ESP هم بیاید.»
 *      این بلوک زنجیرهٔ پله‌ثابت (۰٫۵٪/۰٫۱٪ روی تایمرهای ۵۰۰/۱۰۰۰/۲۰۰۰ms) را
 *      با یک هستهٔ PID موقعیتی جایگزین می‌کند که کمینه‌گیری کلاسیک ‎CC/CV‎
 *      انجام می‌دهد: شاخهٔ جریان (خطا = ست‌پوینت جریان منهای جریان اندازه‌گیری
 *      شده) همیشه ردیف ضریب مرحلهٔ ۱ را می‌خواند؛ شاخهٔ ولتاژ (خطا = ست‌پوینت
 *      ابزورب منهای ولتاژ) وقتی ولتاژ زیر ست‌پوینت است ردیف مرحلهٔ ۲ و وقتی
 *      روی آن یا بالاتر است ردیف مرحلهٔ ۳ را می‌خواند. هر به‌روزرسانی هر دو
 *      شاخه حساب می‌شوند و هرکدام دیوتی کمتری بخواهد برنده است؛ فقط برنده،
 *      انتگرال‌گیر مشترک و سقف شیب خودش را می‌راند. دلیل جدا بودن ردیف‌ها همین
 *      است: یک ولت خطای ولتاژ و یک آمپر خطای جریان دو کمیت فیزیکی متفاوت‌اند،
 *      پس با Kp مشترک مقایسه بی‌معنا می‌شد و زانوی CC→CV هرگز رخ نمی‌داد
 *      (در شبیه‌سازی: پک تا ۱۴٫۶ ولت بالا می‌رفت و شاخهٔ ولتاژ هیچ‌وقت برنده
 *      نمی‌شد). پس سه «مرحلهٔ» قابل دیدن کاربر به ترتیب شارژ این‌هاست:
 *      مرحلهٔ ۱ «شروع» = حلقهٔ جریان ثابت/بالک، مرحلهٔ ۲ «وسط» = حلقهٔ ولتاژ
 *      در حال بالا رفتن به سمت ست‌پوینت (همان ناحیه‌ای که کاربر خواست کندتر
 *      شود)، مرحلهٔ ۳ «آخر» = حلقهٔ ولتاژ روی ست‌پوینت و عقب‌نشینی از آن.
 *      ماشین حالت و همهٔ حفاظت‌ها دست‌نخورده‌اند؛ PID فقط عدد دیوتی را داخل
 *      پنجره‌ای که آن‌ها اجازه داده‌اند تعیین می‌کند. این تنها
 *      تنظیم‌کنندهٔ دیوتی است؛ مسیر پله‌ای قدیمی حذف شده و راه برگشتی
 *      وجود ندارد (دستور کاربر ۲۰۲۶-۰۹-۲۹). */

/* [EN] Update cadence. The control task still runs every 10 ms, but the PID
 *      math advances once per CHG_PID_PERIOD_MS - one window of the
 *      current filter (median-3 + average-10 = ~100 ms), so the loop never
 *      reacts to a value the filter has not finished forming.
 * [FA] ضرب‌آهنگ به‌روزرسانی: تسک کنترل همان هر ۱۰ms اجرا می‌شود ولی ریاضی
 *      PID هر CHG_PID_PERIOD_MS یک‌بار جلو می‌رود - یک پنجرهٔ فیلتر جریان
 *      (~۱۰۰ms) تا حلقه به عددی که فیلتر هنوز نساخته واکنش ندهد. */
#define CHG_PID_PERIOD_MS               100u
#define CHG_PID_DT_MIN_MS                10u
#define CHG_PID_DT_MAX_MS              1000u

/* [EN] Internal duty resolution: milli-permille (1 unit = 0.001 permille =
 *      0.0001%). The PWM stage takes whole permille, so the fine units are
 *      what let a 0.03 permille/s creep exist at all - the integrator moves
 *      inside the unit and the applied duty steps when it crosses.
 * [FA] وضوح داخلی دیوتی: میلی‌پرمیل (هر واحد ۰٫۰۰۱ پرمیل). سخت‌افزار PWM
 *      پرمیل صحیح می‌گیرد، پس همین واحد ریز است که خزش ۰٫۰۳ پرمیل بر ثانیه
 *      را ممکن می‌کند: انتگرال‌گیر داخل واحد حرکت می‌کند و دیوتی اعمالی با
 *      عبور از مرز یک پله می‌خورد. */
#define CHG_PID_DUTY_SCALE             1000u

/* [EN] Gain units (integers only, panel-friendly, no floats in firmware).
 *      Each term has its OWN divider so every gain lands on a round, human
 *      number instead of a five-digit one:
 *        P term  [milli-permille]   = Kp x error / CHG_PID_KP_DIV
 *        I rate  [milli-permille/s] = Ki x error / CHG_PID_KI_DIV
 *        D term  [milli-permille]   = Kd x (error - previous) / CHG_PID_KD_DIV
 *      error is mV in the voltage branch and mA in the current branch, so
 *      with KP_DIV = 1 the readable unit of Kp is "permille of duty per
 *      volt of error" (voltage branch) or "permille per amp" (current
 *      branch), and with KI_DIV = 1000 the unit of Ki is "milli-permille
 *      per second per millivolt" - Ki = 1000 means one permille per second
 *      for every volt of error.
 *      Worked example (defaults, stage 2): 100 mV under the setpoint gives
 *      an integral rate of 300 x 100 / 1000 = 30 milli-permille/s =
 *      0.03 permille/s - SLOWER than the legacy 0.1 permille per 2000 ms
 *      (0.05 permille/s) the user asked to slow down, and it fades to zero
 *      as the error closes instead of stepping over the setpoint.
 * [FA] واحد ضرایب (فقط عدد صحیح، بدون ممیز شناور در فریم‌ور). هر جمله
 *      تقسیم‌کنندهٔ خودش را دارد تا ضرایب عددهای گرد و انسانی بمانند:
 *      جملهٔ ‎P = Kp‎×خطا÷CHG_PID_KP_DIV، نرخ ‎I = Ki‎×خطا÷CHG_PID_KI_DIV،
 *      جملهٔ ‎D = Kd‎×تغییر خطا÷CHG_PID_KD_DIV. خطا در شاخهٔ ولتاژ mV و در
 *      شاخهٔ جریان mA است، پس با KP_DIV=۱ واحد خواندنی Kp می‌شود «پرمیل
 *      دیوتی به ازای هر ولت خطا» (شاخهٔ ولتاژ) یا «پرمیل به ازای هر آمپر»
 *      (شاخهٔ جریان)، و با KI_DIV=۱۰۰۰ واحد Ki می‌شود «میلی‌پرمیل بر ثانیه
 *      به ازای هر میلی‌ولت» یعنی Ki=۱۰۰۰ یعنی یک پرمیل بر ثانیه به ازای هر
 *      ولت خطا. مثال با پیش‌فرض مرحلهٔ ۲: ۱۰۰mV زیر ست‌پوینت یعنی نرخ
 *      ۳۰۰×۱۰۰÷۱۰۰۰ = ۳۰ میلی‌پرمیل بر ثانیه = ۰٫۰۳ پرمیل بر ثانیه - کندتر
 *      از ۰٫۰۵ پرمیل بر ثانیهٔ قدیمی که کاربر خواست کم شود، و با بسته‌شدن
 *      خطا خودش صفر می‌شود به‌جای اینکه از ست‌پوینت رد شود. */
#define CHG_PID_KP_DIV                    1u
#define CHG_PID_KI_DIV                 1000u
#define CHG_PID_KD_DIV                    1u
#define CHG_PID_GAIN_MAX              20000u

/* [EN] Error saturation before any gain is applied (a 24 V pack read on a
 *      12 V channel, or a first-frame garbage current, must not slam the
 *      integrator). Both branches share it; mV and mA both fit.
 * [FA] اشباع خطا پیش از اعمال ضریب (خوانش خراب نباید انتگرال‌گیر را بکوبد)؛
 *      مشترک هر دو شاخه و برای mV و mA کافی است. */
#define CHG_PID_ERROR_CLAMP            4000

/* [EN] Slew limits in milli-permille per second. IMPORTANT: they cap the
 *      rate of the INTEGRAL (the operating point the loop is walking
 *      toward), not the finished output. Rate-limiting the finished output
 *      instead looks equivalent but is not: the P term ripples by a few
 *      milli-permille every time the applied duty quantises to the next
 *      whole permille, and an output limiter with a fast down-rate and a
 *      slow up-rate RECTIFIES that ripple into a steady downward ratchet -
 *      simulated, the loop stalled at 268 mA and never reached the 640 mA
 *      bulk band. Limiting the integral leaves the ripple zero-mean and the
 *      ramp rate exactly what the user dialled in.
 *      One up-rate and one down-rate per stage: the stage-2 up-rate is the
 *      user's "grow slower in absorb" knob, and the default down-rates of
 *      1000 = 1 permille/s match the legacy coarse escape rate (0.5
 *      permille per 500 ms) so backing off is never slower than before.
 * [FA] حد شیب بر حسب میلی‌پرمیل بر ثانیه. نکتهٔ مهم: این سقف روی نرخ
 *      «انتگرال» (نقطهٔ کاری که حلقه به سمتش راه می‌رود) است نه روی خروجی
 *      نهایی. سقف‌گذاری روی خروجی نهایی شبیه همین به نظر می‌رسد ولی نیست:
 *      هر بار دیوتی اعمالی به پرمیل صحیح بعدی گرد می‌شود جملهٔ P چند
 *      میلی‌پرمیل ریپل می‌خورد، و محدودکنندهٔ خروجی با نرخ نزول تند و نرخ
 *      صعود کند آن ریپل را «یکسوسازی» می‌کند و به یک جغجغهٔ رو به پایین
 *      تبدیل می‌کند - در شبیه‌سازی حلقه روی ۲۶۸mA گیر کرد و هرگز به باند
 *      ۶۴۰mA بالک نرسید. با سقف‌گذاری روی انتگرال، ریپل میانگین‌صفر می‌ماند
 *      و نرخ رمپ دقیقاً همانی می‌شود که کاربر تنظیم کرده. هر مرحله یک نرخ
 *      صعود و یک نرخ نزول دارد: نرخ صعود مرحلهٔ ۲ همان کلید «در ابزورب
 *      آهسته‌تر رشد کن» کاربر است، و پیش‌فرض نزول ۱۰۰۰ = ۱ پرمیل بر ثانیه
 *      برابر نرخ فرار قدیمی است پس عقب‌نشینی هرگز کندتر از قبل نیست. */
#define CHG_PID_RATE_MIN                 10u
#define CHG_PID_RATE_MAX              20000u

/* [EN] The current branch aims BELOW the profile ceiling by this margin, so
 *      the loop settles in the middle of the old 630..650 mA band instead
 *      of resting exactly on its top edge (a setpoint sitting on the limit
 *      would let every noise sample ask for a duty cut).
 * [FA] شاخهٔ جریان به اندازهٔ این حاشیه زیر سقف پروفایل را هدف می‌گیرد تا
 *      حلقه وسط باند قدیمی ۶۳۰..۶۵۰ بنشیند نه دقیقاً روی لبهٔ بالایش
 *      (ست‌پوینت روی خود حد یعنی هر نمونهٔ نویزی تقاضای کاهش دیوتی می‌کند). */
#define CHG_PID_CURRENT_MARGIN_MA        10u

/* [EN] Boot defaults, one row per stage (Kp, Ki, Kd, up-rate, down-rate).
 *      Tuned against a flyback + lead-acid plant model (DCM transfer
 *      i = 230.4 x D^2 / V, 0.15 ohm series resistance, an exponential
 *      gassing sink that puts the absorb operating point near 50 mA /
 *      56 permille) and re-checked over 0.05..0.30 ohm, a new pack and a
 *      worn pack, and +/-20 mV of sensor noise:
 *        stage 1 current 20 / 800 / 0 /  500 / 1000
 *              -> Kp small on purpose. The current branch sees about
 *                 7 mA of change per applied permille at the bulk operating
 *                 point, so a big Kp would swing the P term by more than a
 *                 whole permille on every quantisation step and the loop
 *                 would chatter instead of climb. 800 gives the familiar
 *                 0.5 permille/s soft ramp at a 640 mA error and fades out
 *                 as the band is reached. Result: 633..640 mA held flat,
 *                 no reversals at all during bulk.
 *        stage 2 voltage 150 / 300 / 0 /   30 / 1000
 *              -> 0.03 permille/s at 100 mV below the setpoint: the slowed
 *                 absorb rise the user asked for, proportional so it eases
 *                 off further as the setpoint is approached.
 *        stage 3 voltage 300 / 12000 / 0 / 10 / 1000
 *              -> the setpoint region needs real authority: the duty has to
 *                 fall from ~190 to ~56 permille as the pack stops taking
 *                 current. Ki does that work (the down-rate still caps it
 *                 at 1 permille/s) while Kp stays modest so sensor noise is
 *                 not amplified into duty jitter. Simulated peak overshoot
 *                 +19 mV (14419 mV), settling to 14400 +/- 1 mV at 55..56
 *                 permille - far below the 14.6 V over-voltage step and the
 *                 14.8 V fault threshold.
 *      Kd = 0 on purpose: the current/voltage chain is filtered but still
 *      noisy, and a derivative on noise is duty jitter. The panel can raise
 *      it if the bench ever wants damping.
 * [FA] پیش‌فرض بوت، هر مرحله یک ردیف (Kp، Ki، Kd، نرخ صعود، نرخ نزول). با
 *      یک مدل فلای‌بک + باتری سرب‌اسید تیون شده (انتقال DCM با
 *      i = ۲۳۰٫۴×D²÷V، مقاومت سری ۰٫۱۵ اهم، و یک سینک گازدهی نمایی که نقطهٔ
 *      کار ابزورب را حدود ۵۰mA و ۵۶ پرمیل می‌نشاند) و روی ۰٫۰۵ تا ۰٫۳۰ اهم،
 *      باتری نو و فرسوده، و نویز ±۲۰mV سنسور بازبینی شده:
 *      مرحلهٔ ۱ جریان ۲۰/۸۰۰/۰/۵۰۰/۱۰۰۰ - Kp عمداً کوچک است: شاخهٔ جریان در
 *      نقطهٔ کار بالک حدود ۷ میلی‌آمپر تغییر به ازای هر پرمیل می‌بیند، پس Kp
 *      بزرگ یعنی جملهٔ P با هر پلهٔ گردکردن بیش از یک پرمیل کامل می‌جهد و
 *      حلقه به‌جای بالا رفتن می‌لرزد؛ ۸۰۰ همان رمپ نرم ۰٫۵ پرمیل بر ثانیه را
 *      با خطای ۶۴۰mA می‌دهد و نزدیک باند محو می‌شود (نتیجهٔ شبیه‌سازی:
 *      ۶۳۳ تا ۶۴۰ میلی‌آمپر صاف، بدون حتی یک بار تغییر جهت در کل بالک).
 *      مرحلهٔ ۲ ولتاژ ۱۵۰/۳۰۰/۰/۳۰/۱۰۰۰ - ۰٫۰۳ پرمیل بر ثانیه در ۱۰۰mV زیر
 *      ست‌پوینت: همان رشد کندشدهٔ ابزورب که کاربر خواست، و چون تناسبی است با
 *      نزدیک شدن به ست‌پوینت باز هم آرام‌تر می‌شود. مرحلهٔ ۳ ولتاژ
 *      ۳۰۰/۱۲۰۰۰/۰/۱۰/۱۰۰۰ - ناحیهٔ ست‌پوینت اقتدار واقعی می‌خواهد چون دیوتی
 *      باید از حدود ۱۹۰ به حدود ۵۶ پرمیل بیاید؛ این کار را Ki انجام می‌دهد
 *      (نرخ نزول باز هم آن را روی ۱ پرمیل بر ثانیه سقف می‌زند) و Kp متوسط
 *      می‌ماند تا نویز سنسور به لرزش دیوتی تبدیل نشود. اوج اورشوت شبیه‌سازی
 *      فقط ۱۹ میلی‌ولت (۱۴۴۱۹mV) و نشست روی ۱۴۴۰۰±۱ میلی‌ولت با ۵۵..۵۶
 *      پرمیل - بسیار پایین‌تر از پلهٔ اضافه‌ولتاژ ۱۴٫۶V و آستانهٔ فالت ۱۴٫۸V.
 *      Kd عمداً صفر است: زنجیرهٔ جریان/ولتاژ فیلتر شده ولی بی‌نویز نیست و
 *      مشتقِ نویز یعنی لرزش دیوتی؛ پنل هر وقت بنچ میرایی خواست بالا می‌برد. */
/* [EN] FACTORY CALIBRATION - TWO loops, ten numbers (user orders
 *      2026-09-29: "calibrate it yourself from the tables for the first
 *      time, so I can optimise it later", then "if we do it with one PID
 *      over the whole path does it not work? it does not matter if it is
 *      slow, because the battery itself is slow; maximum accuracy by the
 *      SIMPLEST method").
 *
 *      HOW SIMPLE CAN IT GET - measured, not argued (Firmware/Modules/Charger/Tester/pid_tuning_sim.py):
 *        1 shared row .... FAILS. A volt of voltage error and an amp of
 *                          current error are not the same quantity, so the
 *                          min-select compares millivolts with milliamps.
 *                          1408..84660 duty reversals per 10 h versus 4.
 *        1 voltage row + the 650 mA backstop as the only current limiter
 *                   ...... FAILS, and on the safety requirement itself: a
 *                          backstop is reactive, so current limit-cycles
 *                          and peaks at 707 mA instead of sitting on 640.
 *        2 rows ........... CORRECT. One PI for current, one PI for
 *                          voltage - textbook CC/CV, and the simplest
 *                          thing that actually regulates both.
 *        3 rows ........... no better. The extra at/above-setpoint row was
 *                          measured to buy nothing once the noise model
 *                          was broadband instead of a tone, so it is gone.
 *
 *      THE TEN NUMBERS, each from a sweep against the charge-profile table
 *      (14.4 V absorb, 650 mA bulk, 500 permille DCM ceiling):
 *        Kp_I 12     flat optimum 8..15; above ~45 the 7 mA-per-permille
 *                    plant quantisation makes the loop chatter
 *        Ki_I 1600   reaches the 640 mA band in 545 s from a flat pack with
 *                    ZERO duty reversals; 800 needed 1373 s and held a
 *                    looser 617..640 mA; 3000 got there in 350 s but cost
 *                    44 reversals
 *        Kp_V 50     the voltage loop's NOISE GAIN, and the reason this is
 *                    not 150: the pack voltage is unfiltered upstream, so
 *                    Kp multiplies raw ADC noise straight onto the duty.
 *                    With broadband +/-7 mV noise, Kp=150 gives 744 duty
 *                    reversals per 10 h, Kp=50 gives 62 - with identical
 *                    overshoot, identical hold error (0.5 mV) and the same
 *                    113 min to reach 14.4 V on the clean plant
 *        Ki_V 18000  the setpoint region needs the authority to walk the
 *                    duty from ~190 down to ~56 permille as the pack stops
 *                    accepting current. Least overshoot of the sweep
 *                    (13.5 mV versus 18.7 at 12000 and 33.6 at 6000);
 *                    stability bound is ~37000
 *        up_V 10     THE "SLOW THE ABSORB RISE" KNOB of the earlier order,
 *                    now the only voltage up-rate, so it governs the whole
 *                    climb: 0.01 permille/s against the legacy chain's 0.05
 *        up_I 1000   the current loop may climb at 1 permille/s
 *        down 1000   both loops fall at 1 permille/s so a sagging pack is
 *                    followed promptly
 *        Kd   0      the derivative of a noisy sensor is duty jitter; the
 *                    panel can raise it if the bench ever wants damping
 * [FA] کالیبراسیون کارخانه - دو حلقه، ده عدد (دستورهای کاربر ۲۰۲۶-۰۹-۲۹:
 *      «خودت بر اساس جدول‌ها برای اولین بار کالیبره کن تا بعداً اگر خواستم
 *      بهینه‌اش کنم»، سپس «اگر با یک PID در کل مسیر انجام بدهیم کار
 *      درنمی‌آید؟ مهم نیست کند باشد، چون باتری خودش کند است؛ با نهایت دقت
 *      ولی ساده‌ترین روش»).
 *
 *      چقدر می‌شود ساده کرد - اندازه‌گیری‌شده نه استدلالی:
 *        یک ردیف مشترک: شکست. یک ولت خطای ولتاژ و یک آمپر خطای جریان یک
 *          کمیت نیستند، پس کمینه‌گیری میلی‌ولت را با میلی‌آمپر می‌سنجد.
 *          ۱۴۰۸ تا ۸۴۶۶۰ تغییر جهت دیوتی در ۱۰ ساعت در برابر ۴.
 *        یک ردیف ولتاژ + پشتیبان ۶۵۰ به‌عنوان تنها محدودکنندهٔ جریان: شکست،
 *          آن هم روی خودِ خواستهٔ ایمنی: پشتیبان واکنشی است، پس جریان چرخهٔ
 *          حدی می‌زند و به‌جای ۶۴۰ تا ۷۰۷ میلی‌آمپر می‌رود.
 *        دو ردیف: درست. یک PI برای جریان، یک PI برای ولتاژ - همان ‎CC/CV‎
 *          کتابی و ساده‌ترین چیزی که واقعاً هر دو را تنظیم می‌کند.
 *        سه ردیف: بهتر نبود. ردیف اضافهٔ «روی ست‌پوینت» وقتی مدل نویز از
 *          تک‌تن به پهن‌باند تغییر کرد، هیچ سودی نشان نداد و حذف شد.
 *
 *      ده عدد، هرکدام از یک جاروب بر پایهٔ جدول پروفایل شارژ:
 *        Kp_I=۱۲ بهینهٔ مسطح ۸..۱۵؛ بالای ۴۵ می‌لرزد
 *        Ki_I=۱۶۰۰ از پک خالی در ۵۴۵ ثانیه به باند ۶۴۰ با صفر تغییر جهت
 *        Kp_V=۵۰ بهرهٔ نویز حلقهٔ ولتاژ و دلیل اینکه ۱۵۰ نیست: ولتاژ پک
 *          بالادست فیلتر ندارد، پس Kp نویز خام ADC را مستقیم روی دیوتی
 *          ضرب می‌کند. با نویز پهن‌باند ±۷ میلی‌ولت، ۱۵۰ یعنی ۷۴۴ تغییر
 *          جهت در ۱۰ ساعت و ۵۰ یعنی ۶۲ - با اورشوت یکسان، خطای تثبیت یکسان
 *          (۰٫۵ میلی‌ولت) و همان ۱۱۳ دقیقه تا ۱۴٫۴ ولت روی مدل تمیز
 *        Ki_V=۱۸۰۰۰ کمترین اورشوت جاروب (۱۳٫۵ در برابر ۱۸٫۷ و ۳۳٫۶)
 *        up_V=۱۰ همان «کلید رشد کندتر ابزورب»، حالا تنها نرخ صعود ولتاژ
 *        up_I=۱۰۰۰ و down=۱۰۰۰ یک پرمیل بر ثانیه
 *        Kd=۰ مشتقِ سنسور نویزی یعنی لرزش دیوتی */
#define CHG_PID_CURRENT_KP               12u
#define CHG_PID_CURRENT_KI             1600u
#define CHG_PID_CURRENT_KD                0u
#define CHG_PID_CURRENT_UP_RATE        1000u
#define CHG_PID_CURRENT_DOWN_RATE      1000u
#define CHG_PID_VOLTAGE_KP               50u
#define CHG_PID_VOLTAGE_KI            18000u
#define CHG_PID_VOLTAGE_KD                0u
#define CHG_PID_VOLTAGE_UP_RATE          10u
#define CHG_PID_VOLTAGE_DOWN_RATE      1000u

/* [EN] Re-seed guard: whenever the duty actually applied to the hardware
 *      differs from the PID's own integral by more than this many permille,
 *      the PID re-seeds from the hardware value (bumpless transfer). That
 *      single rule covers every foreign writer - JIT retry halving, manual
 *      test mode, fixed-duty mode, a lowered panel duty ceiling, the BULK
 *      soft start - without any of them having to know the PID exists.
 * [FA] نگهبان هم‌ترازی: هر وقت دیوتی واقعاً اعمال‌شده بیش از این مقدار
 *      پرمیل با انتگرال PID فرق کند، PID از مقدار سخت‌افزار دوباره بذر
 *      می‌گیرد (انتقال بدون پرش). همین یک قانون همهٔ نویسنده‌های بیرونی را
 *      پوشش می‌دهد - نصف‌شدن دیوتی در ری‌تریِ JIT، مود تست دستی، مود دیوتی
 *      فیکس، پایین‌آمدن سقف دیوتی از پنل، شروع نرم بالک - بدون اینکه هیچ‌کدام
 *      لازم باشد از وجود PID خبر داشته باشند. */
#define CHG_PID_RESEED_TOLERANCE_PERMILLE 1u

/* [EN] Output quantisation hysteresis, in milli-permille. The PWM stage
 *      takes WHOLE permille while the loop thinks in thousandths of one, so
 *      without this the applied duty toggles between two neighbouring
 *      integers every time the integral sits near a boundary - a 1 permille
 *      dither at up to 10 Hz. It is harmless electrically but it is
 *      exactly the "hunting" the user complained about, and it is what the
 *      duty readout shows. So the applied duty only moves once the demand
 *      has drifted at least this far from the value already on the
 *      hardware. Measured on the plant model over 10 h: 15570 duty
 *      direction changes without it, 4 with it (the legacy step chain
 *      managed 6456), with no measurable loss of regulation quality.
 *      MUST stay below CHG_PID_RESEED_TOLERANCE_PERMILLE x
 *      CHG_PID_DUTY_SCALE (asserted in charger.c): the deliberate lag it
 *      introduces must never look like a foreign writer and trigger a
 *      bumpless re-seed, or the two mechanisms would fight each other.
 * [FA] هیسترزیس گردکردن خروجی بر حسب میلی‌پرمیل. سخت‌افزار PWM پرمیل صحیح
 *      می‌گیرد ولی حلقه با هزارم پرمیل فکر می‌کند، پس بدون این، هر وقت
 *      انتگرال نزدیک مرز دو عدد صحیح بنشیند دیوتی اعمالی بین آن دو بالا و
 *      پایین می‌پرد - لرزش یک پرمیلی تا ۱۰ بار در ثانیه. از نظر برقی بی‌ضرر
 *      است ولی دقیقاً همان «بالا-پایین پریدن» است که کاربر شکایت کرد و
 *      همان چیزی است که در نمایش دیوتی دیده می‌شود. پس دیوتی اعمالی فقط
 *      وقتی تکان می‌خورد که تقاضا دست‌کم به این اندازه از مقدار روی
 *      سخت‌افزار فاصله گرفته باشد. اندازه‌گیری روی مدل در ۱۰ ساعت: بدون آن
 *      ۱۵۵۷۰ بار تغییر جهت دیوتی، با آن ۴ بار (زنجیرهٔ پله‌ای قدیمی ۶۴۵۶
 *      بار) و بدون افت محسوس کیفیت تنظیم. باید زیر حاصل‌ضرب
 *      CHG_PID_RESEED_TOLERANCE_PERMILLE در CHG_PID_DUTY_SCALE بماند (در
 *      charger.c اثبات شده): تأخیر عمدی‌اش هرگز نباید شبیه نویسندهٔ بیرونی
 *      دیده شود و بذرگیری دوباره را راه بیندازد، وگرنه این دو سازوکار با
 *      هم می‌جنگند. */
#define CHG_PID_OUTPUT_HYST_MILLI       700u

/* ==================== Hard backstops (user order 2026-09-29) ====================
 * [EN] "the 650 mA limit and the 14.8 V must be ACTIVE so the batteries are
 *      not damaged." These two limits are the last line of defence inside
 *      the PID and they are deliberately NOT tunable from the panel. The
 *      reason is the user's own next sentence: they intend to re-tune the
 *      coefficients later. A badly tuned gain set can overshoot; the
 *      backstops are what makes that experiment safe, so they must not be
 *      reachable from the same screen as the gains.
 *
 *      They work as a SHRINKING DUTY CEILING, not as a trip:
 *
 *          over-current -> ceiling = applied - GAIN_I x (I - limit_i)
 *          over-voltage -> ceiling = applied - GAIN_V x (V - limit_v)
 *
 *      Proportional to the excess, so 1 mA over costs 0.1 permille and
 *      nothing visibly moves, while 50 mA over costs 5 permille and the
 *      duty is pulled down hard. That shape matters: a fixed step would
 *      re-introduce exactly the hunting this whole rewrite removed, and a
 *      latching trip would stop a healthy charge over one noisy sample.
 *      The integral is clamped to the same shrunk ceiling, so there is no
 *      windup to unwind once the excess clears.
 *
 *      Current limit = the live profile value (id 25, 650 mA default), so
 *      lowering the charge current on the panel lowers the backstop with
 *      it. Voltage limit is the compile-time CHG_PID_BACKSTOP_MV (14.8 V),
 *      which is 400 mV above the absorb setpoint and 200 mV below the OV
 *      cutoff - it can only ever act on a real fault (a disconnected pack,
 *      a drifted sensor, or a future mis-tune), never during normal
 *      charging, where the measured peak is 14419 mV.
 *
 *      These do NOT replace the existing protections. The 950 mA hard
 *      fault, the 15.0 V OV cutoff, the battery-valid floor and the JIT
 *      gating all still run: those cut the charge and raise alarms, these
 *      merely keep the regulator from ever taking the pack there.
 * [FA] «حد ۶۵۰ میلی‌آمپر و ۱۴٫۸ ولت باید فعال باشند تا باتری‌ها آسیب
 *      نبینند.» این دو حد، آخرین خط دفاع داخل PID هستند و عمداً از پنل
 *      قابل تغییر نیستند. دلیلش جملهٔ بعدی خود کاربر است: قصد دارند بعداً
 *      ضرایب را بهینه کنند. یک دستهٔ ضریبِ بد می‌تواند اورشوت کند؛ همین
 *      پشتیبان‌ها هستند که آن آزمایش را ایمن می‌کنند، پس نباید از همان
 *      صفحه‌ای که ضرایب هستند در دسترس باشند.
 *
 *      این‌ها مثل «سقف دیوتیِ جمع‌شونده» کار می‌کنند نه مثل قطع‌کننده:
 *      کاهش، متناسب با مقدار تجاوز است، پس ۱ میلی‌آمپر تجاوز فقط ۰٫۱
 *      پرمیل هزینه دارد و چیزی دیده نمی‌شود، ولی ۵۰ میلی‌آمپر تجاوز ۵
 *      پرمیل و دیوتی محکم پایین کشیده می‌شود. این شکل مهم است: پلهٔ ثابت
 *      دقیقاً همان بالا-پایین پریدنی را برمی‌گرداند که این بازنویسی حذفش
 *      کرد، و قطع قفل‌شونده یک شارژ سالم را با یک نمونهٔ نویزی متوقف
 *      می‌کند. انتگرال هم به همین سقف جمع‌شده مقید می‌شود تا وقتی تجاوز
 *      رفع شد چیزی برای باز شدن نمانده باشد.
 *
 *      حد جریان = مقدار زندهٔ پروفایل (شناسهٔ ۲۵، پیش‌فرض ۶۵۰)، پس کم‌کردن
 *      جریان شارژ از پنل، پشتیبان را هم با خودش پایین می‌آورد. حد ولتاژ
 *      همان CHG_PID_BACKSTOP_MV کامپایل‌تایم (۱۴٫۸ ولت) است: ۴۰۰ میلی‌ولت
 *      بالای ست‌پوینت ابزورب و ۲۰۰ میلی‌ولت زیر قطع OV - فقط روی خطای
 *      واقعی عمل می‌کند (باتری جداشده، سنسور منحرف، یا تنظیم بد آینده)،
 *      نه در شارژ عادی که اوج اندازه‌گیری‌شده ۱۴۴۱۹ میلی‌ولت است.
 *
 *      این‌ها جایگزین حفاظت‌های موجود نیستند: خطای سخت ۹۵۰ میلی‌آمپر، قطع
 *      OV روی ۱۵ ولت، کف اعتبار باتری و گیت JIT همه سر جایشان کار می‌کنند؛
 *      آن‌ها شارژ را قطع و آلارم می‌دهند، این‌ها فقط نمی‌گذارند
 *      تنظیم‌کننده باتری را اصلاً به آنجا ببرد. */
#define CHG_PID_BACKSTOP_MV           14800u
#define CHG_PID_BACKSTOP_GAIN_I         100u  /* [EN] milli-permille per mA over / میلی‌پرمیل به ازای هر میلی‌آمپر تجاوز */
#define CHG_PID_BACKSTOP_GAIN_V         500u  /* [EN] milli-permille per mV over / میلی‌پرمیل به ازای هر میلی‌ولت تجاوز */

/* [EN] Voltage prefilter for the PID input, as a leaky-integrator divisor:
 *      S += V - S/N, and the loop reads S/N. WHY IT EXISTS: the charge
 *      CURRENT is filtered upstream (median-3 + moving average in
 *      measurement.c) but the battery VOLTAGE is not, and one ADC step on
 *      the pack divider is about 7 mV, so the raw reading carries roughly
 *      +/-15 mV of noise. That noise lands directly on the voltage loop's
 *      P term - at Kp = 150 that is 2.25 permille of pure noise on the
 *      duty, far more than the output hysteresis can absorb, and the duty
 *      visibly hunts again. Measured over 4 h with +/-15 mV of sensor
 *      noise: 50913 duty direction changes unfiltered, 217 at N = 8, and
 *      62 at N = 32 (broadband noise, mean of four seeds). N = 32 at one
 *      update per 100 ms is a 3.2 s time constant - invisible against a battery whose own dynamics are
 *      measured in minutes, and it costs 1 mV of truncation bias.
 *      The HARD BACKSTOPS deliberately do NOT use this filtered value:
 *      protection reads the raw sample so a real over-voltage is never
 *      delayed by a filter.
 * [FA] پیش‌فیلتر ولتاژ برای ورودی PID به‌صورت مقسوم‌علیهِ انتگرال‌گیر نشتی:
 *      ‎S += V - S/N‎ و حلقه ‎S/N‎ را می‌خواند. چرا لازم است: جریان شارژ
 *      بالادست فیلتر می‌شود (میانهٔ ۳ + میانگین متحرک در measurement.c) ولی
 *      ولتاژ باتری نه، و یک پلهٔ ADC روی مقسم پک حدود ۷ میلی‌ولت است، پس
 *      خواندن خام تقریباً ±۱۵ میلی‌ولت نویز دارد. این نویز مستقیم روی جملهٔ
 *      P حلقهٔ ولتاژ می‌نشیند - با Kp=۱۵۰ یعنی ۲٫۲۵ پرمیل نویز خالص روی
 *      دیوتی، خیلی بیشتر از آنچه هیسترزیس خروجی جذب می‌کند، و دیوتی دوباره
 *      آشکارا بالا-پایین می‌پرد. اندازه‌گیری ۴ ساعته با نویز ±۱۵ میلی‌ولت:
 *      بدون فیلتر ۵۰۹۱۳ بار تغییر جهت، با N=۸ برابر ۱۱۵، با N=۳۲ برابر ۶۲
 *      (نویز پهن‌باند، میانگین چهار seed). N=۳۲ یعنی ثابت زمانی ۳٫۲ ثانیه - در
 *      برابر باتری‌ای که دینامیکش با دقیقه سنجیده می‌شود نامرئی است و
 *      هزینه‌اش ۱ میلی‌ولت خطای قطع اعشار است. پشتیبان‌های سخت عمداً از این
 *      مقدار فیلترشده استفاده نمی‌کنند: حفاظت نمونهٔ خام را می‌خواند تا
 *      اضافه‌ولتاژ واقعی هرگز با فیلتر عقب نیفتد. */
#define CHG_PID_VOLT_FILTER_N            32u

/* [EN] Symmetric per-update cap on how far the APPLIED duty may move, in
 *      permille. This exists purely to bound a mis-tune, and it was added
 *      because the audit of the backstops proved they were not enough on
 *      their own: the integral is rate-limited but the P term is not (by
 *      design - rate-limiting P is what created the downward ratchet), so
 *      with every gain pushed to the panel maximum of 20000 the duty could
 *      jump from 190 to 500 permille inside ONE update and the pack saw
 *      4475 mA for 100 ms before the backstop could react. With this cap
 *      the same abuse peaks at 704 mA, below the 950 mA hard fault.
 *      SYMMETRIC is the whole point. The asymmetric output limiter tried
 *      earlier (slow up, fast down) rectified the P-term ripple into a
 *      downward ratchet and the loop stalled at 268 mA. A symmetric cap
 *      cannot rectify anything, and at 8 permille it sits 2.7x above the
 *      largest move the calibrated loop ever makes (3 permille, measured
 *      across flat/worn/new packs, 0.05..0.30 ohm and 40 mV of noise), so
 *      in normal charging it never engages at all - verified: identical
 *      reversal count, peak and time-to-setpoint with the cap present.
 *      It is applied BEFORE the hard ceiling so it can never slow a
 *      backstop down: protection always reaches the hardware in one pass.
 * [FA] سقف متقارن روی اندازهٔ حرکت دیوتی اعمالی در هر به‌روزرسانی، بر حسب
 *      پرمیل. فقط برای مهار تنظیم اشتباه است و وقتی اضافه شد که ممیزی
 *      پشتیبان‌ها ثابت کرد به‌تنهایی کافی نیستند: انتگرال محدودِ نرخ است
 *      ولی جملهٔ P نه (عمداً - محدودکردن نرخ P همان چیزی بود که جغجغهٔ رو
 *      به پایین را ساخت)، پس با تمام ضرایب روی بیشینهٔ پنل یعنی ۲۰۰۰۰،
 *      دیوتی می‌توانست در یک به‌روزرسانی از ۱۹۰ به ۵۰۰ پرمیل بپرد و باتری
 *      ۱۰۰ میلی‌ثانیه ۴۴۷۵ میلی‌آمپر ببیند، پیش از آنکه پشتیبان فرصت
 *      واکنش پیدا کند. با این سقف همان بدرفتاری روی ۷۰۴ میلی‌آمپر می‌ماند،
 *      زیر خطای سخت ۹۵۰. «متقارن» بودن تمام ماجراست: محدودکنندهٔ نامتقارن
 *      قبلی (صعود کند، نزول تند) ریپل P را یکسو کرد و حلقه روی ۲۶۸
 *      میلی‌آمپر گیر کرد. سقف متقارن چیزی را یکسو نمی‌کند، و با ۸ پرمیل
 *      ۲٫۷ برابر بزرگ‌ترین حرکتی است که حلقهٔ کالیبره‌شده اصلاً انجام
 *      می‌دهد (۳ پرمیل، اندازه‌گیری‌شده روی پک خالی/فرسوده/نو، مقاومت
 *      ۰٫۰۵ تا ۰٫۳۰ اهم و نویز ۴۰ میلی‌ولت)، پس در شارژ عادی هرگز فعال
 *      نمی‌شود - راستی‌آزمایی شد: تعداد تغییر جهت، اوج و زمان رسیدن به
 *      ست‌پوینت با و بدون آن یکی است. پیش از سقف سخت اعمال می‌شود تا هرگز
 *      نتواند پشتیبان را کند کند: حفاظت همیشه در یک پاس به سخت‌افزار
 *      می‌رسد. */
#define CHG_PID_MAX_STEP_PERMILLE         8u

#define CHG_DUTY_RETRY_SECOND_MAX       100u
/* [EN] DCM ceiling: 50% max - anything higher risks core/MOSFET overlap and
 *      burns the MOSFET (board requirement). The regulation band settles near
 *      ~19%, so this cap is only an upper bound.
 * [FA] سقف DCM: حداکثر ۵۰٪ — بالاتر از آن ماسفت می‌سوزد (شرط برد). نقطه کار
 *      تنظیم نزدیک ~۱۹٪ است؛ این فقط کران بالاست. */
#define CHG_DUTY_MAX_PERMILLE          500u
#define CHG_ABSORB_HOLD_MS          600000u
/* [EN] Tail-current ("taper") absorb completion - the classic lead-acid
 *      criterion, user bench decisions 2026-09-20 (bench pack = 4.5 Ah, so
 *      CHG_TAPER_CURRENT_MA 50 is ~C/90): absorb ends into FLOAT only when
 *      the minimum soak (CHG_ABSORB_HOLD_MS) has passed AND the tail current
 *      stays below CHG_TAPER_CURRENT_MA steadily for CHG_TAPER_SUSTAIN_MS
 *      (60 s; the sense chain wobbles +/-10..20 mA, so a single dipping frame
 *      must not complete the charge). A never-tapering battery still leaves
 *      absorb at the CHG_ABSORB_MAX_MS = 1 hour ceiling (forced FLOAT), so
 *      the pump cannot stay awake forever.
 * [FA] پایان‌دهی ابزورب به روش زیرجریان (تیپر) - معیار کلاسیک سرب-اسیدی و
 *      دستور بنچ کاربر (پک ۴٫۵ آمپرساعت =‎C/90‎ روچنار): ابزورب فقط وقتی
 *      FLOAT می‌شود که حداقل ۱۰ دقیقه شستشو گذشته باشد **و** زیرجریان <۵۰mA
 *      به‌مدت پایدار ۶۰ ثانیه بماند؛ سقف امن ۱ ساعت در هرحال FLOAT اجباری
 *      می‌کند تا باتری هرگز-تیپر‌نشده پمپ را بیدار نگه ندارد. */
#define CHG_TAPER_CURRENT_MA           50u
#define CHG_TAPER_SUSTAIN_MS        60000u
/* [EN] USER-ORDERED LOGIC CHANGE 2026-09-29. The one-hour absorb ceiling used
 *      to start the moment absorb was ENTERED, at 14.3 V, and it ended the
 *      charge an hour later whether the tail current had come down or not. On
 *      a pack that is still pulling hundreds of milliamps that cuts the charge
 *      off before the battery is full - it then sags past the 12.8 V reentry
 *      and starts all over again, which is the restart loop the user was
 *      seeing on the bench.
 *      The ceiling now ARMS on current, not on voltage: it starts counting the
 *      first time the tail falls below CHG_ABSORB_MAX_ARM_MA. Its job was
 *      never "limit absorb to an hour" - it was "once we are plainly in the
 *      tail, do not sit here forever" - and that is what it now does.
 *      Once armed it stays armed for the episode: re-arming on every wobble
 *      would let a noisy sense chain defeat the ceiling completely, and the
 *      chain is specified at +/-10..20 mA.
 * [FA] تغییر منطق به دستور کاربر ۲۰۲۹-۰۹-۲۹. سقف یک‌ساعتهٔ ابزورب از لحظهٔ
 *      ورود به ابزورب (۱۴٫۳ ولت) شروع می‌شد و یک ساعت بعد شارژ را تمام می‌کرد،
 *      چه جریان پایین آمده باشد چه نه. روی پکی که هنوز صدها میلی‌آمپر می‌کشد،
 *      این یعنی قطع شارژ پیش از پرشدن باتری؛ بعد ولتاژ تا زیر ۱۲٫۸ می‌افتد و
 *      همه‌چیز از نو شروع می‌شود - همان حلقهٔ بازگشتی که کاربر سر بنچ می‌دید.
 *      حالا سقف با «جریان» مسلح می‌شود نه «ولتاژ»: اولین باری که جریان دنباله
 *      زیر CHG_ABSORB_MAX_ARM_MA برود شمارش آغاز می‌شود. کار این سقف هیچ‌وقت
 *      «ابزورب حداکثر یک ساعت» نبود، «وقتی آشکارا در دنباله‌ایم اینجا ابدی
 *      نمان» بود. پس از مسلح‌شدن تا پایان همین اپیزود مسلح می‌ماند: مسلح‌کردن
 *      دوباره با هر نوسان، اجازه می‌داد زنجیرهٔ نویزی حس (مشخصهٔ ±۱۰ تا ۲۰
 *      میلی‌آمپر) سقف را کاملاً بی‌اثر کند. */
#define CHG_ABSORB_MAX_ARM_MA          100u
#define CHG_ABSORB_MAX_MS         3600000u
/* [EN] Battery-lost (both cases: pumped >14.8 V while charging, and battery
 *      absent with valid input) is OWNED BY THE FAULT MODULE since 2026-09-19
 *      per user directive: fault.c evaluates the snapshot centrally and
 *      latches/clears FAULT_CHARGER_BAT_LOST. The charger only MIRRORS the
 *      bit into CHG_STATE_BAT_LOST (stop PWM now) and releases the channel
 *      to OFF when the bit clears, so a soft BULK restart at 1% duty follows.
 *      Thresholds/timers: FAULT_BAT_* in fault.h, not here.
 * [FA] تشخیص قطع باتری از ۲۰۲۶-۰۹-۱۹ به مالکیت ماژول Fault منتقل شد؛ شارژر
 *      فقط آینهٔ پرچم است و آستانه/تایمری اینجا ندارد (fault.h ببین). */
#define CHG_JIT_LOCKOUT_MS            3000u
#define CHG_RELAY_SETTLE_MS            100u
/* [EN] Manual test mode link dead-man (user order 2026-09-23): while the
 *      mode is on, the STM expects at least one valid ESP frame every
 *      3 s; on expiry both duties drop to 0 and the autonomous charger
 *      resumes. A crashed browser must never leave a battery on an
 *      unregulated fixed duty.
 * [FA] ددمنِ لینک مود تست دستی (دستور کاربر ۲۰۲۶-۰۹-۲۳): تا وقتی مود
 *      روشن است، برد هر ۳ ثانیه دست‌کم یک فریم معتبر از ESP می‌خواهد؛
 *      با انقضا هر دو duty صفر و شارژر خودکار ادامه می‌دهد. کرش مرورگر
 *      هرگز نباید باتری را روی duty ثابتِ بدون تنظیم رها کند. */
#define CHG_MANUAL_WATCHDOG_MS        3000u
/* [EN] 2000 mV is battery-sense validity for the same channel, NOT a charge
 * setpoint. For Trans2 it is VLOW = MID - GND. A free resistor alone is not
 * a valid battery simulator; only an electronic load with voltage clamp or a
 * battery simulator is allowed for no-battery tests.
 * [FA] ۲۰۰۰ میلی‌ولت آستانه اعتبار sense باتری همان کانال است، نه setpoint
 * شارژ. برای Trans2 این مقدار روی ‎VLOW = MID-GND‎ بررسی می‌شود. مقاومت آزاد
 * به‌تنهایی شبیه‌ساز باتری نیست؛ برای تست بدون باتری فقط electronic load با
 * voltage clamp یا battery simulator مجاز است. */
#define CHG_MIN_VALID_BATTERY_MV      2000u
/* [EN] Hard upper battery-sense cutoff for every control path. A 12 V VRLA
 *      battery must not be connected to a charger that is already above this
 *      conservative 15.0 V limit. This is a firmware cutoff, not a substitute
 *      for a fuse, current-limited source, or a battery manufacturer's limits.
 * [FA] قطع سخت بالای سنجش باتری برای همهٔ مسیرهای کنترل. باتری VRLA دوازده
 *      ولت نباید به شارژری که از حد محافظه‌کارانهٔ ۱۵٫۰V بالاتر است وصل شود.
 *      این قطع firmware جای فیوز، منبع محدودشده یا حدود سازندهٔ باتری نیست. */
/* [EN] DECIDE EARLIER, DO NOT BEND THE SCALE (user order 2026-09-29).
 *      The pack divider used to be falsified (TOP 66200 instead of the real
 *      68K+1.2K) partly so that the over-voltage cut would trip ~100 mV
 *      early. That bought a little safety margin at the cost of every
 *      voltage the product reports. The divider is honest again, so the
 *      margin is taken where it belongs - at the DECISION.
 *      CHG_MAX_VALID_BATTERY_MV stays the absolute ceiling a half-pack may
 *      ever read; CHG_OV_DECIDE_EARLY_MV is how far BELOW it the default
 *      cut-off sits, so the charger stops before the ceiling rather than at
 *      it. Raise the early margin to act sooner; never re-scale a reading to
 *      fake it.
 * [FA] زودتر تصمیم بگیر، مقیاس را خم نکن (دستور کاربر ۲۰۲۶-۰۹-۲۹).
 *      مقسم پک قبلاً جعل شده بود (۶۶۲۰۰ به‌جای ‎68K+1.2K‎ واقعی) تا از جمله قطع
 *      اضافه‌ولتاژ حدود ۱۰۰ میلی‌ولت زودتر بزند. آن حاشیه به قیمت خراب‌شدن هر
 *      ولتاژی که محصول گزارش می‌کند خریده شده بود. مقسم دوباره صادق است، پس
 *      حاشیه جایی گرفته می‌شود که باید: سر تصمیم. */
#define CHG_MAX_VALID_BATTERY_MV     15000u
#define CHG_OV_DECIDE_EARLY_MV         150u
#define CHG_OV_CUTOFF_DEFAULT_MV \
    (CHG_MAX_VALID_BATTERY_MV - CHG_OV_DECIDE_EARLY_MV)

/* ==================== Charger diag array / آرایه دیاگ شارژر ==================== */
/* [EN] Live-diagnostics array (user order 2026-09-22): every value the charge
 *      decisions run on, refreshed at the top of EVERY Charger_Evaluate pass
 *      (control period), so it can be watched in one Live Expressions entry.
 *      Layout (5 slots per channel, then shared):
 *        [0..4]  ch0 = VHIGH half: state, duty permille, vbat mV,
 *                primary mA, output-estimate mA (the regulated value)
 *        [5..9]  ch1 = VLOW half: same five
 *        [10] v_in_mv, [11] v_bat24_mv, [12] v_bat12_mv,
 *        [13] v_bat_high_mv (derived = V24 - V12), [14] fault mask,
 *        [15] snapshot valid flag (0/1; 0 => current/voltage slots are 0)
 *      State codes: 0=OFF 1=BULK 2=ABSORB 3=FLOAT 4=BRINGUP
 *                   5=JIT_RETRY_WAIT 6=INPUT_WAIT 7=FINAL_FAULT 8=BAT_LOST
 * [FA] آرایهٔ دیاگ زنده (دستور کاربر ۲۰۲۶-۰۹-۲۲): همهٔ مقادیری که تصمیم‌های
 *      شارژ روی آن‌ها گرفته می‌شود، ابتدای هر پاس Evaluate (دورهٔ کنترل)
 *      به‌روز می‌شود تا در Live Expressions با یک ورودی دیده شود.
 *      چیدمان (۵ خانه per channel + مشترک‌ها):
 *        [‎0..4]‎  کانال ۰ = نیم VHIGH: state، duty پرمیل، vbat mV،
 *                جریان اولیه mA، تخمین خروجی mA (مقدار تنظیم‌شونده)
 *        [‎5..9]‎  کانال ۱ = نیم VLOW: همان پنج‌تا
 *        [10] v_in_mv، [11] v_bat24_mv، [12] v_bat12_mv،
 *        [13] v_bat_high_mv (مشتق = ‎V24 - V12)‎، [14] ماسک خطا،
 *        [15] بیت اعتبار snapshot (۰/۱؛ صفر یعنی خانه‌های جریان/ولتاژ صفرند)
 *      کدهای ‎state: 0=OFF 1=BULK 2=ABSORB 3=FLOAT 4=BRINGUP‎
 *                   5=JIT_RETRY_WAIT 6=INPUT_WAIT 7=FINAL_FAULT 8=BAT_LOST */
#define CHG_DIAG_COUNT                 16u
#define CHG_DIAG_CHANNEL_STRIDE         5u
#define CHG_DIAG_IDX_STATE              0u
#define CHG_DIAG_IDX_DUTY               1u
#define CHG_DIAG_IDX_VBAT               2u
#define CHG_DIAG_IDX_IPRI               3u
#define CHG_DIAG_IDX_IEST               4u
#define CHG_DIAG_IDX_VIN               10u
#define CHG_DIAG_IDX_V24               11u
#define CHG_DIAG_IDX_V12               12u
#define CHG_DIAG_IDX_VHIGH             13u
#define CHG_DIAG_IDX_FAULT             14u
#define CHG_DIAG_IDX_VALID             15u

extern volatile uint32_t UINT32_T__G__ChargerDiag[CHG_DIAG_COUNT];

/* ==================== Charger calib capture / آرایهٔ کالیبراسیون ==================== */
/* [EN] Calibration worksheet array (user order 2026-09-22): ONE variable to
 *      read during bench calibration, ordered exactly as the calibration
 *      math consumes it; the user records the real-world counterpart values
 *      (multimeter) at the same moment, with duty held steady.
 *        [0]  snapshot valid (0/1)
 *        [1]  v_in_mv (fw)      [2]  v_bat24_mv (fw)
 *        [3]  v_bat12_mv (fw)   [4]  v_bat_high_mv (fw, derived)
 *        [5..9]   channel UP   (upper battery): duty, state, vbat mV,
 *                                     Ipri mA, Iout_est mA
 *        [10..14] channel DOWN (lower battery): same five
 *      State codes as CHG_DIAG above.
 * [FA] آرایهٔ برگهٔ کالیبراسیون (دستور کاربر ۲۰۲۶-۰۹-۲۲): یک متغیر برای
 *      خواندن حین کالیبراسیون بنچ، به همان ترتیب مصرف محاسبات؛ مقادیر واقعی
 *      (مولتی‌متر) هم همان لحظه و با duty پایدار ثبت می‌شود.
 *        [0] اعتبار snapshot (۰/۱)
 *        [1] v_in_mv (فریمور)  [2] v_bat24_mv (فریمور)
 *        [3] v_bat12_mv (فریمور) [4] v_bat_high_mv (فریمور، مشتق)
 *        [‎5..9]‎   کانال بالا (باتری بالا): duty، state، vbat mV،
 *                                     Ipri mA، Iout_est mA
 *        [‎10..14]‎ کانال پایین (باتری پایین): همان پنج‌تا
 *      کدهای state مثل CHG_DIAG بالا. */
#define CHG_CALIB_COUNT                    15u
#define CHG_CALIB_IDX_VALID                 0u
#define CHG_CALIB_IDX_VIN                   1u
#define CHG_CALIB_IDX_V24                   2u
#define CHG_CALIB_IDX_V12                   3u
#define CHG_CALIB_IDX_VHIGH                 4u
#define CHG_CALIB_CH_UP_BASE                5u
#define CHG_CALIB_CH_DN_BASE               10u
#define CHG_CALIB_CH_STRIDE                 5u
#define CHG_CALIB_OFF_DUTY                  0u
#define CHG_CALIB_OFF_STATE                 1u
#define CHG_CALIB_OFF_VBAT                  2u
#define CHG_CALIB_OFF_IPRI                  3u
#define CHG_CALIB_OFF_IEST                  4u

extern volatile uint32_t UINT32_T__G__ChargerCalib[CHG_CALIB_COUNT];

/* ==================== Charger live estimate currents / جریان‌های تخمینی زنده ==================== */
/* [EN] The two OUTPUT-current estimates the charger actually decides with
 *      (user order 2026-09-22: watch them directly in Live Expressions).
 *      ch1 = VHIGH half (upper battery), ch2 = VLOW half (lower battery);
 *      refreshed every control pass together with the diag/calib arrays.
 *      The PID drives this value onto its setpoint of
 *      CHG_BULK_CURRENT_MAX_MA - CHG_PID_CURRENT_MARGIN_MA (640 mA).
 * [FA] همان دو جریان خروجی تخمینی که شارژر واقعاً با آن‌ها تصمیم می‌گیرد
 *      (دستور کاربر ۲۰۲۶-۰۹-۲۲: مستقیم در Live Expressions دیده شوند).
 *      کانال ۱ = نیم VHIGH (باتری بالا)، کانال ۲ = نیم VLOW (باتری پایین)؛
 *      هر پاس کنترل همراه آرایه‌های دیاگ/کالیبراسیون به‌روز می‌شوند.
 *      PID این مقدار را روی ست‌پوینت ۶۴۰ میلی‌آمپر می‌نشاند. */
extern volatile uint32_t UINT32_T__G__ChargerIest1Ma;
extern volatile uint32_t UINT32_T__G__ChargerIest2Ma;

/* ==================== Runtime config API (ESP panel) / API پیکربندی زمان اجرا ==================== */

/**
 * @brief  [EN] Set the runtime flyback efficiency of one channel, clamped
 *              to CHG_ETA_MIN_PERMILLE..CHG_ETA_MAX_PERMILLE;
 *              flash-persisted since v1.14, ESP panel (user order 2026-09-22).
 *         [FA] بازدهی flyback یک کانال در زمان اجرا، گیرهٔ
 *              ‎CHG_ETA_MIN_PERMILLE..CHG_ETA_MAX_PERMILLE‎؛ روی فلش
 *              می‌ماند از نسخهٔ ۱.۱۴، پنل ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @param  uint32_t__etaPermille [EN] Requested efficiency / بازدهی درخواستی
 * @return uint32_t [EN] Applied efficiency permille / بازدهی اعمال‌شده
 */
uint32_t func__Charger_SetEfficiencyPermille(uint8_t uint8_t__channelIndex,
                                             uint32_t uint32_t__etaPermille);

/**
 * @brief  [EN] Read the live flyback efficiency of one channel.
 *         [FA] بازدهی flyback زندهٔ یک کانال.
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @return uint32_t [EN] Live efficiency permille / بازدهی زندهٔ پرمیل
 */
uint32_t func__Charger_GetEfficiencyPermille(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Set the ESP enable gate of one charger channel: false = PWM
 *              off + state OFF (FINAL_FAULT never released by this gate),
 *              true = soft BULK restart; flash-persisted since v1.14
 *              (user order 2026-09-22).
 *         [FA] گیت فعال‌سازی ESP یک کانال شارژر: ‎false = PWM‎ قطع + وضعیت
 *              OFF (قفل FINAL_FAULT با این گیت آزاد نمی‌شود)، true =
 *              ری‌استارت نرم BULK؛ روی فلش می‌ماند از نسخهٔ ۱.۱۴ (دستور
 *              کاربر ۲۰۲۶-۰۹-۲۲).
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @‎param  bool__enable [EN] true = channel allowed‎ / کانال آزاد
 */
void func__Charger_SetChannelEspEnable(uint8_t uint8_t__channelIndex, bool bool__enable);

/**
 * @brief  [EN] Read the ESP enable gate of one charger channel.
 *         [FA] خواندن گیت فعال‌سازی ESP یک کانال شارژر.
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @‎return bool [EN] true = channel allowed‎ / کانال آزاد
 */
bool func__Charger_GetChannelEspEnable(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Set the runtime PWM duty ceiling of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE; every applied duty respects it.
 *              Flash-persisted since v1.14, ESP panel (user order 2026-09-22).
 *         [FA] سقف duty ی PWM یک کانال در زمان اجرا، گیرهٔ
 *              ۰..CHG_DUTY_MAX_PERMILLE؛ هر duty اعمالی آن را رعایت
 *              می‌کند. روی فلش می‌ماند از نسخهٔ ۱.۱۴، پنل ESP (دستور
 *              کاربر ۲۰۲۶-۰۹-۲۲).
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @param  uint32_t__ceilingPermille [EN] Requested ceiling / سقف درخواستی
 * @return uint32_t [EN] Applied ceiling / سقف اعمال‌شده
 */
uint32_t func__Charger_SetDutyCeilingPermille(uint8_t uint8_t__channelIndex,
                                              uint32_t uint32_t__ceilingPermille);

/**
 * @brief  [EN] Read the live PWM duty ceiling of one channel.
 *         [FA] سقف زندهٔ duty یک کانال.
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @return uint32_t [EN] Ceiling permille / سقف پرمیل
 */
uint32_t func__Charger_GetDutyCeilingPermille(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Set the runtime fixed-duty mode of one channel: hold the PWM
 *              at one chosen number instead of the regulation loop, with
 *              the bench-test safety wrapper (switching stops above
 *              CHG_ABSORB_MV; JIT/input/battery/ESP cuts stay active). RAM
 *              only (user order 2026-09-22).
 *         [FA] مود duty فیکس یک کانال در زمان اجرا: نگه‌داشتن PWM روی
 *              یک عدد به‌جای حلقهٔ تنظیم، با پوشش امنیتی تست بنچ (توقف
 *              سوئیچینگ بالای CHG_ABSORB_MV؛ حفاظت‌های JIT/ورودی/باتری/
 *              ESP فعال). فقط RAM (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @‎param  bool__enable [EN] true = fixed mode on‎ / مود فیکس روشن
 */
void func__Charger_SetDutyFixedEnable(uint8_t uint8_t__channelIndex, bool bool__enable);

/**
 * @brief  [EN] Read the runtime fixed-duty switch of one channel.
 *         [FA] کلید مود duty فیکس یک کانال.
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @‎return bool [EN] true = fixed mode on‎ / مود فیکس روشن
 */
bool func__Charger_GetDutyFixedEnable(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Set the fixed duty value of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE; effective only while fixed mode is
 *              enabled, and additionally clamped to the runtime ceiling on
 *              apply. Flash-persisted since v1.14 (user order 2026-09-22).
 *         [FA] مقدار duty فیکس یک کانال، گیرهٔ ۰..CHG_DUTY_MAX_PERMILLE؛
 *              فقط با روشن‌بودن مود فیکس مؤثر و موقع اعمال به‌علاوه به
 *              سقف زمان اجرا گیره می‌خورد. روی فلش می‌ماند از نسخهٔ ۱.۱۴
 *              (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @‎param  uint32_t__dutyPermille [EN] Requested duty / duty‎ درخواستی
 * @return uint32_t [EN] Applied stored value / مقدار ذخیره‌شده
 */
uint32_t func__Charger_SetDutyFixedPermille(uint8_t uint8_t__channelIndex,
                                            uint32_t uint32_t__dutyPermille);

/**
 * @brief  [EN] Read the stored fixed duty value of one channel.
 *         [FA] مقدار ذخیره‌شدهٔ duty فیکس یک کانال.
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @‎return uint32_t [EN] Duty permille / duty‎ پرمیل
 */
uint32_t func__Charger_GetDutyFixedPermille(uint8_t uint8_t__channelIndex);

/* ==================== Manual test mode / مود تست دستی ==================== */
/* [EN] Global bench/test switch (user order 2026-09-23, protocol v1.2
 *      param ID 19). While active the automatic charger is suspended and
 *      each channel is driven directly at its stored fixed-duty value
 *      (SetDutyFixedPermille) with every battery condition bypassed; the
 *      hardware floor stays: input presence, the JIT trip, the 15.0 V
 *      overvoltage cutoff, the duty ceilings and the FINAL_FAULT latch.
 *      See ESP_AGENT_SPEC.md section 5.2 for the full contract.
 * [FA] کلید سراسری تست/بنچ (دستور کاربر ۲۰۲۶-۰۹-۲۳، پارامتر ۱۹ پروتکل
 *      v1.2). تا وقتی فعال است شارژر خودکار تعلیق و هر کانال مستقیم روی
 *      مقدار duty فیکس خودشران می‌شود با حذف همهٔ شرط‌های باتری؛ کفِ
 *      سخت‌افزاری می‌ماند: حضور ورودی، تریپ JIT، قطع ۱۵٫۰V، سقف‌های duty
 *      و قفل FINAL_FAULT. قرارداد کامل: ESP_AGENT_SPEC.md بخش 5.2. */

/**
 * @brief  [EN] Request manual test mode on/off (called by the ESP link
 *              task; the charger task adopts the request on its next pass
 *              and performs the enter/exit actions in its own context).
 *         [FA] درخواست روشن/خاموش مود تست دستی (از تسک ESP صدا زده می‌شود؛
 *              تسک شارژر در پاس بعدی درخواست را برمی‌دارد و عملیات
 *              ورود/خروج را در زمینهٔ خودش انجام می‌دهد).
 * @‎param  bool__enable [EN] true = manual on‎ / روشن
 */
void func__Charger_SetManualTestMode(bool bool__enable);

/**
 * @brief  [EN] Read the REQUESTED manual mode flag (the parameter value
 *              the panel wrote; equals IsManualTestModeActive within one
 *              control period).
 *         [FA] پرچم «درخواست‌شدهٔ» مود دستی (مقدار پارامتری که پنل
 *              نوشته؛ حداکثر یک دورهٔ کنترل با وضعیت فعال اختلاف دارد).
 * @‎return bool [EN] true = requested on‎ / درخواست روشن
 */
bool func__Charger_GetManualTestMode(void);

/**
 * @brief  [EN] Read the ACTIVE manual mode flag (owned by the charger
 *              task; used by telemetry flag b5 and by the Fault module to
 *              freeze battery-lost detection).
 *         [FA] پرچم «فعالِ» مود دستی (مالکش تسک شارژر؛ تله‌متری b5 و
 *              ماژول Fault برای فریز کردن تشخیص قطع باتری استفاده‌اش
 *              می‌کنند).
 * @‎return bool [EN] true = manual active‎ / مود دستی فعال است
 */
bool func__Charger_IsManualTestModeActive(void);

/**
 * @brief  [EN] Feed the manual-mode link dead-man: every VALID frame from
 *              the ESP (SET_PARAM / GET_PARAMS) refreshes the stamp; 3 s
 *              of silence exits manual mode and drops both duties.
 *         [FA] غذای ددمنِ لینک مود دستی: هر فریم معتبر از ESP مهر را
 *              تازه می‌کند؛ ۳ ثانیه سکوت = خروج از مود دستی و صفرشدن
 *              هر دو duty.
 */
void func__Charger_NotifyEspLinkActivity(void);

/* ==================== Charger NVM-save suspension / تعلیق برای ذخیره NVM ==================== */
/**
 * @brief  [EN] Suspend / resume switching for an NVM flash save (user order
 *              2026-09-27: idle the charger, save, restart it). While
 *              suspended, Charger_Evaluate holds both gates at 0 and skips
 *              the pass; state machines, soak accumulators and the 15 s
 *              settle gate are untouched, so the resume continues
 *              seamlessly. Fault and Changeover keep evaluating (separate
 *              calls in the control task). Single volatile flag, no lock
 *              needed. Pre-kernel safe (plain bool store/load).
 *         [FA] تعلیق/ادامهٔ سوییچینگ برای ذخیرهٔ فلش NVM (دستور کاربر):
 *              در تعلیق هر دو گیت صفر و پاس رد می‌شود؛ ماشین‌های حالت و
 *              شستشو دست نمی‌خورند پس ادامه یکپارچه است. فالت و چنج‌اور
 *              به ارزیابی ادامه می‌دهند. تک‌پرچم volatile بدون قفل.
 * @‎param  bool__suspended [EN] true = hold gates at 0‎ / گیت‌ها صفر نگه داشته شوند
 */
void func__Charger_SetSuspended(bool bool__suspended);

/**
 * @brief  [EN] Read the NVM-save suspension flag.
 *         [FA] خواندن پرچم تعلیق ذخیرهٔ NVM.
 * @‎return bool [EN] true = suspension active‎ / تعلیق فعال است
 */
bool func__Charger_IsSuspended(void);

/* ==================== Charger_Init / مقداردهی اولیه ==================== */
/**
 * @brief  [EN] Initialize policy state, stop every PWM channel and force a
 *              deterministic safe-idle state. With CHG_MASTER_ENABLE=0 this
 *              remains the only active behavior.
 *         [FA] state سیاست را مقداردهی اولیه می‌کند، همه PWMها را متوقف و
 *              وضعیت ‎safe-idle‎ قطعی را اعمال می‌کند. با ‎CHG_MASTER_ENABLE=0‎
 *              این تنها رفتار فعال باقی می‌ماند.
 */
void func__Charger_Init(void);

/* ==================== Charger_Evaluate / ارزیابی شارژر ==================== */
/**
 * @brief  [EN] Run the same control/protection algorithm independently for
 *              every installed 12 V channel. Input voltage is checked from
 *              real ADC mV, not only the PB4 digital signal. A low current is
 *              a regulation request to increase duty, not a fault.
 *         [FA] الگوریتم یکسان کنترل و حفاظت را برای هر کانال نصب‌شدهٔ ۱۲ ولت
 *              مستقل اجرا می‌کند. ولتاژ ورودی از مقدار واقعی ADC mV بررسی
 *              می‌شود، نه فقط سیگنال دیجیتال PB4. جریان کم درخواست افزایش
 *              duty است، نه fault.
 * @‎param  measurement_snapshot_t__snap [EN] Independent low/high battery snapshot‎ / نمونه مستقل دو باتری
 * @param  app_state_t__state [EN] System state / حالت سیستم
 */
void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state);

/**
 * @brief  [EN] True while at least one installed channel is actually
 *         charging - its state machine sits in BULK or ABSORB (user
 *         directive 2026-09-19: a FLOAT channel is parked at zero duty, the
 *         charge is DONE, not active - v1.17b: the UI full face keys on
 *         IsChargeComplete, and the fault pump-window is not armed there
 *         either).
 *         [FA] true وقتی دست‌کم یک کانال نصب‌شده واقعاً در حال شارژ است
 *         (بالک/ابزورب؛ فلوت پارک‌شده یعنی کار تمام شده و فعال حساب
 *         نمی‌شود - چهرهٔ فول با IsChargeComplete می‌آید و آشکارساز قطع
 *         باتری هم آنجا مسلح نیست).
 * @return bool [EN] true if any channel is charging / اگر هر کانالی شارژ کند true
 */
bool func__Charger_IsAnyChannelActive(void);

/**
 * @brief  [EN] True while ONE given channel (0 = charger 1 / upper battery,
 *         1 = charger 2 / lower battery) is actually pumping: installed and
 *         in BULK or ABSORB - the per-channel split of the predicate above
 *         (v1.21). The Fault module arms its 14.8 V pump-rule PER HALF with
 *         it: a parked channel has no pump, so its half cannot fly up and
 *         must not be judged (kills the false 3-beep cycle during charge,
 *         user order 2026-09-28).
 *         [FA] کانالِ داده‌شده (۰ = شارژر ۱ / باتری بالا، ۱ = شارژر ۲ /
 *         باتری پایین) واقعاً پمپ می‌کند؟ نصب‌شده و در BULK یا ABSORB -
 *         تجزیهٔ به‌ازای کانال همان محمول بالا (v1.21). فالت با آن قانون
 *         پمپ ۱۴٫۸V را «به‌ازای هر نیم» مسلح می‌کند: کانال پارک‌شده پمپی
 *         ندارد پس نیمش قضاوت نمی‌شود (رفع چرخهٔ کاذب سه‌بوق حین شارژ،
 *         دستور کاربر ۲۰۲۶-۰۹-۲۸).
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / ۰ یا ۱
 * @return bool [EN] true while that channel pumps / وقتی همان کانال پمپ کند
 */
bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] True when every relevant channel finished its charge: at
 *         least one installed+enabled channel exists and ALL of them sit
 *         in FLOAT. FLOAT is entered from one place only (ABSORB done:
 *         soak + taper, or the 1 h safety ceiling), so FLOAT means DONE;
 *         any restart (reentry BULK, fresh OFF->BULK, disable) leaves it.
 *         v1.17b (user order 2026-09-27: "after a full charge the blinking
 *         must be gone"): the UI latches its full face on this - not on
 *         the unreachable voltage 100 (pack 29 V tops the 28.8 V absorb).
 *         [FA] true وقتی همهٔ کانال‌های مربوط شارژشان تمام شده: دست‌کم یک
 *         کانال نصب+فعال هست و همه در FLOATاند. ورود به FLOAT فقط از یک
 *         جا (پایان ابزورب: شستشو+تیپر یا سقف ۱ساعت) پس FLOAT یعنی تمام؛
 *         هر شروع دوباره از آن بیرون می‌آید. نسخه ۱.۱۷b: چهرهٔ فول UI
 *         روی همین لچ می‌شود نه روی ۱۰۰ ولتاژی دست‌نیافتنی.
 * @return bool [EN] true if the charge is complete on all relevant channels /
 *         اگر شارژ همهٔ کانال‌های مربوط کامل شده true
 */
bool func__Charger_IsChargeComplete(void);

/* [EN] Charge-profile wire ids (MUST equal ESPLINK_PARAM_CHG_PROFILE_* in
 *      esp_link.h; the host test enforces the match).
 * [FA] شناسه‌های سیمی پروفایل شارژ (باید برابر ESPLINK_PARAM_CHG_PROFILE_*
 *      در esp_link.h باشند؛ تست هاست همین را قفل می‌کند). */
#define CHG_PROFILE_PARAM_ABSORB_MV           20u  /* [EN] mV, 11000..14600 / mV */
#define CHG_PROFILE_PARAM_ABSORB_ENTER_MV     21u  /* [EN] mV, absorb-500..absorb-50 / mV */
#define CHG_PROFILE_PARAM_ABSORB_OVER_MV      22u  /* [EN] mV, absorb+100..min(absorb+400,14750) / mV */
#define CHG_PROFILE_PARAM_FLOAT_MV            23u  /* [EN] mV, 9000..absorb-300 / mV */
#define CHG_PROFILE_PARAM_REENTRY_MV          24u  /* [EN] mV, 8000..float-300 / mV */
#define CHG_PROFILE_PARAM_BULK_CURRENT_MAX_MA 25u  /* [EN] mA, 100..900 / mA */
#define CHG_PROFILE_PARAM_TAPER_CURRENT_MA    26u  /* [EN] mA, 10..min(300,imax) / mA */

/* ==================== Charge Profile (user order 2026-09-25) ====================
 * [EN] Runtime-settable charge profile, shared by BOTH channels, written from
 *      the ESP panel tab "تنظیمات شارژ" (‎wire params 20..26, section 5.7 of‎
 *      ESP_AGENT_SPEC.md). Boot defaults equal the old compile-time setpoints
 *      (CHG_*_MV / CHG_*_MA below); values live in RAM and reset at boot,
 *      like every other parameter. v1.15: the hard safety stack
 *      (the hard-fault trip, CHG_MAX_VALID_BATTERY_MV, the 15.0 V
 *      overvoltage cutoff) is runtime-LOWERABLE from the alarms tab (ids
 *      35..37) but can NEVER be raised above the compile maxima.
 * [FA] پروفایل شارژِ قابل‌تنظیم در زمان اجرا، مشترک بین هر دو کانال، از تب
 *      «تنظیمات شارژ» پنل نوشته می‌شود (پارامترهای سیمی ۲۰..۲۶، بخش 5.7
 *      سند). پیش‌فرض بوت همان ست‌پوینت‌های کامپایل‌تایم قبلی است (ماکروهای
 *      ‎CHG_*_MV / CHG_*_MA‎ پایین)؛ مقادیر در RAM می‌مانند و با ریست به
 *      پیش‌فرض برمی‌گردند، مثل بقیهٔ پارامترها. v1.15: پشتهٔ ایمنی سخت
 *      از تب آلارم‌ها (شناسه‌های ۳۵..۳۷) فقط پایین‌بردنی است و هرگز بالای
 *      سقف کامپایل نمی‌رود. */

/**
 * @brief  [EN] Write one charge-profile parameter (ESP link, ids 20..26:
 *              20=ABSORB_MV, 21=ABSORB_ENTER_MV, 22=ABSORB_OVER_MV,
 *              23=FLOAT_MV, 24=REENTRY_MV, 25=BULK_CURRENT_MAX_MA,
 *              26=TAPER_CURRENT_MA). The value is clamped and every
 *              dependent is re-clamped so the set stays consistent
 *              (ENTER in [ABSORB-500, ABSORB-50], OVER in
 *              [ABSORB+100, ABSORB+400], FLOAT <= ABSORB-300, REENTRY <=
 *              FLOAT-300, TAPER <= BULK_CURRENT_MAX). Returns the APPLIED
 *              value (SET_PARAM reports it back).
 *         [FA] نوشتن یک پارامتر پروفایل شارژ (لینک ESP، شناسه‌های ۲۰..۲۶:
 *              ۲۰=ولتاژ ابزورب، ۲۱=آستانهٔ ورود، ۲۲=سقف تجاوز، ۲۳=ولتاژ
 *              شناور، ۲۴=ولتاژ بازگشت، ۲۵=جریان حداکثر بالک، ۲۶=جریان
 *              تیپر/شناوری). مقدار گیره می‌خورد و همهٔ وابسته‌ها دوباره
 *              گیره می‌شوند تا مجموعه سازنده بماند. مقدار «اعمال‌شده» را
 *              برمی‌گرداند (SET_PARAM همان را پاس می‌دهد).
 * @‎param  uint8_t__paramId [EN] 20..26‎ / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw requested value / مقدار درخواستی خام
 * @‎param  uint32_t *uint32_t__appliedValue [EN] Applied value out‎ / مقدار اعمال‌شده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_SetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one charge-profile parameter (ESP link GET/PARAMS_BULK).
 *         [FA] خواندن یک پارامتر پروفایل شارژ (لینک ESP).
 * @‎param  uint8_t__paramId [EN] 20..26‎ / شناسهٔ پارامتر
 * @‎param  uint32_t *uint32_t__value [EN] Live value out‎ / مقدار زنده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_GetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value);

/* [EN] Charger-owned alarm wire ids (MUST equal ESPLINK_PARAM_CHG_ALARM_* in
 *      esp_link.h; the host test enforces the match). v1.15: the hard
 *      current/voltage ceilings become runtime-lowerable (never raisable
 *      above the compile maxima) from the alarms tab.
 * [FA] شناسه‌های سیمی آلارم‌های شارژر (باید برابر ESPLINK_PARAM_CHG_ALARM_*
 *      در esp_link.h باشند؛ تست هاست همین را قفل می‌کند). v1.15: سقف‌های
 *      سخت جریان/ولتاژ از تب آلارم‌ها فقط پایین‌بردنی‌اند (هرگز بالاتر از
 *      سقف کامپایل). */
#define CHG_ALARM_PARAM_HARD_CURRENT_MA       35u  /* [EN] mA, imax+50 .. CHG_CURRENT_HARD_FAULT_MAX_MA / mA */
#define CHG_ALARM_PARAM_OV_CUTOFF_MV          36u  /* [EN] mV, over+150..15000, never above 15000 / mV */
#define CHG_ALARM_PARAM_VALID_FLOOR_MV        37u  /* [EN] mV, 0..8000 / mV */

/**
 * @brief  [EN] Write one charger alarm parameter (ESP link, ids 35..37:
 *              35=HARD_CURRENT_MA, 36=OV_CUTOFF_MV, 37=VALID_FLOOR_MV).
 *              Safety direction is DOWN ONLY: the hard current fault can
 *              never exceed 950 mA and the OV cutoff never 15000 mV; both
 *              keep clearance above the live profile band (hard >= imax+50,
 *              OV >= over+150) so legit regulation can never trip them.
 *              Returns the APPLIED value (SET_PARAM reports it back).
 *         [FA] نوشتن یک پارامتر آلارم شارژر (لینک ESP، شناسه‌های ۳۵..۳۷).
 *              جهت ایمنی فقط پایین است: خطای سخت جریان هرگز بالای ۹۵۰mA و
 *              قطع OV هرگز بالای ۱۵۰۰۰mV نمی‌رود؛ هر دو بالای
 *              باند زندهٔ پروفایل فاصله نگه می‌دارند تا تنظیم سالم تریپ نکند.
 * @‎param  uint8_t__paramId [EN] 35..37‎ / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw requested value / مقدار درخواستی خام
 * @‎param  uint32_t *uint32_t__appliedValue [EN] Applied value out‎ / مقدار اعمال‌شده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_SetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one charger alarm parameter (ESP link GET/PARAMS_BULK).
 *         [FA] خواندن یک پارامتر آلارم شارژر (لینک ESP).
 * @‎param  uint8_t__paramId [EN] 35..37‎ / شناسهٔ پارامتر
 * @‎param  uint32_t *uint32_t__value [EN] Live value out‎ / مقدار زنده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_GetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value);

/* [EN] Two-loop CC/CV PID wire ids (MUST equal ESPLINK_PARAM_CHG_PID_* in
 *      esp_link.h; the host test enforces the match). Dense 83..98 in the
 *      same order as charger_pid_t packs them, so Set/Get index instead of
 *      switching (same "flash diet" contract as the profile ids 20..26).
 *      Each stage is a complete five-field row (Kp, Ki, Kd, up-rate,
 *      down-rate), so the index arithmetic is uniform: (id - 84) % 5 says
 *      which field, (id - 84) / 5 says which stage, and fields 3 and 4 are
 *      the slew rates. Remember what a "stage" is here (see the regulator
 *      block above): stage 1 is the CURRENT loop, stages 2 and 3 are the
 *      VOLTAGE loop below and at the setpoint.
 * [FA] شناسه‌های سیمی PID دوحلقه‌ای (باید برابر ESPLINK_PARAM_CHG_PID_* در
 *      esp_link.h باشند؛ تست هاست همین را قفل می‌کند). ۸۳..۹۲ پشت‌سرهم و
 *      دقیقاً به ترتیب فیلدهای charger_pid_t، پس ‎Set/Get‎ به‌جای switch
 *      نمایه می‌زنند (همان قرارداد «رژیم فلش» شناسه‌های ۲۰..۲۶). هر مرحله یک
 *      ردیف کامل پنج‌فیلدی است (Kp، Ki، Kd، نرخ صعود، نرخ نزول) تا حساب
 *      نمایه یکنواخت بماند: باقیماندهٔ (شناسه−۸۴) بر ۵ یعنی کدام فیلد، خارج
 *      قسمتش یعنی کدام مرحله، و فیلدهای ۳ و ۴ همان سقف‌های شیب‌اند. یادآوری
 *      معنی «مرحله» (بلوک تنظیم‌کننده در بالا): مرحلهٔ ۱ حلقهٔ جریان است و
 *      مرحله‌های ۲ و ۳ حلقهٔ ولتاژ زیر ست‌پوینت و روی ست‌پوینت. */
#define CHG_PID_PARAM_CURRENT_KP        83u  /* [EN] 0..20000, permille per amp / پرمیل بر آمپر، حلقهٔ جریان */
#define CHG_PID_PARAM_CURRENT_KI        84u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_CURRENT_KD        85u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_CURRENT_UP_RATE   86u  /* [EN] milli-permille/s, 10..20000 */
#define CHG_PID_PARAM_CURRENT_DOWN_RATE 87u  /* [EN] milli-permille/s, 10..20000 */
#define CHG_PID_PARAM_VOLTAGE_KP        88u  /* [EN] 0..20000, permille per volt / پرمیل بر ولت، حلقهٔ ولتاژ */
#define CHG_PID_PARAM_VOLTAGE_KI        89u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_VOLTAGE_KD        90u  /* [EN] 0..20000 */
#define CHG_PID_PARAM_VOLTAGE_UP_RATE   91u  /* [EN] the "slow the absorb rise" knob / کلید «رشد کندتر ابزورب» */
#define CHG_PID_PARAM_VOLTAGE_DOWN_RATE 92u  /* [EN] milli-permille/s, 10..20000 */

/**
 * @brief  [EN] Write one two-loop CC/CV PID parameter (ESP link, ids 83..92).
 *              Values are clamped to the compiled windows: enable 0/1,
 *              gains 0..CHG_PID_GAIN_MAX, slew rates CHG_PID_RATE_MIN..
 *              CHG_PID_RATE_MAX milli-permille/s. Changing a gain never
 *              bumps the duty: the integrator keeps the present operating
 *              point and only its rate of change is re-scheduled. Returns
 *              the APPLIED value.
 *         [FA] نوشتن یک پارامتر PID دوحلقه‌ای (لینک ESP، ۸۳..۹۲). مقادیر
 *              به پنجره‌های کامپایل گیره می‌خورند: فعال‌ساز ۰/۱، ضرایب تا
 *              CHG_PID_GAIN_MAX و شیب‌ها بین CHG_PID_RATE_MIN و
 *              CHG_PID_RATE_MAX میلی‌پرمیل بر ثانیه. تغییر ضریب هیچ پرشی در
 *              دیوتی نمی‌سازد: انتگرال‌گیر نقطهٔ کار فعلی را نگه می‌دارد و
 *              فقط نرخ تغییرش زمان‌بندی دوباره می‌شود. مقدار اعمال‌شده
 *              برگردانده می‌شود.
 * @‎param  uint8_t__paramId [EN] 83..98‎ / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw requested value / مقدار درخواستی خام
 * @‎param  uint32_t *uint32_t__appliedValue [EN] Applied value out‎ / مقدار اعمال‌شده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_SetPidParam(uint8_t uint8_t__paramId,
                               uint32_t uint32_t__value,
                               uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one two-loop CC/CV PID parameter (ESP link GET/PARAMS_BULK).
 *         [FA] خواندن یک پارامتر PID دوحلقه‌ای (لینک ESP).
 * @‎param  uint8_t__paramId [EN] 83..98‎ / شناسهٔ پارامتر
 * @‎param  uint32_t *uint32_t__value [EN] Live value out‎ / مقدار زنده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_GetPidParam(uint8_t uint8_t__paramId,
                               uint32_t *uint32_t__value);

/* ============ Charger limits & backstop gains (user order 2026-10-03) ============
 * [EN] USER-ORDERED LOGIC CHANGE. Every remaining charger gain, limit and
 *      stage timer becomes settable from the panel. These were compile-time
 *      constants, and the panel even admitted it in writing - the PID help
 *      bubble said the two hard backstops "are always on and are not
 *      adjustable from the panel", and the absorb ceiling was described as
 *      "1 hour" with no way to change it. Anything a bench session needs to
 *      try is now a parameter.
 *
 *      Deliberately NOT in this block, and why:
 *        - CHG_DUTY_MAX_PERMILLE (500) stays a compile constant: it is the
 *          DCM/MOSFET rating of the board, not a tuning choice, and the
 *          per-channel ceiling (ids 13/14) already limits duty downward.
 *        - CHG_CURRENT_HARD_FAULT_MA (950) and the OV cutoff keep their
 *          existing down-only ids 35/36. A hard fault limit that can be
 *          RAISED from a web page is not a hard fault limit.
 *        - CHG_CURRENT_LIMIT_MA was deleted outright: nothing read it, so
 *          publishing it would have handed the user a knob wired to nothing.
 *
 *      Implementation is TABLE-DRIVEN on purpose. Fifteen more switch cases
 *      with hand-written clamps is the pattern that has repeatedly gone
 *      stale here, and this part is already tight on flash (a 92-byte
 *      overflow had to be cleared in v1.26). One const row per limit -
 *      {min, max, default} - costs a few hundred bytes of rodata and makes
 *      the clamp impossible to forget.
 * [FA] تغییر منطق به دستور کاربر. هر گین، حد و تایمر مرحله‌ای باقی‌ماندهٔ
 *      شارژر از پنل تنظیم‌شدنی می‌شود. این‌ها ثابت کامپایل بودند و خود پنل هم
 *      کتباً اعتراف کرده بود: راهنمای PID نوشته بود دو پشتیبان سخت «همیشه
 *      فعال‌اند و از پنل تنظیم نمی‌شوند» و سقف ابزورب «۱ ساعت» معرفی شده بود
 *      بی‌آنکه راهی برای تغییرش باشد. هرچه یک جلسهٔ بنچ لازم دارد امتحان کند
 *      حالا پارامتر است.
 *
 *      عمداً در این بلوک نیست و چرا:
 *        - CHG_DUTY_MAX_PERMILLE (۵۰۰) ثابت می‌ماند: مشخصهٔ DCM/ماسفت برد است
 *          نه انتخاب تنظیمی، و سقف هر کانال (۱۳/۱۴) از قبل پایین‌آورنده است.
 *        - خطای سخت جریان (۹۵۰) و قطع OV همان شناسه‌های فقط-پایین ۳۵/۳۶ را
 *          نگه می‌دارند. حد خطای سختی که از یک صفحهٔ وب «بالا» برود، حد خطای
 *          سخت نیست.
 *        - CHG_CURRENT_LIMIT_MA کلاً حذف شد: هیچ‌جا خوانده نمی‌شد، پس منتشر
 *          کردنش یعنی دادن پیچی به کاربر که به هیچ‌چیز وصل نیست.
 *
 *      پیاده‌سازی عمداً جدول‌محور است. پانزده case دیگر با گیره‌های دستی همان
 *      الگویی است که بارها در این پروژه کهنه شده، و این بخش از نظر فلش تنگ
 *      است (در v1.26 یک سرریز ۹۲ بایتی رفع شد). یک ردیف const برای هر حد -
 *      {کمینه، بیشینه، پیش‌فرض} - چند صد بایت rodata می‌گیرد و فراموش‌کردن
 *      گیره را ناممکن می‌کند. */
#define CHG_LIMIT_PARAM_ABSORB_MAX_MS        93u  /* [EN] ms, 0=no ceiling..21600000 (6 h) */
#define CHG_LIMIT_PARAM_ABSORB_MAX_ARM_MA    94u  /* [EN] mA, 10..500 - tail level that ARMS the ceiling */
#define CHG_LIMIT_PARAM_ABSORB_HOLD_MS       95u  /* [EN] ms, 0..7200000 - minimum soak before FLOAT */
#define CHG_LIMIT_PARAM_TAPER_SUSTAIN_MS     96u  /* [EN] ms, 1000..600000 - tail must hold this long */
#define CHG_LIMIT_PARAM_PID_MAX_STEP_PM      97u  /* [EN] permille/tick, 1..100 */
#define CHG_LIMIT_PARAM_PID_OUT_HYST_MILLI   98u  /* [EN] milli-permille, 0..5000 */
#define CHG_LIMIT_PARAM_PID_VOLT_FILTER_N    99u  /* [EN] EWMA divisor, 1..64 (1 = filter off) */
#define CHG_LIMIT_PARAM_BACKSTOP_MV         100u  /* [EN] mV, 13000..14800 - down only, never above compile max */
#define CHG_LIMIT_PARAM_BACKSTOP_GAIN_I     101u  /* [EN] 0..2000 - permille of duty per amp of overshoot */
#define CHG_LIMIT_PARAM_BACKSTOP_GAIN_V     102u  /* [EN] 0..2000 - permille of duty per volt of overshoot */
#define CHG_LIMIT_PARAM_PID_CUR_MARGIN_MA   103u  /* [EN] mA, 0..100 */
#define CHG_LIMIT_PARAM_CONNECT_SETTLE_MS   104u  /* [EN] ms, 0..120000 */
#define CHG_LIMIT_PARAM_JIT_LOCKOUT_MS      105u  /* [EN] ms, 0..60000 */
#define CHG_LIMIT_PARAM_MANUAL_WATCHDOG_MS  106u  /* [EN] ms, 500..60000 - deadman for manual mode */
#define CHG_LIMIT_PARAM_RAMP_DOWN_INT_MS    107u  /* [EN] ms, 50..5000 */

#define CHG_LIMIT_PARAM_FIRST_ID            CHG_LIMIT_PARAM_ABSORB_MAX_MS
#define CHG_LIMIT_PARAM_LAST_ID             CHG_LIMIT_PARAM_RAMP_DOWN_INT_MS
#define CHG_LIMIT_COUNT  \
    ((CHG_LIMIT_PARAM_LAST_ID - CHG_LIMIT_PARAM_FIRST_ID) + 1u)

/**
 * @brief  [EN] Write one charger limit / backstop gain (ESP link, ids
 *              93..107). The value is clamped into the compiled window for
 *              that row and the APPLIED value is returned, exactly like the
 *              profile and PID blocks.
 *         [FA] نوشتن یک حد یا گین پشتیبان شارژر (لینک ESP، ۹۳..۱۰۷). مقدار
 *              به پنجرهٔ کامپایل همان ردیف گیره می‌خورد و مقدار اعمال‌شده
 *              برگردانده می‌شود، دقیقاً مثل بلوک پروفایل و PID.
 * @‎param  uint8_t__paramId [EN] 93..107‎ / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw requested value / مقدار درخواستی خام
 * @‎param  uint32_t *uint32_t__appliedValue [EN] Applied value out‎ / مقدار اعمال‌شده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_SetLimitParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one charger limit / backstop gain (ESP link GET/BULK).
 *         [FA] خواندن یک حد یا گین پشتیبان شارژر (لینک ESP).
 * @‎param  uint8_t__paramId [EN] 93..107‎ / شناسهٔ پارامتر
 * @‎param  uint32_t *uint32_t__value [EN] Live value out‎ / مقدار زنده
 * @‎return bool [EN] true = id known‎ / شناسه شناخته شده
 */
bool func__Charger_GetLimitParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value);

/* ==================== Scenario 6: dead battery / سناریوی ۶: باتری خراب ====================
 *
 * [EN] User order (2026-10-05): "a battery may never charge; it must not sit
 *      under the charger forever. After about 24 hours of continuous
 *      charging without reaching FLOAT, declare the battery dead: red LED,
 *      stop charging, and keep it stopped until the battery is replaced -
 *      like the imbalance latch."
 *
 *      The timer is PER CHANNEL, because each half has its own charger and
 *      only one of them may be the dead one. It counts the time a channel
 *      spends actively charging (BULK or ABSORB). Reaching FLOAT - the
 *      definition of "it charged" - clears it. A charge that merely pauses
 *      (input lost, JIT retry, a short OFF) does NOT clear it: the pause has
 *      to last longer than the reset gap (id 126), otherwise a battery that
 *      flickers in and out of charge would reset the clock forever and the
 *      24 h would never be reached.
 *
 *      The verdict is latched and persisted (slot 203, one bit per channel)
 *      and only a real battery swap clears it, exactly like scenario 5:
 *      the channel must report battery-absent continuously for 3 s.
 *
 * [FA] دستور کاربر: «شاید یک باتری هیچ‌وقت شارژ نشود؛ نباید دائم زیر شارژ
 *      بماند. اگر حدود ۲۴ ساعت پیوسته شارژ شد و پر نشد، باتری خراب است:
 *      LED قرمز، قطع شارژ، و تا تعویض باتری هم قطع بماند - مثل عدم‌توازن.»
 *
 *      تایمر برای هر کانال جداست، چون هر نیم شارژر خودش را دارد و ممکن است
 *      فقط یکی خراب باشد. زمانِ «در حال شارژ بودن» شمرده می‌شود و رسیدن به
 *      FLOAT (یعنی واقعاً پر شد) صفرش می‌کند. مکث کوتاه شارژ آن را صفر
 *      نمی‌کند؛ مکث باید از فاصلهٔ ریست (شناسهٔ ۱۲۶) بلندتر باشد، وگرنه
 *      باتری‌ای که مدام قطع و وصل می‌شود هرگز به ۲۴ ساعت نمی‌رسد.
 */

/** [EN] Continuous charge time after which the battery is declared dead
 *  (ms, 0 = scenario off), default 86400000 = 24 h.
 *  [FA] مدت شارژ پیوسته‌ای که پس از آن باتری خراب اعلام می‌شود (صفر = خاموش). */
#define CHG_DEAD_PARAM_TIMEOUT_MS          125u

/** [EN] A charge pause shorter than this does NOT restart the timer (ms),
 *  default 600000 = 10 min.
 *  [FA] مکث کوتاه‌تر از این، تایمر را از نو شروع نمی‌کند. */
#define CHG_DEAD_PARAM_RESET_GAP_MS        126u

/** [EN] 1 = also keep the pack off the output while the dead verdict is
 *  latched (mirror of the imbalance checkbox, id 117), default 0.
 *  [FA] یک یعنی در حالت قفلِ «باتری خراب»، باتری روی خروجی هم نرود. */
#define CHG_DEAD_PARAM_BLOCK_OUTPUT        127u

#define CHG_DEAD_PARAM_FIRST_ID            CHG_DEAD_PARAM_TIMEOUT_MS
#define CHG_DEAD_PARAM_LAST_ID             CHG_DEAD_PARAM_BLOCK_OUTPUT
#define CHG_DEAD_PARAM_OWNS(id) \
    (((id) >= CHG_DEAD_PARAM_FIRST_ID) && ((id) <= CHG_DEAD_PARAM_LAST_ID))

/** [EN] Persisted runtime slot (never a user parameter, never in a backup):
 *  bit 0 = charger 1 dead, bit 1 = charger 2 dead.
 *  [FA] اسلات ماندگار زمان‌اجرا: بیت ۰ شارژر ۱، بیت ۱ شارژر ۲. */
#define CHG_DEAD_SLOT_MASK_ID              203u

/** [EN] Battery-absent time that clears the verdict (ms) - same 3 s the
 *  imbalance scenario uses, for the same reason: a swap takes minutes.
 *  [FA] مدت نبود باتری که قفل را پاک می‌کند. */
#define CHG_DEAD_ABSENT_RESET_MS           3000u

/**
 * @brief  [EN] Write one scenario-6 parameter (ids 125..127) or replay the
 *              persisted verdict slot 203 at boot. Clamped like every other
 *              block; the applied value is returned.
 *         [FA] نوشتن پارامتر سناریوی ۶ یا پخش اسلات ۲۰۳ هنگام بوت.
 * @‎param  uint8_t__paramId [EN] 125..127 or 203‎ / شناسه
 * @param  uint32_t__value [EN] Requested value / مقدار درخواستی
 * @param  uint32_t__appliedValue [EN] Applied value out, may be NULL / مقدار اعمال‌شده
 * @return bool [EN] true when the id belongs here / شناسه متعلق است
 */
bool func__Charger_SetDeadParam(uint8_t uint8_t__paramId,
                                uint32_t uint32_t__value,
                                uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one scenario-6 parameter or the verdict slot.
 *         [FA] خواندن پارامتر سناریوی ۶ یا اسلات قضاوت.
 * @‎param  uint8_t__paramId [EN] 125..127 or 203‎ / شناسه
 * @param  uint32_t__value [EN] Live value out / مقدار زنده
 * @return bool [EN] true when the id belongs here / شناسه متعلق است
 */
bool func__Charger_GetDeadParam(uint8_t uint8_t__paramId,
                                uint32_t *uint32_t__value);

/**
 * @brief  [EN] Latched dead-battery verdict, bit 0 = charger 1, bit 1 = charger 2.
 *         [FA] قضاوت قفل‌شده: بیت ۰ شارژر ۱، بیت ۱ شارژر ۲.
 * @‎return uint8_t [EN] 0..3‎ / ماسک
 */
uint8_t func__Charger_DeadMask(void);

/**
 * @brief  [EN] Continuous charge time of ONE channel right now, in seconds -
 *              what the panel shows as that battery's progress toward the
 *              24 h verdict. v1.76 (user order: "this timer has to be counted
 *              separately for each battery"): the clock was always per channel
 *              inside this module, but only the larger of the two left the
 *              board, so the panel could not tell the two batteries apart.
 *              Each channel is now reported on its own.
 *         [FA] زمان شارژ پیوستهٔ همین کانال بر حسب ثانیه. شمارش از ابتدا هم
 *              جداگانه بود، ولی فقط بزرگ‌ترین مقدار از برد بیرون می‌رفت؛
 *              حالا هر کانال جدا گزارش می‌شود.
 * @‎param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2‎ / شمارهٔ کانال
 * @return uint32_t [EN] seconds, 0 when the index is out of range / ثانیه
 */
uint32_t func__Charger_DeadElapsedSeconds(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] True when a latched dead verdict must also keep the battery
 *              off the output (param 127).
 *         [FA] آیا قفلِ باتری خراب باید خروجی را هم ببندد.
 * @‎return bool [EN] true = keep battery off the output‎ / وتوی خروجی
 */
bool func__Charger_DeadBlocksOutput(void);

/**
 * @brief  [EN] Take-and-clear flag: a persisted verdict changed, so the
 *              caller should mark NVM slot 203 dirty.
 *         [FA] پرچم «قضاوت ماندگار عوض شد» را می‌گیرد و پاک می‌کند.
 * @‎return bool [EN] true = slot 203 needs saving‎ / نیاز به ذخیره
 */
bool func__Charger_DeadTakePersistFlag(void);

#endif /* CHARGER_H */
