# Integration با پروژه‌ی CubeMX

فایل‌های این Repository باید به پروژه‌ی واقعی CubeMX اضافه شوند. فایل `main.c` توسط CubeMX تولید می‌شود و در این اسکلت قرار ندارد.

نمونه‌ی اتصال:

```c
#include "app_entry.h"
#include "FreeRTOS.h"
#include "task.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_USART1_UART_Init();

    firmware_init();
    if (firmware_is_ready())
    {
        vTaskStartScheduler();
    }

    while (1)
    {
        /* نباید به این نقطه برسد. */
    }
}
```

`firmware_entry.c` به handleهای زیر وابسته است:

```text
hadc1
htim2
htim3
huart1
```

اگر CubeMX نام دیگری تولید کرد، همان فایل را تغییر بده.

اگر CubeMX یا کاربر یک `HAL_GPIO_EXTI_Callback` دیگر تعریف کرده است، فقط یک تعریف نهایی باید در Link وجود داشته باشد و هر دو منطق باید در یک تابع ادغام شوند.
