/**
 * @file ESP_WebModel.h
 * @brief Arduino ESP8266 sample for ESP_WebModel.
 * @details This file belongs to the ESP8266 Web Server and UI integration layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_WEB_MODEL_H
#define ESP_WEB_MODEL_H

#include <Arduino.h>
#include "ESP_AutoTest.h"
#include "ESP_DiagnosticsStore.h"
#include "ESP_LinkManager.h"
#include "ESP_RuntimeStats.h"
#include "ESP_TelemetryStore.h"

class EspWebModel
{
public:
    EspWebModel(EspRuntimeStats& stats,
                EspDiagnosticsStore& diagnostics,
                EspTelemetryStore& telemetry,
                EspLinkManager& link,
                EspHandshake& handshake,
                EspAutoTest& autoTest);
/**
 * @brief Implements the module operation represented by this API.
 * @function healthJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String healthJson() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function statusJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String statusJson() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function diagnosticsJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String diagnosticsJson() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function telemetryJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String telemetryJson() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function featuresJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String featuresJson() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function testJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String testJson() const;

private:
    EspRuntimeStats& stats_;
    EspDiagnosticsStore& diagnostics_;
    EspTelemetryStore& telemetry_;
    EspLinkManager& link_;
    EspHandshake& handshake_;
    EspAutoTest& autoTest_;
};

#endif /* ESP_WEB_MODEL_H */
