/**
 * @file    bsp_adc.c
 * @brief   [EN] ADC+DMA board layer. HAL start only; afterwards the ADC+DMA
 *              hardware fills the circular buffer in RAM without any CPU
 *              involvement (no polling, no interrupt). Static RAM only,
 *              no malloc (MISRA / project memory rule).
 *          [FA] لایهٔ برد ADC+DMA. فقط استارت HAL؛ بعد از آن سخت‌افزار
 *              ADC+DMA بافر چرخشی را در RAM بدون هیچ درگیری CPU پر می‌کند
 *              (بدون پالتینگ و بدون قطع‌کننده). فقط RAM استاتیک، بدون
 *              malloc (MISRA / قانون حافظهٔ پروژه).
 *
 * @note    [EN] The DMA buffer is intentionally NOT volatile: the F1 HAL DMA
 *              API requires a plain pointer, and GetRaw() reads it under a
 *              short PRIMASK critical section, which makes the 5-halfword
 *              copy atomic with respect to the CPU (the DMA keeps writing in
 *              hardware either way).
 *          [FA] بافر DMA عمداً volatile نیست: API درایور DMA در F1 اشاره‌گر
 *              معمولی می‌خواهد و GetRaw() آن را داخل critical section کوتاه
 *              PRIMASK می‌خواند که کپی ۵ نصف‌واژه را از دید CPU اتمی
 *              می‌کند (DMA در هر حال در سخت‌افزار می‌نویسد).
 */

/* ==================== Includes ==================== */
#include "bsp_adc.h"
#include <stddef.h>

/* ==================== Static State ==================== */

/* [EN] CubeMX handle (hadc1), stored once at init / هندل مکعب، یک‌بار در Init */
static ADC_HandleTypeDef *ADC_HANDLETYPEDEF__G__Hadc = NULL;

/* [EN] Hardware-filled circular DMA buffer: 2 frames x 5 channels.
 *      This is the "variable the micro fills without the CPU".
 *      [FA] بافر چرخشی پرشده‌ی سخت‌افزاری: ۲ فریم x ۵ کانال.
 *      همین «متغیری» است که میکرو بدون CPU پر می‌کند. */
static uint16_t UINT16_T__G__DmaBuffer[BSP_ADC_DMA_SAMPLE_COUNT];

/* [EN] Set when Start succeeded; gates IsFrameReady/GetRaw.
 *      [FA] وقتی Start موفق بود true می‌شود؛ دروازهٔ IsFrameReady/GetRaw. */
static bool BOOL__G__Running = false;

/* ==================== BspAdc_Init ==================== */

/**
 * @brief  [EN] Store the CubeMX HAL handle and zero the DMA buffer.
 *         [FA] هندل HAL مکعب را نگه می‌دارد و بافر DMA را صفر می‌کند.
 * @param  ADC_HandleTypeDef__hadc [EN] HAL ADC handle from CubeMX (hadc1);
 *                                     NULL clears the handle / هندل ADC مکعب
 */
void func__BspAdc_Init(ADC_HandleTypeDef *ADC_HandleTypeDef__hadc)
{
    uint32_t uint32_t__i;

    ADC_HANDLETYPEDEF__G__Hadc = ADC_HandleTypeDef__hadc;
    BOOL__G__Running = false;

    for (uint32_t__i = 0u; uint32_t__i < BSP_ADC_DMA_SAMPLE_COUNT; uint32_t__i++)
    {
        UINT16_T__G__DmaBuffer[uint32_t__i] = 0u;
    }
}

/* ==================== BspAdc_Start ==================== */

/**
 * @brief  [EN] Start continuous scan + circular DMA.
 *         [FA] شروع اسکن مداوم + DMA چرخشی.
 * @return bool [EN] true when HAL started the DMA / اگر شروع شد true
 */
bool func__BspAdc_Start(void)
{
    if (ADC_HANDLETYPEDEF__G__Hadc == NULL)
    {
        return false;
    }

    /* [EN] F1 HAL signature takes uint32_t*, the DMA itself moves halfwords
       (halfword/halfword in the MSP). The cast is the standard HAL usage.
       [FA] امضای HAL در F1، uint32_t* می‌خواهد ولی خود DMA نصف‌واژه جابه‌جا
       می‌کند (نیم‌واژه/نیم‌واژه در MSP). این cast همان استفادهٔ استاندارد HAL است. */
    if (HAL_ADC_Start_DMA(ADC_HANDLETYPEDEF__G__Hadc,
                          (uint32_t *)UINT16_T__G__DmaBuffer,
                          BSP_ADC_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        BOOL__G__Running = false;
        return false;
    }

    BOOL__G__Running = true;
    return true;
}

/* ==================== BspAdc_IsFrameReady ==================== */

/**
 * @brief  [EN] True once Start succeeded. The first full frame is available
 *              ~0.1 ms after Start (10 conversions at 9 MHz); the measurement
 *              task delays MEASUREMENT_SETTLE_MS (1 ms) before the first read.
 *         [FA] وقتی Start موفق بود true. اولین فریم کامل ~0.1ms بعد از Start
 *              (۱۰ تبدیل در 9MHz)؛ تسک اندازه‌گیری قبل از اولین خواندن 1ms
 *              صبر می‌کند.
 * @return bool [EN] true when a frame can be read / وقتی فریم قابل‌خواندن است
 */
bool func__BspAdc_IsFrameReady(void)
{
    return BOOL__G__Running;
}

/* ==================== BspAdc_GetRaw ==================== */

/**
 * @brief  [EN] Copy the newest 5-sample frame (DMA slot 1) into out[] under
 *              a short critical section.
 *         [FA] آخرین فریم ۵ نمونه‌ای (اسلات ۱ DMA) را در out[] داخل یک
 *              critical section کوتاه کپی می‌کند.
 * @param  uint16_t__out [EN] Output array, BSP_ADC_CHANNEL_COUNT elements /
 *                            آرایهٔ خروجی
 * @return bool [EN] true when copied, false if not started / اگر کپی شد true
 */
bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT])
{
    uint32_t uint32_t__i;
    uint32_t uint32_t__savedPrimask;

    if (BOOL__G__Running == false)
    {
        return false;
    }

    /* [EN] Critical section is a few CPU cycles (5 halfword copies). It makes
       the copy atomic with respect to other CPU code; the DMA still writes in
       hardware, so a torn frame can mix at most one adjacent rotation
       (~40 us) - acceptable for the 10 ms measurement period.
       [FA] critical section چند سیکل CPU است (کپی ۵ نصف‌واژه) و کپی را از
       دید بقیهٔ کد CPU اتمی می‌کند؛ DMA در حال نوشتن سخت‌افزاری است، پس
       بدترین فریمِ پاره‌شده حداکثر یک دور مجاور (~40us) را قاطی می‌کند —
       برای دورهٔ ۱۰ms اندازه‌گیری قابل‌قبول است. */
    uint32_t__savedPrimask = __get_PRIMASK();
    __disable_irq();

    for (uint32_t__i = 0u; uint32_t__i < BSP_ADC_CHANNEL_COUNT; uint32_t__i++)
    {
        uint16_t__out[uint32_t__i] =
            UINT16_T__G__DmaBuffer[BSP_ADC_CHANNEL_COUNT + uint32_t__i];
    }

    __set_PRIMASK(uint32_t__savedPrimask);

    return true;
}
