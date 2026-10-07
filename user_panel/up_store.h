/**
 * @file    up_store.h
 * @brief   [EN] The panel's own flash journal: sample ring, event ring, daily
 *              rollups and lifetime totals on LittleFS. Every ring has a byte
 *              budget derived from the REAL flash size, and when a budget is
 *              reached the OLDEST records are dropped first - exactly what the
 *              user asked for ("if it fills up, delete the old ones").
 *          [FA] دفتر فلش خود پنل: حلقهٔ نمونه، حلقهٔ رویداد، آمار روزانه و جمع
 *              کل روی LittleFS. هر حلقه سهمیهٔ بایتی دارد که از حجم واقعی فلش
 *              مشتق می‌شود و با پر شدن، اول قدیمی‌ترین رکوردها حذف می‌شوند -
 *              دقیقاً همان چیزی که کاربر خواست.
 *
 * @note    [EN] Records are written through a streaming compactor: nothing is
 *              ever buffered whole, so the RAM cost of the store is constant no
 *              matter how much history the flash holds. The last two minutes of
 *              daily/total counters may be lost on a power cut; raw samples are
 *              flushed as they are written.
 *          [FA] رکوردها از یک فشرده‌ساز جریانی عبور می‌کنند و هیچ‌وقت کل فایل
 *              در RAM نمی‌آید، پس هزینهٔ RAM ثابت است. حداکثر دو دقیقهٔ آخرِ
 *              شمارنده‌های روزانه/کل ممکن است با قطع برق از دست برود؛ نمونه‌های
 *              خام همان لحظه نوشته می‌شوند.
 */

#ifndef UP_STORE_H
#define UP_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <LittleFS.h>
#include "up_config.h"
#include "up_state.h"

/* ==================== Types / تایپ‌ها ==================== */
/* [EN] Ring file prefix: 8 bytes (magic + version). The record count is derived
   from the file size, so a power cut can never leave a stale count behind.
   [FA] سرآیند فایل حلقه: ۸ بایت. تعداد رکورد از حجم فایل حساب می‌شود تا قطع
   برق هیچ‌وقت شمارندهٔ کهنه جا نگذارد. */
#define UP_RING_MAGIC        0x55505249u   /* [EN] "UPRI" / [FA] «UPRI» */
#define UP_RING_PREFIX_BYTES 8u

#define UP_RING_KEEP_PERCENT 75u           /* [EN] kept share on compaction / [FA] سهم باقی‌مانده */

typedef struct
{
    bool     bool__open;
    uint32_t uint32_t__count;
    uint32_t uint32_t__index;
    File     up_file_t__file;
} up_ring_reader_t;

/* ==================== State / وضعیت ==================== */
static uint32_t     UINT32_T__G__SampleCapBytes = 0u;
static uint32_t     UINT32_T__G__EventCapBytes = 0u;
static uint32_t     UINT32_T__G__DailySlots = 0u;
static uint32_t     UINT32_T__G__DailyTodayIndex = 0u;
static up_daily_t   UPPANEL_DAILY_T__G__Today;
static bool         BOOL__G__DailyValid = false;
static bool         BOOL__G__DailyDirty = false;
static bool         BOOL__G__TotalsDirty = false;
static uint32_t     UINT32_T__G__LastFlushMs = 0u;
static uint32_t     UINT32_T__G__PurgeCount = 0u;

/* [EN] Forward declarations: the lifecycle function at the top of this file
   calls loaders defined further down, and the rest of the file is in call
   order on purpose so a reader never jumps backwards to find a callee.
   [FA] اعلان جلوتر: تابع چرخهٔ عمر بالای این فایل، خواننده‌هایی را صدا می‌زند
   که پایین‌تر تعریف شده‌اند؛ بقیهٔ فایل عمداً به ترتیب فراخوان است تا خواننده
   برای یافتن تابع صدا‌زده‌شده به عقب برنگردد. */
static bool func__UpStore_EnsureRing(const char *char__path, uint32_t uint32_t__recordSize);
static bool func__UpStore_TotalsLoad(void);
static bool func__UpStore_DailyLoadToday(void);
static bool func__UpStore_DailyFlush(void);

/* ==================== Small answers / پاسخ‌های کوچک ==================== */
/**
 * @brief  [EN] Real filesystem size in bytes.
 *         [FA] حجم واقعی فایل‌سیستم.
 * @return [EN] bytes / [FA] بایت
 */
static uint32_t func__UpStore_TotalBytes(void)
{
    return (uint32_t)LittleFS.totalBytes();
}

/**
 * @brief  [EN] Bytes currently in use.
 *         [FA] بایت‌های مصرف‌شده.
 * @return [EN] bytes / [FA] بایت
 */
static uint32_t func__UpStore_UsedBytes(void)
{
    size_t size_t__used = LittleFS.usedBytes();
    return (uint32_t)size_t__used;
}

/**
 * @brief  [EN] How many records a ring file currently holds (count derived from
 *              size, so it is always true even after an unclean restart).
 *         [FA] تعداد رکوردهای فعلی یک فایل حلقه (از حجم حساب می‌شود، پس بعد از
 *              ری‌استارت ناگهانی هم درست است).
 * @param  char__path [EN] file path / [FA] مسیر فایل
 * @param  uint32_t__recordSize [EN] bytes per record / [FA] بایت هر رکورد
 * @return [EN] record count / [FA] تعداد رکورد
 */
static uint32_t func__UpStore_FileCount(const char *char__path, uint32_t uint32_t__recordSize)
{
    File up_file_t__f = LittleFS.open(char__path, "r");
    uint32_t uint32_t__size;

    if (!up_file_t__f)
    {
        return 0u;
    }

    uint32_t__size = (uint32_t)up_file_t__f.size();
    up_file_t__f.close();

    if (uint32_t__size < UP_RING_PREFIX_BYTES)
    {
        return 0u;
    }

    return ((uint32_t__size - UP_RING_PREFIX_BYTES) / uint32_t__recordSize);
}

/**
 * @brief  [EN] Create a ring file with a fresh prefix when it does not exist.
 *         [FA] ساختن فایل حلقه با سرآیند تازه وقتی وجود ندارد.
 * @param  char__path [EN] file path / [FA] مسیر فایل
 * @return [EN] true when the file exists afterwards / [FA] اگر فایل موجود باشد true
 */
static bool func__UpStore_EnsureRing(const char *char__path, uint32_t uint32_t__recordSize)
{
    uint32_t uint32_t__magic = UP_RING_MAGIC;
    uint32_t uint32_t__version = 1u;
    File up_file_t__f;
    size_t size_t__got;

    up_file_t__f = LittleFS.open(char__path, "r");
    if (up_file_t__f)
    {
        uint32_t uint32_t__size = (uint32_t)up_file_t__f.size();
        size_t__got = up_file_t__f.read((uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic));
        up_file_t__f.close();

        /* [EN] The file is usable only when the magic is right AND the payload
           is a whole number of records: a power cut in the middle of an append
           leaves a bad tail, and continuing to write after it would put every
           later read half a record off - the one corruption that would quietly
           turn history into nonsense instead of failing loudly.
           [FA] فایل فقط وقتی قابل استفاده است که مهر درست باشد و بدنه مضرب
           صحیحی از رکورد باشد: قطع برق در میانهٔ یک افزودن دنبالهٔ خراب
           می‌گذارد و ادامهٔ نوشتن بعد از آن هر خواندن بعدی را نصفِ رکورد
           جابه‌جا می‌کند - همان خرابی‌ای که تاریخچه را بی‌صدا مزخرف می‌کند نه
           اینکه با صدای بلند شکست بخورد. */
        bool bool__magicOk = ((size_t__got == sizeof(uint32_t__magic)) && (uint32_t__magic == UP_RING_MAGIC));
        bool bool__aligned = ((uint32_t__recordSize != 0u) &&
                              (uint32_t__size >= UP_RING_PREFIX_BYTES) &&
                              (((uint32_t__size - UP_RING_PREFIX_BYTES) % uint32_t__recordSize) == 0u));
        if (bool__magicOk && bool__aligned)
        {
            return true;
        }
        LittleFS.remove(char__path);
    }

    up_file_t__f = LittleFS.open(char__path, "w");
    if (!up_file_t__f)
    {
        return false;
    }

    uint32_t__magic = UP_RING_MAGIC;
    uint32_t__version = 1u;
    (void)up_file_t__f.write((const uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic));
    (void)up_file_t__f.write((const uint8_t *)&uint32_t__version, sizeof(uint32_t__version));
    up_file_t__f.close();

    return true;
}

/* ==================== Compaction / فشرده‌سازی ==================== */
/**
 * @brief  [EN] Streaming compaction: keeps the NEWEST `keepPercent` of a ring,
 *              copies it through a small stack buffer into a temporary file,
 *              then swaps the files. This is the "delete the oldest when full"
 *              mechanism at its core - the copy direction is chosen so the
 *              oldest records are the ones that disappear.
 *         [FA] فشرده‌سازی جریانی: جدیدترین «keepPercent» درصد حلقه را نگه
 *              می‌دارد، با یک بافر کوچک روی پشته در فایل موقت می‌ریزد و بعد
 *              فایل‌ها جابه‌جا می‌شوند. همین هستهٔ سازوکار «پر شد، قدیمی را
 *              پاک کن» است - جهت کپی طوری است که قدیمی‌ها حذف شوند.
 * @param  char__path [EN] ring file / [FA] فایل حلقه
 * @param  uint32_t__recordSize [EN] bytes per record / [FA] بایت هر رکورد
 * @return [EN] true when the ring is valid afterwards / [FA] اگر حلقه سالم بماند true
 */
static bool func__UpStore_CompactRing(const char *char__path, uint32_t uint32_t__recordSize)
{
    char char__tmpPath[32];
    uint32_t uint32_t__magic = UP_RING_MAGIC;
    uint32_t uint32_t__version = 1u;
    uint32_t uint32_t__count = func__UpStore_FileCount(char__path, uint32_t__recordSize);
    uint32_t uint32_t__keep = (uint32_t__count * UP_RING_KEEP_PERCENT) / 100u;
    uint32_t uint32_t__skip = uint32_t__count - uint32_t__keep;
    uint32_t uint32_t__skipBytes = uint32_t__skip * uint32_t__recordSize;
    uint8_t uint8_t__chunk[64];
    File up_file_t__src;
    File up_file_t__dst;

    if (uint32_t__count == 0u)
    {
        return func__UpStore_EnsureRing(char__path, uint32_t__recordSize);
    }

    snprintf(char__tmpPath, sizeof(char__tmpPath), "%s.tmp", char__path);

    up_file_t__src = LittleFS.open(char__path, "r");
    if (!up_file_t__src)
    {
        return false;
    }
    up_file_t__dst = LittleFS.open(char__tmpPath, "w");
    if (!up_file_t__dst)
    {
        up_file_t__src.close();
        return false;
    }

    (void)up_file_t__src.seek(UP_RING_PREFIX_BYTES + uint32_t__skipBytes);
    (void)up_file_t__dst.write((const uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic));
    (void)up_file_t__dst.write((const uint8_t *)&uint32_t__version, sizeof(uint32_t__version));

    uint32_t uint32_t__remaining = uint32_t__keep * uint32_t__recordSize;
    while (uint32_t__remaining > 0u)
    {
        uint32_t uint32_t__take = uint32_t__remaining;
        if (uint32_t__take > sizeof(uint8_t__chunk))
        {
            uint32_t__take = sizeof(uint8_t__chunk);
        }

        size_t size_t__got = up_file_t__src.read(uint8_t__chunk, uint32_t__take);
        if (size_t__got == 0u)
        {
            break;
        }

        (void)up_file_t__dst.write(uint8_t__chunk, size_t__got);
        uint32_t__remaining -= (uint32_t)size_t__got;
    }

    up_file_t__src.close();
    up_file_t__dst.close();

    LittleFS.remove(char__path);
    LittleFS.rename(char__tmpPath, char__path);
    UINT32_T__G__PurgeCount++;

    return true;
}

/* ==================== Append / افزودن ==================== */
/**
 * @brief  [EN] Append one record to a ring; compacts first when the byte budget
 *              says the oldest records must go.
 *         [FA] افزودن یک رکورد به حلقه؛ اگر سهمیهٔ بایتی بگوید قدیمی‌ها باید
 *              بروند، اول فشرده می‌کند.
 * @param  char__path [EN] ring file / [FA] فایل حلقه
 * @param  void__record [EN] pointer to the record / [FA] اشاره‌گر رکورد
 * @param  uint32_t__recordSize [EN] bytes / [FA] بایت
 * @param  uint32_t__capBytes [EN] budget for this ring / [FA] سهمیهٔ این حلقه
 * @return [EN] true when written / [FA] در صورت نوشتن true
 */
static bool func__UpStore_RingAppend(const char *char__path, const void *void__record,
                                     uint32_t uint32_t__recordSize, uint32_t uint32_t__capBytes)
{
    File up_file_t__f;
    uint32_t uint32_t__count;

    if (!func__UpStore_EnsureRing(char__path, uint32_t__recordSize))
    {
        return false;
    }

    uint32_t__count = func__UpStore_FileCount(char__path, uint32_t__recordSize);
    if (((uint32_t__count + 1u) * uint32_t__recordSize) > uint32_t__capBytes)
    {
        if (!func__UpStore_CompactRing(char__path, uint32_t__recordSize))
        {
            return false;
        }
    }

    up_file_t__f = LittleFS.open(char__path, "a");
    if (!up_file_t__f)
    {
        return false;
    }

    size_t size_t__written = up_file_t__f.write((const uint8_t *)void__record, uint32_t__recordSize);
    up_file_t__f.close();

    return (size_t__written == uint32_t__recordSize);
}

/* ==================== Lifecycle / چرخهٔ عمر ==================== */
/**
 * @brief  [EN] Mount LittleFS, size every ring from the real flash, load totals
 *              and today's daily row.
 *         [FA] سوار کردن LittleFS، اندازه‌گیری هر حلقه از فلش واقعی، خواندن
 *              جمع کل و ردیف امروز.
 * @return [EN] true when the store is usable / [FA] اگر انبار قابل استفاده باشد true
 */
static bool func__UpStore_Begin(void)
{
    uint32_t uint32_t__total;
    uint32_t uint32_t__budget;

    UPPANEL_STATE_T__G__State.bool__storageOk = LittleFS.begin();
    if (!UPPANEL_STATE_T__G__State.bool__storageOk)
    {
        return false;
    }

    uint32_t__total = func__UpStore_TotalBytes();
    UPPANEL_STATE_T__G__State.uint32_t__storageTotal = uint32_t__total;

    uint32_t__budget = (uint32_t__total > UP_FS_RESERVE_BYTES) ? (uint32_t__total - UP_FS_RESERVE_BYTES) : 0u;

    UINT32_T__G__SampleCapBytes = (uint32_t__budget * UP_FS_SAMPLE_SHARE) / 100u;
    UINT32_T__G__EventCapBytes = (uint32_t__budget * UP_FS_EVENT_SHARE) / 100u;
    if (UINT32_T__G__SampleCapBytes < UP_FS_RING_MIN_BYTES)
    {
        UINT32_T__G__SampleCapBytes = UP_FS_RING_MIN_BYTES;
    }
    if (UINT32_T__G__EventCapBytes < UP_FS_RING_MIN_BYTES)
    {
        UINT32_T__G__EventCapBytes = UP_FS_RING_MIN_BYTES;
    }

    UINT32_T__G__DailySlots = (uint32_t__budget * UP_FS_DAILY_SHARE) / 100u / (uint32_t)sizeof(up_daily_t);
    if (UINT32_T__G__DailySlots < 32u)
    {
        UINT32_T__G__DailySlots = 32u;
    }

    (void)func__UpStore_EnsureRing(UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t));
    (void)func__UpStore_EnsureRing(UP_F_EVENTS, (uint32_t)sizeof(up_event_t));

    UINT32_T__G__PurgeCount = 0u;
    UINT32_T__G__LastFlushMs = (uint32_t)millis();

    (void)func__UpStore_TotalsLoad();
    (void)func__UpStore_DailyLoadToday();

    return true;
}

/* ==================== Samples / نمونه‌ها ==================== */
/**
 * @brief  [EN] Store one sample (called on the 15 s cadence and at every event
 *              boundary so charts never miss a step).
 *         [FA] ذخیرهٔ یک نمونه (با بازهٔ ۱۵ ثانیه و در هر مرز رویداد صدا زده
 *              می‌شود تا نمودار هیچ پله‌ای را از دست ندهد).
 * @param  up_sample_t__sample [EN] filled by the caller / [FA] توسط فراخوان پر می‌شود
 * @return [EN] true when written / [FA] در صورت نوشتن true
 */
static bool func__UpStore_AppendSample(const up_sample_t *up_sample_t__sample)
{
    return func__UpStore_RingAppend(UP_F_SAMPLES, up_sample_t__sample, (uint32_t)sizeof(up_sample_t), UINT32_T__G__SampleCapBytes);
}

/**
 * @brief  [EN] Store one closed event / [FA] ذخیرهٔ یک رویداد بسته‌شده
 * @param  up_event_t__event [EN] fully filled record / [FA] رکورد کامل
 * @return [EN] true when written / [FA] در صورت نوشتن true
 */
static bool func__UpStore_AppendEvent(const up_event_t *up_event_t__event)
{
    return func__UpStore_RingAppend(UP_F_EVENTS, up_event_t__event, (uint32_t)sizeof(up_event_t), UINT32_T__G__EventCapBytes);
}

/**
 * @brief  [EN] Total stored samples / [FA] تعداد کل نمونه‌های ذخیره‌شده
 * @return [EN] count / [FA] تعداد
 */
static uint32_t func__UpStore_SampleCount(void)
{
    return func__UpStore_FileCount(UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t));
}

/**
 * @brief  [EN] Total stored events / [FA] تعداد کل رویدادهای ذخیره‌شده
 * @return [EN] count / [FA] تعداد
 */
static uint32_t func__UpStore_EventCount(void)
{
    return func__UpStore_FileCount(UP_F_EVENTS, (uint32_t)sizeof(up_event_t));
}

/* ==================== Ring readers / خواننده‌های حلقه ==================== */
/**
 * @brief  [EN] Put a reader into a known-empty state WITHOUT memset: the struct
 *              owns a File, and on the real board a File holds a shared handle
 *              - zeroing its memory would leak or corrupt it. The host build's
 *              -Werror caught exactly that mistake, which is why this function
 *              exists instead of the memset that used to be at each call site.
 *         [FA] بردن خواننده به وضعیت خالیِ معلوم، «بدون» memset: این ساختار
 *              صاحب یک File است و روی برد واقعی File یک دستهٔ اشتراکی دارد -
 *              صفر کردن حافظه‌اش نشت یا خرابی می‌آورد. بیلد هاست با -Werror
 *              دقیقاً همین اشتباه را گرفت و همین دلیل وجود این تابع است، به‌جای
 *              memsetی که قبلاً در هر محل فراخوان بود.
 * @param  up_ring_reader_t__reader [EN] reader to reset / [FA] خواننده‌ای که صفر شود
 * @return [EN] None / [FA] ندارد
 */
static void func__UpStore_ReaderInit(up_ring_reader_t *up_ring_reader_t__reader)
{
    up_ring_reader_t__reader->bool__open = false;
    up_ring_reader_t__reader->uint32_t__count = 0u;
    up_ring_reader_t__reader->uint32_t__index = 0u;
}
/**
 * @brief  [EN] Open a ring for sequential reading.
 *         [FA] باز کردن حلقه برای خواندن ترتیبی.
 * @param  up_ring_reader_t__reader [EN] reader state / [FA] وضعیت خواننده
 * @param  char__path [EN] ring file / [FA] فایل حلقه
 * @param  uint32_t__recordSize [EN] bytes per record / [FA] بایت هر رکورد
 * @return [EN] true when open / [FA] در صورت باز شدن true
 */
static bool func__UpStore_ReaderOpen(up_ring_reader_t *up_ring_reader_t__reader, const char *char__path, uint32_t uint32_t__recordSize)
{
    if (up_ring_reader_t__reader->bool__open)
    {
        up_ring_reader_t__reader->up_file_t__file.close();
        up_ring_reader_t__reader->bool__open = false;
    }

    up_ring_reader_t__reader->up_file_t__file = LittleFS.open(char__path, "r");
    if (!up_ring_reader_t__reader->up_file_t__file)
    {
        return false;
    }

    up_ring_reader_t__reader->bool__open = true;
    up_ring_reader_t__reader->uint32_t__count = func__UpStore_FileCount(char__path, uint32_t__recordSize);
    up_ring_reader_t__reader->uint32_t__index = 0u;

    return up_ring_reader_t__reader->up_file_t__file.seek(UP_RING_PREFIX_BYTES);
}

/**
 * @brief  [EN] Read the next record forward, caller keeps the record size.
 *         [FA] خواندن رکورد بعدی به جلو؛ اندازهٔ رکورد را فراخوان می‌داند.
 * @param  up_ring_reader_t__reader [EN] reader / [FA] خواننده
 * @param  void__out [EN] destination / [FA] مقصد
 * @param  uint32_t__recordSize [EN] bytes / [FA] بایت
 * @return [EN] true when a record was read / [FA] در صورت خواندن true
 */
static bool func__UpStore_ReaderNext(up_ring_reader_t *up_ring_reader_t__reader, void *void__out, uint32_t uint32_t__recordSize)
{
    size_t size_t__got;

    if (!up_ring_reader_t__reader->bool__open)
    {
        return false;
    }
    if (up_ring_reader_t__reader->uint32_t__index >= up_ring_reader_t__reader->uint32_t__count)
    {
        return false;
    }

    size_t__got = up_ring_reader_t__reader->up_file_t__file.read((uint8_t *)void__out, uint32_t__recordSize);
    up_ring_reader_t__reader->uint32_t__index++;

    return (size_t__got == uint32_t__recordSize);
}

/**
 * @brief  [EN] Close a reader / [FA] بستن خواننده
 * @return [EN] None / [FA] ندارد
 */
static void func__UpStore_ReaderClose(up_ring_reader_t *up_ring_reader_t__reader)
{
    if (up_ring_reader_t__reader->bool__open)
    {
        up_ring_reader_t__reader->up_file_t__file.close();
        up_ring_reader_t__reader->bool__open = false;
    }
}

/* ==================== Totals / جمع کل ==================== */
/**
 * @brief  [EN] Load lifetime totals; a missing or foreign file starts a clean
 *              one instead of pretending the panel has a history it does not.
 *         [FA] خواندن جمع کل؛ فایل نبود/غریبه، جمع تازه می‌سازد نه اینکه
 *              ادعای تاریخچه‌ای بکند که ندارد.
 * @return [EN] true when an existing file was loaded / [FA] در صورت خواندن فایل موجود true
 */
static bool func__UpStore_TotalsLoad(void)
{
    File up_file_t__f = LittleFS.open(UP_F_TOTALS, "r");
    bool bool__ok = false;

    if (up_file_t__f)
    {
        size_t size_t__got = up_file_t__f.read((uint8_t *)&UPPANEL_STATE_T__G__State.up_totals_t__totals, sizeof(up_totals_t));
        up_file_t__f.close();
        bool__ok = (size_t__got == sizeof(up_totals_t)) &&
                   (UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__magic == UP_TOTALS_MAGIC);
    }

    if (!bool__ok)
    {
        memset(&UPPANEL_STATE_T__G__State.up_totals_t__totals, 0, sizeof(up_totals_t));
        UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__magic = UP_TOTALS_MAGIC;
        UPPANEL_STATE_T__G__State.up_totals_t__totals.uint16_t__minV = 0xFFFFu;
    }

    BOOL__G__TotalsDirty = false;
    return bool__ok;
}

/**
 * @brief  [EN] Write lifetime totals back to flash.
 *         [FA] نوشتن جمع کل روی فلش.
 * @return [EN] true on success / [FA] در صورت موفقیت true
 */
static bool func__UpStore_TotalsSave(void)
{
    File up_file_t__f;

    LittleFS.remove(UP_F_TOTALS);
    up_file_t__f = LittleFS.open(UP_F_TOTALS, "w");
    if (!up_file_t__f)
    {
        return false;
    }

    size_t size_t__written = up_file_t__f.write((const uint8_t *)&UPPANEL_STATE_T__G__State.up_totals_t__totals, sizeof(up_totals_t));
    up_file_t__f.close();
    BOOL__G__TotalsDirty = false;

    return (size_t__written == sizeof(up_totals_t));
}

/**
 * @brief  [EN] Point totals at the panel state so callers can read them.
 *         [FA] برگرداندن جمع کل.
 * @return [EN] pointer to the totals / [FA] اشاره‌گر جمع کل
 */
static up_totals_t *func__UpStore_Totals(void)
{
    return &UPPANEL_STATE_T__G__State.up_totals_t__totals;
}

/* ==================== Daily rollups / آمار روزانه ==================== */
/**
 * @brief  [EN] Offset of a day's slot inside the daily file (ring of slots).
 *         [FA] جایگاه بایت یک روز داخل فایل روزانه (حلقهٔ خانه‌ها).
 * @param  uint32_t__dayIndex [EN] days since 2000-01-01 / [FA] روز از مبدأ
 * @return [EN] byte offset / [FA] آفست بایت
 */
static uint32_t func__UpStore_DailyOffset(uint32_t uint32_t__dayIndex)
{
    uint32_t uint32_t__slot = uint32_t__dayIndex % UINT32_T__G__DailySlots;
    return (uint32_t__slot * (uint32_t)sizeof(up_daily_t));
}

/**
 * @brief  [EN] Read today's daily row into RAM (or start a fresh one).
 *         [FA] خواندن ردیف امروز در RAM (یا ساخت ردیف تازه).
 * @return [EN] true when an existing row was loaded / [FA] در صورت خواندن ردیف موجود true
 */
static bool func__UpStore_DailyLoadToday(void)
{
    File up_file_t__f;
    uint32_t uint32_t__dayIndex = func__UpState_DayIndex();
    bool bool__ok = false;

    memset(&UPPANEL_DAILY_T__G__Today, 0, sizeof(up_daily_t));
    UPPANEL_DAILY_T__G__Today.uint32_t__dayIndex = uint32_t__dayIndex;
    UPPANEL_DAILY_T__G__Today.uint16_t__minV = 0xFFFFu;
    UINT32_T__G__DailyTodayIndex = uint32_t__dayIndex;
    BOOL__G__DailyValid = false;
    BOOL__G__DailyDirty = false;

    up_file_t__f = LittleFS.open(UP_F_DAILY, "r");
    if (up_file_t__f)
    {
        (void)up_file_t__f.seek(func__UpStore_DailyOffset(uint32_t__dayIndex));
        size_t size_t__got = up_file_t__f.read((uint8_t *)&UPPANEL_DAILY_T__G__Today, sizeof(up_daily_t));
        up_file_t__f.close();
        bool__ok = (size_t__got == sizeof(up_daily_t)) &&
                   (UPPANEL_DAILY_T__G__Today.uint32_t__dayIndex == uint32_t__dayIndex);
        if (!bool__ok)
        {
            memset(&UPPANEL_DAILY_T__G__Today, 0, sizeof(up_daily_t));
            UPPANEL_DAILY_T__G__Today.uint32_t__dayIndex = uint32_t__dayIndex;
            UPPANEL_DAILY_T__G__Today.uint16_t__minV = 0xFFFFu;
        }
    }

    BOOL__G__DailyValid = bool__ok;
    return bool__ok;
}

/**
 * @brief  [EN] Roll to a new day when midnight passes, flushing yesterday first.
 *         [FA] با گذر از نیمه‌شب به روز تازه می‌رود و اول دیروز را ذخیره می‌کند.
 * @return [EN] true when the day changed / [FA] در صورت تغییر روز true
 */
static bool func__UpStore_DailyRollIfNeeded(void)
{
    uint32_t uint32_t__dayIndex = func__UpState_DayIndex();

    if (uint32_t__dayIndex == UINT32_T__G__DailyTodayIndex)
    {
        return false;
    }

    (void)func__UpStore_DailyFlush();
    (void)func__UpStore_DailyLoadToday();
    return true;
}

/**
 * @brief  [EN] Write today's row to its slot.
 *         [FA] نوشتن ردیف امروز در خانه‌اش.
 * @return [EN] true on success / [FA] در صورت موفقیت true
 */
/**
 * @brief  [EN] Make the daily file long enough to hold one given slot, by
 *              growing it with zero bytes. The slots are addressed by OFFSET,
 *              so a write must land at an offset that exists - a filesystem
 *              that refuses to seek past the end would otherwise leave the
 *              write at position zero, silently overwriting the first day with
 *              today's row. Growth is lazy: the file only becomes as long as
 *              the newest slot written so far.
 *         [FA] بلندتر کردن فایل روزانه تا گنجایش یک خانهٔ مشخص، با رشد دادنش
 *              به‌وسیلهٔ بایت‌های صفر. خانه‌ها با «آفست» آدرس‌دهی می‌شوند، پس یک
 *              نوشتن باید روی آفستی بنشیند که وجود دارد - وگرنه فایل‌سیستمی که
 *              جست‌وجو بیرون از انتها را نپذیرد، نوشتن را در موقعیت صفر می‌گذارد
 *              و بی‌صدا روز اول را با ردیف امروز بازنویسی می‌کند. رشد تنبل است:
 *              فایل فقط به اندازهٔ جدیدترین خانهٔ نوشته‌شده بلند می‌شود.
 * @param  uint32_t__needBytes [EN] required length / [FA] طول لازم
 * @return [EN] true when the file is at least that long / [FA] اگر فایل دست‌کم آن‌قدر باشد true
 */
static bool func__UpStore_DailyEnsureRoom(uint32_t uint32_t__needBytes)
{
    static const uint8_t UINT8_T__A__Zeros[32] = { 0u };
    File up_file_t__f;
    bool bool__ok = true;

    up_file_t__f = LittleFS.open(UP_F_DAILY, "r+");
    if (!up_file_t__f)
    {
        return false;
    }

    while ((uint32_t)up_file_t__f.size() < uint32_t__needBytes)
    {
        uint32_t uint32_t__missing = uint32_t__needBytes - (uint32_t)up_file_t__f.size();
        uint32_t uint32_t__chunk = (uint32_t__missing > (uint32_t)sizeof(UINT8_T__A__Zeros))
                                 ? (uint32_t)sizeof(UINT8_T__A__Zeros)
                                 : uint32_t__missing;

        (void)up_file_t__f.seek((uint32_t)up_file_t__f.size());   /* [EN] the end is always addressable */
        if (up_file_t__f.write(UINT8_T__A__Zeros, (size_t)uint32_t__chunk) != (size_t)uint32_t__chunk)
        {
            bool__ok = false;
            break;
        }
    }

    up_file_t__f.close();
    return bool__ok;
}

static bool func__UpStore_DailyFlush(void)
{
    File up_file_t__f;
    size_t size_t__written = 0u;
    uint32_t uint32_t__offset;

    if (!BOOL__G__DailyDirty)
    {
        return true;
    }

    if (!LittleFS.exists(UP_F_DAILY))
    {
        File up_file_t__create = LittleFS.open(UP_F_DAILY, "w");
        if (!up_file_t__create)
        {
            return false;
        }
        up_file_t__create.close();
    }

    uint32_t__offset = func__UpStore_DailyOffset(UPPANEL_DAILY_T__G__Today.uint32_t__dayIndex);

    /* [EN] Grow first, then write: never seek into a hole that is not there.
       [FA] اول رشد، بعد نوشتن: هرگز داخل حفره‌ای که نیست جست‌وجو نکن. */
    if (!func__UpStore_DailyEnsureRoom(uint32_t__offset + (uint32_t)sizeof(up_daily_t)))
    {
        return false;
    }

    up_file_t__f = LittleFS.open(UP_F_DAILY, "r+");
    if (!up_file_t__f)
    {
        return false;
    }

    /* [EN] A failed seek must NOT be followed by a write: that write would land
            at the start of the file and overwrite somebody else's day.
       [FA] جست‌وجوی ناموفق نباید با نوشتن ادامه یابد: آن نوشتن در ابتدای فایل
            می‌نشیند و روزِ دیگری را بازنویسی می‌کند. */
    if (up_file_t__f.seek(uint32_t__offset))
    {
        size_t__written = up_file_t__f.write((const uint8_t *)&UPPANEL_DAILY_T__G__Today, sizeof(up_daily_t));
    }
    up_file_t__f.close();

    if (size_t__written == sizeof(up_daily_t))
    {
        BOOL__G__DailyDirty = false;
    }

    return (size_t__written == sizeof(up_daily_t));
}

/**
 * @brief  [EN] Pointer to the live daily row so the history logic can add to it.
 *         [FA] اشاره‌گر ردیف روز جاری تا منطق تاریخچه به آن اضافه کند.
 * @return [EN] pointer / [FA] اشاره‌گر
 */
static up_daily_t *func__UpStore_DailyToday(void)
{
    return &UPPANEL_DAILY_T__G__Today;
}

/**
 * @brief  [EN] Mark the daily row dirty (called by every accumulator).
 *         [FA] علامت‌گذاری ردیف روز به‌عنوان تغییر‌یافته.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpStore_DailyMarkDirty(void)
{
    BOOL__G__DailyDirty = true;
}

/**
 * @brief  [EN] Read the daily row for a given day index, if the slot still
 *              holds that day (older days were overwritten by the ring).
 *         [FA] خواندن ردیف روزانهٔ یک روز مشخص، اگر خانه هنوز همان روز را
 *              داشته باشد (روزهای قدیمی‌تر با حلقه بازنویسی شده‌اند).
 * @param  uint32_t__dayIndex [EN] days since 2000-01-01 / [FA] روز از مبدأ
 * @param  up_daily_t__out [EN] destination / [FA] مقصد
 * @return [EN] true when the slot holds that day / [FA] اگر خانه همان روز باشد true
 */
static bool func__UpStore_DailyRead(uint32_t uint32_t__dayIndex, up_daily_t *up_daily_t__out)
{
    File up_file_t__f;
    up_daily_t up_daily_t__temp;
    bool bool__ok = false;

    up_file_t__f = LittleFS.open(UP_F_DAILY, "r");
    if (!up_file_t__f)
    {
        return false;
    }

    if (up_file_t__f.seek(func__UpStore_DailyOffset(uint32_t__dayIndex)))
    {
        size_t size_t__got = up_file_t__f.read((uint8_t *)&up_daily_t__temp, sizeof(up_daily_t));
        bool__ok = (size_t__got == sizeof(up_daily_t)) &&
                   (up_daily_t__temp.uint32_t__dayIndex == uint32_t__dayIndex);
        if (bool__ok)
        {
            *up_daily_t__out = up_daily_t__temp;
        }
    }

    up_file_t__f.close();
    return bool__ok;
}

/**
 * @brief  [EN] Point the stored clock at a known instant (called when a browser
 *              hands the panel its time). Both halves of the pair are written
 *              together on purpose: an epoch without the monotonic reading it
 *              belongs to describes nothing.
 *         [FA] نشانه‌گذاری ساعت ذخیره‌شده روی یک لحظهٔ معلوم (وقتی مرورگر
 *              زمانش را به پنل می‌دهد). هر دو نیمهٔ جفت عمداً با هم نوشته
 *              می‌شوند: زمان مطلق بدون خوانش یکنوایی که به آن تعلق دارد هیچ
 *              چیزی را توصیف نمی‌کند.
 * @param  uint32_t__absS [EN] monotonic seconds of that instant / [FA] ثانیهٔ یکنوای آن لحظه
 * @param  uint32_t__epochS [EN] wall clock of that instant / [FA] ساعت مطلق آن لحظه
 * @return [EN] None / [FA] ندارد
 */
static void func__UpStore_ClockBaseSet(uint32_t uint32_t__absS, uint32_t uint32_t__epochS)
{
    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__clockBaseS = uint32_t__absS;
    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__clockEpochS = uint32_t__epochS;
    BOOL__G__TotalsDirty = true;
}

/* ==================== Flush / ذخیرهٔ دوره‌ای ==================== */
/**
 * @brief  [EN] Save whatever is dirty when the flush interval has passed, and
 *              honour the daily roll at the same moment.
 *         [FA] ذخیرهٔ تغییرات وقتی بازهٔ ذخیره گذشته باشد، و همان لحظه هم
 *              گذر روز را انجام می‌دهد.
 * @return [EN] true when something was written / [FA] در صورت نوشتن چیزی true
 */
static bool func__UpStore_FlushIfDue(void)
{
    bool bool__wrote = false;
    uint32_t uint32_t__nowMs = (uint32_t)millis();

    (void)func__UpStore_DailyRollIfNeeded();

    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastFlushMs) < UP_TOTALS_FLUSH_MS)
    {
        return false;
    }

    UINT32_T__G__LastFlushMs = uint32_t__nowMs;

    /* [EN] Keep the monotonic base fresh: this is what lets a record written
            before a reboot and one written after it share one timeline.
       [FA] تازه نگه‌داشتن مبنای یکنوا: همین است که به رکورد نوشته‌شده قبل و بعد
            از ری‌استارت اجازه می‌دهد یک خط زمان مشترک داشته باشند. */
    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__clockBaseS =
        UPPANEL_STATE_T__G__State.uint32_t__clockBaseAtBootS + func__UpState_UptimeS();

    /* [EN] The clock travels with that same instant. One pair - "what the
            monotonic counter read, and what the wall clock read" - is all it
            takes for every stored record to be convertible to a real time,
            even across the reboot that follows.
       [FA] ساعت هم همراه همان لحظه ذخیره می‌شود. یک جفت - «شمارندهٔ یکنوا چه
            می‌خواند و ساعت مطلق چه می‌خواند» - برای قابل‌تبدیل‌بودن هر رکورد
            ذخیره‌شده به زمان واقعی کافی است، حتی بعد از ری‌استارتی که می‌آید. */
    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__clockEpochS =
        func__UpState_NowEpochS();
    BOOL__G__TotalsDirty = true;

    if (BOOL__G__TotalsDirty)
    {
        bool__wrote = func__UpStore_TotalsSave() || bool__wrote;
    }
    if (BOOL__G__DailyDirty)
    {
        bool__wrote = func__UpStore_DailyFlush() || bool__wrote;
    }

    UPPANEL_STATE_T__G__State.uint32_t__storageUsed = func__UpStore_UsedBytes();
    return bool__wrote;
}

/* ==================== Purge / پاک‌سازی ==================== */
/**
 * @brief  [EN] Drop the oldest HALF of each ring right now. Called when the
 *              filesystem itself is nearly full, so the panel always keeps
 *              working instead of failing writes.
 *         [FA] همین حالا نیمهٔ قدیمی هر حلقه را حذف می‌کند. وقتی خود فایل‌سیستم
 *              نزدیک پر شدن است صدا زده می‌شود تا پنل به‌جای شکست در نوشتن،
 *              همچنان کار کند.
 * @return [EN] true when at least one ring was compacted / [FA] در صورت فشرده‌سازی حداقل یک حلقه true
 */
static bool func__UpStore_PurgeOldestHalf(void)
{
    bool bool__did = false;
    uint32_t uint32_t__count;

    uint32_t__count = func__UpStore_FileCount(UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t));
    if (uint32_t__count > 1u)
    {
        bool__did = func__UpStore_CompactRing(UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t)) || bool__did;
    }

    uint32_t__count = func__UpStore_FileCount(UP_F_EVENTS, (uint32_t)sizeof(up_event_t));
    if (uint32_t__count > 1u)
    {
        bool__did = func__UpStore_CompactRing(UP_F_EVENTS, (uint32_t)sizeof(up_event_t)) || bool__did;
    }

    return bool__did;
}

/**
 * @brief  [EN] Watch the filesystem; purge and raise the flag when free space
 *              falls under the reserve.
 *         [FA] پایش فایل‌سیستم؛ وقتی فضای آزاد زیر ذخیرهٔ احتیاطی برود پاک‌سازی
 *              و پرچم‌گذاری می‌کند.
 * @return [EN] true when a purge happened / [FA] در صورت پاک‌سازی true
 */
static bool func__UpStore_HandlePressure(void)
{
    uint32_t uint32_t__total = func__UpStore_TotalBytes();
    uint32_t uint32_t__used = func__UpStore_UsedBytes();

    UPPANEL_STATE_T__G__State.uint32_t__storageTotal = uint32_t__total;
    UPPANEL_STATE_T__G__State.uint32_t__storageUsed = uint32_t__used;

    if ((uint32_t__total - uint32_t__used) > UP_FS_LOW_BYTES)
    {
        UPPANEL_STATE_T__G__State.bool__storagePurging = false;
        return false;
    }

    UPPANEL_STATE_T__G__State.bool__storagePurging = true;
    UPPANEL_STATE_T__G__State.uint32_t__lastPurgeMs = (uint32_t)millis();
    return func__UpStore_PurgeOldestHalf();
}

/**
 * @brief  [EN] Admin action: forget the recorded history (rings, daily rows,
 *              totals) but keep users, sessions and the action log. The event
 *              code of the purge itself is written by the caller afterwards.
 *         [FA] اقدام مدیر: پاک‌کردن تاریخچه (حلقه‌ها، روزها، جمع کل) ولی
 *              نگه‌داشتن کاربران، نشست‌ها و گزارش اقدامات. کد رویداد خودِ پاک‌سازی
 *              بعداً توسط فراخوان نوشته می‌شود.
 * @return [EN] true when everything was removed / [FA] در صورت حذف همه true
 */
static bool func__UpStore_PurgeAll(void)
{
    bool bool__ok = true;

    bool__ok = LittleFS.remove(UP_F_SAMPLES) || !LittleFS.exists(UP_F_SAMPLES);
    bool__ok = (LittleFS.remove(UP_F_EVENTS) || !LittleFS.exists(UP_F_EVENTS)) && bool__ok;
    bool__ok = (LittleFS.remove(UP_F_DAILY) || !LittleFS.exists(UP_F_DAILY)) && bool__ok;
    bool__ok = (LittleFS.remove(UP_F_TOTALS) || !LittleFS.exists(UP_F_TOTALS)) && bool__ok;

    memset(&UPPANEL_STATE_T__G__State.up_totals_t__totals, 0, sizeof(up_totals_t));
    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint32_t__magic = UP_TOTALS_MAGIC;
    UPPANEL_STATE_T__G__State.up_totals_t__totals.uint16_t__minV = 0xFFFFu;
    BOOL__G__TotalsDirty = false;

    (void)func__UpStore_EnsureRing(UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t));
    (void)func__UpStore_EnsureRing(UP_F_EVENTS, (uint32_t)sizeof(up_event_t));
    (void)func__UpStore_DailyLoadToday();

    UINT32_T__G__PurgeCount++;
    return bool__ok;
}

/**
 * @brief  [EN] How many times the panel had to drop old data (shown to admins so
 *              "the history is short" is explained instead of mysterious).
 *         [FA] چند بار پنل مجبور شده دادهٔ قدیمی را حذف کند (به مدیر نشان داده
 *              می‌شود تا «تاریخچه کوتاه است» توضیح داشته باشد، نه معما).
 * @return [EN] count since boot / [FA] تعداد از زمان روشن شدن
 */
static uint32_t func__UpStore_PurgeCount(void)
{
    return UINT32_T__G__PurgeCount;
}

/* ==================== CSV / برون‌بری ==================== */
/**
 * @brief  [EN] Format one sample as one CSV line. Kept in the store because the
 *              record layout and this text are the same fact in two places, and
 *              two places is one place too many.
 *         [FA] قالب‌بندی یک نمونه به‌شکل یک خط CSV. در همین ماژول می‌ماند چون
 *              چیدمان رکورد و این متن یک واقعیت در دو جا هستند و دو جا یکی
 *              زیادی است.
 * @param  up_sample_t__sample [EN] record / [FA] رکورد
 * @param  char__out [EN] line buffer / [FA] بافر خط
 * @param  uint32_t__outLen [EN] buffer size / [FA] اندازهٔ بافر
 * @return [EN] None / [FA] ندارد
 */
static void func__UpStore_CsvRow(const up_sample_t *up_sample_t__sample, char *char__out, uint32_t uint32_t__outLen)
{
    /* [EN] The epoch column is converted FROM THE SAMPLE's own monotonic
            second. Reading the clock now would stamp every historic row with
            "the moment somebody pressed export".
       [FA] ستون زمان مطلق از ثانیهٔ یکنوای «خودِ رکورد» تبدیل می‌شود. خواندن
            ساعت فعلی، هر ردیف تاریخی را با «لحظه‌ای که کسی برون‌بری را زد»
            مهر می‌کرد. */
    snprintf(char__out, uint32_t__outLen,
             "%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
             (unsigned long)func__UpState_EpochFromAbs(up_sample_t__sample->uint32_t__absS),
             (unsigned long)up_sample_t__sample->uint32_t__absS,
             (unsigned)up_sample_t__sample->uint16_t__vinMv,
             (unsigned)up_sample_t__sample->uint16_t__v24Mv,
             (unsigned)up_sample_t__sample->uint16_t__v12Mv,
             (unsigned)up_sample_t__sample->uint16_t__i1Ma,
             (unsigned)up_sample_t__sample->uint16_t__i2Ma,
             (unsigned)up_sample_t__sample->uint16_t__duty1,
             (unsigned)up_sample_t__sample->uint16_t__duty2,
             (unsigned)up_sample_t__sample->uint8_t__state1,
             (unsigned)up_sample_t__sample->uint8_t__state2,
             (unsigned)up_sample_t__sample->uint8_t__flags,
             (unsigned)up_sample_t__sample->uint8_t__flags2);
}

#endif /* UP_STORE_H */
