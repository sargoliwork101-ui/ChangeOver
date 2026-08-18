/**
 * @file ESP_WifiManager.h
 * @brief Arduino ESP8266 sample for ESP_WifiManager.
 * @details This file belongs to the ESP8266 support module layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_WIFI_MANAGER_H
#define ESP_WIFI_MANAGER_H

#include <Arduino.h>

class EspWifiManager
{
public:
    EspWifiManager();
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool begin();
/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void loop();
/**
 * @brief Implements the module operation represented by this API.
 * @function connected
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool connected() const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson() const;

private:
    bool connected_;
    uint32_t reconnectCount_;
};

#endif /* ESP_WIFI_MANAGER_H */
