/**
 * @file    measurement.c
 * @brief   [EN] ADC to engineering units (placeholder).
 *          [FA] تبدیل ADC به واحد مهندسی (اسکلت).
 */

#include "measurement.h"
#include "bsp_adc.h"

#include <stddef.h>

static measurement_snapshot_t s_snap;

/**
 * @brief  [EN] Zero the last snapshot.
 *         [FA] آخرین نمونه را صفر می‌کند.
 */
void Measurement_Init(void)
{
    s_snap.v_in_mv = 0u;
    s_snap.v_bat24_mv = 0u;
    s_snap.v_bat12_mv = 0u;
    s_snap.i_ch1_ma = 0u;
    s_snap.i_ch2_ma = 0u;
    s_snap.input_present = false;
    s_snap.valid = false;
}

/**
 * @brief  [EN] Pull one ADC frame and convert. No-op until ADC is enabled.
 *         [FA] یک فریم ADC می‌گیرد و تبدیل می‌کند. تا ADC روشن نشود کاری نمی‌کند.
 */
void Measurement_Run(void)
{
    uint16_t raw[BSP_ADC_CHANNEL_COUNT];

    if (!BspAdc_GetRaw(raw))
    {
        s_snap.valid = false;
        return;
    }

    (void)raw;
}

/**
 * @brief  [EN] Copy last snapshot. Returns false if pointer is NULL or data is invalid.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر اشاره‌گر NULL یا داده نامعتبر باشد false.
 */
bool Measurement_GetSnapshot(measurement_snapshot_t *out)
{
    if (out == NULL)
    {
        return false;
    }
    *out = s_snap;
    return s_snap.valid;
}
