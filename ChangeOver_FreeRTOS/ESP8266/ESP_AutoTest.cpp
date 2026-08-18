/**
 * @file ESP_AutoTest.cpp
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

#include "ESP_AutoTest.h"
#include "ESP_Config.h"
#include "ESP_FeatureFlags.h"
#if ESP_FEATURE_LITTLEFS
#include <LittleFS.h>
#endif

namespace
{
static const uint8_t TestCount = 8U;
static const uint16_t TestHealth = 100U;
static const uint16_t TestParser = 101U;
static const uint16_t TestLittleFs = 102U;
static const uint16_t TestDiagnostics = 103U;
static const uint16_t TestTelemetry = 104U;
static const uint16_t TestStmLink = 105U;
static const uint16_t TestPower = 106U;
static const uint16_t TestMemory = 107U;
}
/**
 * @brief Constructs the module context with safe default state.
 * @function EspAutoTest::EspAutoTest
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspAutoTest::EspAutoTest(EspLinkManager& link,
                         EspDiagnosticsStore& diagnostics,
                         EspTelemetryStore& telemetry)
    : link_(link),
      diagnostics_(diagnostics),
      telemetry_(telemetry),
      parser_(),
      results_{},
      currentStep_(0U),
      lastStepMs_(0UL),
      running_(false),
      allowPowerTests_(false)
{
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspAutoTest::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspAutoTest::begin()
{
    const uint16_t ids[TestCount] = {
        TestHealth, TestParser, TestLittleFs, TestDiagnostics,
        TestTelemetry, TestStmLink, TestPower, TestMemory};
    const char* names[TestCount] = {
        "health", "parser", "littlefs", "diagnostics",
        "telemetry", "stm_link", "power_safety", "memory"};
    for (uint8_t i = 0U; i < TestCount; ++i)
    {
        results_[i] = {ids[i], TestStatus::NotRun, 0UL, names[i]};
    }
}

/**
 * @brief Starts the requested staged operation.
 * @function EspAutoTest::start
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspAutoTest::start(bool allowPowerTests)
{
#if ESP_FEATURE_AUTO_TEST
    if (running_)
    {
        return false;
    }
    currentStep_ = 0U;
    lastStepMs_ = millis();
    running_ = true;
    allowPowerTests_ = allowPowerTests && (ESP_FEATURE_STM_COMMANDS != 0);
    for (uint8_t i = 0U; i < TestCount; ++i)
    {
        results_[i].status = TestStatus::NotRun;
        results_[i].value = 0UL;
    }
    results_[0].status = TestStatus::Running;
    return true;
#else
    (void)allowPowerTests;
    return false;
#endif
}

/**
 * @brief Stops the requested staged operation.
 * @function EspAutoTest::stop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspAutoTest::stop()
{
    running_ = false;
    allowPowerTests_ = false;
}

/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function EspAutoTest::loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspAutoTest::loop()
{
#if ESP_FEATURE_AUTO_TEST
    if (running_ && ((millis() - lastStepMs_) >= EspConfig::AutoTestStepPeriodMs))
    {
        executeCurrentStep();
        lastStepMs_ = millis();
        currentStep_++;
        if (currentStep_ >= TestCount)
        {
            running_ = false;
        }
        else
        {
            results_[currentStep_].status = TestStatus::Running;
        }
    }
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspAutoTest::executeCurrentStep
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspAutoTest::executeCurrentStep()
{
    bool pass;
    uint32_t value;

    pass = false;
    value = 0UL;
    switch (currentStep_)
    {
    case 0U:
        pass = ESP.getFreeHeap() > 5000U;
        value = ESP.getFreeHeap();
        break;
    case 1U:
        pass = parser_.selfTest();
        break;
    case 2U:
        pass = true;
#if ESP_FEATURE_LITTLEFS
        pass = LittleFS.begin();
#endif
        break;
    case 3U:
        pass = diagnostics_.clear();
        break;
    case 4U:
        pass = telemetry_.clear();
        break;
    case 5U:
        pass = link_.stmOnline();
        break;
    case 6U:
        pass = allowPowerTests_;
        value = allowPowerTests_ ? 1UL : 0UL;
        break;
    case 7U:
        pass = ESP.getFreeHeap() > 5000U;
        value = ESP.getFreeHeap();
        break;
    default:
        pass = false;
        break;
    }
    setResult(pass ? TestStatus::Pass : TestStatus::Fail, value);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspAutoTest::setResult
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspAutoTest::setResult(TestStatus status, uint32_t value)
{
    if (currentStep_ < TestCount)
    {
        results_[currentStep_].status = status;
        results_[currentStep_].value = value;
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspAutoTest::running
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspAutoTest::running() const
{
    return running_;
}

/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function EspAutoTest::toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspAutoTest::toJson() const
{
    String json;
    json.reserve(1200U);
    json += F("{\"running\":");
    json += running_ ? F("true") : F("false");
    json += F(",\"results\":[");
    for (uint8_t i = 0U; i < TestCount; ++i)
    {
        if (i > 0U)
        {
            json += ',';
        }
        json += F("{\"id\":");
        json += results_[i].id;
        json += F(",\"name\":\"");
        json += results_[i].name;
        json += F("\",\"status\":");
        json += static_cast<uint8_t>(results_[i].status);
        json += F(",\"value\":");
        json += results_[i].value;
        json += '}';
    }
    json += F("]}");
    return json;
}
