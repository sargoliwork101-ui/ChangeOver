/**
 * @file    esp_link_nvm.c
 * @brief   [EN] Panel parameter persistence in STM32 on-chip flash (v1.14).
 *          [FA] ماندگاری پارامترهای پنل در فلش رویتراشهٔ STM32 (v1.14).
 *
 * @note    [EN] See esp_link_nvm.h for the contract. The PURE RECORD LOGIC
 *              block below (CRC32, validation, sequence compare, record
 *              build) is self-contained - the host test compiles exactly
 *              this block against a RAM-emulated flash to prove the
 *              power-cut behaviour without hardware.
 *          [FA] قرارداد در esp_link_nvm.h آمده. بلوک «منطق خالص رکورد»
 *              پایین (CRC32، اعتبارسنجی، مقایسهٔ ترتیب، ساخت رکورد)
 *              خودکفاست - تست هاست دقیقاً همین بلوک را روی فلشِ شبیه‌سازی‌
 *              شدهٔ RAM کامپایل می‌کند تا رفتار قطع برق بدون سخت‌افزار
 *              اثبات شود.
 */

#include "esp_link_nvm.h"
#include "esp_link.h"
#include "bsp_flash.h"

#include <stddef.h>

/* ====================================================================
 * ===== EspLink Nvm pure record logic (host-testable, no flash) =====
 * ==================================================================== */

/**
 * @brief  [EN] Reflected CRC32 (poly 0xEDB88320, init/xor 0xFFFFFFFF) - the
 *              same standard zip PNG crc, bit-by-bit so no table in flash.
 *         [FA] CRC32 بازتابیده (چندجمله‌ای 0xEDB88320، مقدار اولی/پایانی
 *              0xFFFFFFFF) - همان CRC استاندارد، بیتی-به-بیتی بدون جدول.
 */
uint32_t func__EspLink_NvmCrc32(const uint8_t *uint8_t__A__Data,
                                uint32_t uint32_t__length)
{
    uint32_t uint32_t__crc = 0xFFFFFFFFu;
    uint32_t uint32_t__i;
    uint8_t uint8_t__bit;

    for (uint32_t__i = 0u; uint32_t__i < uint32_t__length; uint32_t__i++)
    {
        uint32_t__crc ^= (uint32_t)uint8_t__A__Data[uint32_t__i];
        for (uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
        {
            if ((uint32_t__crc & 1u) != 0u)
            {
                uint32_t__crc = (uint32_t__crc >> 1) ^ 0xEDB88320u;
            }
            else
            {
                uint32_t__crc = (uint32_t__crc >> 1);
            }
        }
    }

    return (uint32_t__crc ^ 0xFFFFFFFFu);
}

/**
 * @brief  [EN] Is this parameter id part of the persisted configuration?
 *         [FA] این شناسه جزو پیکربندی ذخیره‌شونده است؟
 */
bool func__EspLink_NvmParamPersisted(uint8_t uint8_t__paramId)
{
    return ((uint8_t__paramId <= ESP_LINK_NVM_PERSISTED_ID_MAX_LOW) ||
            ((uint8_t__paramId >= ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH) &&
             (uint8_t__paramId <= ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH)));
}

/**
 * @brief  [EN] Signed halfword sequence compare: +1 when a is newer, -1 when
 *              b is newer, 0 on a tie. The int16 difference makes the u16
 *              wrap-around at 65535 -> 0 monotonic.
 *         [FA] مقایسهٔ علامت‌دارِ ترتیب نیم‌کلمه: ‎+۱ اگر a تازه‌تر، ‎−۱ اگر
 *              b تازه‌تر، ۰ در برابری. تفاضل int16 چرخش u16 در 65535→0 را
 *              یکنوا می‌کند.
 */
int8_t func__EspLink_NvmSeqCompare(uint16_t uint16_t__a, uint16_t uint16_t__b)
{
    int16_t int16_t__diff =
        (int16_t)(uint16_t__a - uint16_t__b);

    if (int16_t__diff > 0)
    {
        return 1;
    }
    if (int16_t__diff < 0)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief  [EN] Full record check: magic, version, entry count, every entry a
 *              persisted id, and the CRC over all bytes before the CRC
 *              field. A record with any transient test id (15..19) is
 *              REJECTED on purpose.
 *         [FA] بررسی کامل رکورد: جادو، نسخه، تعداد ورودی، persisted بودن
 *              شناسهٔ هر ورودی و CRC روی همهٔ بایت‌های قبل از فیلد CRC.
 *              رکوردی با هر شناسهٔ تست گذرا (۱۵..۱۹) عمداً رد می‌شود.
 */
bool func__EspLink_NvmRecordValidate(const esp_link_nvm_record_t
                                     *esp_link_nvm_record_t__record)
{
    uint32_t uint32_t__crc;
    uint16_t uint16_t__i;

    if (esp_link_nvm_record_t__record == NULL)
    {
        return false;
    }

    if ((esp_link_nvm_record_t__record->uint32_t__magic != ESP_LINK_NVM_MAGIC) ||
        (esp_link_nvm_record_t__record->uint16_t__version != ESP_LINK_NVM_VERSION) ||
        (esp_link_nvm_record_t__record->uint16_t__count > ESP_LINK_NVM_ENTRY_MAX))
    {
        return false;
    }

    for (uint16_t__i = 0u;
         uint16_t__i < esp_link_nvm_record_t__record->uint16_t__count;
         uint16_t__i++)
    {
        if (func__EspLink_NvmParamPersisted(
                (uint8_t)esp_link_nvm_record_t__record
                    ->ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__i].uint16_t__id) ==
            false)
        {
            return false;
        }
    }

    uint32_t__crc = func__EspLink_NvmCrc32(
        (const uint8_t *)esp_link_nvm_record_t__record,
        (uint32_t)(sizeof(esp_link_nvm_record_t) - sizeof(uint32_t)));

    return (uint32_t__crc == esp_link_nvm_record_t__record->uint32_t__crc32);
}

/**
 * @brief  [EN] Fill one record from the given entries (deterministic padding
 *              - the struct is zeroed first so identical inputs give an
 *              identical CRC) and stamp the sequence number.
 *         [FA] پرکردن یک رکورد از ورودی‌های داده‌شده (لایه‌گذاری قطعی -
 *              ابتدا صفر می‌شود تا ورودی یکسان، CRC یکسان بدهد) و زدن
 *              شمارهٔ ترتیب.
 */
void func__EspLink_NvmRecordBuild(esp_link_nvm_record_t
                                  *esp_link_nvm_record_t__record,
                                  uint16_t uint16_t__seq,
                                  const esp_link_nvm_entry_t
                                      *ESP_LINK_NVM_ENTRY_T__A__Entry,
                                  uint16_t uint16_t__count)
{
    uint16_t uint16_t__i;

    if ((esp_link_nvm_record_t__record == NULL) ||
        (ESP_LINK_NVM_ENTRY_T__A__Entry == NULL) ||
        (uint16_t__count > ESP_LINK_NVM_ENTRY_MAX))
    {
        return;
    }

    for (uint16_t__i = 0u;
         uint16_t__i < (uint16_t)(sizeof(esp_link_nvm_record_t) /
                                  sizeof(uint16_t));
         uint16_t__i++)
    {
        ((volatile uint16_t *)esp_link_nvm_record_t__record)[uint16_t__i] = 0u;
    }

    for (uint16_t__i = 0u; uint16_t__i < uint16_t__count; uint16_t__i++)
    {
        esp_link_nvm_record_t__record
            ->ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__i] =
            ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__i];
    }

    esp_link_nvm_record_t__record->uint32_t__magic = ESP_LINK_NVM_MAGIC;
    esp_link_nvm_record_t__record->uint16_t__version = ESP_LINK_NVM_VERSION;
    esp_link_nvm_record_t__record->uint16_t__seq = uint16_t__seq;
    esp_link_nvm_record_t__record->uint16_t__count = uint16_t__count;
    esp_link_nvm_record_t__record->uint16_t__reserved = 0u;
    esp_link_nvm_record_t__record->uint32_t__crc32 =
        func__EspLink_NvmCrc32(
            (const uint8_t *)esp_link_nvm_record_t__record,
            (uint32_t)(sizeof(esp_link_nvm_record_t) - sizeof(uint32_t)));
}

/* ====================================================================
 * ===== EspLink Nvm flash state machine (comm task / pre-scheduler) ==
 * ==================================================================== */

/* [EN] 0 = clean; 1.. = debounce age in comm runs since the last change.
 * [FA] 0 = پاک؛ 1.. = سن دیبانس به تعداد اجرای تسک ارتباط از آخرین تغییر. */
static uint16_t UINT16_T__G__NvmDirtyRuns = 0u;
static uint8_t UINT8_T__G__NvmSaveRetries = 0u;

/* [EN] Sequence of the newest valid record on flash and which page holds it.
 *      Fresh board (no valid record anywhere): the default routes the first
 *      save to page B - arbitrary, harmless, and it keeps the ping-pong rule
 *      "never erase the page holding the newest record" trivially true.
 * [FA] ترتیبِ تازه‌ترین رکورد معتبر روی فلش و صفحهٔ نگهدارندهٔ آن. برد نو
 *      (هیچ رکورد معتبری نیست): پیش‌فرض اولین ذخیره را به صفحهٔ B می‌فرستد
 *      - دلخواه، بی‌ضرر، و قانون پینگ‌پنگ «هرگز صفحهٔ نگهدارندهٔ تازه‌ترین
 *      رکورد را پاک نکن» را به‌سادگی برقرار نگه می‌دارد. */
static uint16_t UINT16_T__G__NvmNewestSeq = 0u;
static bool BOOL__G__NvmNewestIsPageB = false;

/**
 * @brief  [EN] Read one page as a record candidate.
 *         [FA] خواندن یک صفحه به‌عنوان نامزد رکورد.
 */
static const esp_link_nvm_record_t *func__EspLink_NvmPageRecord(
    uint32_t uint32_t__pageAddress)
{
    return (const esp_link_nvm_record_t *)uint32_t__pageAddress;
}

/**
 * @brief  [EN] Boot-time load (see header contract).
 *         [FA] بارگذاری هنگام بوت (قرارداد هدر).
 */
void func__EspLink_NvmInit(void)
{
    const esp_link_nvm_record_t *esp_link_nvm_record_t__pageA =
        func__EspLink_NvmPageRecord(ESP_LINK_NVM_PAGE_A_ADDR);
    const esp_link_nvm_record_t *esp_link_nvm_record_t__pageB =
        func__EspLink_NvmPageRecord(ESP_LINK_NVM_PAGE_B_ADDR);
    const esp_link_nvm_record_t *esp_link_nvm_record_t__newest = NULL;
    bool bool__validA = func__EspLink_NvmRecordValidate(
        esp_link_nvm_record_t__pageA);
    bool bool__validB = func__EspLink_NvmRecordValidate(
        esp_link_nvm_record_t__pageB);
    uint16_t uint16_t__i;

    if ((bool__validA != false) && (bool__validB != false))
    {
        esp_link_nvm_record_t__newest =
            (func__EspLink_NvmSeqCompare(
                 esp_link_nvm_record_t__pageA->uint16_t__seq,
                 esp_link_nvm_record_t__pageB->uint16_t__seq) >= 0)
                ? esp_link_nvm_record_t__pageA
                : esp_link_nvm_record_t__pageB;
    }
    else if (bool__validA != false)
    {
        esp_link_nvm_record_t__newest = esp_link_nvm_record_t__pageA;
    }
    else if (bool__validB != false)
    {
        esp_link_nvm_record_t__newest = esp_link_nvm_record_t__pageB;
    }
    else
    {
        /* [EN] Fresh board or both records corrupt: compiled defaults stay.
           [FA] برد نو یا خرابی هر دو رکورد: پیش‌فرض کامپایل می‌ماند. */
    }

    if (esp_link_nvm_record_t__newest == NULL)
    {
        UINT16_T__G__NvmNewestSeq = 0u;
        BOOL__G__NvmNewestIsPageB = false;
        return;
    }

    /* [EN] Replay through the SAME clamped setters the panel path uses: a
       stale record can only land inside the compiled safety windows, and a
       single bad entry is skipped, never fatal.
       [FA] بازپخش از همان setterهای گیره‌دارِ مسیر پنل: رکورد کهنه فقط
       می‌تواند داخل پنجره‌های ایمنی کامپایل بنشیند و یک ورودی بد رد
       می‌شود، هرگز مهلک نیست. */
    for (uint16_t__i = 0u;
         uint16_t__i < esp_link_nvm_record_t__newest->uint16_t__count;
         uint16_t__i++)
    {
        uint32_t uint32_t__applied = 0u;

        (void)func__EspLink_ApplyParam(
            (uint8_t)esp_link_nvm_record_t__newest
                ->ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__i].uint16_t__id,
            esp_link_nvm_record_t__newest
                ->ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__i].uint32_t__value,
            &uint32_t__applied);
    }

    UINT16_T__G__NvmNewestSeq = esp_link_nvm_record_t__newest->uint16_t__seq;
    BOOL__G__NvmNewestIsPageB =
        (esp_link_nvm_record_t__newest == esp_link_nvm_record_t__pageB);
}

/**
 * @brief  [EN] Mark the persisted configuration dirty (see header contract).
 *         [FA] کثیف‌علامت‌کردن پیکربندی ذخیره‌شونده (قرارداد هدر).
 */
void func__EspLink_NvmMarkDirty(uint8_t uint8_t__paramId)
{
    if (func__EspLink_NvmParamPersisted(uint8_t__paramId) != false)
    {
        UINT16_T__G__NvmDirtyRuns = 1u;
    }
}

/**
 * @brief  [EN] Snapshot every persisted parameter and write the ping-pong
 *              record to the page that does NOT hold the newest one, then
 *              verify by read-back.
 *         [FA] عکس‌فوری همهٔ پارامترهای ذخیره‌شونده و نوشتن رکورد پینگ‌پنگ
 *              در صفحه‌ای که تازه‌ترین را ندارد، بعد صحت‌سنجی با بازخوانی.
 * @return bool [EN] true = record now on flash / رکورد روی فلش است
 */
static bool func__EspLink_NvmSaveNow(void)
{
    esp_link_nvm_record_t esp_link_nvm_record_t__record;
    esp_link_nvm_entry_t ESP_LINK_NVM_ENTRY_T__A__Entry[ESP_LINK_NVM_ENTRY_MAX];
    uint16_t uint16_t__count = 0u;
    uint16_t uint16_t__i;
    uint32_t uint32_t__pageAddress;

    for (uint16_t__i = 0u; uint16_t__i < (uint16_t)ESPLINK_PARAM_COUNT;
         uint16_t__i++)
    {
        uint32_t uint32_t__value = 0u;

        if ((func__EspLink_NvmParamPersisted((uint8_t)uint16_t__i) != false) &&
            (func__EspLink_GetParam((uint8_t)uint16_t__i, &uint32_t__value) !=
             false))
        {
            ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__count].uint16_t__id =
                uint16_t__i;
            ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__count].uint16_t__pad = 0u;
            ESP_LINK_NVM_ENTRY_T__A__Entry[uint16_t__count].uint32_t__value =
                uint32_t__value;
            uint16_t__count++;
        }
    }

    if (uint16_t__count == 0u)
    {
        return false;
    }

    func__EspLink_NvmRecordBuild(&esp_link_nvm_record_t__record,
                                 (uint16_t)(UINT16_T__G__NvmNewestSeq + 1u),
                                 ESP_LINK_NVM_ENTRY_T__A__Entry,
                                 uint16_t__count);

    /* [EN] Ping-pong: never touch the page that holds the only good record.
       [FA] پینگ‌پنگ: هرگز صفحه‌ای را که تنها رکورد خوب را دارد نمی‌آزیم. */
    uint32_t__pageAddress = (BOOL__G__NvmNewestIsPageB != false)
                                ? ESP_LINK_NVM_PAGE_A_ADDR
                                : ESP_LINK_NVM_PAGE_B_ADDR;

    if (func__BspFlash_ErasePage(uint32_t__pageAddress) == false)
    {
        return false;
    }

    if (func__BspFlash_ProgramHalfWords(
            uint32_t__pageAddress,
            (const uint16_t *)&esp_link_nvm_record_t__record,
            (uint32_t)(sizeof(esp_link_nvm_record_t) / sizeof(uint16_t))) ==
        false)
    {
        return false;
    }

    for (uint16_t__i = 0u;
         uint16_t__i < (uint16_t)(sizeof(esp_link_nvm_record_t) /
                                  sizeof(uint16_t));
         uint16_t__i++)
    {
        if (((const volatile uint16_t *)uint32_t__pageAddress)[uint16_t__i] !=
            ((const uint16_t *)&esp_link_nvm_record_t__record)[uint16_t__i])
        {
            return false;
        }
    }

    UINT16_T__G__NvmNewestSeq =
        esp_link_nvm_record_t__record.uint16_t__seq;
    BOOL__G__NvmNewestIsPageB =
        (uint32_t__pageAddress == ESP_LINK_NVM_PAGE_B_ADDR);
    return true;
}

/**
 * @brief  [EN] Comm-task housekeeping (see header contract).
 *         [FA] نگهداری تسک ارتباط (قرارداد هدر).
 */
void func__EspLink_NvmTick(void)
{
    if (UINT16_T__G__NvmDirtyRuns == 0u)
    {
        return;
    }

    UINT16_T__G__NvmDirtyRuns++;

    if (UINT16_T__G__NvmDirtyRuns < (uint16_t)ESP_LINK_NVM_SAVE_DELAY_RUNS)
    {
        return;
    }

    if (func__EspLink_NvmSaveNow() != false)
    {
        UINT16_T__G__NvmDirtyRuns = 0u;
        UINT8_T__G__NvmSaveRetries = 0u;
        return;
    }

    if (UINT8_T__G__NvmSaveRetries < (uint8_t)ESP_LINK_NVM_SAVE_RETRIES)
    {
        UINT8_T__G__NvmSaveRetries++;
        UINT16_T__G__NvmDirtyRuns = (uint16_t)ESP_LINK_NVM_SAVE_DELAY_RUNS;
    }
    else
    {
        UINT16_T__G__NvmDirtyRuns = 0u;
    }
}
