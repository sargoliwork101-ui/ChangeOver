/**
 * @file    rtos_app.h
 * @brief   [EN] Starts application tasks and the FreeRTOS scheduler. Full type naming, func_ prefix.
 *          [FA] تسک‌های برنامه و زمان‌بند FreeRTOS را راه می‌اندازد. نام تایپ کامل.
 */

#ifndef RTOS_APP_H
#define RTOS_APP_H

/**
 * @brief  [EN] Create static tasks then call vTaskStartScheduler(). Does not return.
 *         [FA] تسک‌ها را استاتیک می‌سازد و scheduler را روشن می‌کند. برنمی‌گردد.
 */
void func_Rtos_Start(void);

#endif /* RTOS_APP_H */

