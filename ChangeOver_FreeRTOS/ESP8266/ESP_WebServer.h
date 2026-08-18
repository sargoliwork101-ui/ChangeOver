/**
 * @file ESP_WebServer.h
 * @brief Arduino ESP8266 sample for ESP_WebServer.
 * @details This file belongs to the ESP8266 Web Server and UI integration layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_WEB_SERVER_H
#define ESP_WEB_SERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "ESP_AutoTest.h"
#include "ESP_DiagnosticsStore.h"
#include "ESP_RuntimeStats.h"
#include "ESP_TelemetryStore.h"
#include "ESP_WebModel.h"

class EspWebServer
{
public:
    EspWebServer(ESP8266WebServer& server,
                 EspWebModel& model,
                 EspRuntimeStats& stats,
                 EspDiagnosticsStore& diagnostics,
                 EspTelemetryStore& telemetry,
                 EspAutoTest& autoTest);
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void begin();
/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void loop();

private:
    static EspWebServer* active_;
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleDashboard
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleDashboard();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleDebugPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleDebugPage();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleChartsPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleChartsPage();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleTestPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleTestPage();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleSettingsPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleSettingsPage();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleHealthApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleHealthApi();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleStatusApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleStatusApi();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleDiagnosticsApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleDiagnosticsApi();
/**
 * @brief Clears the selected volatile or persistent data.
 * @function handleDiagnosticsClearApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleDiagnosticsClearApi();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleTelemetryApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleTelemetryApi();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleFeaturesApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleFeaturesApi();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleTestStatusApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleTestStatusApi();
/**
 * @brief Starts the requested staged operation.
 * @function handleTestStartApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleTestStartApi();
/**
 * @brief Stops the requested staged operation.
 * @function handleTestStopApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleTestStopApi();
/**
 * @brief Handles one Web Server route or protocol event.
 * @function handleNotFound
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    static void handleNotFound();

/**
 * @brief Implements the module operation represented by this API.
 * @function servePage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void servePage(const String& path);
/**
 * @brief Implements the module operation represented by this API.
 * @function sendJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void sendJson(const String& json);
/**
 * @brief Implements the module operation represented by this API.
 * @function sendText
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void sendText(uint16_t code, const String& text);

    ESP8266WebServer& server_;
    EspWebModel& model_;
    EspRuntimeStats& stats_;
    EspDiagnosticsStore& diagnostics_;
    EspTelemetryStore& telemetry_;
    EspAutoTest& autoTest_;
};

#endif /* ESP_WEB_SERVER_H */
