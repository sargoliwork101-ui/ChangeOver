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
 *              the transformer input is physically removed. This is only
 *              reached after CHG_MASTER_ENABLE=1 (relay policy active).
 *         [FA] قطع نهایی fault: بعد از سومین JIT همان کانال، هر دو PWM قبلاً
 *              متوقف شده‌اند؛ سپس رله NC باز می‌شود تا ورودی ترانس واقعاً قطع
 *              شود. این مسیر فقط با CHG_MASTER_ENABLE=1 (سیاست رله فعال) اجرا
 *              می‌شود.
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
 * @brief  [EN] Return the maximum allowed duty for the active mode (normal
 *              known-transformer control vs explicit bring-up test).
 *         [FA] حداکثر duty مجاز را برای حالت فعال (کنترل عادی با ترانس شناخته
 *              شده یا تست صریح bring-up) برمی‌گرداند.
 */
static uint16_t func__Charger_MaxDutyPermille(void)
{
    if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
    {
        return CHG_BRINGUP_TEST_MAX_DUTY_PERMILLE;
    }

    return CHG_DUTY_MAX_PERMILLE;
}

/**
 * @brief  [EN] Return the source current limit for the active mode. For the
 *              bring-up first stage this is the external lab source limit.
 *         [FA] حد جریان منبع برای حالت فعال را برمی‌گرداند. برای مرحله اول
 *              bring-up این حد منبع آزمایشگاهی خارجی است.
 */
static uint32_t func__Charger_ActiveCurrentLimitMa(void)
{
    if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
    {
        return CHG_BRINGUP_TEST_SOURCE_LIMIT_MA;
    }

    return CHG_CURRENT_LIMIT_MA;
}

/**
 * @brief  [EN] Apply duty to one logical channel and remember that channel's
 *              duty only. Uninstalled channels and Channel 1 in this test
 *              always stay 0 and stopped. In bring-up mode duty is additionally
 *              clamped to the bring-up max.
 *         [FA] duty را فقط روی یک کانال منطقی اعمال و همان duty را نگه
 *              می‌دارد. کانال‌های غیرنصب‌شده و در این تست کانال ۱ همیشه صفر
 *              و متوقف می‌مانند. در حالت bring-up duty همچنین به حداکثر
 *              bring-up محدود می‌شود.
 */
static void func__Charger_ApplyDuty(uint8_t uint8_t__channelIndex,
                                    uint16_t uint16_t__dutyPermille)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint8_t uint8_t__otherIndex;
    uint16_t uint16_t__maxDuty;

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
    if (uint16_t__dutyPermille > uint16_t__maxDuty)
    {
        uint16_t__dutyPermille = uint16_t__maxDuty;
    }

    charger_channel_state_t__channel->uint16_t__dutyPermille = uint16_t__dutyPermille;
    func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex),
                                  uint16_t__dutyPermille);

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
    uint8_t uint8_t__channelIndex;

    func__BspPwm_StopAll();

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
 *   Trip 1/2 on one channel: only that channel PWM stops, relay stays off
 *   (NC closed), other channel continues unchanged, retry same channel at
 *   half then <=10%.
 *   Trip 3 on one channel: stop both PWM, open relay, final fault/lockout.
 *         [FA] مدیریت JIT کاملاً هر کانال:
 *   تریپ ۱/۲ یک کانال: فقط PWM همان کانال صفر، رله خاموش (NC بسته)، کانال
 *   دیگر بدون تغییر، retry همان کانال با نصف سپس حداکثر ۱۰٪.
 *   تریپ ۳ یک کانال: هر دو PWM صفر، رله روشن، fault/lockout نهایی.
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
}

#endif /* MODULE_JITTER */

/**
 * @brief  [EN] Return a same-channel retry duty: first half of the previous
 *              duty for the same channel, then 10% or less. Relay is not
 *              opened for retry 1/2. Bring-up max duty still clamps retry.
 *         [FA] duty retry همان کانال را برمی‌گرداند: بار اول نصف duty قبلی
 *              همان کانال، بار دوم ۱۰٪ یا کمتر. برای retry اول/دوم رله باز
 *              نمی‌شود. حداکثر duty bring-up همچنان retry را محدود می‌کند.
 */
static uint16_t func__Charger_RetryDuty(uint8_t uint8_t__channelIndex)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;
    uint16_t uint16_t__maxDuty;

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

    uint16_t__maxDuty = func__Charger_MaxDutyPermille();
    if (uint16_t__retryDuty > uint16_t__maxDuty)
    {
        uint16_t__retryDuty = uint16_t__maxDuty;
    }

    return uint16_t__retryDuty;
}

/**
 * @brief  [EN] Service same-channel JIT retry waiting. Retry 1/2 does not
 *              open the relay; only the tripped channel is stopped during
 *              wait and restarted on schedule. The other channel keeps its
 *              regulation in the main loop. If Vin is invalid or master is
 *              off, this returns without arming retries (SafeIdle already
 *              owns the cycle).
 *         [FA] انتظار retry JIT همان کانال را سرویس می‌کند. retry اول/دوم رله
 *              را باز نمی‌کند؛ فقط کانال تریپ‌کرده هنگام انتظار متوقف است و
 *              طبق زمان‌بندی دوباره راه‌اندازی می‌شود. کانال دیگر تنظیم خود
 *              را در حلقه اصلی ادامه می‌دهد. اگر Vin نامعتبر یا master خاموش
 *              باشد، بدون arm کردن retry برمی‌گردد (SafeIdle قبل از این چرخه
 *              را در اختیار دارد).
 */
static void func__Charger_ServiceRetry(uint32_t uint32_t__nowTick)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint16_t uint16_t__retryDuty;

    if (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)
    {
        return;
    }

    if (UINT8_T__G__RetryChannel >= 2u)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return;
    }

    charger_channel_state_t__channel = &CHARGER_CHANNEL_T__G__State[UINT8_T__G__RetryChannel];

    if (charger_channel_state_t__channel->charger_state_t__state != CHG_STATE_JIT_RETRY_WAIT)
    {
        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
        return;
    }

    /* [EN] During lockout keep only the tripped channel at zero; do NOT stop
       the other channel here (other channel continues via main Regulate loop).
       Relay stays OFF for retry 1/2.
       [FA] هنگام lockout فقط کانال تریپ‌کرده را صفر نگه دار؛ کانال دیگر اینجا
       متوقف نشود (از حلقه اصلی ادامه می‌دهد). برای retry ۱/۲ رله خاموش است. */
    func__Charger_StopOneChannel(UINT8_T__G__RetryChannel);

    if (func__Charger_DeadlineElapsed(uint32_t__nowTick,
                                      charger_channel_state_t__channel->uint32_t__retryDeadlineTick) == false)
    {
        return;
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
}

/* ==================== Bring-up regulation / تنظیم تست bring-up ==================== */

/**
 * @brief  [EN] Limited bring-up regulation: only duty ramp with strict max
 *              duty and source-current protection. No Absorb/Float, no pack
 *              voltage, no setpoint CV loop. Used only when CHG_TRANSFORMER_KNOWN=0
 *              and CHG_BRINGUP_TEST_ENABLE=1 and CHG_MASTER_ENABLE=1.
 *         [FA] تنظیم محدود bring-up: فقط شیب duty با حداکثر و حفاظت جریان
 *              منبع. بدون Absorb/Float، بدون ولتاژ پک، بدون حلقه CV. فقط وقتی
 *              CHG_TRANSFORMER_KNOWN=0 و CHG_BRINGUP_TEST_ENABLE=1 و CHG_MASTER_ENABLE=1
 *              استفاده می‌شود.
 */
static void func__Charger_BringupRegulateChannel(uint8_t uint8_t__channelIndex,
                                                 const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    charger_channel_state_t *charger_channel_state_t__channel;
    uint32_t uint32_t__currentMa;
    uint32_t uint32_t__batteryMv;
    uint16_t uint16_t__nextDuty;
    uint16_t uint16_t__maxDuty;

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

    if (uint32_t__batteryMv < CHG_MIN_VALID_BATTERY_MV)
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

    /* [EN] In bring-up we ramp slowly: if current below the external source
       limit and duty under bring-up max, add one step. If voltage rises or
       current hits the external limit, hold/back off.
       [FA] در bring-up شیب آهسته: اگر جریان زیر حد منبع خارجی و duty زیر
       حداکثر bring-up بود، یک گام اضافه کن. اگر ولتاژ بالا رفت یا جریان به
       حد منبع رسید، نگه‌دار/عقب بیا. */
    if (uint32_t__currentMa < func__Charger_ActiveCurrentLimitMa())
    {
        if (uint16_t__nextDuty < uint16_t__maxDuty)
        {
            uint32_t uint32_t__increased = (uint32_t)uint16_t__nextDuty + CHG_DUTY_STEP_PERMILLE;
            if (uint32_t__increased > uint16_t__maxDuty) { uint32_t__increased = uint16_t__maxDuty; }
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

/* ==================== Regulation / تنظیم حلقه ==================== */

/**
 * @brief  [EN] Apply independent bulk, absorb and float regulation to one
 *              channel when CHG_TRANSFORMER_KNOWN=1. Below-target low current
 *              increases duty; it is not a missing-battery or JIT fault.
 *         [FA] تنظیم مستقل bulk، absorb و float را برای یک کانال وقتی
 *              CHG_TRANSFORMER_KNOWN=1 اجرا می‌کند. جریان کم در ولتاژ پایین
 *              duty را زیاد می‌کند و fault نیست.
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

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BRINGUP)
    {
        return;
    }

    uint32_t__batteryMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap, uint8_t__channelIndex);
    uint32_t__currentMa =
        func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap, uint8_t__channelIndex);

    if (uint32_t__batteryMv < CHG_MIN_VALID_BATTERY_MV)
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
 * @brief  [EN] Evaluate every selected 12 V charger independently.
 *   CHG_MASTER_ENABLE=0: whole Charger safe-idle, both PWM zero/stopped, JIT
 *     and relay policy inactive (coil off / NC closed).
 *   CHG_TRANSFORMER_KNOWN=0: normal Bulk/Absorb/Float is NOT allowed. Only
 *     CHG_BRINGUP_TEST_ENABLE=1 runs the explicit limited bring-up.
 *   Real ADC input voltage must be at least 22000 mV throughout charge;
 *     PB4 digital signal alone is not enough. If Vin < 22000 mV: PWM1=0,
 *     PWM2=0, relay coil off, NC closed, no retry/PWM until Vin returns.
 *         [FA] هر شارژر ۱۲ ولت انتخاب‌شده را مستقل ارزیابی می‌کند.
 *   CHG_MASTER_ENABLE=0: کل Charger safe-idle، هر دو PWM صفر/متوقف، سیاست
 *     JIT و رله غیرفعال (coil خاموش / NC بسته).
 *   CHG_TRANSFORMER_KNOWN=0: Bulk/Absorb/Float عادی مجاز نیست. فقط
 *     CHG_BRINGUP_TEST_ENABLE=1 حالت bring-up محدود صریح را اجرا می‌کند.
 *   ولتاژ ورودی واقعی ADC باید در کل شارژ حداقل ۲۲۰۰۰ میلی‌ولت باشد؛ سیگنال
 *     دیجیتال PB4 به‌تنهایی کافی نیست. اگر Vin < ۲۲۰۰۰mV: PWM1=0، PWM2=0،
 *     کویل رله خاموش، NC بسته، هیچ retry/PWM تا بازگشت Vin.
 */
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

    /* [EN] CHG_MASTER_ENABLE is the single master switch. With 0, JIT/relay
       policy stays inactive: whole Charger safe-idle, both PWM zero/stopped,
       NC relay closed, no coil energized. This overrides any latched state.
       [FA] CHG_MASTER_ENABLE تنها کلید اصلی است. با ۰ سیاست JIT/رله غیرفعال
       می‌ماند: کل Charger safe-idle، هر دو PWM صفر/متوقف، رله NC بسته، کویل
       بدون انرژی. این هر وضعیت latch‌شده را override می‌کند. */
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

    /* [EN] CHG_TRANSFORMER_KNOWN is not bypassable. Normal Bulk/Absorb/Float
       requires it to be 1. When 0 the ONLY allowed control path is the
       explicit limited bring-up test; if that is also disabled, safe-idle.
       [FA] CHG_TRANSFORMER_KNOWN bypass نمی‌شود. Bulk/Absorb/Float عادی نیاز
       به ۱ دارد. وقتی ۰ است، تنها مساز مجاز تست صریح محدود bring-up است؛ اگر
       آن هم غیرفعال باشد safe-idle. */
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

    /* [EN] Real ADC input voltage is checked every cycle. If Vin < 22000 mV:
       PWM1=0, PWM2=0, relay coil OFF (NC closed), charging stops, no retry
       and no PWM until Vin returns to >= 22000 mV. Channels return to
       INPUT_WAIT and restart from safe duty after recovery. The PB4 digital
       input signal alone is not sufficient protection.
       [FA] ولتاژ ورودی واقعی ADC هر چرخه چک می‌شود. اگر Vin < ۲۲۰۰۰mV:
       PWM1=0، PWM2=0، کویل رله خاموش (NC بسته)، شارژ متوقف، هیچ retry/PWM تا
       بازگشت Vin >= ۲۲۰۰۰mV. کانال‌ها به INPUT_WAIT برمی‌گردند و بعد از
       بازیابی از duty امن شروع می‌شوند. سیگنال دیجیتال PB4 به‌تنهایی کافی
       نیست. */
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
                /* [EN] ServiceRetry already kept this channel at zero; do not
                   run Regulate/Bringup on it this cycle.
                   [FA] ServiceRetry این کانال را صفر نگه داشته؛ این چرخه
                   روی آن Regulate/Bringup اجرا نشود. */
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
            /* [EN] Uninstalled channel is always stopped and compare 0.
               [FA] کانال غیرنصب‌شده همیشه متوقف و compare صفر است. */
            CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
            func__BspPwm_SetDutyPermille(func__Charger_PwmChannel(uint8_t__channelIndex), 0u);
        }
    }
}
