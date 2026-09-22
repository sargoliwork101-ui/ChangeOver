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
 *   0 = safe-off skeleton only. All PWM outputs are kept stopped and the
 *       transformer input relay keeps its NC contact closed (coil off,
 *       safe-idle). JIT and relay disconnect policy stay inactive: no retry
 *       sequencing and no coil energization. This is the only overall
 *       activation gate. Runtime flags such as power_stage_enabled/pwm_max_duty
 *       are NOT hidden hard gates for Charger; numeric protections are still
 *       not bypassed.
 *   1 = Charger is allowed to enter the per-channel control loop only when
 *       all explicit hardware conditions (valid snapshot, installed channel,
 *       Vin ADC >= 22000 mV, valid battery sense, current limits, JIT policy)
 *       are also satisfied.
 * [FA] تنها کلید فعال‌سازی کلی Charger.
 *   ۰ = فقط اسکلت safe-off. همه خروجی‌های PWM متوقف می‌مانند و رله ورودی
 *       ترانس NC را بسته نگه می‌دارد (safe-idle). این تنها دروازه کلی است.
 *       پرچم‌های زمان اجرا مثل power_stage_enabled/pwm_max_duty دروازه پنهان
 *       برای Charger نیستند؛ حفاظت‌های عددی هیچ‌گاه bypass نمی‌شوند.
 *   ۱ = Charger فقط در صورت برقرار بودن همه شرایط صریح سخت‌افزاری
 *       (snapshot معتبر، کانال نصب‌شده، Vin ADC >= 22000mV، sense باتری معتبر،
 *       حدهای جریان، سیاست JIT) اجازه ورود به حلقه کنترل هر کانال را دارد.
 */
#define CHG_MASTER_ENABLE             1u

/* ==================== Board/test selection constants / ثابت‌های انتخاب برد و تست ==================== */
/*
 * [EN] These are the only two assembly-selection constants to change when the
 * installed transformer changes. Both transformers are installed now. Set CH1 to 1
 * only after the second transformer, its current path and its JIT input have
 * been verified on the board.
 *   Channel 1 (logical ch1) = 1 now (user bring-up order 2026-09-20) →
 *                             PWM1 PA0 / Current1 / JIT1 active; bench
 *                             verification of Trans1/JIT1 stays on the board
 *                             checklist below.
 *   Channel 2 (logical ch2) = 1 now → PWM2 PA6 / Current2 PA7 / JIT2 PB6.
 * Trans2 is connected to one independent 12 V battery on VLOW = MID - GND;
 * the 24 V pack measurement is monitor-only and is never a charge setpoint or
 * missing-battery condition for CH2.
 * [FA] برای عوض‌کردن ترانس مونتاژشده فقط همین دو ثابت تغییر می‌کنند.
 *   کانال ۱ = ۱ اکنون (دستور راه‌اندازی کاربر ۲۰۲۶-۰۹-۲۰) → PWM1 PA0 /
 *             Current1 / JIT1 فعال؛ تأیید بنجی Trans1/مسیر جریان/JIT1 در
 *             چک‌لیست برد می‌ماند.
 *   کانال ۲ = ۱ اکنون → PWM2 PA6 / Current2 PA7 / JIT2 PB6.
 * Trans2 به یک باتری ۱۲ ولت مستقل روی VLOW = MID - GND وصل است؛ مقدار پک
 * ۲۴ ولت فقط مانیتور است و هیچ‌گاه setpoint شارژ یا شرط battery-missing برای
 * CH2 نیست.
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
 * [EN] Explicit bring-up-only mode: only Trans2 (CH2), NO real battery,
 * external source current-limited, waveform-validation only. This is the
 * ONLY way to run switching when CHG_TRANSFORMER_KNOWN=0. Normal Bulk/Absorb/
 * Float setpoint control does NOT run here. Limits:
 *   CHG_BRINGUP_TEST_ENABLE = 1: active board bring-up (gate/shunt waveforms
 *     already scope-verified at the 10% stage with a calibrated current path).
 *   Only installed CH2 is allowed; CH1 remains forced 0.
 *   Start duty = 1% (CHG_DUTY_START_PERMILLE).
 *   Duty step is still CHG_DUTY_STEP_PERMILLE but max duty is clamped to
 *   CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE.
 *     Current stage: max 10% duty (100 permille), source current
 *       limit 100 mA external. Do not raise above this until the transformer
 *       data is measured and CHG_TRANSFORMER_KNOWN flips to 1.
 *   Bring-up test never enters Absorb/Float, never uses a 24 V pack setpoint,
 *   and is still gated by CHG_MASTER_ENABLE=1 and Vin >= 22000 mV and all
 *   numeric protections (current, JIT, missing-battery sense).
 * [FA] حالت صریح تست فقط-bring-up: فقط Trans2 (CH2)، بدون باتری واقعی،
 * منبع خارجی محدودکننده جریان، فقط اعتبارسنجی شکل‌موج. این تنها راه
 * سوئیچینگ وقتی CHG_TRANSFORMER_KNOWN=0 است. کنترل عادی Bulk/Absorb/Float
 * در این حالت اجرا نمی‌شود. حدود:
 *   CHG_BRINGUP_TEST_ENABLE = 1 فعال: bring-up برد (شکل‌موج‌های gate/shunt
 *     روی مرحله ۱۰٪ با اسکوپ تأیید شده و مسیر جریان کالیبره است).
 *   فقط CH2 نصب‌شده مجاز است؛ CH1 همیشه صفر.
 *   duty شروع = ۱٪ (CHG_DUTY_START_PERMILLE).
 *   گام duty همان CHG_DUTY_STEP_PERMILLE ولی حداکثر duty به
 *   CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE محدود می‌شود.
 *     مرحله فعلی: حداکثر ۱۰٪ duty (۱۰۰ پرمیل)، حد جریان منبع خارجی
 *       ۱۰۰ میلی‌آمپر. تا اندازه‌گیری داده ترانس و یک‌شدن
 *       CHG_TRANSFORMER_KNOWN بالاتر از این نرو.
 *   تست bring-up هرگز وارد Absorb/Float نمی‌شود، از setpoint پک ۲۴ ولت
 *   استفاده نمی‌کند، و همچنان با CHG_MASTER_ENABLE=1، Vin >= ۲۲۰۰۰mV و همه
 *   حفاظت‌های عددی (جریان، JIT، sense باتری) گیت می‌شود. */
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
/* [EN] How long a channel must CONTINUOUSLY SEE battery voltage before any
 *      bulk charge start is allowed (user refinement 2026-09-19: the count
 *      starts the moment battery voltage is seen - input validity is NOT
 *      part of it; original directive the same day: "give it 10..20 s to
 *      settle the battery is connected, then start"). 15 s = middle of his
 *      window. Mid-cycle paths (JIT resume, 12.8 V reentry) stay exempt
 *      because their presence stamp is already live by definition. This gate
 *      also kills the bat-lost FLAP: flag clear -> BULK -> re-pump -> flag
 *      set again every 30 s (and the yellow blink inside the buzzer that
 *      came with it) - with the cable out the voltage is invalid, the stamp
 *      stays 0, and BULK never re-arms.
 * [FA] چند ثانیه «دیده‌شدن ولتاژ باتری» پیوسته لازم است تا شروعِ بالک
 *      اجازه بگیرد (اصلاحیهٔ کاربر: شمارش از لحظهٔ دیدن ولتاژ باتری؛ دستورِ
 *      اصلی همان روز: ۱۰ تا ۲۰ ثانیه ثبات، بعد شارژ؛ ۱۵ ثانیه انتخاب شد).
 *      مسیرهای میان‌چرخه معاف‌اند. همین گیت چرخهٔ پینگ‌پنگِ
 *      قطع‌باتری (آلارم پاک → بالک → پمپ → آلارم دوباره، و چشمک زرد وسط
 *      بوق) را کاملاً می‌کشد. */
#define CHG_CONNECT_SETTLE_MS        15000u
#define CHG_BULK_CURRENT_MAX_MA       650u
/* [EN] TEMPORARY bench diagnostic (2026-09-18): fixed duty, NO ramp and NO
 *      band regulation. Set to 0 to return to normal charge control. When 1,
 *      after all the usual gates (valid snapshot, Vin >= 22000 mV, battery
 *      sense valid, JIT, >950 mA hard fault) the installed channel simply
 *      holds CHG_FIXED_DUTY_TEST_DUTY_PERMILLE at 15% and skips every
 *      regulation decision; switching also stops while Vbat >= 14.4 V so the
 *      battery cannot be pushed into overcharge with regulation disabled.
 *      Purpose: one stable operating point to calibrate the measurement
 *      coefficients (scope MEAN at LM358 out, bench input/output V and I
 *      vs. the firmware readings).
 * [FA] حالت تست موقت بنچ: دیوتی ثابت ۱۵٪، بدون رمپ و بدون باند جریان؛ فقط
 *      برای کالیبره‌کردن ضرایب اندازه‌گیری روی یک نقطهٔ پایدار. همهٔ
 *      حفاظت‌ها فعال می‌مانند و روی ۱۴٫۴V سوئیچینگ می‌ایستد. برای برگشت به
 *      شارژ نرمال، مقدار را ۰ کن. */
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
/* [EN] Primary->output current estimate for the charge decisions: the shunt
 *      sits in the MOSFET source leg (primary side), while Bulk/Absorb/Float
 *      limits are output (battery) currents. Estimate Iout =
 *      Ipri_avg * Vin * eta / Vbat.
 *      Fixed at the user's bench diagnostic point with fixed 15% duty
 *      (2026-09-18): MEAN at the LM358 output 362 mV -> Ipri_true =
 *      362/1.01 = 358 mA; real output 441 mA x 13.0 V. So that the estimate
 *      equals the real output current: eta = Iout x Vbat / (Ipri x Vin) =
 *      441 x 13100 / (358 x 22900) = 705 permille. eta is load dependent
 *      (~950 at the light 110 mA point, ~705 at 440 mA out); the band lives
 *      near this heavy point. Vbat is clamped to
 *      CHG_OUTPUT_EST_MIN_VBAT_MV so a momentary bad reading cannot divide
 *      by ~0; the estimate is only used inside the normal charge path, never
 *      in the bring-up source-limit path.
 * [FA] از نقطهٔ تست دیوتی ثابت ۱۵٪: جریان واقعی اولیه ۳۵۸mA (MEAN اسکوپ
 *      تقسیم بر ۱٫۰۱)، خروجی واقعی ۴۴۱mA؛ با ۷۰۵ پرمیل تخمین = واقعیت. */
#define CHG_FLYBACK_EFFICIENCY_PERMILLE 705u
#define CHG_OUTPUT_EST_MIN_VBAT_MV     1000u
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
 *         charge is DONE, not active - for the UI the charging yellow blink
 *         stops as soon as the pump parks, and the fault pump-window is not
 *         armed there either).
 *         [FA] true وقتی دست‌کم یک کانال نصب‌شده واقعاً در حال شارژ است
 *         (بالک/ابزورب؛ فلوت پارک‌شده یعنی کار تمام شده و فعال حساب
 *         نمی‌شود - نه زرد چشمک می‌زند نه آشکارساز قطع باتری مسلح است).
 * @return bool [EN] true if any channel is charging / اگر هر کانالی شارژ کند true
 */
bool func__Charger_IsAnyChannelActive(void);

#endif /* CHARGER_H */
