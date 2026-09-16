/**
 * @file    app.h
 * @brief   [EN] Bridge from CubeMX main.c into application code. Full type naming, func__ prefix.
 *          [FA] پل بین main.c تولیدشده CubeMX و کد برنامه. نام تایپ کامل.
 *
 * @note    [EN] CubeMX may regenerate main.c. Keep product logic out of main.
 *          [FA] CubeMX ممکن است main.c را از نو بسازد. منطق محصول آن‌جا نرود.
 */

#ifndef APP_H
#define APP_H

/**
 * @brief  [EN] Initialize the application and start CMSIS-RTOS2. Does not return.
 *         [FA] برنامه را مقداردهی و CMSIS-RTOS2 را شروع می‌کند. برنمی‌گردد.
 */
/* ==================== Functions ==================== */
void func__App_Start(void);

/**
 * @brief  [EN] Module init only, without starting the scheduler.
 *         [FA] فقط Init ماژول‌ها، بدون روشن کردن scheduler.
 */
void func__App_Init(void);

#endif /* APP_H */

