#ifndef HOST_STUB_CORTEX_H
#define HOST_STUB_CORTEX_H
/**
 * @file    Firmware/Bsp/Tester/host_stub_cortex.h
 * [EN] Host shim that lets the PRODUCTION Firmware/Bsp/Src/bsp_gpio.c be
 *      compiled and RUN on a PC. The real CMSIS/HAL headers are included
 *      first (so every type and macro keeps its board meaning), then the
 *      three Cortex-M intrinsics bsp_gpio.c uses are replaced with no-ops:
 *      the interrupt-masking edge is meaningless off-target, and leaving the
 *      real inline assembly would not link on x86.
 *      This file is included BEFORE the test source and before bsp_gpio.c
 *      with -include, so the replacement is in force for both.
 */
#include "stm32f1xx_hal.h"
#undef __get_PRIMASK
#undef __disable_irq
#undef __set_PRIMASK
#define __get_PRIMASK()  (0u)
#define __disable_irq()  do { } while (0)
#define __set_PRIMASK(x) do { (void)(x); } while (0)
#endif /* HOST_STUB_CORTEX_H */
