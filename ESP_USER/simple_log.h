/* simple_log.h - تاریخچهٔ وضعیت پنل ساده (فقط فارسی)
   هر روز یک فایل CSV روی فلش خود پنل؛ ردیف فقط وقتی نوشته می‌شود که چیزی
   واقعاً عوض شده باشد. هر ردیف همان لحظه بسته می‌شود تا قطع باتری آخرین ردیف
   را از بین نبرد.
   ترتیب include: بعد از simple_state.h و پیش از simple_link.h. */

#include <string.h>

/* ==================== ساعت و روز ==================== */
/* متغیر INT32_T__G__LogTzSec در simple_state.h تعریف شده تا همراه ساعت ذخیره شود. */
/* مقدار نگهبان: هنوز روزی باز نشده. روز صفر یک روز واقعی است (بردی که هنوز
   گوشی‌ای ندیده) و ذخیره‌سازی هرگز منتظر ساعت نمی‌ماند. */
static uint32_t UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
static uint16_t UINT16_T__G__LogRowsToday = 0u;
static bool     BOOL__G__LogDayFull = false;
static bool     BOOL__G__LogBootRowPending = true;
/* [EN] 0 = the last flash write was verified. Anything else = failed writes
        since the last good one. Same fix as the technical panel (2026-10-27):
        a refused write must be reported, not swallowed.
   [FA] صفر یعنی آخرین نوشتن روی فلش تأیید شد. هر مقدار دیگر یعنی تعداد
        نوشتن‌های شکست‌خورده از آخرین نوشتن سالم. همان اصلاح پنل فنی
        (۲۰۲۶-۱۰-۲۷): نوشتن ردشده باید گزارش شود، نه بی‌صدا دور ریخته. */
static uint8_t  UINT8_T__G__LogWriteFault = 0u;
/* [EN] Rows the RAM ring had to throw away because a newer one arrived while the
        flash was still refusing. Counted and shown, never silently forgotten.
   [FA] ردیف‌هایی که حلقهٔ رم ناچار دور ریخت، چون ردیف تازه‌تری رسید و فلش هنوز
        نمی‌پذیرفت. شمرده و نشان داده می‌شوند، بی‌صدا فراموش نمی‌شوند. */
static uint16_t UINT16_T__G__LogDroppedRows = 0u;
/* [EN] the day-full marker is written once per day.
   [FA] ردیف نشانهٔ «سقف روز پر شد» یک‌بار در روز نوشته می‌شود. */
static bool     BOOL__G__LogMarkerDone = false;

/* ==================== وضعیت قبلی و تغییر در انتظار ==================== */
static bool     BOOL__G__LogHasPrev = false;
static uint8_t  UINT8_T__G__LogPrevMains = 0u;
/* [EN] v1.95: the board's general byte (hand mode, snapshot, valid
        measurement) is logged too, so a mode change is an event.
   [FA] نسخهٔ ۱٫۹۵: بایت عمومی برد (مود دستی، snapshot، معتبر بودن
        اندازه‌گیری) هم ثبت می‌شود تا تغییر مود یک رویداد باشد. */
static uint8_t  UINT8_T__G__LogPrevFl = 0u;
/* [EN] v1.95: the condemned-battery verdict is a state too, and it is
        NOT in the fault mask - it has its own field on the wire.
   [FA] حکم باتری خراب هم یک وضعیت است و در بیت‌های خطا نیست؛
        فیلد خودش را روی سیم دارد. */
static uint32_t UINT32_T__G__LogPrevDead = 0u;
/* [EN] v1.96: the pack voltage itself is a change trigger, in visible
        steps - that is what draws the charge curve in the log.
   [FA] نسخهٔ ۱٫۹۶: خودِ ولتاژ پک هم محرک تغییر است، با گام محسوس -
        همان چیزی که منحنی شارژ را در لاگ می‌کشد. */
static uint32_t UINT32_T__G__LogPrevV24 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg1 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg2 = 0u;
static uint8_t  UINT8_T__G__LogPrevFlags2 = 0u;
static uint8_t  UINT8_T__G__LogPrevBand = 0u;
static uint32_t UINT32_T__G__LogPrevFault = 0u;
static uint8_t  UINT8_T__G__LogPrevNote = 0u;
static uint16_t UINT16_T__G__LogPendingMask = 0u;
static uint8_t  UINT8_T__G__LogPendingNote = 0u;
static bool     BOOL__G__LogMainsQueuedValid = false;
static uint32_t UINT32_T__G__LogMainsQueuedMs = 0u;
static uint32_t UINT32_T__G__LogLastRowMs = 0u;
static bool     BOOL__G__LogLastRowValid = false;
static bool     BOOL__G__LogLinkDown = false;
/* لحظهٔ نوشتن ردیف بوتِ همین نشست؛ تایمرِ «برد ساکت است» از همین لحظه
   می‌شمارد، حتی اگر هیچ فریمی از برد نرسیده باشد. */
static uint32_t UINT32_T__G__LogSessionStartMs = 0u;
/* لحظهٔ آخرین ردیف گام ولتاژ، تا باری که روشن و خاموش می‌شود گام را به
   چشمک‌زدن تبدیل نکند. */
static uint32_t UINT32_T__G__LogLastStepMs = 0u;
static bool     BOOL__G__LogLastStepValid = false;

/* ==================== فهرست روزها ==================== */
/* [EN] The list is read from the DIRECTORY, not from a remembered index: a day
        file the index did not know about was invisible in the picker, could
        never be deleted by "clear history", and still came back through the
        automatic "today" read. The scan cache below only saves flash reads
        inside one page refresh.
   [FA] فهرست از «خود پوشه» خوانده می‌شود، نه از فهرستی که به یاد سپرده شده:
        فایل روزی که فهرست نمی‌شناخت نه در انتخاب‌گر دیده می‌شد، نه با «پاک
        کردن تاریخچه» پاک می‌شد، و باز هم از راه خواندن خودکار «امروز» برمی‌گشت.
        حافظهٔ زیر فقط برای این است که یک بار باز شدن صفحه چند بار فلش نخواند. */
static uint32_t UINT32_T__G__LogIndexDay[SIMPLE_LOG_MAX_DAYS + 2u];
static uint8_t  UINT8_T__G__LogIndexCount = 0u;
static uint32_t UINT32_T__G__LogLastScanMs = 0u;
static bool     BOOL__G__LogScanValid = false;
/* [EN] A firmware update was seen at this boot, so the row must be written no
        matter what.
   [FA] در همین بوت یک بروزرسانی فرم‌ور دیده شد، پس ردیفش باید بی‌قید و شرط
        نوشته شود. */
static bool     BOOL__G__LogUpdatePending = false;
/* [EN] One-shot: the next row ignores the day cap too.
   [FA] یک‌بارمصرف: ردیف بعدی سقف روز را هم نادیده می‌گیرد. */
static bool     BOOL__A__LogForceRow = false;
static char     CHAR__G__LogRamRow[SIMPLE_LOG_RAM_ROWS][SIMPLE_LOG_RAM_ROW_MAX];
/* [EN] the lost-row count as text, for the X-Log-Lost header.
   [FA] شمار ردیف‌های ازدست‌رفته به‌صورت متن، برای سرسطر X-Log-Lost. */
static char     CHAR__G__LogLostText[8];
static uint8_t  UINT8_T__G__LogRamHead = 0u;
static uint8_t  UINT8_T__G__LogRamCount = 0u;
static bool     BOOL__G__LogDrainWait = false;
static uint32_t UINT32_T__G__LogLastDrainMs = 0u;

/* ==================== بیت‌های تغییر ==================== */
#define SIMPLE_LOG_CHG_BOOT    0x0001u
#define SIMPLE_LOG_CHG_MAINS   0x0002u
#define SIMPLE_LOG_CHG_CHG1    0x0004u
#define SIMPLE_LOG_CHG_CHG2    0x0008u
#define SIMPLE_LOG_CHG_FAULT   0x0010u
#define SIMPLE_LOG_CHG_FLAGS   0x0020u
#define SIMPLE_LOG_CHG_BAND    0x0040u
#define SIMPLE_LOG_CHG_LINK    0x0080u
#define SIMPLE_LOG_CHG_MODE    0x0100u   /* [EN] hand mode / data validity / [FA] مود دستی و اعتبار داده */
#define SIMPLE_LOG_CHG_DEAD    0x0200u   /* [EN] condemned-battery verdict / [FA] حکم باتری خراب */
#define SIMPLE_LOG_CHG_UPDATE  0x1000u   /* [EN] panel flashed with a new firmware / [FA] پنل با فرم‌ور تازه فلش شد */
#define SIMPLE_LOG_CHG_VSTEP   0x0400u   /* [EN] pack voltage moved a visible step / [FA] ولتاژ پک یک پلهٔ محسوس جابه‌جا شد */

#define SIMPLE_LOG_NOTE_NONE      0u
#define SIMPLE_LOG_NOTE_LOW_STOP  1u
#define SIMPLE_LOG_NOTE_LINK_LOST 2u
#define SIMPLE_LOG_NOTE_LINK_BACK 3u
#define SIMPLE_LOG_NOTE_DAY_FULL  4u
#define SIMPLE_LOG_NOTE_HEARTBEAT  5u   /* چیزی عوض نشد، پنل زنده است */
#define SIMPLE_LOG_NOTE_MANUAL     6u   /* ثبت فوری با زدن دکمهٔ به‌روزرسانی */

#define SIMPLE_LOG_PATH_MAX       24u

/* ==================== تبدیل زمان ==================== */

/* کد روز YYYYMMDD از شمارهٔ روز میلادی (روزهای بعد از ۱۹۷۰) */
static uint32_t func__SimpleLog_DateCode(uint32_t uint32_t__days)
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

static uint32_t func__SimpleLog_TodayCode(void)
{
    uint32_t uint32_t__nowSec;
    int32_t int32_t__local;

    uint32_t__nowSec = func__Simple_NowSec();
    if (uint32_t__nowSec < SIMPLE_TIME_VALID_MIN)
    {
        return 0u;
    }
    int32_t__local = (int32_t)uint32_t__nowSec + INT32_T__G__LogTzSec;
    if (int32_t__local < 0)
    {
        return 0u;
    }
    return func__SimpleLog_DateCode((uint32_t)(int32_t__local / 86400));
}

static void func__SimpleLog_PathOf(char *char__ptr_path, uint32_t uint32_t__dayCode)
{
    (void)snprintf(char__ptr_path, SIMPLE_LOG_PATH_MAX, "%s%08lu.csv",
                   SIMPLE_LOG_FILE_PREFIX, (unsigned long)uint32_t__dayCode);
}

/**
 * @brief  [EN] Day code out of a file name: "/evt-20261028.csv" -> 20261028,
 *              "0" for the day the clock has not named yet. Anything else is
 *              rejected, so no other file in the folder is mistaken for a day.
 *         [FA] کد روز از نام فایل: «‎/evt-20261028.csv‎» ← ۲۰۲۶۱۰۲۸، و صفر
 *              برای روزی که ساعت هنوز نامش را نبرده. هر چیز دیگر رد می‌شود تا
 *              هیچ فایل دیگری در پوشه به‌جای فایل روز گرفته نشود.
 * @param  const char *char__ptr_name [EN] file name / [FA] نام فایل
 * @param  uint32_t *uint32_t__ptr_day [EN] output / [FA] خروجی
 * @return bool [EN] it is a day file / [FA] فایل روز است
 */
static bool func__SimpleLog_DayFromName(const char *char__ptr_name,
                                        uint32_t *uint32_t__ptr_day)
{
    const char *char__ptr_prefix = &SIMPLE_LOG_FILE_PREFIX[1];
    uint8_t uint8_t__nameIndex = 0u;
    uint8_t uint8_t__prefixIndex = 0u;
    uint8_t uint8_t__digits = 0u;
    uint32_t uint32_t__day = 0u;

    if ((char__ptr_name == NULL) || (uint32_t__ptr_day == NULL))
    {
        return false;
    }
    if (char__ptr_name[0] == '/')
    {
        uint8_t__nameIndex = 1u;
    }
    while ((char__ptr_prefix[uint8_t__prefixIndex] != '\0') &&
           (char__ptr_name[uint8_t__nameIndex] == char__ptr_prefix[uint8_t__prefixIndex]))
    {
        uint8_t__nameIndex++;
        uint8_t__prefixIndex++;
    }
    if (char__ptr_prefix[uint8_t__prefixIndex] != '\0')
    {
        return false;
    }
    while ((uint8_t__digits < 8u) &&
           (char__ptr_name[uint8_t__nameIndex] >= '0') &&
           (char__ptr_name[uint8_t__nameIndex] <= '9'))
    {
        uint32_t__day = (uint32_t__day * 10u) +
                        (uint32_t)(char__ptr_name[uint8_t__nameIndex] - '0');
        uint8_t__digits++;
        uint8_t__nameIndex++;
    }
    if (uint8_t__digits != 8u)
    {
        return false;
    }
    if ((char__ptr_name[uint8_t__nameIndex] != '.') ||
        (char__ptr_name[uint8_t__nameIndex + 1u] != 'c') ||
        (char__ptr_name[uint8_t__nameIndex + 2u] != 's') ||
        (char__ptr_name[uint8_t__nameIndex + 3u] != 'v') ||
        (char__ptr_name[uint8_t__nameIndex + 4u] != '\0'))
    {
        return false;
    }
    *uint32_t__ptr_day = uint32_t__day;
    return true;
}

/* ==================== ثبت روز پیداشده در حافظهٔ موقت ==================== */

static void func__SimpleLog_RecordDayEntry(const char *char__ptr_name)
{
    uint32_t uint32_t__day = 0u;
    uint8_t uint8_t__slot;

    if (func__SimpleLog_DayFromName(char__ptr_name, &uint32_t__day) == false)
    {
        return;
    }
    if (UINT8_T__G__LogIndexCount >= (SIMPLE_LOG_MAX_DAYS + 2u))
    {
        return;
    }
    uint8_t__slot = UINT8_T__G__LogIndexCount;
    while ((uint8_t__slot > 0u) &&
           (UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] > uint32_t__day))
    {
        UINT32_T__G__LogIndexDay[uint8_t__slot] =
            UINT32_T__G__LogIndexDay[uint8_t__slot - 1u];
        uint8_t__slot--;
    }
    UINT32_T__G__LogIndexDay[uint8_t__slot] = uint32_t__day;
    UINT8_T__G__LogIndexCount++;
}

/**
 * @brief  [EN] Fill the day cache from the DIRECTORY, oldest first. Two slots
 *              more than the retention keeps are read, so the trim below always
 *              sees the oldest day. A page that asks again inside the cache
 *              window gets the cache without touching the flash.
 *         [FA] پر کردن حافظهٔ روزها از «خود پوشه»، قدیمی‌تر اول. دو خانه بیش
 *              از سقف نگه‌داری خوانده می‌شود تا پاک‌سازی همیشه قدیمی‌ترین روز
 *              را ببیند. صفحه‌ای که داخل پنجرهٔ حافظه دوباره بپرسد، پاسخ را
 *              بدون دست‌زدن به فلش می‌گیرد.
 * @param  bool bool__force [EN] true = read the flash now / [FA] درست = همین حالا
 * @return uint8_t [EN] days found / [FA] تعداد روزهای پیداشده
 */
static uint8_t func__SimpleLog_ScanDays(bool bool__force)
{
    if (BOOL__G__FsOk == false)
    {
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false;
        return 0u;
    }
    if ((bool__force == false) && (BOOL__G__LogScanValid != false) &&
        ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastScanMs) < SIMPLE_LOG_SCAN_MIN_MS))
    {
        return UINT8_T__G__LogIndexCount;
    }

    UINT32_T__G__LogLastScanMs = (uint32_t)millis();
    BOOL__G__LogScanValid = true;
    UINT8_T__G__LogIndexCount = 0u;

#if defined(ESP32)
    {
        File file__root = LittleFS.open("/", "r");
        if (file__root)
        {
            File file__entry = file__root.openNextFile();
            while (file__entry)
            {
                func__SimpleLog_RecordDayEntry(file__entry.name());
                file__entry.close();
                if (UINT8_T__G__LogIndexCount >= (SIMPLE_LOG_MAX_DAYS + 2u))
                {
                    break;
                }
                file__entry = file__root.openNextFile();
            }
            file__root.close();
        }
    }
#else
    {
        Dir dir__root = LittleFS.openDir("/");

        while (dir__root.next())
        {
            func__SimpleLog_RecordDayEntry(dir__root.fileName().c_str());
            if (UINT8_T__G__LogIndexCount >= (SIMPLE_LOG_MAX_DAYS + 2u))
            {
                break;
            }
        }
    }
#endif
    return UINT8_T__G__LogIndexCount;
}

/* ==================== فهرست روزها ==================== */

/* [EN] Write one text piece and report whether every byte landed: File::print
        already returns the count it really wrote, so a full flash is visible.
   [FA] نوشتن یک تکه متن و گزارش این‌که همهٔ بایت‌ها نشستند: print تعداد واقعی
        را برمی‌گرداند، پس فلش پر همین‌جا دیده می‌شود. */
static bool func__SimpleLog_FileText(File &file__target, const char *char__text)
{
    size_t size_t__want;
    size_t size_t__got;

    if (char__text == NULL)
    {
        return false;
    }
    size_t__want = strlen(char__text);
    if (size_t__want == 0u)
    {
        return true;
    }
    size_t__got = file__target.print(char__text);
    return (size_t__got == size_t__want);
}

/* [EN] Remember the failure (saturating, so a flash that stays full cannot
        wrap the counter back to "healthy").
   [FA] ثبت خرابی (اشباع‌شونده، تا فلشی که پر می‌ماند شمارنده را به «سالم»
        برنگرداند). */
static void func__SimpleLog_FaultRaise(void)
{
    if (UINT8_T__G__LogWriteFault < 0xFFu)
    {
        UINT8_T__G__LogWriteFault++;
    }
}

/* [EN] v1.113: the two index functions are gone. The day list is the folder
        itself now (func__SimpleLog_ScanDays), so it can never disagree with
        what is really on the flash: the old "/evt-days.txt" was the reason a
        day file could exist, stay undeletable and never be listed.
   [FA] نسخهٔ ۱٫۱۱۳: دو تابع فهرست حذف شدند. فهرست روزها حالا خودِ پوشه است
        (‎func__SimpleLog_ScanDays‎)، پس هرگز با آنچه واقعاً روی فلش است
        اختلاف ندارد: همان «‎/evt-days.txt‎» باعث می‌شد فایل روزی وجود داشته
        باشد، پاک نشود و در فهرست هم نیاید. */

/* ==================== مهر نسخهٔ فرم‌ور / firmware stamp ==================== */
/**
 * @brief  [EN] Which firmware version is remembered in the clock file? 0 means
 *              "nothing remembered yet" (a fresh flash), never "an update".
 *         [FA] کدام نسخهٔ فرم‌ور در فایل ساعت به یاد سپرده شده؟ صفر یعنی
 *              «هنوز چیزی به یاد نیست» (فلش تازه)، هرگز یعنی «بروزرسانی».
 * @return uint32_t [EN] stored version / [FA] نسخهٔ ذخیره‌شده
 */
static uint32_t func__SimpleLog_FwStampRead(void)
{
    String string__content;
    int32_t int32_t__pos;
    int32_t int32_t__end;
    int32_t int32_t__comma;

    if ((BOOL__G__FsOk == false) || (LittleFS.exists(SIMPLE_TIME_FILE) == false))
    {
        return 0u;
    }
    {
        File file__read = LittleFS.open(SIMPLE_TIME_FILE, "r");

        if (!file__read)
        {
            return 0u;
        }
        string__content = file__read.readString();
        file__read.close();
    }
    int32_t__pos = string__content.indexOf("\"fw\":");
    if (int32_t__pos < 0)
    {
        return 0u;
    }
    int32_t__pos += 5;
    int32_t__end = string__content.indexOf("}", int32_t__pos);
    int32_t__comma = string__content.indexOf(",", int32_t__pos);
    if ((int32_t__comma > int32_t__pos) &&
        ((int32_t__end < 0) || (int32_t__comma < int32_t__end)))
    {
        int32_t__end = int32_t__comma;
    }
    if (int32_t__end <= int32_t__pos)
    {
        return 0u;
    }
    {
        String string__value = string__content.substring(int32_t__pos, int32_t__end);
        string__value.trim();
        if (string__value.length() == 0u)
        {
            return 0u;
        }
        return (uint32_t)string__value.toInt();
    }
}

/**
 * @brief  [EN] Write the running firmware version into the clock file next to
 *              ts/tz. The old values are read back first, so this never erases
 *              the clock; with no file at all it writes zero time (which the
 *              loader ignores) and just the stamp.
 *         [FA] نوشتن نسخهٔ فرم‌ور در حال اجرا در فایل ساعت، کنار ts/tz. مقدار
 *              قبلی اول خوانده می‌شود، پس این کار هرگز ساعت را پاک نمی‌کند؛
 *              اگر فایل نباشد، ساعت صفر (که خواننده نادیده می‌گیرد) و فقط
 *              مهر نوشته می‌شود.
 * @param  uint32_t uint32_t__version [EN] version / [FA] نسخه
 * @return [EN] None / [FA] ندارد
 */
static void func__SimpleLog_FwStampWrite(uint32_t uint32_t__version)
{
    uint32_t uint32_t__savedSec = 0u;
    int32_t  int32_t__savedTz = 0;
    String string__content;
    int32_t int32_t__pos;
    int32_t int32_t__end;

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    if (LittleFS.exists(SIMPLE_TIME_FILE) != false)
    {
        File file__read = LittleFS.open(SIMPLE_TIME_FILE, "r");
        if (file__read)
        {
            string__content = file__read.readString();
            file__read.close();
            int32_t__pos = string__content.indexOf("\"ts\":");
            if (int32_t__pos >= 0)
            {
                int32_t__pos += 5;
                int32_t__end = string__content.indexOf(",", int32_t__pos);
                if (int32_t__end < 0)
                {
                    int32_t__end = string__content.indexOf("}", int32_t__pos);
                }
                if (int32_t__end > int32_t__pos)
                {
                    String string__value =
                        string__content.substring(int32_t__pos, int32_t__end);
                    string__value.trim();
                    uint32_t__savedSec = (uint32_t)string__value.toInt();
                }
            }
            int32_t__pos = string__content.indexOf("\"tz\":");
            if (int32_t__pos >= 0)
            {
                int32_t__pos += 5;
                int32_t__end = string__content.indexOf(",", int32_t__pos);
                if (int32_t__end < 0)
                {
                    int32_t__end = string__content.indexOf("}", int32_t__pos);
                }
                if (int32_t__end > int32_t__pos)
                {
                    String string__value =
                        string__content.substring(int32_t__pos, int32_t__end);
                    string__value.trim();
                    int32_t__savedTz = (int32_t)string__value.toInt();
                }
            }
        }
    }
    File file__write = LittleFS.open(SIMPLE_TIME_FILE, "w");
    if (!file__write)
    {
        return;
    }
    file__write.print("{\"ts\":");
    file__write.print((unsigned long)uint32_t__savedSec);
    file__write.print(",\"tz\":");
    file__write.print((long)int32_t__savedTz);
    file__write.print(",\"fw\":");
    file__write.print((unsigned long)uint32_t__version);
    file__write.print("}");
    file__write.close();
    UINT32_T__G__LastSavedTimeSec = uint32_t__savedSec;
    INT32_T__G__LastSavedTzSec = int32_t__savedTz;
}

/**
 * @brief  [EN] Is this boot the first one of a new firmware? The remembered
 *              version is compared with the running one; "nothing remembered"
 *              is a fresh flash and is not an update. Either way the stamp is
 *              brought up to date.
 *         [FA] آیا این بوت، اولین بوت یک فرم‌ور تازه است؟ نسخهٔ به‌یادسپرده با
 *              نسخهٔ در حال اجرا مقایسه می‌شود؛ «چیزی به یاد نیست» یعنی فلش
 *              تازه و بروزرسانی حساب نمی‌شود. در هر دو حالت مهر تازه می‌شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__SimpleLog_FwCheck(void)
{
    uint32_t uint32_t__stored;

    uint32_t__stored = func__SimpleLog_FwStampRead();
    if ((uint32_t__stored != 0u) &&
        (uint32_t__stored != (uint32_t)SIMPLE_ESP_FW_VERSION))
    {
        BOOL__G__LogUpdatePending = true;
    }
    if (uint32_t__stored != (uint32_t)SIMPLE_ESP_FW_VERSION)
    {
        func__SimpleLog_FwStampWrite((uint32_t)SIMPLE_ESP_FW_VERSION);
    }
}

static uint32_t func__SimpleLog_DayBytes(uint32_t uint32_t__dayCode)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    uint32_t uint32_t__bytes = 0u;

    func__SimpleLog_PathOf(char__ptr_path, uint32_t__dayCode);
    File file__day = LittleFS.open(char__ptr_path, "r");
    if (!file__day)
    {
        return 0u;
    }
    uint32_t__bytes = (uint32_t)file__day.size();
    file__day.close();
    return uint32_t__bytes;
}

static void func__SimpleLog_TrimDays(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    uint8_t uint8_t__slot;

    while (UINT8_T__G__LogIndexCount > SIMPLE_LOG_MAX_DAYS)
    {
        func__SimpleLog_PathOf(char__ptr_path, UINT32_T__G__LogIndexDay[0]);
        (void)LittleFS.remove(char__ptr_path);
        for (uint8_t__slot = 1u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] =
                UINT32_T__G__LogIndexDay[uint8_t__slot];
        }
        UINT8_T__G__LogIndexCount--;
    }

    /* تعداد روز تنها فلش را کران‌دار نمی‌کند؛ یک روز شلوغ از یک هفتهٔ آرام
       بزرگ‌تر است. کل تاریخچه داخل یک بودجهٔ بایتی می‌ماند و اول قدیمی‌ترین
       روز می‌رود، تا پر شدن فلش هرگز ثبت را نخواباند. */
    while (UINT8_T__G__LogIndexCount > 1u)
    {
        uint32_t uint32_t__bytes = 0u;

        for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            uint32_t__bytes += func__SimpleLog_DayBytes(UINT32_T__G__LogIndexDay[uint8_t__slot]);
        }
        if (uint32_t__bytes <= SIMPLE_LOG_TOTAL_BUDGET_BYTES)
        {
            break;
        }
        func__SimpleLog_PathOf(char__ptr_path, UINT32_T__G__LogIndexDay[0]);
        (void)LittleFS.remove(char__ptr_path);
        for (uint8_t__slot = 1u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] =
                UINT32_T__G__LogIndexDay[uint8_t__slot];
        }
        UINT8_T__G__LogIndexCount--;
    }
    BOOL__G__LogScanValid = false;   /* the folder changed: the next read re-scans */
}

static uint16_t func__SimpleLog_CountRows(const char *char__ptr_path)
{
    uint16_t uint16_t__rows = 0u;

    File file__day = LittleFS.open(char__ptr_path, "r");
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

/* پایین‌تر تعریف می‌شود؛ باز کردن فایل روز، دنبالهٔ خراب را تعمیر می‌کند. */
static bool func__SimpleLog_RepairTail(const char *char__ptr_path);

static bool func__SimpleLog_OpenDay(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    bool bool__isNew;
    bool bool__ok = true;
    bool bool__alreadyIndexed = false;
    uint32_t uint32_t__dayCode;
    uint8_t uint8_t__slot;

    if (BOOL__G__FsOk == false)
    {
        return false;
    }
    uint32_t__dayCode = func__SimpleLog_TodayCode();
    if (uint32_t__dayCode == UINT32_T__G__LogDayCode)
    {
        return true;
    }

    func__SimpleLog_PathOf(char__ptr_path, uint32_t__dayCode);
    bool__isNew = (LittleFS.exists(char__ptr_path) == false);
    File file__day = LittleFS.open(char__ptr_path, "a");
    if (!file__day)
    {
        return false;
    }
    if (bool__isNew)
    {
        bool__ok = func__SimpleLog_FileText(file__day, SIMPLE_LOG_HEADER);
    }
    file__day.close();
    if ((bool__ok != false) && (bool__isNew == false))
    {
        bool__ok = func__SimpleLog_RepairTail(char__ptr_path);
    }
    if (bool__ok == false)
    {
        func__SimpleLog_FaultRaise();
    }

    UINT32_T__G__LogDayCode = uint32_t__dayCode;
    UINT16_T__G__LogRowsToday = func__SimpleLog_CountRows(char__ptr_path);
    BOOL__G__LogDayFull = (UINT16_T__G__LogRowsToday >= SIMPLE_LOG_MAX_ROWS_PER_DAY);
    BOOL__G__LogMarkerDone = BOOL__G__LogDayFull;

    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        if (UINT32_T__G__LogIndexDay[uint8_t__slot] == uint32_t__dayCode)
        {
            bool__alreadyIndexed = true;
        }
    }
    if ((bool__alreadyIndexed == false) &&
        (UINT8_T__G__LogIndexCount < (SIMPLE_LOG_MAX_DAYS + 2u)))
    {
        UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount] = uint32_t__dayCode;
        UINT8_T__G__LogIndexCount++;
        func__SimpleLog_TrimDays();
    }
    return bool__ok;
}

/* ==================== نوشتن ردیف ==================== */

static void func__SimpleLog_SaveLast(void);

/**
 * @brief  [EN] Battery percent from the DISCHARGE map (param 74/75, factory
 *              21,000..29,000 mV), stored with every row so later analysis
 *              keeps the same scale even if the map is retuned. Unknown params
 *              fall back to the factory map.
 *         [FA] درصد باتری از نگاشت «دشارژ» (پارامتر ۷۴/۷۵، کارخانه
 *              ۲۱٬۰۰۰..۲۹٬۰۰۰ میلی‌ولت) که با هر ردیف ذخیره می‌شود تا تحلیل
 *              بعدی همان مقیاس را نگه دارد، حتی اگر نگاشت بعدا تنظیم شود.
 *              اگر پارامتر هنوز نرسیده باشد، نگاشت کارخانه به کار می‌رود.
 * @param  uint32_t__batteryMv [EN] Pack voltage in mV / ولتاژ پک
 * @return uint8_t [EN] Percent 0..100‎ / درصد
 */
static uint8_t func__SimpleLog_Pct(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__vminMv = 21000u;
    uint32_t uint32_t__vmaxMv = 29000u;
    uint32_t uint32_t__rangeMv;

    if (BOOL__G__BoardParamKnown[SIMPLE_PARAM_PCT_VMIN_MV] != false)
    {
        uint32_t__vminMv = UINT32_T__G__BoardParam[SIMPLE_PARAM_PCT_VMIN_MV];
    }
    if (BOOL__G__BoardParamKnown[SIMPLE_PARAM_PCT_VMAX_MV] != false)
    {
        uint32_t__vmaxMv = UINT32_T__G__BoardParam[SIMPLE_PARAM_PCT_VMAX_MV];
    }
    if (uint32_t__batteryMv <= uint32_t__vminMv)
    {
        return 0u;
    }
    if (uint32_t__batteryMv >= uint32_t__vmaxMv)
    {
        return 100u;
    }
    uint32_t__rangeMv = uint32_t__vmaxMv - uint32_t__vminMv;
    if (uint32_t__rangeMv == 0u)
    {
        return 0u;
    }
    return (uint8_t)(((uint32_t__batteryMv - uint32_t__vminMv) * 100u) / uint32_t__rangeMv);
}

/* [EN] A refused write is not a lost row: it goes to the RAM ring.
   [FA] نوشتن ردشده یعنی ردیف گم‌شده نیست: به حلقهٔ رم می‌رود. */
static void func__SimpleLog_RamPush(const char *char__row)
{
    if (char__row == NULL)
    {
        return;
    }
    if (UINT8_T__G__LogRamCount >= SIMPLE_LOG_RAM_ROWS)
    {
        if (UINT16_T__G__LogDroppedRows < 0xFFFFu)
        {
            UINT16_T__G__LogDroppedRows++;
        }
    }
    (void)strncpy(CHAR__G__LogRamRow[UINT8_T__G__LogRamHead],
                  char__row,
                  SIMPLE_LOG_RAM_ROW_MAX - 1u);
    CHAR__G__LogRamRow[UINT8_T__G__LogRamHead][SIMPLE_LOG_RAM_ROW_MAX - 1u] = '\0';
    UINT8_T__G__LogRamHead = (uint8_t)((UINT8_T__G__LogRamHead + 1u) % SIMPLE_LOG_RAM_ROWS);
    if (UINT8_T__G__LogRamCount < SIMPLE_LOG_RAM_ROWS)
    {
        UINT8_T__G__LogRamCount++;
    }
}

/* [EN] A power cut in the middle of a row leaves no closing newline, so the next
        append would glue a good row onto a broken one and make BOTH unreadable.
        Runs once, when a day file is opened.
   [FA] قطع برق وسط یک ردیف، پایان‌بند را جا می‌گذارد؛ در آن حالت ردیف بعدی به
        ردیف خراب می‌چسبد و هر دو ناخوانا می‌شوند. یک‌بار هنگام باز شدن فایل
        روز اجرا می‌شود. */
static bool func__SimpleLog_RepairTail(const char *char__ptr_path)
{
    bool bool__needsNewline = false;

    {
        File file__read = LittleFS.open(char__ptr_path, "r");
        if (file__read)
        {
            uint32_t uint32_t__size = (uint32_t)file__read.size();
            if (uint32_t__size > 0u)
            {
                if ((file__read.seek(uint32_t__size - 1u) == false) ||
                    (file__read.read() != '\n'))
                {
                    bool__needsNewline = true;
                }
            }
            file__read.close();
        }
    }
    if (bool__needsNewline == false)
    {
        return true;
    }
    {
        File file__append = LittleFS.open(char__ptr_path, "a");
        if (!file__append)
        {
            func__SimpleLog_FaultRaise();
            return false;
        }
        bool__needsNewline = func__SimpleLog_FileText(file__append, "\n");
        file__append.close();
    }
    if (bool__needsNewline == false)
    {
        func__SimpleLog_FaultRaise();
    }
    return bool__needsNewline;
}

/* [EN] THE one append path: one verified row plus its newline into one day file.
   [FA] تنها مسیر نوشتن: یک ردیف تأییدشده به‌همراه پایان‌بندش در فایل یک روز. */
static bool func__SimpleLog_AppendRow(uint32_t uint32_t__dayCode, const char *char__row)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    bool bool__ok = false;

    if ((BOOL__G__FsOk == false) || (char__row == NULL))
    {
        return false;
    }
    func__SimpleLog_PathOf(char__ptr_path, uint32_t__dayCode);
    {
        File file__day = LittleFS.open(char__ptr_path, "a");
        if (!file__day)
        {
            func__SimpleLog_FaultRaise();
            return false;
        }
        bool__ok = func__SimpleLog_FileText(file__day, char__row);
        if (bool__ok != false)
        {
            bool__ok = func__SimpleLog_FileText(file__day, "\n");
        }
        file__day.close();
    }
    if (bool__ok == false)
    {
        func__SimpleLog_FaultRaise();
    }
    return bool__ok;
}

/* [EN] The leading number of a CSV row (its epoch field).
   [FA] عدد ابتدای یک ردیف (ستون زمان آن). */
static uint32_t func__SimpleLog_RowEpoch(const char *char__row)
{
    uint32_t uint32_t__value = 0u;
    uint8_t uint8_t__index = 0u;

    if (char__row == NULL)
    {
        return 0u;
    }
    while ((uint8_t__index < 10u) &&
           (char__row[uint8_t__index] >= '0') &&
           (char__row[uint8_t__index] <= '9'))
    {
        uint32_t__value = (uint32_t__value * 10u) +
                          (uint32_t)(char__row[uint8_t__index] - '0');
        uint8_t__index++;
    }
    return uint32_t__value;
}

/* [EN] Move the rows the RAM ring is holding back into the day files they belong
        to (each row by its OWN epoch). Oldest first; it stops at the first
        refusal and keeps the rest.
   [FA] برگرداندن ردیف‌های منتظر در رم به فایل روزِ خودشان (هر ردیف با زمان
        خودش). از قدیمی‌ترین شروع می‌شود و با اولین رد شدن می‌ایستد. */
static void func__SimpleLog_DrainRam(void)
{
    while (UINT8_T__G__LogRamCount > 0u)
    {
        uint8_t uint8_t__oldest;
        uint32_t uint32_t__epoch;
        uint32_t uint32_t__dayCode;

        uint8_t__oldest = (uint8_t)((UINT8_T__G__LogRamHead + SIMPLE_LOG_RAM_ROWS -
                                     UINT8_T__G__LogRamCount) % SIMPLE_LOG_RAM_ROWS);
        uint32_t__epoch = func__SimpleLog_RowEpoch(CHAR__G__LogRamRow[uint8_t__oldest]);
        if (uint32_t__epoch >= SIMPLE_TIME_VALID_MIN)
        {
            uint32_t uint32_t__localSec = uint32_t__epoch + (uint32_t)INT32_T__G__LogTzSec;
            uint32_t__dayCode = func__SimpleLog_DateCode(uint32_t__localSec / 86400u);
        }
        else
        {
            uint32_t__dayCode = 0u;
        }
        if (func__SimpleLog_AppendRow(uint32_t__dayCode,
                                      CHAR__G__LogRamRow[uint8_t__oldest]) == false)
        {
            BOOL__G__LogDrainWait = true;
            UINT32_T__G__LogLastDrainMs = (uint32_t)millis();
            return;
        }
        if ((uint32_t__dayCode == UINT32_T__G__LogDayCode) &&
            (UINT16_T__G__LogRowsToday < 0xFFFFu))
        {
            UINT16_T__G__LogRowsToday++;
        }
        UINT8_T__G__LogRamCount--;
    }
    BOOL__G__LogDrainWait = false;
    UINT8_T__G__LogWriteFault = 0u;
}

static bool func__SimpleLog_WriteRow(uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    char char__row[256];
    uint32_t uint32_t__nowSec;
    uint32_t uint32_t__uptimeSec;
    uint8_t uint8_t__mains;
    bool bool__stored = false;

    /* [EN] v1.113: a forced row (the firmware-update row) ignores this cap as
            well - the user asked for it "with no conditions".
       [FA] نسخهٔ ۱٫۱۱۳: ردیف اجباری (ردیف بروزرسانی فرم‌ور) این سقف را هم
            نادیده می‌گیرد - دستور کاربر «بدون در نظر گرفتن شرط‌ها» بود. */
    if ((BOOL__A__LogForceRow == false) &&
        ((UINT16_T__G__LogRowsToday >= SIMPLE_LOG_MAX_ROWS_PER_DAY) ||
         (BOOL__G__LogDayFull != false)))
    {
        BOOL__G__LogDayFull = true;
        return false;
    }

    uint32_t__nowSec = func__Simple_NowSec();
    uint32_t__uptimeSec = (uint32_t)(millis() / 1000u);
    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;

    (void)snprintf(char__row, sizeof(char__row),
                   "%lu,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                   (unsigned long)uint32_t__nowSec,
                   (unsigned long)uint32_t__uptimeSec,
                   (unsigned int)uint16_t__changeMask,
                   (unsigned int)uint8_t__note,
                   (unsigned int)uint8_t__mains,
                   (unsigned int)UINT8_T__G__TlmFlags,
                   (unsigned long)UINT32_T__G__TlmField[6],
                   (unsigned long)UINT32_T__G__TlmField[13],
                   (unsigned long)UINT32_T__G__TlmField[19],
                   (unsigned int)UINT8_T__G__TlmFlags2,
                   (unsigned long)UINT32_T__G__TlmField[15],
                   (unsigned long)UINT32_T__G__TlmField[17],
                   (unsigned long)UINT32_T__G__TlmField[18],
                   (unsigned long)UINT32_T__G__TlmField[25],
                   (unsigned long)UINT32_T__G__TlmField[28],
                   (unsigned long)UINT32_T__G__TlmField[14],
                   (unsigned long)UINT32_T__G__TlmField[16],
                   (unsigned long)UINT32_T__G__TlmField[3],
                   (unsigned long)UINT32_T__G__TlmField[10],
                   (unsigned long)UINT32_T__G__TlmField[5],
                   (unsigned long)UINT32_T__G__TlmField[12],
                   (unsigned long)func__SimpleLog_Pct(UINT32_T__G__TlmField[15]));

    if (BOOL__G__FsOk != false)
    {
        if (func__SimpleLog_OpenDay() != false)
        {
            if ((BOOL__A__LogForceRow == false) &&
                ((UINT16_T__G__LogRowsToday >= SIMPLE_LOG_MAX_ROWS_PER_DAY) ||
                 (BOOL__G__LogDayFull != false)))
            {
                BOOL__G__LogDayFull = true;
                return false;
            }
            /* [EN] the flash takes rows again: the ones waiting in RAM go
                    first, so the history keeps its order.
               [FA] فلش دوباره ردیف می‌پذیرد: اول ردیف‌های منتظر در رم
                    می‌روند تا ترتیب تاریخچه حفظ شود. */
            if (UINT8_T__G__LogRamCount != 0u)
            {
                func__SimpleLog_DrainRam();
            }
            bool__stored = func__SimpleLog_AppendRow(UINT32_T__G__LogDayCode, char__row);
        }
    }

    if ((bool__stored == false) || (BOOL__G__FsOk == false))
    {
        func__SimpleLog_RamPush(char__row);
    }
    if (BOOL__G__FsOk != false)
    {
        if (bool__stored != false)
        {
            BOOL__G__LogDrainWait = false;
            UINT8_T__G__LogWriteFault = 0u;
        }
        else
        {
            BOOL__G__LogDrainWait = true;
            UINT32_T__G__LogLastDrainMs = (uint32_t)millis();
            func__SimpleLog_FaultRaise();
        }
    }

    /* [EN] the row counter is the day-file cap, so it only grows when the file
            really grew; with the file system off it keeps its old meaning.
       [FA] شمارندهٔ ردیف سقف فایل روز است، پس فقط با بزرگ شدن واقعی فایل بالا
            می‌رود؛ با فایل‌سیستم خاموش همان معنای قبلی را دارد. */
    if ((bool__stored != false) || (BOOL__G__FsOk == false))
    {
        if (UINT16_T__G__LogRowsToday < 0xFFFFu)
        {
            UINT16_T__G__LogRowsToday++;
        }
    }
    if ((uint16_t__changeMask & SIMPLE_LOG_CHG_VSTEP) != 0u)
    {
        UINT32_T__G__LogLastStepMs = (uint32_t)millis();
        BOOL__G__LogLastStepValid = true;
    }
    UINT32_T__G__LogPrevV24 = UINT32_T__G__TlmField[15];
    UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    BOOL__G__LogLastRowValid = true;
    UINT16_T__G__LogPendingMask = 0u;
    UINT8_T__G__LogPendingNote = 0u;
    BOOL__G__LogMainsQueuedValid = false;
    BOOL__A__LogForceRow = false;   /* the one forced row has been written */

    /* [EN] one slot before the cap, the day writes its own obituary: a row with
            note 4 saying "the cap was reached". Without it a full day looks
            like a dead panel.
       [FA] یک خانه پیش از سقف، روز خودش را توضیح می‌دهد: ردیفی با یادداشت ۴
            که می‌گوید «سقف پر شد». بدون آن، روزِ پر شبیه پنلِ مرده دیده
            می‌شود. */
    if ((bool__stored != false) && (BOOL__G__LogMarkerDone == false) &&
        (UINT16_T__G__LogRowsToday == (SIMPLE_LOG_MAX_ROWS_PER_DAY - 1u)))
    {
        char char__marker[256];

        BOOL__G__LogMarkerDone = true;
        (void)snprintf(char__marker, sizeof(char__marker),
                       "%lu,%lu,0,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                       (unsigned long)func__Simple_NowSec(),
                       (unsigned long)((uint32_t)millis() / 1000u),
                       (unsigned int)SIMPLE_LOG_NOTE_DAY_FULL,
                       (unsigned int)(((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u),
                       (unsigned int)UINT8_T__G__TlmFlags,
                       (unsigned long)UINT32_T__G__TlmField[6],
                       (unsigned long)UINT32_T__G__TlmField[13],
                       (unsigned long)UINT32_T__G__TlmField[19],
                       (unsigned int)UINT8_T__G__TlmFlags2,
                       (unsigned long)UINT32_T__G__TlmField[15],
                       (unsigned long)UINT32_T__G__TlmField[17],
                       (unsigned long)UINT32_T__G__TlmField[18],
                       (unsigned long)UINT32_T__G__TlmField[25],
                       (unsigned long)UINT32_T__G__TlmField[28],
                       (unsigned long)UINT32_T__G__TlmField[14],
                       (unsigned long)UINT32_T__G__TlmField[16],
                       (unsigned long)UINT32_T__G__TlmField[3],
                       (unsigned long)UINT32_T__G__TlmField[10],
                       (unsigned long)UINT32_T__G__TlmField[5],
                       (unsigned long)UINT32_T__G__TlmField[12],
                       (unsigned long)func__SimpleLog_Pct(UINT32_T__G__TlmField[15]));
        if (func__SimpleLog_AppendRow(UINT32_T__G__LogDayCode, char__marker) != false)
        {
            UINT16_T__G__LogRowsToday++;
        }
        BOOL__G__LogDayFull = true;
    }
    return true;
}

/* یادداشت «آخرین وضعیت پیش از خاموشی» - فقط روی تغییر خطا/قفل یا ورود به ناحیهٔ خطر */
static void func__SimpleLog_SaveLast(void)
{
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    File file__last = LittleFS.open(SIMPLE_LOG_LAST_PATH, "w");
    if (!file__last)
    {
        return;
    }
    file__last.print((unsigned long)func__Simple_NowSec());
    file__last.print(",");
    file__last.print((unsigned long)UINT32_T__G__TlmField[15]);
    file__last.print(",");
    file__last.print((unsigned long)UINT32_T__G__TlmField[19]);
    file__last.print(",");
    file__last.print((unsigned int)UINT8_T__G__TlmFlags2);
    file__last.print(",");
    file__last.println((UINT32_T__G__TlmField[15] < SIMPLE_LOG_CUT_BAND_MV) ? 1 : 0);
    file__last.close();
}

static uint8_t func__SimpleLog_LoadLastDanger(void)
{
    String string__content;
    int32_t int32_t__lastComma;

    if ((BOOL__G__FsOk == false) || (LittleFS.exists(SIMPLE_LOG_LAST_PATH) == false))
    {
        return 0u;
    }
    File file__last = LittleFS.open(SIMPLE_LOG_LAST_PATH, "r");
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
    return (string__content.substring((uint32_t)int32_t__lastComma + 1u).toInt() != 0) ? 1u : 0u;
}

/**
 * @brief  [EN] Absolute difference of two unsigned readings.
 *         [FA] اختلاف مطلق دو خوانش بدون علامت.
 * @‎return [EN] difference / [FA]‎ اختلاف
 */
static uint32_t func__SimpleLog_AbsDiff(uint32_t uint32_t__a, uint32_t uint32_t__b)
{
    return (uint32_t__a >= uint32_t__b) ? (uint32_t__a - uint32_t__b)
                                        : (uint32_t__b - uint32_t__a);
}

/* ==================== تشخیص تغییر ==================== */

static void func__SimpleLog_Queue(uint16_t uint16_t__mask, uint8_t uint8_t__note)
{
    uint32_t uint32_t__nowMs;

    UINT16_T__G__LogPendingMask = (uint16_t)(UINT16_T__G__LogPendingMask | uint16_t__mask);
    if (uint8_t__note != SIMPLE_LOG_NOTE_NONE)
    {
        UINT8_T__G__LogPendingNote = uint8_t__note;
    }

    uint32_t__nowMs = (uint32_t)millis();
    if (((uint16_t__mask & SIMPLE_LOG_CHG_MAINS) != 0u) &&
        (BOOL__G__LogMainsQueuedValid == false))
    {
        BOOL__G__LogMainsQueuedValid = true;
        UINT32_T__G__LogMainsQueuedMs = uint32_t__nowMs;
    }
    if ((BOOL__G__LogLastRowValid == false) ||
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >= SIMPLE_LOG_MIN_INTERVAL_MS) ||
        ((BOOL__G__LogMainsQueuedValid != false) &&
         ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogMainsQueuedMs) >= SIMPLE_LOG_MAINS_FLUSH_MS)))
    {
        (void)func__SimpleLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

static void func__SimpleLog_FlushPending(void)
{
    uint32_t uint32_t__nowMs;

    if (UINT16_T__G__LogPendingMask == 0u)
    {
        return;
    }
    uint32_t__nowMs = (uint32_t)millis();
    if ((BOOL__G__LogLastRowValid == false) ||
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >= SIMPLE_LOG_MIN_INTERVAL_MS) ||
        ((BOOL__G__LogMainsQueuedValid != false) &&
         ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogMainsQueuedMs) >= SIMPLE_LOG_MAINS_FLUSH_MS)))
    {
        (void)func__SimpleLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

static void func__SimpleLog_DetectChanges(void)
{
    uint8_t uint8_t__mains;
    uint8_t uint8_t__chg1;
    uint8_t uint8_t__chg2;
    uint8_t uint8_t__flags2;
    uint8_t uint8_t__band;
    uint8_t uint8_t__fl;
    uint32_t uint32_t__dead;
    uint32_t uint32_t__fault;
    uint16_t uint16_t__mask = 0u;

    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;
    uint8_t__chg1 = (uint8_t)UINT32_T__G__TlmField[6];
    uint8_t__chg2 = (uint8_t)UINT32_T__G__TlmField[13];
    uint8_t__flags2 = UINT8_T__G__TlmFlags2;
    uint32_t__fault = UINT32_T__G__TlmField[19];
    /* مرز قطع هیسترزیس دارد: زیر ۲۱٬۲۰۰ وارد و فقط بالای ۲۱٬۴۰۰ خارج
       می‌شود؛ پس باتریِ خوابیده روی مرز - یا باری که روشن و خاموش می‌شود -
       نمی‌تواند هر دو ثانیه یک ردیف بنویسد و لاگ را پر کند. */
    if (UINT8_T__G__LogPrevBand != 0u)
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] > SIMPLE_LOG_BAND_OFF_MV) ? 0u : 1u;
    }
    else
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] < SIMPLE_LOG_CUT_BAND_MV) ? 1u : 0u;
    }
    /* [EN] every state bit of the general byte except the input-present
            bit, which already has its own change bit: snapshot, valid
            measurement, both charger enables and hand mode.
       [FA] همهٔ بیت‌های وضعیتِ بایت عمومی جز بیت برق ورودی، که بیت تغییر
            خودش را دارد: snapshot، معتبر بودن اندازه‌گیری، اجازهٔ هر دو
            شارژر و مود دستی. */
    uint8_t__fl = (uint8_t)(UINT8_T__G__TlmFlags & 0x3Du);
    uint32_t__dead = UINT32_T__G__TlmField[28];

    if (BOOL__G__LogHasPrev == false)
    {
        BOOL__G__LogHasPrev = true;
        UINT8_T__G__LogPrevMains = uint8_t__mains;
        UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
        UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
        UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
        UINT32_T__G__LogPrevFault = uint32_t__fault;
        UINT8_T__G__LogPrevBand = uint8_t__band;
        UINT8_T__G__LogPrevFl = uint8_t__fl;
        UINT32_T__G__LogPrevDead = uint32_t__dead;
        UINT32_T__G__LogPrevV24 = UINT32_T__G__TlmField[15];
        return;
    }

    if (uint8_t__mains != UINT8_T__G__LogPrevMains)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_MAINS);
    }
    if (uint8_t__chg1 != UINT8_T__G__LogPrevChg1)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_CHG1);
    }
    if (uint8_t__chg2 != UINT8_T__G__LogPrevChg2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_CHG2);
    }
    if (uint32_t__fault != UINT32_T__G__LogPrevFault)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_FAULT);
    }
    if (uint8_t__flags2 != UINT8_T__G__LogPrevFlags2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_FLAGS);
    }
    if (uint8_t__band != UINT8_T__G__LogPrevBand)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_BAND);
    }
    if (uint8_t__fl != UINT8_T__G__LogPrevFl)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_MODE);
    }
    if (uint32_t__dead != UINT32_T__G__LogPrevDead)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_DEAD);
    }
    /* [EN] a step of the pack voltage, either way. A row written anyway
            carries the step for free; only a step-sole row is limited to one
            a minute.
       [FA] یک پلهٔ ولتاژ پک، بالا یا پایین. ردیفی که به هر حال نوشته می‌شود
            گام را مجانی با خود می‌برد؛ فقط ردیفی که تنها دلیلش گام است به
            یکی در دقیقه محدود می‌شود. */
    if (func__SimpleLog_AbsDiff(UINT32_T__G__TlmField[15],
                                UINT32_T__G__LogPrevV24) >= SIMPLE_LOG_VSTEP_MV)
    {
        /* [EN] v1.109 user order: while the mains are OFF (discharge) every
                RISE of the pack is recorded as its own row - the one-a-minute
                throttle stays only for falls and for charging.
           [FA] دستور کاربر نسخهٔ ۱٫۱۰۹: وقتی برق شهر قطع است (دشارژ) هر
                «بالا آمدن» پک در ردیف خودش ثبت می‌شود؛ محدودیتِ یکی در دقیقه
                فقط برای پایین‌رفتن و برای شارژ می‌ماند. */
        bool bool__riseInDischarge = ((uint8_t__mains == 0u) &&
                                      (UINT32_T__G__TlmField[15] > UINT32_T__G__LogPrevV24));
        if ((uint16_t__mask != 0u) ||
            (bool__riseInDischarge != false) ||
            (BOOL__G__LogLastStepValid == false) ||
            ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastStepMs) >= SIMPLE_LOG_VSTEP_MIN_MS))
        {
            uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_VSTEP);
        }
    }

    if (uint16_t__mask != 0u)
    {
        func__SimpleLog_Queue(uint16_t__mask, SIMPLE_LOG_NOTE_NONE);
    }

    if ((uint16_t__mask & (SIMPLE_LOG_CHG_FAULT | SIMPLE_LOG_CHG_FLAGS)) != 0u)
    {
        func__SimpleLog_SaveLast();
    }
    if ((uint8_t__band != 0u) && (UINT8_T__G__LogPrevBand == 0u))
    {
        func__SimpleLog_SaveLast();
    }

    UINT8_T__G__LogPrevMains = uint8_t__mains;
    UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
    UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
    UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
    UINT32_T__G__LogPrevFault = uint32_t__fault;
    UINT8_T__G__LogPrevBand = uint8_t__band;
    UINT8_T__G__LogPrevFl = uint8_t__fl;
    UINT32_T__G__LogPrevDead = uint32_t__dead;
    /* ولتاژ پک اینجا ذخیره نمی‌شود: با مقدار آخرین ردیف مقایسه
       می‌شود تا شارژ آرام هم هر گام یک ردیف بدهد. */
}

/* ==================== قلاب‌ها ==================== */

static void func__SimpleLog_OnFrame(void)
{
    /* قطع/وصل فقط با خودِ قطع/وصل جفت می‌شود: هر «اتصال برقرار شد» پاسخِ
       یک «اتصال قطع شد» است که قبلاً نوشته شده. */
    if (BOOL__G__LogLinkDown != false)
    {
        BOOL__G__LogLinkDown = false;
        func__SimpleLog_Queue(SIMPLE_LOG_CHG_LINK, SIMPLE_LOG_NOTE_LINK_BACK);
    }

    /* [EN] The boot row no longer waits for a board frame (user order
          2026-10-24: "if the board was not connected it logged nothing -
          it must log"). It is written by Tick() at start-up; here only the
          state-change detection runs.
       [FA] ردیف بوت دیگر منتظر فریم برد نمی‌ماند (دستور ۲۰۲۶-۱۰-۲۴: «اگر برد
          وصل نبود لاگ نمی‌گرفت - باید بگیرد»). ردیف را Tick() در شروع
          می‌نویسد؛ اینجا فقط تشخیص تغییر وضعیت اجرا می‌شود. */
    func__SimpleLog_DetectChanges();
}

static void func__SimpleLog_Tick(void)
{
    uint32_t uint32_t__nowMs;

    /* [EN] Boot row FIRST and with no board at all: the panel log is the
          witness of the panel itself, so a session with a silent board still
          starts its own record (user order 2026-10-24).
       [FA] اول ردیف بوت، آن هم بدون هیچ بردی: لاگِ پنل شاهدِ خودِ پنل است،
          پس نشستِ بدون برد هم تاریخچهٔ خودش را شروع می‌کند (۲۰۲۶-۱۰-۲۴). */
    if (BOOL__G__LogBootRowPending != false)
    {
        uint16_t uint16_t__mask = SIMPLE_LOG_CHG_BOOT;

        BOOL__G__LogBootRowPending = false;
        UINT32_T__G__LogSessionStartMs = (uint32_t)millis();
        UINT8_T__G__LogPrevNote = func__SimpleLog_LoadLastDanger();
        /* [EN] v1.113 (user order 2026-10-28: "every time I do an update one
                record must be written, with no conditions at all, and only the
                log timers zeroed"). The update joins the boot row, so one
                switch-on gives one row. The timers of the log are zeroed just
                before it: the merge window, the step throttle and the "last
                row" clock all start again from this row, so nothing that
                happened before the update can swallow it or push it into a
                merged line.
           [FA] نسخهٔ ۱٫۱۱۳ (دستور کاربر ۲۰۲۶-۱۰-۲۸: «هر وقت بروزرسانی را زدم یک
                رکورد ضبط بشه، بدون در نظر گرفتن شرط‌ها، و فقط تایمرهای مربوط
                به لاگ صفر بشه»). بروزرسانی با ردیف بوت یکی می‌شود، پس یک
                روشن‌شدن یک ردیف می‌دهد. تایمرهای لاگ درست پیش از آن صفر
                می‌شوند: پنجرهٔ ادغام، محدودیت گام ولتاژ و ساعتِ «آخرین ردیف»
                از همین ردیف دوباره شروع می‌کنند تا هیچ چیزِ پیش از بروزرسانی
                نه آن را ببلعد و نه با ردیف دیگری ادغامش کند. */
        if (BOOL__G__LogUpdatePending != false)
        {
            BOOL__G__LogUpdatePending = false;
            uint16_t__mask = (uint16_t)(uint16_t__mask | SIMPLE_LOG_CHG_UPDATE);
            UINT32_T__G__LogLastRowMs = 0u;
            BOOL__G__LogLastRowValid = false;
            UINT32_T__G__LogLastStepMs = 0u;
            BOOL__G__LogLastStepValid = false;
            UINT16_T__G__LogPendingMask = 0u;
            UINT8_T__G__LogPendingNote = SIMPLE_LOG_NOTE_NONE;
            BOOL__G__LogMainsQueuedValid = false;
            BOOL__A__LogForceRow = true;
        }
        (void)func__SimpleLog_WriteRow(uint16_t__mask,
                                       (UINT8_T__G__LogPrevNote != 0u) ?
                                       SIMPLE_LOG_NOTE_LOW_STOP : SIMPLE_LOG_NOTE_NONE);
    }

    func__SimpleLog_FlushPending();

    uint32_t__nowMs = (uint32_t)millis();
    /* [EN] Link-silence row: after LINK_TIMEOUT_MS with no frame. If the
          board has never spoken, the clock runs from the session start, so
          "no board" is written too - not only "board left".
       [FA] ردیف سکوت ارتباط: پس از چند میلی‌ثانیه بدون فریم. اگر برد هرگز
          حرفی نزده باشد، ساعت از شروع نشست می‌شمارد تا «برد نیست» هم ثبت
          شود، نه فقط «برد رفت». */
    /* [EN] rows waiting in RAM go back to their day files as soon as the flash
            takes writes again, without waiting for the next event.
       [FA] ردیف‌های منتظر در رم، به‌محض اینکه فلش دوباره بنویسد، بی‌نیاز از
            رویداد بعدی به فایل روزهای خودشان برمی‌گردند. */
    if ((BOOL__G__FsOk != false) && (UINT8_T__G__LogRamCount != 0u))
    {
        uint32_t uint32_t__sinceDrainMs = uint32_t__nowMs - UINT32_T__G__LogLastDrainMs;
        if ((BOOL__G__LogDrainWait == false) ||
            (uint32_t__sinceDrainMs >= SIMPLE_LOG_DRAIN_RETRY_MS))
        {
            func__SimpleLog_DrainRam();
        }
    }

    if (BOOL__G__LogLinkDown == false)
    {
        bool bool__silent;

        if (BOOL__G__TlmSeen != false)
        {
            bool__silent = ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastTlmMs) >
                            SIMPLE_LOG_LINK_TIMEOUT_MS);
        }
        else
        {
            bool__silent = (BOOL__G__LogLastRowValid != false) &&
                           ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogSessionStartMs) >
                            SIMPLE_LOG_LINK_TIMEOUT_MS);
        }
        if (bool__silent != false)
        {
            BOOL__G__LogLinkDown = true;
            func__SimpleLog_Queue(SIMPLE_LOG_CHG_LINK, SIMPLE_LOG_NOTE_LINK_LOST);
        }
    }

    /* اگر در یک بازهٔ کامل هیچ اتفاقی نیفتاده باشد، یک ردیف «ضربان» نوشته
       می‌شود تا معلوم باشد پنل زنده ماند، نه اینکه چیزی ثبت نکرده. */
    if (BOOL__G__LogLastRowValid != false)
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >=
            SIMPLE_LOG_HEARTBEAT_MS)
        {
            func__SimpleLog_Queue(0u, SIMPLE_LOG_NOTE_HEARTBEAT);
        }
    }
}

static void func__SimpleLog_Begin(void)
{
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    /* [EN] v1.113: read the folder, not a remembered index.
       [FA] نسخهٔ ۱٫۱۱۳: پوشه خوانده می‌شود، نه یک فهرست به‌یادسپرده. */
    (void)func__SimpleLog_ScanDays(true);
    func__SimpleLog_FwCheck();
}

/* ==================== ثبت دستی با دکمهٔ به‌روزرسانی ==================== */

/**
 * @brief  [EN] Record one state row unconditionally when the user presses
 *              Refresh, and zero only the log timers so every log window
 *              restarts from this row (user order 2026-10-28).
 *         [FA] ثبت فوری یک رکورد از وضعیت فعلی بدون هیچ شرطی هنگام زدن دکمهٔ
 *              به‌روزرسانی، و صفر کردن فقط تایمرهای مربوط به لاگ تا همهٔ
 *              بازه‌های زمانی لاگ از همین ردیف دوباره شروع شوند.
 * @return [EN] bool stored / [FA]‎ ذخیره شد
 */
static bool func__SimpleLog_SnapshotNow(void)
{
    uint32_t uint32_t__nowMs;
    uint16_t uint16_t__mask;
    bool bool__stored;

    uint32_t__nowMs = (uint32_t)millis();
    uint16_t__mask = UINT16_T__G__LogPendingMask;

    UINT32_T__G__LogLastRowMs = 0u;
    BOOL__G__LogLastRowValid = false;
    UINT32_T__G__LogLastStepMs = 0u;
    BOOL__G__LogLastStepValid = false;
    UINT32_T__G__LogMainsQueuedMs = 0u;
    BOOL__G__LogMainsQueuedValid = false;
    UINT32_T__G__LogLastScanMs = 0u;
    BOOL__G__LogScanValid = false;
    if (BOOL__G__TlmSeen == false)
    {
        UINT32_T__G__LogSessionStartMs = uint32_t__nowMs;
    }
    UINT16_T__G__LogPendingMask = 0u;
    UINT8_T__G__LogPendingNote = SIMPLE_LOG_NOTE_NONE;
    BOOL__A__LogForceRow = true;

    bool__stored = func__SimpleLog_WriteRow(uint16_t__mask, SIMPLE_LOG_NOTE_MANUAL);
    BOOL__A__LogForceRow = false;
    UINT32_T__G__LogLastScanMs = 0u;
    BOOL__G__LogScanValid = false;
    return bool__stored;
}

/* [EN] Where does the window of the newest rows start? It walks BACKWARD from
        the end of the file, so the buffer limit is spent on the NEWEST rows:
        a busy day shows its last rows, never its first ones.
   [FA] پنجرهٔ جدیدترین ردیف‌ها از کجا شروع می‌شود؟ از انتهای فایل عقب‌عقب
        می‌رود، پس سقف بافر خرجِ جدیدترین ردیف‌ها می‌شود: روز شلوغ ردیف‌های
        آخرش را نشان می‌دهد، نه ردیف‌های اولش. */
static uint32_t func__SimpleLog_TailStart(File &file__day,
                                          uint32_t uint32_t__size,
                                          uint16_t uint16_t__maxRows,
                                          size_t size_t__budget)
{
    uint32_t uint32_t__offset = uint32_t__size;
    uint32_t uint32_t__lines = 0u;
    uint32_t uint32_t__bytes = 0u;
    bool bool__cutMidRow = false;

    while (uint32_t__offset > 0u)
    {
        int int__ch;

        uint32_t__offset--;
        if (file__day.seek(uint32_t__offset) == false)
        {
            return 0u;
        }
        int__ch = file__day.read();
        if (int__ch < 0)
        {
            return 0u;
        }
        uint32_t__bytes++;
        /* [EN] the last character of a normal file closes the newest row; it
                is not a row of its own.
           [FA] آخرین کاراکتر فایل عادی، جدیدترین ردیف را می‌بندد؛ خودش ردیف
                نیست. */
        if ((int__ch == '\n') && (uint32_t__bytes > 1u))
        {
            uint32_t__lines++;
            if (uint32_t__lines >= (uint32_t)uint16_t__maxRows)
            {
                uint32_t__offset++;
                return uint32_t__offset;
            }
        }
        if (uint32_t__bytes >= (uint32_t)size_t__budget)
        {
            bool__cutMidRow = true;
            break;
        }
    }

    if ((bool__cutMidRow != false) && (uint32_t__offset > 0u))
    {
        /* [EN] the budget ran out inside a row: drop that half row.
           [FA] بودجه وسط یک ردیف تمام شد: همان نیم‌ردیف دور ریخته می‌شود. */
        while (uint32_t__offset < uint32_t__size)
        {
            int int__ch;

            if (file__day.seek(uint32_t__offset) == false)
            {
                return 0u;
            }
            int__ch = file__day.read();
            uint32_t__offset++;
            if (int__ch == '\n')
            {
                break;
            }
        }
    }
    return uint32_t__offset;
}

/* [EN] The newest rows that never reached the flash, straight from the RAM ring.
   [FA] جدیدترین ردیف‌هایی که به فلش نرسیدند، مستقیم از حلقهٔ رم. */
static size_t func__SimpleLog_TailRam(char *char__ptr_out, size_t size_t__outSize,
                                      uint16_t uint16_t__maxLines)
{
    size_t size_t__used = 0u;
    uint8_t uint8_t__start;
    uint8_t uint8_t__count = UINT8_T__G__LogRamCount;
    uint8_t uint8_t__item;

    if (uint8_t__count == 0u)
    {
        return 0u;
    }
    uint8_t__start = (uint8_t)((UINT8_T__G__LogRamHead + SIMPLE_LOG_RAM_ROWS - uint8_t__count) % SIMPLE_LOG_RAM_ROWS);
    if ((uint16_t)uint8_t__count > uint16_t__maxLines)
    {
        uint8_t uint8_t__drop = (uint8_t)((uint16_t)uint8_t__count - uint16_t__maxLines);
        uint8_t__start = (uint8_t)((uint8_t__start + uint8_t__drop) % SIMPLE_LOG_RAM_ROWS);
        uint8_t__count = (uint8_t)uint16_t__maxLines;
    }
    for (uint8_t__item = 0u; uint8_t__item < uint8_t__count; uint8_t__item++)
    {
        uint8_t uint8_t__slot = (uint8_t)((uint8_t__start + uint8_t__item) % SIMPLE_LOG_RAM_ROWS);
        size_t size_t__len = strlen(CHAR__G__LogRamRow[uint8_t__slot]);
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__ptr_out[size_t__used], CHAR__G__LogRamRow[uint8_t__slot], size_t__len);
        size_t__used += size_t__len;
        char__ptr_out[size_t__used] = '\n';
        size_t__used++;
        char__ptr_out[size_t__used] = '\0';
    }
    return size_t__used;
}

/* [EN] With no day file at all the RAM ring is the only history - but only when
        a write really failed; an empty day must stay empty.
   [FA] وقتی فایل روزی نیست حلقهٔ رم تنها تاریخچه است - اما فقط وقتی نوشتن
        واقعاً شکست خورده باشد؛ روزِ بی‌ردیف باید خالی بماند. */
static size_t func__SimpleLog_TailRamOnly(char *char__ptr_out, size_t size_t__outSize,
                                          uint16_t uint16_t__maxLines, bool bool__isToday)
{
    /* [EN] The RAM ring only ever holds today's rows, so an older day must not
            borrow them; and without a write fault the emptiness is real.
       [FA] حلقهٔ رم فقط ردیف‌های امروز را دارد، پس روز قدیمی‌تر نباید آن‌ها را
            قرض بگیرد؛ و بدون خرابیِ نوشتن، خالی‌بودن واقعی است. */
    if ((UINT8_T__G__LogWriteFault == 0u) || (bool__isToday == false))
    {
        return 0u;
    }
    return func__SimpleLog_TailRam(char__ptr_out, size_t__outSize, uint16_t__maxLines);
}

static size_t func__SimpleLog_Tail(char *char__ptr_out, size_t size_t__outSize,
                                   uint32_t uint32_t__dayCode, uint16_t uint16_t__maxLines)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    size_t size_t__used = 0u;
    uint32_t uint32_t__chosen = uint32_t__dayCode;
    bool bool__isToday;

    if ((char__ptr_out == NULL) || (size_t__outSize < 2u))
    {
        return 0u;
    }
    char__ptr_out[0] = '\0';

    /* [EN] v1.112 (user order 2026-10-28: mirror the log fixes): one refused
            write used to hide the whole day file behind the RAM ring, so a card
            with hundreds of rows showed a handful. The file is read first and
            the rows that never reached the flash follow it.
       [FA] نسخهٔ ۱٫۱۱۲ (دستور کاربر ۲۰۲۶-۱۰-۲۸: بازتاب اصلاح‌های لاگ): یک
            نوشتنِ ردشده کل فایل روز را پشت حلقهٔ رم پنهان می‌کرد، پس کارتی با
            صدها ردیف چند ردیف نشان می‌داد. اول فایل خوانده می‌شود و
            ردیف‌هایی که به فلش نرسیدند پشت آن می‌آیند. */
    bool__isToday = ((uint32_t__dayCode == SIMPLE_LOG_DAY_AUTO) ||
                     (uint32_t__dayCode == 0u) ||
                     (uint32_t__dayCode == func__SimpleLog_TodayCode()));
    /* [EN] No file system at all: the RAM ring is the whole history, so today's
            rows are served as they are.
       [FA] فایل‌سیستم نیست: حلقهٔ رم تمام تاریخچه است، پس ردیف‌های امروز
            همان‌طور که هستند خوانده می‌شوند. */
    if (BOOL__G__FsOk == false)
    {
        return func__SimpleLog_TailRam(char__ptr_out, size_t__outSize,
                                       uint16_t__maxLines);
    }

    if ((uint32_t__chosen == SIMPLE_LOG_DAY_AUTO) || (uint32_t__chosen == 0u))
    {
        uint32_t__chosen = func__SimpleLog_TodayCode();
        func__SimpleLog_PathOf(char__ptr_path, uint32_t__chosen);
        if (LittleFS.exists(char__ptr_path) == false)
        {
            if (UINT8_T__G__LogIndexCount == 0u)
            {
                return func__SimpleLog_TailRamOnly(char__ptr_out, size_t__outSize,
                                                   uint16_t__maxLines, bool__isToday);
            }
            uint32_t__chosen = UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
        }
    }
    else if (uint32_t__chosen == SIMPLE_LOG_DAY_ZERO)
    {
        uint32_t__chosen = 0u;
    }

    func__SimpleLog_PathOf(char__ptr_path, uint32_t__chosen);
    if (LittleFS.exists(char__ptr_path) == false)
    {
        return func__SimpleLog_TailRamOnly(char__ptr_out, size_t__outSize,
                                          uint16_t__maxLines, bool__isToday);
    }

    File file__day = LittleFS.open(char__ptr_path, "r");
    if (!file__day)
    {
        return func__SimpleLog_TailRamOnly(char__ptr_out, size_t__outSize,
                                          uint16_t__maxLines, bool__isToday);
    }
    {
        uint32_t uint32_t__size = (uint32_t)file__day.size();
        uint32_t uint32_t__start;

        if (uint32_t__size == 0u)
        {
            file__day.close();
            return func__SimpleLog_TailRamOnly(char__ptr_out, size_t__outSize,
                                              uint16_t__maxLines, bool__isToday);
        }
        uint32_t__start = func__SimpleLog_TailStart(file__day, uint32_t__size,
                                                    uint16_t__maxLines,
                                                    size_t__outSize - 2u);
        if (file__day.seek(uint32_t__start) == false)
        {
            file__day.close();
            return func__SimpleLog_TailRamOnly(char__ptr_out, size_t__outSize,
                                              uint16_t__maxLines, bool__isToday);
        }
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        size_t size_t__len;

        string__line.trim();
        if ((string__line.length() == 0u) || (string__line[0] == '#'))
        {
            continue;
        }
        size_t__len = (size_t)string__line.length();
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__ptr_out[size_t__used], string__line.c_str(), size_t__len);
        size_t__used += size_t__len;
        char__ptr_out[size_t__used] = '\n';
        size_t__used++;
        char__ptr_out[size_t__used] = '\0';
    }
    file__day.close();

    /* [EN] the rows that failed to reach the flash are newer than everything in
            the file, so they are appended, not substituted.
       [FA] ردیف‌هایی که به فلش نرسیدند از هر چه در فایل است جدیدترند، پس
            پشت آن‌ها می‌آیند، نه به‌جای آن‌ها. */
    if ((UINT8_T__G__LogWriteFault != 0u) && (UINT8_T__G__LogRamCount != 0u) &&
        ((size_t__used + 2u) < size_t__outSize))
    {
        size_t__used += func__SimpleLog_TailRam(&char__ptr_out[size_t__used],
                                                size_t__outSize - size_t__used,
                                                uint16_t__maxLines);
    }
    return size_t__used;
}

static uint8_t func__SimpleLog_Clear(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    uint8_t uint8_t__removed = 0u;

    UINT8_T__G__LogRamHead = 0u;
    UINT8_T__G__LogRamCount = 0u;
    if (BOOL__G__FsOk == false)
    {
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        return 0u;
    }
    /* [EN] v1.113 (user report 2026-10-28: "a pile of «panel switched on» rows
            that the panel cannot delete"): the folder is scanned right here, so
            EVERY day file goes - also the ones the old "/evt-days.txt" index
            had forgotten. Those were the rows that survived the button, stayed
            visible through the automatic "today" read and could never be
            removed again.
       [FA] نسخهٔ ۱٫۱۱۳ (گزارش کاربر ۲۰۲۶-۱۰-۲۸: «یک سری «پنل روشن شد» در حافظه
            هست که پاک نمی‌شود»): همین‌جا پوشه خوانده می‌شود، پس «هر» فایل روز
            می‌رود - از جمله آن‌هایی که فهرست قدیمی «‎/evt-days.txt‎» فراموش
            کرده بود. همان‌ها ردیف‌هایی بودند که بعد از زدن دکمه می‌ماندند، از
            راه خواندن خودکار «امروز» دیده می‌شدند و دیگر هرگز پاک نمی‌شدند. */
    (void)func__SimpleLog_ScanDays(true);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        func__SimpleLog_PathOf(char__ptr_path, UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (LittleFS.remove(char__ptr_path))
        {
            uint8_t__removed++;
        }
    }
    (void)LittleFS.remove(SIMPLE_LOG_INDEX_PATH);
    (void)LittleFS.remove(SIMPLE_LOG_LAST_PATH);
    UINT8_T__G__LogIndexCount = 0u;
    BOOL__G__LogScanValid = false;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    BOOL__G__LogMarkerDone = false;
    BOOL__G__LogDrainWait = false;
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    return uint8_t__removed;
}

static size_t func__SimpleLog_Days(char *char__ptr_out, size_t size_t__outSize)
{
    size_t size_t__used = 0u;
    uint8_t uint8_t__slot;

    if ((char__ptr_out == NULL) || (size_t__outSize < 4u))
    {
        return 0u;
    }
    char__ptr_out[0] = '\0';
    if (BOOL__G__FsOk == false)
    {
        /* [EN] v1.112: today is the only day that can exist without a file
                system. The list stayed empty until the first row arrived, so
                the day picker showed nothing at all.
           [FA] نسخهٔ ۱٫۱۱۲: بی فایل‌سیستم تنها روزی که می‌تواند باشد امروز است.
                فهرست تا رسیدن اولین ردیف خالی می‌ماند و انتخاب‌گر روز هیچ چیز
                نشان نمی‌داد. */
        if (size_t__outSize >= 12u)
        {
            int int__ramWritten = snprintf(char__ptr_out, size_t__outSize, "%08lu",
                                           (unsigned long)func__SimpleLog_TodayCode());
            return (int__ramWritten > 0) ? (size_t)int__ramWritten : 0u;
        }
        return 0u;
    }
    (void)func__SimpleLog_ScanDays(false);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        int int__written = snprintf(&char__ptr_out[size_t__used],
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

/* ==================== بررسی و پاک‌سازی حافظهٔ فلش ==================== */

static bool func__SimpleLog_IsOwnFile(const char *char__ptr_name)
{
    static const char * const CHAR_PTR__OWN_FILES[] =
    {
        &SIMPLE_WIFI_FILE[1],
        &SIMPLE_CHARTS_FILE[1],
        &SIMPLE_USERS_FILE[1],
        &SIMPLE_TIME_FILE[1],
        &SIMPLE_LOG_LAST_PATH[1],
        &SIMPLE_PARAMS_FILE[1],
        &SIMPLE_THEME_FILE[1],
        &SIMPLE_RECOVERY_FILE[1],
        &SIMPLE_CHARGE_HISTORY_FILE[1],
        &SIMPLE_DIAG_FILE[1]
    };
    const char *char__ptr_base;
    uint32_t uint32_t__day = 0u;
    size_t size_t__idx;

    if ((char__ptr_name == NULL) || (char__ptr_name[0] == '\0'))
    {
        return false;
    }
    if (func__SimpleLog_DayFromName(char__ptr_name, &uint32_t__day) != false)
    {
        return true;
    }
    char__ptr_base = (char__ptr_name[0] == '/') ? &char__ptr_name[1] : char__ptr_name;
    for (size_t__idx = 0u;
         size_t__idx < (sizeof(CHAR_PTR__OWN_FILES) / sizeof(CHAR_PTR__OWN_FILES[0]));
         size_t__idx++)
    {
        if (strcmp(char__ptr_base, CHAR_PTR__OWN_FILES[size_t__idx]) == 0)
        {
            return true;
        }
    }
    return false;
}

static void func__SimpleLog_SanitizeFileName(const char *char__ptr_src,
                                             char *char__ptr_out,
                                             size_t size_t__outSize)
{
    const char *char__ptr_base;
    size_t size_t__idx = 0u;

    if ((char__ptr_out == NULL) || (size_t__outSize == 0u))
    {
        return;
    }
    if (char__ptr_src == NULL)
    {
        char__ptr_out[0] = '\0';
        return;
    }
    char__ptr_base = (char__ptr_src[0] == '/') ? &char__ptr_src[1] : char__ptr_src;
    while ((char__ptr_base[size_t__idx] != '\0') &&
           ((size_t__idx + 1u) < size_t__outSize))
    {
        char char__ch = char__ptr_base[size_t__idx];
        if ((char__ch < 0x20) || (char__ch > 0x7E) ||
            (char__ch == '"') || (char__ch == '\\'))
        {
            char__ch = '?';
        }
        char__ptr_out[size_t__idx] = char__ch;
        size_t__idx++;
    }
    char__ptr_out[size_t__idx] = '\0';
}

static size_t func__SimpleLog_FsStatusJson(char *char__ptr_out, size_t size_t__outSize)
{
    uint32_t uint32_t__totalBytes = 0u;
    uint32_t uint32_t__usedBytes = 0u;
    uint32_t uint32_t__sumBytes = 0u;
    uint16_t uint16_t__shown = 0u;
    uint16_t uint16_t__more = 0u;
    size_t size_t__used = 0u;
    int int__written;

    if ((char__ptr_out == NULL) || (size_t__outSize < 64u))
    {
        return 0u;
    }
    if (BOOL__G__FsOk != false)
    {
#if defined(ESP32)
        uint32_t__totalBytes = (uint32_t)LittleFS.totalBytes();
        uint32_t__usedBytes = (uint32_t)LittleFS.usedBytes();
#else
        FSInfo fsInfo__stat;
        if (LittleFS.info(fsInfo__stat))
        {
            uint32_t__totalBytes = (uint32_t)fsInfo__stat.totalBytes;
            uint32_t__usedBytes = (uint32_t)fsInfo__stat.usedBytes;
        }
#endif
    }

    int__written = snprintf(char__ptr_out, size_t__outSize,
                            "{\"fs\":%u,\"mountFail\":%u,\"total\":%lu,\"used\":%lu,\"files\":[",
                            (BOOL__G__FsOk != false) ? 1u : 0u,
                            (BOOL__G__FsMountFailed != false) ? 1u : 0u,
                            (unsigned long)uint32_t__totalBytes,
                            (unsigned long)uint32_t__usedBytes);
    if ((int__written <= 0) || ((size_t)int__written >= size_t__outSize))
    {
        char__ptr_out[0] = '\0';
        return 0u;
    }
    size_t__used = (size_t)int__written;

    if (BOOL__G__FsOk != false)
    {
#if defined(ESP32)
        File file__root = LittleFS.open("/", "r");
        if (file__root)
        {
            File file__entry = file__root.openNextFile();
            while (file__entry)
            {
                char char__safeName[40];
                const char *char__ptr_raw = file__entry.name();
                uint32_t uint32_t__fsize = (uint32_t)file__entry.size();
                bool bool__own = func__SimpleLog_IsOwnFile(char__ptr_raw);

                uint32_t__sumBytes += uint32_t__fsize;
                func__SimpleLog_SanitizeFileName(char__ptr_raw, char__safeName, sizeof(char__safeName));
                if ((uint16_t__shown < 36u) && ((size_t__used + 96u) < size_t__outSize))
                {
                    int int__item = snprintf(&char__ptr_out[size_t__used],
                                             size_t__outSize - size_t__used,
                                             "%s{\"name\":\"%s\",\"size\":%lu,\"own\":%u}",
                                             (uint16_t__shown == 0u) ? "" : ",",
                                             char__safeName,
                                             (unsigned long)uint32_t__fsize,
                                             (bool__own != false) ? 1u : 0u);
                    if ((int__item > 0) && ((size_t__used + (size_t)int__item) < size_t__outSize))
                    {
                        size_t__used += (size_t)int__item;
                        uint16_t__shown++;
                    }
                }
                else
                {
                    uint16_t__more++;
                }
                file__entry.close();
                file__entry = file__root.openNextFile();
            }
            file__root.close();
        }
#else
        Dir dir__root = LittleFS.openDir("/");
        while (dir__root.next())
        {
            char char__safeName[40];
            String string__raw = dir__root.fileName();
            uint32_t uint32_t__fsize = (uint32_t)dir__root.fileSize();
            bool bool__own = func__SimpleLog_IsOwnFile(string__raw.c_str());

            uint32_t__sumBytes += uint32_t__fsize;
            func__SimpleLog_SanitizeFileName(string__raw.c_str(), char__safeName, sizeof(char__safeName));
            if ((uint16_t__shown < 36u) && ((size_t__used + 96u) < size_t__outSize))
            {
                int int__item = snprintf(&char__ptr_out[size_t__used],
                                         size_t__outSize - size_t__used,
                                         "%s{\"name\":\"%s\",\"size\":%lu,\"own\":%u}",
                                         (uint16_t__shown == 0u) ? "" : ",",
                                         char__safeName,
                                         (unsigned long)uint32_t__fsize,
                                         (bool__own != false) ? 1u : 0u);
                if ((int__item > 0) && ((size_t__used + (size_t)int__item) < size_t__outSize))
                {
                    size_t__used += (size_t)int__item;
                    uint16_t__shown++;
                }
            }
            else
            {
                uint16_t__more++;
            }
        }
#endif
        if ((uint32_t__usedBytes == 0u) && (uint32_t__sumBytes > 0u))
        {
            uint32_t__usedBytes = uint32_t__sumBytes;
        }
    }

    int__written = snprintf(&char__ptr_out[size_t__used],
                            size_t__outSize - size_t__used,
                            "],\"fileBytes\":%lu,\"more\":%u}",
                            (unsigned long)uint32_t__sumBytes,
                            (unsigned int)uint16_t__more);
    if ((int__written > 0) && ((size_t__used + (size_t)int__written) < size_t__outSize))
    {
        size_t__used += (size_t)int__written;
    }
    return size_t__used;
}

static uint16_t func__SimpleLog_CleanForeignFiles(uint32_t *uint32_t__ptr_freedBytes)
{
    uint16_t uint16_t__removed = 0u;
    uint32_t uint32_t__freed = 0u;
    uint8_t uint8_t__pass;

    if (uint32_t__ptr_freedBytes != NULL)
    {
        *uint32_t__ptr_freedBytes = 0u;
    }
    if (BOOL__G__FsOk == false)
    {
        return 0u;
    }
    for (uint8_t__pass = 0u; uint8_t__pass < 8u; uint8_t__pass++)
    {
        char char__batch[8][40];
        uint32_t uint32_t__sizes[8];
        uint8_t uint8_t__count = 0u;
        uint8_t uint8_t__idx;

#if defined(ESP32)
        File file__root = LittleFS.open("/", "r");
        if (file__root)
        {
            File file__entry = file__root.openNextFile();
            while (file__entry && (uint8_t__count < 8u))
            {
                const char *char__ptr_nm = file__entry.name();
                if ((char__ptr_nm != NULL) && (func__SimpleLog_IsOwnFile(char__ptr_nm) == false))
                {
                    const char *char__ptr_base = (char__ptr_nm[0] == '/') ? &char__ptr_nm[1] : char__ptr_nm;
                    (void)snprintf(char__batch[uint8_t__count], sizeof(char__batch[0]), "/%s", char__ptr_base);
                    uint32_t__sizes[uint8_t__count] = (uint32_t)file__entry.size();
                    uint8_t__count++;
                }
                file__entry.close();
                file__entry = file__root.openNextFile();
            }
            if (file__entry)
            {
                file__entry.close();
            }
            file__root.close();
        }
#else
        Dir dir__root = LittleFS.openDir("/");
        while (dir__root.next() && (uint8_t__count < 8u))
        {
            String string__nm = dir__root.fileName();
            if (func__SimpleLog_IsOwnFile(string__nm.c_str()) == false)
            {
                const char *char__ptr_base = (string__nm.c_str()[0] == '/')
                    ? &string__nm.c_str()[1]
                    : string__nm.c_str();
                (void)snprintf(char__batch[uint8_t__count], sizeof(char__batch[0]), "/%s", char__ptr_base);
                uint32_t__sizes[uint8_t__count] = (uint32_t)dir__root.fileSize();
                uint8_t__count++;
            }
        }
#endif
        if (uint8_t__count == 0u)
        {
            break;
        }
        {
            uint8_t uint8_t__passRemoved = 0u;
            for (uint8_t__idx = 0u; uint8_t__idx < uint8_t__count; uint8_t__idx++)
            {
                bool bool__ok = LittleFS.remove(char__batch[uint8_t__idx]);
                if (bool__ok == false)
                {
                    bool__ok = LittleFS.remove(&char__batch[uint8_t__idx][1]);
                }
                if (bool__ok != false)
                {
                    uint16_t__removed++;
                    uint8_t__passRemoved++;
                    uint32_t__freed += uint32_t__sizes[uint8_t__idx];
                }
            }
            if (uint8_t__passRemoved == 0u)
            {
                break;
            }
        }
    }
    BOOL__G__LogScanValid = false;
    if (uint32_t__ptr_freedBytes != NULL)
    {
        *uint32_t__ptr_freedBytes = uint32_t__freed;
    }
    return uint16_t__removed;
}

static void func__SimpleLog_ResetStateAfterFormat(void)
{
    UINT8_T__G__LogIndexCount = 0u;
    BOOL__G__LogScanValid = false;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    BOOL__G__LogMarkerDone = false;
    UINT8_T__G__LogRamHead = 0u;
    UINT8_T__G__LogRamCount = 0u;
    BOOL__G__LogDrainWait = false;
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
}

