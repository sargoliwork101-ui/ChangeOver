/**
 * @file App/Src/measurement_manager.c
 * @brief C source for measurement_manager.
 * @details This file belongs to the application service and control logic layer.
 * The implementation is application-owned and is intended for
 * MISRA C:2012 analysis. Vendor HAL, CMSIS and FreeRTOS APIs are
 * third-party boundaries and their deviations are recorded separately.
 * @safety No function in this file may bypass the project safety policy.
 * @note Comments document design intent; comments alone do not establish
 *       MISRA compliance. Static analysis and review are required.
 */

#include <stddef.h>
#include "measurement_manager.h"
#include "app_config.h"

/**
 * @brief Limits an ADC code to the configured converter range.
 * @function measurement_manager_clamp_adc
 * @param code Raw ADC code supplied by the DMA buffer.
 * @return A code not greater than the configured ADC maximum.
 * @safety The configured ADC maximum must be non-zero for later conversion.
 * @misra  The conversion boundary is explicit and uses fixed-width types.
 */
static uint32_t measurement_manager_clamp_adc(uint32_t code)
{
    uint32_t result;

    result = code;
    if (result > APP_CONFIG.adc_max_code)
    {
        result = APP_CONFIG.adc_max_code;
    }
    return result;
}

/**
 * @brief Converts an ADC code to the MCU pin voltage in millivolts.
 * @function measurement_manager_adc_to_pin_mv
 * @param code Raw ADC code.
 * @return MCU pin voltage in millivolts.
 * @safety A zero ADC maximum is treated as an invalid calibration and returns
 *         zero instead of causing a division by zero.
 * @misra  Integer arithmetic avoids floating-point nondeterminism in the
 *         measurement path.
 */
static uint32_t measurement_manager_adc_to_pin_mv(uint32_t code)
{
    uint64_t numerator;
    uint32_t result;

    numerator = (uint64_t)measurement_manager_clamp_adc(code) *
                (uint64_t)APP_CONFIG.adc_reference_mv;
    result = UINT32_C(0);
    if (APP_CONFIG.adc_max_code > UINT32_C(0))
    {
        result = (uint32_t)(numerator / (uint64_t)APP_CONFIG.adc_max_code);
    }
    return result;
}

/**
 * @brief Reconstructs the source voltage through a resistor divider.
 * @function measurement_manager_apply_divider
 * @param code ADC code measured at the MCU pin.
 * @param divider Divider resistance configuration.
 * @return Estimated source voltage in millivolts.
 * @safety A zero bottom resistance is treated as invalid calibration.
 * @misra  Resistance addition is widened before multiplication to avoid an
 *         unintended 32-bit intermediate overflow.
 */
static uint32_t measurement_manager_apply_divider(uint32_t code,
                                                  voltage_divider_t divider)
{
    uint64_t numerator;
    uint64_t denominator;
    uint32_t result;

    denominator = (uint64_t)divider.bottom_resistance_ohm;
    numerator = (uint64_t)measurement_manager_adc_to_pin_mv(code) *
                ((uint64_t)divider.top_resistance_ohm +
                 (uint64_t)divider.bottom_resistance_ohm);
    result = UINT32_C(0);
    if (denominator > UINT64_C(0))
    {
        result = (uint32_t)(numerator / denominator);
    }
    return result;
}

/**
 * @brief Converts the current-sense ADC voltage to milliamps.
 * @function measurement_manager_calculate_current
 * @param code Raw ADC code from a current channel.
 * @param offset_mv Zero-current calibration offset in millivolts.
 * @param mv_per_amp Sensor calibration in millivolts per ampere.
 * @return Estimated current in milliamps, clamped to zero below offset.
 * @safety Invalid divider or calibration denominators return zero.
 * @misra  Fixed-point arithmetic is used so the control path has predictable
 *         execution and does not depend on a floating-point library.
 */
static uint32_t measurement_manager_calculate_current(
    uint32_t code,
    uint32_t offset_mv,
    uint32_t mv_per_amp)
{
    uint64_t sensor_mv;
    uint64_t numerator;
    uint32_t pin_mv;
    uint32_t result;

    pin_mv = measurement_manager_adc_to_pin_mv(code);
    sensor_mv = UINT64_C(0);
    result = UINT32_C(0);
    if ((APP_CONFIG.current_adc_divider_den > UINT32_C(0)) &&
        (mv_per_amp > UINT32_C(0)))
    {
        sensor_mv = ((uint64_t)pin_mv *
                     (uint64_t)APP_CONFIG.current_adc_divider_num) /
                    (uint64_t)APP_CONFIG.current_adc_divider_den;
        if (sensor_mv > (uint64_t)offset_mv)
        {
            numerator = (sensor_mv - (uint64_t)offset_mv) * UINT64_C(1000);
            result = (uint32_t)(numerator / (uint64_t)mv_per_amp);
        }
    }
    return result;
}

/**
 * @brief Creates a deterministic zeroed measurement snapshot.
 * @function measurement_manager_empty_snapshot
 * @return A snapshot with zero measurements and inactive digital inputs.
 * @safety No external resource is accessed.
 * @misra  All members are explicitly initialized to avoid partial-state use.
 */
static measurement_snapshot_t measurement_manager_empty_snapshot(void)
{
    measurement_snapshot_t snapshot;

    snapshot.sequence = UINT32_C(0);
    snapshot.raw_current1 = UINT32_C(0);
    snapshot.raw_input24v = UINT32_C(0);
    snapshot.raw_battery24v = UINT32_C(0);
    snapshot.raw_battery12v = UINT32_C(0);
    snapshot.raw_current2 = UINT32_C(0);
    snapshot.current1_ma = UINT32_C(0);
    snapshot.input24v_mv = UINT32_C(0);
    snapshot.battery24v_mv = UINT32_C(0);
    snapshot.battery12v_mv = UINT32_C(0);
    snapshot.current2_ma = UINT32_C(0);
    snapshot.input_present = false;
    snapshot.jitter1_active = false;
    snapshot.jitter2_active = false;
    return snapshot;
}

/**
 * @brief Initializes the measurement context and its static mutex.
 * @function measurement_manager_init
 * @param manager Measurement context to initialize.
 * @safety Must be called before any other manager API.
 * @misra  The FreeRTOS static object is created without heap allocation.
 */
void measurement_manager_init(measurement_manager_t * manager)
{
    if (manager != NULL)
    {
        manager->snapshot = measurement_manager_empty_snapshot();
        manager->mutex = xSemaphoreCreateMutexStatic(&manager->mutex_storage);
    }
}

/**
 * @brief Converts one complete ADC DMA frame and publishes it atomically.
 * @function measurement_manager_update_from_dma
 * @param manager Measurement context.
 * @param raw DMA buffer in the agreed five-channel order.
 * @param count Number of valid entries in raw.
 * @safety The function blocks only on the application mutex and is not ISR-safe.
 * @misra  The caller must provide at least the configured channel count.
 */
void measurement_manager_update_from_dma(measurement_manager_t * manager,
                                         const uint32_t * raw,
                                         uint32_t count)
{
    measurement_snapshot_t next;

    if ((manager != NULL) && (raw != NULL) &&
        (count >= APP_CONFIG.adc_channel_count) &&
        (manager->mutex != NULL) &&
        (xSemaphoreTake(manager->mutex, portMAX_DELAY) == pdTRUE))
    {
        next = manager->snapshot;
        next.raw_current1 = measurement_manager_clamp_adc(raw[0]);
        next.raw_input24v = measurement_manager_clamp_adc(raw[1]);
        next.raw_battery24v = measurement_manager_clamp_adc(raw[2]);
        next.raw_battery12v = measurement_manager_clamp_adc(raw[3]);
        next.raw_current2 = measurement_manager_clamp_adc(raw[4]);

        next.current1_ma = measurement_manager_calculate_current(
            next.raw_current1,
            APP_CONFIG.current_1_offset_mv,
            APP_CONFIG.current_1_mv_per_amp);
        next.input24v_mv = measurement_manager_apply_divider(
            next.raw_input24v, APP_CONFIG.input_24v_divider);
        next.battery24v_mv = measurement_manager_apply_divider(
            next.raw_battery24v, APP_CONFIG.battery_24v_divider);
        next.battery12v_mv = measurement_manager_apply_divider(
            next.raw_battery12v, APP_CONFIG.battery_12v_divider);
        next.current2_ma = measurement_manager_calculate_current(
            next.raw_current2,
            APP_CONFIG.current_2_offset_mv,
            APP_CONFIG.current_2_mv_per_amp);

        next.sequence = manager->snapshot.sequence + UINT32_C(1);
        next.input_present = manager->snapshot.input_present;
        next.jitter1_active = manager->snapshot.jitter1_active;
        next.jitter2_active = manager->snapshot.jitter2_active;
        manager->snapshot = next;
        (void)xSemaphoreGive(manager->mutex);
    }
}

/**
 * @brief Publishes the debounced digital input state atomically.
 * @function measurement_manager_set_digital_inputs
 * @param manager Measurement context.
 * @param input_present Current input-presence state.
 * @param jitter1_active Current Jitter1 state.
 * @param jitter2_active Current Jitter2 state.
 * @safety This function is task-context only because it may block on a mutex.
 * @misra  Digital states are written as one coherent group.
 */
void measurement_manager_set_digital_inputs(measurement_manager_t * manager,
                                            bool input_present,
                                            bool jitter1_active,
                                            bool jitter2_active)
{
    if ((manager != NULL) && (manager->mutex != NULL) &&
        (xSemaphoreTake(manager->mutex, portMAX_DELAY) == pdTRUE))
    {
        manager->snapshot.input_present = input_present;
        manager->snapshot.jitter1_active = jitter1_active;
        manager->snapshot.jitter2_active = jitter2_active;
        (void)xSemaphoreGive(manager->mutex);
    }
}

/**
 * @brief Publishes only the input-presence state.
 * @function measurement_manager_set_input_present
 * @param manager Measurement context.
 * @param input_present Current input-presence state.
 * @safety Task-context API; it may block on the snapshot mutex.
 * @misra  No ADC data is modified by this function.
 */
void measurement_manager_set_input_present(measurement_manager_t * manager,
                                           bool input_present)
{
    if ((manager != NULL) && (manager->mutex != NULL) &&
        (xSemaphoreTake(manager->mutex, portMAX_DELAY) == pdTRUE))
    {
        manager->snapshot.input_present = input_present;
        (void)xSemaphoreGive(manager->mutex);
    }
}

/**
 * @brief Returns a coherent copy of the latest measurement snapshot.
 * @function measurement_manager_get_snapshot
 * @param manager Measurement context.
 * @return Snapshot copied while holding the static mutex.
 * @safety Task-context API; it may block until the publisher releases the mutex.
 * @misra  The returned object is a value copy and does not expose internal state.
 */
measurement_snapshot_t measurement_manager_get_snapshot(
    const measurement_manager_t * manager)
{
    measurement_snapshot_t snapshot;

    snapshot = measurement_manager_empty_snapshot();
    if ((manager != NULL) && (manager->mutex != NULL) &&
        (xSemaphoreTake(manager->mutex, portMAX_DELAY) == pdTRUE))
    {
        snapshot = manager->snapshot;
        (void)xSemaphoreGive(manager->mutex);
    }
    return snapshot;
}
