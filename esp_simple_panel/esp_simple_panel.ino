/**
 * @file    esp_simple_panel.ino
 * @brief   پنل ساده کاربر - فقط نمایش وضعیت، بدون تنظیمات فنی
 *          برای همان ESP8266 و همان برد ChangeOver
 *          وای‌فای: changeover_userpannel / 123456789
 *          آدرس: http://192.168.4.1
 *          فقط تله‌متری می‌خواند، چیزی به برد نمی‌فرستد
 *          نمایش: باتری موبایل‌طور، دو نیم‌باتری، وضعیت برق شهر، چراغ‌ها، پیغام ساده فارسی
 */

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer esp_web_server_t;
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  typedef WebServer esp_web_server_t;
#else
  #error "برد ESP8266 یا ESP32 را انتخاب کنید"
#endif

#include <LittleFS.h>
#include <stdint.h>
#include <stdbool.h>

/* ترتیب include مهم است - تک واحد ترجمه */
#include "simple_config.h"
#include "simple_state.h"
#include "simple_panel.h"
#include "simple_link.h"
#include "simple_auth.h"
#include "simple_theme.h"
#include "simple_diag.h"
#include "simple_http.h"

/* ==================== راه‌اندازی ==================== */
void setup(void)
{
    Serial.setRxBufferSize(SIMPLE_LINK_RX_BUFFER_SIZE);
    Serial.begin(SIMPLE_LINK_BAUD_RATE, SERIAL_8N1);

    WiFi.mode(WIFI_AP);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    BOOL__G__FsOk = LittleFS.begin();
#else
    WiFi.setSleep(false);
    BOOL__G__FsOk = LittleFS.begin(true);
#endif

    /* کلید رمزنگاری از شناسهٔ چیپ ساخته می‌شود - پیش از هر خواندن فایل حساس */
    func__Simple_CryptoKeyInit();
    func__Simple_AuthLoad();
    func__Simple_ThemeLoad();
    func__Simple_WifiLoad();
    func__Simple_TimeLoad();          /* ساعت ذخیره‌شده - دیگر پرسیده نمی‌شود */
    func__Simple_ParamsLoad();        /* پارامترهای خود برد از آخرین بار */
    func__Simple_ChargeHistoryLoad();
    func__Simple_ChartsLoad();        /* هر چهار بازه */
    func__Simple_DiagLoad();

    // بازیابی prev counts برای تشخیص شارژ بعد از ریستارت
    UINT32_T__G__PrevChargeCountCh1 = UINT32_T__G__ChargeCountCh1;
    UINT32_T__G__PrevChargeCountCh2 = UINT32_T__G__ChargeCountCh2;

    (void)WiFi.softAP(CHAR__G__WifiSsid, CHAR__G__WifiPass);

    SIMPLE_WEB_SERVER_T__G__Server.on("/", HTTP_GET, func__Simple_HttpRoot);
    SIMPLE_WEB_SERVER_T__G__Server.on("/t", HTTP_GET, func__Simple_HttpTelemetry);
    SIMPLE_WEB_SERVER_T__G__Server.on("/charts", HTTP_GET, func__Simple_HttpCharts);
    SIMPLE_WEB_SERVER_T__G__Server.on("/diag", HTTP_GET, func__Simple_HttpDiag);
    SIMPLE_WEB_SERVER_T__G__Server.on("/time", HTTP_POST, func__Simple_HttpTimeSync);
    SIMPLE_WEB_SERVER_T__G__Server.on("/login", HTTP_POST, func__Simple_HttpLogin);
    SIMPLE_WEB_SERVER_T__G__Server.on("/logout", HTTP_POST, func__Simple_HttpLogout);
    SIMPLE_WEB_SERVER_T__G__Server.on("/users", HTTP_GET, func__Simple_HttpUsers);
    SIMPLE_WEB_SERVER_T__G__Server.on("/users", HTTP_POST, func__Simple_HttpUsersPost);
    SIMPLE_WEB_SERVER_T__G__Server.on("/setpass", HTTP_POST, func__Simple_HttpSetPass);   /* تغییر رمز عبور */
    SIMPLE_WEB_SERVER_T__G__Server.on("/adduser", HTTP_POST, func__Simple_HttpAddUser);
    SIMPLE_WEB_SERVER_T__G__Server.on("/deluser", HTTP_POST, func__Simple_HttpDelUser);
    SIMPLE_WEB_SERVER_T__G__Server.on("/theme", HTTP_GET, func__Simple_HttpGetTheme);
    SIMPLE_WEB_SERVER_T__G__Server.on("/settheme", HTTP_POST, func__Simple_HttpSetTheme);
    SIMPLE_WEB_SERVER_T__G__Server.on("/wifi", HTTP_GET, func__Simple_HttpGetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/wifi", HTTP_POST, func__Simple_HttpSetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/setwifi", HTTP_POST, func__Simple_HttpSetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/clear", HTTP_POST, func__Simple_HttpClearHistory);
    SIMPLE_WEB_SERVER_T__G__Server.begin();
}

/* ==================== حلقه اصلی - با ذخیره‌سازی پایدار و ریبوت وای‌فای و تیک نمودار مستقل و دیاگ ==================== */
void loop(void)
{
    func__Simple_DrainSerial();
    SIMPLE_WEB_SERVER_T__G__Server.handleClient();
    func__Simple_ChartTick();

    // درخواست دیاگ مستقل هر ۱۰ ثانیه - حتی اگر تب دیاگ باز نباشد
    static uint32_t uint32_t__lastDiagReqLoopMs = 0u;
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((uint32_t__now - uint32_t__lastDiagReqLoopMs) > 10000u)
    {
        func__Simple_SendDiagReq();
        uint32_t__lastDiagReqLoopMs = uint32_t__now;
    }

    // خواندن پارامترهای خود برد (درصد باتری و چشمک‌ها) - اول بار بعد از آمدن لینک
    static bool bool__paramsReqDone = false;
    if ((BOOL__G__TlmSeen && !bool__paramsReqDone) ||
        ((uint32_t__now - UINT32_T__G__LastParamsReqMs) > SIMPLE_PARAMS_REQ_MS))
    {
        func__Simple_SendGetParams();
        bool__paramsReqDone = true;
    }

    /* ساعت: RTC خود برد منبع دقیق‌تری است (پنل بین دو روشن‌شدن خواب است و
       ساعتش عقب می‌ماند). اگر پنل ساعت نداشت یا با ساعت برد بیشتر از
       SIMPLE_TIME_ADOPT_DIFF_SEC اختلاف داشت، ساعت برد را برمی‌داریم و ذخیره
       می‌کنیم؛ این‌طور دیگر هیچ‌وقت از کاربر پرسیده نمی‌شود. */
    if (UINT32_T__G__BoardTimeSec >= SIMPLE_TIME_VALID_MIN)
    {
        bool bool__adopt = !BOOL__G__TimeValid;
        if (BOOL__G__TimeValid)
        {
            uint32_t uint32_t__panelSec = func__Simple_NowSec();
            uint32_t uint32_t__gap = (UINT32_T__G__BoardTimeSec > uint32_t__panelSec)
                ? (UINT32_T__G__BoardTimeSec - uint32_t__panelSec)
                : (uint32_t__panelSec - UINT32_T__G__BoardTimeSec);
            if (uint32_t__gap > SIMPLE_TIME_ADOPT_DIFF_SEC) bool__adopt = true;
        }
        if (bool__adopt)
        {
            func__Simple_TimeSet(UINT32_T__G__BoardTimeSec, false);
            func__Simple_TimeSave();
            BOOL__G__TimePushed = true;   /* ساعت از خود برد گرفته شد - نیازی به فرستادن نیست */
        }
    }
    if (BOOL__G__TimeValid && BOOL__G__TlmSeen && !BOOL__G__TimePushed)
    {
        bool bool__fresh = ((uint32_t__now - UINT32_T__G__LastTlmMs) < SIMPLE_LINK_TIMEOUT_MS);
        bool bool__due = false;

        if (!bool__fresh)
        {
            /* لینک نبود؛ دفعهٔ بعد که آمد تأخیر از نو شمرده می‌شود */
            UINT32_T__G__TimePushArmedMs = 0u;
        }
        else if (UINT32_T__G__TimePushArmedMs == 0u)
        {
            UINT32_T__G__TimePushArmedMs = uint32_t__now;   /* شروع تأخیر */
        }
        else if ((UINT32_T__G__TimePushMs == 0u) &&
                 ((uint32_t__now - UINT32_T__G__TimePushArmedMs) >= SIMPLE_TIME_SYNC_DELAY_MS))
        {
            /* لینک چند ثانیه پایدار ماند - اولین ارسال */
            bool__due = true;
        }
        else if ((UINT32_T__G__TimePushMs != 0u) &&
                 ((uint32_t__now - UINT32_T__G__TimePushMs) > SIMPLE_TIME_RESEND_MS))
        {
            bool__due = true;                               /* تلاش دوبارهٔ دوره‌ای */
        }

        if (bool__due)
        {
            func__Simple_SendTimeSync(func__Simple_NowSec());
            UINT32_T__G__TimePushMs = uint32_t__now;
        }
        /* برد وقتی ساعت را گرفت، در دیاگ خودش همان را گزارش می‌کند */
        if (UINT32_T__G__BoardTimeSec >= SIMPLE_TIME_VALID_MIN)
        {
            uint32_t uint32_t__diff = (UINT32_T__G__BoardTimeSec > func__Simple_NowSec())
                ? (UINT32_T__G__BoardTimeSec - func__Simple_NowSec())
                : (func__Simple_NowSec() - UINT32_T__G__BoardTimeSec);
            if (uint32_t__diff <= 120u) BOOL__G__TimePushed = true;
        }
    }

    // ذخیره دوره‌ای تاریخچه شارژ و نمودارها و دیاگ هر ۳۰ ثانیه
    // هر کدام فقط اگر عوض شده باشد نوشته می‌شود (کم‌کردن فرسودگی فلش)
    static uint32_t uint32_t__lastSaveMs = 0u;
    if ((uint32_t__now - uint32_t__lastSaveMs) > 30000u)
    {
        if (BOOL__G__ChargeHistDirty) func__Simple_ChargeHistorySave();
        if (BOOL__G__ChartsDirty) func__Simple_ChartsSave();
        if (BOOL__G__DiagDirty) func__Simple_DiagSave();
        if (BOOL__G__BoardParamsDirty) func__Simple_ParamsSave();
        uint32_t__lastSaveMs = uint32_t__now;
    }

    /* ساعت هر ۳۰ دقیقه روی فایل تازه می‌شود تا بعد از قطع برق، اختلاف
       با زمان واقعی بیشتر از همان ۳۰ دقیقه نشود (پنل RTC ندارد) */
    static uint32_t uint32_t__lastTimeSaveMs = 0u;
    if (BOOL__G__TimeValid && ((uint32_t__now - uint32_t__lastTimeSaveMs) > 1800000u))
    {
        func__Simple_TimeSave();
        uint32_t__lastTimeSaveMs = uint32_t__now;
    }

    // ریبوت خودکار بعد از تغییر وای‌فای - ۲ ثانیه تاخیر
    if (BOOL__G__WifiRebootPending)
    {
        if ((uint32_t__now - UINT32_T__G__WifiRebootMs) > 2000u)
        {
            func__Simple_ChargeHistorySave();
            func__Simple_ChartsSave();
            func__Simple_DiagSave();
            func__Simple_ParamsSave();
            func__Simple_TimeSave();
#if defined(ESP8266)
            ESP.restart();
#else
            ESP.restart();
#endif
        }
    }
}
