/**
 * @file ESP_RuntimeStats.h
 * @brief Arduino ESP8266 sample for ESP_RuntimeStats.
 * @details This file belongs to the ESP8266 support module layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_RUNTIME_STATS_H
#define ESP_RUNTIME_STATS_H

#include <Arduino.h>

struct EspRuntimeStatsSnapshot
{
    uint32_t bootCount;
    uint32_t validFrames;
    uint32_t invalidFrames;
    uint32_t diagnosticFrames;
    uint32_t webRequests;
    uint32_t chargeSessionCount;
    uint32_t chargeSeconds;
    uint32_t lastStmFrameMs;
    uint32_t lastDiagnosticCode;
    uint8_t lastState;
    uint32_t lastFaultMask;
    bool stmOnline;
};

class EspRuntimeStats
{
public:
    EspRuntimeStats();
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void begin();
/**
 * @brief Implements the module operation represented by this API.
 * @function onValidFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void onValidFrame();
/**
 * @brief Implements the module operation represented by this API.
 * @function onInvalidFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void onInvalidFrame();
/**
 * @brief Implements the module operation represented by this API.
 * @function onDiagnostic
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void onDiagnostic(uint32_t code);
/**
 * @brief Implements the module operation represented by this API.
 * @function onTelemetry
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void onTelemetry(uint8_t state, uint32_t faultMask);
/**
 * @brief Implements the module operation represented by this API.
 * @function onWebRequest
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void onWebRequest();
    void loop();
/**
 * @brief Returns the requested module state.
 * @function snapshot
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    EspRuntimeStatsSnapshot snapshot() const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson() const;

private:
    EspRuntimeStatsSnapshot stats_;
    uint32_t lastChargeUpdateMs_;
    uint32_t lastPersistentFlushMs_;
    bool dirty_;

    bool loadPersistent();
    bool flushPersistent();
};

#endif /* ESP_RUNTIME_STATS_H */
