/**
 * @file ui.c
 * @brief هر پروفایل = چند قدم. هر قدم: کدام LED روشن باشد و چند ms بماند.
 *
 * چرا جدول، نه if و vTaskDelay؟
 *   1) delay داخل ماژول کل همان Task را قفل می‌کند؛ عوض کردن حالت دیر می‌شود.
 *   2) اگر 10 حالت داشته باشی، if تو در تو غیرقابل‌خواندن می‌شود.
 *   3) حالت جدید = چند خط به جدول اضافه کردن.
 *
 * مثال رویداد 2 همان سه delay مثال تو است، فقط بدون delay:
 *   قدم0: سبز+قرمز  500 ms
 *   قدم1: هر دو خاموش 500 ms
 *   قدم2: فقط قرمز     500 ms  (سبز هنوز خاموش → جمع خاموشی سبز = 1000)
 *   بعد برمی‌گردد قدم0
 */

#include "ui.h"
#include "bsp_gpio.h"
#include "board_pins.h"
#include "app_config.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    bool red;
    bool yellow;
    bool green;
    bool buzzer;
    uint32_t duration_ms;
} ui_step_t;

typedef struct
{
    const ui_step_t *steps;
    uint32_t count;
    bool repeat;           /* false یعنی بعد از آخری برود EVENT1 */
    ui_profile_t next;     /* اگر repeat نباشد */
} ui_profile_desc_t;

/* --- جدول‌ها: این‌جا الگو را عوض کن، نه در Task --- */

static const ui_step_t s_steps_off[] =
{
    { false, false, false, false, 100u }
};

static const ui_step_t s_steps_selftest[] =
{
    { true,  false, false, false, 500u }, /* قرمز */
    { false, true,  false, false, 500u }, /* زرد */
    { false, false, true,  false, 500u }, /* سبز */
    { false, false, false, true,  150u }  /* بوق */
};

static const ui_step_t s_steps_event1[] =
{
    { false, false, true,  false, 500u }, /* سبز روشن */
    { false, false, false, false, 500u }  /* سبز خاموش */
};

static const ui_step_t s_steps_event2[] =
{
    { true,  false, true,  false, 500u }, /* سبز روشن، قرمز روشن */
    { false, false, false, false, 500u }, /* هر دو خاموش */
    { true,  false, false, false, 500u }  /* سبز خاموش، قرمز روشن */
};

static const ui_profile_desc_t s_profiles[] =
{
    { s_steps_off,      1u, true,  UI_PROFILE_OFF },
    { s_steps_selftest, 4u, false, UI_PROFILE_EVENT1 },
    { s_steps_event1,   2u, true,  UI_PROFILE_EVENT1 },
    { s_steps_event2,   3u, true,  UI_PROFILE_EVENT2 }
};

static ui_profile_t s_profile;
static uint32_t s_step_index;
static uint32_t s_elapsed_ms;

static const ui_profile_desc_t *ui_desc(ui_profile_t profile)
{
    uint32_t index;

    index = (uint32_t)profile;
    if (index >= (sizeof(s_profiles) / sizeof(s_profiles[0])))
    {
        index = (uint32_t)UI_PROFILE_OFF;
    }

    return &s_profiles[index];
}

static void ui_apply_step(const ui_step_t *step)
{
    BspGpio_Write(PIN_LED_R_PORT, PIN_LED_R_PIN, step->red);
    BspGpio_Write(PIN_LED_Y_PORT, PIN_LED_Y_PIN, step->yellow);
    BspGpio_Write(PIN_LED_G_PORT, PIN_LED_G_PIN, step->green);
    BspGpio_Write(PIN_BUZZER_PORT, PIN_BUZZER_PIN, step->buzzer);
}

void Ui_Init(void)
{
    s_profile = UI_PROFILE_OFF;
    s_step_index = 0u;
    s_elapsed_ms = 0u;
    ui_apply_step(&s_steps_off[0]);
}

void Ui_SetProfile(ui_profile_t profile)
{
    const ui_profile_desc_t *desc;

    s_profile = profile;
    s_step_index = 0u;
    s_elapsed_ms = 0u;

    desc = ui_desc(s_profile);
    ui_apply_step(&desc->steps[0]);
}

void Ui_Run(void)
{
    const ui_profile_desc_t *desc;
    const ui_step_t *step;

    desc = ui_desc(s_profile);
    step = &desc->steps[s_step_index];

    s_elapsed_ms = s_elapsed_ms + APP_CONFIG.ui_period_ms;

    if (s_elapsed_ms < step->duration_ms)
    {
        return;
    }

    /* مدت این قدم تمام شد → قدم بعدی */
    s_elapsed_ms = 0u;
    s_step_index = s_step_index + 1u;

    if (s_step_index >= desc->count)
    {
        if (desc->repeat == true)
        {
            s_step_index = 0u;
        }
        else
        {
            Ui_SetProfile(desc->next);
            return;
        }
    }

    ui_apply_step(&desc->steps[s_step_index]);
}
