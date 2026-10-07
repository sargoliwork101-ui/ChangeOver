/**
 * @file    up_calendar.h
 * @brief   [EN] Dates and times at the panel's own timezone: Gregorian ⇄ Jalali
 *               conversion, and the two questions the rest of the panel keeps
 *               asking - "what day is it?" and "what hour is it?".
 *
 *          WHY A WHOLE FILE FOR THIS
 *            The panel is in Iran and its owner reads Persian dates. Two things
 *            follow, and both were wrong before this file existed:
 *
 *            1. The panel's clock is UTC (an epoch second has no timezone), but
 *               a day is a LOCAL thing. With a UTC day boundary, "today" rolled
 *               over at 03:30 in the morning and the hour histogram on the
 *               statistics page was 3.5 hours out of step with the wall clock
 *               in the room. Every consumer of "which day" and "which hour" now
 *               goes through this file with the stored offset added first.
 *            2. The Excel report has to carry a date a human recognises as a
 *               date. A Jalali string in a spreadsheet must sort correctly,
 *               which is why the report writes `1405/07/15` - fixed width,
 *               most-significant field first, Latin digits - and keeps the
 *               epoch second in the column beside it for arithmetic.
 *
 *          The Jalali conversion is the standard 33-year-cycle algorithm used
 *          by jalaali-js and the Iranian calendar libraries, implemented here in
 *          integer arithmetic (no floating point, no library). It is validated
 *          two ways: fixed known dates in the host tests, and an independent
 *          Python implementation in tools/check_user_panel.sh that re-reads the
 *          dates out of a generated Excel file and recomputes them.
 *
 *          [FA] تاریخ و ساعت به وقت خود پنل: تبدیل میلادی ⇄ شمسی و همان دو
 *               سؤالی که بقیهٔ پنل مدام می‌پرسد - «امروز چندم است؟» و
 *               «ساعت چند است؟».
 *
 *          چرا یک فایل جدا برای این
 *            پنل در ایران است و صاحبش تاریخ شمسی می‌خواند. دو نتیجه دارد و
 *            هر دو پیش از این فایل غلط بود:
 *
 *            ۱. ساعت پنل UTC است (ثانیهٔ مطلق منطقهٔ زمانی ندارد)، ولی «روز»
 *               چیز محلی است. با مرز روز بر UTC، «امروز» ساعت ۳:۳۰ بامداد عوض
 *               می‌شد و نمودار ساعتی آمار ۳٫۵ ساعت با ساعت دیواری اتاق اختلاف
 *               داشت. از این پس هر مصرف‌کنندهٔ «کدام روز» و «کدام ساعت» از این
 *               فایل می‌گذرد و اول اختلاف ساعت ذخیره‌شده را اضافه می‌کند.
 *            ۲. گزارش اکسل باید تاریخی ببرد که انسان آن را تاریخ بشناسد. رشتهٔ
 *               شمسی در صفحهٔ گسترده باید درست مرتب شود، پس گزارش `1405/07/15`
 *               می‌نویسد - عرض ثابت، بزرگ‌ترین جزء اول، ارقام لاتین - و ثانیهٔ
 *               مطلق را هم در ستون کنارش نگه می‌دارد برای حساب‌وکتاب.
 *
 *          تبدیل شمسی همان الگوریتم استاندارد چرخهٔ ۳۳سالهٔ کتابخانه‌های تقویم
 *          ایرانی است، اینجا با حساب صحیح (بدون ممیز شناور و بدون کتابخانه).
 *          دو جا اعتبارسنجی می‌شود: تاریخ‌های معلوم در تست میزبان، و یک
 *          پیاده‌سازی مستقل پایتون در tools/check_user_panel.sh که تاریخ‌ها را
 *          از فایل اکسل ساخته‌شده می‌خواند و از نو حساب می‌کند.
 */

#ifndef UP_CALENDAR_H
#define UP_CALENDAR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ==================== Types / تایپ‌ها ==================== */
typedef struct
{
    int32_t  int32_t__year;         /* [EN] Jalali or Gregorian year / [FA] سال شمسی یا میلادی */
    uint8_t  uint8_t__month;        /* [EN] 1..12 / [FA] ۱..۱۲ */
    uint8_t  uint8_t__day;          /* [EN] 1..31 / [FA] ۱..۳۱ */
    uint8_t  uint8_t__hour;         /* [EN] 0..23, LOCAL / [FA] ۰..۲۳ محلی */
    uint8_t  uint8_t__minute;       /* [EN] 0..59 / [FA] ۰..۵۹ */
    uint8_t  uint8_t__second;       /* [EN] 0..59 / [FA] ۰..۵۹ */
} up_datetime_t;

/* ==================== Constants / ثابت‌ها ==================== */
/* [EN] JDN of 1970-01-01. The Jalali algorithm works in Julian Day Numbers;
   the panel works in epoch seconds; this is the one place the two meet.
   [FA] شمارهٔ روز ژولیانیِ ۱۹۷۰-۰۱-۰۱. الگوریتم شمسی با روز ژولیانی کار می‌کند و
   پنل با ثانیهٔ مطلق؛ این تنها جایی است که آن دو به هم می‌رسند. */
#define UP_CAL_JDN_1970     2440588

/* [EN] The largest epoch second that can have a timezone offset added to it in
   SIGNED arithmetic without the sum overflowing: 0x7FFF0000, which is January
   2038 minus a day. Below it the panel keeps the careful signed floor division
   (a wrong clock set to 1969 must still print 1969); above it the same job is
   done in unsigned arithmetic, where wrap-around is defined instead of
   undefined. The panel's own clock refuses anything past year 2033, so the
   second branch exists for records and reports, not for the live clock.
   [FA] بزرگ‌ترین ثانیه‌ای که می‌توان اختلاف منطقهٔ زمانی را در حساب علامت‌دار به
   آن افزود بی‌آنکه جمع سرریز کند: ‎0x7FFF0000، یعنی ژانویهٔ ۲۰۳۸ منهای یک روز.
   زیر آن، تقسیم کف علامت‌دار دقیق حفظ می‌شود (ساعت غلطِ ۱۹۶۹ هم باید ۱۹۶۹ چاپ
   شود)؛ بالای آن همان کار با حساب بی‌علامت انجام می‌شود، جایی که دور زدن تعریف
   شده است و نه رفتار تعریف‌نشده. ساعت خود پنل بعد از ۲۰۳۳ را رد می‌کند، پس
   شاخهٔ دوم برای رکوردها و گزارش‌هاست، نه برای ساعت زنده. */
#define UP_CAL_SIGNED_SAFE_EPOCH  0x7FFF0000u

/* [EN] A day index is a LOCAL day number: floor((epoch + tz) / 86400). It is an
   int32 so a date before 1970 (a wrong clock, a test) cannot wrap into a huge
   positive day number and be mistaken for a real one.
   [FA] شمارهٔ روز، شمارهٔ «محلی» است: کف تقسیم (ثانیهٔ مطلق + اختلاف) بر ۸۶۴۰۰.
   از نوع علامت‌دار تا تاریخ پیش از ۱۹۷۰ به عددی غول‌آسا تبدیل نشود. */
static inline int32_t func__UpCal_DaysFromCivil(int32_t int32_t__y, uint32_t uint32_t__m, uint32_t uint32_t__d);

/* ==================== Gregorian core / هستهٔ میلادی ==================== */
/**
 * @brief  [EN] Days since 1970-01-01 for a Gregorian date (Howard Hinnant's
 *              civil calendar algorithm: pure integer, exact for every date the
 *              panel can ever see).
 *         [FA] تعداد روز از ۱۹۷۰-۰۱-۰۱ برای یک تاریخ میلادی (الگوریتم تقویم
 *              مدنی هاوارد هینانت: کاملاً صحیح، برای هر تاریخی که پنل ممکن است
 *              ببیند دقیق).
 * @param  int32_t__y [EN] Gregorian year / [FA] سال میلادی
 * @param  uint32_t__m [EN] month 1..12 / [FA] ماه
 * @param  uint32_t__d [EN] day 1..31 / [FA] روز
 * @return [EN] days since 1970-01-01 (negative before it) / [FA] روز از مبدأ
 */
static inline int32_t func__UpCal_DaysFromCivil(int32_t int32_t__y, uint32_t uint32_t__m, uint32_t uint32_t__d)
{
    int32_t int32_t__era;
    uint32_t uint32_t__yoe;
    uint32_t uint32_t__doy;
    uint32_t uint32_t__doe;

    int32_t__y -= (int32_t)(uint32_t__m <= 2u);
    int32_t__era = (int32_t__y >= 0) ? (int32_t__y / 400) : ((int32_t__y - 399) / 400);
    uint32_t__yoe = (uint32_t)(int32_t__y - (int32_t__era * 400));
    uint32_t__doy = ((153u * (uint32_t__m + ((uint32_t__m > 2u) ? 0xFFFFFFFDu : 9u)) + 2u) / 5u) + uint32_t__d - 1u;
    uint32_t__doe = (uint32_t__yoe * 365u) + (uint32_t__yoe / 4u) - (uint32_t__yoe / 100u) + uint32_t__doy;

    return (int32_t__era * 146097) + (int32_t)uint32_t__doe - 719468;
}

/**
 * @brief  [EN] The inverse: Gregorian year, month and day for a day number.
 *         [FA] عکس آن: سال، ماه و روز میلادی برای یک شمارهٔ روز.
 * @param  int32_t__days [EN] days since 1970-01-01 / [FA] روز از مبدأ
 * @param  int32_t__yearOut [EN] year / [FA] سال
 * @param  uint8_t__monthOut [EN] month / [FA] ماه
 * @param  uint8_t__dayOut [EN] day / [FA] روز
 * @return [EN] None / [FA] ندارد
 */
static inline void func__UpCal_CivilFromDays(int32_t int32_t__days, int32_t *int32_t__yearOut,
                                             uint8_t *uint8_t__monthOut, uint8_t *uint8_t__dayOut)
{
    int32_t int32_t__z = int32_t__days + 719468;
    int32_t int32_t__era = (int32_t__z >= 0) ? (int32_t__z / 146097) : ((int32_t__z - 146096) / 146097);
    uint32_t uint32_t__doe = (uint32_t)(int32_t__z - (int32_t__era * 146097));
    uint32_t uint32_t__yoe = (uint32_t__doe - (uint32_t__doe / 1460u) + (uint32_t__doe / 36524u) - (uint32_t__doe / 146096u)) / 365u;
    int32_t int32_t__year = (int32_t)uint32_t__yoe + (int32_t__era * 400);
    uint32_t uint32_t__doy = uint32_t__doe - ((365u * uint32_t__yoe) + (uint32_t__yoe / 4u) - (uint32_t__yoe / 100u));
    uint32_t uint32_t__mp = ((5u * uint32_t__doy) + 2u) / 153u;
    uint32_t uint32_t__day = uint32_t__doy - (((153u * uint32_t__mp) + 2u) / 5u) + 1u;
    /* [EN] Month 10 of the internal numbering is January, so the two branches
       add +3 or -9. The negative branch is written as its two's complement
       constant because this file does all its arithmetic in unsigned types - and
       -9 is 0xFFFFFFF7, NOT 0xFFFFFFF9: getting that constant wrong moved every
       January into March, which is exactly the kind of bug a host test with
       dated anchors exists to catch.
       [FA] ماه دهمِ شماره‌گذاری داخلی، ژانویه است؛ پس دو شاخه ۳+ یا ۹- اضافه
       می‌کنند. شاخهٔ منفی به‌صورت متمم دویش نوشته شده چون این فایل همهٔ حساب‌ها را
       با تایپ بی‌علامت انجام می‌دهد - و ۹- می‌شود ‎0xFFFFFFF7 و «نه» ‎0xFFFFFFF9:
       غلط‌بودن همان ثابت، هر ژانویه را به مارس می‌برد؛ دقیقاً همان نوع خطایی که
       تست میزبان با لنگرهای تاریخی برای گرفتنش وجود دارد. */
    uint32_t uint32_t__month = uint32_t__mp + ((uint32_t__mp < 10u) ? 3u : 0xFFFFFFF7u);

    int32_t__year += (int32_t)(uint32_t__month <= 2u);
    *int32_t__yearOut = int32_t__year;
    *uint8_t__monthOut = (uint8_t)uint32_t__month;
    *uint8_t__dayOut = (uint8_t)uint32_t__day;
}

/* ==================== Jalali core / هستهٔ شمسی ==================== */
/* [EN] The cycle-break table of the Iranian calendar (the same table the
   standard libraries carry). Every 33-year cycle has 8 leap years; the table
   says where the cycles start and end so the rule stays correct past 2100.
   [FA] جدول شکست چرخهٔ تقویم ایرانی (همان جدولی که کتابخانه‌های استاندارد
   دارند). هر چرخهٔ ۳۳ساله ۸ سال کبیسه دارد؛ جدول می‌گوید چرخه‌ها کجا شروع و
   تمام می‌شوند تا قاعده بعد از ۲۱۰۰ هم درست بماند. */
static const int32_t INT32_T__A__UpCalBreaks[] = {
    -61, 9, 38, 199, 426, 686, 756, 818, 1111, 1181,
    1210, 1635, 2060, 2097, 2192, 2262, 2324, 2394, 2456, 3178
};

#define UP_CAL_BREAK_COUNT   ((uint32_t)(sizeof(INT32_T__A__UpCalBreaks) / sizeof(INT32_T__A__UpCalBreaks[0])))

/**
 * @brief  [EN] Leap state and the day of Farvardin 1's Gregorian March date for
 *              one Jalali year: the standard `jalCal` step.
 *         [FA] وضعیت کبیسه و روز مارسِ آغاز سال شمسی برای یک سال: همان گام
 *              استاندارد `jalCal`.
 * @param  int32_t__jy [EN] Jalali year / [FA] سال شمسی
 * @param  uint8_t__leapOut [EN] position in the leap cycle, exactly as the
 *                             published table reports it: 0 means THIS year is
 *                             a leap year, 1 means the year before it was, and
 *                             so on to 4. Collapsing it to a yes/no flag would
 *                             lose the one value the caller actually needs.
 *                          [FA] جایگاه در چرخهٔ کبیسه، دقیقاً همان‌طور که جدول
 *                             منتشرشده گزارش می‌کند: صفر یعنی «همین» سال کبیسه
 *                             است، ۱ یعنی سال قبلش کبیسه بود و همین‌طور تا ۴.
 *                             تبدیلش به یک بله/خیر، همان یک مقداری را از دست
 *                             می‌دهد که فراخوان لازم دارد.
 * @param  uint8_t__marchOut [EN] March day of Farvardin 1 / [FA] روز مارسِ ۱ فروردین
 * @return [EN] true when the year is inside the tabulated range
 *          [FA] اگر سال داخل بازهٔ جدول باشد true
 */
static inline bool func__UpCal_JalCal(int32_t int32_t__jy, uint8_t *uint8_t__leapOut, uint8_t *uint8_t__marchOut)
{
    int32_t int32_t__gy = int32_t__jy + 621;
    int32_t int32_t__leapJ = -14;
    int32_t int32_t__jp = INT32_T__A__UpCalBreaks[0];
    int32_t int32_t__jump = 0;
    int32_t int32_t__leapG;
    int32_t int32_t__n;
    int32_t int32_t__leap;
    uint32_t uint32_t__i;

    if ((int32_t__jy < INT32_T__A__UpCalBreaks[0]) ||
        (int32_t__jy >= INT32_T__A__UpCalBreaks[UP_CAL_BREAK_COUNT - 1u]))
    {
        return false;
    }

    for (uint32_t__i = 1u; uint32_t__i < UP_CAL_BREAK_COUNT; uint32_t__i++)
    {
        int32_t int32_t__jm = INT32_T__A__UpCalBreaks[uint32_t__i];

        int32_t__jump = int32_t__jm - int32_t__jp;
        if (int32_t__jy < int32_t__jm)
        {
            break;
        }
        /* [EN] A completed segment contributes its whole 33-year cycles plus
           the leap years of its remainder: div(jump,33)*8 + div(mod(jump,33),4).
           Written as three named numbers so it can be compared, term by term,
           with the published table - the earlier one-liner had an extra -1 and a
           +3 in the division and moved every Jalali date three days.
           [FA] هر بخش تمام‌شده، چرخه‌های کامل ۳۳ساله‌اش به‌علاوهٔ سال‌های کبیسهٔ
           باقی‌مانده‌اش را می‌افزاید: div(jump,33)*8 + div(mod(jump,33),4). در سه
           عدد نام‌دار نوشته شده تا جمله‌به‌جمله با جدول منتشرشده مقایسه شود -
           تک‌خطی قبلی یک «۱-» اضافه و یک «۳+» داخل تقسیم داشت و هر تاریخ شمسی
           را سه روز جابه‌جا می‌کرد. */
        int32_t int32_t__whole = int32_t__jump / 33;
        int32_t int32_t__rest = int32_t__jump % 33;

        int32_t__leapJ += (int32_t__whole * 8) + (int32_t__rest / 4);
        int32_t__jp = int32_t__jm;
    }

    /* [EN] `div(jump,33)*8 + div(mod(jump,33),4)` with the running jp: the loop
       above already folded the completed cycles, so only the remainder of the
       CURRENT segment is added here.
       [FA] چرخه‌های تمام‌شده در حلقه جمع شدند، پس اینجا فقط باقی‌ماندهٔ بخش جاری
       اضافه می‌شود. */
    int32_t__n = int32_t__jy - int32_t__jp;
    int32_t__leapJ += ((int32_t__n / 33) * 8) + (((int32_t__n % 33) + 3) / 4);
    if (((int32_t__jump % 33) == 4) && ((int32_t__jump - int32_t__n) == 4))
    {
        int32_t__leapJ += 1;
    }

    int32_t__leapG = (int32_t__gy / 4) - ((((int32_t__gy / 100) + 1) * 3) / 4) - 150;

    if ((int32_t__jump - int32_t__n) < 6)
    {
        int32_t__n = int32_t__n - int32_t__jump + (((int32_t__jump + 4) / 33) * 33);
    }
    int32_t__leap = ((((int32_t__n + 1) % 33) - 1) % 4);
    if (int32_t__leap == -1)
    {
        int32_t__leap = 4;
    }

    *uint8_t__leapOut = (uint8_t)int32_t__leap;
    *uint8_t__marchOut = (uint8_t)(20 + int32_t__leapJ - int32_t__leapG);
    return true;
}

/**
 * @brief  [EN] Jalali year, month and day for a day number. Days before
 *              Farvardin 1 of the computed year step back one Jalali year and
 *              restart the day count from Farvardin 1, which is where the two
 *              31-day month blocks and the 30-day block meet.
 *         [FA] سال، ماه و روز شمسی برای یک شمارهٔ روز. روزهای پیش از ۱ فروردین
 *              سال محاسبه‌شده، یک سال شمسی عقب می‌روند و شمارش را از ۱ فروردین
 *              از نو شروع می‌کنند؛ همان‌جا که بلوک ماه‌های ۳۱روزه و بلوک
 *              ۳۰روزه به هم می‌رسند.
 * @param  int32_t__days [EN] days since 1970-01-01 / [FA] روز از مبدأ
 * @param  up_datetime_t__out [EN] receives year/month/day / [FA] سال/ماه/روز
 * @return [EN] true when converted, false when outside the tabulated range
 *          [FA] در صورت تبدیل true، بیرون از بازهٔ جدول false
 */
static inline bool func__UpCal_JalaliFromDays(int32_t int32_t__days, up_datetime_t *up_datetime_t__out)
{
    int32_t int32_t__year;
    uint8_t uint8_t__month;
    uint8_t uint8_t__day;
    int32_t int32_t__jy;
    int32_t int32_t__jdn;
    int32_t int32_t__jdnFirst;
    int32_t int32_t__k;
    uint8_t uint8_t__leap = 0u;
    uint8_t uint8_t__march = 0u;

    func__UpCal_CivilFromDays(int32_t__days, &int32_t__year, &uint8_t__month, &uint8_t__day);
    int32_t__jy = int32_t__year - 621;
    if (!func__UpCal_JalCal(int32_t__jy, &uint8_t__leap, &uint8_t__march))
    {
        return false;
    }

    int32_t__jdn = int32_t__days + UP_CAL_JDN_1970;
    int32_t__jdnFirst = UP_CAL_JDN_1970 + func__UpCal_DaysFromCivil(int32_t__year, 3u, uint8_t__march);
    int32_t__k = int32_t__jdn - int32_t__jdnFirst;

    if (int32_t__k >= 0)
    {
        if (int32_t__k <= 185)
        {
            up_datetime_t__out->int32_t__year = int32_t__jy;
            up_datetime_t__out->uint8_t__month = (uint8_t)(1 + (int32_t__k / 31));
            up_datetime_t__out->uint8_t__day = (uint8_t)((int32_t__k % 31) + 1);
            return true;
        }
        int32_t__k -= 186;
    }
    else
    {
        int32_t__jy -= 1;
        int32_t__k += 179;
        /* [EN] This branch means the date belongs to the PREVIOUS Jalali year,
           so its second half is one day longer when that previous year was a
           leap year - which the cycle position 1 says exactly.
           [FA] این شاخه یعنی تاریخ به سال شمسی «قبل» تعلق دارد، پس نیمهٔ دومش
           وقتی یک روز بلندتر است که همان سال قبل کبیسه بوده باشد - و جایگاه ۱
           در چرخه دقیقاً همین را می‌گوید. */
        if (uint8_t__leap == 1u)
        {
            int32_t__k += 1;
        }
    }

    up_datetime_t__out->int32_t__year = int32_t__jy;
    up_datetime_t__out->uint8_t__month = (uint8_t)(7 + (int32_t__k / 30));
    up_datetime_t__out->uint8_t__day = (uint8_t)((int32_t__k % 30) + 1);
    return true;
}

/* ==================== Panel-facing helpers / کمکی‌های پنل ==================== */
/**
 * @brief  [EN] One epoch second in the panel's own timezone, split into both
 *              calendars. `int32_t__localDay` is the number the daily rollup and
 *              the heat map are keyed on, so the day boundary is the owner's
 *              midnight and not UTC's.
 *         [FA] یک ثانیهٔ مطلق به وقت خود پنل، شکسته‌شده در هر دو تقویم.
 *              `int32_t__localDay` همان عددی است که جمع روزانه و نقشهٔ حرارتی
 *              بر آن کلید خورده‌اند، پس مرز روز، نیمه‌شب صاحب دستگاه است نه UTC.
 * @param  uint32_t__epochS [EN] epoch seconds (UTC) / [FA] ثانیهٔ مطلق
 * @param  int32_t__tzOffsetS [EN] local offset, -43200..+50400 / [FA] اختلاف محلی
 * @param  up_datetime_t__out [EN] Gregorian date + local time / [FA] تاریخ میلادی + ساعت محلی
 * @return [EN] None / [FA] ندارد
 */
static inline int32_t func__UpCal_LocalDay(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS)
{
    if (uint32_t__epochS <= UP_CAL_SIGNED_SAFE_EPOCH)
    {
        int32_t int32_t__local = (int32_t)uint32_t__epochS + int32_t__tzOffsetS;

        /* [EN] Floor division, because a negative local second (a date before
           1970) must still land on the right day.
           [FA] تقسیم با کف، چون ثانیهٔ محلی منفی (تاریخی پیش از ۱۹۷۰) هم باید
           روی روز درست بیفتد. */
        return (int32_t__local >= 0) ? (int32_t__local / 86400) : ((int32_t__local - 86399) / 86400);
    }

    /* [EN] Far future (a stored record, an export, a test): reduce the epoch to
       days in unsigned arithmetic, then the day number fits again. The earlier
       version added the offset as a signed int here and a date in 2038 came out
       as 1902 - the classic 32-bit time overflow, caught by the dated anchors in
       the host test.
       [FA] آیندهٔ دور (رکورد ذخیره‌شده، خروجی، تست): ثانیه ابتدا با حساب
       بی‌علامت به روز کاهش می‌یابد و بعد شمارهٔ روز دوباره جا می‌شود. نسخهٔ قبلی
       همین‌جا اختلاف را در حساب علامت‌دار جمع می‌کرد و تاریخ ۲۰۳۸ به‌صورت ۱۹۰۲
       بیرون می‌آمد - همان سرریز کلاسیک ۳۲بیتی زمان که لنگرهای تاریخی تست
       میزبان گرفتنش را به عهده دارند. */
    uint32_t uint32_t__local = uint32_t__epochS + (uint32_t)int32_t__tzOffsetS;

    return (int32_t)(uint32_t__local / 86400u);
}

static inline uint32_t func__UpCal_LocalSecondOfDay(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS)
{
    if (uint32_t__epochS <= UP_CAL_SIGNED_SAFE_EPOCH)
    {
        int32_t int32_t__local = (int32_t)uint32_t__epochS + int32_t__tzOffsetS;

        /* [EN] Before 1970 the local second is negative; the clock face starts
           at zero rather than at a negative hour.
           [FA] پیش از ۱۹۷۰ ثانیهٔ محلی منفی است؛ صفحهٔ ساعت از صفر شروع می‌کند و
           نه از ساعت منفی. */
        if (int32_t__local < 0)
        {
            int32_t__local = 0;
        }
        return (uint32_t)(int32_t__local % 86400);
    }

    uint32_t uint32_t__local = uint32_t__epochS + (uint32_t)int32_t__tzOffsetS;

    return uint32_t__local % 86400u;
}

/**
 * @brief  [EN] Fill every field of a datetime from an epoch second.
 *         [FA] پر کردن همهٔ فیلدهای تاریخ و ساعت از یک ثانیهٔ مطلق.
 * @param  uint32_t__epochS [EN] epoch seconds / [FA] ثانیهٔ مطلق
 * @param  int32_t__tzOffsetS [EN] local offset / [FA] اختلاف محلی
 * @param  up_datetime_t__out [EN] destination; the date is JALALI
 *                             [FA] مقصد؛ تاریخ شمسی است
 * @return [EN] true when converted / [FA] در صورت تبدیل true
 */
static inline bool func__UpCal_DateTime(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS, up_datetime_t *up_datetime_t__out)
{
    int32_t int32_t__day = func__UpCal_LocalDay(uint32_t__epochS, int32_t__tzOffsetS);
    uint32_t uint32_t__secondOfDay = func__UpCal_LocalSecondOfDay(uint32_t__epochS, int32_t__tzOffsetS);

    memset(up_datetime_t__out, 0, sizeof(up_datetime_t));

    up_datetime_t__out->uint8_t__hour = (uint8_t)(uint32_t__secondOfDay / 3600u);
    up_datetime_t__out->uint8_t__minute = (uint8_t)((uint32_t__secondOfDay % 3600u) / 60u);
    up_datetime_t__out->uint8_t__second = (uint8_t)(uint32_t__secondOfDay % 60u);

    return func__UpCal_JalaliFromDays(int32_t__day, up_datetime_t__out);
}

/* ==================== Text / متن ==================== */
/**
 * @brief  [EN] Two digits with a leading zero, Latin digits.
 *              Latin on purpose: the report's date column has to sort as text
 *              and be pasted into any tool; the Persian digits belong on the
 *              page, where app.js does that job.
 *         [FA] دو رقم با صفر ابتدایی، ارقام لاتین. لاتین عمدی است: ستون تاریخِ
 *              گزارش باید به‌صورت متن مرتب شود؛ ارقام فارسی جایشان در صفحه است
 *              که app.js آن کار را می‌کند.
 * @param  char__out [EN] destination, at least 3 bytes / [FA] مقصد
 * @param  uint32_t__value [EN] 0..99 / [FA] مقدار
 * @return [EN] None / [FA] ندارد
 */
static inline void func__UpCal_Two(char *char__out, uint32_t uint32_t__value)
{
    char__out[0] = (char)('0' + ((uint32_t__value / 10u) % 10u));
    char__out[1] = (char)('0' + (uint32_t__value % 10u));
    char__out[2] = '\0';
}

/**
 * @brief  [EN] Four digits, Latin, no sign - a year written the way a keyboard
 *              writes it. Digits are placed by hand instead of by printf: a
 *              format string that CAN print more digits than the buffer holds
 *              is a warning today and a corrupt report tomorrow, so the panel
 *              never gives it the chance.
 *         [FA] چهار رقم لاتین بدون علامت - سالی که همان‌طور نوشته می‌شود که
 *              کیبورد می‌نویسد. ارقام دستی چیده می‌شوند نه با printf: قالببندی
 *              که بتواند بیشتر از گنجایش بافر رقم بزند، امروز یک هشدار است و
 *              فردا یک گزارش خراب؛ پس پنل اصلاً این فرصت را نمی‌دهد.
 * @param  char__out [EN] destination, at least 5 bytes / [FA] مقصد
 * @param  uint32_t__value [EN] year (0..9999) / [FA] سال
 * @return [EN] None / [FA] ندارد
 */
static inline void func__UpCal_Four(char *char__out, uint32_t uint32_t__value)
{
    func__UpCal_Two(&char__out[0], (uint32_t__value / 100u) % 100u);
    func__UpCal_Two(&char__out[2], uint32_t__value % 100u);
}

/**
 * @brief  [EN] "YYYY/MM/DD" (Jalali) for an epoch second.
 *         [FA] «YYYY/MM/DD» شمسی برای یک ثانیهٔ مطلق.
 * @param  uint32_t__epochS [EN] epoch seconds / [FA] ثانیهٔ مطلق
 * @param  int32_t__tzOffsetS [EN] local offset / [FA] اختلاف محلی
 * @param  char__out [EN] destination, at least 12 bytes / [FA] مقصد، دست‌کم ۱۲ بایت
 * @return [EN] true when a date was written / [FA] در صورت نوشتن تاریخ true
 */
static inline bool func__UpCal_DateText(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS, char *char__out)
{
    up_datetime_t up_datetime_t__dt;

    if (uint32_t__epochS == 0u)
    {
        (void)snprintf(char__out, 12u, "-");
        return false;
    }
    if (!func__UpCal_DateTime(uint32_t__epochS, int32_t__tzOffsetS, &up_datetime_t__dt))
    {
        (void)snprintf(char__out, 12u, "-");
        return false;
    }

    func__UpCal_Four(&char__out[0], (uint32_t)up_datetime_t__dt.int32_t__year);
    char__out[4] = '/';
    func__UpCal_Two(&char__out[5], (uint32_t)up_datetime_t__dt.uint8_t__month);
    char__out[7] = '/';
    func__UpCal_Two(&char__out[8], (uint32_t)up_datetime_t__dt.uint8_t__day);
    return true;
}

/**
 * @brief  [EN] "HH:MM:SS" local time for an epoch second.
 *         [FA] ساعت محلی «HH:MM:SS» برای یک ثانیهٔ مطلق.
 * @param  uint32_t__epochS [EN] epoch seconds / [FA] ثانیهٔ مطلق
 * @param  int32_t__tzOffsetS [EN] local offset / [FA] اختلاف محلی
 * @param  char__out [EN] destination, at least 10 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static inline void func__UpCal_TimeText(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS, char *char__out)
{
    uint32_t uint32_t__secondOfDay = func__UpCal_LocalSecondOfDay(uint32_t__epochS, int32_t__tzOffsetS);

    func__UpCal_Two(&char__out[0], uint32_t__secondOfDay / 3600u);
    char__out[2] = ':';
    func__UpCal_Two(&char__out[3], (uint32_t__secondOfDay % 3600u) / 60u);
    char__out[5] = ':';
    func__UpCal_Two(&char__out[6], uint32_t__secondOfDay % 60u);
}

/**
 * @brief  [EN] "YYYY-MM-DD" Gregorian for an epoch second - the column that lets
 *              a spreadsheet talk to anything outside Iran.
 *         [FA] «YYYY-MM-DD» میلادی برای یک ثانیهٔ مطلق - همان ستونی که به صفحهٔ
 *              گسترده اجازه می‌دهد با هر چیزی بیرون از ایران حرف بزند.
 * @param  uint32_t__epochS [EN] epoch seconds / [FA] ثانیهٔ مطلق
 * @param  int32_t__tzOffsetS [EN] local offset / [FA] اختلاف محلی
 * @param  char__out [EN] destination, at least 12 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static inline void func__UpCal_GregorianText(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS, char *char__out)
{
    int32_t int32_t__year;
    uint8_t uint8_t__month = 0u;
    uint8_t uint8_t__day = 0u;
    int32_t int32_t__dayNumber = func__UpCal_LocalDay(uint32_t__epochS, int32_t__tzOffsetS);

    if (uint32_t__epochS == 0u)
    {
        (void)snprintf(char__out, 12u, "-");
        return;
    }

    func__UpCal_CivilFromDays(int32_t__dayNumber, &int32_t__year, &uint8_t__month, &uint8_t__day);
    func__UpCal_Four(&char__out[0], (uint32_t)int32_t__year);
    char__out[4] = '-';
    func__UpCal_Two(&char__out[5], (uint32_t)uint8_t__month);
    char__out[7] = '-';
    func__UpCal_Two(&char__out[8], (uint32_t)uint8_t__day);
}

/**
 * @brief  [EN] A duration the way a person says it: "2h 14m", "45m", "38s".
 *         [FA] مدت به زبانی که آدم می‌گوید: «۲ساعت ۱۴دقیقه»، «۴۵دقیقه»، «۳۸ثانیه».
 * @param  uint32_t__seconds [EN] duration / [FA] مدت
 * @param  char__out [EN] destination, at least 24 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static inline void func__UpCal_DurationText(uint32_t uint32_t__seconds, char *char__out)
{
    uint32_t uint32_t__hours = uint32_t__seconds / 3600u;
    uint32_t uint32_t__minutes = (uint32_t__seconds % 3600u) / 60u;

    if (uint32_t__hours > 0u)
    {
        (void)snprintf(char__out, 24u, "%luh %lum", (unsigned long)uint32_t__hours, (unsigned long)uint32_t__minutes);
    }
    else if (uint32_t__minutes > 0u)
    {
        (void)snprintf(char__out, 24u, "%lum", (unsigned long)uint32_t__minutes);
    }
    else
    {
        (void)snprintf(char__out, 24u, "%lus", (unsigned long)uint32_t__seconds);
    }
}

#endif /* UP_CALENDAR_H */
