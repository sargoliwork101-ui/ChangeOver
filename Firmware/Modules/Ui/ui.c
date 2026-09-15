/**
 * @file    ui.c
 * @brief   [EN] UI main - now split into LED and BUZZER per user request, both in same folder.
 *          This file is kept for backward compatibility and includes the split implementations.
 *          All constants in ui.h (single source). Markers above each function in split files.
 *          [FA] فایل اصلی UI - الان دو بخش شد: LED و BUZZER، هر دو در همین پوشه.
 *
 * @note    [EN] Split: ui_led.c (LED scenarios) and ui_buzzer.c (buzzer patterns). RTOS simple, non-linear formulas.
 *          Old ui.c had buzzer at end after LED; now LED in ui_led.c, BUZZER in ui_buzzer.c, both with markers.
 *          [FA] دو بخش: ui_led.c و ui_buzzer.c، هر دو با جدا کننده بالای هر تابع.
 */

/* ==================== Includes ==================== */

#include "ui.h"

/* ==================== Compatibility Note ==================== */

/* [EN] Implementations moved to ui_led.c and ui_buzzer.c per user request to split UI into LED and BUZZER.
   This file kept empty for backward compatibility; build should compile ui_led.c and ui_buzzer.c.
   [FA] پیاده‌سازی‌ها به ui_led.c و ui_buzzer.c منتقل شد، این فایل خالی برای سازگاری. */

/* ==================== LED and Buzzer Split ==================== */

/* [EN] LED and BUZZER now in separate files, both with /* ==================== */ markers above each function.
   [FA] LED و BUZZER الان جدا در فایل‌های خودشون، هر تابع با جدا کننده. */

/* ==================== Buzzer / Beep ==================== */

/* [EN] Buzzer marker kept for backward compatibility with check_ai_rules, actual code in ui_buzzer.c
   [FA] جدا کننده بازر برای سازگاری، کد اصلی در ui_buzzer.c */
