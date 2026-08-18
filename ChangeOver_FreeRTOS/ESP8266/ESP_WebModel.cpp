/**
 * @file ESP_WebModel.cpp
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

#include "ESP_WebModel.h"
#include "ESP_FeatureFlags.h"
#include "ESP_Config.h"
#include <ESP8266WiFi.h>
/**
 * @brief Constructs the module context with safe default state.
 * @function EspWebModel::EspWebModel
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspWebModel::EspWebModel(EspRuntimeStats& stats,
                         EspDiagnosticsStore& diagnostics,
                         EspTelemetryStore& telemetry,
                         EspLinkManager& link,
                         EspHandshake& handshake,
                         EspAutoTest& autoTest)
    : stats_(stats), diagnostics_(diagnostics), telemetry_(telemetry),
      link_(link), handshake_(handshake), autoTest_(autoTest)
{
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebModel::healthJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWebModel::healthJson() const
{
    String json;
    json.reserve(480U);
    json += F("{\"heap\":");
    json += ESP.getFreeHeap();
    json += F(",\"chipId\":");
    json += ESP.getChipId();
    json += F(",\"uptimeMs\":");
    json += millis();
    json += F(",\"wifiMode\":\"");
    json += (WiFi.getMode() == WIFI_AP) ? F("AP") : F("STA");
    json += F("\",\"rssi\":");
    json += WiFi.RSSI();
    json += F(",\"stmOnline\":");
    json += link_.stmOnline() ? F("true}") : F("false}");
    return json;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebModel::statusJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWebModel::statusJson() const
{
    String json;
    json.reserve(1600U);
    json += F("{\"health\":");
    json += healthJson();
    json += F(",\"stats\":");
    json += stats_.toJson();
    json += F(",\"link\":");
    json += link_.toJson();
    json += F(",\"handshake\":");
    json += handshake_.toJson();
    json += F(",\"diagnosticsCount\":");
    json += diagnostics_.count();
    json += F(",\"telemetryCount\":");
    json += telemetry_.count();
    json += '}';
    return json;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebModel::diagnosticsJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWebModel::diagnosticsJson() const
{
    return diagnostics_.toJson(EspConfig::MaxDiagnosticsApiRecords);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebModel::telemetryJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWebModel::telemetryJson() const
{
    return telemetry_.toJson(EspConfig::MaxTelemetryApiRecords);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebModel::featuresJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWebModel::featuresJson() const
{
    String json;
    json.reserve(800U);
    json += F("{\"wifi\":");
    json += (ESP_FEATURE_WIFI != 0) ? F("true") : F("false");
    json += F(",\"webServer\":");
    json += (ESP_FEATURE_WEB_SERVER != 0) ? F("true") : F("false");
    json += F(",\"littlefs\":");
    json += (ESP_FEATURE_LITTLEFS != 0) ? F("true") : F("false");
    json += F(",\"uart\":");
    json += (ESP_FEATURE_UART != 0) ? F("true") : F("false");
    json += F(",\"parser\":");
    json += (ESP_FEATURE_PROTOCOL_PARSER != 0) ? F("true") : F("false");
    json += F(",\"handshake\":");
    json += (ESP_FEATURE_HANDSHAKE != 0) ? F("true") : F("false");
    json += F(",\"diagnosticsStorage\":");
    json += (ESP_FEATURE_DIAGNOSTICS_STORAGE != 0) ? F("true") : F("false");
    json += F(",\"telemetryStorage\":");
    json += (ESP_FEATURE_TELEMETRY_STORAGE != 0) ? F("true") : F("false");
    json += F(",\"runtimeStorage\":");
    json += (ESP_FEATURE_RUNTIME_STORAGE != 0) ? F("true") : F("false");
    json += F(",\"charts\":");
    json += (ESP_FEATURE_CHARTS != 0) ? F("true") : F("false");
    json += F(",\"autoTest\":");
    json += (ESP_FEATURE_AUTO_TEST != 0) ? F("true") : F("false");
    json += F(",\"stmCommands\":");
    json += (ESP_FEATURE_STM_COMMANDS != 0) ? F("true}") : F("false}");
    return json;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebModel::testJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWebModel::testJson() const
{
    return autoTest_.toJson();
}
