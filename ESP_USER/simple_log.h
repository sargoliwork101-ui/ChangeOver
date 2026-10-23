/* simple_log.h - تاریخچهٔ وضعیت پنل ساده (فقط فارسی)
   هر روز یک فایل CSV روی فلش خود پنل؛ ردیف فقط وقتی نوشته می‌شود که چیزی
   واقعاً عوض شده باشد. هر ردیف همان لحظه بسته می‌شود تا قطع باتری آخرین ردیف
   را از بین نبرد.
   ترتیب include: بعد از simple_state.h و پیش از simple_link.h. */

#include <string.h>

/* ==================== ساعت و روز ==================== */
static int32_t  INT32_T__G__LogTzSec = 0;
static uint32_t UINT32_T__G__LogDayCode = 0u;
static uint16_t UINT16_T__G__LogRowsToday = 0u;
static bool     BOOL__G__LogDayFull = false;
static bool     BOOL__G__LogBootRowPending = true;

/* ==================== وضعیت قبلی و تغییر در انتظار ==================== */
static bool     BOOL__G__LogHasPrev = false;
static uint8_t  UINT8_T__G__LogPrevMains = 0u;
/* [EN] v1.95: the board's general byte (hand mode, snapshot, valid
        measurement) is logged too, so a mode change is an event.
   [FA] نسخهٔ ۱٫۹۵: بایت عمومی برد (مود دستی، snapshot، معتبر بودن
        اندازه‌گیری) هم ثبت می‌شود تا تغییر مود یک رویداد باشد. */
static uint8_t  UINT8_T__G__LogPrevFl = 0u;
static uint8_t  UINT8_T__G__LogPrevChg1 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg2 = 0u;
static uint8_t  UINT8_T__G__LogPrevFlags2 = 0u;
static uint8_t  UINT8_T__G__LogPrevBand = 0u;
static uint32_t UINT32_T__G__LogPrevFault = 0u;
static uint8_t  UINT8_T__G__LogPrevNote = 0u;
static uint16_t UINT16_T__G__LogPendingMask = 0u;
static uint8_t  UINT8_T__G__LogPendingNote = 0u;
static uint32_t UINT32_T__G__LogLastRowMs = 0u;
static bool     BOOL__G__LogLastRowValid = false;
static bool     BOOL__G__LogLinkDown = false;

/* ==================== فهرست روزها ==================== */
static uint32_t UINT32_T__G__LogIndexDay[SIMPLE_LOG_MAX_DAYS + 2u];
static uint8_t  UINT8_T__G__LogIndexCount = 0u;

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

#define SIMPLE_LOG_NOTE_NONE      0u
#define SIMPLE_LOG_NOTE_LOW_STOP  1u
#define SIMPLE_LOG_NOTE_LINK_LOST 2u
#define SIMPLE_LOG_NOTE_LINK_BACK 3u
#define SIMPLE_LOG_NOTE_DAY_FULL  4u

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

/* ==================== فهرست روزها ==================== */

static void func__SimpleLog_IndexSave(void)
{
    uint8_t uint8_t__slot;

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    File file__index = LittleFS.open(SIMPLE_LOG_INDEX_PATH, "w");
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

static void func__SimpleLog_IndexLoad(void)
{
    UINT8_T__G__LogIndexCount = 0u;
    if ((BOOL__G__FsOk == false) || (LittleFS.exists(SIMPLE_LOG_INDEX_PATH) == false))
    {
        return;
    }
    File file__index = LittleFS.open(SIMPLE_LOG_INDEX_PATH, "r");
    if (!file__index)
    {
        return;
    }
    while (file__index.available() && (UINT8_T__G__LogIndexCount < (SIMPLE_LOG_MAX_DAYS + 2u)))
    {
        String string__line = file__index.readStringUntil('\n');
        string__line.trim();
        if (string__line.length() > 0u)
        {
            UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount] = (uint32_t)string__line.toInt();
            UINT8_T__G__LogIndexCount++;
        }
    }
    file__index.close();
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
    func__SimpleLog_IndexSave();
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

static bool func__SimpleLog_OpenDay(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    bool bool__isNew;
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
        file__day.print(SIMPLE_LOG_HEADER);
    }
    file__day.close();

    UINT32_T__G__LogDayCode = uint32_t__dayCode;
    UINT16_T__G__LogRowsToday = func__SimpleLog_CountRows(char__ptr_path);
    BOOL__G__LogDayFull = (UINT16_T__G__LogRowsToday >= SIMPLE_LOG_MAX_ROWS_PER_DAY);

    if (bool__isNew)
    {
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
            func__SimpleLog_TrimDays();
        }
    }
    return true;
}

/* ==================== نوشتن ردیف ==================== */

static void func__SimpleLog_SaveLast(void);

static bool func__SimpleLog_WriteRow(uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    uint32_t uint32_t__nowSec;
    uint32_t uint32_t__uptimeSec;
    uint8_t uint8_t__mains;

    if (func__SimpleLog_OpenDay() == false)
    {
        return false;
    }
    if ((UINT16_T__G__LogRowsToday >= SIMPLE_LOG_MAX_ROWS_PER_DAY) ||
        (BOOL__G__LogDayFull != false))
    {
        BOOL__G__LogDayFull = true;
        return false;
    }

    func__SimpleLog_PathOf(char__ptr_path, UINT32_T__G__LogDayCode);
    File file__day = LittleFS.open(char__ptr_path, "a");
    if (!file__day)
    {
        return false;
    }

    uint32_t__nowSec = func__Simple_NowSec();
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
    file__day.print((unsigned int)UINT8_T__G__TlmFlags);
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
    if ((BOOL__G__LogLastRowValid == false) ||
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >= SIMPLE_LOG_MIN_INTERVAL_MS))
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
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >= SIMPLE_LOG_MIN_INTERVAL_MS))
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
    uint32_t uint32_t__fault;
    uint16_t uint16_t__mask = 0u;

    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;
    uint8_t__chg1 = (uint8_t)UINT32_T__G__TlmField[6];
    uint8_t__chg2 = (uint8_t)UINT32_T__G__TlmField[13];
    uint8_t__flags2 = UINT8_T__G__TlmFlags2;
    uint32_t__fault = UINT32_T__G__TlmField[19];
    uint8_t__band = (UINT32_T__G__TlmField[15] < SIMPLE_LOG_CUT_BAND_MV) ? 1u : 0u;
    /* [EN] every state bit of the general byte except the input-present
            bit, which already has its own change bit: snapshot, valid
            measurement, both charger enables and hand mode.
       [FA] همهٔ بیت‌های وضعیتِ بایت عمومی جز بیت برق ورودی، که بیت تغییر
            خودش را دارد: snapshot، معتبر بودن اندازه‌گیری، اجازهٔ هر دو
            شارژر و مود دستی. */
    uint8_t__fl = (uint8_t)(UINT8_T__G__TlmFlags & 0x3Du);

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
}

/* ==================== قلاب‌ها ==================== */

static void func__SimpleLog_OnFrame(void)
{
    if (BOOL__G__LogLinkDown != false)
    {
        BOOL__G__LogLinkDown = false;
        if (BOOL__G__LogHasPrev != false)
        {
            func__SimpleLog_Queue(SIMPLE_LOG_CHG_LINK, SIMPLE_LOG_NOTE_LINK_BACK);
        }
    }

    if ((BOOL__G__LogBootRowPending != false) && (BOOL__G__LogHasPrev == false))
    {
        UINT8_T__G__LogPrevNote = func__SimpleLog_LoadLastDanger();
        (void)func__SimpleLog_WriteRow(SIMPLE_LOG_CHG_BOOT,
                                       (UINT8_T__G__LogPrevNote != 0u) ?
                                       SIMPLE_LOG_NOTE_LOW_STOP : SIMPLE_LOG_NOTE_NONE);
        BOOL__G__LogBootRowPending = false;
    }

    func__SimpleLog_DetectChanges();
}

static void func__SimpleLog_Tick(void)
{
    uint32_t uint32_t__nowMs;

    func__SimpleLog_FlushPending();

    uint32_t__nowMs = (uint32_t)millis();
    if ((BOOL__G__TlmSeen != false) && (BOOL__G__LogLinkDown == false))
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastTlmMs) > SIMPLE_LOG_LINK_TIMEOUT_MS)
        {
            BOOL__G__LogLinkDown = true;
            func__SimpleLog_Queue(SIMPLE_LOG_CHG_LINK, SIMPLE_LOG_NOTE_LINK_LOST);
        }
    }
}

static void func__SimpleLog_Begin(void)
{
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    func__SimpleLog_IndexLoad();
}

static uint16_t func__SimpleLog_RowsOf(const char *char__ptr_path)
{
    return func__SimpleLog_CountRows(char__ptr_path);
}

static size_t func__SimpleLog_Tail(char *char__ptr_out, size_t size_t__outSize,
                                   uint32_t uint32_t__dayCode, uint16_t uint16_t__maxLines)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    size_t size_t__used = 0u;
    uint16_t uint16_t__total;
    uint16_t uint16_t__skip;
    uint16_t uint16_t__index = 0u;
    uint32_t uint32_t__chosen = uint32_t__dayCode;

    if ((char__ptr_out == NULL) || (size_t__outSize < 2u) || (BOOL__G__FsOk == false))
    {
        return 0u;
    }
    char__ptr_out[0] = '\0';

    if (uint32_t__chosen == 0u)
    {
        if (UINT8_T__G__LogIndexCount == 0u)
        {
            return 0u;
        }
        uint32_t__chosen = UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
    }

    func__SimpleLog_PathOf(char__ptr_path, uint32_t__chosen);
    if (LittleFS.exists(char__ptr_path) == false)
    {
        return 0u;
    }

    uint16_t__total = func__SimpleLog_RowsOf(char__ptr_path);
    uint16_t__skip = (uint16_t__total > uint16_t__maxLines) ?
                     (uint16_t)(uint16_t__total - uint16_t__maxLines) : 0u;

    File file__day = LittleFS.open(char__ptr_path, "r");
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
        (void)memcpy(&char__ptr_out[size_t__used], string__line.c_str(), string__line.length());
        size_t__used += string__line.length();
        char__ptr_out[size_t__used] = '\n';
        size_t__used++;
        char__ptr_out[size_t__used] = '\0';
    }
    file__day.close();
    return size_t__used;
}

static uint8_t func__SimpleLog_Clear(void)
{
    char char__ptr_path[SIMPLE_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    uint8_t uint8_t__removed = 0u;

    if (BOOL__G__FsOk == false)
    {
        return 0u;
    }
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
    UINT32_T__G__LogDayCode = 0u;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
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
