/**
 * @file ESP8266_WebDebugger.ino
 * @brief Arduino ESP8266 sample for ESP8266_WebDebugger.
 * @details This file belongs to the Arduino sketch entry point layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "ESP_FeatureFlags.h"
#if ESP_FEATURE_LITTLEFS
#include <LittleFS.h>
#endif

#include "ESP_Config.h"
#include "ESP_DiagnosticsStore.h"
#include "ESP_AutoTest.h"
#include "ESP_Handshake.h"
#include "ESP_LinkManager.h"
#include "ESP_RuntimeStats.h"
#include "ESP_TelemetryStore.h"
#include "ESP_WebModel.h"
#include "ESP_WebServer.h"
#include "ESP_WifiManager.h"

EspRuntimeStats runtimeStats;
EspDiagnosticsStore diagnosticsStore;
EspTelemetryStore telemetryStore;
EspHandshake handshake;
/**
 * @brief Implements the module operation represented by this API.
 * @function linkManager
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
EspLinkManager linkManager(runtimeStats, diagnosticsStore, telemetryStore, handshake);
/**
 * @brief Implements the module operation represented by this API.
 * @function autoTest
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
EspAutoTest autoTest(linkManager, diagnosticsStore, telemetryStore);
EspWifiManager wifiManager;
/**
 * @brief Implements the module operation represented by this API.
 * @function webModel
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
EspWebModel webModel(runtimeStats,
                     diagnosticsStore,
                     telemetryStore,
                     linkManager,
                     handshake,
                     autoTest);
ESP8266WebServer webServer(EspConfig::WebPort);
/**
 * @brief Implements the module operation represented by this API.
 * @function webApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
EspWebServer webApi(webServer,
                    webModel,
                    runtimeStats,
                    diagnosticsStore,
                    telemetryStore,
                    autoTest);

/**
 * @brief Initializes the ESP sample and staged services.
 * @function setup
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void setup()
{
    Serial.begin(EspConfig::UartBaudRate);
    Serial.setDebugOutput(false);

#if ESP_FEATURE_LITTLEFS
    (void)LittleFS.begin();
#endif
    runtimeStats.begin();
    (void)diagnosticsStore.begin();
    (void)telemetryStore.begin();
    (void)wifiManager.begin();
    linkManager.begin(Serial);
    autoTest.begin();
    webApi.begin();
}

/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void loop()
{
    wifiManager.loop();
    linkManager.loop();
    autoTest.loop();
    webApi.loop();
    runtimeStats.loop();
    yield();
}
