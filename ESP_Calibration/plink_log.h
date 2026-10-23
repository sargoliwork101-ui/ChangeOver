/* plink_log.h - live state history on the panel's own flash, one CSV per day.
   [FA] plink_log.h - تاریخچهٔ وضعیت زنده روی فلش خود پنل، روزی یک فایل CSV.
   Included by ESP_Calibration.ino (single translation unit, order matters):
   after plink_state.h and before plink_link.h (the frame parser calls the hook).
   No include guard on purpose: including twice would redefine everything. */

#include <string.h>

/* ==================== Clock / ساعت ==================== */
/* [EN] The panel has no RTC. The phone that opens the page sends its own time
        (POST /clock) and this file keeps it as an anchor: now = anchor + the
        elapsed millis. The time zone offset comes from the same phone, so the
        daily file name rolls over at the operator's midnight, not at UTC's.
   [FA] پنل ساعت ندارد. گوشی/کامپیوتری که صفحه را باز می‌کند ساعت خودش را
        می‌فرستد (‎POST /clock‎) و این فایل آن را لنگر نگه می‌دارد:
        اکنون = لنگر + میلی‌ثانیه‌های گذشته. اختلاف منطقهٔ زمانی هم از همان
        گوشی می‌آید، تا نام فایل روزانه سرِ نیمه‌شب خودِ اپراتور عوض شود نه UTC. */
static uint32_t UINT32_T__G__LogAnchorSec = 0u;
static uint32_t UINT32_T__G__LogAnchorMs = 0u;
static int32_t  INT32_T__G__LogTzSec = 0;
static bool     BOOL__G__LogClockValid = false;
static uint32_t UINT32_T__G__LogLastClockSaveMs = 0u;

/* ==================== Open day / روز باز ==================== */
static uint32_t UINT32_T__G__LogDayCode = 0u;   /* ‎YYYYMMDD‎, 0 = clock unknown */
static uint16_t UINT16_T__G__LogRowsToday = 0u;
static bool     BOOL__G__LogDayFull = false;
static bool     BOOL__G__LogBootRowPending = true;

/* ==================== Previous snapshot / وضعیت قبلی ==================== */
static bool     BOOL__G__LogHasPrev = false;
static uint8_t  UINT8_T__G__LogPrevMains = 0u;
static uint8_t  UINT8_T__G__LogPrevChg1 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg2 = 0u;
static uint8_t  UINT8_T__G__LogPrevFlags2 = 0u;
static uint8_t  UINT8_T__G__LogPrevBand = 0u;
static uint32_t UINT32_T__G__LogPrevFault = 0u;
static uint8_t  UINT8_T__G__LogPrevNote = 0u;   /* [EN] danger before the last restart / [FA] خطر پیش از ری‌استارت */

/* ==================== Pending change / تغییر در انتظار ==================== */
static uint16_t UINT16_T__G__LogPendingMask = 0u;
static uint8_t  UINT8_T__G__LogPendingNote = 0u;
static uint32_t UINT32_T__G__LogLastRowMs = 0u;
static bool     BOOL__G__LogLastRowValid = false;
static bool     BOOL__G__LogLinkDown = false;

/* ==================== Day index / فهرست روزها ==================== */
static uint32_t UINT32_T__G__LogIndexDay[ESP_LOG_MAX_DAYS + 2u];
static uint8_t  UINT8_T__G__LogIndexCount = 0u;

/* ==================== Change bits / بیت‌های تغییر ==================== */
#define ESP_LOG_CHG_BOOT    0x0001u
#define ESP_LOG_CHG_MAINS   0x0002u
#define ESP_LOG_CHG_CHG1    0x0004u
#define ESP_LOG_CHG_CHG2    0x0008u
#define ESP_LOG_CHG_FAULT   0x0010u
#define ESP_LOG_CHG_FLAGS   0x0020u
#define ESP_LOG_CHG_BAND    0x0040u
#define ESP_LOG_CHG_LINK    0x0080u

/* ==================== Notes / یادداشت ردیف ==================== */
#define ESP_LOG_NOTE_NONE      0u
#define ESP_LOG_NOTE_LOW_STOP  1u   /* [EN] board came back after a low-battery stop / [FA] برگشت پس از ایست باتری ضعیف */
#define ESP_LOG_NOTE_LINK_LOST 2u
#define ESP_LOG_NOTE_LINK_BACK 3u
#define ESP_LOG_NOTE_DAY_FULL  4u

#define ESP_LOG_PATH_MAX       24u

/* ==================== Time helpers / کمک‌های زمان ==================== */

/**
 * @brief  [EN] Current epoch seconds from the phone anchor, 0 when unknown.
 *         [FA] ثانیهٔ یونیکس فعلی از لنگر گوشی؛ صفر یعنی نامعلوم.
 * @return [EN] uint32_t / [FA]‎ uint32_t
 */
static uint32_t func__EspLog_NowSec(void)
{
    uint32_t uint32_t__elapsedMs;

    if (BOOL__G__LogClockValid == false)
    {
        return 0u;
    }

    uint32_t__elapsedMs = (uint32_t)millis() - UINT32_T__G__LogAnchorMs;
    return UINT32_T__G__LogAnchorSec + (uint32_t__elapsedMs / 1000u);
}

/**
 * @brief  [EN] Local day number (days since 1970-01-01) of an epoch second.
 *         [FA] شمارهٔ روز محلی (روز از ۱۹۷۰-۰۱-۰۱) برای یک ثانیهٔ یونیکس.
 * @‎param  uint32_t uint32_t__epochSec [EN] epoch seconds / [FA]‎ ثانیهٔ یونیکس
 * @return [EN] uint32_t / [FA]‎ uint32_t
 */
static uint32_t func__EspLog_LocalDay(uint32_t uint32_t__epochSec)
{
    int32_t int32_t__local;

    int32_t__local = (int32_t)uint32_t__epochSec + INT32_T__G__LogTzSec;
    if (int32_t__local < 0)
    {
        return 0u;
    }
    return (uint32_t)(int32_t__local / 86400);
}

/**
 * @brief  [EN] Civil date (Gregorian) of a day number, packed as YYYYMMDD.
 *         [FA] تاریخ میلادی یک شمارهٔ روز، بسته‌بندی‌شده به‌صورت YYYYMMDD.
 * @‎param  uint32_t uint32_t__days [EN] days since 1970-01-01 / [FA]‎ روز از ۱۹۷۰
 * @‎return [EN] uint32_t packed date, 0 when unknown / [FA]‎ تاریخ بسته‌بندی‌شده
 */
static uint32_t func__EspLog_DateCode(uint32_t uint32_t__days)
{
    uint32_t uint32_t__z;
    uint32_t uint32_t__era;
    uint32_t uint32_t__doe;
    uint32_t uint32_t__yoe;
    uint32_t uint32_t__doy;
    uint32_t uint32_t__mp;
    uint32_t uint32_t__year;
    uint32_t uint32_t__month;
    uint32_t uint32_t__day;

    /* [EN] Howard Hinnant's civil_from_days, all-unsigned for dates after 1970.
       [FA] الگوریتم تبدیل روز به تاریخ میلادی؛ برای تاریخ‌های بعد از ۱۹۷۰
            همه‌ی محاسبات مثبت است. */
    uint32_t__z = uint32_t__days + 719468u;
    uint32_t__era = uint32_t__z / 146097u;
    uint32_t__doe = uint32_t__z - (uint32_t__era * 146097u);
    uint32_t__yoe = (uint32_t__doe - (uint32_t__doe / 1460u) +
                     (uint32_t__doe / 36524u) - (uint32_t__doe / 146096u)) / 365u;
    uint32_t__year = uint32_t__yoe + (uint32_t__era * 400u);
    uint32_t__doy = uint32_t__doe - ((365u * uint32_t__yoe) +
                                     (uint32_t__yoe / 4u) - (uint32_t__yoe / 100u));
    uint32_t__mp = ((5u * uint32_t__doy) + 2u) / 153u;
    uint32_t__day = uint32_t__doy - (((153u * uint32_t__mp) + 2u) / 5u) + 1u;
    uint32_t__month = (uint32_t__mp < 10u) ? (uint32_t__mp + 3u) : (uint32_t__mp - 9u);
    if (uint32_t__month <= 2u)
    {
        uint32_t__year = uint32_t__year + 1u;
    }
    if ((uint32_t__year > 9999u) || (uint32_t__month > 12u) || (uint32_t__day > 31u))
    {
        return 0u;
    }
    return (uint32_t__year * 10000u) + (uint32_t__month * 100u) + uint32_t__day;
}

/**
 * @brief  [EN] Day code of "now" (0 when the phone clock is unknown).
 *         [FA] کد روز «همین حالا» (صفر وقتی ساعت گوشی معلوم نیست).
 * @return [EN] uint32_t / [FA]‎ uint32_t
 */
static uint32_t func__EspLog_TodayCode(void)
{
    uint32_t uint32_t__nowSec;

    uint32_t__nowSec = func__EspLog_NowSec();
    if (uint32_t__nowSec == 0u)
    {
        return 0u;
    }
    return func__EspLog_DateCode(func__EspLog_LocalDay(uint32_t__nowSec));
}

/**
 * @brief  [EN] Build a day file path. Day 0 (clock unknown) gets its own file.
 *         [FA] ساخت مسیر فایل یک روز. روز صفر (ساعت نامعلوم) فایل خودش را دارد.
 * @‎param  char *char__path [EN] output / [FA]‎ خروجی
 * @‎param  uint32_t uint32_t__dayCode [EN] YYYYMMDD or 0 / [FA]‎ کد روز
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_PathOf(char *char__path, uint32_t uint32_t__dayCode)
{
    (void)snprintf(char__path, ESP_LOG_PATH_MAX, "%s%08lu.csv",
                   ESP_LOG_FILE_PREFIX, (unsigned long)uint32_t__dayCode);
}

/* ==================== Clock storage / ذخیرهٔ ساعت ==================== */

/**
 * @brief  [EN] Persist the clock anchor (called every 10 minutes and at boot).
 *         [FA] ذخیرهٔ لنگر ساعت (هر ۱۰ دقیقه و هنگام روشن‌شدن).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_ClockSave(void)
{
    File file__clock;
    uint32_t uint32_t__nowSec;

    if ((BOOL__G__FsOk == false) || (BOOL__G__LogClockValid == false))
    {
        return;
    }
    uint32_t__nowSec = func__EspLog_NowSec();
    if (uint32_t__nowSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "w");
    if (!file__clock)
    {
        return;
    }
    file__clock.print((unsigned long)uint32_t__nowSec);
    file__clock.print(",");
    file__clock.print((long)INT32_T__G__LogTzSec);
    file__clock.close();
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
}

/**
 * @brief  [EN] Read the clock anchor back after a restart.
 *         [FA] خواندن دوبارهٔ لنگر ساعت بعد از روشن‌شدن.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_ClockLoad(void)
{
    File file__clock;
    String string__content;
    int32_t int32_t__split;
    uint32_t uint32_t__savedSec;

    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_CLOCK_PATH) == false))
    {
        return;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
    if (!file__clock)
    {
        return;
    }
    string__content = file__clock.readString();
    file__clock.close();
    int32_t__split = string__content.indexOf(',');
    if (int32_t__split <= 0)
    {
        return;
    }
    uint32_t__savedSec = (uint32_t)string__content.substring(0, int32_t__split).toInt();
    if (uint32_t__savedSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return;
    }
    UINT32_T__G__LogAnchorSec = uint32_t__savedSec;
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    INT32_T__G__LogTzSec = (int32_t)string__content.substring(int32_t__split + 1).toInt();
    BOOL__G__LogClockValid = true;
}

/**
 * @brief  [EN] Adopt the phone's clock (POST /clock) and save it.
 *         [FA] گرفتن ساعت گوشی (‎POST /clock‎) و ذخیرهٔ آن.
 * @‎param  uint32_t uint32_t__epochSec [EN] epoch seconds / [FA]‎ ثانیهٔ یونیکس
 * @‎param  int32_t int32_t__tzSec [EN] UTC offset in seconds / [FA]‎ اختلاف با UTC
 * @‎return [EN] bool, false when the time is not believable / [FA]‎ درستی زمان
 */
static bool func__EspLog_TimeSet(uint32_t uint32_t__epochSec, int32_t int32_t__tzSec)
{
    if (uint32_t__epochSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return false;
    }
    if ((int32_t__tzSec < -50400) || (int32_t__tzSec > 50400))
    {
        int32_t__tzSec = 0;
    }
    UINT32_T__G__LogAnchorSec = uint32_t__epochSec;
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    INT32_T__G__LogTzSec = int32_t__tzSec;
    BOOL__G__LogClockValid = true;
    func__EspLog_ClockSave();
    return true;
}

/* ==================== Day index / فهرست روزها ==================== */

/**
 * @brief  [EN] Load the day list used for retention and the panel's picker.
 *         [FA] خواندن فهرست روزها برای نگه‌داری و انتخابگر پنل.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_IndexLoad(void)
{
    File file__index;

    UINT8_T__G__LogIndexCount = 0u;
    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_INDEX_PATH) == false))
    {
        return;
    }
    file__index = LittleFS.open(ESP_LOG_INDEX_PATH, "r");
    if (!file__index)
    {
        return;
    }
    while (file__index.available() && (UINT8_T__G__LogIndexCount < (ESP_LOG_MAX_DAYS + 2u)))
    {
        String string__line = file__index.readStringUntil('\n');
        string__line.trim();
        if (string__line.length() > 0u)
        {
            UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount] =
                (uint32_t)string__line.toInt();
            UINT8_T__G__LogIndexCount++;
        }
    }
    file__index.close();
}

/**
 * @brief  [EN] Rewrite the day list (called only when a day is added/removed).
 *         [FA] بازنویسی فهرست روزها (فقط وقتی روزی اضافه یا حذف می‌شود).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_IndexSave(void)
{
    File file__index;
    uint8_t uint8_t__slot;

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    file__index = LittleFS.open(ESP_LOG_INDEX_PATH, "w");
    if (!file__index)
    {
        return;
    }
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        file__index.println((unsigned long)UINT32_T__G__LogIndexDay[uint8_t__slot]);
    }
    file__index.close();
}

/**
 * @brief  [EN] Delete the oldest day when the retention count is passed.
 *         [FA] حذف قدیمی‌ترین روز وقتی تعداد روزهای نگه‌داری رد شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_TrimDays(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;

    while (UINT8_T__G__LogIndexCount > ESP_LOG_MAX_DAYS)
    {
        func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[0]);
        (void)LittleFS.remove(char__path);
        for (uint8_t__slot = 1u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] =
                UINT32_T__G__LogIndexDay[uint8_t__slot];
        }
        UINT8_T__G__LogIndexCount--;
    }
    func__EspLog_IndexSave();
}

/**
 * @brief  [EN] Count the rows already in a day file (after a restart).
 *         [FA] شمردن ردیف‌های موجود یک فایل روز (بعد از روشن‌شدن).
 * @‎param  const char *char__path [EN] file / [FA]‎ فایل
 * @‎return [EN] uint16_t rows / [FA]‎ تعداد ردیف
 */
static uint16_t func__EspLog_CountRows(const char *char__path)
{
    File file__day;
    uint16_t uint16_t__rows = 0u;

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        if ((string__line.length() > 0u) && (string__line[0] != '#'))
        {
            uint16_t__rows++;
        }
    }
    file__day.close();
    return uint16_t__rows;
}

/**
 * @brief  [EN] Make sure today's file exists and its row counter is right.
 *         [FA] مطمئن شو فایل امروز هست و شمارندهٔ ردیفش درست است.
 * @‎return [EN] bool, false when the file system is not usable / [FA]‎ آماده‌بودن
 */
static bool func__EspLog_OpenDay(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    bool bool__isNew;
    uint32_t uint32_t__dayCode;
    uint8_t uint8_t__slot;

    if (BOOL__G__FsOk == false)
    {
        return false;
    }
    uint32_t__dayCode = func__EspLog_TodayCode();
    if (uint32_t__dayCode == UINT32_T__G__LogDayCode)
    {
        return true;
    }

    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    bool__isNew = (LittleFS.exists(char__path) == false);
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        return false;
    }
    if (bool__isNew)
    {
        file__day.print(ESP_LOG_HEADER);
    }
    file__day.close();

    UINT32_T__G__LogDayCode = uint32_t__dayCode;
    UINT16_T__G__LogRowsToday = func__EspLog_CountRows(char__path);
    BOOL__G__LogDayFull = (UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY);

    if (bool__isNew)
    {
        bool bool__alreadyIndexed = false;

        for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            if (UINT32_T__G__LogIndexDay[uint8_t__slot] == uint32_t__dayCode)
            {
                bool__alreadyIndexed = true;
            }
        }
        if (bool__alreadyIndexed == false)
        {
            UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount] = uint32_t__dayCode;
            UINT8_T__G__LogIndexCount++;
            func__EspLog_TrimDays();
        }
    }
    return true;
}

/* ==================== Row write / نوشتن ردیف ==================== */

/**
 * @brief  [EN] Append one row and close it at once, so a power cut right after
 *              the battery cut cannot lose it. Nothing is written when the day
 *              has reached its row cap.
 *         [FA] یک ردیف را اضافه و همان لحظه می‌بندد، تا قطع برق درست بعد از
 *              قطع باتری آن را از بین نبرد. اگر سقف ردیف روز پر شده باشد
 *              چیزی نوشته نمی‌شود.
 * @‎param  uint16_t uint16_t__changeMask [EN] what changed / [FA]‎ چه عوض شد
 * @‎param  uint8_t uint8_t__note [EN] row note / [FA]‎ یادداشت ردیف
 * @‎return [EN] bool written / [FA]‎ نوشته شد
 */
static bool func__EspLog_WriteRow(uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    uint32_t uint32_t__nowSec;
    uint32_t uint32_t__uptimeSec;
    uint8_t uint8_t__mains;

    if (func__EspLog_OpenDay() == false)
    {
        return false;
    }
    if ((UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY) ||
        (BOOL__G__LogDayFull != false))
    {
        BOOL__G__LogDayFull = true;
        return false;
    }

    func__EspLog_PathOf(char__path, UINT32_T__G__LogDayCode);
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        return false;
    }

    uint32_t__nowSec = func__EspLog_NowSec();
    uint32_t__uptimeSec = (uint32_t)(millis() / 1000u);
    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;

    file__day.print((unsigned long)uint32_t__nowSec);
    file__day.print(",");
    file__day.print((unsigned long)uint32_t__uptimeSec);
    file__day.print(",");
    file__day.print((unsigned int)uint16_t__changeMask);
    file__day.print(",");
    file__day.print((unsigned int)uint8_t__note);
    file__day.print(",");
    file__day.print((unsigned int)uint8_t__mains);
    file__day.print(",");
    file__day.print((unsigned long)UINT32_T__G__TlmField[6]);
    file__day.print(",");
    file__day.print((unsigned long)UINT32_T__G__TlmField[13]);
    file__day.print(",");
    file__day.print((unsigned long)UINT32_T__G__TlmField[19]);
    file__day.print(",");
    file__day.print((unsigned int)UINT8_T__G__TlmFlags2);
    file__day.print(",");
    file__day.print((unsigned long)UINT32_T__G__TlmField[15]);
    file__day.print(",");
    file__day.print((unsigned long)UINT32_T__G__TlmField[17]);
    file__day.print(",");
    file__day.print((unsigned long)UINT32_T__G__TlmField[18]);
    file__day.print(",");
    file__day.println((unsigned long)UINT32_T__G__TlmField[25]);
    file__day.close();

    UINT16_T__G__LogRowsToday++;
    UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    BOOL__G__LogLastRowValid = true;
    UINT16_T__G__LogPendingMask = 0u;
    UINT8_T__G__LogPendingNote = 0u;
    return true;
}

/**
 * @brief  [EN] Keep the "last state before a restart" note, so the boot row can
 *              say the panel went down while the battery was at the cut line.
 *              Written only on a fault/latch change or when the pack enters the
 *              danger band - not on every row, to spare the flash.
 *         [FA] نگه‌داشتن یادداشت «آخرین وضعیت پیش از روشن‌شدن دوباره»، تا ردیف
 *              بوت بتواند بگوید پنل وقتی باتری روی مرز قطع بود خاموش شد. فقط
 *              هنگام تغییر خطا/قفل یا ورود پک به ناحیهٔ خطر نوشته می‌شود، نه
 *              هر ردیف، تا فلش کمتر فرسوده شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_SaveLast(void)
{
    File file__last;

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    file__last = LittleFS.open(ESP_LOG_LAST_PATH, "w");
    if (!file__last)
    {
        return;
    }
    file__last.print((unsigned long)func__EspLog_NowSec());
    file__last.print(",");
    file__last.print((unsigned long)UINT32_T__G__TlmField[15]);
    file__last.print(",");
    file__last.print((unsigned long)UINT32_T__G__TlmField[19]);
    file__last.print(",");
    file__last.print((unsigned int)UINT8_T__G__TlmFlags2);
    file__last.print(",");
    file__last.println((UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1 : 0);
    file__last.close();
}

/**
 * @brief  [EN] Read the saved last-state note; only the danger flag is used.
 *         [FA] خواندن یادداشت آخرین وضعیت؛ فقط پرچم خطر استفاده می‌شود.
 * @‎return [EN] uint8_t 1 = the last run was in the danger band / [FA]‎ ۱ = خطر
 */
static uint8_t func__EspLog_LoadLastDanger(void)
{
    File file__last;
    String string__content;
    int32_t int32_t__lastComma;

    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_LAST_PATH) == false))
    {
        return 0u;
    }
    file__last = LittleFS.open(ESP_LOG_LAST_PATH, "r");
    if (!file__last)
    {
        return 0u;
    }
    string__content = file__last.readString();
    file__last.close();
    int32_t__lastComma = string__content.lastIndexOf(',');
    if (int32_t__lastComma < 0)
    {
        return 0u;
    }
    return (string__content.substring(int32_t__lastComma + 1).toInt() != 0) ? 1u : 0u;
}

/* ==================== Change detection / تشخیص تغییر ==================== */

/**
 * @brief  [EN] Queue a change. Bursts inside the 2 s window are merged into one
 *              row; the row always carries the full snapshot, so no final state
 *              is lost and the flash is not hammered.
 *         [FA] ثبت یک تغییر. تغییرهای پشت‌سرهم داخل پنجرهٔ ۲ ثانیه‌ای در یک ردیف
 *              ادغام می‌شوند؛ ردیف همیشه کل وضعیت را دارد، پس وضعیت نهایی گم
 *              نمی‌شود و فلش هم زیر فشار نمی‌رود.
 * @‎param  uint16_t uint16_t__mask [EN] change bits / [FA]‎ بیت‌های تغییر
 * @‎param  uint8_t uint8_t__note [EN] note / [FA]‎ یادداشت
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Queue(uint16_t uint16_t__mask, uint8_t uint8_t__note)
{
    uint32_t uint32_t__nowMs;

    UINT16_T__G__LogPendingMask = (uint16_t)(UINT16_T__G__LogPendingMask | uint16_t__mask);
    if (uint8_t__note != ESP_LOG_NOTE_NONE)
    {
        UINT8_T__G__LogPendingNote = uint8_t__note;
    }

    uint32_t__nowMs = (uint32_t)millis();
    if ((BOOL__G__LogLastRowValid == false) ||
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >= ESP_LOG_MIN_INTERVAL_MS))
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

/**
 * @brief  [EN] Flush a queued change once the 2 s window has passed.
 *         [FA] نوشتن تغییر در انتظار، وقتی پنجرهٔ ۲ ثانیه‌ای گذشت.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FlushPending(void)
{
    uint32_t uint32_t__nowMs;

    if (UINT16_T__G__LogPendingMask == 0u)
    {
        return;
    }
    uint32_t__nowMs = (uint32_t)millis();
    if ((BOOL__G__LogLastRowValid == false) ||
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >= ESP_LOG_MIN_INTERVAL_MS))
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

/**
 * @brief  [EN] Compare this frame's live state with the previous one and queue
 *              whatever changed. A steady board writes nothing.
 *         [FA] مقایسهٔ وضعیت زندهٔ این فریم با فریم قبلی و ثبت هر چه عوض شده.
 *              برد آرام هیچ چیزی نمی‌نویسد.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_DetectChanges(void)
{
    uint8_t uint8_t__mains;
    uint8_t uint8_t__chg1;
    uint8_t uint8_t__chg2;
    uint8_t uint8_t__flags2;
    uint8_t uint8_t__band;
    uint32_t uint32_t__fault;
    uint16_t uint16_t__mask = 0u;

    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;
    uint8_t__chg1 = (uint8_t)UINT32_T__G__TlmField[6];
    uint8_t__chg2 = (uint8_t)UINT32_T__G__TlmField[13];
    uint8_t__flags2 = UINT8_T__G__TlmFlags2;
    uint32_t__fault = UINT32_T__G__TlmField[19];
    uint8_t__band = (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;

    if (BOOL__G__LogHasPrev == false)
    {
        BOOL__G__LogHasPrev = true;
        UINT8_T__G__LogPrevMains = uint8_t__mains;
        UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
        UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
        UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
        UINT32_T__G__LogPrevFault = uint32_t__fault;
        UINT8_T__G__LogPrevBand = uint8_t__band;
        return;
    }

    if (uint8_t__mains != UINT8_T__G__LogPrevMains)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_MAINS);
    }
    if (uint8_t__chg1 != UINT8_T__G__LogPrevChg1)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_CHG1);
    }
    if (uint8_t__chg2 != UINT8_T__G__LogPrevChg2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_CHG2);
    }
    if (uint32_t__fault != UINT32_T__G__LogPrevFault)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_FAULT);
    }
    if (uint8_t__flags2 != UINT8_T__G__LogPrevFlags2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_FLAGS);
    }
    if (uint8_t__band != UINT8_T__G__LogPrevBand)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_BAND);
    }

    if (uint16_t__mask != 0u)
    {
        func__EspLog_Queue(uint16_t__mask, ESP_LOG_NOTE_NONE);
    }

    if ((uint16_t__mask & (ESP_LOG_CHG_FAULT | ESP_LOG_CHG_FLAGS)) != 0u)
    {
        func__EspLog_SaveLast();
    }
    if ((uint8_t__band != 0u) && (UINT8_T__G__LogPrevBand == 0u))
    {
        func__EspLog_SaveLast();
    }

    UINT8_T__G__LogPrevMains = uint8_t__mains;
    UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
    UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
    UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
    UINT32_T__G__LogPrevFault = uint32_t__fault;
    UINT8_T__G__LogPrevBand = uint8_t__band;
}

/* ==================== Hooks / قلاب‌ها ==================== */

/**
 * @brief  [EN] Called after every decoded TLM frame.
 *         [FA] بعد از هر فریم TLM رمزگشایی‌شده صدا زده می‌شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_OnFrame(void)
{
    if (BOOL__G__LogLinkDown != false)
    {
        BOOL__G__LogLinkDown = false;
        if (BOOL__G__LogHasPrev != false)
        {
            func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_BACK);
        }
    }

    if ((BOOL__G__LogBootRowPending != false) && (BOOL__G__LogHasPrev == false))
    {
        /* [EN] First frame after a restart: one boot row, carrying the state
              the panel saw last time, and whether that run was in the danger
              band. Written exactly once per boot - if nothing is different the
              state fields simply repeat, but the row itself is the evidence
              that the panel restarted.
           [FA] اولین فریم بعد از روشن‌شدن: یک ردیف بوت که وضعیتی را می‌برد که
              پنل آخرین بار دیده بود، به‌همراه اینکه آن اجرا در ناحیهٔ خطر بود یا
              نه. دقیقاً یک بار در هر بوت نوشته می‌شود؛ اگر چیزی فرق نداشته
              باشد خانه‌های وضعیت همان را تکرار می‌کنند، ولی خودِ ردیف شاهد
              روشن‌شدن دوبارهٔ پنل است. */
        UINT8_T__G__LogPrevNote = func__EspLog_LoadLastDanger();
        (void)func__EspLog_WriteRow(ESP_LOG_CHG_BOOT,
                                    (UINT8_T__G__LogPrevNote != 0u) ?
                                    ESP_LOG_NOTE_LOW_STOP : ESP_LOG_NOTE_NONE);
        BOOL__G__LogBootRowPending = false;
    }

    func__EspLog_DetectChanges();
}

/**
 * @brief  [EN] Periodic work: merge pending rows, save the clock, notice a lost
 *              link. Called from loop().
 *         [FA] کار دوره‌ای: نوشتن ردیف در انتظار، ذخیرهٔ ساعت و تشخیص قطع
 *              ارتباط. از حلقه صدا زده می‌شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Tick(void)
{
    uint32_t uint32_t__nowMs;

    func__EspLog_FlushPending();

    uint32_t__nowMs = (uint32_t)millis();
    if (BOOL__G__LogClockValid != false)
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastClockSaveMs) >= ESP_LOG_CLOCK_SAVE_MS)
        {
            func__EspLog_ClockSave();
        }
    }

    if ((BOOL__G__TlmSeen != false) && (BOOL__G__LogLinkDown == false))
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastTlmMs) > ESP_LOG_LINK_TIMEOUT_MS)
        {
            BOOL__G__LogLinkDown = true;
            func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_LOST);
        }
    }
}

/**
 * @brief  [EN] Boot: mount state, clock from flash, day list, boot row pending.
 *         [FA] شروع: وضعیت فایل‌سیستم، ساعت از فلش، فهرست روزها و ردیف بوت.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Begin(void)
{
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    func__EspLog_ClockLoad();
    func__EspLog_IndexLoad();
}

/**
 * @brief  [EN] Count the data rows of a day file.
 *         [FA] شمردن ردیف‌های دادهٔ فایل یک روز.
 * @‎param  const char *char__path [EN] file / [FA]‎ فایل
 * @‎return [EN] uint16_t rows / [FA]‎ تعداد ردیف
 */
static uint16_t func__EspLog_RowsOf(const char *char__path)
{
    File file__day;
    uint16_t uint16_t__rows = 0u;

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        if ((string__line.length() > 0u) && (string__line[0] != '#'))
        {
            uint16_t__rows++;
        }
    }
    file__day.close();
    return uint16_t__rows;
}

/**
 * @brief  [EN] Copy the last N rows of one day into a buffer, oldest first.
 *         Two passes: count, then skip the old ones - a 400-row day never has
 *         to fit in RAM at once.
 *         [FA] کپی آخرین N ردیف یک روز در بافر، قدیمی‌تر اول. دو پاس: شمردن و
 *         بعد رد کردن ردیف‌های قدیمی - فایل ۴۰۰ ردیفی لازم نیست یک‌جا در رم جا
 *         شود.
 * @‎param  char *char__out [EN] output buffer / [FA]‎ بافر خروجی
 * @‎param  size_t size_t__outSize [EN] buffer size / [FA]‎ اندازهٔ بافر
 * @‎param  uint32_t uint32_t__dayCode [EN] day, 0 = newest day / [FA]‎ روز
 * @‎param  uint16_t uint16_t__maxLines [EN] how many rows / [FA]‎ چند ردیف
 * @‎return [EN] size_t bytes written / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_Tail(char *char__out, size_t size_t__outSize,
                                uint32_t uint32_t__dayCode, uint16_t uint16_t__maxLines)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    size_t size_t__used = 0u;
    uint16_t uint16_t__total;
    uint16_t uint16_t__skip;
    uint16_t uint16_t__index = 0u;
    uint32_t uint32_t__chosen = uint32_t__dayCode;

    if ((char__out == NULL) || (size_t__outSize < 2u) || (BOOL__G__FsOk == false))
    {
        return 0u;
    }
    char__out[0] = '\0';

    if (uint32_t__chosen == 0u)
    {
        if (UINT8_T__G__LogIndexCount == 0u)
        {
            return 0u;
        }
        uint32_t__chosen = UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
    }

    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        return 0u;
    }

    uint16_t__total = func__EspLog_RowsOf(char__path);
    uint16_t__skip = (uint16_t__total > uint16_t__maxLines) ?
                     (uint16_t)(uint16_t__total - uint16_t__maxLines) : 0u;

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        string__line.trim();
        if ((string__line.length() == 0u) || (string__line[0] == '#'))
        {
            continue;
        }
        if (uint16_t__index < uint16_t__skip)
        {
            uint16_t__index++;
            continue;
        }
        if ((size_t__used + string__line.length() + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__out[size_t__used], string__line.c_str(), string__line.length());
        size_t__used += string__line.length();
        char__out[size_t__used] = '\n';
        size_t__used++;
        char__out[size_t__used] = '\0';
    }
    file__day.close();
    return size_t__used;
}

/**
 * @brief  [EN] Delete every saved day and start a fresh list.
 *         [FA] حذف همهٔ روزهای ذخیره‌شده و شروع فهرست تازه.
 * @‎return [EN] uint8_t number of files removed / [FA]‎ تعداد فایل حذف‌شده
 */
static uint8_t func__EspLog_Clear(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    uint8_t uint8_t__removed = 0u;

    if (BOOL__G__FsOk == false)
    {
        return 0u;
    }
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (LittleFS.remove(char__path))
        {
            uint8_t__removed++;
        }
    }
    (void)LittleFS.remove(ESP_LOG_INDEX_PATH);
    (void)LittleFS.remove(ESP_LOG_LAST_PATH);
    UINT8_T__G__LogIndexCount = 0u;
    UINT32_T__G__LogDayCode = 0u;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    return uint8_t__removed;
}

/**
 * @brief  [EN] Day list as text ("YYYYMMDD YYYYMMDD ... , newest last").
 *         [FA] فهرست روزها به‌صورت متن (قدیمی‌ترین تا جدیدترین).
 * @‎param  char *char__out [EN] output / [FA]‎ خروجی
 * @‎param  size_t size_t__outSize [EN] size / [FA]‎ اندازه
 * @‎return [EN] size_t / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_Days(char *char__out, size_t size_t__outSize)
{
    size_t size_t__used = 0u;
    uint8_t uint8_t__slot;

    if ((char__out == NULL) || (size_t__outSize < 4u))
    {
        return 0u;
    }
    char__out[0] = '\0';
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        int int__written = snprintf(&char__out[size_t__used],
                                    size_t__outSize - size_t__used,
                                    "%s%08lu",
                                    (size_t__used == 0u) ? "" : " ",
                                    (unsigned long)UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (int__written <= 0)
        {
            break;
        }
        size_t__used += (size_t)int__written;
        if (size_t__used >= (size_t__outSize - 12u))
        {
            break;
        }
    }
    return size_t__used;
}
