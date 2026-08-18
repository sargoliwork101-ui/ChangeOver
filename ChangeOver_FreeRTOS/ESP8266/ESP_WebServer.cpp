/**
 * @file ESP_WebServer.cpp
 * @brief Arduino ESP8266 sample for ESP_WebServer.
 * @details This file belongs to the ESP8266 Web Server and UI integration layer.
 * Features are compile-time gated in ESP_FeatureFlags.h and must be enabled
 * one phase at a time. This code is Embedded Safe C++; MISRA C applies to
 * the STM32 C firmware, while this Arduino layer is kept deterministic and
 * bounded where the Arduino framework permits it.
 * @safety No power-control command is enabled by default. Storage and network
 * failures must not cause an uncontrolled reset loop.
 * @note Update README_ESP8266.md and PROJECT_HISTORY.md for behavior changes.
 */

#include "ESP_WebServer.h"
#include "ESP_Config.h"
#include "ESP_FeatureFlags.h"
#include "ESP_WebPages.h"

#if ESP_FEATURE_LITTLEFS
#include <LittleFS.h>
#endif

EspWebServer* EspWebServer::active_ = nullptr;
/**
 * @brief Constructs the module context with safe default state.
 * @function EspWebServer::EspWebServer
 * @safety Construction performs no power control and no unbounded allocation.
 * @stage Runtime services are enabled later by begin/setup functions.
 */

EspWebServer::EspWebServer(ESP8266WebServer& server,
                           EspWebModel& model,
                           EspRuntimeStats& stats,
                           EspDiagnosticsStore& diagnostics,
                           EspTelemetryStore& telemetry,
                           EspAutoTest& autoTest)
    : server_(server), model_(model), stats_(stats), diagnostics_(diagnostics),
      telemetry_(telemetry), autoTest_(autoTest)
{
}

/**
 * @brief Initializes the module state and dependencies.
 * @function EspWebServer::begin
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::begin()
{
#if ESP_FEATURE_WEB_SERVER
    active_ = this;
    server_.on("/", HTTP_GET, EspWebServer::handleDashboard);
    server_.on("/debug", HTTP_GET, EspWebServer::handleDebugPage);
    server_.on("/charts", HTTP_GET, EspWebServer::handleChartsPage);
    server_.on("/test", HTTP_GET, EspWebServer::handleTestPage);
    server_.on("/settings", HTTP_GET, EspWebServer::handleSettingsPage);
    server_.on("/api/health", HTTP_GET, EspWebServer::handleHealthApi);
    server_.on("/api/status", HTTP_GET, EspWebServer::handleStatusApi);
    server_.on("/api/diagnostics", HTTP_GET, EspWebServer::handleDiagnosticsApi);
    server_.on("/api/diagnostics/clear", HTTP_POST, EspWebServer::handleDiagnosticsClearApi);
    server_.on("/api/telemetry", HTTP_GET, EspWebServer::handleTelemetryApi);
    server_.on("/api/features", HTTP_GET, EspWebServer::handleFeaturesApi);
    server_.on("/api/test/status", HTTP_GET, EspWebServer::handleTestStatusApi);
    server_.on("/api/test/start", HTTP_POST, EspWebServer::handleTestStartApi);
    server_.on("/api/test/stop", HTTP_POST, EspWebServer::handleTestStopApi);
    server_.onNotFound(EspWebServer::handleNotFound);
    server_.begin();
#endif
}

/**
 * @brief Runs one non-blocking ESP service cycle.
 * @function EspWebServer::loop
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::loop()
{
#if ESP_FEATURE_WEB_SERVER
    server_.handleClient();
#endif
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleDashboard
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleDashboard()
{
    if (active_ != nullptr) active_->servePage(EspConfig::DashboardPage);
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleDebugPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleDebugPage()
{
    if (active_ != nullptr) active_->servePage(EspConfig::DebugPage);
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleChartsPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleChartsPage()
{
    if (active_ != nullptr) active_->servePage(EspConfig::ChartsPage);
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleTestPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleTestPage()
{
    if (active_ != nullptr) active_->servePage(EspConfig::TestPage);
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleSettingsPage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleSettingsPage()
{
    if (active_ != nullptr) active_->servePage(EspConfig::SettingsPage);
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleHealthApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleHealthApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->sendJson(active_->model_.healthJson());
    }
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleStatusApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleStatusApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->sendJson(active_->model_.statusJson());
    }
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleDiagnosticsApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleDiagnosticsApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->sendJson(active_->model_.diagnosticsJson());
    }
}

/**
 * @brief Clears the selected volatile or persistent data.
 * @function EspWebServer::handleDiagnosticsClearApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleDiagnosticsClearApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        const bool cleared = active_->diagnostics_.clear();
        active_->sendJson(cleared ? F("{\"ok\":true}") : F("{\"ok\":false}"));
    }
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleTelemetryApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleTelemetryApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->sendJson(active_->model_.telemetryJson());
    }
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleFeaturesApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleFeaturesApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->sendJson(active_->model_.featuresJson());
    }
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleTestStatusApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleTestStatusApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->sendJson(active_->model_.testJson());
    }
}

/**
 * @brief Starts the requested staged operation.
 * @function EspWebServer::handleTestStartApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleTestStartApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        const bool started = active_->autoTest_.start(false);
        active_->sendJson(started ? F("{\"ok\":true}") : F("{\"ok\":false}"));
    }
}

/**
 * @brief Stops the requested staged operation.
 * @function EspWebServer::handleTestStopApi
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleTestStopApi()
{
    if (active_ != nullptr)
    {
        active_->stats_.onWebRequest();
        active_->autoTest_.stop();
        active_->sendJson(F("{\"ok\":true}"));
    }
}

/**
 * @brief Handles one Web Server route or protocol event.
 * @function EspWebServer::handleNotFound
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::handleNotFound()
{
    if (active_ != nullptr)
    {
        active_->servePage(active_->server_.uri());
    }
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebServer::servePage
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::servePage(const String& path)
{
#if ESP_FEATURE_LITTLEFS
    if (LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "r");
        if (file)
        {
            server_.streamFile(file, EspWebPages::contentType(path));
            file.close();
            return;
        }
    }
#endif
    server_.send(200, EspWebPages::contentType(path), EspWebPages::fallback(path));
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebServer::sendJson
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::sendJson(const String& json)
{
    server_.send(200, "application/json; charset=utf-8", json);
}

/**
 * @brief Implements the module operation represented by this API.
 * @function EspWebServer::sendText
 * @safety Buffers, filesystem writes and network operations must remain bounded.
 * @debug Failures are surfaced through the Web API or diagnostic counters.
 * @stage The operation is controlled by ESP_FeatureFlags.h where applicable.
 */
void EspWebServer::sendText(uint16_t code, const String& text)
{
    server_.send(code, "text/plain; charset=utf-8", text);
}
