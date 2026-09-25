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
    CHG_STATE_BAT_LOST,  /* [EN] battery wire cut during charge (fault bit latched) / باتری حین شارژ قطع شده */
    CHG_STATE_MANUAL     /* [EN] manual test mode owns this channel: duty driven from the panel, battery gates bypassed / مود تست دستی مالک کانال: duty از پنل، گیت‌های باتری رد شده‌اند */
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
    uint32_t uint32_t__stableFromTick; /* [EN] tick when installed+input+battery first looked valid; 0 = not present, gates bulk start (CHG_CONNECT_SETTLE_MS) / تیک اولین‌لحظه‌ای که اتصال معتبر دیده شد؛ صفر = باتری حاضر نیست؛ گیت شروع بالک */
    uint32_t uint32_t__taperSinceTick; /* [EN] first tick in this ABSORB episode that the tail current looked below CHG_TAPER_CURRENT_MA; 0 = not tapering / تیک اولین زیرجریان در ابزورب؛ صفر یعنی زیرجریان نیست */
    uint32_t uint32_t__absorbEnterTick; /* [EN] tick this ABSORB episode started; 0 = not in absorb; feeds the CHG_ABSORB_MAX_MS ceiling / تیک ورود به این ابزورب؛ صفر یعنی خارج؛ برای سقف یک‌ساعت */
} charger_channel_state_t;

/* ==================== Static state / وضعیت داخلی ==================== */

static charger_channel_state_t CHARGER_CHANNEL_T__G__State[2];

/* [EN] Runtime charge profile, shared by both channels (user order
 *      2026-09-25: settable from the ESP panel tab, wire ids 20..26). Boot
 *      defaults = the old compile-time setpoints; RAM-only like every other
 *      parameter. func__Charger_ClampProfile() keeps the set consistent.
 * [FA] پروفایل شارژ زمان اجرا، مشترک بین هر دو کانال (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۵: تنظیم از تب پنل، شناسه‌های سیمی ۲۰..۲۶). پیش‌فرض بوت =
 *      ست‌پوینت‌های کامپایل‌تایم قبلی؛ مثل بقیهٔ پارامترها فقط در RAM.
 *      func__Charger_ClampProfile() مجموعه را سازنده نگه می‌دارد. */
typedef struct
{
    uint32_t uint32_t__absorbMv;        /* [EN] absorb hold setpoint / تثبیت ابزورب */
    uint32_t uint32_t__absorbEnterMv;   /* [EN] absorb entry threshold / آستانهٔ ورود ابزورب */
    uint32_t uint32_t__absorbOverMv;    /* [EN] coarse down-step ceiling / سقف کاهش سریع */
    uint32_t uint32_t__floatMv;         /* [EN] float hold setpoint / تثبیت شناور */
    uint32_t uint32_t__reentryMv;       /* [EN] float->bulk reentry / بازگشت شناور به بالک */
    uint32_t uint32_t__bulkCurrentMaxMa;/* [EN] current-regulation top of band / سقف باند جریان */
    uint32_t uint32_t__taperCurrentMa;  /* [EN] float-entry tail current / زیرجریان ورود به شناور */
} charger_profile_t;

static charger_profile_t CHARGER_PROFILE_T__G__Profile =
{
    CHG_ABSORB_MV, CHG_ABSORB_ENTER_MV, CHG_ABSORB_OVER_MV,
    CHG_FLOAT_MV, CHG_REENTRY_MV, CHG_BULK_CURRENT_MAX_MA, CHG_TAPER_CURRENT_MA
};

/* [EN] Live diag array - see the layout map in charger.h (user order
 *      2026-09-22: all charge-decision values visible in one Live
 *      Expressions entry).
 * [FA] آرایهٔ دیاگ زنده - نقشهٔ چیدمان در charger.h (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲: همهٔ مقادیر تصمیم شارژ در یک ورودی Live Expressions). */
volatile uint32_t UINT32_T__G__ChargerDiag[CHG_DIAG_COUNT] = {0u};

/* [EN] Calibration worksheet array - see the layout map in charger.h (user
 *      order 2026-09-22: one Live Expressions entry for bench calibration).
 * [FA] آرایهٔ برگهٔ کالیبراسیون - نقشهٔ چیدمان در charger.h (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲: یک ورودی Live Expressions برای کالیبراسیون بنچ). */
volatile uint32_t UINT32_T__G__ChargerCalib[CHG_CALIB_COUNT] = {0u};

/* [EN] Live per-channel output-current estimates (the decision values) -
 *      see charger.h. ch1 = upper/VHIGH, ch2 = lower/VLOW.
 * [FA] جریان‌های خروجی تخمینی زنده per channel (مقادیر تصمیم) - توضیح در
 *      charger.h. کانال ۱ = بالا/VHIGH، کانال ۲ = پایین/VLOW. */
volatile uint32_t UINT32_T__G__ChargerIest1Ma = 0u;
volatile uint32_t UINT32_T__G__ChargerIest2Ma = 0u;

/* [EN] Runtime per-channel conversion factors ETA1/ETA2 (protocol v1.3,
 *      user order 2026-09-24). 0 (compiled default) = the estimate is the
 *      identity: with the 2026-09-24 battery-calibrated gains the filtered
 *      reading already equals the battery current. Non-zero = the live
 *      conversion iest = I x Vin x eta / (1000 x Vbat), set either directly
 *      (ESP params 9/10) or computed by the panel CAL_REFERENCE command
 *      from a typed battery-side DMM reading. RAM only - a reboot restores
 *      the compiled defaults; written by the EspLink task, read in the
 *      control task; aligned 32-bit values are atomic on Cortex-M3.
 * [FA] ضریب‌های تبدیل زمان اجرای هر کانال ETA1/ETA2 (پروتکل v1.3، دستور
 *      کاربر ۲۰۲۶-۰۹-۲۴). صفر (پیش‌فرض کامپایل) = تخمین همانی است: با
 *      گین‌های کالیبره-باتریِ ۲۰۲۶-۰۹-۲۴ عدد فیلترشده خودش جریان باتری
 *      است. غیرصفر = تبدیل زندهٔ iest = I × Vin × η ÷ (۱۰۰۰ × Vbat) که
 *      یا مستقیم (پارامتر ۹/۱۰ ESP) یا با فرمان CAL_REFERENCE پنل از عدد
 *      مولتی‌متر سمت باتری محاسبه می‌شود. فقط RAM - ری‌استارت به پیش‌فرض
 *      کامپایل برمی‌گرداند؛ نوشتن از تسک EspLink و خواندن در تسک کنترل؛
 *      مقادیر ۳۲ بیتی تراز روی Cortex-M3 اتمیک‌اند. */
static volatile uint32_t UINT32_T__G__ChargerEta1Permille =
    CHG_FLYBACK_ETA1_PERMILLE;
static volatile uint32_t UINT32_T__G__ChargerEta2Permille =
    CHG_FLYBACK_ETA2_PERMILLE;

/* [EN] Runtime per-channel enable gates for the ESP command panel (user
 *      order 2026-09-22: the ESP must be able to cut and reconnect each
 *      charger module). Default true = today's behavior. While false the
 *      channel's PWM is forced off and the state is held at OFF; a faulted
 *      channel (FINAL_FAULT) is never released by this gate. RAM only.
 * [FA] گیت‌های فعال‌سازی هر کانال برای پنل فرمان ESP (دستور کاربر
 *      ۲۰۲۶-۰۹-۲۲: ESP باید بتواند هر ماژول شارژر را قطع/وصل کند).
 *      پیش‌فرض true = رفتار فعلی. تا وقتی false است PWM آن کانال قطع و
 *      وضعیت روی OFF نگه داشته می‌شود؛ کانال FINAL_FAULT هرگز با این
 *      گیت آزاد نمی‌شود. فقط RAM. */
static volatile bool BOOL__G__ChargerEspEnableCh[2] = {true, true};

/* [EN] Runtime per-channel PWM duty ceiling (user order 2026-09-22: the
 *      ESP panel sets the cap). Default CHG_DUTY_MAX_PERMILLE = today's
 *      behavior; ApplyDuty clamps EVERY requested duty (ramp, regulation,
 *      fixed mode) to min(compile max, this ceiling). RAM only.
 * [FA] سقف duty ی PWM هر کانال در زمان اجرا (دستور کاربر ۲۰۲۶-۰۹-۲۲:
 *      پنل ESP سقف را تعیین می‌کند). پیش‌فرض CHG_DUTY_MAX_PERMILLE همان
 *      رفتار فعلی؛ ApplyDuty هر duty درخواستی (رمپ، تنظیم، مود فیکس)
 *      را به min(سقف کامپایل، این سقف) گیره می‌زند. فقط RAM. */
static volatile uint32_t UINT32_T__G__ChargerDutyCeilingPermille[2] =
    {CHG_DUTY_MAX_PERMILLE, CHG_DUTY_MAX_PERMILLE};

/* [EN] Runtime fixed-duty mode per channel (user order 2026-09-22: hold
 *      the PWM at one chosen number instead of the regulation loop). While
 *      enabled, the channel applies its fixed duty each pass instead of
 *      ramping/regulating - with EXACTLY the safety wrapper of the proven
 *      compile-time bench-test mode: switching stops above CHG_ABSORB_MV
 *      (no overcharge with regulation off), all JIT / input / battery /
 *      ESP-cut gates above stay active. Default off. RAM only.
 * [FA] مود duty فیکس هر کانال در زمان اجرا (دستور کاربر ۲۰۲۶-۰۹-۲۲:
 *      نگه‌داشتن PWM روی یک عدد دلخواه به‌جای حلقهٔ تنظیم). تا وقتی
 *      فعال است کانال در هر پاس duty فیکس خودش را اعمال می‌کند نه رمپ/
 *      تنظیم - با دقیقاً همان پوشش امنیتی مود تست بنچ کامپایل‌تایم:
 *      بالای CHG_ABSORB_MV سوئیچینگ متوقف می‌شود (با تنظیم خاموش،
 *      بیش‌شارژ ممکن نیست) و همهٔ گیت‌های JIT/ورودی/باتری/قطع ESP
 *      بالادست فعال می‌مانند. پیش‌فرض خاموش. فقط RAM. */
static volatile bool BOOL__G__ChargerDutyFixedEnable[2] = {false, false};
static volatile uint32_t UINT32_T__G__ChargerDutyFixedPermille[2] = {0u, 0u};

/* [EN] Manual test mode state (user order 2026-09-23, protocol v1.2
 *      param 19). Requested is written by the ESP link task; Active is
 *      owned by the charger task and flips in func__Charger_Evaluate,
 *      where every enter/exit action runs in the charger context. The
 *      link stamp feeds the CHG_MANUAL_WATCHDOG_MS dead-man and is
 *      refreshed by every valid ESP frame. RearmRequest is the manual
 *      JIT re-arm signal: a fresh duty write while parked re-arms the
 *      channel (the write itself happens in the ESP task, the charger
 *      task consumes the request).
 * [FA] وضعیت مود تست دستی (دستور کاربر ۲۰۲۶-۰۹-۲۳، پارامتر ۱۹ v1.2).
 *      Requested را تسک ESP می‌نویسد؛ Active مالکش تسک شارژر است و در
 *      Evaluate برمی‌گردد جایی که همهٔ عملیات ورود/خروج در زمینهٔ شارژر
 *      اجرا می‌شود. مهرِ لینک ددمنِ CHG_MANUAL_WATCHDOG_MS را غذا می‌دهد و
 *      هر فریم معتبر ESP آن را تازه می‌کند. RearmRequest سیگنال re-arm ی
 *      JIT دستی است: نوشتنِ دوبارهٔ duty در حالت پارک کانال را مسلح
 *      می‌کند (نوشتن در تسک ESP، مصرف درخواست در تسک شارژر). */
static volatile bool BOOL__G__ChargerManualModeRequested = false;
static volatile bool BOOL__G__ChargerManualModeActive = false;
static volatile uint32_t UINT32_T__G__ManualLastLinkTick = 0u;
static volatile bool BOOL__G__ChargerManualRearmRequest[2] = {false, false};

static bool BOOL__G__ChargerInitialized;
static bool BOOL__G__RelayOpen;
static uint32_t UINT32_T__G__RelaySettleDeadline;

#define CHG_NO_CHANNEL 0xFFu
static uint8_t UINT8_T__G__RetryChannel;

/* ==================== Forward declarations / اعلان پیش‌موضع ==================== */
static bsp_pwm_channel_t func__Charger_PwmChannel(uint8_t uint8_t__channelIndex);
static void func__Charger_ClearAbsorbWindow(
    charger_channel_state_t *charger_channel_state_t__channel);
static void func__Charger_EnterManualTestMode(uint32_t uint32_t__nowTick);
static void func__Charger_ExitManualTestMode(void);
static void func__Charger_ManualDriveChannel(uint8_t uint8_t__channelIndex,
                                             const measurement_snapshot_t *measurement_snapshot_t__snap);

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
/**
 * @brief  [EN] Clear the absorb-window bookkeeping of one channel (soak
 *              counter, taper stamp and both window stamps). Every restart
 *              or re-entry path calls this so the 10-minute soak always
 *              starts from zero. The entry-into-ABSORB path stamps
 *              absorbEnter/Last with "now" instead and stays inline.
 *         [FA] پاک‌کردن دفترچهٔ پنجرهٔ ابزورب یک کانال (شمارندهٔ شستشو، مهر
 *              تیپر و دو مهر پنجره). هر مسیر ری‌استارت/ورود مجدد این را
 *              صدا می‌زند تا شستشوی ۱۰ دقیقه همیشه از صفر شروع شود؛ مسیر
 *              ورود به ABSORB به‌جای صفر، مهر «اکنون» می‌زند و جدا می‌ماند.
 * @param  charger_channel_state_t__channel [EN] channel to clear / کانال هدف
 */
static void func__Charger_ClearAbsorbWindow(
    charger_channel_state_t *charger_channel_state_t__channel)
{
    charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
    charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
    charger_channel_state_t__channel->uint32_t__absorbEnterTick = 0u;
    charger_channel_state_t__channel->uint32_t__absorbLastTick = 0u;
}

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
            func__Charger_ClearAbsorbWindow(
                &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex]);
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
 * @brief  [EN] Output (battery-side) current estimate - calibration
 *              architecture v1.3 (user order 2026-09-24). Two modes:
 *              (1) ETA = 0 (compiled default): identity - with the
 *              battery-calibrated gains the filtered reading already
 *              equals the battery current (user bench fact 2026-09-24), so
 *              a reflash changes no number.
 *              (2) ETA != 0 (ESP param 9/10, or computed by the panel
 *              CAL_REFERENCE command from a battery-side DMM reading):
 *              iest = I x Vin x eta / (1000 x Vbat) with the LIVE input
 *              and channel-battery voltages, so the reading stays true
 *              while the battery voltage moves during a charge (identity
 *              drifts by Vbat_cal/Vbat). Guards: below CHG_ETA_MIN_VIN_MV
 *              / CHG_ETA_MIN_VBAT_MV the estimate falls back to identity
 *              instead of dividing a garbage snapshot. The math is ordered
 *              to stay inside 32 bits: (I*eta/1000) stays under ~1e7, the
 *              product with Vin under ~3e8.
 *              History: the deleted conversion (Vin*eta/Vbat stacked on
 *              top of the battery-calibrated gain) double-counted the
 *              ratio - iest read ~0.5x with eta 242 and ~1.4x with eta 786;
 *              that record lives in charger.h.
 *         [FA] تخمین جریان خروجی (سمت باتری) - معماری کالیبراسیون v1.3
 *              (دستور کاربر ۲۰۲۶-۰۹-۲۴). دو مود:
 *              (۱) η = ۰ (پیش‌فرض کامپایل): همانی - با گین‌های کالیبره-باتری
 *              عدد فیلترشده خودش جریان باتری است (واقعیت بنچ کاربر
 *              ۲۰۲۶-۰۹-۲۴)؛ پس ریفلش هیچ عددی را عوض نمی‌کند.
 *              (۲) η ≠ ۰ (پارامتر ۹/۱۰ ESP، یا محاسبهٔ فرمان CAL_REFERENCE
 *              پنل با مولتی‌متر سمت باتری): iest = I × Vin × η ÷ (۱۰۰۰ ×
 *              Vbat) با ولتاژهای زندهٔ ورودی و باتری کانال، تا خوانش با
 *              بالا رفتن ولتاژ باتری در طول شارژ درست بماند (حالت همانی
 *              به‌اندازهٔ Vbat_کالیبراسیون÷Vbat منحرف می‌شود). گارد: زیر
 *              CHG_ETA_MIN_VIN_MV / CHG_ETA_MIN_VBAT_MV تخمین به‌جای تقسیم
 *              snapshot بی‌معنی به همانی برمی‌گردد. ترتیب ریاضی طوری است
 *              که داخل ۳۲ بیت بماند: (I×η÷۱۰۰۰) زیر ~۱e7 و حاصل‌ضرب با Vin
 *              زیر ~۳e8 می‌ماند. تاریخچه: تبدیل حذف‌شدهٔ قدیمی (Vin×η÷Vbat
 *              روی گین کالیبره-باتری) نسبت را دوبار می‌شمرد - با η=242
 *              عدد ~۰٫۵ برابر و با η=786 عدد ~۱٫۴ برابر می‌شد؛ ثبتش در
 *              charger.h است.
 * @param  measurement_snapshot_t__snap [EN] Live snapshot (Vin/Vbat) / snapshot زنده (Vin/Vbat)
 * @param  uint8_t__channelIndex [EN] 0 = ch1 (upper battery), 1 = ch2 / ۰=کانال۱ (باتری بالا)، ۱=کانال۲
 * @param  uint32_t__primaryMa [EN] Filtered chain current, mA / جریان فیلترشدهٔ زنجیره، mA
 * @return uint32_t [EN] Battery-side current estimate, mA / تخمین جریان سمت باتری، mA
 */
static uint32_t func__Charger_OutputEstimateMa(const measurement_snapshot_t *measurement_snapshot_t__snap,
                                               uint8_t uint8_t__channelIndex,
                                               uint32_t uint32_t__primaryMa)
{
    uint32_t uint32_t__etaPermille =
        (uint8_t__channelIndex == 0u) ? UINT32_T__G__ChargerEta1Permille
                                      : UINT32_T__G__ChargerEta2Permille;

    if (uint32_t__etaPermille == 0u)
    {
        /* [EN] Identity (default): the reading already is the battery
           current. [FA] همانی (پیش‌فرض): عدد خودش جریان باتری است. */
        return uint32_t__primaryMa;
    }

    uint32_t uint32_t__vinMv = measurement_snapshot_t__snap->v_in_mv;
    uint32_t uint32_t__vbatMv =
        func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                       uint8_t__channelIndex);

    if ((uint32_t__vinMv < CHG_ETA_MIN_VIN_MV) ||
        (uint32_t__vbatMv < CHG_ETA_MIN_VBAT_MV))
    {
        /* [EN] Garbage voltages: fall back to the identity.
           [FA] ولتاژهای بی‌معنی: برگشت به همانی. */
        return uint32_t__primaryMa;
    }

    /* [EN] iest = I x Vin x eta / (1000 x Vbat), 32-bit-safe order.
       [FA] iest = I × Vin × η ÷ (۱۰۰۰ × Vbat)، ترتیب امن برای ۳۲ بیت. */
    return ((((uint32_t__primaryMa * uint32_t__etaPermille) / 1000u) * uint32_t__vinMv) /
            uint32_t__vbatMv);
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

    /* [EN] Derived from the profile band: limit = band + 25 mA (was the
       compile-time 675 over the 650 band). / سقف از باند پروفایل: حد = باند + ۲۵mA. */
    return (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa + 25u);
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
    /* [EN] ESP runtime ceiling (user order 2026-09-22): every applied duty
       is clamped to the smaller of the compile max and the live per-channel
       ceiling - the single choke point for ramp, regulation and fixed mode.
       [FA] سقف زمان اجرای ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲): هر duty اعمالی
       به کمینهٔ سقف کامپایل و سقف زندهٔ همان کانال گیره می‌خورد - تنها
       گلوگاه برای رمپ، تنظیم و مود فیکس. */
    if (UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex] <
        (uint32_t)uint16_t__maxDuty)
    {
        uint16_t__maxDuty =
            (uint16_t)UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex];
    }
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
    func__Charger_ClearAbsorbWindow(
        charger_channel_state_t__channel);
    charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = 0u;
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
    if (BOOL__G__ChargerManualModeActive != false)
    {
        /* [EN] Manual test mode: the hardware cut stands, but there is NO
           auto-retry and no revive queue - the human re-sends the duty
           value (param 16/18) to re-arm the channel. The trip count still
           accumulates, so the 3rd trip latches FINAL_FAULT exactly like in
           the automatic mode.
           [FA] مود تست دستی: قطع سخت‌افزاری می‌ماند اما هیچ ریتری خودکار و
           صف احیایی نیست - کاربر برای مسلح‌کردن دوباره، مقدار duty
           (پارامتر ۱۶/۱۸) را دوباره می‌فرستد. شمارش تریپ جمع می‌شود، پس
           سومین تریپ دقیقاً مثل مود خودکار FINAL_FAULT را قفل می‌کند. */
        charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
    }
    else
    {
        charger_channel_state_t__channel->uint32_t__retryDeadlineTick =
            uint32_t__nowTick + func__Charger_DurationTicks(CHG_JIT_LOCKOUT_MS);
        /* [EN] Take the revive pointer only if it is free; while the rival is
           retrying this channel simply queues in JIT_RETRY_WAIT and the adopt-
           scan in ServiceRetry picks it up next - no pointer stomping, strict
           first-tripped-first-revived order.
           [FA] اشاره‌گر احیا فقط اگر آزاد است گرفته می‌شود؛ وگرنه کانال در صف
           می‌ماند تا اسکن ServiceRetry بردارد - ترتیب احیا حفظ می‌شود. */
        if (UINT8_T__G__RetryChannel == CHG_NO_CHANNEL)
        {
            UINT8_T__G__RetryChannel = uint8_t__channelIndex;
        }
    }
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

    /* [EN] Two-channel retry queue (audit 2026-09-20): a channel tripped
       while the rival was retrying is parked in JIT_RETRY_WAIT with its OWN
       per-channel deadline; when the global pointer frees up, adopt the
       first waiting channel so nothing starves. Revivals stay one-at-a-time
       (the pointer), preserving main's single-resume sequencing.
       [FA] صف ریتری دوکاناله (ممیزی): کانال تریپ‌شده در پنجرهٔ حریف با
       ددلاین خودش پارک می‌شود و با آزادشدن اشاره‌گر، اولین منتظر اخذ
       می‌شود تا قحطی نباشد؛ احیا همچنان تک‌تک. */
    if (uint8_t__retryChannel == CHG_NO_CHANNEL)
    {
        uint8_t uint8_t__waitIndex;

        for (uint8_t__waitIndex = 0u; uint8_t__waitIndex < 2u; uint8_t__waitIndex++)
        {
            if (CHARGER_CHANNEL_T__G__State[uint8_t__waitIndex].charger_state_t__state ==
                CHG_STATE_JIT_RETRY_WAIT)
            {
                uint8_t__retryChannel = uint8_t__waitIndex;
                UINT8_T__G__RetryChannel = uint8_t__retryChannel;
                break;
            }
        }
    }

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
    func__Charger_ClearAbsorbWindow(
        charger_channel_state_t__channel);
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

/**
 * @brief  [EN] True when the channel has seen installed + valid input +
 *         valid battery voltage continuously for at least
 *         CHG_CONNECT_SETTLE_MS (user directive 2026-09-19: let the
 *         connection settle 10..20 s, THEN start the charge). Used as the
 *         mandatory gate for every OFF -> BULK cold start; JIT resume and
 *         the 12.8 V reentry keep their already-live stamps so mid-cycle
 *         paths are not delayed.
 *         [FA] فقط وقتی اتصال به‌طور پیوسته حداقل ۱۵ ثانیه معتبر دیده شده
 *         اجازهٔ شروع بالک می‌دهد (دستور کاربر: اول ثبات، بعد شارژ).
 * @param  uint8_t__channelIndex [EN] channel 0 or 1 / کانال ۰ یا ۱
 * @param  uint32_t__nowTick     [EN] current kernel tick / تیک فعلی کرنل
 * @return bool [EN] true = settled, bulk may start / true = ثابت شده، بالک مجاز
 */
static bool func__Charger_BulkStartSettled(uint8_t uint8_t__channelIndex,
                                           uint32_t uint32_t__nowTick)
{
    uint32_t uint32_t__stableFromTick;

    uint32_t__stableFromTick =
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick;
    if (uint32_t__stableFromTick == 0u)
    {
        return false;
    }
    return ((uint32_t)(uint32_t__nowTick - uint32_t__stableFromTick) >=
            func__Charger_DurationTicks(CHG_CONNECT_SETTLE_MS));
}

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

    /* [EN] ESP enable gate (user order 2026-09-22: the ESP command panel
       must be able to cut and reconnect each charger module). Placed AFTER
       the fault/special states so a FINAL_FAULT latch is never released by
       this gate; while disabled the PWM is forced off and the state is held
       at OFF, so re-enabling soft-restarts BULK from 1% duty.
       [FA] گیت فعال‌سازی ESP (دستور کاربر ۲۰۲۶-۰۹-۲۲: پنل فرمان ESP باید
       بتواند هر ماژول شارژر را قطع/وصل کند). بعد از حالت‌های خطا/ویژه
       قرار گرفته تا قفل FINAL_FAULT هرگز با این گیت آزاد نشود؛ تا زمان
       غیرفعالی PWM قطع و وضعیت روی OFF نگه داشته می‌شود و با وصل دوباره
       BULK از duty ۱٪ نرم شروع می‌شود. */
    if (BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex] == false)
    {
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_OFF;
        return;
    }

    /* [EN] Runtime fixed-duty mode (user order 2026-09-22: hold the PWM at
       one chosen number). Mirrors the proven compile-time bench-test block
       below: switching stops above CHG_ABSORB_MV so the battery cannot be
       overcharged with regulation off, the state shows BULK, and every
       protection cut above (JIT, input, battery, ESP) stays active.
       ApplyDuty clamps to min(compile max, runtime ceiling).
       [FA] مود duty فیکس زمان اجرا (دستور کاربر ۲۰۲۶-۰۹-۲۲: نگه‌داشتن
       PWM روی یک عدد). آینهٔ بلوک تست بنچ کامپایل‌تایم پایین است: بالای
       CHG_ABSORB_MV سوئیچینگ متوقف می‌شود تا با تنظیمِ خاموش باتری
       بیش‌شارژ نشود، وضعیت BULK دیده می‌شود و همهٔ حفاظت‌های بالادست
       (JIT، ورودی، باتری، ESP) فعال می‌مانند. ApplyDuty به کمینهٔ سقف
       کامپایل و سقف زمان اجرا گیره می‌زند. */
    if (BOOL__G__ChargerDutyFixedEnable[uint8_t__channelIndex] != false)
    {
        uint32_t__batteryMv =
            func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                           uint8_t__channelIndex);
        /* [EN] Same overcharge guard as the bench-test mode: with the
           regulation loop off, stop switching at the absorb voltage.
           [FA] همان محافظ بیش‌شارژ مود تست بنچ: با خاموش‌بودن حلقهٔ
           تنظیم، سوئیچینگ در ولتاژ ابزورب متوقف می‌شود. */

        if (uint32_t__batteryMv >= CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv)
        {
            func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
            return;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
            func__Charger_ClearAbsorbWindow(
                charger_channel_state_t__channel);
        }
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        func__Charger_ApplyDuty(uint8_t__channelIndex,
                                (uint16_t)UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex]);
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
           The value already passed Measurement's median-3 + moving-average
           chain (user order 2026-09-22), so this software cut reacts within
           one average window (~10 ms) - the hardware JIT comparator remains
           the fast over-current protection.
           [FA] فقط خطای سخت اضافه‌جریان کانال را ریست می‌کند؛ اضافهٔ عادی در
           باند دیوتی پایین‌تر مدیریت می‌شود. مقدار از زنجیرهٔ مدین-۳ +
           میانگین متحرک Measurement گذشته (دستور کاربر ۲۰۲۶-۰۹-۲۲) پس این
           قطع نرم‌افزاری حداکثر در حد یک پنجرهٔ میانگین (~۱۰ms) واکنش می‌دهد -
           حفاظت سریع اضافه‌جریان همچنان JIT سخت‌افزاری است. */
        func__Charger_ResetChannelToOff(uint8_t__channelIndex);
        func__Charger_StopOneChannel(uint8_t__channelIndex);
        return;
    }

    /* [EN] Charger adds no filter of its own on the current (user order
       2026-09-22): the snapshot current is a clean PWM mid-ON synchronized
       primary sample, already passed through Measurement's switchable
       median-3 / moving-average-10 chain, and converted straight to the
       output estimate above. The regulation band and the >950 mA hard fault
       both decide on that value; duty rate limits (one step per 500/1000 ms)
       prevent hunting and the hardware JIT comparator remains the fast
       over-current protection.
       [FA] شارژر خودش هیچ فیلتری روی جریان اضافه نمی‌کند (دستور کاربر
       ۲۰۲۶-۰۹-۲۲): جریان snapshot نمونهٔ سنکرونِ تمیزِ وسط ON پالس PWM است
       که از زنجیرهٔ کلیددار مدین-۳ / میانگین-۱۰ Measurement عبور کرده و بالا
       به جریان خروجی تخمینی تبدیل شده. باند تنظیم و خطای سخت بالای ۹۵۰mA
       هر دو با همین مقدار تصمیم می‌گیرند؛ محدودیت نرخ پله‌های duty (هر
       ۵۰۰/۱۰۰۰ms) جلوی hunting را می‌گیرد و JIT سخت‌افزاری حفاظت سریع
       اضافه‌جریان باقی می‌ماند. */

#if (CHG_FIXED_DUTY_TEST_ENABLE != 0u)
    /* [EN] Bench diagnostic: fixed duty, no ramp/band/voltage regulation.
       Switching stops at the absorb voltage so the battery cannot be
       overcharged with regulation off; all protection cuts above stay active.
       [FA] حالت تست بنچ: دیوتی ثابت ۱۵٪؛ بالای ۱۴٫۴V سوئیچینگ متوقف؛ همهٔ
       حفاظت‌ها فعال‌اند. */
    if (uint32_t__batteryMv >= CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ClearAbsorbWindow(
            charger_channel_state_t__channel);
    }
    charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
    func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_FIXED_DUTY_TEST_DUTY_PERMILLE);
    return;
#endif

    if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_OFF)
    {
        if (func__Charger_BulkStartSettled(uint8_t__channelIndex,
                                           uint32_t__nowTick) == false)
        {
            /* [EN] Connection not stable long enough yet (user directive
               2026-09-19: 10..20 s of "the battery is really connected"
               before any charge - 15 s): stay OFF at zero duty. This is also
               the flap-killer for battery-lost: after the fault bit clears
               (30 s), a still-missing cable keeps the voltage invalid, the
               stamp stays 0 and BULK never re-arms - no more re-pump /
               re-alarm ping-pong and no more yellow blink flashing inside
               the battery-lost buzzer window.
               [FA] اتصال هنوز ۱۵ ثانیه پایدار نیست (دستور کاربر: اول ثبات،
               بعد شارژ): OFF با دیوتی صفر می‌ماند. همین گیت چرخهٔ پینگ‌پنگ
               آلارم/بازتهیج آلارم و چشمک زرد وسط بوق قطع باتری را می‌کشد. */
            func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
            return;
        }
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ClearAbsorbWindow(
            charger_channel_state_t__channel);
        func__Charger_ApplyDuty(uint8_t__channelIndex, CHG_DUTY_START_PERMILLE);
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = uint32_t__nowTick;
        return;
    }

    uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;

    if ((charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT) &&
        (uint32_t__batteryMv < CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv))
    {
        charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_BULK;
        func__Charger_ClearAbsorbWindow(
            charger_channel_state_t__channel);
        uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
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
    if (uint32_t__batteryMv >= CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv)
    {
        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_BULK)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_ABSORB;
            charger_channel_state_t__channel->uint32_t__absorbAccumTicks = 0u;
            charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
            charger_channel_state_t__channel->uint32_t__absorbEnterTick = uint32_t__nowTick;
            charger_channel_state_t__channel->uint32_t__absorbLastTick = uint32_t__nowTick;
        }

        if (charger_channel_state_t__channel->charger_state_t__state == CHG_STATE_FLOAT)
        {
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
        }
        else
        {
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
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

            bool bool__taperNow;
            bool bool__taperDone;
            bool bool__soakDone;
            bool bool__absorbTimedOut;
            uint32_t uint32_t__taperSustainTicks;
            uint32_t uint32_t__absorbMaxTicks;

            /* [EN] Absorb completion (user formula 2026-09-20, bench pack
               4.5 Ah): FLOAT begins when the minimum soak has passed AND the
               tail current stays below CHG_TAPER_CURRENT_MA (50 mA ~ C/90)
               steadily for CHG_TAPER_SUSTAIN_MS (60 s; the sense chain
               wobbles +/-10..20 mA so single dipping frames must not complete
               the charge). The CHG_ABSORB_MAX_MS = 1 hour ceiling ends
               absorb into FLOAT anyway, so a battery that never tapers
               cannot keep the pump awake forever.
               [FA] پایان ابزورب (فرمول کاربر، پک ۴٫۵ آمپرساعت): وقتی حداقل
               شستشو گذشته باشد **و** زیرجریان <۵۰mA به‌مدت پایدار ۶۰s مانده
               باشد FLOAT آغاز می‌شود؛ سقف امن یک‌ساعت در هرحال به FLOAT
               می‌فرستد تا باتریِ هرگز-تیپر‌نشده پمپ را بیدار نگه ندارد. */
            uint32_t__taperSustainTicks = func__Charger_DurationTicks(CHG_TAPER_SUSTAIN_MS);
            uint32_t__absorbMaxTicks = func__Charger_DurationTicks(CHG_ABSORB_MAX_MS);

            bool__taperNow = (uint32_t__currentMa < CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa);
            if (bool__taperNow == false)
            {
                charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
            }
            else if (charger_channel_state_t__channel->uint32_t__taperSinceTick == 0u)
            {
                charger_channel_state_t__channel->uint32_t__taperSinceTick = uint32_t__nowTick;
            }
            else
            {
                /* [EN] Taper already running. / زیرجریان قبلاً شروع شده. */
            }

            bool__taperDone =
                ((charger_channel_state_t__channel->uint32_t__taperSinceTick != 0u) &&
                 ((uint32_t)(uint32_t__nowTick -
                             charger_channel_state_t__channel->uint32_t__taperSinceTick) >=
                  uint32_t__taperSustainTicks));
            bool__soakDone =
                ((uint32_t__absorbTicks != 0u) &&
                 (charger_channel_state_t__channel->uint32_t__absorbAccumTicks >=
                  uint32_t__absorbTicks));
            bool__absorbTimedOut =
                ((charger_channel_state_t__channel->uint32_t__absorbEnterTick != 0u) &&
                 ((uint32_t)(uint32_t__nowTick -
                             charger_channel_state_t__channel->uint32_t__absorbEnterTick) >=
                  uint32_t__absorbMaxTicks));

            if (((bool__soakDone == true) && (bool__taperDone == true)) ||
                (bool__absorbTimedOut == true))
            {
                /* [EN] Episode over: drop the episode timers so the next
                   absorb starts from a clean sheet.
                   [FA] این سفرقسمت تمام شد؛ تایمرهای اپیزود صفر تا ابزورب بعدی
                   از صفحهٔ تمیز آغاز شود. */
                charger_channel_state_t__channel->uint32_t__taperSinceTick = 0u;
                charger_channel_state_t__channel->uint32_t__absorbEnterTick = 0u;
                charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_FLOAT;
                uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
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
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
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
            uint32_t__targetMv = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
            func__Charger_ClearAbsorbWindow(
                charger_channel_state_t__channel);
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
           <12.8 V reentry (handled above) wakes BULK again. Previously the
           low-current neutral band could freeze a few-% standby duty and
           trickle forever.
           [FA] اتمام سیکل شارژ یعنی پارک پمپ روی صفر (دستور کاربر): دیوتی
           با ضرب‌آهنگ زبری تا صفر پایین می‌آید و پارک می‌شود؛ باتری روی
           ولتاژ طبیعی خودش استراحت می‌کند و فقط reentry زیر ۱۲٫۸V به بالک
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

        if (uint32_t__batteryMv > CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv)
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
        if (uint32_t__currentMa > CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa)
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
        else if (uint32_t__currentMa <
                 ((CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa > 70u)
                      ? (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa - 20u)
                      : 50u)) /* [EN] derived band bottom = top - 20 / کف باند = سقف − ۲۰ */
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
        func__Charger_ClearAbsorbWindow(
            &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex]);
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__lastDutyStepTick = 0u;
        CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick = 0u;
    }

    BOOL__G__ChargerInitialized = true;
    BOOL__G__RelayOpen = false;
    UINT32_T__G__RelaySettleDeadline = 0u;
    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;

    func__Charger_SafeIdle();
}

/* ==================== Manual test mode helpers / هلپرهای مود تست دستی ==================== */

/**
 * @brief  [EN] Adopt the manual test mode: stamp the link dead-man and
 *              clear the battery-lost fault bit so its alarm cannot fire
 *              while bench testing (fault.c freezes the detection while
 *              the mode is active). The channel drive happens in the same
 *              Evaluate pass below.
 *         [FA] مود تست دستی را برمی‌دارد: مهر ددمنِ لینک و پاک‌کردن بیت
 *              خطای قطع باتری تا آلارمش حین تست بنچ نیفتد (fault.c تا
 *              وقتی مود فعال است تشخیص را فریز می‌کند). درایو کانال در
 *              همان پاس Evaluate پایین‌تر انجام می‌شود.
 * @param  uint32_t__nowTick [EN] Current kernel tick / تیک فعلی هسته
 */
static void func__Charger_EnterManualTestMode(uint32_t uint32_t__nowTick)
{
    BOOL__G__ChargerManualModeActive = true;
    UINT32_T__G__ManualLastLinkTick = uint32_t__nowTick;

#if MODULE_FAULT
    func__Fault_Clear(FAULT_CHARGER_BAT_LOST);
#endif
}

/**
 * @brief  [EN] Leave the manual test mode (panel off-switch or link
 *              dead-man): both duties drop to 0 and every channel
 *              restarts the AUTONOMOUS charger from OFF with fresh
 *              bookkeeping (the 15 s connection settle applies again).
 *              A latched FINAL_FAULT is never released here; the JIT trip
 *              latches of parked channels are cleared so the comparator
 *              can fire again.
 *         [FA] خروج از مود تست دستی (کلید خاموش پنل یا ددمن لینک): هر
 *              دو duty صفر و هر کانال شارژر خودکار را از OFF با دفترچهٔ
 *              تمیز ری‌استارت می‌کند (ثبات ۱۵ ثانیه‌ای دوباره اعمال
 *              می‌شود). قفل FINAL_FAULT هرگز اینجا آزاد نمی‌شود؛ لچ‌های
 *              تریپ JIT کانال‌های پارک‌شده پاک می‌شوند تا کمپریتور
 *              دوباره بتواند شلیک کند.
 */
static void func__Charger_ExitManualTestMode(void)
{
    uint8_t uint8_t__channelIndex;
    charger_channel_state_t *charger_channel_state_t__channel;

    BOOL__G__ChargerManualModeRequested = false;
    BOOL__G__ChargerManualModeActive = false;

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        charger_channel_state_t__channel =
            &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];

#if MODULE_JITTER
        if (charger_channel_state_t__channel->charger_state_t__state ==
            CHG_STATE_JIT_RETRY_WAIT)
        {
            func__Jitter_ClearChannel((uint8_t)(uint8_t__channelIndex + 1u));
        }
#endif

        if (charger_channel_state_t__channel->charger_state_t__state !=
            CHG_STATE_FINAL_FAULT)
        {
            charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_OFF;
        }

        func__Charger_StopOneChannel(uint8_t__channelIndex);
        func__Charger_ClearAbsorbWindow(charger_channel_state_t__channel);
        charger_channel_state_t__channel->uint32_t__retryDeadlineTick = 0u;
        charger_channel_state_t__channel->uint32_t__lastDutyStepTick = 0u;
        charger_channel_state_t__channel->uint32_t__stableFromTick = 0u;
        charger_channel_state_t__channel->uint8_t__jitTripCount = 0u;
        charger_channel_state_t__channel->uint16_t__dutyBeforeTripPermille = 0u;
        BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] = false;
    }

    UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
}

/**
 * @brief  [EN] Drive one channel in manual test mode: the commanded duty
 *              (the stored fixed-duty value, param 16/18) is applied
 *              directly - no ramp, no regulation loop, no battery gates.
 *              The hardware floor stays: the ESP channel cut forces 0,
 *              the 15.0 V hard overvoltage cutoff (CHG_MAX_VALID_BATTERY_MV)
 *              forces 0 until the voltage falls back, and ApplyDuty clamps
 *              to min(compile max, runtime ceiling). State shows MANUAL.
 *         [FA] درایو یک کانال در مود تست دستی: duty فرمان‌شده (مقدار فیکس
 *              ذخیره‌شده، پارامتر ۱۶/۱۸) مستقیم اعمال می‌شود - بدون رمپ،
 *              بدون حلقهٔ تنظیم، بدون گیت باتری. کف سخت‌افزاری می‌ماند:
 *              قطع ESP کانال → صفر، قطع سخت ۱۵٫۰V (CHG_MAX_VALID_BATTERY_MV)
 *              → صفر تا افت ولتاژ، و ApplyDuty به کمینهٔ سقف کامپایل و سقف
 *              زمان اجرا گیره می‌زند. وضعیت MANUAL نشان داده می‌شود.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  measurement_snapshot_t__snap [EN] Snapshot / نمونه
 */
static void func__Charger_ManualDriveChannel(uint8_t uint8_t__channelIndex,
                                             const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    charger_channel_state_t *charger_channel_state_t__channel;

    charger_channel_state_t__channel =
        &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex];
    charger_channel_state_t__channel->charger_state_t__state = CHG_STATE_MANUAL;

    /* [EN] ESP channel cut (param 11/12): duty 0, the state stays MANUAL.
       [FA] قطع ESP کانال (پارامتر ۱۱/۱۲): duty صفر، وضعیت MANUAL می‌ماند. */
    if (BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex] == false)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    /* [EN] Hard overvoltage floor: with regulation off and no battery
       clamping the output, stop switching at 15.0 V and resume by itself
       once the voltage is back below it (protocol v1.2, section 5.2).
       [FA] کف سخت اضافه‌ولتاژ: با تنظیمِ خاموش و باتری‌ای که ولتاژ را
       نگه ندارد، سوئیچینگ در ۱۵٫۰V متوقف و با افت زیرش خودکار ادامه
       می‌یابد (پروتکل v1.2، بخش 5.2). */
    if (func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                       uint8_t__channelIndex) >= CHG_MAX_VALID_BATTERY_MV)
    {
        func__Charger_ApplyDuty(uint8_t__channelIndex, 0u);
        return;
    }

    func__Charger_ApplyDuty(
        uint8_t__channelIndex,
        (uint16_t)UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex]);
}

/* ==================== Charger_Evaluate ==================== */
/**
 * @brief  [EN] Refresh the live diag array and the calibration worksheet
 *         array with every decision value.
 *         [FA] آرایهٔ دیاگ زنده و آرایهٔ برگهٔ کالیبراسیون را با همهٔ
 *         مقادیر تصمیم به‌روز می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Current snapshot (may be NULL) /
 *         snapshot فعلی (می‌تواند NULL باشد)
 */
static void func__Charger_CaptureDiag(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint8_t uint8_t__channelIndex;
    uint32_t uint32_t__base;
    uint32_t uint32_t__calibBase;
    uint32_t uint32_t__primaryMa;
    bool bool__snapValid;

    bool__snapValid =
        ((measurement_snapshot_t__snap != NULL) &&
         (measurement_snapshot_t__snap->valid == true));

    for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
    {
        uint32_t__base =
            (uint32_t)uint8_t__channelIndex * CHG_DIAG_CHANNEL_STRIDE;
        uint32_t__calibBase =
            CHG_CALIB_CH_UP_BASE +
            ((uint32_t)uint8_t__channelIndex * CHG_CALIB_CH_STRIDE);

        UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_STATE] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state;
        UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_DUTY] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille;
        UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_STATE] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state;
        UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_DUTY] =
            (uint32_t)CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille;

        if (bool__snapValid == true)
        {
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_VBAT] =
                func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                               uint8_t__channelIndex);
            uint32_t__primaryMa =
                func__Charger_ChannelCurrentMa(measurement_snapshot_t__snap,
                                               uint8_t__channelIndex);
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IPRI] =
                uint32_t__primaryMa;
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST] =
                func__Charger_OutputEstimateMa(measurement_snapshot_t__snap,
                                               uint8_t__channelIndex,
                                               uint32_t__primaryMa);
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_VBAT] =
                UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_VBAT];
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IPRI] =
                uint32_t__primaryMa;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IEST] =
                UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST];
            if (uint8_t__channelIndex == 0u)
            {
                UINT32_T__G__ChargerIest1Ma =
                    UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST];
            }
            else
            {
                UINT32_T__G__ChargerIest2Ma =
                    UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST];
            }
        }
        else
        {
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_VBAT] = 0u;
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IPRI] = 0u;
            UINT32_T__G__ChargerDiag[uint32_t__base + CHG_DIAG_IDX_IEST] = 0u;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_VBAT] = 0u;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IPRI] = 0u;
            UINT32_T__G__ChargerCalib[uint32_t__calibBase + CHG_CALIB_OFF_IEST] = 0u;
            if (uint8_t__channelIndex == 0u)
            {
                UINT32_T__G__ChargerIest1Ma = 0u;
            }
            else
            {
                UINT32_T__G__ChargerIest2Ma = 0u;
            }
        }
    }

    if (bool__snapValid == true)
    {
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VIN] =
            measurement_snapshot_t__snap->v_in_mv;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V24] =
            measurement_snapshot_t__snap->v_bat24_mv;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V12] =
            measurement_snapshot_t__snap->v_bat12_mv;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VHIGH] =
            measurement_snapshot_t__snap->v_bat_high_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VIN] =
            measurement_snapshot_t__snap->v_in_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V24] =
            measurement_snapshot_t__snap->v_bat24_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V12] =
            measurement_snapshot_t__snap->v_bat12_mv;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VHIGH] =
            measurement_snapshot_t__snap->v_bat_high_mv;
    }
    else
    {
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VIN] = 0u;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V24] = 0u;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_V12] = 0u;
        UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VHIGH] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VIN] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V24] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_V12] = 0u;
        UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VHIGH] = 0u;
    }

#if MODULE_FAULT
    UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_FAULT] = (uint32_t)func__Fault_Get();
#else
    UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_FAULT] = 0u;
#endif

    UINT32_T__G__ChargerDiag[CHG_DIAG_IDX_VALID] =
        ((bool__snapValid == true) ? 1u : 0u);
    UINT32_T__G__ChargerCalib[CHG_CALIB_IDX_VALID] =
        ((bool__snapValid == true) ? 1u : 0u);
}


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

    /* [EN] Manual test mode transitions (user order 2026-09-23, protocol
       v1.2 param 19): the ESP link task only writes the REQUEST; every
       enter/exit action runs here in the charger context, so no PWM or
       state write ever races between tasks. Entering also clears the
       battery-lost bit BEFORE the mirror below so a pre-latched alarm
       cannot hold the channels hostage during a bench session (fault.c
       freezes the detection while the mode is active).
       [FA] گذارهای مود تست دستی (دستور کاربر ۲۰۲۶-۰۹-۲۳، پارامتر ۱۹
       v1.2): تسک ESP فقط «درخواست» را می‌نویسد؛ همهٔ عملیات ورود/خروج
       اینجا در زمینهٔ شارژر اجرا می‌شود تا هیچ نوشتنِ PWM یا وضعیتی بین
       تسک‌ها مسابقه نکند. ورود، بیت قطع باتری را قبل از آینهٔ پایین پاک
       می‌کند تا آلارمِ از قبل قفل‌شده کانال‌ها را گروگان نگیرد (fault.c
       تا وقتی مود فعال است تشخیص را فریز می‌کند). */
    if (BOOL__G__ChargerManualModeRequested != BOOL__G__ChargerManualModeActive)
    {
        if (BOOL__G__ChargerManualModeRequested != false)
        {
            func__Charger_EnterManualTestMode(uint32_t__nowTick);
        }
        else
        {
            func__Charger_ExitManualTestMode();
        }
    }

    /* [EN] Link dead-man: while the mode is active the panel must keep the
       link alive; 3 s of silence (CHG_MANUAL_WATCHDOG_MS) drops both duties
       to 0 and returns the charger to autonomous operation.
       [FA] ددمنِ لینک: تا وقتی مود فعال است پنل باید لینک را زنده نگه
       دارد؛ ۳ ثانیه سکوت (CHG_MANUAL_WATCHDOG_MS) هر دو duty را صفر و
       شارژر را به حالت خودکار برمی‌گرداند. */
    if ((BOOL__G__ChargerManualModeActive != false) &&
        (func__Charger_DeadlineElapsed(
             uint32_t__nowTick,
             UINT32_T__G__ManualLastLinkTick +
                 func__Charger_DurationTicks(CHG_MANUAL_WATCHDOG_MS)) != false))
    {
        func__Charger_ExitManualTestMode();
    }

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

    /* [EN] Refresh the diag array before any gate so it stays live in every
       path (safe-idle, fault, normal charge). Duty/state are the values the
       previous pass applied; the current/estimate slots are exactly what the
       regulation below will decide on this pass.
       [FA] آرایهٔ دیاگ قبل از هر گِیت به‌روز می‌شود تا در همهٔ مسیرها زنده
       بماند؛ duty/state مقدار اعمال‌شدهٔ پاس قبل است و جریان/تخمین دقیقاً
       همان چیزی است که تنظیم پایین‌تر در همین پاس رویش تصمیم می‌گیرد. */
    func__Charger_CaptureDiag(measurement_snapshot_t__snap);

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

    /* [EN] Connection-settle bookkeeping for every installed channel
       (user refinement 2026-09-19): the 15-second count must START FROM THE
       MOMENT BATTERY VOLTAGE IS SEEN - the stamp ticks up while the
       snapshot is live and this channel's battery voltage is in range, any
       lapse resets it to 0. The input is deliberately NOT part of the
       predicate (input validity gates the bulk start separately upstream),
       so a battery that sat connected for minutes charges ~instantly when
       the mains comes back.
       [FA] دفترچهٔ ثبات اتصال (اصلاحیهٔ کاربر): شمارش ۱۵ ثانیه از لحظهٔ
       دیده‌شدن ولتاژ باتری شروع می‌شود - فقط snapshot سالم + ولتاژ باتری در
       محدوده؛ ورودی عمداً در این شرط نیست چون گیت جداگانه‌اش بالادست است؛
       پس باتری‌ای که از قبل وصل است با برگشت برق تقریباً بلافاصله شارژ
       می‌شود. */
    {
        for (uint8_t__channelIndex = 0u; uint8_t__channelIndex < 2u; uint8_t__channelIndex++)
        {
            bool bool__readyNow;

            bool__readyNow =
                (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].bool__installed == true) &&
                (func__Charger_BatteryVoltageIsValid(
                    func__Charger_ChannelVoltageMv(measurement_snapshot_t__snap,
                                                   uint8_t__channelIndex)) == true);

            if (bool__readyNow == true)
            {
                if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick == 0u)
                {
                    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick =
                        uint32_t__nowTick;
                }
            }
            else
            {
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__stableFromTick = 0u;
            }
        }
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
            (func__Jitter_ChannelTripped((uint8_t)(uint8_t__channelIndex + 1u)) == true))
        {
            /* [EN] Park EVERY tripped channel at zero duty immediately (two-
               channel audit 2026-09-20: the old "only while no retry is
               running" gate left the second channel pumping for up to one
               full 3 s lockout against an asserted comparator). Waiting in
               JIT_RETRY_WAIT is queued per channel; func__Charger_ServiceRetry
               revives them one at a time, so simultaneous restarts still
               cannot happen.
               [FA] کانال تریپ‌شده بلافاصله با دیوتی صفر پارک می‌شود (ممیزی
               دوکاناله: گیت قدیمی کانال دوم را تا ۳ ثانیه با کمپریتور مسلط
               در حال پمپ نگه می‌داشت)؛ احیای صف تک‌تک با ServiceRetry. */
            func__Charger_HandleJitTrip(uint8_t__channelIndex, uint32_t__nowTick);
        }
    }
#endif

    /* [EN] No automatic JIT revival in manual test mode: a parked channel
       waits for the human to re-send its duty value.
       [FA] در مود تست دستی احیای خودکار JIT نیست: کانال پارک‌شده منتظر
       فرستادن دوبارهٔ duty توسط کاربر می‌ماند. */
    if (BOOL__G__ChargerManualModeActive == false)
    {
        func__Charger_ServiceRetry(uint32_t__nowTick);
    }

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
                func__Charger_ClearAbsorbWindow(
                    &CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex]);
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint16_t__dutyPermille = 0u;
                CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint8_t__jitTripCount = 0u;
            }

            if (CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state ==
                CHG_STATE_JIT_RETRY_WAIT)
            {
                if ((BOOL__G__ChargerManualModeActive != false) &&
                    (BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] != false))
                {
                    /* [EN] Manual JIT re-arm (protocol v1.2 section 5.2): a
                       fresh duty write is the re-arm gesture - clear the park
                       and the trip latch, then the manual drive below applies
                       the commanded duty in this same pass. The trip COUNT is
                       untouched, so the 3rd trip still latches FINAL_FAULT.
                       [FA] مسلح‌کردن دوبارهٔ JIT در مود دستی (بخش 5.2 ی
                       v1.2): نوشتن دوبارهٔ duty یعنی re-arm - پارک و لچ
                       تریپ پاک می‌شوند و درایو دستی پایین همان پاس duty
                       فرمان‌شده را اعمال می‌کند. شمارش تریپ دست نمی‌خورد،
                       پس سومین تریپ همچنان FINAL_FAULT را قفل می‌کند. */
                    BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] = false;
                    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].uint32_t__retryDeadlineTick = 0u;
                    if (UINT8_T__G__RetryChannel == uint8_t__channelIndex)
                    {
                        UINT8_T__G__RetryChannel = CHG_NO_CHANNEL;
                    }
#if MODULE_JITTER
                    func__Jitter_ClearChannel((uint8_t)(uint8_t__channelIndex + 1u));
#endif
                    CHARGER_CHANNEL_T__G__State[uint8_t__channelIndex].charger_state_t__state =
                        CHG_STATE_MANUAL;
                }
                else
                {
                    continue;
                }
            }

            if (BOOL__G__ChargerManualModeActive != false)
            {
                /* [EN] Manual test mode owns the channel: commanded duty,
                   battery gates bypassed, hardware floor only.
                   [FA] مود تست دستی مالک کانال است: duty فرمان‌شده، گیت‌های
                   باتری رد شده، فقط کف سخت‌افزاری. */
                func__Charger_ManualDriveChannel(uint8_t__channelIndex,
                                                 measurement_snapshot_t__snap);
            }
            else if ((CHG_TRANSFORMER_KNOWN == 0u) && (CHG_BRINGUP_TEST_ENABLE != 0u))
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

/* ==================== Charger runtime config API (ESP panel) ==================== */

/**
 * @brief  [EN] Set the runtime conversion factor of one channel, clamped
 *              to 0..999 permille (v1.3): 0 = identity bypass (compiled
 *              default), non-zero = the live iest = I x Vin x eta /
 *              (1000 x Vbat) conversion. Channel 0 = charger 1 (upper
 *              battery), channel 1 = charger 2 (lower battery). RAM only -
 *              a reboot restores the compiled defaults. Written by the ESP
 *              panel (params 9/10) and by the CAL_REFERENCE command (user
 *              order 2026-09-24).
 *         [FA] ضریب تبدیل یک کانال در زمان اجرا، گیرهٔ ۰..۹۹۹ پرمیل (v1.3):
 *              صفر = همانی/گذر (پیش‌فرض کامپایل)، غیرصفر = تبدیل زندهٔ
 *              iest = I × Vin × η ÷ (۱۰۰۰ × Vbat). کانال ۰ = شارژر ۱ (باتری
 *              بالا) و کانال ۱ = شارژر ۲ (باتری پایین). فقط RAM - ری‌استارت
 *              به پیش‌فرض کامپایل برمی‌گرداند. نوشته از پنل ESP (پارامتر
 *              ۹/۱۰) و از فرمان CAL_REFERENCE (دستور کاربر ۲۰۲۶-۰۹-۲۴).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__etaPermille [EN] Requested efficiency / بازدهی درخواستی
 * @return uint32_t [EN] Applied efficiency permille / بازدهی اعمال‌شده
 */
uint32_t func__Charger_SetEfficiencyPermille(uint8_t uint8_t__channelIndex,
                                             uint32_t uint32_t__etaPermille)
{
    if (uint32_t__etaPermille < CHG_ETA_MIN_PERMILLE)
    {
        uint32_t__etaPermille = CHG_ETA_MIN_PERMILLE;
    }
    else if (uint32_t__etaPermille > CHG_ETA_MAX_PERMILLE)
    {
        uint32_t__etaPermille = CHG_ETA_MAX_PERMILLE;
    }
    else
    {
        /* [EN] Value already inside the window. [FA] مقدار داخل پنجره است. */
    }

    if (uint8_t__channelIndex == 0u)
    {
        UINT32_T__G__ChargerEta1Permille = uint32_t__etaPermille;
    }
    else
    {
        UINT32_T__G__ChargerEta2Permille = uint32_t__etaPermille;
    }

    return uint32_t__etaPermille;
}

/**
 * @brief  [EN] Read the live flyback efficiency of one channel.
 *         [FA] بازدهی flyback زندهٔ یک کانال را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Live efficiency permille / بازدهی زندهٔ پرمیل
 */
uint32_t func__Charger_GetEfficiencyPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex == 0u)
    {
        return UINT32_T__G__ChargerEta1Permille;
    }

    return UINT32_T__G__ChargerEta2Permille;
}

/**
 * @brief  [EN] Set the ESP enable gate of one charger channel. false = cut:
 *              PWM forced off and the state held at OFF (a latched
 *              FINAL_FAULT is never released by this gate); true = reconnect
 *              with the normal soft BULK restart from 1% duty. RAM only -
 *              a reboot restores both channels enabled (ESP panel, user
 *              order 2026-09-22).
 *         [FA] گیت فعال‌سازی ESP یک کانال شارژر. false = قطع: PWM قطع و
 *              وضعیت روی OFF نگه داشته می‌شود (قفل FINAL_FAULT هرگز با این
 *              گیت آزاد نمی‌شود)؛ true = وصل با ری‌استارت نرم BULK از duty
 *              ۱٪. فقط RAM - ری‌استارت هر دو کانال را فعال برمی‌گرداند
 *              (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  bool__enable [EN] true = channel allowed / کانال آزاد
 */
void func__Charger_SetChannelEspEnable(uint8_t uint8_t__channelIndex, bool bool__enable)
{
    if (uint8_t__channelIndex < 2u)
    {
        BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex] = bool__enable;
    }
}

/**
 * @brief  [EN] Read the ESP enable gate of one charger channel.
 *         [FA] گیت فعال‌سازی ESP یک کانال شارژر را می‌خواند.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true = channel allowed / کانال آزاد
 */
bool func__Charger_GetChannelEspEnable(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return BOOL__G__ChargerEspEnableCh[uint8_t__channelIndex];
    }

    return true;
}

/* ==================== Manual test mode API / API مود تست دستی ==================== */

/* ==================== Charge Profile (user order 2026-09-25) ==================== */

/* [EN] Interdependency clamps: after ANY profile write the whole set is
 *      re-clamped so it stays physically consistent. The hard safety stack
 *      (CHG_CURRENT_HARD_FAULT_MA 950, CHG_MAX_VALID_BATTERY_MV 15000, the
 *      15.0 V hardware cutoff) stays compile-time and can NOT be raised
 *      from the panel - ABSORB tops out at 14.6 V and the current band at
 *      900 mA (limit = band + 25 < 950).
 * [FA] گیره‌های وابستگی: بعد از هر نوشتن، کل مجموعه دوباره گیره می‌خورد تا
 *      فیزیکیِ سازنده بماند. پشتهٔ ایمنی سخت (۹۵۰mA خطای سخت، ۱۵٫۰V سقف
 *      اعتبار باتری، قطع سخت‌افزاری ۱۵V) کامپایل‌تایم می‌ماند و از پنل
 *      بالا بردنی نیست - ابزورب حداکثر ۱۴٫۶V و باند جریان حداکثر ۹۰۰mA
 *      (سقف = باند + ۲۵ < ۹۵۰). */
static void func__Charger_ClampProfile(void)
{
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv < 11000u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv = 11000u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv > 14600u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv = 14600u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv <
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 500u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 500u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 50u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 50u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv <
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 100u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 100u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 400u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv + 400u;
    }
    /* [EN] Keep the coarse-step ceiling 50 mV under the 14.8 V
            battery-disconnect fault so regulation always acts before the
            fault does (absorb <= 14600 keeps this range non-empty).
       [FA] سقف کاهش سریع را ۵۰mV زیر خطای قطع باتری ۱۴٫۸V نگه می‌داریم
            تا تنظیم همیشه قبل از خطا عمل کند (ابزورب ≤ ۱۴۶۰۰ این بازه را
            تهی نمی‌کند). */
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv > 14750u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv = 14750u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv < 9000u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv = 9000u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 300u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv - 300u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv < 8000u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv = 8000u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv >
        (CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv - 300u))
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv =
            CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv - 300u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa < 100u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa = 100u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa > 900u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa = 900u;
    }

    if (CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa < 10u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa = 10u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa > 300u)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa = 300u;
    }
    if (CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa >
        CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa)
    {
        CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa =
            CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa;
    }
}

bool func__Charger_SetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t uint32_t__value,
                                   uint32_t *uint32_t__appliedValue)
{
    switch (uint8_t__paramId)
    {
        case CHG_PROFILE_PARAM_ABSORB_MV:
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv = uint32_t__value;
            break;
        case CHG_PROFILE_PARAM_ABSORB_ENTER_MV:
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv = uint32_t__value;
            break;
        case CHG_PROFILE_PARAM_ABSORB_OVER_MV:
            CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv = uint32_t__value;
            break;
        case CHG_PROFILE_PARAM_FLOAT_MV:
            CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv = uint32_t__value;
            break;
        case CHG_PROFILE_PARAM_REENTRY_MV:
            CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv = uint32_t__value;
            break;
        case CHG_PROFILE_PARAM_BULK_CURRENT_MAX_MA:
            CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa = uint32_t__value;
            break;
        case CHG_PROFILE_PARAM_TAPER_CURRENT_MA:
            CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa = uint32_t__value;
            break;
        default:
            return false;
    }

    func__Charger_ClampProfile();
    return func__Charger_GetProfileParam(uint8_t__paramId, uint32_t__appliedValue);
}

bool func__Charger_GetProfileParam(uint8_t uint8_t__paramId,
                                   uint32_t *uint32_t__value)
{
    switch (uint8_t__paramId)
    {
        case CHG_PROFILE_PARAM_ABSORB_MV:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbMv;
            return true;
        case CHG_PROFILE_PARAM_ABSORB_ENTER_MV:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbEnterMv;
            return true;
        case CHG_PROFILE_PARAM_ABSORB_OVER_MV:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__absorbOverMv;
            return true;
        case CHG_PROFILE_PARAM_FLOAT_MV:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__floatMv;
            return true;
        case CHG_PROFILE_PARAM_REENTRY_MV:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__reentryMv;
            return true;
        case CHG_PROFILE_PARAM_BULK_CURRENT_MAX_MA:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__bulkCurrentMaxMa;
            return true;
        case CHG_PROFILE_PARAM_TAPER_CURRENT_MA:
            *uint32_t__value = CHARGER_PROFILE_T__G__Profile.uint32_t__taperCurrentMa;
            return true;
        default:
            return false;
    }
}

void func__Charger_SetManualTestMode(bool bool__enable)
{
    BOOL__G__ChargerManualModeRequested = (bool__enable != false);
}

bool func__Charger_GetManualTestMode(void)
{
    return BOOL__G__ChargerManualModeRequested;
}

bool func__Charger_IsManualTestModeActive(void)
{
    return BOOL__G__ChargerManualModeActive;
}

void func__Charger_NotifyEspLinkActivity(void)
{
    UINT32_T__G__ManualLastLinkTick = osKernelGetTickCount();
}

/**
 * @brief  [EN] Set the runtime PWM duty ceiling of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE. Every applied duty (ramp,
 *              regulation, fixed mode) is clamped to min(compile max, this
 *              ceiling) inside ApplyDuty. RAM only - a reboot restores
 *              CHG_DUTY_MAX_PERMILLE (ESP panel, user order 2026-09-22).
 *         [FA] سقف duty ی PWM یک کانال در زمان اجرا، گیرهٔ
 *              ۰..CHG_DUTY_MAX_PERMILLE. هر duty اعمالی (رمپ، تنظیم، مود
 *              فیکس) داخل ApplyDuty به کمینهٔ سقف کامپایل و این سقف گیره
 *              می‌خورد. فقط RAM - ری‌استارت CHG_DUTY_MAX_PERMILLE را
 *              برمی‌گرداند (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__ceilingPermille [EN] Requested ceiling / سقف درخواستی
 * @return uint32_t [EN] Applied ceiling / سقف اعمال‌شده
 */
uint32_t func__Charger_SetDutyCeilingPermille(uint8_t uint8_t__channelIndex,
                                              uint32_t uint32_t__ceilingPermille)
{
    if (uint32_t__ceilingPermille > CHG_DUTY_MAX_PERMILLE)
    {
        uint32_t__ceilingPermille = CHG_DUTY_MAX_PERMILLE;
    }

    if (uint8_t__channelIndex < 2u)
    {
        UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex] =
            uint32_t__ceilingPermille;
    }

    return uint32_t__ceilingPermille;
}

/**
 * @brief  [EN] Read the live PWM duty ceiling of one channel.
 *         [FA] سقف زندهٔ duty ی PWM یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Ceiling permille / سقف پرمیل
 */
uint32_t func__Charger_GetDutyCeilingPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return UINT32_T__G__ChargerDutyCeilingPermille[uint8_t__channelIndex];
    }

    return CHG_DUTY_MAX_PERMILLE;
}

/**
 * @brief  [EN] Set the runtime fixed-duty mode of one channel. While
 *              enabled, the channel holds its fixed duty instead of the
 *              regulation loop, with the bench-test safety wrapper
 *              (switching stops above CHG_ABSORB_MV; JIT/input/battery/ESP
 *              cuts stay active). The duty value itself is set with
 *              SetDutyFixedPermille and clamped on apply. RAM only (ESP
 *              panel, user order 2026-09-22).
 *         [FA] مود duty فیکس یک کانال در زمان اجرا. تا وقتی فعال است
 *              کانال به‌جای حلقهٔ تنظیم، duty فیکس خود را نگه می‌دارد با
 *              همان پوشش امنیتی تست بنچ (توقف سوئیچینگ بالای
 *              CHG_ABSORB_MV؛ حفاظت‌های JIT/ورودی/باتری/ESP فعال).
 *              خودِ عدد duty با SetDutyFixedPermille تنظیم و موقع اعمال
 *              گیره می‌خورد. فقط RAM (پنل ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  bool__enable [EN] true = fixed mode on / مود فیکس روشن
 */
void func__Charger_SetDutyFixedEnable(uint8_t uint8_t__channelIndex, bool bool__enable)
{
    if (uint8_t__channelIndex < 2u)
    {
        BOOL__G__ChargerDutyFixedEnable[uint8_t__channelIndex] = bool__enable;
    }
}

/**
 * @brief  [EN] Read the runtime fixed-duty switch of one channel.
 *         [FA] کلید مود duty فیکس یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return bool [EN] true = fixed mode on / مود فیکس روشن
 */
bool func__Charger_GetDutyFixedEnable(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return BOOL__G__ChargerDutyFixedEnable[uint8_t__channelIndex];
    }

    return false;
}

/**
 * @brief  [EN] Set the fixed duty value of one channel, clamped to
 *              0..CHG_DUTY_MAX_PERMILLE. Takes effect only while the
 *              channel's fixed mode is enabled; ApplyDuty additionally
 *              respects the runtime ceiling. RAM only (ESP panel, user
 *              order 2026-09-22).
 *         [FA] مقدار duty فیکس یک کانال، گیرهٔ ۰..CHG_DUTY_MAX_PERMILLE.
 *              فقط وقتی مود فیکس همان کانال روشن است اثر دارد؛ ApplyDuty
 *              به‌علاوه سقف زمان اجرا را رعایت می‌کند. فقط RAM (پنل
 *              ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲).
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @param  uint32_t__dutyPermille [EN] Requested duty / duty درخواستی
 * @return uint32_t [EN] Applied stored value / مقدار ذخیره‌شده
 */
uint32_t func__Charger_SetDutyFixedPermille(uint8_t uint8_t__channelIndex,
                                            uint32_t uint32_t__dutyPermille)
{
    if (uint32_t__dutyPermille > CHG_DUTY_MAX_PERMILLE)
    {
        uint32_t__dutyPermille = CHG_DUTY_MAX_PERMILLE;
    }

    if (uint8_t__channelIndex < 2u)
    {
        UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex] =
            uint32_t__dutyPermille;
        if (BOOL__G__ChargerManualModeActive != false)
        {
            /* [EN] Manual test mode: a fresh duty write is the re-arm
               gesture for a JIT-parked channel (protocol v1.2, section
               5.2); the charger task consumes the request.
               [FA] مود تست دستی: نوشتن دوبارهٔ duty حرکتِ مسلح‌کردنِ
               کانالِ پارک‌شده در JIT است (پروتکل v1.2 بخش 5.2)؛ تسک
               شارژر درخواست را مصرف می‌کند. */
            BOOL__G__ChargerManualRearmRequest[uint8_t__channelIndex] = true;
        }
    }

    return uint32_t__dutyPermille;
}

/**
 * @brief  [EN] Read the stored fixed duty value of one channel.
 *         [FA] مقدار ذخیره‌شدهٔ duty فیکس یک کانال.
 * @param  uint8_t__channelIndex [EN] 0 = charger 1, 1 = charger 2 / ۰ یا ۱
 * @return uint32_t [EN] Duty permille / duty پرمیل
 */
uint32_t func__Charger_GetDutyFixedPermille(uint8_t uint8_t__channelIndex)
{
    if (uint8_t__channelIndex < 2u)
    {
        return UINT32_T__G__ChargerDutyFixedPermille[uint8_t__channelIndex];
    }

    return 0u;
}
