/**
 * @file    changeover.c
 * @brief   [EN] Input vs battery path state machine - timed cut/reconnect with 3000ms filtering.
 *          [FA] ماشین حالت مسیر ورودی یا باتری - قطع/وصل با فیلتر ۳ ثانیه.
 *
 * @note    [EN] This module uses ONLY: snapshot.valid, snapshot.v_bat24_mv,
 *              snapshot.input_present, fault_mask (the low-battery latch and the
 *              healthy-voltage hold window are internal).
 *              No board_pins.h, no HAL, no PB5/PB7, no float/queue/task.
 *              Time conversion ONLY via rtos_time.h (MillisecondsToTicks); tick=1ms assumption is forbidden.
 *              PB11 (BSP_GPIO_PROTECT_BATTERY) is the ONLY logical pin used.
 *          [FA] فقط از داده‌های مجاز بالا استفاده می‌کند و فقط PB11 منطقی را می‌زند.
 */

#include "changeover.h"
#include "bsp_gpio.h"
#include "rtos_time.h"
#include "cmsis_os2.h"
#include "modules_enable.h"

#if MODULE_IMBALANCE
#include "imbalance.h"
#endif

#if MODULE_CHARGER
#include "charger.h"
#endif

#include <stdbool.h>
#include <stdint.h>

/* ==================== Static State / وضعیت داخلی ==================== */

/**
 * @brief  [EN] Current system state.
 *         [FA] حالت فعلی سیستم.
 */
static app_state_t APP_STATE_T__G__State = APP_STATE_BOOT;

/**
 * @brief  [EN] Logical PB11 protect state: true = battery path cut (protect asserted).
 *         [FA] وضعیت منطقی PB11: true یعنی مسیر باتری قطع شده است.
 */
static bool BOOL__G__ChangeoverProtectAsserted = false;

/**
 * @brief [EN] Boot stays protected until a valid sample passes blocking checks.
 *        [FA] بوت تا عبور نمونهٔ معتبر از بررسی‌های مسدودکننده در حفاظت می‌ماند.
 */
static bool BOOL__G__BootPending = true;

/**
 * @brief  [EN] Tick at which the current cut condition became continuously true.
 *         [FA] تیکی که در آن شرط قطع به‌صورت پیوسته true شد.
 */
static uint32_t UINT32_T__G__CutStartTick = 0u;

/**
 * @brief  [EN] Whether a cut condition is currently being timed.
 *         [FA] آیا شرط قطع در حال زمان‌گیری است.
 */
static bool BOOL__G__CutTimerActive = false;

/**
 * @brief  [EN] Internal low-battery latch (v1.74, was the UI flag + link params 72/73).
 *         Set while v_bat24_mv < CHANGEOVER_BAT_LOW_ALARM_CUT_MV, cleared at or above
 *         CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV, held in between so the gated cut sees a
 *         continuous level and not a chattering edge.
 *         [FA] قفل داخلی باتری کم: زیر ۲۱۰۰۰ میلی‌ولت بسته، در ۲۱۲۰۰ و بالاتر باز،
 *         و در میان این دو حفظ می‌شود تا قطعِ نرم لرزش نگیرد.
 */
static bool BOOL__G__LowBatteryLatched = false;

/**
 * @brief  [EN] Tick at which the reconnect condition became continuously true.
 *         [FA] تیکی که در آن شرط وصل مجدد به‌صورت پیوسته true شد.
 */
static uint32_t UINT32_T__G__ReconnectStartTick = 0u;

/**
 * @brief  [EN] Whether a reconnect condition is currently being timed.
 *         [FA] آیا شرط وصل مجدد در حال زمان‌گیری است.
 */
static bool BOOL__G__ReconnectTimerActive = false;

/**
 * @brief  [EN] Input stable tracking for 30s extra delay before reconnect.
 *         [FA] ردیابی پایداری ورودی برای تاخیر ۳۰ ثانیه اضافه قبل از وصل مجدد.
 */
static uint32_t UINT32_T__G__InputStableSinceTick = 0u;
static bool BOOL__G__InputStableActive = false;

/**
 * @brief  [EN] Continuous healthy-voltage window for the mains-free recovery
 *         rule: true while v_bat24_mv has stayed at or above
 *         CHANGEOVER_BAT_RECONNECT_MV with no break, plus the tick that window
 *         started. One dip restarts it.
 *         [FA] پنجرهٔ پیوستهٔ ولتاژ سالم برای بازیابی بدون برق شهر: تا وقتی
 *         ولتاژ پک بدون وقفه در آستانهٔ وصل یا بالاتر مانده باشد true است،
 *         به‌همراه تیکی که پنجره شروع شده. یک افت، پنجره را از صفر شروع می‌کند.
 */
static uint32_t UINT32_T__G__HealthySinceTick = 0u;
static bool BOOL__G__HealthyActive = false;

/**
 * @brief  [EN] Mains-free reconnect bookkeeping and cycle guard: tick and flag
 *         of the last successful mains-free reconnect, plus the latch that
 *         stops repeating it when the pack cuts again inside
 *         CHANGEOVER_CYCLE_GUARD_MS. The latch clears when the mains returns.
 *         [FA] سابقهٔ وصل بدون برق شهر و محافظ چرخه: تیک و پرچم آخرین وصلِ
 *         موفق بدون برق شهر، به‌علاوهٔ قفلی که اگر باتری ظرف مدت محافظ دوباره
 *         قطع شود از تکرار آن جلوگیری می‌کند. با برگشت برق شهر پاک می‌شود.
 */
static uint32_t UINT32_T__G__MainsFreeReconnectTick = 0u;
static bool BOOL__G__MainsFreeReconnectDone = false;
static bool BOOL__G__MainsFreeReconnectBlocked = false;

/* ==================== Changeover_Init / مقداردهی اولیه ==================== */

/**
 * @brief  [EN] Start protected in BOOT; connect only after valid measurements
 *              pass blocking checks and the existing battery threshold.
 *         [FA] در BOOT حفاظت را فعال می‌کند؛ پس از عبور نمونهٔ معتبر از بررسی
 *              خطا و رسیدن باتری به آستانهٔ موجود، مسیر را وصل می‌کند.
 */
void func__Changeover_Init(void)
{
    APP_STATE_T__G__State = APP_STATE_BOOT;
    BOOL__G__ChangeoverProtectAsserted = true;
    BOOL__G__BootPending = true;
    UINT32_T__G__CutStartTick = 0u;
    BOOL__G__CutTimerActive = false;
    UINT32_T__G__ReconnectStartTick = 0u;
    BOOL__G__ReconnectTimerActive = false;
    BOOL__G__LowBatteryLatched = false;
    UINT32_T__G__InputStableSinceTick = 0u;
    BOOL__G__InputStableActive = false;
    UINT32_T__G__HealthySinceTick = 0u;
    BOOL__G__HealthyActive = false;
    UINT32_T__G__MainsFreeReconnectTick = 0u;
    BOOL__G__MainsFreeReconnectDone = false;
    BOOL__G__MainsFreeReconnectBlocked = false;

    /* [EN] Keep the path physically cut until the first valid, non-blocked
          sample confirms the pack has reached the 21.2 V hysteresis-clear
          threshold. [FA] مسیر را تا نمونهٔ معتبرِ بدون خطا و رسیدن ولتاژ
          به آستانهٔ پاک‌شدن هیسترزیس (۲۱٫۲ ولت) قطع نگه می‌دارد. */
    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
}

/* ==================== Changeover_Evaluate / ارزیابی ==================== */

/**
 * @brief  [EN] Evaluate next system state from snapshot and faults.
 *         [FA] حالت بعدی سیستم را از نمونه و خطا حساب می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot from measurement, may be NULL / نمونه اندازه‌گیری
 * @param  fault_mask_t__faults [EN] Fault bits from Fault module / بیت‌های خطا
 * @return app_state_t [EN] Next system state / حالت بعدی
 */
app_state_t func__Changeover_Evaluate(const measurement_snapshot_t *measurement_snapshot_t__snap, fault_mask_t fault_mask_t__faults)
{
    uint32_t uint32_t__nowTick;
    uint32_t uint32_t__durationTicks;
    uint32_t uint32_t__healthyHoldTicks;
    bool bool__cutCondition;
    bool bool__reconnectCondition;

    uint32_t__nowTick = osKernelGetTickCount();
    uint32_t__durationTicks = func__Rtos_MillisecondsToTicks(CHANGEOVER_DURATION_MS);
    uint32_t__healthyHoldTicks = func__Rtos_MillisecondsToTicks(CHANGEOVER_HEALTHY_HOLD_MS);

    /* [EN] Snapshot check FIRST before any decision: if NULL or invalid,
          preserve state and PB11, reset pending timers, fault does NOT change state,
          and invalid time is not counted.
       [FA] بررسی snapshot قبل از هر تصمیم: اگر نامعتبر، هیچ تصمیمی نگیر و state و PB11 حفظ شود. */
    if (measurement_snapshot_t__snap == NULL)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    if (measurement_snapshot_t__snap->valid == false)
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] Track input-stability on every valid pass before any early return
          (fault, scenario-5/6 vetoes, or cut branch) so a mains loss/restore
          during a fault or veto is never masked by a frozen pre-fault stamp.
       [FA] ردیابی پایداری ورودی در ابتدای هر پاس معتبر و پیش از هر return
          زودهنگام (فالت، وتوی سناریوهای ۵/۶ یا شاخهٔ قطع) انجام می‌شود تا
          قطع و وصل ورودی حین فالت یا وتو با مهر کهنه پنهان نماند. */
    if (measurement_snapshot_t__snap->input_present == true)
    {
        if (BOOL__G__InputStableActive == false)
        {
            BOOL__G__InputStableActive = true;
            UINT32_T__G__InputStableSinceTick = uint32_t__nowTick;
        }
    }
    else
    {
        BOOL__G__InputStableActive = false;
        UINT32_T__G__InputStableSinceTick = 0u;
    }

    /* [EN] Healthy-pack window for the mains-free recovery rule (user decision
          2026-10-23): the voltage must stay at or above the reconnect
          threshold WITHOUT A BREAK for CHANGEOVER_HEALTHY_HOLD_MS before the
          module will put the battery back on the output with no mains. Any
          dip restarts the window, so a pack that only bounced for a moment
          is not accepted. The absent signal of the Fault module is NOT used
          here: it is a threshold guess, and one cut wire does not reliably
          drive any sense input below its absent level (2026-10-23 order).
          Returning mains clears the cycle guard of the next outage.
       [FA] پنجرهٔ باتری سالم برای قانون بازیابی بدون برق شهر (تصمیم کاربر
          ۲۰۲۶-۱۰-۲۳): ولتاژ باید بدون وقفه به‌مدت ‎CHANGEOVER_HEALTHY_HOLD_MS‎
          در حد آستانهٔ وصل یا بالاتر بماند تا ماژول باتری را بدون برق شهر
          روی خروجی برگرداند. هر افت، پنجره را از صفر شروع می‌کند تا باتری‌ای
          که فقط یک لحظه بالا آمده پذیرفته نشود. سیگنال «نبود باتری» ماژول
          Fault اینجا استفاده نمی‌شود: آن یک حدس آستانه‌ای است و با قطع یک سیم
          هیچ ورودی حس به‌طور مطمئن زیر حد نبود نمی‌رود (دستور ۲۰۲۶-۱۰-۲۳).
          برگشت برق شهر قفل چرخهٔ قطع بعدی را پاک می‌کند. */
    if (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV)
    {
        if (BOOL__G__HealthyActive == false)
        {
            BOOL__G__HealthyActive = true;
            UINT32_T__G__HealthySinceTick = uint32_t__nowTick;
        }
    }
    else
    {
        BOOL__G__HealthyActive = false;
        UINT32_T__G__HealthySinceTick = 0u;
    }

    if (measurement_snapshot_t__snap->input_present == true)
    {
        BOOL__G__MainsFreeReconnectDone = false;
        BOOL__G__MainsFreeReconnectBlocked = false;
    }

    /* [EN] Snapshot is valid from here - use ONLY allowed fields.
          Now handle fault: charger-only faults (FAULT_CHARGER_BAT_LOST /
          FAULT_CHARGER_TECHNICAL) belong to the charger and must NOT disable
          24 V battery-to-load changeover or deep-discharge protection (PB11).
          Only non-charger faults (such as FAULT_ADC) enter APP_STATE_FAULT
          and freeze PB11.
       [FA] از اینجا snapshot معتبر است: خطاهای اختصاصی شارژر
          (‎FAULT_CHARGER_BAT_LOST / FAULT_CHARGER_TECHNICAL)‎ نباید چنج‌اور و
          حفاظت دشارژ عمیق باتری (PB11) را از کار بیندازند؛ فقط خطاهای غیرشارژر
          (مثل FAULT_ADC) وارد APP_STATE_FAULT می‌شوند. */
    if ((fault_mask_t__faults &
         (fault_mask_t)~(FAULT_CHARGER_BAT_LOST | FAULT_CHARGER_TECHNICAL)) != FAULT_NONE)
    {
        APP_STATE_T__G__State = APP_STATE_FAULT;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

#if MODULE_IMBALANCE
    /* [EN] Scenario 5 output veto (user order 2026-10-04): while the
          imbalance verdict is latched AND the block checkbox (param 117) is
          on, the battery must NEVER be switched onto the output - behave
          like the critical cut: assert the protect line, sit in SAFE, reset
          the pending timers. Priority: right after hard faults (a latched
          battery is a degraded asset, above every normal threshold logic).
          Charging is NOT affected here (the charger has its own gate and
          imbalance charging stays allowed until the cycle budget is spent).
       [FA] وتوی خروجی سناریوی ۵: در قفل + تیک ۱۱۷، باتری هرگز روی خروجی
          سوئیچ نمی‌شود؛ مثل قطع بحرانی رفتار می‌کنیم (محافظ فعال + SAFE). */
    {
        imbalance_outputs_t imbalance_outputs_t__imbalance;

        func__Imbalance_GetOutputs(&imbalance_outputs_t__imbalance);
        if (imbalance_outputs_t__imbalance.bool__blockOutput == true)
        {
            if (BOOL__G__ChangeoverProtectAsserted == false)
            {
                func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
                BOOL__G__ChangeoverProtectAsserted = true;
            }
            APP_STATE_T__G__State = APP_STATE_SAFE;
            BOOL__G__CutTimerActive = false;
            BOOL__G__ReconnectTimerActive = false;
            return APP_STATE_T__G__State;
        }
    }
#endif

#if MODULE_CHARGER
    /* [EN] v1.72 scenario 6 output veto (user order: "declare the battery
          faulty ... and charge it no more until the battery is replaced,
          like the imbalance case"): with the dead-battery verdict latched
          AND param 127 on, a condemned pack never reaches the output
          either - same handling as the imbalance veto right above.
       [FA] وتوی خروجی سناریوی ۶: باتری خرابِ قفل‌شده با تیک ۱۲۷ هرگز روی
          خروجی نمی‌رود؛ دقیقاً مثل وتوی عدم‌توازن بالا. */
    if (func__Charger_DeadBlocksOutput() != false)
    {
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
            BOOL__G__ChangeoverProtectAsserted = true;
        }
        APP_STATE_T__G__State = APP_STATE_SAFE;
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }
#endif

    /* [EN] Guard: if Rtos tick conversion yields zero ticks (e.g., tick freq 0),
          do not perform immediate cut/reconnect; preserve state and PB11, reset timers.
       [FA] نگهبان: اگر تبدیل میلی‌ثانیه به تیک صفر شد، هیچ قطع/وصلی فوری انجام نشود. */
    if ((uint32_t__durationTicks == 0u) || (uint32_t__healthyHoldTicks == 0u))
    {
        BOOL__G__CutTimerActive = false;
        BOOL__G__ReconnectTimerActive = false;
        return APP_STATE_T__G__State;
    }

    /* [EN] v1.74: update the internal low-battery latch with hysteresis before
          using it. Below 21000 mV it sets, at or above 21200 mV it clears, and in
          between it holds its previous value.
       [FA] ابتدا قفل داخلی باتری کم با هیسترزیس به‌روز می‌شود. */
    if (measurement_snapshot_t__snap->v_bat24_mv < CHANGEOVER_BAT_LOW_ALARM_CUT_MV)
    {
        BOOL__G__LowBatteryLatched = true;
    }
    else if (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV)
    {
        BOOL__G__LowBatteryLatched = false;
    }
    else
    {
        /* [EN] Hysteresis band 21000..21200 mV: keep the previous latch value.
           [FA] بازهٔ هیسترزیس: مقدار قبلی حفظ می‌شود. */
    }

    /* [EN] Boot connection is distinct from recovery after a runtime
          low-battery cut. Require a valid sample at the existing hysteresis
          clear threshold; the reconnect input-stability delay is not needed
          for first startup. [FA] وصل بوت با بازیابی پس از قطع کمبود باتری
          فرق دارد؛ نمونهٔ معتبر باید به حد پاک‌شدن هیسترزیس برسد و تأخیر
          پایداری ورودیِ وصل مجدد برای شروع اولیه لازم نیست. */
    if (BOOL__G__BootPending == true)
    {
        if (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV)
        {
            func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
            BOOL__G__ChangeoverProtectAsserted = false;
            BOOL__G__BootPending = false;
            APP_STATE_T__G__State =
                (measurement_snapshot_t__snap->input_present == true)
                    ? APP_STATE_INPUT
                    : APP_STATE_BATTERY;
        }
        else
        {
            APP_STATE_T__G__State = APP_STATE_SAFE;
        }
        return APP_STATE_T__G__State;
    }

    /* [EN] Evaluate cut conditions (require continuous 3000ms):
          - gated cut: the internal low-battery latch is set (closes below
            21000 mV, opens only at or above 21200 mV)
          - independent cut: v <20800 independent of the latch

          v1.81 (user order 2026-10-06, after the 2026-10-05 review): the
          gated cut now tests the LATCH ALONE. Before, the second term
          repeated the very comparison that had just set the latch, so the
          21000/21200 hysteresis shaped the latch and nothing else: a pack
          that climbed to 21100 mV on second 2 of the 3 s window simply
          froze the cut timer and kept feeding the load while still weak.
          With the latch as the only term, the countdown keeps running
          until the pack really passes CLEAR_MV (21200) and opens the
          latch. The critical cut below CRITICAL_CUT_MV is untouched, the
          3 s filter is untouched, and reconnect is untouched.
       [FA] نسخهٔ ۱٫۸۱ (دستور کاربر ۲۰۲۶-۱۰-۰۶، پس از بازبینی ۲۰۲۶-۱۰-۰۵):
          قطعِ نرم حالا فقط به «قفل» نگاه می‌کند. قبلاً شرط دوم همان
          مقایسه‌ای را تکرار می‌کرد که یک لحظه پیش قفل را بسته بود، پس
          هیسترزیس ‎21000/21200‎ تنها شکل قفل را می‌ساخت: باتری‌ای که در
          ثانیهٔ دوم به ۲۱۱۰۰ میلی‌ولت می‌رسید، تایمر قطع را می‌خواباند و
          با وجود ضعف همچنان بار را تغذیه می‌کرد. حالا شمارش تا وقتی ولتاژ
          واقعاً از ‎CLEAR_MV‎ (۲۱۲۰۰) رد نشود ادامه دارد. قطع بحرانی زیر
          ‎CRITICAL_CUT_MV‎، فیلتر ۳ ثانیه و مسیر وصل مجدد دست‌نخورده‌اند. */
    bool__cutCondition = false;
    if (measurement_snapshot_t__snap->v_bat24_mv < CHANGEOVER_BAT_CRITICAL_CUT_MV)
    {
        bool__cutCondition = true;
    }
    else if (BOOL__G__LowBatteryLatched == true)
    {
        bool__cutCondition = true;
    }
    else
    {
        bool__cutCondition = false;
    }

    if (bool__cutCondition == true)
    {
        if (BOOL__G__CutTimerActive == false)
        {
            BOOL__G__CutTimerActive = true;
            UINT32_T__G__CutStartTick = uint32_t__nowTick;
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;

            uint32_t__elapsedTicks = uint32_t__nowTick - UINT32_T__G__CutStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
                    BOOL__G__ChangeoverProtectAsserted = true;

                    /* [EN] Cycle guard (user decision 2026-10-23): a cut that
                          follows a mains-free reconnect inside
                          CHANGEOVER_CYCLE_GUARD_MS proves the pack is really
                          weak, so no further mains-free reconnect is allowed
                          until the mains returns.
                       [FA] محافظ چرخه (تصمیم کاربر ۲۰۲۶-۱۰-۲۳): قطعی که ظرف
                          ‎CHANGEOVER_CYCLE_GUARD_MS‎ بعد از یک وصلِ بدون برق
                          شهر رخ دهد یعنی باتری واقعاً ضعیف است؛ پس دیگر بدون
                          برق شهر وصل نمی‌شود تا وقتی برق شهر برگردد. */
                    if ((measurement_snapshot_t__snap->input_present == false) &&
                        (BOOL__G__MainsFreeReconnectDone == true) &&
                        ((uint32_t__nowTick - UINT32_T__G__MainsFreeReconnectTick) <
                         func__Rtos_MillisecondsToTicks(CHANGEOVER_CYCLE_GUARD_MS)))
                    {
                        BOOL__G__MainsFreeReconnectBlocked = true;
                    }
                }
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
            else
            {
                /* [EN] Cut pending but not yet elapsed: keep current state
                      as before cut (BATTERY/INPUT will be set below if not SAFE).
                   [FA] قطع در انتظار: حالت هنوز SAFE نشده. */
                if (BOOL__G__ChangeoverProtectAsserted == false)
                {
                    if (measurement_snapshot_t__snap->input_present == true)
                    {
                        APP_STATE_T__G__State = APP_STATE_INPUT;
                    }
                    else
                    {
                        APP_STATE_T__G__State = APP_STATE_BATTERY;
                    }
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                }
            }
        }
        /* [EN] While cut condition holds, reconnect timer must not run.
           [FA] هنگام شرط قطع، تایمر وصل مجدد نباید فعال باشد. */
        BOOL__G__ReconnectTimerActive = false;

        /* [EN] If cut timer just started (first tick), report current valid state
              (BATTERY/INPUT) until duration completes.
           [FA] در شروع تایمر قطع، حالت معتبر فعلی گزارش شود. */
        if (BOOL__G__ChangeoverProtectAsserted == false)
        {
            if (APP_STATE_T__G__State == APP_STATE_BOOT)
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            else if (APP_STATE_T__G__State == APP_STATE_SAFE)
            {
                /* [EN] Keep SAFE if already cut.
                   [FA] اگر قبلاً SAFE شده، حفظ شود. */
            }
            else
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            /* [EN] If protect already asserted, stay SAFE (already handled above).
               [FA] اگر قبلاً قطع شده، SAFE بماند. */
            if (BOOL__G__ChangeoverProtectAsserted == true)
            {
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
        }

        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__CutTimerActive = false;
    }

    /* [EN] Evaluate reconnect: input_present true AND v >=21200 for 3000ms continuous
          PLUS input stable for CHANGEOVER_INPUT_STABLE_MS (30s) after a cut.
          CONFIRMED 2026-10-06 (user decision): requiring the input is
          DELIBERATE. After a low-battery cut the pack voltage recovers on
          its own once the load is gone; reconnecting without mains would
          just put the load back on a weak pack and start a cut/reconnect
          cycle. The battery stays parked until a real charging source is
          present. Do not "fix" this. User order 2026-10-12: add 30s extra
          stable-input delay before reconnect (time only, not voltage 24V).
          REVISION (user order 2026-10-23): the pack that recovers after the
          cut (or is replaced) may come back with NO mains, but only after the
          healthy-voltage window above has held without a single break; a dip
          restarts it, and one quick repeat cut latches the cycle guard. That
          is the "several seconds in a stable state" test the 2026-10-06
          decision was missing. Mains-present reconnect is unchanged, and the
          3 s voltage filter still runs after the gate opens.
       [FA] وصل مجدد فقط وقتی ورودی حاضر و ولتاژ باتری >=21200 به مدت ۳ ثانیه
          به‌علاوه ورودی ۳۰ ثانیه پایدار. تأییدشده ۲۰۲۶-۱۰-۰۶ (تصمیم کاربر):
          شرطِ حضور ورودی عمدی است. دستور ۲۰۲۶-۱۰-۱۲: تاخیر ۳۰ ثانیه اضافه
          پایداری ورودی قبل از وصل مجدد (فقط زمان، نه ولتاژ ۲۴).
          بازنگری (دستور کاربر ۲۰۲۶-۱۰-۲۳): باتری‌ای که بعد از قطع بازیابی (یا
          تعویض) می‌شود می‌تواند بدون برق شهر برگردد، اما فقط بعد از اینکه
          پنجرهٔ ولتاژ سالم بدون هیچ وقفه‌ای کامل شود؛ هر افت شمارش را از صفر
          می‌کند و یک قطع سریع بعد از وصل، محافظ چرخه را قفل می‌کند. این همان
          آزمون «چند ثانیه ماندن در وضعیت پایدار» است که تصمیم ۲۰۲۶-۱۰-۰۶
          نداشت. مسیر حضور برق شهر دست‌نخورده است و فیلتر ۳ ثانیه‌ای هم بعد از
          بازشدن گیت اجرا می‌شود. */
    bool__reconnectCondition = false;
    {
        bool bool__reconnectGateOpen = false;

        if (measurement_snapshot_t__snap->input_present == true)
        {
            if (BOOL__G__InputStableActive == true)
            {
                uint32_t uint32_t__inputStableElapsed;

                uint32_t__inputStableElapsed = uint32_t__nowTick - UINT32_T__G__InputStableSinceTick;
                if (uint32_t__inputStableElapsed >=
                    func__Rtos_MillisecondsToTicks(CHANGEOVER_INPUT_STABLE_MS))
                {
                    bool__reconnectGateOpen = true;
                }
                else
                {
                    bool__reconnectGateOpen = false;
                }
            }
            else
            {
                bool__reconnectGateOpen = false;
            }
        }

        else if ((BOOL__G__MainsFreeReconnectBlocked == false) &&
                 (BOOL__G__HealthyActive == true) &&
                 ((uint32_t__nowTick - UINT32_T__G__HealthySinceTick) >=
                  uint32_t__healthyHoldTicks))
        {
            /* [EN] No mains, but the pack has held a healthy voltage for the
                  whole hold window: it recovered (or was replaced), so the
                  output may come back. The 3 s filter below still applies.
               [FA] بدون برق شهر، اما پک تمام پنجرهٔ نگهداری را سالم مانده:
                  بازیابی (یا تعویض) شده است، پس خروجی می‌تواند برگردد.
                  فیلتر ۳ ثانیه‌ای پایین همچنان لازم است. */
            bool__reconnectGateOpen = true;
        }

        if ((bool__reconnectGateOpen == true) &&
            (measurement_snapshot_t__snap->v_bat24_mv >= CHANGEOVER_BAT_RECONNECT_MV))
        {
            bool__reconnectCondition = true;
        }
        else
        {
            bool__reconnectCondition = false;
        }
    }

    if (bool__reconnectCondition == true)
    {
        if (BOOL__G__ReconnectTimerActive == false)
        {
            BOOL__G__ReconnectTimerActive = true;
            UINT32_T__G__ReconnectStartTick = uint32_t__nowTick;
            /* [EN] While waiting for the reconnect duration, keep SAFE if
                  already cut, otherwise report the mapped state.
               [FA] در انتظار reconnect، اگر قبلاً قطع شده SAFE بماند و
                  وگرنه حالت نگاشت‌شده گزارش شود. */
            if (BOOL__G__ChangeoverProtectAsserted == true)
            {
                APP_STATE_T__G__State = APP_STATE_SAFE;
            }
            else
            {
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
        }
        else
        {
            uint32_t uint32_t__elapsedTicks;

            uint32_t__elapsedTicks = uint32_t__nowTick - UINT32_T__G__ReconnectStartTick;
            if (uint32_t__elapsedTicks >= uint32_t__durationTicks)
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, false);
                    BOOL__G__ChangeoverProtectAsserted = false;

                    /* [EN] A reconnect with no mains is the one the cycle
                          guard watches: remember when it happened.
                       [FA] وصل بدون برق شهر همانی است که محافظ چرخه زیر نظر
                          دارد؛ زمانش را یادداشت کن. */
                    if (measurement_snapshot_t__snap->input_present == false)
                    {
                        BOOL__G__MainsFreeReconnectDone = true;
                        UINT32_T__G__MainsFreeReconnectTick = uint32_t__nowTick;
                    }
                }
                if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
            }
            else
            {
                if (BOOL__G__ChangeoverProtectAsserted == true)
                {
                    APP_STATE_T__G__State = APP_STATE_SAFE;
                }
                else
                {
                    if (measurement_snapshot_t__snap->input_present == true)
                {
                    APP_STATE_T__G__State = APP_STATE_INPUT;
                }
                else
                {
                    APP_STATE_T__G__State = APP_STATE_BATTERY;
                }
                }
            }
        }
        return APP_STATE_T__G__State;
    }
    else
    {
        BOOL__G__ReconnectTimerActive = false;
    }

    /* [EN] No fault, snapshot valid, no pending cut/reconnect:
          Map state directly from input_present, but preserve SAFE until reconnect.
       [FA] بدون fault و با snapshot معتبر و بدون قطع/وصل در انتظار: از input_present نگاشت شود. */
    if (BOOL__G__ChangeoverProtectAsserted == true)
    {
        APP_STATE_T__G__State = APP_STATE_SAFE;
    }
    else
    {
        if (measurement_snapshot_t__snap->input_present == true)
        {
            APP_STATE_T__G__State = APP_STATE_INPUT;
        }
        else
        {
            APP_STATE_T__G__State = APP_STATE_BATTERY;
        }
    }

    return APP_STATE_T__G__State;
}
