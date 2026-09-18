/**
 * @file    host_test_bsp_measurement.c
 * @brief   [EN] Host-side numerical test of the board calibration port
 *              (bsp_measurement.c). Compiled and run on the host, never on the
 *              target: it includes no HAL and touches no register. It feeds
 *              ADC counts that correspond to known board stimuli and checks the
 *              engineering units returned by the calibration functions, so a
 *              changed divider, gain or shunt constant fails loudly.
 *          [FA] تست عددی سمت Host برای پورت کالیبراسیون برد (bsp_measurement.c).
 *              روی Host کامپایل و اجرا می‌شود و هرگز روی هدف نه: هیچ HAL ندارد
 *              و به رجیستری دست نمی‌زند. شمارش‌های ADC متناظر با محرک‌های معلوم
 *              برد را وارد می‌کند و واحدهای مهندسی برگشتی را بررسی می‌کند تا
 *              تغییر تقسیم، گین یا شانت با خطای بلند آشکار شود.
 *
 * @note    [EN] Build from the repository root with:
 *              gcc -std=c11 -Wall -Wextra -I Firmware/Bsp/Inc \
 *                  tools/host_test_bsp_measurement.c \
 *                  Firmware/Bsp/Src/bsp_measurement.c -o /tmp/meas_test
 *          [FA] ساخت از ریشهٔ مخزن با دستور بالا.
 */

#include "bsp_measurement.h"

#include <stdio.h>

/* ==================== Host test constants / ثابت‌های تست هاست ==================== */
/* [EN] Host model of the confirmed schematic, mirrored only inside this test:
 *      3.3 V reference, 12-bit ADC, 24 V divider 68K+1.2K over 6.8K, 12 V
 *      divider 33K+1.2K over 6.8K, current chain 10 mOhm shunt, LM358 gain 100
 *      and the 1K/10K MCU input divider.
 * [FA] مدل Host شماتیک تأییدشده فقط داخل این تست: مرجع 3.3V، ADC دوازده‌بیتی،
 *      تقسیم ۲۴V برابر 68K+1.2K روی 6.8K، تقسیم ۱۲V برابر 33K+1.2K روی 6.8K،
 *      زنجیر جریان با شانت 10mΩ، گین LM358 برابر ۱۰۰ و تقسیم ورودی 1K/10K. */
#define HOST_VREF_MV            3300u
#define HOST_FULL_SCALE         4095u
#define HOST_DIV24_TOP_OHMS     69200u
#define HOST_DIV24_BOTTOM_OHMS  6800u
#define HOST_DIV12_TOP_OHMS     34200u
#define HOST_DIV12_BOTTOM_OHMS  6800u
#define HOST_SHUNT_MOHMS        10u
#define HOST_AMP_GAIN           100u
#define HOST_CUR_DIV_TOP_OHMS   1000u
#define HOST_CUR_DIV_BOTTOM_OHMS 10000u

static int HOST_INT__G__Failures;

/* ==================== Host Counts From Voltage ==================== */
/**
 * @brief  [EN] ADC counts expected at the pin for one source voltage.
 *         [FA] شمارش ADC مورد انتظار روی پایه برای یک ولتاژ منبع.
 * @param  uint32_t__sourceMv [EN] Source voltage in mV / ولتاژ منبع بر حسب mV
 * @param  uint32_t__topOhms [EN] Divider series resistance / مقاومت سری تقسیم
 * @param  uint32_t__bottomOhms [EN] Divider ground resistance / مقاومت به زمین
 * @return uint16_t [EN] Expected ADC counts / شمارش مورد انتظار
 */
static uint16_t func__Host_CountsFromVoltageMv(uint32_t uint32_t__sourceMv,
                                               uint32_t uint32_t__topOhms,
                                               uint32_t uint32_t__bottomOhms)
{
    uint64_t uint64_t__pinMv;
    uint64_t uint64_t__counts;

    uint64_t__pinMv =
        ((uint64_t)uint32_t__sourceMv * (uint64_t)uint32_t__bottomOhms) /
        ((uint64_t)uint32_t__topOhms + (uint64_t)uint32_t__bottomOhms);
    uint64_t__counts = (uint64_t__pinMv * HOST_FULL_SCALE) / HOST_VREF_MV;

    return (uint16_t)uint64_t__counts;
}

/* ==================== Host Counts From Current ==================== */
/**
 * @brief  [EN] ADC counts expected at the pin for one shunt current.
 *         [FA] شمارش ADC مورد انتظار روی پایه برای یک جریان شانت.
 * @param  uint32_t__currentMa [EN] Current in mA / جریان بر حسب mA
 * @return uint16_t [EN] Expected ADC counts / شمارش مورد انتظار
 */
static uint16_t func__Host_CountsFromCurrentMa(uint32_t uint32_t__currentMa)
{
    uint64_t uint64_t__shuntMv;
    uint64_t uint64_t__ampMv;
    uint64_t uint64_t__pinMv;
    uint64_t uint64_t__counts;

    uint64_t__shuntMv =
        ((uint64_t)uint32_t__currentMa * (uint64_t)HOST_SHUNT_MOHMS) / 1000u;
    uint64_t__ampMv = uint64_t__shuntMv * (uint64_t)HOST_AMP_GAIN;
    uint64_t__pinMv =
        (uint64_t__ampMv * (uint64_t)HOST_CUR_DIV_BOTTOM_OHMS) /
        ((uint64_t)HOST_CUR_DIV_TOP_OHMS + (uint64_t)HOST_CUR_DIV_BOTTOM_OHMS);
    uint64_t__counts = (uint64_t__pinMv * HOST_FULL_SCALE) / HOST_VREF_MV;

    return (uint16_t)uint64_t__counts;
}

/* ==================== Host Check ==================== */
/**
 * @brief  [EN] Compare one conversion result with its expected window.
 *         [FA] یک نتیجهٔ تبدیل را با پنجرهٔ مورد انتظار مقایسه می‌کند.
 * @param  const char *char_ptr__name [EN] Check label / برچسب بررسی
 * @param  uint32_t__actual [EN] Value returned by the port / مقدار پورت
 * @param  uint32_t__expectedMv [EN] Expected value / مقدار مورد انتظار
 * @param  uint32_t__tolerance [EN] Allowed deviation / انحراف مجاز
 */
static void func__Host_Check(const char *char_ptr__name,
                             uint32_t uint32_t__actual,
                             uint32_t uint32_t__expectedMv,
                             uint32_t uint32_t__tolerance)
{
    uint32_t uint32_t__difference;

    if (uint32_t__actual >= uint32_t__expectedMv)
    {
        uint32_t__difference = uint32_t__actual - uint32_t__expectedMv;
    }
    else
    {
        uint32_t__difference = uint32_t__expectedMv - uint32_t__actual;
    }

    if (uint32_t__difference > uint32_t__tolerance)
    {
        HOST_INT__G__Failures++;
        printf("FAIL %s: got %u want %u +/-%u\n",
               char_ptr__name,
               (unsigned)uint32_t__actual,
               (unsigned)uint32_t__expectedMv,
               (unsigned)uint32_t__tolerance);
    }
    else
    {
        printf("ok   %s: %u (want %u +/-%u)\n",
               char_ptr__name,
               (unsigned)uint32_t__actual,
               (unsigned)uint32_t__expectedMv,
               (unsigned)uint32_t__tolerance);
    }
}

/* ==================== Main / ورودی ==================== */
int main(void)
{
    uint16_t uint16_t__counts;

    /* [EN] 24 V class channels: 0 V, 24 V and 30 V stimuli.
       [FA] کانال‌های کلاس ۲۴V: محرک‌های 0V و 24V و 30V. */
    uint16_t__counts = func__Host_CountsFromVoltageMv(0u,
                                                      HOST_DIV24_TOP_OHMS,
                                                      HOST_DIV24_BOTTOM_OHMS);
    func__Host_Check("v24_0mV", func__BspMeasurement_V24CountsToMv(uint16_t__counts), 0u, 60u);

    uint16_t__counts = func__Host_CountsFromVoltageMv(24000u,
                                                      HOST_DIV24_TOP_OHMS,
                                                      HOST_DIV24_BOTTOM_OHMS);
    func__Host_Check("v24_24000mV", func__BspMeasurement_V24CountsToMv(uint16_t__counts), 24000u, 60u);

    uint16_t__counts = func__Host_CountsFromVoltageMv(30000u,
                                                      HOST_DIV24_TOP_OHMS,
                                                      HOST_DIV24_BOTTOM_OHMS);
    func__Host_Check("v24_30000mV", func__BspMeasurement_V24CountsToMv(uint16_t__counts), 30000u, 60u);

    /* [EN] 12 V channel: 12.5 V stimulus.
       [FA] کانال ۱۲V: محرک 12.5V. */
    uint16_t__counts = func__Host_CountsFromVoltageMv(12500u,
                                                      HOST_DIV12_TOP_OHMS,
                                                      HOST_DIV12_BOTTOM_OHMS);
    func__Host_Check("v12_12500mV", func__BspMeasurement_V12CountsToMv(uint16_t__counts), 12500u, 60u);

    /* [EN] Current chain: 0 mA, 500 mA and 2000 mA stimuli. The 1K/10K MCU
       input divider must be undone by the port, otherwise these read ~10% low.
       [FA] زنجیر جریان: محرک‌های 0mA و 500mA و 2000mA. تقسیم ورودی 1K/10K سمت
       MCU باید توسط پورت جبران شود وگرنه اینها حدود ۱۰٪ کم خوانده می‌شوند. */
    uint16_t__counts = func__Host_CountsFromCurrentMa(0u);
    func__Host_Check("i_0mA", func__BspMeasurement_CurrentCountsToMa(uint16_t__counts), 0u, 25u);

    uint16_t__counts = func__Host_CountsFromCurrentMa(500u);
    func__Host_Check("i_500mA", func__BspMeasurement_CurrentCountsToMa(uint16_t__counts), 500u, 25u);

    uint16_t__counts = func__Host_CountsFromCurrentMa(2000u);
    func__Host_Check("i_2000mA", func__BspMeasurement_CurrentCountsToMa(uint16_t__counts), 2000u, 40u);

    if (HOST_INT__G__Failures == 0)
    {
        printf("ALL BSP MEASUREMENT HOST TESTS PASSED\n");
        return 0;
    }

    printf("BSP MEASUREMENT HOST TESTS FAILED: %d\n", HOST_INT__G__Failures);
    return 1;
}
