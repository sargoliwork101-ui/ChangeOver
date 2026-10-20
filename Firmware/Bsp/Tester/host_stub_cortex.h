#ifndef HOST_STUB_CORTEX_H
#define HOST_STUB_CORTEX_H

/**
 * [EN] Host shim that lets the PRODUCTION Firmware/Bsp/Src/bsp_gpio.c be
 *      compiled and RUN on a PC. The real CMSIS/HAL headers are included
 *      first (so every type and macro keeps its board meaning), then the
 *      three Cortex-M intrinsics bsp_gpio.c uses are replaced with no-ops:
 *      the interrupt-masking edge is meaningless off-target, and leaving the
 *      real inline assembly would not link on x86.
 *      This file is included BEFORE the test source and before bsp_gpio.c
 *      with -include, so the replacement is in force for both.
 *
 * [FA] شیم هاست که اجازه می‌دهد فایل «واقعی» bsp_gpio.c روی کامپیوتر
 *      کامپایل و اجرا شود. اول هدرهای واقعی ‎CMSIS/HAL‎ خوانده می‌شوند (تا
 *      همهٔ تایپ‌ها و ماکروها همان معنای برد را داشته باشند)، بعد سه
 *      intrinsic کورتکس که bsp_gpio.c استفاده می‌کند با بدل بی‌اثر عوض
 *      می‌شوند: قفل وقفه بیرون از برد معنایی ندارد و اسمبلی واقعی روی x86
 *      لینک نمی‌شود. این فایل با ‎-include‎ پیش از تست و پیش از bsp_gpio.c
 *      خوانده می‌شود تا جایگزینی برای هر دو معتبر باشد.
 */
#include "stm32f1xx_hal.h"

#undef __get_PRIMASK
#undef __disable_irq
#undef __set_PRIMASK
#define __get_PRIMASK()  (0u)
#define __disable_irq()  do { } while (0)
#define __set_PRIMASK(x) do { (void)(x); } while (0)

#endif /* HOST_STUB_CORTEX_H */
