/**
 * @file App/Src/diagnostics.c
 * @brief Deterministic diagnostic event storage and formatter.
 * @details Events are stored as a single latest-event snapshot. Repeated
 *          reports of the same code increment occurrence_count, which keeps
 *          the transport compact while preserving fault visibility.
 * @safety This module is task-context only; no API is ISR-safe.
 * @misra  The formatter uses bounded fixed-point integer conversion instead of
 *         printf-family functions and heap allocation.
 */

#include <stddef.h>
#include "diagnostics.h"

/**
 * @brief Creates an invalid zeroed diagnostic event.
 * @function diagnostics_empty_event
 * @return A deterministic event used for invalid or uninitialized contexts.
 * @safety Pure function with no external side effects.
 * @misra  Every event member is explicitly initialized.
 */
static diagnostic_event_t diagnostics_empty_event(void)
{
    diagnostic_event_t event;

    event.valid = false;
    event.code = DIAG_CODE_NONE;
    event.severity = DIAG_SEVERITY_INFO;
    event.value = UINT32_C(0);
    event.fault_mask = FAULT_NONE;
    event.state = SYSTEM_STATE_BOOT;
    event.timestamp_ms = UINT32_C(0);
    event.occurrence_count = UINT32_C(0);
    return event;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function diagnostics_append_char
 * @param buffer Input or state associated with the operation.
 * @param capacity Input or state associated with the operation.
 * @param position Input or state associated with the operation.
 * @param value Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool diagnostics_append_char(char * buffer,
                                    uint16_t capacity,
                                    uint16_t * position,
                                    char value)
{
    bool appended;

    appended = false;
    if ((buffer != NULL) && (position != NULL) &&
        (capacity > UINT16_C(1)) && (*position < (capacity - UINT16_C(1))))
    {
        buffer[*position] = value;
        *position = (uint16_t)(*position + UINT16_C(1));
        buffer[*position] = '\0';
        appended = true;
    }
    return appended;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function diagnostics_append_u32
 * @param buffer Input or state associated with the operation.
 * @param capacity Input or state associated with the operation.
 * @param position Input or state associated with the operation.
 * @param value Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool diagnostics_append_u32(char * buffer,
                                   uint16_t capacity,
                                   uint16_t * position,
                                   uint32_t value)
{
    char digits[10U];
    uint8_t digit_count;
    uint32_t remainder;
    bool result;

    digit_count = UINT8_C(0);
    do
    {
        remainder = value % UINT32_C(10);
        digits[digit_count] = (char)('0' + (char)remainder);
        digit_count++;
        value /= UINT32_C(10);
    } while ((value > UINT32_C(0)) && (digit_count < UINT8_C(10)));

    result = true;
    while ((digit_count > UINT8_C(0)) && result)
    {
        digit_count--;
        result = diagnostics_append_char(buffer,
                                         capacity,
                                         position,
                                         digits[digit_count]);
    }
    return result;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function diagnostics_append_separator
 * @param buffer Input or state associated with the operation.
 * @param capacity Input or state associated with the operation.
 * @param position Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
static bool diagnostics_append_separator(char * buffer,
                                         uint16_t capacity,
                                         uint16_t * position)
{
    return diagnostics_append_char(buffer, capacity, position, ',');
}

/**
 * @brief Initializes the module state and its dependencies.
 * @function diagnostics_init
 * @param diagnostics Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void diagnostics_init(diagnostics_t * diagnostics)
{
    if (diagnostics != NULL)
    {
        diagnostics->last_event = diagnostics_empty_event();
        diagnostics->mutex = xSemaphoreCreateMutexStatic(
            &diagnostics->mutex_storage);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function diagnostics_report
 * @param diagnostics Input or state associated with the operation.
 * @param code Input or state associated with the operation.
 * @param severity Input or state associated with the operation.
 * @param value Input or state associated with the operation.
 * @param fault_mask Input or state associated with the operation.
 * @param state Input or state associated with the operation.
 * @param timestamp_ms Input or state associated with the operation.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
void diagnostics_report(diagnostics_t * diagnostics,
                        diagnostic_code_t code,
                        diagnostic_severity_t severity,
                        uint32_t value,
                        fault_mask_t fault_mask,
                        system_state_t state,
                        uint32_t timestamp_ms)
{
    diagnostic_event_t event;

    if ((diagnostics != NULL) && (diagnostics->mutex != NULL) &&
        (xSemaphoreTake(diagnostics->mutex, portMAX_DELAY) == pdTRUE))
    {
        event = diagnostics->last_event;
        if ((event.valid) && (event.code == code))
        {
            if (event.occurrence_count < UINT32_MAX)
            {
                event.occurrence_count++;
            }
        }
        else
        {
            event.occurrence_count = UINT32_C(1);
        }
        event.valid = true;
        event.code = code;
        event.severity = severity;
        event.value = value;
        event.fault_mask = fault_mask;
        event.state = state;
        event.timestamp_ms = timestamp_ms;
        diagnostics->last_event = event;
        (void)xSemaphoreGive(diagnostics->mutex);
    }
}

/**
 * @brief Records a terminal event without using a blocking RTOS primitive.
 * @function diagnostics_report_emergency
 * @param diagnostics Diagnostic context.
 * @param code Stable diagnostic code.
 * @param severity Event severity.
 * @param value Numeric event context.
 * @param fault_mask Active fault mask.
 * @param state State at the fault boundary.
 * @param timestamp_ms Timestamp available at the fault boundary.
 * @safety This API is restricted to terminal kernel hooks. The caller must
 *         stop normal scheduling after the call.
 * @misra  Direct state publication is a documented terminal-path deviation.
 */
void diagnostics_report_emergency(diagnostics_t * diagnostics,
                                  diagnostic_code_t code,
                                  diagnostic_severity_t severity,
                                  uint32_t value,
                                  fault_mask_t fault_mask,
                                  system_state_t state,
                                  uint32_t timestamp_ms)
{
    diagnostic_event_t event;

    if (diagnostics != NULL)
    {
        event = diagnostics->last_event;
        event.valid = true;
        event.code = code;
        event.severity = severity;
        event.value = value;
        event.fault_mask = fault_mask;
        event.state = state;
        event.timestamp_ms = timestamp_ms;
        if (event.occurrence_count < UINT32_MAX)
        {
            event.occurrence_count++;
        }
        diagnostics->last_event = event;
    }
}

/**
 * @brief Returns a coherent copy of the last diagnostic event.
 * @function diagnostics_get_last
 * @param diagnostics Diagnostic context.
 * @return Last event or an invalid zeroed event.
 * @safety Task-context API; it may block on the static mutex.
 * @misra  The returned value does not expose mutable internal storage.
 */
diagnostic_event_t diagnostics_get_last(const diagnostics_t * diagnostics)
{
    diagnostic_event_t event;

    event = diagnostics_empty_event();
    if ((diagnostics != NULL) && (diagnostics->mutex != NULL) &&
        (xSemaphoreTake(diagnostics->mutex, portMAX_DELAY) == pdTRUE))
    {
        event = diagnostics->last_event;
        (void)xSemaphoreGive(diagnostics->mutex);
    }
    return event;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function diagnostics_format_last_line
 * @param diagnostics Input or state associated with the operation.
 * @param buffer Input or state associated with the operation.
 * @param capacity Input or state associated with the operation.
 * @param length_out Input or state associated with the operation.
 * @return Result or state value defined by the API contract.
 * @safety Caller shall provide initialized objects and valid handles;
 *         the implementation checks nullable boundaries where applicable.
 * @misra  The return value of external HAL/RTOS calls shall be checked or
 *         explicitly documented when the API has no meaningful result.
 */
bool diagnostics_format_last_line(const diagnostics_t * diagnostics,
                                  char * buffer,
                                  uint16_t capacity,
                                  uint16_t * length_out)
{
    diagnostic_event_t event;
    uint16_t position;
    bool result;

    position = UINT16_C(0);
    result = false;
    if (length_out != NULL)
    {
        *length_out = UINT16_C(0);
    }
    if ((buffer != NULL) && (length_out != NULL) &&
        (capacity > UINT16_C(1)) && (diagnostics != NULL))
    {
        buffer[0] = '\0';
        event = diagnostics_get_last(diagnostics);
        result = diagnostics_append_char(buffer, capacity, &position, 'D');
        result = result && diagnostics_append_separator(buffer,
                                                         capacity,
                                                         &position);
        result = result && diagnostics_append_u32(buffer,
                                                  capacity,
                                                  &position,
                                                  (uint32_t)event.code);
        result = result && diagnostics_append_separator(buffer,
                                                         capacity,
                                                         &position);
        result = result && diagnostics_append_u32(
            buffer,
            capacity,
            &position,
            (uint32_t)event.severity);
        result = result && diagnostics_append_separator(buffer,
                                                         capacity,
                                                         &position);
        result = result && diagnostics_append_u32(buffer,
                                                  capacity,
                                                  &position,
                                                  event.value);
        result = result && diagnostics_append_separator(buffer,
                                                         capacity,
                                                         &position);
        result = result && diagnostics_append_u32(buffer,
                                                  capacity,
                                                  &position,
                                                  event.fault_mask);
        result = result && diagnostics_append_separator(buffer,
                                                         capacity,
                                                         &position);
        result = result && diagnostics_append_u32(buffer,
                                                  capacity,
                                                  &position,
                                                  (uint32_t)event.state);
        result = result && diagnostics_append_separator(buffer,
                                                         capacity,
                                                         &position);
        result = result && diagnostics_append_u32(buffer,
                                                  capacity,
                                                  &position,
                                                  event.occurrence_count);
        result = result && diagnostics_append_char(buffer,
                                                   capacity,
                                                   &position,
                                                   '\r');
        result = result && diagnostics_append_char(buffer,
                                                   capacity,
                                                   &position,
                                                   '\n');
        if (result)
        {
            *length_out = position;
        }
    }
    return result;
}
