/**
 * @file    charger.h
 * @brief   [EN] Two independent 12 V flyback charger channels. CHG_MASTER_ENABLE
 *          is the single overall activation gate; when it is 0 the whole
 *          Charger must remain safe-off regardless of runtime flags. All
 *          numerical protections (voltage, current, JIT and missing-battery
 *          thresholds) remain enforced whenever control is allowed.
 *          [FA] دو کانال مستقل شارژر فلای‌بک ۱۲ ولت. CHG_MASTER_ENABLE تنها
 *          کلید فعال‌سازی کلی است؛ با مقدار ۰ کل Charger صرف‌نظر از پرچم‌های
 *          زمان اجرا safe-off می‌ماند. با فعال‌بودن کنترل، همه حفاظت‌های عددی
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
 *   ۰ = فقط اسکلت safe-off: PWM متوقف، رله NC بسته (safe-idle)، سیاست
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
 *      Current1 / JIT1 (تأیید بنچ در چک‌لیست برد)؛ کانال ۲ = ۱ → PWM2 PA6
 *      / Current2 PA7 / JIT2 PB6. Trans2 به باتری ۱۲V مستقل روی VLOW وصل
 *      است؛ پک ۲۴V فقط مانیتور است و هیچ‌گاه setpoint شارژ یا شرط
 *      battery-missing برای CH2 نیست.
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
 * [FA] CHG_TRANSFORMER_KNOWN = 1: داده ترانس و زنجیره سنجش جریان روی برد
 * تأیید شده‌اند. کنترل عادی Bulk/Absorb/Float حالا اجرا می‌شود. این دروازه
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
 * [FA] مود صریح فقط-bring-up و تنها راه سوئیچینگ با CHG_TRANSFORMER_KNOWN=0:
 *      فقط Trans2 (CH2)، بدون باتری واقعی، منبع خارجی محدود، فقط
 *      اعتبارسنجی شکل‌موج - کنترل عادی اجرا نمی‌شود. حدود: فقط CH2 (CH1
 *      صفر)؛ شروع ۱٪، گام معمول ولی گیرهٔ CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE
 *      - مرحلهٔ فعلی حداکثر ۱۰٪ duty با حد ۱۰۰mA منبع خارجی؛ تا اندازه‌گیری
 *      ترانس و یک‌شدن CHG_TRANSFORMER_KNOWN بالاتر نرو. هرگز Absorb/Float و
 *      setpoint پک ۲۴V نه؛ همچنان گیت CHG_MASTER_ENABLE=1، Vin >= ۲۲۰۰۰mV و
 *      همهٔ حفاظت‌های عددی. (شکل‌موج‌های gate/shunt در مرحلهٔ ۱۰٪ با اسکوپ
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
#define CHG_CURRENT_LIMIT_MA           675u
/* [EN] Output-current regulation band: below CHG_REGULATE_LOW_MA the duty
 *      steps up, above CHG_BULK_CURRENT_MAX_MA it steps down, inside the band
 *      it holds. Only a hard fault (> CHG_CURRENT_HARD_FAULT_MA) resets the
 *      channel. Band narrowed 620..675 -> 630..650 mA (~20 mA tolerance) on
 *      user bench directive 2026-09-18: the measurement and estimate filters
 *      are now strong enough for a tight band without hunting.
 * [FA] باند تنظیم جریان خروجی: زیر ۶۳۰ افزایش دیوتی، بالای ۶۵۰ کاهش دیوتی،
 *      داخل باند نگه‌داشت (~۲۰mA تلورانس طبق دستور). فقط خطای سخت (بالاتر
 *      از ۹۵۰) کانال را ریست می‌کند. */
#define CHG_REGULATE_LOW_MA            630u
#define CHG_CURRENT_HARD_FAULT_MA      950u
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
 *      قابل‌کالیبره ماند: ETA1/ETA2 (پارامتر ۹/۱۰) ضریب هر کانال با
 *      پیش‌فرض ۰ = همانی (ریفلش عددی را عوض نمی‌کند). غیرصفر: iest = I ×
 *      Vin × η ÷ (۱۰۰۰ × Vbat) با ولتاژهای زنده تا خوانش با حرکت Vbat
 *      درست بماند. کالیبراسیون یک فرمان ESP (CAL_REFERENCE): کاربر عدد
 *      مولتی‌متر سمت باتری را می‌دهد و فریم‌ور η را از snapshot خودش
 *      می‌سازد؛ با گین‌های جدید نزدیک ۱۰۰۰×Vbat÷Vin (~۵۳۷) می‌افتد.
 *      مقادیر قدیمی ۷۵۸/۲۴۲ (فرض اشتباه شانت-در-سورس) در هیچ محاسبه‌ای
 *      نیستند - فقط پیش‌فرض بی‌اثر برای خوانده‌شدن پارامتر؛ ۲۴۲ کانال ۲
 *      خطای over-read زنجیرهٔ sense (~۲٫۹×) را هم جذب کرده - ثابت فیزیکی
 *      نیست. */
#define CHG_FLYBACK_ETA1_PERMILLE            0u
#define CHG_FLYBACK_ETA2_PERMILLE            0u

/* [EN] Clamp limits of the ESP-adjustable runtime conversion factor
 *      (v1.3): 0 = identity bypass (compiled default), 1..999 = the live
 *      Vin/Vbat conversion. The panel may zero it to return to the raw
 *      filtered reading at any time.
 * [FA] حدود گیرهٔ ضریب تبدیل زمان اجرای قابل‌تنظیم از ESP (v1.3): صفر =
 *      همانی/گذر (پیش‌فرض کامپایل)، ۱..۹۹۹ = تبدیل زندهٔ Vin/Vbat. پنل
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
/* [EN] The control task evaluates the charger every 10 ms (control_period_ms),
 *      so a plain "+5 permille per pass" would ramp 50%/s - far above the
 *      intended 0.5%/s - overshoot the current band and trip the ~15.5 A JIT.
 *      Steps are therefore rate-limited per channel:
 *      one 0.5% up-step per CHG_DUTY_RAMP_UP_INTERVAL_MS (slow soft-start ramp)
 *      and one 0.5% down-step per CHG_DUTY_RAMP_DOWN_INTERVAL_MS (twice as
 *      fast so over-current/over-voltage recovers gradually instead of
 *      cutting, but slow enough to follow the ~0.64 s current filter without
 *      hunting).
 * [FA] تسک کنترل شارژر را هر ۱۰ms اجرا می‌کند؛ بدون محدودیت زمانی، پلهٔ
 *      ۵ پرمیل ۱۰۰ بار در ثانیه اعمال می‌شد (۵۰٪/s) و JIT تریپ می‌کرد. حالا
 *      به‌ازای هر کانال: افزایش هر ۱ ثانیه یک پلهٔ ۰٫۵٪ (رمپ نرم)، کاهش هر
 *      ۵۰۰ms یک پلهٔ ۰٫۵٪ (کاهش تدریجی به‌جای قطع، برای نگه‌داشتن جریان
 *      نزدیک باند با هیسترزیس). */
#define CHG_DUTY_RAMP_UP_INTERVAL_MS   1000u
#define CHG_DUTY_RAMP_DOWN_INTERVAL_MS  500u
/* [EN] ABSORB pacing (user directive 2026-09-19): inside the 14.3-14.6 V
 *      voltage hold the fine 0.1% steps run at HALF the bulk rate, so the
 *      setpoint creeps instead of twitching (one up-step per 2000 ms, one
 *      down-step per 1000 ms). The >14.6 V overshoot escape keeps the fast
 *      500 ms coarse cadence - it is protection, not regulation.
 * [FA] کِرن‌دنِ پله در ابزورب (دستور کاربر): پله‌های ۰٫۱٪ با نصف سرعت بالک،
 *      یعنی صعود هر ۲۰۰۰ms و نزول هر ۱۰۰۰ms تا ست‌پوینت آهسته بخزد؛ فرار از
 *      اورشوت بالای ۱۴٫۶V همان سرعت ۵۰۰ms امنیتی را حفظ می‌کند. */
#define CHG_DUTY_RAMP_UP_INTERVAL_ABSORB_MS   2000u
#define CHG_DUTY_RAMP_DOWN_INTERVAL_ABSORB_MS 1000u
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
 *      دستور بنچ کاربر (پک ۴٫۵ آمپرساعت =C/90 روچنار): ابزورب فقط وقتی
 *      FLOAT می‌شود که حداقل ۱۰ دقیقه شستشو گذشته باشد **و** زیرجریان <۵۰mA
 *      به‌مدت پایدار ۶۰ ثانیه بماند؛ سقف امن ۱ ساعت در هرحال FLOAT اجباری
 *      می‌کند تا باتری هرگز-تیپر‌نشده پمپ را بیدار نگه ندارد. */
#define CHG_TAPER_CURRENT_MA           50u
#define CHG_TAPER_SUSTAIN_MS        60000u
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
 * شارژ. برای Trans2 این مقدار روی VLOW = MID-GND بررسی می‌شود. مقاومت آزاد
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
#define CHG_MAX_VALID_BATTERY_MV     15000u

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
 *        [0..4]  کانال ۰ = نیم VHIGH: state، duty پرمیل، vbat mV،
 *                جریان اولیه mA، تخمین خروجی mA (مقدار تنظیم‌شونده)
 *        [5..9]  کانال ۱ = نیم VLOW: همان پنج‌تا
 *        [10] v_in_mv، [11] v_bat24_mv، [12] v_bat12_mv،
 *        [13] v_bat_high_mv (مشتق = V24 - V12)، [14] ماسک خطا،
 *        [15] بیت اعتبار snapshot (۰/۱؛ صفر یعنی خانه‌های جریان/ولتاژ صفرند)
 *      کدهای state: 0=OFF 1=BULK 2=ABSORB 3=FLOAT 4=BRINGUP
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
 *        [5..9]   کانال بالا (باتری بالا): duty، state، vbat mV،
 *                                     Ipri mA، Iout_est mA
 *        [10..14] کانال پایین (باتری پایین): همان پنج‌تا
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
 *      The regulation band holds duty when the value sits inside
 *      CHG_REGULATE_LOW_MA..CHG_BULK_CURRENT_MAX_MA (630..650 mA).
 * [FA] همان دو جریان خروجی تخمینی که شارژر واقعاً با آن‌ها تصمیم می‌گیرد
 *      (دستور کاربر ۲۰۲۶-۰۹-۲۲: مستقیم در Live Expressions دیده شوند).
 *      کانال ۱ = نیم VHIGH (باتری بالا)، کانال ۲ = نیم VLOW (باتری پایین)؛
 *      هر پاس کنترل همراه آرایه‌های دیاگ/کالیبراسیون به‌روز می‌شوند.
 *      وقتی مقدار داخل باند ۶۳۰..۶۵۰ باشد دیوتی نگه داشته می‌شود. */
extern volatile uint32_t UINT32_T__G__ChargerIest1Ma;
extern volatile uint32_t UINT32_T__G__ChargerIest2Ma;

/* ==================== Runtime config API (ESP panel) / API پیکربندی زمان اجرا ==================== */

/**
 * @brief  [EN] Set the runtime flyback efficiency of one channel, clamped
 *              to CHG_ETA_MIN_PERMILLE..CHG_ETA_MAX_PERMILLE;
 *              flash-persisted since v1.14, ESP panel (user order 2026-09-22).
 *         [FA] بازدهی flyback یک کانال در زمان اجرا، گیرهٔ
 *              CHG_ETA_MIN_PERMILLE..CHG_ETA_MAX_PERMILLE؛ روی فلش
 *              می‌ماند از نسخهٔ ۱.۱۴، پنل ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__etaPermille [EN] Requested efficiency / بازدهی درخواستی
 * @return uint32_t [EN] Applied efficiency permille / بازدهی اعمال‌شده
 */
uint32_t func__Charger_SetEfficiencyPermille(uint8_t uint8_t__channelIndex,
                                             uint32_t uint32_t__etaPermille);

/**
 * @brief  [EN] Read the live flyback efficiency of one channel.
 *         [FA] بازدهی flyback زندهٔ یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Live efficiency permille / بازدهی زندهٔ پرمیل
 */
uint32_t func__Charger_GetEfficiencyPermille(uint8_t uint8_t__channelIndex);

/**
 * @brief  [EN] Set the ESP enable gate of one charger channel: false = PWM
 *              off + state OFF (FINAL_FAULT never released by this gate),
 *              true = soft BULK restart; flash-persisted since v1.14
 *              (user order 2026-09-22).
 *         [FA] گیت فعال‌سازی ESP یک کانال شارژر: false = PWM قطع + وضعیت
 *              OFF (قفل FINAL_FAULT با این گیت آزاد نمی‌شود)، true =
 *              ری‌استارت نرم BULK؛ روی فلش می‌ماند از نسخهٔ ۱.۱۴ (دستور
 *              کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  bool__enable [EN] true = channel allowed / کانال آزاد
 */
void func__Charger_SetChannelEspEnable(uint8_t uint8_t__channelIndex, bool bool__enable);

/**
 * @brief  [EN] Read the ESP enable gate of one charger channel.
 *         [FA] خواندن گیت فعال‌سازی ESP یک کانال شارژر.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true = channel allowed / کانال آزاد
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
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__ceilingPermille [EN] Requested ceiling / سقف درخواستی
 * @return uint32_t [EN] Applied ceiling / سقف اعمال‌شده
 */
uint32_t func__Charger_SetDutyCeilingPermille(uint8_t uint8_t__channelIndex,
                                              uint32_t uint32_t__ceilingPermille);

/**
 * @brief  [EN] Read the live PWM duty ceiling of one channel.
 *         [FA] سقف زندهٔ duty یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
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
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  bool__enable [EN] true = fixed mode on / مود فیکس روشن
 */
void func__Charger_SetDutyFixedEnable(uint8_t uint8_t__channelIndex, bool bool__enable);

/**
 * @brief  [EN] Read the runtime fixed-duty switch of one channel.
 *         [FA] کلید مود duty فیکس یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true = fixed mode on / مود فیکس روشن
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
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__dutyPermille [EN] Requested duty / duty درخواستی
 * @return uint32_t [EN] Applied stored value / مقدار ذخیره‌شده
 */
uint32_t func__Charger_SetDutyFixedPermille(uint8_t uint8_t__channelIndex,
                                            uint32_t uint32_t__dutyPermille);

/**
 * @brief  [EN] Read the stored fixed duty value of one channel.
 *         [FA] مقدار ذخیره‌شدهٔ duty فیکس یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Duty permille / duty پرمیل
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
 * @param  bool__enable [EN] true = manual on / روشن
 */
void func__Charger_SetManualTestMode(bool bool__enable);

/**
 * @brief  [EN] Read the REQUESTED manual mode flag (the parameter value
 *              the panel wrote; equals IsManualTestModeActive within one
 *              control period).
 *         [FA] پرچم «درخواست‌شدهٔ» مود دستی (مقدار پارامتری که پنل
 *              نوشته؛ حداکثر یک دورهٔ کنترل با وضعیت فعال اختلاف دارد).
 * @return bool [EN] true = requested on / درخواست روشن
 */
bool func__Charger_GetManualTestMode(void);

/**
 * @brief  [EN] Read the ACTIVE manual mode flag (owned by the charger
 *              task; used by telemetry flag b5 and by the Fault module to
 *              freeze battery-lost detection).
 *         [FA] پرچم «فعالِ» مود دستی (مالکش تسک شارژر؛ تله‌متری b5 و
 *              ماژول Fault برای فریز کردن تشخیص قطع باتری استفاده‌اش
 *              می‌کنند).
 * @return bool [EN] true = manual active / مود دستی فعال است
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
 * @param  bool__suspended [EN] true = hold gates at 0 / گیت‌ها صفر نگه داشته شوند
 */
void func__Charger_SetSuspended(bool bool__suspended);

/**
 * @brief  [EN] Read the NVM-save suspension flag.
 *         [FA] خواندن پرچم تعلیق ذخیرهٔ NVM.
 * @return bool [EN] true = suspension active / تعلیق فعال است
 */
bool func__Charger_IsSuspended(void);

/* ==================== Charger_Init / مقداردهی اولیه ==================== */
/**
 * @brief  [EN] Initialize policy state, stop every PWM channel and force a
 *              deterministic safe-idle state. With CHG_MASTER_ENABLE=0 this
 *              remains the only active behavior.
 *         [FA] state سیاست را مقداردهی اولیه می‌کند، همه PWMها را متوقف و
 *              وضعیت safe-idle قطعی را اعمال می‌کند. با CHG_MASTER_ENABLE=0
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
 * @param  measurement_snapshot_t__snap [EN] Independent low/high battery snapshot / نمونه مستقل دو باتری
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
 *      the ESP panel tab "تنظیمات شارژ" (wire params 20..26, section 5.7 of
 *      ESP_AGENT_SPEC.md). Boot defaults equal the old compile-time setpoints
 *      (CHG_*_MV / CHG_*_MA below); values live in RAM and reset at boot,
 *      like every other parameter. v1.15: the hard safety stack
 *      (CHG_CURRENT_HARD_FAULT_MA, CHG_MAX_VALID_BATTERY_MV, the 15.0 V
 *      overvoltage cutoff) is runtime-LOWERABLE from the alarms tab (ids
 *      35..37) but can NEVER be raised above the compile maxima.
 * [FA] پروفایل شارژِ قابل‌تنظیم در زمان اجرا، مشترک بین هر دو کانال، از تب
 *      «تنظیمات شارژ» پنل نوشته می‌شود (پارامترهای سیمی ۲۰..۲۶، بخش 5.7
 *      سند). پیش‌فرض بوت همان ست‌پوینت‌های کامپایل‌تایم قبلی است (ماکروهای
 *      CHG_*_MV / CHG_*_MA پایین)؛ مقادیر در RAM می‌مانند و با ریست به
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
 * @param  uint8_t__paramId [EN] 20..26 / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw requested value / مقدار درخواستی خام
 * @param  uint32_t *uint32_t__appliedValue [EN] Applied value out / مقدار اعمال‌شده
 * @return bool [EN] true = id known / شناسه شناخته شده
 */
bool func__Charger_SetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one charge-profile parameter (ESP link GET/PARAMS_BULK).
 *         [FA] خواندن یک پارامتر پروفایل شارژ (لینک ESP).
 * @param  uint8_t__paramId [EN] 20..26 / شناسهٔ پارامتر
 * @param  uint32_t *uint32_t__value [EN] Live value out / مقدار زنده
 * @return bool [EN] true = id known / شناسه شناخته شده
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
#define CHG_ALARM_PARAM_HARD_CURRENT_MA       35u  /* [EN] mA, imax+50..950, never above 950 / mA */
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
 * @param  uint8_t__paramId [EN] 35..37 / شناسهٔ پارامتر
 * @param  uint32_t__value [EN] Raw requested value / مقدار درخواستی خام
 * @param  uint32_t *uint32_t__appliedValue [EN] Applied value out / مقدار اعمال‌شده
 * @return bool [EN] true = id known / شناسه شناخته شده
 */
bool func__Charger_SetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t uint32_t__value,
                                 uint32_t *uint32_t__appliedValue);

/**
 * @brief  [EN] Read one charger alarm parameter (ESP link GET/PARAMS_BULK).
 *         [FA] خواندن یک پارامتر آلارم شارژر (لینک ESP).
 * @param  uint8_t__paramId [EN] 35..37 / شناسهٔ پارامتر
 * @param  uint32_t *uint32_t__value [EN] Live value out / مقدار زنده
 * @return bool [EN] true = id known / شناسه شناخته شده
 */
bool func__Charger_GetAlarmParam(uint8_t uint8_t__paramId,
                                 uint32_t *uint32_t__value);

#endif /* CHARGER_H */
