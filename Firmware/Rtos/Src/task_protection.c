/**
 * @file    task_protection.c
 * @brief   [EN] CMSIS-RTOS2 protection thread - simple RTOS with osDelay.
 *          [FA] تسک حفاظت ساده RTOS.
 */

#include "rtos_tasks.h"
#include "modules_enable.h"
#include "app_config.h"
#include "app_types.h"
#include "cmsis_os2.h"
#include "rtos_time.h"


#if MODULE_PROTECTION
#include "protection.h"
#include "measurement.h"
#include "bsp_iwdg.h"
#endif

/* ==================== Task Protection ==================== */

/**
 * @brief  [EN] Protection thread entry point - the highest-priority task of
 *              the system, so a trip is evaluated before any lower task can
 *              keep the stage running. Each pass reads a measurement
 *              snapshot, hands it to the Protection module and checks in
 *              with the watchdog. While MODULE_PROTECTION is 0 (the shipped
 *              configuration, enforced by the rule checker) the body
 *              compiles down to the watchdog check-in and the periodic
 *              delay, which keeps the slot alive and the timing honest
 *              without the module having to exist.
 *         [FA] نقطهٔ ورود نخ حفاظت؛ پرالویت‌ترین تسک سیستم، تا تریپ پیش از
 *              آنکه تسک پایین‌تری بتواند استیج را روشن نگه دارد سنجیده شود.
 *              هر پاس یک اسنپ‌شات اندازه‌گیری می‌خواند، به ماژول حفاظت می‌دهد و
 *              به واچ‌داگ اعلام حضور می‌کند. تا وقتی ‎MODULE_PROTECTION‎ صفر است
 *              (همان پیکربندی عرضه‌شده که بررسی‌کنندهٔ قوانین هم آن را الزام
 *              می‌کند) بدنه فقط به اعلام حضور واچ‌داگ و تأخیر دوره‌ای تقلیل
 *              می‌یابد، که بدون نیاز به وجود ماژول، شکاف را زنده و زمان‌بندی را
 *              درست نگه می‌دارد.
 * @param  void_ptr__argument [EN] CMSIS-RTOS2 thread argument, unused here
 *                                and deliberately cast to void /
 *                                آرگومان نخ ‎CMSIS-RTOS2‎ که اینجا استفاده
 *                                نمی‌شود و عمداً به void ریخته می‌شود
 */
void func__TaskProtection(void *void_ptr__argument)
{
    (void)void_ptr__argument;

    for (;;)
    {
#if MODULE_PROTECTION
        {
            measurement_snapshot_t measurement_snapshot_t__snap;

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
            func__Protection_Run(&measurement_snapshot_t__snap);
            func__BspIwdg_CheckIn(BSP_IWDG_SLOT_PROTECTION, osKernelGetTickCount());
        }
        func__Rtos_DelayMilliseconds(APP_CONFIG.protection_period_ms);
#else
        func__Rtos_DelayMilliseconds(1000u);
#endif
    }
}
