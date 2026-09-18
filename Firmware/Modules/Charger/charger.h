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
 * installed transformer changes. Current board has Trans2 only. Set CH1 to 1
 * only after the second transformer, its current path and its JIT input have
 * been verified on the board.
 *   Channel 1 (logical ch1) = 0 now → PWM1 compare must stay 0/PWM1 stopped,
 *                             no nonzero duty for CH1 in any path.
 *   Channel 2 (logical ch2) = 1 now → PWM2 PA6 / Current2 PA7 / JIT2 PB6.
 * Trans2 is connected to one independent 12 V battery on VLOW = MID - GND;
 * the 24 V pack measurement is monitor-only and is never a charge setpoint or
 * missing-battery condition for CH2.
 * [FA] برای عوض‌کردن ترانس مونتاژشده فقط همین دو ثابت تغییر می‌کنند. برد فعلی
 * فقط Trans2 دارد. CH1 را فقط پس از تأیید سخت‌افزاری ترانس دوم، مسیر جریان
 * و ورودی JIT آن ۱ کنید.
 *   کانال ۱ = ۰ اکنون → PWM1 compare همیشه صفر، PWM1 متوقف، هیچ duty غیرصفری
 *             برای CH1 اعمال نشود.
 *   کانال ۲ = ۱ اکنون → PWM2 PA6 / Current2 PA7 / JIT2 PB6.
 * Trans2 به یک باتری ۱۲ ولت مستقل روی VLOW = MID - GND وصل است؛ مقدار پک
 * ۲۴ ولت فقط مانیتور است و هیچ‌گاه setpoint شارژ یا شرط battery-missing برای
 * CH2 نیست.
 */
#define CHG_CHANNEL_1_INSTALLED       0u
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
#define CHG_FLOAT_MV                  13500u
#define CHG_REENTRY_MV               12800u
#define CHG_BULK_CURRENT_MAX_MA       675u
#define CHG_CURRENT_LIMIT_MA           675u
/* [EN] Output-current regulation band: below CHG_REGULATE_LOW_MA the duty
 *      steps up, above CHG_BULK_CURRENT_MAX_MA it steps down, inside the band
 *      it holds. Only a hard fault (> CHG_CURRENT_HARD_FAULT_MA) resets the
 *      channel. This band is what keeps the normal path from oscillating
 *      ramp/cut/restart around a single 675 mA threshold.
 * [FA] باند تنظیم جریان خروجی: زیر ۶۲۰ افزایش دیوتی، بالای ۶۷۵ کاهش دیوتی،
 *      داخل باند نگه‌داشت. فقط خطای سخت (بالاتر از ۹۵۰) کانال را ریست می‌کند. */
#define CHG_REGULATE_LOW_MA            620u
#define CHG_CURRENT_HARD_FAULT_MA      950u

/* [EN] Primary->output current estimate for the charge decisions: the shunt
 *      sits in the MOSFET source leg (primary side), while Bulk/Absorb/Float
 *      limits are output (battery) currents. Estimate Iout =
 *      Ipri_avg * Vin * eta / Vbat. eta measured on the bench at
 *      Vin 23.5 V, Vbat 12.55 V, ~110 mA primary (output 195 mA): about
 *      946-956 per mille, rounded to 950. Re-measure if the operating point
 *      moves far. Vbat is clamped to CHG_OUTPUT_EST_MIN_VBAT_MV so a momentary
 *      bad reading cannot divide by ~0; the estimate is only used inside the
 *      normal charge path, never in the bring-up source-limit path.
 * [FA] تخمین جریان خروجی از اندازه‌گیری اولیه: شنت در پایه سورس ماسفت (سمت
 *      اولیه) است ولی حدهای شارژ جریان خروجی‌اند. Iout = Ipri*Vin*eta/Vbat
 *      با eta برابر 950 پرمیل (اندازه‌گیری روی برد). فقط در مسیر شارژ نرمال
 *      مصرف می‌شود، نه در مسیر برینگ‌آپ. */
#define CHG_FLYBACK_EFFICIENCY_PERMILLE 950u
#define CHG_OUTPUT_EST_MIN_VBAT_MV     1000u
#define CHG_INPUT_VALID_MV           22000u
#define CHG_DUTY_START_PERMILLE        10u
#define CHG_DUTY_STEP_PERMILLE          5u
#define CHG_DUTY_RETRY_SECOND_MAX       100u
#define CHG_DUTY_MAX_PERMILLE          300u  /* [EN] 30% cap: keeps primary peak below the ~15.5 A JIT trip even at the cap; regulation band settles near ~19% / سقف ۳۰٪: پیک اولیه زیر تریپ JIT می‌ماند */
#define CHG_ABSORB_HOLD_MS          600000u
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

#endif /* CHARGER_H */
