/**
 * @file    charger.c
 * @brief   [EN] Generic per-channel 12 V charger control. CHG_MASTER_ENABLE is
 *          the single master switch; when it is 0 the module stays in
 *          safe-idle with all PWM stopped, NC relay closed, and JIT/relay
 *          policy inactive. CHG_TRANSFORMER_KNOWN is not bypassable; when it
 *          is 0 only the explicit limited bring-up test mode is allowed.
 *          The two channels share implementation only; their voltage,
 *          current, duty, state, retry counter and JIT sequence are separate.
 *          [FA] کنترل عمومی شارژرهای مستقل ۱۲ ولت. CHG_MASTER_ENABLE تنها
 *          کلید اصلی است؛ با مقدار ۰ ماژول در safe-idle با همه PWM متوقف،
 *          رله NC بسته و سیاست JIT/رله غیرفعال باقی می‌ماند. CHG_TRANSFORMER_KNOWN
 *          bypass نمی‌شود؛ وقتی صفر است فقط حالت صریح و محدود bring-up مجاز است.
 *          فقط پیاده‌سازی مشترک است؛ ولتاژ، جریان، duty، state، retry و توالی
 *          JIT هر کانال جداست.
 */

/* ==================== Includes / شامل‌ها ==================== */
#include "charger.h"
#include "app_config.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "cmsis_os2.h"
#include "modules_enable.h"
#include "rtos_time.h"

#include <stddef.h>

#if MODULE_JITTER
#include "jitter.h"
#endif

#if MODULE_FAULT
#include "fault.h"
#endif

/* ==================== Local types / نوع‌های داخلی ==================== */

typedef enum
{
    CHG_STATE_OFF = 0,
    CHG_STATE_BULK,
    CHG_STATE_ABSORB,
    CHG_STATE_FLOAT,
    CHG_STATE_BRINGUP,
    CHG_STATE_JIT_RETRY_WAIT,
    CHG_STATE_INPUT_WAIT,
    CHG_STATE_FINAL_FAULT,
    CHG_STATE_BAT_LOST   /* [EN] battery wire cut during charge (fault bit latched) / باتری حین شارژ قطع شده */
} charger_state_t;

typedef struct
{
    bool bool__installed;
    uint16_t uint16_t__dutyPermille;
    uint16_t uint16_t__dutyBeforeTripPermille;
    uint8_t uint8_t__jitTripCount;
    charger_state_t charger_state_t__state;
    uint32_t uint32_t__absorbAccumTicks; /* [EN] accumulated time inside the 14.4..14.5 V window, ticks / زمان جمع‌شده داخل پنجره ۱۴٫۴ تا ۱۴٫۵ ولت */
    uint32_t uint32_t__absorbLastTick;   /* [EN] previous-pass tick while in ABSORB, for the accumulation delta / تیک پاس قبلی در ابزورب برای دلتای جمع */
    uint32_t uint32_t__retryDeadlineTick;
    uint32_t uint32_t__lastDutyStepTick;
    uint32_t uint32_t__currentEmaMa;
    bool bool__currentEmaSeeded;
} charger_channel_state_t;

/* ==================== Static state / وضعیت داخلی ==================== */

static charger_channel_state_t CHARGER_CHANNEL_T__G__State[2];
static bool BOOL__G__ChargerInitialized;
static bool BOOL__G__RelayOpen;
static uint32_t UINT32_T__G__RelaySettleDeadline;

#define CHG_NO_CHANNEL 0xFFu
static uint8_t UINT8_T__G__RetryChannel;

/* ==================== Forward declarations / اعلان پیش‌موضع ==================== */
static bsp_pwm_channel_t func__Charger_PwmChannel(uint8_t uint8_t__channelIndex);

/* ==================== Safe hardware policy / سیاست سخت‌افزاری امن ==================== */

/**
 * @brief  [EN] Keep every PWM output stopped, clear all per-channel duty, and
 *              keep the NC relay closed (coil off). This is the safe-idle
 *              state for master-disabled, low-input, transformer-unknown (no
 *              bring-up enabled) and non-final non-charging conditions. With
 *              CHG_MASTER_ENABLE=0 JIT/relay disconnect policy is inactive,
 *              so the NC contact is closed and no coil is energized.
 *         [FA] همه خروجی‌های PWM را متوقف می‌کند، duty هر کانال را صفر و رله
 *              NC را بسته (coil خاموش) نگه می‌دارد. این وضعیت safe-idle برای
 *              master غیرفعال، ورودی کم، ترانس ناشناخته (بدون bring-up فعال)
 *              و شرایط غیرنهایی بدون شارژ است. با CHG_MASTER_ENABLE=0 سیاست
 *              JIT/رله قطع‌کننده غیرفعال است، بنابراین کنتاکت NC بسته و کویل
 *              بدون انرژی است.
 */
static void func__Charger_SafeIdle(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyBeforeTripPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
        /* [EN] FINAL_FAULT and BAT_LOST survive SafeIdle so their latches and
           recovery logic are not wiped by fault-idle passes.
           [FA] حالت‌های قفل (خطای نهایی و قطع باتری) با SafeIdle پاک نمی‌شوند. */
        if ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_FINAL_FAULT) &&
            (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_BAT_LOST))
        {
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                CHG_STATE_OFF;
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbAccumTicks = 0u;
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbLastTick = 0u;
        }
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/**
 * @brief  [EN] Final fault disconnect: after the third same-channel JIT, both
 *              PWM outputs are already stopped; then the NC relay opens so
 *              the transformer input is physically removed.
 *         [FA] قطع نهایی fault: بعد از سومین JIT همان کانال، هر دو PWM قبلاً
 *              متوقف شده‌اند؛ سپس رله NC باز می‌شود تا ورودی ترانس واقعاً قطع
 *              شود.
 */
static void func__Charger_FinalDisconnect(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, true);
    BOOL__G__RelayOpen = true;
    UINT32_T__G__RelaySettleDeadline = 0u;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/**
 * @brief  [EN] Final-fault standby while the input is cut: keep the fault
 *              latched (BOOL__G__RelayOpen stays true) but release the relay
 *              coil. With the transformer input physically absent, energizing
 *              the NC relay disconnects nothing and only drains the battery
 *              (coil ~40 mA, plus ~7 mA board base = the observed ~50 mA).
 *              The coil re-energizes automatically through FinalDisconnect()
 *              the moment the input returns and the latch is still set.
 *         [FA] standby خطای نهایی وقتی ورودی قطع است: قفل خطا باقی می‌ماند
 *              ولی کویل رله رها می‌شود تا باتری را خالی نکند (~۵۰mA). با
 *              برگشت ورودی، کویل دوباره به‌صورت خودکار وصل می‌شود.
 */
static void func__Charger_FinalDisconnectIdle(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    UINT32_T__G__RelaySettleDeadline = 0u;
    /* [EN] BOOL__G__RelayOpen intentionally NOT cleared: the final fault
       stays latched. / [FA] قفل خطا دست‌نخورده می‌ماند. */

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/* ==================== Channel helpers / توابع کمکی کانال ==================== */

static bool func__Charger_IsChannelInstalled(uint8_t uint8_t__channelIndex)
{
    uint32_t uint32_t__channelMask;

    if (uint8_t__channelIndex >= 2u)
    {
        return false;
    }

    uint32_t__channelMask = (1u << uint8_t__channelIndex);
    return ((CHG_INSTALLED_CHANNEL_MASK & uint32_t__channelMask) != 0u);
}

static bsp_pwm_channel_t func__Charger_PwmChannel(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return BSP_PWM_CHARGER_1;
    }

    return BSP_PWM_CHARGER_2;
}

static uint32_t func__Charger_ChannelVoltageMv(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                                uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return measurement_snapshot_t__snap->v_bat_high_mv;
    }

    return measurement_snapshot_t__snap->v_bat_low_mv;
}

static uint32_t func__Charger_ChannelCurrentMa(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                               uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return measurement_snapshot_t__snap->i_ch1_ma;
    }

    return measurement_snapshot_t__snap->i_ch2_ma;
}

/* ==================== Charger_OutputEstimateMa / تخمین جریان خروجی ==================== */

/**
 * @brief  [EN] Convert the primary-side shunt current to the estimated output
 *              (battery) current used by the charge decisions:
 *              Iout = Ipri_avg * Vin * eta / Vbat. Vbat is clamped so a bad
 *              momentary reading cannot divide by ~0. Used ONLY on the normal
 *              charge path; the bring-up source-limit path keeps primary mA.
 *         [FA] تبدیل جریان شنتِ اولیه به جریان خروجی تخمینی برای تصمیم‌های
 *              شارژ. فقط مسیر نرمال، نه مسیر برینگ‌آپ.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 * @param  uint8_t__channelIndex [EN] Channel / کانال
 * @param  uint32_t__primaryMa [EN] Measured primary current / جریان اولیه
 * @return uint32_t [EN] Estimated output current in mA / جریان خروجی تخمینی mA
 */
static uint32_t func__Charger_OutputEstimateMa(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                               uint8_t uint8_t__channelIndex,
                                               uint32_t uint32_t__primaryMa)
{
    uint32_t uint32_t__vbatMv;
    uint64_t uint64_t__numerator;

    if ((uint32_t__primaryMa == 0u) ||
        (measurement_snapshot_t__snap->v_in_mv == 0u))
    {
        return 0u;
    }

    uint32_t__vbatMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    if (uint32_t__vbatMv < CHG_OUTPUT_EST_MIN_VBAT_MV)
    {
        uint32_t__vbatMv = CHG_OUTPUT_EST_MIN_VBAT_MV;
    }

    uint64_t__numerator = (uint64_t)uint32_t__primaryMa *
                          (uint64_t)measurement_snapshot_t__snap->v_in_mv *
                          (uint64_t)CHG_FLYBACK_EFFICIENCY_PERMILLE;

    return (uint32_t)(uint64_t__numerator /
                      ((uint64_t)uint32_t__vbatMv * 1000u));
}

static uint16_t func__Charger_MaxDutyPermille(void)
{
    if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
    {
        return CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE;
    }

    return CHG_DUTY_MAX_PERMILLE;
}

static uint32_t func__Charger_ActiveCurrentLimitMa(void)
{
    if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
    {
        return CHG_BRINGUP_TEST_SOURCE_LIMIT_MA;
    }

    return CHG_CURRENT_LIMIT_MA;
}

static bool func__Charger_BatteryVoltageIsValid(uint32_t uint32_t__batteryMv)
{
    return ((uint32_t__batteryMv >= CHG_MIN_VALID_BATTERY_MV) &&
            (uint32_t__batteryMv <= CHG_MAX_VALID_BATTERY_MV));
}

static void func__Charger_ApplyDuty(uint8_t uint8_t__channelIndex,
                                    uint16_t uint16_t__dutyPermille)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint8_t uint8_t__otherIndex;
    uint16_t uint16_t__maxDuty;
    uint16_t uint16_t__clampedDuty;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->bool__installed == false)
    {
        charger_channel_state_t__channel->uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        return;
    }

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    uint16_t__clampedDuty = uint16_t__dutyPermille;
    if (uint16_t__clampedDuty > uint16_t__maxDuty)
    {
        uint16_t__clampedDuty = uint16_t__maxDuty;
    }

    charger_channel_state_t__channel->uint16_t__dutyPermille = uint16_t__clampedDuty;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex),
                                  uint16_t__clampedDuty);

    uint8_t__otherIndex = (uint8_t__channelIndex == 0u) ? 1u : 0u;
    if (CHARGER_CHANNEL_T__G__State[uint8_t__otherIndex].bool__installed == false)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__otherIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__otherIndex), 0u);
    }
}

static void func__Charger_StopOneChannel(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
}

/* ==================== Time helpers ==================== */

static uint32_t func__Charger_DurationTicks(uint32_t uint32_t__milliseconds)
{
    uint32_t uint32_t__ticks;

    uint32_t__ticks = func__Rtos_MillisecondsToTicks(uint32_t__milliseconds);
    if (uint32_t__ticks == 0u)
    {
        uint32_t__ticks = 1u;
    }

    return uint32_t__ticks;
}

static bool func__Charger_DeadlineElapsed(uint32_t uint32_t__nowTick,
                                          uint32_t uint32_t__deadlineTick)
{
    return ((int32_t)(uint32_t__nowTick - uint32_t__deadlineTick) >= 0);
}

/* ==================== Protection helpers ==================== */

static void func__Charger_ResetChannelToOff(uint8_t uint8_t__channelIndex)
{
    charger_channel_state_t *charger_channel_state_t__channel;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_OFF;
    charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
    charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = 0u;
    charger_channel_state_t__channel->uint32_t__currentEmaMa = 0u;
    charger_channel_state_t__channel->bool__currentEmaSeeded = false;
    charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille =
        charger_channel_state_t__channel->uint16_t__dutyPermille;
}

static void func__Charger_LatchFinalFault(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
            CHG_STATE_FINAL_FAULT;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 3u;
    }

    func__Charger_FinalDisconnect();
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

#if MODULE_JITTER

static void func__Charger_HandleJitTrip(uint8_t uint8_t__channelIndex,
                                        uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__dutyBeforeTripPermille;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FINAL_FAULT)
    {
        return;
    }

    charger_channel_state_t__channel->uint8_t__jitTripCount++;

    if (charger_channel_state_t__channel->uint8_t__jitTripCount >= 3u)
    {
        func__Charger_LatchFinalFault(uint8_t__channelIndex);
        return;
    }

    /* Capture before StopOneChannel(), which deliberately clears the live duty. */
    uint16_t__dutyBeforeTripPermille =
        charger_channel_state_t__channel->uint16_t__dutyPermille;
    func__Charger_StopOneChannel(uint8_t__channelIndex);
    charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille =
        uint16_t__dutyBeforeTripPermille;
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_JIT_RETRY_WAIT;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick =
        uint32_t__nowTick + func__Charger_DurationTicks(CHG_JIT_LOCKOUT_MS);
    UINT8_T__G__RetryChannel = uint8_t__channelIndex;
}

#endif

static uint16_t func__Charger_RetryDuty(uint8_t uint8_t__channelIndex)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;
    uint16_t uint16_t__maxDuty;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    uint16_t__retryDuty =
        (uint16_t)(charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille / 2u);

    if ((charger_channel_state_t__channel->uint8_t__jitTripCount > 1u) &&
        (uint16_t__retryDuty > CHG_DUTY_RETRY_SECOND_MAX))
    {
        uint16_t__retryDuty = CHG_DUTY_RETRY_SECOND_MAX;
    }

    if ((uint16_t__retryDuty == 0u) &&
        (charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille > 0u))
    {
        uint16_t__retryDuty = 1u;
    }

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    if (uint16_t__retryDuty > uint16_t__maxDuty)
    {
        uint16_t__retryDuty = uint16_t__maxDuty;
    }

    return uint16_t__retryDuty;
}

static void func__Charger_ServiceRetry(uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;
    uint8_t uint8_t__retryChannel;

    uint8_t__retryChannel = UINT8_T__G__RetryChannel;

    if (uint8_t__retryChannel == CHG_NO_CHANNEL)
    {
        return;
    }

    if (uint8_t__retryChannel >= 2u)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__retryChannel];

    if (charger_channel_state_t__channel->charger_state_t__state != CHG_STATE_JIT_RETRY_WAIT)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return;
    }

    func__Charger_StopOneChannel(uint8_t__retryChannel);

    if (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                      charger_channel_state_t__channel->uint32_t__retryDeadlineTick) == false)
    {
        return;
    }

    uint16_t__retryDuty = func__Charger_RetryDuty(uint8_t__retryChannel);
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
    charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
    charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;

#if MODULE_JITTER
    func__Jitter_ClearChannel((uint8_t)(uint8_t__retryChannel + 1u));
#endif

    func__Charger_ApplyDuty(uint8_t__retryChannel, uint16_t__retryDuty);
    /* [EN] Stamp the step timer so the resumed ramp waits a full up-interval
       before growing again (soft resume after a JIT trip).
       [FA] بعد از ری‌استارت JIT هم رمپ باید یک بازهٔ کامل صبر کند. */
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

/* ==================== Bring-up regulation ==================== */

static void func__Charger_BringupRegulateChannel(uint8_t uint8_t__channelIndex,
                                                 const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint32_t uint32_t__currentMa;
    uint32_t uint32_t__batteryMv;
    uint16_t uint16_t__nextDuty;
    uint16_t uint16_t__maxDuty;
    uint32_t uint32_t__increased;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    if (charger_channel_state_t__channel->charger_state_t__state != CHG_STATE_BRINGUP)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BRINGUP;
        charger_channel_state_t__channel->uint16_t__dutyPermille = 0u;
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    if (func__Charger_BatteryVoltageIsValid(uint32_t__batteryMv) == false)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    if (uint32_t__currentMa > func__Charger_ActiveCurrentLimitMa())
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    uint16_t__nextDuty = charger_channel_state_t__channel->uint16_t__dutyPermille;

    if (uint32_t__currentMa < func__Charger_ActiveCurrentLimitMa())
    {
        if (uint16_t__nextDuty < uint16_t__maxDuty)
        {
            uint32_t__increased = (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_PERMILLE;
            if (uint32_t__increased > (uint32_t)uint16_t__maxDuty)
            {
                uint32_t__increased = (uint32_t)uint16_t__maxDuty;
            }
            uint16_t__nextDuty = (uint16_t)uint32_t__increased;
        }
    }
    else if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
    {
        uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
    }
    else
    {
        uint16_t__nextDuty = 0u;
    }

    func__Charger_ApplyDuty(uint8_t__channelIndex, uint16_t__nextDuty);
}

/* ==================== Regulation ==================== */

static void func__Charger_RegulateChannel(uint8_t uint8_t__channelIndex,
                                          const measurement_snapshot_t *measurement_snapshot_t__snap,
                                          uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint32_t uint32_t__batteryMv;
    uint32_t uint32_t__currentMa;
    uint32_t uint32_t__targetMv;
    uint32_t uint32_t__absorbTicks;
    uint16_t uint16_t__nextDuty;
    uint32_t uint32_t__increasedDuty;
    uint32_t uint32_t__upIntervalTicks;
    uint32_t uint32_t__downIntervalTicks;
    int32_t int32_t__currentDelta;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FINAL_FAULT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_JIT_RETRY_WAIT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_INPUT_WAIT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BRINGUP)
    {
        return;
    }

    /* [EN] Battery-lost: the central Fault module owns detection and clear
       timing; Charger_Evaluate mirrors the bit into this state and back to
       OFF. As long as the state is BAT_LOST, never regulate.
       [FA] مالکیت تشخیص و پاک‌سازی قطع باتری با ماژول Fault است؛ تا وقتی
       وضعیت BAT_LOST است هیچ تنظیمی انجام نشود. */
    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BAT_LOST)
    {
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    /* [EN] The snapshot current is primary-side; Bulk/Absorb/Float limits are
       output (battery) currents, so this normal-charge path decides with the
       converted value. The bring-up regulator above keeps primary mA.
       [FA] جریان snapshot سمت اولیه است؛ حدهای شارژ خروجی‌اند، پس مسیر نرمال با
       مقدار تبدیل‌شده تصمیم می‌گیرد. */
    uint32_t__currentMa =
        func__Charger_OutputEstimateMa(measurement_snapshot_t__snap,
                                       uint8_t__channelIndex,
                                       uint32_t__currentMa);

    /* [EN] Battery-disconnect detection used to live here; it moved to the
       central Fault module (fault.c, FAULT_BAT_*), which latches
       FAULT_CHARGER_BAT_LOST - see the mirror block in Charger_Evaluate.
       [FA] تشخیص قطع باتری به ماژول Fault منتقل شد؛ بلوک آینه در Evaluate. */
    if (func__Charger_BatteryVoltageIsValid(uint32_t__batteryMv) == false)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    if (uint32_t__currentMa > CHG_CURRENT_HARD_FAULT_MA)
    {
        /* [EN] Only a hard over-current fault resets the channel; normal
           over-target is handled by the duty band below (no cut/restart).
           Uses the raw sample so protection speed is unchanged by the filter.
           [FA] فقط خطای سخت اضافه‌جریان کانال را ریست می‌کند (روی نمونهٔ خام،
           بدون تأخیر فیلتر). */
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    /* [EN] EMA low-pass on the estimated output current (tau ~0.64 s, one
       update per 10 ms pass). The 630..650 band decides on this smooth value
       so the duty does not hunt from sample noise; seeded with the first
       sample after any restart.
       [FA] فیلتر نمایی روی جریان تخمینی (ثابت زمانی ~۰٫۶۴ ثانیه)؛ باند تنظیم
       با مقدار صاف تصمیم می‌گیرد تا دیوتی تندتند عوض نشود. */
    if (charger_channel_state_t__channel->bool__currentEmaSeeded == false)
    {
        charger_channel_state_t__channel->uint32_t__currentEmaMa = uint32_t__currentMa;
        charger_channel_state_t__channel->bool__currentEmaSeeded = true;
    }
    else
    {
        int32_t__currentDelta =
            (int32_t)uint32_t__currentMa -
            (int32_t)charger_channel_state_t__channel->uint32_t__currentEmaMa;
        charger_channel_state_t__channel->uint32_t__currentEmaMa =
            (uint32_t)((int32_t)charger_channel_state_t__channel->uint32_t__currentEmaMa +
                       (int32_t__currentDelta >> CHG_CURRENT_EMA_SHIFT));
    }
    uint32_t__currentMa = charger_channel_state_t__channel->uint32_t__currentEmaMa;

#if (CHG_FIXED_DUTY_TEST_ENABLE != 0u)
    /* [EN] Bench diagnostic: fixed duty, no ramp/band/voltage regulation.
       Switching stops at the absorb voltage so the battery cannot be
       overcharged with regulation off; all protection cuts above stay active.
       [FA] حالت تست بنچ: دیوتی ثابت ۱۵٪؛ بالای ۱۴٫۴V سوئیچینگ متوقف؛ همهٔ
       حفاظت‌ها فعال‌اند. */
    if (uint32_t__batteryMv >= CHG_ABSORB_MV)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
        charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
    }
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
    func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_FIXED_DUTY_TEST_DUTY_PERMILLE);
    return;
#endif

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
        charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        return;
    }

    uint32_t__targetMv = CHG_ABSORB_MV;

    if ((charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT) &&
        (uint32_t__batteryMv < CHG_REENTRY_MV))
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
        charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
        uint32_t__targetMv = CHG_ABSORB_MV;
    }

    /* [EN] Absorb is a VOLTAGE-HOLD window now (user directive 2026-09-19):
       entered at 14.3 V, duty steps shrink to 0.1% so the 14.4 V setpoint
       holds steady without the old boundary hunting, the 10-minute soak
       counts only while 14.4..14.5 V, dipping below 14.3 V returns to BULK
       and RESETS the soak (his requirement until the offset case is solved),
       overshoot above 14.6 V gets coarse 0.5% down-steps to come back fast.
       [FA] ابزورب = پنجره تثبیت ولتاژ: ورود ۱۴٫۳V، پله ۰٫۱٪ برای نگه‌داشت
       ۱۴٫۴V بدون تلاطم مرز؛ شستشوی ۱۰ دقیقه فقط در ۱۴٫۴..۱۴٫۵V جمع می‌شود؛
       افت زیر ۱۴٫۳V برگشت به بالک + ریست شستشو؛ عبور از ۱۴٫۶V کاهش سریع
       ۰٫۵٪. */
    if (uint32_t__batteryMv >= CHG_ABSORB_ENTER_MV)
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BULK)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_ABSORB;
            charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
            charger_channel_state_t__channel->uint32_t__absorbLastTick = uint32_t__nowTick;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
        {
            uint32_t__targetMv = CHG_FLOAT_MV;
        }
        else
        {
            uint32_t__targetMv = CHG_ABSORB_MV;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_ABSORB)
        {
            uint32_t uint32_t__absorbDeltaTicks;

            uint32_t__absorbTicks = func__Charger_DurationTicks(CHG_ABSORB_HOLD_MS);

            /* [EN] The soak counts during the WHOLE ABSORB stay (user
               directive 2026-09-19), from the 14.3 V entry through every
               overshoot episode - no sub-window. It resets only when the
               voltage falls below 14.3 V (else-branch), so the offset-affected
               equilibrium right under 14.4 V can no longer stall the soak.
               [FA] شستشو کل مدتِ حالت ابزورب جمع می‌شود (دستور کاربر)، بدون
               پنجره‌باریک؛ فقط زیر ۱۴٫۳V ریست می‌شود تا آفست تعادلِ مرز،
               شستشو را گیر نیندازد. */
            uint32_t__absorbDeltaTicks =
                (uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__absorbLastTick);
            charger_channel_state_t__channel->uint32_t__absorbLastTick = uint32_t__nowTick;

            charger_channel_state_t__channel->uint32_t__absorbAccumTicks +=
                uint32_t__absorbDeltaTicks;
            if ((uint32_t__absorbTicks != 0u) &&
                (charger_channel_state_t__channel->uint32_t__absorbAccumTicks >
                 uint32_t__absorbTicks))
            {
                /* [EN] Clamp against long-soak overflow. / سقف برای اضافه‌سرریز. */
                charger_channel_state_t__channel->uint32_t__absorbAccumTicks =
                    uint32_t__absorbTicks;
            }

            if ((uint32_t__absorbTicks != 0u) &&
                (charger_channel_state_t__channel->uint32_t__absorbAccumTicks >=
                 uint32_t__absorbTicks))
            {
                charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_FLOAT;
                uint32_t__targetMv = CHG_FLOAT_MV;
            }
        }
    }
    else
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
        {
            /* [EN] FLOAT descending through and below the absorb window is
               the whole point of floating: hold 13.5 V, do NOT touch the
               soak bookkeeping and do NOT fall back to BULK (2026-09-19 bug
               caught on bench: the unguarded else->BULK restarted a fresh
               10-minute soak right after every soak completed, because the
               float descent crossed the window bottom).
               [FA] نزول فلوت به زیر پنجره جذب تعریف خود فلوت است: نگه‌داشت
               ۱۳٫۵V؛ نه شستشو دست می‌خورد نه به بالک برمی‌گردیم - باگ بنچ:
               else بی‌قید قبلی بلافاصله پس از اتمام شستشو به بالک پس می‌زد
               و شستشو را از صفر راه می‌انداخت. */
            uint32_t__targetMv = CHG_FLOAT_MV;
        }
        else
        {
            /* [EN] Only while in ABSORB does a dip below the 14.3 V window
               bottom mean "the voltage hold failed": back to
               current-regulated BULK and RESET the soak (user directive).
               An already-BULK channel just stays BULK with a zeroed soak.
               [FA] فقط در حالت ابزورب افت زیر ۱۴٫۳V یعنی تثبیت شکست خورد:
               برگشت به بالک و ریست شستشو (دستور کاربر). */
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
            uint32_t__targetMv = CHG_ABSORB_MV;
            charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
            charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
        }
    }

    uint16_t__nextDuty = charger_channel_state_t__channel->uint16_t__dutyPermille;
    uint32_t__upIntervalTicks = func__Charger_DurationTicks(CHG_DUTY_RAMP_UP_INTERVAL_MS);
    uint32_t__downIntervalTicks = func__Charger_DurationTicks(CHG_DUTY_RAMP_DOWN_INTERVAL_MS);

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
    {
        /* [EN] End of the charge cycle = PARK THE PUMP AT ZERO (user
           directive 2026-09-19: "why is the charger not off, why is duty
           still 4-5%"): step duty down to 0 on the coarse cadence and keep
           it parked. The battery rests at its natural voltage; only the
           <13.0 V reentry (handled above) wakes BULK again. Previously the
           low-current neutral band could freeze a few-% standby duty and
           trickle forever.
           [FA] اتمام سیکل شارژ یعنی پارک پمپ روی صفر (دستور کاربر): دیوتی
           با ضرب‌آهنگ زبری تا صفر پایین می‌آید و پارک می‌شود؛ باتری روی
           ولتاژ طبیعی خودش استراحت می‌کند و فقط reentry زیر ۱۳٫۰V به بالک
           برمی‌گرداند. قبلاً باند خنثیِ جریان کم، چند درصد دیوتی آماده‌باش
           را تا ابد فریز می‌کرد و شارژ خاموش نمی‌شد. */
        if ((uint16_t__nextDuty != 0u) &&
            ((uint32_t)(uint32_t__nowTick -
                        charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
             uint32_t__downIntervalTicks))
        {
            if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
            {
                uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
            }
            else
            {
                uint16_t__nextDuty = 0u;
            }
            charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        }
    }
    else if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_ABSORB)
    {
        uint32_t uint32_t__absorbUpIntervalTicks;
        uint32_t uint32_t__absorbDownIntervalTicks;

        /* [EN] Voltage hold at 14.4 V with fine 0.1% duty steps at HALF the
           bulk rate (user directive 2026-09-19: no back-to-back duty moves):
           up every 2000 ms, down every 1000 ms. Above 14.6 V the step grows
           to 0.5% at the normal 500 ms cadence - that path is protection.
           [FA] تثبیت ۱۴٫۴V با پلهٔ ۰٫۱٪ و نصف سرعت بالک (دستور کاربر): صعود
           هر ۲۰۰۰ms، نزول هر ۱۰۰۰ms؛ بالای ۱۴٫۶V کاهش ۰٫۵٪ با فرکانس ۵۰۰ms. */
        uint32_t__absorbUpIntervalTicks =
            func__Charger_DurationTicks(CHG_DUTY_RAMP_UP_INTERVAL_ABSORB_MS);
        uint32_t__absorbDownIntervalTicks =
            func__Charger_DurationTicks(CHG_DUTY_RAMP_DOWN_INTERVAL_ABSORB_MS);

        if (uint32_t__batteryMv > CHG_ABSORB_OVER_MV)
        {
            if ((uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
                uint32_t__downIntervalTicks)
            {
                if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
                {
                    uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
                }
                else
                {
                    uint16_t__nextDuty = 0u;
                }
                charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
            }
        }
        else if (uint32_t__batteryMv > uint32_t__targetMv)
        {
            if ((uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
                uint32_t__absorbDownIntervalTicks)
            {
                if (uint16_t__nextDuty > CHG_DUTY_STEP_FINE_PERMILLE)
                {
                    uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_FINE_PERMILLE);
                }
                else
                {
                    uint16_t__nextDuty = 0u;
                }
                charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
            }
        }
        else if (uint32_t__batteryMv < uint32_t__targetMv)
        {
            if ((uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
                uint32_t__absorbUpIntervalTicks)
            {
                uint32_t__increasedDuty =
                    (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_FINE_PERMILLE;
                if (uint32_t__increasedDuty > CHG_DUTY_MAX_PERMILLE)
                {
                    uint16_t__nextDuty = CHG_DUTY_MAX_PERMILLE;
                }
                else
                {
                    uint16_t__nextDuty = (uint16_t)uint32_t__increasedDuty;
                }
                charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
            }
        }
        else
        {
            /* [EN] Exactly on the 14.4 V setpoint: hold.
               / دقیقاً روی ۱۴٫۴V: نگه‌داشت. */
        }
    }
    else if (uint32_t__batteryMv < uint32_t__targetMv)
    {
        /* [EN] Current regulation band with rate-limited steps: above 650 mA
           step duty DOWN (one 0.5% step per 500 ms), below 630 mA step duty
           UP (one 0.5% step per 1000 ms), inside 630..650 hold. Gradual
           down-steps let the loop sit near the band with hysteresis instead
           of cutting and restarting from zero.
           [FA] باند تنظیم جریان با پله‌های محدودشدهٔ زمانی: بالای ۶۵۰ کاهش
           تدریجی (هر ۵۰۰ms)، زیر ۶۳۰ افزایش تدریجی (هر ۱ ثانیه)، داخل باند
           نگه‌داشت — بدون قطع و شروع از صفر، مثل یه هیسترزیس. */
        if (uint32_t__currentMa > CHG_BULK_CURRENT_MAX_MA)
        {
            if ((uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
                uint32_t__downIntervalTicks)
            {
                if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
                {
                    uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
                }
                else
                {
                    uint16_t__nextDuty = 0u;
                }
                charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
            }
        }
        else if (uint32_t__currentMa < CHG_REGULATE_LOW_MA)
        {
            if ((uint32_t)(uint32_t__nowTick -
                           charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
                uint32_t__upIntervalTicks)
            {
                uint32_t__increasedDuty =
                    (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_PERMILLE;
                if (uint32_t__increasedDuty > CHG_DUTY_MAX_PERMILLE)
                {
                    uint16_t__nextDuty = CHG_DUTY_MAX_PERMILLE;
                }
                else
                {
                    uint16_t__nextDuty = (uint16_t)uint32_t__increasedDuty;
                }
                charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
            }
        }
        else
        {
            /* [EN] Inside the 630..650 band: hold duty. / داخل باند: نگه‌داشت دیوتی */
        }
    }
    else if (uint32_t__batteryMv > uint32_t__targetMv)
    {
        if ((uint32_t)(uint32_t__nowTick -
                       charger_channel_state_t__channel->uint32_t__lastDutyStepTick) >=
            uint32_t__downIntervalTicks)
        {
            if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
            {
                uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
            }
            else
            {
                uint16_t__nextDuty = 0u;
            }
            charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        }
    }

    func__Charger_ApplyDuty(uint8_t__channelIndex, uint16_t__nextDuty);
}

/* ==================== Charger_Init ==================== */

void func__Charger_Init(void)
{
    uint8_t uint8_t__channelIndex;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed =
            func__Charger_IsChannelInstalled(uint8_t__channelIndex);
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyBeforeTripPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state = CHG_STATE_OFF;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbAccumTicks = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbLastTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__lastDutyStepTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__currentEmaMa = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__currentEmaSeeded = false;
    }

    BOOL__G__ChargerInitialized = true;
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;

    func__Charger_SafeIdle();
}

/* ==================== Charger_Evaluate ==================== */

void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state)
{
    uint32_t uint32_t__nowTick;
    uint8_t uint8_t__channelIndex;
    bool bool__anyFinalFault;
    bool bool__inputAdcValid;
    bool bool__controlAllowed;

    (void)app_state_t__state;

    if (BOOL__G__ChargerInitialized == false)
    {
        func__Charger_Init();
    }

    uint32_t__nowTick = osKernelGetTickCount();

#if MODULE_FAULT
    /* [EN] Battery-lost mirror (detection and clear timing live in the Fault
       module). While the bit is latched, stop PWM once per channel and hold
       the state at BAT_LOST; when Fault clears the bit, release the channel
       to OFF so the first normal pass soft-restarts BULK at 1% duty. Running
       here BEFORE the FAULT/SAFE gate keeps the mirror working while the bit
       forces app_state FAULT; SafeIdle preserves BAT_LOST states meanwhile.
       [FA] آینهٔ پرچم قطع باتری: وقتی بیت قفل است یک‌بار PWM را قطع و کانال
       را BAT_LOST نگه می‌داریم؛ با پاک‌شدن بیت کانال به OFF آزاد می‌شود تا
       رمپ نرم از ۱٪ شروع شود. این بلوک قبل از گِیت FAULT اجرا می‌شود. */
    {
        bool bool__batLostLatched;

        bool__batLostLatched =
            ((func__Fault_Get() & FAULT_CHARGER_BAT_LOST) != FAULT_NONE);

        for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
        {
            if ((bool__batLostLatched == true) &&
                (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
                 CHG_STATE_BAT_LOST))
            {
                func__Charger_StopOneChannel(uint8_t__channelIndex);
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_BAT_LOST;
            }
            else if ((bool__batLostLatched == false) &&
                     (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                      CHG_STATE_BAT_LOST))
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_OFF;
            }
            else
            {
                /* [EN] Already mirrored. [FA] هم‌اکنون هماهنگ است. */
            }
        }
    }
#endif

    if (CHG_MASTER_ENABLE == 0u)
    {
        func__Charger_SafeIdle();
        return;
    }

    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false) ||
        (CHG_INSTALLED_CHANNEL_MASK == 0u) ||
        (app_state_t__state == APP_STATE_FAULT) ||
        (app_state_t__state == APP_STATE_SAFE))
    {
        func__Charger_SafeIdle();
        return;
    }

    bool__controlAllowed = false;
    if (CHG_TRANSFORMER_KNOWN != 0u)
    {
        bool__controlAllowed = true;
    }
    else if (CHG_BRINGUP_TEST_ENABLE != 0u)
    {
        bool__controlAllowed = true;
    }

    if (bool__controlAllowed == false)
    {
        func__Charger_SafeIdle();
        return;
    }

    bool__inputAdcValid = (measurement_snapshot_t__snap->v_in_mv >= CHG_INPUT_VALID_MV);

    bool__anyFinalFault = false;
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
            CHG_STATE_FINAL_FAULT)
        {
            bool__anyFinalFault = true;
        }
    }

    if (bool__anyFinalFault == true)
    {
        /* [EN] With the input cut, keep the latch but release the coil so it
           cannot drain the battery; FinalDisconnect resumes when input returns.
           [FA] با ورودی قطع، قفل خطا بماند ولی کویل رها شود تا باتری خالی نشود. */
        if (bool__inputAdcValid == true)
        {
            func__Charger_FinalDisconnect();
        }
        else
        {
            func__Charger_FinalDisconnectIdle();
        }
        return;
    }

    if (bool__inputAdcValid == false)
    {
        for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
        {
            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
                CHG_STATE_FINAL_FAULT)
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_INPUT_WAIT;
            }
        }
        func__Charger_SafeIdle();
        return;
    }

#if MODULE_JITTER
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
            (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_FINAL_FAULT) &&
            (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
             CHG_STATE_JIT_RETRY_WAIT) &&
            (func__Jitter_ChannelTripped((uint8_t)(uint8_t__channelIndex + 1u)) == true) &&
            (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL))
        {
            func__Charger_HandleJitTrip(uint8_t__channelIndex, uint32_t__nowTick);
        }
    }
#endif

    func__Charger_ServiceRetry(uint32_t__nowTick);

    if (BOOL__G__RelayOpen == true)
    {
        if (bool__inputAdcValid == true)
        {
            func__Charger_FinalDisconnect();
        }
        else
        {
            func__Charger_FinalDisconnectIdle();
        }
        return;
    }

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true)
        {
            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                CHG_STATE_INPUT_WAIT)
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_OFF;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbAccumTicks = 0u;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbLastTick = 0u;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
            }

            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                CHG_STATE_JIT_RETRY_WAIT)
            {
                continue;
            }

            if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
            {
                func__Charger_BringupRegulateChannel(uint8_t__channelIndex,
                                                     measurement_snapshot_t__snap);
            }
            else
            {
                func__Charger_RegulateChannel(uint8_t__channelIndex,
                                              measurement_snapshot_t__snap,
                                              uint32_t__nowTick);
            }
        }
        else
        {
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
            func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        }
    }
}

/* ==================== Charger_IsAnyChannelActive ==================== */

/**
 * @brief  [EN] Report whether any installed channel is currently PUMPING
 *         charge into a battery (BULK / ABSORB only). A channel idling in
 *         OFF, JIT_RETRY_WAIT, INPUT_WAIT, FINAL_FAULT or BAT_LOST does NOT
 *         count; since 2026-09-19 a FLOAT channel does not count either -
 *         the pump is parked at zero duty there, so the charge is DONE, not
 *         active. Used by (a) the UI so the charging yellow blink stops as
 *         soon as the charger shuts off, and (b) the fault pump-window, so
 *         a transient above 14.8 V in the parked/done phase can no longer
 *         catch the battery-lost buzzer (nothing is pumping then).
 *         [FA] آیا دست‌کم یک کانال نصب‌شده واقعاً در حال پمپ‌کردن شارژ است؟
 *         فقط BULK/ABSORB؛ FLOAT پارک‌شده (دیوتی صفر) یعنی کار تمام شده و
 *         فعال حساب نمی‌شود - نه زرد باید بچشمکد نه آشکارساز قطع باتری
 *         مسلح است.
 * @return bool [EN] true if any installed channel is pumping / true اگر هر کانال نصب‌شده پمپ کند
 */
bool func__Charger_IsAnyChannelActive(void)
{
    uint8_t uint8_t__channelIndex;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
            ((CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
              CHG_STATE_BULK) ||
             (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
              CHG_STATE_ABSORB)))
        {
            return true;
        }
    }

    return false;
}
