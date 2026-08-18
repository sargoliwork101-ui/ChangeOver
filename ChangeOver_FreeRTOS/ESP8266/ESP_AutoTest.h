/**
 * @file ESP_AutoTest.h
 * @brief Arduino ESP8266 sample for ESP_AutoTest.
 * @details This file belongs to the ESP8266 staged test service layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_AUTO_TEST_H
#define ESP_AUTO_TEST_H

#include <Arduino.h>
#include "ESP_DiagnosticsStore.h"
#include "ESP_LinkManager.h"
#include "ESP_TelemetryStore.h"

class EspAutoTest
{
public:
    EspAutoTest(EspLinkManager& link,
                EspDiagnosticsStore& diagnostics,
                EspTelemetryStore& telemetry);
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void begin();
/**
 * @brief Starts the requested staged operation.
 * @function start
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool start(bool allowPowerTests);
/**
 * @brief Stops the requested staged operation.
 * @function stop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void stop();
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
 * @function running
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool running() const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson() const;

private:
    enum class TestStatus : uint8_t
    {
        NotRun = 0U,
        Running,
        Pass,
        Fail,
        Skipped
    };

    struct TestResult
    {
        uint16_t id;
        TestStatus status;
        uint32_t value;
        const char* name;
    };

/**
 * @brief Implements the module operation represented by this API.
 * @function executeCurrentStep
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void executeCurrentStep();
/**
 * @brief Implements the module operation represented by this API.
 * @function setResult
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void setResult(TestStatus status, uint32_t value);

    EspLinkManager& link_;
    EspDiagnosticsStore& diagnostics_;
    EspTelemetryStore& telemetry_;
    EspProtocolParser parser_;
    TestResult results_[8U];
    uint8_t currentStep_;
    uint32_t lastStepMs_;
    bool running_;
    bool allowPowerTests_;
};

#endif /* ESP_AUTO_TEST_H */
