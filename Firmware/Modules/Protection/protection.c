/**
 * @file    protection.c
 * @brief   [EN] Over-current, low-battery and ADC validity checks.
 *          Snapshot-first: NULL/invalid sets FAULT_ADC and preserves other faults.
 *          Valid snapshot clears FAULT_ADC and evaluates current and battery limits
 *          from APP_CONFIG. Latched faults stay set until explicitly cleared.
 *          [FA] بررسی اضافه‌جریان، باتری کم و اعتبار ADC.
 *          اول snapshot: نامعتبر FAULT_ADC می‌گذارد و بقیه خطاها حفظ می‌شود.
 *          snapshot معتبر FAULT_ADC را پاک و حدهای جریان و باتری را از APP_CONFIG می‌سنجد.
 */

#include "protection.h"
#include "app_config.h"
#include "fault.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ==================== Protection_Init / مقداردهی حفاظت ==================== */

/**
 * @brief  [EN] Init protection state - no internal state, faults are in Fault module.
 *         [FA] حالت حفاظت را مقداردهی می‌کند - وضعیت داخلی ندارد، خطاها در ماژول Fault هستند.
 */
void func__Protection_Init(void)
{
    /* [EN] Fault module owns the latch; nothing to init here.
       [FA] مالک latch ماژول Fault است؛ اینجا کاری نیست. */
}

/* ==================== Protection_Run / اجرای حفاظت ==================== */

/**
 * @brief  [EN] Compare snapshot against limits; latch faults.
 *         [FA] نمونه را با حد مقایسه می‌کند و خطا را قفل می‌کند.
 * @param  measurement_snapshot_t__snap [EN] Snapshot pointer, may be NULL, valid flag checked /
 *                                          اشاره‌گر نمونه، NULL یا valid بررسی می‌شود
 */
void func__Protection_Run(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    bool bool__snapshotValid;
    uint32_t uint32_t__current1Ma;
    uint32_t uint32_t__current2Ma;
    uint32_t uint32_t__battery24Mv;

    if (measurement_snapshot_t__snap == NULL)
    {
        func__Fault_Set(FAULT_ADC);
        return;
    }

    bool__snapshotValid = measurement_snapshot_t__snap->valid;

    if (bool__snapshotValid == false)
    {
        func__Fault_Set(FAULT_ADC);
        return;
    }

    /* [EN] Valid snapshot - clear ADC fault, keep other latched faults until cleared.
       [FA] نمونه معتبر - خطای ADC پاک می‌شود، بقیه خطاهای قفل‌شده تا پاک‌شدن صریح می‌مانند. */
    func__Fault_Clear(FAULT_ADC);

    uint32_t__current1Ma = measurement_snapshot_t__snap->i_ch1_ma;
    uint32_t__current2Ma = measurement_snapshot_t__snap->i_ch2_ma;
    uint32_t__battery24Mv = measurement_snapshot_t__snap->v_bat24_mv;

    /* [EN] Over-current checks from APP_CONFIG.
       [FA] بررسی اضافه‌جریان از APP_CONFIG. */
    if (uint32_t__current1Ma > APP_CONFIG.overcurrent1_ma)
    {
        func__Fault_Set(FAULT_OVERCURRENT_1);
    }

    if (uint32_t__current2Ma > APP_CONFIG.overcurrent2_ma)
    {
        func__Fault_Set(FAULT_OVERCURRENT_2);
    }

    /* [EN] Low-battery check - latched, cleared only by Fault_Clear.
       [FA] بررسی باتری کم - قفل‌شونده، فقط با Fault_Clear پاک می‌شود. */
    if (uint32_t__battery24Mv < APP_CONFIG.low_battery_mv)
    {
        func__Fault_Set(FAULT_LOW_BATTERY);
    }
}