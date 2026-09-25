/**
 * @file    bsp_flash.h
 * @brief   [EN] STM32F103C8T6 on-chip flash writer for parameter persistence.
 *          [FA] نگارندهٔ فلش رویتراشهٔ STM32F103C8T6 برای ماندگاری پارامترها.
 *
 * @note    [EN] Direct F1 flash-register driver (KEYR/CR/SR/AR), no HAL flash
 *              sources needed. Page erase on F1 stalls every code fetch from
 *              flash (single bank) for typ. 20..40 ms - callers must run in
 *              THREAD context, never an ISR (user order 2026-09-25: the panel
 *              values must survive power loss; the save runs in the comm
 *              task after the 1.5 s debounce, so a regulation hiccup of one
 *              comm period is acceptable).
 *          [FA] درایور مستقیم رجیسترهای فلش F1 (KEYR/CR/SR/AR) بدون نیاز به
 *              سورس‌های HAL فلش. پاک‌کردن صفحه در F1 همهٔ خواندن‌های کد از فلش
 *              را (تک-بنک) حدود ۲۰..۴۰ms نگه می‌دارد - صداکننده باید در بافت
 *              THREAD باشد نه ISR (دستور کاربر ۲۰۲۶-۰۹-۲۵: مقادیر پنل باید
 *              با قطع برق بمانند؛ ذخیره پس از ۱٫۵ ثانیه دیبانس در تسک ارتباط
 *              اجرا می‌شود، پس یک دورهٔ تاخیر تنظیم قابل قبول است).
 */

#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  [EN] Erase one 1 KiB flash page (page-aligned address inside the
 *         64 KiB bank). Blocking; thread context only.
 *         [FA] پاک‌کردن یک صفحهٔ ۱کیلوبایتی فلش (آدرس هم‌خطِ صفحه داخل بنک
 *         ۶۴کیلوبایتی). مسدودکننده؛ فقط بافت تسک.
 * @param  uint32_t__pageAddress [EN] Any address inside the target page /
 *         هر آدرسی داخل صفحهٔ هدف
 * @return bool [EN] true on success / موفقیت
 */
bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress);

/**
 * @brief  [EN] Program an even count of halfwords to main flash. The whole
 *         range must sit in one erased page; unaligned addresses are refused.
 *         [FA] نوشتن تعداد زوج نیم‌کلمه به فلش اصلی. کل بازه باید داخل یک
 *         صفحهٔ پاک‌شده باشد؛ آدرس فرد رد می‌شود.
 * @param  uint32_t__address [EN] Even start address / آدرس شروع زوج
 * @param  uint16_t__A__Data [EN] Halfword source array / آرایهٔ نیم‌کلمه‌ها
 * @param  uint32_t__count [EN] Halfword count / تعداد نیم‌کلمه‌ها
 * @return bool [EN] true on success / موفقیت
 */
bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count);

#endif /* BSP_FLASH_H */
