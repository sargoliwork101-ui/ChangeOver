/**
 * @file    host_test_diag.c
 * @brief   [EN] Host unit test for the Diag module. The PRODUCTION diag.c is
 *              compiled as-is and run against a fake kernel clock, a fake
 *              charger and two real flash pages mapped at their hardware
 *              addresses. No board, no HAL.
 *          [FA] تست هاست ماژول Diag: همان فایل محصول با ساعت بدلی کرنل،
 *              شارژر بدلی و دو صفحهٔ فلش واقعی که روی آدرس سخت‌افزاری خودشان
 *              نگاشت شده‌اند اجرا می‌شود. بدون برد و بدون HAL.
 *
 * @note    [EN] Proves the two audit findings of 2026-10-28:
 *              (1) uptime is read from the kernel tick counter, so a slow or
 *                  late control pass can no longer stretch the board's second
 *                  (the panel anchors its wall clock on this uptime);
 *              (2) the record is written DURING a long charge as well, at most
 *                  once per DIAG_SAVE_CHARGE_SEC, instead of waiting for the
 *                  charge to end - a mains cut mid-charge used to throw away
 *                  every charge second since it started.
 *          [FA] دو یافتهٔ ممیزی ۲۰۲۶-۱۰-۲۸ را ثابت می‌کند:
 *              (۱) آپتایم از شمارندهٔ تیک کرنل خوانده می‌شود، پس پاس دیررس یا
 *                  کند دیگر ثانیهٔ برد را بلندتر نمی‌کند (پنل ساعت دیواری‌اش را
 *                  روی همین آپتایم می‌بندد)؛
 *              (۲) رکورد حین شارژ طولانی هم نوشته می‌شود، حداکثر یک بار در هر
 *                  ‎DIAG_SAVE_CHARGE_SEC‎، نه فقط با تمام شدن شارژ - قبلاً قطع
 *                  برق در میانهٔ شارژ همهٔ ثانیه‌های از شروع را دور می‌ریخت.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "diag.h"
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "charger.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake kernel / کرنل بدلی ==================== */

static uint32_t UINT32_T__G__FakeTick = 0u;

uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}

uint32_t osKernelGetTickFreq(void);
uint32_t osKernelGetTickFreq(void)
{
    return 1000u;               /* [EN] 1 kHz kernel / کرنل ۱ کیلوهرتز */
}

/* [EN] One thread on the host, so a token is a complete lock.
   [FA] روی هاست یک نخ داریم، پس یک ژتون قفل کامل است. */
int32_t osKernelLock(void);
int32_t osKernelLock(void)
{
    return 0;
}

int32_t osKernelRestoreLock(int32_t int32_t__state);
int32_t osKernelRestoreLock(int32_t int32_t__state)
{
    return int32_t__state;
}

/* ==================== Fake charger / شارژر بدلی ==================== */

static bool BOOL__G__Ch0Active = false;
static bool BOOL__G__Ch1Active = false;
static bool BOOL__G__ChargerSuspended = false;

bool func__Charger_IsChannelActive(uint8_t uint8_t__channelIndex)
{
    return (uint8_t__channelIndex == 0u) ? BOOL__G__Ch0Active : BOOL__G__Ch1Active;
}

bool func__Charger_IsSuspended(void)
{
    return BOOL__G__ChargerSuspended;
}

void func__Charger_SetSuspended(bool bool__suspended)
{
    BOOL__G__ChargerSuspended = bool__suspended;
}

void func__BspPwm_StopAll(void)
{
}

/* [EN] The charger publishes its per-channel decision snapshot here; the diag
        record copies the state of both channels when a fault is logged. An
        all-zero table is fine for these tests - no fault path is exercised.
   [FA] شارژر تصویر تصمیم هر کانال را این‌جا منتشر می‌کند؛ رکورد دیاگ هنگام
        ثبت خطا حالت هر دو کانال را از همین‌جا برمی‌دارد. برای این تست‌ها
        جدول تمام‌صفر کافی است - هیچ مسیر خطایی اجرا نمی‌شود. */
volatile uint32_t UINT32_T__G__ChargerDiag[CHG_DIAG_COUNT];

/* ==================== Fake flash / فلش بدلی ==================== */

/* [EN] The real pages, mapped where the firmware believes they are: the module
        reads its record straight from those addresses, exactly like on the
        chip.
   [FA] همان صفحات واقعی، روی آدرسی که فرم‌ور باور دارد نگاشت شده‌اند: ماژول
        رکوردش را مستقیم از همان آدرس می‌خواند، دقیقاً مثل روی تراشه. */
#define HOST_TEST_FLASH_BASE   0x0800F000u
#define HOST_TEST_FLASH_SIZE   0x00001000u

static uint32_t UINT32_T__G__EraseCountA = 0u;
static uint32_t UINT32_T__G__EraseCountB = 0u;

static bool func__Host_FlashReady(void)
{
    static bool bool__mapped = false;

    if (bool__mapped == false)
    {
        void *void__ptr = mmap((void *)(uintptr_t)HOST_TEST_FLASH_BASE,
                               (size_t)HOST_TEST_FLASH_SIZE,
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                               -1, 0);
        if (void__ptr == MAP_FAILED)
        {
            (void)printf("cannot map the fake flash pages\n");
            return false;
        }
        (void)memset(void__ptr, 0xFF, (size_t)HOST_TEST_FLASH_SIZE);
        bool__mapped = true;
    }
    return true;
}

bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress)
{
    if (func__Host_FlashReady() == false)
    {
        return false;
    }
    if (uint32_t__pageAddress == DIAG_PAGE_A_ADDR)
    {
        UINT32_T__G__EraseCountA++;
    }
    else if (uint32_t__pageAddress == DIAG_PAGE_B_ADDR)
    {
        UINT32_T__G__EraseCountB++;
    }
    (void)memset((void *)(uintptr_t)uint32_t__pageAddress, 0xFF, 1024u);
    return true;
}

bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__ptr_source,
                                     uint32_t uint32_t__halfWordCount)
{
    (void)memcpy((void *)(uintptr_t)uint32_t__address, uint16_t__ptr_source,
                 (size_t)uint32_t__halfWordCount * 2u);
    return true;
}

/* ==================== Helpers / کمک‌کننده‌ها ==================== */

static uint32_t func__T_EraseCount(void)
{
    return UINT32_T__G__EraseCountA + UINT32_T__G__EraseCountB;
}

/* [EN] An erased pair of pages: every test starts on a device that has never
        written a record, so one test's save cannot colour the next one.
   [FA] یک جفت صفحهٔ پاک: هر تست روی دستگاهی شروع می‌شود که هرگز رکوردی
        ننوشته، پس ذخیرهٔ یک تست رنگ تست بعدی را عوض نمی‌کند. */
static void func__T_WipeFlash(void)
{
    (void)memset((void *)(uintptr_t)DIAG_PAGE_A_ADDR, 0xFF, 1024u);
    (void)memset((void *)(uintptr_t)DIAG_PAGE_B_ADDR, 0xFF, 1024u);
    UINT32_T__G__EraseCountA = 0u;
    UINT32_T__G__EraseCountB = 0u;
}

/* [EN] One simulated control second: the kernel tick moves on, the diag tick
        runs and, when a channel is charging, the charge second is offered.
   [FA] یک ثانیهٔ کنترل شبیه‌سازی‌شده: تیک کرنل جلو می‌رود، تیک دیاگ اجرا
        می‌شود و اگر کانالی شارژ کند، ثانیهٔ شارژ هم پیشنهاد می‌شود. */
static void func__T_Second(bool bool__ch0, bool bool__ch1)
{
    BOOL__G__Ch0Active = bool__ch0;
    BOOL__G__Ch1Active = bool__ch1;
    UINT32_T__G__FakeTick += 1000u;
    func__Diag_Tick();
    if (bool__ch0 != false)
    {
        func__Diag_AddChargeSec(0u);
    }
    if (bool__ch1 != false)
    {
        func__Diag_AddChargeSec(1u);
    }
}

/* ==================== Test 1: uptime comes from the kernel ==================== */

/**
 * [EN] A control pass costs 10 ms of delay PLUS its own work. The old code
 *      counted passes, so a pass that took 12 ms made every "second" 20 % too
 *      long. Now the kernel tick is the only clock.
 * [FA] هر پاس کنترل ۱۰ میلی‌ثانیه تأخیر «به‌علاوهٔ» کار خودش است. کد قبلی
 *      پاس‌ها را می‌شمارد، پس پاس ۱۲ میلی‌ثانیه‌ای هر «ثانیه» را ۲۰٪ بلندتر
 *      می‌کرد. حالا تیک کرنل تنها ساعت است.
 */
static void func__T_TestUptimeFromKernel(void)
{
    const diag_record_t *diag_record_t__rec;

    (void)printf("\n== آپتایم از شمارندهٔ تیک کرنل ==\n");
    func__T_WipeFlash();
    func__Diag_Init();
    diag_record_t__rec = func__Diag_GetRecord();
    CHECK(diag_record_t__rec->uint32_t__uptimeSec == 0u);

    /* [EN] 30 real seconds pass before the first tick ever runs.
       [FA] ۳۰ ثانیهٔ واقعی می‌گذرد تا اولین تیک اجرا شود. */
    UINT32_T__G__FakeTick = 30000u;
    func__Diag_Tick();
    CHECK(diag_record_t__rec->uint32_t__uptimeSec == 30u);

    /* [EN] A late tick does not lose the time that passed.
       [FA] تیکِ دیررس زمانی را که گذشته از دست نمی‌دهد. */
    UINT32_T__G__FakeTick = 35000u;
    func__Diag_Tick();
    CHECK(diag_record_t__rec->uint32_t__uptimeSec == 35u);

    /* [EN] The 32-bit tick counter wraps after ~49.7 days; uptime must not
            jump back with it.
       [FA] شمارندهٔ ۳۲بیتی تیک بعد از حدود ۴۹٫۷ روز سرریز می‌کند؛ آپتایم
            نباید با آن عقب بپرد. */
    UINT32_T__G__FakeTick = 0xFFFFFFFFu;
    func__Diag_Tick();
    {
        const uint32_t uint32_t__beforeWrap =
            diag_record_t__rec->uint32_t__uptimeSec;
        UINT32_T__G__FakeTick = 2000u;
        func__Diag_Tick();
        CHECK(diag_record_t__rec->uint32_t__uptimeSec >
              uint32_t__beforeWrap);
        CHECK((diag_record_t__rec->uint32_t__uptimeSec - uint32_t__beforeWrap) == 3u);
    }
}

/* ==================== Test 2: charge seconds by real delta ==================== */

/**
 * [EN] The count must follow the real clock: eight seconds of charging add
 *      eight, two channels in one second still add ONE to the total, and a
 *      channel that was idle for minutes adds nothing from its idle time.
 * [FA] شمارش باید از ساعت واقعی بیاید: هشت ثانیه شارژ هشت ثانیه اضافه
 *      می‌کند، دو کانال در یک ثانیه به «مجموع» فقط یکی اضافه می‌کنند، و
 *      کانالی که دقایقی بی‌کار بوده از زمان بی‌کاری چیزی اضافه نمی‌کند.
 */
static void func__T_TestChargeSeconds(void)
{
    const diag_record_t *diag_record_t__rec;
    uint32_t uint32_t__i;

    (void)printf("\n== ثانیه‌های شارژ ==\n");
    func__T_WipeFlash();
    func__Diag_Init();
    diag_record_t__rec = func__Diag_GetRecord();

    /* [EN] The first call only takes the starting point.
       [FA] اولین فراخوانی فقط نقطهٔ شروع را می‌گیرد. */
    func__T_Second(true, false);
    CHECK(diag_record_t__rec->uint32_t__totalChargeSec == 0u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 0u);

    for (uint32_t__i = 0u; uint32_t__i < 8u; uint32_t__i++)
    {
        func__T_Second(true, false);
    }
    CHECK(diag_record_t__rec->uint32_t__totalChargeSec == 8u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 8u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh2 == 0u);

    /* [EN] Channel 2 has been idle since boot: its first call opens its own
            session and adds nothing - idle time is not charge time.
       [FA] کانال ۲ از بوت بی‌کار بوده: اولین فراخوانی‌اش نشست خودش را باز
            می‌کند و چیزی اضافه نمی‌کند - زمان بی‌کاری زمان شارژ نیست. */
    func__T_Second(true, true);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh2 == 0u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 9u);
    CHECK(diag_record_t__rec->uint32_t__totalChargeSec == 9u);

    /* [EN] From the next second both channels charge: each channel counter
            grows by one, the TOTAL grows by one as well, because the total is
            the time the product spent charging, not the sum of the channels.
       [FA] از ثانیهٔ بعد هر دو کانال شارژ می‌کنند: شمارندهٔ هر کانال یکی بالا
            می‌رود و مجموع هم فقط «یکی» بالا می‌رود، چون مجموع زمان شارژ
            دستگاه است نه جمع دو کانال. */
    func__T_Second(true, true);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 10u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh2 == 1u);
    CHECK(diag_record_t__rec->uint32_t__totalChargeSec == 10u);

    /* [EN] Calling twice inside the same second adds nothing.
       [FA] دو بار صدا زدن در همان ثانیه چیزی اضافه نمی‌کند. */
    func__Diag_AddChargeSec(0u);
    func__Diag_AddChargeSec(1u);
    CHECK(diag_record_t__rec->uint32_t__totalChargeSec == 10u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 10u);

    /* [EN] Even a caller that comes every two seconds adds exactly two: the
            count follows the clock, not the call rate.
       [FA] حتی صدازننده‌ای که هر دو ثانیه می‌آید هم دقیقاً دو اضافه می‌کند:
            شمارش از ساعت می‌آید، نه از نرخ فراخوانی. */
    BOOL__G__Ch0Active = true;
    BOOL__G__Ch1Active = false;
    UINT32_T__G__FakeTick += 2000u;
    func__Diag_Tick();
    func__Diag_AddChargeSec(0u);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 12u);
    CHECK(diag_record_t__rec->uint32_t__totalChargeSec == 12u);
}

/* ==================== Test 3: save during a long charge ==================== */

/**
 * [EN] The finding: while a channel charged continuously, the debounce was
 *      re-armed every second, so the record never reached the flash until the
 *      charge ended. A mains cut in between lost the whole session.
 * [FA] یافته: تا وقتی کانالی پیوسته شارژ می‌کرد، دیبانس هر ثانیه از نو مسلح
 *      می‌شد و رکورد تا پایان شارژ هرگز به فلش نمی‌رسید. قطع برق در این
 *      فاصله کل نشست را دور می‌ریخت.
 */
static void func__T_TestSaveDuringCharge(void)
{
    const diag_record_t *diag_record_t__rec;
    uint32_t uint32_t__i;

    (void)printf("\n== ذخیره حین شارژ ==\n");
    func__T_WipeFlash();
    func__Diag_Init();
    diag_record_t__rec = func__Diag_GetRecord();

    func__T_Second(true, false);                    /* the starting point */
    const uint32_t uint32_t__savesAtStart = func__T_EraseCount();

    /* [EN] The old 60 s debounce would have written after the first minute.
            The charge policy waits for ten minutes of charge: nothing reaches
            the flash in the first 598 seconds.
       [FA] دیبانس ۶۰ ثانیه‌ای قبلی بعد از یک دقیقهٔ اول می‌نوشت. سیاست شارژ
            ده دقیقهٔ شارژ صبر می‌کند: در ۵۹۸ ثانیهٔ اول چیزی به فلش نمی‌رسد. */
    for (uint32_t__i = 0u; uint32_t__i < 598u; uint32_t__i++)
    {
        func__T_Second(true, false);
    }
    CHECK(func__T_EraseCount() == uint32_t__savesAtStart);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 598u);

    /* [EN] Minute ten arrives - and the record goes to the flash WITHOUT the
            charge ever stopping.
       [FA] دقیقهٔ دهم می‌رسد - و رکورد «بی‌آنکه شارژ تمام شود» به فلش می‌رود. */
    for (uint32_t__i = 0u; (uint32_t__i < 20u) && (func__T_EraseCount() == uint32_t__savesAtStart); uint32_t__i++)
    {
        func__T_Second(true, false);
    }
    CHECK(func__T_EraseCount() == (uint32_t__savesAtStart + 1u));
    CHECK(BOOL__G__Ch0Active != false);
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 >= 600u);

    /* [EN] And it keeps doing that: the next ten minutes bring the next save,
            so a mains cut can never throw away more than ten minutes.
       [FA] و همین‌طور ادامه می‌دهد: ده دقیقهٔ بعد ذخیرهٔ بعدی می‌آید، پس قطع
            برق هرگز بیشتر از ده دقیقه را دور نمی‌ریزد. */
    for (uint32_t__i = 0u; uint32_t__i < 620u; uint32_t__i++)
    {
        func__T_Second(true, false);
    }
    CHECK(func__T_EraseCount() == (uint32_t__savesAtStart + 2u));
}

/* ==================== Test 4: the charge ends ==================== */

/**
 * [EN] A charge shorter than a minute keeps its debounce: the last charge
 *      seconds land about a minute after the charge started, and a quiet
 *      second after that writes nothing at all.
 * [FA] شارژ کوتاه‌تر از یک دقیقه دیبانس خودش را نگه می‌دارد: ثانیه‌های آخر
 *      حدود یک دقیقه بعد از شروع شارژ می‌نشینند و ثانیهٔ آرام بعدش هیچ
 *      چیزی نمی‌نویسد.
 */
static void func__T_TestSaveWhenChargeStops(void)
{
    const diag_record_t *diag_record_t__rec;
    uint32_t uint32_t__i;

    (void)printf("\n== ذخیره با پایان شارژ ==\n");
    func__T_WipeFlash();
    func__Diag_Init();
    diag_record_t__rec = func__Diag_GetRecord();

    func__T_Second(true, false);
    for (uint32_t__i = 0u; uint32_t__i < 30u; uint32_t__i++)
    {
        func__T_Second(true, false);
    }
    const uint32_t uint32_t__savesBefore = func__T_EraseCount();
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 30u);
    CHECK(func__T_EraseCount() == uint32_t__savesBefore);

    /* [EN] The charger stopped; the debounce finishes and the row lands.
       [FA] شارژر قطع شد؛ دیبانس تمام می‌شود و ردیف می‌نشیند. */
    for (uint32_t__i = 0u; (uint32_t__i < 120u) && (func__T_EraseCount() == uint32_t__savesBefore); uint32_t__i++)
    {
        func__T_Second(false, false);
    }
    CHECK(func__T_EraseCount() == (uint32_t__savesBefore + 1u));
    CHECK(diag_record_t__rec->uint32_t__chargeSecCh1 == 30u);

    /* [EN] A quiet second after that writes nothing more: no change, no write.
       [FA] ثانیهٔ آرام بعدی چیز بیشتری نمی‌نویسد: تغییر نیست، نوشتن نیست. */
    const uint32_t uint32_t__savesAfter = func__T_EraseCount();
    func__T_Second(false, false);
    CHECK(func__T_EraseCount() == uint32_t__savesAfter);
}

/* ==================== main ==================== */

int main(void)
{
    (void)printf("Diag host test - the real diag.c, compiled\n");
    (void)printf("=====================================================\n");

    if (func__Host_FlashReady() == false)
    {
        return 1;
    }

    func__T_TestUptimeFromKernel();
    func__T_TestChargeSeconds();
    func__T_TestSaveDuringCharge();
    func__T_TestSaveWhenChargeStops();

    (void)printf("\n== بررسی‌ها: %d، شکست‌ها: %d ==\n", INT32_T__G__Checks,
                 INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
