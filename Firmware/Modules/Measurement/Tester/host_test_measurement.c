/**
 * @file    host_test_measurement.c
 * @brief   [EN] Host unit test for the Measurement module. The PRODUCTION
 *              measurement.c is compiled as-is and fed synthetic ADC frames
 *              through a fake BSP. No board, no RTOS, no HAL.
 *          [FA] تست هاست ماژول Measurement: کد محصول بدون تغییر کامپایل
 *              می‌شود و فریم‌های ADC مصنوعی از طریق BSP بدلی به آن داده
 *              می‌شود. بدون برد، RTOS و HAL.
 *
 * @note    [EN] Conversion maths are deliberately LINEAR in the fake board
 *              layer (1 count = 10 mV, 1 count = 1 mA), so every expected
 *              number in this file can be worked out by hand and the test
 *              checks the MODULE's behaviour - warm-up, filtering, snapshot
 *              publication, offsets, parameter clamps - instead of
 *              re-deriving the board calibration.
 *          [FA] ریاضیات تبدیل در لایهٔ بدلی عمداً خطی است تا هر عدد انتظاری
 *              با دست قابل‌محاسبه باشد و تست «رفتار ماژول» را بسنجد: گرم‌شدن،
 *              فیلتر، انتشار snapshot، آفست و گیرهٔ پارامترها.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "measurement.h"
#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "cal_lut.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake board layer / لایهٔ برد بدلی ==================== */

static uint16_t UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_COUNT];
static bool     BOOL__G__FrameReady     = true;
static bool     BOOL__G__InputPin       = true;
static uint32_t UINT32_T__G__GetRawCalls = 0u;

bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT])
{
    uint32_t uint32_t__i;

    UINT32_T__G__GetRawCalls++;

    if ((BOOL__G__FrameReady == false) || (uint16_t__out == NULL))
    {
        return false;
    }

    for (uint32_t__i = 0u; uint32_t__i < BSP_ADC_CHANNEL_COUNT; uint32_t__i++)
    {
        uint16_t__out[uint32_t__i] = UINT16_T__G__A__Frame[uint32_t__i];
    }

    return true;
}

bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id)
{
    CHECK(bsp_gpio_id_t__id == BSP_GPIO_INPUT_24V_PRESENT);
    return BOOL__G__InputPin;
}

/* [‎EN] 1 count = 10 mV at the connector. / [FA]‎ هر شمارش ۱۰ میلی‌ولت. */
uint32_t func__BspMeasurement_CountsToMv(uint16_t uint16_t__counts)
{
    return ((uint32_t)uint16_t__counts * 10u);
}

/* [EN] Per-rail helpers: same linear 10 mV per count so the expected
       millivolts in this file stay hand-checkable.
   [FA] کمک‌کننده‌های هر ریل: همان ۱۰ میلی‌ولت به‌ازای هر شمارش. */
uint32_t func__BspMeasurement_V24CountsToMv(uint16_t uint16_t__counts)
{
    return ((uint32_t)uint16_t__counts * 10u);
}

uint32_t func__BspMeasurement_Battery24CountsToMv(uint16_t uint16_t__counts)
{
    return ((uint32_t)uint16_t__counts * 10u);
}

uint32_t func__BspMeasurement_V12CountsToMv(uint16_t uint16_t__counts)
{
    return ((uint32_t)uint16_t__counts * 10u);
}

/* [‎EN] 1 count = 1 mA of chain current. / [FA]‎ هر شمارش یک میلی‌آمپر. */
uint32_t func__BspMeasurement_Current1CountsToMa(uint16_t uint16_t__counts)
{
    return (uint32_t)uint16_t__counts;
}

uint32_t func__BspMeasurement_Current2CountsToMa(uint16_t uint16_t__counts)
{
    return (uint32_t)uint16_t__counts;
}

/* [‎EN] 1 count = 1 uV across the shunt. / [FA]‎ هر شمارش یک میکروولت شانت. */
uint32_t func__BspMeasurement_CurrentCountsToShuntUv(uint16_t uint16_t__counts)
{
    return (uint32_t)uint16_t__counts;
}

uint32_t func__BspMeasurement_VddaMv(uint16_t uint16_t__vrefintCounts,
                                     uint32_t uint32_t__vrefintMv)
{
    (void)uint16_t__vrefintCounts;
    (void)uint32_t__vrefintMv;
    return 3300u;
}

/* [EN] No flash LUT in this test: the compiled tables stay in charge.
   [FA] در این تست جدول فلش فعال نیست و جدول کامپایل سر کار است. */
bool func__CalLut_Active(uint8_t uint8_t__channel)
{
    (void)uint8_t__channel;
    return false;
}

uint32_t func__CalLut_Points(uint8_t uint8_t__channel)
{
    (void)uint8_t__channel;
    return 0u;
}

const uint32_t *func__CalLut_ChainMa(uint8_t uint8_t__channel)
{
    (void)uint8_t__channel;
    return NULL;
}

const uint32_t *func__CalLut_PowerMw(uint8_t uint8_t__channel)
{
    (void)uint8_t__channel;
    return NULL;
}

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

/* ==================== Test driver / رانندهٔ تست ==================== */

/**
 * @brief  [EN] Load one synthetic ADC frame (counts, not millivolts).
 *         [FA] بارگذاری یک فریم ADC مصنوعی (شمارش، نه میلی‌ولت).
 */
static void func__SetFrame(uint16_t uint16_t__vin,
                           uint16_t uint16_t__v24,
                           uint16_t uint16_t__v12,
                           uint16_t uint16_t__i1,
                           uint16_t uint16_t__i2)
{
    UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_24V_IN]   = uint16_t__vin;
    UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_24V_BAT]  = uint16_t__v24;
    UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_12V_BAT]  = uint16_t__v12;
    UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_CURRENT1] = uint16_t__i1;
    UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_CURRENT2] = uint16_t__i2;
    UINT16_T__G__A__Frame[BSP_ADC_CHANNEL_VREFINT]  = 1500u;
}

static void func__RunFrames(uint32_t uint32_t__count)
{
    uint32_t uint32_t__i;

    for (uint32_t__i = 0u; uint32_t__i < uint32_t__count; uint32_t__i++)
    {
        func__Measurement_Run();
    }
}

int main(void)
{
    measurement_snapshot_t measurement_snapshot_t__snap;
    uint32_t uint32_t__i;
    uint8_t  uint8_t__median;
    uint16_t uint16_t__window;
    int32_t  int32_t__offset;

    printf("== Measurement host test ==\n");

    /* ---- 1. nothing is valid before the warm-up frames are collected ----
       [EN] Publishing early would let every consumer act on a half-filled
            filter; the module must hold "valid = false" until the warm-up
            count is reached. GetSnapshot RETURNS that validity, and copies
            the struct either way so a consumer never reads uninitialised
            memory.
       [FA] انتشار زودهنگام یعنی تصمیم‌گیری روی فیلتر نیمه‌پر. خروجی
            ‎GetSnapshot‎ همان اعتبار است و کپی در هر حال انجام می‌شود. */
    func__SetFrame(2400u, 2500u, 1250u, 0u, 0u);
    BOOL__G__FrameReady = true;
    BOOL__G__InputPin   = true;
    func__Measurement_Init();

    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == false);
    CHECK(measurement_snapshot_t__snap.valid == false);

    for (uint32_t__i = 1u; uint32_t__i < MEASUREMENT_WARMUP_FRAME_COUNT; uint32_t__i++)
    {
        func__Measurement_Run();
        CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == false);
        CHECK(measurement_snapshot_t__snap.valid == false);
    }

    func__Measurement_Run();
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
    CHECK(measurement_snapshot_t__snap.valid == true);

    /* ---- 2. the published numbers follow the fake board maths ---- */
    func__RunFrames(60u);                       /* [EN] let the average settle */
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
    CHECK(measurement_snapshot_t__snap.v_in_mv    == 24000u);
    CHECK(measurement_snapshot_t__snap.v_bat24_mv == 25000u);
    CHECK(measurement_snapshot_t__snap.v_bat12_mv == 12500u);
    CHECK(measurement_snapshot_t__snap.input_present == true);

    /* [EN] The two halves are derived, never measured directly: the low half
            is the 12 V reading and the high half is what is left of the 24 V
            pack. A change in that rule shows up here immediately.
       [FA] دو نیم مشتق‌اند نه اندازه‌گیری مستقیم: نیم پایین همان ۱۲ ولت و نیم
            بالا باقی‌ماندهٔ پک ۲۴ ولت است. */
    CHECK(measurement_snapshot_t__snap.v_bat_low_mv  == 12500u);
    CHECK(measurement_snapshot_t__snap.v_bat_high_mv == 12500u);

    /* ---- 3. the input-present flag follows the board pin ---- */
    BOOL__G__InputPin = false;
    func__RunFrames(3u);
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
    CHECK(measurement_snapshot_t__snap.input_present == false);
    BOOL__G__InputPin = true;
    func__RunFrames(3u);
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
    CHECK(measurement_snapshot_t__snap.input_present == true);

    /* ---- 4. a frame the board cannot deliver invalidates the snapshot ----
       [EN] A dead DMA must not freeze the last good numbers in place and let
            the loops keep regulating on stale data.
       [FA] DMA مرده نباید آخرین اعداد خوب را منجمد کند و حلقه‌ها را روی دادهٔ
            کهنه نگه دارد. */
    BOOL__G__FrameReady = false;
    func__RunFrames(3u);
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == false);
    CHECK(measurement_snapshot_t__snap.valid == false);

    /* [EN] ... and it recovers by itself once frames come back, after a
            fresh warm-up.
       [FA] و با بازگشت فریم‌ها، پس از گرم‌شدن دوباره، خودش برمی‌گردد. */
    BOOL__G__FrameReady = true;
    func__RunFrames(MEASUREMENT_WARMUP_FRAME_COUNT + 2u);
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
    CHECK(measurement_snapshot_t__snap.valid == true);

    /* ---- 5. GetSnapshot refuses a NULL target instead of faulting ---- */
    CHECK(func__Measurement_GetSnapshot(NULL) == false);

    /* ---- 6. the current filter: a single spike must not pass through ----
       [EN] The median stage exists for exactly this: switching noise lands
            as one wild sample among quiet ones.
       [FA] مرحلهٔ میانه دقیقاً برای همین است: نویز کلیدزنی یک نمونهٔ پرت
            میان نمونه‌های آرام می‌گذارد. */
    func__SetFrame(2400u, 2500u, 1250u, 1000u, 1000u);
    func__Measurement_Init();
    func__RunFrames(80u);
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
    {
        uint32_t uint32_t__quietMa = measurement_snapshot_t__snap.i_ch1_ma;

        func__SetFrame(2400u, 2500u, 1250u, 60000u, 1000u);
        func__Measurement_Run();
        func__SetFrame(2400u, 2500u, 1250u, 1000u, 1000u);
        func__RunFrames(2u);

        CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
        CHECK(measurement_snapshot_t__snap.i_ch1_ma <=
              (uint32_t__quietMa + (uint32_t__quietMa / 4u) + 50u));
    }

    /* ---- 7. filter parameters are clamped and read back ---- */
    uint8_t__median = func__Measurement_SetFilterMedianSize(0u);
    CHECK(uint8_t__median >= 1u);
    CHECK(func__Measurement_GetFilterMedianSize() == uint8_t__median);

    uint8_t__median = func__Measurement_SetFilterMedianSize(255u);
    CHECK(uint8_t__median <= 31u);
    CHECK(func__Measurement_GetFilterMedianSize() == uint8_t__median);

    uint16_t__window = func__Measurement_SetFilterAverageWindow(0u);
    CHECK(uint16_t__window >= 1u);
    CHECK(func__Measurement_GetFilterAverageWindow() == uint16_t__window);

    uint16_t__window = func__Measurement_SetFilterAverageWindow(100000u);
    CHECK(func__Measurement_GetFilterAverageWindow() == uint16_t__window);

    /* [EN] An even median size is a trap (no middle sample); whatever the
            module answers, reading it back must agree with it.
       [FA] اندازهٔ میانهٔ زوج تله است؛ هر چه ماژول بپذیرد، خواندن دوباره باید
            همان باشد. */
    uint8_t__median = func__Measurement_SetFilterMedianSize(4u);
    CHECK(func__Measurement_GetFilterMedianSize() == uint8_t__median);

    /* ---- 8. voltage offsets: clamped, symmetric, applied to the reading ---- */
    (void)func__Measurement_SetFilterMedianSize(3u);
    (void)func__Measurement_SetFilterAverageWindow(8u);

    int32_t__offset = func__Measurement_SetVoltageOffsetMv(0u, 200);
    CHECK(int32_t__offset == 200);
    CHECK(func__Measurement_GetVoltageOffsetMv(0u) == 200);

    int32_t__offset = func__Measurement_SetVoltageOffsetMv(0u, 999999);
    CHECK(int32_t__offset == (int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV);
    int32_t__offset = func__Measurement_SetVoltageOffsetMv(0u, -999999);
    CHECK(int32_t__offset == -(int32_t)MEASUREMENT_VOLTAGE_OFFSET_LIMIT_MV);

    /* [EN] An unknown channel index must be refused, not written anywhere.
       [FA] شاخص کانال ناشناخته باید رد شود، نه جایی نوشته شود. */
    CHECK(func__Measurement_GetVoltageOffsetMv(99u) == 0);

    /* [EN] With a +500 mV offset on the input channel the published value
            must be exactly 500 mV above the un-offset one.
       [FA] با آفست ‎+۵۰۰‎ روی کانال ورودی، مقدار منتشرشده باید دقیقاً ۵۰۰
            میلی‌ولت بالاتر باشد. */
    {
        uint32_t uint32_t__before;
        uint32_t uint32_t__after;

        (void)func__Measurement_SetVoltageOffsetMv(0u, 0);
        func__SetFrame(2400u, 2500u, 1250u, 0u, 0u);
        func__Measurement_Init();
        func__RunFrames(40u);
        CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
        uint32_t__before = measurement_snapshot_t__snap.v_in_mv;

        (void)func__Measurement_SetVoltageOffsetMv(0u, 500);
        func__RunFrames(40u);
        CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == true);
        uint32_t__after = measurement_snapshot_t__snap.v_in_mv;

        CHECK(uint32_t__after == (uint32_t__before + 500u));
        (void)func__Measurement_SetVoltageOffsetMv(0u, 0);
    }

    /* ---- 9. the public conversion helpers agree with the board layer ---- */
    CHECK(func__Measurement_CountsToMv(1000u) == 10000u);
    CHECK(func__Measurement_CurrentCountsToShuntUv(1234u) == 1234u);
    CHECK(func__Measurement_CountsToMv(0u) == 0u);

    /* ---- 10. Init really restarts the module ----
       [EN] After Init the snapshot must be invalid again even though the
            board is still delivering perfect frames.
       [FA] پس از Init، با وجود فریم‌های سالم، snapshot باید دوباره نامعتبر شود. */
    func__Measurement_Init();
    CHECK(func__Measurement_GetSnapshot(&measurement_snapshot_t__snap) == false);
    CHECK(measurement_snapshot_t__snap.valid == false);
    CHECK(UINT32_T__G__GetRawCalls > 0u);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
