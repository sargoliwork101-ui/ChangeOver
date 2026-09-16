/**
 * @file    bsp_adc.h
 * @brief   [EN] ADC+DMA board layer. ADC1 scans five analog channels and DMA
 *              fills a two-frame circular RAM buffer autonomously. The reader
 *              polls the DMA counter and copies only a completed frame; no
 *              DMA interrupt is required.
 *          [FA] لایهٔ برد ADC+DMA. ADC1 پنج کانال آنالوگ را اسکن می‌کند و DMA
 *              به‌طور مستقل بافر چرخشی دو فریمی RAM را پر می‌کند. خواننده
 *              شمارندهٔ DMA را می‌خواند و فقط یک فریم کامل را کپی می‌کند؛
 *              نیازی به وقفهٔ DMA نیست.
 *
 * @note    [EN] Channel order is fixed by the .ioc rank order (see Defines).
 *              Keep it in sync with CubeMX/CubeIDE.ioc.
 *          [FA] ترتیب کانال‌ها با ترتیب رنک‌های .ioc ثابت است (بخوانید
 *              Defines). با CubeMX/CubeIDE.ioc یکی نگه‌اش دارید.
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

#ifndef HAL_ADC_MODULE_ENABLED
typedef struct __ADC_HandleTypeDef ADC_HandleTypeDef;
#endif

/* ==================== ADC channel map ==================== */
/* [EN] ADC channel order == .ioc rank order. PA1/PA2/PA3/PA5/PA7 =
 *      ADC1_IN1/IN2/IN3/IN5/IN7 (schematic MICROCONTROLLER.SchDoc,
 *      Analog Input block).
 * [FA] ترتیب کانال‌های ADC == ترتیب رنک‌های .ioc.
 *      PA1/PA2/PA3/PA5/PA7 = ADC1_IN1/IN2/IN3/IN5/IN7 (شماتیک، بلوک
 *      Analog Input). */
#define BSP_ADC_CHANNEL_COUNT        5u
#define BSP_ADC_CHANNEL_CURRENT1     0u   /* PA1  ADC1_IN1  charge current 24V ch. 1 / جریان شارژ ۲۴ ولت کانال ۱ */
#define BSP_ADC_CHANNEL_24V_IN       1u   /* PA2  ADC1_IN2  24V main input   / ولتاژ ورودی ۲۴ */
#define BSP_ADC_CHANNEL_24V_BAT      2u   /* PA3  ADC1_IN3  24V battery      / ولتاژ باتری ۲۴ */
#define BSP_ADC_CHANNEL_12V_BAT      3u   /* PA5  ADC1_IN5  12V battery      / ولتاژ باتری ۱۲ */
#define BSP_ADC_CHANNEL_CURRENT2     4u   /* PA7  ADC1_IN7  charge current 12V ch. 2 / جریان شارژ ۱۲ ولت کانال ۲ */

/* ==================== DMA buffer ==================== */
/* [EN] The DMA buffer holds two full frames. When DMA writes one half, the
 *      other half is stable and can be copied. BSP_ADC_DMA_RETRY_COUNT
 *      protects the short copy from crossing a half-buffer boundary.
 * [FA] بافر DMA دو فریم کامل نگه می‌دارد. وقتی DMA در یک نیمه می‌نویسد،
 *      نیمهٔ دیگر پایدار است و می‌توان آن را کپی کرد. شمارندهٔ تلاش، کپی
 *      کوتاه را از عبور هم‌زمان از مرز نیمه محافظت می‌کند. */
#define BSP_ADC_DMA_FRAME_COUNT      2u
#define BSP_ADC_DMA_SAMPLE_COUNT     (BSP_ADC_CHANNEL_COUNT * BSP_ADC_DMA_FRAME_COUNT)
#define BSP_ADC_DMA_RETRY_COUNT      3u
#define BSP_ADC_START_TIMEOUT_MS     2u

/* ==================== BspAdc_Init ==================== */

/**
 * @brief  [EN] Store the CubeMX HAL handle and clear the DMA buffer.
 *         [FA] هندل HAL مکعب را نگه می‌دارد و بافر DMA را صفر می‌کند.
 * @param  ADC_HandleTypeDef__hadc [EN] ADC handle from CubeMX; NULL disables
 *                                     the BSP / هندل ADC مکعب؛ NULL یعنی خاموش
 */
void func__BspAdc_Init(ADC_HandleTypeDef *ADC_HandleTypeDef__hadc);

/* ==================== BspAdc_Start ==================== */

/**
 * @brief  [EN] Calibrate ADC1, then start continuous scan and circular DMA.
 *              The F1 ADC prescaler is configured by CubeMX at the highest
 *              legal value for PCLK2 = 72 MHz: 12 MHz (PCLK2 / 6).
 *              DMA interrupt sources are disabled because the reader polls
 *              the DMA counter instead of using an ISR.
 *         [FA] ADC1 را کالیبره می‌کند و سپس اسکن مداوم و DMA چرخشی را شروع
 *              می‌کند. پیش‌تقسیم‌کنندهٔ ADC در CubeMX برای بیشترین مقدار
 *              مجاز با PCLK2 برابر ۷۲MHz روی ۱۲MHz (تقسیم بر ۶) است.
 *              چون خواننده شمارندهٔ DMA را پالت می‌کند، منابع وقفهٔ DMA
 *              خاموش می‌شوند و ISR لازم نیست.
 * @return bool [EN] true when calibration and HAL start succeed /
 *                   اگر کالیبراسیون و شروع HAL موفق باشد true
 */
bool func__BspAdc_Start(void);

/* ==================== BspAdc_IsFrameReady ==================== */

/**
 * @brief  [EN] Report whether ADC+DMA was started successfully. GetRaw still
 *              verifies that a complete half-frame is available before copy.
 *         [FA] اعلام می‌کند ADC+DMA با موفقیت شروع شده است. GetRaw پیش از
 *              کپی، کامل بودن نیم‌فریم را دوباره بررسی می‌کند.
 * @return bool [EN] true after successful start / بعد از شروع موفق true
 */
bool func__BspAdc_IsFrameReady(void);

/* ==================== BspAdc_GetRaw ==================== */

/**
 * @brief  [EN] Copy the newest completed five-sample frame into out[]. The
 *              DMA counter selects the half that DMA is not currently writing;
 *              a before/after counter check rejects a boundary-crossing copy.
 *         [FA] جدیدترین فریم کامل پنج‌نمونه‌ای را در out[] کپی می‌کند.
 *              شمارندهٔ DMA نیمه‌ای را انتخاب می‌کند که DMA در آن نمی‌نویسد؛
 *              بررسی شمارنده قبل و بعد، کپی عبوری از مرز را رد می‌کند.
 * @param  uint16_t__out [EN] Output array with BSP_ADC_CHANNEL_COUNT elements;
 *                            index order = BSP_ADC_CHANNEL_* /
 *                            آرایهٔ خروجی با ترتیب BSP_ADC_CHANNEL_*
 * @return bool [EN] true when a stable frame was copied, false otherwise /
 *                   اگر فریم پایدار کپی شد true وگرنه false
 */
bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT]);

#endif /* BSP_ADC_H */
