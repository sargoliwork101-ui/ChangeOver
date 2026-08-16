/**
 * @file App/Inc/diagnostics.h
 * @brief C interface for the deterministic diagnostic event service.
 * @details The service stores the most recent diagnostic event and exposes a
 *          bounded CSV line for transport to the ESP8266 display layer.
 * @safety No heap allocation is used. The service is protected by a static
 *         FreeRTOS mutex and must not be called from ISR context.
 * @misra  The wire format is fixed-width and integer-only to keep the debug
 *         path deterministic and independent of printf-family functions.
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>
#include "app_types.h"
#include "FreeRTOS.h"
#include "semphr.h"

typedef enum
{
    DIAG_SEVERITY_INFO = 1,
    DIAG_SEVERITY_WARNING,
    DIAG_SEVERITY_ERROR,
    DIAG_SEVERITY_FATAL
} diagnostic_severity_t;

typedef enum
{
    DIAG_CODE_NONE = 0x0000,
    DIAG_BOOT_STARTED = 0x1000,
    DIAG_INIT_FAILED = 0x1001,
    DIAG_ADC_DMA_START_FAILED = 0x2001,
    DIAG_ADC_OUT_OF_RANGE = 0x2002,
    DIAG_INPUT_PRESENT = 0x3001,
    DIAG_INPUT_LOST = 0x3002,
    DIAG_SOURCE_INPUT = 0x3101,
    DIAG_SOURCE_BATTERY = 0x3102,
    DIAG_LOW_BATTERY = 0x4001,
    DIAG_OVERCURRENT_CH1 = 0x4002,
    DIAG_OVERCURRENT_CH2 = 0x4003,
    DIAG_PROTECTION_ACTIVE = 0x4004,
    DIAG_JITTER1_LOST = 0x5001,
    DIAG_JITTER2_LOST = 0x5002,
    DIAG_ESP_TX_FAILED = 0x6001,
    DIAG_UART_TX_FAILED = 0x6002,
    DIAG_STACK_OVERFLOW = 0x7001,
    DIAG_SYSTEM_FAULT = 0x7002
} diagnostic_code_t;

typedef struct
{
    bool valid;
    diagnostic_code_t code;
    diagnostic_severity_t severity;
    uint32_t value;
    fault_mask_t fault_mask;
    system_state_t state;
    uint32_t timestamp_ms;
    uint32_t occurrence_count;
} diagnostic_event_t;

typedef struct
{
    StaticSemaphore_t mutex_storage;
    SemaphoreHandle_t mutex;
    diagnostic_event_t last_event;
} diagnostics_t;

/**
 * @brief Initializes the diagnostic context and its static mutex.
 * @function diagnostics_init
 * @param diagnostics Diagnostic context to initialize.
 * @safety Must be called before any diagnostic API and from task/startup context.
 * @misra  No dynamic memory is allocated.
 */
void diagnostics_init(diagnostics_t * diagnostics);

/**
 * @brief Publishes one diagnostic event and updates its occurrence count.
 * @function diagnostics_report
 * @param diagnostics Diagnostic context.
 * @param code Stable code from the diagnostic table.
 * @param severity Event severity.
 * @param value Numeric context such as millivolts, milliamps or a pin value.
 * @param fault_mask Fault mask active when the event was reported.
 * @param state System state active when the event was reported.
 * @param timestamp_ms Monotonic timestamp in milliseconds.
 * @safety Task-context API; it may block on a static mutex and is not ISR-safe.
 * @misra  Codes are never reused for a different meaning.
 */
void diagnostics_report(diagnostics_t * diagnostics,
                        diagnostic_code_t code,
                        diagnostic_severity_t severity,
                        uint32_t value,
                        fault_mask_t fault_mask,
                        system_state_t state,
                        uint32_t timestamp_ms);

/**
 * @brief Records a terminal diagnostic without taking a mutex.
 * @function diagnostics_report_emergency
 * @param diagnostics Diagnostic context.
 * @param code Stable diagnostic code.
 * @param severity Event severity.
 * @param value Numeric context.
 * @param fault_mask Active fault mask.
 * @param state Current state.
 * @param timestamp_ms Timestamp available at the fault boundary.
 * @safety Intended only for terminal kernel hooks. It must not block or call
 *         any scheduler service.
 * @misra  This controlled data race is isolated to a terminal fault path and
 *         is recorded as a project deviation.
 */
void diagnostics_report_emergency(diagnostics_t * diagnostics,
                                  diagnostic_code_t code,
                                  diagnostic_severity_t severity,
                                  uint32_t value,
                                  fault_mask_t fault_mask,
                                  system_state_t state,
                                  uint32_t timestamp_ms);

/**
 * @brief Returns a coherent copy of the most recent diagnostic event.
 * @function diagnostics_get_last
 * @param diagnostics Diagnostic context.
 * @return Last event, or a zero/invalid event when the context is unavailable.
 * @safety Task-context API; it may block on a static mutex.
 * @misra  The caller receives a value copy and cannot alter internal state.
 */
diagnostic_event_t diagnostics_get_last(const diagnostics_t * diagnostics);

/**
 * @brief Formats the last event as a bounded ESP8266 diagnostic line.
 * @function diagnostics_format_last_line
 * @param diagnostics Diagnostic context.
 * @param buffer Destination character buffer.
 * @param capacity Capacity of buffer including the terminating null.
 * @param length_out Number of characters excluding the terminating null.
 * @return true when the line fits; false for invalid or insufficient storage.
 * @safety Task-context API; it does not use printf or dynamic allocation.
 * @misra  The formatter checks every append operation before writing.
 */
bool diagnostics_format_last_line(const diagnostics_t * diagnostics,
                                  char * buffer,
                                  uint16_t capacity,
                                  uint16_t * length_out);

#endif /* DIAGNOSTICS_H */
