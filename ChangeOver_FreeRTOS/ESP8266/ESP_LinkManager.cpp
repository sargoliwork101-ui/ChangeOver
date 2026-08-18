/**
 * @file ESP_LinkManager.cpp
 * @brief Arduino ESP8266 sample for ESP_LinkManager.
 * @details This file belongs to the ESP8266 STM32 link protocol layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include "ESP_LinkManager.h"
#include "ESP_Config.h"
#include "ESP_FeatureFlags.h"
/**
 * @brief Constructs the module context with safe default state.
 * @function EspLinkManager::EspLinkManager
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspLinkManager::EspLinkManager(EspRuntimeStats& stats,
                               EspDiagnosticsStore& diagnostics,
                               EspTelemetryStore& telemetry,
                               EspHandshake& handshake)
    : serial_(nullptr),
      stats_(stats),
      diagnostics_(diagnostics),
      telemetry_(telemetry),
      handshake_(handshake),
      parser_(),
      lineBuffer_{},
      lineLength_(0U),
      lastFrame_{},
      lastSimulationMs_(0UL)
{
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspLinkManager::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspLinkManager::begin(HardwareSerial& serial)
{
    serial_ = &serial;
    lineLength_ = 0U;
    lastFrame_ = {};
    handshake_.begin(serial);
}

/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function EspLinkManager::loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspLinkManager::loop()
{
#if ESP_FEATURE_UART
    if (serial_ != nullptr)
    {
        while (serial_->available() > 0)
        {
            const char received = static_cast<char>(serial_->read());
            if ((received == '\n') || (received == '\r'))
            {
                if (lineLength_ > 0U)
                {
                    lineBuffer_[lineLength_] = '\0';
                    (void)processLine(lineBuffer_);
                    lineLength_ = 0U;
                }
            }
            else if (lineLength_ < (sizeof(lineBuffer_) - 1U))
            {
                lineBuffer_[lineLength_] = received;
                lineLength_++;
            }
            else
            {
                lineLength_ = 0U;
                stats_.onInvalidFrame();
            }
        }
    }
#endif
#if ESP_FEATURE_SIMULATED_DATA
    simulateFrame();
#endif
    handshake_.loop();
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspLinkManager::injectLine
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspLinkManager::injectLine(const char* line)
{
    return processLine(line);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspLinkManager::processLine
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspLinkManager::processLine(const char* line)
{
#if !ESP_FEATURE_PROTOCOL_PARSER
    (void)line;
    stats_.onInvalidFrame();
    return false;
#else
    EspParsedFrame frame{};
    if (!parser_.parse(line, frame))
    {
        stats_.onInvalidFrame();
        return false;
    }
    stats_.onValidFrame();
    processFrame(frame);
    return true;
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspLinkManager::processFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspLinkManager::processFrame(const EspParsedFrame& frame)
{
    lastFrame_ = frame;
    handshake_.onFrame(frame);
    if (frame.type == EspFrameType::Telemetry)
    {
        EspTelemetryRecord record{};
        record.input24vMv = frame.input24vMv;
        record.battery24vMv = frame.battery24vMv;
        record.battery12vMv = frame.battery12vMv;
        record.current1Ma = frame.current1Ma;
        record.current2Ma = frame.current2Ma;
        record.timestampMs = millis();
        record.state = frame.state;
        record.faultMask = frame.faultMask;
        stats_.onTelemetry(frame.state, frame.faultMask);
        (void)telemetry_.append(record);
    }
    else if (frame.type == EspFrameType::Diagnostic)
    {
        EspDiagnosticRecord record{};
        record.code = frame.code;
        record.severity = frame.severity;
        record.value = frame.value;
        record.faultMask = frame.faultMask;
        record.state = frame.state;
        record.occurrenceCount = frame.occurrenceCount;
        record.timestampMs = millis();
        stats_.onDiagnostic(frame.code);
        (void)diagnostics_.append(record);
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspLinkManager::simulateFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspLinkManager::simulateFrame()
{
#if ESP_FEATURE_SIMULATED_DATA
    if ((millis() - lastSimulationMs_) >= 1000UL)
    {
        lastSimulationMs_ = millis();
        (void)injectLine("T,24000,25000,12000,1000,1100,2,0");
        (void)injectLine("D,4096,1,0,0,2,1");
    }
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspLinkManager::stmOnline
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspLinkManager::stmOnline() const
{
    return stats_.snapshot().stmOnline;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspLinkManager::lastFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
EspParsedFrame EspLinkManager::lastFrame() const
{
    return lastFrame_;
}

/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function EspLinkManager::toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspLinkManager::toJson() const
{
    String json;
    json.reserve(200U);
    json += F("{\"stmOnline\":");
    json += stmOnline() ? F("true") : F("false");
    json += F(",\"lastType\":");
    json += static_cast<uint8_t>(lastFrame_.type);
    json += F(",\"sequence\":");
    json += lastFrame_.sequence;
    json += F(",\"handshake\":");
    json += handshake_.toJson();
    json += '}';
    return json;
}
