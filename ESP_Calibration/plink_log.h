/* plink_log.h - live state history on the panel's own flash, one CSV per day.
   [FA] plink_log.h - تاریخچهٔ وضعیت زنده روی فلش خود پنل، روزی یک فایل CSV.
   Included by ESP_Calibration.ino (single translation unit, order matters):
   after plink_state.h and before plink_link.h (the frame parser calls the hook).
   No include guard on purpose: including twice would redefine everything.

   [EN] Rewritten 2026-10-27 after the user reported "the event log never comes
        up". Three defects were proven in the old version and are the shape of
        this file:
        1) a flash write was never checked: print()/println() return the bytes
           they really wrote, and the old code threw that away, so a full or
           broken flash lost the row in silence and the panel said "nothing has
           been recorded yet" - the exact symptom the user saw;
        2) when the write failed the row was NOT copied to the RAM ring either,
           so nothing at all was kept;
        3) the tail reader filled its buffer from the OLDEST row of the window
           and stopped, so a busy day showed the oldest rows and threw the
           newest away.
        The promises of the old file are kept one for one: boot row with no
        board at all, change-only rows, hysteresis on the cut band, one step row
        a minute, mains rows inside a short window, six-hour heartbeat,
        day rotation, day/byte retention, RAM ring when the file system is off.
   [FA] بازنویسی ۲۰۲۶-۱۰-۲۷ پس از گزارش کاربر «لاگ‌گیری بالا نمی‌آید». سه ایراد
        در نسخهٔ قبل ثابت شد و ساختار این فایل از آن‌ها آمده است:
        ۱) نوشتن روی فلش هرگز بررسی نمی‌شد: print و println تعداد بایت
           نوشته‌شده را برمی‌گردانند و کد قبلی آن را دور می‌ریخت، پس فلش پر یا
           خراب، ردیف را بی‌صدا از بین می‌برد و پنل می‌گفت «هنوز چیزی ثبت
           نشده» - همان چیزی که کاربر دید؛
        ۲) وقتی نوشتن شکست می‌خورد ردیف به حلقهٔ رم هم نمی‌رفت، پس هیچ‌جا
           نمی‌ماند؛
        ۳) خوانندهٔ آخرین ردیف‌ها بافرش را از قدیمی‌ترین ردیف پنجره پر می‌کرد
           و بعد می‌ایستاد، پس روز شلوغ قدیمی‌ها را نشان می‌داد و جدیدها را
           دور می‌ریخت.
        قول‌های فایل قبلی یکی‌به‌یکی می‌مانند: ردیف بوت حتی بدون برد، ردیف
        فقط روی تغییر، هیسترزیس مرز قطع، حداکثر یک ردیف گام در دقیقه، ردیف
        برق شهر داخل پنجرهٔ کوتاه، ضربان شش‌ساعته، عوض شدن روز، نگه‌داری
        روزانه و بایتی، و حلقهٔ رم وقتی فایل‌سیستم خاموش است. */

#include <string.h>

/* ==================== Clock / ساعت ==================== */
/* [EN] The panel has no RTC. The phone that opens the page sends its own time
        (POST /clock) and this file keeps it as an anchor: now = anchor + the
        elapsed millis. The time zone offset comes from the same phone, so the
        daily file name rolls over at the operator's midnight, not at UTC's.
   [FA] پنل ساعت ندارد. گوشی/کامپیوتری که صفحه را باز می‌کند ساعت خودش را
        می‌فرستد و این فایل آن را لنگر نگه می‌دارد:
        اکنون = لنگر + میلی‌ثانیه‌های گذشته. اختلاف منطقهٔ زمانی هم از همان
        گوشی می‌آید، تا نام فایل روزانه سرِ نیمه‌شب خودِ اپراتور عوض شود نه UTC. */
static uint32_t UINT32_T__G__LogAnchorSec = 0u;
static uint32_t UINT32_T__G__LogAnchorMs = 0u;
static int32_t  INT32_T__G__LogTzSec = 0;
static bool     BOOL__G__LogClockValid = false;
static uint32_t UINT32_T__G__LogLastClockSaveMs = 0u;

/* ==================== Open day / روز باز ==================== */
/* [EN] The sentinel says "no day opened yet". Day 0 is a real day: the day of a
        board that has never seen a phone yet. Storage must never wait for the
        clock - the phone may connect hours later.
   [FA] این مقدار یعنی «هنوز روزی باز نشده». روز صفر یک روز واقعی است: روزِ
        بردی که هنوز گوشی‌ای ندیده. ذخیره‌سازی هرگز نباید منتظر ساعت بماند -
        ممکن است گوشی ساعت‌ها بعد وصل شود. */
static uint32_t UINT32_T__G__LogDayCode = 0xFFFFFFFFu;   /* ‎YYYYMMDD‎, sentinel = none */
static uint16_t UINT16_T__G__LogRowsToday = 0u;
static bool     BOOL__G__LogDayFull = false;
static bool     BOOL__G__LogBootRowPending = true;
/* [EN] 0 = the last flash write was verified. Anything else = the number of
        failed writes since the last good one. It is what lets the panel tell
        "nothing happened" from "I could not write" instead of guessing.
   [FA] صفر یعنی آخرین نوشتن روی فلش تأیید شد. هر مقدار دیگر یعنی تعداد
        نوشتن‌های شکست‌خورده از آخرین نوشتن سالم. همین است که به پنل اجازه
        می‌دهد «چیزی نشده» را از «نتوانستم بنویسم» جدا کند. */
static uint8_t  UINT8_T__G__LogWriteFault = 0u;

/* ==================== Previous snapshot / وضعیت قبلی ==================== */
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
static uint8_t  UINT8_T__G__LogPrevNote = 0u;   /* [EN] danger before the last restart / [FA] خطر پیش از ری‌استارت */

/* ==================== Pending change / تغییر در انتظار ==================== */
static uint16_t UINT16_T__G__LogPendingMask = 0u;
static uint8_t  UINT8_T__G__LogPendingNote = 0u;
static bool     BOOL__G__LogMainsQueuedValid = false;
static uint32_t UINT32_T__G__LogMainsQueuedMs = 0u;
static uint32_t UINT32_T__G__LogLastRowMs = 0u;
static bool     BOOL__G__LogLastRowValid = false;
static bool     BOOL__G__LogLinkDown = false;
/* [EN] when the boot row of this session was written; the "board silent"
        clock runs from here even if no frame ever arrived.
   [FA] لحظهٔ نوشتن ردیف بوتِ همین نشست؛ تایمرِ «برد ساکت است» از همین لحظه
        می‌شمارد، حتی اگر هیچ فریمی نرسیده باشد. */
static uint32_t UINT32_T__G__LogSessionStartMs = 0u;
/* [EN] v1.98: when the last voltage-step row was written, so a load that
        switches on and off cannot turn the step into a strobe.
   [FA] نسخهٔ ۱٫۹۸: لحظهٔ آخرین ردیف گام ولتاژ، تا باری که روشن و خاموش
        می‌شود گام را به چشمک‌زدن تبدیل نکند. */
static uint32_t UINT32_T__G__LogLastStepMs = 0u;
static bool     BOOL__G__LogLastStepValid = false;

/* ==================== Day index / فهرست روزها ==================== */
static uint32_t UINT32_T__G__LogIndexDay[ESP_LOG_MAX_DAYS + 2u];
static uint8_t  UINT8_T__G__LogIndexCount = 0u;
/* [EN] RAM ring: the last rows survive even when the flash cannot take them -
        either because the file system is off or because a write just failed.
   [FA] حلقهٔ رم: آخرین ردیف‌ها حتی وقتی فلش نمی‌پذیرد زنده می‌مانند - یا چون
        فایل‌سیستم خاموش است یا چون نوشتن همین حالا شکست خورد. */
static char     CHAR__G__LogRamRow[ESP_LOG_RAM_ROWS][ESP_LOG_RAM_ROW_MAX];
static uint8_t  UINT8_T__G__LogRamHead = 0u;
static uint8_t  UINT8_T__G__LogRamCount = 0u;

/* ==================== Change bits / بیت‌های تغییر ==================== */
#define ESP_LOG_CHG_BOOT    0x0001u
#define ESP_LOG_CHG_MAINS   0x0002u
#define ESP_LOG_CHG_CHG1    0x0004u
#define ESP_LOG_CHG_CHG2    0x0008u
#define ESP_LOG_CHG_FAULT   0x0010u
#define ESP_LOG_CHG_FLAGS   0x0020u
#define ESP_LOG_CHG_BAND    0x0040u
#define ESP_LOG_CHG_LINK    0x0080u
#define ESP_LOG_CHG_MODE    0x0100u   /* [EN] hand mode / data validity / [FA] مود دستی و اعتبار داده */
#define ESP_LOG_CHG_DEAD    0x0200u   /* [EN] condemned-battery verdict / [FA] حکم باتری خراب */
#define ESP_LOG_CHG_VSTEP   0x0400u   /* [EN] pack voltage moved a visible step / [FA] ولتاژ پک یک پلهٔ محسوس جابه‌جا شد */

/* ==================== Notes / یادداشت ردیف ==================== */
#define ESP_LOG_NOTE_NONE      0u
#define ESP_LOG_NOTE_LOW_STOP  1u   /* [EN] board came back after a low-battery stop / [FA] برگشت پس از ایست باتری ضعیف */
#define ESP_LOG_NOTE_LINK_LOST 2u
#define ESP_LOG_NOTE_LINK_BACK 3u
#define ESP_LOG_NOTE_DAY_FULL  4u
#define ESP_LOG_NOTE_HEARTBEAT  5u   /* [EN] nothing changed, the panel is alive / [FA] چیزی عوض نشد، پنل زنده است */

#define ESP_LOG_PATH_MAX       24u
#define ESP_LOG_ROW_MAX        256u   /* [EN] one CSV row, built once / [FA] یک ردیف CSV که یک‌جا ساخته می‌شود */
/* [EN] The value the /evt answer carries in X-Log-Fs: 1 = rows come from the
        flash, 0 = the file system is off (RAM ring), 2 = the file system is
        mounted but a write failed (RAM ring holds the newest rows).
   [FA] مقداری که پاسخ /evt در ‎X-Log-Fs‎ می‌گذارد: ۱ یعنی ردیف‌ها از فلش
        می‌آیند، ۰ یعنی فایل‌سیستم خاموش است (حلقهٔ رم)، ۲ یعنی فایل‌سیستم
        سوار است ولی نوشتن شکست خورد (جدیدترین ردیف‌ها در حلقهٔ رم هستند). */
#define ESP_LOG_FS_OK          1u
#define ESP_LOG_FS_OFF         0u
#define ESP_LOG_FS_WRITE_FAULT 2u

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
 *         [FA] گرفتن ساعت گوشی و ذخیرهٔ آن.
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

/* ==================== Flash write / نوشتن روی فلش ==================== */

/**
 * @brief  [EN] Write one text piece and REPORT whether every byte landed.
 *              print() already returns the count it really wrote, so a full or
 *              damaged flash is visible here instead of silent.
 *         [FA] نوشتن یک تکه متن و گزارش این‌که همهٔ بایت‌ها نشستند یا نه.
 *              خود print تعداد بایت نوشته‌شده را برمی‌گرداند، پس فلش پر یا
 *              خراب همین‌جا دیده می‌شود نه بی‌صدا.
 * @‎param  File &file__target [EN] open file / [FA]‎ فایل باز
 * @‎param  const char *char__text [EN] text / [FA]‎ متن
 * @return [EN] bool, true when the whole text was written / [FA]‎ نوشته شد
 */
static bool func__EspLog_FileText(File &file__target, const char *char__text)
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

/**
 * @brief  [EN] A write that failed is not a lost row: the row goes to the RAM
 *              ring and the fault counter is raised, which the panel shows.
 *         [FA] نوشتنِ شکست‌خورده یعنی ردیفِ گم‌شده نیست: ردیف به حلقهٔ رم
 *              می‌رود و شمارندهٔ خطای نوشتن بالا می‌رود، همان که پنل نشان می‌دهد.
 * @‎param  const char *char__row [EN] one CSV row / [FA]‎ یک ردیف
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_RamPush(const char *char__row)
{
    if (char__row == NULL)
    {
        return;
    }
    (void)strncpy(CHAR__G__LogRamRow[UINT8_T__G__LogRamHead],
                  char__row,
                  ESP_LOG_RAM_ROW_MAX - 1u);
    CHAR__G__LogRamRow[UINT8_T__G__LogRamHead][ESP_LOG_RAM_ROW_MAX - 1u] = '\0';
    UINT8_T__G__LogRamHead = (uint8_t)((UINT8_T__G__LogRamHead + 1u) % ESP_LOG_RAM_ROWS);
    if (UINT8_T__G__LogRamCount < ESP_LOG_RAM_ROWS)
    {
        UINT8_T__G__LogRamCount++;
    }
}

/**
 * @brief  [EN] Remember that a flash write failed (saturating, so a flash that
 *              is full forever cannot wrap the counter back to "healthy").
 *         [FA] ثبت این‌که نوشتن روی فلش شکست خورد (اشباع‌شونده، تا فلشی که
 *              همیشه پر است شمارنده را به «سالم» برنگرداند).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FaultRaise(void)
{
    if (UINT8_T__G__LogWriteFault < 0xFFu)
    {
        UINT8_T__G__LogWriteFault++;
    }
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
    char char__number[16];

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    file__index = LittleFS.open(ESP_LOG_INDEX_PATH, "w");
    if (!file__index)
    {
        func__EspLog_FaultRaise();
        return;
    }
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        (void)snprintf(char__number, sizeof(char__number), "%08lu\n",
                       (unsigned long)UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (func__EspLog_FileText(file__index, char__number) == false)
        {
            func__EspLog_FaultRaise();
            break;
        }
    }
    file__index.close();
}

/**
 * @brief  [EN] Size of one day file, so the log can be kept inside the flash.
 *         [FA] حجم یک فایل روز، تا لاگ داخل فلش بماند.
 * @‎param  uint32_t uint32_t__dayCode [EN] day / [FA]‎ روز
 * @‎return [EN] bytes / [FA]‎ بایت
 */
static uint32_t func__EspLog_DayBytes(uint32_t uint32_t__dayCode)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    uint32_t uint32_t__bytes = 0u;

    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    uint32_t__bytes = (uint32_t)file__day.size();
    file__day.close();
    return uint32_t__bytes;
}

/**
 * @brief  [EN] Delete the oldest day when the retention count or the byte
 *              budget is passed.
 *         [FA] حذف قدیمی‌ترین روز وقتی تعداد روزهای نگه‌داری یا بودجهٔ بایتی
 *              رد شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_TrimDays(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    bool bool__dropped = false;

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
        bool__dropped = true;
    }

    /* [EN] v1.96: the day count alone does not bound the flash -
            a busy day is bigger than a quiet week. Keep the whole
            history inside one byte budget and drop the oldest day
            first, so a full flash can never stop the log.
       [FA] نسخهٔ ۱٫۹۶: تعداد روز تنها فلش را کران‌دار نمی‌کند -
            یک روز شلوغ از یک هفتهٔ آرام بزرگ‌تر است. کل تاریخچه
            داخل یک بودجهٔ بایتی نگه داشته می‌شود و اول قدیمی‌ترین
            روز می‌رود، تا پر شدن فلش هرگز لاگ را نخواباند. */
    while (UINT8_T__G__LogIndexCount > 1u)
    {
        uint32_t uint32_t__bytes = 0u;

        for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            uint32_t__bytes += func__EspLog_DayBytes(UINT32_T__G__LogIndexDay[uint8_t__slot]);
        }
        if (uint32_t__bytes <= ESP_LOG_TOTAL_BUDGET_BYTES)
        {
            break;
        }
        func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[0]);
        (void)LittleFS.remove(char__path);
        for (uint8_t__slot = 1u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] =
                UINT32_T__G__LogIndexDay[uint8_t__slot];
        }
        UINT8_T__G__LogIndexCount--;
        bool__dropped = true;
    }
    if (bool__dropped != false)
    {
        func__EspLog_IndexSave();
    }
}

/**
 * @brief  [EN] Count the rows already in a day file (after a restart).
 *         [FA] شمردن ردیف‌های موجود یک فایل روز (بعد از روشن‌شدن).
 * @‎param  const char *char__path [EN] file / [FA]‎ فایل
 * @return [EN] uint16_t rows / [FA]‎ تعداد ردیف
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
 *         [FA] مطمئن شو فایل امروز هست و شمارندهٔ ردیفش درست است؛ نبودش
 *              گزارش می‌شود نه نادیده گرفته.
 * @return [EN] bool, false when the day file could not be opened or written
 *             / [FA]‎ آماده‌بودن
 */
static bool func__EspLog_OpenDay(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    bool bool__isNew;
    bool bool__ok = true;
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
        func__EspLog_FaultRaise();
        return false;
    }
    if (bool__isNew)
    {
        bool__ok = func__EspLog_FileText(file__day, ESP_LOG_HEADER);
    }
    file__day.close();
    if (bool__ok == false)
    {
        func__EspLog_FaultRaise();
    }

    UINT32_T__G__LogDayCode = uint32_t__dayCode;
    UINT16_T__G__LogRowsToday = func__EspLog_CountRows(char__path);
    BOOL__G__LogDayFull = (UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY);

    {
        bool bool__alreadyIndexed = false;

        for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            if (UINT32_T__G__LogIndexDay[uint8_t__slot] == uint32_t__dayCode)
            {
                bool__alreadyIndexed = true;
            }
        }
        if ((bool__alreadyIndexed == false) &&
            (UINT8_T__G__LogIndexCount < (ESP_LOG_MAX_DAYS + 2u)))
        {
            UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount] = uint32_t__dayCode;
            UINT8_T__G__LogIndexCount++;
            func__EspLog_TrimDays();
        }
    }
    return bool__ok;
}

/* ==================== Row write / نوشتن ردیف ==================== */

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
static uint8_t func__EspLog_Pct(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__vminMv = 21000u;
    uint32_t uint32_t__vmaxMv = 29000u;
    uint32_t uint32_t__rangeMv;

    if (BOOL__G__ParamKnown[74] != false)
    {
        uint32_t__vminMv = UINT32_T__G__ParamApplied[74];
    }
    if (BOOL__G__ParamKnown[75] != false)
    {
        uint32_t__vmaxMv = UINT32_T__G__ParamApplied[75];
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

/**
 * @brief  [EN] Append one row and close it at once, so a power cut right after
 *              the battery cut cannot lose it. The write is VERIFIED: when the
 *              flash does not take the bytes, the row is kept in the RAM ring
 *              and the fault counter is raised, so the panel can say "the flash
 *              did not accept the row" instead of "nothing happened".
 *         [FA] یک ردیف را اضافه و همان لحظه می‌بندد، تا قطع برق درست بعد از
 *              قطع باتری آن را از بین نبرد. نوشتن تأیید می‌شود: اگر فلش
 *              بایت‌ها را نپذیرد، ردیف در حلقهٔ رم می‌ماند و شمارندهٔ خطا بالا
 *              می‌رود، تا پنل بگوید «فلش ردیف را نپذیرفت» نه «چیزی نشده».
 * @‎param  uint16_t uint16_t__changeMask [EN] what changed / [FA]‎ چه عوض شد
 * @‎param  uint8_t uint8_t__note [EN] row note / [FA]‎ یادداشت ردیف
 * @return [EN] bool stored somewhere / [FA]‎ ذخیره شد
 */
static bool func__EspLog_WriteRow(uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    char char__path[ESP_LOG_PATH_MAX];
    char char__row[ESP_LOG_ROW_MAX];
    File file__day;
    uint32_t uint32_t__nowSec;
    uint32_t uint32_t__uptimeSec;
    uint8_t uint8_t__mains;
    bool bool__stored = false;

    if ((UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY) ||
        (BOOL__G__LogDayFull != false))
    {
        BOOL__G__LogDayFull = true;
        return false;
    }

    uint32_t__nowSec = func__EspLog_NowSec();
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
                   (unsigned long)func__EspLog_Pct(UINT32_T__G__TlmField[15]));

    if (BOOL__G__FsOk != false)
    {
        if (func__EspLog_OpenDay() != false)
        {
            if ((UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY) ||
                (BOOL__G__LogDayFull != false))
            {
                BOOL__G__LogDayFull = true;
                return false;
            }
            func__EspLog_PathOf(char__path, UINT32_T__G__LogDayCode);
            file__day = LittleFS.open(char__path, "a");
            if (file__day)
            {
                bool__stored = func__EspLog_FileText(file__day, char__row);
                if (bool__stored != false)
                {
                    bool__stored = func__EspLog_FileText(file__day, "\n");
                }
                file__day.close();
            }
        }
    }

    if ((bool__stored == false) || (BOOL__G__FsOk == false))
    {
        func__EspLog_RamPush(char__row);
    }
    if (BOOL__G__FsOk != false)
    {
        if (bool__stored != false)
        {
            UINT8_T__G__LogWriteFault = 0u;   /* the flash works again */
        }
        else
        {
            func__EspLog_FaultRaise();
        }
    }

    /* [EN] The row counter is the day-file cap, so it only grows when the file
            really grew - a broken flash must not "fill" a file that stays empty.
            With the file system off there is no file, and the session count
            keeps its old meaning.
       [FA] شمارندهٔ ردیف سقف فایل روز است، پس فقط وقتی بالا می‌رود که فایل
            واقعاً بزرگ شده باشد - فلش خراب نباید فایلی را که خالی مانده «پر»
            کند. با فایل‌سیستم خاموش فایلی نیست و شمارش همان معنای قبلی را
            دارد. */
    if ((bool__stored != false) || (BOOL__G__FsOk == false))
    {
        if (UINT16_T__G__LogRowsToday < 0xFFFFu)
        {
            UINT16_T__G__LogRowsToday++;
        }
    }

    if ((uint16_t__changeMask & ESP_LOG_CHG_VSTEP) != 0u)
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
    return true;
}

/* ==================== Last state / آخرین وضعیت ==================== */

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
    char char__line[96];
    bool bool__ok;

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    (void)snprintf(char__line, sizeof(char__line), "%lu,%lu,%lu,%u,%u\n",
                   (unsigned long)func__EspLog_NowSec(),
                   (unsigned long)UINT32_T__G__TlmField[15],
                   (unsigned long)UINT32_T__G__TlmField[19],
                   (unsigned int)UINT8_T__G__TlmFlags2,
                   (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u);

    file__last = LittleFS.open(ESP_LOG_LAST_PATH, "w");
    if (!file__last)
    {
        func__EspLog_FaultRaise();
        return;
    }
    bool__ok = func__EspLog_FileText(file__last, char__line);
    file__last.close();
    if (bool__ok == false)
    {
        func__EspLog_FaultRaise();
    }
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

/**
 * @brief  [EN] Absolute difference of two unsigned readings.
 *         [FA] اختلاف مطلق دو خوانش بدون علامت.
 * @‎return [EN] difference / [FA]‎ اختلاف
 */
static uint32_t func__EspLog_AbsDiff(uint32_t uint32_t__a, uint32_t uint32_t__b)
{
    return (uint32_t__a >= uint32_t__b) ? (uint32_t__a - uint32_t__b)
                                        : (uint32_t__b - uint32_t__a);
}

/* ==================== Change detection / تشخیص تغییر ==================== */

/* [EN] Defined below; the queue uses it to decide whether the row may go now.
   [FA] پایین‌تر تعریف می‌شود؛ صف از آن می‌پرسد ردیف الان اجازهٔ رفتن دارد یا نه. */
static bool func__EspLog_FlushDue(uint32_t uint32_t__nowMs);

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
    if (((uint16_t__mask & ESP_LOG_CHG_MAINS) != 0u) &&
        (BOOL__G__LogMainsQueuedValid == false))
    {
        BOOL__G__LogMainsQueuedValid = true;
        UINT32_T__G__LogMainsQueuedMs = uint32_t__nowMs;
    }
    if (func__EspLog_FlushDue(uint32_t__nowMs) != false)
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

/**
 * @brief  [EN] Is the pending row allowed to go to the flash now? Either the
 *              merge window has passed, or a mains flip has waited its short
 *              coalescing window (the board may cut the panel's power the same
 *              instant, so that row cannot wait two seconds).
 *         [FA] آیا ردیف در انتظار الان اجازهٔ رفتن به فلش دارد؟ یا پنجرهٔ ادغام
 *              گذشته، یا یک پرش برق شهر پنجرهٔ کوتاهش را صبر کرده (برد ممکن
 *              است همان لحظه برق پنل را ببرد، پس آن ردیف دو ثانیه صبر نمی‌کند).
 * @‎param  uint32_t uint32_t__nowMs [EN] now / [FA]‎ اکنون
 * @return [EN] bool due / [FA]‎ موقع نوشتن
 */
static bool func__EspLog_FlushDue(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__sinceRowMs;
    uint32_t uint32_t__sinceMainsMs;

    if (BOOL__G__LogLastRowValid == false)
    {
        return true;
    }
    uint32_t__sinceRowMs = uint32_t__nowMs - UINT32_T__G__LogLastRowMs;
    if (uint32_t__sinceRowMs >= ESP_LOG_MIN_INTERVAL_MS)
    {
        return true;
    }
    if (BOOL__G__LogMainsQueuedValid == false)
    {
        return false;
    }
    uint32_t__sinceMainsMs = uint32_t__nowMs - UINT32_T__G__LogMainsQueuedMs;
    return (uint32_t__sinceMainsMs >= ESP_LOG_MAINS_FLUSH_MS);
}

/**
 * @brief  [EN] Flush a queued change once its window has passed.
 *         [FA] نوشتن تغییر در انتظار، وقتی پنجره‌اش گذشت.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FlushPending(void)
{
    if (UINT16_T__G__LogPendingMask == 0u)
    {
        return;
    }
    if (func__EspLog_FlushDue((uint32_t)millis()) != false)
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
    uint8_t uint8_t__fl;
    uint32_t uint32_t__dead;
    uint32_t uint32_t__fault;
    uint16_t uint16_t__mask = 0u;

    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;
    uint8_t__chg1 = (uint8_t)UINT32_T__G__TlmField[6];
    uint8_t__chg2 = (uint8_t)UINT32_T__G__TlmField[13];
    uint8_t__flags2 = UINT8_T__G__TlmFlags2;
    uint32_t__fault = UINT32_T__G__TlmField[19];
    /* [EN] v1.98: the cut band has HYSTERESIS. It is entered below
            21,200 mV and left only above 21,400 mV, so a pack resting on the
            line - or a load switching on and off - cannot write a row every
            couple of seconds and fill the flash with one repeated event.
       [FA] نسخهٔ ۱٫۹۸: مرز قطع هیسترزیس دارد. زیر ۲۱٬۲۰۰ میلی‌ولت وارد و
            فقط بالای ۲۱٬۴۰۰ میلی‌ولت خارج می‌شود؛ پس باتریِ خوابیده روی
            مرز - یا باری که روشن و خاموش می‌شود - نمی‌تواند هر دو ثانیه
            یک ردیف بنویسد و فلش را با یک اتفاق تکراری پر کند. */
    if (UINT8_T__G__LogPrevBand != 0u)
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] > ESP_LOG_BAND_OFF_MV) ? 0u : 1u;
    }
    else
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
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
    if (uint8_t__fl != UINT8_T__G__LogPrevFl)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_MODE);
    }
    if (uint32_t__dead != UINT32_T__G__LogPrevDead)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_DEAD);
    }
    /* [EN] v1.98: a step of the pack voltage, either way. A row that is
            written anyway (any other change) carries the step for free - the
            voltage is on that row regardless. Only a row whose SOLE reason is
            the step is limited to one a minute, so a load switching on and
            off cannot flood the log while a slow charge still draws its
            curve.
       [FA] نسخهٔ ۱٫۹۸: یک پلهٔ ولتاژ پک، بالا یا پایین. ردیفی که به هر حال
            نوشته می‌شود (تغییر دیگر) گام را مجانی با خود می‌برد - ولتاژ روی
            همان ردیف هست. فقط ردیفی که تنها دلیلش گام است به یکی در دقیقه
            محدود می‌شود؛ پس باری که روشن و خاموش می‌شود لاگ را پر نمی‌کند و
            شارژ آرام همچنان منحنی‌اش را می‌کشد. */
    if (func__EspLog_AbsDiff(UINT32_T__G__TlmField[15],
                             UINT32_T__G__LogPrevV24) >= ESP_LOG_VSTEP_MV)
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
            ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastStepMs) >= ESP_LOG_VSTEP_MIN_MS))
        {
            uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_VSTEP);
        }
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
    UINT8_T__G__LogPrevFl = uint8_t__fl;
    UINT32_T__G__LogPrevDead = uint32_t__dead;
    /* [EN] the pack voltage is NOT stored here: it is compared
            with the value at the last ROW, so a slow charge still
            produces one row per step.
       [FA] ولتاژ پک اینجا ذخیره نمی‌شود: با مقدار آخرین ردیف
            مقایسه می‌شود تا شارژ آرام هم هر گام یک ردیف بدهد. */
}

/* ==================== Hooks / قلاب‌ها ==================== */

/**
 * @brief  [EN] Called after every decoded TLM frame.
 *         [FA] بعد از هر فریم TLM رمزگشایی‌شده صدا زده می‌شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_OnFrame(void)
{
    /* [EN] Link back always pairs a link-lost row that was written before.
       [FA] «اتصال برقرار شد» همیشه جفتِ همان «اتصال قطع شد»ی است که قبلاً
            نوشته شده. */
    if (BOOL__G__LogLinkDown != false)
    {
        BOOL__G__LogLinkDown = false;
        func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_BACK);
    }

    /* [EN] The boot row no longer waits for a board frame (user order
          2026-10-24: "if the board was not connected it logged nothing -
          it must log"). Tick() writes it at start-up; here only the
          state-change detection runs.
       [FA] ردیف بوت دیگر منتظر فریم برد نمی‌ماند (دستور ۲۰۲۶-۱۰-۲۴: «اگر برد
          وصل نبود لاگ نمی‌گرفت - باید بگیرد»). ردیف را Tick در شروع
          می‌نویسد؛ اینجا فقط تشخیص تغییر وضعیت اجرا می‌شود. */
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

    /* [EN] Boot row FIRST and with no board at all: the panel log is the
          witness of the panel itself, so a session with a silent board still
          starts its own record (user order 2026-10-24). It carries the state
          the panel saw last time and whether that run was in the danger band.
       [FA] اول ردیف بوت، آن هم بدون هیچ بردی: لاگِ پنل شاهدِ خودِ پنل است،
          پس نشستِ بدون برد هم تاریخچهٔ خودش را شروع می‌کند (۲۰۲۶-۱۰-۲۴).
          این ردیف وضعیتِ آخرین اجرا و خطرناک‌بودن آن را هم می‌برد. */
    if (BOOL__G__LogBootRowPending != false)
    {
        BOOL__G__LogBootRowPending = false;
        UINT32_T__G__LogSessionStartMs = (uint32_t)millis();
        UINT8_T__G__LogPrevNote = func__EspLog_LoadLastDanger();
        (void)func__EspLog_WriteRow(ESP_LOG_CHG_BOOT,
                                    (UINT8_T__G__LogPrevNote != 0u) ?
                                    ESP_LOG_NOTE_LOW_STOP : ESP_LOG_NOTE_NONE);
    }

    func__EspLog_FlushPending();

    uint32_t__nowMs = (uint32_t)millis();
    if (BOOL__G__LogClockValid != false)
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastClockSaveMs) >= ESP_LOG_CLOCK_SAVE_MS)
        {
            func__EspLog_ClockSave();
        }
    }

    /* [EN] Link-silence row: after LINK_TIMEOUT_MS with no frame. If the
          board has never spoken, the clock runs from the session start, so
          "no board" is written too - not only "board left".
       [FA] ردیف سکوت ارتباط: پس از چند میلی‌ثانیه بدون فریم. اگر برد هرگز
          حرفی نزده باشد، ساعت از شروع نشست می‌شمارد تا «برد نیست» هم ثبت
          شود، نه فقط «برد رفت». */
    if (BOOL__G__LogLinkDown == false)
    {
        bool bool__silent;

        if (BOOL__G__TlmSeen != false)
        {
            bool__silent = ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastTlmMs) >
                            ESP_LOG_LINK_TIMEOUT_MS);
        }
        else
        {
            bool__silent = (BOOL__G__LogLastRowValid != false) &&
                           ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogSessionStartMs) >
                            ESP_LOG_LINK_TIMEOUT_MS);
        }
        if (bool__silent != false)
        {
            BOOL__G__LogLinkDown = true;
            func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_LOST);
        }
    }

    /* [EN] v1.96: if nothing at all has happened for a whole heartbeat
            window, write one row that says so. It is the difference
            between "nothing changed" and "the panel stopped working":
            a board that keeps beating is a board that kept running.
       [FA] نسخهٔ ۱٫۹۶: اگر در یک بازهٔ کامل هیچ اتفاقی نیفتاده باشد، یک
            ردیف نوشته می‌شود که همین را بگوید. تفاوت «چیزی عوض نشده» با
            «پنل از کار افتاده» همین است: ضربانِ پیوسته یعنی پنل زنده ماند. */
    if (BOOL__G__LogLastRowValid != false)
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >=
            ESP_LOG_HEARTBEAT_MS)
        {
            func__EspLog_Queue(0u, ESP_LOG_NOTE_HEARTBEAT);
        }
    }
}

/**
 * @brief  [EN] Boot: clock from flash, day list, boot row pending.
 *         [FA] شروع: ساعت از فلش، فهرست روزها و ردیف بوت.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Begin(void)
{
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
    UINT8_T__G__LogWriteFault = 0u;
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    func__EspLog_ClockLoad();
    func__EspLog_IndexLoad();
}

/* ==================== Read back / خواندن دوباره ==================== */

/**
 * @brief  [EN] Where does the window of the newest rows start? It walks
 *              BACKWARD from the end of the file, so the limit is spent on the
 *              newest rows: the buffer may hold the last 18 rows of a busy day,
 *              and they are rows 383..400, never rows 1..18.
 *         [FA] پنجرهٔ جدیدترین ردیف‌ها از کجا شروع می‌شود؟ از انتهای فایل
 *              عقب‌عقب می‌رود، پس سقف بافر خرجِ جدیدترین ردیف‌ها می‌شود: بافر
 *              می‌تواند ۱۸ ردیف آخر یک روز شلوغ را نگه دارد و آن‌ها ردیف‌های
 *              آخر هستند، نه اولین ردیف‌های همان روز.
 * @‎param  File &file__day [EN] open day file / [FA]‎ فایل روز باز
 * @‎param  uint32_t uint32_t__size [EN] file size / [FA]‎ اندازهٔ فایل
 * @‎param  uint16_t uint16_t__maxRows [EN] row limit / [FA]‎ سقف ردیف
 * @‎param  size_t size_t__budget [EN] byte limit / [FA]‎ سقف بایت
 * @return [EN] offset to start from / [FA]‎ آفست شروع
 */
static uint32_t func__EspLog_TailStart(File &file__day,
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
        /* [EN] the last character of a normal file is the newline that closes
                the newest row; it is not a row of its own.
           [FA] آخرین کاراکتر فایل عادی، همان نوی‌لاینی است که جدیدترین ردیف
                را می‌بندد؛ خودش ردیف نیست. */
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
        /* [EN] the budget ran out inside a row: drop that half row, a row the
                panel cannot parse is worse than one row less.
           [FA] بودجه وسط یک ردیف تمام شد: همان نیم‌ردیف دور ریخته می‌شود؛
                ردیفی که پنل نتواند بخواندش از یک ردیف کمتر بدتر است. */
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

/**
 * @brief  [EN] Copy the last N rows of the RAM ring into a buffer, oldest first.
 *         [FA] کپی آخرین N ردیف حلقهٔ رم در بافر، قدیمی‌تر اول.
 * @return [EN] size_t bytes written / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_TailRam(char *char__out, size_t size_t__outSize,
                                   uint16_t uint16_t__maxLines)
{
    uint8_t uint8_t__start;
    uint8_t uint8_t__count = UINT8_T__G__LogRamCount;
    uint8_t uint8_t__item;
    size_t size_t__used = 0u;

    if (uint8_t__count == 0u)
    {
        return 0u;
    }
    uint8_t__start = (uint8_t)((UINT8_T__G__LogRamHead + ESP_LOG_RAM_ROWS - uint8_t__count) % ESP_LOG_RAM_ROWS);
    if ((uint16_t)uint8_t__count > uint16_t__maxLines)
    {
        uint8_t uint8_t__drop = (uint8_t)((uint16_t)uint8_t__count - uint16_t__maxLines);
        uint8_t__start = (uint8_t)((uint8_t__start + uint8_t__drop) % ESP_LOG_RAM_ROWS);
        uint8_t__count = (uint8_t)uint16_t__maxLines;
    }
    for (uint8_t__item = 0u; uint8_t__item < uint8_t__count; uint8_t__item++)
    {
        uint8_t uint8_t__slot = (uint8_t)((uint8_t__start + uint8_t__item) % ESP_LOG_RAM_ROWS);
        size_t size_t__len = strlen(CHAR__G__LogRamRow[uint8_t__slot]);
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__out[size_t__used], CHAR__G__LogRamRow[uint8_t__slot], size_t__len);
        size_t__used += size_t__len;
        char__out[size_t__used] = '\n';
        size_t__used++;
        char__out[size_t__used] = '\0';
    }
    return size_t__used;
}

/**
 * @brief  [EN] Which file does an automatic read pick? Today's file when it
 *              exists, otherwise the newest day of the list - the day the phone
 *              clock has not named yet keeps its own file and is never hidden.
 *         [FA] خواندن خودکار کدام فایل را می‌گیرد؟ فایل امروز اگر باشد، وگرنه
 *              جدیدترین روز فهرست - روزی که ساعت گوشی هنوز نامش را نبرده فایل
 *              خودش را دارد و هرگز پنهان نمی‌شود.
 * @return [EN] day code or ESP_LOG_DAY_AUTO when nothing exists / [FA]‎ کد روز
 */
static uint32_t func__EspLog_PickDay(uint32_t uint32_t__dayCode)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint32_t uint32_t__chosen = uint32_t__dayCode;

    if ((uint32_t__chosen == ESP_LOG_DAY_ZERO))
    {
        return 0u;
    }
    if ((uint32_t__chosen != ESP_LOG_DAY_AUTO) && (uint32_t__chosen != 0u))
    {
        return uint32_t__chosen;
    }

    uint32_t__chosen = func__EspLog_TodayCode();
    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        if (UINT8_T__G__LogIndexCount == 0u)
        {
            return ESP_LOG_DAY_AUTO;
        }
        uint32_t__chosen = UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
    }
    return uint32_t__chosen;
}

/**
 * @brief  [EN] Copy the last N rows of one day into a buffer, oldest first.
 *              The window is chosen from the END of the file (see
 *              func__EspLog_TailStart), so when the buffer cannot hold 30 rows
 *              the OLDEST of them are dropped, never the newest.
 *         [FA] کپی آخرین N ردیف یک روز در بافر، قدیمی‌تر اول. پنجره از انتهای
 *              فایل انتخاب می‌شود، پس وقتی بافر ۳۰ ردیف را جا نمی‌دهد
 *              قدیمی‌ترین‌ها دور ریخته می‌شوند، نه جدیدترین‌ها.
 * @‎param  char *char__out [EN] output buffer / [FA]‎ بافر خروجی
 * @‎param  size_t size_t__outSize [EN] buffer size / [FA]‎ اندازهٔ بافر
 * @‎param  uint32_t uint32_t__dayCode [EN] day, AUTO = newest / [FA]‎ روز
 * @‎param  uint16_t uint16_t__maxLines [EN] how many rows / [FA]‎ چند ردیف
 * @return [EN] size_t bytes written / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_Tail(char *char__out, size_t size_t__outSize,
                                uint32_t uint32_t__dayCode, uint16_t uint16_t__maxLines)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    size_t size_t__used = 0u;
    uint32_t uint32_t__chosen;
    uint32_t uint32_t__size;
    uint32_t uint32_t__start;

    if ((char__out == NULL) || (size_t__outSize < 2u))
    {
        return 0u;
    }
    char__out[0] = '\0';

    /* [EN] No file system, or a file system that refused the last writes: the
            newest rows are the ones in RAM. Serving them is what makes the
            panel show "the flash did not accept it" with real rows under it,
            instead of an empty card.
       [FA] فایل‌سیستم نیست، یا فایل‌سیستمی که آخرین نوشتن‌ها را نپذیرفت:
            جدیدترین ردیف‌ها همان‌هایی هستند که در رم‌اند. نشان دادن همان‌ها
            کاری است که پنل را «فلش نپذیرفت» با ردیف‌های واقعی زیرش نشان
            می‌دهد، نه کارت خالی. */
    if ((BOOL__G__FsOk == false) ||
        ((UINT8_T__G__LogWriteFault != 0u) && (UINT8_T__G__LogRamCount != 0u)))
    {
        return func__EspLog_TailRam(char__out, size_t__outSize, uint16_t__maxLines);
    }

    uint32_t__chosen = func__EspLog_PickDay(uint32_t__dayCode);
    if (uint32_t__chosen == ESP_LOG_DAY_AUTO)
    {
        return 0u;
    }

    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        return 0u;
    }
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size == 0u)
    {
        file__day.close();
        return 0u;
    }
    uint32_t__start = func__EspLog_TailStart(file__day, uint32_t__size,
                                             uint16_t__maxLines, size_t__outSize - 2u);
    if (file__day.seek(uint32_t__start) == false)
    {
        file__day.close();
        return 0u;
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
        (void)memcpy(&char__out[size_t__used], string__line.c_str(), size_t__len);
        size_t__used += size_t__len;
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

    UINT8_T__G__LogRamHead = 0u;
    UINT8_T__G__LogRamCount = 0u;
    if (BOOL__G__FsOk == false)
    {
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
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
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    UINT8_T__G__LogWriteFault = 0u;
    return uint8_t__removed;
}

/**
 * @brief  [EN] Day list as text ("YYYYMMDD YYYYMMDD ... , newest last").
 *         [FA] فهرست روزها به‌صورت متن (قدیمی‌ترین تا جدیدترین).
 * @‎param  char *char__out [EN] output / [FA]‎ خروجی
 * @‎param  size_t size_t__outSize [EN] size / [FA]‎ اندازه
 * @return [EN] size_t / [FA]‎ تعداد بایت
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
    if (BOOL__G__FsOk == false)
    {
        if ((UINT8_T__G__LogRamCount > 0u) && (size_t__outSize >= 12u))
        {
            int int__ramWritten = snprintf(char__out, size_t__outSize, "%08lu",
                                           (unsigned long)func__EspLog_TodayCode());
            return (int__ramWritten > 0) ? (size_t)int__ramWritten : 0u;
        }
        return 0u;
    }
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
