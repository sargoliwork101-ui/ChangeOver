/**
 * @file ESP_Config.h
 * @brief Arduino ESP8266 sample for ESP_Config.
 * @details This file belongs to the ESP8266 support module layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_CONFIG_H
#define ESP_CONFIG_H

#include <Arduino.h>
#include "ESP_FeatureFlags.h"

namespace EspConfig
{
static const uint32_t UartBaudRate = 115200UL;
static const uint16_t WebPort = 80U;
static const uint16_t MaxLineLength = 192U;
static const uint16_t MaxDiagnosticsRecords = 64U;
static const uint16_t MaxTelemetryRecords = 120U;
static const uint16_t MaxDiagnosticsApiRecords = 32U;
static const uint16_t MaxTelemetryApiRecords = 60U;
static const uint32_t StorageFlushPeriodMs = 5000UL;
static const uint16_t StorageFlushEventCount = 8U;
static const uint32_t HeartbeatTimeoutMs = 5000UL;
static const uint32_t StmDataTimeoutMs = 3000UL;
static const uint32_t AutoTestStepPeriodMs = 250UL;

static const char* const SoftApSsid = "ChangeOver-Debug";
static const char* const SoftApPassword = "changeover";
static const char* const DiagnosticsFile = "/diagnostics.bin";
static const char* const TelemetryFile = "/telemetry.bin";
static const char* const RuntimeConfigFile = "/esp_config.txt";
static const char* const RuntimeStatsFile = "/runtime_stats.bin";
static const char* const DashboardPage = "/index.html";
static const char* const DebugPage = "/debug.html";
static const char* const ChartsPage = "/charts.html";
static const char* const TestPage = "/test.html";
static const char* const SettingsPage = "/settings.html";
}

#endif /* ESP_CONFIG_H */
