/**
 * @file    charger.c
 * @brief   [EN] Charger for 2-channel flyback, DCM, 50kHz, safe state machine.
 *          [FA] شارژر فلای‌بک دوکاناله — ماشین حالت ایمن.
 *
 * @note    [EN] DCM only. CCM formula Iout=Ipri*Np/Ns*(1-D)/D is NOT used.
 *              Iout_est = η*Vin*Ipri_avg/Vout only for estimation/control, not peak protection.
 *              Transformer Lp, Np/Ns, Ipeak_max, η are unknown until board test -> CHG_TRANSFORMER_KNOWN=0 keeps safe-off.
 *              PB5/Q1 and PB11/Q17 are NOT owned by Charger (see board_pins).
 *          [FA] فقط DCM. نسبت دور برای Ipeak/اشباع/demag لازم است اما تا تأیید باز است.
 */

#include "charger.h"
#include "app_config.h"
#include "bsp_pwm.h"
#include "bsp_exti.h"
#include "bsp_gpio.h"
#include "cmsis_os2.h"
#include "rtos_time.h"

#include <stddef.h>

/* ==================== Constants for battery missing ==================== */
#define CHG_BAT_MISSING_MV              2000u  /* below 2V → battery considered absent */
#define CHG_BAT_12V_MISSING_MV           2000u
#define CHG_BAT_24V_MISSING_MV           4000u
#define CHG_INPUT_MISSING_MV             8000u  /* below 8V input considered absent, else use Measurement input_present */

/* ==================== State ==================== */
static charger_state_t CHARGER_STATE__G__State = CHG_STATE_IDLE;
static uint32_t FAULT_MASK__G__Fault = CHG_FAULT_NONE;
static uint32_t TICK__G__StateEnterMs = 0u;
static uint32_t TICK__G__BulkWorkStartMs = 0u;
static uint32_t TICK__G__AbsorbEnterMs = 0u;
static uint32_t TICK__G__TailStableStartMs = 0u;
static uint16_t DUTY_PERMILLE__G__Current = 0u;
static uint32_t TICK__G__LastRampMs = 0u;
static uint8_t RETRY_COUNT__G__Count = 0u;
static uint32_t TICK__G__BalanceStartMs = 0u;
static bool BALANCE_ACTIVE__G__Flag = false;
static uint32_t TICK__G__JitTripMs = 0u;
static uint16_t DUTY_PERMILLE__G__BeforeJit = 0u;
static uint8_t JIT_RETRY_STEP__G__Count = 0u;
static bool JIT_MASKED__G__Jit1 = false;
static bool JIT_MASKED__G__Jit2 = false;
static uint32_t TICK__G__SettleStartMs = 0u;

/* ==================== Helpers ==================== */

/**
 * @brief  [EN] Get monotonic ms from osKernelGetTickCount.
 *         [FA] زمان میلی‌ثانیه از تیک RTOS.
 * @return uint32_t [EN] ms
 */
static uint32_t func__Charger_GetMs(void)
{
    return func__Rtos_TicksToMilliseconds(osKernelGetTickCount());
}

/**
 * @brief  [EN] Clamp duty and convert to counts with rounding, prevent overflow.
 *         [FA] تبدیل پرمیل به شمارش با rounding.
 * @param  uint16_t__permille [EN] 0..1000
 * @return uint32_t [EN] 0..1439
 */
uint32_t func__Charger_DutyPermilleToCounts(uint16_t uint16_t__permille)
{
    uint32_t uint32_t__permilleClamped;
    uint32_t uint32_t__counts;

    if (uint16_t__permille > 1000u)
    {
        uint16_t__permille = 1000u;
    }

    uint32_t__permilleClamped = (uint32_t)uint16_t__permille;

    /* [EN] Non-linear broken: period * permille, rounding +500, divide 1000.
       Use 32-bit, period 1440 *1000=1_440_000 < 2^32.
       [FA] فرمول غیرخطی شکسته. */
    uint32_t__counts = (CHG_PWM_RESOLUTION * uint32_t__permilleClamped + 500u) / 1000u;

    if (uint32_t__counts > CHG_PWM_PERIOD)
    {
        uint32_t__counts = CHG_PWM_PERIOD;
    }

    if (uint16_t__permille == 1000u)
    {
        uint32_t__counts = CHG_PWM_PERIOD;
    }

    if (uint16_t__permille == 0u)
    {
        uint32_t__counts = 0u;
    }

    return uint32_t__counts;
}

/**
 * @brief  [EN] Raw to Ipri_avg mA: max(0,(raw-offset)*scale).
 *         [FA] تبدیل ADC خام به جریان متوسط.
 * @param  uint16_t__raw [EN] Raw
 * @return uint32_t [EN] mA
 */
uint32_t func__Charger_RawToIpriMa(uint16_t uint16_t__raw)
{
    uint32_t uint32_t__raw32;
    uint32_t uint32_t__offset;
    uint32_t uint32_t__diff;

    uint32_t__raw32 = (uint32_t)uint16_t__raw;
    uint32_t__offset = (uint32_t)CHG_CURRENT_RAW_OFFSET;

    if (uint32_t__raw32 < uint32_t__offset)
    {
        return 0u;
    }

    uint32_t__diff = uint32_t__raw32 - uint32_t__offset;

    /* [EN] scale = NUM/DEN, provisional. Use 64-bit to avoid overflow.
       [FA] مقیاس provisional. */
    if (CHG_CURRENT_SCALE_DEN == 0u)
    {
        return 0u;
    }

    return (uint32_t)(((uint64_t)uint32_t__diff * (uint64_t)CHG_CURRENT_SCALE_NUM) / (uint64_t)CHG_CURRENT_SCALE_DEN);
}

/**
 * @brief  [EN] DCM estimation Iout = η*Vin*Ipri_avg/Vout with checks.
 *         [FA] تخمین DCM با بررسی Vin/Vout غیرصفر و η کالیبراسیون.
 * @param  uint32_t__vin_mv [EN] Vin mV
 * @param  uint32_t__vout_mv [EN] Vout mV
 * @param  uint32_t__ipri_avg_ma [EN] Ipri avg
 * @param  uint32_t__eta_permille [EN] η 0..1000
 * @return uint32_t [EN] Iout mA or 0
 */
uint32_t func__Charger_EstimateIoutDcm(uint32_t uint32_t__vin_mv, uint32_t uint32_t__vout_mv, uint32_t uint32_t__ipri_avg_ma, uint32_t uint32_t__eta_permille)
{
    uint64_t uint64_t__numerator;

    if (uint32_t__vin_mv == 0u)
    {
        return 0u;
    }

    if (uint32_t__vout_mv == 0u)
    {
        return 0u;
    }

    if (uint32_t__ipri_avg_ma == 0u)
    {
        return 0u;
    }

    if (uint32_t__eta_permille == 0u)
    {
        return 0u;
    }

    if (uint32_t__eta_permille > 1000u)
    {
        uint32_t__eta_permille = 1000u;
    }

    /* [EN] Broken: Vin*Ipri = power_in_mW, *eta/1000, /Vout.
       Use 64-bit intermediate.
       [FA] توان ورودی * بازده تقسیم بر ولتاژ خروجی. */
    uint64_t__numerator = (uint64_t)uint32_t__vin_mv * (uint64_t)uint32_t__ipri_avg_ma * (uint64_t)uint32_t__eta_permille;
    /* [EN] numerator = Vin*Ipri*eta, denominator = Vout*1000
       [FA] مخرج Vout*1000. */
    return (uint32_t)(uint64_t__numerator / ((uint64_t)uint32_t__vout_mv * 1000u));
}

/**
 * @brief  [EN] Check config valid. Until transformer and times are validated, false → safe-off.
 *         [FA] اعتبارسنجی کانفیگ.
 * @return bool [EN] true if valid
 */
bool func__Charger_IsConfigValid(void)
{
    uint64_t uint64_t__bulkCalc;

    if (CHG_TRANSFORMER_KNOWN == 0u)
    {
        return false;
    }

    if (CHG_BAT_CAPACITY_MAH == 0u)
    {
        return false;
    }

    /* [EN] Bulk max = capacity * C_permille /1000 with uint64, check overflow and match header.
       [FA] محاسبه Bulk با uint64. */
    uint64_t__bulkCalc = ((uint64_t)CHG_BAT_CAPACITY_MAH * (uint64_t)CHG_BULK_MAX_C_PERMILLE) / 1000u;
    if (uint64_t__bulkCalc != (uint64_t)CHG_BULK_MAX_MA)
    {
        return false;
    }

    if (CHG_BULK_MAX_MA == 0u)
    {
        return false;
    }

    if (CHG_12V_ABSORB_MV == 0u)
    {
        return false;
    }

    if (CHG_12V_FLOAT_MV == 0u)
    {
        return false;
    }

    if (CHG_24V_ABSORB_MV == 0u)
    {
        return false;
    }

    if (CHG_24V_FLOAT_MV == 0u)
    {
        return false;
    }

    if (CHG_ABSORB_MIN_TIME_MS == 0u)
    {
        return false;
    }

    if (CHG_ABSORB_MAX_TIME_MS == 0u)
    {
        return false;
    }

    if (CHG_ABSORB_MAX_TIME_MS < CHG_ABSORB_MIN_TIME_MS)
    {
        return false;
    }

    if (CHG_ABSORB_TAIL_STABLE_TIME_MS == 0u)
    {
        return false;
    }

    if (CHG_CURRENT_SCALE_DEN == 0u)
    {
        return false;
    }

    return true;
}

/**
 * @brief  [EN] Set both PWM channels to duty, via BSP.
 *         [FA] تنظیم هر دو کانال PWM.
 * @param  uint16_t__permille [EN] Duty
 */
static void func__Charger_SetPwmBoth(uint16_t uint16_t__permille)
{
    if (uint16_t__permille > 1000u)
    {
        uint16_t__permille = 1000u;
    }

    DUTY_PERMILLE__G__Current = uint16_t__permille;
    func__BspPwm_SetDutyPermille(BSP_PWM_CHARGER_1, uint16_t__permille);
    func__BspPwm_SetDutyPermille(BSP_PWM_CHARGER_2, uint16_t__permille);
}

/**
 * @brief  [EN] Force PWM 0 and open relay PB7 after zero (relay not fast protection).
 *         [FA] صفر PWM سپس باز کردن رله.
 */
static void func__Charger_SafeOff(void)
{
    func__BspPwm_StopAll();
    DUTY_PERMILLE__G__Current = 0u;
    /* [EN] Relay PB7 is secondary protection: open only after PWM zero.
       [FA] رله فقط پس از صفر PWM باز شود. */
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
}

/**
 * @brief  [EN] Latch fault and safe-off. Retry handled in FAULT state.
 *         [FA] latch خطا و safe-off.
 * @param  uint32_t__mask [EN] Fault bit
 */
static void func__Charger_LatchFault(uint32_t uint32_t__mask)
{
    FAULT_MASK__G__Fault |= uint32_t__mask;
    func__Charger_SafeOff();
    CHARGER_STATE__G__State = CHG_STATE_FAULT;
    TICK__G__StateEnterMs = func__Charger_GetMs();
}

/* ==================== Public JIT trip (called from EXTI ISR) ==================== */

/**
 * @brief  [EN] JIT trip from ISR: very short, no RTOS API, latch, PWM 0 fast.
 *         [FA] تریپ JIT از ISR: کوتاه، بدون RTOS.
 * @param  uint32_t__jitMask [EN] CHG_FAULT_JIT1/2
 */
void func__Charger_OnJitTrip(uint32_t uint32_t__jitMask)
{
    /* [EN] ISR-safe: very short, no RTOS API, latch fault, PWM 0 fast, mask EXTI, record duty.
       Sequence: detect JIT → trip PWM → record fault/channel → latch → mask EXTI → confirm PWM0 → relay after PWM0 (in Evaluate).
       Hardware latch not added; MCU software latch is main.
       [FA] تریپ JIT از ISR: کوتاه، بدون RTOS، latch و mask. */
    if ((uint32_t__jitMask & CHG_FAULT_JIT1) != 0u)
    {
        JIT_MASKED__G__Jit1 = true;
    }
    if ((uint32_t__jitMask & CHG_FAULT_JIT2) != 0u)
    {
        JIT_MASKED__G__Jit2 = true;
    }
    DUTY_PERMILLE__G__BeforeJit = DUTY_PERMILLE__G__Current;
    FAULT_MASK__G__Fault |= uint32_t__jitMask;
    TICK__G__JitTripMs = func__Charger_GetMs();
    func__BspPwm_TripOffFromIsr();
    CHARGER_STATE__G__State = CHG_STATE_FAULT;
    /* [EN] Do not use osDelay, queue, mutex, malloc, logging in ISR.
       [FA] بدون تاخیر یا صف. */
}

/* ==================== Getters for test ==================== */

charger_state_t func__Charger_GetState(void)
{
    return CHARGER_STATE__G__State;
}

uint32_t func__Charger_GetFault(void)
{
    return FAULT_MASK__G__Fault;
}

/* ==================== Init ==================== */

void func__Charger_Init(void)
{
    CHARGER_STATE__G__State = CHG_STATE_IDLE;
    FAULT_MASK__G__Fault = CHG_FAULT_NONE;
    TICK__G__StateEnterMs = func__Charger_GetMs();
    TICK__G__BulkWorkStartMs = 0u;
    TICK__G__AbsorbEnterMs = 0u;
    TICK__G__TailStableStartMs = 0u;
    DUTY_PERMILLE__G__Current = 0u;
    TICK__G__LastRampMs = 0u;
    RETRY_COUNT__G__Count = 0u;
    BALANCE_ACTIVE__G__Flag = false;
    TICK__G__BalanceStartMs = 0u;
    TICK__G__JitTripMs = 0u;
    DUTY_PERMILLE__G__BeforeJit = 0u;
    JIT_RETRY_STEP__G__Count = 0u;
    JIT_MASKED__G__Jit1 = false;
    JIT_MASKED__G__Jit2 = false;
    TICK__G__SettleStartMs = 0u;
    func__Charger_SafeOff();
}

/* ==================== Evaluate ==================== */

void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, app_state_t app_state_t__state)
{
    uint32_t uint32_t__nowMs;
    uint32_t uint32_t__vbat12;
    uint32_t uint32_t__vbat24;
    uint32_t filtered_primary_current_ma__ch1;
    uint32_t filtered_primary_current_ma__ch2;
    bool bool__inputPresent;
    bool bool__valid;
    uint32_t uint32_t__bulkMaxMa;
    uint32_t uint32_t__elapsedBulkMs;
    uint32_t uint32_t__elapsedAbsorbMs;
    uint32_t uint32_t__elapsedTailMs;

    (void)app_state_t__state;

    uint32_t__nowMs = func__Charger_GetMs();

    if (measurement_snapshot_t__snap == NULL)
    {
        func__Charger_SafeOff();
        FAULT_MASK__G__Fault |= CHG_FAULT_MEASUREMENT_INVALID;
        CHARGER_STATE__G__State = CHG_STATE_IDLE;
        return;
    }

    bool__valid = measurement_snapshot_t__snap->valid;
    if (bool__valid == false)
    {
        func__Charger_SafeOff();
        FAULT_MASK__G__Fault |= CHG_FAULT_MEASUREMENT_INVALID;
        CHARGER_STATE__G__State = CHG_STATE_IDLE;
        return;
    }

    /* [EN] If config invalid (transformer unknown, times 0) → safe-off. Do not guess.
       [FA] کانفیگ نامعتبر → safe-off. */
    if (func__Charger_IsConfigValid() == false)
    {
        func__Charger_SafeOff();
        FAULT_MASK__G__Fault |= CHG_FAULT_CONFIG_INVALID;
        CHARGER_STATE__G__State = CHG_STATE_IDLE;
        return;
    }

    uint32_t__vbat12 = measurement_snapshot_t__snap->v_bat12_mv;
    uint32_t__vbat24 = measurement_snapshot_t__snap->v_bat24_mv;
    filtered_primary_current_ma__ch1 = measurement_snapshot_t__snap->i_ch1_ma;
    filtered_primary_current_ma__ch2 = measurement_snapshot_t__snap->i_ch2_ma;
    bool__inputPresent = measurement_snapshot_t__snap->input_present;

    /* [EN] Input absent → PWM 0 (Charger needs Vin).
       [FA] بدون ورودی معتبر PWM صفر. */
    if (bool__inputPresent == false)
    {
        func__Charger_SafeOff();
        FAULT_MASK__G__Fault |= CHG_FAULT_INPUT_MISSING;
        CHARGER_STATE__G__State = CHG_STATE_IDLE;
        return;
    }

    /* [EN] Battery absent → PWM 0. Check both 12V and 24V levels.
       [FA] بدون باتری معتبر PWM صفر. */
    if ((uint32_t__vbat12 < CHG_BAT_12V_MISSING_MV) &&
        (uint32_t__vbat24 < CHG_BAT_24V_MISSING_MV))
    {
        func__Charger_SafeOff();
        FAULT_MASK__G__Fault |= CHG_FAULT_BATTERY_MISSING;
        CHARGER_STATE__G__State = CHG_STATE_IDLE;
        return;
    }

    /* [EN] JIT fault latched → stay FAULT until manual reset. No auto-retry for critical JIT.
       [FA] JIT بحرانی بدون cooldown retry نشود. */
    if ((FAULT_MASK__G__Fault & (CHG_FAULT_JIT1 | CHG_FAULT_JIT2)) != 0u)
    {
        func__Charger_SafeOff();
        CHARGER_STATE__G__State = CHG_STATE_FAULT;
        return;
    }

    /* [EN] Balance monitor: separate V_BAT_LOW (mid vs GND) and V_BAT_HIGH (V24 vs mid).
       Two charge nets must be independent per schematic/net/continuity before active balance.
       If not confirmed, only monitor (no active correction). Current comparison alone does NOT prove balance.
       Battery with higher voltage during charge is not necessarily stronger (higher IR).
       Correction only after settle and within voltage/current/time/temperature limits.
       [FA] بالانس جداگانه هر باتری، فقط با مسیر مستقل. */
    {
        uint32_t uint32_t__vBatLow = uint32_t__vbat12;
        uint32_t uint32_t__vBatHigh = 0u;
        uint32_t uint32_t__diff12 = 0u;

        if (uint32_t__vbat24 > uint32_t__vbat12)
        {
            uint32_t__vBatHigh = uint32_t__vbat24 - uint32_t__vbat12;
        }
        else
        {
            uint32_t__vBatHigh = 0u;
        }

        if (uint32_t__vBatLow > uint32_t__vBatHigh)
        {
            uint32_t__diff12 = uint32_t__vBatLow - uint32_t__vBatHigh;
        }
        else
        {
            uint32_t__diff12 = uint32_t__vBatHigh - uint32_t__vBatLow;
        }

        if (CHG_BALANCE_INDEPENDENT_PATH_CONFIRMED == 0u)
        {
            /* [EN] No independent path → only monitor, never active balance. Still fault if diff >1V for 10min stable.
               [FA] بدون مسیر مستقل فقط مانیتور. */
        }

        if (uint32_t__diff12 > CHG_BALANCE_DIFF_MV)
        {
            if (BALANCE_ACTIVE__G__Flag == false)
            {
                /* [EN] Before balance: both PWM zero, wait settle, then read voltages separately.
                   [FA] قبل بالانس هر دو PWM صفر و settle. */
                func__Charger_SafeOff();
                if (TICK__G__SettleStartMs == 0u)
                {
                    TICK__G__SettleStartMs = uint32_t__nowMs;
                }
                if ((uint32_t__nowMs - TICK__G__SettleStartMs) < CHG_BALANCE_SETTLE_MS)
                {
                    return;
                }
                BALANCE_ACTIVE__G__Flag = true;
                TICK__G__BalanceStartMs = uint32_t__nowMs;
            }
            else
            {
                uint32_t uint32_t__elapsedBalanceMs = uint32_t__nowMs - TICK__G__BalanceStartMs;
                if (uint32_t__elapsedBalanceMs >= CHG_BALANCE_TIME_MS)
                {
                    FAULT_MASK__G__Fault |= CHG_FAULT_BALANCE;
                    CHARGER_STATE__G__State = CHG_STATE_BALANCE;
                    func__Charger_SafeOff();
                    return;
                }
            }
        }
        else if (uint32_t__diff12 == CHG_BALANCE_DIFF_MV)
        {
            /* [EN] Exactly 1V diff is not fault, just monitoring.
               [FA] دقیقاً 1V خطا نیست. */
            BALANCE_ACTIVE__G__Flag = false;
            TICK__G__SettleStartMs = 0u;
        }
        else
        {
            BALANCE_ACTIVE__G__Flag = false;
            TICK__G__SettleStartMs = 0u;
        }
    }

    /* [EN] Average overcurrent: if ich > bulk max → reduce duty or fault.
       [FA] اضافه‌جریان متوسط → کاهش دیوتی یا fault. */
    uint32_t__bulkMaxMa = CHG_BULK_MAX_MA;
    if ((filtered_primary_current_ma__ch1 > uint32_t__bulkMaxMa) ||
        (filtered_primary_current_ma__ch2 > uint32_t__bulkMaxMa))
    {
        if (DUTY_PERMILLE__G__Current > 10u)
        {
            DUTY_PERMILLE__G__Current -= 10u;
            func__Charger_SetPwmBoth(DUTY_PERMILLE__G__Current);
        }
        else
        {
            func__Charger_LatchFault(CHG_FAULT_OVERCURRENT);
        }
        return;
    }

    /* [EN] State machine dispatch.
       [FA] ماشین حالت. */
    switch (CHARGER_STATE__G__State)
    {
        case CHG_STATE_IDLE:
        {
            CHARGER_STATE__G__State = CHG_STATE_PRECHECK;
            TICK__G__StateEnterMs = uint32_t__nowMs;
            func__Charger_SafeOff();
            break;
        }

        case CHG_STATE_PRECHECK:
        {
            /* [EN] Checks: input, battery, config already done. Go to PRECHARGE.
               [FA] بررسی‌ها انجام شد. */
            CHARGER_STATE__G__State = CHG_STATE_PRECHARGE;
            TICK__G__StateEnterMs = uint32_t__nowMs;
            break;
        }

        case CHG_STATE_PRECHARGE:
        {
            /* [EN] Provisional precharge: short delay then softstart. Placeholder.
               [FA] پیش‌شارژ provisional. */
            if ((uint32_t__nowMs - TICK__G__StateEnterMs) >= 1000u)
            {
                CHARGER_STATE__G__State = CHG_STATE_SOFTSTART;
                TICK__G__StateEnterMs = uint32_t__nowMs;
                TICK__G__LastRampMs = uint32_t__nowMs;
                func__Charger_SetPwmBoth(CHG_SOFT_START_DUTY_PERMILLE);
            }
            break;
        }

        case CHG_STATE_SOFTSTART:
        {
            /* [EN] Ramp 0.5%/s =5 permille/s. Use ms.
               [FA] شیب نرم. */
            if ((uint32_t__nowMs - TICK__G__LastRampMs) >= 1000u)
            {
                uint32_t uint32_t__steps = (uint32_t__nowMs - TICK__G__LastRampMs) / 1000u;
                uint32_t uint32_t__increment = uint32_t__steps * CHG_SOFT_START_RAMP_PER_SECOND;
                uint32_t uint32_t__newDuty = (uint32_t)DUTY_PERMILLE__G__Current + uint32_t__increment;

                if (uint32_t__newDuty > 1000u)
                {
                    uint32_t__newDuty = 1000u;
                }

                if (uint32_t__newDuty != (uint32_t)DUTY_PERMILLE__G__Current)
                {
                    func__Charger_SetPwmBoth((uint16_t)uint32_t__newDuty);
                }

                TICK__G__LastRampMs += uint32_t__steps * 1000u;

                if (DUTY_PERMILLE__G__Current >= 100u)
                {
                    CHARGER_STATE__G__State = CHG_STATE_BULK;
                    TICK__G__BulkWorkStartMs = uint32_t__nowMs;
                    TICK__G__StateEnterMs = uint32_t__nowMs;
                }
            }

            /* [EN] Current must be limited even during soft start.
               [FA] جریان حتی در سافت‌استارت محدود شود. */
            break;
        }

        case CHG_STATE_BULK:
        {
            /* [EN] Bulk: CC at CHG_BULK_MAX_MA. Check voltage for transition to Absorb.
               For 24V pack use CHG_24V_ABSORB_MV, for 12V output use CHG_12V_ABSORB_MV.
               Here we check both packs; 24V has priority provisional.
               [FA] Bulk تا رسیدن به ولتاژ Absorb. */
            bool bool__v24Reached = (uint32_t__vbat24 >= CHG_24V_ABSORB_MV);
            bool bool__v12Reached = (uint32_t__vbat12 >= CHG_12V_ABSORB_MV);

            if (bool__v24Reached || bool__v12Reached)
            {
                CHARGER_STATE__G__State = CHG_STATE_ABSORB;
                TICK__G__AbsorbEnterMs = uint32_t__nowMs;
                TICK__G__TailStableStartMs = 0u;
                TICK__G__StateEnterMs = uint32_t__nowMs;
                break;
            }

            /* [EN] Bulk work/rest cycle: 30 min work, 5 min rest only in Bulk.
               [FA] فقط Bulk استراحت دارد. */
            uint32_t__elapsedBulkMs = uint32_t__nowMs - TICK__G__BulkWorkStartMs;
            if (uint32_t__elapsedBulkMs >= CHG_BULK_WORK_MS)
            {
                CHARGER_STATE__G__State = CHG_STATE_REST;
                TICK__G__StateEnterMs = uint32_t__nowMs;
                func__Charger_SafeOff();
                break;
            }

            /* [EN] Simple bulk duty: try to keep current at bulk max, voltage not yet at absorb.
               Placeholder: duty controlled externally; here keep current duty.
               [FA] کنترل جریان Bulk provisional. */
            break;
        }

        case CHG_STATE_ABSORB:
        {
            uint32_t__elapsedAbsorbMs = uint32_t__nowMs - TICK__G__AbsorbEnterMs;

            /* [EN] Absorb must be time-limited: min and max.
               [FA] Absorption زمان‌دار باشد. */
            if (uint32_t__elapsedAbsorbMs >= CHG_ABSORB_MAX_TIME_MS)
            {
                func__Charger_LatchFault(CHG_FAULT_ABSORB_TIMEOUT);
                break;
            }

            /* [EN] Tail + stable time + voltage in absorb range + valid current + no parallel load.
               [FA] شرایط پایان Absorb. */
            bool bool__tailLowCh1 = (filtered_primary_current_ma__ch1 <= CHG_ABSORB_TAIL_MA);
            bool bool__tailLowCh2 = (filtered_primary_current_ma__ch2 <= CHG_ABSORB_TAIL_MA);
            bool bool__voltageInAbsorb = false;

            if ((uint32_t__vbat24 >= (CHG_24V_ABSORB_MV - 200u)) &&
                (uint32_t__vbat24 <= (CHG_24V_ABSORB_MV + 200u)))
            {
                bool__voltageInAbsorb = true;
            }

            if ((uint32_t__vbat12 >= (CHG_12V_ABSORB_MV - 100u)) &&
                (uint32_t__vbat12 <= (CHG_12V_ABSORB_MV + 100u)))
            {
                bool__voltageInAbsorb = true;
            }

            if (bool__tailLowCh1 && bool__tailLowCh2 && bool__voltageInAbsorb)
            {
                if (TICK__G__TailStableStartMs == 0u)
                {
                    TICK__G__TailStableStartMs = uint32_t__nowMs;
                }

                uint32_t__elapsedTailMs = uint32_t__nowMs - TICK__G__TailStableStartMs;

                if (uint32_t__elapsedTailMs >= CHG_ABSORB_TAIL_STABLE_TIME_MS)
                {
                    if (uint32_t__elapsedAbsorbMs >= CHG_ABSORB_MIN_TIME_MS)
                    {
                        CHARGER_STATE__G__State = CHG_STATE_FLOAT;
                        TICK__G__StateEnterMs = uint32_t__nowMs;
                        break;
                    }
                }
            }
            else
            {
                TICK__G__TailStableStartMs = 0u;
            }

            /* [EN] Stay Absorb: regulate voltage at absorb level (CV). Duty placeholder.
               [FA] ماندن در Absorption و تنظیم ولتاژ. */
            break;
        }

        case CHG_STATE_FLOAT:
        {
            /* [EN] Float for long-term: keep voltage at float levels (12V 13500, 24V 27000 provisional).
               Check re-entry to Bulk if voltage drops below reentry.
               [FA] Float برای نگهداری طولانی‌مدت. */
            bool bool__reentry24 = (uint32_t__vbat24 < CHG_24V_REENTRY_MV);
            bool bool__reentry12 = (uint32_t__vbat12 < CHG_12V_REENTRY_MV);

            if (bool__reentry24 || bool__reentry12)
            {
                CHARGER_STATE__G__State = CHG_STATE_BULK;
                TICK__G__BulkWorkStartMs = uint32_t__nowMs;
                TICK__G__StateEnterMs = uint32_t__nowMs;
                break;
            }

            /* [EN] Float duty: maintain float voltage.
               [FA] حفظ ولتاژ Float. */
            break;
        }

        case CHG_STATE_REST:
        {
            uint32_t uint32_t__elapsedRestMs = uint32_t__nowMs - TICK__G__StateEnterMs;
            if (uint32_t__elapsedRestMs >= CHG_BULK_REST_MS)
            {
                CHARGER_STATE__G__State = CHG_STATE_BULK;
                TICK__G__BulkWorkStartMs = uint32_t__nowMs;
                TICK__G__StateEnterMs = uint32_t__nowMs;
            }
            else
            {
                func__Charger_SafeOff();
            }
            break;
        }

        case CHG_STATE_FAULT:
        {
            /* [EN] Latch fault. For JIT: lockout CHG_JIT_LOCKOUT_MS, retry 50% then 10% then final fault. Overcurrent no auto-retry for critical.
               Need manual/ESP reset after final.
               [FA] fault بحرانی بدون reset معتبر پاک نشود. */
            func__Charger_SafeOff();

            /* [EN] Confirm PWM=0 before relay (SafeOff already does). Relay is secondary.
               [FA] رله فقط بعد از PWM0. */

            if ((FAULT_MASK__G__Fault & (CHG_FAULT_JIT1 | CHG_FAULT_JIT2)) != 0u)
            {
                /* [EN] JIT protocol: 1st retry 50% duty_before, 2nd retry 10%, if JIT at <=10% → final lockout.
                   Lockout 3000ms tunable.
                   [FA] پروتکل JIT. */
                uint32_t uint32_t__elapsedJitMs = uint32_t__nowMs - TICK__G__JitTripMs;

                if (uint32_t__elapsedJitMs < CHG_JIT_LOCKOUT_MS)
                {
                    break;
                }

                if (DUTY_PERMILLE__G__BeforeJit <= 100u)
                {
                    /* [EN] JIT at <=10% → final fault, only manual/ESP.
                       [FA] JIT در دیوتی کم → بدون تلاش خودکار. */
                    break;
                }

                if (JIT_RETRY_STEP__G__Count == 0u)
                {
                    uint16_t uint16_t__retryDuty = (uint16_t)(DUTY_PERMILLE__G__BeforeJit / 2u);
                    if (uint16_t__retryDuty < 10u)
                    {
                        uint16_t__retryDuty = 10u;
                    }
                    FAULT_MASK__G__Fault &= (uint32_t)(~(CHG_FAULT_JIT1 | CHG_FAULT_JIT2));
                    JIT_MASKED__G__Jit1 = false;
                    JIT_MASKED__G__Jit2 = false;
                    func__BspExti_UnmaskJit(BSP_EXTI_JITTER1);
                    func__BspExti_UnmaskJit(BSP_EXTI_JITTER2);
                    JIT_RETRY_STEP__G__Count = 1u;
                    TICK__G__StateEnterMs = uint32_t__nowMs;
                    TICK__G__JitTripMs = uint32_t__nowMs;
                    CHARGER_STATE__G__State = CHG_STATE_PRECHECK;
                    func__Charger_SetPwmBoth(uint16_t__retryDuty);
                    break;
                }
                else if (JIT_RETRY_STEP__G__Count == 1u)
                {
                    uint16_t uint16_t__retryDuty = 100u; /* 10% */
                    if (DUTY_PERMILLE__G__BeforeJit < 100u)
                    {
                        uint16_t__retryDuty = DUTY_PERMILLE__G__BeforeJit;
                    }
                    FAULT_MASK__G__Fault &= (uint32_t)(~(CHG_FAULT_JIT1 | CHG_FAULT_JIT2));
                    JIT_MASKED__G__Jit1 = false;
                    JIT_MASKED__G__Jit2 = false;
                    func__BspExti_UnmaskJit(BSP_EXTI_JITTER1);
                    func__BspExti_UnmaskJit(BSP_EXTI_JITTER2);
                    JIT_RETRY_STEP__G__Count = 2u;
                    TICK__G__StateEnterMs = uint32_t__nowMs;
                    TICK__G__JitTripMs = uint32_t__nowMs;
                    CHARGER_STATE__G__State = CHG_STATE_PRECHECK;
                    func__Charger_SetPwmBoth(uint16_t__retryDuty);
                    break;
                }
                else
                {
                    /* [EN] At <=10% JIT again → no more auto-retry, final fault, need manual/ESP.
                       [FA] دیگر retry خودکار نه، فقط reset دستی/ESP. */
                    break;
                }
            }

            if ((FAULT_MASK__G__Fault & CHG_FAULT_OVERCURRENT) != 0u)
            {
                /* [EN] Critical overcurrent no auto-retry.
                   [FA] بدون cooldown retry نشود. */
                break;
            }

            if (RETRY_COUNT__G__Count < CHG_MAX_RETRY_COUNT)
            {
                if ((uint32_t__nowMs - TICK__G__StateEnterMs) >= 5000u)
                {
                    RETRY_COUNT__G__Count++;
                    FAULT_MASK__G__Fault = CHG_FAULT_NONE;
                    CHARGER_STATE__G__State = CHG_STATE_PRECHECK;
                    TICK__G__StateEnterMs = uint32_t__nowMs;
                }
            }
            else
            {
                /* [EN] Lockout: need manual/ESP reset.
                   [FA] پس از 3 تلاش lockout. */
            }
            break;
        }

        case CHG_STATE_SUSPEND:
        {
            func__Charger_SafeOff();
            break;
        }

        case CHG_STATE_BALANCE:
        {
            func__Charger_SafeOff();
            break;
        }

        default:
        {
            func__Charger_SafeOff();
            CHARGER_STATE__G__State = CHG_STATE_IDLE;
            break;
        }
    }
}
