/**
 * @file ESP_RuntimeStats.cpp
 * @brief Runtime counters, charge-session metrics and optional persistence.
 * @details Counters are kept in RAM by default. Runtime persistence is an
 *          independent feature gate so Flash wear can be tested separately.
 * @safety Persistent writes are batched and never occur from an ISR.
 * @stage Enable ESP_FEATURE_RUNTIME_STORAGE only after LittleFS bring-up.
 */

#include "ESP_RuntimeStats.h"
#include "ESP_Config.h"
#include "ESP_FeatureFlags.h"

#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_RUNTIME_STORAGE
#include <LittleFS.h>
#endif

namespace
{
static const uint32_t RuntimeMagic = 0x43485254UL;
static const uint16_t RuntimeVersion = 1U;
struct RuntimeHeader
{
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t bootCount;
    uint32_t chargeSessionCount;
    uint32_t chargeSeconds;
};
}

/**
 * @brief Constructs the runtime statistics context.
 * @function EspRuntimeStats::EspRuntimeStats
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */
EspRuntimeStats::EspRuntimeStats()
    : stats_{}, lastChargeUpdateMs_(0UL), lastPersistentFlushMs_(0UL), dirty_(false)
{
}

/**
 * @brief Initializes counters and optionally restores persistent counters.
 * @function EspRuntimeStats::begin
 * @safety Invalid persistent data falls back to deterministic zero-based state.
 * @stage Runtime persistence is controlled by ESP_FEATURE_RUNTIME_STORAGE.
 */
void EspRuntimeStats::begin()
{
    stats_ = {};
    stats_.lastState = 0U;
    stats_.stmOnline = false;
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_RUNTIME_STORAGE
    (void)loadPersistent();
#endif
    stats_.bootCount++;
    lastChargeUpdateMs_ = millis();
    lastPersistentFlushMs_ = lastChargeUpdateMs_;
    dirty_ = true;
    (void)flushPersistent();
}

/**
 * @brief Restores boot and charge counters from LittleFS.
 * @function EspRuntimeStats::loadPersistent
 * @return true when the file is valid or persistence is disabled.
 * @safety Corrupt files are ignored without entering a reset loop.
 * @stage Called only during boot before normal Web/API operation.
 */
bool EspRuntimeStats::loadPersistent()
{
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_RUNTIME_STORAGE
    if (!LittleFS.exists(EspConfig::RuntimeStatsFile))
    {
        return true;
    }
    File file = LittleFS.open(EspConfig::RuntimeStatsFile, "r");
    if (!file)
    {
        return false;
    }
    RuntimeHeader header{};
    const size_t readCount = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header));
    file.close();
    if ((readCount != sizeof(header)) || (header.magic != RuntimeMagic) ||
        (header.version != RuntimeVersion))
    {
        return false;
    }
    stats_.bootCount = header.bootCount;
    stats_.chargeSessionCount = header.chargeSessionCount;
    stats_.chargeSeconds = header.chargeSeconds;
#endif
    return true;
}

/**
 * @brief Flushes persistent runtime counters in one bounded write.
 * @function EspRuntimeStats::flushPersistent
 * @return true when persistence is disabled or the write succeeds.
 * @safety The function is task-context only and is called outside ISR paths.
 * @stage Flash writes are disabled until ESP_FEATURE_RUNTIME_STORAGE is enabled.
 */
bool EspRuntimeStats::flushPersistent()
{
#if ESP_FEATURE_LITTLEFS && ESP_FEATURE_RUNTIME_STORAGE
    if (!dirty_)
    {
        return true;
    }
    File file = LittleFS.open(EspConfig::RuntimeStatsFile, "w");
    if (!file)
    {
        return false;
    }
    RuntimeHeader header{RuntimeMagic, RuntimeVersion, 0U,
                         stats_.bootCount, stats_.chargeSessionCount,
                         stats_.chargeSeconds};
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    file.close();
    if (written != sizeof(header))
    {
        return false;
    }
#endif
    dirty_ = false;
    lastPersistentFlushMs_ = millis();
    return true;
}

/**
 * @brief Records one valid STM32 frame.
 * @function EspRuntimeStats::onValidFrame
 * @safety Only bounded counters and timestamps are modified.
 * @stage Works with Legacy and Handshake link modes.
 */
void EspRuntimeStats::onValidFrame()
{
    stats_.validFrames++;
    stats_.lastStmFrameMs = millis();
    stats_.stmOnline = true;
}

/**
 * @brief Records one invalid STM32 frame.
 * @function EspRuntimeStats::onInvalidFrame
 * @safety No parser recovery or reset is attempted here.
 * @stage The Web Debugger exposes the counter for diagnosis.
 */
void EspRuntimeStats::onInvalidFrame()
{
    stats_.invalidFrames++;
}

/**
 * @brief Records one diagnostic frame code.
 * @function EspRuntimeStats::onDiagnostic
 * @param code Diagnostic code received from STM32.
 * @safety The code is copied without dynamic allocation.
 * @stage The full record is handled by EspDiagnosticsStore.
 */
void EspRuntimeStats::onDiagnostic(uint32_t code)
{
    stats_.diagnosticFrames++;
    stats_.lastDiagnosticCode = code;
}

/**
 * @brief Updates charge-session counters from a telemetry state.
 * @function EspRuntimeStats::onTelemetry
 * @param state STM32 system state.
 * @param faultMask STM32 fault mask.
 * @safety Values are observational and never control STM32 outputs.
 * @stage Charge state code 4 is the current CHARGING contract.
 */
void EspRuntimeStats::onTelemetry(uint8_t state, uint32_t faultMask)
{
    const uint32_t now = millis();
    const bool wasCharging = stats_.lastState == 4U;
    const bool isCharging = state == 4U;

    if ((!wasCharging) && isCharging)
    {
        stats_.chargeSessionCount++;
        dirty_ = true;
    }
    if (wasCharging && isCharging)
    {
        const uint32_t elapsed = now - lastChargeUpdateMs_;
        stats_.chargeSeconds += elapsed / 1000UL;
        dirty_ = true;
    }
    stats_.lastState = state;
    stats_.lastFaultMask = faultMask;
    lastChargeUpdateMs_ = now;
}

/**
 * @brief Records one Web API request.
 * @function EspRuntimeStats::onWebRequest
 * @safety Counter-only operation.
 * @stage Used to assess Web Server activity during bring-up.
 */
void EspRuntimeStats::onWebRequest()
{
    stats_.webRequests++;
}

/**
 * @brief Performs periodic batched persistence and link-age maintenance.
 * @function EspRuntimeStats::loop
 * @safety Non-blocking in normal operation; a bounded LittleFS flush may occur.
 * @stage Call once from the ESP main loop.
 */
void EspRuntimeStats::loop()
{
    if (stats_.stmOnline &&
        ((millis() - stats_.lastStmFrameMs) > EspConfig::StmDataTimeoutMs))
    {
        stats_.stmOnline = false;
    }
    if (dirty_ && ((millis() - lastPersistentFlushMs_) >= EspConfig::StorageFlushPeriodMs))
    {
        (void)flushPersistent();
    }
}

/**
 * @brief Returns a value copy of runtime statistics.
 * @function EspRuntimeStats::snapshot
 * @return Current runtime statistics.
 * @safety No internal storage is exposed.
 * @stage Safe for Web Model reads.
 */
EspRuntimeStatsSnapshot EspRuntimeStats::snapshot() const
{
    return stats_;
}

/**
 * @brief Builds the runtime statistics JSON object.
 * @function EspRuntimeStats::toJson
 * @return JSON representation for the Web API.
 * @safety Output is bounded by reserved String capacity and fixed fields.
 * @stage No network operation is performed here.
 */
String EspRuntimeStats::toJson() const
{
    String json;
    json.reserve(360U);
    json += F("{\"bootCount\":");
    json += stats_.bootCount;
    json += F(",\"validFrames\":");
    json += stats_.validFrames;
    json += F(",\"invalidFrames\":");
    json += stats_.invalidFrames;
    json += F(",\"diagnosticFrames\":");
    json += stats_.diagnosticFrames;
    json += F(",\"webRequests\":");
    json += stats_.webRequests;
    json += F(",\"chargeSessionCount\":");
    json += stats_.chargeSessionCount;
    json += F(",\"chargeSeconds\":");
    json += stats_.chargeSeconds;
    json += F(",\"lastStmFrameMs\":");
    json += stats_.lastStmFrameMs;
    json += F(",\"lastDiagnosticCode\":");
    json += stats_.lastDiagnosticCode;
    json += F(",\"lastState\":");
    json += stats_.lastState;
    json += F(",\"lastFaultMask\":");
    json += stats_.lastFaultMask;
    json += F(",\"stmOnline\":");
    json += stats_.stmOnline ? F("true}") : F("false}");
    return json;
}
