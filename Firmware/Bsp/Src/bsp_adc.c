/**
 * @file    bsp_adc.c
 * @brief   [EN] ADC+DMA board layer. ADC1 is calibrated once, then hardware
 *              continuously fills a circular two-frame buffer. The CPU only
 *              polls DMA progress when a measurement snapshot is requested.
 *              Static RAM only; no malloc (MISRA / project memory rule).
 *          [FA] لایهٔ برد ADC+DMA. ADC1 یک‌بار کالیبره می‌شود و سپس سخت‌افزار
 *              بافر چرخشی دو فریمی را پیوسته پر می‌کند. CPU فقط هنگام درخواست
 *              snapshot پیشرفت DMA را می‌خواند. فقط RAM استاتیک؛ بدون malloc.
 *
 * @note    [EN] The raw buffer is volatile because DMA changes it outside the
 *              C execution flow. HAL receives its address as an integer-shaped
 *              pointer only; transfers are configured as halfwords in the MSP.
 *          [FA] بافر خام volatile است چون DMA خارج از جریان اجرای C آن را
 *              تغییر می‌دهد. HAL فقط آدرس آن را به‌شکل اشاره‌گر عددی می‌گیرد؛
 *              انتقال‌ها در MSP به‌صورت نصف‌واژه تنظیم شده‌اند.
 */

/* ==================== Includes ==================== */
#include "bsp_adc.h"
#include "main.h"

#include <stddef.h>

/* ==================== Static State ==================== */

/* [EN] Board ADC handle is private to this STM32 BSP implementation.
 *      [FA] هندل ADC برد فقط در پیاده‌سازی BSP مخصوص STM32 خصوصی است. */
static ADC_HandleTypeDef *ADC_HANDLETYPEDEF__G__Hadc = NULL;

/* [EN] Hardware-filled circular DMA buffer: 2 frames x 5 channels.
 *      [FA] بافر چرخشی پرشدهٔ سخت‌افزاری: ۲ فریم x ۵ کانال. */
static volatile uint16_t UINT16_T__G__DmaBuffer[BSP_ADC_DMA_SAMPLE_COUNT];

/* [EN] Set when calibration and DMA start succeed.
 *      [FA] وقتی کالیبراسیون و شروع DMA موفق باشد true می‌شود. */
static bool BOOL__G__Running = false;

/* ==================== BspAdc_Init ==================== */

/**
 * @brief  [EN] Select the current board ADC handle and clear the DMA buffer.
 *         [FA] هندل ADC برد فعلی را انتخاب و بافر DMA را صفر می‌کند.
 */
void func__BspAdc_Init(void)
{
    uint32_t uint32_t__i;

    ADC_HANDLETYPEDEF__G__Hadc = &hadc1;
    BOOL__G__Running = false;

    for (uint32_t__i = 0u; uint32_t__i < BSP_ADC_DMA_SAMPLE_COUNT; uint32_t__i++)
    {
        UINT16_T__G__DmaBuffer[uint32_t__i] = 0u;
    }
}

/* ==================== BspAdc_Start ==================== */

/**
 * @brief  [EN] Calibrate ADC1 and start continuous scan with circular DMA.
 *         [FA] ADC1 را کالیبره و اسکن مداوم با DMA چرخشی را شروع می‌کند.
 * @return bool [EN] true when calibration and HAL start succeed / اگر هر دو
 *                   کالیبراسیون و شروع HAL موفق باشند true
 */
bool func__BspAdc_Start(void)
{
    uint32_t uint32_t__dmaStartTick;
    uint32_t uint32_t__dmaCounter;

    BOOL__G__Running = false;

    if ((ADC_HANDLETYPEDEF__G__Hadc == NULL) ||
        (ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle == NULL))
    {
        return false;
    }

    /* [EN] F1 calibration must run while the ADC is not converting.
       [FA] کالیبراسیون F1 باید زمانی اجرا شود که ADC در حال تبدیل نیست. */
    if (HAL_ADCEx_Calibration_Start(ADC_HANDLETYPEDEF__G__Hadc) != HAL_OK)
    {
        return false;
    }

    /* [EN] F1 HAL accepts uint32_t* but the MSP configures halfword DMA. The
       integer-shaped cast passes the buffer address; DMA writes uint16_t items.
       [FA] HAL در F1 پارامتر uint32_t* می‌گیرد، اما MSP DMA را نصف‌واژه تنظیم
       کرده است. cast فقط آدرس بافر را می‌دهد و DMA آیتم‌های uint16_t می‌نویسد. */
    if (HAL_ADC_Start_DMA(ADC_HANDLETYPEDEF__G__Hadc,
                          (uint32_t *)(uintptr_t)&UINT16_T__G__DmaBuffer[0],
                          BSP_ADC_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        return false;
    }

    /* [EN] HAL_ADC_Start_DMA internally uses HAL_DMA_Start_IT(). Polling is
       intentional here, so suppress the DMA interrupt sources after start.
       [FA] HAL_ADC_Start_DMA در داخل از HAL_DMA_Start_IT استفاده می‌کند.
       اینجا عمداً polling داریم، پس منابع وقفهٔ DMA بعد از start خاموش می‌شوند. */
    __HAL_DMA_DISABLE_IT(ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle,
                         DMA_IT_TC | DMA_IT_HT | DMA_IT_TE);

    /* [EN] Do not expose a running reader until the first half has completed.
       This bounded startup wait prevents the initial zero-filled second half
       from being reported as a valid frame. Normal wait is about 28 us at
       12 MHz; the timeout only handles a stalled peripheral.
       [FA] تا کامل‌شدن نیمهٔ اول، خواننده را فعال اعلام نمی‌کند. این انتظار
       محدود مانع می‌شود نیمهٔ دومِ اولیه و صفرشده فریم معتبر گزارش شود.
       زمان عادی در 12MHz حدود 28us است و timeout فقط خرابی peripheral را
       پوشش می‌دهد. */
    uint32_t__dmaStartTick = HAL_GetTick();
    do
    {
        uint32_t__dmaCounter = __HAL_DMA_GET_COUNTER(ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle);
    }
    while ((uint32_t__dmaCounter > BSP_ADC_CHANNEL_COUNT) &&
           ((HAL_GetTick() - uint32_t__dmaStartTick) < BSP_ADC_START_TIMEOUT_MS));

    if (uint32_t__dmaCounter > BSP_ADC_CHANNEL_COUNT)
    {
        (void)HAL_ADC_Stop_DMA(ADC_HANDLETYPEDEF__G__Hadc);
        return false;
    }

    BOOL__G__Running = true;
    return true;
}

/* ==================== BspAdc_IsFrameReady ==================== */

/**
 * @brief  [EN] Report whether calibration and DMA start completed successfully.
 *         [FA] اعلام می‌کند کالیبراسیون و شروع DMA با موفقیت کامل شده است.
 * @return bool [EN] true after successful start / بعد از شروع موفق true
 */
bool func__BspAdc_IsFrameReady(void)
{
    return BOOL__G__Running;
}

/* ==================== BspAdc_GetRaw ==================== */

/**
 * @brief  [EN] Copy the newest completed five-sample frame. DMA CNDTR selects
 *              the half not being written; the counter is checked before and
 *              after the copy so a moving half-buffer boundary is rejected.
 *         [FA] جدیدترین فریم کامل پنج‌نمونه‌ای را کپی می‌کند. CNDTR DMA نیمه‌ای
 *              را که در حال نوشتن نیست انتخاب می‌کند؛ شمارنده قبل و بعد بررسی
 *              می‌شود تا مرز متحرک نیمه باعث کپی ناپایدار نشود.
 * @param  uint16_t__out [EN] Output array with BSP_ADC_CHANNEL_COUNT elements /
 *                            آرایهٔ خروجی با تعداد کانال‌ها
 * @return bool [EN] true when a stable frame was copied / اگر فریم پایدار کپی شد
 */
bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT])
{
    DMA_HandleTypeDef *DMA_HANDLETYPEDEF__dmaHandle;
    uint32_t uint32_t__attempt;
    uint32_t uint32_t__i;
    uint32_t uint32_t__dmaCounterBefore;
    uint32_t uint32_t__dmaCounterAfter;
    uint32_t uint32_t__sourceOffset;
    uint32_t uint32_t__savedPrimask;
    bool bool__stableWindow;

    if ((BOOL__G__Running == false) ||
        (uint16_t__out == NULL) ||
        (ADC_HANDLETYPEDEF__G__Hadc == NULL) ||
        (ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle == NULL))
    {
        return false;
    }

    DMA_HANDLETYPEDEF__dmaHandle = ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle;

    for (uint32_t__attempt = 0u;
         uint32_t__attempt < BSP_ADC_DMA_RETRY_COUNT;
         uint32_t__attempt++)
    {
        uint32_t__dmaCounterBefore = __HAL_DMA_GET_COUNTER(DMA_HANDLETYPEDEF__dmaHandle);

        if ((uint32_t__dmaCounterBefore == 0u) ||
            (uint32_t__dmaCounterBefore > BSP_ADC_DMA_SAMPLE_COUNT))
        {
            continue;
        }

        /* [EN] CNDTR > 5 means DMA writes the first half, so the second half
           is complete. CNDTR <= 5 means the first half is complete.
           [FA] اگر CNDTR بزرگ‌تر از ۵ باشد DMA در نیمهٔ اول می‌نویسد، پس
           نیمهٔ دوم کامل است. اگر CNDTR <= ۵ باشد نیمهٔ اول کامل است. */
        if (uint32_t__dmaCounterBefore > BSP_ADC_CHANNEL_COUNT)
        {
            uint32_t__sourceOffset = BSP_ADC_CHANNEL_COUNT;
        }
        else
        {
            uint32_t__sourceOffset = 0u;
        }

        uint32_t__savedPrimask = __get_PRIMASK();
        __disable_irq();

        for (uint32_t__i = 0u;
             uint32_t__i < BSP_ADC_CHANNEL_COUNT;
             uint32_t__i++)
        {
            uint16_t__out[uint32_t__i] =
                UINT16_T__G__DmaBuffer[uint32_t__sourceOffset + uint32_t__i];
        }

        uint32_t__dmaCounterAfter = __HAL_DMA_GET_COUNTER(DMA_HANDLETYPEDEF__dmaHandle);
        __set_PRIMASK(uint32_t__savedPrimask);

        bool__stableWindow =
            ((uint32_t__dmaCounterBefore > BSP_ADC_CHANNEL_COUNT) ==
             (uint32_t__dmaCounterAfter > BSP_ADC_CHANNEL_COUNT));

        if (bool__stableWindow &&
            (uint32_t__dmaCounterAfter != 0u) &&
            (uint32_t__dmaCounterAfter <= BSP_ADC_DMA_SAMPLE_COUNT))
        {
            return true;
        }
    }

    return false;
}
