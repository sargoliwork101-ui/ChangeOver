/**
 * @file    stm32f1xx.h
 * @brief   [EN] Test double for the ST device header. cal_lut.c includes
 *              stm32f1xx.h for exactly one symbol - NVIC_SystemReset() -
 *              and the real CMSIS header carries ARM inline assembly that
 *              an x86 host cannot assemble. This shim is placed FIRST on
 *              the include path of the host test only; the firmware build
 *              never sees it.
 *          [FA] بدل هدر دستگاه ST. فایل ‎cal_lut.c‎ این هدر را فقط برای یک
 *              نماد (‎NVIC_SystemReset‎) می‌خواهد و هدر واقعی CMSIS اسمبلی
 *              ARM دارد که روی x86 اسمبل نمی‌شود. این بدل فقط در مسیر
 *              include تست هاست است و بیلد فرم‌ور هرگز آن را نمی‌بیند.
 */

#ifndef HOST_TEST_STM32F1XX_H
#define HOST_TEST_STM32F1XX_H

#include <stdio.h>
#include <stdlib.h>

/* [EN] A real reset would take the test process with it, so the double
 *      records the request loudly and aborts instead - a test that reaches
 *      it by accident fails visibly rather than vanishing.
 * [FA] ریست واقعی پروسهٔ تست را می‌کشد؛ این بدل درخواست را چاپ و تست را با
 *      خطا تمام می‌کند تا رسیدن تصادفی به آن دیده شود. */
static inline void NVIC_SystemReset(void)
{
    printf("FAIL: NVIC_SystemReset() reached in a host test\n");
    exit(1);
}

#endif /* HOST_TEST_STM32F1XX_H */
