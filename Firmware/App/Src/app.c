/**
 * @file    app.c
 * @brief   [EN] Starts the application through the CMSIS-RTOS2 boundary.
 *          [FA] برنامه را از مرز CMSIS-RTOS2 راه‌اندازی می‌کند.
 */

#include "app.h"
#include "rtos_app.h"
#include "modules_enable.h"
#include "esp_link_nvm.h"

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
#endif
}

/**
 * @brief  [EN] Initialize the CMSIS-RTOS2 application and start its threads.
 *         [FA] برنامهٔ CMSIS-RTOS2 را مقداردهی و تسک‌های آن را شروع می‌کند.
 */
void func__App_Start(void)
{
    func__App_Init();
    func__Rtos_Start();
}
