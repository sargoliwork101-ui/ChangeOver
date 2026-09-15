/**
 * @file    measurement.c
 * @brief   [EN] ADC to engineering units (placeholder). Full type naming, func_ prefix.
 *          [FA] تبدیل ADC به واحد مهندسی (اسکلت). نام تایپ کامل.
 */

#include "measurement.h"
#include "bsp_adc.h"

#include <stddef.h>

static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T_G_Snap;

/**
 * @brief  [EN] Zero the last snapshot.
 *         [FA] آخرین نمونه را صفر می‌کند.
 */
void func_Measurement_Init(void)
{
    MEASUREMENT_SNAPSHOT_T_G_Snap.v_in_mv = 0u;
    MEASUREMENT_SNAPSHOT_T_G_Snap.v_bat24_mv = 0u;
    MEASUREMENT_SNAPSHOT_T_G_Snap.v_bat12_mv = 0u;
    MEASUREMENT_SNAPSHOT_T_G_Snap.i_ch1_ma = 0u;
    MEASUREMENT_SNAPSHOT_T_G_Snap.i_ch2_ma = 0u;
    MEASUREMENT_SNAPSHOT_T_G_Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T_G_Snap.valid = false;
}

/**
 * @brief  [EN] Pull one ADC frame and convert. No-op until ADC is enabled.
 *         [FA] یک فریم ADC می‌گیرد و تبدیل می‌کند. تا ADC روشن نشود کاری نمی‌کند.
 */
void func_Measurement_Run(void)
{
    uint16_t uint16_t_raw[BSP_ADC_CHANNEL_COUNT];

    if (!func_BspAdc_GetRaw(uint16_t_raw))
    {
        MEASUREMENT_SNAPSHOT_T_G_Snap.valid = false;
        return;
    }

    (void)uint16_t_raw;
}

/**
 * @brief  [EN] Copy last snapshot. Returns false if pointer is NULL or data is invalid.
 *         [FA] آخرین نمونه را کپی می‌کند. اگر اشاره‌گر NULL یا داده نامعتبر باشد false.
 * @param  measurement_snapshot_t_out [EN] Output pointer for snapshot, must not be NULL / اشاره‌گر خروجی
 * @return bool [EN] true if valid snapshot copied / اگر نمونه معتبر کپی شد true
 */
bool func_Measurement_GetSnapshot(measurement_snapshot_t *measurement_snapshot_t_out)
{
    if (measurement_snapshot_t_out == NULL)
    {
        return false;
    }
    *measurement_snapshot_t_out = MEASUREMENT_SNAPSHOT_T_G_Snap;
    return MEASUREMENT_SNAPSHOT_T_G_Snap.valid;
}
