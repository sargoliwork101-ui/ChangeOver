/**
 * @file App/Src/esp8266_service.c
 * @brief ESP8266 transport service for telemetry and diagnostic lines.
 * @details The transport is intentionally bounded and blocking only inside
 *          the CommunicationTask. Control and protection tasks never call it.
 * @safety UART failures are reported through the diagnostic service; no power
 *         output is changed from this module.
 * @misra  The wire protocol uses bounded UART API calls and fixed storage.
 */

#include <stddef.h>
#include "esp8266_service.h"
#include "task.h"

/**
 * @brief Appends one character to a bounded transport buffer.
 * @function esp8266_service_append_char
 * @param buffer Destination buffer.
 * @param capacity Buffer capacity including null terminator.
 * @param position Current write position.
 * @param value Character to append.
 * @return true when the character fits.
 * @safety No write occurs when the buffer contract is invalid.
 * @misra  The append operation is explicitly bounds-checked.
 */
static bool esp8266_service_append_char(char * buffer,
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
 * @brief Appends an unsigned 32-bit value in decimal form.
 * @function esp8266_service_append_u32
 * @param buffer Destination buffer.
 * @param capacity Buffer capacity including null terminator.
 * @param position Current write position.
 * @param value Value to append.
 * @return true when all digits fit.
 * @safety Uses a fixed ten-character local digit buffer.
 * @misra  No printf-family function or dynamic allocation is used.
 */
static bool esp8266_service_append_u32(char * buffer,
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
        result = esp8266_service_append_char(buffer,
                                             capacity,
                                             position,
                                             digits[digit_count]);
    }
    return result;
}

/**
 * @brief Appends the CSV field separator.
 * @function esp8266_service_append_separator
 * @param buffer Destination buffer.
 * @param capacity Buffer capacity including null terminator.
 * @param position Current write position.
 * @return true when the separator fits.
 * @safety No write occurs when the buffer contract is invalid.
 * @misra  Delegates to the bounded character append function.
 */
static bool esp8266_service_append_separator(char * buffer,
                                             uint16_t capacity,
                                             uint16_t * position)
{
    return esp8266_service_append_char(buffer, capacity, position, ',');
}

/**
 * @brief Formats the current measurements as a bounded telemetry line.
 * @function esp8266_service_format_telemetry
 * @param measurement Measurement snapshot to encode.
 * @param state Current system state.
 * @param faults Current fault mask.
 * @param buffer Destination buffer.
 * @param capacity Destination capacity including null terminator.
 * @param length_out Number of encoded characters excluding null terminator.
 * @return true when the complete line fits in buffer.
 * @safety Pure bounded formatter; it does not access hardware or block.
 * @misra  Fixed-point integer encoding avoids printf-family functions.
 */
static bool esp8266_service_format_telemetry(
    const measurement_snapshot_t * measurement,
    system_state_t state,
    fault_mask_t faults,
    char * buffer,
    uint16_t capacity,
    uint16_t * length_out)
{
    uint16_t position;
    bool result;

    position = UINT16_C(0);
    result = false;
    if ((measurement != NULL) && (buffer != NULL) &&
        (length_out != NULL) && (capacity > UINT16_C(1)))
    {
        *length_out = UINT16_C(0);
        buffer[0] = '\0';
        result = esp8266_service_append_char(buffer, capacity, &position, 'T');
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      measurement->input24v_mv);
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      measurement->battery24v_mv);
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      measurement->battery12v_mv);
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      measurement->current1_ma);
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      measurement->current2_ma);
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      (uint32_t)state);
        result = result && esp8266_service_append_separator(buffer,
                                                             capacity,
                                                             &position);
        result = result && esp8266_service_append_u32(buffer,
                                                      capacity,
                                                      &position,
                                                      faults);
        result = result && esp8266_service_append_char(buffer,
                                                       capacity,
                                                       &position,
                                                       '\r');
        result = result && esp8266_service_append_char(buffer,
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

/**
 * @brief Sends telemetry and the latest diagnostic line to ESP8266.
 * @function esp8266_service_send_telemetry
 * @param service ESP8266 transport context.
 * @param measurement Latest measurement snapshot.
 * @param state Current system state.
 * @param faults Current fault mask.
 * @return true when all enabled transport writes succeed.
 * @safety This function is intended for CommunicationTask context and may
 *         block for the configured UART timeout.
 * @misra  All frames are bounded and encoded without printf-family APIs.
 */
bool esp8266_service_send_telemetry(const esp8266_service_t * service,
                                    const measurement_snapshot_t * measurement,
                                    system_state_t state,
                                    fault_mask_t faults)
{
    char telemetry_line[128U];
    char diagnostic_line[96U];
    uint16_t telemetry_length;
    uint16_t diagnostic_length;
    bool sent;

    sent = true;
    telemetry_length = UINT16_C(0);
    diagnostic_length = UINT16_C(0);
    if ((service != NULL) && service->enabled &&
        (service->uart != NULL) && (measurement != NULL))
    {
        sent = esp8266_service_format_telemetry(measurement,
                                                 state,
                                                 faults,
                                                 telemetry_line,
                                                 (uint16_t)sizeof(telemetry_line),
                                                 &telemetry_length);
        if (sent)
        {
            sent = uart_driver_write(service->uart,
                                     (const uint8_t *)telemetry_line,
                                     telemetry_length,
                                     UINT32_C(100));
        }
        if (sent && (service->diagnostics != NULL))
        {
            sent = diagnostics_format_last_line(service->diagnostics,
                                                diagnostic_line,
                                                (uint16_t)sizeof(diagnostic_line),
                                                &diagnostic_length);
            if (sent)
            {
                sent = uart_driver_write(service->uart,
                                         (const uint8_t *)diagnostic_line,
                                         diagnostic_length,
                                         UINT32_C(100));
            }
        }

        if ((!sent) && (service->diagnostics != NULL))
        {
            diagnostics_report(service->diagnostics,
                               DIAG_ESP_TX_FAILED,
                               DIAG_SEVERITY_ERROR,
                               UINT32_C(0),
                               faults,
                               state,
                               (uint32_t)xTaskGetTickCount());
        }
    }
    return sent;
}

/**
 * @brief Initializes the ESP8266 transport context.
 * @function esp8266_service_init
 * @param service ESP8266 transport context.
 * @param uart UART driver used for the transport.
 * @param diagnostics Diagnostic context used for D-lines.
 * @safety Must be called after the referenced contexts are initialized.
 * @misra  The service starts disabled so boot cannot transmit unexpectedly.
 */
void esp8266_service_init(esp8266_service_t * service,
                          uart_driver_t * uart,
                          diagnostics_t * diagnostics)
{
    if (service != NULL)
    {
        service->uart = uart;
        service->diagnostics = diagnostics;
        service->enabled = false;
    }
}

/**
 * @brief Enables or disables the ESP8266 transport.
 * @function esp8266_service_enable
 * @param service ESP8266 transport context.
 * @param enabled Requested transport state.
 * @safety This function changes only the transport-enable flag.
 * @misra  No hardware output is changed directly.
 */
void esp8266_service_enable(esp8266_service_t * service, bool enabled)
{
    if (service != NULL)
    {
        service->enabled = enabled;
    }
}
