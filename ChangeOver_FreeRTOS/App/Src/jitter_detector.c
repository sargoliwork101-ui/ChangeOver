/**
 * @file App/Src/jitter_detector.c
 * @brief C source for jitter_detector.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "jitter_detector.h"

#include "app_config.h"

/**
 * @brief Initializes the module state and its dependencies.
 * @function jitter_detector_init
 * @param detector Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void jitter_detector_init(jitter_detector_t * detector)
{
    if (detector != NULL)
    {
        detector->queue = xQueueCreateStatic(UINT32_C(16),
                                             (UBaseType_t)sizeof(jitter_edge_event_t),
                                             detector->queue_buffer,
                                             &detector->queue_storage);
        detector->last_edge[0] = (TickType_t)0;
        detector->last_edge[1] = (TickType_t)0;
        detector->active[0] = false;
        detector->active[1] = false;
    }
}

/**
 * @brief Handles a hardware or RTOS event in callback/ISR context.
 * @function jitter_detector_notify_edge_from_isr
 * @param detector Input or state associated with the operation.
 * @param channel Input or state associated with the operation.
 * @safety ISR/callback rule: use only ISR-safe APIs, do not block,
 *         and keep the execution path deterministic.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void jitter_detector_notify_edge_from_isr(jitter_detector_t * detector,
                                          jitter_channel_t channel)
{
    jitter_edge_event_t event;
    BaseType_t higher_priority_task_woken;

    if ((detector != NULL) && (detector->queue != NULL))
    {
        event.channel = channel;
        event.tick = xTaskGetTickCountFromISR();
        higher_priority_task_woken = pdFALSE;
        (void)xQueueSendFromISR(detector->queue,
                                &event,
                                &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

/**
 * @brief Executes one deterministic update cycle.
 * @function jitter_detector_process
 * @param detector Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void jitter_detector_process(jitter_detector_t * detector)
{
    jitter_edge_event_t event;
    TickType_t now;
    TickType_t timeout;
    uint32_t index;

    if ((detector != NULL) && (detector->queue != NULL))
    {
        while (xQueueReceive(detector->queue, &event, (TickType_t)0) == pdPASS)
        {
            index = (uint32_t)event.channel;
            if (index < UINT32_C(2))
            {
                detector->last_edge[index] = event.tick;
                detector->active[index] = true;
            }
        }

        now = xTaskGetTickCount();
        timeout = pdMS_TO_TICKS(APP_CONFIG.jitter_timeout_ms);
        for (index = UINT32_C(0); index < UINT32_C(2); ++index)
        {
            if ((detector->last_edge[index] == (TickType_t)0) ||
                ((now - detector->last_edge[index]) > timeout))
            {
                detector->active[index] = false;
            }
        }
    }
}

/**
 * @brief Reports the requested module condition.
 * @function jitter_detector_is_active
 * @param detector Input or state associated with the operation.
 * @param channel Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool jitter_detector_is_active(const jitter_detector_t * detector,
                               jitter_channel_t channel)
{
    uint32_t index;
    bool active;

    active = false;
    if (detector != NULL)
    {
        index = (uint32_t)channel;
        if (index < UINT32_C(2))
        {
            active = detector->active[index];
        }
    }
    return active;
}
