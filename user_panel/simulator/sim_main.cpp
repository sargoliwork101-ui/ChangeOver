/**
 * @file    sim_main.cpp
 * @brief   [EN] The desktop simulator: the REAL panel, compiled and run on a
 *               laptop, next to a model of the machine, with a control page.
 *
 *          WHY IT EXISTS
 *            The host tests check pieces. This program checks the whole thing the
 *            way a person will: it serves the real page over a real socket to a
 *            real browser while the real link state machine reads telemetry from
 *            the machine model once a second, and the real Excel writer streams
 *            a workbook when somebody clicks the button. The first screen this
 *            panel is ever seen on is a laptop, not the cabinet.
 *
 *          WHAT IS REAL AND WHAT IS NOT
 *            REAL: every up_*.h of the panel, the routes, the cookies, the
 *            roles, the request handlers, the workbook writer and the page.
 *            NOT REAL: the ESP8266 (tools/stub_esp.h), the STM32 and the machine
 *            (sim_machine.h), and time - the clock can be advanced by days in a
 *            second, because a daily rollup cannot be tested by waiting.
 *
 *          HOW TO RUN
 *            bash user_panel/simulator/run_sim.sh             # build and serve
 *            bash user_panel/simulator/run_sim.sh --selftest   # no sockets
 *            Then open http://localhost:8090/sim (control) and / (the panel).
 *
 * @brief   [FA] شبیه‌ساز رومیزی: خودِ پنل واقعی، کامپایل‌شده و در حال اجرا روی
 *               لپ‌تاپ، در کنار مدلی از ماشین و یک صفحهٔ کنترل.
 *
 *          چرا وجود دارد
 *            تست‌های میزبان تکه‌تکه را می‌سنجند. این برنامه کل ماجرا را همان‌طور
 *            می‌سنجد که یک آدم می‌بیند: صفحهٔ واقعی را روی سوکت واقعی به مرورگر
 *            واقعی می‌دهد، در حالی که ماشین حالت واقعیِ لینک هر ثانیه تلمتری را از
 *            مدل ماشین می‌خواند و نویسندهٔ واقعی اکسل وقتی کسی دکمه را می‌زند یک
 *            کتاب کار جریانی می‌فرستد. اولین صفحه‌ای که این پنل رویش دیده می‌شود
 *            لپ‌تاپ است، نه تابلو.
 *
 *          چه چیزی واقعی است و چه چیزی نه
 *            واقعی: تک‌تک up_*.hهای پنل، مسیرها، کوکی‌ها، نقش‌ها، هندلرها،
 *            نویسندهٔ اکسل و خود صفحه.
 *            غیرواقعی: خود ESP8266 (tools/stub_esp.h)، STM32 و ماشین
 *            (sim_machine.h) و خود زمان - ساعت را می‌توان در یک ثانیه چند روز
 *            جلو برد، چون جمع روزانه با انتظار کشیدن آزمایش نمی‌شود.
 *
 *          طرز اجرا
 *            bash user_panel/simulator/run_sim.sh             # ساخت و اجرا
 *            bash user_panel/simulator/run_sim.sh --selftest    # بدون سوکت
 *            بعد http://localhost:8090/sim (کنترل) و / (خود پنل) را باز کنید.
 */

#include "stub_esp.h"

/* [EN] the globals the stubs declare extern / [FA] متغیرهای جهانی که استاب‌ها extern کرده‌اند */
std::map<uint8_t, int> G_StubGpio;
StubSerial Serial;
StubWiFi WiFi;
StubFS LittleFS;
std::string WiFiClient::G_NextResponse;
std::string WiFiClient::G_LastRequest;
int WiFiClient::G_ConnectFails = 0;

/* [EN] The REAL panel, unmodified: one translation unit, ESP8266 branch. */
/* [FA] خودِ پنل، دست‌نخورده: یک واحد ترجمه، شاخهٔ ESP8266. */
#define ESP8266 1
#include "../user_panel.ino"

#include "sim_machine.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define SIM_TICK_MS        100u      /* [EN] one machine tick / [FA] یک تیک ماشین */
#define SIM_LOOP_MS        20u       /* [EN] server wait between visits / [FA] مکث سرور بین سرکشی‌ها */
#define SIM_RAW_MAX        262144u   /* [EN] an xlsx report fits many times over / [FA] چند برابر یک گزارش اکسل */
#define SIM_TRANSCRIPT_MAX 8192u     /* [EN] trim the wire transcript here / [FA] کوتاه‌کردن متن سیم از اینجا */

static size_t SIM_SIZE_T__G__SeenRequest = 0u;
static uint32_t SIM_UINT32_T__G__VirtualMs = 0u;
static std::string SIM_STRING__G__LastSetBody;
static std::string SIM_STRING__G__Cookie;
static int SIM_INT__G__Checks = 0;
static int SIM_INT__G__Fails = 0;

/* ==================== Small helpers / کمک‌های کوچک ==================== */
/**
 * @brief  [EN] One line of the simulator's own checklist. Deliberately the same
 *              shape as the host tests' so a failure reads the same way in both.
 *         [FA] یک خط از چک‌لیست خود شبیه‌ساز. عمداً هم‌شکل تست‌های میزبان تا یک
 *              خطا در هر دو یک‌جور خوانده شود.
 * @param  bool__ok [EN] the result / [FA] نتیجه
 * @param  std_string__what [EN] what was checked / [FA] چه چیزی بررسی شد
 * @param  std_string__detail [EN] optional evidence / [FA] شاهد اختیاری
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimCheck(bool bool__ok, const std::string &std_string__what,
                             const std::string &std_string__detail = "")
{
    SIM_INT__G__Checks++;
    if (bool__ok)
    {
        std::cout << "  ok   " << std_string__what << "\n";
        return;
    }
    SIM_INT__G__Fails++;
    std::cout << "  FAIL " << std_string__what;
    if (!std_string__detail.empty())
    {
        std::cout << "   [" << std_string__detail << "]";
    }
    std::cout << "\n";
}

static void func__UpSimWriteAll(int int__fd, const std::string &std_string__text)
{
    size_t size_t__sent = 0u;

    while (size_t__sent < std_string__text.size())
    {
        ssize_t ssize_t__n = send(int__fd, std_string__text.data() + size_t__sent,
                                  std_string__text.size() - size_t__sent, 0);

        if (ssize_t__n <= 0)
        {
            return;
        }
        size_t__sent += (size_t)ssize_t__n;
    }
}

/* ==================== Virtual time / زمان مجازی ==================== */
/**
 * @brief  [EN] Answer whatever the panel just asked the machine over the stub
 *              socket. The stub records every byte the panel writes; this looks
 *              for new text and answers it - `/t` with live telemetry, `/m` with
 *              the peak window, and `POST /s` by handing the body to the model,
 *              which answers the same refusal the engineering ESP gives for an
 *              id the protocol does not carry.
 *         [FA] پاسخ به هر چیزی که پنل همین حالا از ماشین پرسید، روی سوکت جعلی.
 *              استاب هر بایتی که پنل می‌نویسد را ثبت می‌کند؛ این تابع متن تازه را
 *              می‌بیند و جواب می‌دهد - `/t` با تلمتری زنده، `/m` با پنجرهٔ اوج و
 *              `POST /s` با دادن بدنه به مدل، که همان ردی را می‌دهد که ESP
 *              مهندسی برای شناسهٔ بیرون از پروتکل می‌دهد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimFeedLink(void)
{
    size_t size_t__total = WiFiClient::G_LastRequest.size();

    if (size_t__total > SIM_SIZE_T__G__SeenRequest)
    {
        std::string std_string__fresh = WiFiClient::G_LastRequest.substr(SIM_SIZE_T__G__SeenRequest);

        SIM_SIZE_T__G__SeenRequest = size_t__total;

        if (std_string__fresh.find("POST /s") != std::string::npos)
        {
            size_t size_t__at = std_string__fresh.find("\r\n\r\n");
            std::string std_string__body = (size_t__at != std::string::npos)
                                         ? std_string__fresh.substr(size_t__at + 4u) : std::string();

            SIM_STRING__G__LastSetBody = std_string__body;
            WiFiClient::G_NextResponse =
                std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") +
                func__UpSim_SetParam(std_string__body);
        }
        else if (std_string__fresh.find("GET /m") != std::string::npos)
        {
            std::string std_string__body = func__UpSim_PeaksBody();

            WiFiClient::G_NextResponse =
                std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                std::to_string(std_string__body.size()) + "\r\n\r\n" + std_string__body;
        }
        else if (std_string__fresh.find("GET /t") != std::string::npos)
        {
            std::string std_string__body = func__UpSim_TelemetryBody();

            WiFiClient::G_NextResponse =
                std::string("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ") +
                std::to_string(std_string__body.size()) + "\r\n\r\n" + std_string__body;
        }
    }

    if (WiFiClient::G_LastRequest.size() > SIM_TRANSCRIPT_MAX)
    {
        WiFiClient::G_LastRequest.clear();
        SIM_SIZE_T__G__SeenRequest = 0u;
    }
}

/**
 * @brief  [EN] Push the panel's millisecond clock and the machine forward by
 *              `ms` of VIRTUAL time. Everything the panel does - the one-second
 *              telemetry read, the fifteen-second sample, the daily rollup - is
 *              driven by `millis()`, so this one function is what makes "advance
 *              one day" instant.
 *         [FA] جلو بردن ساعت میلی‌ثانیهٔ پنل و ماشین به اندازهٔ `ms` زمان
 *              **مجازی**. هر کاری که پنل می‌کند - خواندن یک‌ثانیه‌ای تلمتری،
 *              نمونهٔ پانزده‌ثانیه‌ای، جمع روزانه - با `millis()` کار می‌کند، پس
 *              همین یک تابع است که «یک روز جلو برو» را لحظه‌ای می‌کند.
 * @param  uint32_t uint32_t__ms [EN] virtual ms / [FA] میلی‌ثانیهٔ مجازی
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSimAdvanceMs(uint32_t uint32_t__ms)
{
    uint32_t uint32_t__left = uint32_t__ms;

    while (uint32_t__left > 0u)
    {
        uint32_t uint32_t__step = (uint32_t__left > SIM_TICK_MS) ? SIM_TICK_MS : uint32_t__left;

        stub_millis_ref() += (unsigned long)uint32_t__step;
        stub_micros_ref() += (unsigned long)uint32_t__step * 1000ul;
        SIM_UINT32_T__G__VirtualMs += uint32_t__step;
        func__UpSim_Tick(uint32_t__step);
        func__UpSimFeedLink();
        loop();
        uint32_t__left -= uint32_t__step;
    }
}

/* ==================== The panel's own routes / مسیرهای خود پنل ==================== */
/**
 * @brief  [EN] Call one of the panel's routes the way the ESP8266WebServer would,
 *              with the query string or the form body already split into
 *              arguments, and return what the handler produced.
 *         [FA] صدا زدن یکی از مسیرهای پنل همان‌طور که ESP8266WebServer می‌کند،
 *              با رشتهٔ پرس‌وجو یا بدنهٔ فرم که از قبل به آرگومان شکسته شده، و
 *              برگرداندن چیزی که هندلر ساخت.
 * @param  std_string__method [EN] GET or POST / [FA] GET یا POST
 * @param  std_string__path [EN] route / [FA] مسیر
 * @param  std_string__query [EN] query string / [FA] رشتهٔ پرس‌وجو
 * @param  std_string__body [EN] form body / [FA] بدنهٔ فرم
 * @param  std_string__cookie [EN] Cookie header, may be empty / [FA] هدر کوکی
 * @param  int__codeOut [EN] status the handler sent / [FA] کدی که هندلر فرستاد
 * @return [EN] the reply body / [FA] بدنهٔ پاسخ
 */
static std::string func__UpSimCallRoute(const std::string &std_string__method, const std::string &std_string__path,
                                        const std::string &std_string__query, const std::string &std_string__body,
                                        const std::string &std_string__cookie, int *int__codeOut)
{
    std::string std_string__pairs = (std_string__method == "POST") ? std_string__body : std_string__query;
    size_t size_t__at = 0u;

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.clearHeaders();
    UP_WEBSERVER_T__G__Server.clearResponse();

    if (!std_string__cookie.empty())
    {
        UP_WEBSERVER_T__G__Server.setHeader("Cookie", std_string__cookie.c_str());
    }

    while (size_t__at < std_string__pairs.size())
    {
        size_t size_t__amp = std_string__pairs.find('&', size_t__at);
        size_t size_t__eq;

        if (size_t__amp == std::string::npos)
        {
            size_t__amp = std_string__pairs.size();
        }

        std::string std_string__pair = std_string__pairs.substr(size_t__at, size_t__amp - size_t__at);

        size_t__eq = std_string__pair.find('=');
        if (size_t__eq != std::string::npos)
        {
            UP_WEBSERVER_T__G__Server.setArg(std_string__pair.substr(0u, size_t__eq).c_str(),
                                             std_string__pair.substr(size_t__eq + 1u).c_str());
        }
        size_t__at = size_t__amp + 1u;
    }

    if (!UP_WEBSERVER_T__G__Server.call(std_string__path.c_str(),
                                        (std_string__method == "POST") ? HTTP_POST : HTTP_GET))
    {
        if (int__codeOut != NULL)
        {
            *int__codeOut = 404;
        }
        return std::string("not found");
    }

    if (int__codeOut != NULL)
    {
        *int__codeOut = (UP_WEBSERVER_T__G__Server.lastCode != 0) ? UP_WEBSERVER_T__G__Server.lastCode : 200;
    }

    return UP_WEBSERVER_T__G__Server.lastBody;
}

/**
 * @brief  [EN] Log in through the REAL login route and keep the cookie it sets,
 *              exactly as a browser would.
 *         [FA] ورود از همان مسیر واقعی ورود و نگه‌داشتن کوکی‌ای که می‌گذارد،
 *              دقیقاً همان‌طور که یک مرورگر می‌کند.
 * @param  char__user [EN] user name / [FA] نام کاربری
 * @param  char__pass [EN] password / [FA] گذرواژه
 * @return [EN] the cookie, empty when it failed / [FA] کوکی، خالی اگر نشد
 */
static std::string func__UpSimLogin(const char *char__user, const char *char__pass)
{
    int int__code = 0;
    std::string std_string__body = func__UpSimCallRoute("POST", "/api/login", std::string(),
                                                        std::string("u=") + char__user + "&p=" + char__pass,
                                                        std::string(), &int__code);
    std::string std_string__header = UP_WEBSERVER_T__G__Server.sentHeaders["Set-Cookie"];
    size_t size_t__end;

    if ((int__code != 200) || (std_string__body.find("\"ok\":1") == std::string::npos))
    {
        return std::string();
    }

    size_t__end = std_string__header.find(';');
    return std_string__header.substr(0u, size_t__end);
}

/* ==================== The control page's data / دادهٔ صفحهٔ کنترل ==================== */
/**
 * @brief  [EN] How many fault bits are set right now. The control page shows the
 *              number the panel shows, computed the same way: by counting bits,
 *              not by trusting a counter.
 *         [FA] همین حالا چند بیت خطا روشن است. صفحهٔ کنترل همان عددی را نشان
 *              می‌دهد که پنل نشان می‌دهد و همان‌طور حسابش می‌کند: با شمردن بیت‌ها،
 *              نه با اعتماد به یک شمارنده.
 * @return [EN] number of set bits / [FA] تعداد بیت‌های روشن
 */
static uint8_t func__UpSim_FaultCount(void)
{
    uint32_t uint32_t__bits = UP_SIM_T__G__Plant.uint32_t__flags;
    uint8_t uint8_t__count = 0u;

    while (uint32_t__bits != 0u)
    {
        uint8_t__count += (uint8_t)(uint32_t__bits & 1u);
        uint32_t__bits >>= 1;
    }

    return uint8_t__count;
}

/**
 * @brief  [EN] The machine and the panel as one object for the control page. It
 *              reports the panel's own view too (link up, clock set) because the
 *              page's job is to explain what the panel is showing and why.
 *         [FA] ماشین و پنل در یک شیء برای صفحهٔ کنترل. دید خود پنل را هم گزارش
 *              می‌کند (وصل بودن لینک، تنظیم بودن ساعت) چون کار آن صفحه توضیح
 *              این است که پنل چه چیزی و چرا نشان می‌دهد.
 * @return [EN] JSON / [FA] JSON
 */
static uint8_t func__UpSim_FaultCount(void);

static std::string func__UpSim_StateJson(void)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;
    char char__json[1024];

    snprintf(char__json, sizeof(char__json),
             "{\"vin\":%lu,\"v24\":%lu,\"v12\":%lu,\"vhigh\":%lu,\"vlow\":%lu,"
             "\"i1\":%lu,\"i2\":%lu,\"duty1\":%lu,\"duty2\":%lu,\"st1\":%lu,\"st2\":%lu,"
             "\"fl\":%lu,\"fl2\":%lu,\"imb\":%lu,\"latched\":%u,\"cleared\":%u,"
             "\"manual\":%u,\"chg1\":%u,\"chg2\":%u,\"frames\":%lu,\"seq\":%lu,"
             "\"linkUp\":%u,\"clockSet\":%u,\"ssid\":\"%s\",\"uptimeS\":%lu,"
             "\"wSsid\":\"%s\",\"wPass\":\"%s\","
             "\"panelV24\":%lu,\"panelFaults\":%u}",
             (unsigned long)up_sim_t__p->uint32_t__vinMv,
             (unsigned long)up_sim_t__p->uint32_t__v24Mv,
             (unsigned long)up_sim_t__p->uint32_t__v12Mv,
             (unsigned long)(up_sim_t__p->uint32_t__v24Mv - up_sim_t__p->uint32_t__v12Mv),
             (unsigned long)up_sim_t__p->uint32_t__v12Mv,
             (unsigned long)up_sim_t__p->uint32_t__i1Ma,
             (unsigned long)up_sim_t__p->uint32_t__i2Ma,
             (unsigned long)up_sim_t__p->uint32_t__duty1,
             (unsigned long)up_sim_t__p->uint32_t__duty2,
             (unsigned long)up_sim_t__p->uint32_t__st1,
             (unsigned long)up_sim_t__p->uint32_t__st2,
             (unsigned long)up_sim_t__p->uint32_t__flags,
             (unsigned long)up_sim_t__p->uint32_t__flags2,
             (unsigned long)up_sim_t__p->uint32_t__imbOffsetMv,
             (unsigned)up_sim_t__p->uint8_t__latched,
             (unsigned)up_sim_t__p->uint8_t__imbCleared,
             (unsigned)up_sim_t__p->uint8_t__manual,
             (unsigned)up_sim_t__p->uint8_t__chg1Enable,
             (unsigned)up_sim_t__p->uint8_t__chg2Enable,
             (unsigned long)up_sim_t__p->uint32_t__frames,
             (unsigned long)up_sim_t__p->uint32_t__seq,
             (UPPANEL_STATE_T__G__State.bool__staUp ? 1u : 0u),
             (func__UpState_NowEpochS() != 0u) ? 1u : 0u,
             func__UpSettings_Active()->char__ssid,
             (unsigned long)(SIM_UINT32_T__G__VirtualMs / 1000u),
             WiFi.staSsid.c_str(),
             WiFi.staPass.c_str(),
             (unsigned long)func__UpState_TlmWord(UP_TLM_V24),
             (unsigned)func__UpSim_FaultCount());

    return std::string(char__json);
}

/* ==================== A request, parsed / یک درخواست، تجزیه‌شده ==================== */
typedef struct
{
    std::string std_string__method;
    std::string std_string__path;
    std::string std_string__query;
    std::string std_string__body;
    std::string std_string__cookie;
} sim_request_t;

static std::string SIM_STRING__G__Page;

/**
 * @brief  [EN] One value out of a form/query string, or an empty string.
 *         [FA] یک مقدار از رشتهٔ فرم یا پرس‌وجو، یا رشتهٔ خالی.
 * @param  std_string__pairs [EN] "a=1&b=2" / [FA] همان
 * @param  char__name [EN] key / [FA] کلید
 * @return [EN] the value / [FA] مقدار
 */
static std::string func__UpSim_Arg(const std::string &std_string__pairs, const char *char__name)
{
    std::string std_string__needle = std::string(char__name) + "=";
    size_t size_t__at = std_string__pairs.find(std_string__needle);

    if (size_t__at == std::string::npos)
    {
        return std::string();
    }
    if ((size_t__at > 0u) && (std_string__pairs[size_t__at - 1u] != '&'))
    {
        return std::string();
    }

    size_t size_t__from = size_t__at + std_string__needle.size();
    size_t size_t__end = std_string__pairs.find('&', size_t__from);

    return (size_t__end == std::string::npos) ? std_string__pairs.substr(size_t__from)
                                              : std_string__pairs.substr(size_t__from, size_t__end - size_t__from);
}

/**
 * @brief  [EN] The control endpoints. Nothing here exists on the board: `/sim`
 *              is the page, `/sim/state` is what the model is doing and
 *              `/sim/cmd` is a hand on the machine (open the mains switch, pull
 *              a battery wire, inject a fault, jump the clock forward). Keeping
 *              them in one function is what makes the promise checkable - the
 *              panel's own routes are never faked, they are the real ones.
 *         [FA] نقاط پایانی کنترل. هیچ‌کدام روی برد وجود ندارند: `/sim` صفحه است،
 *              `/sim/state` کارِ مدل و `/sim/cmd` دستِ آدم روی ماشین (کلید برق را
 *              باز کن، سیم باتری را بکش، خطا تزریق کن، ساعت را جلو ببر). بودنشان
 *              در یک تابع همان چیزی است که آن وعده را قابل بررسی می‌کند: مسیرهای
 *              خود پنل هرگز جعل نمی‌شوند، همان‌های واقعی‌اند.
 * @param  sim_request_t__req [EN] the request / [FA] درخواست
 * @param  std_string__type [EN] out: content type / [FA] خروجی: نوع محتوا
 * @param  std_string__body [EN] out: body / [FA] خروجی: بدنه
 * @param  int__code [EN] out: status / [FA] خروجی: کد وضعیت
 * @return [EN] true when this was a control request / [FA] اگر درخواست کنترل بود
 */
static bool func__UpSim_Control(const sim_request_t &sim_request_t__req, std::string &std_string__type,
                                std::string &std_string__body, int &int__code)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;

    if ((sim_request_t__req.std_string__path == "/sim") || (sim_request_t__req.std_string__path == "/sim.html"))
    {
        std_string__type = "text/html; charset=utf-8";
        std_string__body = SIM_STRING__G__Page;
        int__code = 200;
        return true;
    }

    if (sim_request_t__req.std_string__path == "/sim/state")
    {
        std_string__type = "application/json; charset=utf-8";
        std_string__body = func__UpSim_StateJson();
        int__code = 200;
        return true;
    }

    if (sim_request_t__req.std_string__path == "/sim/cmd")
    {
        std::string std_string__cmd = func__UpSim_Arg(sim_request_t__req.std_string__query, "c");
        std::string std_string__value = func__UpSim_Arg(sim_request_t__req.std_string__query, "v");

        if (std_string__cmd == "input")
        {
            up_sim_t__p->uint32_t__vinMv = (std_string__value == "1") ? SIM_INPUT_MV : 0u;
        }
        else if (std_string__cmd == "chg")
        {
            std::string std_string__ch = func__UpSim_Arg(sim_request_t__req.std_string__query, "ch");
            uint8_t uint8_t__on = (std_string__value == "1") ? 1u : 0u;

            if (std_string__ch == "2")
            {
                up_sim_t__p->uint8_t__chg2Enable = uint8_t__on;
            }
            else
            {
                up_sim_t__p->uint8_t__chg1Enable = uint8_t__on;
            }
        }
        else if (std_string__cmd == "imb")
        {
            long long__mv = strtol(std_string__value.c_str(), NULL, 10);

            up_sim_t__p->uint32_t__imbOffsetMv = (long__mv > 0) ? (uint32_t)long__mv : 0u;
            if (long__mv <= 0)
            {
                up_sim_t__p->uint32_t__ageMs = 0u;   /* [EN] the battery-off clock starts / [FA] ساعت باتری‌جدا شروع می‌شود */
            }
        }
        else if (std_string__cmd == "fault")
        {
            std::string std_string__bit = func__UpSim_Arg(sim_request_t__req.std_string__query, "b");
            uint32_t uint32_t__bit = 1u << (uint32_t)strtoul(std_string__bit.c_str(), NULL, 10);

            if (std_string__value == "1")
            {
                up_sim_t__p->uint32_t__flags |= uint32_t__bit;
                if (uint32_t__bit == SIM_FAULT_CHG_LOST)
                {
                    up_sim_t__p->uint32_t__jitterCount++;
                    if (up_sim_t__p->uint32_t__jitterCount >= SIM_JITTER_LIMIT)
                    {
                        up_sim_t__p->uint32_t__flags2 |= 0x01u;   /* [EN] FINAL_FAULT / [FA] خطای نهایی */
                    }
                }
            }
            else
            {
                up_sim_t__p->uint32_t__flags &= ~uint32_t__bit;
            }
        }
        else if (std_string__cmd == "pack")
        {
            long long__mv = strtol(std_string__value.c_str(), NULL, 10);

            up_sim_t__p->uint32_t__v24Mv = (long__mv > 0) ? (uint32_t)long__mv : 26500u;
        }
        else if (std_string__cmd == "reset")
        {
            uint32_t uint32_t__v24 = up_sim_t__p->uint32_t__v24Mv;

            func__UpSim_Begin();
            up_sim_t__p->uint32_t__v24Mv = uint32_t__v24;
            up_sim_t__p->uint32_t__v12Mv = uint32_t__v24 / 2u;
        }
        else if (std_string__cmd == "advance")
        {
            long long__seconds = strtol(std_string__value.c_str(), NULL, 10);
            uint32_t uint32_t__ms = (long__seconds > 0) ? (uint32_t)((long)long__seconds * 1000L) : 0u;

            if (uint32_t__ms > 0u)
            {
                func__UpSimAdvanceMs(uint32_t__ms);
            }
        }
        else if (std_string__cmd == "radio")
        {
            /* [EN] The board's network going away is the one failure a laptop
                    cannot produce by unplugging anything, and it is exactly the
                    failure the panel must explain calmly instead of showing
                    stale numbers. Off = the same state the firmware reaches
                    when the join fails.
               [FA] رفتنِ شبکهٔ برد تنها خرابی‌ای است که با کشیدن هیچ سیمی روی
                    لپ‌تاپ ساخته نمی‌شود، و دقیقاً همان خرابی‌ای است که پنل باید
                    آرام توضیحش بدهد و نه عدد کهنه نشان بدهد. خاموش = همان حالتی
                    که فرم‌ور وقتی پیوستن شکست می‌خورد می‌رسد. */
            if (std_string__value == "1")
            {
                WiFi.currentStatus = WL_CONNECTED;
            }
            else
            {
                WiFi.currentStatus = WL_DISCONNECTED;
            }
        }
        else if (std_string__cmd == "nothing")
        {
            /* [EN] a no-op the page can send to prove the link is alive / [FA] بی‌کاری برای اثبات زنده بودن */
        }
        else
        {
            std_string__type = "application/json; charset=utf-8";
            std_string__body = "{\"ok\":0,\"err\":\"unknown command\"}";
            int__code = 200;
            return true;
        }

        std_string__type = "application/json; charset=utf-8";
        std_string__body = "{\"ok\":1}";
        int__code = 200;
        return true;
    }

    return false;
}

/* ==================== Serving / سرو کردن ==================== */
/**
 * @brief  [EN] Read one HTTP request off the socket and answer it. The panel's
 *              routes are the ONLY ones that serve `/api/...`: this function
 *              decides between the control endpoints and the real ones, and
 *              nothing else. The reply carries whatever headers the handler set
 *              - which is how the login cookie reaches the browser.
 *         [FA] خواندن یک درخواست HTTP از سوکت و جواب دادنش. مسیرهای پنل تنها
 *              مسیرهایی هستند که `/api/...` را سرو می‌کنند: این تابع بین نقاط
 *              پایانی کنترل و مسیرهای واقعی تصمیم می‌گیرد و بس. پاسخ هر هدری را
 *              می‌برد که هندلر گذاشته - و کوکی ورود همین‌طور به مرورگر می‌رسد.
 * @param  int int__fd [EN] accepted socket / [FA] سوکت پذیرفته‌شده
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_Serve(int int__fd)
{
    std::string std_string__raw;
    char char__chunk[2048];
    ssize_t ssize_t__got = 0;
    size_t size_t__headEnd = std::string::npos;

    while (std_string__raw.size() < SIM_RAW_MAX)
    {
        ssize_t__got = recv(int__fd, char__chunk, sizeof(char__chunk), 0);
        if (ssize_t__got <= 0)
        {
            break;
        }

        std_string__raw.append(char__chunk, (size_t)ssize_t__got);

        size_t__headEnd = std_string__raw.find("\r\n\r\n");
        if (size_t__headEnd != std::string::npos)
        {
            size_t size_t__clAt = std_string__raw.find("Content-Length:");
            size_t size_t__want = 0u;

            if (size_t__clAt != std::string::npos)
            {
                size_t__want = (size_t)strtoul(std_string__raw.c_str() + size_t__clAt + 15u, NULL, 10);
            }
            if (std_string__raw.size() >= (size_t__headEnd + 4u + size_t__want))
            {
                break;
            }
        }
    }

    size_t size_t__lineEnd = std_string__raw.find("\r\n");

    if (size_t__lineEnd == std::string::npos)
    {
        close(int__fd);
        return;
    }

    std::string std_string__line = std_string__raw.substr(0u, size_t__lineEnd);
    sim_request_t sim_request_t__req;

    sim_request_t__req.std_string__method = std_string__line.substr(0u, std_string__line.find(' '));

    std::string std_string__target = std_string__line.substr(std_string__line.find(' ') + 1u);
    std_string__target = std_string__target.substr(0u, std_string__target.find(' '));
    sim_request_t__req.std_string__path = std_string__target.substr(0u, std_string__target.find('?'));
    sim_request_t__req.std_string__query = (std_string__target.find('?') != std::string::npos)
                                         ? std_string__target.substr(std_string__target.find('?') + 1u)
                                         : std::string();

    size_t size_t__ck = std_string__raw.find("Cookie: ");
    if (size_t__ck != std::string::npos)
    {
        size_t size_t__ckEnd = std_string__raw.find("\r\n", size_t__ck);

        sim_request_t__req.std_string__cookie =
            std_string__raw.substr(size_t__ck + 8u, size_t__ckEnd - size_t__ck - 8u);
    }

    sim_request_t__req.std_string__body = (size_t__headEnd != std::string::npos)
                                        ? std_string__raw.substr(size_t__headEnd + 4u) : std::string();

    std::string std_string__type = "text/plain; charset=utf-8";
    std::string std_string__body;
    int int__code = 200;
    bool bool__control;

    /* [EN] The stub still holds the headers of whatever route answered last, so
            a control reply is built on a clean slate: otherwise a Set-Cookie
            from an earlier login would ride along with /sim/state.
       [FA] استاب هنوز هدرهای آخرین مسیری که جواب داده را نگه داشته، پس پاسخ
            کنترل روی صفحة تمیز ساخته می‌شود: وگرنه یک Set-Cookie از ورود قبلی
            همراه /sim/state می‌رود. */
    UP_WEBSERVER_T__G__Server.clearResponse();

    bool__control = func__UpSim_Control(sim_request_t__req, std_string__type, std_string__body, int__code);

    if (!bool__control)
    {
        std_string__body = func__UpSimCallRoute(sim_request_t__req.std_string__method, sim_request_t__req.std_string__path,
                                                sim_request_t__req.std_string__query, sim_request_t__req.std_string__body,
                                                sim_request_t__req.std_string__cookie, &int__code);
        if (int__code == 404)
        {
            std_string__type = "text/plain; charset=utf-8";
        }
        else
        {
            std_string__type = UP_WEBSERVER_T__G__Server.lastType;
        }
    }

    std::string std_string__head = "HTTP/1.1 " + std::to_string(int__code) + " " +
                                   ((int__code == 200) ? "OK" : ((int__code == 401) ? "Unauthorized" : "Error")) + "\r\n" +
                                   "Content-Type: " + std_string__type + "\r\n" +
                                   "Content-Length: " + std::to_string(std_string__body.size()) + "\r\n" +
                                   "Cache-Control: no-store\r\n";

    for (std::map<std::string, std::string>::const_iterator std_map__it = UP_WEBSERVER_T__G__Server.sentHeaders.begin();
         std_map__it != UP_WEBSERVER_T__G__Server.sentHeaders.end(); ++std_map__it)
    {
        std_string__head += std_map__it->first + ": " + std_map__it->second + "\r\n";
    }
    std_string__head += "Connection: close\r\n\r\n";

    func__UpSimWriteAll(int__fd, std_string__head + std_string__body);
    close(int__fd);
}

/* ==================== Self test / خودآزمایی ==================== */
/**
 * @brief  [EN] The simulator's own checks, run without opening a socket: boot
 *              the real panel, let the real link read the model, log in through
 *              the real login route, cut a charger through the real action route
 *              and watch it arrive on the wire as parameter 11, and stream the
 *              real workbook. A green run says the panel and the model still
 *              agree; it says nothing about the machine, and the gate's output
 *              says so on every run.
 *         [FA] چک‌های خود شبیه‌ساز، بدون باز کردن سوکت: بالا آوردن پنل واقعی،
 *              خواندن مدل با لینک واقعی، ورود از مسیر واقعی ورود، قطع شارژر از
 *              مسیر واقعی اقدام و دیدن رسیدنش روی سیم به‌صورت پارامتر ۱۱، و
 *              فرستادن کتاب کار واقعی. اجرای سبز یعنی پنل و مدل هنوز هم‌نظرند؛
 *              دربارهٔ ماشین چیزی نمی‌گوید و خروجی گیت در هر اجرا همین را می‌گوید.
 * @return [EN] 0 when everything passed / [FA] صفر یعنی همه قبول شدند
 */
static int func__UpSim_Selftest(void)
{
    int int__code = 0;
    std::string std_string__cookie;

    std::cout << "ChangeOver user panel - simulator self test\n";
    std::cout << "==========================================\n";

    func__UpSim_Begin();
    WiFi.currentStatus = WL_CONNECTED;
    setup();
    func__UpSimAdvanceMs(1500u);

    func__UpSimCheck(UPPANEL_STATE_T__G__State.bool__storageOk, "the panel mounted its flash");
    func__UpSimCheck(UINT8_T__G__UserCount >= 2u, "the seeded accounts exist");
    func__UpSimCheck(UP_SIM_T__G__Plant.uint32_t__v12Mv == (UP_SIM_T__G__Plant.uint32_t__v24Mv / 2u),
                     "the lower half follows the pack instead of being pinned",
                     std::to_string((unsigned long)UP_SIM_T__G__Plant.uint32_t__v12Mv));

    {
        uint32_t uint32_t__before = UP_SIM_T__G__Plant.uint32_t__v24Mv;

        func__UpSimAdvanceMs(60000u);
        func__UpSimCheck((UP_SIM_T__G__Plant.uint32_t__v24Mv > uint32_t__before) &&
                         (UP_SIM_T__G__Plant.uint32_t__v24Mv <= SIM_PACK_FULL_MV),
                         "with the mains connected the model charges and stops at full",
                         std::to_string((unsigned long)uint32_t__before) + " -> " +
                         std::to_string((unsigned long)UP_SIM_T__G__Plant.uint32_t__v24Mv));
    }

    {
        /* [EN] Freeze the machine - mains on, chargers off - so the panel's next
                read has an exact number to be compared with. A moving machine
                can only be compared with a tolerance, and a tolerance is how a
                wrong word survives a test.
           [FA] ماشین را ثابت کن - برق هست، شارژرها خاموش - تا خواندن بعدی پنل
                عدد دقیقی برای مقایسه داشته باشد. ماشین متحرک را فقط با رواداری
                می‌توان مقایسه کرد، و رواداری همان چیزی است که یک کلمهٔ اشتباه را
                از تست زنده بیرون می‌آورد. */
        UP_SIM_T__G__Plant.uint8_t__chg1Enable = 0u;
        UP_SIM_T__G__Plant.uint8_t__chg2Enable = 0u;
        func__UpSimAdvanceMs(2000u);

        func__UpSimCheck(func__UpState_TlmWord(UP_TLM_V24) == UP_SIM_T__G__Plant.uint32_t__v24Mv,
                         "the panel read the machine's own pack voltage through the real parser",
                         std::to_string((unsigned long)func__UpState_TlmWord(UP_TLM_V24)) + " vs " +
                         std::to_string((unsigned long)UP_SIM_T__G__Plant.uint32_t__v24Mv));
        func__UpSimCheck(func__UpState_TlmWord(UP_TLM_VHIGH) ==
                         (UP_SIM_T__G__Plant.uint32_t__v24Mv - UP_SIM_T__G__Plant.uint32_t__v12Mv),
                         "and the upper battery is the pack minus the lower one, as the page assumes");

        UP_SIM_T__G__Plant.uint8_t__chg1Enable = 1u;
        UP_SIM_T__G__Plant.uint8_t__chg2Enable = 1u;
    }

    {
        std::string std_string__cookie = func__UpSimLogin("admin", "admin");

        func__UpSimCheck(!std_string__cookie.empty(), "the real login route set a session cookie");
        SIM_STRING__G__Cookie = std_string__cookie;
    }

    (void)func__UpSimCallRoute("GET", "/api/live", std::string(), std::string(), std::string(), &int__code);
    func__UpSimCheck(int__code == 401, "an anonymous browser is refused", std::to_string(int__code));

    {
        std::string std_string__live = func__UpSimCallRoute("GET", "/api/live", std::string(), std::string(),
                                                            SIM_STRING__G__Cookie, &int__code);

        func__UpSimCheck((int__code == 200) && (std_string__live.find("\"faultCodes\":[") != std::string::npos),
                         "a logged-in browser gets the live view", std::to_string(int__code));
    }

    {
        std::string std_string__reply = func__UpSimCallRoute("POST", "/api/admin/action", std::string(),
                                                             "a=charger1_off&cs=sim-1", SIM_STRING__G__Cookie,
                                                             &int__code);

        func__UpSimAdvanceMs(3000u);

        func__UpSimCheck(UP_SIM_T__G__Plant.uint8_t__chg1Enable == 0u,
                         "the panel's cut reached the machine (parameter 11 = 0)",
                         SIM_STRING__G__LastSetBody + "  " + std_string__reply);
    }

    func__UpSimCheck(func__UpSim_SetParam("id=200&v=0").find("range") != std::string::npos,
                     "the simulator refuses an id the engineering ESP refuses");

    {
        std::string std_string__live;

        UP_SIM_T__G__Plant.uint32_t__flags |= SIM_FAULT_ADC;
        func__UpSimAdvanceMs(1500u);
        std_string__live = func__UpSimCallRoute("GET", "/api/live", std::string(), std::string(),
                                                SIM_STRING__G__Cookie, &int__code);
        func__UpSimCheck(std_string__live.find("\"faultCodes\":[]") == std::string::npos,
                         "an injected fault shows up in the panel's fault list");
        UP_SIM_T__G__Plant.uint32_t__flags &= ~SIM_FAULT_ADC;
    }

    {
        std::string std_string__live;

        UP_SIM_T__G__Plant.uint32_t__imbOffsetMv = 1200u;
        func__UpSimAdvanceMs(2000u);
        std_string__live = func__UpSimCallRoute("GET", "/api/live", std::string(), std::string(),
                                                SIM_STRING__G__Cookie, &int__code);
        func__UpSimCheck((std_string__live.find("\"latched\":1") != std::string::npos) &&
                         (std_string__live.find("\"blocked\":") != std::string::npos),
                         "an injected imbalance latches, and the panel reports it as the board does");

        UP_SIM_T__G__Plant.uint32_t__imbOffsetMv = 0u;
        func__UpSimAdvanceMs(3500u);
        std_string__live = func__UpSimCallRoute("GET", "/api/live", std::string(), std::string(),
                                                SIM_STRING__G__Cookie, &int__code);
        func__UpSimCheck((std_string__live.find("\"latched\":0") != std::string::npos) &&
                         (UP_SIM_T__G__Plant.uint8_t__imbCleared == 1u),
                         "and the three-second battery-off rule frees it, as the panel's text promises");
    }

    {
        /* [EN] The whole point of the network card: after the admin stores a
                second Wi-Fi network and switches to it, the RADIO must be told
                the new name AND the new password. Reading the table back would
                only prove the table.
           [FA] تمام هدف کارت شبکه: بعد از اینکه مدیر شبکهٔ وای‌فای دومی را ذخیره
                و به آن سوئیچ کرد، باید به رادیو هم نام جدید و هم رمز جدید گفته
                شود. خواندن دوبارهٔ جدول فقط خودِ جدول را ثابت می‌کند. */
        (void)func__UpSimCallRoute("POST", "/api/admin/network", std::string(),
                                   "action=save&ssid=Lab-Net&pass=Lab-Pass-2", SIM_STRING__G__Cookie, &int__code);
        (void)func__UpSimCallRoute("POST", "/api/admin/network", std::string(),
                                   "action=use&i=1", SIM_STRING__G__Cookie, &int__code);
        func__UpSimCheck((WiFi.staSsid == "Lab-Net") && (WiFi.staPass == "Lab-Pass-2"),
                         "the admin's new Wi-Fi name and password reach the radio",
                         WiFi.staSsid + " / " + std::to_string(WiFi.staPass.size()) + " chars");

        (void)func__UpSimCallRoute("POST", "/api/admin/network", std::string(),
                                   "action=use&i=0", SIM_STRING__G__Cookie, &int__code);
        func__UpSimCheck(WiFi.staSsid == UP_STA_SSID,
                         "and switching back returns the radio to the factory network",
                         WiFi.staSsid);
    }

    {
        std::string std_string__book = func__UpSimCallRoute("GET", "/api/admin/report.xlsx", "days=1", std::string(),
                                                            SIM_STRING__G__Cookie, &int__code);

        func__UpSimCheck((int__code == 200) && (std_string__book.size() > 2u) && (std_string__book[0] == 'P') &&
                         (std_string__book[1] == 'K'),
                         "the real Excel writer streamed a workbook",
                         std::to_string(std_string__book.size()) + " bytes");
    }

    {
        std::string std_string__card;

        (void)func__UpSimCallRoute("POST", "/api/clock", std::string(), "t=1791331200&tz=210",
                                   SIM_STRING__G__Cookie, &int__code);
        std_string__card = func__UpSimCallRoute("GET", "/api/admin/clock", std::string(), std::string(),
                                                SIM_STRING__G__Cookie, &int__code);
        func__UpSimCheck(std_string__card.find("1405/07/15") != std::string::npos,
                         "the clock route reads back an Iran date", std_string__card);
    }

    std::cout << "==========================================\n";
    std::cout << SIM_INT__G__Checks << " checks, " << SIM_INT__G__Fails << " failures\n";

    return (SIM_INT__G__Fails == 0) ? 0 : 1;
}

/* ==================== Entry / ورود ==================== */
/**
 * @brief  [EN] `--selftest` runs the checks and exits; otherwise the simulator
 *              serves HTTP on `--port` (8090 by default) and drives the panel's
 *              runtime in a loop: advance the virtual clock by the real time
 *              that passed, answer whatever the link asked the machine, run one
 *              pass of the panel's own loop.
 *         [FA] `--selftest` چک‌ها را اجرا و خارج می‌شود؛ در غیر این صورت
 *              شبیه‌ساز روی `--port` (پیش‌فرض ۸۰۹۰) سرو می‌کند و زمان اجرای پنل را
 *              در یک حلقه می‌چرخاند: ساعت مجازی را به اندازهٔ زمان واقعیِ گذشته
 *              جلو می‌برد، هر چه لینک از ماشین پرسید جواب می‌دهد و یک دور از حلقهٔ
 *              خود پنل را اجرا می‌کند.
 * @param  int__argc [EN] count / [FA] تعداد
 * @param  char__argv [EN] arguments / [FA] آرگومان‌ها
 * @return [EN] 0 on a clean exit / [FA] صفر در خروج سالم
 */
int main(int int__argc, char **char__argv)
{
    int int__port = 8090;
    bool bool__selftest = false;
    std::string std_string__pagePath = "sim.html";
    int int__listen = -1;
    struct timespec int__lastWall;
    int int__index;

    for (int__index = 1; int__index < int__argc; int__index++)
    {
        if (strcmp(char__argv[int__index], "--selftest") == 0)
        {
            bool__selftest = true;
        }
        else if ((strcmp(char__argv[int__index], "--port") == 0) && ((int__index + 1) < int__argc))
        {
            int__port = (int)strtol(char__argv[++int__index], NULL, 10);
        }
        else if ((strcmp(char__argv[int__index], "--page") == 0) && ((int__index + 1) < int__argc))
        {
            std_string__pagePath = char__argv[++int__index];
        }
    }

    if (bool__selftest)
    {
        return func__UpSim_Selftest();
    }

    {
        std::ifstream std_ifstream__page(std_string__pagePath.c_str());

        if (std_ifstream__page)
        {
            std::stringstream std_stringstream__buffer;

            std_stringstream__buffer << std_ifstream__page.rdbuf();
            SIM_STRING__G__Page = std_stringstream__buffer.str();
        }
        else
        {
            SIM_STRING__G__Page = "<html><body style=\"font-family:sans-serif\"><h2>/sim is not available</h2>"
                                  "<p>The control page file was not found. Start the simulator with "
                                  "<code>bash user_panel/simulator/run_sim.sh</code>, which passes its path.</p>"
                                  "</body></html>";
        }
    }

    func__UpSim_Begin();
    WiFi.currentStatus = WL_CONNECTED;
    setup();

    {
        struct sockaddr_in sockaddr_in__addr;

        int__listen = socket(AF_INET, SOCK_STREAM, 0);
        int int__one = 1;

        (void)setsockopt(int__listen, SOL_SOCKET, SO_REUSEADDR, &int__one, sizeof(int__one));
        memset(&sockaddr_in__addr, 0, sizeof(sockaddr_in__addr));
        sockaddr_in__addr.sin_family = AF_INET;
        sockaddr_in__addr.sin_addr.s_addr = htonl(INADDR_ANY);
        sockaddr_in__addr.sin_port = htons((uint16_t)int__port);

        if ((int__listen < 0) || (bind(int__listen, (struct sockaddr *)&sockaddr_in__addr, sizeof(sockaddr_in__addr)) != 0) ||
            (listen(int__listen, 8) != 0))
        {
            std::cout << "cannot listen on port " << int__port << "\n";
            return 1;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &int__lastWall);

    std::cout << "user panel simulator / شبیه‌ساز پنل کاربر\n";
    std::cout << "  panel   : http://localhost:" << int__port << "/\n";
    std::cout << "  control : http://localhost:" << int__port << "/sim\n";
    std::cout << "  machine : 24 V pack, mains present, both chargers enabled\n";
    std::cout << "  stop    : Ctrl-C\n";

    for (;;)
    {
        fd_set fd_set__read;
        struct timeval struct_timeval__wait;
        struct timespec int__nowWall;

        FD_ZERO(&fd_set__read);
        FD_SET(int__listen, &fd_set__read);
        struct_timeval__wait.tv_sec = 0;
        struct_timeval__wait.tv_usec = (long)SIM_LOOP_MS * 1000L;

        if (select(int__listen + 1, &fd_set__read, NULL, NULL, &struct_timeval__wait) > 0)
        {
            int int__client = accept(int__listen, NULL, NULL);

            if (int__client >= 0)
            {
                func__UpSim_Serve(int__client);
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &int__nowWall);
        {
            long long__ms = (long)((int__nowWall.tv_sec - int__lastWall.tv_sec) * 1000L) +
                            (long)((int__nowWall.tv_nsec - int__lastWall.tv_nsec) / 1000000L);

            int__lastWall = int__nowWall;
            if ((long__ms > 0) && (long__ms < 5000))
            {
                func__UpSimAdvanceMs((uint32_t)long__ms);
            }
        }
    }

    return 0;
}
