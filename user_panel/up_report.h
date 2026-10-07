/**
 * @file    up_report.h
 * @brief   [EN] The Excel report: what to write into the workbook, in what
 *              order, and in which language. The writer next door (up_xlsx.h)
 *              knows ZIP and XML; this file knows the MACHINE - charges,
 *              outages, faults, imbalance, the day ring and the sample ring -
 *              and it is the only place where a column means something.
 *          [FA] گزارش اکسل: چه چیزی در کتاب نوشته شود، به چه ترتیبی و به چه
 *              زبانی. نویسندهٔ کناری (up_xlsx.h) ZIP و XML می‌داند؛ این فایل
 *              «دستگاه» را می‌شناسد - شارژها، قطع ورودی، خطاها، عدم‌توازن، حلقهٔ
 *              روزها و حلقهٔ نمونه‌ها - و تنها جایی است که یک ستون معنا دارد.
 *
 * @note    [EN] Five sheets, in the order an operator reads them:
 *              خلاصه (what happened), روزانه (one line per day),
 *              شارژها (one line per charge), رویدادها (everything else that
 *              happened) and نمونه‌ها (the raw voltage/current trail).
 *              Every sheet is streamed, so the report is limited by flash, not
 *              by RAM - the only fixed limit is the sample sheet, capped so a
 *              three-month range still opens in a second.
 *          [FA] پنج برگه، به همان ترتیبی که اپراتور می‌خواند: خلاصه (چه شد)،
 *              روزانه (هر روز یک خط)، شارژها (هر شارژ یک خط)، رویدادها (هر چیز
 *              دیگری که رخ داد) و نمونه‌ها (رد ولتاژ و جریان خام). همهٔ برگه‌ها
 *              جریانی‌اند، پس محدودیت گزارش فلش است نه رم - تنها سقف ثابت، برگهٔ
 *              نمونه‌هاست که بسته می‌شود تا بازهٔ سه‌ماهه هم در یک ثانیه باز شود.
 *
 * @note    [EN] WHY A "RANGE" IS FILTERED BY MONOTONIC SECONDS
 *              The filter compares the panel's own monotonic clock, not the wall
 *              clock, so "the last 7 days" keeps working even when nobody set
 *              the date: the board has been counting since its first boot, and
 *              that count is what a record carries.
 *          [FA] چرا «بازه» با ثانیهٔ یکنوا فیلتر می‌شود
 *              فیلتر ساعت یکنوای خود پنل را می‌سنجد، نه ساعت دیواری را؛ پس
 *              «۷ روز گذشته» حتی وقتی کسی تاریخ را تنظیم نکرده هم کار می‌کند:
 *              برد از اولین بوتش می‌شمارد و رکورد همان شمارش را با خود دارد.
 */

#ifndef UP_REPORT_H
#define UP_REPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "up_config.h"
#include "up_state.h"
#include "up_store.h"
#include "up_history.h"
#include "up_calendar.h"
#include "up_xlsx.h"

/* ==================== Constants / ثابت‌ها ==================== */
#define UP_REPORT_SHEETS         5u      /* [EN] how many sheets the report has / [FA] تعداد برگه‌های گزارش */
#define UP_REPORT_COL_MAX        14u     /* [EN] widest table in the report / [FA] عریض‌ترین جدول گزارش */
#define UP_REPORT_DAYS_ALL        0u     /* [EN] "?days=0": no filter / [FA] «?days=0»: بدون فیلتر */
#define UP_REPORT_TEXT_MAX       24u     /* [EN] room for a date or a clock / [FA] جای یک تاریخ یا ساعت */
#define UP_REPORT_NAME_MAX       64u     /* [EN] room for a translated name / [FA] جای یک نام برگردانده‌شده */
/* [EN] 64 bytes, not 40: a Persian letter is two UTF-8 bytes, so a 20-letter
   label such as "زمان تولید گزارش (شمسی)" needs more room than its letter count.
   [FA] ۶۴ بایت و نه ۴۰: هر حرف فارسی دو بایت UTF-8 است، پس برچسبی ۲۰ حرفی مانند
   «زمان تولید گزارش (شمسی)» بیش از شمار حروفش جا می‌خواهد. */
#define UP_REPORT_LABEL_MAX      64u     /* [EN] room for a row label / [FA] جای برچسب یک ردیف */
#define UP_REPORT_SECONDS_PER_DAY 86400u
#define UP_REPORT_HOURS_PER_DAY  24u
#define UP_REPORT_BYTES_PER_KB   1024u

/* [EN] Column numbers, sheet by sheet. A bare "2" in the middle of a row of
   numbers is how a report ends up with the duration in the voltage column, so
   every column has a name.
   [FA] شمارهٔ ستون‌ها، برگه به برگه. یک «۲» لخت در میان ردیفی از عدد، همان
   چیزی است که باعث می‌شود مدت شارژ در ستون ولتاژ بیفتد؛ پس هر ستون نام دارد. */
#define UP_REPORT_SUM_LABEL      0u
#define UP_REPORT_SUM_VALUE      1u
#define UP_REPORT_SUM_UNIT       2u
#define UP_REPORT_SUM_COLS       3u

#define UP_REPORT_DAY_DATE       0u
#define UP_REPORT_DAY_WEEKDAY    1u
#define UP_REPORT_DAY_DONE       2u
#define UP_REPORT_DAY_PART       3u
#define UP_REPORT_DAY_CHARGE_MIN 4u
#define UP_REPORT_DAY_ENERGY_WH  5u
#define UP_REPORT_DAY_RUN_MIN    6u
#define UP_REPORT_DAY_INPUT_MIN  7u
#define UP_REPORT_DAY_OUTAGES    8u
#define UP_REPORT_DAY_BOOTS      9u
#define UP_REPORT_DAY_MAX_V      10u
#define UP_REPORT_DAY_MIN_V      11u
#define UP_REPORT_DAY_PEAK_HOUR  12u
#define UP_REPORT_DAY_COLS       13u

#define UP_REPORT_CHG_DATE       0u
#define UP_REPORT_CHG_CLOCK      1u
#define UP_REPORT_CHG_CHANNEL    2u
#define UP_REPORT_CHG_RESULT     3u
#define UP_REPORT_CHG_MINUTES    4u
#define UP_REPORT_CHG_SECONDS    5u
#define UP_REPORT_CHG_ENERGY_WH  6u
#define UP_REPORT_CHG_COLS       7u

#define UP_REPORT_EV_DATE        0u
#define UP_REPORT_EV_CLOCK       1u
#define UP_REPORT_EV_INDEX       2u
#define UP_REPORT_EV_NAME        3u
#define UP_REPORT_EV_CHANNEL     4u
#define UP_REPORT_EV_SEVERITY    5u
#define UP_REPORT_EV_SECONDS     6u
#define UP_REPORT_EV_VALUE_A     7u
#define UP_REPORT_EV_VALUE_B     8u
#define UP_REPORT_EV_STATE       9u
#define UP_REPORT_EV_COLS        10u

#define UP_REPORT_SMP_DATE       0u
#define UP_REPORT_SMP_CLOCK      1u
#define UP_REPORT_SMP_ABS        2u
#define UP_REPORT_SMP_VIN        3u
#define UP_REPORT_SMP_V24        4u
#define UP_REPORT_SMP_V12        5u
#define UP_REPORT_SMP_I1         6u
#define UP_REPORT_SMP_I2         7u
#define UP_REPORT_SMP_DUTY1      8u
#define UP_REPORT_SMP_DUTY2      9u
#define UP_REPORT_SMP_STATE1     10u
#define UP_REPORT_SMP_STATE2     11u
#define UP_REPORT_SMP_FLAGS      12u
#define UP_REPORT_SMP_COLS       13u

/* [EN] Millivolt and milliamp columns become volts and amps with three
   decimals: 24000 mV is 24.000 V and 12440 mA is 12.440 A.
   [FA] ستون‌های میلی‌ولت و میلی‌آمپر با سه رقم اعشار به ولت و آمپر تبدیل
   می‌شوند: ۲۴۰۰۰ میلی‌ولت می‌شود 24.000 و ۱۲۴۴۰ میلی‌آمپر می‌شود 12.440. */
#define UP_REPORT_UNIT_DECIMALS   3u
#define UP_REPORT_ENERGY_DECIMALS 2u

/* ==================== Types / تایپ‌ها ==================== */
/* [EN] What the summary needs about the chosen range: everything a day row
   carries, added up. Kept as one struct so the sheet can print it in one go and
   so a test can check the arithmetic without parsing a workbook.
   [FA] آنچه خلاصه دربارهٔ بازهٔ انتخابی لازم دارد: هرچه یک ردیف روز دارد، جمع
   زده‌شده. یک ساختار است تا برگه یک‌جا چاپش کند و تست بتواند حساب را بدون
   خواندن کتاب بررسی کند. */
typedef struct
{
    uint32_t uint32_t__days;            /* [EN] days that actually have data / [FA] روزهای دارای داده */
    uint32_t uint32_t__charges;         /* [EN] completed charges / [FA] شارژهای کامل */
    uint32_t uint32_t__incomplete;      /* [EN] unfinished charges / [FA] شارژهای ناتمام */
    uint32_t uint32_t__chargeS;         /* [EN] total charging seconds / [FA] مجموع ثانیه‌های شارژ */
    uint32_t uint32_t__energyWh100;     /* [EN] charged energy, 0.01 Wh / [FA] انرژی شارژشده */
    uint32_t uint32_t__outages;         /* [EN] input cuts / [FA] قطع ورودی */
    uint32_t uint32_t__inputS;          /* [EN] seconds on input / [FA] ثانیه‌های روی ورودی */
    uint32_t uint32_t__runS;            /* [EN] seconds on battery / [FA] ثانیه‌های روی باتری */
    uint32_t uint32_t__boots;           /* [EN] board restarts seen / [FA] ری‌استارت برد */
    uint32_t uint32_t__imbEvents;       /* [EN] imbalance episodes / [FA] رویدادهای عدم‌توازن */
    uint32_t uint32_t__faultSets;       /* [EN] faults that appeared / [FA] خطاهایی که ظاهر شدند */
} up_report_totals_t;

/* ==================== State / وضعیت ==================== */
/* [EN] The range filter and the counting results, all static: one report runs at
   a time and static allocation is a project rule.
   [FA] فیلتر بازه و نتایج شمارش، همه استاتیک: هر بار یک گزارش اجرا می‌شود و
   تخصیص استاتیک قاعدهٔ پروژه است. */
static uint32_t UINT32_T__G__UpReportDays = UP_REPORT_DAYS_ALL;
static uint32_t UINT32_T__G__UpReportCutoffAbs = 0u;
static uint32_t UINT32_T__G__UpReportFirstDay = 0u;
static uint32_t UINT32_T__G__UpReportTodayDay = 0u;
static uint32_t UINT32_T__G__UpReportRow = 1u;
static uint32_t UINT32_T__G__UpReportSamplesInRange = 0u;
static uint32_t UINT32_T__G__UpReportSampleSkip = 0u;
static uint32_t UINT32_T__G__UpReportEventsInRange = 0u;
static uint32_t UINT32_T__G__UpReportImbalanceInRange = 0u;
static uint32_t UINT32_T__G__UpReportFaultsInRange = 0u;
static bool     BOOL__G__UpReportSampleCapped = false;

/* [EN] The sheet names, in the order the report fills them. Kept as one table
   because the workbook and the sheet writers must never disagree about which
   sheet is number three.
   [FA] نام برگه‌ها، به همان ترتیبی که گزارش پُرشان می‌کند. یک جدول‌اند چون کتاب
   و نویسندهٔ برگه‌ها هرگز نباید دربارهٔ این‌که برگهٔ شمارهٔ سه کدام است اختلاف
   داشته باشند. */
static const char *const CHAR__A__G__UpReportSheets[UP_REPORT_SHEETS] =
{
    "خلاصه", "روزانه", "شارژها", "رویدادها", "نمونه‌ها"
};

/* ==================== Translations / برگردان‌ها ==================== */
/* [EN] The device stores CODES, never sentences: one byte per event fits in
   flash and stays translatable. This is where a code becomes a word an operator
   can read, and it is the same list the page shows.
   [FA] دستگاه «کد» ذخیره می‌کند، هرگز جمله: یک بایت برای هر رویداد در فلش جا
   می‌شود و ترجمه‌پذیر می‌ماند. اینجا همان جایی است که کد به کلمه‌ای تبدیل
   می‌شود که اپراتور می‌خواند، و همان فهرستی است که صفحه نشان می‌دهد. */

/* ==================== Event name ==================== */
/**
 * @brief  [EN] Persian name of a stored event code.
 *         [FA] نام فارسی یک کد رویداد ذخیره‌شده.
 * @param  uint8_t__code [EN] up_event_code_t / [FA] کد رویداد
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_EventText(uint8_t uint8_t__code)
{
    static const char *const CHAR__A__Names[17] =
    {
        "نامشخص", "شروع شارژ", "شارژ کامل شد", "شارژ ناتمام ماند",
        "قطع ورودی ۲۴ ولت", "بازگشت ورودی ۲۴ ولت", "شروع کار روی باتری",
        "پایان کار روی باتری", "خطا فعال شد", "خطا پاک شد",
        "برد ریست شد", "راه‌اندازی پنل", "قطع شارژ از پنل",
        "وصل شارژ از پنل", "رویداد عدم‌توازن", "پاک‌کردن تاریخچه",
        "مود دستی فعال شد"
    };

    if (uint8_t__code >= 17u)
    {
        return CHAR__A__Names[0];
    }
    return CHAR__A__Names[uint8_t__code];
}

/* ==================== Severity name ==================== */
/**
 * @brief  [EN] Persian name of an event severity.
 *         [FA] نام فارسی شدت یک رویداد.
 * @param  uint8_t__severity [EN] UP_SEV_* / [FA] شدت
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_SeverityText(uint8_t uint8_t__severity)
{
    if (uint8_t__severity == UP_SEV_CRIT)
    {
        return "خطا";
    }
    if (uint8_t__severity == UP_SEV_WARN)
    {
        return "هشدار";
    }
    return "اطلاع";
}

/* ==================== Charger state name ==================== */
/**
 * @brief  [EN] Persian name of a charger state as the board reports it.
 *         [FA] نام فارسی حالت شارژر همان‌طور که برد گزارش می‌کند.
 * @param  uint8_t__state [EN] 0..9 / [FA] صفر تا ۹
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_StateText(uint8_t uint8_t__state)
{
    static const char *const CHAR__A__Names[10] =
    {
        "خاموش", "شارژ سریع", "تثبیت ولتاژ", "شارژ کامل", "آماده‌سازی",
        "تلاش مجدد", "انتظار ورودی", "خطای نهایی", "قطع باتری", "دستی (تعمیر)"
    };

    if (uint8_t__state >= 10u)
    {
        return CHAR__A__Names[0];
    }
    return CHAR__A__Names[uint8_t__state];
}

/* ==================== Channel name ==================== */
/**
 * @brief  [EN] Persian name of an event channel. Channel 1 is the UPPER charger
   *              in this panel, channel 2 the lower one.
 *         [FA] نام فارسی کانال یک رویداد. کانال ۱ در این پنل شارژر «بالایی» و
 *              کانال ۲ شارژر «پایینی» است.
 * @param  uint8_t__channel [EN] 0, 1 or 2 / [FA] صفر، ۱ یا ۲
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_ChannelText(uint8_t uint8_t__channel)
{
    if (uint8_t__channel == 1u)
    {
        return "کانال بالایی";
    }
    if (uint8_t__channel == 2u)
    {
        return "کانال پایینی";
    }
    return "کل دستگاه";
}

/* ==================== Fault name ==================== */
/**
 * @brief  [EN] Persian name of the fault a bit stands for. The board reports a
 *              MASK (1, 2, 4, ...), so the mask is turned back into a bit number
 *              here and the sentence comes out of the same list the page uses.
 *         [FA] نام فارسی خطایی که یک بیت نشان می‌دهد. برد «ماسک» گزارش می‌کند
 *              (۱، ۲، ۴، ...)، پس ماسک همین‌جا به شمارهٔ بیت برمی‌گردد و جمله از
 *              همان فهرستی می‌آید که صفحه استفاده می‌کند.
 * @param  uint16_t__mask [EN] one fault bit / [FA] یک بیت خطا
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_FaultText(uint16_t uint16_t__mask)
{
    static const char *const CHAR__A__Names[7] =
    {
        "خرابی اندازه‌گیری برد", "اضافه‌جریان کانال بالایی", "اضافه‌جریان کانال پایینی",
        "باتری ضعیف", "حفاظت ترانس کانال بالا", "حفاظت ترانس کانال پایین",
        "قطع سیم باتری"
    };

    for (uint8_t uint8_t__bit = 0u; uint8_t__bit < 7u; uint8_t__bit++)
    {
        if (uint16_t__mask == (uint16_t)(1u << uint8_t__bit))
        {
            return CHAR__A__Names[uint8_t__bit];
        }
    }
    return "خطای نامشخص";
}

/* ==================== Row state name ==================== */
/**
 * @brief  [EN] Is the event still running? An event that is still open has no
 *              duration yet, and saying so is more honest than printing zero.
 *         [FA] آیا رویداد هنوز در جریان است؟ رویدادی که باز است هنوز مدتی
 *              ندارد و گفتنش از چاپ کردن صفر صادقانه‌تر است.
 * @param  uint8_t__flags [EN] event flags / [FA] فلگ‌های رویداد
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_EventStateText(uint8_t uint8_t__flags)
{
    return ((uint8_t__flags & UP_EVENT_FLAG_OPEN) != 0u) ? "در جریان" : "بسته";
}

/* ==================== Histogram bucket name ==================== */
/**
 * @brief  [EN] Persian name of a charge-duration bucket. The edges match
 *              func__UpHistory_HistogramBucket exactly; if one moves, both move.
 *         [FA] نام فارسی یک سبد مدت شارژ. مرزها دقیقاً همان مرزهای
 *              func__UpHistory_HistogramBucket هستند؛ اگر یکی جابه‌جا شود،
 *              هر دو جابه‌جا می‌شوند.
 * @param  uint8_t__bucket [EN] 0..UP_EVENT_HIST_BUCKETS-1 / [FA] شمارهٔ سبد
 * @return [EN] a Persian name / [FA] یک نام فارسی
 */
static const char *func__UpReport_BucketText(uint8_t uint8_t__bucket)
{
    static const char *const CHAR__A__Names[UP_EVENT_HIST_BUCKETS] =
    {
        "کمتر از ۱ دقیقه", "۱ تا ۲ دقیقه", "۲ تا ۵ دقیقه", "۵ تا ۱۰ دقیقه",
        "۱۰ تا ۱۵ دقیقه", "۱۵ تا ۲۰ دقیقه", "۲۰ تا ۳۰ دقیقه", "۳۰ تا ۴۵ دقیقه",
        "۴۵ تا ۶۰ دقیقه", "۱ تا ۱٫۵ ساعت", "۱٫۵ تا ۲ ساعت", "۲ تا ۳ ساعت",
        "۳ تا ۴ ساعت", "۴ تا ۶ ساعت", "۶ تا ۸ ساعت", "بیش از ۸ ساعت"
    };

    if (uint8_t__bucket >= (uint8_t)UP_EVENT_HIST_BUCKETS)
    {
        return CHAR__A__Names[UP_EVENT_HIST_BUCKETS - 1u];
    }
    return CHAR__A__Names[uint8_t__bucket];
}

/* ==================== Text helpers / کمکی‌های متن ==================== */
/* ==================== Hex text ==================== */
/**
 * @brief  [EN] A byte as "0x2A". The page and the report then talk about flags
 *              the same way, and a support call can be about one number.
 *         [FA] یک بایت به‌شکل «0x2A». این‌طور صفحه و گزارش دربارهٔ فلگ‌ها یک‌جور
 *              حرف می‌زنند و یک تماس پشتیبانی می‌تواند دربارهٔ یک عدد باشد.
 * @param  char__out [EN] destination, at least 5 bytes / [FA] مقصد
 * @param  uint8_t__value [EN] value / [FA] مقدار
 * @return [EN] bytes written / [FA] بایت‌های نوشته‌شده
 */
static uint16_t func__UpReport_HexText(char *char__out, uint8_t uint8_t__value)
{
    static const char CHAR__A__Hex[] = "0123456789ABCDEF";

    char__out[0] = '0';
    char__out[1] = 'x';
    char__out[2] = CHAR__A__Hex[(uint8_t__value >> 4) & 0x0Fu];
    char__out[3] = CHAR__A__Hex[uint8_t__value & 0x0Fu];
    char__out[4] = '\0';

    return 4u;
}

/* ==================== Local date text ==================== */
/**
 * @brief  [EN] Jalali date of an epoch second, in the panel's own timezone.
 *         [FA] تاریخ شمسی یک ثانیهٔ مطلق، در منطقهٔ زمانی خود پنل.
 * @param  uint32_t__epochS [EN] epoch seconds, 0 = unknown / [FA] ثانیهٔ مطلق، صفر = نامعلوم
 * @param  char__out [EN] destination, at least 12 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_DateText(uint32_t uint32_t__epochS, char *char__out)
{
    if (uint32_t__epochS == 0u)
    {
        char__out[0] = '-';
        char__out[1] = '\0';
        return;
    }
    (void)func__UpCal_DateText(uint32_t__epochS, func__UpState_TzOffsetS(), char__out);
}

/**
 * @brief  [EN] Local clock of an epoch second.
 *         [FA] ساعت محلی یک ثانیهٔ مطلق.
 * @param  uint32_t__epochS [EN] epoch seconds, 0 = unknown / [FA] ثانیهٔ مطلق، صفر = نامعلوم
 * @param  char__out [EN] destination, at least 10 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_ClockText(uint32_t uint32_t__epochS, char *char__out)
{
    if (uint32_t__epochS == 0u)
    {
        char__out[0] = '-';
        char__out[1] = '\0';
        return;
    }
    func__UpCal_TimeText(uint32_t__epochS, func__UpState_TzOffsetS(), char__out);
}

/**
 * @brief  [EN] Gregorian date of an epoch second - the column that lets the
 *              same file be read outside Iran.
 *         [FA] تاریخ میلادی یک ثانیهٔ مطلق - همان ستونی که اجازه می‌دهد همین
 *              فایل بیرون از ایران هم خوانده شود.
 * @param  uint32_t__epochS [EN] epoch seconds, 0 = unknown / [FA] ثانیهٔ مطلق، صفر = نامعلوم
 * @param  char__out [EN] destination, at least 12 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_GregText(uint32_t uint32_t__epochS, char *char__out)
{
    if (uint32_t__epochS == 0u)
    {
        char__out[0] = '-';
        char__out[1] = '\0';
        return;
    }
    func__UpCal_GregorianText(uint32_t__epochS, func__UpState_TzOffsetS(), char__out);
}

/* ==================== Day midnight ==================== */
/**
 * @brief  [EN] The epoch second of a LOCAL midnight: a day number times 86400,
 *              minus the panel's own offset. The day ring stores local days, so
 *              this is the inverse of how the ring counts.
 *         [FA] ثانیهٔ مطلقِ نیمه‌شب «محلی»: شمارهٔ روز ضرب در ۸۶۴۰۰ منهای اختلاف
 *              خود پنل. حلقهٔ روزها روزهای محلی را ذخیره می‌کند، پس این عکس
 *              همان روشی است که حلقه می‌شمارد.
 * @param  uint32_t__dayIndex [EN] days since 2000-01-01, local / [FA] روز از ۲۰۰۰-۰۱-۰۱ محلی
 * @return [EN] epoch seconds / [FA] ثانیهٔ مطلق
 */
static uint32_t func__UpReport_DayMidnight(uint32_t uint32_t__dayIndex)
{
    uint32_t uint32_t__midnight = uint32_t__dayIndex * UP_REPORT_SECONDS_PER_DAY;
    int32_t int32_t__tz = func__UpState_TzOffsetS();

    if (int32_t__tz > 0)
    {
        uint32_t__midnight -= (uint32_t)int32_t__tz;
    }
    else
    {
        uint32_t__midnight += (uint32_t)(-int32_t__tz);
    }

    return uint32_t__midnight;
}

/* ==================== Jalali date of a day number ==================== */
/**
 * @brief  [EN] Jalali date of a local day number.
 *         [FA] تاریخ شمسی یک شمارهٔ روز محلی.
 * @param  uint32_t__dayIndex [EN] day number / [FA] شمارهٔ روز
 * @param  char__out [EN] destination, at least 12 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_DayDateText(uint32_t uint32_t__dayIndex, char *char__out)
{
    (void)func__UpCal_DateText(func__UpReport_DayMidnight(uint32_t__dayIndex), 0, char__out);
}

/* ==================== Weekday of a day number ==================== */
/**
 * @brief  [EN] Persian weekday name of a local day number. 1970-01-01 was a
 *              Thursday, so day 0 lands on index 4 of a Sunday-first week.
 *         [FA] نام روز هفتهٔ یک شمارهٔ روز محلی. ۱۹۷۰-۰۱-۰۱ پنجشنبه بود، پس روز
 *              صفر روی خانهٔ ۴ هفته‌ای که با یکشنبه شروع می‌شود می‌افتد.
 * @param  uint32_t__dayIndex [EN] day number / [FA] شمارهٔ روز
 * @return [EN] a Persian weekday / [FA] یک روز هفتهٔ فارسی
 */
static const char *func__UpReport_WeekdayText(uint32_t uint32_t__dayIndex)
{
    static const char *const CHAR__A__Names[7] =
    {
        "یکشنبه", "دوشنبه", "سه‌شنبه", "چهارشنبه", "پنجشنبه", "جمعه", "شنبه"
    };

    uint32_t uint32_t__index = ((uint32_t__dayIndex + 4u) % 7u);

    return CHAR__A__Names[uint32_t__index];
}

/* ==================== Peak hour ==================== */
/**
 * @brief  [EN] The hour of the day that saw the most charges - the one column
 *              of the hour histogram worth keeping in a table, and the answer to
 *              "when does this machine actually work".
 *         [FA] ساعتی از شبانه‌روز که بیشترین شارژ را دیده - تنها ستونی از
 *              هیستوگرام ساعتی که ارزش نگه‌داشتن در یک جدول را دارد و پاسخ این
 *              پرسش که «این دستگاه واقعاً چه وقتی کار می‌کند».
 * @param  uint8_t__hours [EN] 24 counters / [FA] ۲۴ شمارنده
 * @return [EN] 0..23, or -1 when the day saw no charge / [FA] صفر تا ۲۳ یا -۱
 */
static int8_t func__UpReport_PeakHour(const uint8_t *uint8_t__hours)
{
    uint8_t uint8_t__best = 0u;
    int8_t int8_t__hour = -1;

    for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)UP_REPORT_HOURS_PER_DAY; uint8_t__i++)
    {
        if (uint8_t__hours[uint8_t__i] > uint8_t__best)
        {
            uint8_t__best = uint8_t__hours[uint8_t__i];
            int8_t__hour = (int8_t)uint8_t__i;
        }
    }

    return int8_t__hour;
}

/* ==================== Range / بازه ==================== */
/* ==================== Range set ==================== */
/**
 * @brief  [EN] Choose the report's range. "All" clears the filter; anything else
 *              becomes a monotonic cutoff, so every sheet asks the same question
 *              ("is this record newer than the cutoff?") and gets the same
 *              answer.
 *         [FA] انتخاب بازهٔ گزارش. «همه» فیلتر را پاک می‌کند؛ هر چیز دیگری به
 *              یک مرز یکنوا تبدیل می‌شود، پس هر برگه یک سؤال می‌پرسد («این
 *              رکورد از مرز جدیدتر است؟») و یک جواب می‌گیرد.
 * @param  uint32_t__days [EN] 1..UP_REPORT_MAX_DAYS or UP_REPORT_DAYS_ALL
 *                        [FA] ۱ تا سقف یا «همه»
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_RangeSet(uint32_t uint32_t__days)
{
    uint32_t uint32_t__nowAbs = func__UpHistory_AbsoluteS();

    UINT32_T__G__UpReportDays = (uint32_t__days > UP_REPORT_MAX_DAYS) ? UP_REPORT_MAX_DAYS : uint32_t__days;
    UINT32_T__G__UpReportTodayDay = func__UpState_DayIndex();

    if (UINT32_T__G__UpReportDays == UP_REPORT_DAYS_ALL)
    {
        UINT32_T__G__UpReportCutoffAbs = 0u;
        UINT32_T__G__UpReportFirstDay = 0u;
        return;
    }

    uint32_t uint32_t__backS = UINT32_T__G__UpReportDays * UP_REPORT_SECONDS_PER_DAY;
    UINT32_T__G__UpReportCutoffAbs = (uint32_t__nowAbs > uint32_t__backS) ? (uint32_t__nowAbs - uint32_t__backS) : 0u;

    if (UINT32_T__G__UpReportTodayDay >= (UINT32_T__G__UpReportDays - 1u))
    {
        UINT32_T__G__UpReportFirstDay = (UINT32_T__G__UpReportTodayDay - (UINT32_T__G__UpReportDays - 1u));
    }
    else
    {
        UINT32_T__G__UpReportFirstDay = 0u;
    }
}

/* ==================== In range ==================== */
/**
 * @brief  [EN] Is a monotonic second inside the chosen range?
 *         [FA] آیا یک ثانیهٔ یکنوا داخل بازهٔ انتخابی است؟
 * @param  uint32_t__absS [EN] record time / [FA] زمان رکورد
 * @return [EN] true when it belongs in the report / [FA] در صورت تعلق به گزارش true
 */
static bool func__UpReport_InRange(uint32_t uint32_t__absS)
{
    if (UINT32_T__G__UpReportDays == UP_REPORT_DAYS_ALL)
    {
        return true;
    }
    return (uint32_t__absS >= UINT32_T__G__UpReportCutoffAbs);
}

/* ==================== Range label ==================== */
/**
 * @brief  [EN] The range in words, for the summary sheet.
 *         [FA] بازه با کلمات، برای برگهٔ خلاصه.
 * @param  char__out [EN] destination, at least UP_REPORT_LABEL_MAX bytes
 *                    [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_RangeText(char *char__out)
{
    if (UINT32_T__G__UpReportDays == UP_REPORT_DAYS_ALL)
    {
        (void)snprintf(char__out, UP_REPORT_LABEL_MAX, "همهٔ تاریخچهٔ ذخیره‌شده");
        return;
    }
    (void)snprintf(char__out, UP_REPORT_LABEL_MAX, "%lu روز گذشته", (unsigned long)UINT32_T__G__UpReportDays);
}

/* ==================== Counting passes / گذرهای شمارش ==================== */
/* ==================== Count samples ==================== */
/**
 * @brief  [EN] How many samples fall in the range, and how many of them the
 *              report will have to skip because the sheet is capped. One pass
 *              over the ring before the summary sheet, because the summary is
 *              written first and has to state a number that is true.
 *         [FA] چند نمونه در بازه می‌افتد و گزارش باید از آن‌ها چند تا را رد کند
 *              چون برگه سقف دارد. یک گذر روی حلقه پیش از برگهٔ خلاصه، چون خلاصه
 *              اول نوشته می‌شود و باید عددی درست بگوید.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_CountSamples(void)
{
    up_ring_reader_t up_ring_reader_t__reader;

    UINT32_T__G__UpReportSamplesInRange = 0u;
    BOOL__G__UpReportSampleCapped = false;

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);

    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_sample_t up_sample_t__sample;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_sample_t__sample, (uint32_t)sizeof(up_sample_t)))
            {
                break;
            }
            if (func__UpReport_InRange(up_sample_t__sample.uint32_t__absS))
            {
                UINT32_T__G__UpReportSamplesInRange++;
            }
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    /* [EN] The sheet keeps the NEWEST rows: a report that drops the last hours
       of a three-month range would be answering the wrong question.
       [FA] برگه «جدیدترین» ردیف‌ها را نگه می‌دارد: گزارشی که ساعت‌های آخر یک
       بازهٔ سه‌ماهه را بیندازد، پاسخ سؤال اشتباهی را می‌دهد. */
    if (UINT32_T__G__UpReportSamplesInRange > UP_XLSX_SAMPLE_ROWS)
    {
        UINT32_T__G__UpReportSampleSkip = UINT32_T__G__UpReportSamplesInRange - UP_XLSX_SAMPLE_ROWS;
        BOOL__G__UpReportSampleCapped = true;
    }
    else
    {
        UINT32_T__G__UpReportSampleSkip = 0u;
    }
}

/* ==================== Count events ==================== */
/**
 * @brief  [EN] How many events fall in the range, plus the two counts the
 *              summary wants specifically: imbalance episodes and faults that
 *              appeared.
 *         [FA] چند رویداد در بازه می‌افتد، به‌علاوهٔ همان دو شمارشی که خلاصه
 *              به‌طور خاص می‌خواهد: رویدادهای عدم‌توازن و خطاهایی که ظاهر شدند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_CountEvents(void)
{
    up_ring_reader_t up_ring_reader_t__reader;

    UINT32_T__G__UpReportEventsInRange = 0u;
    UINT32_T__G__UpReportImbalanceInRange = 0u;
    UINT32_T__G__UpReportFaultsInRange = 0u;

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);

    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_EVENTS, (uint32_t)sizeof(up_event_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_event_t up_event_t__event;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_event_t__event, (uint32_t)sizeof(up_event_t)))
            {
                break;
            }
            if (!func__UpReport_InRange(up_event_t__event.uint32_t__absS))
            {
                continue;
            }

            UINT32_T__G__UpReportEventsInRange++;
            if (up_event_t__event.uint8_t__code == (uint8_t)UP_EV_IMBALANCE)
            {
                UINT32_T__G__UpReportImbalanceInRange++;
            }
            if (up_event_t__event.uint8_t__code == (uint8_t)UP_EV_FAULT_SET)
            {
                UINT32_T__G__UpReportFaultsInRange++;
            }
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }
}

/* ==================== Daily range totals ==================== */
/**
 * @brief  [EN] Add up the day rows inside the range. Today's row comes from RAM
 *              (it is the freshest copy - the flash copy is written every couple
 *              of minutes), every other day comes from the ring, and a day the
 *              panel was switched off simply has no row and adds nothing.
 *         [FA] جمع‌زدن ردیف‌های روز داخل بازه. ردیف امروز از رم می‌آید (تازه‌ترین
 *              نسخه است - نسخهٔ فلش هر چند دقیقه یک‌بار نوشته می‌شود)، بقیهٔ روزها
 *              از حلقه می‌آیند و روزی که پنل خاموش بوده ردیفی ندارد و چیزی اضافه
 *              نمی‌کند.
 * @param  up_report_totals_t__out [EN] destination, zeroed by the caller
 *                                 [FA] مقصد، توسط فراخوان صفر شده
 * @return [EN] true when at least one day was found / [FA] در صورت یافتن روز true
 */
static bool func__UpReport_DailyRangeTotals(up_report_totals_t *up_report_totals_t__out)
{
    uint32_t uint32_t__today = func__UpState_DayIndex();
    uint32_t uint32_t__first = UINT32_T__G__UpReportFirstDay;

    if (UINT32_T__G__UpReportDays != UP_REPORT_DAYS_ALL)
    {
        if (uint32_t__first > uint32_t__today)
        {
            return false;
        }
    }

    for (uint32_t uint32_t__day = uint32_t__first; uint32_t__day <= uint32_t__today; uint32_t__day++)
    {
        up_daily_t up_daily_t__row;

        if (uint32_t__day == uint32_t__today)
        {
            up_daily_t__row = *func__UpStore_DailyToday();
        }
        else if (!func__UpStore_DailyRead(uint32_t__day, &up_daily_t__row))
        {
            continue;
        }

        up_report_totals_t__out->uint32_t__days++;
        up_report_totals_t__out->uint32_t__charges += (uint32_t)up_daily_t__row.uint16_t__charges;
        up_report_totals_t__out->uint32_t__incomplete += (uint32_t)up_daily_t__row.uint16_t__incomplete;
        up_report_totals_t__out->uint32_t__chargeS += up_daily_t__row.uint32_t__chargeSeconds;
        up_report_totals_t__out->uint32_t__energyWh100 += up_daily_t__row.uint32_t__energyWh100;
        up_report_totals_t__out->uint32_t__outages += (uint32_t)up_daily_t__row.uint16_t__outages;
        up_report_totals_t__out->uint32_t__inputS += up_daily_t__row.uint32_t__inputSeconds;
        up_report_totals_t__out->uint32_t__runS += up_daily_t__row.uint32_t__runSeconds;
        up_report_totals_t__out->uint32_t__boots += (uint32_t)up_daily_t__row.uint16_t__boots;
    }

    up_report_totals_t__out->uint32_t__imbEvents = UINT32_T__G__UpReportImbalanceInRange;
    up_report_totals_t__out->uint32_t__faultSets = UINT32_T__G__UpReportFaultsInRange;

    return (up_report_totals_t__out->uint32_t__days > 0u);
}

/* ==================== Rows / ردیف‌ها ==================== */
/* ==================== Row begin ==================== */
/**
 * @brief  [EN] Start the next row of the sheet being written and keep the row
 *              number in one place, so no sheet ever has to count its own rows.
 *         [FA] آغاز ردیف بعدی برگهٔ در حال نوشتن و نگه‌داشتن شمارهٔ ردیف در یک
 *              جا، تا هیچ برگه‌ای مجبور نباشد ردیف‌هایش را خودش بشمارد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_RowBegin(void)
{
    func__UpXlsx_RowOpen(UINT32_T__G__UpReportRow);
}

/**
 * @brief  [EN] Close the row and move to the next one.
 *         [FA] بستن ردیف و رفتن به ردیف بعدی.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_RowEnd(void)
{
    func__UpXlsx_RowClose();
    UINT32_T__G__UpReportRow++;
}

/* ==================== Table header row ==================== */
/**
 * @brief  [EN] The blue header row of a table: the frozen row every sheet is
 *              read from.
 *         [FA] ردیف سرصفحهٔ آبی یک جدول: همان ردیف ثابتی که خواندن هر برگه از
 *              آن شروع می‌شود.
 * @param  char__names [EN] one heading per column / [FA] یک سرصفحه برای هر ستون
 * @param  uint16_t__cols [EN] column count / [FA] تعداد ستون
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_HeaderRow(const char *const char__names[], uint16_t uint16_t__cols)
{
    func__UpReport_RowBegin();
    for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__cols; uint16_t__i++)
    {
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, uint16_t__i, UP_XLSX_STYLE_HEADER, char__names[uint16_t__i]);
    }
    func__UpReport_RowEnd();
}

/* ==================== Section label row ==================== */
/**
 * @brief  [EN] A section heading inside the summary: one label, nothing else, so
 *              the eye can find "storage" without reading the whole sheet.
 *         [FA] عنوان یک بخش داخل خلاصه: یک برچسب و نه چیز دیگر، تا چشم بدون
 *              خواندن کل برگه «حافظه» را پیدا کند.
 * @param  char__text [EN] heading / [FA] عنوان
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_LabelRow(const char *char__text)
{
    func__UpReport_RowBegin();
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_LABEL, UP_XLSX_STYLE_LABEL, char__text);
    func__UpReport_RowEnd();
}

/* ==================== Text row ==================== */
/**
 * @brief  [EN] "thing / value" with the value as text.
 *         [FA] «موضوع / مقدار» با مقدار متنی.
 * @param  char__label [EN] what it is / [FA] چه چیزی
 * @param  char__value [EN] the value / [FA] مقدار
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_TextRow(const char *char__label, const char *char__value)
{
    func__UpReport_RowBegin();
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_LABEL, UP_XLSX_STYLE_TEXT, char__label);
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_VALUE, UP_XLSX_STYLE_TEXT, char__value);
    func__UpReport_RowEnd();
}

/* ==================== Number row ==================== */
/**
 * @brief  [EN] "thing / number / unit": three columns, because a number without
 *              its unit is a number nobody can act on.
 *         [FA] «موضوع / عدد / یکا»: سه ستون، چون عدد بی‌یکا عددی است که کسی
 *              نمی‌تواند بر اساسش کاری بکند.
 * @param  char__label [EN] what it is / [FA] چه چیزی
 * @param  int32_t__value [EN] the value / [FA] مقدار
 * @param  char__unit [EN] its unit / [FA] یکایش
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_NumRow(const char *char__label, int32_t int32_t__value, const char *char__unit)
{
    func__UpReport_RowBegin();
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_LABEL, UP_XLSX_STYLE_TEXT, char__label);
    func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_SUM_VALUE, UP_XLSX_STYLE_NUMBER, int32_t__value);
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_UNIT, UP_XLSX_STYLE_TEXT, char__unit);
    func__UpReport_RowEnd();
}

/* ==================== Decimal row ==================== */
/**
 * @brief  [EN] "thing / decimal number / unit" from a value the panel keeps
 *              scaled (millivolts, hundredths of a watt-hour).
 *         [FA] «موضوع / عدد اعشاری / یکا» از مقداری که پنل مقیاس‌شده نگه می‌دارد
 *              (میلی‌ولت، صدم وات‌ساعت).
 * @param  char__label [EN] what it is / [FA] چه چیزی
 * @param  int32_t__scaled [EN] value times 10^decimals / [FA] مقدار ضرب‌شده در ۱۰^اعشار
 * @param  uint8_t__decimals [EN] decimals to show / [FA] تعداد اعشار
 * @param  char__unit [EN] its unit / [FA] یکایش
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_FixedRow(const char *char__label, int32_t int32_t__scaled, uint8_t uint8_t__decimals,
                                    const char *char__unit)
{
    func__UpReport_RowBegin();
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_LABEL, UP_XLSX_STYLE_TEXT, char__label);
    func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_SUM_VALUE, UP_XLSX_STYLE_NUMBER,
                           int32_t__scaled, uint8_t__decimals);
    func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SUM_UNIT, UP_XLSX_STYLE_TEXT, char__unit);
    func__UpReport_RowEnd();
}

/* ==================== Scaled helpers / کمکی‌های مقیاس ==================== */
/**
 * @brief  [EN] Seconds as tenths of an hour, for the hour columns: 5400 s comes
 *              out as 15.0 hours.
 *         [FA] ثانیه به‌صورت دهم ساعت، برای ستون‌های ساعتی: ۵۴۰۰ ثانیه می‌شود
 *              15.0 ساعت.
 * @param  uint32_t__seconds [EN] seconds / [FA] ثانیه
 * @return [EN] tenths of an hour / [FA] دهم ساعت
 */
static int32_t func__UpReport_HourTenths(uint32_t uint32_t__seconds)
{
    return (int32_t)((uint32_t__seconds * 10u) / 3600u);
}

/**
 * @brief  [EN] Seconds as a whole number of minutes, rounded down: a report line
 *              about a charge that lasted 92 minutes is more use than 5500
 *              seconds.
 *         [FA] ثانیه به‌صورت دقیقهٔ کامل و رُندشده به پایین: خط گزارشی دربارهٔ
 *              شارژی که ۹۲ دقیقه طول کشیده از ۵۵۰۰ ثانیه به‌کارتر است.
 * @param  uint32_t__seconds [EN] seconds / [FA] ثانیه
 * @return [EN] minutes / [FA] دقیقه
 */
static int32_t func__UpReport_Minutes(uint32_t uint32_t__seconds)
{
    return (int32_t)(uint32_t__seconds / 60u);
}

/**
 * @brief  [EN] Bytes as tenths of a kilobyte, for the storage rows.
 *         [FA] بایت به‌صورت دهم کیلوبایت، برای ردیف‌های حافظه.
 * @param  uint32_t__bytes [EN] bytes / [FA] بایت
 * @return [EN] tenths of a KiB / [FA] دهم کیلوبایت
 */
static int32_t func__UpReport_KiloTenths(uint32_t uint32_t__bytes)
{
    return (int32_t)((uint32_t__bytes * 10u) / UP_REPORT_BYTES_PER_KB);
}

/* ==================== Sheet one: summary / برگهٔ یک: خلاصه ==================== */
/* ==================== Identity block ==================== */
/**
 * @brief  [EN] Who, when and with which clock: the four lines that make the rest
 *              of the file interpretable months later.
 *         [FA] کی، کِی و با کدام ساعت: همان چهار خطی که ماه‌ها بعد بقیهٔ فایل را
 *              قابل تفسیر می‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_IdentityBlock(void)
{
    static const char CHAR__A__Labels[][UP_REPORT_LABEL_MAX] =
    {
        "دستگاه",
        "بازهٔ گزارش",
        "زمان تولید گزارش (شمسی)",
        "زمان تولید گزارش (میلادی)",
        "منطقهٔ زمانی پنل (ساعت)",
        "ساعت پنل تنظیم شده است؟",
        "وضعیت ارتباط با برد"
    };
    char char__device[UP_REPORT_NAME_MAX];
    char char__range[UP_REPORT_LABEL_MAX];
    char char__date[UP_REPORT_TEXT_MAX];
    char char__clock[UP_REPORT_TEXT_MAX];
    char char__greg[UP_REPORT_TEXT_MAX];
    char char__both[UP_REPORT_TEXT_MAX + UP_REPORT_TEXT_MAX];
    char char__link[UP_REPORT_LABEL_MAX];
    uint32_t uint32_t__now = func__UpState_NowEpochS();
    int32_t int32_t__tzMinutes = func__UpState_TzOffsetS() / 60;

    func__UpReport_LabelRow("مشخصات گزارش");

    (void)snprintf(char__device, sizeof(char__device), "%s %s", UP_PANEL_NAME, UP_PANEL_VERSION);
    func__UpReport_TextRow(CHAR__A__Labels[0], char__device);

    func__UpReport_RangeText(char__range);
    func__UpReport_TextRow(CHAR__A__Labels[1], char__range);

    /* [EN] An unset clock is said once, in words: "-" and "- -" are what a
       machine writes when it has nothing to say, and this row is the first thing
       the reader looks at. The admin sets the clock by hand, so this line is the
       report telling them it has not happened yet.
       [FA] ساعت تنظیم‌نشده یک‌بار و با کلمه گفته می‌شود: «-» و «- -» همانی است که
       ماشین وقتی حرفی ندارد می‌نویسد، و این ردیف اولین چیزی است که خواننده
       می‌بیند. ساعت را مدیر دستی می‌گذارد، پس این خط گزارش می‌گوید هنوز این کار
       انجام نشده است. */
    func__UpReport_DateText(uint32_t__now, char__date);
    func__UpReport_ClockText(uint32_t__now, char__clock);
    func__UpReport_GregText(uint32_t__now, char__greg);

    if ((uint32_t__now == 0u) || (char__clock[0] == '-'))
    {
        func__UpReport_TextRow(CHAR__A__Labels[2], "ساعت پنل تنظیم نشده است");
        func__UpReport_TextRow(CHAR__A__Labels[3], "ساعت پنل تنظیم نشده است");
    }
    else
    {
        (void)snprintf(char__both, sizeof(char__both), "%s %s", char__date, char__clock);
        func__UpReport_TextRow(CHAR__A__Labels[2], char__both);

        (void)snprintf(char__both, sizeof(char__both), "%s %s", char__greg, char__clock);
        func__UpReport_TextRow(CHAR__A__Labels[3], char__both);
    }

    /* [EN] The offset is shown the way a human writes it: +3:30, not 210.
       [FA] اختلاف به همان شکلی که آدم می‌نویسد دیده می‌شود: +3:30 و نه ۲۱۰. */
    (void)snprintf(char__clock, sizeof(char__clock), "%c%ld:%02lu",
                   (int32_t__tzMinutes < 0) ? '-' : '+',
                   (long)((int32_t__tzMinutes < 0) ? (-int32_t__tzMinutes) : int32_t__tzMinutes) / 60L,
                   (unsigned long)((long)((int32_t__tzMinutes < 0) ? (-int32_t__tzMinutes) : int32_t__tzMinutes) % 60L));
    func__UpReport_TextRow(CHAR__A__Labels[4], char__clock);

    func__UpReport_TextRow(CHAR__A__Labels[5], (uint32_t__now != 0u) ? "بله" : "خیر — تاریخ رویدادها معتبر نیست");

    if (func__UpState_LinkOnline())
    {
        func__UpReport_TextRow(CHAR__A__Labels[6], "متصل");
    }
    else
    {
        (void)snprintf(char__link, sizeof(char__link), "قطع (%lu ثانیه)",
                       (unsigned long)(func__UpState_AgeMs() / 1000u));
        func__UpReport_TextRow(CHAR__A__Labels[6], char__link);
    }

    func__UpReport_FixedRow("مدت کارکرد پنل", func__UpReport_HourTenths(func__UpState_UptimeS()), 1u, "ساعت");
}

/* ==================== Storage block ==================== */
/**
 * @brief  [EN] What is on the flash and how full it is. The number that matters
 *              to an operator is "how many days are still here", because that is
 *              what a report can still answer.
 *         [FA] چه چیزی روی فلش هست و چقدر پر است. عددی که برای اپراتور مهم است
 *              «چند روز هنوز اینجاست» است، چون گزارشی که هنوز می‌تواند جواب دهد
 *              به همان بستگی دارد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_StorageBlock(void)
{
    uint32_t uint32_t__used = func__UpStore_UsedBytes();
    uint32_t uint32_t__total = func__UpStore_TotalBytes();
    uint32_t uint32_t__percent = (uint32_t__total > 0u) ? ((uint32_t__used * 100u) / uint32_t__total) : 0u;
    uint32_t uint32_t__days = func__UpStore_FileCount(UP_F_DAILY, (uint32_t)sizeof(up_daily_t));

    func__UpReport_LabelRow("حافظهٔ پنل");
    func__UpReport_FixedRow("کل حجم حافظه", func__UpReport_KiloTenths(uint32_t__total), 1u, "کیلوبایت");
    func__UpReport_FixedRow("مصرف‌شده", func__UpReport_KiloTenths(uint32_t__used), 1u, "کیلوبایت");
    func__UpReport_NumRow("درصد پرشدگی", (int32_t)uint32_t__percent, "درصد");
    func__UpReport_NumRow("نمونه‌های ذخیره‌شده", (int32_t)func__UpStore_SampleCount(), "رکورد");
    func__UpReport_NumRow("رویدادهای ذخیره‌شده", (int32_t)func__UpStore_EventCount(), "رکورد");
    func__UpReport_NumRow("روزهای ذخیره‌شده", (int32_t)uint32_t__days, "روز");
    func__UpReport_NumRow("دفعات پاک‌کردن خودکار", (int32_t)func__UpStore_PurgeCount(), "بار");

    if (BOOL__G__UpReportSampleCapped)
    {
        func__UpReport_NumRow("نمونه‌های نوشته‌شده در این گزارش", (int32_t)UP_XLSX_SAMPLE_ROWS, "جدیدترین رکورد");
        func__UpReport_NumRow("نمونه‌های بازه", (int32_t)UINT32_T__G__UpReportSamplesInRange, "رکورد");
    }
    else
    {
        func__UpReport_NumRow("نمونه‌های بازه", (int32_t)UINT32_T__G__UpReportSamplesInRange, "رکورد");
    }
}

/* ==================== Range block ==================== */
/**
 * @brief  [EN] What happened INSIDE the chosen range, added up from the day
 *              ring.
 *         [FA] در بازهٔ انتخابی چه اتفاقی افتاد، جمع‌زده‌شده از حلقهٔ روزها.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_RangeBlock(void)
{
    up_report_totals_t up_report_totals_t__range;

    (void)memset(&up_report_totals_t__range, 0, sizeof(up_report_totals_t__range));
    (void)func__UpReport_DailyRangeTotals(&up_report_totals_t__range);

    func__UpReport_LabelRow("خلاصهٔ بازه");
    func__UpReport_NumRow("روزهای دارای داده", (int32_t)up_report_totals_t__range.uint32_t__days, "روز");
    func__UpReport_NumRow("شارژ کامل", (int32_t)up_report_totals_t__range.uint32_t__charges, "بار");
    func__UpReport_NumRow("شارژ ناتمام", (int32_t)up_report_totals_t__range.uint32_t__incomplete, "بار");
    func__UpReport_FixedRow("مجموع زمان شارژ", func__UpReport_HourTenths(up_report_totals_t__range.uint32_t__chargeS),
                            1u, "ساعت");
    func__UpReport_FixedRow("انرژی شارژشده", (int32_t)up_report_totals_t__range.uint32_t__energyWh100,
                            UP_REPORT_ENERGY_DECIMALS, "وات‌ساعت");
    func__UpReport_NumRow("قطع ورودی", (int32_t)up_report_totals_t__range.uint32_t__outages, "بار");
    func__UpReport_FixedRow("زمان روی ورودی", func__UpReport_HourTenths(up_report_totals_t__range.uint32_t__inputS),
                            1u, "ساعت");
    /* [EN] Time on battery IS time without input: the load only runs on the
       battery when the 24 V input is gone, so one row covers both questions.
       [FA] زمان روی باتری همان زمان بی‌ورودی است: بار فقط وقتی روی باتری کار
       می‌کند که ورودی ۲۴ ولت رفته باشد، پس یک ردیف هر دو پرسش را جواب می‌دهد. */
    func__UpReport_FixedRow("کار روی باتری (بی‌ورودی)", func__UpReport_HourTenths(up_report_totals_t__range.uint32_t__runS),
                            1u, "ساعت");
    func__UpReport_NumRow("ری‌استارت برد", (int32_t)up_report_totals_t__range.uint32_t__boots, "بار");
    func__UpReport_NumRow("رویداد عدم‌توازن", (int32_t)up_report_totals_t__range.uint32_t__imbEvents, "بار");
    func__UpReport_NumRow("خطاهای فعال‌شده", (int32_t)up_report_totals_t__range.uint32_t__faultSets, "بار");
    func__UpReport_NumRow("رویدادهای ثبت‌شده در بازه", (int32_t)UINT32_T__G__UpReportEventsInRange, "رکورد");
}

/* ==================== Lifetime block ==================== */
/**
 * @brief  [EN] Since the first boot: the totals the panel keeps even after the
 *              raw records that produced them have been pushed out of the ring.
 *         [FA] از اولین بوت تا کنون: جمع‌هایی که پنل حتی بعد از بیرون‌رفتن
 *              رکوردهای خامی که آن‌ها را ساخته‌اند نگه می‌دارد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_LifetimeBlock(void)
{
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    uint32_t uint32_t__avgS = (up_totals_t__totals->uint32_t__charges > 0u)
        ? (up_totals_t__totals->uint32_t__sumChargeS / up_totals_t__totals->uint32_t__charges)
        : 0u;

    func__UpReport_LabelRow("جمع کل از ابتدا");
    func__UpReport_NumRow("شارژ کامل", (int32_t)up_totals_t__totals->uint32_t__charges, "بار");
    func__UpReport_NumRow("شارژ ناتمام", (int32_t)up_totals_t__totals->uint32_t__incomplete, "بار");
    func__UpReport_FixedRow("مجموع زمان شارژ", func__UpReport_HourTenths(up_totals_t__totals->uint32_t__sumChargeS), 1u, "ساعت");
    func__UpReport_FixedRow("میانگین مدت شارژ", func__UpReport_Minutes(uint32_t__avgS), 0u, "دقیقه");
    func__UpReport_FixedRow("کوتاه‌ترین شارژ", func__UpReport_Minutes(up_totals_t__totals->uint32_t__minChargeS), 0u, "دقیقه");
    func__UpReport_FixedRow("بلندترین شارژ", func__UpReport_Minutes(up_totals_t__totals->uint32_t__maxChargeS), 0u, "دقیقه");
    func__UpReport_FixedRow("انرژی شارژشده", (int32_t)up_totals_t__totals->uint32_t__chargeWh100,
                            UP_REPORT_ENERGY_DECIMALS, "وات‌ساعت");
    func__UpReport_FixedRow("انرژی مصرف‌شده", (int32_t)up_totals_t__totals->uint32_t__runWh100,
                            UP_REPORT_ENERGY_DECIMALS, "وات‌ساعت");
    func__UpReport_NumRow("قطع ورودی", (int32_t)up_totals_t__totals->uint32_t__outages, "بار");
    func__UpReport_FixedRow("مجموع زمان بی‌ورودی", func__UpReport_HourTenths(up_totals_t__totals->uint32_t__outageS), 1u, "ساعت");
    func__UpReport_FixedRow("مجموع زمان روی ورودی", func__UpReport_HourTenths(up_totals_t__totals->uint32_t__inputS), 1u, "ساعت");
    func__UpReport_FixedRow("مجموع کار روی باتری", func__UpReport_HourTenths(up_totals_t__totals->uint32_t__runS), 1u, "ساعت");
    func__UpReport_FixedRow("بلندترین باری", func__UpReport_Minutes(up_totals_t__totals->uint32_t__maxRunS), 0u, "دقیقه");
    func__UpReport_NumRow("ری‌استارت برد", (int32_t)up_totals_t__totals->uint32_t__boots, "بار");
    func__UpReport_NumRow("رویدادهای عدم‌توازن", (int32_t)up_totals_t__totals->uint32_t__imbEvents, "بار");
    func__UpReport_FixedRow("اوج جریان کانال بالایی", (int32_t)up_totals_t__totals->uint32_t__peakI1,
                            UP_REPORT_UNIT_DECIMALS, "آمپر");
    func__UpReport_FixedRow("اوج جریان کانال پایینی", (int32_t)up_totals_t__totals->uint32_t__peakI2,
                            UP_REPORT_UNIT_DECIMALS, "آمپر");
    func__UpReport_NumRow("اوج duty کانال بالایی", (int32_t)up_totals_t__totals->uint32_t__peakDuty1, "پرمیل");
    func__UpReport_NumRow("اوج duty کانال پایینی", (int32_t)up_totals_t__totals->uint32_t__peakDuty2, "پرمیل");
    func__UpReport_FixedRow("بیشترین ولتاژ پک", (int32_t)up_totals_t__totals->uint16_t__maxV,
                            UP_REPORT_UNIT_DECIMALS, "ولت");
    func__UpReport_FixedRow("کمترین ولتاژ پک", (int32_t)up_totals_t__totals->uint16_t__minV,
                            UP_REPORT_UNIT_DECIMALS, "ولت");
    func__UpReport_FixedRow("زمان ثبت‌شده در کل", func__UpReport_HourTenths(up_totals_t__totals->uint32_t__coverageS), 1u, "ساعت");
}

/* ==================== Histogram block ==================== */
/**
 * @brief  [EN] How long this machine's charges usually last. The lifetime
 *              histogram, one line per bucket - the shape of the machine's work
 *              in sixteen lines.
 *         [FA] شارژهای این دستگاه معمولاً چقدر طول می‌کشند. هیستوگرام کل عمر،
 *              هر سبد یک خط - شکلِ کارِ دستگاه در شانزده خط.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpReport_HistogramBlock(void)
{
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();

    func__UpReport_LabelRow("توزیع مدت شارژها (کل عمر)");
    for (uint8_t uint8_t__i = 0u; uint8_t__i < (uint8_t)UP_EVENT_HIST_BUCKETS; uint8_t__i++)
    {
        func__UpReport_NumRow(func__UpReport_BucketText(uint8_t__i),
                              (int32_t)up_totals_t__totals->uint16_t__hist[uint8_t__i], "بار");
    }
}

/* ==================== Sheet one ==================== */
/**
 * @brief  [EN] Write the summary sheet: identity, storage, the chosen range, the
 *              lifetime totals and the duration histogram.
 *         [FA] نوشتن برگهٔ خلاصه: مشخصات، حافظه، بازهٔ انتخابی، جمع‌های کل عمر و
 *              هیستوگرام مدت‌ها.
 * @return [EN] true when the sheet was written / [FA] در صورت نوشتن برگه true
 */
static bool func__UpReport_SheetSummary(void)
{
    static const char *const CHAR__A__Heads[UP_REPORT_SUM_COLS] = { "شرح", "مقدار", "یکا" };
    static const uint8_t UINT8_T__A__Widths[UP_REPORT_SUM_COLS] = { 34u, 22u, 12u };

    if (!func__UpXlsx_SheetMember(1u))
    {
        return false;
    }

    UINT32_T__G__UpReportRow = 1u;
    func__UpXlsx_SheetOpen(2u);
    func__UpXlsx_Cols(UINT8_T__A__Widths, (uint16_t)UP_REPORT_SUM_COLS);
    func__UpXlsx_DataOpen();

    func__UpReport_RowBegin();
    func__UpXlsx_CellText(1u, UP_REPORT_SUM_LABEL, UP_XLSX_STYLE_TITLE, "گزارش پنل کاربر ChangeOver");
    func__UpReport_RowEnd();
    func__UpReport_HeaderRow(CHAR__A__Heads, (uint16_t)UP_REPORT_SUM_COLS);
    func__UpReport_IdentityBlock();
    func__UpReport_StorageBlock();
    func__UpReport_RangeBlock();
    func__UpReport_LifetimeBlock();
    func__UpReport_HistogramBlock();
    func__UpXlsx_SheetClose((uint16_t)UP_REPORT_SUM_COLS, 2u);

    return func__UpXlsx_End();
}

/* ==================== Sheet two: daily / برگهٔ دو: روزانه ==================== */
/**
 * @brief  [EN] One line per local day: charges, energy, battery time, outages,
 *              restarts and the busiest charging hour. Days the panel was off
 *              are simply absent, which is honest - an empty day would look like
 *              a day with no activity.
 *         [FA] هر روز محلی یک خط: شارژها، انرژی، زمان باتری، قطع ورودی،
 *              ری‌استارت‌ها و پرکارترین ساعت شارژ. روزهایی که پنل خاموش بوده
 *              غایب‌اند و همین صادقانه است - روز خالی به‌نظر روزی می‌آید که
 *              فعالیتی نداشته.
 * @return [EN] true when the sheet was written / [FA] در صورت نوشتن برگه true
 */
static bool func__UpReport_SheetDaily(void)
{
    static const char *const CHAR__A__Heads[UP_REPORT_DAY_COLS] =
    {
        "تاریخ شمسی", "روز هفته", "شارژ کامل", "شارژ ناتمام", "زمان شارژ (ساعت)",
        "انرژی (وات‌ساعت)", "کار روی باتری (ساعت)", "زمان روی ورودی (ساعت)",
        "قطع ورودی", "ری‌استارت", "بیشترین ولتاژ پک (ولت)", "کمترین ولتاژ پک (ولت)",
        "ساعت اوج شارژ"
    };
    static const uint8_t UINT8_T__A__Widths[UP_REPORT_DAY_COLS] =
    {
        12u, 11u, 10u, 11u, 15u, 15u, 19u, 19u, 10u, 10u, 22u, 22u, 15u
    };
    uint32_t uint32_t__today = func__UpState_DayIndex();
    uint32_t uint32_t__first = UINT32_T__G__UpReportFirstDay;

    if (!func__UpXlsx_SheetMember(2u))
    {
        return false;
    }

    UINT32_T__G__UpReportRow = 1u;
    func__UpXlsx_SheetOpen(1u);
    func__UpXlsx_Cols(UINT8_T__A__Widths, (uint16_t)UP_REPORT_DAY_COLS);
    func__UpXlsx_DataOpen();
    func__UpReport_HeaderRow(CHAR__A__Heads, (uint16_t)UP_REPORT_DAY_COLS);

    for (uint32_t uint32_t__day = uint32_t__first; uint32_t__day <= uint32_t__today; uint32_t__day++)
    {
        up_daily_t up_daily_t__row;
        char char__date[UP_REPORT_TEXT_MAX];
        int8_t int8_t__peak;

        if (uint32_t__day == uint32_t__today)
        {
            up_daily_t__row = *func__UpStore_DailyToday();
        }
        else if (!func__UpStore_DailyRead(uint32_t__day, &up_daily_t__row))
        {
            continue;
        }

        func__UpReport_DayDateText(uint32_t__day, char__date);

        func__UpReport_RowBegin();
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_DAY_DATE, UP_XLSX_STYLE_TEXT, char__date);
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_DAY_WEEKDAY, UP_XLSX_STYLE_TEXT,
                              func__UpReport_WeekdayText(uint32_t__day));
        func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_DAY_DONE, UP_XLSX_STYLE_NUMBER,
                             (int32_t)up_daily_t__row.uint16_t__charges);
        func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_DAY_PART, UP_XLSX_STYLE_NUMBER,
                             (int32_t)up_daily_t__row.uint16_t__incomplete);
        func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_DAY_CHARGE_MIN, UP_XLSX_STYLE_NUMBER,
                               func__UpReport_HourTenths(up_daily_t__row.uint32_t__chargeSeconds), 1u);
        func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_DAY_ENERGY_WH, UP_XLSX_STYLE_NUMBER,
                               (int32_t)up_daily_t__row.uint32_t__energyWh100, UP_REPORT_ENERGY_DECIMALS);
        func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_DAY_RUN_MIN, UP_XLSX_STYLE_NUMBER,
                               func__UpReport_HourTenths(up_daily_t__row.uint32_t__runSeconds), 1u);
        func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_DAY_INPUT_MIN, UP_XLSX_STYLE_NUMBER,
                               func__UpReport_HourTenths(up_daily_t__row.uint32_t__inputSeconds), 1u);
        func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_DAY_OUTAGES, UP_XLSX_STYLE_NUMBER,
                             (int32_t)up_daily_t__row.uint16_t__outages);
        func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_DAY_BOOTS, UP_XLSX_STYLE_NUMBER,
                             (int32_t)up_daily_t__row.uint16_t__boots);
        func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_DAY_MAX_V, UP_XLSX_STYLE_NUMBER,
                               (int32_t)up_daily_t__row.uint16_t__maxV, UP_REPORT_UNIT_DECIMALS);
        func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_DAY_MIN_V, UP_XLSX_STYLE_NUMBER,
                               (int32_t)up_daily_t__row.uint16_t__minV, UP_REPORT_UNIT_DECIMALS);

        int8_t__peak = func__UpReport_PeakHour(up_daily_t__row.uint8_t__hourCharges);
        if (int8_t__peak < 0)
        {
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_DAY_PEAK_HOUR, UP_XLSX_STYLE_TEXT, "بدون شارژ");
        }
        else
        {
            (void)snprintf(char__date, sizeof(char__date), "ساعت %ld", (long)int8_t__peak);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_DAY_PEAK_HOUR, UP_XLSX_STYLE_TEXT, char__date);
        }
        func__UpReport_RowEnd();
    }

    func__UpXlsx_SheetClose((uint16_t)UP_REPORT_DAY_COLS, 1u);

    return func__UpXlsx_End();
}

/* ==================== Sheet three: charges / برگهٔ سه: شارژها ==================== */
/**
 * @brief  [EN] One line per charge session: when it started, which channel, how
 *              it ended, how long it lasted and how much energy went in. This is
 *              the sheet a maintenance engineer actually reads.
 *         [FA] هر نشست شارژ یک خط: کِی شروع شد، کدام کانال، چطور تمام شد، چقدر
 *              طول کشید و چقدر انرژی رفت. این همان برگه‌ای است که مهندس تعمیرات
 *              واقعاً می‌خواند.
 * @return [EN] true when the sheet was written / [FA] در صورت نوشتن برگه true
 */
static bool func__UpReport_SheetCharges(void)
{
    static const char *const CHAR__A__Heads[UP_REPORT_CHG_COLS] =
    {
        "تاریخ شمسی", "ساعت", "کانال", "نتیجه", "مدت (دقیقه)", "مدت (ثانیه)", "انرژی (وات‌ساعت)"
    };
    static const uint8_t UINT8_T__A__Widths[UP_REPORT_CHG_COLS] =
    {
        12u, 10u, 14u, 18u, 13u, 13u, 16u
    };
    up_ring_reader_t up_ring_reader_t__reader;
    uint32_t uint32_t__rows = 0u;

    if (!func__UpXlsx_SheetMember(3u))
    {
        return false;
    }

    UINT32_T__G__UpReportRow = 1u;
    func__UpXlsx_SheetOpen(1u);
    func__UpXlsx_Cols(UINT8_T__A__Widths, (uint16_t)UP_REPORT_CHG_COLS);
    func__UpXlsx_DataOpen();
    func__UpReport_HeaderRow(CHAR__A__Heads, (uint16_t)UP_REPORT_CHG_COLS);

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_EVENTS, (uint32_t)sizeof(up_event_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_event_t up_event_t__event;
            char char__date[UP_REPORT_TEXT_MAX];
            char char__clock[UP_REPORT_TEXT_MAX];

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_event_t__event, (uint32_t)sizeof(up_event_t)))
            {
                break;
            }
            if (!func__UpReport_InRange(up_event_t__event.uint32_t__absS))
            {
                continue;
            }
            if ((up_event_t__event.uint8_t__code != (uint8_t)UP_EV_CHARGE_START) &&
                (up_event_t__event.uint8_t__code != (uint8_t)UP_EV_CHARGE_DONE) &&
                (up_event_t__event.uint8_t__code != (uint8_t)UP_EV_CHARGE_INCOMPLETE))
            {
                continue;
            }

            func__UpReport_DateText(up_event_t__event.uint32_t__epoch, char__date);
            func__UpReport_ClockText(up_event_t__event.uint32_t__epoch, char__clock);

            func__UpReport_RowBegin();
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_CHG_DATE, UP_XLSX_STYLE_TEXT, char__date);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_CHG_CLOCK, UP_XLSX_STYLE_TEXT, char__clock);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_CHG_CHANNEL, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_ChannelText(up_event_t__event.uint8_t__channel));
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_CHG_RESULT, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_EventText(up_event_t__event.uint8_t__code));
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_CHG_MINUTES, UP_XLSX_STYLE_NUMBER,
                                 func__UpReport_Minutes(up_event_t__event.uint32_t__durS));
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_CHG_SECONDS, UP_XLSX_STYLE_NUMBER,
                                 (int32_t)up_event_t__event.uint32_t__durS);
            /* [EN] valueB carries the energy of a charge in 0.01 Wh.
               [FA] valueB انرژی شارژ را در یکای صدم وات‌ساعت دارد. */
            func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_CHG_ENERGY_WH, UP_XLSX_STYLE_NUMBER,
                                   (int32_t)up_event_t__event.uint16_t__valueB, UP_REPORT_ENERGY_DECIMALS);
            func__UpReport_RowEnd();
            uint32_t__rows++;
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    if (uint32_t__rows == 0u)
    {
        func__UpReport_RowBegin();
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_CHG_DATE, UP_XLSX_STYLE_TEXT,
                              "در این بازه شارژی ثبت نشده است");
        func__UpReport_RowEnd();
    }

    func__UpXlsx_SheetClose((uint16_t)UP_REPORT_CHG_COLS, 1u);

    return func__UpXlsx_End();
}

/* ==================== Sheet four: events / برگهٔ چهار: رویدادها ==================== */
/**
 * @brief  [EN] Everything else that happened: input cuts and returns, faults
 *              appearing and clearing, board restarts, admin cuts, imbalance
 *              episodes and manual-mode sightings.
 *         [FA] هر چیز دیگری که رخ داد: قطع و بازگشت ورودی، آمدن و رفتن خطاها،
 *              ری‌استارت برد، قطع‌های ادمین، رویدادهای عدم‌توازن و دیده‌شدن مود
 *              دستی.
 * @return [EN] true when the sheet was written / [FA] در صورت نوشتن برگه true
 */
static bool func__UpReport_SheetEvents(void)
{
    static const char *const CHAR__A__Heads[UP_REPORT_EV_COLS] =
    {
        "تاریخ شمسی", "ساعت", "کد", "رویداد", "کانال", "شدت", "مدت (ثانیه)", "مقدار ۱", "مقدار ۲", "وضعیت"
    };
    static const uint8_t UINT8_T__A__Widths[UP_REPORT_EV_COLS] =
    {
        12u, 10u, 7u, 26u, 14u, 10u, 14u, 26u, 12u, 11u
    };
    up_ring_reader_t up_ring_reader_t__reader;
    uint32_t uint32_t__rows = 0u;

    if (!func__UpXlsx_SheetMember(4u))
    {
        return false;
    }

    UINT32_T__G__UpReportRow = 1u;
    func__UpXlsx_SheetOpen(1u);
    func__UpXlsx_Cols(UINT8_T__A__Widths, (uint16_t)UP_REPORT_EV_COLS);
    func__UpXlsx_DataOpen();
    func__UpReport_HeaderRow(CHAR__A__Heads, (uint16_t)UP_REPORT_EV_COLS);

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_EVENTS, (uint32_t)sizeof(up_event_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_event_t up_event_t__event;
            char char__date[UP_REPORT_TEXT_MAX];
            char char__clock[UP_REPORT_TEXT_MAX];
            char char__code[8];
            char char__valueA[UP_REPORT_NAME_MAX];

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_event_t__event, (uint32_t)sizeof(up_event_t)))
            {
                break;
            }
            if (!func__UpReport_InRange(up_event_t__event.uint32_t__absS))
            {
                continue;
            }

            func__UpReport_DateText(up_event_t__event.uint32_t__epoch, char__date);
            func__UpReport_ClockText(up_event_t__event.uint32_t__epoch, char__clock);
            (void)snprintf(char__code, sizeof(char__code), "E%02lu", (unsigned long)up_event_t__event.uint8_t__code);

            /* [EN] valueA means "the fault bit" for fault events and "the
                      duration in seconds" for a charge. Saying which one it is
                      makes the column usable instead of mysterious.
               [FA] valueA برای رویدادهای خطا یعنی «بیت خطا» و برای شارژ یعنی
                      «مدت به ثانیه». گفتن این‌که کدام است، ستون را قابل استفاده
                      می‌کند نه مرموز. */
            if ((up_event_t__event.uint8_t__code == (uint8_t)UP_EV_FAULT_SET) ||
                (up_event_t__event.uint8_t__code == (uint8_t)UP_EV_FAULT_CLEAR))
            {
                (void)snprintf(char__valueA, sizeof(char__valueA), "%s",
                               func__UpReport_FaultText(up_event_t__event.uint16_t__valueA));
            }
            else if (up_event_t__event.uint32_t__durS > 0u)
            {
                (void)snprintf(char__valueA, sizeof(char__valueA), "%lu ثانیه",
                               (unsigned long)up_event_t__event.uint32_t__durS);
            }
            else
            {
                (void)snprintf(char__valueA, sizeof(char__valueA), "-");
            }

            func__UpReport_RowBegin();
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_DATE, UP_XLSX_STYLE_TEXT, char__date);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_CLOCK, UP_XLSX_STYLE_TEXT, char__clock);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_INDEX, UP_XLSX_STYLE_TEXT, char__code);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_NAME, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_EventText(up_event_t__event.uint8_t__code));
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_CHANNEL, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_ChannelText(up_event_t__event.uint8_t__channel));
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_SEVERITY, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_SeverityText(up_event_t__event.uint8_t__severity));
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_EV_SECONDS, UP_XLSX_STYLE_NUMBER,
                                 (int32_t)up_event_t__event.uint32_t__durS);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_VALUE_A, UP_XLSX_STYLE_TEXT, char__valueA);
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_EV_VALUE_B, UP_XLSX_STYLE_NUMBER,
                                 (int32_t)up_event_t__event.uint16_t__valueB);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_STATE, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_EventStateText(up_event_t__event.uint8_t__flags));
            func__UpReport_RowEnd();
            uint32_t__rows++;
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    if (uint32_t__rows == 0u)
    {
        func__UpReport_RowBegin();
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_EV_DATE, UP_XLSX_STYLE_TEXT,
                              "در این بازه رویدادی ثبت نشده است");
        func__UpReport_RowEnd();
    }

    func__UpXlsx_SheetClose((uint16_t)UP_REPORT_EV_COLS, 1u);

    return func__UpXlsx_End();
}

/* ==================== Sheet five: samples / برگهٔ پنج: نمونه‌ها ==================== */
/**
 * @brief  [EN] The raw trail: input, pack and lower-battery volts, both charge
 *              currents, both duties and the two charger states, sample by
 *              sample. Capped at UP_XLSX_SAMPLE_ROWS rows, keeping the NEWEST,
 *              because that is the part a fault hunter needs and because an
 *              unbounded sheet is a report that takes minutes to open.
 *         [FA] رد خام داده: ورودی، پک و باتری پایینی، هر دو جریان شارژ، هر دو
 *              duty و هر دو حالت شارژر، نمونه به نمونه. با سقف
 *              UP_XLSX_SAMPLE_ROWS ردیف، با نگه‌داشتن «جدیدترین‌ها»، چون همان
 *              بخشی است که شکارچی خطا لازم دارد و چون برگهٔ بی‌سقف گزارشی است که
 *              دقیقه‌ها طول می‌کشد باز شود.
 * @return [EN] true when the sheet was written / [FA] در صورت نوشتن برگه true
 */
static bool func__UpReport_SheetSamples(void)
{
    static const char *const CHAR__A__Heads[UP_REPORT_SMP_COLS] =
    {
        "تاریخ شمسی", "ساعت", "ثانیهٔ یکنوای پنل", "ورودی ۲۴ ولت (ولت)", "ولتاژ پک (ولت)",
        "باتری پایینی (ولت)", "جریان کانال ۱ (آمپر)", "جریان کانال ۲ (آمپر)",
        "duty کانال ۱ (پرمیل)", "duty کانال ۲ (پرمیل)", "حالت کانال ۱", "حالت کانال ۲", "فلگ‌ها"
    };
    static const uint8_t UINT8_T__A__Widths[UP_REPORT_SMP_COLS] =
    {
        12u, 10u, 16u, 18u, 15u, 17u, 21u, 21u, 19u, 19u, 15u, 15u, 11u
    };
    up_ring_reader_t up_ring_reader_t__reader;
    uint32_t uint32_t__written = 0u;
    uint32_t uint32_t__seen = 0u;

    if (!func__UpXlsx_SheetMember(5u))
    {
        return false;
    }

    UINT32_T__G__UpReportRow = 1u;
    func__UpXlsx_SheetOpen(1u);
    func__UpXlsx_Cols(UINT8_T__A__Widths, (uint16_t)UP_REPORT_SMP_COLS);
    func__UpXlsx_DataOpen();
    func__UpReport_HeaderRow(CHAR__A__Heads, (uint16_t)UP_REPORT_SMP_COLS);

    if (BOOL__G__UpReportSampleCapped)
    {
        func__UpReport_RowBegin();
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_DATE, UP_XLSX_STYLE_TEXT,
                              "این برگه برای حجم کمتر، فقط جدیدترین نمونه‌های بازه را دارد");
        func__UpReport_RowEnd();
    }

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_sample_t up_sample_t__sample;
            char char__date[UP_REPORT_TEXT_MAX];
            char char__clock[UP_REPORT_TEXT_MAX];
            char char__flags[8];
            uint32_t uint32_t__epoch;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_sample_t__sample, (uint32_t)sizeof(up_sample_t)))
            {
                break;
            }
            if (!func__UpReport_InRange(up_sample_t__sample.uint32_t__absS))
            {
                continue;
            }

            uint32_t__seen++;
            if (uint32_t__seen <= UINT32_T__G__UpReportSampleSkip)
            {
                continue;
            }

            /* [EN] The wall clock of a sample comes from the panel's own clock
                      base, never from "now": reading the current time here would
                      stamp a week-old row with this second.
               [FA] ساعت دیواری هر نمونه از پایهٔ ساعت خود پنل می‌آید، هرگز از
                      «حالا»: خواندن زمان فعلی اینجا، ردیفی یک‌هفته‌ای را با همین
                      ثانیه مهر می‌کرد. */
            uint32_t__epoch = func__UpState_EpochFromAbs(up_sample_t__sample.uint32_t__absS);
            func__UpReport_DateText(uint32_t__epoch, char__date);
            func__UpReport_ClockText(uint32_t__epoch, char__clock);
            (void)func__UpReport_HexText(char__flags, up_sample_t__sample.uint8_t__flags);

            func__UpReport_RowBegin();
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_DATE, UP_XLSX_STYLE_TEXT, char__date);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_CLOCK, UP_XLSX_STYLE_TEXT, char__clock);
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_SMP_ABS, UP_XLSX_STYLE_NUMBER,
                                 (int32_t)up_sample_t__sample.uint32_t__absS);
            func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_SMP_VIN, UP_XLSX_STYLE_NUMBER,
                                   (int32_t)up_sample_t__sample.uint16_t__vinMv, UP_REPORT_UNIT_DECIMALS);
            func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_SMP_V24, UP_XLSX_STYLE_NUMBER,
                                   (int32_t)up_sample_t__sample.uint16_t__v24Mv, UP_REPORT_UNIT_DECIMALS);
            func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_SMP_V12, UP_XLSX_STYLE_NUMBER,
                                   (int32_t)up_sample_t__sample.uint16_t__v12Mv, UP_REPORT_UNIT_DECIMALS);
            func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_SMP_I1, UP_XLSX_STYLE_NUMBER,
                                   (int32_t)up_sample_t__sample.uint16_t__i1Ma, UP_REPORT_UNIT_DECIMALS);
            func__UpXlsx_CellFixed(UINT32_T__G__UpReportRow, UP_REPORT_SMP_I2, UP_XLSX_STYLE_NUMBER,
                                   (int32_t)up_sample_t__sample.uint16_t__i2Ma, UP_REPORT_UNIT_DECIMALS);
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_SMP_DUTY1, UP_XLSX_STYLE_NUMBER,
                                 (int32_t)up_sample_t__sample.uint16_t__duty1);
            func__UpXlsx_CellInt(UINT32_T__G__UpReportRow, UP_REPORT_SMP_DUTY2, UP_XLSX_STYLE_NUMBER,
                                 (int32_t)up_sample_t__sample.uint16_t__duty2);
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_STATE1, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_StateText(up_sample_t__sample.uint8_t__state1));
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_STATE2, UP_XLSX_STYLE_TEXT,
                                  func__UpReport_StateText(up_sample_t__sample.uint8_t__state2));
            func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_FLAGS, UP_XLSX_STYLE_TEXT, char__flags);
            func__UpReport_RowEnd();
            uint32_t__written++;
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    if (uint32_t__written == 0u)
    {
        func__UpReport_RowBegin();
        func__UpXlsx_CellText(UINT32_T__G__UpReportRow, UP_REPORT_SMP_DATE, UP_XLSX_STYLE_TEXT,
                              "در این بازه نمونه‌ای ثبت نشده است");
        func__UpReport_RowEnd();
    }

    func__UpXlsx_SheetClose((uint16_t)UP_REPORT_SMP_COLS, 1u);

    return func__UpXlsx_End();
}

/* ==================== The report / گزارش ==================== */
/* ==================== Write ==================== */
/**
 * @brief  [EN] Write the whole workbook to a sink: open the zip, count what is
 *              in range, then one sheet after another, then close. The counts
 *              happen first because the summary sheet - which is written first,
 *              because it is the sheet Excel opens on - states them.
 *         [FA] نوشتن کل کتاب در یک مقصد: باز کردن زیپ، شمردن آنچه در بازه است،
 *              سپس برگه پس از برگه و در پایان بستن. شمارش‌ها اول انجام می‌شوند
 *              چون برگهٔ خلاصه - که اول نوشته می‌شود، چون اکسل روی آن باز
 *              می‌شود - همان‌ها را اعلام می‌کند.
 * @param  up_xlsx_sink_t__sink [EN] where the bytes go / [FA] بایت‌ها کجا می‌روند
 * @return [EN] true when a complete workbook was produced / [FA] در صورت تولید کتاب کامل true
 */
static bool func__UpReport_Write(up_xlsx_sink_t up_xlsx_sink_t__sink)
{
    if (!func__UpXlsx_Begin(CHAR__A__G__UpReportSheets, (uint16_t)UP_REPORT_SHEETS, up_xlsx_sink_t__sink))
    {
        return false;
    }

    func__UpReport_CountSamples();
    func__UpReport_CountEvents();

    if (!func__UpReport_SheetSummary()) { return false; }
    if (!func__UpReport_SheetDaily()) { return false; }
    if (!func__UpReport_SheetCharges()) { return false; }
    if (!func__UpReport_SheetEvents()) { return false; }
    if (!func__UpReport_SheetSamples()) { return false; }

    return func__UpXlsx_Finish();
}

#endif /* UP_REPORT_H */
