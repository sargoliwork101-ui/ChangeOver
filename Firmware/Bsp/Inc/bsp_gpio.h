/**
 * @file    bsp_gpio.h
 * @brief   [EN] Board-independent logical GPIO interface.
 * @note    [EN] Product modules use logical signals; physical ports, pins and
 *              active levels remain private to the board-specific port.
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H
#include <stdbool.h>
typedef enum
{
    BSP_GPIO_BUZZER = 0,
    BSP_GPIO_LED_GREEN,
    BSP_GPIO_LED_RED,
    BSP_GPIO_LED_YELLOW,
    BSP_GPIO_ESP_CHPD,
    BSP_GPIO_INPUT_24V_PRESENT,
    BSP_GPIO_BATTERY_SWITCH,
    BSP_GPIO_RELAY,
    BSP_GPIO_PROTECT_BATTERY,
    BSP_GPIO_JITTER1,
    BSP_GPIO_JITTER2
} bsp_gpio_id_t;
/* ==================== BspGpio_Init ==================== */
/**
 * @brief  [EN] Apply the board safe state to every controllable output.
 */
void func__BspGpio_Init(void);
/* ==================== BspGpio_Write ==================== */
/**
 * @brief  [EN] Assert or deassert a logical output. Board-specific active
 *              polarity is applied privately; input-only identifiers are ignored.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 * @param  bool__asserted [EN] true=asserted/on, false=deasserted/off /
 */
void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted);
/* ==================== BspGpio_DisconnectBatteryPathIfInputPresent ==================== */
/**
 * @brief  [EN] Cut the MCU self-supply battery path ONLY when the DC input is
 *              still present at the instant of the write. The presence read,
 *              the decision and the pin write run inside one short PRIMASK
 *              critical section, so the PB4 EXTI handler (which reconnects the
 *              battery immediately) cannot slip between them and be undone by
 *              a stale task decision. A short critical section is used instead
 *              of a scheduler lock on purpose: EXTI is an interrupt, and only
 *              masking interrupts excludes it.
 *              Returns true when the path was actually disconnected; false
 *              when the input was already gone and the battery side was left
 *              connected.
* @return bool true = battery path is now cut /
 */
bool func__BspGpio_DisconnectBatteryPathIfInputPresent(void);
/* ==================== BspGpio_Read ==================== */
/**
 * @brief  [EN] Read a logical signal. Board-specific active polarity is
 *              applied privately for outputs and active-level inputs.
 * @param  bsp_gpio_id_t__id [EN] Logical signal identifier /
 * @return bool [EN] true when the logical signal is asserted /
 */
bool func__BspGpio_Read(bsp_gpio_id_t bsp_gpio_id_t__id);
#endif /* BSP_GPIO_H */
