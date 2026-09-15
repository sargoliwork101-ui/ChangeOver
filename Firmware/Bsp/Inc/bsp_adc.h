/**
 * @file    bsp_adc.h
 * @brief   [EN] ADC+DMA board layer. The hardware (ADC1 + DMA1, continuous scan,
 *              circular buffer) fills a RAM buffer autonomously; the CPU never
 *              polls the ADC and no interrupt is used. The consumer copies the
 *              newest 5-sample frame on demand.
 *          [FA] لایهٔ برد ADC+DMA. سخت‌افزار (ADC1 + DMA1، اسکن مداوم، بافر
 *              چرخشی) به‌طور مستقل بافر RAM را پر می‌کند؛ CPU هرگز ADC را
 *              پالس نمی‌زند و قطع‌کننده‌ای استفاده نمی‌شود. مصرف‌کننده آخرین
 *              فریم ۵ نمونه‌ای را به‌درخواست کپی می‌کند.
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

/* ==================== Defines ==================== */
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

/* [EN] The DMA buffer holds two full frames (10 halfwords). The reader
 *      always copies the second slot. Two slots give the hardware a full
 *      frame of write margin, so a torn copy can mix at most one adjacent
 *      frame (one rotation ~= 40 us at 9 MHz) - far below the 10 ms
 *      measurement period.
 * [FA] بافر DMA دو فریم کامل (۱۰ نصف‌واژه) نگه می‌دارد. خواننده همیشه
 *      اسلات دوم را کپی می‌کند. دو اسلات به سخت‌افزار یک فریم فضای
 *      نوشتن می‌دهد؛ بدترین کپیِ پاره‌شده حداکثر یک فریم مجاور را قاطی
 *      می‌کند (یک دور ~= 40us در 9MHz) که بسیار کمتر از دوره ۱۰ms است. */
#define BSP_ADC_DMA_FRAME_COUNT      2u
#define BSP_ADC_DMA_SAMPLE_COUNT     (BSP_ADC_CHANNEL_COUNT * BSP_ADC_DMA_FRAME_COUNT)

/* ==================== BspAdc_Init ==================== */

/**
 * @brief  [EN] Store the CubeMX HAL handle and zero the DMA buffer.
 *         [FA] هندل HAL مکعب را نگه می‌دارد و بافر DMA را صفر می‌کند.
 * @param  ADC_HandleTypeDef__hadc [EN] HAL ADC handle from CubeMX (hadc1);
 *                                     must not be NULL / هندل ADC مکعب (hadc1)
 */
void func__BspAdc_Init(ADC_HandleTypeDef *ADC_HandleTypeDef__hadc);

/* ==================== BspAdc_Start ==================== */

/**
 * @brief  [EN] Start continuous scan + circular DMA. After this call the
 *              hardware converts forever without CPU help; the first full
 *              frame is ready ~0.1 ms later (10 conversions at 9 MHz).
 *         [FA] شروع اسکن مداوم + DMA چرخشی. بعد از این صدا، سخت‌افزار بدون
 *              کمک CPU مدام تبدیل می‌کند؛ اولین فریم کامل ~0.1ms بعد آماده
 *              است (۱۰ تبدیل در 9MHz).
 * @return bool [EN] true when HAL started the DMA, false if no handle /
 *                   اگر HAL شروع کرد true، بدون هندل false
 */
bool func__BspAdc_Start(void);

/* ==================== BspAdc_IsFrameReady ==================== */

/**
 * @brief  [EN] True once Start succeeded. The first full frame is available
 *              ~0.1 ms after Start; callers should delay at least
 *              MEASUREMENT_SETTLE_MS (1 ms) before the first GetRaw.
 *         [FA] وقتی Start موفق بود true. اولین فریم کامل ~0.1ms بعد از Start
 *              موجود است؛ صداکننده قبل از اولین GetRaw حداقل
 *              MEASUREMENT_SETTLE_MS (1ms) صبر کند.
 * @return bool [EN] true when a frame can be read / وقتی فریم قابل‌خواندن است
 */
bool func__BspAdc_IsFrameReady(void);

/* ==================== BspAdc_GetRaw ==================== */

/**
 * @brief  [EN] Copy the newest 5-sample frame (DMA slot 1) into out[].
 *              Copies under a short critical section so the 5 halfwords come
 *              from one consistent window.
 *         [FA] آخرین فریم ۵ نمونه‌ای (اسلات ۱ DMA) را در out[] کپی می‌کند.
 *              کپی داخل یک critical section کوتاه انجام می‌شود تا ۵ نصف‌واژه
 *              از یک پنجرهٔ یکسان باشند.
 * @param  uint16_t__out [EN] Output array, must have BSP_ADC_CHANNEL_COUNT
 *                            elements; index order = BSP_ADC_CHANNEL_* /
 *                            آرایهٔ خروجی به ترتیب BSP_ADC_CHANNEL_*
 * @return bool [EN] true when copied, false if not started / اگر کپی شد true
 */
bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT]);

#endif /* BSP_ADC_H */
