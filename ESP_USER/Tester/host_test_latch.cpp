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
#include "../simple_panel.h"
#include "../simple_link.h"
#include "../simple_auth.h"
#include "../simple_theme.h"
#include "../simple_diag.h"
#include "../simple_http.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
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
bool SimLittleFS::format(void)
{
    static const std::string PREFIX = "changeover_simple_test_";
    DIR *dir__handle = ::opendir("/tmp");
    if (dir__handle != NULL)
    {
        struct dirent *entry__item;
        while ((entry__item = ::readdir(dir__handle)) != NULL)
        {
            std::string string__name(entry__item->d_name);
            if (string__name.compare(0u, PREFIX.size(), PREFIX) != 0)
            {
                continue;
            }
            std::string string__victim = std::string("/tmp/") + string__name;
            (void)::unlink(string__victim.c_str());
        }
        ::closedir(dir__handle);
    }
    return true;
}
bool SimLittleFS::info(FSInfo &fsInfo__out)
{
    static const std::string PREFIX = "changeover_simple_test_";
    size_t size_t__used = 0u;
    DIR *dir__handle = ::opendir("/tmp");
    if (dir__handle != NULL)
    {
        struct dirent *entry__item;
        while ((entry__item = ::readdir(dir__handle)) != NULL)
        {
            std::string string__name(entry__item->d_name);
            if (string__name.compare(0u, PREFIX.size(), PREFIX) != 0)
            {
                continue;
            }
            struct stat struct_stat__st;
            std::string string__p = std::string("/tmp/") + string__name;
            if (::stat(string__p.c_str(), &struct_stat__st) == 0)
            {
                size_t__used += (size_t)struct_stat__st.st_size;
            }
        }
        ::closedir(dir__handle);
    }
    fsInfo__out.totalBytes = 2097152u;
    fsInfo__out.usedBytes = size_t__used;
    return true;
}
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

/* فهرست پوشه: همین پوشهٔ /tmp که فایل‌های تست در آن هستند، بی زیرپوشه. */
SimDir SimLittleFS::openDir(const char *char__ptr_path)
{
    static const std::string PREFIX = "changeover_simple_test_";
    SimDir dir__result;
    DIR *dir__handle = ::opendir("/tmp");

    (void)char__ptr_path;
    if (dir__handle == NULL)
    {
        return dir__result;
    }
    {
        struct dirent *entry__item;

        while ((entry__item = ::readdir(dir__handle)) != NULL)
        {
            std::string string__name(entry__item->d_name);

            if (string__name.compare(0u, PREFIX.size(), PREFIX) != 0)
            {
                continue;
            }
            struct stat struct_stat__st;
            std::string string__full = std::string("/tmp/") + string__name;
            size_t size_t__sz = (::stat(string__full.c_str(), &struct_stat__st) == 0)
                ? (size_t)struct_stat__st.st_size : 0u;
            std::string string__base = string__name.substr(PREFIX.size());
            /* مسیر داخل فلش با «/» شروع می‌شود، مگر در تست نام بدون خط مورب */
            dir__result.names.push_back(slashlessDirNames ? string__base : ("/" + string__base));
            dir__result.sizes.push_back(size_t__sz);
        }
    }
    ::closedir(dir__handle);
    return dir__result;
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

/* ==================== توان آنتن روی باتری: فقط وقتی کسی تماشا می‌کند (v1.112) ====================
   دستور کاربر ۲۰۲۶-۱۰-۲۵: «می‌توانی توان وای‌فای را کم کنی».
   چرا این تست: ایراد واقعی این بود که گوشیِ وصل‌شده به شبکه (حتی با پنل بسته)
   تا بی‌نهایت «کاربر» حساب می‌شد و توان آنتن روی بیشینه می‌ماند. اکنون توان
   بیشینه فقط تا وقتی است که درخواست HTTP تازه بیاید یا کاربر تازه وصل شده باشد. */
static void func__T_TestWifiViewerPower(void)
{
    uint32_t uint32_t__t = 5000000u;

    (void)printf("\n== توان آنتن روی باتری: فقط با تماشاگر (v1.112) ==\n");

    /* آماده‌سازی: روی باتری، باتری ۵۰٪، یک گوشی به شبکه وصل، هیچ درخواستی نیامده */
    BOOL__G__TlmSeen = true;
    UINT8_T__G__TlmFlags = 0u;
    UINT32_T__G__TlmField[15] = 25000u;
    WiFi.setStationNum(1);
    (void)WiFi.softAP(CHAR__G__WifiSsid, CHAR__G__WifiPass);
    WiFi.setOutputPower(SIMPLE_WIFI_TX_MAX_DBM);
    BOOL__G__WifiApActive = true;
    BOOL__G__WifiHighPower = true;
    BOOL__G__WifiBatCutoffLatched = false;
    BOOL__G__WifiBatteryIdleTracking = false;
    BOOL__G__WifiLastReportedConnected = false;
    BOOL__G__BrowserSeen = false;
    UINT32_T__G__WifiStationSinceMs = 0u;

    /* ۱. کاربر تازه وصل شده: تا ۱۰ دقیقه فرصت دارد پنل را باز کند */
    func__Simple_WifiPowerSaveTick(uint32_t__t);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() > 20.0f),
                  "کاربری که تازه به شبکه وصل شد، ۱۰ دقیقه توان بیشینه دارد");

    /* ۲. ۱۱ دقیقه گذشت و هیچ درخواستی نیامد: وای‌فای روشن می‌ماند ولی توان کم */
    uint32_t__t += 660000u;
    func__Simple_WifiPowerSaveTick(uint32_t__t);
    func__T_Check(WiFi.isApActive() && (WiFi.outputPowerDbm() < 0.1f),
                  "گوشیِ وصل ولی بدون باز کردن پنل: وای‌فای روشن، توان کم");

    /* ۳. کاربر پنل را باز کرد (درخواست HTTP): توان فوراً بیشینه */
    BOOL__G__BrowserSeen = true;
    UINT32_T__G__LastBrowserPollMs = uint32_t__t;
    func__Simple_WifiPowerSaveTick(uint32_t__t + 1000u);
    func__T_Check(WiFi.outputPowerDbm() > 20.0f,
                  "با باز شدن پنل: توان آنتن فوراً بیشینه می‌شود");

    /* ۴. پنل بسته شد و ۱۱ دقیقه خبری نشد: دوباره کم */
    uint32_t__t += 1000u + 660000u;
    func__Simple_WifiPowerSaveTick(uint32_t__t);
    func__T_Check(WiFi.outputPowerDbm() < 0.1f,
                  "۱۰ دقیقه پس از بستن پنل: توان دوباره کم می‌شود");

    /* ۵. با برق شهر همیشه بیشینه، حتی بدون تماشاگر */
    UINT8_T__G__TlmFlags = SIMPLE_TLM_FLAG_INPUT_PRESENT;
    func__Simple_WifiPowerSaveTick(uint32_t__t + 1000u);
    func__T_Check(WiFi.outputPowerDbm() > 20.0f,
                  "با برق شهر: توان همیشه بیشینه است (هزینهٔ باتری ندارد)");

    /* ۶. بازگشت به باتری و تازه‌واردِ جدید: باز هم بیشینه */
    UINT8_T__G__TlmFlags = 0u;
    WiFi.setStationNum(0);
    func__Simple_WifiPowerSaveTick(uint32_t__t + 2000u);
    WiFi.setStationNum(1);
    func__Simple_WifiPowerSaveTick(uint32_t__t + 3000u);
    func__T_Check(WiFi.outputPowerDbm() > 20.0f,
                  "اتصال دوبارهٔ گوشی روی باتری: باز هم ۱۰ دقیقه بیشینه");
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

    /* ---- v1.112: یک نوشتنِ ردشده کل فایل روز را پنهان نمی‌کند ---- */
    {
        char char__ptr_day[SIMPLE_LOG_PATH_MAX];
        char char__ptr_text[4096];
        uint32_t uint32_t__day = func__SimpleLog_TodayCode();
        uint8_t uint8_t__i;
        uint32_t uint32_t__lines = 0u;

        func__SimpleLog_PathOf(char__ptr_day, uint32_t__day);
        (void)LittleFS.remove(char__ptr_day);
        for (uint8_t__i = 0u; uint8_t__i < 3u; uint8_t__i++)
        {
            char char__ptr_row[128];
            (void)snprintf(char__ptr_row, sizeof(char__ptr_row),
                           "179275680%d,10,1,0,1,2,24000,22000,26000,0,0,0,0,0,0,0,0,0,0,0,0,0",
                           (int)uint8_t__i);
            (void)func__SimpleLog_AppendRow(uint32_t__day, char__ptr_row);
        }
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        (void)func__SimpleLog_RamPush("1792756809,14,64,0,1,2,24000,22000,26000,0,0,0,0,0,0,0,0,0,0,0,0,0");
        (void)func__SimpleLog_RamPush("1792756810,15,512,0,1,2,24000,22000,26000,0,0,0,0,0,0,0,0,0,0,0,0,0");
        UINT8_T__G__LogWriteFault = 1u;

        (void)func__SimpleLog_Tail(char__ptr_text, sizeof(char__ptr_text), uint32_t__day, 30u);
        {
            size_t size_t__i;
            for (size_t__i = 0u; char__ptr_text[size_t__i] != '\0'; size_t__i++)
            {
                if (char__ptr_text[size_t__i] == '\n') { uint32_t__lines++; }
            }
        }
        func__T_Check(uint32_t__lines == 5u,
                      "کارت لاگ سه ردیف فایل و دو ردیف رم را با هم نشان می‌دهد");
        func__T_Check(char__ptr_text[0] == '1',
                      "اولین ردیف کارت، قدیمی‌ترین ردیف فایل است");

        /* روزِ بی‌ردیف باید خالی بماند، نه اینکه از رم چیزی بسازد */
        char__ptr_text[0] = '\0';
        (void)func__SimpleLog_Tail(char__ptr_text, sizeof(char__ptr_text), 20260101u, 30u);
        func__T_Check(char__ptr_text[0] == '\0',
                      "روزِ بی‌ردیف خالی می‌ماند و از رم ردیف نمی‌سازد");

        /* فایل‌سیستم خاموش: فهرست روزها باز هم امروز را نام می‌برد */
        UINT8_T__G__LogRamCount = 0u;
        UINT8_T__G__LogWriteFault = 0u;
        BOOL__G__FsOk = false;
        (void)func__SimpleLog_Days(char__ptr_text, sizeof(char__ptr_text));
        {
            char char__ptr_want[16];
            (void)snprintf(char__ptr_want, sizeof(char__ptr_want), "%08lu",
                           (unsigned long)uint32_t__day);
            func__T_Check(strcmp(char__ptr_text, char__ptr_want) == 0,
                          "با فایل‌سیستم خاموش، فهرست روزها باز هم امروز را نام می‌برد");
        }
        BOOL__G__FsOk = true;

        /* پاک‌کردن وضعیت برای تست‌های بعدی */
        UINT8_T__G__LogWriteFault = 0u;
        (void)LittleFS.remove(char__ptr_day);
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
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
        {
            char char__zeroTail[512];
            (void)func__SimpleLog_Tail(char__zeroTail, sizeof(char__zeroTail), SIMPLE_LOG_DAY_ZERO, 10u);
            std::string string__zero(char__zeroTail);
            func__T_Check(string__zero.rfind("0,", 0u) == 0u,
                          "خواندن صریح روز صفر (00000000) حتی پس از ساخته‌شدن فایل امروز همان فایل بدون ساعت را برمی‌گرداند");
        }
        {
            BOOL__G__FsOk = false;
            UINT8_T__G__LogRamHead = 0u;
            UINT8_T__G__LogRamCount = 0u;
            BOOL__G__LogHasPrev = false;
            BOOL__G__LogBootRowPending = true;
            BOOL__G__LogLastRowValid = false;
            BOOL__G__LogDayFull = false;
            UINT16_T__G__LogRowsToday = 0u;
            UINT16_T__G__LogPendingMask = 0u;
            func__SimpleLog_Tick();
            char char__ramTail[512];
            (void)func__SimpleLog_Tail(char__ramTail, sizeof(char__ramTail), SIMPLE_LOG_DAY_AUTO, 10u);
            func__T_Check(strlen(char__ramTail) > 0u,
                          "در صورت غیرفعال بودن فایل‌سیستم فلش هم رویدادها در حلقهٔ رم ثبت و خوانده می‌شوند");
            (void)func__SimpleLog_Clear();
            BOOL__G__FsOk = true;
        }
    }

    /* فلشی که نوشتن را رد می‌کند (۲۰۲۶-۱۰-۲۷): همان ایراد پنل فنی. ردیف
       نباید گم شود، خرابی باید گزارش شود، و اولین نوشتن سالم گزارش را پاک
       کند. */
    {
        (void)func__SimpleLog_Clear();
        (void)LittleFS.remove("/evt-00000000.csv");
        SIM_BOOL__G__RefuseWrites = true;
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        BOOL__G__LogLinkDown = false;
        UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__TlmFlags = 0x02u;
        UINT32_T__G__TlmField[15] = 24000u;
        func__Simple_TimeSet(1792756800u, false);

        func__SimpleLog_Tick();
        func__T_Check(UINT8_T__G__LogWriteFault != 0u,
                      "نوشتن ردشده روی فلش گزارش می‌شود (شمارندهٔ خطا بالا می‌رود)");
        func__T_Check(UINT8_T__G__LogRamCount == 1u,
                      "ردیفی که فلش نپذیرفت در حلقهٔ رم می‌ماند و گم نمی‌شود");
        {
            char char__refused[512];
            (void)func__SimpleLog_Tail(char__refused, sizeof(char__refused), SIMPLE_LOG_DAY_AUTO, 10u);
            func__T_Check(strlen(char__refused) > 0u,
                          "خواندن تاریخچه در همین حالت ردیف را نشان می‌دهد، نه کارت خالی");
        }

        SIM_BOOL__G__RefuseWrites = false;
        UINT8_T__G__TlmFlags = 0x00u;
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();   /* کاشتن وضعیت قبلی: برق شهر قطع */
        UINT8_T__G__TlmFlags = 0x02u;   /* برق شهر آمد - یک تغییر واقعی */
        UINT32_T__G__LogLastRowMs = (uint32_t)millis() - SIMPLE_LOG_MIN_INTERVAL_MS - 10u;
        func__SimpleLog_OnFrame();
        func__T_Check(UINT8_T__G__LogWriteFault == 0u,
                      "اولین نوشتن سالم، گزارش خرابی را پاک می‌کند");
        func__T_Check(UINT16_T__G__LogRowsToday >= 1u,
                      "و آن ردیف واقعاً در فایل روز نشست");
        (void)func__SimpleLog_Clear();
    }

    /* روز شلوغ: جدیدترین ردیف‌ها باید بیایند، نه قدیمی‌ترین‌ها */
    {
        const uint32_t uint32_t__busy = 20261005u;
        char char__busyPath[SIMPLE_LOG_PATH_MAX];
        func__SimpleLog_PathOf(char__busyPath, uint32_t__busy);
        (void)LittleFS.remove(char__busyPath);
        {
            File file__busy = LittleFS.open(char__busyPath, "w");
            std::string string__rows = SIMPLE_LOG_HEADER;
            if (file__busy)
            {
                file__busy.print(string__rows.c_str());
                for (int int__k = 1; int__k <= 40; int__k++)
                {
                    char char__one[220];
                    snprintf(char__one, sizeof(char__one),
                             "%lu,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d\n",
                             4000000000UL + (unsigned long)int__k, 4000000000UL,
                             (unsigned)1024, (unsigned)(100 + int__k), (unsigned)1,
                             (unsigned)255, 4294967295UL, 4294967295UL, 4294967295UL,
                             (unsigned)255, 4294967295UL, 4294967295UL, 4294967295UL,
                             4294967295UL, 4294967295UL, 4294967295UL, 4294967295UL,
                             4294967295UL, 4294967295UL, 4294967295UL, 4294967295UL,
                             int__k);
                    file__busy.print(char__one);
                }
                file__busy.close();
            }
        }
        {
            char char__busyTail[512];
            std::string string__got;
            size_t size_t__lastBreak;

            (void)func__SimpleLog_Tail(char__busyTail, sizeof(char__busyTail), uint32_t__busy, 30u);
            string__got = char__busyTail;
            func__T_Check(string__got.size() <= sizeof(char__busyTail),
                          "پاسخ تاریخچه در بافر خودش می‌ماند");
            func__T_Check(string__got.find(",140,") != std::string::npos,
                          "جدیدترین ردیف روز شلوغ در پاسخ هست");
            func__T_Check(string__got.find(",101,") == std::string::npos,
                          "قدیمی‌ترین ردیف روز شلوغ دور ریخته می‌شود، نه جدیدترین");
            std::string string__lastLine = string__got;
            while ((!string__lastLine.empty()) &&
                   (string__lastLine[string__lastLine.size() - 1u] == '\n'))
            {
                string__lastLine.erase(string__lastLine.size() - 1u);
            }
            size_t__lastBreak = string__lastLine.rfind('\n');
            if (size_t__lastBreak != std::string::npos)
            {
                string__lastLine = string__lastLine.substr(size_t__lastBreak + 1u);
            }
            func__T_Check(string__lastLine.find(",140,") != std::string::npos,
                          "آخرین خط پاسخ همان جدیدترین ردیف روز است (کامل، نه نصفه)");
        }
        (void)LittleFS.remove(char__busyPath);
    }

    /* ردیف‌های منتظر در رم، با سالم شدن فلش به فایل روز خودشان برمی‌گردند */
    {
        char char__dayPath[SIMPLE_LOG_PATH_MAX];
        (void)func__SimpleLog_Clear();
        (void)LittleFS.remove("/evt-00000000.csv");
        func__Simple_TimeSet(1792756800u, false);
        INT32_T__G__LogTzSec = 12600;
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogHasPrev = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        UINT16_T__G__LogPendingMask = 0u;
        UINT8_T__G__TlmFlags = 0x02u;
        UINT32_T__G__TlmField[15] = 24000u;
        func__SimpleLog_PathOf(char__dayPath, func__SimpleLog_TodayCode());
        (void)LittleFS.remove(char__dayPath);

        SIM_BOOL__G__RefuseWrites = true;
        func__SimpleLog_Tick();
        SIM_BOOL__G__RefuseWrites = false;
        func__T_Check(UINT8_T__G__LogRamCount == 1u,
                      "ردیفِ ردشده در رم منتظر می‌ماند");

        /* قبل از رسیدن فاصلهٔ ۳۰ ثانیه‌ای، Tick به فلش نمی‌کوبد */
        func__SimpleLog_Tick();
        func__T_Check(UINT8_T__G__LogRamCount == 1u,
                      "پیش از گذشتن مهلت خنک‌شدن ۳۰ ثانیه‌ای، حلقهٔ اصلی به فلش نمی‌کوبد");

        /* پس از گذشتن فاصلهٔ مجاز، فلش سالم شده و Tick ردیف را برمی‌گرداند */
        UINT32_T__G__LogLastDrainMs = (uint32_t)millis() - SIMPLE_LOG_DRAIN_RETRY_MS - 10u;
        func__SimpleLog_Tick();
        func__T_Check(UINT8_T__G__LogRamCount == 0u,
                      "با سالم شدن فلش، ردیف منتظر به فایل روز برگردانده می‌شود");
        func__T_Check(LittleFS.exists(char__dayPath),
                      "و فایل همان روز ساخته می‌شود");
        {
            char char__drained[512];
            (void)func__SimpleLog_Tail(char__drained, sizeof(char__drained), SIMPLE_LOG_DAY_AUTO, 10u);
            func__T_Check(strlen(char__drained) > 0u,
                          "و همان ردیف از فلش خوانده می‌شود");
        }

        /* حلقهٔ رم پر شود: ردیف‌های دورریخته شمرده می‌شوند */
        SIM_BOOL__G__RefuseWrites = true;
        {
            SIMPLE_WEB_SERVER_T__G__Server.simClear();
            func__Simple_HttpEventLog();
            func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simHeader("X-Log-Lost") == "0",
                          "در حالت سالم، شمار ردیف‌های ازدست‌رفته صفر است");
        }
        for (uint8_t uint8_t__k = 0u; uint8_t__k < (SIMPLE_LOG_RAM_ROWS + 3u); uint8_t__k++)
        {
            (void)func__SimpleLog_WriteRow(0u, SIMPLE_LOG_NOTE_NONE);
        }
        func__T_Check(UINT8_T__G__LogRamCount == SIMPLE_LOG_RAM_ROWS,
                      "حلقهٔ رم تا سقف خودش پر می‌شود");
        func__T_Check(UINT16_T__G__LogDroppedRows == 3u,
                      "هر ردیفی که حلقه ناچار دور ریخت شمرده می‌شود");
        {
            SIMPLE_WEB_SERVER_T__G__Server.simClear();
            func__Simple_HttpEventLog();
            func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simHeader("X-Log-Lost") == "3",
                          "و صفحه شمار ردیف‌های دورریخته را می‌بیند");
        }
        SIM_BOOL__G__RefuseWrites = false;
        UINT8_T__G__LogRamHead = 0u;
        UINT8_T__G__LogRamCount = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        UINT8_T__G__LogWriteFault = 0u;
        (void)func__SimpleLog_Clear();
    }

    /* دنبالهٔ خراب: ردیفی که پایان‌بند ندارد، ردیف بعدی را نمی‌خورد */
    {
        char char__cutPath[SIMPLE_LOG_PATH_MAX];
        (void)func__SimpleLog_Clear();
        func__Simple_TimeSet(1792756800u, false);
        INT32_T__G__LogTzSec = 12600;
        BOOL__G__FsOk = true;
        UINT8_T__G__LogWriteFault = 0u;
        UINT8_T__G__LogIndexCount = 0u;
        UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        BOOL__G__LogMarkerDone = false;
        BOOL__G__LogBootRowPending = true;
        BOOL__G__LogLastRowValid = false;
        UINT16_T__G__LogPendingMask = 0u;
        func__SimpleLog_PathOf(char__cutPath, func__SimpleLog_TodayCode());
        (void)LittleFS.remove(char__cutPath);
        {
            File file__cut = LittleFS.open(char__cutPath, "w");
            if (file__cut)
            {
                file__cut.print(SIMPLE_LOG_HEADER);
                file__cut.print("1792756000,1,1024,0,1,0,0,0,0,0,23900,12000,11900,0,0,0,0,0,0,0,0,36");
                file__cut.close();
            }
        }
        func__SimpleLog_Tick();
        {
            char char__cutTail[512];
            (void)func__SimpleLog_Tail(char__cutTail, sizeof(char__cutTail), SIMPLE_LOG_DAY_AUTO, 10u);
            std::string string__cut(char__cutTail);
            func__T_Check(string__cut.find(",23900,") != std::string::npos,
                          "ردیفِ نیمه‌نوشتهٔ پیش از قطع برق خوانا می‌ماند");
            func__T_Check(string__cut.find(",36\n") != std::string::npos,
                          "و پایان‌بندش نوشته می‌شود تا ردیف بعدی به آن نچسبد");
        }
        (void)func__SimpleLog_Clear();
        (void)LittleFS.remove(char__cutPath);
    }
    }
}

/* ==================== دست‌دادن دوطرفه با برد (v1.111) ====================
   چرا این بخش: تا امروز تنها نشانهٔ ارتباط، آمدن تله‌متری بود. آن فقط می‌گوید
   برد حرف می‌زند؛ نمی‌گوید ESP هم به برد می‌رسد و نمی‌گوید ارتباط «درست» است.
   اکنون ESP هر ۲ ثانیه HELLO می‌فرستد و برد با HELLO_ACK جواب می‌دهد و در آن
   سلامت دریافت خودش را می‌گوید. اینجا همان توابع خود برنامه صدا زده می‌شوند:
   - قاب HELLO بایت‌به‌بایت (SOF، نسخه، نوع ۰x40، طول ۸، CRC) بررسی می‌شود.
   - رسیدن پاسخ: وضعیت «دوطرفه سالم» و مقدارهای گزارش برد ثبت می‌شوند.
   - سکوت برد: بعد از مهلت، ارتباط قطع اعلام می‌شود و شمرده می‌شود.
   اجرا: bash ESP_USER/Tester/run_simple_tests.sh */
static void func__T_TestHandshake(void)
{
    uint8_t uint8_t__frame[64];
    uint8_t uint8_t__payload[SIMPLE_HELLO_ACK_LEN];
    uint8_t uint8_t__byte;
    uint16_t uint16_t__len;
    uint16_t uint16_t__i;
    uint16_t uint16_t__crc = SIMPLE_LINK_CRC16_INIT;

    (void)printf("\n== دست‌دادن دوطرفه با برد (v1.111) ==\n");

    /* ۱) قاب HELLO که ESP می‌فرستد */
    while (Serial.takeEspByte(&uint8_t__byte)) { }
    func__Simple_SendHello();
    uint16_t__len = 0u;
    while (Serial.takeEspByte(&uint8_t__byte))
    {
        if (uint16_t__len < (uint16_t)sizeof(uint8_t__frame))
        {
            uint8_t__frame[uint16_t__len] = uint8_t__byte;
        }
        uint16_t__len++;
    }
    uint16_t__len = (uint16_t)(SIMPLE_LINK_HEADER_SIZE + SIMPLE_HELLO_LEN + SIMPLE_LINK_CRC_SIZE);
    func__T_Check(uint8_t__frame[0] == SIMPLE_LINK_SOF_BYTE0 &&
                  uint8_t__frame[1] == SIMPLE_LINK_SOF_BYTE1,
                  "قاب دست‌دادن با SOF درست شروع می‌شود");
    func__T_Check(uint8_t__frame[2] == (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION,
                  "نسخهٔ سیم در قاب دست‌دادن درست است");
    func__T_Check(uint8_t__frame[3] == (uint8_t)SIMPLE_MSG_HELLO,
                  "نوع پیام ۰x40 (HELLO) است");
    func__T_Check(uint8_t__frame[4] == 8u && uint8_t__frame[5] == 0u,
                  "طول پیام دست‌دادن ۸ بایت است");
    func__T_Check(uint8_t__frame[6] == (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION &&
                  uint8_t__frame[7] == (uint8_t)SIMPLE_ESP_FW_VERSION &&
                  uint8_t__frame[8] == (uint8_t)SIMPLE_ESP_ROLE_USER,
                  "پیام دست‌دادن نسخه، نسخهٔ پنل و نقش را می‌برد");
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, (uint8_t)SIMPLE_MSG_HELLO);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 8u);
    uint16_t__crc = func__Simple_Crc16(uint16_t__crc, 0u);
    for (uint16_t__i = 0u; uint16_t__i < (uint16_t)SIMPLE_HELLO_LEN; uint16_t__i++)
    {
        uint16_t__crc = func__Simple_Crc16(uint16_t__crc, uint8_t__frame[SIMPLE_LINK_HEADER_SIZE + uint16_t__i]);
    }
    func__T_Check(uint8_t__frame[uint16_t__len - 2u] == (uint8_t)(uint16_t__crc & 0xFFu) &&
                  uint8_t__frame[uint16_t__len - 1u] == (uint8_t)((uint16_t__crc >> 8) & 0xFFu),
                  "CRC قاب دست‌دادن همان چیزی است که برد محاسبه می‌کند");

    /* ۲) پاسخ برد: هویت، سلامت دریافت خودش و بازپس آپتایم ما */
    for (uint16_t__i = 0u; uint16_t__i < (uint16_t)SIMPLE_HELLO_ACK_LEN; uint16_t__i++)
    {
        uint8_t__payload[uint16_t__i] = 0u;
    }
    uint8_t__payload[0] = (uint8_t)SIMPLE_LINK_PROTOCOL_VERSION;
    uint8_t__payload[1] = 111u;              /* نسخهٔ فرم‌ور برد */
    uint8_t__payload[2] = 0x0Fu;             /* همهٔ ماژول‌ها */
    uint8_t__payload[8] = 0x34u;             /* فریم سالم از دید برد = 52 */
    uint8_t__payload[12] = 0u;               /* خطای CRC برد = 0 */
    uint8_t__payload[13] = 0u;
    uint8_t__payload[16] = (uint8_t)(169u & 0xFFu);  /* تعداد پارامتر */
    uint8_t__payload[18] = (uint8_t)(100u & 0xFFu);  /* دورهٔ تله‌متری ۱۰۰ms */
    uint16_t__len = func__T_MakeFrame(uint8_t__frame, SIMPLE_MSG_HELLO_ACK,
                                      uint8_t__payload, (uint16_t)SIMPLE_HELLO_ACK_LEN);
    Serial.boardToEsp(uint8_t__frame, (size_t)uint16_t__len);
    func__Simple_DrainSerial();

    func__T_Check(BOOL__G__HelloSeen, "با رسیدن پاسخ برد، ارتباط دوطرفه سالم اعلام می‌شود");
    func__T_Check(UINT8_T__G__BoardFwVersion == 111u, "نسخهٔ فرم‌ور برد از پاسخ خوانده می‌شود");
    func__T_Check(UINT32_T__G__BoardRxFrames == 52u, "تعداد فریم سالمِ دیده‌شده توسط برد خوانده می‌شود");
    func__T_Check(UINT16_T__G__BoardCrcErrors == 0u && UINT16_T__G__BoardVerErrors == 0u,
                  "خطای CRC و ناهم‌نسخگی برد (صفر = ارتباط درست) خوانده می‌شود");
    func__T_Check(UINT16_T__G__BoardParamCount == 169u && UINT16_T__G__BoardPeriodMs == 100u,
                  "تعداد پارامتر و دورهٔ تله‌متری برد خوانده می‌شود");
    func__T_Check(UINT32_T__G__HelloAckCount == 1u, "پاسخ‌های برد شمرده می‌شوند");

    /* ۳) سکوت برد: بعد از مهلت، ارتباط قطع اعلام و شمرده می‌شود */
    UINT32_T__G__LastHelloAckMs = (uint32_t)millis() - SIMPLE_HELLO_TIMEOUT_MS - 10u;
    func__Simple_HandshakeTick((uint32_t)millis());
    func__T_Check(!BOOL__G__HelloSeen, "با سکوت برد، ارتباط دوطرفه قطع اعلام می‌شود");
    func__T_Check(UINT32_T__G__HelloLossCount == 1u, "قطع ارتباط یک بار شمرده می‌شود");

    /* ۴) پاسخ با طول غلط نباید ارتباط بسازد */
    UINT32_T__G__LastHelloAckMs = (uint32_t)millis() - SIMPLE_HELLO_TIMEOUT_MS - 10u;
    func__Simple_HandshakeTick((uint32_t)millis());
    uint16_t__len = func__T_MakeFrame(uint8_t__frame, SIMPLE_MSG_HELLO_ACK, uint8_t__payload, 8u);
    Serial.boardToEsp(uint8_t__frame, (size_t)uint16_t__len);
    func__Simple_DrainSerial();
    func__T_Check(!BOOL__G__HelloSeen, "پاسخ با طول غلط نادیده گرفته می‌شود");
}

/* ---- v1.113: فایل روزی که فهرست نمی‌شناخت، و ردیف بروزرسانی فرم‌ور ----
   [EN] Two user reports of 2026-10-28: rows that the clear button could not
        delete, and one record per firmware update with no conditions.
   [FA] دو گزارش کاربر ۲۰۲۶-۱۰-۲۸: ردیف‌هایی که دکمهٔ پاک‌کردن نمی‌توانست
        حذفشان کند، و یک رکورد برای هر بروزرسانی فرم‌ور، بدون هیچ شرطی. */
static void func__T_TestClearOrphansAndUpdateRow(void)
{
    (void)printf("\n-- v1.113: فایل روزِ بیرون از فهرست و ردیف بروزرسانی --\n");

    /* ۱) فایل روزی که هیچ فهرستی آن را به یاد ندارد (همان حالتی که روی برد
          دیده شد: ردیف روی فلش هست، در فهرست نیست و پاک نمی‌شود). */
    (void)func__SimpleLog_Clear();
    {
        File file__orphan = LittleFS.open("/evt-20250101.csv", "w");

        if (file__orphan) {
            file__orphan.print("# روز قدیمی\n100,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n");
            file__orphan.close();
        }
    }
    (void)func__SimpleLog_ScanDays(true);
    {
        char char__days[128];

        char__days[0] = '\0';
        (void)func__SimpleLog_Days(char__days, sizeof char__days);
        func__T_Check(std::string(char__days).find("20250101") != std::string::npos,
                      "فایل روزی که در فهرست نبود، حالا در فهرست روزها دیده می‌شود");
        func__T_Check(func__SimpleLog_Clear() == 1u,
                      "همان فایل روز با «پاک کردن تاریخچه» پاک می‌شود");
        func__T_Check(!LittleFS.exists("/evt-20250101.csv"),
                      "و بعد از پاک‌کردن دیگر روی فلش نیست");
    }

    /* ۲) ردیف بروزرسانی: مهر نسخهٔ کهنه روی فلش، فرم‌ور در حال اجرا تازه */
    (void)func__SimpleLog_Clear();
    {
        File file__time = LittleFS.open(SIMPLE_TIME_FILE, "w");

        if (file__time) {
            file__time.print("{\"ts\":0,\"tz\":0,\"fw\":1}");
            file__time.close();
        }
    }
    BOOL__G__LogUpdatePending = false;
    func__SimpleLog_Begin();
    func__T_Check(BOOL__G__LogUpdatePending,
                  "نسخهٔ کهنهٔ روی فلش، بروزرسانی فرم‌ور را اعلام می‌کند");
    func__T_Check(func__SimpleLog_FwStampRead() == (uint32_t)SIMPLE_ESP_FW_VERSION,
                  "مهر نسخه با نسخهٔ در حال اجرا تازه می‌شود");

    /* تایمرهای لاگ را دستی کثیف می‌کنیم: ردیف بروزرسانی باید بی‌اعتنا بنویسد */
    UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    BOOL__G__LogLastRowValid = true;
    UINT32_T__G__LogLastStepMs = (uint32_t)millis();
    BOOL__G__LogLastStepValid = true;
    BOOL__G__LogLinkDown = true;
    BOOL__G__TlmSeen = false;
    BOOL__G__LogBootRowPending = true;
    BOOL__G__LogHasPrev = false;
    func__SimpleLog_Tick();
    func__T_Check(BOOL__G__LogUpdatePending == false,
                  "بروزرسانی فقط یک بار اعلام می‌شود، نه در هر تیک");
    func__T_Check(BOOL__A__LogForceRow == false,
                  "اجبارِ نوشتن یک‌بارمصرف است و بعد خاموش می‌شود");
    func__T_Check(BOOL__G__LogLastStepValid == false,
                  "تایمر گام ولتاژ در همین ردیف صفر شد");

    /* ردیف نوشته‌شده: ستون سوم باید هر دو بیت «روشن شد» و «بروزرسانی» را داشته باشد */
    {
        char char__path[SIMPLE_LOG_PATH_MAX];
        std::string string__text;

        func__SimpleLog_PathOf(char__path, func__SimpleLog_TodayCode());
        {
            File file__day = LittleFS.open(char__path, "r");

            if (file__day) {
                while (file__day.available() > 0) {
                    string__text += file__day.readStringUntil('\n').c_str();
                    string__text += "\n";
                }
                file__day.close();
            }
        }
        bool bool__found = false;

        {
            size_t size_t__start = 0u;

            while (size_t__start < string__text.size()) {
                size_t size_t__end = string__text.find('\n', size_t__start);

                if (size_t__end == std::string::npos) {
                    break;
                }
                {
                    std::string string__row = string__text.substr(size_t__start,
                                                                  size_t__end - size_t__start);
                    size_t size_t__first = string__row.find(',');

                    if ((string__row.empty() == false) && (string__row[0] != '#')) {
                        /* ستون سوم ردیف است: زمان، آپتایم، ماسک تغییر */
                        size_t size_t__second = (size_t__first == std::string::npos)
                                                ? std::string::npos
                                                : string__row.find(',', size_t__first + 1u);
                        size_t size_t__third = (size_t__second == std::string::npos)
                                               ? std::string::npos
                                               : string__row.find(',', size_t__second + 1u);
                        if ((size_t__second != std::string::npos) &&
                            (size_t__third != std::string::npos)) {
                            std::string string__mask = string__row.substr(size_t__second + 1u,
                                                                          size_t__third - size_t__second - 1u);
                            if ((string__mask == "4097") || (string__mask == "4097\r")) {
                                bool__found = true;
                            }
                        }
                    }
                }
                size_t__start = size_t__end + 1u;
            }
        }
        func__T_Check(bool__found,
                      "ردیف نوشته‌شده هم «پنل روشن شد» و هم «بروزرسانی» را با هم دارد");
    }

    /* ۳) بوت بعدی با همان نسخه: دیگر ردیف بروزرسانی نمی‌سازد */
    (void)func__SimpleLog_Clear();
    BOOL__G__LogUpdatePending = false;
    func__SimpleLog_Begin();
    func__T_Check(BOOL__G__LogUpdatePending == false,
                  "بوت بعدی با همان نسخه، بروزرسانی حساب نمی‌شود");

    /* ۴) فلشِ تازه (بی‌مهر) هم بروزرسانی حساب نمی‌شود */
    (void)func__SimpleLog_Clear();
    (void)LittleFS.remove(SIMPLE_TIME_FILE);
    BOOL__G__LogUpdatePending = false;
    func__SimpleLog_Begin();
    func__T_Check(BOOL__G__LogUpdatePending == false,
                  "پنلِ تازه‌فلش‌شده بی‌مهر قبلی، بروزرسانی حساب نمی‌شود");

    /* ۵) زدن دکمهٔ «به‌روزرسانی» (POST /evt/snap): بدون هیچ شرطی یک رکورد ثبت
          می‌کند (حتی در پنجرهٔ ۲ ثانیه و حتی در سقف پر بودن روز) و فقط تایمرهای
          مربوط به لاگ را صفر می‌کند. */
    (void)func__SimpleLog_Clear();
    func__Simple_TimeSet(1792756800u, false);
    UINT16_T__G__LogRowsToday = SIMPLE_LOG_MAX_ROWS_PER_DAY;
    BOOL__G__LogDayFull = true;
    UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    BOOL__G__LogLastRowValid = true;
    UINT32_T__G__LogLastStepMs = (uint32_t)millis();
    BOOL__G__LogLastStepValid = true;
    UINT32_T__G__LogMainsQueuedMs = (uint32_t)millis();
    BOOL__G__LogMainsQueuedValid = true;
    UINT32_T__G__LastDiagReqMs = 5151u;
    UINT32_T__G__WifiBatteryIdleStartMs = 8282u;

    SIMPLE_WEB_SERVER_T__G__Server.simClear();
    func__Simple_HttpEventSnap();
    func__T_Check(BOOL__G__LogLastStepValid == false && UINT32_T__G__LogLastStepMs == 0u &&
                  BOOL__G__LogMainsQueuedValid == false && UINT32_T__G__LogMainsQueuedMs == 0u,
                  "زدن به‌روزرسانی تایمرهای مربوط به لاگ را صفر می‌کند");
    func__T_Check(UINT32_T__G__LastDiagReqMs == 5151u &&
                  UINT32_T__G__WifiBatteryIdleStartMs == 8282u,
                  "زدن به‌روزرسانی به تایمرهای غیرِ لاگ دست نمی‌زند");

    /* بار دوم بلافاصله (زیر ۲ ثانیه) هم باید رکورد دوم را ثبت کند */
    SIMPLE_WEB_SERVER_T__G__Server.simClear();
    func__Simple_HttpEventSnap();
    {
        char char__tail[1024];
        size_t size_t__hits = 0u;
        size_t size_t__pos = 0u;

        (void)func__SimpleLog_Tail(char__tail, sizeof char__tail, SIMPLE_LOG_DAY_AUTO, 30u);
        std::string string__tail(char__tail);
        while ((size_t__pos = string__tail.find(",0,6,", size_t__pos)) != std::string::npos)
        {
            size_t__hits++;
            size_t__pos += 5u;
        }
        func__T_Check(size_t__hits == 2u,
                      "دو بار زدن پیاپی دکمهٔ به‌روزرسانی دو رکورد دستی با note=6 ثبت می‌کند");
    }
    (void)func__SimpleLog_Clear();

    /* بررسی ضربان ۱ ساعته، نام بدون اسلش ESP8266، پاک‌سازی فایل‌های جامانده و فرمت امن */
    {
        func__T_Check(SIMPLE_LOG_HEARTBEAT_MS == 3600000u,
                      "ضربان دوره‌ای بدون تغییر دقیقاً ۱ ساعت (۳٬۶۰۰٬۰۰۰ میلی‌ثانیه) است");

        LittleFS.slashlessDirNames = true;
        BOOL__G__FsOk = true;
        BOOL__G__LogScanValid = false;
        {
            File file__day = LittleFS.open("/evt-20261028.csv", "w");
            if (file__day) { file__day.print("1792700000,1,1,0\n"); file__day.close(); }
            File file__old = LittleFS.open("/leftover_old_fw.dat", "w");
            if (file__old) { file__old.print("OLD_DATA_123456"); file__old.close(); }
        }
        func__T_Check(func__SimpleLog_ScanDays(true) >= 1u,
                      "اسکن روزها نام بدون خط مورب ابتدایی در ESP8266 واقعی را می‌شناسد");

        UINT8_T__G__CurrentRole = 1u; /* admin */
        (void)snprintf(CHAR__G__CurrentUser, sizeof(CHAR__G__CurrentUser), "%s", "admin");
        SIMPLE_WEB_SERVER_T__G__Server.simClear();
        func__Simple_HttpEventFsStatus();
        func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simBody().find("leftover_old_fw.dat") != std::string::npos &&
                      SIMPLE_WEB_SERVER_T__G__Server.simBody().find("\"own\":0") != std::string::npos,
                      "مسیر /evt/fs فایل جامانده از برنامه‌های قبلی را با own=0 نشان می‌دهد");

        SIMPLE_WEB_SERVER_T__G__Server.simClear();
        func__Simple_HttpEventClean();
        func__T_Check(!LittleFS.exists("/leftover_old_fw.dat") && LittleFS.exists("/evt-20261028.csv"),
                      "مسیر /evt/clean فقط فایل جاماندهٔ قدیمی را حذف می‌کند و لاگ برنامهٔ فعلی را نگه می‌دارد");

        SIMPLE_WEB_SERVER_T__G__Server.simClear();
        func__Simple_HttpEventFormat();
        func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simBody() == "ok" &&
                      !LittleFS.exists("/evt-20261028.csv") &&
                      LittleFS.exists(SIMPLE_USERS_FILE) &&
                      LittleFS.exists(SIMPLE_WIFI_FILE),
                      "فرمت کامل حافظهٔ ESP همهٔ لاگ‌ها و فایل‌های قدیمی را پاک و کاربران و وای‌فای را بازنویسی می‌کند");

        LittleFS.slashlessDirNames = false;
        (void)func__SimpleLog_Clear();
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
    func__T_TestWifiViewerPower();
    func__T_TestStm32TimeAndChartScheduling();
    func__T_TestDiagStatePersistenceAndClear();
    func__T_TestAuthAdminProtection();
    func__T_TestStateHistoryLog();
    func__T_TestHandshake();
    func__T_TestClearOrphansAndUpdateRow();

    (void)printf("\n== بررسی‌ها: %u، شکست‌ها: %u ==\n", (unsigned)UINT32_T__G__Checks, (unsigned)UINT32_T__G__Failures);
    if (UINT32_T__G__Failures != 0u)
    {
        (void)printf("تست هاست صف ریست: رد\n");
        return 1;
    }
    (void)printf("تست هاست صف ریست: قبول\n");
    return 0;
}
