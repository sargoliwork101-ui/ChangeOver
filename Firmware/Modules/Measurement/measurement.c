/**
 * @file    measurement.c
 * @brief   [EN] ADC to engineering units (placeholder).
 *          [FA] تبدیل ADC به واحد مهندسی (اسکلت).
 *
 * @stage   Placeholder
 */

#include "measurement.h"
#include "bsp_adc.h"

static measurement_snapshot_t s_snap;

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

bool Measurement_GetSnapshot(measurement_snapshot_t *out)
{
    if (out == 0)
    {
        return false;
    }
    *out = s_snap;
    return s_snap.valid;
}
