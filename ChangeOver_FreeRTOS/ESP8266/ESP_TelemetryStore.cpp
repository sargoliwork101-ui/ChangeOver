/**
 * @file ESP_TelemetryStore.cpp
 * @brief Arduino ESP8266 sample for ESP_TelemetryStore.
 * @details This file belongs to the ESP8266 persistent/in-memory data store layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include "ESP_TelemetryStore.h"

#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_TELEMETRY_STORAGE
#include <LittleFS.h>
#endif

namespace
{
static const uint32_t TelemetryMagic = 0x4348544CUL;
}
/**
 * @brief Constructs the module context with safe default state.
 * @function EspTelemetryStore::EspTelemetryStore
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspTelemetryStore::EspTelemetryStore()
    : records_{}, head_(0U), count_(0U), pendingWrites_(0U), lastFlushMs_(0UL), dirty_(false)
{
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspTelemetryStore::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspTelemetryStore::begin()
{
    head_ = 0U;
    count_ = 0U;
    pendingWrites_ = 0U;
    lastFlushMs_ = millis();
    dirty_ = false;
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_TELEMETRY_STORAGE
    if (!LittleFS.exists(EspConfig::TelemetryFile))
    {
        return true;
    }
    File file = LittleFS.open(EspConfig::TelemetryFile, "r");
    if (!file)
    {
        return false;
    }
    uint32_t magic = 0UL;
    uint16_t storedCount = 0U;
    uint16_t storedHead = 0U;
    const size_t headerRead = file.read(reinterpret_cast<uint8_t*>(&magic), sizeof(magic)) +
                              file.read(reinterpret_cast<uint8_t*>(&storedCount), sizeof(storedCount)) +
                              file.read(reinterpret_cast<uint8_t*>(&storedHead), sizeof(storedHead));
    const size_t dataRead = file.read(reinterpret_cast<uint8_t*>(records_), sizeof(records_));
    file.close();
    if ((headerRead != (sizeof(magic) + sizeof(storedCount) + sizeof(storedHead))) ||
        (dataRead != sizeof(records_)) || (magic != TelemetryMagic) ||
        (storedCount > EspConfig::MaxTelemetryRecords) ||
        (storedHead >= EspConfig::MaxTelemetryRecords))
    {
        return false;
    }
    count_ = storedCount;
    head_ = storedHead;
#endif
    return true;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspTelemetryStore::append
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspTelemetryStore::append(const EspTelemetryRecord& record)
{
    uint16_t index;
    if (count_ < EspConfig::MaxTelemetryRecords)
    {
        index = static_cast<uint16_t>((head_ + count_) % EspConfig::MaxTelemetryRecords);
        count_++;
    }
    else
    {
        index = head_;
        head_ = static_cast<uint16_t>((head_ + 1U) % EspConfig::MaxTelemetryRecords);
    }
    records_[index] = record;
    pendingWrites_++;
    dirty_ = true;
    if ((pendingWrites_ >= EspConfig::StorageFlushEventCount) ||
        ((millis() - lastFlushMs_) >= EspConfig::StorageFlushPeriodMs))
    {
        return flush();
    }
    return true;
}

/**
 * @brief Clears the selected volatile or persistent data.
 * @function EspTelemetryStore::clear
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspTelemetryStore::clear()
{
    head_ = 0U;
    count_ = 0U;
    pendingWrites_ = 0U;
    dirty_ = false;
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_TELEMETRY_STORAGE
    if (LittleFS.exists(EspConfig::TelemetryFile))
    {
        return LittleFS.remove(EspConfig::TelemetryFile);
    }
#endif
    return true;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspTelemetryStore::flush
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspTelemetryStore::flush()
{
    if (!dirty_)
    {
        return true;
    }
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_TELEMETRY_STORAGE
    File file = LittleFS.open(EspConfig::TelemetryFile, "w");
    if (!file)
    {
        return false;
    }
    const uint32_t magic = TelemetryMagic;
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(&magic), sizeof(magic)) +
                           file.write(reinterpret_cast<const uint8_t*>(&count_), sizeof(count_)) +
                           file.write(reinterpret_cast<const uint8_t*>(&head_), sizeof(head_)) +
                           file.write(reinterpret_cast<const uint8_t*>(records_), sizeof(records_));
    file.close();
    if (written != (sizeof(magic) + sizeof(count_) + sizeof(head_) + sizeof(records_)))
    {
        return false;
    }
#endif
    pendingWrites_ = 0U;
    lastFlushMs_ = millis();
    dirty_ = false;
    return true;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspTelemetryStore::physicalIndex
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint16_t EspTelemetryStore::physicalIndex(uint16_t logicalIndex) const
{
    return static_cast<uint16_t>((head_ + logicalIndex) % EspConfig::MaxTelemetryRecords);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspTelemetryStore::count
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint16_t EspTelemetryStore::count() const
{
    return count_;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspTelemetryStore::get
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspTelemetryStore::get(uint16_t index, EspTelemetryRecord& record) const
{
    if (index >= count_)
    {
        return false;
    }
    record = records_[physicalIndex(index)];
    return true;
}

/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function EspTelemetryStore::toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspTelemetryStore::toJson(uint16_t maxRecords) const
{
    const uint16_t returned = (count_ < maxRecords) ? count_ : maxRecords;
    const uint16_t start = (count_ > returned) ?
        static_cast<uint16_t>(count_ - returned) : 0U;
    String json;
    json.reserve(8192U);
    json += F("{\"count\":");
    json += count_;
    json += F(",\"returned\":");
    json += returned;
    json += F(",\"truncated\":");
    json += (count_ > returned) ? F("true") : F("false");
    json += F(",\"records\":[");
    for (uint16_t i = start; i < count_; ++i)
    {
        if (i > start)
        {
            json += ',';
        }
        const EspTelemetryRecord& record = records_[physicalIndex(i)];
        json += F("{\"input24vMv\":");
        json += record.input24vMv;
        json += F(",\"battery24vMv\":");
        json += record.battery24vMv;
        json += F(",\"battery12vMv\":");
        json += record.battery12vMv;
        json += F(",\"current1Ma\":");
        json += record.current1Ma;
        json += F(",\"current2Ma\":");
        json += record.current2Ma;
        json += F(",\"state\":");
        json += record.state;
        json += F(",\"faultMask\":");
        json += record.faultMask;
        json += F(",\"timestampMs\":");
        json += record.timestampMs;
        json += '}';
    }
    json += F("]}");
    return json;
}
