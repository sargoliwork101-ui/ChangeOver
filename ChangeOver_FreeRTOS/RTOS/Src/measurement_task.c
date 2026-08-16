/**
 * @file RTOS/Src/measurement_task.c
 * @brief C source for measurement_task.
 * @details This file belongs to the FreeRTOS task integration layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "measurement_task.h"

/**
 * @brief Implements the module operation represented by this API.
 * @function measurement_task_entry
 * @param context Input or state associated with the operation.
 * @safety This is a FreeRTOS task entry. It may block only through
 *         approved RTOS scheduling calls and must not return normally.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static void measurement_task_entry(void * context)
{
    measurement_task_t * task;
    uint32_t notified;
    uint32_t index;
    bool adc_started;
    bool out_of_range;

    task = (measurement_task_t *)context;
    if ((task != NULL) && (task->adc != NULL) &&
        (task->measurements != NULL))
    {
        task->handle = xTaskGetCurrentTaskHandle();
        adc_driver_attach_task(task->adc, task->handle);
        adc_started = adc_driver_start_scan_dma(task->adc,
                                                task->dma_buffer,
                                                APP_CONFIG.adc_channel_count);
        if ((!adc_started) && (task->diagnostics != NULL))
        {
            diagnostics_report(task->diagnostics,
                               DIAG_ADC_DMA_START_FAILED,
                               DIAG_SEVERITY_ERROR,
                               UINT32_C(0),
                               FAULT_ADC_OUT_OF_RANGE,
                               SYSTEM_STATE_SELF_TEST,
                               (uint32_t)xTaskGetTickCount());
        }

        for (;;)
        {
            notified = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (notified > UINT32_C(0))
            {
                out_of_range = false;
                for (index = UINT32_C(0);
                     index < APP_CONFIG.adc_channel_count;
                     ++index)
                {
                    if (task->dma_buffer[index] > APP_CONFIG.adc_max_code)
                    {
                        out_of_range = true;
                    }
                }
                if (out_of_range && (task->diagnostics != NULL))
                {
                    diagnostics_report(task->diagnostics,
                                       DIAG_ADC_OUT_OF_RANGE,
                                       DIAG_SEVERITY_ERROR,
                                       APP_CONFIG.adc_max_code,
                                       FAULT_ADC_OUT_OF_RANGE,
                                       SYSTEM_STATE_FAULT,
                                       (uint32_t)xTaskGetTickCount());
                }
                measurement_manager_update_from_dma(
                    task->measurements,
                    task->dma_buffer,
                    APP_CONFIG.adc_channel_count);
            }
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Creates the statically allocated FreeRTOS object.
 * @function measurement_task_create
 * @param task Input or state associated with the operation.
 * @param adc Input or state associated with the operation.
 * @param measurements Measurement manager context.
 * @param diagnostics Diagnostic event context.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool measurement_task_create(measurement_task_t * task,
                             adc_driver_t * adc,
                             measurement_manager_t * measurements,
                             diagnostics_t * diagnostics)
{
    bool created;

    created = false;
    if ((task != NULL) && (adc != NULL) &&
        (measurements != NULL) && (diagnostics != NULL))
    {
        task->adc = adc;
        task->measurements = measurements;
        task->diagnostics = diagnostics;
        task->handle = NULL;
        task->handle = xTaskCreateStatic(measurement_task_entry,
                                         "measurement",
                                         MEASUREMENT_TASK_STACK_WORDS,
                                         task,
                                         MEASUREMENT_TASK_PRIORITY,
                                         task->stack,
                                         &task->control_block);
        created = task->handle != NULL;
    }
    return created;
}
