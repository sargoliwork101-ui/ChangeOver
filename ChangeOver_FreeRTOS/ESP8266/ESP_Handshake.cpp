/**
 * @file ESP_Handshake.cpp
 * @brief Arduino ESP8266 sample for ESP_Handshake.
 * @details This file belongs to the ESP8266 STM32 link protocol layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include "ESP_Handshake.h"
#include "ESP_Crc.h"
#include "ESP_FeatureFlags.h"
/**
 * @brief Constructs the module context with safe default state.
 * @function EspHandshake::EspHandshake
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspHandshake::EspHandshake()
    : serial_(nullptr),
      state_(EspLinkState::Disabled),
      sessionId_(0UL),
      peerSessionId_(0UL),
      lastSequence_(0UL),
      lastHeartbeatMs_(0UL)
{
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspHandshake::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspHandshake::begin(HardwareSerial& serial)
{
    serial_ = &serial;
    sessionId_ = ESP.getChipId() ^ micros();
    lastHeartbeatMs_ = millis();
#if ESP_FEATURE_HANDSHAKE
    state_ = EspLinkState::WaitingHello;
    sendHello();
#else
    state_ = EspLinkState::Legacy;
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::onFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspHandshake::onFrame(const EspParsedFrame& frame)
{
#if ESP_FEATURE_HANDSHAKE
    lastSequence_ = frame.sequence;
    if (frame.type == EspFrameType::Hello)
    {
        peerSessionId_ = frame.session;
        state_ = EspLinkState::Established;
        sendFrame('A', peerSessionId_);
    }
    else if (frame.type == EspFrameType::PowerDownPrepare)
    {
        peerSessionId_ = frame.session;
        state_ = EspLinkState::PowerDownPending;
        sendPowerDownReady();
        state_ = EspLinkState::ReadyForPowerDown;
    }
    else if (frame.type == EspFrameType::Ack)
    {
        if ((frame.session == sessionId_) || (frame.session == peerSessionId_))
        {
            state_ = EspLinkState::Established;
        }
    }
    else
    {
        /* Telemetry and diagnostic frames do not change link state. */
    }
#else
    (void)frame;
#endif
}

/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function EspHandshake::loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspHandshake::loop()
{
#if ESP_FEATURE_HANDSHAKE
    if ((serial_ != nullptr) && ((millis() - lastHeartbeatMs_) >= 2000UL))
    {
        sendFrame('B', sessionId_);
        lastHeartbeatMs_ = millis();
    }
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::requestPowerDown
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspHandshake::requestPowerDown()
{
#if ESP_FEATURE_HANDSHAKE
    if ((serial_ == nullptr) || (state_ != EspLinkState::Established))
    {
        return false;
    }
    sendFrame('P', sessionId_);
    state_ = EspLinkState::PowerDownPending;
    return true;
#else
    return false;
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::readyForPowerDown
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspHandshake::readyForPowerDown() const
{
    return state_ == EspLinkState::ReadyForPowerDown;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::state
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
EspLinkState EspHandshake::state() const
{
    return state_;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::sessionId
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint32_t EspHandshake::sessionId() const
{
    return sessionId_;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::lastSequence
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint32_t EspHandshake::lastSequence() const
{
    return lastSequence_;
}

/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function EspHandshake::toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspHandshake::toJson() const
{
    String json;
    json.reserve(160U);
    json += F("{\"state\":");
    json += static_cast<uint8_t>(state_);
    json += F(",\"sessionId\":");
    json += sessionId_;
    json += F(",\"peerSessionId\":");
    json += peerSessionId_;
    json += F(",\"lastSequence\":");
    json += lastSequence_;
    json += F(",\"readyForPowerDown\":");
    json += readyForPowerDown() ? F("true}") : F("false}");
    return json;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::sendHello
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspHandshake::sendHello()
{
#if ESP_FEATURE_HANDSHAKE
    sendFrame('H', sessionId_);
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::sendPowerDownReady
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspHandshake::sendPowerDownReady()
{
#if ESP_FEATURE_HANDSHAKE
    sendFrame('R', peerSessionId_);
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspHandshake::sendFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspHandshake::sendFrame(char type, uint32_t session)
{
#if ESP_FEATURE_HANDSHAKE
    if (serial_ != nullptr)
    {
        String prefix;
        prefix.reserve(64U);
        prefix += type;
        prefix += F(",1,");
        prefix += session;
        prefix += ',';
        prefix += lastSequence_;
        const uint16_t crc = espCrc16Text(prefix.c_str(), prefix.length());
        serial_->print(prefix);
        serial_->print(',');
        serial_->println(crc);
    }
#else
    (void)type;
    (void)session;
#endif
}
