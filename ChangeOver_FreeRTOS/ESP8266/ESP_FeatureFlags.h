/**
 * @file ESP_FeatureFlags.h
 * @brief Arduino ESP8266 sample for ESP_FeatureFlags.
 * @details This file belongs to the ESP8266 support module layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_FEATURE_FLAGS_H
#define ESP_FEATURE_FLAGS_H

/*
 * Compile-time feature gates for staged bring-up.
 * Set only one new feature to 1 per test phase and record the result in the
 * repository history. A feature disabled here cannot be enabled by the web UI.
 */

#define ESP_FEATURE_WIFI                 0
#define ESP_FEATURE_WEB_SERVER           0
#define ESP_FEATURE_LITTLEFS             0
#define ESP_FEATURE_UART                 0
#define ESP_FEATURE_PROTOCOL_PARSER      0
#define ESP_FEATURE_HANDSHAKE            0
#define ESP_FEATURE_DIAGNOSTICS_STORAGE  0
#define ESP_FEATURE_TELEMETRY_STORAGE    0
#define ESP_FEATURE_RUNTIME_STORAGE      0
#define ESP_FEATURE_CHARTS               0
#define ESP_FEATURE_AUTO_TEST            0
#define ESP_FEATURE_STM_COMMANDS         0
#define ESP_FEATURE_AUTHENTICATION       0
#define ESP_FEATURE_OTA                  0
#define ESP_FEATURE_SIMULATED_DATA       0
#define ESP_FEATURE_VERBOSE_LOG          0

#endif /* ESP_FEATURE_FLAGS_H */
