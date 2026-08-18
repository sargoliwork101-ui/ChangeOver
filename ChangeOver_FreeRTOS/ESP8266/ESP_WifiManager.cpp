/**
 * @file ESP_WifiManager.cpp
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

#include "ESP_WifiManager.h"
#include "ESP_Config.h"
#include "ESP_FeatureFlags.h"
#include <ESP8266WiFi.h>
/**
 * @brief Constructs the module context with safe default state.
 * @function EspWifiManager::EspWifiManager
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspWifiManager::EspWifiManager()
    : connected_(false), reconnectCount_(0UL)
{
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspWifiManager::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspWifiManager::begin()
{
#if ESP_FEATURE_WIFI
    WiFi.mode(WIFI_AP);
    connected_ = WiFi.softAP(EspConfig::SoftApSsid,
                             EspConfig::SoftApPassword);
#else
    connected_ = false;
#endif
    return connected_;
}

/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function EspWifiManager::loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWifiManager::loop()
{
#if ESP_FEATURE_WIFI
    connected_ = WiFi.getMode() == WIFI_AP;
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWifiManager::connected
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspWifiManager::connected() const
{
    return connected_;
}

/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function EspWifiManager::toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspWifiManager::toJson() const
{
    String json;
    json.reserve(180U);
    json += F("{\"connected\":");
    json += connected_ ? F("true") : F("false");
    json += F(",\"mode\":\"");
    json += (WiFi.getMode() == WIFI_AP) ? F("AP") : F("STA");
    json += F("\",\"ip\":\"");
    json += WiFi.softAPIP().toString();
    json += F("\",\"rssi\":");
    json += WiFi.RSSI();
    json += F(",\"reconnectCount\":");
    json += reconnectCount_;
    json += '}';
    return json;
}
