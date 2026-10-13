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

#include <stdint.h>
#include <stdbool.h>

/* ترتیب include مهم است - تک واحد ترجمه */
#include "simple_config.h"
#include "simple_state.h"
#include "simple_panel.h"
#include "simple_link.h"
#include "simple_http.h"

/* ==================== راه‌اندازی ==================== */
void setup(void)
{
    Serial.setRxBufferSize(SIMPLE_LINK_RX_BUFFER_SIZE);
    Serial.begin(SIMPLE_LINK_BAUD_RATE, SERIAL_8N1);

    WiFi.mode(WIFI_AP);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
    WiFi.setSleep(false);
#endif
    (void)WiFi.softAP(SIMPLE_WIFI_AP_SSID, SIMPLE_WIFI_AP_PASS);

    SIMPLE_WEB_SERVER_T__G__Server.on("/", HTTP_GET, func__Simple_HttpRoot);
    SIMPLE_WEB_SERVER_T__G__Server.on("/t", HTTP_GET, func__Simple_HttpTelemetry);
    SIMPLE_WEB_SERVER_T__G__Server.begin();
}

/* ==================== حلقه اصلی ==================== */
void loop(void)
{
    func__Simple_DrainSerial();
    SIMPLE_WEB_SERVER_T__G__Server.handleClient();
}
