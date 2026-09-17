/**
 * @file    bsp_exti.c
 * @brief   [EN] STM32F103C8T6 EXTI adapter for JITTER1, JITTER2 and 24V input.
 *          [FA] adapter خطوط EXTI برای JITTER1، JITTER2 و ورودی ۲۴ ولت.
 *
 * @note    [EN] PB2, PB6 and PB4 are configured as rising/falling EXTI inputs
 *              in CubeMX. This port translates HAL pin callbacks into logical
 *              flags and keeps HAL details out of modules.
 *          [FA] پایه‌های PB2، PB6 و PB4 در CubeMX به‌صورت EXTI دو لبه تنظیم
 *              شده‌اند. این پورت callback HAL را به پرچم منطقی تبدیل می‌کند.
 *          [EN] JIT (LM393) fast protection: very short ISR, no RTOS API, latch fault, PWM trip.
 *          [FA] حفاظت سریع JIT: بسیار کوتاه، بدون RTOS.
 */

#include "bsp_exti.h"
#include "board_pins.h"
#include "bsp_pwm.h"
#include "charger.h" 

static volatile uint8_t UINT8_T__G__Flags[BSP_EXTI_SOURCE_COUNT];

/* ==================== BspExti_Init ==================== */
/**
 * @brief  [EN] Clear all board event flags before the application starts.
 *         [FA] پیش از شروع برنامه همهٔ پرچم‌های رویداد برد را پاک می‌کند.
 */
void func__BspExti_Init(void)
{
    uint32_t uint32_t__index;

    for (uint32_t__index = 0u;
         uint32_t__index < (uint32_t)BSP_EXTI_SOURCE_COUNT;
         uint32_t__index++)
    {
        UINT8_T__G__Flags[uint32_t__index] = 0u;
    }
}

/* ==================== BspExti_OnIrq ==================== */
/**
 * @brief  [EN] Latch one logical event from an IRQ callback.
 *         [FA] یک رویداد منطقی را از callback وقفه قفل می‌کند.
 * @param  bsp_exti_src_t__src [EN] Logical source / منبع منطقی
 */
void func__BspExti_OnIrq(bsp_exti_src_t bsp_exti_src_t__src)
{
    if ((uint32_t)bsp_exti_src_t__src < (uint32_t)BSP_EXTI_SOURCE_COUNT)
    {
        UINT8_T__G__Flags[bsp_exti_src_t__src] = 1u;
    }
}

/* ==================== BspExti_TakeEvent ==================== */
/**
 * @brief  [EN] Read and clear one logical event flag.
 *         [FA] پرچم یک رویداد منطقی را می‌خواند و پاک می‌کند.
 * @param  bsp_exti_src_t__src [EN] Logical source / منبع منطقی
 * @return bool [EN] true when an event was pending / اگر رویداد pending باشد true
 */
bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__src)
{
    bool bool__taken = false;

    if ((uint32_t)bsp_exti_src_t__src < (uint32_t)BSP_EXTI_SOURCE_COUNT)
    {
        bool__taken = (UINT8_T__G__Flags[bsp_exti_src_t__src] != 0u);
        UINT8_T__G__Flags[bsp_exti_src_t__src] = 0u;
    }

    return bool__taken;
}

/* ==================== HAL_GPIO_EXTI_Callback ==================== */
/**
 * @brief  [EN] Translate a HAL EXTI pin callback into a logical board event.
 *         Fast JIT path: latch fault and trip PWM directly in ISR, no RTOS API, no blocking.
 *         [FA] callback پایهٔ EXTI در HAL را به رویداد منطقی برد تبدیل می‌کند. مسیر سریع JIT.
 * @param  uint16_t__GPIO_Pin [EN] HAL pin mask / ماسک پایهٔ HAL
 */
void HAL_GPIO_EXTI_Callback(uint16_t uint16_t__GPIO_Pin)
{
    if (uint16_t__GPIO_Pin == PIN_JITTER1_PIN)
    {
        /* [EN] Fast protection: latch, trip PWM ISR-safe, mask EXTI to prevent storm at 50kHz.
           Only active edge (provisional RISING) should be used; verify with scope.
           No osDelay, queue, mutex, malloc, logging. PB4 not mixed.
           [FA] حفاظت سریع JIT: latch، تریپ، mask. */
        func__BspPwm_TripOffFromIsr();
        func__Charger_OnJitTrip(CHG_FAULT_JIT1);
        func__BspExti_MaskJit(BSP_EXTI_JITTER1);
        func__BspExti_OnIrq(BSP_EXTI_JITTER1);
    }
    else if (uint16_t__GPIO_Pin == PIN_JITTER2_PIN)
    {
        func__BspPwm_TripOffFromIsr();
        func__Charger_OnJitTrip(CHG_FAULT_JIT2);
        func__BspExti_MaskJit(BSP_EXTI_JITTER2);
        func__BspExti_OnIrq(BSP_EXTI_JITTER2);
    }
    else if (uint16_t__GPIO_Pin == PIN_INT_24_IN_PIN)
    {
        /* [EN] Input detect (PB4) independent from PB5/Q1 and PB2/PB6 JIT. Only latch event, no PB5/Q1.
           [FA] ورودی 24ولت مستقل از PB5/Q1 و JIT. */
        func__BspExti_OnIrq(BSP_EXTI_INPUT_DETECT);
    }
    else
    {
        /* [EN] Unknown EXTI sources are intentionally ignored.
           [FA] منابع EXTI ناشناخته عمداً نادیده گرفته می‌شوند. */
    }
}

/* ==================== BspExti Mask/Unmask JIT ==================== */
void func__BspExti_MaskJit(bsp_exti_src_t bsp_exti_src_t__src)
{
    if (bsp_exti_src_t__src == BSP_EXTI_JITTER1)
    {
        HAL_NVIC_DisableIRQ(EXTI2_IRQn);
    }
    else if (bsp_exti_src_t__src == BSP_EXTI_JITTER2)
    {
        HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    }
    else
    {
        /* [EN] Only JIT channels are maskable.
           [FA] فقط JIT. */
    }
}

void func__BspExti_UnmaskJit(bsp_exti_src_t bsp_exti_src_t__src)
{
    if (bsp_exti_src_t__src == BSP_EXTI_JITTER1)
    {
        HAL_NVIC_ClearPendingIRQ(EXTI2_IRQn);
        HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    }
    else if (bsp_exti_src_t__src == BSP_EXTI_JITTER2)
    {
        HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
        HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    }
    else
    {
    }
}
