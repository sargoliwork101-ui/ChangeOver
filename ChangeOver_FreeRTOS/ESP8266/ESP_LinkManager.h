/**
 * @file ESP_LinkManager.h
 * @brief Arduino ESP8266 sample for ESP_LinkManager.
 * @details This file belongs to the ESP8266 STM32 link protocol layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_LINK_MANAGER_H
#define ESP_LINK_MANAGER_H

#include <Arduino.h>
#include "ESP_DiagnosticsStore.h"
#include "ESP_Handshake.h"
#include "ESP_ProtocolParser.h"
#include "ESP_RuntimeStats.h"
#include "ESP_TelemetryStore.h"

class EspLinkManager
{
public:
    EspLinkManager(EspRuntimeStats& stats,
                   EspDiagnosticsStore& diagnostics,
                   EspTelemetryStore& telemetry,
                   EspHandshake& handshake);
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void begin(HardwareSerial& serial);
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
 * @function injectLine
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool injectLine(const char* line);
/**
 * @brief Implements the module operation represented by this API.
 * @function stmOnline
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool stmOnline() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function lastFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    EspParsedFrame lastFrame() const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson() const;

private:
/**
 * @brief Implements the module operation represented by this API.
 * @function processLine
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool processLine(const char* line);
/**
 * @brief Implements the module operation represented by this API.
 * @function processFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void processFrame(const EspParsedFrame& frame);
/**
 * @brief Implements the module operation represented by this API.
 * @function simulateFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void simulateFrame();

    HardwareSerial* serial_;
    EspRuntimeStats& stats_;
    EspDiagnosticsStore& diagnostics_;
    EspTelemetryStore& telemetry_;
    EspHandshake& handshake_;
    EspProtocolParser parser_;
    char lineBuffer_[192U];
    uint16_t lineLength_;
    EspParsedFrame lastFrame_;
    uint32_t lastSimulationMs_;
};

#endif /* ESP_LINK_MANAGER_H */
