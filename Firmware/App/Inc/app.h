/**
 * @file    app.h
 * @brief   تنها پل بین main.c تولیدشده CubeMX و کد خودمان.
 *
 * main.c را CubeMX هر بار ممکن است از نو بسازد.
 * پس در main فقط App_Start() صدا می‌شود؛ بقیه کار اینجاست.
 */

#ifndef APP_H
#define APP_H

/**
 * @brief ماژول‌ها را Init می‌کند و FreeRTOS را راه می‌اندازد.
 *
 * برنمی‌گردد. معادل این است که در آردوینو loop برای همیشه در FreeRTOS باشد.
 * از main.c بعد از MX_GPIO_Init صدا بزن.
 */
void App_Start(void);

/**
 * @brief فقط Init، بدون scheduler.
 *
 * اگر روزی خواستی scheduler را خود CubeMX با osKernelStart راه بیندازد،
 * این را از Default Task صدا می‌زنی. در این مرحله از App_Start استفاده کن.
 */
void App_Init(void);

#endif /* APP_H */
