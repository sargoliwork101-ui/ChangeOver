/**
 * @file    up_xlsx.h
 * @brief   [EN] A real Excel workbook, written by a board with 40 KB of RAM.
 *              The file is a ZIP archive, so this module is two writers in one:
 *              a streaming ZIP writer (stored entries, no compression - the
 *              ESP's flash is the slow part here, not the wire) and just enough
 *              of the SpreadsheetML dialect to make Excel, LibreOffice and
 *              Google Sheets open the result without one complaint. Nothing is
 *              buffered whole: the workbook goes out member by member, and a
 *              member goes out row by row, so a report is allowed to be far
 *              bigger than any buffer this device could ever hold.
 *          [FA] یک فایل واقعی اکسل، نوشته‌شده توسط بردی با ۴۰ کیلوبایت رم. فایل
 *              یک آرشیو ZIP است، پس این ماژول دو نویسنده در یکی است: یک نویسندهٔ
 *              جریانیِ ZIP (عضوهای ذخیره‌شده، بدون فشرده‌سازی - قسمت کند ماجرا
 *              فلش ESP است نه سیم) و همان‌قدر از گویش SpreadsheetML که اکسل،
 *              لیبره‌آفیس و گوگل‌شیت بدون یک ایراد بازش کنند. هیچ‌چیز یک‌جا بافر
 *              نمی‌شود: کتاب عضو به عضو بیرون می‌رود و هر عضو ردیف به ردیف، پس
 *              گزارش می‌تواند بسیار بزرگ‌تر از هر بافری باشد که این دستگاه
 *              می‌تواند داشته باشد.
 *
 * @note    [EN] WHY "NO COMPRESSION" IS THE RIGHT ANSWER HERE
 *              Stored entries need no zlib and no extra RAM; a spreadsheet full
 *              of numbers compresses poorly anyway, and the report is normally
 *              read once and then saved by the operator. What the stored method
 *              does need is one CRC-32 per member - computed on the fly, byte by
 *              byte, exactly once.
 *          [FA] چرا «بدون فشرده‌سازی» اینجا جواب درست است
 *              عضوهای ذخیره‌شده نه zlib می‌خواهند نه رم اضافه؛ صفحهٔ گستردهٔ
 *              پر از عدد به‌هرحال خوب فشرده نمی‌شود و گزارش معمولاً یک‌بارخوانده
 *              و توسط اپراتور ذخیره می‌شود. آنچه روش «ذخیره» لازم دارد یک
 *              CRC-32 برای هر عضو است - همان لحظه، بایت‌به‌بایت و دقیقاً یک‌بار.
 *
 * @note    [EN] The writer knows nothing about HTTP. Its output goes to a sink
 *              function the caller supplies; that is what makes a whole workbook
 *              testable on a host with no web server in sight.
 *          [FA] نویسنده هیچ‌چیز از HTTP نمی‌داند. خروجی‌اش به تابع مقصدی می‌رود
 *              که فراخوان می‌دهد؛ همین باعث می‌شود کل کتاب روی میزبان و بدون
 *              هیچ سرور وبی قابل تست باشد.
 */

#ifndef UP_XLSX_H
#define UP_XLSX_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "up_config.h"

/* ==================== Constants / ثابت‌ها ==================== */
#define UP_XLSX_MAX_ENTRIES      14u     /* [EN] workbook members / [FA] عضوهای کتاب */
#define UP_XLSX_NAME_MAX         40u     /* [EN] longest member path / [FA] بلندترین مسیر عضو */
#define UP_XLSX_OUT_CHUNK        768u    /* [EN] bytes buffered before a send / [FA] بایت بافر پیش از ارسال */
#define UP_XLSX_ZIP_LOCAL        30u     /* [EN] local file header size / [FA] اندازهٔ سرصفحهٔ محلی */
#define UP_XLSX_ZIP_CENTRAL      46u     /* [EN] central directory entry / [FA] ردیف دایرکتوری مرکزی */
#define UP_XLSX_ZIP_EOCD         22u     /* [EN] end-of-directory record / [FA] رکورد پایان دایرکتوری */
#define UP_XLSX_ZIP_LOCAL_SIG    0x04034B50u
#define UP_XLSX_ZIP_CENTRAL_SIG  0x02014B50u
#define UP_XLSX_ZIP_EOCD_SIG     0x06054B50u
#define UP_XLSX_ZIP_DESC_SIG     0x08074B50u
#define UP_XLSX_ZIP_VERSION      20u     /* [EN] "2.0": stored + descriptor / [FA] «۲٫۰»: ذخیره + توصیفگر */
#define UP_XLSX_ZIP_METHOD       0u      /* [EN] stored, never deflated / [FA] ذخیره، هرگز فشرده */
#define UP_XLSX_ZIP_FLAGS        0x0808u /* [EN] bit3 = data descriptor, bit11 = UTF-8 names / [FA] بیت ۳ توصیفگر، بیت ۱۱ نام UTF-8 */
#define UP_XLSX_NAME_SHEET       24u     /* [EN] "xl/worksheets/sheet12.xml" / [FA] مسیر برگه */

/* [EN] A fixed DOS stamp (2020-01-01 12:00:00) on every member. Two reasons: a
   report must be byte-for-byte reproducible for the tests, and a board whose
   clock was never set must not stamp 1980 on the file it just wrote.
   [FA] مهر ثابت DOS (۲۰۲۰-۰۱-۰۱ ۱۲:۰۰:۰۰) روی همهٔ عضوها. دو دلیل: گزارش باید
   برای تست‌ها بایت‌به‌بایت بازتولیدپذیر باشد، و بردی که ساعتش هرگز تنظیم نشده
   نباید روی فایلی که همین حالا نوشت مهر ۱۹۸۰ بزند. */
#define UP_XLSX_DOS_TIME         0x6000u
#define UP_XLSX_DOS_DATE         0x5021u

/* [EN] Cell styles, one per cellXfs row in styles.xml - named here so the report
   can say "header row" instead of "style 1".
   [FA] سبک سلول‌ها، هر کدام یک ردیف cellXfs در styles.xml - اینجا نام‌دار شده‌اند
   تا گزارش بگوید «ردیف سرصفحه» نه «سبک ۱». */
#define UP_XLSX_STYLE_PLAIN      0u      /* [EN] unstyled / [FA] بی‌سبک */
#define UP_XLSX_STYLE_HEADER     1u      /* [EN] white on blue, centered, boxed / [FA] سفید روی آبی، وسط‌چین */
#define UP_XLSX_STYLE_TEXT       2u      /* [EN] body text / [FA] متن بدنه */
#define UP_XLSX_STYLE_NUMBER     3u      /* [EN] body number, centered / [FA] عدد بدنه، وسط‌چین */
#define UP_XLSX_STYLE_TITLE      4u      /* [EN] sheet title / [FA] عنوان برگه */
#define UP_XLSX_STYLE_LABEL      5u      /* [EN] section label / [FA] برچسب بخش */

/* [EN] Longest text a single cell can be built from before the writer refuses
   it. Cells are built in a stack buffer, so this is the size of that buffer.
   [FA] بلندترین متنی که یک سلول می‌تواند از آن ساخته شود پیش از آنکه نویسنده
   ردش کند. سلول‌ها در بافری روی پشته ساخته می‌شوند، پس این اندازهٔ همان بافر است. */
#define UP_XLSX_CELL_MAX         48u

/* ==================== Types / تایپ‌ها ==================== */
/* [EN] What the central directory needs to know about a member that is already
   finished: where it starts, how big it came out and what its CRC was.
   [FA] آنچه دایرکتوری مرکزی باید دربارهٔ عضوی که تمام شده بداند: از کجا شروع
   می‌شود، چقدر بزرگ شد و CRC‌اش چه بود. */
typedef struct
{
    uint32_t uint32_t__offset;              /* [EN] local header offset / [FA] جای سرصفحهٔ محلی */
    uint32_t uint32_t__size;                /* [EN] stored size in bytes / [FA] اندازهٔ ذخیره‌شده */
    uint32_t uint32_t__crc;                 /* [EN] CRC-32 of the data / [FA] CRC-32 داده */
    uint16_t uint16_t__nameLen;             /* [EN] path length / [FA] طول مسیر */
    char     char__name[UP_XLSX_NAME_MAX];  /* [EN] path inside the zip / [FA] مسیر داخل زیپ */
} up_xlsx_entry_t;

/* [EN] The sink: where finished bytes go. The web layer hands in a function that
   writes to the HTTP client; a host test hands in one that fills a buffer.
   [FA] مقصد: جایی که بایت‌های آماده می‌روند. لایهٔ وب تابعی می‌دهد که به کلاینت
   HTTP می‌نویسد؛ تست میزبان تابعی می‌دهد که یک بافر را پر می‌کند. */
typedef void (*up_xlsx_sink_t)(const char *char__data, uint32_t uint32_t__len);

/* [EN] The whole writer state. One instance, static: a board writes one report
   at a time, and static allocation is a project rule.
   [FA] کل حالت نویسنده. یک نمونه، استاتیک: برد در هر لحظه یک گزارش می‌نویسد و
   تخصیص استاتیک قاعدهٔ پروژه است. */
typedef struct
{
    uint16_t uint16_t__used;                /* [EN] bytes in the output buffer / [FA] بایت‌های بافر خروجی */
    uint16_t uint16_t__entries;             /* [EN] members finished so far / [FA] عضوهای تمام‌شده */
    uint32_t uint32_t__sent;                /* [EN] zip bytes already handed to the sink / [FA] بایت‌های تحویل‌شده */
    uint32_t uint32_t__entryCrc;            /* [EN] running CRC of the open member / [FA] CRC جاری عضو باز */
    uint32_t uint32_t__entrySize;           /* [EN] bytes in the open member / [FA] بایت‌های عضو باز */
    uint32_t uint32_t__entryOffset;         /* [EN] where the open member started / [FA] شروع عضو باز */
    uint16_t uint16_t__entryNameLen;        /* [EN] its path length / [FA] طول مسیرش */
    bool     bool__entryOpen;               /* [EN] a member is being written / [FA] عضوی در حال نوشتن */
    up_xlsx_sink_t up_xlsx_sink_t__sink;    /* [EN] destination / [FA] مقصد */
    char     char__buffer[UP_XLSX_OUT_CHUNK];  /* [EN] the only big buffer / [FA] تنها بافر بزرگ */
    up_xlsx_entry_t up_xlsx_entry_t__entry[UP_XLSX_MAX_ENTRIES];
} up_xlsx_t;

/* ==================== State / وضعیت ==================== */
static up_xlsx_t UP_XLSX_T__G__Xlsx;

/* ==================== Output plumbing / لوله‌کشی خروجی ==================== */
/* ==================== Flush ==================== */
/**
 * @brief  [EN] Hand the buffer to the sink and empty it. Called whenever the
 *              buffer would otherwise overflow, so callers never think about
 *              sizes.
 *         [FA] سپردن بافر به مقصد و خالی‌کردنش. هر وقت بافر در شرف سرریز باشد
 *              صدا زده می‌شود، پس فراخوان هیچ‌وقت به اندازه‌ها فکر نمی‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Flush(void)
{
    if (UP_XLSX_T__G__Xlsx.uint16_t__used == 0u)
    {
        return;
    }
    if (UP_XLSX_T__G__Xlsx.up_xlsx_sink_t__sink != NULL)
    {
        UP_XLSX_T__G__Xlsx.up_xlsx_sink_t__sink(UP_XLSX_T__G__Xlsx.char__buffer,
                                                (uint32_t)UP_XLSX_T__G__Xlsx.uint16_t__used);
    }
    UP_XLSX_T__G__Xlsx.uint32_t__sent += (uint32_t)UP_XLSX_T__G__Xlsx.uint16_t__used;
    UP_XLSX_T__G__Xlsx.uint16_t__used = 0u;
}

/* ==================== Put raw bytes ==================== */
/**
 * @brief  [EN] Append bytes that belong to the ZIP furniture (headers and
 *              descriptors) and are therefore NOT part of any member's CRC or
 *              size.
 *         [FA] افزودن بایت‌هایی که جزو ساخت ZIP هستند (سرصفحه‌ها و توصیفگرها) و
 *              پس نه در CRC هیچ عضوی حساب می‌شوند نه در اندازه‌اش.
 * @param  uint8_t__data [EN] bytes / [FA] بایت‌ها
 * @param  uint32_t__len [EN] how many / [FA] چند تا
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_PutRaw(const uint8_t *uint8_t__data, uint32_t uint32_t__len)
{
    uint32_t uint32_t__done = 0u;

    while (uint32_t__done < uint32_t__len)
    {
        uint32_t uint32_t__room = (uint32_t)UP_XLSX_OUT_CHUNK - (uint32_t)UP_XLSX_T__G__Xlsx.uint16_t__used;
        uint32_t uint32_t__take;

        if (uint32_t__room == 0u)
        {
            func__UpXlsx_Flush();
            uint32_t__room = (uint32_t)UP_XLSX_OUT_CHUNK;
        }

        uint32_t__take = uint32_t__len - uint32_t__done;
        if (uint32_t__take > uint32_t__room)
        {
            uint32_t__take = uint32_t__room;
        }

        (void)memcpy(&UP_XLSX_T__G__Xlsx.char__buffer[UP_XLSX_T__G__Xlsx.uint16_t__used],
                     &uint8_t__data[uint32_t__done], (size_t)uint32_t__take);
        UP_XLSX_T__G__Xlsx.uint16_t__used =
            (uint16_t)((uint32_t)UP_XLSX_T__G__Xlsx.uint16_t__used + uint32_t__take);
        uint32_t__done += uint32_t__take;
    }
}

/* ==================== CRC-32 step ==================== */
/**
 * @brief  [EN] Move a CRC-32 (the one ZIP uses: reflected, polynomial
 *              0xEDB88320) forward by a block of bytes without keeping any
 *              table in flash. Eight shifts per byte is fast enough next to a
 *              Wi-Fi socket, and it saves a kilobyte of the board's flash.
 *         [FA] جلو بردن CRC-32 (همان که ZIP استفاده می‌کند: بازتابی، چندجمله‌ای
 *              0xEDB88320) با یک بلوک بایت، بدون نگه‌داشتن هیچ جدولی در فلش.
 *              هشت شیفت برای هر بایت در برابر سوکت وای‌فای به‌قدر کافی سریع است و
 *              یک کیلوبایت از فلش برد را نگه می‌دارد.
 * @param  uint32_t__crc [EN] running CRC / [FA] CRC جاری
 * @param  uint8_t__data [EN] bytes / [FA] بایت‌ها
 * @param  uint32_t__len [EN] how many / [FA] چند تا
 * @return [EN] the new CRC / [FA] CRC تازه
 */
static uint32_t func__UpXlsx_Crc32(uint32_t uint32_t__crc, const uint8_t *uint8_t__data, uint32_t uint32_t__len)
{
    uint32_t uint32_t__state = uint32_t__crc;

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__len; uint32_t__i++)
    {
        uint32_t__state ^= (uint32_t)uint8_t__data[uint32_t__i];

        for (uint8_t uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
        {
            uint32_t uint32_t__shifted = uint32_t__state >> 1;
            uint32_t uint32_t__mask = (uint32_t__state & 1u) ? 0xEDB88320u : 0u;

            uint32_t__state = uint32_t__shifted ^ uint32_t__mask;
        }
    }

    return uint32_t__state;
}

/* ==================== Put member bytes ==================== */
/**
 * @brief  [EN] Append bytes that ARE a member's content: counted, CRC'd and
 *              eventually written into the workbook.
 *         [FA] افزودن بایت‌هایی که «محتوای» عضو هستند: شمرده می‌شوند، CRC
 *              می‌گیرند و در نهایت در کتاب نوشته می‌شوند.
 * @param  uint8_t__data [EN] bytes / [FA] بایت‌ها
 * @param  uint32_t__len [EN] how many / [FA] چند تا
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Put(const uint8_t *uint8_t__data, uint32_t uint32_t__len)
{
    if (!UP_XLSX_T__G__Xlsx.bool__entryOpen)
    {
        return;
    }

    UP_XLSX_T__G__Xlsx.uint32_t__entryCrc =
        func__UpXlsx_Crc32(UP_XLSX_T__G__Xlsx.uint32_t__entryCrc, uint8_t__data, uint32_t__len);
    UP_XLSX_T__G__Xlsx.uint32_t__entrySize += uint32_t__len;

    func__UpXlsx_PutRaw(uint8_t__data, uint32_t__len);
}

/* ==================== Put text ==================== */
/**
 * @brief  [EN] Append a NUL-terminated piece of XML or text as member content.
 *         [FA] افزودن یک تکه متن یا XML پایان‌یافته با NUL به‌عنوان محتوای عضو.
 * @param  char__text [EN] text / [FA] متن
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Text(const char *char__text)
{
    if (char__text == NULL)
    {
        return;
    }
    func__UpXlsx_Put((const uint8_t *)char__text, (uint32_t)strlen(char__text));
}

/* ==================== Numbers into text / عدد به متن ==================== */
/* ==================== Unsigned digits ==================== */
/**
 * @brief  [EN] Decimal digits of an unsigned value, no sign, no padding. Placed
 *              by hand on purpose: a printf format that could print more digits
 *              than the buffer holds is a compiler warning today and a corrupt
 *              report tomorrow.
 *         [FA] ارقام ده‌دهی یک مقدار بدون علامت، بی‌علامت و بی‌پُرکننده. عمداً
 *              دستی چیده می‌شود: قالب printf که بتواند بیشتر از گنجایش بافر رقم
 *              بزند، امروز هشدار کامپایلر است و فردا گزارش خراب.
 * @param  char__out [EN] destination, at least 11 bytes / [FA] مقصد
 * @param  uint32_t__value [EN] value / [FA] مقدار
 * @return [EN] digits written / [FA] تعداد ارقام نوشته‌شده
 */
static uint16_t func__UpXlsx_Unsigned(char *char__out, uint32_t uint32_t__value)
{
    char char__digits[10];
    uint16_t uint16_t__count = 0u;
    uint16_t uint16_t__out = 0u;

    do
    {
        char__digits[uint16_t__count] = (char)('0' + (char)(uint32_t__value % 10u));
        uint16_t__count++;
        uint32_t__value /= 10u;
    } while ((uint32_t__value > 0u) && (uint16_t__count < 10u));

    while (uint16_t__count > 0u)
    {
        uint16_t__count--;
        char__out[uint16_t__out] = char__digits[uint16_t__count];
        uint16_t__out++;
    }

    return uint16_t__out;
}

/* ==================== Magnitude ==================== */
/**
 * @brief  [EN] The magnitude of a signed value without ever negating an
 *              int32_t outright: the most negative 32-bit number has no positive
 *              twin in the same type, so the sign is peeled off in two steps
 *              (+1, negate, +1) that are exact for every value including that
 *              one. A fraction that came out one unit short would be a volt
 *              displayed as 12.439 instead of 12.440.
 *         [FA] قدر مطلق یک مقدار علامت‌دار بدون قرینه‌کردن مستقیم int32_t:
 *              منفی‌ترین عدد ۳۲ بیتی همتای مثبتی در همان تایپ ندارد، پس علامت در
 *              دو گام برداشته می‌شود (+۱، قرینه، +۱) که برای هر مقداری از جمله
 *              همان یکی دقیق است. اعشاری که یک واحد کم بیاید، ولتاژی است که
 *              ۱۲٫۴۳۹ نشان داده می‌شود به‌جای ۱۲٫۴۴۰.
 * @param  int32_t__value [EN] value / [FA] مقدار
 * @return [EN] magnitude / [FA] قدر مطلق
 */
static uint32_t func__UpXlsx_Magnitude(int32_t int32_t__value)
{
    if (int32_t__value < 0)
    {
        return ((uint32_t)(-(int32_t__value + 1))) + 1u;
    }
    return (uint32_t)int32_t__value;
}

/* ==================== Signed digits ==================== */
/**
 * @brief  [EN] Decimal digits of a signed value with a leading minus when
 *              negative.
 *         [FA] ارقام ده‌دهی یک مقدار علامت‌دار با منهای ابتدایی در صورت منفی.
 * @param  char__out [EN] destination, at least 12 bytes / [FA] مقصد
 * @param  int32_t__value [EN] value / [FA] مقدار
 * @return [EN] characters written / [FA] تعداد نویسه‌های نوشته‌شده
 */
static uint16_t func__UpXlsx_Signed(char *char__out, int32_t int32_t__value)
{
    uint32_t uint32_t__magnitude;
    uint16_t uint16_t__out = 0u;

    if (int32_t__value < 0)
    {
        char__out[0] = '-';
        uint16_t__out++;
    }
    uint32_t__magnitude = func__UpXlsx_Magnitude(int32_t__value);

    return (uint16_t)(uint16_t__out + func__UpXlsx_Unsigned(&char__out[uint16_t__out], uint32_t__magnitude));
}

/* ==================== Fixed-point digits ==================== */
/**
 * @brief  [EN] A scaled integer as a decimal number: 12440 with two decimals
 *              becomes "124.40". The panel keeps millivolts and milliamps as
 *              integers everywhere, so this is the one place that turns them
 *              into the numbers a spreadsheet can add up.
 *         [FA] یک عدد صحیح مقیاس‌شده به‌صورت عدد اعشاری: ۱۲۴۴۰ با دو رقم اعشار
 *              می‌شود «124.40». پنل همه‌جا میلی‌ولت و میلی‌آمپر را صحیح نگه
 *              می‌دارد، پس این تنها جایی است که آن‌ها را به عددی تبدیل می‌کند که
 *              صفحهٔ گسترده بتواند جمع بزند.
 * @param  char__out [EN] destination, at least 16 bytes / [FA] مقصد
 * @param  int32_t__value [EN] value already multiplied by 10^decimals / [FA] مقدار ضرب‌شده در ۱۰^اعشار
 * @param  uint8_t__decimals [EN] 0..3 / [FA] صفر تا ۳
 * @return [EN] characters written / [FA] تعداد نویسه‌های نوشته‌شده
 */
static uint16_t func__UpXlsx_Fixed(char *char__out, int32_t int32_t__value, uint8_t uint8_t__decimals)
{
    uint32_t uint32_t__scale = 1u;
    uint32_t uint32_t__magnitude;
    uint32_t uint32_t__fraction;
    uint16_t uint16_t__out = 0u;

    if (uint8_t__decimals > 3u)
    {
        uint8_t__decimals = 3u;
    }
    for (uint8_t uint8_t__i = 0u; uint8_t__i < uint8_t__decimals; uint8_t__i++)
    {
        uint32_t__scale *= 10u;
    }

    uint16_t__out = func__UpXlsx_Signed(char__out, int32_t__value / (int32_t)uint32_t__scale);

    if (uint8_t__decimals == 0u)
    {
        return uint16_t__out;
    }

    /* [EN] The fraction comes from the MAGNITUDE, never from a negative
       remainder: -12440 with two decimals must read "-124.40" and never
       "-124.-40".
       [FA] اعشار از «قدر مطلق» می‌آید، هرگز از باقی‌ماندهٔ منفی: ۱۲۴۴۰- با دو رقم
       اعشار باید «124.40-» خوانده شود و هرگز «124.-40». */
    uint32_t__magnitude = func__UpXlsx_Magnitude(int32_t__value);
    uint32_t__fraction = uint32_t__magnitude % uint32_t__scale;

    char__out[uint16_t__out] = '.';
    uint16_t__out++;

    /* [EN] Fraction digits, most significant first, zero padded: 7 with two
       decimals is ".07", never ".7".
       [FA] ارقام اعشار، از پرارزش‌ترین، با صفر پُرکننده: ۷ با دو رقم اعشار
       می‌شود «.07» و هرگز «.7». */
    for (uint8_t uint8_t__i = uint8_t__decimals; uint8_t__i > 0u; uint8_t__i--)
    {
        uint32_t uint32_t__divisor = 1u;

        for (uint8_t uint8_t__k = 1u; uint8_t__k < uint8_t__i; uint8_t__k++)
        {
            uint32_t__divisor *= 10u;
        }
        char__out[uint16_t__out] = (char)('0' + (char)((uint32_t__fraction / uint32_t__divisor) % 10u));
        uint16_t__out++;
    }

    return uint16_t__out;
}

/* ==================== Writer bookkeeping / دفترداری نویسنده ==================== */
/**
 * @brief  [EN] How many zip bytes the file has logically reached: what left
 *              through the sink PLUS what is still sitting in the buffer. The
 *              central directory must record these offsets, so asking "how much
 *              did we write" has to include the unflushed tail.
 *         [FA] فایل منطقاً به چند بایت زیپ رسیده: آنچه از مقصد بیرون رفته به‌علاوهٔ
 *              آنچه هنوز در بافر نشسته. دایرکتوری مرکزی همین جای‌ها را ثبت
 *              می‌کند، پس پرسیدن «چقدر نوشتیم» باید دنبالهٔ فلاش‌نشده را هم
 *              بشمارد.
 * @return [EN] byte offset in the file / [FA] جای بایتی در فایل
 */
static uint32_t func__UpXlsx_Tell(void)
{
    return UP_XLSX_T__G__Xlsx.uint32_t__sent + (uint32_t)UP_XLSX_T__G__Xlsx.uint16_t__used;
}

/* ==================== Little-endian writers / نویسنده‌های کم‌ارزش‌ترین بایت اول ==================== */
/**
 * @brief  [EN] One 16-bit ZIP field, low byte first.
 *         [FA] یک میدان ۱۶ بیتی ZIP، بایت کم‌ارزش اول.
 * @param  uint16_t__value [EN] value / [FA] مقدار
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Put16(uint16_t uint16_t__value)
{
    uint8_t uint8_t__bytes[2];

    uint8_t__bytes[0] = (uint8_t)(uint16_t__value & 0xFFu);
    uint8_t__bytes[1] = (uint8_t)((uint16_t__value >> 8) & 0xFFu);
    func__UpXlsx_PutRaw(uint8_t__bytes, 2u);
}

/**
 * @brief  [EN] One 32-bit ZIP field, low byte first.
 *         [FA] یک میدان ۳۲ بیتی ZIP، بایت کم‌ارزش اول.
 * @param  uint32_t__value [EN] value / [FA] مقدار
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Put32(uint32_t uint32_t__value)
{
    uint8_t uint8_t__bytes[4];

    uint8_t__bytes[0] = (uint8_t)(uint32_t__value & 0xFFu);
    uint8_t__bytes[1] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
    uint8_t__bytes[2] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
    uint8_t__bytes[3] = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
    func__UpXlsx_PutRaw(uint8_t__bytes, 4u);
}

/* ==================== Member lifecycle / چرخهٔ عمر عضو ==================== */
/* ==================== Entry open ==================== */
/**
 * @brief  [EN] Start a new member: remember where it begins and write its local
 *              header. The header carries zeroes for CRC and sizes because they
 *              are not known yet - the ZIP flag bit 3 tells every reader to take
 *              them from the data descriptor that follows the data.
 *         [FA] آغاز عضو تازه: به‌خاطر سپردن جای شروع و نوشتن سرصفحهٔ محلی‌اش.
 *              سرصفحه برای CRC و اندازه‌ها صفر می‌گذارد چون هنوز معلوم نیستند -
 *              بیت ۳ فلگ ZIP به هر خواننده‌ای می‌گوید آن‌ها را از توصیفگر دادهٔ
 *              بعد از داده بردارد.
 * @param  char__name [EN] path inside the zip / [FA] مسیر داخل زیپ
 * @return [EN] true when the member was opened / [FA] در صورت باز شدن عضو true
 */
static bool func__UpXlsx_Entry(const char *char__name)
{
    size_t size_t__nameLen;

    if ((char__name == NULL) || UP_XLSX_T__G__Xlsx.bool__entryOpen)
    {
        return false;
    }
    if (UP_XLSX_T__G__Xlsx.uint16_t__entries >= (uint16_t)UP_XLSX_MAX_ENTRIES)
    {
        return false;
    }

    size_t__nameLen = strlen(char__name);
    if ((size_t__nameLen == 0u) || (size_t__nameLen >= (size_t)UP_XLSX_NAME_MAX))
    {
        return false;
    }

    UP_XLSX_T__G__Xlsx.uint32_t__entryOffset = func__UpXlsx_Tell();
    UP_XLSX_T__G__Xlsx.uint32_t__entryCrc = 0xFFFFFFFFu;
    UP_XLSX_T__G__Xlsx.uint32_t__entrySize = 0u;
    UP_XLSX_T__G__Xlsx.uint16_t__entryNameLen = (uint16_t)size_t__nameLen;
    UP_XLSX_T__G__Xlsx.bool__entryOpen = true;

    func__UpXlsx_Put32(UP_XLSX_ZIP_LOCAL_SIG);
    func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_VERSION);
    func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_FLAGS);
    func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_METHOD);
    func__UpXlsx_Put16((uint16_t)UP_XLSX_DOS_TIME);
    func__UpXlsx_Put16((uint16_t)UP_XLSX_DOS_DATE);
    func__UpXlsx_Put32(0u);                       /* [EN] CRC comes later / [FA] CRC بعداً */
    func__UpXlsx_Put32(0u);                       /* [EN] stored size / [FA] اندازهٔ ذخیره‌شده */
    func__UpXlsx_Put32(0u);                       /* [EN] original size / [FA] اندازهٔ اصلی */
    func__UpXlsx_Put16((uint16_t)size_t__nameLen);
    func__UpXlsx_Put16(0u);                       /* [EN] no extra field / [FA] بدون فیلد اضافه */
    func__UpXlsx_PutRaw((const uint8_t *)char__name, (uint32_t)size_t__nameLen);

    return true;
}

/* ==================== Entry close ==================== */
/**
 * @brief  [EN] Finish the open member: write the data descriptor that carries
 *              its real CRC and sizes, then remember it for the central
 *              directory.
 *         [FA] تمام‌کردن عضو باز: نوشتن توصیفگر داده که CRC و اندازه‌های واقعی‌اش
 *              را دارد و سپس به‌خاطر سپردنش برای دایرکتوری مرکزی.
 * @return [EN] true when a member was closed / [FA] در صورت بسته شدن عضو true
 */
static bool func__UpXlsx_End(void)
{
    up_xlsx_entry_t *up_xlsx_entry_t__slot;

    if ((!UP_XLSX_T__G__Xlsx.bool__entryOpen) ||
        (UP_XLSX_T__G__Xlsx.uint16_t__entries >= (uint16_t)UP_XLSX_MAX_ENTRIES))
    {
        return false;
    }

    uint32_t uint32_t__crc = ~UP_XLSX_T__G__Xlsx.uint32_t__entryCrc;
    uint32_t uint32_t__size = UP_XLSX_T__G__Xlsx.uint32_t__entrySize;

    func__UpXlsx_Put32(UP_XLSX_ZIP_DESC_SIG);
    func__UpXlsx_Put32(uint32_t__crc);
    func__UpXlsx_Put32(uint32_t__size);
    func__UpXlsx_Put32(uint32_t__size);

    up_xlsx_entry_t__slot = &UP_XLSX_T__G__Xlsx.up_xlsx_entry_t__entry[UP_XLSX_T__G__Xlsx.uint16_t__entries];
    up_xlsx_entry_t__slot->uint32_t__offset = UP_XLSX_T__G__Xlsx.uint32_t__entryOffset;
    up_xlsx_entry_t__slot->uint32_t__size = uint32_t__size;
    up_xlsx_entry_t__slot->uint32_t__crc = uint32_t__crc;
    up_xlsx_entry_t__slot->uint16_t__nameLen = UP_XLSX_T__G__Xlsx.uint16_t__entryNameLen;

    UP_XLSX_T__G__Xlsx.uint16_t__entries++;
    UP_XLSX_T__G__Xlsx.bool__entryOpen = false;

    return true;
}

/* ==================== Member name copy ==================== */
/**
 * @brief  [EN] Copy the member path into its central-directory slot. Kept
 *              separate from End() so the closing routine stays about the ZIP
 *              and nothing else.
 *         [FA] رونوشت مسیر عضو در خانهٔ دایرکتوری مرکزی‌اش. از End() جدا نگه
 *              داشته شده تا کار بستن، فقط دربارهٔ ZIP باشد و نه چیز دیگر.
 * @param  uint16_t__slot [EN] entry index / [FA] شمارهٔ عضو
 * @param  char__name [EN] path / [FA] مسیر
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_EntryNameSet(uint16_t uint16_t__slot, const char *char__name)
{
    size_t size_t__len = strlen(char__name);

    if (size_t__len > (size_t)(UP_XLSX_NAME_MAX - 1u))
    {
        size_t__len = (size_t)(UP_XLSX_NAME_MAX - 1u);
    }
    (void)memcpy(UP_XLSX_T__G__Xlsx.up_xlsx_entry_t__entry[uint16_t__slot].char__name, char__name, size_t__len);
    UP_XLSX_T__G__Xlsx.up_xlsx_entry_t__entry[uint16_t__slot].char__name[size_t__len] = '\0';
}

/* ==================== One whole member ==================== */
/**
 * @brief  [EN] Open a member and remember its name in one step: what every
 *              caller in this file actually does.
 *         [FA] باز کردن یک عضو و به‌خاطر سپردن نامش در یک گام: کاری که هر
 *              فراخوان در این فایل واقعاً انجام می‌دهد.
 * @param  char__name [EN] path inside the zip / [FA] مسیر داخل زیپ
 * @return [EN] true when opened / [FA] در صورت باز شدن true
 */
static bool func__UpXlsx_Part(const char *char__name)
{
    uint16_t uint16_t__slot = UP_XLSX_T__G__Xlsx.uint16_t__entries;

    if (!func__UpXlsx_Entry(char__name))
    {
        return false;
    }
    func__UpXlsx_EntryNameSet(uint16_t__slot, char__name);
    return true;
}

/* ==================== Member path for a sheet ==================== */
/**
 * @brief  [EN] The path a sheet lives at, built here so the spelling of
 *              "xl/worksheets/sheetN.xml" exists in exactly one place.
 *         [FA] مسیری که یک برگه در آن زندگی می‌کند، همین‌جا ساخته می‌شود تا
 *              املای «xl/worksheets/sheetN.xml» فقط در یک جا وجود داشته باشد.
 * @param  char__out [EN] destination, at least UP_XLSX_NAME_SHEET + 2 bytes
 *                    [FA] مقصد
 * @param  uint16_t__sheet [EN] 1-based sheet number / [FA] شمارهٔ برگه (از ۱)
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_SheetPath(char *char__out, uint16_t uint16_t__sheet)
{
    uint16_t uint16_t__len = 0u;
    const char *char__prefix = "xl/worksheets/sheet";

    while (char__prefix[uint16_t__len] != '\0')
    {
        char__out[uint16_t__len] = char__prefix[uint16_t__len];
        uint16_t__len++;
    }

    if (uint16_t__sheet >= 10u)
    {
        char__out[uint16_t__len] = (char)('0' + ((uint16_t__sheet / 10u) % 10u));
        uint16_t__len++;
    }
    char__out[uint16_t__len] = (char)('0' + (uint16_t__sheet % 10u));
    uint16_t__len++;
    char__out[uint16_t__len] = '.';
    uint16_t__len++;
    char__out[uint16_t__len] = 'x';
    uint16_t__len++;
    char__out[uint16_t__len] = 'm';
    uint16_t__len++;
    char__out[uint16_t__len] = 'l';
    uint16_t__len++;
    char__out[uint16_t__len] = '\0';
}

/* ==================== Workbook members / عضوهای ثابت کتاب ==================== */
/* ==================== Content types ==================== */
/**
 * @brief  [EN] [Content_Types].xml - the map that tells a reader what each part
 *              of the archive is. One override per sheet, one for the workbook
 *              and one for the styles.
 *         [FA] [Content_Types].xml - نقشه‌ای که به خواننده می‌گوید هر بخش آرشیو
 *              چیست. برای هر برگه یک override، یکی برای کتاب و یکی برای سبک‌ها.
 * @param  uint16_t__sheets [EN] sheet count / [FA] تعداد برگه
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_ContentTypes(uint16_t uint16_t__sheets)
{
    static const char CHAR__A__Head[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>";
    char char__part[48];

    func__UpXlsx_Text(CHAR__A__Head);

    for (uint16_t uint16_t__i = 1u; uint16_t__i <= uint16_t__sheets; uint16_t__i++)
    {
        func__UpXlsx_SheetPath(char__part, uint16_t__i);
        func__UpXlsx_Text("<Override PartName=\"/");
        func__UpXlsx_Text(char__part);
        func__UpXlsx_Text("\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>");
    }

    func__UpXlsx_Text("</Types>");
}

/* ==================== Package relationships ==================== */
/**
 * @brief  [EN] _rels/.rels - the single rule that points a reader at the
 *              workbook inside the archive.
 *         [FA] _rels/.rels - همان یک قاعده‌ای که خواننده را به کتاب داخل آرشیو
 *              راهنمایی می‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_PackageRels(void)
{
    static const char CHAR__A__Rels[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\""
        " Target=\"xl/workbook.xml\"/>"
        "</Relationships>";

    func__UpXlsx_Text(CHAR__A__Rels);
}

/* ==================== Workbook ==================== */
/**
 * @brief  [EN] xl/workbook.xml - the sheet list. The names are the ones the
 *              caller passed, in the order the caller will fill them.
 *         [FA] xl/workbook.xml - فهرست برگه‌ها. نام‌ها همان‌هایی هستند که
 *              فراخوان داده، به همان ترتیبی که پُرشان می‌کند.
 * @param  char__names [EN] sheet names, in order / [FA] نام برگه‌ها، به ترتیب
 * @param  uint16_t__sheets [EN] how many / [FA] چند تا
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Workbook(const char *const char__names[], uint16_t uint16_t__sheets)
{
    char char__id[16];
    uint16_t uint16_t__len;

    func__UpXlsx_Text("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                      "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\""
                      " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                      "<workbookPr/><sheets>");

    for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__sheets; uint16_t__i++)
    {
        func__UpXlsx_Text("<sheet name=\"");
        func__UpXlsx_Text((char__names[uint16_t__i] != NULL) ? char__names[uint16_t__i] : "sheet");
        uint16_t__len = func__UpXlsx_Unsigned(char__id, (uint32_t)uint16_t__i + 1u);
        char__id[uint16_t__len] = '\0';
        func__UpXlsx_Text("\" sheetId=\"");
        func__UpXlsx_Text(char__id);
        func__UpXlsx_Text("\" r:id=\"rId");
        func__UpXlsx_Text(char__id);
        func__UpXlsx_Text("\"/>");
    }

    func__UpXlsx_Text("</sheets></workbook>");
}

/* ==================== Workbook relationships ==================== */
/**
 * @brief  [EN] xl/_rels/workbook.xml.rels - one relationship per sheet plus the
 *              styles part, numbered exactly like the sheet list above.
 *         [FA] xl/_rels/workbook.xml.rels - یک رابطه برای هر برگه به‌علاوهٔ بخش
 *              سبک‌ها، با همان شماره‌گذاری فهرست برگه‌های بالا.
 * @param  uint16_t__sheets [EN] sheet count / [FA] تعداد برگه
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_WorkbookRels(uint16_t uint16_t__sheets)
{
    char char__id[16];
    char char__target[32];
    uint16_t uint16_t__len;

    func__UpXlsx_Text("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                      "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");

    for (uint16_t uint16_t__i = 1u; uint16_t__i <= uint16_t__sheets; uint16_t__i++)
    {
        func__UpXlsx_SheetPath(char__target, uint16_t__i);
        uint16_t__len = func__UpXlsx_Unsigned(char__id, (uint32_t)uint16_t__i);
        char__id[uint16_t__len] = '\0';
        func__UpXlsx_Text("<Relationship Id=\"rId");
        func__UpXlsx_Text(char__id);
        func__UpXlsx_Text("\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\""
                          " Target=\"");
        /* [EN] The workbook's rels are relative to xl/, so the path loses its
           own "xl/" prefix here and nowhere else.
           [FA] روابط کتاب نسبت به پوشهٔ xl/اند، پس مسیر فقط همین‌جا پیشوند
           «xl/» خودش را از دست می‌دهد و جای دیگری نه. */
        func__UpXlsx_Text(&char__target[3]);
        func__UpXlsx_Text("\"/>");
    }

    uint16_t__len = func__UpXlsx_Unsigned(char__id, (uint32_t)uint16_t__sheets + 1u);
    char__id[uint16_t__len] = '\0';
    func__UpXlsx_Text("<Relationship Id=\"rId");
    func__UpXlsx_Text(char__id);
    func__UpXlsx_Text("\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\""
                      " Target=\"styles.xml\"/></Relationships>");
}

/* ==================== Styles ==================== */
/**
 * @brief  [EN] xl/styles.xml - fonts, fills, borders and the six cell formats
 *              the sheets refer to by number. Deliberately small: a report that
 *              looks tidy in a spreadsheet does not need a theme.
 *         [FA] xl/styles.xml - قلم‌ها، پر‌کننده‌ها، قاب‌ها و شش قالب سلولی که
 *              برگه‌ها با شماره به آن‌ها ارجاع می‌دهند. عمداً کوچک: گزارشی که در
 *              صفحهٔ گسترده مرتب دیده شود به هیچ تمی نیاز ندارد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Styles(void)
{
    static const char CHAR__A__Styles[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"3\">"
        "<font><sz val=\"10\"/><name val=\"Tahoma\"/></font>"
        "<font><b/><sz val=\"10\"/><color rgb=\"FFFFFFFF\"/><name val=\"Tahoma\"/></font>"
        "<font><b/><sz val=\"12\"/><color rgb=\"FF0F3D5C\"/><name val=\"Tahoma\"/></font>"
        "</fonts>"
        "<fills count=\"4\">"
        "<fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF14547A\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFE8F1F8\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "</fills>"
        "<borders count=\"2\">"
        "<border><left/><right/><top/><bottom/><diagonal/></border>"
        "<border><left style=\"thin\"><color rgb=\"FFB9CBDB\"/></left>"
        "<right style=\"thin\"><color rgb=\"FFB9CBDB\"/></right>"
        "<top style=\"thin\"><color rgb=\"FFB9CBDB\"/></top>"
        "<bottom style=\"thin\"><color rgb=\"FFB9CBDB\"/></bottom><diagonal/></border>"
        "</borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"6\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"1\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\">"
        "<alignment horizontal=\"center\" vertical=\"center\" wrapText=\"1\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\">"
        "<alignment horizontal=\"right\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\" applyAlignment=\"1\">"
        "<alignment horizontal=\"center\" vertical=\"center\"/></xf>"
        "<xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"3\" borderId=\"1\" xfId=\"0\" applyFill=\"1\" applyBorder=\"1\"/>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>";

    func__UpXlsx_Text(CHAR__A__Styles);
}

/* ==================== Workbook start ==================== */
/**
 * @brief  [EN] Begin a workbook: reset the writer, point it at a sink and lay
 *              down the five fixed members every reader expects. Each stored
 *              sheet member is opened later by the report itself.
 *         [FA] آغاز یک کتاب: صفر کردن نویسنده، وصل‌کردنش به مقصد و نوشتن همان
 *              پنج عضو ثابتی که هر خواننده‌ای انتظار دارد. هر عضو برگه بعداً
 *              خودِ گزارش باز می‌کند.
 * @param  char__names [EN] sheet names / [FA] نام برگه‌ها
 * @param  uint16_t__sheets [EN] sheet count, must fit in the tables
 *                          [FA] تعداد برگه، باید در جدول‌ها جا شود
 * @param  up_xlsx_sink_t__sink [EN] destination function / [FA] تابع مقصد
 * @return [EN] true when the workbook was started / [FA] در صورت شروع true
 */
static bool func__UpXlsx_Begin(const char *const char__names[], uint16_t uint16_t__sheets,
                               up_xlsx_sink_t up_xlsx_sink_t__sink)
{
    uint16_t uint16_t__needed = (uint16_t)(5u + uint16_t__sheets);

    if ((uint16_t__sheets == 0u) || (uint16_t__sheets > 6u) ||
        (uint16_t__needed > (uint16_t)UP_XLSX_MAX_ENTRIES))
    {
        return false;
    }

    (void)memset(&UP_XLSX_T__G__Xlsx, 0, sizeof(UP_XLSX_T__G__Xlsx));
    UP_XLSX_T__G__Xlsx.up_xlsx_sink_t__sink = up_xlsx_sink_t__sink;

    if (!func__UpXlsx_Part("[Content_Types].xml")) { return false; }
    func__UpXlsx_ContentTypes(uint16_t__sheets);
    if (!func__UpXlsx_End()) { return false; }

    if (!func__UpXlsx_Part("_rels/.rels")) { return false; }
    func__UpXlsx_PackageRels();
    if (!func__UpXlsx_End()) { return false; }

    if (!func__UpXlsx_Part("xl/workbook.xml")) { return false; }
    func__UpXlsx_Workbook(char__names, uint16_t__sheets);
    if (!func__UpXlsx_End()) { return false; }

    if (!func__UpXlsx_Part("xl/_rels/workbook.xml.rels")) { return false; }
    func__UpXlsx_WorkbookRels(uint16_t__sheets);
    if (!func__UpXlsx_End()) { return false; }

    if (!func__UpXlsx_Part("xl/styles.xml")) { return false; }
    func__UpXlsx_Styles();
    if (!func__UpXlsx_End()) { return false; }

    return true;
}

/* ==================== Sheet member ==================== */
/**
 * @brief  [EN] Open one sheet's member so the report can start writing rows
 *              into it.
 *         [FA] باز کردن عضو یک برگه تا گزارش بتواند نوشتن ردیف‌ها را در آن
 *              آغاز کند.
 * @param  uint16_t__sheet [EN] 1-based sheet number / [FA] شمارهٔ برگه (از ۱)
 * @return [EN] true when opened / [FA] در صورت باز شدن true
 */
static bool func__UpXlsx_SheetMember(uint16_t uint16_t__sheet)
{
    char char__path[UP_XLSX_NAME_SHEET + 2u];

    func__UpXlsx_SheetPath(char__path, uint16_t__sheet);
    return func__UpXlsx_Part(char__path);
}

/* ==================== Workbook finish ==================== */
/**
 * @brief  [EN] Close the workbook: central directory, end-of-directory record,
 *              then flush. After this the sink has seen a complete .xlsx and
 *              the caller only has to end the HTTP response.
 *         [FA] بستن کتاب: دایرکتوری مرکزی، رکورد پایان دایرکتوری و سپس فلاش.
 *              بعد از این، مقصد یک فایل .xlsx کامل دیده و فراخوان فقط باید
 *              پاسخ HTTP را تمام کند.
 * @return [EN] true when the file is complete / [FA] در صورت کامل‌بودن فایل true
 */
static bool func__UpXlsx_Finish(void)
{
    uint32_t uint32_t__centralOffset;
    uint32_t uint32_t__centralSize;
    uint16_t uint16_t__count;

    if (UP_XLSX_T__G__Xlsx.bool__entryOpen)
    {
        if (!func__UpXlsx_End())
        {
            return false;
        }
    }

    uint16_t__count = UP_XLSX_T__G__Xlsx.uint16_t__entries;
    uint32_t__centralOffset = func__UpXlsx_Tell();

    for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__count; uint16_t__i++)
    {
        up_xlsx_entry_t *up_xlsx_entry_t__row = &UP_XLSX_T__G__Xlsx.up_xlsx_entry_t__entry[uint16_t__i];

        func__UpXlsx_Put32(UP_XLSX_ZIP_CENTRAL_SIG);
        func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_VERSION);          /* [EN] made by / [FA] ساخته‌شده با */
        func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_VERSION);          /* [EN] needed to open / [FA] لازم برای باز شدن */
        func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_FLAGS);
        func__UpXlsx_Put16((uint16_t)UP_XLSX_ZIP_METHOD);
        func__UpXlsx_Put16((uint16_t)UP_XLSX_DOS_TIME);
        func__UpXlsx_Put16((uint16_t)UP_XLSX_DOS_DATE);
        func__UpXlsx_Put32(up_xlsx_entry_t__row->uint32_t__crc);
        func__UpXlsx_Put32(up_xlsx_entry_t__row->uint32_t__size);
        func__UpXlsx_Put32(up_xlsx_entry_t__row->uint32_t__size);
        func__UpXlsx_Put16(up_xlsx_entry_t__row->uint16_t__nameLen);
        func__UpXlsx_Put16(0u);                                     /* [EN] extra field / [FA] فیلد اضافه */
        func__UpXlsx_Put16(0u);                                     /* [EN] comment / [FA] توضیح */
        func__UpXlsx_Put16(0u);                                     /* [EN] disk number / [FA] شمارهٔ دیسک */
        func__UpXlsx_Put16(0u);                                     /* [EN] internal attributes / [FA] ویژگی‌های داخلی */
        func__UpXlsx_Put32(0u);                                     /* [EN] external attributes / [FA] ویژگی‌های خارجی */
        func__UpXlsx_Put32(up_xlsx_entry_t__row->uint32_t__offset);
        func__UpXlsx_PutRaw((const uint8_t *)up_xlsx_entry_t__row->char__name,
                            (uint32_t)up_xlsx_entry_t__row->uint16_t__nameLen);
    }

    uint32_t__centralSize = func__UpXlsx_Tell() - uint32_t__centralOffset;

    func__UpXlsx_Put32(UP_XLSX_ZIP_EOCD_SIG);
    func__UpXlsx_Put16(0u);                                         /* [EN] this disk / [FA] همین دیسک */
    func__UpXlsx_Put16(0u);                                         /* [EN] directory disk / [FA] دیسک دایرکتوری */
    func__UpXlsx_Put16(uint16_t__count);                            /* [EN] entries on this disk / [FA] عضوهای این دیسک */
    func__UpXlsx_Put16(uint16_t__count);                            /* [EN] entries in total / [FA] عضوهای کل */
    func__UpXlsx_Put32(uint32_t__centralSize);
    func__UpXlsx_Put32(uint32_t__centralOffset);
    func__UpXlsx_Put16(0u);                                         /* [EN] no comment / [FA] بدون توضیح */

    func__UpXlsx_Flush();

    return true;
}

/* ==================== XML escaping / فرار دادن XML ==================== */
/**
 * @brief  [EN] Write text as XML content, escaping the four characters that
 *              would otherwise end the element early. Persian letters are UTF-8
 *              bytes above 0x7F and pass through untouched; that is exactly why
 *              the report can be in Persian at all.
 *         [FA] نوشتن متن به‌عنوان محتوای XML، با فرار دادن آن چهار نویسه‌ای که
 *              وگرنه عنصر را زود تمام می‌کنند. حروف فارسی بایت‌های UTF-8 بالای
 *              0x7F هستند و دست‌نخورده رد می‌شوند؛ دقیقاً همین است که گزارش را
 *              فارسی‌شدنی می‌کند.
 * @param  char__text [EN] text / [FA] متن
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Escaped(const char *char__text)
{
    uint32_t uint32_t__i = 0u;

    if (char__text == NULL)
    {
        return;
    }

    while (char__text[uint32_t__i] != '\0')
    {
        char char__one = char__text[uint32_t__i];

        if (char__one == '&')
        {
            func__UpXlsx_Text("&amp;");
        }
        else if (char__one == '<')
        {
            func__UpXlsx_Text("&lt;");
        }
        else if (char__one == '>')
        {
            func__UpXlsx_Text("&gt;");
        }
        else if (char__one == '"')
        {
            func__UpXlsx_Text("&quot;");
        }
        else
        {
            func__UpXlsx_Put((const uint8_t *)&char__one, 1u);
        }
        uint32_t__i++;
    }
}

/* ==================== Sheet skeleton / اسکلت برگه ==================== */
/* ==================== Sheet open ==================== */
/**
 * @brief  [EN] Open a worksheet: the XML declaration, the RTL sheet view with
 *              the first row frozen, and the row height. Every sheet in this
 *              report is right-to-left because every sheet is read by a Persian
 *              speaker.
 *         [FA] باز کردن یک برگه: اعلان XML، نمای برگهٔ راست‌به‌چپ با ردیف‌های
 *              ثابت‌شده و ارتفاع ردیف. هر برگهٔ این گزارش راست‌به‌چپ است چون هر
 *              برگه را یک فارسی‌زبان می‌خواند.
 * @param  uint8_t__frozenRows [EN] rows that stay on screen while scrolling;
 *                             1 = just a header row, 2 = title plus header
 *                             [FA] ردیف‌هایی که موقع اسکرول می‌مانند؛ ۱ یعنی فقط
 *                             سرصفحه و ۲ یعنی عنوان به‌علاوهٔ سرصفحه
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_SheetOpen(uint8_t uint8_t__frozenRows)
{
    char char__text[16];
    uint16_t uint16_t__len;
    uint32_t uint32_t__first = (uint32_t)uint8_t__frozenRows + 1u;

    func__UpXlsx_Text("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                      "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                      "<sheetViews><sheetView rightToLeft=\"1\" workbookViewId=\"0\"><pane ySplit=\"");
    uint16_t__len = func__UpXlsx_Unsigned(char__text, (uint32_t)uint8_t__frozenRows);
    char__text[uint16_t__len] = '\0';
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text("\" topLeftCell=\"A");
    uint16_t__len = func__UpXlsx_Unsigned(char__text, uint32_t__first);
    char__text[uint16_t__len] = '\0';
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text("\" activePane=\"bottomLeft\" state=\"frozen\"/><selection pane=\"bottomLeft\" activeCell=\"A");
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text("\" sqref=\"A");
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text("\"/></sheetView></sheetViews><sheetFormatPr defaultRowHeight=\"15\"/>");
}

/* ==================== Sheet data open ==================== */
/**
 * @brief  [EN] Open the row container. It comes after the column widths,
 *              because a worksheet's elements have a fixed order and a file that
 *              ignores it is the file a spreadsheet offers to "repair".
 *         [FA] باز کردن ظرف ردیف‌ها. بعد از عرض ستون‌ها می‌آید، چون اجزای یک
 *              برگه ترتیب ثابتی دارند و فایلی که آن را نادیده بگیرد همان فایلی
 *              است که صفحهٔ گسترده پیشنهاد «ترمیم» می‌دهد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_DataOpen(void)
{
    func__UpXlsx_Text("<sheetData>");
}

/* ==================== Sheet close ==================== */
/**
 * @brief  [EN] Close a worksheet, filter bar included: the header row is frozen
 *              and filterable, which is the difference between a dump and a
 *              report somebody uses.
 *         [FA] بستن یک برگه، همراه با نوار فیلتر: ردیف سرصفحه ثابت و فیلترپذیر
 *              است، و همین تفاوت میان «ریختن داده» و گزارشی است که کسی واقعاً
 *              استفاده می‌کند.
 * @param  uint16_t__cols [EN] column count of the table / [FA] تعداد ستون‌های جدول
 * @param  uint16_t__headerRow [EN] the row the filter bar belongs to / [FA] ردیفی که نوار فیلتر مال آن است
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_SheetClose(uint16_t uint16_t__cols, uint16_t uint16_t__headerRow)
{
    char char__text[16];
    char char__last[4];
    uint16_t uint16_t__len;

    func__UpXlsx_Text("</sheetData><autoFilter ref=\"A");
    uint16_t__len = func__UpXlsx_Unsigned(char__text, (uint32_t)uint16_t__headerRow);
    char__text[uint16_t__len] = '\0';
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text(":");

    if (uint16_t__cols > 26u)
    {
        char__last[0] = (char)('A' + (char)(((uint16_t__cols - 1u) / 26u) - 1u));
        char__last[1] = (char)('A' + (char)((uint16_t__cols - 1u) % 26u));
        char__last[2] = '\0';
    }
    else
    {
        char__last[0] = (char)('A' + (char)(uint16_t__cols - 1u));
        char__last[1] = '\0';
    }
    func__UpXlsx_Text(char__last);
    uint16_t__len = func__UpXlsx_Unsigned(char__text, (uint32_t)uint16_t__headerRow);
    char__text[uint16_t__len] = '\0';
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text("\"/></worksheet>");
}

/* ==================== Column widths ==================== */
/**
 * @brief  [EN] Column widths, so the operator does not have to drag every
 *              column before the report is readable.
 *         [FA] عرض ستون‌ها، تا اپراتور مجبور نشود پیش از خواناشدن گزارش هر ستون
 *              را با ماوس بکشد.
 * @param  uint8_t__widths [EN] width per column, in characters / [FA] عرض هر ستون به نویسه
 * @param  uint16_t__cols [EN] how many / [FA] چند تا
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Cols(const uint8_t *uint8_t__widths, uint16_t uint16_t__cols)
{
    char char__text[16];

    if ((uint8_t__widths == NULL) || (uint16_t__cols == 0u))
    {
        return;
    }

    func__UpXlsx_Text("<cols>");
    for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__cols; uint16_t__i++)
    {
        uint16_t uint16_t__len;

        func__UpXlsx_Text("<col min=\"");
        uint16_t__len = func__UpXlsx_Unsigned(char__text, (uint32_t)uint16_t__i + 1u);
        char__text[uint16_t__len] = '\0';
        func__UpXlsx_Text(char__text);
        func__UpXlsx_Text("\" max=\"");
        uint16_t__len = func__UpXlsx_Unsigned(char__text, (uint32_t)uint16_t__i + 1u);
        char__text[uint16_t__len] = '\0';
        func__UpXlsx_Text(char__text);
        func__UpXlsx_Text("\" width=\"");
        uint16_t__len = func__UpXlsx_Unsigned(char__text, (uint32_t)uint8_t__widths[uint16_t__i]);
        char__text[uint16_t__len] = '\0';
        func__UpXlsx_Text(char__text);
        func__UpXlsx_Text("\" customWidth=\"1\"/>");
    }
    func__UpXlsx_Text("</cols>");
}

/* ==================== Row open ==================== */
/**
 * @brief  [EN] Start a row of cells.
 *         [FA] آغاز یک ردیف سلول.
 * @param  uint32_t__row [EN] 1-based row number / [FA] شمارهٔ ردیف (از ۱)
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_RowOpen(uint32_t uint32_t__row)
{
    char char__text[16];
    uint16_t uint16_t__len = func__UpXlsx_Unsigned(char__text, uint32_t__row);

    char__text[uint16_t__len] = '\0';
    func__UpXlsx_Text("<row r=\"");
    func__UpXlsx_Text(char__text);
    func__UpXlsx_Text("\">");
}

/**
 * @brief  [EN] Finish a row.
 *         [FA] تمام‌کردن یک ردیف.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_RowClose(void)
{
    func__UpXlsx_Text("</row>");
}

/* ==================== Cell reference ==================== */
/**
 * @brief  [EN] Build an A1-style cell reference: column letters then the row
 *              number, so column 27 row 4 is "AA4".
 *         [FA] ساختن ارجاع سلول به سبک A1: حروف ستون و بعد شمارهٔ ردیف، یعنی
 *              ستون ۲۷ ردیف ۴ می‌شود «AA4».
 * @param  char__out [EN] destination, at least 10 bytes / [FA] مقصد
 * @param  uint16_t__col [EN] 0-based column / [FA] ستون (از ۰)
 * @param  uint32_t__row [EN] 1-based row / [FA] ردیف (از ۱)
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_Ref(char *char__out, uint16_t uint16_t__col, uint32_t uint32_t__row)
{
    char char__letters[3];
    uint16_t uint16_t__count = 0u;
    uint32_t uint32_t__rest = (uint32_t)uint16_t__col;
    uint16_t uint16_t__out = 0u;

    do
    {
        char__letters[uint16_t__count] = (char)('A' + (char)(uint32_t__rest % 26u));
        uint16_t__count++;
        uint32_t__rest = (uint32_t__rest / 26u);
        if (uint32_t__rest == 0u)
        {
            break;
        }
        uint32_t__rest--;
    } while (uint16_t__count < 3u);

    while (uint16_t__count > 0u)
    {
        uint16_t__count--;
        char__out[uint16_t__out] = char__letters[uint16_t__count];
        uint16_t__out++;
    }

    uint16_t__out = (uint16_t)(uint16_t__out + func__UpXlsx_Unsigned(&char__out[uint16_t__out], uint32_t__row));
    char__out[uint16_t__out] = '\0';
}

/* ==================== Cell with text ==================== */
/**
 * @brief  [EN] Write one cell holding text, as an inline string: no shared
 *              string table, no second pass, and the file stays readable.
 *         [FA] نوشتن سلولی که متن دارد، به‌صورت رشتهٔ درون‌خطی: بدون جدول
 *              رشته‌های مشترک، بدون گذر دوم و فایل خوانا می‌ماند.
 * @param  uint32_t__row [EN] 1-based row / [FA] ردیف
 * @param  uint16_t__col [EN] 0-based column / [FA] ستون
 * @param  uint8_t__style [EN] UP_XLSX_STYLE_* / [FA] سبک سلول
 * @param  char__text [EN] text, already safe to write / [FA] متن، آماده برای نوشتن
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_CellText(uint32_t uint32_t__row, uint16_t uint16_t__col, uint8_t uint8_t__style,
                                  const char *char__text)
{
    char char__ref[10];
    char char__style[4];
    uint16_t uint16_t__len;

    func__UpXlsx_Ref(char__ref, uint16_t__col, uint32_t__row);
    uint16_t__len = func__UpXlsx_Unsigned(char__style, (uint32_t)uint8_t__style);
    char__style[uint16_t__len] = '\0';

    func__UpXlsx_Text("<c r=\"");
    func__UpXlsx_Text(char__ref);
    func__UpXlsx_Text("\" s=\"");
    func__UpXlsx_Text(char__style);
    func__UpXlsx_Text("\" t=\"inlineStr\"><is><t xml:space=\"preserve\">");
    func__UpXlsx_Escaped(char__text);
    func__UpXlsx_Text("</t></is></c>");
}

/* ==================== Cell with a number ==================== */
/**
 * @brief  [EN] Write one cell holding a number, so the spreadsheet can add,
 *              average and chart the column instead of sorting strings.
 *         [FA] نوشتن سلولی که عدد دارد، تا صفحهٔ گسترده بتواند جمع و میانگین
 *              بگیرد و نمودار بکشد، به‌جای مرتب‌کردن رشته‌ها.
 * @param  uint32_t__row [EN] 1-based row / [FA] ردیف
 * @param  uint16_t__col [EN] 0-based column / [FA] ستون
 * @param  uint8_t__style [EN] UP_XLSX_STYLE_* / [FA] سبک سلول
 * @param  int32_t__value [EN] value / [FA] مقدار
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_CellInt(uint32_t uint32_t__row, uint16_t uint16_t__col, uint8_t uint8_t__style,
                                 int32_t int32_t__value)
{
    char char__ref[10];
    char char__style[4];
    char char__value[16];
    uint16_t uint16_t__len;

    func__UpXlsx_Ref(char__ref, uint16_t__col, uint32_t__row);
    uint16_t__len = func__UpXlsx_Unsigned(char__style, (uint32_t)uint8_t__style);
    char__style[uint16_t__len] = '\0';
    uint16_t__len = func__UpXlsx_Signed(char__value, int32_t__value);
    char__value[uint16_t__len] = '\0';

    func__UpXlsx_Text("<c r=\"");
    func__UpXlsx_Text(char__ref);
    func__UpXlsx_Text("\" s=\"");
    func__UpXlsx_Text(char__style);
    func__UpXlsx_Text("\"><v>");
    func__UpXlsx_Text(char__value);
    func__UpXlsx_Text("</v></c>");
}

/* ==================== Cell with a decimal number ==================== */
/**
 * @brief  [EN] Write one cell holding a number that has decimals, from the
 *              scaled integer the panel keeps: 12440 with 3 decimals is 12.440,
 *              which is what millivolts look like as volts.
 *         [FA] نوشتن سلولی که عدد اعشاری دارد، از همان عدد صحیح مقیاس‌شدهٔ پنل:
 *              ۱۲۴۴۰ با ۳ رقم اعشار می‌شود 12.440، یعنی همان‌طور که میلی‌ولت
 *              به‌صورت ولت دیده می‌شود.
 * @param  uint32_t__row [EN] 1-based row / [FA] ردیف
 * @param  uint16_t__col [EN] 0-based column / [FA] ستون
 * @param  uint8_t__style [EN] UP_XLSX_STYLE_* / [FA] سبک سلول
 * @param  int32_t__value [EN] scaled value / [FA] مقدار مقیاس‌شده
 * @param  uint8_t__decimals [EN] number of decimals / [FA] تعداد اعشار
 * @return [EN] None / [FA] ندارد
 */
static void func__UpXlsx_CellFixed(uint32_t uint32_t__row, uint16_t uint16_t__col, uint8_t uint8_t__style,
                                   int32_t int32_t__value, uint8_t uint8_t__decimals)
{
    char char__ref[10];
    char char__style[4];
    char char__value[20];
    uint16_t uint16_t__len;

    func__UpXlsx_Ref(char__ref, uint16_t__col, uint32_t__row);
    uint16_t__len = func__UpXlsx_Unsigned(char__style, (uint32_t)uint8_t__style);
    char__style[uint16_t__len] = '\0';
    uint16_t__len = func__UpXlsx_Fixed(char__value, int32_t__value, uint8_t__decimals);
    char__value[uint16_t__len] = '\0';

    func__UpXlsx_Text("<c r=\"");
    func__UpXlsx_Text(char__ref);
    func__UpXlsx_Text("\" s=\"");
    func__UpXlsx_Text(char__style);
    func__UpXlsx_Text("\"><v>");
    func__UpXlsx_Text(char__value);
    func__UpXlsx_Text("</v></c>");
}

#endif /* UP_XLSX_H */
