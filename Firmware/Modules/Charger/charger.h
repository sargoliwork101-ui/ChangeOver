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
#define CHG_MASTER_ENABLE             0u

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
/* [EN] CHG_TRANSFORMER_KNOWN must stay 0 until transformer data and current
 * calibration are measured. It is NOT bypassable; it is a separate
 * compile-time safety gate from CHG_MASTER_ENABLE. When this is 0 the only
 * allowed control path is the explicit bring-up test mode below.
 * [FA] CHG_TRANSFORMER_KNOWN باید تا اندازه‌گیری داده ترانس و کالیبراسیون جریان
 * صفر بماند. bypass نمی‌شود و دروازه امن زمان کامپایل جدا از CHG_MASTER_ENABLE است.
 * وقتی این ۰ است، تنها مسیر مجاز کنترل حالت تست صریح bring-up زیر است. */
#define CHG_TRANSFORMER_KNOWN         0u

/* ==================== Explicit limited bring-up test mode / حالت صریح تست bring-up ==================== */
/*
 * [EN] Explicit bring-up-only mode: only Trans2 (CH2), NO real battery,
 * external source current-limited, waveform-validation only. This is the
 * ONLY way to run switching when CHG_TRANSFORMER_KNOWN=0. Normal Bulk/Absorb/
 * Float setpoint control does NOT run here. Limits:
 *   CHG_BRINGUP_TEST_ENABLE = 0 for now (safe).
 *   Only installed CH2 is allowed; CH1 remains forced 0.
 *   Start duty = 1% (CHG_DUTY_START_PERMILLE).
 *   Duty step is still CHG_DUTY_STEP_PERMILLE but max duty is clamped to
 *   CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE.
 *     First bring-up stage: max 1%..2% duty (20 permille), source current
 *       limit 50 mA external. If Vin falls below 22000 mV after start, PWM is
 *       stopped and bring-up latches off until reset; it must not auto-chop.
 *     Only after scope confirms gate/shunt/relay polarity and latency, stage
 *       may be raised up to 10% and source limit up to 100 mA.
 *   Bring-up test never enters Absorb/Float, never uses a 24 V pack setpoint,
 *   and is still gated by CHG_MASTER_ENABLE=1 and Vin >= 22000 mV and all
 *   numeric protections (current, JIT, missing-battery sense).
 * [FA] حالت صریح تست فقط-bring-up: فقط Trans2 (CH2)، بدون باتری واقعی،
 * منبع خارجی محدودکننده جریان، فقط اعتبارسنجی شکل‌موج. این تنها راه
 * سوئیچینگ وقتی CHG_TRANSFORMER_KNOWN=0 است. کنترل عادی Bulk/Absorb/Float
 * در این حالت اجرا نمی‌شود. حدود:
 *   CHG_BRINGUP_TEST_ENABLE = 0 فعلاً (امن).
 *   فقط CH2 نصب‌شده مجاز است؛ CH1 همیشه صفر.
 *   duty شروع = ۱٪ (CHG_DUTY_START_PERMILLE).
 *   گام duty همان CHG_DUTY_STEP_PERMILLE ولی حداکثر duty به
 *   CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE محدود می‌شود.
 *     مرحله اول bring-up: حداکثر ۱٪ تا ۲٪ duty (۲۰ پرمیل)، حد جریان منبع
 *       خارجی ۵۰ میلی‌آمپر.
 *     فقط پس از تأیید اسکوپ gate/shunt/relay polarity/latency، مرحله را می‌توان
 *       تا ۱۰٪ و حد منبع را تا ۱۰۰ میلی‌آمپر بالا برد.
 *   تست bring-up هرگز وارد Absorb/Float نمی‌شود، از setpoint پک ۲۴ ولت
 *   استفاده نمی‌کند، و همچنان با CHG_MASTER_ENABLE=1، Vin >= ۲۲۰۰۰mV و همه
 *   حفاظت‌های عددی (جریان، JIT، sense باتری) گیت می‌شود. */
#define CHG_BRINGUP_TEST_ENABLE                0u
#define CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE     20u  /* [EN] 2% max for the first bring-up stage / حداکثر ۲٪ مرحله اول */
#define CHG_BRINGUP_TEST_SOURCE_LIMIT_MA       50u  /* [EN] external total-source limit only; not measured by MCU / فقط حد خارجی کل منبع */
#define CHG_BRINGUP_TEST_OUTPUT_LIMIT_MA       50u  /* [EN] conservative measured channel-current limit / حد محافظه‌کارانه خروجی */
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
#define CHG_INPUT_VALID_MV           22000u
/* [EN] Recovery threshold prevents PWM chatter when the current-limited
 *      source sags around the 22 V cutoff. A fresh/recovered input must reach
 *      23 V; an already-ready input is stopped below 22 V.
 * [FA] آستانهٔ برگشت برای جلوگیری از قطع‌و‌وصل PWM هنگام افت منبع محدودشده
 *      اطراف ۲۲V است. ورودی تازه/برگشته باید به ۲۳V برسد و ورودی آماده زیر
 *      ۲۲V متوقف می‌شود. */
#define CHG_INPUT_RECOVER_MV         23000u
#define CHG_DUTY_START_PERMILLE        10u
#define CHG_DUTY_STEP_PERMILLE          5u
#define CHG_DUTY_RETRY_SECOND_MAX       100u
#define CHG_DUTY_MAX_PERMILLE         1000u
#define CHG_ABSORB_HOLD_MS          600000u
#define CHG_JIT_LOCKOUT_MS            3000u
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

/* ==================== Debug watch variables / متغیرهای قابل مشاهده در دیباگ ==================== */
/* [EN] Volatile telemetry is intentionally small and read-only from the
 *      debugger. It explains why PWM was stopped without changing policy.
 * [FA] این telemetry کوچک volatile است و فقط برای مشاهده در debugger است؛
 *      سیاست کنترل را تغییر نمی‌دهد و علت توقف PWM را نشان می‌دهد. */
#define CHG_DEBUG_REASON_NONE                 0u
#define CHG_DEBUG_REASON_MASTER_OFF           1u
#define CHG_DEBUG_REASON_SNAPSHOT_INVALID     2u
#define CHG_DEBUG_REASON_APP_SAFE_OR_FAULT    3u
#define CHG_DEBUG_REASON_TRANSFORMER_GATE     4u
#define CHG_DEBUG_REASON_INPUT_LOW            5u
#define CHG_DEBUG_REASON_BATTERY_INVALID      6u
#define CHG_DEBUG_REASON_CURRENT_LIMIT        7u
#define CHG_DEBUG_REASON_JIT_TRIP             8u
#define CHG_DEBUG_REASON_FINAL_FAULT          9u

extern volatile uint32_t CHG_DEBUG__G__EvaluateCount;
extern volatile uint32_t CHG_DEBUG__G__InputMv;
extern volatile uint32_t CHG_DEBUG__G__BatteryMv;
extern volatile uint32_t CHG_DEBUG__G__CurrentMa;
extern volatile uint16_t CHG_DEBUG__G__AppliedDutyPermille;
extern volatile uint16_t CHG_DEBUG__G__LastRequestedDutyPermille;
extern volatile uint8_t CHG_DEBUG__G__AppliedChannel;
extern volatile uint8_t CHG_DEBUG__G__Channel2State;
extern volatile uint8_t CHG_DEBUG__G__Channel2JitTrips;
extern volatile uint8_t CHG_DEBUG__G__InputReady;
extern volatile uint8_t CHG_DEBUG__G__InputLockout;
extern volatile uint8_t CHG_DEBUG__G__StopReason;

#endif /* CHARGER_H */
