/* host_test_latch.cpp - تست هاست صف ریست قفل‌های پنل ساده
   فقط فارسی - مثل بقیهٔ این پوشه

   چرا این فایل: کارت «ریست قفل‌ها» یک مسیر نوشتن تازه به پنل اضافه کرده
   (صف اسلات + فریم SET_PARAM + پژواک REPORT + NAK). هیچ تستی این مسیر را
   نگه نمی‌داشت و اشتباه در چیدمان فریم یا CRC فقط روی برد و با تأخیر
   معلوم می‌شد. اینجا همان توابع خود برنامه (`simple_link.h`، بدون کپی)
   روی PC صدا زده می‌شوند و بایت‌به‌بایت بررسی می‌شوند.

   چه چیزهایی ثابت می‌شود:
   - صف: درخواست تکراری ادغام می‌شود و شناسهٔ بیرون بازه رد می‌شود.
   - فاصلهٔ ارسال: تا نرسیدن SIMPLE_SLOT_TX_GAP_MS هیچ فریمی نمی‌رود.
   - فریم: SOF، نسخه، نوع ۰x01، طول ۵، شناسه، مقدار ۳۲بیتی LE و CRC16 LE.
   - پژواک REPORT: مقدار اعمال‌شده ثبت و خانهٔ همان درخواست آزاد می‌شود.
   - NAK: شناسه و کد ثبت می‌شوند و خانهٔ صف دست نمی‌خورد.
   - پارامترهای عادی (شناسهٔ ۵) دست‌نخورده از مسیر قبلی می‌روند.
   - سناریوی ۵ در مسیر واقعی: اول شمارنده‌ها، بعد قفل؛ پنل فقط پس از پژواک
     تازهٔ شناسهٔ هدف و دریافت پاسخ‌های لازم موفقیت اعلام می‌کند.

   اجرا: bash ESP_USER/Tester/run_simple_tests.sh            */

#include "stub_arduino.h"
#include "stub_web_server.h"

/* خود اسکچ این typedef را انتخاب می‌کند (شاخهٔ ESP8266). اینجا همان شاخه با
   شیم سیمولاتور انتخاب می‌شود تا هدرها بدون تغییر برنامه کامپایل شوند. */
typedef ESP8266WebServer esp_web_server_t;

#include "../simple_config.h"
#include "../simple_state.h"
#include "../simple_link.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

/* ==================== نمونه‌های سراسری استاب‌ها ==================== */
SimSerialLink Serial;
SimWiFi WiFi;
SimEsp ESP;
SimLittleFS LittleFS;

/* فایل‌سیستم اینجا لازم نیست (setup صدا زده نمی‌شود) ولی نمادهایش باید
   تعریف شوند تا لینک کامل شود. */
bool SimLittleFS::begin(bool bool__formatOnFail) { (void)bool__formatOnFail; return true; }
File SimLittleFS::open(const char *char__ptr_path, const char *char__ptr_mode)
{
    return File(std::string(char__ptr_path), char__ptr_mode);
}
bool SimLittleFS::remove(const char *char__ptr_path) { (void)char__ptr_path; return false; }
bool SimLittleFS::exists(const char *char__ptr_path) { (void)char__ptr_path; return false; }

/* ==================== شمارندهٔ بررسی‌ها ==================== */
static uint32_t UINT32_T__G__Checks = 0u;
static uint32_t UINT32_T__G__Failures = 0u;

static void func__T_Check(bool bool__condition, const char *char__ptr_what)
{
    UINT32_T__G__Checks++;
    if (bool__condition)
    {
        (void)printf("  ok   %s\n", char__ptr_what);
    }
    else
    {
        UINT32_T__G__Failures++;
        (void)printf("  FAIL %s\n", char__ptr_what);
    }
}

/* ==================== کمک‌تابع‌ها ==================== */
static uint8_t func__T_PendingCount(void)
{
    uint8_t uint8_t__count = 0u;
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
    {
        if (BOOL__G__SlotQueuePending[uint8_t__i]) { uint8_t__count++; }
    }
    return uint8_t__count;
}

static void func__T_ClearQueue(void)
{
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_SLOT_QUEUE_LEN; uint8_t__i++)
    {
        BOOL__G__SlotQueuePending[uint8_t__i] = false;
    }
}

/* فریم برد -> ESP را با همان الگوریتم CRC خود برنامه می‌سازد */
static uint16_t func__T_MakeFrame(uint8_t *uint8_t__ptr_out, uint8_t uint8_t__type,
                                  const uint8_t *uint8_t__ptr_payload, uint16_t uint16_t__length)
{
    uint16_t uint16_t__crc = func__Simple_Crc16(SIMPLE_LINK_CRC16_INIT, SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t uint16_t__index = 0u;
    uint16_t uint16_t__i;

    uint8_t__ptr_out[uint16_t__index++] = SIMPLE_LINK_SOF_BYTE0;
    uint8_t__ptr_out[uint16_t__index++] = SIMPLE_LINK_SOF_BYTE1;
    uint8_t__ptr_out[uint16_t__index++] = SIMPLE_LINK_PROTOCOL_VERSION;
    uint8_t__ptr_out[uint16_t__index++] = uint8_t__type;
    uint8_t__ptr_out[uint16_t__index++] = (uint8_t)(uint16_t__length & 0xFFu);
    uint8_t__ptr_out[uint16_t__index++] = (uint8_t)((uint16_t__length >> 8) & 0xFFu);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, uint8_t__type);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)(uint16_t__length & 0xFFu));
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)((uint16_t__length >> 8) & 0xFFu));
    for (uint16_t__i = 0u; uint16_t__i < uint16_t__length; uint16_t__i++)
    {
        uint8_t__ptr_out[uint16_t__index++] = uint8_t__ptr_payload[uint16_t__i];
        uint16_t__crc = func__Simple_Crc16(uint16_t__crc, uint8_t__ptr_payload[uint16_t__i]);
    }
    uint8_t__ptr_out[uint16_t__index++] = (uint8_t)(uint16_t__crc & 0xFFu);
    uint8_t__ptr_out[uint16_t__index++] = (uint8_t)((uint16_t__crc >> 8) & 0xFFu);
    return uint16_t__index;
}

/* فریم را وارد گیرندهٔ برنامه می‌کند (همان مسیری که UART واقعی می‌رود) */
static void func__T_InjectFrame(uint8_t uint8_t__type, const uint8_t *uint8_t__ptr_payload,
                                uint16_t uint16_t__length)
{
    uint8_t uint8_t__frame[SIMPLE_LINK_MAX_PAYLOAD + 8u];
    uint16_t uint16_t__size = func__T_MakeFrame(uint8_t__frame, uint8_t__type, uint8_t__ptr_payload, uint16_t__length);
    Serial.boardToEsp(uint8_t__frame, (size_t)uint16_t__size);
    func__Simple_DrainSerial();
}

/* ==================== بررسی‌ها ==================== */
static std::string func__T_ReadEspUserSource(const char *char__ptr_fileName)
{
    std::string string__path(__FILE__);
    std::size_t size_t__separator = string__path.find_last_of("/\\\\");
    if (size_t__separator != std::string::npos) { string__path.resize(size_t__separator); }
    size_t__separator = string__path.find_last_of("/\\\\");
    if (size_t__separator != std::string::npos) { string__path.resize(size_t__separator); }
    string__path += "/";
    string__path += char__ptr_fileName;

    std::ifstream file__source(string__path.c_str());
    std::ostringstream stream__source;
    if (!file__source) { return std::string(); }
    stream__source << file__source.rdbuf();
    return stream__source.str();
}

static void func__T_TestResetOrderAndHandshake(void)
{
    const std::string string__http = func__T_ReadEspUserSource("simple_http.h");
    const std::string string__panel = func__T_ReadEspUserSource("simple_panel.h");
    const std::size_t size_t__routeStart = string__http.find("static void func__Simple_HttpLatchReset(");
    const std::size_t size_t__routeEnd = string__http.find("static void func__Simple_HttpLatchState(", size_t__routeStart);
    const std::string string__route = (size_t__routeStart != std::string::npos &&
                                       size_t__routeEnd != std::string::npos)
        ? string__http.substr(size_t__routeStart, size_t__routeEnd - size_t__routeStart)
        : std::string();
    const std::size_t size_t__events = string__route.find("func__Simple_QueueSlot(SIMPLE_SLOT_EVENTS_ID, 0u)");
    const std::size_t size_t__cycles = string__route.find("func__Simple_QueueSlot(SIMPLE_SLOT_CYCLES_ID, 0u)");
    const std::size_t size_t__latch = string__route.find("func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u)");
    const std::size_t size_t__panelStart = string__panel.find("async function latchReset(n){");
    const std::size_t size_t__panelEnd = string__panel.find("async function loadUsers", size_t__panelStart);
    const std::string string__panelReset = (size_t__panelStart != std::string::npos &&
                                            size_t__panelEnd != std::string::npos)
        ? string__panel.substr(size_t__panelStart, size_t__panelEnd - size_t__panelStart)
        : std::string();
    const std::size_t size_t__baseline = string__panelReset.find("const beforeResp=await fetch('/latch'");
    const std::size_t size_t__post = string__panelReset.find("const postResp=await fetch('/latch/reset?n='+n");

    (void)printf("\n== ترتیب ریست و تأیید پنل ==\n");
    func__T_Check(size_t__routeStart != std::string::npos && size_t__routeEnd != std::string::npos &&
                  size_t__events < size_t__cycles && size_t__cycles < size_t__latch,
                  "مسیر واقعی اول شمارندهٔ رویداد/سیکل و در آخر قفل را صف می‌کند");
    func__T_Check(size_t__baseline != std::string::npos && size_t__post > size_t__baseline &&
                  string__panelReset.find("if(!beforeResp.ok)") < size_t__post,
                  "پنل پیش از ارسال، وضعیت قبلی را با پاسخ سالم می‌خواند");
    func__T_Check(string__panelReset.find("const targetId=n===5?202:203;") != std::string::npos &&
                  string__panelReset.find("const requiredEchoes=n===5?3:1;") != std::string::npos &&
                  string__panelReset.find("Number(st.id)===targetId") != std::string::npos &&
                  string__panelReset.find("Number(st.val)===0") != std::string::npos,
                  "موفقیت فقط با پژواک صفرِ شناسهٔ هدف و هر سه پژواک سناریوی ۵ اعلام می‌شود");
}

static void func__T_TestQueue(void)
{
    (void)printf("\n== صف درخواست ریست ==\n");

    func__T_ClearQueue();
    func__T_Check(func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u), "درخواست ریست قفل در صف نشست");
    func__T_Check(func__T_PendingCount() == 1u, "یک خانه اشغال شد");

    func__T_Check(func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 7u), "درخواست تکراری هم پذیرفته می‌شود");
    func__T_Check(func__T_PendingCount() == 1u, "درخواست تکراری خانهٔ تازه نمی‌گیرد (ادغام)");
    func__T_Check(UINT32_T__G__SlotQueueValue[0] == 7u, "مقدار درخواست تکراری تازه می‌شود");

    func__T_Check(!func__Simple_QueueSlot(SIMPLE_SLOT_FIRST_ID - 1u, 0u), "شناسهٔ کم‌تر از بازه رد می‌شود");
    func__T_Check(!func__Simple_QueueSlot(SIMPLE_SLOT_LAST_ID + 1u, 0u), "شناسهٔ بیش‌تر از بازه رد می‌شود");

    func__T_ClearQueue();
    func__T_Check(func__Simple_QueueSlot(SIMPLE_SLOT_EVENTS_ID, 0u) &&
                  func__Simple_QueueSlot(SIMPLE_SLOT_CYCLES_ID, 0u) &&
                  func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u) &&
                  func__Simple_QueueSlot(SIMPLE_SLOT_DEAD_MASK_ID, 0u),
                  "هر چهار شناسهٔ ریست در صف می‌نشینند");
    func__T_Check(func__T_PendingCount() == SIMPLE_SLOT_QUEUE_LEN, "صف پر شد");
    /* شناسه‌های مجاز و طول صف یکی‌اند، پس با ادغام تکراری‌ها صف پر عملاً
       غیرقابل‌رسیدن است؛ شمارندهٔ رد باید صفر بماند. */
    func__T_Check(func__Simple_QueueSlot(SIMPLE_SLOT_EVENTS_ID, 0u) && (uint8_t)UINT8_T__G__SlotBusyReject == 0u,
                  "با صف پر، درخواست تکراری ادغام می‌شود و ردی ثبت نمی‌شود");
}

static void func__T_TestSender(void)
{
    uint8_t uint8_t__byte;
    uint8_t uint8_t__frame[16];
    uint8_t uint8_t__size = 0u;
    uint16_t uint16_t__crc;

    (void)printf("\n== فرستندهٔ SET_PARAM ==\n");

    func__T_ClearQueue();
    (void)func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u);

    /* فاصلهٔ ارسال: بلافاصله بعد از صف‌شدن چیزی نمی‌رود */
    func__Simple_SlotTick();
    func__T_Check(Serial.takeEspByte(&uint8_t__byte) == false, "قبل از رسیدن فاصلهٔ ارسال فریمی نمی‌رود");

    usleep((useconds_t)(SIMPLE_SLOT_TX_GAP_MS + 40u) * 1000u);
    func__Simple_SlotTick();
    while (Serial.takeEspByte(&uint8_t__byte))
    {
        if (uint8_t__size < (uint8_t)sizeof(uint8_t__frame)) { uint8_t__frame[uint8_t__size] = uint8_t__byte; }
        uint8_t__size++;
    }
    func__T_Check(uint8_t__size == 13u, "فریم SET_PARAM سیزده بایت است");
    func__T_Check(func__T_PendingCount() == 0u, "خانهٔ ارسال‌شده از صف آزاد شد");
    if (uint8_t__size == 13u)
    {
        func__T_Check(uint8_t__frame[0] == SIMPLE_LINK_SOF_BYTE0 && uint8_t__frame[1] == SIMPLE_LINK_SOF_BYTE1,
                      "SOF درست است");
        func__T_Check(uint8_t__frame[2] == (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION, "نسخهٔ پروتکل درست است");
        func__T_Check(uint8_t__frame[3] == SIMPLE_MSG_SET_PARAM, "نوع فریم SET_PARAM است");
        func__T_Check((uint8_t__frame[4] == 5u) && (uint8_t__frame[5] == 0u), "طول بار مفید ۵ است");
        func__T_Check(uint8_t__frame[6] == SIMPLE_SLOT_LATCH_ID, "شناسهٔ اسلات درست است");
        func__T_Check((uint8_t__frame[7] == 0u) && (uint8_t__frame[8] == 0u) &&
                      (uint8_t__frame[9] == 0u) && (uint8_t__frame[10] == 0u), "مقدار ریست صفر و LE است");
        uint16_t__crc = func__Simple_Crc16(SIMPLE_LINK_CRC16_INIT, uint8_t__frame[2]);
        for (uint8_t uint8_t__i = 3u; uint8_t__i < 11u; uint8_t__i++)
        {
            uint16_t__crc = func__Simple_Crc16(uint16_t__crc, uint8_t__frame[uint8_t__i]);
        }
        func__T_Check(((uint16_t)((uint16_t)uint8_t__frame[12] << 8) | (uint16_t)uint8_t__frame[11]) == uint16_t__crc,
                      "CRC16 کم‌بایت‌اول و مطابق محاسبهٔ خود برنامه است");
    }

    /* مقدار غیرصفر هم درست شکسته می‌شود */
    func__T_ClearQueue();
    (void)func__Simple_QueueSlot(SIMPLE_SLOT_DEAD_MASK_ID, 0x01020304u);
    usleep((useconds_t)(SIMPLE_SLOT_TX_GAP_MS + 40u) * 1000u);
    func__Simple_SlotTick();
    uint8_t__size = 0u;
    while (Serial.takeEspByte(&uint8_t__byte))
    {
        if (uint8_t__size < (uint8_t)sizeof(uint8_t__frame)) { uint8_t__frame[uint8_t__size] = uint8_t__byte; }
        uint8_t__size++;
    }
    func__T_Check((uint8_t__size == 13u) && (uint8_t__frame[6] == SIMPLE_SLOT_DEAD_MASK_ID) &&
                  (uint8_t__frame[7] == 0x04u) && (uint8_t__frame[8] == 0x03u) &&
                  (uint8_t__frame[9] == 0x02u) && (uint8_t__frame[10] == 0x01u),
                  "مقدار ۳۲بیتی کم‌بایت‌اول می‌رود");
}

static void func__T_TestEchoAndNak(void)
{
    uint8_t uint8_t__payload[5];
    uint32_t uint32_t__echoBefore = UINT32_T__G__SlotEchoCount;
    uint32_t uint32_t__nakBefore = UINT32_T__G__SlotNakCount;

    (void)printf("\n== پژواک REPORT و NAK ==\n");

    func__T_ClearQueue();
    (void)func__Simple_QueueSlot(SIMPLE_SLOT_LATCH_ID, 0u);
    uint8_t__payload[0] = SIMPLE_SLOT_LATCH_ID;
    uint8_t__payload[1] = 0u;
    uint8_t__payload[2] = 0u;
    uint8_t__payload[3] = 0u;
    uint8_t__payload[4] = 0u;
    func__T_InjectFrame(SIMPLE_MSG_PARAM_REPORT, uint8_t__payload, 5u);
    func__T_Check(UINT32_T__G__SlotEchoCount == (uint32_t__echoBefore + 1u), "پژواک اسلات شمرده شد");
    func__T_Check(BOOL__G__SlotLastKnown && (UINT8_T__G__SlotLastId == SIMPLE_SLOT_LATCH_ID) &&
                  (UINT32_T__G__SlotLastValue == 0u), "آخرین مقدار اعمال‌شده ثبت شد");
    func__T_Check(func__T_PendingCount() == 0u, "خانهٔ تأییدشده از صف آزاد شد");

    /* پارامتر عادی نباید از مسیر صف رد شود */
    uint8_t__payload[0] = 5u;
    uint8_t__payload[1] = 0x39u;
    uint8_t__payload[2] = 0x30u;
    uint8_t__payload[3] = 0u;
    uint8_t__payload[4] = 0u;
    func__T_InjectFrame(SIMPLE_MSG_PARAM_REPORT, uint8_t__payload, 5u);
    func__T_Check(func__Simple_BoardParam(5u) == 0x3039u, "پارامتر عادی از مسیر قبلی ذخیره می‌شود");
    func__T_Check(UINT32_T__G__SlotEchoCount == (uint32_t__echoBefore + 1u), "پارامتر عادی شمارندهٔ پژواک را بالا نمی‌برد");

    uint8_t__payload[0] = SIMPLE_SLOT_DEAD_MASK_ID;
    uint8_t__payload[1] = 9u;
    func__T_InjectFrame(SIMPLE_MSG_PARAM_NAK, uint8_t__payload, 2u);
    func__T_Check(UINT32_T__G__SlotNakCount == (uint32_t__nakBefore + 1u), "NAK اسلات شمرده شد");
    func__T_Check((UINT8_T__G__SlotNakId == SIMPLE_SLOT_DEAD_MASK_ID) && (UINT8_T__G__SlotNakCode == 9u),
                  "شناسه و کد NAK ثبت شدند");

    uint8_t__payload[0] = 5u;
    uint8_t__payload[1] = 9u;
    func__T_InjectFrame(SIMPLE_MSG_PARAM_NAK, uint8_t__payload, 2u);
    func__T_Check(UINT32_T__G__SlotNakCount == (uint32_t__nakBefore + 1u), "NAK پارامتر عادی شمرده نمی‌شود");
}

static void func__T_TestWifiPowerSave(void)
{
    uint8_t uint8_t__byte = 0u;
    uint8_t uint8_t__frame[16];
    uint8_t uint8_t__size = 0u;

    (void)printf("\n== سناریوی ترکیبی کاهش مصرف و قدرت آنتن وای‌فای ==\n");

    /* ۱. حضور برق ورودی: روشنِ دائم با بیشترین قدرت آنتن (۲۰٫۵ dBm) */
    BOOL__G__TlmSeen = true;
    UINT8_T__G__TlmFlags = SIMPLE_TLM_FLAG_INPUT_PRESENT;
    BOOL__G__BrowserSeen = false;
    WiFi.setStationNum(0);
    (void)WiFi.softAP(CHAR__G__WifiSsid, CHAR__G__WifiPass);
    WiFi.setOutputPower(SIMPLE_WIFI_TX_MAX_DBM);
    BOOL__G__WifiApActive = true;
    BOOL__G__WifiHighPower = true;
    BOOL__G__WifiBatteryIdleTracking = false;
    BOOL__G__WifiLastReportedConnected = false;

    func__Simple_WifiPowerSaveTick(1000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() > 20.0f),
                  "در زمان وجود برق ورودی: وای‌فای روشن و قدرت آنتن روی بیشترین مقدار است");

    /* ۲. قطع برق (روی باتری) و بدون کاربر: ۳۰ دقیقهٔ اول روشن با قدرت آنتن پایین (۰ dBm) */
    UINT8_T__G__TlmFlags = 0u;
    func__Simple_WifiPowerSaveTick(2000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() < 0.1f),
                  "شروع کار روی باتری بدون کاربر: وای‌فای روشن با قدرت آنتن پایین");

    func__Simple_WifiPowerSaveTick(2000u + 1799000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() < 0.1f),
                  "انتهای ۳۰ دقیقهٔ اول روی باتری: همچنان روشن با قدرت آنتن پایین");

    /* ۳. پس از ۳۰ دقیقه (۱۸۰۰۰۰۰ms): ۲ دقیقه خاموش و ۱ دقیقه روشن با قدرت آنتن پایین */
    func__Simple_WifiPowerSaveTick(2000u + 1800000u);
    func__T_Check(!WiFi.isApActive(),
                  "پس از ۳۰ دقیقه بدون کاربر: ورود به بازهٔ ۲ دقیقه خاموشی وای‌فای");

    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 119000u);
    func__T_Check(!WiFi.isApActive(),
                  "در ثانیهٔ ۱۱۹ از بازهٔ خاموشی: همچنان خاموش");

    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 120000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() < 0.1f),
                  "پس از ۲ دقیقه خاموشی: روشن شدن ۱ دقیقه‌ای با قدرت آنتن پایین");

    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 180000u);
    func__T_Check(!WiFi.isApActive(),
                  "پایان ۱ دقیقه روشنایی: شروع چرخهٔ بعدی ۲ دقیقه خاموشی");

    /* ۴. اتصال کاربر در حین کار روی باتری: روشنِ دائم + بیشترین قدرت آنتن + ارسال فریم ۰x۳۳ به برد */
    while (Serial.takeEspByte(&uint8_t__byte)) {}
    WiFi.setStationNum(1);
    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 190000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() > 20.0f),
                  "به‌محض اتصال کاربر: وای‌فای روشن و قدرت آنتن روی بیشترین مقدار (۲۰٫۵ dBm)");

    uint8_t__size = 0u;
    while (Serial.takeEspByte(&uint8_t__byte))
    {
        if (uint8_t__size < (uint8_t)sizeof(uint8_t__frame)) { uint8_t__frame[uint8_t__size] = uint8_t__byte; }
        uint8_t__size++;
    }
    func__T_Check((uint8_t__size == 9u) &&
                  (uint8_t__frame[3] == SIMPLE_MSG_WIFI_CLIENT_STATE) &&
                  (uint8_t__frame[6] == 1u),
                  "فریم گزارش اتصال کاربر (۰x۳۳ با مقدار ۱) برای برد ارسال شد");

    /* ۵. قطع اتصال کاربر: تایمر ۳۰ دقیقه‌ای از اول شروع می‌شود و قدرت آنتن پایین می‌آید */
    WiFi.setStationNum(0);
    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 200000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() < 0.1f),
                  "پس از قطع اتصال کاربر روی باتری: شروع مجدد ۳۰ دقیقهٔ اول با قدرت آنتن پایین");
}

int main(void)
{
    (void)printf("== تست هاست صف ریست قفل پنل ساده ==\n");

    func__T_TestResetOrderAndHandshake();
    func__T_TestQueue();
    func__T_TestSender();
    func__T_TestEchoAndNak();
    func__T_TestWifiPowerSave();

    (void)printf("\n== بررسی‌ها: %u، شکست‌ها: %u ==\n", (unsigned)UINT32_T__G__Checks, (unsigned)UINT32_T__G__Failures);
    if (UINT32_T__G__Failures != 0u)
    {
        (void)printf("تست هاست صف ریست: رد\n");
        return 1;
    }
    (void)printf("تست هاست صف ریست: قبول\n");
    return 0;
}
