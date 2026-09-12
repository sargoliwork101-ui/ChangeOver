#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    APP_STATE_BOOT = 0,
    APP_STATE_IDLE,
    APP_STATE_INPUT,
    APP_STATE_BATTERY,
    APP_STATE_FAULT,
    APP_STATE_SAFE
} app_state_t;

typedef struct
{
    uint32_t v_in_mv;
    uint32_t v_bat24_mv;
    uint32_t v_bat12_mv;
    uint32_t i_ch1_ma;
    uint32_t i_ch2_ma;
    bool     input_present;
    bool     valid;
} measurement_snapshot_t;

typedef uint32_t fault_mask_t;

#define FAULT_NONE              0u
#define FAULT_ADC               (1u << 0)
#define FAULT_OVERCURRENT_1     (1u << 1)
#define FAULT_OVERCURRENT_2     (1u << 2)
#define FAULT_LOW_BATTERY       (1u << 3)
#define FAULT_JITTER_1          (1u << 4)
#define FAULT_JITTER_2          (1u << 5)

#endif /* APP_TYPES_H */
