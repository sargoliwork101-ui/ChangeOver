/**
 * @file    charger.c
 * @brief   [EN] Generic per-channel 12 V charger control. CHG_MASTER_ENABLE is
 *          the single master switch; when it is 0 the module stays in
 *          safe-idle with all PWM stopped and the NC relay closed. The two
 *          channels share implementation only; their voltage, current, duty,
 *          state, retry counter and JIT sequence are separate.
 *          [FA] کنترل عمومی شارژرهای مستقل ۱۲ ولت. CHG_MASTER_ENABLE تنها
 *          کلید اصلی است؛ با مقدار ۰ ماژول در safe-idle با همه PWM متوقف و
 *          رله NC بسته باقی می‌ماند. فقط پیاده‌سازی مشترک است؛ ولتاژ، جریان،
 *          duty، state، retry و توالی JIT هر کانال جداست.
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
 *              state for master-disabled, low-input, restart and non-final
 *              non-charging conditions.
 *         [FA] همه خروجی‌های PWM را متوقف می‌کند، duty هر کانال را صفر و رله
 *              NC را بسته (coil خاموش) نگه می‌دارد. این وضعیت safe-idle برای
 *              master غیرفعال، ورودی کم، شروع مجدد و شرایط غیرنهایی بدون شارژ است.
 */
static void func__Charger_SafeIdle(void)
{
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, false);
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/**
 * @brief  [EN] Open the NC transformer input after both PWM outputs are
 *              already stopped. This is only used for final faults after
 *              the third same-channel JIT.
 *         [FA] پس از توقف همه PWMها ورودی NC ترانس را باز می‌کند. این فقط برای
 *              fault نهایی بعد از سومین JIT همان کانال استفاده می‌شود.
 */
static void func__Charger_FinalDisconnect(void)
{
    func__BspPwm_StopAll();
    func__BspGpio_Write(BSP_GPIO_RELAY, true);
    BOOL__G__RelayOpen = true;
    UINT32_T__G__RelaySettleDeadline = 0u;

    uint8_t uint8_t__channelIndex;
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

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
 * @brief  [EN] Read the independent battery voltage for one channel. In the
 *              current board test, Channel 2 (Trans2) charges only the low
 *              12 V battery measured as VLOW = MID - GND; the 24 V pack
 *              value is monitor-only and is never used for CH2 setpoints or
 *              missing-battery decisions.
 *         [FA] ولتاژ مستقل باتری یک کانال را می‌خواند. در تست برد فعلی،
 *              کانال ۲ (Trans2) فقط باتری پایین ۱۲ ولت را با VLOW = MID - GND
 *              شارژ می‌کند؛ مقدار پک ۲۴ ولت فقط مانیتور است و برای setpoint
 *              یا تشخیص باتری غایب CH2 استفاده نمی‌شود.
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
 * @brief  [EN] Apply duty to one logical channel and remember that channel's
 *              duty only. There is deliberately no SetPwmBoth operation.
 *              Uninstalled channels and Channel 1 in this test always stay 0
 *              and stopped.
 *         [FA] duty را فقط روی یک کانال منطقی اعمال و همان duty را نگه
 *              می‌دارد. عمداً هیچ عملیات SetPwmBoth وجود ندارد. کانال‌های
 *              غیرنصب‌شده و در این تست کانال ۱ همیشه صفر و متوقف می‌مانند.
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس کانال
 * @param  uint16_t__dutyPermille [EN] Duty in permille / duty بر حسب پرمیل
 */
static void func__Charger_ApplyDuty(uint8_t uint8_t__channelIndex,
                                    uint16_t uint16_t__dutyPermille)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint8_t uint8_t__otherIndex;

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

    if (uint16_t__dutyPermille > CHG_DUTY_MAX_PERMILLE)
    {
        uint16_t__dutyPermille = CHG_DUTY_MAX_PERMILLE;
    }

    charger_channel_state_t__channel->uint16_t__dutyPermille = uint16_t__dutyPermille;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex),
                                  uint16_t__dutyPermille);

    /* [EN] The other channel must not be changed accidentally. This explicit
       zero enforces Channel 1 = 0 when only Channel 2 is installed, and vice
       versa, even if a future caller shares a policy variable.
       [FA] کانال دیگر نباید اتفاقی تغییر کند. این صفر صریح تضمین می‌کند
       وقتی فقط کانال ۲ نصب است کانال ۱ صفر می‌ماند و برعکس. */
    uint8_t__otherIndex = (uint8_t__channelIndex == 0u) ? 1u : 0u;
    if (CHARGER_CHANNEL_T__G__State[uint8_t__otherIndex].bool__installed == false)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__otherIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__otherIndex), 0u);
    }
}

/**
 * @brief  [EN] Stop one channel PWM output and clear its duty.
 *         [FA] PWM یک کانال را متوقف و duty آن را صفر می‌کند.
 * @param  uint8_t__channelIndex [EN] Zero-based channel index / اندیس کانال
 */
static void func__Charger_StopOneChannel(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex >= 2u)
    {
        return;
    }

    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
}

/**
 * @brief  [EN] Stop every PWM output and clear each per-channel duty.
 *         [FA] همه خروجی‌های PWM را متوقف و duty هر کانال را صفر می‌کند.
 */
static void func__Charger_StopAllPwm(void)
{
    func__BspPwm_StopAll();

    uint8_t uint8_t__channelIndex;
    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
        func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
    }
}

/* ==================== Relay helpers / توابع کمکی رله ==================== */

/**
 * @brief  [EN] Close the NC transformer input by de-energizing the coil. The
 *              caller must respect CHG_RELAY_SETTLE_MS before applying PWM.
 *         [FA] با غیرفعال‌کردن کویل، ورودی NC ترانس را می‌بندد. caller باید
 *              قبل از اعمال PWM زمان CHG_RELAY_SETTLE_MS را رعایت کند.
 * @param  uint32_t__nowTick [EN] Current RTOS tick / تیک فعلی RTOS
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
 * @brief  [EN] Reset the per-channel charging state back to OFF so the next
 *              entry starts from the safe initial duty after a stop.
 *         [FA] وضعیت شارژ هر کانال را به OFF برمی‌گرداند تا ورود بعدی از duty
 *              اولیه امن شروع شود.
 * @param  uint8_t__channelIndex [EN] Channel index / اندیس کانال
 */
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

/**
 * @brief  [EN] Latch a final channel fault after the third same-channel JIT.
 *              Both PWM outputs stop, then the NC relay opens and the input
 *              is disconnected; this requires manual/host reset.
 *         [FA] بعد از سومین JIT همان کانال fault نهایی را latch می‌کند. هر دو
 *              PWM متوقف می‌شوند، سپس رله NC باز و ورودی قطع می‌شود؛ این حالت
 *              نیاز به reset دستی/میزبان دارد.
 * @param  uint8_t__channelIndex [EN] Faulted channel / کانال دارای fault
 */
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

/**
 * @brief  [EN] Handle one JIT trip per channel:
 *   Trip 1/2 on one channel: only that channel PWM stops, relay stays off,
 *   other channel continues unchanged, retry same channel at half then <=10%.
 *   Trip 3 on one channel: stop both PWM, open relay, final fault/lockout.
 *         [FA] مدیریت JIT کاملاً هر کانال:
 *   تریپ ۱/۲ یک کانال: فقط PWM همان کانال صفر، رله خاموش، کانال دیگر بدون تغییر،
 *   retry همان کانال با نصف سپس حداکثر ۱۰٪.
 *   تریپ ۳ یک کانال: هر دو PWM صفر، رله روشن، fault/lockout نهایی.
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

    charger_channel_state_t__channel->uint8_t__jitTripCount++;

    if (charger_channel_state_t__channel->uint8_t__jitTripCount >= 3u)
    {
        func__Charger_LatchFinalFault(uint8_t__channelIndex);
        return;
    }

    /* [EN] First/second JIT on one channel: stop only that channel PWM.
       Relay remains OFF, NC stays closed, other channel keeps running.
       [FA] JIT اول/دوم یک کانال: فقط PWM همان کانال صفر. رله خاموش، NC بسته،
       کانال دیگر بدون تغییر به کار ادامه می‌دهد. */
    func__Charger_StopOneChannel(uint8_t__channelIndex);
    charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille =
        charger_channel_state_t__channel->uint16_t__dutyPermille;
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_JIT_RETRY_WAIT;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick =
        uint32_t__nowTick + func__Charger_DurationTicks(CHG_JIT_LOCKOUT_MS);
    UINT8_T__G__RetryChannel = uint8_t__channelIndex;
    UINT32_T__G__RelaySettleDeadline = 0u;
}

#endif /* MODULE_JITTER */

/**
 * @brief  [EN] Return a same-channel retry duty: first half of the previous
 *              duty for the same channel, then 10% or less. Relay is not
 *              opened for retry 1/2.
 *         [FA] duty retry همان کانال را برمی‌گرداند: بار اول نصف duty قبلی
 *              همان کانال، بار دوم ۱۰٪ یا کمتر. برای retry اول/دوم رله باز
 *              نمی‌شود.
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

    if (charger_channel_state_t__channel->uint8_t__jitTripCount > 1u &&
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
 * @brief  [EN] Service same-channel JIT retry waiting. First/second retries do
 *              not open the relay; only the tripped channel is stopped during
 *              wait and restarted. A third trip opens the relay through final
 *              fault.
 *         [FA] انتظار retry JIT همان کانال را سرویس می‌کند. retry اول/دوم رله
 *              را باز نمی‌کند؛ فقط کانال تریپ‌کرده هنگام انتظار متوقف است و
 *              دوباره راه‌اندازی می‌شود. تریپ سوم از طریق fault نهایی رله را
 *              باز می‌کند.
 * @param  uint32_t__nowTick [EN] Current RTOS tick / تیک فعلی
 * @return bool [EN] true while retry sequencing owns a channel / اگر توالی retry یک کانال را اداره می‌کند true
 */
static bool func__Charger_ServiceRetry(uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;

    if (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)
    {
        return false;
    }

    if (UINT8_T__G__RetryChannel >= 2u)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return false;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[UINT8_T__G__RetryChannel];

    if (charger_channel_state_t__channel->charger_state_t__state != CHG_STATE_JIT_RETRY_WAIT)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return false;
    }

    if (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                      charger_channel_state_t__channel->uint32_t__retryDeadlineTick) == false)
    {
        func__Charger_StopOneChannel(UINT8_T__G__RetryChannel);
        return true;
    }

    uint16_t__retryDuty = func__Charger_RetryDuty(UINT8_T__G__RetryChannel);
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
    charger_channel_state_t__channel->uint32_t__absorbStartTick = 0u;
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;

#if MODULE_JITTER
    func__Jitter_ClearChannel((uint8_t)(UINT8_T__G__RetryChannel + 1u));
#endif

    func__Charger_ApplyDuty(UINT8_T__G__RetryChannel, uint16_t__retryDuty);
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
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

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_JIT_RETRY_WAIT)
    {
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_INPUT_WAIT)
    {
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    /* [EN] Missing battery is checked on the same channel battery sense. For
       Trans2 this is VLOW = MID - GND; the 24 V pack value is never used.
       [FA] نبود باتری روی sense همان کانال بررسی می‌شود. برای Trans2 این
       مقدار VLOW = MID - GND است؛ مقدار پک ۲۴ ولت استفاده نمی‌شود. */
    if (uint32_t__batteryMv < CHG_MIN_VALID_BATTERY_MV)
    {
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    /* [EN] Only excessive current is a current protection trip. There is no
       low-current fault: a low current below the voltage target asks for more
       duty. Current greater than 675 mA enters protection.
       [FA] فقط جریان بیش‌ازحد حفاظت را تریپ می‌کند. جریان کم fault نیست و
       در ولتاژ پایین درخواست duty بیشتر می‌دهد. جریان بیشتر از ۶۷۵ میلی‌آمپر
       وارد حفاظت می‌شود. */
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
        /* [EN] Low current is expected at low duty/voltage: increase the same
           channel duty by 5 permille steps.
           [FA] جریان کم در duty/ولتاژ پایین طبیعی است: duty همان کانال با گام
           ۵ پرمیل افزایش می‌یابد. */
        if (uint32_t__currentMa < CHG_BULK_CURRENT_MAX_MA)
        {
            uint32_t uint32_t__increasedDuty;

            uint32_t__increasedDuty =
                (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_PERMILLE;
            uint16_t__nextDuty = (uint16_t)((uint32_t__increasedDuty > CHG_DUTY_MAX_PERMILLE) ?
                                  CHG_DUTY_MAX_PERMILLE : uint32_t__increasedDuty);
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
 * @brief  [EN] Initialize all per-channel records and force safe-idle: all
 *              PWM stopped and NC relay closed (coil off).
 *         [FA] همه رکوردهای مستقل کانال را مقداردهی اولیه و safe-idle را
 *              اعمال می‌کند: همه PWM متوقف و رله NC بسته (coil خاموش).
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

/* ==================== Charger_Evaluate / ارزیابی شارژر ==================== */

/**
 * @brief  [EN] Evaluate every selected 12 V charger independently. The real
 *              ADC input voltage must be at least 22000 mV throughout charge;
 *              the PB4 digital signal is not enough by itself. If Vin drops
 *              below 22000 mV, all PWM stops, the relay stays off/NC closed,
 *              and restart begins from CHG_DUTY_START_PERMILLE after recovery.
 *         [FA] هر شارژر ۱۲ ولت انتخاب‌شده را مستقل ارزیابی می‌کند. ولتاژ ورودی
 *              واقعی ADC باید در کل شارژ حداقل ۲۲۰۰۰ میلی‌ولت باشد؛ سیگنال
 *              دیجیتال PB4 به‌تنهایی کافی نیست. اگر Vin کمتر از ۲۲۰۰۰mV شود،
 *              همه PWM صفر، رله خاموش/NC بسته می‌ماند و شروع مجدد بعد از
 *              بازگشت از CHG_DUTY_START_PERMILLE انجام می‌شود.
 * @param  measurement_snapshot_t__snap [EN] Snapshot / snapshot
 * @param  app_state_t__state [EN] System state / حالت سیستم
 */
void func__Charger_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap,
                            app_state_t app_state_t__state)
{
    uint32_t uint32_t__nowTick;
    uint8_t uint8_t__channelIndex;
    bool bool__anyFinalFault;
    bool bool__inputAdcValid;

    (void)app_state_t__state;

    if (BOOL__G__ChargerInitialized == false)
    {
        func__Charger_Init();
    }

    uint32_t__nowTick = osKernelGetTickCount();

    /* [EN] CHG_MASTER_ENABLE is the single master switch. With 0, the whole
       Charger stays safe-off. No hidden extra charger-enable flags remain.
       Numerical protections are not bypassed because no control loop runs.
       [FA] CHG_MASTER_ENABLE تنها کلید اصلی است. با ۰ کل Charger safe-off
       می‌ماند. هیچ دروازه پنهان اضافی برای فعال‌سازی Charger باقی نمی‌ماند.
       حفاظت‌های عددی bypass نمی‌شوند چون هیچ حلقه کنترلی اجرا نمی‌شود. */
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

    /* [EN] Real ADC input voltage is checked every cycle. If Vin < 22000 mV,
       stop all PWM, do not turn on the relay, and move channels to INPUT_WAIT
       so restart begins from safe duty when Vin returns to >= 22000 mV. The
       PB4 digital input signal alone is not sufficient protection.
       [FA] ولتاژ ورودی واقعی ADC در هر چرخه چک می‌شود. اگر Vin < ۲۲۰۰۰mV،
       همه PWM صفر، رله روشن نشود و کانال‌ها به INPUT_WAIT بروند تا هنگام
       بازگشت Vin >= ۲۲۰۰۰mV شروع مجدد با duty امن انجام شود. سیگنال دیجیتال
       PB4 به‌تنهایی برای حفاظت کافی نیست. */
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

    if (func__Charger_ServiceRetry(uint32_t__nowTick) == true)
    {
        return;
    }

    if (BOOL__G__RelayOpen == true)
    {
        /* [EN] A final fault already disconnected; keep disconnected until reset.
           [FA] fault نهایی قبلاً قطع کرده؛ تا reset قطع بماند. */
        func__Charger_FinalDisconnect();
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
            /* [EN] Returning from low Vin: each channel restarts from safe duty.
               [FA] بازگشت از Vin کم: هر کانال از duty امن دوباره شروع می‌شود. */
            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                CHG_STATE_INPUT_WAIT)
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                    CHG_STATE_OFF;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__absorbStartTick = 0u;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
            }

            func__Charger_RegulateChannel(uint8_t__channelIndex,
                                          measurement_snapshot_t__snap,
                                          uint32_t__nowTick);
        }
        else
        {
            /* [EN] The uninstalled channel is always stopped and compare 0,
               even if any future caller or policy tries to set duty.
               [FA] کانال غیرنصب‌شده همیشه متوقف و compare صفر است، حتی اگر
               caller یا سیاستی در آینده بخواهد duty بدهد. */
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
            func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        }
    }
}
