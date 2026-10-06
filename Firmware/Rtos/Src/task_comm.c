/**
 * @file    task_comm.c
 * @brief   [EN] CMSIS-RTOS2 communication thread - simple RTOS with osDelay.
 *          [FA] تسک ارتباط ساده RTOS.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"


#if MODULE_ESP
#include "esp_link.h"
#include "bsp_iwdg.h"
#endif
#if MODULE_MEASUREMENT
#include "measurement.h"
#endif
#if MODULE_FAULT
#include "fault.h"
#endif

/* ==================== Task Comm ==================== */

/**
 * @brief  [EN] Communication thread entry point. It owns the whole link
 *              lifecycle: before the periodic loop it brings up the UART
 *              backend, the frame parser and ESP power exactly once, then
 *              every COMM period it services the receive path, answers the
 *              panel, pushes the telemetry it owes and checks in with the
 *              watchdog so a dead link is visible to the supervisor. The
 *              body is a CMSIS-RTOS2 osDelayUntil cadence, never a busy
 *              wait, and the function never returns - returning from a
 *              thread would be a fault on this port.
 *         [FA] نقطهٔ ورود نخ ارتباطی. چرخهٔ عمر کامل لینک مال اوست: پیش از
 *              حلقهٔ دوره‌ای دقیقاً یک‌بار بک‌اند UART، تجزیه‌گر فریم و تغذیهٔ
 *              ESP را بالا می‌آورد، و بعد در هر دورهٔ COMM مسیر دریافت را
 *              سرویس می‌دهد، به پنل جواب می‌دهد، تله‌متری بدهکار را می‌فرستد و
 *              به واچ‌داگ اعلام حضور می‌کند تا لینک مرده برای ناظر دیده شود.
 *              بدنه آهنگ ‎osDelayUntil‎ استاندارد ‎CMSIS-RTOS2‎ است نه انتظار
 *              مشغول، و تابع هرگز برنمی‌گردد؛ برگشتن از یک نخ در این پورت خطا
 *              حساب می‌شود.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused here
 *                                and deliberately cast to void /
 *                                آرگومان نخ ‎CMSIS-RTOS2‎ که اینجا استفاده
 *                                نمی‌شود و عمداً به void ریخته می‌شود
 */
void func__TaskComm(void *void_ptr__argument)
{
    (void)void_ptr__argument;

#if MODULE_ESP
    /* [EN] The comm thread owns the link lifecycle: bring the UART backend,
       parser and ESP power up once before the periodic loop (ESP panel,
       user order 2026-09-22).
       [FA] تسک ارتباط مالک چرخهٔ حیات لینک است: قبل از حلقهٔ دوره‌ای،
       backend ی UART و پارسر و تغذیهٔ ESP را یک‌بار بالا می‌آورد (پنل
       ESP، دستور کاربر ۲۰۲۶-۰۹-۲۲). */
    func__EspLink_Init();
#endif

    for (;;)
    {
#if MODULE_ESP
        {
            measurement_snapshot_t measurement_snapshot_t__snap;
            fault_mask_t fault_mask_t__faults = FAULT_NONE;

            /* [EN] Full-program audit 2026-10-05: only .valid used to be
               initialized here, so the other eight fields of this automatic
               struct held stack garbage whenever the measurement module is
               compiled out or the getter declines. Every consumer does gate
               on .valid, so nothing was observably broken - but handing a
               partly-indeterminate struct across a module boundary is the
               kind of latent defect that only shows up after someone adds a
               reader that trusts the payload. Zeroing every field costs a
               handful of stores per pass and makes the contract true by
               construction. task_ui.c already did exactly this.
               [FA] ممیزی ۲۰۲۶-۱۰-۰۵: اینجا فقط ‎.valid‎ مقداردهی می‌شد و هشت
               فیلد دیگرِ این ساختار خودکار، هر وقت ماژول اندازه‌گیری کامپایل
               نشده باشد یا گیرنده جواب ندهد، زبالهٔ پشته داشتند. همهٔ
               مصرف‌کننده‌ها روی ‎.valid‎ گیت می‌زنند، پس چیزی عملاً خراب نبود؛
               اما ردکردن ساختار نیمه‌نامعین از مرز ماژول همان نقص نهفته‌ای است
               که تازه وقتی کسی خواننده‌ای اضافه کند که به محتوا اعتماد دارد
               خودش را نشان می‌دهد. صفرکردن همهٔ فیلدها چند ذخیره در هر پاس
               هزینه دارد و قرارداد را ذاتاً درست می‌کند. ‎task_ui.c‎ از قبل
               دقیقاً همین کار را می‌کرد. */
            measurement_snapshot_t__snap.v_in_mv       = 0u;
            measurement_snapshot_t__snap.v_bat24_mv    = 0u;
            measurement_snapshot_t__snap.v_bat12_mv    = 0u;
            measurement_snapshot_t__snap.v_bat_low_mv  = 0u;
            measurement_snapshot_t__snap.v_bat_high_mv = 0u;
            measurement_snapshot_t__snap.i_ch1_ma      = 0u;
            measurement_snapshot_t__snap.i_ch2_ma      = 0u;
            measurement_snapshot_t__snap.input_present = false;
            measurement_snapshot_t__snap.valid         = false;
#if MODULE_MEASUREMENT
            (void)func__Measurement_GetSnapshot(&measurement_snapshot_t__snap);
#endif
#if MODULE_FAULT
            fault_mask_t__faults = func__Fault_Get();
#endif
            func__EspLink_Run(&measurement_snapshot_t__snap, fault_mask_t__faults);
            func__BspIwdg_CheckIn(BSP_IWDG_SLOT_COMM, osKernelGetTickCount());
        }
        func__Rtos_DelayMilliseconds(APP_CONFIG.comm_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
