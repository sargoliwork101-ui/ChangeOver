/**
 * @file   host_test_user_panel.cpp
 * @brief  [EN] The user panel, compiled and exercised on a PC.
 *
 *         WHY
 *           Nothing else in this repository compiles user_panel/. The Arduino
 *           IDE does it at flashing time, on a machine with the ESP8266 core
 *           installed, which means a typo, a missing cast or a C++ construct
 *           hidden in a C habit is found by flashing a board - the slowest
 *           possible place to find it. This file includes user_panel.ino
 *           UNMODIFIED, in the same single translation unit and the same
 *           include order the board uses, compiles it with -Wall -Wextra
 *           -Werror and the address/undefined-behaviour sanitizers, and then
 *           runs the paths where being wrong is silent: the login handshake,
 *           the role gates, the ONLY write this panel ever sends, the ring that
 *           throws away its oldest data, and the statistics reply the page
 *           draws.
 *
 *         WHAT IT ALREADY FOUND
 *           - func__UpLink_PumpParams() was never called from the loop, so the
 *             admin's "cut the charger" button queued the write and never sent
 *             it. The page would have said "done" forever.
 *           - ClearSessions / AddUser were used above their definitions.
 *           - two identifiers that did not exist (a mistyped global and a
 *             mistyped loop variable) - a C compiler error each, on the board.
 *           - the JSON writer overflowed on /api/series: three 120-point arrays
 *             plus per-point tooltip strings do not fit in the dashboard-sized
 *             buffer, so the reply arrived truncated and the chart was empty.
 *             The payload was made smaller (numeric ages, not text) and the
 *             buffer sized for the worst case.
 *
 *         The tests are deliberately about BEHAVIOUR: they call the real route
 *         handlers through the fake web server and read what came back, rather
 *         than poking at internals that a refactor is allowed to move.
 *
 * @brief  [FA] پنل کاربر، کامپایل و اجرا‌شده روی PC.
 *
 *         چرا
 *           هیچ‌چیز دیگری در این مخزن user_panel/ را کامپایل نمی‌کند. Arduino
 *           IDE این کار را سر فلش‌کردن و روی دستگاهی که هستهٔ ESP8266 دارد
 *           انجام می‌دهد، یعنی یک غلط تایپی، یک cast جاافتاده یا یک ساختار ++C
 *           که زیر عادت C پنهان شده، با فلش‌کردن برد پیدا می‌شود - کندترین جای
 *           ممکن. این فایل user_panel.ino را «دست‌نخورده» include می‌کند، در
 *           همان یک واحد ترجمه و همان ترتیب include‌ای که برد دارد، با
 *           -Wall -Wextra -Werror و سنیتایزرهای آدرس/رفتار تعریف‌نشده
 *           کامپایلش می‌کند و بعد مسیرهایی را اجرا می‌کند که خرابی‌شان بی‌صداست:
 *           دست‌دادن ورود، نگهبان‌های نقش، تنها نوشتنی‌ای که این پنل می‌فرستد،
 *           حلقه‌ای که قدیمی‌ترین داده‌اش را دور می‌ریزد و پاسخ آماری که صفحه
 *           می‌کشد.
 *
 *         همین حالا چه چیزهایی را پیدا کرد
 *           - func__UpLink_PumpParams() هرگز از حلقه صدا زده نمی‌شد، پس دکمهٔ
 *             «قطع شارژر» مدیر نوشتن را صف می‌کرد و هرگز نمی‌فرستاد. صفحه
 *             تا ابد «انجام شد» می‌گفت.
 *           - ClearSessions و AddUser پیش از تعریفشان استفاده شده بودند.
 *           - دو شناسه که وجود نداشتند (یک متغیر جهانی و یک متغیر حلقهٔ غلط) -
 *             هر کدام یک خطای کامپایل C، روی برد.
 *           - نویسندهٔ JSON روی /api/series سرریز می‌کرد: سه آرایهٔ ۱۲۰ نقطه‌ای
 *             به‌علاوهٔ متن راهنما برای هر نقطه در بافرِ اندازهٔ داشبورد جا
 *             نمی‌شود، پس پاسخ بریده می‌رسید و نمودار خالی می‌ماند. بار پاسخ
 *             کوچک‌تر شد (سن عددی به‌جای متن) و بافر برای بدترین حالت اندازه
 *             گرفت.
 *
 *         تست‌ها عمداً رفتاری‌اند: هندلرهای واقعی مسیرها را از راه سرور وب جعلی
 *         صدا می‌زنند و آنچه برگشته را می‌خوانند، نه اینکه به درون‌ریزهایی
 *         دست بزنند که یک بازآرایی مجاز است جابه‌جا کند.
 */

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
/* [FA] خودِ پنل، دست‌نخورده: یک واحد ترجمه، شاخهٔ ESP8266. */
#define ESP8266 1
#include "../user_panel.ino"

#include <iostream>
#include <string>
#include <vector>

static int checkCount = 0;
static int failCount = 0;

static void check(bool condition, const std::string &what, const std::string &detail = "")
{
    checkCount++;
    if (condition)
    {
        std::cout << "  ok   " << what << "\n";
        return;
    }
    failCount++;
    std::cout << "  FAIL " << what;
    if (!detail.empty())
    {
        std::cout << "   [" << detail << "]";
    }
    std::cout << "\n";
}

static std::string sessionCookie;

/* ---------------- helpers the tests share ------------------------------- */

/* [EN] One /t body, built the way the engineering panel builds it, so the
   parser is tested against the real shape rather than a convenient one. */
static std::string telemetryBody(uint32_t seq, uint32_t vIn, uint32_t v24, uint32_t v12,
                                 uint32_t lowCurrent, uint32_t state1, uint32_t state2,
                                 uint32_t flags, uint32_t flags2)
{
    char numbers[UP_TLM_FIELDS][32];
    uint32_t vLow = v12;
    uint32_t vHigh = (v24 > v12) ? (v24 - v12) : 0u;
    uint32_t values[UP_TLM_FIELDS];
    std::string body = "{\"on\":1,\"age\":40,\"seq\":" + std::to_string(seq) + ",\"fl\":" + std::to_string(flags) +
                       ",\"fl2\":" + std::to_string(flags2) +
                       ",\"n\":1234,\"q\":0,\"q2\":0,\"q3\":0,\"q4\":0,\"ka\":120,\"vm\":0,\"ce\":0,\"t\":[";

    memset(values, 0, sizeof(values));
    values[UP_TLM_RAW1] = 310u;
    values[UP_TLM_MA1_IEST] = lowCurrent;
    values[UP_TLM_STATE1] = state1;
    values[UP_TLM_STATE2] = state2;
    values[UP_TLM_VIN] = vIn;
    values[UP_TLM_V24] = v24;
    values[UP_TLM_V12] = v12;
    values[UP_TLM_VLOW] = vLow;
    values[UP_TLM_VHIGH] = vHigh;
    values[UP_TLM_VDDA] = 3300u;
    values[UP_TLM_IMB_MV] = (vHigh > vLow) ? (vHigh - vLow) : (vLow - vHigh);

    for (uint32_t i = 0; i < UP_TLM_FIELDS; i++)
    {
        snprintf(numbers[i], sizeof(numbers[i]), "%lu", (unsigned long)values[i]);
        if (i) { body += ","; }
        body += numbers[i];
    }
    body += "],\"p\":[";

    for (uint32_t id = 0; id < UP_PARAM_COUNT; id++)
    {
        if (id) { body += ","; }
        if (id == UP_PARAM_PCT_VMIN) { body += "21000"; }
        else if (id == UP_PARAM_PCT_VMAX) { body += "29000"; }
        else if (id == UP_PARAM_CHG1_ENABLE) { body += "1"; }
        else if (id == UP_PARAM_CHG2_ENABLE) { body += "1"; }
        else { body += "null"; }
    }
    body += "]}";
    return body;
}

static std::string httpOk(const std::string &body)
{
    char header[160];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n\r\n",
             (unsigned long)body.size());
    return std::string(header) + body;
}

/* [EN] Feed one answer to the link and let its state machine consume it. */
static void feedLink(uint8_t job, const std::string &response)
{
    WiFi.currentStatus = WL_CONNECTED;
    WiFiClient::G_NextResponse = response;
    WiFiClient::G_LastRequest.clear();
    if (!func__UpLink_StartJob(job))
    {
        check(false, "link job starts", "StartJob returned false");
        return;
    }
    for (int i = 0; i < 200; i++)
    {
        func__UpLink_Loop();
    }
}

static bool callRoute(const char *path, int method = HTTP_GET)
{
    return UP_WEBSERVER_T__G__Server.call(path, method);
}

static void useCookie(void)
{
    UP_WEBSERVER_T__G__Server.setHeader("Cookie", sessionCookie.c_str());
}

static std::string jsonBody(void)
{
    return UP_WEBSERVER_T__G__Server.lastBody;
}

static bool bodyHas(const std::string &needle)
{
    return UP_WEBSERVER_T__G__Server.lastBody.find(needle) != std::string::npos;
}

/* [EN] How many day rows the heat map carries: every row opens with '['. */
static uint32_t countHeatRows(const std::string &stats)
{
    size_t start = stats.find("\"heat\":[");
    uint32_t rows = 0u;
    uint32_t depth = 0u;
    bool bool__inside = false;

    if (start == std::string::npos)
    {
        return 0u;
    }

    for (size_t i = start + 7u; i < stats.size(); i++)   /* at the array's own bracket */
    {
        if (stats[i] == '[')
        {
            depth++;
            if (depth == 2u)
            {
                rows++;
            }
            bool__inside = true;
        }
        else if ((stats[i] == ']') && bool__inside)
        {
            depth--;
            if (depth == 0u)
            {
                break;
            }
        }
    }
    return rows;
}

static uint32_t countCommas(const std::string &text)
{
    uint32_t commas = 0u;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == ',') { commas++; }
    }
    return commas;
}

/* [EN] Log in over the real route and keep the cookie it set. */
static bool loginAs(const char *user, const char *password)
{
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.clearResponse();
    UP_WEBSERVER_T__G__Server.setArg("u", user);
    UP_WEBSERVER_T__G__Server.setArg("p", password);
    UP_WEBSERVER_T__G__Server.call("/api/login", HTTP_POST);

    if (!bodyHas("\"ok\":1"))
    {
        return false;
    }
    std::string header = UP_WEBSERVER_T__G__Server.sentHeaders["Set-Cookie"];
    size_t end = header.find(';');
    sessionCookie = header.substr(0u, end);
    return sessionCookie.size() > 8u;
}

/* [EN] A frame's sequence number must climb, exactly as the board's does: the
   boot detector compares it with the previous one, and a test that reuses one
   number tests nothing about that path.
   [FA] شمارهٔ ترتیب فریم باید بالا برود، همان‌طور که روی برد بالا می‌رود: آشکارساز
   ری‌استارت آن را با قبلی مقایسه می‌کند و تستی که یک شماره را تکرار کند،
   چیزی از آن مسیر را نمی‌سنجد. */
static uint32_t UINT32_T__G__TestSeq = 100u;

static std::string nextFrame(uint32_t vIn, uint32_t v24, uint32_t v12, uint32_t current,
                            uint32_t state1, uint32_t state2, uint32_t flags, uint32_t flags2)
{
    UINT32_T__G__TestSeq++;
    return telemetryBody(UINT32_T__G__TestSeq, vIn, v24, v12, current, state1, state2, flags, flags2);
}

static void pushFrame(const std::string &body)
{
    feedLink(UP_LINK_JOB_TELEMETRY, httpOk(body));
}

/* [EN] One simulated second: the clock moves, then the panel's own accounting
   runs. Never more than UP_LINK_TIMEOUT_MS in one go, or the panel would
   correctly decide the data is stale and count nothing - which is itself worth
   knowing, but not what a test of the counters is about.
   [FA] یک ثانیهٔ شبیه‌سازی‌شده: ساعت جلو می‌رود و بعد حسابداری خود پنل اجرا
   می‌شود. هرگز بیشتر از UP_LINK_TIMEOUT_MS یک‌جا، وگرنه پنل درست تصمیم می‌گیرد
   که داده کهنه است و چیزی نمی‌شمارد - این خودش ارزش دانستن دارد، ولی موضوع
   تست شمارنده‌ها نیست. */
static void runSecond(uint32_t count)
{
    for (uint32_t i = 0u; i < count; i++)
    {
        stub_millis_ref() += 1000u;
        func__UpHistory_Tick((uint32_t)millis());
        (void)func__UpHistory_SampleIfDue((uint32_t)millis());
    }
}

/* ==================== the tests / تست‌ها ==================== */
static void testRoutesRegistered(void)
{
    std::cout << "\n-- routes / مسیرها\n";
    check(callRoute("/", HTTP_GET), "GET /");
    check(callRoute("/app.css"), "GET /app.css");
    check(callRoute("/app.js"), "GET /app.js");
    check(callRoute("/f.css"), "GET /f.css");
    check(callRoute("/version"), "GET /version");
    check(callRoute("/api/live"), "GET /api/live");
    check(callRoute("/api/series"), "GET /api/series");
    check(callRoute("/api/events"), "GET /api/events");
    check(callRoute("/api/stats"), "GET /api/stats");
    check(callRoute("/api/login", HTTP_POST), "POST /api/login");
    check(callRoute("/api/admin/users"), "GET /api/admin/users");
    check(callRoute("/api/admin/user", HTTP_POST), "POST /api/admin/user");
    check(callRoute("/api/admin/action", HTTP_POST), "POST /api/admin/action");
    check(callRoute("/api/export"), "GET /api/export");
    /* [EN] There is no route that writes an arbitrary parameter - the safety
       property the user asked for, expressed as a test.
       [FA] هیچ مسیری پارامتر دلخواه نمی‌نویسد - همان ویژگی امنیتی که کاربر
       خواست، به‌صورت یک تست. */
    check(!callRoute("/api/set", HTTP_POST), "no arbitrary parameter route / مسیر پارامتر دلخواه وجود ندارد");
}

static void testPageAndVersion(void)
{
    std::cout << "\n-- page and version / صفحه و نسخه\n";
    callRoute("/");
    check(UP_WEBSERVER_T__G__Server.lastCode == 200, "page answers 200");
    check(UP_WEBSERVER_T__G__Server.lastBody.find("ChangeOver") != std::string::npos, "page mentions ChangeOver");
    check(UP_WEBSERVER_T__G__Server.sentHeaders.count("Cache-Control") > 0u, "page is never cached");

    callRoute("/version");
    check(bodyHas("\"name\":\"user panel\""), "version names the panel");
    check(bodyHas(UP_BUILD_STAMP), "version carries the build stamp");
    check(bodyHas("\"source\":\"192.168.4.1\""), "version says which board it reads");
}

static void testLoginAndPassword(void)
{
    std::cout << "\n-- login and password / ورود و گذرواژه\n";
    UP_WEBSERVER_T__G__Server.clearHeaders();

    callRoute("/api/live");
    check(UP_WEBSERVER_T__G__Server.lastCode == 401, "live without a cookie is 401");

    check(!loginAs("admin", "definitely-wrong"), "wrong password is refused");
    check(bodyHas("\"ok\":0"), "refusal has ok:0");
    check(loginAs("admin", "admin"), "seeded admin logs in");
    check(bodyHas("\"role\":\"admin\""), "admin role in the reply");
    check(bodyHas("\"must\":1"), "first login is forced to change the password");
    useCookie();
    callRoute("/api/me");
    check(bodyHas("\"user\":\"admin\""), "me returns the logged-in name");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("p0", "wrong-old");
    UP_WEBSERVER_T__G__Server.setArg("p1", "newsecret1");
    callRoute("/api/pass", HTTP_POST);
    check(bodyHas("\"ok\":0"), "password change needs the correct old password");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("p0", "admin");
    UP_WEBSERVER_T__G__Server.setArg("p1", "short");
    callRoute("/api/pass", HTTP_POST);
    check(bodyHas("\"ok\":0"), "password change refuses a short password");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("p0", "admin");
    UP_WEBSERVER_T__G__Server.setArg("p1", "newsecret1");
    callRoute("/api/pass", HTTP_POST);
    check(bodyHas("\"ok\":1"), "password change accepted");

    useCookie();
    callRoute("/api/me");
    check(bodyHas("\"must\":0"), "the forced-change flag is cleared");

    check(!loginAs("admin", "admin"), "the old password no longer works");
    check(loginAs("admin", "newsecret1"), "the new password works");
    useCookie();
}

static void testTelemetryAndHistory(void)
{
    std::cout << "\n-- telemetry and history / تلمتری و تاریخچه\n";

    /* [EN] First frame after boot is a BASELINE, not a change: the panel adopts
            the world as it finds it, and only then starts comparing.
       [FA] اولین فریم پس از بوت، خط پایه است نه تغییر: پنل دنیا را همان‌طور که
            می‌بیند می‌پذیرد و تازه بعد شروع به مقایسه می‌کند. */
    pushFrame(nextFrame(24160u, 25800u, 12900u, 0u, UP_ST_OFF, UP_ST_OFF, 0x02u, 0u));
    runSecond(1u);

    check(UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.bool__seen, "telemetry frame parsed");
    check(UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.uint32_t__frames == 1u, "frame counter is 1");
    check(func__UpState_TlmWord(UP_TLM_VIN) == 24160u, "/t words landed in order");
    check(func__UpState_InputPresent() == 1u, "input present flag decoded");
    check(UPPANEL_STATE_T__G__State.up_telemetry_t__tlm.bool__paramKnown[UP_PARAM_PCT_VMIN], "parameter 74 parsed");
    check(func__UpState_BatteryPercent(25000u) == 50u, "battery percent uses the board's 74/75 mapping",
          std::to_string(func__UpState_BatteryPercent(25000u)));
    check(func__UpStore_SampleCount() >= 1u, "the one-second sample is stored");

    /* A charge: eight seconds in BULK, then FLOAT. */
    uint32_t eventsBefore = func__UpStore_EventCount();

    pushFrame(nextFrame(24160u, 25800u, 12900u, 1400u, UP_ST_BULK, UP_ST_OFF, 0x02u, 0u));
    runSecond(1u);
    for (uint32_t i = 0u; i < 8u; i++)
    {
        runSecond(1u);
        pushFrame(nextFrame(24160u, 26000u, 13000u, 1400u, UP_ST_BULK, UP_ST_OFF, 0x02u, 0u));
    }
    pushFrame(nextFrame(24160u, 26200u, 13100u, 300u, UP_ST_FLOAT, UP_ST_OFF, 0x02u, 0u));
    runSecond(1u);

    check(func__UpStore_EventCount() > eventsBefore, "the charge wrote events",
          std::to_string(func__UpStore_EventCount()) + " vs " + std::to_string(eventsBefore));
    check(func__UpStore_Totals()->uint32_t__charges == 1u, "one completed charge counted",
          std::to_string(func__UpStore_Totals()->uint32_t__charges));
    check(func__UpStore_DailyToday()->uint16_t__charges == 1u, "today's counter agrees");
    check(func__UpStore_Totals()->uint32_t__sumChargeS >= 5u, "the charge duration was measured",
          std::to_string(func__UpStore_Totals()->uint32_t__sumChargeS));
    check(func__UpStore_Totals()->uint32_t__chargeWh100 > 0u, "energy was accumulated, not just counted",
          std::to_string(func__UpStore_Totals()->uint32_t__chargeWh100));

    /* [EN] A charge that lasts under a second is contact noise, not a charge.
       [FA] شارژی که زیر یک ثانیه طول بکشد نویز تماس است، نه شارژ. */
    uint32_t chargesBefore = func__UpStore_Totals()->uint32_t__charges;
    pushFrame(nextFrame(24160u, 26200u, 13100u, 900u, UP_ST_ABSORB, UP_ST_OFF, 0x02u, 0u));
    pushFrame(nextFrame(24160u, 26200u, 13100u, 300u, UP_ST_FLOAT, UP_ST_OFF, 0x02u, 0u));
    runSecond(1u);
    check(func__UpStore_Totals()->uint32_t__charges == chargesBefore, "a blink of a charge is dropped as noise");

    /* The input drops, the machine runs on the battery, the input comes back. */
    uint32_t outagesBefore = func__UpStore_Totals()->uint32_t__outages;
    pushFrame(nextFrame(0u, 25500u, 12800u, 0u, UP_ST_OFF, UP_ST_OFF, 0x00u, 0u));
    runSecond(1u);
    check(func__UpStore_Totals()->uint32_t__outages == (outagesBefore + 1u), "input loss counted once",
          std::to_string(func__UpStore_Totals()->uint32_t__outages));
    check(func__UpStore_DailyToday()->uint16_t__outages == 1u, "today's outage counter too");

    runSecond(2u);
    check(func__UpStore_Totals()->uint32_t__runS >= 1u, "time on the battery accumulated",
          std::to_string(func__UpStore_Totals()->uint32_t__runS));

    pushFrame(nextFrame(24100u, 25500u, 12800u, 0u, UP_ST_OFF, UP_ST_OFF, 0x02u, 0u));
    runSecond(1u);
    check(func__UpStore_Totals()->uint32_t__inputS >= 1u, "time back on the input accumulated",
          std::to_string(func__UpStore_Totals()->uint32_t__inputS));
    check(func__UpStore_Totals()->uint32_t__maxRunS >= 1u, "the longest run was remembered");

    /* [EN] A channel parked in FINAL_FAULT must light E09 - the one fault the
            page tells the user needs a physical board reset.
       [FA] کانالی که در FINAL_FAULT بماند باید E09 را روشن کند - همان خطایی که
            صفحه می‌گوید انسان باید با ری‌استارت برد پاکش کند. */
    pushFrame(nextFrame(24100u, 25500u, 12800u, 0u, UP_ST_FINAL_FAULT, UP_ST_OFF, 0x02u, 0u));
    runSecond(1u);
    useCookie();
    callRoute("/api/live");
    check(bodyHas("\"E09\""), "FINAL_FAULT is reported as E09");
    check(bodyHas("\"faultBits\":["), "fault bits are in the live reply");
    check(bodyHas("\"faultCodes\":["), "the fault-code list is in the live reply");

    /* [EN] And a real fault bit from the board's own mask lights its code too.
       [FA] و یک بیت خطای واقعی از ماسک خود برد هم کد خودش را روشن می‌کند. */
    pushFrame(nextFrame(24100u, 25500u, 12800u, 0u, UP_ST_OFF, UP_ST_OFF, 0x02u, UP_IMB_LATCHED_BIT));
    runSecond(1u);
    useCookie();
    callRoute("/api/live");
    check(bodyHas("\"E08\""), "an imbalance flag is reported as E08");
    check(bodyHas("\"latched\":1"), "the latched imbalance bit reaches the page");

    /* [EN] The page's own storage card and diagnostics need the storage block,
            and the live view must always carry it even with no history at all.
       [FA] کارت حافظه و بخش دیاگ صفحه به بلوک حافظه نیاز دارند و نمای زنده باید
            آن را همیشه داشته باشد، حتی با تاریخچهٔ صفر. */
    check(bodyHas("\"usedBytes\":"), "storage use is in the live reply");
    check(bodyHas("\"days\":"), "storage coverage is in the live reply");
}

static void testApiShapes(void)
{
    std::cout << "\n-- API shapes / شکل پاسخ‌ها\n";
    useCookie();

    callRoute("/api/live");
    std::string live = jsonBody();
    size_t tStart = live.find("\"t\":[");
    size_t tEnd = live.find(']', tStart);
    check(countCommas(live.substr(tStart, tEnd - tStart)) == (UP_TLM_FIELDS - 1u), "/api/live carries 28 telemetry words");
    check(live.find("\"flags\"") != std::string::npos, "flags block present");
    check(live.find("\"store\"") != std::string::npos, "storage block present");
    check(live.find("\"imb\"") != std::string::npos, "imbalance block present");
    check(live.find("\"tot\"") != std::string::npos, "lifetime totals present");

    callRoute("/api/series");
    std::string series = jsonBody();
    check(series.find("\"n\":120") != std::string::npos, "/api/series decimates to the requested point count");
    size_t v24Start = series.find("\"v24\":[") + 7u;
    size_t v24End = series.find(']', v24Start);
    check(countCommas(series.substr(v24Start, v24End - v24Start)) == (UP_SERIES_POINTS - 1u), "v24 has one value per point");
    check(series.find("\"ageMin\":[") != std::string::npos, "series carries numeric ages");
    check(series.find("\"label\":[") != std::string::npos, "series carries axis labels");

    callRoute("/api/events");
    std::string events = jsonBody();
    check(events.find("\"e\":[") != std::string::npos, "events list present under the key the page reads");
    check(events.find("\"code\":") != std::string::npos, "events carry a code");
    check(events.find("\"dur\":") != std::string::npos, "events carry a duration");

    /* [EN] A daily row only reaches flash on the periodic flush, so a panel
            that has just booted honestly reports no history yet. Move the clock
            past that interval first - the same thing a panel that has been
            running for two minutes does on its own.
       [FA] ردیف روزانه فقط با ذخیرهٔ دوره‌ای به فلش می‌رسد، پس پنلی که تازه بالا
            آمده صادقانه می‌گوید هنوز تاریخچه‌ای نیست. اول ساعت را از آن بازه
            بگذرانید - همان کاری که پنلی که دو دقیقه روشن بوده خودش می‌کند. */
    stub_millis_ref() += UP_TOTALS_FLUSH_MS + 1000u;
    (void)func__UpStore_FlushIfDue();
    up_daily_t probe;
    check(func__UpStore_DailyRead(func__UpState_DayIndex(), &probe), "the daily row reached flash");
    check(probe.uint16_t__charges == 1u, "and it carries today's charge count");

    callRoute("/api/stats");
    std::string stats = jsonBody();
    check(stats.find("\"kpi\"") != std::string::npos, "stats has a KPI block");
    check(stats.find("\"sessions\":[") != std::string::npos, "stats has sessions");
    check(stats.find("\"daily\":[") != std::string::npos, "stats has daily rows");
    check(stats.find("\"heatDays\":[") != std::string::npos, "stats has heat-map days");
    check(stats.find("\"heat\":[") != std::string::npos, "stats has the heat map");

    check(countHeatRows(stats) >= 1u, "heat map has at least one day row",
          std::to_string(countHeatRows(stats)));
    check(countHeatRows(stats) <= UP_STAT_HEAT_ROWS, "heat map is bounded to a week of rows");
    check(stats.find("\"clock\":") != std::string::npos, "stats says whether it is dated at all");
}

static void testRolesAndActions(void)
{
    std::cout << "\n-- roles and the only write / نقش‌ها و تنها نوشتن\n";
    useCookie();

    /* add a viewer and an operator through the real admin route */
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("action", "add");
    UP_WEBSERVER_T__G__Server.setArg("u", "view1");
    UP_WEBSERVER_T__G__Server.setArg("p", "viewpass1");
    UP_WEBSERVER_T__G__Server.setArg("r", "viewer");
    callRoute("/api/admin/user", HTTP_POST);
    check(bodyHas("\"ok\":1"), "admin can add a viewer");

    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("action", "add");
    UP_WEBSERVER_T__G__Server.setArg("u", "op1");
    UP_WEBSERVER_T__G__Server.setArg("p", "oppass11");
    UP_WEBSERVER_T__G__Server.setArg("r", "operator");
    callRoute("/api/admin/user", HTTP_POST);
    check(bodyHas("\"ok\":1"), "admin can add an operator");

    useCookie();
    callRoute("/api/admin/users");
    check(bodyHas("[\"view1\",\"viewer\""), "the viewer is listed with its role");
    check(UP_WEBSERVER_T__G__Server.lastBody.find("viewpass1") == std::string::npos, "no password ever leaves the device");

    /* the viewer may read, and may do nothing else */
    check(loginAs("view1", "viewpass1"), "viewer logs in");
    useCookie();
    callRoute("/api/live");
    check(UP_WEBSERVER_T__G__Server.lastCode == 200, "viewer can read the live view");
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("a", "charger1_off");
    UP_WEBSERVER_T__G__Server.setArg("cs", "stamp-viewer");
    callRoute("/api/admin/action", HTTP_POST);
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "viewer cannot cut a charger");
    callRoute("/api/admin/users");
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "viewer cannot list users");
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("what", "csv");
    callRoute("/api/export");
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "viewer cannot export history");

    /* the operator may cut a charger - and the write really leaves the board */
    check(loginAs("op1", "oppass11"), "operator logs in");
    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("a", "charger1_off");
    UP_WEBSERVER_T__G__Server.setArg("cs", "stamp-op-1");
    callRoute("/api/admin/action", HTTP_POST);
    check(bodyHas("\"ok\":1"), "operator cuts charger 1");

    WiFi.currentStatus = WL_CONNECTED;
    WiFiClient::G_LastRequest.clear();
    func__UpLink_Loop();
    check(WiFiClient::G_LastRequest.find("POST /s") != std::string::npos, "the cut is sent to the board");
    check(WiFiClient::G_LastRequest.find("id=11&v=0") != std::string::npos, "and it is parameter 11 = 0");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("a", "charger1_off");
    UP_WEBSERVER_T__G__Server.setArg("cs", "stamp-op-1");
    callRoute("/api/admin/action", HTTP_POST);
    check(bodyHas("\"ok\":0"), "the same confirmation stamp cannot cut twice");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("a", "purge");
    UP_WEBSERVER_T__G__Server.setArg("cs", "stamp-op-purge");
    callRoute("/api/admin/action", HTTP_POST);
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "operator cannot purge history");

    /* the admin may, and the last-admin rules hold */
    check(loginAs("admin", "newsecret1"), "admin logs in again");
    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("action", "del");
    UP_WEBSERVER_T__G__Server.setArg("u", "admin");
    callRoute("/api/admin/user", HTTP_POST);
    check(bodyHas("cannot delete yourself"), "nobody deletes their own account");

    useCookie();
    callRoute("/api/admin/audit");
    check(bodyHas("charger1_off"), "the action log remembers the cut");
    check(bodyHas("user_add"), "and it remembers the new users");
}

static void testExportAndPurge(void)
{
    std::cout << "\n-- export and purge / برون‌بری و پاک‌سازی\n";
    useCookie();

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("what", "csv");
    callRoute("/api/export");
    check(UP_WEBSERVER_T__G__Server.lastCode == 200, "CSV export answers 200");
    check(UP_WEBSERVER_T__G__Server.sentHeaders["Content-Disposition"].find("user_panel_history.csv") != std::string::npos,
          "CSV export names its own file, not the engineering panel's");
    check(UP_WEBSERVER_T__G__Server.lastBody.find("epoch,abs_s,vin_mv") == 0u, "CSV starts with a header line");
    check(UP_WEBSERVER_T__G__Server.lastBody.find("\r\n") != std::string::npos, "CSV rows are newline separated");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("what", "json");
    callRoute("/api/export");
    check(UP_WEBSERVER_T__G__Server.lastBody.find("\"list\":[") != std::string::npos, "JSON export carries the samples");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("a", "purge");
    UP_WEBSERVER_T__G__Server.setArg("cs", "stamp-purge-1");
    callRoute("/api/admin/action", HTTP_POST);
    check(bodyHas("\"ok\":1"), "admin purges the history");
    check(func__UpStore_SampleCount() == 0u, "samples are gone after the purge");

    useCookie();
    callRoute("/api/live");
    check(bodyHas("\"purges\":1"), "the purge counter is visible to the user");
}

static void testStorageRotation(void)
{
    std::cout << "\n-- storage rotation / چرخش حافظه\n";
    /* [EN] A tiny flash: the ring must keep the NEWEST records and drop the
       oldest, which is the user's requirement expressed as a test.
       [FA] یک فلش کوچک: حلقه باید جدیدترین رکوردها را نگه دارد و قدیمی‌ترین را
       دور بریزد؛ همان خواستهٔ کاربر به‌صورت تست. */
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.clearResponse();
    UP_WEBSERVER_T__G__Server.clearHeaders();

    LittleFS.capacity = 60000u;
    bool mounted = func__UpStore_Begin();
    check(mounted, "storage comes back up on a small flash");

    uint32_t budget = UINT32_T__G__SampleCapBytes / (uint32_t)sizeof(up_sample_t);
    uint32_t wanted = budget + 50u;

    for (uint32_t i = 0u; i < wanted; i++)
    {
        up_sample_t sample;
        memset(&sample, 0, sizeof(sample));
        sample.uint32_t__absS = 1000u + i;
        sample.uint16_t__v24Mv = 25000u;
        (void)func__UpStore_AppendSample(&sample);
    }

    uint32_t count = func__UpStore_SampleCount();
    check(count > 0u, "samples are stored");
    check(count < wanted, "the ring is bounded by the flash budget",
          std::to_string(count) + " of " + std::to_string(wanted));
    check(count <= budget, "and it never exceeds its own share of the partition",
          std::to_string(count) + " vs " + std::to_string(budget));
    check(func__UpStore_PurgeCount() > 0u, "the oldest data is what gets dropped", std::to_string(func__UpStore_PurgeCount()));

    up_ring_reader_t reader;
    func__UpStore_ReaderInit(&reader);
    if (func__UpStore_ReaderOpen(&reader, UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t)))
    {
        up_sample_t first;
        (void)func__UpStore_ReaderNext(&reader, &first, (uint32_t)sizeof(up_sample_t));
        check(first.uint32_t__absS > 1000u, "the surviving oldest record is indeed younger",
              std::to_string(first.uint32_t__absS));
        func__UpStore_ReaderClose(&reader);
    }
    else
    {
        check(false, "the ring reopens after rotation");
    }

    LittleFS.capacity = 262144u;
    (void)func__UpStore_Begin();
}

static void testCryptoAndClock(void)
{
    std::cout << "\n-- hashing and clock / چکیده و ساعت\n";
    uint8_t digest[UP_SHA256_DIGEST_BYTES];
    const char *abc = "abc";

    func__UpSha256_Buffer((const uint8_t *)abc, 3u, digest);
    const uint8_t expected[UP_SHA256_DIGEST_BYTES] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    check(memcmp(digest, expected, UP_SHA256_DIGEST_BYTES) == 0, "SHA-256 matches the published vector for \"abc\"");

    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "990000000");
    callRoute("/api/clock", HTTP_POST);
    check(bodyHas("implausible"), "an impossible clock is refused");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1780000000");
    callRoute("/api/clock", HTTP_POST);
    check(bodyHas("\"ok\":1"), "a plausible clock is accepted");

    callRoute("/api/live");
    check(bodyHas("\"wallValid\":1"), "the live view says the clock is set");
    check(func__UpState_NowEpochS() >= 1780000000u, "epoch counting starts from what the browser said");

    callRoute("/api/live");
    check(bodyHas("\"wallValid\":1"), "the clock survives the next poll");
}


/* ---------------- the calendar / تقویم ------------------------------- */

/* [EN] Dated anchors, supplied by an INDEPENDENT library (jdatetime) rather than
   by the code under test: if the panel's calendar and these ten rows agree, the
   panel agrees with the world. The first three also caught two real bugs -
   every Jalali date three days out, and January printed as March.
   [FA] لنگرهای تاریخی، از یک کتابخانهٔ «مستقل» (jdatetime) و نه از خود کدِ تحت
   تست: اگر تقویم پنل و این ده ردیف توافق داشته باشند، پنل با دنیا توافق دارد.
   سه ردیف اول دو خطای واقعی را هم گرفتند - هر تاریخ شمسی سه روز جابه‌جا، و
   ژانویه که مارس چاپ می‌شد. */
static void testCalendar(void)
{
    /* [EN] Dated anchors from the published calendar, not from this code: two
       Iranian dates everyone knows (22 Bahman 1357, 1 Farvardin 1400), every
       Nowruz from 1400 to 1405, the panel's own clock ceiling, the 32-bit time
       limit and the four dates past it, two Jalali leap years that only exist as
       30 Esfand, and 1 January 2100. The far-future group is the one that earns
       its keep: an epoch past 2038 used to overflow the signed addition of the
       timezone offset and print 1902 - undefined behaviour that no assertion
       about "today" would ever have caught.
       [FA] لنگرهای تاریخی از تقویم منتشرشده، نه از خود این کد: دو تاریخ شناختهٔ
       ایرانی (۲۲ بهمن ۱۳۵۷، ۱ فروردین ۱۴۰۰)، هر نوروز از ۱۴۰۰ تا ۱۴۰۵، سقف
       ساعتِ خود پنل، مرز زمان ۳۲بیتی و چهار تاریخ بعد از آن، دو سال کبیسهٔ شمسی
       که فقط به‌صورت ۳۰ اسفند وجود دارند، و ۱ ژانویه ۲۰۰۰... ۲۱۰۰. گروه آیندهٔ
       دور همان است که خودش را ثابت می‌کند: ثانیه‌ای بعد از ۲۰۳۸، جمع علامت‌دار
       اختلاف زمانی را سرریز می‌کرد و ۱۹۰۲ چاپ می‌شد - رفتار تعریف‌نشده‌ای که هیچ
       ادعایی دربارهٔ «امروز» هرگز نمی‌گرفت. */
    static const uint32_t UINT32_T__A__Epoch[] =
    {
        287539200u,  946684800u,  1234567890u, 1600000000u, 1616284800u,
        1647820800u, 1679356800u, 1710892800u, 1742515200u, 1767225600u,
        1774051200u, 1791331200u, 2000000000u, 2147483647u, 2153865600u,
        2220912000u, 4102444800u
    };
    static const char *const CHAR__A__Jalali[] =
    {
        "1357/11/22", "1378/10/11", "1387/11/25", "1399/06/23", "1400/01/01",
        "1401/01/01", "1402/01/01", "1403/01/01", "1404/01/01", "1404/10/11",
        "1405/01/01", "1405/07/15", "1412/02/29", "1416/10/30", "1417/01/14",
        "1419/02/29", "1478/10/12"
    };
    static const char *const CHAR__A__Gregorian[] =
    {
        "1979-02-11", "2000-01-01", "2009-02-13", "2020-09-13", "2021-03-21",
        "2022-03-21", "2023-03-21", "2024-03-20", "2025-03-21", "2026-01-01",
        "2026-03-21", "2026-10-07", "2033-05-18", "2038-01-19", "2038-04-03",
        "2040-05-18", "2100-01-01"
    };
    const uint32_t uint32_t__anchors = (uint32_t)(sizeof(UINT32_T__A__Epoch) / sizeof(UINT32_T__A__Epoch[0]));
    char text[UP_REPORT_TEXT_MAX];

    std::cout << "\n-- calendar / تقویم\n";

    for (uint32_t i = 0u; i < uint32_t__anchors; i++)
    {
        (void)func__UpCal_DateText(UINT32_T__A__Epoch[i], 0, text);
        check(strcmp(text, CHAR__A__Jalali[i]) == 0,
              std::string("Jalali of ") + CHAR__A__Gregorian[i] + " is " + CHAR__A__Jalali[i], text);
    }
    for (uint32_t i = 0u; i < uint32_t__anchors; i++)
    {
        func__UpCal_GregorianText(UINT32_T__A__Epoch[i], 0, text);
        check(strcmp(text, CHAR__A__Gregorian[i]) == 0,
              std::string("Gregorian of epoch ") + std::to_string(UINT32_T__A__Epoch[i]), text);
    }

    /* [EN] The two far enders again, this time in Tehran's offset: the same
       additions that overflowed as signed ints must stay right with an offset
       added, and an offset must still not change the day when it cannot.
       [FA] دو سر دور دست، این بار با اختلاف تهران: همان جمعی که در حساب
       علامت‌دار سرریز می‌کرد باید با اختلاف هم درست بماند، و اختلاف نباید روزی
       را که عوض نمی‌کند عوض کند. */
    (void)func__UpCal_DateText(2153865600u, 12600, text);
    check(strcmp(text, "1417/01/14") == 0, "2038 with Tehran's offset is still its own Jalali day", text);
    (void)func__UpCal_DateText(4102444800u, 12600, text);
    check(strcmp(text, "1478/10/12") == 0, "and so is 1 January 2100", text);
    func__UpCal_TimeText(2153865600u, 12600, text);
    check(strcmp(text, "03:30:00") == 0, "the far date keeps the clock too", text);

    /* [EN] Epoch zero is the panel's "no clock has ever been set" - a dash, not
       1 January 1970. A report that printed 1970 would be lying about a board
       that has never been told the time.
       [FA] ثانیهٔ صفر یعنی «هیچ ساعتی تنظیم نشده» - خط تیره، نه ۱ ژانویهٔ ۱۹۷۰.
       گزارشی که ۱۹۷۰ چاپ کند دربارهٔ بردی که هرگز ساعت نگرفته دروغ گفته است. */
    (void)func__UpCal_DateText(0u, 12600, text);
    check(strcmp(text, "-") == 0, "an unset clock prints a dash, not 1970", text);
    func__UpCal_GregorianText(0u, 12600, text);
    check(strcmp(text, "-") == 0, "and so does the Gregorian side", text);

    /* [EN] The day boundary is the OWNER's midnight: 21:00 UTC is already the
       next Jalali day in Tehran, and 23:00 UTC the day before is not.
       [FA] مرز روز، نیمه‌شب «صاحب دستگاه» است: ساعت ۲۱:۰۰ UTC در تهران روز شمسی
       بعدی است و ۲۳:۰۰ UTC روز قبل، روز قبل می‌ماند. */
    (void)func__UpCal_DateText(1791331200u + 75600u, 12600, text);
    check(strcmp(text, "1405/07/16") == 0, "21:00 UTC is already tomorrow in Tehran", text);
    (void)func__UpCal_DateText(1791331200u - 3600u, 12600, text);
    check(strcmp(text, "1405/07/15") == 0, "23:00 UTC yesterday is still today in Tehran", text);

    func__UpCal_TimeText(1791331200u, 12600, text);
    check(strcmp(text, "03:30:00") == 0, "the clock carries the offset", text);
    func__UpCal_TimeText(1791331200u, 0, text);
    check(strcmp(text, "00:00:00") == 0, "and no offset means UTC", text);

    func__UpCal_DurationText(3720u, text);
    check(strcmp(text, "1h 2m") == 0, "durations read the way a person says them", text);

    /* [EN] The offsets the panel accepts, and the two it must refuse.
       [FA] اختلاف‌هایی که پنل می‌پذیرد و دو تایی که باید رد کند. */
    check(func__UpState_TzIsValid(12600), "Tehran's +3:30 is a legal offset");
    check(func__UpState_TzIsValid(-43200), "the lower bound is legal");
    check(func__UpState_TzIsValid(50400), "the upper bound is legal");
    check(!func__UpState_TzIsValid(50460), "a minute past the upper bound is not");
    check(!func__UpState_TzIsValid(-43260), "and neither is a minute below the lower bound");
}

/* ---------------- the clock, admin only / ساعت، فقط مدیر ------------- */

static void testAdminClock(void)
{
    std::cout << "\n-- admin clock / ساعت مدیر\n";

    /* [EN] An operator may cut a charger, and may not touch the clock: the time
       decides which day a charge belongs to, and that is an admin's decision.
       [FA] اپراتور می‌تواند شارژر را قطع کند و نمی‌تواند به ساعت دست بزند: زمان
       تعیین می‌کند شارژ مال کدام روز است و این تصمیم مدیر است. */
    check(loginAs("op1", "oppass11"), "operator logs in again");
    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1780000000");
    UP_WEBSERVER_T__G__Server.setArg("tz", "210");
    callRoute("/api/clock", HTTP_POST);
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "an operator cannot set the panel clock");
    callRoute("/api/admin/clock");
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "an operator cannot read the admin clock card");

    check(loginAs("admin", "newsecret1"), "admin logs in again");
    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1791331200");
    UP_WEBSERVER_T__G__Server.setArg("tz", "210");
    callRoute("/api/clock", HTTP_POST);
    check(bodyHas("\"ok\":1"), "the admin sets date, time and offset in one write");
    check(bodyHas("\"date\":\"1405/07/15\""), "the reply reads back a Jalali date",
          UP_WEBSERVER_T__G__Server.lastBody);
    check(bodyHas("\"gregorian\":\"2026-10-07\""), "and the same day in Gregorian");
    check(func__UpState_NowEpochS() == 1791331200u, "the epoch is what the admin typed");

    callRoute("/api/admin/clock");
    check(bodyHas("\"set\":1"), "the admin card sees the clock as set");
    check(bodyHas("\"tzMin\":210"), "and reports the offset in minutes");
    check(bodyHas("\"date\":\"1405/07/15\""), "with the date the panel will stamp on new rows");
    check(bodyHas("\"time\":\"03:30:00\""), "and the local time that offset implies",
          UP_WEBSERVER_T__G__Server.lastBody);

    /* [EN] A second write without an offset keeps the offset already chosen:
       fixing the minute must not quietly move the panel to another country.
       [FA] نوشتن دوم بدون اختلاف، همان اختلاف انتخاب‌شده را نگه می‌دارد: اصلاح
       دقیقه نباید بی‌صدا پنل را به کشور دیگری ببرد. */
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1791331260");
    callRoute("/api/clock", HTTP_POST);
    check(func__UpState_TzOffsetS() == 12600, "an absent offset keeps the panel's own +3:30");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1791331260");
    UP_WEBSERVER_T__G__Server.setArg("tz", "180");
    callRoute("/api/clock", HTTP_POST);
    check(func__UpState_TzOffsetS() == 10800, "another country's offset is accepted");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1791331260");
    UP_WEBSERVER_T__G__Server.setArg("tz", "99999");
    callRoute("/api/clock", HTTP_POST);
    check(bodyHas("bad tz"), "an impossible offset is refused instead of stored");
    check(func__UpState_TzOffsetS() == 10800, "and the refused offset left the old one alone");

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("t", "1791331200");
    UP_WEBSERVER_T__G__Server.setArg("tz", "210");
    callRoute("/api/clock", HTTP_POST);
    check(func__UpState_TzOffsetS() == 12600, "Tehran is put back before the report tests");

    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("limit", "6");
    callRoute("/api/admin/audit");
    check(bodyHas("clock_set"), "every clock change is in the action log",
          UP_WEBSERVER_T__G__Server.lastBody);
}


/* ---------------- an independent zip reader / خوانندهٔ مستقل زیپ --------- */

/* [EN] A SECOND implementation of the ZIP reader and of CRC-32, written for the
   test only and deliberately different from the one inside up_xlsx.h (a table
   driven CRC and a central-directory walk, where the writer uses a bitwise CRC
   and local headers). Two implementations that agree are evidence; one
   implementation checking itself is decoration.
   [FA] پیاده‌سازی «دوم» خوانندهٔ زیپ و CRC-32، فقط برای تست و عمداً متفاوت با
   آن‌که داخل up_xlsx.h است (CRC جدولی و پیمایش دایرکتوری مرکزی، در حالی که
   نویسنده CRC بیتی و سرصفحهٔ محلی دارد). دو پیاده‌سازی که توافق کنند «شاهد»
   هستند؛ یک پیاده‌سازی که خودش را بررسی کند «تزئین» است. */
struct TestZipMember
{
    std::string name;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
};

static uint16_t testLe16(const std::string &data, size_t at)
{
    return (uint16_t)((uint16_t)(uint8_t)data[at] | ((uint16_t)(uint8_t)data[at + 1] << 8));
}

static uint32_t testLe32(const std::string &data, size_t at)
{
    return (uint32_t)(uint8_t)data[at] |
           ((uint32_t)(uint8_t)data[at + 1] << 8) |
           ((uint32_t)(uint8_t)data[at + 2] << 16) |
           ((uint32_t)(uint8_t)data[at + 3] << 24);
}

static uint32_t testCrc32(const std::string &data, size_t offset, size_t length)
{
    uint32_t table[256];

    for (uint32_t i = 0u; i < 256u; i++)
    {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
        {
            c = ((c & 1u) != 0u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++)
    {
        crc = table[(crc ^ (uint8_t)data[offset + i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

static bool testZipRead(const std::string &zip, std::vector<TestZipMember> &members, std::string &why)
{
    if (zip.size() < 22u)
    {
        why = "the file is too short to be a zip";
        return false;
    }

    size_t eocd = std::string::npos;
    for (size_t at = zip.size() - 22u;; at--)
    {
        if (testLe32(zip, at) == 0x06054B50u) { eocd = at; break; }
        if (at == 0u) { break; }
    }
    if (eocd == std::string::npos)
    {
        why = "no end-of-central-directory record";
        return false;
    }

    uint16_t count = testLe16(zip, eocd + 10u);
    uint32_t centralSize = testLe32(zip, eocd + 12u);
    uint32_t centralOffset = testLe32(zip, eocd + 16u);

    if ((size_t)centralOffset + (size_t)centralSize + 22u != zip.size())
    {
        why = "the central directory does not end where the file ends";
        return false;
    }

    size_t at = centralOffset;
    for (uint16_t i = 0u; i < count; i++)
    {
        TestZipMember member;

        if (testLe32(zip, at) != 0x02014B50u)
        {
            why = "a central directory entry has the wrong signature";
            return false;
        }
        member.crc = testLe32(zip, at + 16u);
        member.size = testLe32(zip, at + 24u);
        member.offset = testLe32(zip, at + 42u);
        uint16_t nameLen = testLe16(zip, at + 28u);
        uint16_t extraLen = testLe16(zip, at + 30u);
        uint16_t commentLen = testLe16(zip, at + 32u);
        member.name = zip.substr(at + 46u, nameLen);

        if (testLe32(zip, member.offset) != 0x04034B50u)
        {
            why = "member " + member.name + " does not start with a local header";
            return false;
        }
        uint16_t localNameLen = testLe16(zip, member.offset + 26u);
        uint16_t localExtraLen = testLe16(zip, member.offset + 28u);
        if (zip.substr(member.offset + 30u, localNameLen) != member.name)
        {
            why = "member " + member.name + " is named differently in its local header";
            return false;
        }
        size_t dataAt = member.offset + 30u + localNameLen + localExtraLen;
        if (dataAt + member.size + 16u > zip.size())
        {
            why = "member " + member.name + " claims more data than the file holds";
            return false;
        }
        if (testCrc32(zip, dataAt, member.size) != member.crc)
        {
            why = "member " + member.name + " fails its own CRC";
            return false;
        }
        /* [EN] the data descriptor that follows the data must repeat the same
           CRC and sizes - that is the whole contract of a streaming zip.
           [FA] توصیفگر داده‌ای که بعد از داده می‌آید باید همان CRC و اندازه‌ها را
           تکرار کند - تمام قرارداد یک زیپ جریانی همین است. */
        if ((testLe32(zip, dataAt + member.size) != 0x08074B50u) ||
            (testLe32(zip, dataAt + member.size + 4u) != member.crc) ||
            (testLe32(zip, dataAt + member.size + 8u) != member.size))
        {
            why = "member " + member.name + " has a data descriptor that disagrees with it";
            return false;
        }

        members.push_back(member);
        at += 46u + nameLen + extraLen + commentLen;
    }

    if (at != (size_t)centralOffset + centralSize)
    {
        why = "the central directory walk ended early";
        return false;
    }
    return true;
}

static bool zipHas(const std::vector<TestZipMember> &members, const std::string &name)
{
    for (size_t i = 0; i < members.size(); i++)
    {
        if (members[i].name == name) { return true; }
    }
    return false;
}

static std::string zipRead(const std::string &zip, const std::vector<TestZipMember> &members, const std::string &name)
{
    for (size_t i = 0; i < members.size(); i++)
    {
        if (members[i].name == name)
        {
            uint16_t nameLen = testLe16(zip, members[i].offset + 26u);
            uint16_t extraLen = testLe16(zip, members[i].offset + 28u);
            return zip.substr(members[i].offset + 30u + nameLen + extraLen, members[i].size);
        }
    }
    return std::string();
}

/* ---------------- the Excel report / گزارش اکسل ---------------------- */

/* ==================== Aged sample helper / کمکی نمونهٔ کهنه ==================== */
/**
 * [EN] Append one sample whose monotonic stamp is "now minus age", so the
 *      report tests can place a record exactly on either side of a range edge.
 *      The remaining fields are filled with the caller's numbers and quiet
 *      defaults, because what the report tests read is the time column and the
 *      pack voltage.
 * [FA] افزودن یک نمونه که مهر یکنوایش «حالا منهای عمر» است، تا تست‌های گزارش
 *      بتوانند رکوردی را دقیقاً دو طرف یک مرز بازه بگذارند. بقیهٔ میدان‌ها با
 *      اعداد فراخوان و پیش‌فرض‌های آرام پر می‌شوند، چون تست گزارش ستون زمان و
 *      ولتاژ پک را می‌خواند.
 * @param nowAbs  [EN] panel monotonic now / [FA] «حالا»ی یکنوای پنل
 * @param ageS    [EN] record age in seconds / [FA] عمر رکورد به ثانیه
 * @param v24Mv   [EN] pack voltage, mV / [FA] ولتاژ پک به میلی‌ولت
 * @param v12Mv   [EN] lower battery, mV / [FA] باتری پایینی
 * @param i1Ma    [EN] channel-1 current, mA / [FA] جریان کانال ۱
 * @param duty1   [EN] channel-1 duty, permille / [FA] دیوتی کانال ۱
 * @param state1  [EN] channel-1 state code / [FA] کد حالت کانال ۱
 * @param flags   [EN] sample flag byte / [FA] بایت فلگ نمونه
 * @return [EN] None / [FA] ندارد
 */
static void appendSampleAged(uint32_t nowAbs, uint32_t ageS, uint16_t v24Mv, uint16_t v12Mv,
                             uint16_t i1Ma, uint16_t duty1, uint8_t state1, uint8_t flags)
{
    up_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    sample.uint32_t__absS = nowAbs - ageS;
    sample.uint16_t__vinMv = 24000u;
    sample.uint16_t__v24Mv = v24Mv;
    sample.uint16_t__v12Mv = v12Mv;
    sample.uint16_t__i1Ma = i1Ma;
    sample.uint16_t__duty1 = duty1;
    sample.uint8_t__state1 = state1;
    sample.uint8_t__flags = flags;
    (void)func__UpStore_AppendSample(&sample);
}

static void seedReportData(void)
{
    uint32_t nowAbs;

    /* [EN] A sample outside the one-day window only EXISTS if the panel's own
       clock is far enough from zero: on a board that booted a minute ago,
       "three days old" is not representable at all, and a filter test written
       against it would pass by accident. So the test moves the panel's clock
       base first (which is exactly what an admin's clock write does).
       [FA] نمونه‌ای بیرون از پنجرهٔ یک‌روزه فقط وقتی «وجود دارد» که ساعت خود پنل
       به‌قدر کافی از صفر دور باشد: روی بردی که یک دقیقه پیش بوت شده، «سه روز
       پیش» اصلاً بازنمایی‌شدنی نیست و تست فیلتری که بر آن نوشته شود، تصادفی سبز
       می‌شود. پس تست اول پایهٔ ساعت پنل را جلو می‌برد (دقیقاً همان کاری که نوشتن
       ساعت توسط مدیر می‌کند). */
    func__UpStore_ClockBaseSet(1000000u, 1791331200u);
    func__UpHistory_Begin();
    /* [EN] The RAM clock is rebuilt from the pair the same way the board does it
       at boot, so the report's "generated at" line is the seeded wall clock and
       not the millisecond counter. Without it the summary would carry one time
       and the sample rows another, which is exactly the mismatch this report
       exists to prevent.
       [FA] ساعت RAM از همان جفت، مثل بوت برد، بازسازی می‌شود تا خط «زمان تولید»
       ساعت دیواری کاشته‌شده باشد و نه شمارندهٔ میلی‌ثانیه. */
    func__UpState_ClockRestore();
    nowAbs = func__UpHistory_AbsoluteS();

    /* [EN] Four witnesses, written OLDEST FIRST, exactly as the board writes
       them: the storage ring keeps append order, so a test that appends a fresh
       sample before an old one would produce a workbook whose rows are not in
       time order - a shape the firmware is not expected to fix and the bench
       would never see.
       [FA] چهار شاهد، از قدیمی به جدید، دقیقاً همان‌طور که برد می‌نویسد:
       حلقهٔ ذخیره ترتیب افزودن را نگه می‌دارد، پس تستی که نمونهٔ تازه را پیش از
       نمونهٔ قدیمی بیفزاید کتابی می‌سازد که ردیف‌هایش مرتب به زمان نیست -
       شکلی که قرار نیست فرم‌ور درستش کند و میز آزمایش هرگز نمی‌بیند. */
    appendSampleAged(nowAbs, (3u * 86400u) + 100u, 11111u, 11100u, 0u, 0u, 0u, 0x00u);
    appendSampleAged(nowAbs, (24u * 3600u) + 3000u, 7900u, 7800u, 0u, 0u, 0u, 0x00u);
    appendSampleAged(nowAbs, (23u * 3600u) + 3000u, 7700u, 7600u, 0u, 0u, 0u, 0x00u);
    appendSampleAged(nowAbs, 60u, 12345u, 12300u, 4200u, 700u, 1u, 0x27u);

    up_event_t event;
    memset(&event, 0, sizeof(event));
    event.uint32_t__absS = nowAbs - 120u;
    event.uint32_t__durS = 5400u;
    event.uint32_t__epoch = 1791331080u;
    event.uint16_t__valueA = 5400u;
    event.uint16_t__valueB = 1234u;
    event.uint8_t__code = (uint8_t)UP_EV_CHARGE_DONE;
    event.uint8_t__channel = 1u;
    event.uint8_t__severity = UP_SEV_INFO;
    (void)func__UpStore_AppendEvent(&event);

    memset(&event, 0, sizeof(event));
    event.uint32_t__absS = nowAbs - 90u;
    event.uint32_t__epoch = 1791331110u;
    event.uint16_t__valueA = (uint16_t)UP_FAULT_OVERCURRENT_2;
    event.uint8_t__code = (uint8_t)UP_EV_FAULT_SET;
    event.uint8_t__severity = UP_SEV_CRIT;
    (void)func__UpStore_AppendEvent(&event);

    memset(&event, 0, sizeof(event));
    event.uint32_t__absS = nowAbs - 80u;
    event.uint32_t__epoch = 1791331120u;
    event.uint8_t__code = (uint8_t)UP_EV_IMBALANCE;
    event.uint8_t__severity = UP_SEV_WARN;
    (void)func__UpStore_AppendEvent(&event);

    up_daily_t *day = func__UpStore_DailyToday();
    day->uint16_t__charges = 3u;
    day->uint16_t__incomplete = 1u;
    day->uint32_t__chargeSeconds = 5400u;
    day->uint32_t__energyWh100 = 1234u;
    day->uint32_t__runSeconds = 1800u;
    day->uint32_t__inputSeconds = 7200u;
    day->uint16_t__outages = 1u;
    day->uint16_t__boots = 0u;
    day->uint16_t__maxV = 28800u;
    day->uint16_t__minV = 24100u;
    day->uint8_t__hourCharges[9] = 2u;
    day->uint8_t__hourCharges[17] = 1u;
    func__UpStore_DailyMarkDirty();
}

static void testExcelReport(void)
{
    std::cout << "\n-- Excel report / گزارش اکسل\n";
    std::vector<TestZipMember> members;
    std::string why;

    check(loginAs("view1", "viewpass1"), "viewer logs in for the report test");
    useCookie();
    UP_WEBSERVER_T__G__Server.clearArgs();
    callRoute("/api/admin/report.xlsx");
    check(UP_WEBSERVER_T__G__Server.lastCode == 403, "a viewer cannot download the report");

    check(loginAs("admin", "newsecret1"), "admin logs in for the report test");
    useCookie();
    seedReportData();

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("days", "1");
    callRoute("/api/admin/report.xlsx");
    check(UP_WEBSERVER_T__G__Server.lastCode == 200, "the admin gets a report");
    check(UP_WEBSERVER_T__G__Server.lastType.find("spreadsheetml.sheet") != std::string::npos,
          "and it is sent as a spreadsheet", UP_WEBSERVER_T__G__Server.lastType);
    check(UP_WEBSERVER_T__G__Server.sentHeaders["Content-Disposition"].find("user_panel_report.xlsx") != std::string::npos,
          "with a name that says what it is");
    check(UP_WEBSERVER_T__G__Server.declaredLength == CONTENT_LENGTH_UNKNOWN,
          "streamed as chunks, because nobody knows how big it will be");

    const std::string oneDay = UP_WEBSERVER_T__G__Server.lastBody;
    check(oneDay.compare(0u, 4u, "PK\x03\x04", 4u) == 0, "the file starts like the zip it is");
    check(oneDay.find("\x50\x4B\x05\x06") != std::string::npos, "and ends with an end-of-directory record");

    if (!testZipRead(oneDay, members, why))
    {
        check(false, "the workbook is a readable zip", why);
        return;
    }
    check(members.size() == 10u, "ten members: five fixed parts and five sheets",
          std::to_string(members.size()));
    check(zipHas(members, "[Content_Types].xml"), "the content-type map is there");
    check(zipHas(members, "xl/workbook.xml"), "the workbook is there");
    check(zipHas(members, "xl/styles.xml"), "the styles are there");
    for (uint16_t sheet = 1u; sheet <= 5u; sheet++)
    {
        check(zipHas(members, "xl/worksheets/sheet" + std::to_string(sheet) + ".xml"),
              "sheet" + std::to_string(sheet) + " is there");
    }

    std::string workbook = zipRead(oneDay, members, "xl/workbook.xml");
    check(workbook.find("خلاصه") != std::string::npos, "the sheet names are Persian");
    check(workbook.find("نمونه") != std::string::npos, "including the samples sheet");
    check(workbook.find("r:id=\"rId5\"") != std::string::npos, "and all five are linked by id");

    std::string summary = zipRead(oneDay, members, "xl/worksheets/sheet1.xml");
    check(summary.find("<sheetView rightToLeft=\"1\"") != std::string::npos, "the sheets read right to left");
    check(summary.find("<pane ySplit=\"2\"") != std::string::npos,
          "with the title and the header row both frozen");
    check(summary.find("روز گذشته") != std::string::npos, "the summary states the chosen range");
    check(summary.find("توزیع مدت شارژها") != std::string::npos, "the histogram is on the summary sheet");
    check(summary.find("جمع کل از ابتدا") != std::string::npos, "and the lifetime totals");

    /* [EN] Both workbooks are left on disk for the shell gate: the filtered one
       and the full one, so the Python reader can prove the range selector
       changed the file itself and not only the label on the summary sheet.
       [FA] هر دو کتاب روی دیسک می‌مانند تا خوانندهٔ پایتونی ثابت کند انتخاب
       بازه خود فایل را عوض کرده و نه فقط برچسب برگهٔ خلاصه را. */
    FILE *dumpRange = fopen("/tmp/up_report_1day.xlsx", "wb");
    if (dumpRange != NULL)
    {
        fwrite(oneDay.data(), 1u, oneDay.size(), dumpRange);
        fclose(dumpRange);
    }

    std::string samples = zipRead(oneDay, members, "xl/worksheets/sheet5.xml");
    check(samples.find("<pane ySplit=\"1\"") != std::string::npos, "and the sample sheet freezes its header row");
    check(samples.find("12.345") != std::string::npos, "a sample from the last day is in the sample sheet");
    check(samples.find("11.111") == std::string::npos, "a three-day-old sample is NOT in a one-day report");
    /* [EN] Two more samples sit either side of the one-day edge (23h50m old and
       24h50m old). Without them the exclusion above could pass for the wrong
       reason - a filter that drops everything would look just as green.
       [FA] دو نمونهٔ دیگر دو طرف مرز یک‌روزه نشسته‌اند (۲۳:۵۰ و ۲۴:۵۰ ساعت).
       بدون آن‌ها، ردِ بالا ممکن بود به دلیل غلط سبز شود: فیلتری که همه‌چیز را
       می‌اندازد هم همین‌طور سبز به نظر می‌رسد. */
    check(samples.find("7.700") != std::string::npos, "a sample just INSIDE the one-day edge is kept");
    check(samples.find("7.900") == std::string::npos, "a sample just OUTSIDE the one-day edge is dropped");
    check(samples.find("cols") < samples.find("<sheetData>"), "column widths come before the rows, as the format demands");

    std::string charges = zipRead(oneDay, members, "xl/worksheets/sheet3.xml");
    check(charges.find("شارژ کامل شد") != std::string::npos, "the charge sheet names the charge");
    check(charges.find("کانال بالایی") != std::string::npos, "and which channel it was");
    check(charges.find("<v>90</v>") != std::string::npos, "5400 seconds is on the sheet as 90 minutes");

    std::string events = zipRead(oneDay, members, "xl/worksheets/sheet4.xml");
    check(events.find("اضافه‌جریان کانال پایینی") != std::string::npos,
          "a fault event is written as the sentence the operator knows");
    check(events.find("رویداد عدم‌توازن") != std::string::npos, "so is the imbalance episode");

    /* [EN] The whole range: the old sample must come back. This is the check
       that the range selector really changes the file and not just the label.
       [FA] کل بازه: نمونهٔ قدیمی باید برگردد. همین بررسی نشان می‌دهد انتخاب بازه
       واقعاً فایل را عوض می‌کند و نه فقط برچسب را. */
    members.clear();
    UP_WEBSERVER_T__G__Server.clearArgs();
    callRoute("/api/admin/report.xlsx");
    check(UP_WEBSERVER_T__G__Server.lastCode == 200, "a report with no range filter is served");
    const std::string allDays = UP_WEBSERVER_T__G__Server.lastBody;
    check(allDays.size() > oneDay.size(), "and it is bigger, because more samples are in it");
    if (!testZipRead(allDays, members, why))
    {
        check(false, "the unfiltered workbook is a readable zip", why);
    }
    else
    {
        summary = zipRead(allDays, members, "xl/worksheets/sheet1.xml");
        samples = zipRead(allDays, members, "xl/worksheets/sheet5.xml");
        check(summary.find("همهٔ تاریخچهٔ ذخیره‌شده") != std::string::npos, "the summary says the range is everything");
        check(samples.find("11.111") != std::string::npos, "and the three-day-old sample is back");
        check(samples.find("12.345") != std::string::npos, "next to the fresh one");
    }

    /* [EN] Leave the workbook on disk: the shell gate then opens the SAME bytes
       with Python's zipfile and parses every XML part, which is a third reader
       agreeing with the first two.
       [FA] کتاب را روی دیسک می‌گذاریم: گیت شل همان بایت‌ها را با zipfile پایتون
       باز می‌کند و همهٔ اجزای XML را پارس می‌کند؛ خوانندهٔ سومی که با دو خوانندهٔ
       اول توافق می‌کند. */
    FILE *dump = fopen("/tmp/up_report.xlsx", "wb");
    if (dump != NULL)
    {
        fwrite(allDays.data(), 1u, allDays.size(), dump);
        fclose(dump);
        check(true, "the workbook is left at /tmp/up_report.xlsx for the shell gate");
    }
    else
    {
        check(false, "the test can write the workbook to /tmp for the shell gate");
    }

    UP_WEBSERVER_T__G__Server.clearArgs();
    UP_WEBSERVER_T__G__Server.setArg("limit", "8");
    callRoute("/api/admin/audit");
    check(bodyHas("report_all"), "downloading a report leaves an audit line",
          UP_WEBSERVER_T__G__Server.lastBody);
}

int main(void)
{
    std::cout << "ChangeOver user panel - host tests\n";
    std::cout << "=================================\n";

    setup();

    std::cout << "\n-- boot / راه‌اندازی\n";
    check(UPPANEL_STATE_T__G__State.bool__storageOk, "LittleFS mounted");
    check(UINT8_T__G__UserCount == 2u, "two seeded accounts exist", std::to_string(UINT8_T__G__UserCount));
    check(UINT8_T__G__UserCount >= 2u, "the admin account exists");
    check(WiFi.apStarted, "the user access point is up");
    check(WiFi.apIp.toString() == "192.168.5.1", "the panel's own AP avoids the engineering subnet", WiFi.apIp.toString());
    check(WiFi.staStarted, "the panel joined the engineering network");

    testRoutesRegistered();
    testPageAndVersion();
    testLoginAndPassword();
    testTelemetryAndHistory();
    testApiShapes();
    testRolesAndActions();
    testExportAndPurge();
    testStorageRotation();
    testCryptoAndClock();
    testCalendar();
    testAdminClock();
    testExcelReport();

    std::cout << "\n=================================\n";
    std::cout << checkCount << " checks, " << failCount << " failures\n";
    return (failCount == 0) ? 0 : 1;
}
