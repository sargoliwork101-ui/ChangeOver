/**
 * @file ESP_Crc.cpp
 * @brief Arduino ESP8266 sample for ESP_Crc.
 * @details This file belongs to the ESP8266 support module layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include "ESP_Crc.h"

/**
 * @brief Implements the module operation represented by this API.
 * @function espCrc16
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint16_t espCrc16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    if (data == nullptr)
    {
        return crc;
    }
    for (size_t i = 0U; i < length; ++i)
    {
        crc ^= static_cast<uint16_t>(data[i]) << 8U;
        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 0x8000U) != 0U
                    ? static_cast<uint16_t>((crc << 1U) ^ 0x1021U)
                    : static_cast<uint16_t>(crc << 1U);
        }
    }
    return crc;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function espCrc16Text
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint16_t espCrc16Text(const char* text, size_t length)
{
    return espCrc16(reinterpret_cast<const uint8_t*>(text), length);
}
