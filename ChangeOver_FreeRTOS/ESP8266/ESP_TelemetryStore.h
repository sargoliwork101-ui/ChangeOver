/**
 * @file ESP_TelemetryStore.h
 * @brief Arduino ESP8266 sample for ESP_TelemetryStore.
 * @details This file belongs to the ESP8266 persistent/in-memory data store layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_TELEMETRY_STORE_H
#define ESP_TELEMETRY_STORE_H

#include <Arduino.h>
#include "ESP_Config.h"

struct EspTelemetryRecord
{
    uint32_t input24vMv;
    uint32_t battery24vMv;
    uint32_t battery12vMv;
    uint32_t current1Ma;
    uint32_t current2Ma;
    uint32_t timestampMs;
    uint8_t state;
    uint32_t faultMask;
};

class EspTelemetryStore
{
public:
    EspTelemetryStore();
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool begin();
/**
 * @brief Implements the module operation represented by this API.
 * @function append
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool append(const EspTelemetryRecord& record);
/**
 * @brief Clears the selected volatile or persistent data.
 * @function clear
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool clear();
/**
 * @brief Implements the module operation represented by this API.
 * @function flush
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool flush();
/**
 * @brief Implements the module operation represented by this API.
 * @function count
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    uint16_t count() const;
/**
 * @brief Returns the requested module state.
 * @function get
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool get(uint16_t index, EspTelemetryRecord& record) const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson(uint16_t maxRecords) const;

private:
    EspTelemetryRecord records_[EspConfig::MaxTelemetryRecords];
    uint16_t head_;
    uint16_t count_;
    uint16_t pendingWrites_;
    uint32_t lastFlushMs_;
    bool dirty_;
/**
 * @brief Implements the module operation represented by this API.
 * @function physicalIndex
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    uint16_t physicalIndex(uint16_t logicalIndex) const;
};

#endif /* ESP_TELEMETRY_STORE_H */
