/**
 * @file ESP_WebPages.h
 * @brief Arduino ESP8266 sample for ESP_WebPages.
 * @details This file belongs to the ESP8266 Web Server and UI integration layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_WEB_PAGES_H
#define ESP_WEB_PAGES_H

#include <Arduino.h>

namespace EspWebPages
{
/**
 * @brief Implements the module operation represented by this API.
 * @function fallback
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String fallback(const String& path);
/**
 * @brief Implements the module operation represented by this API.
 * @function contentType
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
const char* contentType(const String& path);
}

#endif /* ESP_WEB_PAGES_H */
