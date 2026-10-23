/**
 * @file    ESP_USER.ino
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

    WiFi.persistent(false);
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
    func__Simple_RecoveryApply();     /* بوت قبلی بدون ورود بوده ⇒ بازگشت به admin/admin */
    func__Simple_ThemeLoad();
    func__Simple_WifiLoad();
    func__Simple_TimeLoad();          /* ساعت ذخیره‌شده - دیگر پرسیده نمی‌شود */
    func__Simple_ParamsLoad();        /* پارامترهای خود برد از آخرین بار */
    func__Simple_ChargeHistoryLoad();
    func__Simple_ChartsLoad();        /* هر چهار بازه از آخرین ذخیرهٔ روزانه */
    func__Simple_DiagLoad();
    func__SimpleLog_Begin();          /* فهرست روزهای تاریخچه */

    // بازیابی prev counts برای تشخیص شارژ بعد از ریستارت
    UINT32_T__G__ChargeCountCh1 = UINT32_T__G__PrevChargeCountCh1;
    UINT32_T__G__ChargeCountCh2 = UINT32_T__G__PrevChargeCountCh2;

    (void)WiFi.softAP(CHAR__G__WifiSsid, CHAR__G__WifiPass);
#if defined(ESP8266)
    WiFi.setOutputPower(SIMPLE_WIFI_TX_MAX_DBM);
#endif
    BOOL__G__WifiApActive = true;
    BOOL__G__WifiHighPower = true;

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
    /* ریست دستی قفل‌ها: خواندن وضعیت صف و درخواست ریست (فقط ادمین). */
    SIMPLE_WEB_SERVER_T__G__Server.on("/latch", HTTP_GET, func__Simple_HttpLatchState);
    SIMPLE_WEB_SERVER_T__G__Server.on("/latch/reset", HTTP_POST, func__Simple_HttpLatchReset);
    /* تاریخچهٔ وضعیت (لاگ روزانه) */
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt", HTTP_GET, func__Simple_HttpEventLog);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/days", HTTP_GET, func__Simple_HttpEventDays);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/dl", HTTP_GET, func__Simple_HttpEventDownload);
    SIMPLE_WEB_SERVER_T__G__Server.on("/evt/clr", HTTP_POST, func__Simple_HttpEventClear);
    SIMPLE_WEB_SERVER_T__G__Server.begin();
}

/* ==================== حلقه اصلی - با ذخیره‌سازی پایدار و ریبوت وای‌فای و تیک نمودار مستقل و دیاگ ==================== */
void loop(void)
{
    func__Simple_DrainSerial();
    SIMPLE_WEB_SERVER_T__G__Server.handleClient();
    func__Simple_WifiPowerSaveTick((uint32_t)millis());
    func__Simple_ChartTick();

    /* صف ریست قفل‌ها: هر ۱۲۰ms حداکثر یک فریم SET_PARAM */
    func__Simple_SlotTick();

    /* تاریخچهٔ وضعیت: نوشتن ردیف‌های در انتظار و تشخیص قطع ارتباط */
    func__SimpleLog_Tick();

    // درخواست دیاگ مستقل: اولین بار به محض دریافت تله‌متری (برای همگام‌سازی فوری ساعت از STM32) و سپس هر ۱۰ ثانیه
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

    // خواندن پارامترهای خود برد (درصد باتری و چشمک‌ها) - اول بار بعد از آمدن لینک
    static bool bool__paramsReqDone = false;
    if ((BOOL__G__TlmSeen && !bool__paramsReqDone) ||
        ((uint32_t__now - UINT32_T__G__LastParamsReqMs) > SIMPLE_PARAMS_REQ_MS))
    {
        func__Simple_SendGetParams();
        bool__paramsReqDone = true;
    }

    /* ساعت: ساعت میکرو STM32 منبع دقیق‌تری است (چون در زمان خاموشی موقت وای‌فای
       میکرو روشن می‌ماند و ساعتش جلو می‌رود). اگر پنل ساعت نداشت، یا ساعت STM32
       جلوتر بود، یا بیش از ۵ دقیقه اختلاف داشت، ساعت STM32 را برمی‌داریم و
       در صورت رسیدن به آستانهٔ ۵ دقیقه روی فایل ذخیره می‌کنیم. */
    if (UINT32_T__G__BoardTimeSec >= SIMPLE_TIME_VALID_MIN)
    {
        bool bool__adopt = !BOOL__G__TimeValid;
        if (BOOL__G__TimeValid)
        {
            uint32_t uint32_t__panelSec = func__Simple_NowSec();
            if (UINT32_T__G__BoardTimeSec > uint32_t__panelSec)
            {
                bool__adopt = true;
            }
            else if ((uint32_t__panelSec - UINT32_T__G__BoardTimeSec) >= SIMPLE_TIME_ADOPT_DIFF_SEC)
            {
                bool__adopt = true;
            }
        }
        if (bool__adopt)
        {
            func__Simple_TimeSet(UINT32_T__G__BoardTimeSec, false);
            func__Simple_TimeSave();
            BOOL__G__TimePushed = true;   /* ساعت از خود برد گرفته شد - نیازی به فرستادن نیست */
        }
    }
    if (BOOL__G__TimeValid && BOOL__G__TlmSeen && BOOL__G__DiagValid && !BOOL__G__TimePushed)
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

    /* بازیابی رمز: ۱۰ ثانیه پس از روشن‌شدن، اگر هنوز کسی وارد نشده باشد پرچم
       «بوت بدون ورود» روی فلش می‌نشیند؛ قطع و وصل برق آن را به بوت بعدی
       می‌رساند و بوت بعدی رمز ادمین را به پیش‌فرض برمی‌گرداند */
    if (!BOOL__G__SessionHadLogin && ((uint32_t)millis() > SIMPLE_RECOVERY_ARM_MS))
    {
        func__Simple_RecoveryArm();
    }

    // ذخیره دوره‌ای تاریخچه شارژ و دیاگ و پارامترها هر ۳۰ ثانیه (فقط در صورت تغییر)
    static uint32_t uint32_t__lastSaveMs = 0u;
    if ((uint32_t__now - uint32_t__lastSaveMs) > 30000u)
    {
        if (BOOL__G__ChargeHistDirty) func__Simple_ChargeHistorySave();
        if (BOOL__G__DiagDirty) func__Simple_DiagSave();
        if (BOOL__G__BoardParamsDirty) func__Simple_ParamsSave();
        uint32_t__lastSaveMs = uint32_t__now;
    }

    // ذخیرهٔ نمودارها روی فلش فقط روزی ۱ بار (هر ۲۴ ساعت = ۸۶۴۰۰ ثانیهٔ واقعی یا ۸۶۴۰۰۰۰۰ میلی‌ثانیه) و فقط در صورت تغییر
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

    // ریبوت خودکار بعد از تغییر وای‌فای - ۲ ثانیه تاخیر
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
