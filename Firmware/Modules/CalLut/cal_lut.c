/**
 * @file    cal_lut.c
 * @brief   [EN] Runtime bench LUT storage in its own flash block (v1.66).
 *          [FA] نگهداری جدول بنچ در بلوک فلش مخصوص خودش (v1.66).
 *
 * @note    [EN] Contract in cal_lut.h. Everything below the "pure logic"
 *              banner is host-testable: define CAL_LUT_HOST_TEST and the
 *              module compiles against a RAM-emulated flash with the board
 *              headers left out, so the host test exercises THIS state
 *              machine rather than a copy of it.
 *          [FA] قرارداد در cal_lut.h. هرچه زیر بنرِ «منطق خالص» است روی
 *              هاست تست‌پذیر است: با تعریف CAL_LUT_HOST_TEST ماژول روی فلشِ
 *              شبیه‌سازی‌شدهٔ RAM و بدون هدرهای برد کامپایل می‌شود، پس تست
 *              «همین» ماشین حالت را اجرا می‌کند نه رونوشتش را.
 */

#include "cal_lut.h"

#ifndef CAL_LUT_HOST_TEST
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "modules_enable.h"
#include "rtos_time.h"
#include "cmsis_os2.h"
#include "stm32f1xx.h"
#if MODULE_CHARGER
#include "charger.h"
#endif
#else
/* [EN] Host test: the flash driver is emulated in RAM by the harness, so
   only its prototypes are needed here.
   [FA] تست هاست: درایور فلش در RAM شبیه‌سازی می‌شود و فقط پروتوتایپ لازم است. */
bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress);
bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count);
#endif /* CAL_LUT_HOST_TEST */

#include <stddef.h>
#include <string.h>

/* [EN] The record must fit the page, and the page span is DERIVED from the
   two page addresses instead of retyped - the one claim in this project
   that has gone stale most often is hand arithmetic in a comment.
   [FA] رکورد باید در صفحه جا شود و اندازهٔ صفحه از فاصلهٔ دو آدرس مشتق
   می‌شود نه تایپ دوباره. */
_Static_assert(sizeof(cal_lut_record_t) <=
                   (CAL_LUT_PAGE_B_ADDR - CAL_LUT_PAGE_A_ADDR),
               "CalLut record must fit inside one flash page");

/* [EN] 2026-10-05: two facts are now proved by the compiler instead of by a
   comment. (1) The LUT pages sit inside the window the flash driver may
   erase (bsp_flash.h) - the driver's old hard-coded range excluded them, so
   a LUT save was refused on the board. The second fact - that the LUT block
   never collides with the EspLink parameter banks - CANNOT be asserted here,
   because this module must not reference the parameter module at all
   (separate storage was the whole point of the order). It is a cross-file
   invariant in tools/audit_consistency.py instead of the hand-written
   address the old assert used, which went stale the moment v1.80 moved the
   parameter bank.
   [FA] از امروز کامپایلر اثبات می‌کند که صفحه‌های جدول داخل پنجرهٔ مجاز
   درایور فلش‌اند - بازهٔ ثابت قبلیِ درایور آن‌ها را بیرون می‌گذاشت و ذخیرهٔ
   جدول روی برد رد می‌شد. بررسی دوم، یعنی برخورد‌نکردن با بانک‌های پارامتر،
   نمی‌تواند اینجا باشد چون این ماژول نباید به ماژول پارامترها ارجاع دهد
   (جدا‌بودن مسیر ذخیره‌سازی اصل دستور بود)؛ پس به‌شکل invariant بین‌فایلی در
   ‎tools/audit_consistency.py‎ آمده، نه آدرس دستی‌ای که با جابه‌جایی بانک در
   v1.80 کهنه شد. */
#ifdef BSP_FLASH_STORAGE_BASE_ADDR
_Static_assert((CAL_LUT_PAGE_A_ADDR >= BSP_FLASH_STORAGE_BASE_ADDR) &&
                   ((CAL_LUT_PAGE_B_ADDR +
                     (CAL_LUT_PAGE_B_ADDR - CAL_LUT_PAGE_A_ADDR)) <=
                    BSP_FLASH_STORAGE_END_ADDR),
               "CalLut pages must sit inside the writable flash window");
#endif

/* [EN] Flash programming is halfword-wide: an odd-sized record would leave
   the last byte unwritten.  [FA] نوشتن فلش نیم‌کلمه‌ای است. */
_Static_assert((sizeof(cal_lut_record_t) % 2u) == 0u,
               "CalLut record size must be even (halfword programming)");

/* ====================================================================
 * ===== CalLut pure record logic (host-testable, no flash) ===========
 * ==================================================================== */

/**
 * @brief  [EN] Reflected CRC32, bit-by-bit (no table in flash).
 *         [FA] CRC32 بازتابیده، بیت‌به‌بیت (بدون جدول در فلش).
 */
/**
 * @brief  [EN] Fold one byte into a running reflected CRC32. Exposed as a
 *              separate step so the staged table can be CRC-ed as a STREAM:
 *              the comm task has a 1 KiB stack and a 386-byte scratch
 *              buffer there is exactly the kind of quiet overflow nothing
 *              would report.
 *         [FA] افزودن یک بایت به CRC32 در حال اجرا. جدا شد تا جدول
 *              چیده‌شده «جریانی» CRC شود: پشتهٔ تسک ارتباط ۱KB است و بافر
 *              ۳۸۶ بایتی روی آن دقیقاً همان سرریز بی‌صدایی است که هیچ‌چیز
 *              گزارشش نمی‌کند.
 */
static uint32_t func__CalLut_Crc32Byte(uint32_t uint32_t__crc, uint8_t uint8_t__byte)
{
    uint8_t uint8_t__bit;

    uint32_t__crc ^= (uint32_t)uint8_t__byte;
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
    return uint32_t__crc;
}

uint32_t func__CalLut_Crc32(const uint8_t *uint8_t__A__Data,
                            uint32_t uint32_t__length)
{
    uint32_t uint32_t__crc = 0xFFFFFFFFu;
    uint32_t uint32_t__i;

    for (uint32_t__i = 0u; uint32_t__i < uint32_t__length; uint32_t__i++)
    {
        uint32_t__crc = func__CalLut_Crc32Byte(uint32_t__crc,
                                               uint8_t__A__Data[uint32_t__i]);
    }

    return (uint32_t__crc ^ 0xFFFFFFFFu);
}

/**
 * @brief  [EN] Axis legality for ONE channel: 0 points = "not overridden"
 *              and is legal; otherwise 2..MAX points, a strictly increasing
 *              chain axis (a flat segment divides by zero, a falling one
 *              makes the search meaningless) and a non-decreasing power
 *              axis (the interpolation computes yHigh - yLow UNSIGNED, so a
 *              dip wraps to ~4e9 mW and the reported current explodes).
 *         [FA] قانونی‌بودن محورهای یک کانال: ۰ نقطه یعنی «جایگزین نشده» و
 *              مجاز است؛ وگرنه ۲..سقف نقطه، محور زنجیرهٔ اکیداً صعودی و
 *              محور توان بدون افت (تفریق بدون‌علامت در درون‌یابی، افت را به
 *              ~۴e۹ می‌پیچاند).
 */
static uint8_t func__CalLut_ChannelCheck(const cal_lut_channel_t
                                             *cal_lut_channel_t__channel)
{
    uint32_t uint32_t__i;

    if (cal_lut_channel_t__channel->uint32_t__points == 0u)
    {
        return (uint8_t)CAL_LUT_ST_OK;
    }

    if ((cal_lut_channel_t__channel->uint32_t__points <
         (uint32_t)CAL_LUT_POINTS_MIN) ||
        (cal_lut_channel_t__channel->uint32_t__points >
         (uint32_t)CAL_LUT_POINTS_MAX))
    {
        return (uint8_t)CAL_LUT_ST_COUNT;
    }

    for (uint32_t__i = 1u;
         uint32_t__i < cal_lut_channel_t__channel->uint32_t__points;
         uint32_t__i++)
    {
        if (cal_lut_channel_t__channel->UINT32_T__A__ChainMa[uint32_t__i] <=
            cal_lut_channel_t__channel->UINT32_T__A__ChainMa[uint32_t__i - 1u])
        {
            return (uint8_t)CAL_LUT_ST_CHAIN;
        }
        if (cal_lut_channel_t__channel->UINT32_T__A__PowerMw[uint32_t__i] <
            cal_lut_channel_t__channel->UINT32_T__A__PowerMw[uint32_t__i - 1u])
        {
            return (uint8_t)CAL_LUT_ST_POWER;
        }
    }

    return (uint8_t)CAL_LUT_ST_OK;
}

/**
 * @brief  [EN] Validate a candidate record (see header contract).
 *         [FA] اعتبارسنجی رکورد نامزد (قرارداد هدر).
 */
bool func__CalLut_RecordValidate(const cal_lut_record_t *cal_lut_record_t__record)
{
    uint32_t uint32_t__crc;
    uint8_t uint8_t__channel;

    if (cal_lut_record_t__record == NULL)
    {
        return false;
    }
    if (cal_lut_record_t__record->uint32_t__magic != (uint32_t)CAL_LUT_MAGIC)
    {
        return false;
    }
    if (cal_lut_record_t__record->uint16_t__version != (uint16_t)CAL_LUT_VERSION)
    {
        return false;
    }

    for (uint8_t__channel = 0u; uint8_t__channel < 2u; uint8_t__channel++)
    {
        if (func__CalLut_ChannelCheck(
                &cal_lut_record_t__record
                     ->CAL_LUT_CHANNEL_T__A__Channel[uint8_t__channel]) !=
            (uint8_t)CAL_LUT_ST_OK)
        {
            return false;
        }
    }

    uint32_t__crc = func__CalLut_Crc32(
        (const uint8_t *)cal_lut_record_t__record,
        (uint32_t)(sizeof(cal_lut_record_t) - sizeof(uint32_t)));

    return (uint32_t__crc == cal_lut_record_t__record->uint32_t__crc32);
}

/* ====================================================================
 * ===== CalLut flash state machine ===================================
 * ==================================================================== */

/* [EN] The ACTIVE table (RAM copy, read by measurement.c every pass) and
   the STAGING table the link fills. They are separate on purpose: an
   interrupted or rejected push must not disturb a working board.
   [FA] جدول «فعال» (نسخهٔ RAM که measurement هر پاس می‌خواند) و جدول در حال
   «چیده‌شدن». عمداً جدا هستند تا ارسال نیمه‌کاره یا ردشده بردِ سالم را به هم
   نزند. */
static cal_lut_record_t CAL_LUT_RECORD_T__G__Active;
static cal_lut_record_t CAL_LUT_RECORD_T__G__Stage;
static bool BOOL__G__ActiveValid = false;
static bool BOOL__G__StageOpen = false;
static uint16_t UINT16_T__G__NewestSeq = 0u;
static bool BOOL__G__NewestIsPageB = false;
/* [EN] Points still missing from the staging buffer, per channel: a chunk
   lost on the wire must fail the commit, not write a half table.
   [FA] نقاط نرسیدهٔ هر کانال: گم‌شدن یک تکه باید کامیت را بشکند نه اینکه
   نصف جدول نوشته شود. */
static uint32_t UINT32_T__G__StageMissing[2];
/* [EN] One bit per staged point makes StagePoint idempotent: retransmitting
   a chunk after a lost ACK updates the value but cannot make the missing-point
   count reach zero while holes remain. CAL_LUT_POINTS_MAX is 24, so one u32
   covers each channel without another allocation.
   [FA] برای هر نقطهٔ چیده‌شده یک بیت داریم تا StagePoint idempotent باشد:
   ارسال دوبارهٔ تکه بعد از ACK گمشده مقدار را به‌روز می‌کند، اما شمارندهٔ
   نقاط گمشده را تا وقتی سوراخی هست صفر نمی‌کند. سقف ۲۴ نقطه است، پس یک u32
   برای هر کانال کافی است. */
static uint32_t UINT32_T__G__StageReceivedMask[2];
/* [EN] Armed reboot countdown in comm runs (0 = disarmed). The reset is
   deliberately NOT taken inside the link handler: the ACK frame has to
   leave the UART first, otherwise the panel sees a dead board and cannot
   tell a successful push from a crash.
   [FA] شمارش معکوس ریست مسلح‌شده (۰ = غیرفعال). ریست عمداً داخل هندلر لینک
   گرفته نمی‌شود: فریم ACK باید اول از UART خارج شود، وگرنه پنل بردِ مرده
   می‌بیند و موفقیت را از کرش تشخیص نمی‌دهد. */
static uint8_t UINT8_T__G__ResetRuns = 0u;
#define CAL_LUT_RESET_DELAY_RUNS   3u

/**
 * @brief  [EN] Read one page as a record candidate.
 *         [FA] خواندن یک صفحه به‌عنوان نامزد رکورد.
 */
static const cal_lut_record_t *func__CalLut_PageRecord(uint32_t uint32_t__pageAddress)
{
    return (const cal_lut_record_t *)(uintptr_t)uint32_t__pageAddress;
}

/**
 * @brief  [EN] Unsigned wrap-safe sequence compare (same rule as the
 *              parameter NVM: a > b when (a - b) is a small positive).
 *         [FA] مقایسهٔ ترتیب با در نظر گرفتن چرخش شمارنده.
 */
static int8_t func__CalLut_SeqCompare(uint16_t uint16_t__a, uint16_t uint16_t__b)
{
    uint16_t uint16_t__delta = (uint16_t)(uint16_t__a - uint16_t__b);

    if (uint16_t__delta == 0u)
    {
        return 0;
    }
    return (uint16_t__delta < 0x8000u) ? (int8_t)1 : (int8_t)-1;
}

/**
 * @brief  [EN] Boot load (see header contract).
 *         [FA] بارگذاری بوت (قرارداد هدر).
 */
void func__CalLut_Init(void)
{
    const cal_lut_record_t *cal_lut_record_t__pageA =
        func__CalLut_PageRecord(CAL_LUT_PAGE_A_ADDR);
    const cal_lut_record_t *cal_lut_record_t__pageB =
        func__CalLut_PageRecord(CAL_LUT_PAGE_B_ADDR);
    const cal_lut_record_t *cal_lut_record_t__newest = NULL;
    bool bool__validA = func__CalLut_RecordValidate(cal_lut_record_t__pageA);
    bool bool__validB = func__CalLut_RecordValidate(cal_lut_record_t__pageB);

    BOOL__G__ActiveValid = false;
    BOOL__G__StageOpen = false;
    UINT8_T__G__ResetRuns = 0u;

    if ((bool__validA != false) && (bool__validB != false))
    {
        cal_lut_record_t__newest =
            (func__CalLut_SeqCompare(cal_lut_record_t__pageA->uint16_t__seq,
                                     cal_lut_record_t__pageB->uint16_t__seq) >= 0)
                ? cal_lut_record_t__pageA
                : cal_lut_record_t__pageB;
    }
    else if (bool__validA != false)
    {
        cal_lut_record_t__newest = cal_lut_record_t__pageA;
    }
    else if (bool__validB != false)
    {
        cal_lut_record_t__newest = cal_lut_record_t__pageB;
    }
    else
    {
        /* [EN] Fresh board or both records corrupt: the COMPILED tables in
           calibration.h stay in charge - the board always measures.
           [FA] برد نو یا خرابی هر دو رکورد: جدول‌های کامپایل‌شده سر کار
           می‌مانند - برد همیشه اندازه می‌گیرد. */
    }

    if (cal_lut_record_t__newest == NULL)
    {
        UINT16_T__G__NewestSeq = 0u;
        BOOL__G__NewestIsPageB = false;
        return;
    }

    (void)memcpy(&CAL_LUT_RECORD_T__G__Active, cal_lut_record_t__newest,
                 sizeof(cal_lut_record_t));
    BOOL__G__ActiveValid = true;
    UINT16_T__G__NewestSeq = cal_lut_record_t__newest->uint16_t__seq;
    BOOL__G__NewestIsPageB = (cal_lut_record_t__newest == cal_lut_record_t__pageB);
}

/**
 * @brief  [EN] Channel index 1/2 -> array index, 0xFF when out of range.
 *         [FA] تبدیل شمارهٔ کانال به اندیس آرایه.
 */
static uint8_t func__CalLut_Index(uint8_t uint8_t__channel)
{
    if (uint8_t__channel == (uint8_t)CAL_LUT_CHANNEL_1)
    {
        return 0u;
    }
    if (uint8_t__channel == (uint8_t)CAL_LUT_CHANNEL_2)
    {
        return 1u;
    }
    return 0xFFu;
}

bool func__CalLut_Active(uint8_t uint8_t__channel)
{
    uint8_t uint8_t__index = func__CalLut_Index(uint8_t__channel);

    if ((BOOL__G__ActiveValid == false) || (uint8_t__index == 0xFFu))
    {
        return false;
    }
    return (CAL_LUT_RECORD_T__G__Active
                .CAL_LUT_CHANNEL_T__A__Channel[uint8_t__index]
                .uint32_t__points >= (uint32_t)CAL_LUT_POINTS_MIN);
}

uint32_t func__CalLut_Points(uint8_t uint8_t__channel)
{
    uint8_t uint8_t__index = func__CalLut_Index(uint8_t__channel);

    if (func__CalLut_Active(uint8_t__channel) == false)
    {
        return 0u;
    }
    return CAL_LUT_RECORD_T__G__Active
        .CAL_LUT_CHANNEL_T__A__Channel[uint8_t__index].uint32_t__points;
}

const uint32_t *func__CalLut_ChainMa(uint8_t uint8_t__channel)
{
    uint8_t uint8_t__index = func__CalLut_Index(uint8_t__channel);

    if (func__CalLut_Active(uint8_t__channel) == false)
    {
        return NULL;
    }
    return CAL_LUT_RECORD_T__G__Active
        .CAL_LUT_CHANNEL_T__A__Channel[uint8_t__index].UINT32_T__A__ChainMa;
}

const uint32_t *func__CalLut_PowerMw(uint8_t uint8_t__channel)
{
    uint8_t uint8_t__index = func__CalLut_Index(uint8_t__channel);

    if (func__CalLut_Active(uint8_t__channel) == false)
    {
        return NULL;
    }
    return CAL_LUT_RECORD_T__G__Active
        .CAL_LUT_CHANNEL_T__A__Channel[uint8_t__index].UINT32_T__A__PowerMw;
}

uint32_t func__CalLut_ActiveCrc32(void)
{
    if (BOOL__G__ActiveValid == false)
    {
        return 0u;
    }
    return CAL_LUT_RECORD_T__G__Active.uint32_t__crc32;
}

bool func__CalLut_StageBegin(uint32_t uint32_t__points1,
                             uint32_t uint32_t__points2)
{
    if (((uint32_t__points1 != 0u) &&
         ((uint32_t__points1 < (uint32_t)CAL_LUT_POINTS_MIN) ||
          (uint32_t__points1 > (uint32_t)CAL_LUT_POINTS_MAX))) ||
        ((uint32_t__points2 != 0u) &&
         ((uint32_t__points2 < (uint32_t)CAL_LUT_POINTS_MIN) ||
          (uint32_t__points2 > (uint32_t)CAL_LUT_POINTS_MAX))))
    {
        BOOL__G__StageOpen = false;
        return false;
    }

    /* [EN] Both channels empty would write a record that overrides nothing:
       that is a no-op push, and silently "succeeding" would be a lie.
       [FA] خالی‌بودن هر دو کانال یعنی ارسالِ بی‌اثر؛ «موفقیت» اعلام‌کردنش
       دروغ است. */
    if ((uint32_t__points1 == 0u) && (uint32_t__points2 == 0u))
    {
        BOOL__G__StageOpen = false;
        return false;
    }

    (void)memset(&CAL_LUT_RECORD_T__G__Stage, 0, sizeof(cal_lut_record_t));
    CAL_LUT_RECORD_T__G__Stage.uint32_t__magic = (uint32_t)CAL_LUT_MAGIC;
    CAL_LUT_RECORD_T__G__Stage.uint16_t__version = (uint16_t)CAL_LUT_VERSION;
    CAL_LUT_RECORD_T__G__Stage.CAL_LUT_CHANNEL_T__A__Channel[0]
        .uint32_t__points = uint32_t__points1;
    CAL_LUT_RECORD_T__G__Stage.CAL_LUT_CHANNEL_T__A__Channel[1]
        .uint32_t__points = uint32_t__points2;
    UINT32_T__G__StageMissing[0] = uint32_t__points1;
    UINT32_T__G__StageMissing[1] = uint32_t__points2;
    UINT32_T__G__StageReceivedMask[0] = 0u;
    UINT32_T__G__StageReceivedMask[1] = 0u;

    BOOL__G__StageOpen = true;
    return true;
}

bool func__CalLut_StagePoint(uint8_t uint8_t__channel,
                             uint32_t uint32_t__index,
                             uint32_t uint32_t__chainMa,
                             uint32_t uint32_t__powerMw)
{
    uint8_t uint8_t__slot = func__CalLut_Index(uint8_t__channel);

    if ((BOOL__G__StageOpen == false) || (uint8_t__slot == 0xFFu))
    {
        return false;
    }
    if (uint32_t__index >=
        CAL_LUT_RECORD_T__G__Stage.CAL_LUT_CHANNEL_T__A__Channel[uint8_t__slot]
            .uint32_t__points)
    {
        return false;
    }

    CAL_LUT_RECORD_T__G__Stage.CAL_LUT_CHANNEL_T__A__Channel[uint8_t__slot]
        .UINT32_T__A__ChainMa[uint32_t__index] = uint32_t__chainMa;
    CAL_LUT_RECORD_T__G__Stage.CAL_LUT_CHANNEL_T__A__Channel[uint8_t__slot]
        .UINT32_T__A__PowerMw[uint32_t__index] = uint32_t__powerMw;

    /* [EN] A chunk may be retransmitted after its ACK was lost. Only the
       first receipt of this index consumes one missing point; every receipt
       still replaces the staged value above, which is the useful idempotent
       retry behaviour.
       [FA] یک تکه ممکن است بعد از گم‌شدن ACK دوباره برسد. فقط دریافت اولِ
       این اندیس یک نقطهٔ گمشده را کم می‌کند؛ هر دریافت همچنان مقدار چیده‌شده
       را جایگزین می‌کند تا retry واقعاً idempotent باشد. */
    {
        uint32_t uint32_t__pointMask = (uint32_t)1u << uint32_t__index;

        if ((UINT32_T__G__StageReceivedMask[uint8_t__slot] &
             uint32_t__pointMask) == 0u)
        {
            UINT32_T__G__StageReceivedMask[uint8_t__slot] |= uint32_t__pointMask;
            if (UINT32_T__G__StageMissing[uint8_t__slot] > 0u)
            {
                UINT32_T__G__StageMissing[uint8_t__slot]--;
            }
        }
    }
    return true;
}

/**
 * @brief  [EN] Idle the charger around the flash stall (same rule as the
 *              parameter NVM: no switching inside the ~30..50 ms blind
 *              window a page erase creates).
 *         [FA] بیکارکردن شارژر دور استال فلش (همان قانون NVM پارامترها).
 */
static bool func__CalLut_SuspendCharger(void)
{
#if defined(CAL_LUT_HOST_TEST)
    return false;
#elif MODULE_CHARGER
    uint8_t uint8_t__poll;

    if (osKernelGetState() != osKernelRunning)
    {
        return false;
    }

    func__Charger_SetSuspended(true);

    for (uint8_t__poll = 0u; uint8_t__poll < 20u; uint8_t__poll++)
    {
        if ((func__BspPwm_IsGatePulsing(BSP_PWM_CHARGER_1) == false) &&
            (func__BspPwm_IsGatePulsing(BSP_PWM_CHARGER_2) == false))
        {
            return true;
        }
        func__Rtos_DelayMilliseconds(2u);
    }

    return true;
#else
    return false;
#endif
}

static void func__CalLut_ResumeCharger(bool bool__wasSuspended)
{
#if !defined(CAL_LUT_HOST_TEST) && MODULE_CHARGER
    if (bool__wasSuspended != false)
    {
        func__Charger_SetSuspended(false);
    }
#else
    (void)bool__wasSuspended;
#endif
}

uint8_t func__CalLut_Commit(uint32_t uint32_t__panelCrc32,
                            uint32_t *uint32_t__ptr_boardCrc32)
{
    uint32_t uint32_t__pageAddress;
    uint32_t uint32_t__crc;
    uint8_t uint8_t__channel;
    bool bool__chargerSuspended;

    if (uint32_t__ptr_boardCrc32 != NULL)
    {
        *uint32_t__ptr_boardCrc32 = 0u;
    }

    if (BOOL__G__StageOpen == false)
    {
        return (uint8_t)CAL_LUT_ST_NO_STAGE;
    }

    if ((UINT32_T__G__StageMissing[0] != 0u) ||
        (UINT32_T__G__StageMissing[1] != 0u))
    {
        return (uint8_t)CAL_LUT_ST_MISSING;
    }

    for (uint8_t__channel = 0u; uint8_t__channel < 2u; uint8_t__channel++)
    {
        uint8_t uint8_t__status = func__CalLut_ChannelCheck(
            &CAL_LUT_RECORD_T__G__Stage
                 .CAL_LUT_CHANNEL_T__A__Channel[uint8_t__channel]);
        if (uint8_t__status != (uint8_t)CAL_LUT_ST_OK)
        {
            return uint8_t__status;
        }
    }

    CAL_LUT_RECORD_T__G__Stage.uint16_t__seq =
        (uint16_t)(UINT16_T__G__NewestSeq + 1u);
    uint32_t__crc = func__CalLut_Crc32(
        (const uint8_t *)&CAL_LUT_RECORD_T__G__Stage,
        (uint32_t)(sizeof(cal_lut_record_t) - sizeof(uint32_t)));
    CAL_LUT_RECORD_T__G__Stage.uint32_t__crc32 = uint32_t__crc;

    /* [EN] The panel's CRC is computed over the TABLE CONTENT it sent, not
       over this record (the record carries a sequence number the panel
       cannot know). So the content CRC is what must match, and it is
       re-derived here from the staged axes.
       [FA] CRC پنل روی «محتوای جدول» است نه روی رکورد (رکورد شمارهٔ ترتیب
       دارد که پنل نمی‌داند). پس همان CRC محتوا باید بخواند و اینجا دوباره
       از محورهای چیده‌شده ساخته می‌شود. */
    {
        uint32_t uint32_t__content = 0xFFFFFFFFu;
        uint32_t uint32_t__i;

        for (uint8_t__channel = 0u; uint8_t__channel < 2u; uint8_t__channel++)
        {
            const cal_lut_channel_t *cal_lut_channel_t__ch =
                &CAL_LUT_RECORD_T__G__Stage
                     .CAL_LUT_CHANNEL_T__A__Channel[uint8_t__channel];

            uint32_t__content = func__CalLut_Crc32Byte(
                uint32_t__content,
                (uint8_t)cal_lut_channel_t__ch->uint32_t__points);

            for (uint32_t__i = 0u;
                 uint32_t__i < cal_lut_channel_t__ch->uint32_t__points;
                 uint32_t__i++)
            {
                uint32_t uint32_t__pair[2];
                uint8_t uint8_t__b;

                uint32_t__pair[0] =
                    cal_lut_channel_t__ch->UINT32_T__A__ChainMa[uint32_t__i];
                uint32_t__pair[1] =
                    cal_lut_channel_t__ch->UINT32_T__A__PowerMw[uint32_t__i];

                for (uint8_t__b = 0u; uint8_t__b < 8u; uint8_t__b++)
                {
                    uint32_t__content = func__CalLut_Crc32Byte(
                        uint32_t__content,
                        (uint8_t)((uint32_t__pair[uint8_t__b / 4u] >>
                                   (8u * (uint8_t__b % 4u))) & 0xFFu));
                }
            }
        }

        if ((uint32_t__content ^ 0xFFFFFFFFu) != uint32_t__panelCrc32)
        {
            return (uint8_t)CAL_LUT_ST_CRC;
        }
    }

    /* [EN] Ping-pong: never erase the page that holds the only good record.
       [FA] پینگ‌پنگ: هرگز صفحهٔ نگهدارندهٔ تنها رکورد خوب پاک نمی‌شود. */
    uint32_t__pageAddress = (BOOL__G__NewestIsPageB != false)
                                ? CAL_LUT_PAGE_A_ADDR
                                : CAL_LUT_PAGE_B_ADDR;

    bool__chargerSuspended = func__CalLut_SuspendCharger();

    if (func__BspFlash_ErasePage(uint32_t__pageAddress) == false)
    {
        func__CalLut_ResumeCharger(bool__chargerSuspended);
        return (uint8_t)CAL_LUT_ST_FLASH;
    }

    if (func__BspFlash_ProgramHalfWords(
            uint32_t__pageAddress,
            (const uint16_t *)&CAL_LUT_RECORD_T__G__Stage,
            (uint32_t)(sizeof(cal_lut_record_t) / sizeof(uint16_t))) == false)
    {
        func__CalLut_ResumeCharger(bool__chargerSuspended);
        return (uint8_t)CAL_LUT_ST_FLASH;
    }

    func__CalLut_ResumeCharger(bool__chargerSuspended);

    /* [EN] Re-read from FLASH and validate: the ACK must describe what is
       actually stored, not what we meant to store.
       [FA] بازخوانی از «خود فلش» و اعتبارسنجی: ACK باید آنچه واقعاً ذخیره
       شده را توصیف کند، نه آنچه قصد داشتیم. */
    if (func__CalLut_RecordValidate(
            func__CalLut_PageRecord(uint32_t__pageAddress)) == false)
    {
        return (uint8_t)CAL_LUT_ST_FLASH;
    }

    (void)memcpy(&CAL_LUT_RECORD_T__G__Active,
                 func__CalLut_PageRecord(uint32_t__pageAddress),
                 sizeof(cal_lut_record_t));
    BOOL__G__ActiveValid = true;
    UINT16_T__G__NewestSeq = CAL_LUT_RECORD_T__G__Active.uint16_t__seq;
    BOOL__G__NewestIsPageB = (uint32_t__pageAddress == CAL_LUT_PAGE_B_ADDR);
    BOOL__G__StageOpen = false;

    if (uint32_t__ptr_boardCrc32 != NULL)
    {
        *uint32_t__ptr_boardCrc32 = uint32_t__panelCrc32;
    }
    return (uint8_t)CAL_LUT_ST_OK;
}

void func__CalLut_RequestReset(void)
{
    UINT8_T__G__ResetRuns = (uint8_t)CAL_LUT_RESET_DELAY_RUNS;
}

void func__CalLut_Tick(void)
{
    if (UINT8_T__G__ResetRuns == 0u)
    {
        return;
    }

    UINT8_T__G__ResetRuns--;
    if (UINT8_T__G__ResetRuns != 0u)
    {
        return;
    }

#if defined(CAL_LUT_HOST_TEST)
    CAL_LUT_HOST_RESET_HOOK();
#else
#if MODULE_CHARGER
    /* [EN] Stop switching before the core restarts: a reset in the middle
       of a gate pulse leaves the MOSFET on until the peripheral is
       re-initialised.
       [FA] قبل از ریستِ هسته سوییچینگ بایستد: ریست وسط پالس گیت، ماسفت را
       تا مقداردهی دوبارهٔ جانبی روشن می‌گذارد. */
    func__Charger_SetSuspended(true);
#endif
    NVIC_SystemReset();
#endif
}
