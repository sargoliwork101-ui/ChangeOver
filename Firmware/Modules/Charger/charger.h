/**
 * @file    charger.h
 * @brief   [EN] Charger for 2-channel flyback, DCM, 50kHz, safe state machine.
 *          [FA] شارژر فلای‌بک دوکاناله، DCM، 50kHz، ماشین حالت ایمن.
 *
 * @note    [EN] All thresholds are provisional until battery datasheet and transformer
 *              parameters (Lp, Np/Ns, Ipeak_max, η) are validated on board.
 *              If IsConfigValid() is false or measurement invalid, PWM stays 0.
 *              PB5/Q1 (MCU Power-Path) and PB11/Q17 (Changeover) are NOT owned by Charger.
 *          [FA] همه آستانه‌ها تا تأیید دیتاشیت و پارامترهای ترانس provisional هستند.
 *              اگر IsConfigValid نادرست یا measurement نامعتبر، PWM صفر می‌ماند.
 */

#ifndef CHARGER_H
#define CHARGER_H

/* ==================== Includes ==================== */
#include "app_types.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== PWM timing ==================== */
/* [EN] 72MHz/(0+1)/(1439+1)=50kHz, 1440 counts, 0.0694%/step. 1%≈14-15 counts.
   [FA] فرمول 50kHz، 1440 شمارش، 1% حدود 14-15. Period 999 resolution 1000 داشت اما فرکانس نادرست بود؛ دلیل تغییر فرکانس است. */
#define CHG_PWM_PRESCALER               0u
#define CHG_PWM_PERIOD                  1439u
#define CHG_PWM_RESOLUTION              1440u
#define CHG_PWM_FREQ_HZ                 50000u
#define CHG_TIMER_CLOCK_HZ              72000000u

/* ==================== Battery capacity (24V pack) ==================== */
/* [EN] 2×12V 4.5Ah in series → 24V pack still 4.5Ah (not 9Ah). Bulk 0.15C×4.5Ah=675mA provisional.
   [FA] دو 12V سری → پک 24V همان 4.5Ah. Bulk 0.15C=675mA اولیه. */
#define CHG_BAT_CAPACITY_MAH            4500u
#define CHG_BULK_MAX_C_PERMILLE         150u   /* 0.15C =150/1000 */
#define CHG_BULK_MAX_MA                 675u   /* ((4500*150)+500)/1000 with uint64, provisional */

/* ==================== 12V battery voltages (provisional until datasheet) ==================== */
/* [EN] 14.4V/13.5V only for 12V output; provisional, need AGM/GEL/Flooded datasheet.
   [FA] فقط برای خروجی 12V؛ provisional. */
#define CHG_12V_ABSORB_MV                14400u
#define CHG_12V_FLOAT_MV                 13500u
#define CHG_12V_REENTRY_MV               12800u  /* provisional: float re-entry ~12.8V, need datasheet */

/* ==================== 24V pack voltages (provisional) ==================== */
/* [EN] Pack 24V absorb ~28800, float 27000-27600. Final from battery datasheet.
   [FA] پک 24V Absorb 28800 Float 27000-27600 provisional. */
#define CHG_24V_ABSORB_MV                28800u
#define CHG_24V_FLOAT_MV                 27000u  /* lower of range, provisional */
#define CHG_24V_FLOAT_MAX_MV             27600u  /* upper for validation */
#define CHG_24V_REENTRY_MV               25200u  /* provisional ~25.2V, need datasheet */

/* ==================== Absorption tail ==================== */
/* [EN] Tail 200mA ≈0.044C for 4.5Ah. Need min/max time + stable time + valid measurement + no parallel load.
   [FA] Tail 200mA، با زمان حداقل/حداکثر و پایداری. */
#define CHG_ABSORB_TAIL_MA               200u
/* [EN] Provisional times until client confirms; 0 means invalid → safe-off per spec.
   [FA] تا تأیید کارفرما 0 → invalid و safe-off؛ حدس نزن. Host test uses provisional non-zero for behavior. */
#define CHG_ABSORB_MIN_TIME_MS           1800000u  /* 30 min provisional, must be confirmed */
#define CHG_ABSORB_MAX_TIME_MS           14400000u /* 4 h provisional, must be confirmed */
#define CHG_ABSORB_TAIL_STABLE_TIME_MS   300000u   /* 5 min provisional */

/* ==================== Soft start & bulk rest ==================== */
/* [EN] Start 1% (10 permille), ramp 0.5%/s =5 permille/s. Duty not safety alone, current must be limited.
   [FA] شروع 1%، شیب 0.5%/s. */
#define CHG_SOFT_START_DUTY_PERMILLE     10u
#define CHG_SOFT_START_RAMP_PER_SECOND   5u

/* [EN] Product cycle: 30 min work, 5 min rest only in Bulk (not Absorb/Float). Tunable.
   [FA] چرخه محصول فقط Bulk: 30 دقیقه کار 5 دقیقه استراحت. */
#define CHG_BULK_WORK_MS                 1800000u  /* 30 min */
#define CHG_BULK_REST_MS                 300000u   /* 5 min */

/* ==================== Current measurement ==================== */
/* [EN] Shunt 0.01Ω, LM358 avg, LM393 peak. Need offset/gain calibration per board.
   Ipri_mA = max(0,(raw - offset)*scale). For 0.4-0.7A expect few hundred counts, not 76.
   Raw values at 0A and known current + Vshunt + LM358 + ADC pin + Vin/Vout must be logged.
   [FA] تبدیل با offset و gain، نیاز به کالیبراسیون. */
#define CHG_CURRENT_RAW_OFFSET           0u      /* provisional: to be measured at 0A */
#define CHG_CURRENT_SCALE_NUM            10u     /* provisional placeholders */
#define CHG_CURRENT_SCALE_DEN            1u

/* State-machine retries */
#define CHG_MAX_RETRY_COUNT              3u

/* [EN] JIT lockout after trip, tunable. Default 3000ms safe.
   [FA] زمان قفل بعد از JIT، قابل تنظیم. */
#define CHG_JIT_LOCKOUT_MS               3000u

/* ==================== Balance ==================== */
/* [EN] Balance requires both PWM zero, settle time, separate V_BAT_LOW (mid) and V_BAT_HIGH (V24-mid).
   If no independent charge path, only monitor (BALANCE_REQUIRED) not active balance.
   Difference >1V for 10min → fault. Exactly 1V is not fault (monitor only).
   Battery with higher voltage during charge is not necessarily stronger (higher IR).
   Independent path must be verified via net/continuity before active balance.
   [FA] بالانس فقط با مسیر مستقل و پس از settle. */
#define CHG_BALANCE_DIFF_MV              1000u
#define CHG_BALANCE_TIME_MS              600000u  /* 10 min */
#define CHG_BALANCE_SETTLE_MS            1000u    /* settle after PWM0 before reading */
/* [EN] Set to 1 only after net/continuity confirms two independent charge nets/paths.
   [FA] فقط پس از تأیید اتصال واقعی هر دو مسیر. */
#define CHG_BALANCE_INDEPENDENT_PATH_CONFIRMED 0u

/* ==================== Temperature ==================== */
/* [EN] No NTC now. No temp compensation. Keep 30/5 as thermal fallback,
   current limit and timing conservative. Real thermal needs NTC on battery and power stage.
   [FA] فعلاً بدون سنسور دما، ادعای جبران دمایی نکن. */

/* ==================== Transformer params (unknown → report as open) ==================== */
/* [EN] Lp, Np/Ns, Ipeak_max, Duty_max, η must be provided and validated. Until then safe-off.
   Charger is NOT yet enableable when CHG_TRANSFORMER_KNOWN=0; report must not claim full implementation.
   [FA] پارامترهای ترانس تا تأیید باز، شارژر هنوز قابل فعال‌سازی نیست. */
#define CHG_TRANSFORMER_KNOWN            0u  /* 0=unknown → IsConfigValid false, 1=validated */
#define CHG_EFFICIENCY_PERMILLE          850u /* provisional η 85% for Iout_est, calibrate on board */
#define CHG_NO_TEMP_COMPENSATION         1u   /* no NTC, thermal fallback 30/5, no claim of compensation */

/* ==================== Config struct (for ESP runtime, not #define override) ==================== */
typedef struct
{
    uint32_t version;
    uint32_t crc;
    uint32_t bat_capacity_mah;
    uint32_t bulk_max_ma;
    uint32_t v12_absorb_mv;
    uint32_t v12_float_mv;
    uint32_t v12_reentry_mv;
    uint32_t v24_absorb_mv;
    uint32_t v24_float_mv;
    uint32_t v24_reentry_mv;
    uint32_t absorb_min_ms;
    uint32_t absorb_max_ms;
    uint32_t absorb_tail_ma;
    uint32_t absorb_tail_stable_ms;
    uint16_t soft_start_permille;
    uint16_t soft_ramp_permille_per_s;
    uint32_t bulk_work_ms;
    uint32_t bulk_rest_ms;
    uint32_t current_raw_offset;
    uint32_t current_scale_num;
    uint32_t current_scale_den;
    uint8_t  reserved[8];
} charger_config_t;

/* ==================== State machine ==================== */
typedef enum
{
    CHG_STATE_IDLE = 0,
    CHG_STATE_PRECHECK,
    CHG_STATE_PRECHARGE,
    CHG_STATE_SOFTSTART,
    CHG_STATE_BULK,
    CHG_STATE_ABSORB,
    CHG_STATE_FLOAT,
    CHG_STATE_REST,
    CHG_STATE_FAULT,
    CHG_STATE_SUSPEND,
    CHG_STATE_BALANCE
} charger_state_t;

/* ==================== Fault bits (internal) ==================== */
#define CHG_FAULT_NONE                  0u
#define CHG_FAULT_JIT1                  (1u<<0)
#define CHG_FAULT_JIT2                  (1u<<1)
#define CHG_FAULT_OVERCURRENT           (1u<<2)
#define CHG_FAULT_CONFIG_INVALID        (1u<<3)
#define CHG_FAULT_MEASUREMENT_INVALID   (1u<<4)
#define CHG_FAULT_BATTERY_MISSING       (1u<<5)
#define CHG_FAULT_INPUT_MISSING         (1u<<6)
#define CHG_FAULT_ABSORB_TIMEOUT        (1u<<7)
#define CHG_FAULT_BALANCE               (1u<<8)

/* ==================== Functions ==================== */

/**
 * @brief  [EN] Init charger policy. Must leave PWM at 0%.
 *         [FA] سیاست شارژر را Init می‌کند. PWM باید ۰٪ بماند.
 */
void func__Charger_Init(void);

/**
 * @brief  [EN] Compute PWM from snapshot and system state.
 *         [FA] PWM را از نمونه و حالت حساب می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  app_state_t__state [EN] System state / حالت
 */
void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, app_state_t app_state_t__state);

/**
 * @brief  [EN] Check if charger config is valid (provisional params, transformer, times).
 *         [FA] بررسی اعتبارسنجی کانفیگ شارژر.
 * @return bool [EN] true if valid / معتبر
 */
bool func__Charger_IsConfigValid(void);

/**
 * @brief  [EN] Convert permille duty 0..1000 to timer compare counts for 1440 resolution.
 *         [FA] تبدیل پرمیل به شمارش تایمر.
 * @param  uint16_t__permille [EN] Duty 0..1000
 * @return uint32_t [EN] Compare counts 0..1439
 */
uint32_t func__Charger_DutyPermilleToCounts(uint16_t uint16_t__permille);

/**
 * @brief  [EN] Estimate output current: Iout_est = η*Vin*Ipri_avg/Vout (DCM, with checks).
 *         [FA] تخمین جریان خروجی DCM.
 * @param  uint32_t__vin_mv [EN] Vin mV
 * @param  uint32_t__vout_mv [EN] Vout mV
 * @param  uint32_t__ipri_avg_ma [EN] Ipri avg mA
 * @param  uint32_t__eta_permille [EN] η 0..1000 (e.g., 850=85%)
 * @return uint32_t [EN] Iout mA or 0 on invalid
 */
uint32_t func__Charger_EstimateIoutDcm(uint32_t uint32_t__vin_mv, uint32_t uint32_t__vout_mv, uint32_t uint32_t__ipri_avg_ma, uint32_t uint32_t__eta_permille);

/**
 * @brief  [EN] Convert raw ADC to Ipri mA: max(0,(raw-offset)*scale)
 *         [FA] تبدیل ADC خام به جریان.
 * @param  uint16_t__raw [EN] Raw ADC
 * @return uint32_t [EN] mA
 */
uint32_t func__Charger_RawToIpriMa(uint16_t uint16_t__raw);

/**
 * @brief  [EN] Trip handler for JIT (LM393) — latch fault, PWM 0 fast.
 *         [FA] مدیریت تریپ JIT.
 * @param  uint32_t__jitMask [EN] CHG_FAULT_JIT1/2
 */
void func__Charger_OnJitTrip(uint32_t uint32_t__jitMask);

/**
 * @brief  [EN] Get current state (for tests).
 *         [FA] گرفتن حالت فعلی.
 * @return charger_state_t [EN] State
 */
charger_state_t func__Charger_GetState(void);

/**
 * @brief  [EN] Get last fault mask.
 *         [FA] گرفتن ماسک خطا.
 * @return uint32_t [EN] Mask
 */
uint32_t func__Charger_GetFault(void);

#endif /* CHARGER_H */
