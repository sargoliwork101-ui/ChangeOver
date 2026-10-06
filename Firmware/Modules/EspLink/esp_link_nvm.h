/**
 * @file    esp_link_nvm.h
 * @brief   [EN] Panel parameter persistence in STM32 on-chip flash (v1.14).
 *          [FA] ماندگاری پارامترهای پنل در فلش رویتراشهٔ STM32 (v1.14).
 *
 * @note    [EN] Every settable parameter EXCEPT the transient test modes
 *              (15..18 fixed duty, 19 manual test) is snapshotted to the
 *              last two 1 KiB flash pages with a CRC and a sequence
 *              number, alternating pages per save so a power cut mid-write
 *              can never lose the previous good record. Boot replays the
 *              newest valid record through the SAME clamped setters as
 *              the panel path, so a stale or hostile record can only land
 *              inside the compiled safety windows. Saves are debounced
 *              ~1.5 s and run in the comm task (a page erase stalls the
 *              CPU ~20..40 ms once per save).
 *          [FA] همهٔ پارامترهای قابل‌تنظیم به‌جز مودهای تست گذرا (۱۵..۱۸ و
 *              ۱۹) با CRC و شمارهٔ ترتیب در دو صفحهٔ آخر ۱KB فلش ذخیره
 *              می‌شوند و هر ذخیره صفحه را عوض می‌کند تا قطع برق وسط نوشتن
 *              رکورد خوب قبلی را از بین نبرد. بوت تازه‌ترین رکورد معتبر را
 *              از همان setterهای گیره‌دارِ مسیر پنل بازپخش می‌کند. ذخیره
 *              ~۱٫۵ ثانیه دیبانس و در تسک ارتباط است (پاک‌کردن صفحه یک‌بار
 *              ~۲۰..۴۰ms CPU را نگه می‌دارد).
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
/* [EN] v1.80: each bank is TWO 1 KiB pages now (A = 0x0800E000..E7FF,
 *      B = 0x0800E800..EFFF). The scenario-6 lamp/buzzer ids 128..131 took
 *      the persisted set past the 126 entries that fitted in a single page,
 *      and the record is written whole, so the bank - not the entry - had to
 *      grow. A save erases every 1 KiB page of the target bank before
 *      programming (ESP_LINK_NVM_FLASH_PAGE_SIZE below); ping-pong is
 *      unchanged, so a power cut mid-write still leaves the other bank
 *      intact. The old top pages 0x0800F800/FC00 are left reserved-unused:
 *      a record written there by an older firmware simply never validates.
 * [FA] از v1.80 هر بانک دو صفحهٔ ۱KB است، چون با شناسه‌های ۱۲۸..۱۳۱
 *      مجموعهٔ ذخیره‌شونده از ۱۲۶ جای یک صفحه گذشت. هر ذخیره همهٔ صفحه‌های
 *      بانک مقصد را پاک می‌کند و پینگ‌پنگ دست‌نخورده است. */
#ifndef ESP_LINK_NVM_PAGE_A_ADDR
#define ESP_LINK_NVM_PAGE_A_ADDR        0x0800E000u
#endif
#ifndef ESP_LINK_NVM_PAGE_B_ADDR
#define ESP_LINK_NVM_PAGE_B_ADDR        0x0800E800u
#endif
/* [EN] Hardware erase granularity (STM32F103 medium density = 1 KiB). A bank
 *      spans ESP_LINK_NVM_PAGE_B_ADDR - ESP_LINK_NVM_PAGE_A_ADDR bytes, i.e.
 *      an exact number of these pages; the save loop erases them all.
 * [FA] دانهٔ پاک‌کردن سخت‌افزار ۱KB است؛ حلقهٔ ذخیره همهٔ صفحه‌های بانک را
 *      پاک می‌کند. */
#ifndef ESP_LINK_NVM_FLASH_PAGE_SIZE
#define ESP_LINK_NVM_FLASH_PAGE_SIZE    0x400u
#endif

/* [EN] Record identity: "CHO1" + format version. A bump invalidates old
 *      records (they fail validation and the compiled defaults stay).
 * [FA] هویت رکورد: «CHO1» + نسخهٔ قالب. تغییر نسخه رکوردهای قدیمی را
 *      نامعتبر می‌کند (اعتبارسنجی می‌شکنند و پیش‌فرض کامپایل می‌ماند). */
#define ESP_LINK_NVM_MAGIC              0x43484F31u
/* [EN] v11 (v1.71) is a MEANING bump, not a layout bump: ids 57 and 122 kept
 *      their slots but changed units - 57 went from "critical duty percent"
 *      to "critical per-beep duration in ms", and 122 went from "band-2 gap"
 *      to "band-2 repeat interval in ms". A stored v10 record holds 57=100
 *      and 122=100, which under the new meaning would mean a 100 ms beep
 *      repeating every 100 ms. Replaying it would be wrong and loud, so the
 *      version bump deliberately invalidates it and the compiled defaults
 *      (which reproduce the old sound exactly) take over. Re-tune once from
 *      the panel after this upgrade.
 * [FA] نسخهٔ ۱۱ تغییر «معنی» است نه چیدمان: شناسه‌های ۵۷ و ۱۲۲ جایشان عوض
 *      نشد ولی واحدشان عوض شد. رکورد قدیمی برای این دو عدد ۱۰۰ دارد که با
 *      معنی تازه یعنی بوق ۱۰۰ms با تکرار هر ۱۰۰ms؛ پس عمداً نامعتبر می‌شود و
 *      پیش‌فرض کامپایل (که همان صدای قبلی را می‌دهد) می‌ماند. بعد از این
 *      ارتقا یک‌بار از پنل تنظیم‌ها را دوباره بفرستید. */
/* [EN] Record version history: v3 = 77 slots (v1.16 LED/buzzer
 *      mirror), v4 = id 76 became a panel-session mute, never persisted
 *      (v1.16b), v5 = 83 slots incl. the six full/hysteresis ids 77..82
 *      (v1.17), v6/v7 = 98 slots incl. the fifteen three-stage PID ids
 *      83..97 (v1.22/v1.23), v8 = 93 slots incl. the ten
 *      two-loop CC/CV PID ids 83..92 (v1.24 deleted the redundant third
 *      gain row), v9 = 108 slots incl. the fifteen charger
 *      limits/timer/gain ids 93..107 (v1.28),

 *      v10 = CURRENT: 122 slots incl. the eleven imbalance scenario ids
 *      108..118 (v1.43) and the three imbalance runtime slots 200..202
 *      (events/cycles/latch; never user parameters). A record with an older version fails the version check
 *      and falls back to the compiled defaults - after any upgrade that
 *      changes the record layout, re-tune from the panel once (v8 IS such
 *      an upgrade, and the bump is mandatory rather than cosmetic: a v7
 *      record's slots 88..92 hold the OLD third row, which would otherwise
 *      be restored straight into the new voltage row).
 * [FA] تاریخچهٔ نسخهٔ رکورد: v3 = ۷۷ جای (آینهٔ LED/بازر v1.16)، v4 =
 *      میوت ۷۶ جلسه‌ای شد و دیگر ذخیره نمی‌شود (v1.16b)، v5 = ۸۳ جای
 *      شامل ۷۷..۸۲ (v1.17)، ‎v6/v7‎ = ۹۸ جای شامل پانزده شناسهٔ PID
 *      سه‌مرحله‌ای ۸۳..۹۷، v8 = ۹۳ جای شامل ده شناسهٔ PID دوحلقه‌ای
 *      ۸۳..۹۲ (v1.24 ردیف سوم زائد را حذف کرد)، v9 = فعلی: ۱۰۸ جای شامل
 *      پانزده شناسهٔ ۹۳..۱۰۷ (حدها/زمان‌ها/گین‌های شارژر). رکورد قدیمی‌تر
 *      می‌افتد و
 *      پیش‌فرض کامپایل می‌ماند - بعد از هر ارتقای چیدمان یک‌بار از پنل
 *      دوباره تنظیم کنید (v1.22 دقیقاً چنین ارتقایی است: اولین بوت پس از
 *      فلش با مقادیر کارخانه بالا می‌آید). */
#define ESP_LINK_NVM_VERSION            12u

/* [EN] Slot cap: 122 persisted ids today (0..14 config + 20..26 charge
 *      profile + 27..37 alarms + 38..75 UI cadence + 77..82 full/
 *      hysteresis + 83..92 two-loop PID + 93..107 charger limits and
 *      backstop gains + 108..118 imbalance scenario + 200..202 imbalance
 *      runtime slots; id 76 = panel-session mute, transient like 15..19).
 *      Cap 122 -> record = 12 + 122 x 8 + 4 = 992 B, still inside one 1 KiB
 *      page with 32 B to spare - the assert in the .c proves it rather
 *      than trusting this arithmetic. Keep the C harness in sync (it once
 *      caught a wrong count as a silent early-return).
 * [FA] سقف جای‌ها: امروز ۱۲۲ شناسهٔ ذخیره‌شونده (… + ۱۰۸..۱۱۸ سناریوی
 *      عدم‌توازن + ۲۰۰..۲۰۲ اسلات زمان‌اجرا؛ ۷۶ گذرا). سقف ۱۲۲ یعنی رکورد
 *      ۹۹۲ بایت، باز هم داخل یک صفحهٔ ۱KB با ۳۲ بایت حاشیه - گزارهٔ داخل
 *      فایل .c این را «اثبات» می‌کند. هارنس C را هم‌روز نگه دارید. */
/* [EN] v1.50: 126 slots (12 + 126 x 8 + 4 = 1024 B = exactly one 1 KiB page).
 *      v1.72 spends the last of that headroom: the three scenario-6 ids
 *      (125..127) and slot 203 bring the persisted set to exactly 126, so
 *      the cap and the real set now meet. Anything further needs a second
 *      page, and the _Static_assert in the .c is what will say so. The record stores its own entry count, so an
 *      older, shorter record still replays correctly - no version bump.
 * [FA] ۱۲۶ جا (۱۰۲۴ بایت، دقیقاً یک صفحه). رکورد تعداد خودش را
 *      ذخیره می‌کند پس رکورد کوتاه‌تر قدیمی هم درست پخش می‌شود. */
/* [EN] v1.80: 144 slots (12 + 144 x 8 + 4 = 1168 B) inside a 2 KiB bank. The
 *      real persisted set is 131 ids today (the four scenario-6 face ids,
 *      imbalance beep count/gap and clean-cycle threshold pushed it past the
 *      old 126-slot single page), so there are 13 spare slots and ~872 B of
 *      page left - the _Static_assert
 *      in the .c proves both rather than trusting this arithmetic.
 * [FA] ۱۴۴ جا (۱۱۶۸ بایت) داخل بانک ۲KB؛ امروز ۱۳۰ شناسه ذخیره می‌شود. */
#define ESP_LINK_NVM_ENTRY_MAX         144u

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
 *      gains, filters, eta, charger enables, duty ceilings; 20..26 =
 *      charge profile; 27..37 = alarms; 38..75 = UI cadence; 77..82 =
 *      full/hysteresis; 83..92 = two-loop PID; 119..120 = charge-side percent map, v1.49; 121..122 = band-2 beep shape, v1.50; 132..133 = imbalance beep count/gap, v1.81; 134..135 = dead-battery beep count/gap, v1.82; 136 = imbalance clean-FLOAT-cycle threshold, v1.83) EXCEPT the transient
 *      test modes 15..18 (fixed duty), 19 (manual test) and 76
 *      (panel-session mute) - those must never survive a reboot. Id 76
 *      sits INSIDE the high range, so the predicate excludes it
 *      explicitly (see the .c). The PID gains are ordinary tuning numbers,
 *      so they persist like the profile does.
 * [FA] بازه‌های شناسهٔ ذخیره‌شونده: تمام پیکربندی قابل‌تنظیم (‎0..14‎ =
 *      آفست‌ها، گین‌ها، فیلترها، eta، فعال‌بودن شارژر و سقف دیوتی؛ ‎20..26‎ =
 *      پروفایل شارژ؛ ۲۷..۳۷ = آلارم‌ها؛ ۳۸..۷۵ = اعداد UI؛ ۷۷..۸۲ =
 *      فول/هیسترزیس؛ ۸۳..۹۲ = PID دوحلقه‌ای) به‌جز مودهای گذرای ۱۵..۱۸،
 *      ۱۹ و ۷۶ - آنها هرگز از ریبوت جان به در نمی‌برند. ۷۶ داخل بازهٔ بالا
 *      است پس محمول صریحاً کنارش می‌گذارد. ضرایب PID عدد تنظیم عادی‌اند و
 *      مثل پروفایل ماندگارند. */
#define ESP_LINK_NVM_PERSISTED_ID_MAX_LOW     14u
#define ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH    20u
#define ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH   142u /* [EN] v1.84 adds scenario-7 UI controls 137..142. / [FA] نسخهٔ ۱٫۸۴ کنترل‌های سناریوی ۷ را اضافه می‌کند. */

/* [EN] v1.74: ids 72/73 are retired (the low-battery window became a fixed
   constant inside the Changeover module), so they are carved out of the high
   persisted range - storing a value no owner can read back would be dead
   weight in the record. Mirrors ESPLINK_PARAM_RETIRED_LOWBAT_* in esp_link.h;
   esp_link_nvm.c static-asserts the two pairs agree.
   [FA] شناسه‌های بازنشستهٔ ۷۲/۷۳ از بازهٔ ذخیره‌شونده بیرون کشیده می‌شوند. */
#define ESP_LINK_NVM_RETIRED_ID_FIRST        72u
#define ESP_LINK_NVM_RETIRED_ID_LAST         73u
#define ESP_LINK_NVM_TRANSIENT_ID_MUTE        76u
/* [EN] Imbalance runtime slots (scenario 5, v10): persisted but NEVER user
 *      parameters - the module itself writes them; the panel never draws and
 *      never backs them up.
 * [FA] اسلات‌های زمان‌اجرا عدم‌توازن: فقط خود ماژول می‌نویسد. */
#define ESP_LINK_NVM_SLOT_MIN_ID            200u
/* [EN] v1.72: 203 = dead-battery latch mask (scenario 6). Same rule - the
 *      module writes it, the panel never backs it up, and it must survive a
 *      power cycle so a faulty battery stays condemned until it is replaced.
 * [FA] ۲۰۳ = ماسک قفل باتری خراب؛ تا تعویض باتری باید بماند. */
#define ESP_LINK_NVM_SLOT_MAX_ID            203u

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
 *              bytes. 992 B for 122 entries (12 + 122 x 8 + 4), inside one
 *              1 KiB page with 32 B to spare. Do not trust this sentence:
 *              the two halves of this very comment disagreed for several
 *              releases (EN said 760 B for 93, FA said 808 B for 99) because
 *              both were hand-arithmetic nobody re-ran. The guarantee is the
 *              _Static_assert in esp_link_nvm.c, which derives the page size
 *              from the two page addresses and fails the build instead.
 *         [FA] رکورد فلش: سربرگ + فهرست ورودی‌ها + CRC32 روی همهٔ بایت‌های
 *              قبل از خودش. ۹۹۲ بایت برای ۱۲۲ ورودی (۱۲ + ۱۲۲×۸ + ۴)، داخل
 *              یک صفحهٔ ۱ کیلوبایتی با ۳۲ بایت حاشیه. به همین جمله اعتماد
 *              نکنید: دو نیمهٔ همین کامنت چند نسخه با هم اختلاف داشتند
 *              (انگلیسی ۷۶۰ بایت برای ۹۳، فارسی ۸۰۸ بایت برای ۹۹) چون هر دو
 *              حساب دستی بودند که کسی دوباره اجرایشان نکرد. ضمانت واقعی
 *              _Static_assert در esp_link_nvm.c است که اندازهٔ صفحه را از دو
 *              آدرس صفحه مشتق می‌کند و به‌جای کامنت، بیلد را می‌شکند.
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
 *              setters (fails are skipped, never fatal). Runs
 *              pre-scheduler in func__App_Init under MODULE_ESP - module
 *              Init functions only reset channel state, never the
 *              settable statics, so the loaded values survive them.
 *         [FA] بارگذاری هنگام بوت: خواندن هر دو صفحه، انتخاب تازه‌ترین
 *              رکوردِ سالمِ CRC و بازپخش ورودی‌ها از setterهای گیره‌دار
 *              (خطا رد می‌شود، هرگز مهلک نیست). قبل از زمان‌بند داخل
 *              func__App_Init زیر MODULE_ESP اجرا می‌شود - Initهای ماژول
 *              فقط وضعیت کانال را ریست می‌کنند نه staticهای قابل‌تنظیم را.
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
