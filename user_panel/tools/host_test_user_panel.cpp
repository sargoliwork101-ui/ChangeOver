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

    std::cout << "\n=================================\n";
    std::cout << checkCount << " checks, " << failCount << " failures\n";
    return (failCount == 0) ? 0 : 1;
}
