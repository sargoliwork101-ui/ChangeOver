/**
 * @file ESP_Handshake.h
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

#ifndef ESP_HANDSHAKE_H
#define ESP_HANDSHAKE_H

#include <Arduino.h>
#include "ESP_ProtocolParser.h"

enum class EspLinkState : uint8_t
{
    Disabled = 0U,
    Legacy,
    WaitingHello,
    Established,
    PowerDownPending,
    ReadyForPowerDown
};

class EspHandshake
{
public:
    EspHandshake();
/**
 * @brief Initializes the module state and dependencies.
 * @function begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void begin(HardwareSerial& serial);
/**
 * @brief Implements the module operation represented by this API.
 * @function onFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void onFrame(const EspParsedFrame& frame);
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
 * @function requestPowerDown
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool requestPowerDown();
/**
 * @brief Implements the module operation represented by this API.
 * @function readyForPowerDown
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool readyForPowerDown() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function state
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    EspLinkState state() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function sessionId
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    uint32_t sessionId() const;
/**
 * @brief Implements the module operation represented by this API.
 * @function lastSequence
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    uint32_t lastSequence() const;
/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    String toJson() const;

private:
/**
 * @brief Implements the module operation represented by this API.
 * @function sendHello
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void sendHello();
/**
 * @brief Implements the module operation represented by this API.
 * @function sendPowerDownReady
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void sendPowerDownReady();
/**
 * @brief Implements the module operation represented by this API.
 * @function sendFrame
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    void sendFrame(char type, uint32_t session);

    HardwareSerial* serial_;
    EspLinkState state_;
    uint32_t sessionId_;
    uint32_t peerSessionId_;
    uint32_t lastSequence_;
    uint32_t lastHeartbeatMs_;
};

#endif /* ESP_HANDSHAKE_H */
