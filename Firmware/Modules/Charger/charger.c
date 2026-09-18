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
    CHG_STATE_FINAL_FAULT
} charger_state_t;

typedef struct
{
    bool bool__installed;
    uint16_t uint16_t__dutyPermille;
    uint16_t uint16_t__dutyBeforeTripPermille;
    uint8_t uint8_t__jitTripCount;
    charger_state_t charger_state_t__state;
    uint32_t uint32_t__absorbStartTick;
    uint32_t uint32_t__retryDeadlineTick;
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
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state !=
            CHG_STATE_FINAL_FAULT)
        {
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                CHG_STATE_OFF;
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbStartTick = 0u;
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

static void func__Charger_StopAllPwm(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/* ==================== Relay helpers ==================== */

static void func__Charger_CloseTransformerInput(uint32_t uint32_t__nowTick)
{
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline =
        uint32_t__nowTick + func__Rtos_MillisecondsToTicks(CHG_RELAY_SETTLE_MS);
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
    charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
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
    charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;

#if MODULE_JITTER
    func__Jitter_ClearChannel((uint8_t)(uint8_t__retryChannel + 1u));
#endif

    func__Charger_ApplyDuty(uint8_t__retryChannel, uint16_t__retryDuty);
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

    if (func__Charger_BatteryVoltageIsValid(uint32_t__batteryMv) == false)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    if (uint32_t__currentMa > CHG_CURRENT_LIMIT_MA)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
        return;
    }

    uint32_t__targetMv = CHG_ABSORB_MV;

    if ((charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT) &&
        (uint32_t__batteryMv < CHG_REENTRY_MV))
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;
        uint32_t__targetMv = CHG_ABSORB_MV;
    }

    if (uint32_t__batteryMv >= CHG_ABSORB_MV)
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BULK)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_ABSORB;
            charger_channel_state_t__channel->uint32_t__absorbStartTick = uint32_t__nowTick;
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
            uint32_t__absorbTicks = func__Charger_DurationTicks(CHG_ABSORB_HOLD_MS);
            if ((uint32_t__nowTick - charger_channel_state_t__channel->uint32_t__absorbStartTick) >=
                uint32_t__absorbTicks)
            {
                charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_FLOAT;
                uint32_t__targetMv = CHG_FLOAT_MV;
            }
        }
    }
    else
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        uint32_t__targetMv = CHG_ABSORB_MV;
    }

    uint16_t__nextDuty = charger_channel_state_t__channel->uint16_t__dutyPermille;

    if (uint32_t__batteryMv < uint32_t__targetMv)
    {
        if (uint32_t__currentMa < CHG_BULK_CURRENT_MAX_MA)
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
        }
    }
    else if (uint32_t__batteryMv > uint32_t__targetMv)
    {
        if (uint16_t__nextDuty > CHG_DUTY_STEP_PERMILLE)
        {
            uint16_t__nextDuty = (uint16_t)(uint16_t__nextDuty - CHG_DUTY_STEP_PERMILLE);
        }
        else
        {
            uint16_t__nextDuty = 0u;
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
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbStartTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
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
        func__Charger_FinalDisconnect();
        return;
    }

    bool__inputAdcValid = (measurement_snapshot_t__snap->v_in_mv >= CHG_INPUT_VALID_MV);
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
        func__Charger_FinalDisconnect();
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
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbStartTick = 0u;
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
