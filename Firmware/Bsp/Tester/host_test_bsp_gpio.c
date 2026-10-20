/**
 * [EN] Host test for the REAL board GPIO port (Firmware/Bsp/Src/bsp_gpio.c).
 *      bsp_gpio.c is the only file that turns a logical signal into a pin and
 *      a physical level, so it is also the only place where input polarity can
 *      be wrong. Until now that polarity was never executed on any machine:
 *      the Changeover host test uses a fake BSP, so a flipped active level
 *      would have shipped silently (audit 2026-10-20 found exactly that for
 *      JITTER1/JITTER2, both active-low from the LM393).
 *
 *      The production file is compiled unchanged; only HAL_GPIO_ReadPin /
 *      HAL_GPIO_WritePin are replaced below by a one-pin world, and the
 *      Cortex intrinsics by host_stub_cortex.h. Every check states a HARDWARE
 *      fact taken from the schematic, not a restatement of the macro:
 *        - JITTER1 / JITTER2: LM393 open-collector -> asserted = LOW
 *        - INPUT_24V_PRESENT: opto output -> asserted = HIGH
 *        - BUZZER active-high, PROTECT_BATTERY / BATTERY_SWITCH active-low
 *
 * [FA] تست هاست برای پورت واقعی پایه‌های برد (bsp_gpio.c). این فایل تنها
 *      جایی است که سیگنال منطقی به پایه و سطح فیزیکی تبدیل می‌شود، پس تنها
 *      جایی است که قطبیت ورودی می‌تواند غلط باشد. تا امروز این قطبیت روی
 *      هیچ ماشینی اجرا نمی‌شد: تست هاست Changeover از BSP بدلی استفاده
 *      می‌کند، بنابراین سطح فعالِ برعکس بی‌صدا به برد می‌رسید (ممیزی
 *      ۲۰۲۶-۱۰-۲۰ دقیقاً همین را برای ‎JITTER1/JITTER2‎ پیدا کرد؛ هر دو از
 *      LM393 فعال-پایین هستند).
 *
 *      فایل محصول بدون تغییر کامپایل می‌شود؛ فقط ‎HAL_GPIO_ReadPin‎ و
 *      ‎HAL_GPIO_WritePin‎ پایین با دنیای تک‌پایه بدل می‌شوند و intrinsic‌های
 *      کورتکس با host_stub_cortex.h. هر چک یک «واقعیت سخت‌افزاری» از شماتیک
 *      را می‌گوید، نه تکرار ماکرو را:
 *        - ‎JITTER1/JITTER2‎: خروجی ‎open-collector LM393 ->‎ تریپ = سطح پایین
 *        - INPUT_24V_PRESENT: خروجی اپتو -> حضور = سطح بالا
 *        - BUZZER فعال-بالا، PROTECT_BATTERY و BATTERY_SWITCH فعال-پایین
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "stm32f1xx_hal.h"
#include "bsp_gpio.h"
#include "board_pins.h"   /* [EN] pin numbers checked below / [FA] شمارهٔ پایه‌های چک‌شده */
#include "host_stub_cortex.h"

/* [EN] The one-pin world the fakes below read and write. */
/* [FA] دنیای تک‌پایه‌ای که بدل‌های پایین می‌خوانند و می‌نویسند. */
static GPIO_PinState GPIO_PinState__G__ReadLevel = GPIO_PIN_RESET;
static int32_t INT32_T__G__WriteCount = 0;
static GPIO_TypeDef *GPIO_TypeDef__G__LastPort = 0;
static uint16_t UINT16_T__G__LastPin = 0u;
static GPIO_PinState GPIO_PinState__G__LastLevel = GPIO_PIN_RESET;
static int32_t INT32_T__G__CheckCount = 0;
static int32_t INT32_T__G__FailCount = 0;

/**
 * [EN] Fake input: returns whatever level the current test case set.
 * [FA] ورودی بدلی: همان سطحی را برمی‌گرداند که سناریوی جاری گذاشته است.
 */
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    return GPIO_PinState__G__ReadLevel;
}

/**
 * [EN] Fake output: records the port, pin and level of the last write.
 * [FA] خروجی بدلی: پورت و پایه و سطح آخرین نوشتن را ثبت می‌کند.
 */
void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
    INT32_T__G__WriteCount++;
    GPIO_TypeDef__G__LastPort = GPIOx;
    UINT16_T__G__LastPin = GPIO_Pin;
    GPIO_PinState__G__LastLevel = PinState;
}

#define CHECK(condition)                                                       \
    do {                                                                       \
        INT32_T__G__CheckCount++;                                               \
        if (!(condition)) {                                                     \
            INT32_T__G__FailCount++;                                            \
            printf("FAIL line %d: %s\n", __LINE__, #condition);                 \
        }                                                                       \
    } while (0)

/**
 * [EN] Runs the polarity checklist: inputs against their real active level,
 *      outputs against theirs, and the rule that an input can never be driven.
 * [FA] چک‌لیست قطبیت را اجرا می‌کند: ورودی‌ها با سطح فعال واقعی خودشان،
 *      خروجی‌ها با سطح خودشان، و قاعدهٔ اینکه ورودی هرگز نوشته نمی‌شود.
 */
int main(void)
{
    /* --- LM393 trip inputs: asserted means PHYSICAL LOW --- */
    /* [FA] ورودی‌های تریپ LM393: تریپ یعنی سطح فیزیکی پایین. */
    GPIO_PinState__G__ReadLevel = GPIO_PIN_RESET;
    CHECK(func__BspGpio_Read(BSP_GPIO_JITTER1) == true);
    CHECK(func__BspGpio_Read(BSP_GPIO_JITTER2) == true);

    GPIO_PinState__G__ReadLevel = GPIO_PIN_SET;
    CHECK(func__BspGpio_Read(BSP_GPIO_JITTER1) == false);
    CHECK(func__BspGpio_Read(BSP_GPIO_JITTER2) == false);

    /* --- 24 V presence: asserted means PHYSICAL HIGH --- */
    /* [FA] حضور ۲۴ ولت: حضور یعنی سطح فیزیکی بالا. */
    GPIO_PinState__G__ReadLevel = GPIO_PIN_SET;
    CHECK(func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT) == true);

    GPIO_PinState__G__ReadLevel = GPIO_PIN_RESET;
    CHECK(func__BspGpio_Read(BSP_GPIO_INPUT_24V_PRESENT) == false);

    /* --- outputs keep their own polarity --- */
    /* [FA] خروجی‌ها قطبیت خودشان را نگه می‌دارند. */
    INT32_T__G__WriteCount = 0;
    func__BspGpio_Write(BSP_GPIO_BUZZER, true);
    CHECK(INT32_T__G__WriteCount == 1);
    CHECK(GPIO_PinState__G__LastLevel == GPIO_PIN_SET);

    INT32_T__G__WriteCount = 0;
    func__BspGpio_Write(BSP_GPIO_BUZZER, false);
    CHECK(INT32_T__G__WriteCount == 1);
    CHECK(GPIO_PinState__G__LastLevel == GPIO_PIN_RESET);

    INT32_T__G__WriteCount = 0;
    func__BspGpio_Write(BSP_GPIO_PROTECT_BATTERY, true);
    CHECK(INT32_T__G__WriteCount == 1);
    CHECK(GPIO_PinState__G__LastLevel == GPIO_PIN_RESET);

    INT32_T__G__WriteCount = 0;
    func__BspGpio_Write(BSP_GPIO_BATTERY_SWITCH, false);
    CHECK(INT32_T__G__WriteCount == 1);
    CHECK(GPIO_PinState__G__LastLevel == GPIO_PIN_SET);

    /* --- inputs are NOT writable --- */
    /* [FA] ورودی‌ها نوشتنی نیستند. */
    INT32_T__G__WriteCount = 0;
    func__BspGpio_Write(BSP_GPIO_JITTER1, true);
    func__BspGpio_Write(BSP_GPIO_JITTER2, true);
    func__BspGpio_Write(BSP_GPIO_INPUT_24V_PRESENT, true);
    CHECK(INT32_T__G__WriteCount == 0);

    /* --- atomic cut helper keeps its meaning --- */
    /* [EN] The helper always drives BATTERY_SWITCH (active-low): input present
     *      -> deasserted (physical high) = path cut, and it reports true;
     *      input gone -> asserted (physical low) = battery kept, reports false.
     * [FA] کمک‌تابع همیشه ‎BATTERY_SWITCH‎ را می‌زند (فعال-پایین): ورودی حاضر
     *      -> غیرفعال (سطح بالا) یعنی قطع و true؛ ورودی رفته -> فعال (سطح
     *      پایین) یعنی باتری وصل و false. */
    GPIO_PinState__G__ReadLevel = GPIO_PIN_SET;
    INT32_T__G__WriteCount = 0;
    CHECK(func__BspGpio_DisconnectBatteryPathIfInputPresent() == true);
    CHECK(INT32_T__G__WriteCount == 1);
    CHECK(UINT16_T__G__LastPin == PIN_BAT_SWITCH_PIN);
    CHECK(GPIO_PinState__G__LastLevel == GPIO_PIN_SET);

    GPIO_PinState__G__ReadLevel = GPIO_PIN_RESET;
    INT32_T__G__WriteCount = 0;
    CHECK(func__BspGpio_DisconnectBatteryPathIfInputPresent() == false);
    CHECK(INT32_T__G__WriteCount == 1);
    CHECK(UINT16_T__G__LastPin == PIN_BAT_SWITCH_PIN);
    CHECK(GPIO_PinState__G__LastLevel == GPIO_PIN_RESET);

    printf("checks: %ld, fails: %ld\n", (long)INT32_T__G__CheckCount,
           (long)INT32_T__G__FailCount);
    return (INT32_T__G__FailCount == 0) ? 0 : 1;
}
