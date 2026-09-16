/**
 * @file    app.c
 * @brief   [EN] Starts the application through the CMSIS-RTOS2 boundary.
 *          [FA] برنامه را از مرز CMSIS-RTOS2 راه‌اندازی می‌کند.
 */

#include "app.h"
#include "rtos_app.h"

/**
 * @brief  [EN] Application initialization hook; thread-owned module init runs in its thread.
 *         [FA] نقطهٔ مقداردهی اولیهٔ برنامه؛ Init ماژول صاحب تسک داخل همان تسک انجام می‌شود.
 */
void func__App_Init(void)
{
    /* [EN] Keep startup free of UI state initialization; the UI thread owns it.
       [FA] مقداردهی وضعیت UI در شروع برنامه انجام نمی‌شود؛ مالک آن تسک UI است. */
}

/**
 * @brief  [EN] Initialize the CMSIS-RTOS2 application and start its threads.
 *         [FA] برنامهٔ CMSIS-RTOS2 را مقداردهی و تسک‌های آن را شروع می‌کند.
 */
void func__App_Start(void)
{
    func__App_Init();
    func__Rtos_Start();
}
