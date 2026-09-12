#ifndef APP_H
#define APP_H

/**
 * Called from CubeMX main.c after MX_*_Init().
 * Initializes modules then starts FreeRTOS. Does not return.
 */
void App_Start(void);

/**
 * Module init only. Use this if CubeMX already starts the scheduler
 * and you call us from the default task.
 */
void App_Init(void);

#endif /* APP_H */
