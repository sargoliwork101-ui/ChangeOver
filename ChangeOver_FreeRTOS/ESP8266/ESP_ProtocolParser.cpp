/**
 * @file ESP_ProtocolParser.cpp
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

#include "ESP_ProtocolParser.h"
#include "ESP_Config.h"
#include "ESP_Crc.h"
#include "ESP_FeatureFlags.h"
#include <cstdlib>
#include <cstring>

namespace
{
/**
 * @brief Implements the module operation represented by this API.
 * @function tokenEquals
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
static bool tokenEquals(const char* token, const char* expected)
{
    return (token != nullptr) && (expected != nullptr) &&
           (std::strcmp(token, expected) == 0);
}
}
/**
 * @brief Constructs the module context with safe default state.
 * @function EspProtocolParser::EspProtocolParser
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspProtocolParser::EspProtocolParser() = default;

/**
 * @brief Parses one bounded protocol input.
 * @function EspProtocolParser::parseUnsigned
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspProtocolParser::parseUnsigned(const char* token, uint32_t& value) const
{
    char* end = nullptr;
    if ((token == nullptr) || (*token == '\0'))
    {
        return false;
    }
    const unsigned long parsed = std::strtoul(token, &end, 10);
    if ((end == token) || (*end != '\0') || (parsed > 0xFFFFFFFFUL))
    {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

/**
 * @brief Parses one bounded protocol input.
 * @function EspProtocolParser::parseFields
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspProtocolParser::parseFields(char* work, EspParsedFrame& frame) const
{
    char* token = std::strtok(work, ",");
    uint32_t parsed = 0UL;
    if (token == nullptr)
    {
        return false;
    }

    if (tokenEquals(token, "T"))
    {
        frame.type = EspFrameType::Telemetry;
        uint32_t* fields[] = {&frame.input24vMv, &frame.battery24vMv,
                              &frame.battery12vMv, &frame.current1Ma,
                              &frame.current2Ma, &parsed, &frame.faultMask};
        for (uint8_t index = 0U; index < 7U; ++index)
        {
            token = std::strtok(nullptr, ",");
            if ((token == nullptr) || !parseUnsigned(token, *fields[index]))
            {
                return false;
            }
            if (index == 5U)
            {
                frame.state = static_cast<uint8_t>(parsed);
            }
        }
        return true;
    }

    if (tokenEquals(token, "D"))
    {
        frame.type = EspFrameType::Diagnostic;
        uint32_t* fields[] = {&parsed, &parsed, &frame.value,
                              &frame.faultMask, &parsed,
                              &frame.occurrenceCount};
        for (uint8_t index = 0U; index < 6U; ++index)
        {
            token = std::strtok(nullptr, ",");
            if ((token == nullptr) || !parseUnsigned(token, *fields[index]))
            {
                return false;
            }
            if (index == 0U)
            {
                frame.code = static_cast<uint16_t>(parsed);
            }
            else if (index == 1U)
            {
                frame.severity = static_cast<uint8_t>(parsed);
            }
            else if (index == 4U)
            {
                frame.state = static_cast<uint8_t>(parsed);
            }
        }
        return true;
    }

    if (tokenEquals(token, "H") || tokenEquals(token, "A") ||
        tokenEquals(token, "S") || tokenEquals(token, "P") ||
        tokenEquals(token, "R") || tokenEquals(token, "B"))
    {
        if (tokenEquals(token, "H")) frame.type = EspFrameType::Hello;
        else if (tokenEquals(token, "A")) frame.type = EspFrameType::Ack;
        else if (tokenEquals(token, "S")) frame.type = EspFrameType::SyncRequest;
        else if (tokenEquals(token, "P")) frame.type = EspFrameType::PowerDownPrepare;
        else if (tokenEquals(token, "R")) frame.type = EspFrameType::PowerDownReady;
        else frame.type = EspFrameType::Heartbeat;

        token = std::strtok(nullptr, ",");
        if ((token == nullptr) || !parseUnsigned(token, parsed)) return false;
        frame.protocolVersion = static_cast<uint8_t>(parsed);
        token = std::strtok(nullptr, ",");
        if ((token == nullptr) || !parseUnsigned(token, frame.session)) return false;
        token = std::strtok(nullptr, ",");
        if ((token == nullptr) || !parseUnsigned(token, frame.sequence)) return false;
        token = std::strtok(nullptr, ",");
        if ((token == nullptr) || !parseUnsigned(token, parsed)) return false;
        frame.crc = static_cast<uint16_t>(parsed);
        frame.hasCrc = true;
        return true;
    }

    return false;
}

/**
 * @brief Parses one bounded protocol input.
 * @function EspProtocolParser::parse
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspProtocolParser::parse(const char* line, EspParsedFrame& frame) const
{
    char work[EspConfig::MaxLineLength];
    const size_t length = (line == nullptr) ? 0U : std::strlen(line);
    if ((line == nullptr) || (length == 0U) || (length >= sizeof(work)))
    {
        return false;
    }
    std::memcpy(work, line, length + 1U);
    frame = {};
    if (!parseFields(work, frame))
    {
        return false;
    }
#if ESP_FEATURE_HANDSHAKE
    if ((frame.hasCrc) && (frame.type != EspFrameType::Invalid))
    {
        const char* lastComma = std::strrchr(line, ',');
        if (lastComma == nullptr)
        {
            return false;
        }
        const size_t prefixLength = static_cast<size_t>(lastComma - line);
        const uint16_t expected = espCrc16Text(line, prefixLength);
        if (expected != frame.crc)
        {
            return false;
        }
    }
#endif
    return true;
}

/**
 * @brief Parses one bounded protocol input.
 * @function EspProtocolParser::selfTest
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspProtocolParser::selfTest() const
{
    EspParsedFrame frame{};
    return parse("T,24000,25000,12000,1000,1100,2,0", frame) &&
           (frame.type == EspFrameType::Telemetry) &&
           (frame.input24vMv == 24000UL) &&
           (frame.state == 2U);
}
