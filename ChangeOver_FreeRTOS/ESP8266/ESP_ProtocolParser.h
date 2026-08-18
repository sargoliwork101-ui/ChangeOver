/**
 * @file ESP_ProtocolParser.h
 * @brief Arduino ESP8266 sample for ESP_ProtocolParser.
 * @details This file belongs to the ESP8266 STM32 link protocol layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#ifndef ESP_PROTOCOL_PARSER_H
#define ESP_PROTOCOL_PARSER_H

#include <Arduino.h>

enum class EspFrameType : uint8_t
{
    Invalid = 0U,
    Telemetry,
    Diagnostic,
    Hello,
    HelloAck,
    SyncRequest,
    SyncData,
    SyncEnd,
    Ack,
    Heartbeat,
    PowerDownPrepare,
    PowerDownReady
};

struct EspParsedFrame
{
    EspFrameType type;
    uint8_t protocolVersion;
    uint32_t sequence;
    uint32_t session;
    uint32_t input24vMv;
    uint32_t battery24vMv;
    uint32_t battery12vMv;
    uint32_t current1Ma;
    uint32_t current2Ma;
    uint16_t code;
    uint8_t severity;
    uint32_t value;
    uint32_t faultMask;
    uint8_t state;
    uint32_t occurrenceCount;
    uint16_t crc;
    bool hasCrc;
};

class EspProtocolParser
{
public:
    EspProtocolParser();
/**
 * @brief Parses one bounded protocol input.
 * @function parse
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool parse(const char* line, EspParsedFrame& frame) const;
/**
 * @brief Implements the module operation represented by this API.
 * @function selfTest
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool selfTest() const;

private:
/**
 * @brief Parses one bounded protocol input.
 * @function parseUnsigned
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool parseUnsigned(const char* token, uint32_t& value) const;
/**
 * @brief Parses one bounded protocol input.
 * @function parseFields
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
    bool parseFields(char* work, EspParsedFrame& frame) const;
};

#endif /* ESP_PROTOCOL_PARSER_H */
