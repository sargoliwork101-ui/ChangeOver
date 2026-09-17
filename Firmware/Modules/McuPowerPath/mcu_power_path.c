/**
 * @file    mcu_power_path.c
 * @brief   [EN] MCU self-supply path via Q1 (PB5) - owns battery switch independent from Changeover.
 *              PB5 (active-low): true->Low=on, false->High=off. PB11 remains Changeover-only.
 *              Valid input = v_in_mv >= 22000 for continuous 5000 ms before Q1 disconnects.
 *              PB4 falling edge immediately reconnects Q1 in ISR and cancels any pending timer.
 *          [FA] مسیر تغذیه MCU با Q1 (PB5) - مستقل از Changeover.
 */

#include "mcu_power_path.h"

/* ==================== Includes ==================== */
#include "modules_enable.h"
#include "bsp_gpio.h"
#include "measurement.h"
#include "rtos_time.h"
#include "cmsis_os2.h"

/* ==================== Constants / ثابت‌ها ==================== */
/* Uses MCU_POWER_INPUT_STABLE_MS and MCU_POWER_INPUT_VALID_MV from header. */

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
        /* [EN] Start tick left as-is; will be re-armed on next valid input.
         * [FA] تیک شروع تغییری نمی‌کند تا با ورودی معتبر بعدی دوباره تنظیم شود. */
    }
    /* [EN] On rising edge (input present) do NOT disconnect here; let Run qualify 5 s.
     * [FA] در لبه صعودی قطع نکن؛ Run باید 5 ثانیه را بسنجد. */
}

/**
 * @brief  [EN] || func__McuPowerPath_Run || Periodic qualification (~10 ms) for normal delayed disconnect.
 *              Uses valid v_in_mv >= 22000 for stable 5000 ms. Battery voltage changes do not affect Q1.
 *              No PB11 or Changeover thresholds used here.
 *         [FA] || func__McuPowerPath_Run || سنجش دوره‌ای برای قطع تأخیری عادی.
 */
void func__McuPowerPath_Run(void)
{
#if !MODULE_MCU_POWER_PATH
    return;
#else
    /* [EN] Determine valid DC input via measured bus voltage.
     * [FA] ورودی معتبر را از ولتاژ باس اندازه‌گیری‌شده تعیین می‌کنیم. */
    bool input_valid = false;

#if MODULE_MEASUREMENT
    measurement_snapshot_t snap;
    if (func__Measurement_GetSnapshot(&snap) && snap.valid)
    {
        if (snap.v_in_mv >= MCU_POWER_INPUT_VALID_MV)
        {
            input_valid = true;
        }
    }
#else
    /* [EN] Fallback when measurement is off: use PB4 level directly.
     * [FA] اگر اندازه‌گیری خاموش است، از سطح PB4 استفاده می‌کنیم. */
    input_valid = func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);
#endif

    uint32_t now_tick = osKernelGetTickCount();

    if (input_valid)
    {
        /* [EN] Input is valid; if battery already disconnected, keep it off.
         * [FA] اگر باتری قبلاً قطع شده، قطع بماند. */
        if (!BOOL__G__BatteryConnected)
        {
            /* [EN] Already disconnected after prior qualification; stay off, no timer.
             * [FA] پس از قطع قبلی، خاموش بماند و تایمر نچرخد. */
            BOOL__G__McuPowerTimerActive = false;
            return;
        }

        /* [EN] Battery is still connected and input is valid: run/stabilize timer.
         * [FA] باتری هنوز وصل و ورودی معتبر است: تایمر را اجرا/پایدار کن. */
        if (!BOOL__G__McuPowerTimerActive)
        {
            /* [EN] Arrival of valid input starts 5 s qualification.
             *      Also covers boot-with-input-present (no missing rising edge).
             * [FA] ورود ورودی معتبر، احراز 5 ثانیه‌ای را آغاز می‌کند. */
            BOOL__G__McuPowerTimerActive = true;
            TICK_T__G__McuPowerStartTick  = now_tick;
        }
        else
        {
            /* [EN] Timer is running; check elapsed via rtos_time (no 1-tick=1ms assumption).
             * [FA] تایمر در حال اجراست؛ گذشت زمان را با rtos_time بسنج. */
            uint32_t elapsed_ticks = now_tick - TICK_T__G__McuPowerStartTick;
            uint32_t elapsed_ms    = func__Rtos_TicksToMilliseconds(elapsed_ticks);

            if (elapsed_ms >= MCU_POWER_INPUT_STABLE_MS)
            {
                /* [EN] Stable 5 s achieved -> disconnect MCU battery path (Q1 off = PB5 High).
                 *      Must not touch PB11/Q17.
                 * [FA] پس از 5 ثانیه پایدار، مسیر باتری MCU را قطع کن (Q1 خاموش = PB5 High). */
                func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false); /* High = battery off */
                BOOL__G__BatteryConnected     = false;
                BOOL__G__McuPowerTimerActive = false;
            }
        }
    }
    else
    {
        /* [EN] Input not valid: cancel pending timer; battery must stay/return connected.
         *      Loss-before-5s is normally handled in ISR (immediate Low), but task also
         *      guarantees the battery is reconnected when v_in drops below threshold
         *      even if PB4 stayed high, and cancels any stray timer.
         *      Battery voltage change alone does not trigger this branch when input stays valid.
         * [FA] ورودی نامعتبر: تایمر لغو و باتری وصل می‌ماند/برمی‌گردد. */
        if (BOOL__G__McuPowerTimerActive)
        {
            BOOL__G__McuPowerTimerActive = false;
        }

        if (!BOOL__G__BatteryConnected)
        {
            /* [EN] Battery was off but input is no longer valid: reconnect immediately (task context).
             *      ISR already does this for PB4 low; this covers ADC-low case.
             * [FA] باتری قطع بود اما ورودی نامعتبر شد: فوراً وصل کن. */
            func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* Low = battery on */
            BOOL__G__BatteryConnected = true;
        }
        /* [EN] If already connected, nothing to do.
         * [FA] اگر قبلاً وصل بود، کاری نیست. */
    }
#endif
}
