/**
 * @file    ESP_USER.ino
 */
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <WiFiUdp.h>
  typedef ESP8266WebServer esp_web_server_t;
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <WiFiUdp.h>
  typedef WebServer esp_web_server_t;
#else
  #error "برد ESP8266 یا ESP32 را انتخاب کنید"
#endif
#include <LittleFS.h>
#include <stdint.h>
#include <stdbool.h>
#include "simple_config.h"
#include "simple_state.h"
#include "simple_panel.h"
#include "simple_link.h"
#include "simple_auth.h"
#include "simple_theme.h"
#include "simple_diag.h"
#include "simple_http.h"
void setup(void)
{
    Serial.setRxBufferSize(SIMPLE_LINK_RX_BUFFER_SIZE);
    Serial.begin(SIMPLE_LINK_BAUD_RATE, SERIAL_8N1);
    WiFi.persistent(false);
#if defined(ESP8266)
    (void)LittleFS.setConfig(LittleFSConfig(false));
    BOOL__G__FsOk = LittleFS.begin();
#else
    BOOL__G__FsOk = LittleFS.begin(false);
#endif
    BOOL__G__FsMountFailed = (BOOL__G__FsOk == false);
    func__Simple_CryptoKeyInit();
    func__Simple_AuthLoad();
    func__Simple_RecoveryApply();
    func__Simple_ThemeLoad();
    func__Simple_WifiLoad();
    func__Simple_TimeLoad();
    func__Simple_ParamsLoad();
    func__Simple_ChargeHistoryLoad();
    func__Simple_ChartsLoad();
    func__Simple_DiagLoad();
    func__SimpleLog_Begin();
    UINT32_T__G__ChargeCountCh1 = UINT32_T__G__PrevChargeCountCh1;
    UINT32_T__G__ChargeCountCh2 = UINT32_T__G__PrevChargeCountCh2;
    func__Simple_StartSoftAp();
#if defined(ESP8266)
    WiFi.setOutputPower(SIMPLE_WIFI_TX_MAX_DBM);
#endif
    BOOL__G__WifiHighPower = true;
    SIMPLE_WEB_SERVER_T__G__Server.on("/", HTTP_GET, func__Simple_HttpRoot);
    SIMPLE_WEB_SERVER_T__G__Server.on("/t", HTTP_GET, func__Simple_HttpTelemetry);
    SIMPLE_WEB_SERVER_T__G__Server.on("/charts", HTTP_GET, func__Simple_HttpCharts);
    SIMPLE_WEB_SERVER_T__G__Server.on("/diag", HTTP_GET, func__Simple_HttpDiag);
    SIMPLE_WEB_SERVER_T__G__Server.on("/time", HTTP_POST, func__Simple_HttpTimeSync);
    SIMPLE_WEB_SERVER_T__G__Server.on("/login", HTTP_POST, func__Simple_HttpLogin);
    SIMPLE_WEB_SERVER_T__G__Server.on("/logout", HTTP_POST, func__Simple_HttpLogout);
    SIMPLE_WEB_SERVER_T__G__Server.on("/setpass", HTTP_POST, func__Simple_HttpSetPass);
    SIMPLE_WEB_SERVER_T__G__Server.on("/wifi", HTTP_GET, func__Simple_HttpGetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/wifi", HTTP_POST, func__Simple_HttpSetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/setwifi", HTTP_POST, func__Simple_HttpSetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/clear", HTTP_POST, func__Simple_HttpClearHistory);
    SIMPLE_WEB_SERVER_T__G__Server.on("/latch", HTTP_GET, func__Simple_HttpLatchState);
    SIMPLE_WEB_SERVER_T__G__Server.on("/latch/reset", HTTP_POST, func__Simple_HttpLatchReset);
    SIMPLE_WEB_SERVER_T__G__Server.on("/clock", HTTP_POST, func__Simple_HttpClockSet);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt", HTTP_GET, func__Simple_HttpEventLog);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/days", HTTP_GET, func__Simple_HttpEventDays);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/dl", HTTP_GET, func__Simple_HttpEventDownload);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/clr", HTTP_POST, func__Simple_HttpEventClear);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/snap", HTTP_POST, func__Simple_HttpEventSnap);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/format", HTTP_POST, func__Simple_HttpEventFormat);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/fs", HTTP_GET, func__Simple_HttpEventFsStatus);
    {
        static const char *char__ptr_headerKeys[] = {"Cookie", "X-Admin-Token"};
        SIMPLE_WEB_SERVER_T__G__Server.collectHeaders(char__ptr_headerKeys, 2u);
    }
    SIMPLE_WEB_SERVER_T__G__Server.begin();
}
void loop(void)
{
    func__Simple_DrainSerial();
    func__Simple_DnsTick();
    SIMPLE_WEB_SERVER_T__G__Server.handleClient();
    func__Simple_WifiPowerSaveTick((uint32_t)millis());
    func__Simple_ChartTick();
    func__Simple_SlotTick();
    func__Simple_HandshakeTick((uint32_t)millis());
    func__SimpleLog_Tick();
    static uint32_t uint32_t__lastDiagReqLoopMs = 0u;
    static bool bool__diagReqDone = false;
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((BOOL__G__TlmSeen && !bool__diagReqDone) ||
        ((uint32_t__now - uint32_t__lastDiagReqLoopMs) > 10000u))
    {
        func__Simple_SendDiagReq();
        uint32_t__lastDiagReqLoopMs = uint32_t__now;
        if (BOOL__G__TlmSeen) bool__diagReqDone = true;
    }
    static bool bool__paramsReqDone = false;
    if ((BOOL__G__TlmSeen && !bool__paramsReqDone) ||
        ((uint32_t__now - UINT32_T__G__LastParamsReqMs) > SIMPLE_PARAMS_REQ_MS))
    {
        func__Simple_SendGetParams();
        bool__paramsReqDone = true;
    }
    if (func__Simple_TimeAdoptFromBoard(UINT32_T__G__BoardTimeSec) != false)
    {
        func__Simple_TimeSave();
        BOOL__G__TimePushed = true;
    }
    if (BOOL__G__TimeValid && BOOL__G__TlmSeen && BOOL__G__DiagValid && !BOOL__G__TimePushed)
    {
        bool bool__fresh = ((uint32_t__now - UINT32_T__G__LastTlmMs) < SIMPLE_LINK_TIMEOUT_MS);
        bool bool__due = false;
        if (!bool__fresh)
        {
            UINT32_T__G__TimePushArmedMs = 0u;
        }
        else if (UINT32_T__G__TimePushArmedMs == 0u)
        {
            UINT32_T__G__TimePushArmedMs = uint32_t__now;
        }
        else if ((UINT32_T__G__TimePushMs == 0u) &&
                 ((uint32_t__now - UINT32_T__G__TimePushArmedMs) >= SIMPLE_TIME_SYNC_DELAY_MS))
        {
            bool__due = true;
        }
        else if ((UINT32_T__G__TimePushMs != 0u) &&
                 ((uint32_t__now - UINT32_T__G__TimePushMs) > SIMPLE_TIME_RESEND_MS))
        {
            bool__due = true;
        }
        if (bool__due)
        {
            func__Simple_SendTimeSync(func__Simple_NowSec());
            UINT32_T__G__TimePushMs = uint32_t__now;
        }
        if (UINT32_T__G__BoardTimeSec >= SIMPLE_TIME_VALID_MIN)
        {
            uint32_t uint32_t__diff = (UINT32_T__G__BoardTimeSec > func__Simple_NowSec())
                ? (UINT32_T__G__BoardTimeSec - func__Simple_NowSec())
                : (func__Simple_NowSec() - UINT32_T__G__BoardTimeSec);
            if (uint32_t__diff <= 120u) BOOL__G__TimePushed = true;
        }
    }
    if (!BOOL__G__SessionHadLogin && ((uint32_t)millis() > SIMPLE_RECOVERY_ARM_MS))
    {
        func__Simple_RecoveryArm();
    }
    static uint32_t uint32_t__lastSaveMs = 0u;
    if ((uint32_t__now - uint32_t__lastSaveMs) > 30000u)
    {
        if (BOOL__G__ChargeHistDirty) func__Simple_ChargeHistorySave();
        if (BOOL__G__DiagDirty) func__Simple_DiagSave();
        if (BOOL__G__BoardParamsDirty) func__Simple_ParamsSave();
        uint32_t__lastSaveMs = uint32_t__now;
    }
    static uint32_t uint32_t__lastChartSaveMs = 0u;
    uint32_t uint32_t__nowSecForChartSave = func__Simple_NowSec();
    if ((uint32_t__nowSecForChartSave >= SIMPLE_TIME_VALID_MIN) &&
        (UINT32_T__G__LastChartSaveSec == 0u))
    {
        UINT32_T__G__LastChartSaveSec = uint32_t__nowSecForChartSave;
    }
    if (((uint32_t__nowSecForChartSave >= SIMPLE_TIME_VALID_MIN) &&
         (uint32_t__nowSecForChartSave >= UINT32_T__G__LastChartSaveSec) &&
         ((uint32_t__nowSecForChartSave - UINT32_T__G__LastChartSaveSec) >= 86400u)) ||
        ((uint32_t__now - uint32_t__lastChartSaveMs) >= 86400000u))
    {
        if (BOOL__G__ChartsDirty) func__Simple_ChartsSave();
        if (BOOL__G__TimeValid) func__Simple_TimeSave();
        uint32_t__lastChartSaveMs = uint32_t__now;
        if (uint32_t__nowSecForChartSave >= SIMPLE_TIME_VALID_MIN)
        {
            UINT32_T__G__LastChartSaveSec = uint32_t__nowSecForChartSave;
        }
    }
    if (BOOL__G__WifiRebootPending)
    {
        if ((uint32_t__now - UINT32_T__G__WifiRebootMs) > 2000u)
        {
            if (BOOL__G__ChargeHistDirty) func__Simple_ChargeHistorySave();
            if (BOOL__G__ChartsDirty) func__Simple_ChartsSave();
            if (BOOL__G__DiagDirty) func__Simple_DiagSave();
            if (BOOL__G__BoardParamsDirty) func__Simple_ParamsSave();
#if defined(ESP8266)
            ESP.restart();
#else
            ESP.restart();
#endif
        }
    }
}
