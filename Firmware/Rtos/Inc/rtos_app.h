/**
 * @file    rtos_app.h
 * @brief   [EN] Starts application threads and the CMSIS-RTOS2 kernel.
 *          [FA] تسک‌های برنامه و کرنل CMSIS-RTOS2 را راه‌اندازی می‌کند.
 */

#ifndef RTOS_APP_H
#define RTOS_APP_H

/**
 * @brief  [EN] Create statically allocated CMSIS-RTOS2 threads and start the kernel.
 *         [FA] تسک‌های CMSIS-RTOS2 با حافظهٔ ثابت را می‌سازد و کرنل را شروع می‌کند.
 */
/* ==================== Functions ==================== */
void func__Rtos_Start(void);

#endif /* RTOS_APP_H */
