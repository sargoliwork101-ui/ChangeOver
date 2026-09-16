/**
 * @file    bsp_adc.h
 * @brief   [EN] Logical board ADC+DMA interface. The port supplies five
 *              normalized analog values in a two-frame buffer; the reader
 *              copies only a completed frame.
 *          [FA] رابط منطقی ADC+DMA برد. پورت پنج مقدار آنالوگ استانداردشده
 *              را در بافر دو فریمی می‌دهد و خواننده فقط فریم کامل را کپی می‌کند.
 *
 * @note    [EN] The public channel order is a normalized data contract. Each
 *              board-specific BSP maps its own ADC hardware to this order.
 *          [FA] ترتیب عمومی کانال‌ها قرارداد دادهٔ استاندارد است. BSP مخصوص
 *              هر برد سخت‌افزار ADC همان برد را به این ترتیب نگاشت می‌کند.
 */

#ifndef BSP_ADC_H
#define BSP_ADC_H

/* ==================== Includes ==================== */
#include <stdint.h>
#include <stdbool.h>

/* ==================== ADC channel map ==================== */
/* [EN] These are normalized frame positions; physical channels and pins
 *      are selected only by the board-specific BSP implementation.
 * [FA] این‌ها موقعیت‌های استاندارد فریم هستند؛ کانال‌ها و پایه‌های فیزیکی
 *      فقط در پیاده‌سازی BSP مخصوص برد انتخاب می‌شوند. */
#define BSP_ADC_CHANNEL_COUNT        5u
#define BSP_ADC_CHANNEL_CURRENT1     0u   /* normalized charge current 1 / جریان شارژ استاندارد ۱ */
#define BSP_ADC_CHANNEL_24V_IN       1u   /* normalized 24V input / ورودی ۲۴ ولت استاندارد */
#define BSP_ADC_CHANNEL_24V_BAT      2u   /* normalized 24V battery / باتری ۲۴ ولت استاندارد */
#define BSP_ADC_CHANNEL_12V_BAT      3u   /* normalized 12V battery / باتری ۱۲ ولت استاندارد */
#define BSP_ADC_CHANNEL_CURRENT2     4u   /* normalized charge current 2 / جریان شارژ استاندارد ۲ */

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

/* ==================== BspAdc_Init ==================== */

/**
 * @brief  [EN] Select the board ADC backend and clear its DMA buffer.
 *         [FA] Backend ADC برد را انتخاب و بافر DMA آن را صفر می‌کند.
 */
void func__BspAdc_Init(void);

/* ==================== BspAdc_Start ==================== */

/**
 * @brief  [EN] Calibrate the board ADC backend, then start its scan and DMA.
 *              DMA interrupt sources remain private to the board port; the
 *              public reader only returns completed normalized frames.
 *         [FA] ADC مخصوص برد را کالیبره و اسکن و DMA آن را شروع می‌کند.
 *              منابع وقفهٔ DMA در پورت برد خصوصی هستند و خوانندهٔ عمومی فقط
 *              فریم‌های استانداردشدهٔ کامل را برمی‌گرداند.
 * @return bool [EN] true when the board ADC backend starts successfully /
 *                   اگر backend ADC برد با موفقیت شروع شود true
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
