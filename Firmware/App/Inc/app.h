/**
 * @file    app.h
 * @brief   [EN] Bridge from CubeMX main.c into application code. Full type naming, func__ prefix.
 * @note    [EN] CubeMX may regenerate main.c. Keep product logic out of main.
 */
#ifndef APP_H
#define APP_H
/**
 * @brief  [EN] Initialize the application and start CMSIS-RTOS2. Does not return.
 */
/* ==================== Functions ==================== */
void func__App_Start(void);
/**
 * @brief  [EN] Module init only, without starting the scheduler.
 */
void func__App_Init(void);
#endif /* APP_H */
