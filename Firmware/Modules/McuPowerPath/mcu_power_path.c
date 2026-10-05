/**
 * @file    mcu_power_path.c
 * @brief   [EN] MCU self-supply path via Q1 (PB5) - owns battery switch independent from Changeover.
 *              PB5 (active-low): true->Low=on, false->High=off. PB11 remains Changeover-only.
 *              Hysteresis: qualify v_in >=22000 for 5000ms → Q1 off (High); reconnect v_in <21500 → Q1 on (Low);
 *              21500..21999 → cancel pending timer, preserve Q1; PB4 falling edge immediate Low in ISR.
 *          [FA] مسیر تغذیه MCU با Q1 (PB5) - مستقل از Changeover. هیسترزیس ‎22000/21500‎.
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
static volatile uint32_t UINT32_T__G__McuPowerStartTick  = 0u;
static volatile bool     BOOL__G__BatteryConnected     = true;

/* ==================== Functions ==================== */

/* ==================== McuPowerPath_Init ==================== */

/**
 * @brief  [EN] Put the MCU self-supply path into its safe boot state: the
 *              battery switch is driven to CONNECTED and the qualification
 *              timer is cleared. Connected is the only correct power-on
 *              state, because the board has to be able to run from the
 *              battery before anyone has proved that the DC input is
 *              present and healthy. Call this once before the product
 *              tasks start. It deliberately never touches PB11, which
 *              belongs to Changeover alone.
 *         [FA] مسیر خودتغذیهٔ MCU را به حالت امن بوت می‌برد: کلید باتری روی
 *              «وصل» رانده می‌شود و تایمر احراز صفر می‌شود. «وصل» تنها حالت
 *              درست روشن‌شدن است، چون برد باید بتواند پیش از آنکه کسی حاضر و
 *              سالم‌بودن ورودی DC را ثابت کند از باتری کار کند. این تابع
 *              یک‌بار پیش از شروع تسک‌های محصول صدا زده می‌شود و عمداً هرگز به
 *              PB11 دست نمی‌زند که فقط مال Changeover است.
 */
void func__McuPowerPath_Init(void)
{
    /* [EN] Battery path must be connected at boot so the MCU can run without DC input.
     * [FA] برای بوت بدون ورودی DC، مسیر باتری باید وصل باشد. */
    BOOL__G__BatteryConnected     = true;
    BOOL__G__McuPowerTimerActive = false;
    UINT32_T__G__McuPowerStartTick   = 0u;
    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, true); /* active-low -> Low = on */
}

/* ==================== McuPowerPath_OnInputIrq ==================== */

/**
 * @brief  [EN] Interrupt-context handler for the PB4 input-present edges.
 *              When PB4 says the DC input is gone, the battery path is
 *              reconnected immediately and any pending disconnect timer is
 *              cancelled - waiting for the next periodic pass could brown
 *              the MCU out. The body is restricted to one GPIO write and
 *              two volatile flags: no RTOS call, no ADC, no mutex and no
 *              delay, so it is safe from an ISR. A rising edge does NOT
 *              disconnect here; that decision needs the 5 s qualification
 *              and the hysteresis band, which only the periodic Run owns.
 *         [FA] مدیریت‌کنندهٔ لبه‌های حضور ورودی روی PB4 در بافت وقفه. وقتی PB4
 *              می‌گوید ورودی DC رفته، مسیر باتری فوراً وصل و هر تایمر قطعِ
 *              در انتظار لغو می‌شود؛ منتظرماندن تا پاس دوره‌ای بعدی می‌تواند
 *              MCU را بی‌برق کند. بدنه به یک نوشتن GPIO و دو پرچم volatile
 *              محدود است: بدون فراخوانی RTOS، بدون ADC، بدون میوتکس و بدون
 *              تأخیر، پس از داخل وقفه امن است. لبهٔ صعودی اینجا قطع نمی‌کند؛
 *              آن تصمیم به احراز ۵ ثانیه و باند هیسترزیس نیاز دارد که فقط
 *              مال Run دوره‌ای است.
 */
void func__McuPowerPath_OnInputIrq(void)
{
    /* [EN] PB4 is BSP_GPIO_INPUT_24V_PRESENT; level low means input absent.
     * [FA] PB4 حضور ورودی را نشان می‌دهد؛ سطح پایین یعنی قطع ورودی. */
    bool bool__inputPresent = func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT);

    if (bool__inputPresent == false)
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

/* ==================== McuPowerPath_Run ==================== */

/**
 * @brief  [EN] Periodic qualification (~10 ms): v_in >= 22000 for 5 s =>
 *         Q1 off; 21500..21999 => cancel timer, preserve Q1; < 21500 =>
 *         Q1 on. Battery voltage is irrelevant while the input is valid;
 *         PB4 falling is still handled in the ISR.
 *         [FA] سنجش دوره‌ای (~۱۰ms) با هیسترزیس: ‎v_in >= 22000‎ برای ۵s ←
 *         قطع؛ ۲۱۵۰۰..۲۱۹۹۹ ← لغو تایمر و حفظ Q1؛ < 21500 ← وصل. ولتاژ
 *         باتری بی‌اثر است و لبهٔ PB4 همچنان در ISR مدیریت می‌شود.
 */
void func__McuPowerPath_Run(void)
{
#if !MODULE_MCU_POWER_PATH
    return;
#else
    /* [EN] Determine DC input band via measured bus voltage with hysteresis.
     * [FA] باند ورودی DC را با ولتاژ باس و هیسترزیس تعیین می‌کنیم. */
    bool bool__inputQualify = false;   /* v_in >= QUALIFY (22000) */
    bool bool__inputLow     = false;   /* v_in < RECONNECT (21500) */

#if MODULE_MEASUREMENT
    measurement_snapshot_t measurement_snapshot_t__snap;
    bool                   bool__snapshotUsable;

    bool__snapshotUsable =
        ((func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) != false) &&
         (measurement_snapshot_t__snap.valid != false));

    if (bool__snapshotUsable != false)
    {
        if (measurement_snapshot_t__snap.v_in_mv >= MCU_POWER_INPUT_QUALIFY_MV)
        {
            bool__inputQualify = true;
        }
        else if (measurement_snapshot_t__snap.v_in_mv < MCU_POWER_INPUT_RECONNECT_MV)
        {
            bool__inputLow = true;
        }
        else
        {
            /* [EN] Hysteresis dead-band 21500..21999: preserve current Q1, cancel pending timer.
             * [FA] ناحیه هیسترزیس ‎21500..21999‎: تایمر لغو و وضعیت Q1 حفظ شود. */
            bool__inputQualify = false;
            bool__inputLow     = false;
        }
    }
    else
    {
        /* [EN] Snapshot invalid: cannot qualify nor reconnect by voltage; preserve Q1, cancel pending timer.
         *      Timer is not counted while invalid (like Changeover snapshot-first).
         * [FA] snapshot نامعتبر: نه احراز و نه اتصال مجدد ولتاژی؛ Q1 حفظ و تایمر لغو. */
        bool__inputQualify = false;
        bool__inputLow     = false;
    }
#else
    /* [EN] Fallback when measurement is off: use PB4 level directly (no voltage hysteresis).
     *      PB4 High → qualify band; PB4 Low → low band (should already be handled in ISR).
     * [FA] اگر اندازه‌گیری خاموش است، از سطح PB4 استفاده می‌کنیم. */
    if (func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT) != false)
    {
        bool__inputQualify = true;
    }
    else
    {
        bool__inputLow = true;
    }
#endif

    uint32_t uint32_t__nowTick = osKernelGetTickCount();

    /* ==================== Hysteresis band handling ==================== */
    if (bool__inputQualify != false)
    {
        /* [EN] Band >=22000: qualify for 5s disconnect.
         * [FA] باند >=22000: واجد شرایط 5 ثانیه برای قطع. */
        if (BOOL__G__BatteryConnected == false)
        {
            /* [EN] Already disconnected after prior qualification; stay off, no timer (hysteresis prevents 21.9V reconnect).
             * [FA] قبلاً قطع شده؛ قطع بماند و تایمر نچرخد. */
            BOOL__G__McuPowerTimerActive = false;
            return;
        }

        if (BOOL__G__McuPowerTimerActive == false)
        {
            /* [EN] Arrival of qualifying input starts 5 s qualification.
             *      Also covers boot-with-input-present (no missing rising edge).
             * [FA] ورود ورودی واجد شرایط، احراز 5 ثانیه‌ای را آغاز می‌کند. */
            BOOL__G__McuPowerTimerActive = true;
            UINT32_T__G__McuPowerStartTick  = uint32_t__nowTick;
        }
        else
        {
            uint32_t uint32_t__elapsedTicks =
                uint32_t__nowTick - UINT32_T__G__McuPowerStartTick;
            uint32_t uint32_t__elapsedMs =
                func__Rtos_TicksToMilliseconds(uint32_t__elapsedTicks);

            if (uint32_t__elapsedMs >= MCU_POWER_INPUT_STABLE_MS)
            {
                /* [EN] Stable 5 s achieved -> disconnect MCU battery path (Q1 off = PB5 High).
                 *      Must not touch PB11/Q17.
                 * [FA] پس از 5 ثانیه پایدار، مسیر باتری MCU را قطع کن (Q1 خاموش = PB5 High). */
                /* [EN] Live PB4 veto: the snapshot can be stale - if the
                 *      input died in the very pass the 5 s elapsed, cutting
                 *      now would brown out the MCU. PB4 is real-time
                 *      hardware: absent => take the reconnect path instead.
                 * [FA] وتوی زندهٔ PB4: snapshot می‌تواند کهنه باشد؛ اگر ورودی
                 *      همان پاسِ پایان ۵s مرده باشد، قطع‌کردن MCU را بی‌برق
                 *      می‌کند. PB4 بلادرنگ است: غایب ← مسیر اتصال مجدد. */
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
    else if (bool__inputLow != false)
    {
        /* [EN] Band <21500: reconnect battery (also covers falling PB4 via ISR, but ADC path needs it too).
         *      Cancel pending disconnect timer.
         * [FA] باند <21500: باتری دوباره وصل و تایمر لغو. */
        if (BOOL__G__McuPowerTimerActive != false)
        {
            BOOL__G__McuPowerTimerActive = false;
        }

        if (BOOL__G__BatteryConnected == false)
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
        if (BOOL__G__McuPowerTimerActive != false)
        {
            BOOL__G__McuPowerTimerActive = false;
        }
        /* [EN] Battery voltage change alone does not affect Q1 in this band.
         * [FA] تغییر ولتاژ باتری در این باند روی Q1 اثری ندارد. */
    }
#endif
}
