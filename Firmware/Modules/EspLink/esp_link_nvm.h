/**
 * @file    esp_link_nvm.h
 * @brief   [EN] Panel parameter persistence in STM32 on-chip flash (v1.14).
 *          [FA] ماندگاری پارامترهای پنل در فلش رویتراشهٔ STM32 (v1.14).
 *
 * @note    [EN] User order 2026-09-25: "I want to send the constants from the
 *              panel to the board and, once sent, they must stay on the board
 *              and survive power loss." Every settable parameter EXCEPT the
 *              transient test modes (fixed-duty 15..18, manual test 19) is
 *              snapshotted to the last two 1 KiB flash pages with a CRC and a
 *              sequence number, alternating pages per save so a power cut in
 *              the middle of an erase/program can never lose the previous
 *              good record. Boot loads the newest valid record through the
 *              SAME clamped setters the panel path uses, so a stale or
 *              hostile record can only land inside the compiled safety
 *              windows. Saves are debounced 1.5 s after the last change and
 *              run in the comm task (a page erase stalls flash-fetching code
 *              for ~20..40 ms once per save - acceptable for a rare event).
 *          [FA] دستور کاربر ۲۰۲۶-۰۹-۲۵: «ثابت‌ها را از روی پنل کلا بفرستم
 *              برای برد و وقتی فرستادم بمونه روی برد با قطع برق هم از بین
 *              نره.» همهٔ پارامترهای قابل‌تنظیم به‌جز مودهای تست گذرا
 *              (فیکس‌دیوتی ۱۵..۱۸ و تست دستی ۱۹) با CRC و شمارهٔ ترتیب در
 *              دو صفحهٔ آخر ۱KB فلش ذخیره می‌شوند و هر ذخیره صفحه را عوض
 *              می‌کند تا قطع برق وسط پاک‌کردن/نوشتن هرگز رکورد خوب قبلی را
 *              از بین نبرد. بوت، تازه‌ترین رکورد معتبر را از همان setterهای
 *              گیره‌دارِ مسیر پنل اعمال می‌کند، پس رکورد کهنه یا خراب فقط
 *              می‌تواند داخل پنجره‌های ایمنی کامپایل‌شده بنشیند. ذخیره
 *              ۱٫۵ ثانیه بعد از آخرین تغییر در تسک ارتباط اجرا می‌شود (پاک‌
 *              کردن صفحه یک‌بار ~۲۰..۴۰ms کدِ خوانده‌شده از فلش را نگه
 *              می‌دارد - برای رخداد کمی قابل قبول است).
 */

#ifndef ESP_LINK_NVM_H
#define ESP_LINK_NVM_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== EspLink Nvm constants ==================== */

/* [EN] The two reserved 1 KiB pages at the top of the 64 KiB bank (the linker
 *      script shrinks FLASH to 62 KiB so the application can never collide).
 *      Power-cut-safe ping-pong: each save erases the page that does NOT hold
 *      the newest record, then programs the new record there.
 * [FA] دو صفحهٔ رزروشدهٔ ۱KB در بالای بنک ۶۴KB (اسکریپت لینکر FLASH را به
 *      ۶۲KB کوچک می‌کند تا برنامه هرگز تصادم نکند). پینگ‌پنگ امن در برابر
 *      قطع برق: هر ذخیره صفحه‌ای را پاک می‌کند که رکورد تازه را ندارد و
 *      رکورد جدید را همان‌جا می‌نویسد. */
/* [EN] Address macros are guard-defined so the host test can redirect the
 *      two pages into a RAM emulation and compile the EXACT flash-state code.
 * [FA] ماکروهای آدرس گارد دارند تا تست هاست بتواند دو صفحه را به شبیه‌سازی
 *      RAM ببرد و دقیقاً همین کدِ ماشین حالت فلش را کامپایل کند. */
#ifndef ESP_LINK_NVM_PAGE_A_ADDR
#define ESP_LINK_NVM_PAGE_A_ADDR        0x0800F800u
#endif
#ifndef ESP_LINK_NVM_PAGE_B_ADDR
#define ESP_LINK_NVM_PAGE_B_ADDR        0x0800FC00u
#endif

/* [EN] Record identity: "CHO1" + format version. A bump invalidates old
 *      records (they fail validation and the compiled defaults stay).
 * [FA] هویت رکورد: «CHO1» + نسخهٔ قالب. تغییر نسخه رکوردهای قدیمی را
 *      نامعتبر می‌کند (اعتبارسنجی می‌شکنند و پیش‌فرض کامپایل می‌ماند). */
#define ESP_LINK_NVM_MAGIC              0x43484F31u
/* [EN] v1.15 (user order 2026-09-26): 2. The slot growth (27 -> 38) changes
 *      the record size, so v1 records fail CRC and fall back to the compiled
 *      defaults - a v1.12 profile saved on flash is lost on upgrade.
 * [FA] v1.15 (دستور کاربر ۲۰۲۶-۰۹-۲۶): نسخه ۲. رشد جای‌ها اندازهٔ رکورد را
 *      عوض می‌کند پس رکوردهای v1 در CRC می‌افتند و پیش‌فرض کامپایل می‌ماند -
 *      پروفایل ذخیره‌شدهٔ v1.12 با ارتقا از دست می‌رود. */
#define ESP_LINK_NVM_VERSION            2u

/* [EN] Slot cap: 33 persisted parameters today (0..14 = 15 config ids +
 *      20..37 = 7 charge-profile ids + 11 alarm ids). The cap is 38 so a
 *      future parameter can join the set without touching the record layout
 *      (320 B with 38 slots still fits one 1 KiB page with room to grow).
 *      The C harness caught the first draft's wrong count (17) as a silent
 *      early-return that would have programmed stack garbage - keep the
 *      harness in sync.
 * [FA] سقف جای‌ها: امروز ۳۳ پارامتر ذخیره می‌شود (0..14 = ۱۵ شناسهٔ
 *      پیکربندی + 20..37 = ۷ شناسهٔ پروفایل + ۱۱ شناسهٔ آلارم). سقف ۳۸ است
 *      تا پارامتر آینده بدون دست‌زدن به چیدمان رکورد به مجموعه بپیوندد (با
 *      ۳۸ جای ۳۲۰ بایت می‌شود که هنوز در یک صفحهٔ ۱KB جا می‌گیرد). هارنس C
 *      خطای شمارش نسخهٔ اول (۱۷) را به‌صورت بازگشت زودهنگامِ بی‌صدا گرفت که
 *      آشغال استک را فلش می‌کرد - هارنس را هم‌روز نگه دارید. */
#define ESP_LINK_NVM_ENTRY_MAX          38u

/* [EN] Save debounce in comm-task runs (period 100 ms -> 1.5 s after the last
 *      change; a shorter window would rewrite flash on every keystroke burst).
 * [FA] دیبانس ذخیره به تعداد اجرای تسک ارتباط (دورهٔ ۱۰۰ms ← ۱٫۵ ثانیه
 *      بعد از آخرین تغییر؛ پنجرهٔ کوتاه‌تر با هر رگبار تغییر، فلش را
 *      بازنویسی می‌کرد). */
#define ESP_LINK_NVM_SAVE_DELAY_RUNS    15u

/* [EN] Max save retries after a failed verify before giving up until the next
 *      change (a genuinely worn page must not enter an erase loop).
 * [FA] حداکثر تلاش مجدد ذخیره پس از شکست صحت‌سنجی، تا تغییر بعدی (صفحهٔ
 *      واقعاً فرسوده نباید حلقهٔ پاک‌کردن بی‌نهایت بسازد). */
#define ESP_LINK_NVM_SAVE_RETRIES       3u

/* [EN] Persisted id ranges: ALL settable configuration (0..14 = offsets,
 *      gains, filters, eta, charger enables and duty ceilings; 20..26 =
 *      charge profile; 27..37 = alarms) EXCEPT the transient test modes
 *      15..18 (fixed duty) and 19 (manual test) - those must never survive
 *      a reboot.
 * [FA] بازه‌های شناسهٔ ذخیره‌شونده: تمام پیکربندی قابل‌تنظیم (0..14 =
 *      آفست‌ها، گین‌ها، فیلترها، eta، فعال‌بودن شارژر و سقف دیوتی؛ 20..26 =
 *      پروفایل شارژ؛ ۲۷..۳۷ = آلارم‌ها) به‌جز مودهای تست گذرای ۱۵..۱۸
 *      (فیکس‌دیوتی) و ۱۹ (تست دستی) - آنها هرگز نباید از ریبوت جان به در
 *      ببرند. */
#define ESP_LINK_NVM_PERSISTED_ID_MAX_LOW     14u
#define ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH    20u
#define ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH    37u

/**
 * @brief  [EN] Is this parameter id persisted to flash? (config + charge
 *              profile, never the transient test modes).
 *         [FA] این شناسهٔ پارامتر در فلش ذخیره می‌شود؟ (پیکربندی + پروفایل
 *              شارژ، هرگز مودهای تست گذرا).
 */
bool func__EspLink_NvmParamPersisted(uint8_t uint8_t__paramId);

/**
 * @brief  [EN] One persisted parameter slot (id + applied value).
 *         [FA] یک جای پارامتر ذخیره‌شونده (شناسه + مقدار اعمال‌شده).
 */
typedef struct
{
    uint16_t uint16_t__id;      /* [EN] ESPLINK_PARAM_* / شناسهٔ پارامتر */
    uint16_t uint16_t__pad;     /* [EN] Halfword alignment padding / لایه‌گذاری */
    uint32_t uint32_t__value;   /* [EN] Value as applied / مقدار اعمال‌شده */
} esp_link_nvm_entry_t;

/**
 * @brief  [EN] Flash record: header + entry list + CRC32 over all preceding
 *              bytes. Size 152 B for 17 entries - one erased page holds it
 *              with room to grow.
 *         [FA] رکورد فلش: سربرگ + فهرست ورودی‌ها + CRC32 روی همهٔ بایت‌های
 *              قبل از خودش. اندازه ۱۵۲ بایت برای ۱۷ ورودی - یک صفحهٔ پاک‌
 *              شده با حاشیه جایش را می‌دهد.
 */
typedef struct
{
    uint32_t uint32_t__magic;                       /* ESP_LINK_NVM_MAGIC */
    uint16_t uint16_t__version;                     /* ESP_LINK_NVM_VERSION */
    uint16_t uint16_t__seq;                         /* [EN] Wrap-around sequence / شمارهٔ چرخشی */
    uint16_t uint16_t__count;                       /* [EN] Valid entries / تعداد ورودی معتبر */
    uint16_t uint16_t__reserved;
    esp_link_nvm_entry_t
        ESP_LINK_NVM_ENTRY_T__A__Entry[ESP_LINK_NVM_ENTRY_MAX];
    uint32_t uint32_t__crc32;
} esp_link_nvm_record_t;

/* ==================== EspLink Nvm public functions ==================== */

/* [EN] Pure record helpers - public because the host test compiles this
 *      exact code against a RAM-emulated flash.
 * [FA] توابع خالص رکورد - عمومی چون تست هاست همین کد را روی فلشِ
 *      شبیه‌سازی‌شدهٔ RAM کامپایل می‌کند. */
uint32_t func__EspLink_NvmCrc32(const uint8_t *uint8_t__A__Data,
                                uint32_t uint32_t__length);
bool func__EspLink_NvmRecordValidate(
    const esp_link_nvm_record_t *esp_link_nvm_record_t__record);
int8_t func__EspLink_NvmSeqCompare(uint16_t uint16_t__a, uint16_t uint16_t__b);
void func__EspLink_NvmRecordBuild(
    esp_link_nvm_record_t *esp_link_nvm_record_t__record,
    uint16_t uint16_t__seq,
    const esp_link_nvm_entry_t *ESP_LINK_NVM_ENTRY_T__A__Entry,
    uint16_t uint16_t__count);

/**
 * @brief  [EN] Boot-time load: read both pages, pick the newest CRC-valid
 *              record and replay every entry through the clamped parameter
 *              setters (fails are skipped, never fatal). Runs pre-scheduler
 *              in func__App_Init under MODULE_ESP - module Init functions
 *              only reset channel state, never the settable statics, so the
 *              loaded values survive them.
 *         [FA] بارگذاری هنگام بوت: خواندن هر دو صفحه، انتخاب تازه‌ترین
 *              رکوردِ سالمِ CRC و بازپخش همهٔ ورودی‌ها از setterهای گیره‌دار
 *              (خطا رد می‌شود، هرگز مهلک نیست). قبل از زمان‌بند داخل
 *              func__App_Init زیر MODULE_ESP اجرا می‌شود - Initهای ماژول فقط
 *              وضعیت کانال را ریست می‌کنند نه staticهای قابل‌تنظیم را، پس
 *              مقادیر بارگذاری‌شده از آنها جان سالم به در می‌برند.
 */
void func__EspLink_NvmInit(void);

/**
 * @brief  [EN] Remember that a persisted parameter changed; a debounced save
 *              follows (see func__EspLink_NvmTick).
 *         [FA] ثبت اینکه یک پارامتر ذخیره‌شونده عوض شده؛ ذخیرهٔ دیبانس‌شده
 *              در ادامه می‌آید (func__EspLink_NvmTick).
 * @param  uint8_t__paramId [EN] The SET_PARAM id / شناسهٔ SET_PARAM
 */
void func__EspLink_NvmMarkDirty(uint8_t uint8_t__paramId);

/**
 * @brief  [EN] Comm-task housekeeping: count the debounce after the last
 *              change, then snapshot every persisted parameter and write the
 *              ping-pong record. Also retries a failed verify (bounded).
 *         [FA] نگهداری تسک ارتباط: شمارش دیبانس بعد از آخرین تغییر و سپس
 *              عکس‌فوری همهٔ پارامترهای ذخیره‌شونده و نوشتن رکورد پینگ‌پنگ.
 *              تلاش مجدد محدود برای صحت‌سنجی شکست‌خورده هم همینجاست.
 */
void func__EspLink_NvmTick(void);

#endif /* ESP_LINK_NVM_H */
