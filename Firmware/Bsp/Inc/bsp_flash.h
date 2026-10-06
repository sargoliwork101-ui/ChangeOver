/**
 * @file    bsp_flash.h
 * @brief   [EN] STM32F103C8T6 on-chip flash writer for parameter persistence.
 *          [FA] نگارندهٔ فلش رویتراشهٔ STM32F103C8T6 برای ماندگاری پارامترها.
 *
 * @note    [EN] Direct F1 flash-register driver (KEYR/CR/SR/AR), no HAL.
 *              A page erase on the single-bank F1 stalls every code fetch
 *              from flash for typ. 20..40 ms - callers must be in THREAD
 *              context, never an ISR (user order 2026-09-25: panel values
 *              must survive power loss; the save runs in the comm task
 *              after the 1.5 s debounce, so one regulation hiccup is
 *              acceptable).
 *          [FA] درایور مستقیم رجیسترهای فلش ‎F1 (KEYR/CR/SR/AR)‎ بدون HAL.
 *              پاک‌کردن صفحه در F1 تک‌بانک همهٔ fetchهای کد را ~۲۰..۴۰ms نگه
 *              می‌دارد - صداکننده باید THREAD باشد نه ISR (دستور کاربر:
 *              مقادیر پنل باید بمانند؛ ذخیره بعد از دیبانس ۱٫۵ ثانیه‌ای در
 *              تسک ارتباط است و یک تاخیر تنظیم قابل قبول است).
 */

#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== Writable storage window / پنجرهٔ مجاز نوشتن ==================== */
/* [EN] SINGLE SOURCE OF TRUTH for every byte of flash this driver is allowed
 *      to erase or program: the data area above the application image, i.e.
 *      0x0800E000..0x0800FFFF (the last 8 KiB of the 64 KiB part, matching
 *      the NVM + LUTNVM + SPARE regions of STM32F103C8TX_FLASH.ld). The two
 *      owners - EspLink parameter records and the CalLut bench table - sit
 *      inside it and each one _Static_asserts that it does, so moving a bank
 *      without widening this window breaks the BUILD.
 *      Bug fixed 2026-10-05: the guard used to be the hard-coded pair
 *      0x0800F800/0x0800FC00. When v1.80 moved the parameter bank down to
 *      0x0800E000 the guard stayed put, so every erase was refused and the
 *      save silently failed on the board - invisible to the host tests,
 *      which emulate flash in RAM. Deriving the window instead of retyping
 *      an address is what makes that class of drift impossible.
 * [FA] تنها مرجع آدرس‌هایی که این درایور حق پاک‌کردن یا نوشتن دارد: ناحیهٔ
 *      دادهٔ بالای تصویر برنامه، یعنی ‎0x0800E000..0x0800FFFF‎ (۸ کیلوبایت
 *      آخر، همان ناحیه‌های NVM و LUTNVM و SPARE در لینکر اسکریپت). هر دو
 *      مالک - رکورد پارامترهای EspLink و جدول بنچ CalLut - داخل آن‌اند و هر
 *      کدام با ‎_Static_assert‎ همین را اثبات می‌کند، پس جابه‌جایی یک بانک
 *      بدون گشادکردن این پنجره «بیلد» را می‌شکند.
 *      ایراد رفع‌شده ۲۰۲۶-۱۰-۰۵: گارد قبلی دو آدرس ثابت ‎0x0800F800/0x0800FC00‎
 *      بود؛ وقتی v1.80 بانک پارامترها را به ‎0x0800E000‎ برد، گارد سر جایش
 *      ماند و هر پاک‌کردن رد می‌شد - یعنی ذخیره روی برد بی‌صدا شکست می‌خورد
 *      و تست هاست هم چون فلش را در RAM شبیه‌سازی می‌کند آن را نمی‌دید. */
#define BSP_FLASH_STORAGE_BASE_ADDR    0x0800E000u

/* [EN] One past the last writable byte (end of the 64 KiB part).
 * [FA] یک بایت بعد از آخرین بایت قابل نوشتن (پایان قطعهٔ ۶۴KB). */
#define BSP_FLASH_STORAGE_END_ADDR     0x08010000u

/* [EN] Hardware erase granularity of the F1 medium-density part: 1 KiB.
 * [FA] دانهٔ پاک‌کردن سخت‌افزار در F1 چگالی متوسط: ۱KB. */
#define BSP_FLASH_PAGE_SIZE_BYTES      0x400u

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
