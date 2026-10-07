/* ==========================================================================
   sim_main.cpp - run the REAL user panel on a laptop, with a fake machine
   ==========================================================================

   [EN] WHY THIS EXISTS
        Flashing an ESP8266 to find out whether a screen shows the right number
        is a slow way to debug. This program compiles the panel's OWN code - the
        same user_panel.ino, the same up_*.h, the same single translation unit
        the board uses - against a fake ESP (the stubs in ../tools) and a fake
        machine (sim_machine.h), and then serves the panel on
        http://localhost:8090 exactly as the board would, chunked Excel report
        and all.

        What it catches before a board is ever plugged in:
          - a screen that reads the wrong telemetry word (the numbers arrive
            through the real parser, from a model you control),
          - the role gates (log in as admin, operator, viewer),
          - a charger cut that never reaches /s, or reaches it with the wrong id,
          - the report's range selector, sheet by sheet,
          - the imbalance latch and FINAL_FAULT conversations, in seconds
            instead of days.

        What it CANNOT catch, and says so on the control page:
          - a wrong pin, a wrong divider, a weak antenna, a slow flash chip,
          - anything about timing that only exists on real silicon.

        Control page: http://localhost:8090/sim     Panel: http://localhost:8090/

   [FA] چرا این وجود دارد
        فلش‌کردن ESP8266 برای فهمیدن اینکه صفحهٔ درستی عدد را نشان می‌دهد یا نه،
        راه کندی برای اشکال‌زدایی است. این برنامه کدِ «خودِ» پنل را کامپایل
        می‌کند - همان user_panel.ino، همان up_*.h، همان یک واحد ترجمه‌ای که برد
        استفاده می‌کند - در کنار یک ESP جعلی (استاب‌های tools) و یک ماشین جعلی
        (sim_machine.h)، و پنل را روی http://localhost:8090 دقیقاً مثل برد سرو
        می‌کند، با گزارش اکسل جریانی و همه‌چیز.

        چه چیزهایی را پیش از وصل‌کردن برد می‌گیرد: صفحه‌ای که کلمهٔ تلمتری غلط
        را می‌خواند (اعداد از دل پارسر واقعی و از مدلی که خودتان کنترل می‌کنید
        می‌آیند)، دروازه‌های نقش، فرمانی که به /s نمی‌رسد یا با شناسهٔ غلط
        می‌رسد، انتخاب بازهٔ گزارش برگه‌به‌برگه، و گفت‌وگوی قفل عدم‌توازن و
        FINAL_FAULT در چند ثانیه به‌جای چند روز.

        چه چیزهایی را نمی‌گیرد و روی صفحهٔ کنترل هم همین را می‌گوید: پایهٔ غلط،
        تقسیم‌کنندهٔ غلط، آنتن ضعیف، فلش کند، و هر زمان‌بندی‌ای که فقط روی
        سیلیکون واقعی وجود دارد.

   Build / ساخت:   bash user_panel/simulator/run_sim.sh
   Selftest / خودآزمایی (no sockets, used by the gate):
                   bash user_panel/simulator/run_sim.sh --selftest
*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "stub_esp.h"

/* the globals the stubs declare extern */
std::map<uint8_t, int> G_StubGpio;
StubSerial Serial;
StubWiFi WiFi;
StubFS LittleFS;
std::string WiFiClient::G_NextResponse;
std::string WiFiClient::G_LastRequest;
int WiFiClient::G_ConnectFails = 0;

/* [EN] The REAL panel, unmodified: one translation unit, ESP8266 branch. */
#include "../user_panel.ino"
#include "sim_machine.h"

#define SIM_DEFAULT_PORT 8090
#define SIM_REQUEST_MAX  16384u

/* ==================== Result counters / شمارنده‌های نتیجه ==================== */
static unsigned SIM_UINT__G__Passed = 0u;
static unsigned SIM_UINT__G__Failed = 0u;

/* ==================== Check / بررسی ==================== */
/**
 * @brief  [EN] Print one self-test result and count it. The wording is the same
 *              as the host tests on purpose: two tools that say "ok" and "FAIL"
 *              the same way are easier to trust than two that do not.
 *         [FA] چاپ یک نتیجهٔ خودآزمایی و شمردنش. عمداً همان واژه‌های تست‌های
 *              میزبان: دو ابزاری که یک‌جور «ok» و «FAIL» می‌گویند از دو ابزاری
 *              که یک‌جور نمی‌گویند قابل‌اعتمادترند.
 * @param  bool__ok [EN] result / [FA] نتیجه
 * @param  char__what [EN] what was checked / [FA] آنچه بررسی شد
 * @param  char__detail [EN] extra text or NULL / [FA] متن اضافه یا NULL
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimCheck(bool bool__ok, const char *char__what, const char *char__detail = NULL)
{
    if (bool__ok) { SIM_UINT__G__Passed++; }
    else          { SIM_UINT__G__Failed++; }
    printf("  %s %s%s%s\n", bool__ok ? "ok  " : "FAIL", char__what,
           (char__detail != NULL) ? " - " : "", (char__detail != NULL) ? char__detail : "");
}

/* ==================== URL decoding / بازکردن نشانی ==================== */
/**
 * @brief  [EN] Percent-decode one query value in place-safe fashion: '+' is a
 *              space, %XX is a byte. The panel's page posts plain ASCII
 *              usernames and digits, but a password may hold anything.
 *         [FA] بازکردن یک مقدار نشانی: '+' فاصله است و %XX یک بایت. صفحهٔ پنل
 *              نام‌کاربری و ارقام ساده می‌فرستد، ولی گذرواژه می‌تواند هر چیزی
 *              داشته باشد.
 */
static std::string func__UpSimUrlDecode(const std::string &std_string__in)
{
    std::string std_string__out;
    for (size_t i = 0u; i < std_string__in.size(); i++)
    {
        char char__c = std_string__in[i];
        if ((char__c == '%') && ((i + 2u) < std_string__in.size()))
        {
            char char__hex[3] = { std_string__in[i + 1u], std_string__in[i + 2u], '\0' };
            std_string__out += (char)strtol(char__hex, NULL, 16);
            i += 2u;
        }
        else if (char__c == '+')
        {
            std_string__out += ' ';
        }
        else
        {
            std_string__out += char__c;
        }
    }
    return std_string__out;
}

/**
 * @brief  [EN] Split "a=1&b=2" into a map. Used for the query string and for a
 *              form body, because the panel reads both through the same
 *              has-arg/arg pair.
 *         [FA] شکستن «a=1&b=2» به یک نگاشت. برای رشتهٔ پرس‌وجو و بدنهٔ فرم، چون
 *              پنل هر دو را از همان یک جفت hasArg/arg می‌خواند.
 */
static void func__UpSimParsePairs(const std::string &std_string__text, std::map<std::string, std::string> &map_out)
{
    size_t size_t__at = 0u;

    while (size_t__at < std_string__text.size())
    {
        size_t size_t__end = std_string__text.find('&', size_t__at);
        if (size_t__end == std::string::npos) { size_t__end = std_string__text.size(); }

        std::string std_string__pair = std_string__text.substr(size_t__at, size_t__end - size_t__at);
        size_t size_t__equals = std_string__pair.find('=');
        if (size_t__equals != std::string::npos)
        {
            map_out[func__UpSimUrlDecode(std_string__pair.substr(0u, size_t__equals))] =
                func__UpSimUrlDecode(std_string__pair.substr(size_t__equals + 1u));
        }
        else if (!std_string__pair.empty())
        {
            map_out[func__UpSimUrlDecode(std_string__pair)] = "";
        }

        size_t__at = size_t__end + 1u;
    }
}

/* ==================== HTTP request / درخواست ==================== */
typedef struct
{
    std::string std_string__method;
    std::string std_string__path;
    std::string std_string__query;
    std::map<std::string, std::string> map__args;
    std::map<std::string, std::string> map__headers;
    std::string std_string__body;
} sim_request_t;

typedef struct
{
    int int__code;
    std::string std_string__type;
    std::string std_string__body;
    std::map<std::string, std::string> map__headers;
} sim_response_t;

/**
 * @brief  [EN] Read one request from a socket: the request line, the headers,
 *              and the body when the headers promise one. A browser request is
 *              a few hundred bytes; the cap is there so a wrong port or a
 *              probe cannot make the simulator allocate without end.
 *         [FA] خواندن یک درخواست از سوکت: خط درخواست، سرصفحه‌ها و بدنه، اگر
 *              سرصفحه‌ها وعده‌اش را داده باشند. درخواست مرورگر چند صد بایت است؛
 *              سقف گذاشته شده تا پورت اشتباه یا یک کاوشگر نتواند شبیه‌ساز را
 *              بی‌نهایت حافظه‌خوار کند.
 * @return [EN] true when a request was read / [FA] در صورت خواندن true
 */
static bool func__UpSimReadRequest(int int__fd, sim_request_t *sim_request_t__request)
{
    std::string std_string__raw;
    char char__buffer[2048];
    size_t size_t__headerEnd = std::string::npos;

    while (std_string__raw.size() < SIM_REQUEST_MAX)
    {
        ssize_t ssize_t__got = recv(int__fd, char__buffer, sizeof(char__buffer), 0);
        if (ssize_t__got <= 0) { return false; }
        std_string__raw.append(char__buffer, (size_t)ssize_t__got);
        size_t__headerEnd = std_string__raw.find("\r\n\r\n");
        if (size_t__headerEnd != std::string::npos)
        {
            size_t size_t__head = size_t__headerEnd + 4u;
            size_t size_t__want = 0u;
            size_t size_t__at = std_string__raw.find("\r\nContent-Length:");
            if (size_t__at != std::string::npos)
            {
                size_t__want = (size_t)strtoul(std_string__raw.c_str() + size_t__at + 17u, NULL, 10);
            }
            if (std_string__raw.size() >= (size_t__head + size_t__want)) { break; }
        }
    }
    if (size_t__headerEnd == std::string::npos) { return false; }

    size_t size_t__lineEnd = std_string__raw.find("\r\n");
    std::string std_string__line = std_string__raw.substr(0u, size_t__lineEnd);
    size_t size_t__firstSpace = std_string__line.find(' ');
    size_t size_t__secondSpace = std_string__line.find(' ', size_t__firstSpace + 1u);
    if ((size_t__firstSpace == std::string::npos) || (size_t__secondSpace == std::string::npos)) { return false; }

    sim_request_t__request->std_string__method = std_string__line.substr(0u, size_t__firstSpace);
    std::string std_string__target = std_string__line.substr(size_t__firstSpace + 1u, size_t__secondSpace - size_t__firstSpace - 1u);

    size_t size_t__question = std_string__target.find('?');
    if (size_t__question == std::string::npos)
    {
        sim_request_t__request->std_string__path = std_string__target;
    }
    else
    {
        sim_request_t__request->std_string__path = std_string__target.substr(0u, size_t__question);
        sim_request_t__request->std_string__query = std_string__target.substr(size_t__question + 1u);
    }

    size_t size_t__at = size_t__lineEnd + 2u;
    while (size_t__at < size_t__headerEnd)
    {
        size_t size_t__end = std_string__raw.find("\r\n", size_t__at);
        if (size_t__end == std::string::npos) { break; }
        std::string std_string__header = std_string__raw.substr(size_t__at, size_t__end - size_t__at);
        size_t size_t__colon = std_string__header.find(':');
        if (size_t__colon != std::string::npos)
        {
            std::string std_string__key = std_string__header.substr(0u, size_t__colon);
            std::string std_string__value = std_string__header.substr(size_t__colon + 1u);
            while (!std_string__value.empty() && (std_string__value[0] == ' ')) { std_string__value.erase(0u, 1u); }
            sim_request_t__request->map__headers[std_string__key] = std_string__value;
        }
        size_t__at = size_t__end + 2u;
    }

    sim_request_t__request->std_string__body = std_string__raw.substr(size_t__headerEnd + 4u);
    func__UpSimParsePairs(sim_request_t__request->std_string__query, sim_request_t__request->map__args);
    func__UpSimParsePairs(sim_request_t__request->std_string__body, sim_request_t__request->map__args);

    return true;
}

/**
 * @brief  [EN] Write one response. Content-Length is always declared, even for
 *              the streamed report: this HTTP server already holds the whole
 *              body in memory (the fake ESP's socket is a std::string), so a
 *              chunked encoding here would model nothing and confuse a read of
 *              the logs.
 *         [FA] نوشتن یک پاسخ. طول محتوا همیشه اعلام می‌شود، حتی برای گزارش
 *              جریانی: این سرور HTTP کل بدنه را در حافظه دارد (سوکت ESP جعلی
 *              یک std::string است)، پس کدگذاری تکه‌ای اینجا چیزی را مدل نمی‌کند
 *              و فقط خواندن گزارش‌ها را گیج می‌کند.
 */
static void func__UpSimWriteResponse(int int__fd, const sim_response_t &sim_response_t__response)
{
    std::string std_string__head = "HTTP/1.1 " + std::to_string(sim_response_t__response.int__code) + " " +
        ((sim_response_t__response.int__code == 200) ? "OK" : "Error") + "\r\n";
    std_string__head += "Content-Type: " + sim_response_t__response.std_string__type + "\r\n";
    std_string__head += "Content-Length: " + std::to_string(sim_response_t__response.std_string__body.size()) + "\r\n";
    std_string__head += "Connection: close\r\n";
    std_string__head += "Cache-Control: no-store\r\n";

    for (std::map<std::string, std::string>::const_iterator it = sim_response_t__response.map__headers.begin();
         it != sim_response_t__response.map__headers.end(); ++it)
    {
        std_string__head += it->first + ": " + it->second + "\r\n";
    }
    std_string__head += "\r\n";

    std::string std_string__all = std_string__head + sim_response_t__response.std_string__body;
    size_t size_t__sent = 0u;
    while (size_t__sent < std_string__all.size())
    {
        ssize_t ssize_t__wrote = send(int__fd, std_string__all.data() + size_t__sent, std_string__all.size() - size_t__sent, 0);
        if (ssize_t__wrote <= 0) { break; }
        size_t__sent += (size_t)ssize_t__wrote;
    }
}

/* ==================== Reading a file / خواندن فایل ==================== */
/**
 * @brief  [EN] Read a whole file into a string; returns false when it is not
 *              there. Used for the control page, which is a real file next to
 *              this program - editing it does not require a rebuild, which is
 *              the point.
 *         [FA] خواندن کل یک فایل به رشته؛ اگر نباشد false. برای صفحهٔ کنترل
 *              استفاده می‌شود که فایلی واقعی کنار همین برنامه است - ویرایشش
 *              کامپایل دوباره نمی‌خواهد و همین هدف است.
 */
static bool func__UpSimReadFile(const std::string &std_string__path, std::string &std_string__out)
{
    FILE *file = fopen(std_string__path.c_str(), "rb");
    if (file == NULL) { return false; }
    char char__buffer[4096];
    size_t size_t__got;
    std_string__out.clear();
    while ((size_t__got = fread(char__buffer, 1u, sizeof(char__buffer), file)) > 0u)
    {
        std_string__out.append(char__buffer, size_t__got);
    }
    fclose(file);
    return true;
}

/* ==================== Commands / فرمان‌ها ==================== */
/**
 * @brief  [EN] The control page's commands, in one place. Every one of them
 *              changes the MACHINE, never the panel: the panel is the thing
 *              under test and must react on its own.
 *         [FA] فرمان‌های صفحهٔ کنترل، همه در یک جا. هیچ‌کدام پنل را تغییر
 *              نمی‌دهد، همه ماشین را: پنل چیزی است که آزمایش می‌شود و باید
 *              خودش واکنش بدهد.
 * @param  sim_request_t__request [EN] the request / [FA] درخواست
 * @return [EN] a short status string / [FA] یک متن وضعیت کوتاه
 */
static std::string func__UpSimCommand(const sim_request_t &sim_request_t__request)
{
    const std::map<std::string, std::string> &map__args = sim_request_t__request.map__args;
    std::string std_string__command = map__args.count("c") ? map__args.at("c") : "";
    long long__value = map__args.count("v") ? strtol(map__args.at("v").c_str(), NULL, 10) : 0L;
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;

    if (std_string__command == "reset")
    {
        func__UpSim_Reset();
        WiFi.currentStatus = WL_CONNECTED;
        return "machine reset";
    }
    if (std_string__command == "input")
    {
        up_sim_t__sim->uint8_t__inputPresent = (long__value != 0L) ? 1u : 0u;
        return "input set";
    }
    if (std_string__command == "ch")
    {
        long long__channel = map__args.count("n") ? strtol(map__args.at("n").c_str(), NULL, 10) : 1L;
        if (long__channel == 1L) { up_sim_t__sim->uint8_t__ch1Enabled = (long__value != 0L) ? 1u : 0u; }
        else                     { up_sim_t__sim->uint8_t__ch2Enabled = (long__value != 0L) ? 1u : 0u; }
        return "charger set";
    }
    if (std_string__command == "fault")
    {
        long long__bit = map__args.count("bit") ? strtol(map__args.at("bit").c_str(), NULL, 10) : 0L;
        func__UpSim_FaultSet((uint8_t)long__bit, (long__value != 0L) ? 1u : 0u);
        return "fault set";
    }
    if (std_string__command == "jitter")
    {
        func__UpSim_JitterTrip();
        return "jitter trip";
    }
    if (std_string__command == "board")
    {
        func__UpSim_BoardReset();
        return "board reset";
    }
    if (std_string__command == "swap")
    {
        func__UpSim_BatterySwap();
        return "battery swapped";
    }
    if (std_string__command == "imb")
    {
        func__UpSim_ImbalanceEpisode();
        return "imbalance started";
    }
    if (std_string__command == "link")
    {
        up_sim_t__sim->uint8_t__linkUp = (long__value != 0L) ? 1u : 0u;
        WiFi.currentStatus = (up_sim_t__sim->uint8_t__linkUp != 0u) ? WL_CONNECTED : WL_DISCONNECTED;
        return "link set";
    }
    if (std_string__command == "absent")
    {
        up_sim_t__sim->uint8_t__batteryAbsent = (long__value != 0L) ? 1u : 0u;
        return "battery absence set";
    }
    if (std_string__command == "manual")
    {
        up_sim_t__sim->uint8_t__manualMode = (long__value != 0L) ? 1u : 0u;
        return "manual mode set";
    }
    return "unknown command";
}

/* ==================== Panel dispatch / رساندن به پنل ==================== */
/**
 * @brief  [EN] Hand a request to the REAL panel: its own route table decides
 *              what happens, its own session check decides who may do it. The
 *              simulator only carries the bytes - which is exactly what makes
 *              the result worth reading.
 *         [FA] رساندن درخواست به «خودِ» پنل: جدول مسیرهای خودش تصمیم می‌گیرد و
 *              بررسی نشست خودش تصمیم می‌گیرد چه کسی مجاز است. شبیه‌ساز فقط
 *              بایت‌ها را می‌برد - و همین است که نتیجه را خواندنی می‌کند.
 * @return [EN] true when a route answered / [FA] در صورت پاسخ یک مسیر true
 */
static bool func__UpSimDispatchPanel(const sim_request_t &sim_request_t__request, sim_response_t *sim_response_t__response)
{
    int int__method = HTTP_GET;

    if (sim_request_t__request.std_string__method == "POST") { int__method = HTTP_POST; }

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.clearResponse();

    for (std::map<std::string, std::string>::const_iterator it = sim_request_t__request.map__args.begin();
         it != sim_request_t__request.map__args.end(); ++it)
    {
        UP_WEBSERVER_T__G__Server.setArg(it->first.c_str(), it->second.c_str());
    }
    for (std::map<std::string, std::string>::const_iterator it = sim_request_t__request.map__headers.begin();
         it != sim_request_t__request.map__headers.end(); ++it)
    {
        UP_WEBSERVER_T__G__Server.setHeader(it->first.c_str(), it->second.c_str());
    }

    if (!UP_WEBSERVER_T__G__Server.call(sim_request_t__request.std_string__path.c_str(), int__method))
    {
        return false;
    }

    sim_response_t__response->int__code = UP_WEBSERVER_T__G__Server.lastCode;
    sim_response_t__response->std_string__type = UP_WEBSERVER_T__G__Server.lastType;
    sim_response_t__response->std_string__body = UP_WEBSERVER_T__G__Server.lastBody;

    /* [EN] Only the headers the panel actually set travel on: an empty map
            keeps a "Cache-Control: " line out of the answer. */
    for (std::map<std::string, std::string>::const_iterator it = UP_WEBSERVER_T__G__Server.sentHeaders.begin();
         it != UP_WEBSERVER_T__G__Server.sentHeaders.end(); ++it)
    {
        if (!it->second.empty()) { sim_response_t__response->map__headers[it->first] = it->second; }
    }
    return true;
}

/* ==================== Time / زمان ==================== */
/**
 * @brief  [EN] The virtual clock, in milliseconds, shared by the fake firmware
 *              (millis()) and the model. Advancing them together is the whole
 *              trick: the panel's own timers, its 1 s sampling and the machine's
 *              physics then all agree on what "now" is.
 *         [FA] ساعت مجازی، به میلی‌ثانیه، مشترک بین فرم‌ور جعلی (millis()) و
 *              مدل. جلو بردن هم‌زمانشان تمام ترفند است: تایمرهای خود پنل،
 *              نمونه‌برداری یک‌ثانیه‌ای و فیزیک ماشین همه روی «حالا» توافق
 *              می‌کنند.
 */
static uint32_t SIM_UINT32_T__G__VirtualMs = 100000u;   /* [EN] start at t=100 s / [FA] شروع از ۱۰۰ ثانیه */
static uint32_t SIM_UINT32_T__G__Speed = 1u;
static std::string SIM_STRING__G__Body;   /* [EN] current telemetry body / [FA] بدنهٔ تلمتری جاری */

/**
 * @brief  [EN] Rebuild the body from the model. Called once per logical second
 *              (and after every change to the model), because building it walks
 *              128 parameters and no screen needs that sixty times a second.
 *         [FA] ساخت دوبارهٔ بدنه از مدل. هر ثانیهٔ منطقی (و بعد از هر تغییر
 *              مدل) صدا زده می‌شود، چون ساختنش یعنی گذر از ۱۲۸ پارامتر و هیچ
 *              صفحه‌ای این را شصت بار در ثانیه لازم ندارد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimRefreshBody(void)
{
    func__UpSim_Body(SIM_STRING__G__Body);
}

/**
 * @brief  [EN] Feed the link its next answer. The panel opens a connection, the
 *              fake socket hands it this body, and the panel's own parser turns
 *              it into the numbers on the screen.
 *         [FA] دادن پاسخ بعدی به لینک. پنل اتصال را باز می‌کند، سوکت جعلی این
 *              بدنه را می‌دهد و پارسر خود پنل آن را به اعداد روی صفحه تبدیل
 *              می‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimFeedLink(void)
{
    WiFiClient::G_NextResponse = std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") +
                                 SIM_STRING__G__Body;
}

/**
 * @brief  [EN] Advance the shared clock and the model together.
 *         [FA] جلو بردن هم‌زمان ساعت مشترک و مدل.
 * @param  uint32_t__deltaMs [EN] milliseconds to add / [FA] میلی‌ثانیهٔ افزودنی
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimAdvanceMs(uint32_t uint32_t__deltaMs)
{
    SIM_UINT32_T__G__VirtualMs += uint32_t__deltaMs;
    stub_millis_ref() = SIM_UINT32_T__G__VirtualMs;
    stub_micros_ref() = SIM_UINT32_T__G__VirtualMs * 1000u;
    func__UpSim_Tick(SIM_UINT32_T__G__VirtualMs);
    func__UpSimRefreshBody();
}



static std::string SIM_STRING__G__LastSetBody;   /* [EN] last write the panel sent / [FA] آخرین نوشتن پنل */

/**
 * @brief  [EN] Take whatever the panel wrote to the fake ESP and give it to the
 *              machine. Without this step the simulator would be a machine that
 *              ignores every command - and the panel's charger-cut path would
 *              look like it worked while nothing happened.
 *         [FA] برداشتن آنچه پنل به ESP جعلی نوشته و دادنش به ماشین. بدون این
 *              گام، شبیه‌ساز ماشینی است که هر فرمانی را نادیده می‌گیرد - و مسیر
 *              قطع شارژر پنل سالم به نظر می‌رسد در حالی که هیچ اتفاقی نمی‌افتد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimDrainRequests(void)
{
    std::string &std_string__request = WiFiClient::G_LastRequest;

    if (std_string__request.empty()) { return; }

    size_t size_t__at = std_string__request.find("POST /s");
    if (size_t__at != std::string::npos)
    {
        size_t size_t__body = std_string__request.find("\r\n\r\n", size_t__at);
        size_t size_t__length = 0u;

        /* [EN] The captured socket text is a transcript of EVERYTHING the panel
           wrote since the last drain - the header of this write and the next
           request behind it. The body is therefore cut to the length the header
           promised, exactly as a real server cuts it.
           [FA] متن سوکت، رونوشت «همهٔ» چیزی است که پنل از آخرین تخلیه نوشته -
           سرصفحهٔ این نوشتن و درخواست بعدی پشتش. پس بدنه به همان طولی بریده
           می‌شود که سرصفحه وعده داده، دقیقاً مثل یک سرور واقعی. */
        size_t size_t__header = std_string__request.find("Content-Length:", size_t__at);
        if ((size_t__header != std::string::npos) && (size_t__header < size_t__body))
        {
            size_t__length = (size_t)strtoul(std_string__request.c_str() + size_t__header + 15u, NULL, 10);
        }
        if (size_t__body != std::string::npos)
        {
            std::map<std::string, std::string> map__args;
            func__UpSimParsePairs(std_string__request.substr(size_t__body + 4u, size_t__length), map__args);
            if ((map__args.count("id") != 0u) && (map__args.count("v") != 0u))
            {
                int int__code = 200;
                std::string std_string__answer;
                (void)func__UpSim_SetParam(map__args["id"].c_str(), map__args["v"].c_str(), &int__code, std_string__answer);
                SIM_STRING__G__LastSetBody = "id=" + map__args["id"] + "&v=" + map__args["v"];
            }
        }
    }
    std_string__request.clear();
}

/**
 * @brief  [EN] Run the panel's loop the way the board does: a handful of
 *              iterations per millisecond of virtual time, reading and writing
 *              through the fake sockets, and hand the board whatever the panel
 *              wrote.
 *         [FA] اجرای حلقهٔ پنل همان‌طور که برد اجرا می‌کند: چند دور در هر
 *              میلی‌ثانیهٔ مجازی، با خواندن و نوشتن از سوکت‌های جعلی، و دادن
 *              آنچه پنل نوشته به برد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimLoopTimes(uint32_t uint32_t__times)
{
    for (uint32_t i = 0u; i < uint32_t__times; i++)
    {
        loop();
    }
    func__UpSimDrainRequests();
}

/* ==================== Selftest / خودآزمایی ==================== */
static bool func__UpSimRoute(const char *char__path, int int__method, const char *char__cookie)
{
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.clearResponse();
    if (char__cookie != NULL)
    {
        UP_WEBSERVER_T__G__Server.setHeader("Cookie", char__cookie);
    }
    else
    {
        UP_WEBSERVER_T__G__Server.setHeader("Cookie", "");
    }
    return UP_WEBSERVER_T__G__Server.call(char__path, int__method);
}

static bool func__UpSimLogin(const char *char__user, const char *char__password, std::string &std_string__cookie)
{
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.clearResponse();
    UP_WEBSERVER_T__G__Server.setArg("u", char__user);
    UP_WEBSERVER_T__G__Server.setArg("p", char__password);
    (void)UP_WEBSERVER_T__G__Server.call("/api/login", HTTP_POST);

    std::string std_string__header = UP_WEBSERVER_T__G__Server.sentHeaders["Set-Cookie"];
    size_t size_t__end = std_string__header.find(';');
    std_string__cookie = std_string__header.substr(0u, size_t__end);
    return (UP_WEBSERVER_T__G__Server.lastBody.find("\"ok\":1") != std::string::npos) && (std_string__cookie.size() > 8u);
}

/**
 * @brief  [EN] The no-sockets check the gate runs: boot the panel against the
 *              model, feed it a few frames, and make it answer the questions a
 *              bench would ask - as each role. It is deliberately short; the
 *              deep checks live in the host tests, and duplicating them here
 *              would only create a second place to update.
 *         [FA] همان بررسی بی‌سوکتی که گیت اجرا می‌کند: بالا آوردن پنل در برابر
 *              مدل، خوراندن چند فریم و وادارکردنش به پاسخ‌دادن به پرسش‌های میز
 *              آزمایش - با هر نقش. عمداً کوتاه است؛ بررسی‌های عمیق در تست‌های
 *              میزبان‌اند و تکرارشان اینجا فقط یک جای دوم برای به‌روزرسانی
 *              می‌سازد.
 * @return [EN] process exit code / [FA] کد خروج فرآیند
 */
/* ==================== The third trip, named / سقوط سوم، نام‌دار ==================== */
/**
 * @brief  [EN] The third trip on its own, so the selftest reads as the rule it
 *              is checking: two trips warn, the third latches the channel.
 *         [FA] سقوط سوم جدا، تا خودآزمایی همان قاعده‌ای را بخواند که بررسی
 *              می‌کند: دو سقوط هشدار، سومی قفل کانال.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimJitterCheck(void)
{
    func__UpSim_JitterTrip();
    func__UpSimRefreshBody();
    func__UpSimFeedLink();
    func__UpSimLoopTimes(40u);
}

static int func__UpSimSelftest(void)
{
    std::string std_string__admin;
    std::string std_string__viewer;

    printf("user panel simulator - selftest / خودآزمایی شبیه‌ساز\n");
    printf("=================================================\n");

    printf("\n-- boot / راه‌اندازی\n");
    func__UpSimCheck(UPPANEL_STATE_T__G__State.bool__storageOk, "the panel mounted its flash");
    func__UpSimCheck(UINT8_T__G__UserCount >= 2u, "the seeded accounts exist");

    printf("\n-- the model reaches the screen / رسیدن مدل به صفحه\n");
    UP_SIM_T__G__Sim.uint16_t__v24Mv = 26800u;
    UP_SIM_T__G__Sim.uint16_t__vinMv = 24100u;
    func__UpSimRefreshBody();
    func__UpSimFeedLink();
    func__UpSimAdvanceMs(1100u);
    func__UpSimLoopTimes(60u);
    func__UpSimCheck(func__UpSimLogin("admin", "admin", std_string__admin), "the admin can log in with the seeded password");
    (void)func__UpSimRoute("/api/live", HTTP_GET, std_string__admin.c_str());
    /* [EN] The live reply carries the raw telemetry words in t[]; the pack
       voltage is word 15. A named key would have been easier to assert and
       would have proved less: this way the number travelled through the real
       parser and the real index map.
       [FA] پاسخ زنده کلمه‌های خام تلمتری را در t[] می‌برد؛ ولتاژ پک کلمهٔ ۱۵
       است. یک کلید نام‌دار تأییدش آسان‌تر بود و کمتر ثابت می‌کرد: این‌طور عدد
       از دل پارسر واقعی و نقشهٔ واقعی اندیس‌ها گذشته است. */
    func__UpSimCheck(UP_WEBSERVER_T__G__Server.lastBody.find(",26800") != std::string::npos,
                     "the pack voltage on the screen is the model's, through the real parser");
    func__UpSimCheck(UP_WEBSERVER_T__G__Server.lastBody.find("\"online\":1") != std::string::npos,
                     "the link reads online while frames arrive");

    printf("\n-- roles / نقش‌ها\n");
    func__UpSimCheck(func__UpSimLogin("user", "user", std_string__viewer), "the seeded viewer can log in");
    (void)func__UpSimRoute("/api/admin/report.xlsx", HTTP_GET, std_string__viewer.c_str());
    func__UpSimCheck(UP_WEBSERVER_T__G__Server.lastCode == 403, "and is refused the Excel report");

    printf("\n-- the only write the panel makes / تنها نوشتن پنل\n");
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setHeader("Cookie", std_string__admin.c_str());
    UP_WEBSERVER_T__G__Server.setArg("a", "charger1_off");
    UP_WEBSERVER_T__G__Server.setArg("cs", "sim-selftest-1");
    (void)UP_WEBSERVER_T__G__Server.call("/api/admin/action", HTTP_POST);
    SIM_STRING__G__LastSetBody.clear();
    func__UpSimAdvanceMs(1100u);
    func__UpSimLoopTimes(80u);
    func__UpSimCheck(SIM_STRING__G__LastSetBody == "id=11&v=0",
                     "cutting charger 1 really reaches /s as id=11&v=0", SIM_STRING__G__LastSetBody.c_str());
    func__UpSimCheck(UP_SIM_T__G__Sim.uint8_t__ch1Enabled == 0u, "and the model obeyed it");

    printf("\n-- the report / گزارش\n");
    (void)func__UpSimRoute("/api/admin/report.xlsx", HTTP_GET, std_string__admin.c_str());
    func__UpSimCheck((UP_WEBSERVER_T__G__Server.lastBody.size() > 4000u) &&
                     (UP_WEBSERVER_T__G__Server.lastBody.compare(0u, 2u, "PK") == 0),
                     "the admin's Excel report streams out as a real zip",
                     std::to_string(UP_WEBSERVER_T__G__Server.lastBody.size()).c_str());

    printf("\n-- faults and the truth about clearing them / خطاها و حقیقت پاک‌کردن\n");
    func__UpSim_JitterTrip();
    func__UpSim_JitterTrip();
    func__UpSimJitterCheck();
    func__UpSimCheck(UP_SIM_T__G__Sim.uint16_t__st2 == 7u, "the third jitter trip lands in FINAL_FAULT on the board");
    func__UpSim_BoardReset();
    func__UpSimRefreshBody();
    func__UpSimFeedLink();
    func__UpSimAdvanceMs(1100u);
    func__UpSimLoopTimes(60u);
    func__UpSimCheck(UP_SIM_T__G__Sim.uint16_t__st2 == 1u, "and only a board reset clears it");
    func__UpSim_ImbalanceEpisode();
    func__UpSimAdvanceMs(3000u);
    func__UpSimRefreshBody();
    func__UpSimFeedLink();
    func__UpSimLoopTimes(60u);
    (void)func__UpSimRoute("/api/live", HTTP_GET, std_string__admin.c_str());
    func__UpSimCheck(UP_WEBSERVER_T__G__Server.lastBody.find("\"latched\":1") != std::string::npos,
                     "a lasting imbalance latches, and the screen says so");
    func__UpSim_BatterySwap();
    func__UpSimRefreshBody();
    func__UpSimFeedLink();
    func__UpSimAdvanceMs(1100u);
    func__UpSimLoopTimes(60u);
    (void)func__UpSimRoute("/api/live", HTTP_GET, std_string__admin.c_str());
    func__UpSimCheck(UP_WEBSERVER_T__G__Server.lastBody.find("\"latched\":0") != std::string::npos,
                     "a real battery swap clears the latch - the only way that works");

    printf("\n=================================================\n");
    printf("%u checks, %u failures\n", SIM_UINT__G__Passed, SIM_UINT__G__Failed);
    return (SIM_UINT__G__Failed == 0u) ? 0 : 1;
}

/* ==================== Main / اصلی ==================== */
int main(int int__argc, char **char__argv)
{
    int int__port = SIM_DEFAULT_PORT;
    bool bool__selftest = false;
    std::string std_string__here = "user_panel/simulator/sim.html";

    for (int int__index = 1; int__index < int__argc; int__index++)
    {
        std::string std_string__argument = char__argv[int__index];
        if (std_string__argument == "--selftest") { bool__selftest = true; }
        else if ((std_string__argument == "--port") && (int__index + 1 < int__argc)) { int__port = atoi(char__argv[++int__index]); }
        else if ((std_string__argument == "--page") && (int__index + 1 < int__argc)) { std_string__here = char__argv[++int__index]; }
        else if ((std_string__argument == "--speed") && (int__index + 1 < int__argc)) { SIM_UINT32_T__G__Speed = (uint32_t)atoi(char__argv[++int__index]); }
    }
    if (SIM_UINT32_T__G__Speed == 0u) { SIM_UINT32_T__G__Speed = 1u; }

    /* [EN] Settle the model's own "now" before the panel boots, so the first
       physics step is one step and not a leap from zero.
       [FA] «حالا»ی خود مدل پیش از بوت پنل جا بیفتد، تا گام اول فیزیک یک گام
       باشد و نه جهشی از صفر. */
    func__UpSim_Reset();
    stub_millis_ref() = SIM_UINT32_T__G__VirtualMs;
    stub_micros_ref() = SIM_UINT32_T__G__VirtualMs * 1000u;
    func__UpSim_Tick(SIM_UINT32_T__G__VirtualMs);
    func__UpSimRefreshBody();
    setup();

    /* [EN] The fake ESP is joined to the engineering network, exactly as a real
       one is by the time the panel starts polling. The control page's link
       button drops it on purpose, to see the panel's "no data" screens.
       [FA] ESP جعلی به شبکهٔ مهندسی وصل است، همان‌طور که یک ESP واقعی تا وقتی
       پنل پایش را شروع کند وصل شده است. دکمهٔ ارتباط در صفحهٔ کنترل عمداً
       قطعش می‌کند تا صفحه‌های «بدون داده» پنل دیده شوند. */
    WiFi.currentStatus = WL_CONNECTED;
    func__UpSimFeedLink();

    if (bool__selftest) { return func__UpSimSelftest(); }

    int int__server = socket(AF_INET, SOCK_STREAM, 0);
    int int__reuse = 1;
    setsockopt(int__server, SOL_SOCKET, SO_REUSEADDR, &int__reuse, sizeof(int__reuse));
    struct sockaddr_in struct__address;
    memset(&struct__address, 0, sizeof(struct__address));
    struct__address.sin_family = AF_INET;
    struct__address.sin_addr.s_addr = htonl(INADDR_ANY);
    struct__address.sin_port = htons((uint16_t)int__port);
    if (bind(int__server, (struct sockaddr *)&struct__address, sizeof(struct__address)) != 0)
    {
        fprintf(stderr, "cannot bind port %d\n", int__port);
        return 2;
    }
    listen(int__server, 8);

    printf("user panel simulator / شبیه‌ساز پنل کاربر\n");
    printf("  panel   : http://localhost:%d/\n", int__port);
    printf("  control : http://localhost:%d/sim\n", int__port);
    printf("  speed   : x%u of real time\n", SIM_UINT32_T__G__Speed);
    printf("  stop with Ctrl-C\n");
    fflush(stdout);

    struct timespec struct__last;
    clock_gettime(CLOCK_MONOTONIC, &struct__last);
    bool bool__running = true;

    while (bool__running)
    {
        struct timeval struct__wait;
        struct__wait.tv_sec = 0;
        struct__wait.tv_usec = 5000;
        fd_set set__read;
        FD_ZERO(&set__read);
        FD_SET(int__server, &set__read);
        if (select(int__server + 1, &set__read, NULL, NULL, &struct__wait) > 0)
        {
            int int__client = accept(int__server, NULL, NULL);
            if (int__client > 0)
            {
                sim_request_t sim_request_t__request;
                sim_response_t sim_response_t__response;
                sim_response_t__response.int__code = 200;
                sim_response_t__response.std_string__type = "application/json";

                if (func__UpSimReadRequest(int__client, &sim_request_t__request))
                {
                    if (sim_request_t__request.std_string__path == "/sim")
                    {
                        if (!func__UpSimReadFile(std_string__here, sim_response_t__response.std_string__body))
                        {
                            sim_response_t__response.int__code = 404;
                            sim_response_t__response.std_string__body = "sim.html not found";
                        }
                        sim_response_t__response.std_string__type = "text/html; charset=utf-8";
                    }
                    else if (sim_request_t__request.std_string__path == "/sim/state")
                    {
                        sim_response_t__response.std_string__body = func__UpSim_StateJson();
                    }
                    else if (sim_request_t__request.std_string__path == "/sim/cmd")
                    {
                        std::string std_string__command = sim_request_t__request.map__args.count("c") ? sim_request_t__request.map__args.at("c") : "";

                        if (std_string__command == "advance")
                        {
                            long long__seconds = sim_request_t__request.map__args.count("s") ? strtol(sim_request_t__request.map__args.at("s").c_str(), NULL, 10) : 60L;
                            if (long__seconds > 86400L) { long__seconds = 86400L; }
                            for (long step = 0L; step < long__seconds; step++)
                            {
                                func__UpSimFeedLink();
                                func__UpSimAdvanceMs(1000u);
                                func__UpSimLoopTimes(4u);
                            }
                            sim_response_t__response.std_string__body = "{\"ok\":1,\"advanced\":" + std::to_string(long__seconds) + "}";
                        }
                        else if (std_string__command == "speed")
                        {
                            SIM_UINT32_T__G__Speed = (uint32_t)(sim_request_t__request.map__args.count("v") ? strtoul(sim_request_t__request.map__args.at("v").c_str(), NULL, 10) : 1u);
                            if (SIM_UINT32_T__G__Speed == 0u) { SIM_UINT32_T__G__Speed = 1u; }
                            sim_response_t__response.std_string__body = "{\"ok\":1,\"speed\":" + std::to_string(SIM_UINT32_T__G__Speed) + "}";
                        }
                        else
                        {
                            std::string std_string__status = func__UpSimCommand(sim_request_t__request);
                            func__UpSimRefreshBody();
                            sim_response_t__response.std_string__body = "{\"ok\":1,\"did\":\"" + std_string__status + "\"}";
                        }
                    }
                    else if (!func__UpSimDispatchPanel(sim_request_t__request, &sim_response_t__response))
                    {
                        sim_response_t__response.int__code = 404;
                        sim_response_t__response.std_string__type = "text/plain; charset=utf-8";
                        sim_response_t__response.std_string__body = "no such route on the panel";
                    }
                }
                func__UpSimWriteResponse(int__client, sim_response_t__response);
                close(int__client);
            }
        }

        struct timespec struct__now;
        clock_gettime(CLOCK_MONOTONIC, &struct__now);
        uint32_t uint32_t__elapsedMs = (uint32_t)(((struct__now.tv_sec - struct__last.tv_sec) * 1000) +
                                                  ((struct__now.tv_nsec - struct__last.tv_nsec) / 1000000));
        struct__last = struct__now;
        if (uint32_t__elapsedMs > 0u)
        {
            func__UpSimAdvanceMs(uint32_t__elapsedMs * SIM_UINT32_T__G__Speed);
            func__UpSimFeedLink();
            func__UpSimLoopTimes(8u);
        }
    }

    close(int__server);
    return 0;
}
