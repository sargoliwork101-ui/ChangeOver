/**
 * @file ESP_DiagnosticsStore.cpp
 * @brief Arduino ESP8266 sample for ESP_DiagnosticsStore.
 * @details This file belongs to the ESP8266 persistent/in-memory data store layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include "ESP_DiagnosticsStore.h"

#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_DIAGNOSTICS_STORAGE
#include <LittleFS.h>
#endif

namespace
{
static const uint32_t StoreMagic = 0x43484447UL;
static const uint16_t StoreVersion = 1U;

struct StoreHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t head;
    uint16_t count;
    uint16_t reserved;
};
}
/**
 * @brief Constructs the module context with safe default state.
 * @function EspDiagnosticsStore::EspDiagnosticsStore
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspDiagnosticsStore::EspDiagnosticsStore()
    : records_{},
      head_(0U),
      count_(0U),
      pendingWrites_(0U),
      lastFlushMs_(0UL),
      dirty_(false)
{
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::resetMemory
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspDiagnosticsStore::resetMemory()
{
    head_ = 0U;
    count_ = 0U;
    pendingWrites_ = 0U;
    lastFlushMs_ = millis();
    dirty_ = false;
    for (uint16_t i = 0U; i < EspConfig::MaxDiagnosticsRecords; ++i)
    {
        records_[i] = {};
    }
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspDiagnosticsStore::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::begin()
{
    resetMemory();
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_DIAGNOSTICS_STORAGE
    return load();
#else
    return true;
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::load
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::load()
{
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_DIAGNOSTICS_STORAGE
    if (!LittleFS.exists(EspConfig::DiagnosticsFile))
    {
        return true;
    }
    File file = LittleFS.open(EspConfig::DiagnosticsFile, "r");
    if (!file)
    {
        return false;
    }
    StoreHeader header{};
    const size_t headerRead = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header));
    if ((headerRead != sizeof(header)) || (header.magic != StoreMagic) ||
        (header.version != StoreVersion) ||
        (header.count > EspConfig::MaxDiagnosticsRecords) ||
        (header.head >= EspConfig::MaxDiagnosticsRecords))
    {
        file.close();
        resetMemory();
        return false;
    }
    const size_t dataRead = file.read(reinterpret_cast<uint8_t*>(records_), sizeof(records_));
    file.close();
    if (dataRead != sizeof(records_))
    {
        resetMemory();
        return false;
    }
    head_ = header.head;
    count_ = header.count;
    dirty_ = false;
    return true;
#else
    return true;
#endif
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::append
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::append(const EspDiagnosticRecord& record)
{
    uint16_t index;
    if (count_ < EspConfig::MaxDiagnosticsRecords)
    {
        index = static_cast<uint16_t>((head_ + count_) % EspConfig::MaxDiagnosticsRecords);
        count_++;
    }
    else
    {
        index = head_;
        head_ = static_cast<uint16_t>((head_ + 1U) % EspConfig::MaxDiagnosticsRecords);
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
 * @function EspDiagnosticsStore::clear
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::clear()
{
    resetMemory();
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_DIAGNOSTICS_STORAGE
    if (LittleFS.exists(EspConfig::DiagnosticsFile))
    {
        return LittleFS.remove(EspConfig::DiagnosticsFile);
    }
#endif
    return true;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::flush
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::flush()
{
    if (!dirty_)
    {
        return true;
    }
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_DIAGNOSTICS_STORAGE
    File file = LittleFS.open(EspConfig::DiagnosticsFile, "w");
    if (!file)
    {
        return false;
    }
    StoreHeader header{StoreMagic, StoreVersion, head_, count_, 0U};
    const size_t headerWritten = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    const size_t dataWritten = file.write(reinterpret_cast<const uint8_t*>(records_), sizeof(records_));
    file.close();
    if ((headerWritten != sizeof(header)) || (dataWritten != sizeof(records_)))
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
 * @function EspDiagnosticsStore::physicalIndex
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint16_t EspDiagnosticsStore::physicalIndex(uint16_t logicalIndex) const
{
    return static_cast<uint16_t>((head_ + logicalIndex) % EspConfig::MaxDiagnosticsRecords);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::count
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
uint16_t EspDiagnosticsStore::count() const
{
    return count_;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::get
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::get(uint16_t index, EspDiagnosticRecord& record) const
{
    if (index >= count_)
    {
        return false;
    }
    record = records_[physicalIndex(index)];
    return true;
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspDiagnosticsStore::getLast
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
bool EspDiagnosticsStore::getLast(EspDiagnosticRecord& record) const
{
    if (count_ == 0U)
    {
        return false;
    }
    return get(static_cast<uint16_t>(count_ - 1U), record);
}

/**
 * @brief Builds a bounded diagnostic or Web representation.
 * @function EspDiagnosticsStore::toJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
String EspDiagnosticsStore::toJson(uint16_t maxRecords) const
{
    const uint16_t returned = (count_ < maxRecords) ? count_ : maxRecords;
    const uint16_t start = (count_ > returned) ?
        static_cast<uint16_t>(count_ - returned) : 0U;
    String json;
    json.reserve(4096U);
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
        const EspDiagnosticRecord& record = records_[physicalIndex(i)];
        json += F("{\"code\":");
        json += record.code;
        json += F(",\"severity\":");
        json += record.severity;
        json += F(",\"value\":");
        json += record.value;
        json += F(",\"faultMask\":");
        json += record.faultMask;
        json += F(",\"state\":");
        json += record.state;
        json += F(",\"occurrenceCount\":");
        json += record.occurrenceCount;
        json += F(",\"timestampMs\":");
        json += record.timestampMs;
        json += '}';
    }
    json += F("]}");
    return json;
}
