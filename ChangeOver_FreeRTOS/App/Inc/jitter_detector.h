/**
 * @file App/Inc/jitter_detector.h
 * @brief C interface for jitter_detector.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#ifndef JITTER_DETECTOR_H
#define JITTER_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include "app_types.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

typedef struct
{
    jitter_channel_t channel;
    TickType_t tick;
} jitter_edge_event_t;

typedef struct
{
    StaticQueue_t queue_storage;
    uint8_t queue_buffer[16U * sizeof(jitter_edge_event_t)];
    QueueHandle_t queue;
    TickType_t last_edge[2];
    bool active[2];
} jitter_detector_t;

/**
 * @brief Initializes the module state and its dependencies.
 * @function jitter_detector_init
 * @param detector Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void jitter_detector_init(jitter_detector_t * detector);
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
                                          jitter_channel_t channel);
/**
 * @brief Executes one deterministic update cycle.
 * @function jitter_detector_process
 * @param detector Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void jitter_detector_process(jitter_detector_t * detector);
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
                               jitter_channel_t channel);

#endif /* JITTER_DETECTOR_H */
