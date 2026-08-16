/**
 * @file RTOS/Src/control_task.c
 * @brief FreeRTOS control task and transition diagnostics.
 * @details This task owns the periodic control transaction: it processes
 *          Jitter events, publishes digital inputs, runs the system state
 *          machine and reports observable transitions.
 * @safety Output writes remain centralized in ActuatorManager. The task never
 *         performs blocking UART operations or power-stage calculations in ISR.
 * @misra  The task is statically allocated and uses bounded periodic work.
 */

#include <stddef.h>
#include "control_task.h"
#include "app_config.h"

/**
 * @brief Converts a state transition into the stable diagnostic code.
 * @function control_task_code_for_state
 * @param state New system state.
 * @return Diagnostic code associated with the state, or NONE when no event is
 *         required for the state.
 * @safety Pure function; no hardware or RTOS service is accessed.
 * @misra  The switch provides an explicit mapping for every public state.
 */
static diagnostic_code_t control_task_code_for_state(system_state_t state)
{
    diagnostic_code_t code;

    code = DIAG_CODE_NONE;
    switch (state)
    {
    case SYSTEM_STATE_INPUT_SOURCE:
        code = DIAG_SOURCE_INPUT;
        break;
    case SYSTEM_STATE_BATTERY_SOURCE:
        code = DIAG_SOURCE_BATTERY;
        break;
    case SYSTEM_STATE_LOW_BATTERY:
        code = DIAG_LOW_BATTERY;
        break;
    case SYSTEM_STATE_FAULT:
        code = DIAG_SYSTEM_FAULT;
        break;
    default:
        break;
    }
    return code;
}

/**
 * @brief Selects the severity for a state transition diagnostic.
 * @function control_task_severity_for_state
 * @param state New system state.
 * @return Severity associated with the state.
 * @safety Pure function with no side effects.
 * @misra  The default severity is informational for non-fault transitions.
 */
static diagnostic_severity_t control_task_severity_for_state(
    system_state_t state)
{
    diagnostic_severity_t severity;

    severity = DIAG_SEVERITY_INFO;
    if (state == SYSTEM_STATE_LOW_BATTERY)
    {
        severity = DIAG_SEVERITY_WARNING;
    }
    else if (state == SYSTEM_STATE_FAULT)
    {
        severity = DIAG_SEVERITY_FATAL;
    }
    else
    {
        /* Informational state transition. */
    }
    return severity;
}

/**
 * @brief Handles one scheduled control transaction.
 * @function control_task_entry
 * @param context Control task context.
 * @safety This is a FreeRTOS task entry. It may block only through approved
 *         RTOS scheduling calls and must not return normally.
 * @misra  ISR work is limited to event enqueueing; state logic runs here.
 */
static void control_task_entry(void * context)
{
    control_task_t * task;
    TickType_t last_wake;
    measurement_snapshot_t measurement;
    system_state_t state;
    diagnostic_code_t state_code;
    diagnostic_severity_t severity;
    bool input_present;
    bool jitter1_active;
    bool jitter2_active;
    fault_mask_t faults;

    task = (control_task_t *)context;
    if ((task != NULL) && (task->jitter != NULL) &&
        (task->measurements != NULL) && (task->actuators != NULL) &&
        (task->system != NULL) && (task->diagnostics != NULL) &&
        (task->faults != NULL))
    {
        task->last_state = SYSTEM_STATE_BOOT;
        task->last_input_present = false;
        task->last_jitter1_active = false;
        task->last_jitter2_active = false;
        last_wake = xTaskGetTickCount();
        for (;;)
        {
            jitter_detector_process(task->jitter);
            input_present = actuator_manager_input_present(task->actuators);
            jitter1_active = jitter_detector_is_active(
                task->jitter, JITTER_CHANNEL_1);
            jitter2_active = jitter_detector_is_active(
                task->jitter, JITTER_CHANNEL_2);
            measurement_manager_set_digital_inputs(task->measurements,
                                                   input_present,
                                                   jitter1_active,
                                                   jitter2_active);
            system_controller_update(task->system);
            measurement = measurement_manager_get_snapshot(task->measurements);
            state = system_controller_get_state(task->system);
            faults = fault_manager_get(task->faults);

            if (input_present != task->last_input_present)
            {
                diagnostics_report(
                    task->diagnostics,
                    input_present ? DIAG_INPUT_PRESENT : DIAG_INPUT_LOST,
                    input_present ? DIAG_SEVERITY_INFO : DIAG_SEVERITY_WARNING,
                    measurement.input24v_mv,
                    faults,
                    state,
                    (uint32_t)xTaskGetTickCount());
                task->last_input_present = input_present;
            }

            if ((!jitter1_active) && task->last_jitter1_active)
            {
                diagnostics_report(task->diagnostics,
                                   DIAG_JITTER1_LOST,
                                   DIAG_SEVERITY_WARNING,
                                   APP_CONFIG.jitter_timeout_ms,
                                   faults | FAULT_JITTER_LOST,
                                   state,
                                   (uint32_t)xTaskGetTickCount());
            }
            if ((!jitter2_active) && task->last_jitter2_active)
            {
                diagnostics_report(task->diagnostics,
                                   DIAG_JITTER2_LOST,
                                   DIAG_SEVERITY_WARNING,
                                   APP_CONFIG.jitter_timeout_ms,
                                   faults | FAULT_JITTER_LOST,
                                   state,
                                   (uint32_t)xTaskGetTickCount());
            }
            task->last_jitter1_active = jitter1_active;
            task->last_jitter2_active = jitter2_active;

            if (state != task->last_state)
            {
                state_code = control_task_code_for_state(state);
                severity = control_task_severity_for_state(state);
                if (state_code != DIAG_CODE_NONE)
                {
                    diagnostics_report(task->diagnostics,
                                       state_code,
                                       severity,
                                       (state == SYSTEM_STATE_BATTERY_SOURCE) ?
                                           measurement.battery24v_mv :
                                           measurement.input24v_mv,
                                       faults,
                                       state,
                                       (uint32_t)xTaskGetTickCount());
                }
                task->last_state = state;
            }

            vTaskDelayUntil(&last_wake,
                            pdMS_TO_TICKS(APP_CONFIG.control_period_ms));
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Creates the statically allocated control task.
 * @function control_task_create
 * @param task Control task context.
 * @param jitter Jitter detector context.
 * @param measurements Measurement manager context.
 * @param actuators Actuator manager context.
 * @param system System controller context.
 * @param faults Fault manager context.
 * @param diagnostics Diagnostic event context.
 * @return true when the static task was created.
 * @safety The scheduler must not start if this function returns false.
 * @misra  No task heap allocation is used.
 */
bool control_task_create(control_task_t * task,
                         jitter_detector_t * jitter,
                         measurement_manager_t * measurements,
                         actuator_manager_t * actuators,
                         system_controller_t * system,
                         fault_manager_t * faults,
                         diagnostics_t * diagnostics)
{
    bool created;

    created = false;
    if ((task != NULL) && (jitter != NULL) &&
        (measurements != NULL) && (actuators != NULL) &&
        (system != NULL) && (faults != NULL) && (diagnostics != NULL))
    {
        task->jitter = jitter;
        task->measurements = measurements;
        task->actuators = actuators;
        task->system = system;
        task->faults = faults;
        task->diagnostics = diagnostics;
        task->handle = NULL;
        task->handle = xTaskCreateStatic(control_task_entry,
                                         "control",
                                         CONTROL_TASK_STACK_WORDS,
                                         task,
                                         CONTROL_TASK_PRIORITY,
                                         task->stack,
                                         &task->control_block);
        created = task->handle != NULL;
    }
    return created;
}
