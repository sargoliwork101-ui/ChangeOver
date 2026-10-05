/**
 * @file    task_control.c
 * @brief   [EN] CMSIS-RTOS2 control thread - simple RTOS with osDelay, readable.
 *          [FA] تسک کنترل ساده RTOS.
 *
 * @note    [EN] When MODULE_CHANGEOVER is enabled this task obtains the real
 *              Measurement snapshot via func__Measurement_GetSnapshot() and
 *              passes it with the fault bits to func__Changeover_Evaluate().
 *              The UI-owned flag BOOL__G__UiBatteryAlarmIssued is global:
 *              UI (task_ui / ui_led) owns and updates it, Changeover reads
 *              it directly - no duplicate flag or API is created here.
 *          [FA] فلگ UI به‌صورت سراسری در اختیار Changeover است؛ API تکراری ساخته نمی‌شود.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"
#include "bsp_iwdg.h"

#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif
#if MODULE_CHANGEOVER
#include "changeover.h"
#include "ui_led.h"
#endif
#if MODULE_CHARGER
#include "charger.h"
#endif
#if MODULE_JITTER
#include "jitter.h"
#endif
#if MODULE_MCU_POWER_PATH
#include "mcu_power_path.h"
#endif
#if MODULE_IMBALANCE
#include "imbalance.h"
#endif
#if (MODULE_IMBALANCE && MODULE_ESP)
#include "esp_link_nvm.h"   /* [EN] persisted-slot dirty marking / علامت‌گذاری اسلات‌ها */
#endif

#if MODULE_IMBALANCE
/* [EN] Scenario 6 feed: the changeover state from the PREVIOUS pass is the
 *      "on battery = discharging" qualifier (one control period of lag at
 *      100 ms is negligible against 30 s stability times).
 * [FA] حالت چنج‌اور پاس قبلی = نیرولهٔ «روی باتری» برای گیت دشارژ. */
static app_state_t APP_STATE_T__G__ImbalancePrevState = APP_STATE_BOOT;
#endif

/* ==================== Task Control ==================== */

void func__TaskControl(void *void_ptr__argument)
{
    (void)void_ptr__argument;

#if MODULE_CHANGEOVER
    /* [EN] Initialize Changeover once before the first evaluation so BOOT,
       timers and the logical protect state are explicit, not only C defaults.
       [FA] Changeover را پیش از اولین ارزیابی یک‌بار مقداردهی کن تا BOOT،
       تایمرها و وضعیت منطقی حفاظت صریح باشند، نه فقط مقدار پیش‌فرض C. */
    func__Changeover_Init();
#endif
#if MODULE_MCU_POWER_PATH
    /* [EN] Initialize MCU battery path Q1 (PB5) so the MCU stays supplied from
       battery at boot. Keeps PB11 independent (Changeover-owned).
       [FA] مسیر باتری MCU با Q1 (PB5) را مقداردهی می‌کند تا در بوت از باتری
       تغذیه شود. PB11 مستقل می‌ماند. */
    func__McuPowerPath_Init();
#endif
#if MODULE_CHARGER
    /* [EN] Charger owns its safe PWM/relay init before the first evaluation.
       [FA] Charger پیش از اولین Evaluate، PWM و رله را در وضعیت امن init می‌کند. */
    func__Charger_Init();
#endif
#if MODULE_JITTER
    /* [EN] Clear stale comparator events before control starts.
       [FA] رویدادهای قدیمی comparator را پیش از شروع کنترل پاک می‌کند. */
    func__Jitter_Init();
#endif
#if MODULE_IMBALANCE
    /* [EN] Scenario 6 defaults; persisted counters arrive through the NVM
       replay (EspLink boot) - both orders are safe by design.
       [FA] پیش‌فرض‌های سناریوی ۶؛ شمارنده‌های ماندگار از پخش NVM می‌آیند. */
    func__Imbalance_Init();
#endif

    for (;;)
    {
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER)
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            fault_mask_t fault_mask_t__faults = FAULT_NONE;
            app_state_t app_state_t__state = APP_STATE_IDLE;

            measurement_snapshot_t__snap.valid = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
#if MODULE_FAULT
            /* [EN] Central battery-lost detection runs first, so the fresh bit
               is already latched/cleared in the mask this pass consumes.
               [FA] تشخیص متمرکز قطع باتری اول اجرا شود تا بیت تازه در همین
               پاس داخل ماسک دیده شود. */
            func__Fault_Evaluate(&measurement_snapshot_t__snap);
            fault_mask_t__faults = func__Fault_Get();
#endif
#if MODULE_JITTER
            func__Jitter_Run();
#endif
#if MODULE_MCU_POWER_PATH
            func__McuPowerPath_Run();
#endif
#if MODULE_IMBALANCE
            /* [EN] Scenario 6 evaluation, BEFORE Changeover so the output
                   veto (latched + checkbox 117) and the charger gate are
                   visible to the consumers in this same pass. Time is
                   ms-from-ticks via CMSIS (no tick=1ms assumption): tick
                   count / tick frequency * 1000 with 64-bit math.
               [FA] ارزیابی سناریوی ۶ قبل از چنج‌اور تا وتوی خروجی و گیت
                   شارژ در همین پاس دیده شوند؛ زمان از فرکانس تیک CMSIS. */
            {
                imbalance_inputs_t  imbalance_inputs_t__imbalanceInputs;
                imbalance_outputs_t imbalance_outputs_t__imbalanceOutputs;
                uint64_t            uint64_t__nowMs64;

                uint64_t__nowMs64 = ((uint64_t)osKernelGetTickCount() *
                                    (uint64_t)1000u) / (uint64_t)osKernelGetTickFreq();

                imbalance_inputs_t__imbalanceInputs.uint32_t__vHighMv = measurement_snapshot_t__snap.v_bat_high_mv;
                imbalance_inputs_t__imbalanceInputs.uint32_t__vLowMv  = measurement_snapshot_t__snap.v_bat_low_mv;
                imbalance_inputs_t__imbalanceInputs.bool__inputPresent = measurement_snapshot_t__snap.input_present;
                imbalance_inputs_t__imbalanceInputs.bool__valid =
                    ((measurement_snapshot_t__snap.valid != false) &&
                     ((fault_mask_t__faults & FAULT_ADC) == 0u));
                imbalance_inputs_t__imbalanceInputs.bool__batAbsent =
                    ((fault_mask_t__faults & FAULT_CHARGER_BAT_LOST) != 0u);
                imbalance_inputs_t__imbalanceInputs.bool__charging =
#if MODULE_CHARGER
                    func__Charger_IsAnyChannelActive();
#else
                    false;
#endif
                /* [EN] v1.72: per-channel state, so imbalance only compares
                   halves that are in the same state.
                   [FA] حالت هر کانال برای مقایسهٔ هم‌حالت. */
                imbalance_inputs_t__imbalanceInputs.bool__chargingCh1 =
#if MODULE_CHARGER
                    func__Charger_IsChannelActive(0u);
#else
                    false;
#endif
                imbalance_inputs_t__imbalanceInputs.bool__chargingCh2 =
#if MODULE_CHARGER
                    func__Charger_IsChannelActive(1u);
#else
                    false;
#endif
                imbalance_inputs_t__imbalanceInputs.bool__onBattery =
                    (APP_STATE_T__G__ImbalancePrevState == APP_STATE_BATTERY);

                if (func__Imbalance_Evaluate(&imbalance_inputs_t__imbalanceInputs,
                                             (uint32_t)uint64_t__nowMs64,
                                             &imbalance_outputs_t__imbalanceOutputs) != false)
                {
#if MODULE_ESP
                    /* [EN] A counter or the latch moved: mark slots 200..202
                       dirty so the debounced NVM save captures them.
                       [FA] تغییر ماندگار: اسلات‌های ۲۰۰..۲۰۲ برای ذخیره علامت‌گذاری. */
                    func__EspLink_NvmMarkDirty(IMBAL_SLOT_EVENTS_ID);
                    func__EspLink_NvmMarkDirty(IMBAL_SLOT_CYCLES_ID);
                    func__EspLink_NvmMarkDirty(IMBAL_SLOT_LATCH_ID);
#else
                    (void)imbalance_outputs_t__imbalanceOutputs;
#endif
                }
                (void)imbalance_outputs_t__imbalanceOutputs;
            }
#endif
#if MODULE_CHANGEOVER
            /* [EN] UI owns BOOL__G__UiBatteryAlarmIssued and updates it in func__Ui_Tick()
               from snapshot.v_bat24_mv. Changeover reads the same global flag directly;
               no extra wiring is needed in this task beyond the snapshot + faults.
               [FA] UI مالک فلگ است و Changeover همان فلگ سراسری را می‌خواند. */
            app_state_t__state = func__Changeover_Evaluate(&measurement_snapshot_t__snap, fault_mask_t__faults);
#endif
#if MODULE_IMBALANCE
            /* [EN] Remember this pass for the next imbalance cycle.
               [FA] حالت این پاس برای چرخهٔ بعدی عدم‌توازن. */
            APP_STATE_T__G__ImbalancePrevState = app_state_t__state;
#endif
#if MODULE_CHARGER
            func__Charger_Evaluate(&measurement_snapshot_t__snap, app_state_t__state);
            /* [EN] v1.72 scenario 7: the dead-battery latch changed, so slot
               203 must reach NVM (survives power cycles exactly like the
               imbalance latch; only a battery swap clears it).
               [FA] قفل باتری خراب تغییر کرد: اسلات ۲۰۳ ذخیره شود. */
            if (func__Charger_DeadTakePersistFlag() != false)
            {
#if MODULE_ESP
                func__EspLink_NvmMarkDirty(CHG_DEAD_SLOT_MASK_ID);
#endif
            }
#endif
            (void)app_state_t__state;
        }
        /* [EN] Watchdog pump (user order 2026-09-27): refresh the IWDG only
           when every supervised task checked in fresh (see bsp_iwdg.h).
           [FA] پمپ واچ‌داگ: فقط وقتی همهٔ تسک‌ها تازه‌اند تازه کن. */
        func__BspIwdg_PollKick(osKernelGetTickCount());
        func__Rtos_DelayMilliseconds(APP_CONFIG.control_period_ms);
#else
        func__BspIwdg_PollKick(osKernelGetTickCount());
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
