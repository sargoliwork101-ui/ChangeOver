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

    func__Simple_AuthLoad();
    func__Simple_ThemeLoad();
    func__Simple_WifiLoad();
    func__Simple_ChargeHistoryLoad();
    func__Simple_ChartsLoad();

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
    SIMPLE_WEB_SERVER_T__G__Server.on("/adduser", HTTP_POST, func__Simple_HttpAddUser);
    SIMPLE_WEB_SERVER_T__G__Server.on("/deluser", HTTP_POST, func__Simple_HttpDelUser);
    SIMPLE_WEB_SERVER_T__G__Server.on("/theme", HTTP_GET, func__Simple_HttpGetTheme);
    SIMPLE_WEB_SERVER_T__G__Server.on("/settheme", HTTP_POST, func__Simple_HttpSetTheme);
    SIMPLE_WEB_SERVER_T__G__Server.on("/wifi", HTTP_GET, func__Simple_HttpGetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/setwifi", HTTP_POST, func__Simple_HttpSetWifi);
    SIMPLE_WEB_SERVER_T__G__Server.on("/clear", HTTP_POST, func__Simple_HttpClearHistory);
    SIMPLE_WEB_SERVER_T__G__Server.begin();
}

/* ==================== حلقه اصلی - با ذخیره‌سازی پایدار و ریبوت وای‌فای ==================== */
void loop(void)
{
    func__Simple_DrainSerial();
    SIMPLE_WEB_SERVER_T__G__Server.handleClient();

    // ذخیره دوره‌ای تاریخچه شارژ و نمودارها هر ۳۰ ثانیه
    static uint32_t uint32_t__lastSaveMs = 0u;
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((uint32_t__now - uint32_t__lastSaveMs) > 30000u)
    {
        func__Simple_ChargeHistorySave();
        func__Simple_ChartsSave();
        uint32_t__lastSaveMs = uint32_t__now;
    }

    // ریبوت خودکار بعد از تغییر وای‌فای - ۲ ثانیه تاخیر
    if (BOOL__G__WifiRebootPending)
    {
        if ((uint32_t__now - UINT32_T__G__WifiRebootMs) > 2000u)
        {
            func__Simple_ChargeHistorySave();
            func__Simple_ChartsSave();
#if defined(ESP8266)
            ESP.restart();
#else
            ESP.restart();
#endif
        }
    }
}
