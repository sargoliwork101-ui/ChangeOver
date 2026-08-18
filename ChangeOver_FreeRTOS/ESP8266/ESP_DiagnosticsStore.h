/**
 * @file ESP_DiagnosticsStore.h
 * @brief Arduino ESP8266 sample for ESP_DiagnosticsStore.
 * @details This file belongs to the ESP8266 persistent/in-memory data store layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_DIAGNOSTICS_STORE_H
#define ESP_DIAGNOSTICS_STORE_H

#include <Arduino.h>
#include "ESP_Config.h"

struct EspDiagnosticRecord
{
    uint16_t code;
    uint8_t severity;
    uint32_t value;
    uint32_t faultMask;
    uint8_t state;
    uint32_t occurrenceCount;
    uint32_t timestampMs;
};

class EspDiagnosticsStore
{
public:
    EspDiagnosticsStore();
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
    bool append(const EspDiagnosticRecord& record);
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
    bool get(uint16_t index, EspDiagnosticRecord& record) const;
/**
 * @brief Returns the requested module state.
 * @function getLast
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool getLast(EspDiagnosticRecord& record) const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson(uint16_t maxRecords) const;

private:
/**
 * @brief Implements the module operation represented by this API.
 * @function load
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool load();
/**
 * @brief Implements the module operation represented by this API.
 * @function physicalIndex
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    uint16_t physicalIndex(uint16_t logicalIndex) const;
/**
 * @brief Implements the module operation represented by this API.
 * @function resetMemory
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void resetMemory();

    EspDiagnosticRecord records_[EspConfig::MaxDiagnosticsRecords];
    uint16_t head_;
    uint16_t count_;
    uint16_t pendingWrites_;
    uint32_t lastFlushMs_;
    bool dirty_;
};

#endif /* ESP_DIAGNOSTICS_STORE_H */
