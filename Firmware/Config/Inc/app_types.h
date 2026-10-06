/**
 * @file    app_types.h
 * @brief   [EN] Shared types: system state, ADC snapshot, fault bits.
 *          [FA] نوع‌های مشترک: حالت سیستم، نمونه ADC، بیت خطا.
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    APP_STATE_BOOT = 0,
    APP_STATE_IDLE,
    APP_STATE_INPUT,
    APP_STATE_BATTERY,
    APP_STATE_FAULT,
    APP_STATE_SAFE
} app_state_t;

typedef struct
{
    uint32_t v_in_mv;
    uint32_t v_bat24_mv;
    uint32_t v_bat12_mv;
    uint32_t v_bat_low_mv;
    uint32_t v_bat_high_mv;
    uint32_t i_ch1_ma;
    uint32_t i_ch2_ma;
    bool     input_present;
    bool     valid;
} measurement_snapshot_t;

typedef uint32_t fault_mask_t;

/* ==================== Defines ==================== */
#define FAULT_NONE              0u

/* [EN] Live bit: set while the measurement snapshot is invalid, cleared by
 *      the first valid one (deliberately NOT latched - see protection.c).
 * [FA] بیت زنده: تا وقتی نمونهٔ اندازه‌گیری نامعتبر است ست می‌شود و با اولین
 *      نمونهٔ سالم پاک می‌شود (عمداً قفل نمی‌شود - توضیح در protection.c). */
#define FAULT_ADC               (1u << 0)

/* [EN] RESERVED bits (audit 2026-10-05, user order: keep, do not delete).
 *      NOTHING sets these five bits in the current firmware - the behaviour
 *      they used to describe now lives inside the owning module:
 *        - over-current  -> the charger's own current ceiling + backstop,
 *        - low battery   -> the internal latch inside changeover.c (v1.74),
 *        - jitter        -> the per-channel trip handling inside charger.c.
 *      They are kept ONLY so the bit numbers never shift under the panel,
 *      the NVM records and the bench logs. Do not reuse a number for a new
 *      meaning; append a new bit instead.
 * [FA] بیت‌های رزرو (ممیزی ۲۰۲۶-۱۰-۰۵، دستور کاربر: بمانند، حذف نشوند).
 *      هیچ‌جای فرمور این پنج بیت را ست نمی‌کند؛ رفتاری که قبلاً توصیف
 *      می‌کردند حالا داخل خود ماژول صاحبش است: اضافه‌جریان در سقف جریان و
 *      ترمز اضطراری شارژر، باتری کم در قفل داخلی ‎changeover.c‎ (از v1.74) و
 *      جیتر در مدیریت تریپ هر کانال در ‎charger.c‎. فقط برای اینکه شمارهٔ
 *      بیت‌ها زیر پنل و رکوردهای NVM و لاگ بنچ جابه‌جا نشود نگه داشته شده‌اند.
 *      شمارهٔ یک بیت را برای معنای جدید بازاستفاده نکنید؛ بیت تازه اضافه کنید. */
#define FAULT_OVERCURRENT_1     (1u << 1)   /* [EN] reserved / رزرو */
#define FAULT_OVERCURRENT_2     (1u << 2)   /* [EN] reserved / رزرو */
#define FAULT_LOW_BATTERY       (1u << 3)   /* [EN] reserved / رزرو */
#define FAULT_JITTER_1          (1u << 4)   /* [EN] reserved / رزرو */
#define FAULT_JITTER_2          (1u << 5)   /* [EN] reserved / رزرو */
/* [EN] Battery-lost during charge, detected and cleared centrally by the
 *      Fault module (since 2026-09-19): output voltage pumped above 14.8 V
 *      (no real 12 V battery reaches that), or battery absent with a valid
 *      input. Charger only mirrors the bit into CHG_STATE_BAT_LOST.
 * [FA] قطع باتری حین شارژ؛ تشخیص و پاک‌سازی متمرکز در ماژول Fault (از
 *      ۲۰۲۶-۰۹-۱۹): پمپ ولتاژ بالای ۱۴٫۸V یا نبود باتری با ورودی سالم.
 *      شارژر فقط بیت را به CHG_STATE_BAT_LOST آینه می‌کند. */
#define FAULT_CHARGER_BAT_LOST  (1u << 6)

#endif /* APP_TYPES_H */
