/**
 * @file    app.c
 * @brief   [EN] Starts the application through the CMSIS-RTOS2 boundary.
 *          [FA] برنامه را از مرز ‎CMSIS-RTOS2‎ راه‌اندازی می‌کند.
 */

#include "app.h"
#include "rtos_app.h"
#include "modules_enable.h"
#include "esp_link_nvm.h"
#include "cal_lut.h"

/**
 * @brief  [EN] Application initialization hook; thread-owned module init runs in its thread.
 *         [FA] نقطهٔ مقداردهی اولیهٔ برنامه؛ Init ماژول صاحب تسک داخل همان تسک انجام می‌شود.
 */
void func__App_Init(void)
{
#if MODULE_ESP
    /* [EN] v1.14 (user order 2026-09-25: panel values must survive power
       loss): replay the newest CRC-valid flash record through the clamped
       parameter setters BEFORE the scheduler starts. The module Init
       functions that run later inside their own threads only reset channel
       state and measurement outputs - never the settable statics - so the
       loaded values survive them. A missing or corrupt record changes
       nothing (compiled defaults stay).
       [FA] v1.14 (دستور کاربر: مقادیر پنل با قطع برق باید بمانند):
       بازپخش تازه‌ترین رکورد سالمِ CRC فلش از setterهای گیره‌دار «قبل از
       راه‌افتادن زمان‌بند». Initهای ماژول که بعداً داخل تسک خودشان اجرا
       می‌شوند فقط وضعیت کانال و خروجی‌های اندازه‌گیری را ریست می‌کنند -
       هرگز staticهای قابل‌تنظیم را نه - پس مقادیر بارگذاری‌شده از آنها
       جان سالم به در می‌برند. رکورد گم یا خراب هیچ چیزی را عوض نمی‌کند
       (پیش‌فرض کامپایل می‌ماند). */
    func__EspLink_NvmInit();
    /* [EN] v1.66 (user order 2026-10-05: push the calibration table straight
       into the micro, on its OWN storage path): load the bench LUT from its
       dedicated flash block. No valid record = the compiled tables in
       calibration.h stay in charge, exactly as before.
       [FA] v1.66 (دستور کاربر: جدول مستقیم به میکرو، روی «مسیر ذخیره‌سازی
       جدا»): جدول بنچ از بلوک فلش اختصاصی‌اش بار می‌شود. نبودِ رکورد معتبر
       یعنی همان جدول‌های کامپایل‌شده سر کارند. */
    func__CalLut_Init();
#endif
}

/**
 * @brief  [EN] Initialize the CMSIS-RTOS2 application and start its threads.
 *         [FA] برنامهٔ ‎CMSIS-RTOS2‎ را مقداردهی و تسک‌های آن را شروع می‌کند.
 */
void func__App_Start(void)
{
    func__App_Init();
    func__Rtos_Start();
}
