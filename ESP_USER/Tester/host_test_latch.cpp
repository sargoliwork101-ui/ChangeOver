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
#include "../simple_auth.h"
#include "../simple_diag.h"

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

/* فایل‌سیستم تست در /tmp نگه داشته می‌شود تا مسیر ذخیره و بازیابی هم بررسی شود. */
static std::string func__T_FsPath(const char *char__ptr_path)
{
    const char *char__ptr_name = (char__ptr_path[0] == '/') ? (char__ptr_path + 1) : char__ptr_path;
    return std::string("/tmp/changeover_simple_test_") + std::string(char__ptr_name);
}
bool SimLittleFS::begin(bool bool__formatOnFail) { (void)bool__formatOnFail; return true; }
File SimLittleFS::open(const char *char__ptr_path, const char *char__ptr_mode)
{
    return File(func__T_FsPath(char__ptr_path), char__ptr_mode);
}
bool SimLittleFS::remove(const char *char__ptr_path)
{
    return (::unlink(func__T_FsPath(char__ptr_path).c_str()) == 0);
}
bool SimLittleFS::exists(const char *char__ptr_path)
{
    return (::access(func__T_FsPath(char__ptr_path).c_str(), F_OK) == 0);
}

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
    func__T_Check(string__panelReset.find("const targetId=n===5?202:(n===6?203:204);") != std::string::npos &&
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
                  func__Simple_QueueSlot(SIMPLE_SLOT_DEAD_MASK_ID, 0u) &&
                  func__Simple_QueueSlot(SIMPLE_SLOT_WEAK_RESET_ID, 0u),
                  "هر پنج شناسهٔ ریست در صف می‌نشینند");
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

    /* ۶. باتری زیر ۲۰ درصد روی باتری: خاموش شدن کامل وای‌فای حتی در صورت حضور کاربر */
    WiFi.setStationNum(1);
    UINT32_T__G__TlmField[15] = 22000u; /* ۱۲ درصد (< ۲۰٪) */
    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 210000u);
    func__T_Check(!WiFi.isApActive(),
                  "وقتی باتری زیر ۲۰ درصد بیاید، وای‌فای کاملاً خاموش می‌شود و روشن نمی‌شود");

    /* ۷. بازگشت برق ورودی: روشن شدن مجدد با بیشترین قدرت آنتن */
    UINT8_T__G__TlmFlags = SIMPLE_TLM_FLAG_INPUT_PRESENT;
    func__Simple_WifiPowerSaveTick(2000u + 1800000u + 220000u);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() > 20.0f),
                  "با وصل شدن مجدد برق ورودی، وای‌فای با بیشترین قدرت آنتن روشن می‌شود");
}

/* ==================== همگام‌سازی ساعت از میکرو STM32 و ثبت نمودار با ثانیهٔ واقعی ==================== */
static void func__T_TestStm32TimeAndChartScheduling(void)
{
    uint8_t uint8_t__payload[23];
    uint8_t uint8_t__frame[48];
    uint16_t uint16_t__len;
    uint16_t uint16_t__i;
    uint32_t uint32_t__stmTimeSec = 1790000000u;

    (void)printf("\n== همگام‌سازی ساعت از میکرو STM32 و ثبت نمودار زمان‌دار ==\n");

    func__Simple_ChartsClear();
    /* پنل با یک ساعت قدیمی‌تر (مثلاً ۲ دقیقه عقب‌تر از میکرو بعد از خواب CH_PD) بیدار می‌شود */
    func__Simple_TimeSet(uint32_t__stmTimeSec - 120u, false);

    for (uint16_t__i = 0u; uint16_t__i < 23u; uint16_t__i++)
    {
        uint8_t__payload[uint16_t__i] = 0u;
    }
    uint8_t__payload[0] = 0u; /* chunk 0 */
    uint8_t__payload[17] = (uint8_t)(uint32_t__stmTimeSec & 0xFFu);
    uint8_t__payload[18] = (uint8_t)((uint32_t__stmTimeSec >> 8) & 0xFFu);
    uint8_t__payload[19] = (uint8_t)((uint32_t__stmTimeSec >> 16) & 0xFFu);
    uint8_t__payload[20] = (uint8_t)((uint32_t__stmTimeSec >> 24) & 0xFFu);

    uint16_t__len = func__T_MakeFrame(uint8_t__frame, SIMPLE_MSG_DIAG_DATA, uint8_t__payload, 23u);
    for (uint16_t__i = 0u; uint16_t__i < uint16_t__len; uint16_t__i++)
    {
        func__Simple_ParseByte(uint8_t__frame[uint16_t__i]);
    }

    func__T_Check(func__Simple_NowSec() >= uint32_t__stmTimeSec,
                  "به‌محض دریافت دیاگ از میکرو STM32، ساعت پنل با ساعت جلوترِ میکرو همگام می‌شود");

    func__Simple_ChartAddTimed(80u, 24000u, 25000u);
    func__T_Check((UINT8_T__G__Chart1D_Count == 1u) && (UINT8_T__G__Chart7D_Count == 1u),
                  "اولین نقطهٔ نمودار ۱ روزه و ۷ روزه با ساعت میکرو ثبت شد");

    /* شبیه‌سازی گذشت ۵ دقیقه روی ساعت میکرو بدون افزایش millis بیش از ۱ دقیقه */
    uint32_t__stmTimeSec += 300u;
    uint8_t__payload[17] = (uint8_t)(uint32_t__stmTimeSec & 0xFFu);
    uint8_t__payload[18] = (uint8_t)((uint32_t__stmTimeSec >> 8) & 0xFFu);
    uint8_t__payload[19] = (uint8_t)((uint32_t__stmTimeSec >> 16) & 0xFFu);
    uint8_t__payload[20] = (uint8_t)((uint32_t__stmTimeSec >> 24) & 0xFFu);
    uint16_t__len = func__T_MakeFrame(uint8_t__frame, SIMPLE_MSG_DIAG_DATA, uint8_t__payload, 23u);
    for (uint16_t__i = 0u; uint16_t__i < uint16_t__len; uint16_t__i++)
    {
        func__Simple_ParseByte(uint8_t__frame[uint16_t__i]);
    }

    func__Simple_ChartAddTimed(82u, 24000u, 25200u);
    func__T_Check((UINT8_T__G__Chart1D_Count == 2u) && (UINT8_T__G__Chart7D_Count == 2u),
                  "نقطهٔ بعدی نمودارها بر اساس ثانیهٔ واقعی میکرو (حتی پس از ریست millis) ثبت شد");
}

/* ==================== ۷) ذخیرهٔ وضعیت کانال‌ها در خطا و پاک‌سازی تاریخچه ==================== */
static void func__T_TestDiagStatePersistenceAndClear(void)
{
    (void)printf("\n== ذخیرهٔ وضعیت کانال‌ها در خطا و پاک‌سازی تاریخچه ==\n");

    BOOL__G__FsOk = LittleFS.begin();
    UINT8_T__G__DiagErrorCount = 0u;
    UINT32_T__G__LastLocalFaultMask = 0u;
    UINT32_T__G__TlmField[6] = 1u;
    UINT32_T__G__TlmField[13] = 10u;
    UINT32_T__G__TlmField[19] = 0x40u;

    func__Simple_LocalRecordFault(0x40u, 24000u, 25000u, 12500u, 12500u);
    UINT32_T__G__LastLocalFaultMask = 0x40u;
    func__T_Check((UINT8_T__G__DiagErrorCount == 1u) &&
                  (SIMPLE_DIAG_ERROR_T__G__Errors[0].state1 == 1u) &&
                  (SIMPLE_DIAG_ERROR_T__G__Errors[0].state2 == 10u),
                  "ثبت محلی خطا وضعیت کانال ۱ و ۲ (s1 و s2) را از تله‌متری ذخیره می‌کند");

    func__Simple_DiagSave();
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state1 = 0u;
    SIMPLE_DIAG_ERROR_T__G__Errors[0].state2 = 0u;
    UINT8_T__G__DiagErrorCount = 0u;
    func__Simple_DiagLoad();
    func__T_Check((UINT8_T__G__DiagErrorCount == 1u) &&
                  (SIMPLE_DIAG_ERROR_T__G__Errors[0].state1 == 1u) &&
                  (SIMPLE_DIAG_ERROR_T__G__Errors[0].state2 == 10u),
                  "فایل /diag.json وضعیت کانال ۱ و ۲ را پس از بارگذاری حفظ می‌کند");

    BOOL__G__DiagDirty = true;
    BOOL__G__ChartsDirty = true;
    func__Simple_DiagClear();
    func__Simple_ChartsClear();
    func__T_Check((BOOL__G__DiagDirty == false) && (BOOL__G__ChartsDirty == false),
                  "پاک‌سازی تاریخچه پرچم‌های Dirty را صفر می‌کند");

    /* همان خطای فعال قبلی نباید در تیک بعدی دوباره وارد تاریخچه شود */
    func__Simple_LocalRecordFault(0x40u, 24000u, 25000u, 12500u, 12500u);
    func__T_Check(UINT8_T__G__DiagErrorCount == 0u,
                  "خطای فعالِ فعلی بلافاصله بعد از پاک کردن تاریخچه دوباره ثبت نمی‌شود");
}

/* ==================== ۸) حفاظت از نقش ادمین و شمارش کاربران ==================== */
static void func__T_TestAuthAdminProtection(void)
{
    (void)printf("\n== حفاظت از نقش ادمین و شمارش کاربران ==\n");

    BOOL__G__FsOk = LittleFS.begin();
    func__Simple_CryptoKeyInit();
    func__Simple_AuthDefaults();
    (void)func__Simple_AuthLogin("admin", "admin");

    /* تلاش برای تنزل دادن ادمین اصلی به کاربر ساده (role=0) باید نقش ادمین (1) را حفظ کند */
    (void)func__Simple_AddUser("admin", "newadminpass", 0u);
    func__T_Check(SIMPLE_USER_T__G__Users[0].role == 1u,
                  "حساب admin حتی در صورت ارسال role=0 در AddUser نقش ادمین (1) را حفظ می‌کند");

    /* افزودن کاربر با نام ۲۴ کاراکتری (حداکثر طول) و بررسی پایان‌دهندهٔ رشته و شمارنده */
    const char *char__ptr_maxName = "123456789012345678901234";
    bool bool__added = func__Simple_AddUser(char__ptr_maxName, "pass1234", 0u);
    func__T_Check(bool__added &&
                  (strlen(SIMPLE_USER_T__G__Users[1].name) == 24u) &&
                  (UINT8_T__G__UserCount == 2u),
                  "افزودن کاربر با نام ۲۴ کاراکتری رشته را با '\\0' می‌بندد و UserCount به ۲ می‌رسد");

    /* حذف کاربر دوم باید UserCount را به ۱ برگرداند */
    bool bool__deleted = func__Simple_DeleteUser(char__ptr_maxName);
    func__T_Check(bool__deleted && (UINT8_T__G__UserCount == 1u),
                  "حذف کاربر شمارندهٔ UserCount را به تعداد واقعی کاربران فعال کاهش می‌دهد");
}

/* ==================== تاریخچهٔ وضعیت ====================
   دستور کاربر ۲۰۲۶-۱۰-۲۳: هر پنل تاریخچهٔ وضعیت خودش را دارد. اینجا سه قول
   قفل می‌شود: فقط تغییر واقعی ردیف می‌سازد، ردیف در فایل همان روز می‌نشیند و
   پاک کردن، فایل‌ها را برمی‌دارد. */
static void func__T_TestStateHistoryLog(void)
{
    (void)printf("\n== تاریخچهٔ وضعیت ==\n");

    /* ساعت گوشی و منطقهٔ زمانی */
    func__Simple_TimeSet(1792756800u, false);
    INT32_T__G__LogTzSec = 12600;

    /* فایل‌های /tmp از اجرای قبلی پاک می‌شوند تا شمارش ردیف‌ها از صفر شروع شود */
    {
        char char__ptr_old[SIMPLE_LOG_PATH_MAX];
        const uint32_t uint32_t__old[3] = { 20261001u, 20261002u, 20261003u };
        for (uint8_t uint8_t__i = 0u; uint8_t__i < 3u; uint8_t__i++)
        {
            func__SimpleLog_PathOf(char__ptr_old, uint32_t__old[uint8_t__i]);
            (void)LittleFS.remove(char__ptr_old);
        }
        func__SimpleLog_PathOf(char__ptr_old, func__SimpleLog_TodayCode());
        (void)LittleFS.remove(char__ptr_old);
        (void)LittleFS.remove(SIMPLE_LOG_INDEX_PATH);
        (void)LittleFS.remove(SIMPLE_LOG_LAST_PATH);
    }

    /* پاک کردن وضعیت قبلی */
    UINT8_T__G__LogIndexCount = 0u;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    BOOL__G__LogHasPrev = false;
    BOOL__G__LogBootRowPending = true;
    BOOL__G__LogLastRowValid = false;
    BOOL__G__LogLinkDown = false;
    BOOL__G__TlmSeen = false;
    UINT32_T__G__LastTlmMs = (uint32_t)millis();
    UINT16_T__G__LogPendingMask = 0u;
    BOOL__G__LogMainsQueuedValid = false;

    UINT32_T__G__TlmField[15] = 24000u;
    UINT8_T__G__TlmFlags = 0u;
    /* v1.106: ردیف بوت دیگر منتظر برد نمی‌ماند - خودِ Tick در شروع می‌نویسد */
    func__SimpleLog_Tick();
    func__T_Check(UINT16_T__G__LogRowsToday == 1u,
                  "با روشن‌شدن پنل - حتی بدون هیچ فریمی از برد - یک ردیف «پنل روشن شد» نوشته می‌شود");

    const uint32_t uint32_t__day = func__SimpleLog_TodayCode();
    char char__path[SIMPLE_LOG_PATH_MAX];
    func__SimpleLog_PathOf(char__path, uint32_t__day);
    func__T_Check((uint32_t__day != 0u) && LittleFS.exists(char__path),
                  "ردیف در فایل روزِ خودِ گوشی ذخیره می‌شود");

    func__SimpleLog_OnFrame();   /* اولین فریم فقط وضعیت قبلی را می‌کارد */
    func__SimpleLog_OnFrame();
    func__T_Check(UINT16_T__G__LogRowsToday == 1u,
                  "وضعیت ثابت ردیف تازه نمی‌سازد");

    /* پنجرهٔ ادغام ۲ ثانیه‌ای: بی‌آنکه تست ۲ ثانیه بخوابد، ساعت را جلو می‌بریم */
    UINT8_T__G__TlmFlags = 0x02u;   /* برق شهر آمد */
    UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
    func__SimpleLog_OnFrame();
    func__T_Check(UINT16_T__G__LogRowsToday == 2u,
                  "آمدن برق شهر یک ردیف می‌نویسد");

    UINT32_T__G__TlmField[15] = 20608u;   /* زیر مرز قطع */
    UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
    func__SimpleLog_OnFrame();
    func__T_Check(UINT16_T__G__LogRowsToday == 3u,
                  "رسیدن باتری به مرز قطع یک ردیف می‌نویسد");
    func__T_Check(LittleFS.exists(SIMPLE_LOG_LAST_PATH),
                  "آخرین وضعیت روی فلش می‌ماند تا ردیف بوت بداند پنل کجا خاموش شد");

    /* مود دستی هم یک وضعیت است و باید در تاریخچه بیاید */
    UINT8_T__G__TlmFlags = 0x22u;   /* برق شهر + مود دستی */
    UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
    func__SimpleLog_OnFrame();
    func__T_Check(UINT16_T__G__LogRowsToday == 4u,
                  "روشن شدن مود دستی یک ردیف می‌نویسد");
    std::string string__head;
    {
        File file__head = LittleFS.open(char__path, "r");
        while (file__head.available() > 0) {
            string__head += file__head.readStringUntil('\n').c_str();
            string__head += "\n";
        }
        file__head.close();
    }
    func__T_Check(string__head.find(",fl,") != std::string::npos,
                  "ستون بایت عمومی وضعیت در سرصفحهٔ فایل هست");

    char char__ptr_out[1024];
    (void)func__SimpleLog_Tail(char__ptr_out, sizeof(char__ptr_out), 0u, 10u);
    std::string string__tail(char__ptr_out);
    func__T_Check(string__tail.find("20608") != std::string::npos,
                  "خواندن تاریخچه ردیف‌های ذخیره‌شده را برمی‌گرداند");

    /* ---------- v1.96: گام ولتاژ، ضربان و بودجهٔ حجم ---------- */
    UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
    UINT32_T__G__TlmField[15] = 20608u + 199u;
    func__SimpleLog_OnFrame();
    func__T_Check(UINT16_T__G__LogRowsToday == 4u,
                  "جهش کمتر از گام ولتاژ ردیف نمی‌سازد");

    /* پنجرهٔ «یک ردیف گام در دقیقه» را باز می‌کنیم تا گامِ تنها بنویسد */
    UINT32_T__G__LogLastStepMs = (uint32_t)millis() - SIMPLE_LOG_VSTEP_MIN_MS - 10u;
    UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
    UINT32_T__G__TlmField[15] = 20608u + 200u;
    func__SimpleLog_OnFrame();
    func__T_Check(UINT16_T__G__LogRowsToday == 5u,
                  "پلهٔ ۲۰۰ میلی‌ولتی ولتاژ پک یک ردیف می‌سازد");
    {
        char char__ptr_tail[1024];
        (void)func__SimpleLog_Tail(char__ptr_tail, sizeof(char__ptr_tail), 0u, 10u);
        std::string string__last(char__ptr_tail);
        func__T_Check(string__last.find(",1024,") != std::string::npos,
                      "آن ردیف با بیت «گام ولتاژ» علامت می‌خورد");
    }

    /* ---------- v1.98: لاگ نباید خودش را پر کند ---------- */
    {
        const uint16_t uint16_t__before = UINT16_T__G__LogRowsToday;

        /* نوسان روی مرز قطع: ۲۱٬۱۵۰ ← ۲۱٬۲۵۰ ← ۲۱٬۱۸۰، همه داخل مرز */
        const uint32_t uint32_t__wobble[3] = { 21150u, 21250u, 21180u };
        for (uint8_t uint8_t__k = 0u; uint8_t__k < 3u; uint8_t__k++)
        {
            UINT32_T__G__TlmField[15] = uint32_t__wobble[uint8_t__k];
            UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
            func__SimpleLog_OnFrame();
        }
        func__T_Check(UINT16_T__G__LogRowsToday == uint16_t__before,
                      "نوسان باتری روی مرز قطع هیچ ردیفی نمی‌نویسد");

        /* خروج از مرز فقط بالای ۲۱٬۴۰۰: یک ردیف، نه یکی به ازای هر عبور */
        UINT32_T__G__TlmField[15] = 21500u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__before + 1u),
                      "خروج از مرز بالای ۲۱٬۴۰۰ دقیقاً یک ردیف می‌نویسد");

        /* بازهٔ مردهٔ هیسترزیس + گام‌های داخل یک دقیقه: بی‌صدا */
        const uint16_t uint16_t__after = UINT16_T__G__LogRowsToday;
        UINT32_T__G__TlmField[15] = 21300u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        UINT32_T__G__TlmField[15] = 21800u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == uint16_t__after,
                      "گام‌های پشت‌سرهم داخل یک دقیقه ردیف تازه نمی‌سازند");

        /* بعد از گذشتن دقیقه، گام دوباره می‌نویسد */
        UINT32_T__G__LogLastStepMs = (uint32_t)millis() - SIMPLE_LOG_VSTEP_MIN_MS - 10u;
        UINT32_T__G__TlmField[15] = 22100u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__after + 1u),
                      "بعد از گذشتن یک دقیقه، گام ولتاژ دوباره می‌نویسد");
    }

    /* ---- v1.109 (دستور کاربر: «توی دشارژ هر وقت اومد بالا باید یک رکورد
       ذخیره بشه»): در دشارژ هر بالا آمدنِ پک، حتی داخل یک دقیقه، ردیف خودش را
       می‌نویسد؛ پایین‌رفتن هنوز داخل دقیقه بی‌صداست. درصد باتری هم ستون آخر
       هر ردیف است و از نگاشت ۲۱٬۰۰۰..۲۹٬۰۰۰ حساب می‌شود. ---- */
    {
        /* نگاشت را صریح می‌کنیم: تستِ پارامترها قبلاً سقف نگاشت را جابه‌جا کرده */
        UINT32_T__G__BoardParam[SIMPLE_PARAM_PCT_VMIN_MV] = 21000u;
        BOOL__G__BoardParamKnown[SIMPLE_PARAM_PCT_VMIN_MV] = true;
        UINT32_T__G__BoardParam[SIMPLE_PARAM_PCT_VMAX_MV] = 29000u;
        BOOL__G__BoardParamKnown[SIMPLE_PARAM_PCT_VMAX_MV] = true;
        func__T_Check((func__SimpleLog_Pct(21000u) == 0u) &&
                      (func__SimpleLog_Pct(25000u) == 50u) &&
                      (func__SimpleLog_Pct(29000u) == 100u),
                      "درصد باتری از نگاشت ۲۱٬۰۰۰..۲۹٬۰۰۰ حساب می‌شود");

        const uint16_t uint16_t__before = UINT16_T__G__LogRowsToday;
        UINT8_T__G__TlmFlags = 0x00u;   /* برق شهر قطع = دشارژ */
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__before + 1u),
                      "قطع برق شهر برای شروع دشارژ یک ردیف می‌نویسد");

        /* بالا آمدنِ ۳۰۰ میلی‌ولتی، هنوز داخل همان دقیقهٔ گام قبلی */
        const uint16_t uint16_t__mid = UINT16_T__G__LogRowsToday;
        UINT32_T__G__TlmField[15] = UINT32_T__G__TlmField[15] + 300u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__mid + 1u),
                      "هر بالا آمدن در دشارژ، حتی داخل یک دقیقه، ردیف خودش را می‌نویسد");

        const uint32_t uint32_t__riseV = UINT32_T__G__TlmField[15];

        /* پایین‌رفتنِ همان اندازه، هنوز داخل دقیقه، بی‌صدا می‌ماند */
        const uint16_t uint16_t__mid2 = UINT16_T__G__LogRowsToday;
        UINT32_T__G__TlmField[15] = UINT32_T__G__TlmField[15] - 300u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == uint16_t__mid2,
                      "پایین‌رفتن در دشارژ هنوز داخل یک دقیقه ردیف نمی‌سازد");

        /* ستون آخر هر ردیف، درصد باتریِ همان لحظه است */
        std::string string__day;
        {
            File file__day = LittleFS.open(char__path, "r");
            while (file__day.available() > 0) {
                string__day += file__day.readStringUntil('\n').c_str();
                string__day += "\n";
            }
            file__day.close();
        }
        std::string string__last;
        {
            size_t size_t__cut = string__day.rfind('\n', string__day.size() - 2u);
            if (size_t__cut != std::string::npos) {
                string__last = string__day.substr(size_t__cut + 1u);
            }
        }
        while ((string__last.empty() == false) &&
               ((string__last.back() == '\n') || (string__last.back() == '\r')))
        {
            string__last.pop_back();
        }
        std::string string__want = "," + std::to_string(func__SimpleLog_Pct(uint32_t__riseV));
        func__T_Check((string__last.size() >= string__want.size()) &&
                      (string__last.rfind(string__want) == (string__last.size() - string__want.size())),
                      "ردیف با درصد باتریِ همان لحظه تمام می‌شود");

        /* برق شهر را برمی‌گردانیم تا وضعیت برای ادامهٔ تست‌ها مثل قبل بماند */
        UINT8_T__G__TlmFlags = 0x02u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
    }

    /* ---- v1.108 (دستور ۲۰۲۶-۱۰-۲۴: «لاگ باید کامل باشد»): قطع/وصل برق شهر
       هرگز پشت پنجرهٔ ادغام نمی‌ماند. برد ممکن است همان لحظهٔ قطع، برق پنل را
       ببرد؛ پس ردیف باید بی‌درنگ به فلش برسد - تست عمداً پنجره را باز نمی‌کند. */
    {
        const uint16_t uint16_t__before = UINT16_T__G__LogRowsToday;
        UINT8_T__G__TlmFlags = 0x00u;   /* برق شهر قطع شد */
        func__SimpleLog_OnFrame();      /* داخل پنجرهٔ ادغام، نگه داشته می‌شود */
        UINT32_T__G__LogMainsQueuedMs = (uint32_t)millis() - SIMPLE_LOG_MAINS_FLUSH_MS - 10u;
        func__SimpleLog_Tick();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__before + 1u),
                      "قطع برق شهر در پنجرهٔ کوتاه برق شهر می‌نشیند، نه پنجرهٔ ۲ثانیه‌ای");
        UINT8_T__G__TlmFlags = 0x02u;   /* برق شهر وصل شد */
        func__SimpleLog_OnFrame();
        UINT32_T__G__LogMainsQueuedMs = (uint32_t)millis() - SIMPLE_LOG_MAINS_FLUSH_MS - 10u;
        func__SimpleLog_Tick();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__before + 2u),
                      "وصل برق شهر هم به همان سرعت ردیف می‌نویسد");
    }

    /* ---- v1.106 (دستور ۲۰۲۶-۱۰-۲۴): لاگ با وصل‌نبودن برد هم می‌نویسد ----
       پنل شاهدِ خودش است: بوت، «برد نیست»، ضربان و «برد آمد» - همه بدون فریم. */
    {
        (void)func__SimpleLog_Clear();
        BOOL__G__TlmSeen = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        UINT16_T__G__LogPendingMask = 0u;
        BOOL__G__LogMainsQueuedValid = false;

        func__SimpleLog_Tick();
        func__T_Check(UINT16_T__G__LogRowsToday == 1u,
                      "با بردِ هرگز وصل‌نشده، ردیف بوت نوشته می‌شود");

        /* سکوت از شروع نشست: یک ردیف «اتصال قطع است» */
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        UINT32_T__G__LogSessionStartMs = (uint32_t)millis() - SIMPLE_LOG_LINK_TIMEOUT_MS - 10u;
        func__SimpleLog_Tick();
        func__T_Check(UINT16_T__G__LogRowsToday == 2u,
                      "با سکوتِ برد از همان شروع، ردیف «اتصال قطع است» نوشته می‌شود");
        func__T_Check(BOOL__G__LogLinkDown != false,
                      "وضعیت قطع ثبت می‌ماند تا با اولین فریم جفت شود");

        /* ضربان هم بدون برد ادامه می‌یابد */
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_HEARTBEAT_MS;
        func__SimpleLog_Tick();
        func__T_Check(UINT16_T__G__LogRowsToday == 3u,
                      "ضربان «زنده‌ام» هم بدون برد ادامه می‌یابد");

        /* اولین فریمِ دیررسیده: جفتِ همان قطع، «اتصال برقرار شد» */
        UINT32_T__G__TlmField[15] = 24000u;
        UINT8_T__G__TlmFlags = 0x02u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT16_T__G__LogRowsToday == 4u,
                      "اولین فریم، ردیف «اتصال برقرار شد» را جفتِ قطع می‌نویسد");
    }

    /* ضربان: در یک بازهٔ کامل هیچ اتفاقی نمی‌افتد، یک ردیف «زنده‌ام» نوشته می‌شود */
    {
        const uint16_t uint16_t__before = UINT16_T__G__LogRowsToday;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_HEARTBEAT_MS;
        UINT32_T__G__LastTlmMs = (uint32_t)millis();
        func__SimpleLog_Tick();
        func__T_Check(UINT16_T__G__LogRowsToday == (uint16_t)(uint16_t__before + 1u),
                      "بعد از یک بازهٔ آرام، یک ردیف «زنده‌ام» نوشته می‌شود");
        char char__ptr_tail[1024];
        (void)func__SimpleLog_Tail(char__ptr_tail, sizeof(char__ptr_tail), 0u, 10u);
        std::string string__beat(char__ptr_tail);
        func__T_Check(string__beat.find(",5,") != std::string::npos,
                      "آن ردیف یادداشت ضربان را دارد");
    }

    (void)func__SimpleLog_Clear();
    func__T_Check((UINT8_T__G__LogIndexCount == 0u) && (!LittleFS.exists(char__path)),
                  "پاک کردن، فایل روزها را برمی‌دارد");


    /* بودجهٔ حجم: روزهای بزرگ‌تر از بودجه، از قدیمی‌ترین حذف می‌شوند */
    {
        const uint32_t uint32_t__days[3] = { 20261001u, 20261002u, 20261003u };
        std::string string__big(80000u, 'x');
        UINT8_T__G__LogIndexCount = 0u;
        for (uint8_t uint8_t__k = 0u; uint8_t__k < 3u; uint8_t__k++)
        {
            char char__ptr_day[SIMPLE_LOG_PATH_MAX];
            func__SimpleLog_PathOf(char__ptr_day, uint32_t__days[uint8_t__k]);
            File file__big = LittleFS.open(char__ptr_day, "w");
            if (file__big) { file__big.print(string__big.c_str()); file__big.close(); }
            UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount] = uint32_t__days[uint8_t__k];
            UINT8_T__G__LogIndexCount++;
        }
        func__SimpleLog_TrimDays();
        func__T_Check(UINT8_T__G__LogIndexCount == 1u,
                      "روزهای بیشتر از بودجهٔ حجم، از قدیمی‌ترین حذف می‌شوند");
        func__T_Check(UINT32_T__G__LogIndexDay[0] == 20261003u,
                      "روزی که می‌ماند جدیدترین است");
        for (uint8_t uint8_t__i = 0u; uint8_t__i < 3u; uint8_t__i++)
        {
            char char__ptr_day[SIMPLE_LOG_PATH_MAX];
            func__SimpleLog_PathOf(char__ptr_day, uint32_t__days[uint8_t__i]);
            (void)LittleFS.remove(char__ptr_day);
        }

    /* بدون ساعت هم ذخیره می‌شود: گوشی ممکن است ساعت‌ها بعد وصل شود */
    {
        (void)func__SimpleLog_Clear();
        (void)LittleFS.remove("/evt-00000000.csv");
        BOOL__G__TimeValid = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__TlmFlags = 0x02u;
        UINT32_T__G__TlmField[15] = 23800u;

        func__SimpleLog_Tick();
        func__T_Check(LittleFS.exists("/evt-00000000.csv"),
                      "بدون ساعت هم اولین ردیف در فایل روزِ «بدون ساعت» ذخیره می‌شود");
        func__SimpleLog_OnFrame();   /* کاشتن وضعیت قبلی برای تشخیص تغییر */

        func__Simple_TimeSet(1792756800u, false);
        UINT8_T__G__TlmFlags = 0x00u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT8_T__G__LogIndexCount == 2u,
                      "بعد از رسیدن ساعت، فایل روزِ امروز هم به فهرست اضافه می‌شود");
        func__T_Check(LittleFS.exists("/evt-00000000.csv"),
                      "فایل «بدون ساعت» دور ریخته نمی‌شود");
    }
    }
}

int main(void)
{
    (void)printf("== تست هاست صف ریست قفل پنل ساده ==\n");

    func__T_TestResetOrderAndHandshake();
    func__T_TestQueue();
    func__T_TestSender();
    func__T_TestEchoAndNak();
    func__T_TestWifiPowerSave();
    func__T_TestStm32TimeAndChartScheduling();
    func__T_TestDiagStatePersistenceAndClear();
    func__T_TestAuthAdminProtection();
    func__T_TestStateHistoryLog();

    (void)printf("\n== بررسی‌ها: %u، شکست‌ها: %u ==\n", (unsigned)UINT32_T__G__Checks, (unsigned)UINT32_T__G__Failures);
    if (UINT32_T__G__Failures != 0u)
    {
        (void)printf("تست هاست صف ریست: رد\n");
        return 1;
    }
    (void)printf("تست هاست صف ریست: قبول\n");
    return 0;
}
