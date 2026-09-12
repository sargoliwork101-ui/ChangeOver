#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Single writer for power outputs.
 * Other modules request; they do not touch GPIO/PWM.
 */
void Actuator_Init(void);
void Actuator_EnterSafeState(void);

void Actuator_RequestRelay(bool on);
void Actuator_RequestBatSwitchDisconnect(bool disconnect_psu_from_battery);
void Actuator_RequestProtectBatt(bool force_battery_path_off);
void Actuator_RequestPwm1Permille(uint16_t permille);
void Actuator_RequestPwm2Permille(uint16_t permille);

#endif /* ACTUATOR_H */
