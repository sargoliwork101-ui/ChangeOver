/**
 * @file    mcu_power_path.c
 * @brief   [EN] MCU self-supply path via Q1 (PB5) - owns battery switch independent from Changeover.
 *              PB5 (active-low): true->Low=on, false->High=off. PB11 remains Changeover-only.
 *              Hysteresis: qualify v_in >=22000 for 5000ms → Q1 off (High); reconnect v_in <21500 → Q1 on (Low);
 *              21500..21999 → cancel pending timer, preserve Q1; PB4 falling edge immediate Low in ISR.
 *          [FA] مسیر تغذیه MCU با Q1 (PB5) - مستقل از Changeover. هیسترزیس 22000/21500.
 */

#include "mcu_power_path.h"

/* ==================== Includes ==================== */
#include "modules_enable.h"
#include "bsp_gpio.h"
#include "measurement.h"
#include "rtos_time.h"
#include "cmsis_os2.h"

/* ==================== Constants / ثابت‌ها ==================== */
/* Uses MCU_POWER_INPUT_STABLE_MS, QUALIFY_MV, RECONNECT_MV, HYSTERESIS_MV from header. */

/* ==================== Internal state / وضعیت داخلی ==================== */
/* ISR-safe: accessed from both ISR (OnInputIrq) and task (Run). */
static volatile bool     BOOL__G__McuPowerTimerActive = false;
static volatile uint32_t TICK_T__G__McuPowerStartTick  = 0u;
static volatile bool     BOOL__G__BatteryConnected     = true;

/* ==================== Functions ==================== */

/**
 * @brief  [EN] || func__McuPowerPath_Init || Initialize Q1 to battery-connected (PB5 Low) and reset timer.
 *              Call once before product tasks. Keeps PB11 independent.
 *         [FA] || func__McuPowerPath_Init || Q1 را وصل (PB5 Low) و تایمر را صفر می‌کند.
 */
void func__McuPowerPath_Init(void)
{
    /* [EN] Battery path must be connected at boot so the MCU can run without DC input.
     * [FA] برای بوت بدون ورودی DC، مسیر باتری باید وصل باشد. */
    BOOL__G__BatteryConnected     = true;
    BOOL__G__McuPowerTimerActive = false;
    TICK_T__G__McuPowerStartTick   = 0u;
    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* active-low -> Low = on */
}

/**
 * @brief  [EN] || func__McuPowerPath_OnInputIrq || ISR-safe handler for PB4 edges.
 *              If PB4 indicates input lost, immediately drive PB5 Low and cancel timer.
 *              Only GPIO write + volatile flags, no RTOS/ADC/mutex/delay.
 *         [FA] || func__McuPowerPath_OnInputIrq || تابع امن وقفه برای PB4.
 */
void func__McuPowerPath_OnInputIrq(void)
{
    /* [EN] PB4 is BSP_GPIO_INPUT_24V_PRESENT; level low means input absent.
     * [FA] PB4 حضور ورودی را نشان می‌دهد؛ سطح پایین یعنی قطع ورودی. */
    bool input_present = func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);

    if (!input_present)
    {
        /* [EN] Emergency reconnect: battery must be on immediately, timer cancelled.
         *      Order matters: PB5 Low first, then flag, so even if task preempts after this,
         *      the hardware is already safe.
         * [FA] اتصال اضطراری: باتری فوراً وصل و تایمر لغو می‌شود. اول PB5 سپس پرچم. */
        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* Low = battery on */
        BOOL__G__BatteryConnected     = true;
        BOOL__G__McuPowerTimerActive = false;
        /* [EN] Start tick left as-is; will be re-armed on next qualify.
         * [FA] تیک شروع تغییری نمی‌کند تا با ورودی واجد شرایط بعدی دوباره تنظیم شود. */
    }
    /* [EN] On rising edge (input present) do NOT disconnect here; let Run qualify 5 s and apply hysteresis.
     * [FA] در لبه صعودی قطع نکن؛ Run باید 5 ثانیه و هیسترزیس را بسنجد. */
}

/**
 * @brief  [EN] || func__McuPowerPath_Run || Periodic qualification (~10 ms) for normal delayed disconnect with hysteresis.
 *              Hysteresis bands on v_in (not v_bat): >=22000 → 5s qualify then Q1 off; 21500..21999 → cancel timer, preserve Q1;
 *              <21500 → reconnect Q1 on (Low) and cancel timer. Battery voltage irrelevant while input valid.
 *              No PB11 or Changeover thresholds used here. PB4 falling still handled in ISR immediately.
 *         [FA] || func__McuPowerPath_Run || سنجش دوره‌ای با هیسترزیس برای قطع تأخیری عادی.
 */
void func__McuPowerPath_Run(void)
{
#if !MODULE_MCU_POWER_PATH
    return;
#else
    /* [EN] Determine DC input band via measured bus voltage with hysteresis.
     * [FA] باند ورودی DC را با ولتاژ باس و هیسترزیس تعیین می‌کنیم. */
    bool input_qualify = false;   /* v_in >= QUALIFY (22000) */
    bool input_low     = false;   /* v_in < RECONNECT (21500) */

#if MODULE_MEASUREMENT
    measurement_snapshot_t snap;
    if (func__Measurement_GetSnapshot(&snap) && snap.valid)
    {
        if (snap.v_in_mv >= MCU_POWER_INPUT_QUALIFY_MV)
        {
            input_qualify = true;
        }
        else if (snap.v_in_mv < MCU_POWER_INPUT_RECONNECT_MV)
        {
            input_low = true;
        }
        else
        {
            /* [EN] Hysteresis dead-band 21500..21999: preserve current Q1, cancel pending timer.
             * [FA] ناحیه هیسترزیس 21500..21999: تایمر لغو و وضعیت Q1 حفظ شود. */
            input_qualify = false;
            input_low     = false;
        }
    }
    else
    {
        /* [EN] Snapshot invalid: cannot qualify nor reconnect by voltage; preserve Q1, cancel pending timer.
         *      Timer is not counted while invalid (like Changeover snapshot-first).
         * [FA] snapshot نامعتبر: نه احراز و نه اتصال مجدد ولتاژی؛ Q1 حفظ و تایمر لغو. */
        input_qualify = false;
        input_low     = false;
    }
#else
    /* [EN] Fallback when measurement is off: use PB4 level directly (no voltage hysteresis).
     *      PB4 High → qualify band; PB4 Low → low band (should already be handled in ISR).
     * [FA] اگر اندازه‌گیری خاموش است، از سطح PB4 استفاده می‌کنیم. */
    if (func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT))
    {
        input_qualify = true;
    }
    else
    {
        input_low = true;
    }
#endif

    uint32_t now_tick = osKernelGetTickCount();

    /* ==================== Hysteresis band handling ==================== */
    if (input_qualify)
    {
        /* [EN] Band >=22000: qualify for 5s disconnect.
         * [FA] باند >=22000: واجد شرایط 5 ثانیه برای قطع. */
        if (!BOOL__G__BatteryConnected)
        {
            /* [EN] Already disconnected after prior qualification; stay off, no timer (hysteresis prevents 21.9V reconnect).
             * [FA] قبلاً قطع شده؛ قطع بماند و تایمر نچرخد. */
            BOOL__G__McuPowerTimerActive = false;
            return;
        }

        if (!BOOL__G__McuPowerTimerActive)
        {
            /* [EN] Arrival of qualifying input starts 5 s qualification.
             *      Also covers boot-with-input-present (no missing rising edge).
             * [FA] ورود ورودی واجد شرایط، احراز 5 ثانیه‌ای را آغاز می‌کند. */
            BOOL__G__McuPowerTimerActive = true;
            TICK_T__G__McuPowerStartTick  = now_tick;
        }
        else
        {
            uint32_t elapsed_ticks = now_tick - TICK_T__G__McuPowerStartTick;
            uint32_t elapsed_ms    = func__Rtos_TicksToMilliseconds(elapsed_ticks);

            if (elapsed_ms >= MCU_POWER_INPUT_STABLE_MS)
            {
                /* [EN] Stable 5 s achieved -> disconnect MCU battery path (Q1 off = PB5 High).
                 *      Must not touch PB11/Q17.
                 * [FA] پس از 5 ثانیه پایدار، مسیر باتری MCU را قطع کن (Q1 خاموش = PB5 High). */
                /* [EN] Live PB4 veto (full-program audit 2026-09-26): the
                 *      snapshot above can be stale - if the input died in
                 *      the same pass the 5 s elapsed, disconnecting now
                 *      would brown out the MCU. PB4 is real-time hardware;
                 *      when it reads absent, fall through to the reconnect
                 *      path instead (same as the <21500 band).
                 * [FA] وتوی زندهٔ PB4 (ممیزی کل برنامه): snapshot بالا
                 *      می‌تواند کهنه باشد - اگر ورودی همان پاسی که ۵ ثانیه
                 *      تمام شد مرده باشد، قطع‌کردن الان MCU را بی‌برق
                 *      می‌کند. PB4 سخت‌افزار بلادرنگ است؛ اگر غایب خواند،
                 *      به‌جای قطع به مسیر اتصال مجدد برو (مثل باند ۲۱۵۰۰>). */
                if (func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT) == false)
                {
                    BOOL__G__McuPowerTimerActive = false;
                    if (BOOL__G__BatteryConnected == false)
                    {
                        func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true);
                        BOOL__G__BatteryConnected = true;
                    }
                }
                else
                {
                    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false); /* High = battery off */
                    BOOL__G__BatteryConnected     = false;
                    BOOL__G__McuPowerTimerActive = false;
                }
            }
        }
    }
    else if (input_low)
    {
        /* [EN] Band <21500: reconnect battery (also covers falling PB4 via ISR, but ADC path needs it too).
         *      Cancel pending disconnect timer.
         * [FA] باند <21500: باتری دوباره وصل و تایمر لغو. */
        if (BOOL__G__McuPowerTimerActive)
        {
            BOOL__G__McuPowerTimerActive = false;
        }

        if (!BOOL__G__BatteryConnected)
        {
            func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* Low = battery on */
            BOOL__G__BatteryConnected = true;
        }
        /* [EN] If already connected, just keep it on.
         * [FA] اگر قبلاً وصل بود، وصل بماند. */
    }
    else
    {
        /* [EN] Hysteresis dead-band 21500..21999 or invalid snapshot: cancel pending timer, preserve Q1.
         *      Important: do NOT reconnect merely because voltage fell from 22000 to 21900.
         * [FA] ناحیه مرده هیسترزیس یا snapshot نامعتبر: تایمر لغو و Q1 حفظ؛ 21.9V به‌تنهایی reconnect نکند. */
        if (BOOL__G__McuPowerTimerActive)
        {
            BOOL__G__McuPowerTimerActive = false;
        }
        /* [EN] Battery voltage change alone does not affect Q1 in this band.
         * [FA] تغییر ولتاژ باتری در این باند روی Q1 اثری ندارد. */
    }
#endif
}
