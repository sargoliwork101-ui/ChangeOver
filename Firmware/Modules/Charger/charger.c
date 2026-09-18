/**
 * @file    charger.c
 * @brief   [EN] Generic per-channel 12 V charger control. The two channels
 *          share implementation only; their voltage, current, duty, state,
 *          retry counter and protection decision are separate.
 *          [FA] کنترل عمومی شارژرهای مستقل ۱۲ ولت. فقط پیاده‌سازی مشترک است؛
 *          ولتاژ، جریان، duty، state، retry و حفاظت هر کانال جداست.
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
    CHG_STATE_RETRY_WAIT,
    CHG_STATE_FINAL_FAULT
} charger_state_t;

typedef struct
{
    bool bool__installed;
    uint16_t uint16_t__dutyPermille;
    uint16_t uint16_t__dutyBeforeTripPermille;
    uint8_t uint8_t__retryAttempts;
    charger_state_t charger_state_t__state;
    uint32_t uint32_t__absorbStartTick;
} charger_channel_state_t;

/* ==================== Static state / وضعیت داخلی ==================== */

static charger_channel_state_t CHARGER_CHANNEL_T__G__State[2];
static bool BOOL__G__ChargerInitialized;
static bool BOOL__G__RelayOpen;
static uint32_t UINT32_T__G__RelaySettleDeadline;
static uint32_t UINT32_T__G__JitLockoutDeadline;
static uint8_t UINT8_T__G__RetryChannel;

#define CHG_NO_CHANNEL 0xFFu

/* ==================== Channel helpers / توابع کمکی کانال ==================== */

/**
 * @brief  [EN] Return whether one logical channel is selected by the two
 *              installation constants in charger.h.
 *         [FA] مشخص می‌کند یک کانال منطقی با دو ثابت انتخاب نصب شده است یا نه.
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس صفرمبنای کانال
 * @return bool [EN] true when installed / اگر نصب‌شده باشد true
 */
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

/**
 * @brief  [EN] Resolve the logical PWM channel without duplicating a channel
 *              implementation.
 *         [FA] کانال PWM منطقی را بدون تکرار پیاده‌سازی کانال برمی‌گرداند.
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس صفرمبنای کانال
 * @return bsp_pwm_channel_t [EN] PWM backend channel / کانال backend PWM
 */
static bsp_pwm_channel_t func__Charger_PwmChannel(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return BSP_PWM_CHARGER_1;
    }

    return BSP_PWM_CHARGER_2;
}

/**
 * @brief  [EN] Read the independent battery voltage for one channel. Channel 2
 *              uses VLOW only; it never uses the 24 V pack value in this test.
 *         [FA] ولتاژ مستقل باتری یک کانال را می‌خواند. کانال ۲ فقط VLOW را
 *              مصرف می‌کند و در این تست هرگز مقدار پک ۲۴ ولت را بررسی نمی‌کند.
 * @param  measurement_snapshot_t__snap [EN] Valid measurement snapshot / snapshot معتبر
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس کانال
 * @return uint32_t [EN] Selected 12 V battery voltage in mV / ولتاژ باتری ۱۲ ولت
 */
static uint32_t func__Charger_ChannelVoltageMv(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                                uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return measurement_snapshot_t__snap->v_bat_high_mv;
    }

    return measurement_snapshot_t__snap->v_bat_low_mv;
}

/**
 * @brief  [EN] Read the independent current feedback for one channel.
 *         [FA] فیدبک جریان مستقل یک کانال را می‌خواند.
 * @param  measurement_snapshot_t__snap [EN] Valid measurement snapshot / snapshot معتبر
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس کانال
 * @return uint32_t [EN] Filtered channel current in mA / جریان فیلترشده کانال
 */
static uint32_t func__Charger_ChannelCurrentMa(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                               uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return measurement_snapshot_t__snap->i_ch1_ma;
    }

    return measurement_snapshot_t__snap->i_ch2_ma;
}

/**
 * @brief  [EN] Apply duty to one installed channel and remember that channel's
 *              duty only. There is deliberately no SetPwmBoth operation.
 *         [FA] duty را فقط روی یک کانال نصب‌شده اعمال و همان duty را نگه می‌دارد.
 *              عمداً هیچ عملیات SetPwmBoth وجود ندارد.
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس کانال
 * @param  uint16_t__dutyPermille [EN] Duty in permille / duty بر حسب پرمیل
 */
static void func__Charger_ApplyDuty(uint8_t uint8_t__channelIndex,
                                    uint16_t uint16_t__dutyPermille)
{
    charger_channel_state_t *charger_channel_state_t__channel;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    charger_channel_state_t__channel->uint16_t__dutyPermille = uint16_t__dutyPermille;

    if (charger_channel_state_t__channel->bool__installed == true)
    {
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex),
                                      uint16_t__dutyPermille);
    }
}

/**
 * @brief  [EN] Stop every PWM output and clear each per-channel duty.
 *         [FA] همه خروجی‌های PWM را متوقف و duty هر کانال را صفر می‌کند.
 */
static void func__Charger_StopAllPwm(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
    }
}

/* ==================== Relay helpers / توابع کمکی رله ==================== */

/**
 * @brief  [EN] Assert the relay coil so its NC contact opens and transformer
 *              input is physically disconnected.
 *         [FA] کویل رله را فعال می‌کند تا کنتاکت NC باز و ورودی ترانس واقعاً
 *              قطع شود.
 */
static void func__Charger_OpenTransformerInput(void)
{
    func__BspGpio_Write(BSP_GPIO_RELAY, true);
    BOOL__G__RelayOpen = true;
    UINT32_T__G__RelaySettleDeadline = 0u;
}

/**
 * @brief  [EN] Deassert the relay coil so its NC contact closes; caller must
 *              wait the settle interval before applying PWM.
 *         [FA] کویل رله را غیرفعال می‌کند تا NC بسته شود؛ caller باید قبل از
 *              اعمال PWM زمان settle را رعایت کند.
 */
static void func__Charger_CloseTransformerInput(uint32_t uint32_t__nowTick)
{
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline =
        uint32_t__nowTick + func__Rtos_MillisecondsToTicks(CHG_RELAY_SETTLE_MS);
}

/* ==================== Time helpers / توابع زمان ==================== */

/**
 * @brief  [EN] Convert a policy duration to at least one RTOS tick.
 *         [FA] مدت سیاست را به حداقل یک تیک RTOS تبدیل می‌کند.
 * @param  uint32_t__milliseconds [EN] Duration in ms / مدت بر حسب میلی‌ثانیه
 * @return uint32_t [EN] Nonzero tick duration / مدت تیک غیرصفر
 */
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

/**
 * @brief  [EN] Test a wrap-safe RTOS deadline.
 *         [FA] سررسید RTOS را با پشتیبانی از wrap بررسی می‌کند.
 * @param  uint32_t__nowTick [EN] Current tick / تیک فعلی
 * @param  uint32_t__deadlineTick [EN] Deadline / سررسید
 * @return bool [EN] true when deadline elapsed / اگر سررسید گذشته باشد true
 */
static bool func__Charger_DeadlineElapsed(uint32_t uint32_t__nowTick,
                                          uint32_t uint32_t__deadlineTick)
{
    return ((int32_t)(uint32_t__nowTick - uint32_t__deadlineTick) >= 0);
}

/* ==================== Protection helpers / توابع حفاظت ==================== */

/**
 * @brief  [EN] Latch a final channel fault and remove all PWM because the
 *              transformer-input relay is common to the tested power path.
 *         [FA] fault نهایی کانال را latch و به‌علت مشترک‌بودن رله ورودی، همه
 *              PWMها را قطع می‌کند.
 * @param  uint8_t__channelIndex [EN] Faulted channel / کانال دارای fault
 */
static void func__Charger_LatchFinalFault(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
            CHG_STATE_FINAL_FAULT;
    }

    func__Charger_StopAllPwm();
    func__Charger_OpenTransformerInput();
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

#if MODULE_JITTER

/**
 * @brief  [EN] Handle one JIT trip: capture duty, stop both PWM outputs, open
 *              the NC relay, lock out, then schedule a channel-only retry.
 *         [FA] تریپ JIT را مدیریت می‌کند: duty را ثبت، هر دو PWM را صفر، رله
 *              NC را باز، lockout را اجرا و retry همان کانال را برنامه‌ریزی می‌کند.
 * @param  uint8_t__channelIndex [EN] Tripped channel / کانال تریپ‌کرده
 * @param  uint32_t__nowTick [EN] Current RTOS tick / تیک فعلی RTOS
 */
static void func__Charger_HandleJitTrip(uint8_t uint8_t__channelIndex,
                                        uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;

    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FINAL_FAULT)
    {
        return;
    }

    if (charger_channel_state_t__channel->uint8_t__retryAttempts >= 2u)
    {
        func__Charger_LatchFinalFault(uint8_t__channelIndex);
        return;
    }

    charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille =
        charger_channel_state_t__channel->uint16_t__dutyPermille;
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_RETRY_WAIT;

    func__Charger_StopAllPwm();
    func__Charger_OpenTransformerInput();
    UINT32_T__G__JitLockoutDeadline =
        uint32_t__nowTick + func__Charger_DurationTicks(CHG_JIT_LOCKOUT_MS);
    UINT8_T__G__RetryChannel = uint8_t__channelIndex;
}

#endif /* MODULE_JITTER */

/**
 * @brief  [EN] Return a retry duty: first half of the previous duty, then 10%
 *              or less. A low current never enters this fault path.
 *         [FA] duty retry را برمی‌گرداند: بار اول نصف duty قبلی و بار دوم ۱۰٪
 *              یا کمتر. جریان کم هرگز وارد این مسیر fault نمی‌شود.
 * @param  uint8_t__channelIndex [EN] Retry channel / کانال retry
 * @return uint16_t [EN] Retry duty in permille / duty retry بر حسب پرمیل
 */
static uint16_t func__Charger_RetryDuty(uint8_t uint8_t__channelIndex)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    uint16_t__retryDuty =
        (uint16_t)(charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille / 2u);

    if (charger_channel_state_t__channel->uint8_t__retryAttempts > 0u &&
        uint16_t__retryDuty > CHG_DUTY_RETRY_SECOND_MAX)
    {
        uint16_t__retryDuty = CHG_DUTY_RETRY_SECOND_MAX;
    }

    if ((uint16_t__retryDuty == 0u) &&
        (charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille > 0u))
    {
        uint16_t__retryDuty = 1u;
    }

    return uint16_t__retryDuty;
}

/**
 * @brief  [EN] Complete relay-open lockout and relay-close settle sequencing,
 *              then apply PWM only to the channel that is being retried.
 *         [FA] توالی lockout رله باز و settle رله بسته را کامل و سپس فقط PWM
 *              کانال در حال retry را اعمال می‌کند.
 * @param  uint32_t__nowTick [EN] Current RTOS tick / تیک فعلی
 * @return bool [EN] true while retry sequencing owns the cycle / اگر توالی retry فعال باشد true
 */
static bool func__Charger_ServiceRetry(uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;

    if (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)
    {
        return false;
    }

    if (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                      UINT32_T__G__JitLockoutDeadline) == false)
    {
        func__Charger_StopAllPwm();
        return true;
    }

    if (BOOL__G__RelayOpen == true)
    {
        /* [EN] NC must close before any retry PWM is allowed. */
        /* [FA] پیش از هر PWM retry باید NC بسته شود. */
        func__Charger_CloseTransformerInput(uint32_t__nowTick);
        func__Charger_StopAllPwm();
        return true;
    }

    if (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                      UINT32_T__G__RelaySettleDeadline) == false)
    {
        func__Charger_StopAllPwm();
        return true;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[UINT8_T__G__RetryChannel];
    uint16_t__retryDuty = func__Charger_RetryDuty(UINT8_T__G__RetryChannel);
    charger_channel_state_t__channel->uint8_t__retryAttempts++;
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
    charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;

#if MODULE_JITTER
    func__Jitter_ClearChannel((uint8_t)(UINT8_T__G__RetryChannel + 1u));
#endif

    func__Charger_ApplyDuty(UINT8_T__G__RetryChannel, uint16_t__retryDuty);
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
    UINT32_T__G__RelaySettleDeadline = 0u;
    return true;
}

/* ==================== Regulation / تنظیم حلقه ==================== */

/**
 * @brief  [EN] Apply independent bulk, absorb and float regulation to one
 *              channel. Below-target low current increases duty; it is not a
 *              missing-battery or JIT fault.
 *         [FA] تنظیم مستقل bulk، absorb و float را برای یک کانال اجرا می‌کند.
 *              جریان کم در ولتاژ پایین duty را زیاد می‌کند و fault نیست.
 * @param  uint8_t__channelIndex [EN] Channel index / اندیس کانال
 * @param  measurement_snapshot_t__snap [EN] Snapshot / snapshot
 * @param  uint32_t__nowTick [EN] Current RTOS tick / تیک فعلی
 */
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

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FINAL_FAULT)
    {
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    /* [EN] Missing battery is checked per channel, never as a 24 V pack test. */
    /* [FA] نبود باتری برای هر کانال جدا بررسی می‌شود، نه با تست پک ۲۴ ولت. */
    if (uint32_t__batteryMv < CHG_MIN_VALID_BATTERY_MV)
    {
        func__Charger_LatchFinalFault(uint8_t__channelIndex);
        return;
    }

    /* [EN] Only excessive current is a current protection trip. There is no
       low-current fault: a low current below the voltage target asks for more duty. */
    /* [FA] فقط جریان بیش‌ازحد حفاظت را تریپ می‌کند. جریان کم fault نیست و در
       ولتاژ پایین درخواست duty بیشتر می‌دهد. */
    if (uint32_t__currentMa > CHG_CURRENT_LIMIT_MA)
    {
        func__Charger_LatchFinalFault(uint8_t__channelIndex);
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
    }

    if ((charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT) &&
        (uint32_t__batteryMv < CHG_REENTRY_MV))
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;
    }

    if (uint32_t__batteryMv >= CHG_ABSORB_MV)
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BULK)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_ABSORB;
            charger_channel_state_t__channel->uint32_t__absorbStartTick = uint32_t__nowTick;
        }

        uint32_t__targetMv =
            (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT) ?
            CHG_FLOAT_MV : CHG_ABSORB_MV;

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
        /* [EN] Low current is expected at low duty: increase duty. */
        /* [FA] جریان کم در duty پایین طبیعی است: duty را افزایش بده. */
        if (uint32_t__currentMa < CHG_BULK_CURRENT_MAX_MA)
        {
            uint32_t uint32_t__increasedDuty;

            uint32_t__increasedDuty =
                (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_PERMILLE;
            uint16_t__nextDuty = (uint16_t)((uint32_t__increasedDuty > APP_CONFIG.pwm_max_duty_permille) ?
                                  APP_CONFIG.pwm_max_duty_permille : uint32_t__increasedDuty);
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
    else
    {
        /* [EN] At the voltage target, hold the last independently calculated duty. */
        /* [FA] در ولتاژ هدف، آخرین duty مستقل همان کانال حفظ می‌شود. */
    }

    func__Charger_ApplyDuty(uint8_t__channelIndex, uint16_t__nextDuty);
}

/* ==================== Charger_Init / مقداردهی اولیه ==================== */

/**
 * @brief  [EN] Initialize all per-channel records and force safe hardware state.
 *         [FA] همه رکوردهای مستقل کانال را مقداردهی و سخت‌افزار را امن می‌کند.
 */
void func__Charger_Init(void)
{
    uint8_t uint8_t__channelIndex;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed =
            func__Charger_IsChannelInstalled(uint8_t__channelIndex);
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyBeforeTripPermille = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__retryAttempts = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state = CHG_STATE_OFF;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbStartTick = 0u;
    }

    BOOL__G__ChargerInitialized = true;
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;
    UINT32_T__G__JitLockoutDeadline = 0u;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;

    func__Charger_StopAllPwm();
    func__Charger_OpenTransformerInput();
}

/* ==================== Charger_Evaluate / ارزیابی شارژر ==================== */

/**
 * @brief  [EN] Evaluate every selected 12 V charger independently.
 *         [FA] هر شارژر ۱۲ ولت انتخاب‌شده را مستقل ارزیابی می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / snapshot
 * @param  app_state_t__state [EN] System state / حالت سیستم
 */
void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state)
{
    uint32_t uint32_t__nowTick;
    uint8_t uint8_t__channelIndex;

    if (BOOL__G__ChargerInitialized == false)
    {
        func__Charger_Init();
    }

    uint32_t__nowTick = osKernelGetTickCount();

    /* [EN] This gate intentionally keeps the uncharacterized flyback safe-off. */
    /* [FA] این دروازه عمداً فلای‌بک ناشناخته را در safe-off نگه می‌دارد. */
    if ((measurement_snapshot_t__snap == NULL) ||
        (measurement_snapshot_t__snap->valid == false) ||
        (CHG_INSTALLED_CHANNEL_MASK == 0u) ||
        (CHG_TRANSFORMER_KNOWN == 0u) ||
        (APP_CONFIG.power_stage_enabled == false) ||
        (APP_CONFIG.pwm_max_duty_permille == 0u) ||
        (measurement_snapshot_t__snap->input_present == false) ||
        (app_state_t__state == APP_STATE_FAULT) ||
        (app_state_t__state == APP_STATE_SAFE))
    {
        func__Charger_StopAllPwm();
        func__Charger_OpenTransformerInput();
        return;
    }

#if MODULE_JITTER
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true &&
            func__Jitter_ChannelTripped((uint8_t)(uint8_t__channelIndex + 1u)) == true &&
            UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)
        {
            func__Charger_HandleJitTrip(uint8_t__channelIndex, uint32_t__nowTick);
            return;
        }
    }
#endif

    if (func__Charger_ServiceRetry(uint32_t__nowTick) == true)
    {
        return;
    }

    if (BOOL__G__RelayOpen == true)
    {
        /* [EN] Close NC first; no PWM in the same cycle. */
        /* [FA] ابتدا NC را ببند؛ در همان چرخه PWM نده. */
        func__Charger_CloseTransformerInput(uint32_t__nowTick);
        func__Charger_StopAllPwm();
        return;
    }

    if ((UINT32_T__G__RelaySettleDeadline != 0u) &&
        (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                       UINT32_T__G__RelaySettleDeadline) == false))
    {
        func__Charger_StopAllPwm();
        return;
    }

    UINT32_T__G__RelaySettleDeadline = 0u;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true)
        {
            func__Charger_RegulateChannel(uint8_t__channelIndex,
                                          measurement_snapshot_t__snap,
                                          uint32_t__nowTick);
        }
        else
        {
            /* [EN] The uninstalled channel is always stopped, even if a caller
               accidentally passes a nonzero shared policy value. */
            /* [FA] کانال غیرمونتاژشده همیشه متوقف است، حتی اگر caller اشتباهاً
               مقدار duty مشترک غیرصفر بدهد. */
            func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        }
    }
}
