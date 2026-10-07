/**
 * @file    up_http.h
 * @brief   [EN] The user panel's web layer: the page, the JSON API and the
 *              three things a user is actually allowed to do.
 *
 *          WHAT THIS FILE IS
 *            One small web server on port 80 that serves the panel (HTML, CSS,
 *            JS and the embedded font out of PROGMEM) plus a JSON API for the
 *            live view, the statistics, the event log and the diagnostics. It
 *            is the ONLY part of this program that a browser talks to.
 *
 *          WHAT IT IS NOT ALLOWED TO DO
 *            Read a parameter from the board: yes. WRITE one: only through
 *            POST /api/admin/action, which is limited to the two charger
 *            enable bits (parameters 11 and 12) and to purging this panel's own
 *            history. There is no route - not even for an administrator - that
 *            changes a threshold, a calibration value or a PID term, because
 *            the user asked for a display panel and the safest way to keep a
 *            promise like that is to not have the code that could break it.
 *
 *          ROLES (every data route is gated)
 *            viewer   : the page, /api/live, /api/series, /api/events, /api/stats
 *            operator : + POST /api/admin/action (cut / reconnect a charger)
 *            admin    : + /api/admin/users, /api/admin/user, /api/admin/audit,
 *                       /api/export, and the purge action
 *
 *          PASSWORDS
 *            The panel holds a salted, 600-times iterated SHA-256 digest per
 *            user (up_auth.h) and never the password. Sessions are random
 *            32-character tokens in an HttpOnly cookie with an 8-hour idle
 *            timeout. This is access control for a private access point, not
 *            transport security: there is no TLS on an ESP8266, and the file
 *            says so out loud rather than implying otherwise.
 *
 * @brief   [FA] لایهٔ وب پنل کاربر: خود صفحه، سرویس JSON و سه کاری که کاربر
 *              واقعاً اجازه دارد.
 *
 *          این فایل چیست
 *            یک سرور وب کوچک روی پورت ۸۰ که پنل را سرو می‌کند (HTML، CSS، JS
 *            و فونت جاسازی‌شده از PROGMEM) به‌علاوهٔ یک سرویس JSON برای نمای
 *            زنده، آمار، گزارش رویدادها و دیاگ. تنها بخشی از این برنامه است که
 *            مرورگر با آن حرف می‌زند.
 *
 *          چه کاری اجازه ندارد
 *            خواندن پارامتر از برد: بله. نوشتن: فقط از راه
 *            POST /api/admin/action که محدود است به دو بیت فعال‌بودن شارژر
 *            (پارامتر ۱۱ و ۱۲) و پاک‌کردن تاریخچهٔ خود پنل. هیچ مسیری - حتی
 *            برای مدیر - آستانه، کالیبراسیون یا ضریب PID را عوض نمی‌کند، چون
 *            کاربر پنل نمایشگر خواست و امن‌ترین راهِ نگه‌داشتن چنین قولی این
 *            است که کدی که می‌تواند آن را بشکند وجود نداشته باشد.
 *
 *          نقش‌ها (هر مسیر داده نگهبان دارد)
 *            بیننده  : صفحه، /api/live، /api/series، /api/events، /api/stats
 *            اپراتور : + POST /api/admin/action (قطع/وصل شارژر)
 *            مدیر    : + /api/admin/users، /api/admin/user، /api/admin/audit،
 *                      /api/export و اقدام پاک‌سازی
 *
 *          گذرواژه‌ها
 *            پنل برای هر کاربر یک چکیدهٔ SHA-256 نمک‌دار و ۶۰۰ بار تکرارشده
 *            نگه می‌دارد (up_auth.h) و هرگز خودِ گذرواژه را. نشست‌ها توکن‌های
 *            تصادفی ۳۲ نویسه‌ای در کوکی HttpOnly با مهلت بی‌کاری ۸ ساعته‌اند.
 *            این کنترل دسترسی برای یک اکسس‌پوینت خصوصی است، نه امنیت انتقال:
 *            روی ESP8266 هیچ TLSی نیست و فایل این را با صدای بلند می‌گوید نه
 *            اینکه چیز دیگری را القا کند.
 */

#ifndef UP_HTTP_H
#define UP_HTTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ESP8266WebServer.h>
#include "up_config.h"
#include "up_state.h"
#include "up_sha256.h"
#include "up_store.h"
#include "up_history.h"
#include "up_auth.h"
#include "up_link.h"

/* ==================== Constants / ثابت‌ها ==================== */
#define UP_HTTP_PORT            80u
#define UP_INPUT_OVER_MV        28000u   /* [EN] input over-voltage alarm, the number the fault catalog names / [FA] آستانهٔ اضافه‌ولتاژ ورودی، همان عددی که کاتالوگ خطا نام می‌برد */
#define UP_IMB_WARN_MV          300u     /* [EN] half-to-half difference worth a word / [FA] اختلاف دو نیمه که ارزش گفتن دارد */
#define UP_IMB_LATCHED_BIT         0x02u  /* [EN] fl2 bit: imbalance latched / [FA] بیت fl2: عدم‌توازن قفل‌شده */
#define UP_IMB_BLOCK_BIT           0x04u  /* [EN] fl2 bit: charging blocked / [FA] بیت fl2: شارژ مسدود */
#define UP_STORE_FULL_PERCENT   92u      /* [EN] above this the panel says so / [FA] بالاتر از این پنل اعلام می‌کند */
#define UP_STORE_WARN_PERCENT   75u
#define UP_COUNTERS_MS          5000u    /* [EN] ring counters cache / [FA] کش شمارنده‌های حلقه */
#define UP_CLOCK_MIN_EPOCH      1600000000u
#define UP_CLOCK_MAX_EPOCH      2000000000u
#define UP_SERIES_LABELS        5u       /* [EN] time labels on the long chart / [FA] برچسب‌های زمان روی نمودار بلند */
#define UP_FAULT_BITS           7u       /* [EN] fault bits the board reports / [FA] بیت‌های خطای برد */
#define UP_STAT_HEAT_ROWS       7u       /* [EN] heat-map rows / [FA] ردیف‌های نقشهٔ حرارتی */
#define UP_STAT_DAILY_ROWS      32u      /* [EN] daily rows in one reply / [FA] ردیف‌های روزانه در یک پاسخ */
#define UP_STAT_SESSIONS        24u      /* [EN] charge sessions in one reply / [FA] شارژها در یک پاسخ */
#define UP_STAT_SLOT_SWEEP      48u      /* [EN] daily slots walked backwards / [FA] خانه‌های روزانه که به عقب خوانده می‌شود */
#define UP_EVENT_TAIL_MAX       64u      /* [EN] events one reply may carry / [FA] رویدادی که یک پاسخ می‌تواند ببرد */
#define UP_CSV_CHUNK            512u     /* [EN] one CSV chunk pushed at a time / [FA] یک تکهٔ CSV در هر ارسال */

/* ==================== State / وضعیت ==================== */
static ESP8266WebServer UP_WEBSERVER_T__G__Server(UP_HTTP_PORT);
static char     CHAR__G__UpWebJson[UP_JSON_BUF];
static uint32_t UINT32_T__G__UpWebJsonUsed = 0u;
static bool     BOOL__G__UpWebJsonOverflow = false;
static char     CHAR__G__UpCookieHeader[128];
static const char CHAR__A__UpBuildStamp[] = UP_BUILD_STAMP;

static uint32_t UINT32_T__G__UpCachedSamples = 0u;
static uint32_t UINT32_T__G__UpCachedEvents = 0u;
static uint32_t UINT32_T__G__UpCachedDays = 0u;
static uint32_t UINT32_T__G__UpCountersMs = 0u;

/* [EN] Forward declarations: the request handlers below are written in the
   order a reader meets them on the page, so a few helpers are used before the
   section that defines them.
   [FA] اعلان جلوتر: هندلرها به همان ترتیبی نوشته شده‌اند که خواننده در صفحه
   می‌بیند، پس چند کمکی پیش از بخش تعریفشان استفاده می‌شوند. */
static void func__UpHttp_ResolveSession(void);
static bool func__UpHttp_RequireRole(uint8_t uint8_t__needed);
static void func__UpHttp_RefreshCounters(uint32_t uint32_t__nowMs);
static const char *func__UpHttp_CurrentUserName(void);
static void func__UpHttp_ActiveCodes(void);

/* ==================== JSON writer / نویسندهٔ JSON ==================== */
/**
 * @brief  [EN] Start a fresh JSON document in the shared buffer.
 *         [FA] شروع یک سند JSON تازه در بافر مشترک.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_JsonReset(void)
{
    UINT32_T__G__UpWebJsonUsed = 0u;
    BOOL__G__UpWebJsonOverflow = false;
    CHAR__G__UpWebJson[0u] = '\0';
}

/**
 * @brief  [EN] Append formatted text. Overflow is remembered instead of
 *              truncating: a half-written JSON document is worse than an error,
 *              because the browser silently gets nonsense it cannot detect.
 *         [FA] افزودن متن قالب‌بندی‌شده. سرریز «یادداشت» می‌شود نه بریده: سند
 *              JSON نصفه از خطا بدتر است، چون مرورگر بی‌صدا مزخرفی می‌گیرد
 *              که نمی‌تواند تشخیص دهد.
 * @param  char__format [EN] printf format / [FA] قالب printf
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_JsonAdd(const char *char__format, ...)
{
    va_list up_va_list_t__args;
    int int__written;

    if (BOOL__G__UpWebJsonOverflow)
    {
        return;
    }

    va_start(up_va_list_t__args, char__format);
    int__written = vsnprintf(&CHAR__G__UpWebJson[UINT32_T__G__UpWebJsonUsed],
                             (size_t)(UP_JSON_BUF - UINT32_T__G__UpWebJsonUsed),
                             char__format, up_va_list_t__args);
    va_end(up_va_list_t__args);

    if (int__written < 0)
    {
        BOOL__G__UpWebJsonOverflow = true;
        return;
    }

    uint32_t uint32_t__room = (uint32_t)(UP_JSON_BUF - 1u - UINT32_T__G__UpWebJsonUsed);
    if ((uint32_t)int__written > uint32_t__room)
    {
        BOOL__G__UpWebJsonOverflow = true;
        return;
    }

    UINT32_T__G__UpWebJsonUsed += (uint32_t)int__written;
}

/**
 * @brief  [EN] Send whatever has been built, or a 500 that says so.
 *         [FA] فرستادن هرچه ساخته شده، یا یک ۵۰۰ که همان را می‌گوید.
 * @param  int__code [EN] HTTP status / [FA] کد وضعیت
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_JsonSend(int int__code)
{
    if (BOOL__G__UpWebJsonOverflow)
    {
        UP_WEBSERVER_T__G__Server.send(500, "application/json; charset=utf-8",
                                       "{\"ok\":0,\"err\":\"reply too big\"}");
        return;
    }

    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    UP_WEBSERVER_T__G__Server.send(int__code, "application/json; charset=utf-8", CHAR__G__UpWebJson);
}

/**
 * @brief  [EN] The one error shape the page understands: `{"ok":0,"err":...}`.
 *              A short ASCII token, translated by the page - the panel never
 *              stores Persian error text in RAM.
 *         [FA] تنها شکل خطایی که صفحه می‌فهمد: `{"ok":0,"err":...}`. یک توکن
 *              کوتاه ASCII که صفحه ترجمه می‌کند - پنل هیچ‌وقت متن خطای فارسی
 *              در RAM نگه نمی‌دارد.
 * @param  int__code [EN] HTTP status / [FA] کد وضعیت
 * @param  char__err [EN] token / [FA] توکن
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_SendErr(int int__code, const char *char__err)
{
    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":0,\"err\":\"%s\"}", char__err);
    func__UpHttp_JsonSend(int__code);
}

/* ==================== Numbers and time as text / عدد و زمان به‌صورت متن ==================== */
/**
 * @brief  [EN] Write an unsigned number in Persian digits (U+06F0..U+06F9 are
 *              the ASCII digits plus 0x06C0, two UTF-8 bytes each).
 *         [FA] نوشتن یک عدد بدون علامت با ارقام فارسی (U+06F0 تا U+06F9 یعنی
 *              ارقام ASCII به‌اضافهٔ 0x06C0، هر کدام دو بایت UTF-8).
 * @param  char__out [EN] destination / [FA] مقصد
 * @param  uint32_t__outRoom [EN] bytes available there / [FA] بایت موجود
 * @param  uint32_t__value [EN] number / [FA] عدد
 * @return [EN] None; a number that does not fit loses its last digits
 *          [FA] ندارد؛ عددی که جا نشود آخرین ارقامش را از دست می‌دهد
 */
static void func__UpHttp_PersianUint(char *char__out, uint32_t uint32_t__outRoom, uint32_t uint32_t__value)
{
    char char__latin[12];
    uint32_t uint32_t__index = 0u;

    (void)snprintf(char__latin, sizeof(char__latin), "%lu", (unsigned long)uint32_t__value);

    for (uint32_t uint32_t__i = 0u; char__latin[uint32_t__i] != '\0'; uint32_t__i++)
    {
        uint8_t uint8_t__digit = (uint8_t)(char__latin[uint32_t__i] - '0');

        /* [EN] Two bytes per digit plus the terminator: stop instead of writing
                past the caller's buffer. A number too long for the space simply
                loses its last digits rather than the panel losing its memory.
           [FA] هر رقم دو بایت به‌علاوهٔ پایان‌بخش: به‌جای نوشتن بیرون از بافر
                فراخوان می‌ایستیم. عددی که در جا نمی‌گنجد فقط آخرین ارقامش را
                از دست می‌دهد، نه پنل حافظه‌اش را. */
        if ((uint32_t__index + 3u) > uint32_t__outRoom)
        {
            break;
        }

        char__out[uint32_t__index] = (char)0xDBu;                         /* [EN] UTF-8 lead byte */
        char__out[uint32_t__index + 1u] = (char)(0xB0u + uint8_t__digit); /* [EN] ۰..۹ */
        uint32_t__index += 2u;
    }
    char__out[uint32_t__index] = '\0';
}

/**
 * @brief  [EN] "HH:MM" in Persian digits from an epoch second.
 *         [FA] «HH:MM» با ارقام فارسی از یک ثانیهٔ مطلق.
 * @param  uint32_t__epoch [EN] epoch seconds / [FA] ثانیهٔ مطلق
 * @param  char__out [EN] destination, at least 16 bytes / [FA] مقصد، دست‌کم ۱۶ بایت
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_ClockLabel(uint32_t uint32_t__epoch, char *char__out)
{
    /* [EN] Local hour, not UTC: an axis that says "03:00" while the sun is
       overhead is worse than no axis at all.
       [FA] ساعت محلی، نه UTC: محوری که وقتی خورشید بالای سر است «۰۳:۰۰» بگوید،
       از محور نداشتن بدتر است. */
    uint32_t uint32_t__daySecond = func__UpCal_LocalSecondOfDay(uint32_t__epoch, func__UpState_TzOffsetS());
    uint32_t uint32_t__hour = uint32_t__daySecond / 3600u;
    uint32_t uint32_t__minute = (uint32_t__daySecond % 3600u) / 60u;
    char char__hourText[8];
    char char__minuteText[8];

    func__UpHttp_PersianUint(char__hourText, sizeof(char__hourText), uint32_t__hour);
    func__UpHttp_PersianUint(char__minuteText, sizeof(char__minuteText), uint32_t__minute);
    (void)snprintf(char__out, 16u, "%s:%s", char__hourText, char__minuteText);
}

/**
 * @brief  [EN] How long ago, short: minutes under two hours, hours above that.
 *              Used on chart axes where the width is precious.
 *         [FA] چند وقت پیش، کوتاه: زیر دو ساعت دقیقه، بالاتر ساعت. روی محور
 *              نمودار که پهنا گران است.
 * @param  uint32_t__ageS [EN] age in seconds / [FA] سن به ثانیه
 * @param  char__out [EN] destination, at least 24 bytes / [FA] مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AgeLabel(uint32_t uint32_t__ageS, char *char__out)
{
    char char__number[12];

    if (uint32_t__ageS < 7200u)
    {
        func__UpHttp_PersianUint(char__number, sizeof(char__number), (uint32_t__ageS + 59u) / 60u);
        (void)snprintf(char__out, 24u, "%s د", char__number);
    }
    else
    {
        func__UpHttp_PersianUint(char__number, sizeof(char__number), (uint32_t__ageS + 1800u) / 3600u);
        (void)snprintf(char__out, 24u, "%s س", char__number);
    }
}

/* ==================== Sessions and roles / نشست‌ها و نقش‌ها ==================== */
/**
 * @brief  [EN] Read the session cookie and put the caller into the role globals
 *              for the duration of this request.
 *         [FA] خواندن کوکی نشست و گذاشتن فراخوان در متغیرهای نقش برای مدت
 *              همین درخواست.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_ResolveSession(void)
{
    String up_string_t__cookie = UP_WEBSERVER_T__G__Server.header("Cookie");
    const char *char__cursor = up_string_t__cookie.c_str();
    char char__token[UP_SESSION_TOKEN_LEN + 1u];

    UINT8_T__G__CurrentRole = 0u;
    UINT8_T__G__CurrentUserIndex = 0xFFu;

    char__cursor = strstr(char__cursor, UP_SESSION_COOKIE "=");
    if (char__cursor == NULL)
    {
        return;
    }
    char__cursor += strlen(UP_SESSION_COOKIE "=");

    for (uint32_t uint32_t__i = 0u; uint32_t__i < UP_SESSION_TOKEN_LEN; uint32_t__i++)
    {
        char char__c = char__cursor[uint32_t__i];
        if ((char__c == '\0') || (char__c == ';') || (char__c == ' '))
        {
            break;
        }
        char__token[uint32_t__i] = char__c;
        char__token[uint32_t__i + 1u] = '\0';
    }

    up_session_t *up_session_t__session = func__UpAuth_FindSession(char__token);
    if (up_session_t__session != NULL)
    {
        UINT8_T__G__CurrentRole = up_session_t__session->uint8_t__role;
        UINT8_T__G__CurrentUserIndex = up_session_t__session->uint8_t__userIndex;
    }
}

/**
 * @brief  [EN] Role gate used at the top of every protected handler. Answers
 *              401 for "who are you?" and 403 for "not you", because the page
 *              shows a login screen for the first and a refusal for the second.
 *         [FA] نگهبان نقش در ابتدای هر هندلر محافظت‌شده. برای «تو کیستی؟»
 *              ۴۰۱ و برای «تو نه» ۴۰۳ می‌دهد، چون صفحه برای اولی فرم ورود و
 *              برای دومی پیام رد نشان می‌دهد.
 * @param  uint8_t__needed [EN] minimum UP_ROLE_* / [FA] کمینهٔ نقش
 * @return [EN] true when the caller may continue / [FA] اگر فراخوان اجازه دارد true
 */
static bool func__UpHttp_RequireRole(uint8_t uint8_t__needed)
{
    func__UpHttp_ResolveSession();

    if (UINT8_T__G__CurrentRole == 0u)
    {
        func__UpHttp_SendErr(401, "auth");
        return false;
    }
    if (!func__UpAuth_AtLeast(uint8_t__needed))
    {
        func__UpHttp_SendErr(403, "forbidden");
        return false;
    }

    return true;
}

/**
 * @brief  [EN] Role as the token the page compares against.
 *         [FA] نقش به‌صورت توکنی که صفحه با آن مقایسه می‌کند.
 * @param  uint8_t__role [EN] UP_ROLE_* / [FA] نقش
 * @return [EN] "admin" | "operator" | "viewer" / [FA] نام نقش
 */
static const char *func__UpHttp_RoleName(uint8_t uint8_t__role)
{
    if (uint8_t__role == UP_ROLE_ADMIN)
    {
        return "admin";
    }
    if (uint8_t__role == UP_ROLE_OPERATOR)
    {
        return "operator";
    }
    return "viewer";
}

/**
 * @brief  [EN] Name of the caller, or "" when anonymous.
 *         [FA] نام فراخوان، یا "" وقتی ناشناس است.
 * @return [EN] pointer into the user table / [FA] اشاره‌گر به جدول کاربران
 */
static const char *func__UpHttp_CurrentUserName(void)
{
    if (UINT8_T__G__CurrentUserIndex >= UP_USERS_MAX)
    {
        return "";
    }

    return UP_USER_T__A__Users[UINT8_T__G__CurrentUserIndex].char__name;
}

/* ==================== Static page / صفحهٔ ثابت ==================== */
/**
 * @brief  [EN] GET / - the panel shell, straight from PROGMEM.
 *         [FA] مسیر GET / - پوستهٔ پنل، مستقیم از PROGMEM.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Root(void)
{
    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    UP_WEBSERVER_T__G__Server.send_P(200, "text/html; charset=utf-8", UP_INDEX_HTML);
}

/**
 * @brief  [EN] GET /app.css - the theme, cached for ten minutes (it changes
 *              only when a new firmware is flashed).
 *         [FA] مسیر GET /app.css - پوستهٔ ظاهری، ده دقیقه کش می‌شود (فقط با
 *              فلش فرم‌ور تازه عوض می‌شود).
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Css(void)
{
    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "max-age=600");
    UP_WEBSERVER_T__G__Server.send_P(200, "text/css; charset=utf-8", UP_APP_CSS);
}

/**
 * @brief  [EN] GET /app.js - the whole application, one file, no CDN.
 *         [FA] مسیر GET /app.js - کل برنامه، یک فایل، بدون CDN.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Js(void)
{
    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "max-age=600");
    UP_WEBSERVER_T__G__Server.send_P(200, "application/javascript; charset=utf-8", UP_APP_JS);
}

/**
 * @brief  [EN] GET /f.css - the embedded Persian font, cached for a year: it is
 *              the heaviest asset on the device and it never changes.
 *         [FA] مسیر GET /f.css - فونت فارسی جاسازی‌شده، یک سال کش می‌شود:
 *              سنگین‌ترین دارایی دستگاه است و هرگز عوض نمی‌شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Font(void)
{
    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "max-age=31536000");
    UP_WEBSERVER_T__G__Server.send_P(200, "text/css; charset=utf-8", UP_PANEL_FONT_CSS);
}

/**
 * @brief  [EN] GET /version - who this is and which build is in the box. Not
 *              gated: the login page itself shows the build stamp, and a device
 *              that will not say what it is until you log in is a device nobody
 *              can diagnose over the phone.
 *         [FA] مسیر GET /version - این چیست و چه بیلدی داخلش است. نگهبان
 *              ندارد: خود صفحهٔ ورود شناسهٔ ساخت را نشان می‌دهد و دستگاهی که
 *              تا وارد نشوی نمی‌گوید چیست، دستگاهی است که تلفنی قابل عیب‌یابی
 *              نیست.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Version(void)
{
    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"name\":\"%s\",\"fw\":\"%s\",\"build\":\"%s\",\"board\":\"%s\","
                         "\"ap\":\"%s\",\"source\":\"%s\",\"router\":%u}",
                         UP_PANEL_NAME, UP_PANEL_VERSION, CHAR__A__UpBuildStamp, UP_TARGET_BOARD,
                         UP_AP_SSID, UP_SRC_HOST, (unsigned)UP_TLM_FIELDS);
    func__UpHttp_JsonSend(200);
}

/* ==================== Auth endpoints / سرویس‌های ورود ==================== */
/**
 * @brief  [EN] GET /api/me - who this cookie belongs to, or 401.
 *         [FA] مسیر GET /api/me - این کوکی مال کیست، یا ۴۰۱.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Me(void)
{
    func__UpHttp_ResolveSession();

    if ((UINT8_T__G__CurrentRole == 0u) || (UINT8_T__G__CurrentUserIndex >= UP_USERS_MAX))
    {
        func__UpHttp_SendErr(401, "auth");
        return;
    }

    bool bool__mustChange = (UP_USER_T__A__Users[UINT8_T__G__CurrentUserIndex].uint8_t__mustChange != 0u);

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"user\":\"%s\",\"role\":\"%s\",\"must\":%u}",
                         func__UpHttp_CurrentUserName(),
                         func__UpHttp_RoleName(UINT8_T__G__CurrentRole),
                         bool__mustChange ? 1u : 0u);
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] POST /api/login (form: u, p) - open a session and set the cookie.
 *              A refusal is deliberately identical for a wrong name and a wrong
 *              password, and the cool-down lives in the auth module, so neither
 *              the page nor an attacker learns which half was right.
 *         [FA] مسیر POST /api/login (فرم: u، p) - باز کردن نشست و گذاشتن
 *              کوکی. رد شدن برای نام غلط و گذرواژهٔ غلط عمداً یکسان است و
 *              خنک‌سازی در ماژول احراز هویت است، پس نه صفحه و نه مهاجم نمی‌فهمد
 *              کدام نیمه درست بوده.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Login(void)
{
    char char__token[UP_SESSION_TOKEN_LEN + 1u];
    uint8_t uint8_t__role = 0u;
    bool bool__mustChange = false;

    if ((!UP_WEBSERVER_T__G__Server.hasArg("u")) || (!UP_WEBSERVER_T__G__Server.hasArg("p")))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }

    String up_string_t__name = UP_WEBSERVER_T__G__Server.arg("u");
    String up_string_t__pass = UP_WEBSERVER_T__G__Server.arg("p");

    if (!func__UpAuth_Login(up_string_t__name.c_str(), up_string_t__pass.c_str(),
                            char__token, &uint8_t__role, &bool__mustChange))
    {
        func__UpHttp_SendErr(200, "bad login");
        return;
    }

    (void)snprintf(CHAR__G__UpCookieHeader, sizeof(CHAR__G__UpCookieHeader),
                   UP_SESSION_COOKIE "=%s; Path=/; Max-Age=%lu; HttpOnly; SameSite=Lax",
                   char__token, (unsigned long)UP_SESSION_IDLE_S);
    UP_WEBSERVER_T__G__Server.sendHeader("Set-Cookie", CHAR__G__UpCookieHeader);

    func__UpAuth_LogAction(up_string_t__name.c_str(), "login");

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1,\"user\":\"%s\",\"role\":\"%s\",\"must\":%u}",
                         up_string_t__name.c_str(), func__UpHttp_RoleName(uint8_t__role),
                         bool__mustChange ? 1u : 0u);
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] POST /api/logout - close the caller's own session.
 *         [FA] مسیر POST /api/logout - بستن نشست خودِ فراخوان.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Logout(void)
{
    func__UpHttp_ResolveSession();

    if (UINT8_T__G__CurrentUserIndex < UP_USERS_MAX)
    {
        func__UpAuth_CloseUserSessions(UINT8_T__G__CurrentUserIndex);
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "logout");
    }

    UP_WEBSERVER_T__G__Server.sendHeader("Set-Cookie", UP_SESSION_COOKIE "=; Path=/; Max-Age=0; HttpOnly");
    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1}");
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] POST /api/pass (form: p0, p1) - change MY password. The forced
 *              first-login change goes through exactly this route, so there is
 *              one password path in the whole program, not two.
 *         [FA] مسیر POST /api/pass (فرم: p0، p1) - تغییر گذرواژهٔ خودم. تغییر
 *              اجباری اولین ورود دقیقاً از همین مسیر می‌رود، پس در کل برنامه
 *              یک مسیر گذرواژه وجود دارد، نه دو.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Pass(void)
{
    if (!func__UpHttp_RequireRole(UP_ROLE_VIEWER))
    {
        return;
    }
    if ((!UP_WEBSERVER_T__G__Server.hasArg("p0")) || (!UP_WEBSERVER_T__G__Server.hasArg("p1")))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }

    String up_string_t__oldPass = UP_WEBSERVER_T__G__Server.arg("p0");
    String up_string_t__newPass = UP_WEBSERVER_T__G__Server.arg("p1");
    up_user_t *up_user_t__user = &UP_USER_T__A__Users[UINT8_T__G__CurrentUserIndex];

    if (up_string_t__newPass.length() < 6u)
    {
        func__UpHttp_SendErr(200, "password too short");
        return;
    }

    uint8_t uint8_t__digest[UP_HASH_BYTES];
    func__UpAuth_Digest(up_user_t__user->uint8_t__salt, up_string_t__oldPass.c_str(), uint8_t__digest);
    if (!func__UpAuth_DigestEquals(uint8_t__digest, up_user_t__user->uint8_t__digest))
    {
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "pass_fail");
        func__UpHttp_SendErr(200, "wrong password");
        return;
    }

    (void)func__UpAuth_SetPassword(func__UpHttp_CurrentUserName(), up_string_t__newPass.c_str(), false);
    func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "password_change");

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1}");
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] POST /api/clock (form: t) - the browser hands over its epoch
 *              seconds; the panel counts from there and keeps the mapping on
 *              flash. Implausible values are refused: a wrong clock silently
 *              rewrites what every chart claims, and that is worse than an
 *              unknown clock, which the page admits to.
 *         [FA] مسیر POST /api/clock (فرم: t) - مرورگر ثانیه‌های مطلقش را
 *              تحویل می‌دهد؛ پنل از آن لحظه می‌شمارد و نگاشت را روی فلش نگه
 *              می‌دارد. مقدار نامعقول رد می‌شود: ساعت غلط بی‌صدا ادعای همهٔ
 *              نمودارها را بازنویسی می‌کند و این از ساعت نامعلوم بدتر است،
 *              همان که صفحه به آن اعتراف می‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_ClockPost(void)
{
    char char__date[12];
    char char__time[10];
    char char__gregorian[12];
    int32_t int32_t__tz;

    /* [EN] ADMIN ONLY, on the owner's instruction: the panel does not take its
       time from whatever browser happens to open the page. A browser clock can
       be wrong by hours, and every daily row, every event stamp and the whole
       Excel report would quietly follow it. The admin decides the time; the page
       only helps with a "copy this device's time into the form" button.
       [FA] فقط مدیر، به دستور صاحب دستگاه: پنل ساعتش را از هر مرورگری که صفحه
       را باز کند نمی‌گیرد. ساعت یک مرورگر می‌تواند ساعت‌ها غلط باشد و آن‌وقت هر
       ردیف روزانه، هر مهر رویداد و کل گزارش اکسل بی‌صدا همراهش می‌روند. مدیر
       وقت را تعیین می‌کند؛ صفحه فقط با دکمهٔ «ساعت این دستگاه را بگذار» کمک
       می‌کند. */
    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }
    if (!UP_WEBSERVER_T__G__Server.hasArg("t"))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }

    String up_string_t__value = UP_WEBSERVER_T__G__Server.arg("t");
    long long__parsed = strtol(up_string_t__value.c_str(), NULL, 10);

    if ((long__parsed < (long)UP_CLOCK_MIN_EPOCH) || (long__parsed > (long)UP_CLOCK_MAX_EPOCH))
    {
        func__UpHttp_SendErr(200, "implausible");
        return;
    }

    /* [EN] The offset arrives in MINUTES because that is what a human and a form
       think in (Tehran = 210). Seconds would make the form a calculator.
       [FA] اختلاف به‌صورت «دقیقه» می‌آید چون انسان و فرم با دقیقه فکر می‌کنند
       (تهران = ۲۱۰). ثانیه، فرم را به ماشین‌حساب تبدیل می‌کرد. */
    int32_t__tz = func__UpState_TzOffsetS();
    if (UP_WEBSERVER_T__G__Server.hasArg("tz"))
    {
        long long__tzMinutes = strtol(UP_WEBSERVER_T__G__Server.arg("tz").c_str(), NULL, 10);

        if ((long__tzMinutes < ((long)UP_TZ_MIN_OFFSET_S / 60L)) ||
            (long__tzMinutes > ((long)UP_TZ_MAX_OFFSET_S / 60L)))
        {
            func__UpHttp_SendErr(200, "bad tz");
            return;
        }
        int32_t__tz = (int32_t)(long__tzMinutes * 60L);
    }

    func__UpState_SetClock((uint32_t)long__parsed, int32_t__tz);
    func__UpStore_ClockBaseSet(func__UpHistory_AbsoluteS(), (uint32_t)long__parsed);

    /* [EN] Answer with what the panel BELIEVES it was told - the date it will
       write into every row from now on. If that does not match the wall clock in
       the room, the admin sees it immediately instead of a week later in a
       report.
       [FA] پاسخ می‌گوید پنل چه فهمیده - تاریخی که از این لحظه در هر ردیف
       می‌نویسد. اگر با ساعت دیواری اتاق نمی‌خواند، مدیر همان لحظه می‌بیند نه یک
       هفته بعد در گزارش. */
    (void)func__UpCal_DateText((uint32_t)long__parsed, int32_t__tz, char__date);
    (void)func__UpCal_TimeText((uint32_t)long__parsed, int32_t__tz, char__time);
    (void)func__UpCal_GregorianText((uint32_t)long__parsed, int32_t__tz, char__gregorian);

    func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "clock_set");

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1,\"epoch\":%lu,\"tz\":%ld,\"date\":\"%s\",\"time\":\"%s\",\"gregorian\":\"%s\"}",
                         (unsigned long)long__parsed, (long)int32_t__tz, char__date, char__time, char__gregorian);
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] GET /api/admin/clock - what the panel thinks the time is, so the
 *              admin's form opens on the truth instead of a blank field.
 *         [FA] مسیر GET /api/admin/clock - پنل فکر می‌کند ساعت چند است، تا فرم
 *              مدیر روی واقعیت باز شود نه روی فیلد خالی.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminClockGet(void)
{
    uint32_t uint32_t__epoch;
    int32_t int32_t__tz;
    char char__date[12];
    char char__time[10];
    char char__gregorian[12];
    bool bool__set;

    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }

    uint32_t__epoch = func__UpState_NowEpochS();
    int32_t__tz = func__UpState_TzOffsetS();
    bool__set = (uint32_t__epoch != 0u);

    if (bool__set)
    {
        (void)func__UpCal_DateText(uint32_t__epoch, int32_t__tz, char__date);
        (void)func__UpCal_TimeText(uint32_t__epoch, int32_t__tz, char__time);
        (void)func__UpCal_GregorianText(uint32_t__epoch, int32_t__tz, char__gregorian);
    }
    else
    {
        (void)snprintf(char__date, sizeof(char__date), "-");
        (void)snprintf(char__time, sizeof(char__time), "-");
        (void)snprintf(char__gregorian, sizeof(char__gregorian), "-");
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"set\":%u,\"epoch\":%lu,\"tz\":%ld,\"tzMin\":%ld,"
                         "\"date\":\"%s\",\"time\":\"%s\",\"gregorian\":\"%s\",\"uptimeS\":%lu}",
                         bool__set ? 1u : 0u, (unsigned long)uint32_t__epoch,
                         (long)int32_t__tz, (long)(int32_t__tz / 60),
                         char__date, char__time, char__gregorian,
                         (unsigned long)func__UpState_UptimeS());
    func__UpHttp_JsonSend(200);
}


/* ==================== Ring counters / شمارنده‌های حلقه ==================== */
/**
 * @brief  [EN] Refresh the cached ring sizes and the storage numbers. Counting
 *              a ring means opening the file, so it is cached for five seconds
 *              instead of running on every two-second poll of the dashboard.
 *         [FA] تازه‌سازی اندازهٔ حلقه‌ها و اعداد حافظه. شمردن حلقه یعنی باز
 *              کردن فایل، پس پنج ثانیه کش می‌شود نه اینکه در هر پایش دو ثانیه‌ای
 *              داشبورد اجرا شود.
 * @param  uint32_t__nowMs [EN] millis() snapshot / [FA] مقدار millis()
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_RefreshCounters(uint32_t uint32_t__nowMs)
{
    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__UpCountersMs) < UP_COUNTERS_MS)
    {
        return;
    }
    UINT32_T__G__UpCountersMs = uint32_t__nowMs;

    UINT32_T__G__UpCachedSamples = func__UpStore_SampleCount();
    UINT32_T__G__UpCachedEvents = func__UpStore_EventCount();
    UPPANEL_STATE_T__G__State.uint32_t__storageUsed = func__UpStore_UsedBytes();
    UPPANEL_STATE_T__G__State.uint32_t__storageTotal = func__UpStore_TotalBytes();

    /* [EN] "Days of statistics" is coverage, not the number of daily rows: the
            rows are a rolling window and a panel that ran for a month must not
            claim a month while it has four days of rows left.
       [FA] «روزهای آمار» یعنی پوشش، نه تعداد ردیف‌های روزانه: ردیف‌ها پنجرهٔ
            غلتان‌اند و پنلی که یک ماه کار کرده نباید ادعای یک ماه کند وقتی
            چهار روز ردیف دارد. */
    uint32_t uint32_t__coverageS = func__UpStore_Totals()->uint32_t__coverageS;
    UINT32_T__G__UpCachedDays = (uint32_t__coverageS + 86399u) / 86400u;
}

/* ==================== Active faults / خطاهای فعال ==================== */
/**
 * @brief  [EN] Append one fault code to the JSON array being built.
 *         [FA] افزودن یک کد خطا به آرایهٔ JSON در حال ساخت.
 * @param  uint8_t__number [EN] 1..22 / [FA] ۱ تا ۲۲
 * @param  bool__first [EN] true while nothing has been written / [FA] تا وقتی چیزی نوشته نشده true
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AddCodeNum(uint8_t uint8_t__number, bool *bool__first)
{
    char char__code[8];

    (void)snprintf(char__code, sizeof(char__code), "E%02u", (unsigned)uint8_t__number);
    func__UpHttp_JsonAdd("%s\"%s\"", (*bool__first) ? "" : ",", char__code);
    *bool__first = false;
}

/**
 * @brief  [EN] Which of the 22 catalogued conditions are true right now. The
 *              mapping is the `src:` line of each entry in the page's own
 *              catalog, so the two halves cannot drift: a code the page can
 *              explain is a code the panel can light up, and nothing else is
 *              ever sent.
 *         [FA] کدام‌یک از ۲۲ وضعیت کاتالوگ همین حالا برقرار است. نگاشت همان
 *              خط `src:` هر ورودی در کاتالوگ خودِ صفحه است، پس دو نیمه نمی‌توانند
 *              از هم جدا بیفتند: کدی که صفحه می‌تواند توضیح دهد، کدی است که پنل
 *              می‌تواند روشن کند، و چیز دیگری هرگز فرستاده نمی‌شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_ActiveCodes(void)
{
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    uint32_t uint32_t__faultMask = func__UpState_TlmWord(UP_TLM_FAULTS);
    uint32_t uint32_t__vIn = func__UpState_TlmWord(UP_TLM_VIN);
    uint32_t uint32_t__vLow = func__UpState_TlmWord(UP_TLM_VLOW);
    uint32_t uint32_t__vHigh = func__UpState_TlmWord(UP_TLM_VHIGH);
    uint32_t uint32_t__state1 = func__UpState_TlmWord(UP_TLM_STATE1);
    uint32_t uint32_t__state2 = func__UpState_TlmWord(UP_TLM_STATE2);
    bool bool__measValid = ((up_telemetry_t__tlm->uint8_t__flags & 0x04u) != 0u);
    bool bool__input = ((up_telemetry_t__tlm->uint8_t__flags & 0x02u) != 0u);
    bool bool__manual = ((up_telemetry_t__tlm->uint8_t__flags & 0x20u) != 0u);
    bool bool__link = func__UpState_LinkOnline();
    bool bool__first = true;

    /* E01..E07: the board's own fault mask, bit for bit */
    for (uint8_t uint8_t__bit = 0u; uint8_t__bit < UP_FAULT_BITS; uint8_t__bit++)
    {
        if ((uint32_t__faultMask & (1u << uint8_t__bit)) != 0u)
        {
            func__UpHttp_AddCodeNum((uint8_t)(uint8_t__bit + 1u), &bool__first);
        }
    }

    /* E08: any imbalance status bit at all (detected, latched or blocked) */
    if (up_telemetry_t__tlm->uint8_t__flags2 != 0u)
    {
        func__UpHttp_AddCodeNum(8u, &bool__first);
    }

    /* E09 / E10: a channel parked in FINAL_FAULT waits for a board reset */
    if (uint32_t__state1 == UP_ST_FINAL_FAULT)
    {
        func__UpHttp_AddCodeNum(9u, &bool__first);
    }
    if (uint32_t__state2 == UP_ST_FINAL_FAULT)
    {
        func__UpHttp_AddCodeNum(10u, &bool__first);
    }

    /* E11: the link to the board is stale, so every other number is old news */
    if (!bool__link)
    {
        func__UpHttp_AddCodeNum(11u, &bool__first);
    }

    /* E12 / E13: the two counters that say the pair is talking badly rather
       than not at all (a version mismatch and a CRC error are both cumulative
       since the board booted) */
    if (up_telemetry_t__tlm->uint32_t__versionMismatch > 0u)
    {
        func__UpHttp_AddCodeNum(12u, &bool__first);
    }
    if (up_telemetry_t__tlm->uint32_t__crcErrors > 0u)
    {
        func__UpHttp_AddCodeNum(13u, &bool__first);
    }

    /* E14: running on the battery (information, not a fault) */
    if (!bool__input && bool__measValid)
    {
        func__UpHttp_AddCodeNum(14u, &bool__first);
    }

    /* E15: the input is present but too high */
    if (bool__input && (uint32_t__vIn > UP_INPUT_OVER_MV))
    {
        func__UpHttp_AddCodeNum(15u, &bool__first);
    }

    /* E16: a battery half reads nothing at all */
    if (bool__measValid && ((uint32_t__vLow < UP_BAT_ABSENT_MV) || (uint32_t__vHigh < UP_BAT_ABSENT_MV)))
    {
        func__UpHttp_AddCodeNum(16u, &bool__first);
    }

    /* E17 / E18: the two transient wait states the board parks in itself */
    if ((uint32_t__state1 == UP_ST_JIT_RETRY) || (uint32_t__state2 == UP_ST_JIT_RETRY))
    {
        func__UpHttp_AddCodeNum(17u, &bool__first);
    }
    if ((uint32_t__state1 == UP_ST_INPUT_WAIT) || (uint32_t__state2 == UP_ST_INPUT_WAIT))
    {
        func__UpHttp_AddCodeNum(18u, &bool__first);
    }

    /* E19: a charger was cut - from here or from the engineering panel */
    bool bool__cut1 = up_telemetry_t__tlm->bool__paramKnown[UP_PARAM_CHG1_ENABLE] &&
                      (up_telemetry_t__tlm->int32_t__param[UP_PARAM_CHG1_ENABLE] == 0);
    bool bool__cut2 = up_telemetry_t__tlm->bool__paramKnown[UP_PARAM_CHG2_ENABLE] &&
                      (up_telemetry_t__tlm->int32_t__param[UP_PARAM_CHG2_ENABLE] == 0);
    if (bool__cut1 || bool__cut2)
    {
        func__UpHttp_AddCodeNum(19u, &bool__first);
    }

    /* E20: the engineering panel put the machine in manual mode */
    if (bool__manual)
    {
        func__UpHttp_AddCodeNum(20u, &bool__first);
    }

    /* E21: the board has not finished a measurement cycle yet */
    if (!bool__measValid && bool__link)
    {
        func__UpHttp_AddCodeNum(21u, &bool__first);
    }

    /* E22: this panel's own storage is nearly full and is dropping old rows */
    uint32_t uint32_t__total = UPPANEL_STATE_T__G__State.uint32_t__storageTotal;
    uint32_t uint32_t__used = UPPANEL_STATE_T__G__State.uint32_t__storageUsed;
    bool bool__storeFull = UPPANEL_STATE_T__G__State.bool__storagePurging;
    if ((uint32_t__total > 0u) && (((uint32_t__used * 100u) / uint32_t__total) >= UP_STORE_FULL_PERCENT))
    {
        bool__storeFull = true;
    }
    if (bool__storeFull)
    {
        func__UpHttp_AddCodeNum(22u, &bool__first);
    }

    (void)up_totals_t__totals;
}

/* ==================== GET /api/live / نمای زنده ==================== */
/**
 * @brief  [EN] Everything the dashboard needs, in one reply: the 28 telemetry
 *              words, the decoded flags, link health, the panel clock, today's
 *              counters, imbalance, storage and the active fault codes.
 *         [FA] هرچه داشبورد لازم دارد، در یک پاسخ: ۲۸ کلمهٔ تلمتری، فلگ‌های
 *              رمزگشایی‌شده، سلامت لینک، ساعت پنل، شمارنده‌های امروز، عدم‌توازن،
 *              حافظه و کدهای خطای فعال.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Live(void)
{
    if (!func__UpHttp_RequireRole(UP_ROLE_VIEWER))
    {
        return;
    }

    func__UpHttp_RefreshCounters((uint32_t)millis());

    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;
    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    up_daily_t *up_daily_t__day = func__UpStore_DailyToday();
    bool bool__link = func__UpState_LinkOnline();
    bool bool__clock = (func__UpState_NowEpochS() != 0u);
    uint32_t uint32_t__faultMask = func__UpState_TlmWord(UP_TLM_FAULTS);

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"t\":[");
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_TLM_FIELDS; uint8_t__i++)
    {
        func__UpHttp_JsonAdd("%s%lu", (uint8_t__i == 0u) ? "" : ",",
                             (unsigned long)func__UpState_TlmWord(uint8_t__i));
    }

    func__UpHttp_JsonAdd("],\"fl\":%u,\"fl2\":%u,"
                         "\"flags\":{\"snapshot\":%u,\"input\":%u,\"measValid\":%u,\"ch1\":%u,\"ch2\":%u,\"manual\":%u},"
                         "\"link\":{\"online\":%u,\"ageMs\":%lu,\"frames\":%lu,\"vm\":%lu,\"ce\":%lu,\"err\":%lu,\"sta\":%u},"
                         "\"now\":{\"wallValid\":%u,\"wall\":%lu,\"uptimeS\":%lu,\"absS\":%lu},"
                         "\"today\":{\"charges\":%u,\"incomplete\":%u,\"runS\":%lu,\"chargeS\":%lu,\"energyWh10\":%lu},"
                         "\"imb\":{\"mv\":%lu,\"events\":%lu,\"cycles\":%lu,\"latched\":%u,\"blocked\":%u},"
                         "\"store\":{\"usedBytes\":%lu,\"totalBytes\":%lu,\"samples\":%lu,\"events\":%lu,\"days\":%lu,\"purging\":%u,\"purges\":%lu},"
                         "\"faultBits\":[",
                         (unsigned)up_telemetry_t__tlm->uint8_t__flags,
                         (unsigned)up_telemetry_t__tlm->uint8_t__flags2,
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags & 0x01u) ? 1u : 0u),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags & 0x02u) ? 1u : 0u),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags & 0x04u) ? 1u : 0u),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags & 0x08u) ? 1u : 0u),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags & 0x10u) ? 1u : 0u),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags & 0x20u) ? 1u : 0u),
                         bool__link ? 1u : 0u,
                         (unsigned long)func__UpState_AgeMs(),
                         (unsigned long)up_telemetry_t__tlm->uint32_t__frames,
                         (unsigned long)up_telemetry_t__tlm->uint32_t__versionMismatch,
                         (unsigned long)up_telemetry_t__tlm->uint32_t__crcErrors,
                         (unsigned long)UPPANEL_STATE_T__G__State.uint32_t__tlmErrors,
                         (unsigned)(UPPANEL_STATE_T__G__State.bool__staUp ? 1u : 0u),
                         bool__clock ? 1u : 0u,
                         (unsigned long)func__UpState_NowEpochS(),
                         (unsigned long)func__UpState_UptimeS(),
                         (unsigned long)func__UpHistory_AbsoluteS(),
                         (unsigned)up_daily_t__day->uint16_t__charges,
                         (unsigned)up_daily_t__day->uint16_t__incomplete,
                         (unsigned long)up_daily_t__day->uint32_t__runSeconds,
                         (unsigned long)up_daily_t__day->uint32_t__chargeSeconds,
                         (unsigned long)(up_daily_t__day->uint32_t__energyWh100 / 10u),
                         (unsigned long)func__UpState_TlmWord(UP_TLM_IMB_MV),
                         (unsigned long)func__UpState_TlmWord(UP_TLM_IMB_EVENTS),
                         (unsigned long)func__UpState_TlmWord(UP_TLM_IMB_CYCLES),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags2 & UP_IMB_LATCHED_BIT) ? 1u : 0u),
                         (unsigned)((up_telemetry_t__tlm->uint8_t__flags2 & UP_IMB_BLOCK_BIT) ? 1u : 0u),
                         (unsigned long)UPPANEL_STATE_T__G__State.uint32_t__storageUsed,
                         (unsigned long)UPPANEL_STATE_T__G__State.uint32_t__storageTotal,
                         (unsigned long)UINT32_T__G__UpCachedSamples,
                         (unsigned long)UINT32_T__G__UpCachedEvents,
                         (unsigned long)UINT32_T__G__UpCachedDays,
                         (unsigned)(UPPANEL_STATE_T__G__State.bool__storagePurging ? 1u : 0u),
                         (unsigned long)func__UpStore_PurgeCount());

    for (uint8_t uint8_t__bit = 0u; uint8_t__bit < UP_FAULT_BITS; uint8_t__bit++)
    {
        if ((uint32_t__faultMask & (1u << uint8_t__bit)) != 0u)
        {
            func__UpHttp_JsonAdd("%s%u", (uint8_t__bit == 0u) ? "" : ",", (unsigned)uint8_t__bit);
        }
    }

    func__UpHttp_JsonAdd("],\"faultCodes\":[");
    func__UpHttp_ActiveCodes();
    func__UpHttp_JsonAdd("],\"par\":{");

    bool bool__firstParam = true;
    const uint8_t UINT8_T__A__ReportedIds[2] = { UP_PARAM_PCT_VMIN, UP_PARAM_PCT_VMAX };
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 2u; uint8_t__i++)
    {
        uint8_t uint8_t__id = UINT8_T__A__ReportedIds[uint8_t__i];
        if (up_telemetry_t__tlm->bool__paramKnown[uint8_t__id])
        {
            func__UpHttp_JsonAdd("%s\"%u\":%ld", bool__firstParam ? "" : ",", (unsigned)uint8_t__id,
                                 (long)up_telemetry_t__tlm->int32_t__param[uint8_t__id]);
            bool__firstParam = false;
        }
    }

    func__UpHttp_JsonAdd("},\"tot\":{\"charges\":%lu,\"runS\":%lu,\"inputS\":%lu,\"coverageS\":%lu}}",
                         (unsigned long)up_totals_t__totals->uint32_t__charges,
                         (unsigned long)up_totals_t__totals->uint32_t__runS,
                         (unsigned long)up_totals_t__totals->uint32_t__inputS,
                         (unsigned long)up_totals_t__totals->uint32_t__coverageS);

    func__UpHttp_JsonSend(200);
}

/* ==================== GET /api/series / نمودار بلند ==================== */
/**
 * @brief  [EN] The stored samples decimated into at most UP_SERIES_POINTS
 *              points: average pack, lower battery and input voltage per time
 *              bucket. Buckets are computed from the CLOCK, not from the record
 *              index, so a gap (panel off, link down) shows as a gap instead of
 *              being squeezed into a straight line.
 *         [FA] نمونه‌های ذخیره‌شده، کم‌شده به حداکثر UP_SERIES_POINTS نقطه:
 *              میانگین پک، باتری پایینی و ولتاژ ورودی در هر سبد زمانی. سبدها
 *              از «ساعت» حساب می‌شوند نه از شمارهٔ رکورد، تا یک شکاف (خاموشی
 *              پنل، قطع لینک) به‌شکل شکاف دیده شود نه خط صاف کشیده‌شده.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Series(void)
{
    static uint32_t UINT32_T__A__SumV24[UP_SERIES_POINTS];
    static uint32_t UINT32_T__A__SumV12[UP_SERIES_POINTS];
    static uint32_t UINT32_T__A__SumVin[UP_SERIES_POINTS];
    static uint32_t UINT32_T__A__Count[UP_SERIES_POINTS];
    static uint32_t UINT32_T__A__AbsFirst[UP_SERIES_POINTS];
    up_ring_reader_t up_ring_reader_t__reader;

    if (!func__UpHttp_RequireRole(UP_ROLE_VIEWER))
    {
        return;
    }

    uint32_t uint32_t__hours = 24u;
    uint32_t uint32_t__points = UP_SERIES_POINTS;

    if (UP_WEBSERVER_T__G__Server.hasArg("hours"))
    {
        long long__hours = strtol(UP_WEBSERVER_T__G__Server.arg("hours").c_str(), NULL, 10);
        if ((long__hours >= 1) && (long__hours <= 168))
        {
            uint32_t__hours = (uint32_t)long__hours;
        }
    }
    if (UP_WEBSERVER_T__G__Server.hasArg("points"))
    {
        long long__points = strtol(UP_WEBSERVER_T__G__Server.arg("points").c_str(), NULL, 10);
        if ((long__points >= 16) && (long__points <= (long)UP_SERIES_POINTS))
        {
            uint32_t__points = (uint32_t)long__points;
        }
    }

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__points; uint32_t__i++)
    {
        UINT32_T__A__SumV24[uint32_t__i] = 0u;
        UINT32_T__A__SumV12[uint32_t__i] = 0u;
        UINT32_T__A__SumVin[uint32_t__i] = 0u;
        UINT32_T__A__Count[uint32_t__i] = 0u;
        UINT32_T__A__AbsFirst[uint32_t__i] = 0u;
    }

    uint32_t uint32_t__absNow = func__UpHistory_AbsoluteS();
    uint32_t uint32_t__windowS = uint32_t__hours * 3600u;
    uint32_t uint32_t__bucketS = (uint32_t__windowS + uint32_t__points - 1u) / uint32_t__points;

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    bool bool__open = func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t));

    if (bool__open)
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_sample_t up_sample_t__sample;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_sample_t__sample, (uint32_t)sizeof(up_sample_t)))
            {
                break;
            }

            /* [EN] Unsigned subtraction: a record from before a wrap is simply
               outside the window. */
            if ((uint32_t)(uint32_t__absNow - up_sample_t__sample.uint32_t__absS) > uint32_t__windowS)
            {
                continue;
            }

            uint32_t uint32_t__offsetS = uint32_t__absNow - up_sample_t__sample.uint32_t__absS;
            uint32_t uint32_t__bucket = uint32_t__offsetS / uint32_t__bucketS;
            uint32_t uint32_t__index = (uint32_t__points - 1u) - uint32_t__bucket;

            if (uint32_t__bucket >= uint32_t__points)
            {
                continue;
            }
            if (UINT32_T__A__Count[uint32_t__index] == 0u)
            {
                UINT32_T__A__AbsFirst[uint32_t__index] = up_sample_t__sample.uint32_t__absS;
            }
            UINT32_T__A__SumV24[uint32_t__index] += up_sample_t__sample.uint16_t__v24Mv;
            UINT32_T__A__SumV12[uint32_t__index] += up_sample_t__sample.uint16_t__v12Mv;
            UINT32_T__A__SumVin[uint32_t__index] += up_sample_t__sample.uint16_t__vinMv;
            UINT32_T__A__Count[uint32_t__index]++;
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"n\":%lu,\"v24\":[", (unsigned long)uint32_t__points);
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__points; uint32_t__i++)
    {
        uint32_t uint32_t__value = (UINT32_T__A__Count[uint32_t__i] > 0u) ? (UINT32_T__A__SumV24[uint32_t__i] / UINT32_T__A__Count[uint32_t__i]) : 0u;
        func__UpHttp_JsonAdd("%s%lu", (uint32_t__i == 0u) ? "" : ",", (unsigned long)uint32_t__value);
    }
    func__UpHttp_JsonAdd("],\"v12\":[");
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__points; uint32_t__i++)
    {
        uint32_t uint32_t__value = (UINT32_T__A__Count[uint32_t__i] > 0u) ? (UINT32_T__A__SumV12[uint32_t__i] / UINT32_T__A__Count[uint32_t__i]) : 0u;
        func__UpHttp_JsonAdd("%s%lu", (uint32_t__i == 0u) ? "" : ",", (unsigned long)uint32_t__value);
    }
    func__UpHttp_JsonAdd("],\"vin\":[");
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__points; uint32_t__i++)
    {
        uint32_t uint32_t__value = (UINT32_T__A__Count[uint32_t__i] > 0u) ? (UINT32_T__A__SumVin[uint32_t__i] / UINT32_T__A__Count[uint32_t__i]) : 0u;
        func__UpHttp_JsonAdd("%s%lu", (uint32_t__i == 0u) ? "" : ",", (unsigned long)uint32_t__value);
    }
    func__UpHttp_JsonAdd("],\"ageMin\":[");
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__points; uint32_t__i++)
    {
        uint32_t uint32_t__ageS = (UINT32_T__A__Count[uint32_t__i] > 0u) ? (uint32_t__absNow - UINT32_T__A__AbsFirst[uint32_t__i]) : 0u;
        func__UpHttp_JsonAdd("%s%lu", (uint32_t__i == 0u) ? "" : ",", (unsigned long)((uint32_t__ageS + 30u) / 60u));
    }
    func__UpHttp_JsonAdd("],\"label\":[");
    for (uint32_t uint32_t__i = 0u; uint32_t__i < UP_SERIES_LABELS; uint32_t__i++)
    {
        uint32_t uint32_t__index = (uint32_t)(((uint64_t)uint32_t__i * (uint64_t)(uint32_t__points - 1u)) / (uint64_t)(UP_SERIES_LABELS - 1u));
        char char__label[24];

        if ((UINT32_T__A__Count[uint32_t__index] > 0u) && (func__UpState_NowEpochS() != 0u))
        {
            func__UpHttp_ClockLabel(func__UpState_EpochFromAbs(UINT32_T__A__AbsFirst[uint32_t__index]), char__label);
        }
        else if (UINT32_T__A__Count[uint32_t__index] > 0u)
        {
            func__UpHttp_AgeLabel(uint32_t__absNow - UINT32_T__A__AbsFirst[uint32_t__index], char__label);
        }
        else
        {
            (void)snprintf(char__label, sizeof(char__label), "-");
        }
        func__UpHttp_JsonAdd("%s\"%s\"", (uint32_t__i == 0u) ? "" : ",", char__label);
    }
    func__UpHttp_JsonAdd("]}");

    func__UpHttp_JsonSend(200);
}

/* ==================== GET /api/events / گزارش رویدادها ==================== */
/**
 * @brief  [EN] The newest closed events, newest first. The ring is only
 *              readable forwards, so a rolling window of the requested size is
 *              kept while streaming past everything older.
 *         [FA] جدیدترین رویدادهای بسته، از جدید به قدیم. حلقه فقط رو به جلو
 *              خوانده می‌شود، پس پنجرهٔ غلتانی به اندازهٔ درخواست نگه داشته
 *              می‌شود و از همهٔ قدیمی‌ترها رد می‌شویم.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Events(void)
{
    static up_event_t UP_EVENT_T__A__Tail[UP_EVENT_TAIL_MAX];
    up_ring_reader_t up_ring_reader_t__reader;
    uint32_t uint32_t__limit = 40u;

    if (!func__UpHttp_RequireRole(UP_ROLE_VIEWER))
    {
        return;
    }

    if (UP_WEBSERVER_T__G__Server.hasArg("limit"))
    {
        long long__limit = strtol(UP_WEBSERVER_T__G__Server.arg("limit").c_str(), NULL, 10);
        if ((long__limit >= 1) && (long__limit <= (long)UP_EVENT_TAIL_MAX))
        {
            uint32_t__limit = (uint32_t)long__limit;
        }
    }

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    uint32_t uint32_t__kept = 0u;

    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_EVENTS, (uint32_t)sizeof(up_event_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_event_t up_event_t__event;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_event_t__event, (uint32_t)sizeof(up_event_t)))
            {
                break;
            }
            if (uint32_t__kept < uint32_t__limit)
            {
                UP_EVENT_T__A__Tail[uint32_t__kept] = up_event_t__event;
                uint32_t__kept++;
            }
            else
            {
                for (uint32_t uint32_t__k = 1u; uint32_t__k < uint32_t__limit; uint32_t__k++)
                {
                    UP_EVENT_T__A__Tail[uint32_t__k - 1u] = UP_EVENT_T__A__Tail[uint32_t__k];
                }
                UP_EVENT_T__A__Tail[uint32_t__limit - 1u] = up_event_t__event;
            }
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    func__UpHttp_JsonReset();
    /* [EN] The list travels as `e`, the same shape as the user list (`u`) and
       the action log (`a`): one letter per collection, and the page already
       reads `ev.e`.
       [FA] فهرست با کلید `e` می‌رود، هم‌شکل فهرست کاربران (`u`) و گزارش
       اقدامات (`a`): یک حرف برای هر مجموعه، و صفحه از قبل `ev.e` را می‌خواند. */
    func__UpHttp_JsonAdd("{\"n\":%lu,\"e\":[", (unsigned long)uint32_t__kept);

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__kept; uint32_t__i++)
    {
        up_event_t *up_event_t__row = &UP_EVENT_T__A__Tail[uint32_t__kept - 1u - uint32_t__i];

        func__UpHttp_JsonAdd("%s{\"code\":%u,\"ch\":%u,\"dur\":%lu,\"sev\":%u,\"t\":%lu,\"a\":%u,\"b\":%u}",
                             (uint32_t__i == 0u) ? "" : ",",
                             (unsigned)up_event_t__row->uint8_t__code,
                             (unsigned)up_event_t__row->uint8_t__channel,
                             (unsigned long)up_event_t__row->uint32_t__durS,
                             (unsigned)up_event_t__row->uint8_t__severity,
                             (unsigned long)up_event_t__row->uint32_t__epoch,
                             (unsigned)up_event_t__row->uint16_t__valueA,
                             (unsigned)up_event_t__row->uint16_t__valueB);
    }
    func__UpHttp_JsonAdd("]}");

    func__UpHttp_JsonSend(200);
}

/* ==================== GET /api/stats / آمار و ارقام ==================== */
/**
 * @brief  [EN] The KPI block, the newest charge sessions, the newest daily rows
 *              and the hour-of-day heat map - everything the statistics page
 *              draws, computed from this panel's own flash.
 *         [FA] بلوک شاخص‌ها، جدیدترین شارژها، جدیدترین ردیف‌های روزانه و نقشهٔ
 *              حرارتی ساعت‌های شبانه‌روز - هرچه صفحهٔ آمار می‌کشد، از فلش خود
 *              پنل حساب می‌شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Stats(void)
{
    static uint32_t UINT32_T__A__DayEpoch[UP_STAT_SLOT_SWEEP];
    static uint32_t UINT32_T__A__DayCharges[UP_STAT_SLOT_SWEEP];
    static uint32_t UINT32_T__A__DayRunS[UP_STAT_SLOT_SWEEP];
    static uint8_t UINT8_T__A__Heat[UP_STAT_HEAT_ROWS][UP_DAY_HOURS];
    static uint32_t UINT32_T__A__SessDur[UP_STAT_SESSIONS];
    static uint32_t UINT32_T__A__SessEnd[UP_STAT_SESSIONS];
    static uint8_t UINT8_T__A__SessDone[UP_STAT_SESSIONS];
    up_ring_reader_t up_ring_reader_t__reader;

    if (!func__UpHttp_RequireRole(UP_ROLE_VIEWER))
    {
        return;
    }

    func__UpHttp_RefreshCounters((uint32_t)millis());

    up_totals_t *up_totals_t__totals = func__UpStore_Totals();
    bool bool__clock = (func__UpState_NowEpochS() != 0u);

    /* ---- daily rows: walk the slots backwards from today, keep the newest --- */
    uint32_t uint32_t__dailyCount = 0u;
    uint32_t uint32_t__heatRows = 0u;
    uint32_t uint32_t__todayIndex = func__UpState_DayIndex();

    /* [EN] Walk the days backwards from today and keep the ones the ring has
            not rotated away yet. Asking by DAY INDEX - rather than reading the
            ring slot by slot - is what makes a day the panel was switched off
            show up as a missing bar instead of pushing every other day's
            numbers one place along.
       [FA] از امروز به عقب روزها را می‌پیماییم و آن‌هایی را نگه می‌داریم که
            حلقه هنوز دور نریخته. پرسیدن بر اساس «شمارهٔ روز» - نه خواندن
            خانه‌های حلقه یکی‌یکی - همان چیزی است که باعث می‌شود روزی که پنل
            خاموش بوده به‌شکل میلهٔ غایب دیده شود، نه اینکه اعداد بقیهٔ روزها
            یک خانه جابه‌جا شوند. */
    for (uint32_t uint32_t__back = 0u; uint32_t__back < UP_STAT_SLOT_SWEEP; uint32_t__back++)
    {
        up_daily_t up_daily_t__row;

        if (uint32_t__back >= uint32_t__todayIndex)
        {
            break;
        }
        if (!func__UpStore_DailyRead(uint32_t__todayIndex - uint32_t__back, &up_daily_t__row))
        {
            continue;
        }

        /* [EN] The epoch of that day's LOCAL midnight: the day number is local,
           so subtracting the offset turns it back into an epoch second the
           browser draws at the right date.
           [FA] ثانیهٔ مطلقِ نیمه‌شب محلی آن روز: شمارهٔ روز محلی است، پس کسر
           کردن اختلاف، آن را به ثانیهٔ مطلقی برمی‌گرداند که مرورگر در تاریخ
           درست می‌کشد. */
        UINT32_T__A__DayEpoch[uint32_t__dailyCount] = bool__clock
            ? (((uint32_t__todayIndex - uint32_t__back) * 86400u) - (uint32_t)func__UpState_TzOffsetS())
            : 0u;
        UINT32_T__A__DayCharges[uint32_t__dailyCount] = (uint32_t)up_daily_t__row.uint16_t__charges;
        UINT32_T__A__DayRunS[uint32_t__dailyCount] = up_daily_t__row.uint32_t__runSeconds;

        /* [EN] The newest UP_STAT_HEAT_ROWS days become the heat map, in the
                order they were collected: newest first. The reply reverses them
                so the picture reads top-to-bottom as oldest-to-newest.
           [FA] جدیدترین روزها نقشهٔ حرارتی می‌شوند، به همان ترتیبی که جمع
                شده‌اند: جدیدترین اول. پاسخ آن‌ها را برمی‌گرداند تا تصویر از
                بالا به پایین قدیمی به جدید خوانده شود. */
        if (uint32_t__heatRows < UP_STAT_HEAT_ROWS)
        {
            memcpy(UINT8_T__A__Heat[uint32_t__heatRows], up_daily_t__row.uint8_t__hourCharges, UP_DAY_HOURS);
            uint32_t__heatRows++;
        }

        uint32_t__dailyCount++;
    }

    /* ---- charge sessions: the closed charge events, newest UP_STAT_SESSIONS -- */
    uint32_t uint32_t__sessSeen = 0u;

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_EVENTS, (uint32_t)sizeof(up_event_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_event_t up_event_t__event;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_event_t__event, (uint32_t)sizeof(up_event_t)))
            {
                break;
            }
            if ((up_event_t__event.uint8_t__code != UP_EV_CHARGE_DONE) &&
                (up_event_t__event.uint8_t__code != UP_EV_CHARGE_INCOMPLETE))
            {
                continue;
            }

            uint32_t uint32_t__slot = uint32_t__sessSeen % UP_STAT_SESSIONS;
            UINT32_T__A__SessDur[uint32_t__slot] = up_event_t__event.uint32_t__durS;
            UINT32_T__A__SessEnd[uint32_t__slot] = up_event_t__event.uint32_t__epoch;
            UINT8_T__A__SessDone[uint32_t__slot] = (up_event_t__event.uint8_t__code == UP_EV_CHARGE_DONE) ? 1u : 0u;
            uint32_t__sessSeen++;
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    uint32_t uint32_t__sessCount = (uint32_t__sessSeen < UP_STAT_SESSIONS) ? uint32_t__sessSeen : UP_STAT_SESSIONS;

    /* ---- median charge duration: the middle of every charge ever finished -- */
    uint32_t uint32_t__sumAll = 0u;
    uint32_t uint32_t__countAll = up_totals_t__totals->uint32_t__charges + up_totals_t__totals->uint32_t__incomplete;

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__sessCount; uint32_t__i++)
    {
        uint32_t uint32_t__slot = (uint32_t__sessSeen - 1u - uint32_t__i) % UP_STAT_SESSIONS;
        uint32_t uint32_t__value = UINT32_T__A__SessDur[uint32_t__slot];
        uint32_t uint32_t__j = uint32_t__i;

        while ((uint32_t__j > 0u) && (UINT32_T__A__SessDur[(uint32_t__sessSeen - 1u - (uint32_t__j - 1u)) % UP_STAT_SESSIONS] > uint32_t__value))
        {
            UINT32_T__A__SessDur[(uint32_t__sessSeen - 1u - uint32_t__j) % UP_STAT_SESSIONS] =
                UINT32_T__A__SessDur[(uint32_t__sessSeen - 1u - (uint32_t__j - 1u)) % UP_STAT_SESSIONS];
            uint32_t__j--;
        }
        UINT32_T__A__SessDur[(uint32_t__sessSeen - 1u - uint32_t__j) % UP_STAT_SESSIONS] = uint32_t__value;
        uint32_t__sumAll += uint32_t__value;
    }

    uint32_t uint32_t__medianS = 0u;
    if (uint32_t__sessCount > 0u)
    {
        uint32_t__medianS = UINT32_T__A__SessDur[(uint32_t__sessSeen - uint32_t__sessCount) % UP_STAT_SESSIONS];
        if (uint32_t__sessCount > 1u)
        {
            uint32_t uint32_t__middle = uint32_t__sessCount / 2u;
            uint32_t uint32_t__low = UINT32_T__A__SessDur[(uint32_t__sessSeen - uint32_t__sessCount + uint32_t__middle - 1u) % UP_STAT_SESSIONS];
            uint32_t uint32_t__high = UINT32_T__A__SessDur[(uint32_t__sessSeen - uint32_t__sessCount + uint32_t__middle) % UP_STAT_SESSIONS];
            uint32_t__medianS = (uint32_t__low + uint32_t__high) / 2u;
        }
    }

    uint32_t uint32_t__avgS = (uint32_t__countAll > 0u)
                            ? ((up_totals_t__totals->uint32_t__sumChargeS + (uint32_t__countAll / 2u)) / uint32_t__countAll)
                            : 0u;

    /* ---- the heat map's scale, so seven rows are comparable ----------------- */
    uint32_t uint32_t__heatMax = 0u;

    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__heatRows; uint32_t__i++)
    {
        for (uint32_t uint32_t__h = 0u; uint32_t__h < UP_DAY_HOURS; uint32_t__h++)
        {
            uint32_t uint32_t__value = UINT8_T__A__Heat[uint32_t__i][uint32_t__h];

            if (uint32_t__value > uint32_t__heatMax)
            {
                uint32_t__heatMax = uint32_t__value;
            }
        }
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"kpi\":{\"charges\":%lu,\"incomplete\":%lu,\"sumChargeS\":%lu,\"avgChargeS\":%lu,"
                         "\"minChargeS\":%lu,\"maxChargeS\":%lu,\"medianChargeS\":%lu,"
                         "\"outages\":%lu,\"outageS\":%lu,\"runS\":%lu,\"maxRunS\":%lu,\"inputS\":%lu,"
                         "\"chargeWh10\":%lu,\"runWh10\":%lu,\"peakI1\":%lu,\"peakI2\":%lu,"
                         "\"minV\":%u,\"maxV\":%u,\"imbEvents\":%lu,\"boots\":%lu,"
                         "\"coverageS\":%lu,\"days\":%lu,\"sessions\":%lu,\"maxChargesPerHour\":%lu,\"clock\":%u},",
                         (unsigned long)up_totals_t__totals->uint32_t__charges,
                         (unsigned long)up_totals_t__totals->uint32_t__incomplete,
                         (unsigned long)up_totals_t__totals->uint32_t__sumChargeS,
                         (unsigned long)uint32_t__avgS,
                         (unsigned long)up_totals_t__totals->uint32_t__minChargeS,
                         (unsigned long)up_totals_t__totals->uint32_t__maxChargeS,
                         (unsigned long)uint32_t__medianS,
                         (unsigned long)up_totals_t__totals->uint32_t__outages,
                         (unsigned long)up_totals_t__totals->uint32_t__outageS,
                         (unsigned long)up_totals_t__totals->uint32_t__runS,
                         (unsigned long)up_totals_t__totals->uint32_t__maxRunS,
                         (unsigned long)up_totals_t__totals->uint32_t__inputS,
                         (unsigned long)(up_totals_t__totals->uint32_t__chargeWh100 / 10u),
                         (unsigned long)(up_totals_t__totals->uint32_t__runWh100 / 10u),
                         (unsigned long)up_totals_t__totals->uint32_t__peakI1,
                         (unsigned long)up_totals_t__totals->uint32_t__peakI2,
                         (unsigned)up_totals_t__totals->uint16_t__minV,
                         (unsigned)up_totals_t__totals->uint16_t__maxV,
                         (unsigned long)up_totals_t__totals->uint32_t__imbEvents,
                         (unsigned long)up_totals_t__totals->uint32_t__boots,
                         (unsigned long)up_totals_t__totals->uint32_t__coverageS,
                         (unsigned long)UINT32_T__G__UpCachedDays,
                         (unsigned long)uint32_t__sessCount,
                         (unsigned long)uint32_t__heatMax,
                         bool__clock ? 1u : 0u);

    /* [EN] Both arrays are written OLDEST FIRST, the same direction every chart
            on the page draws, so the browser never has to reverse anything.
       [FA] هر دو آرایه «قدیمی‌ترین اول» نوشته می‌شوند، همان جهتی که همهٔ
            نمودارهای صفحه می‌کشند، تا مرورگر هیچ‌وقت مجبور به برعکس‌کردن نباشد. */
    func__UpHttp_JsonAdd("\"sessions\":[");
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__sessCount; uint32_t__i++)
    {
        uint32_t uint32_t__slot = (uint32_t__sessSeen - uint32_t__sessCount + uint32_t__i) % UP_STAT_SESSIONS;

        func__UpHttp_JsonAdd("%s[%lu,%u,%lu]", (uint32_t__i == 0u) ? "" : ",",
                             (unsigned long)UINT32_T__A__SessEnd[uint32_t__slot],
                             (unsigned)(UINT8_T__A__SessDone[uint32_t__slot]),
                             (unsigned long)UINT32_T__A__SessDur[uint32_t__slot]);
    }

    func__UpHttp_JsonAdd("],\"daily\":[");
    for (uint32_t uint32_t__i = uint32_t__dailyCount; uint32_t__i > 0u; uint32_t__i--)
    {
        func__UpHttp_JsonAdd("%s[%lu,%lu,%u,%lu]", (uint32_t__i == uint32_t__dailyCount) ? "" : ",",
                             (unsigned long)UINT32_T__A__DayEpoch[uint32_t__i - 1u],
                             (unsigned long)UINT32_T__A__DayCharges[uint32_t__i - 1u],
                             0u,
                             (unsigned long)UINT32_T__A__DayRunS[uint32_t__i - 1u]);
    }

    func__UpHttp_JsonAdd("],\"heatDays\":[");
    for (uint32_t uint32_t__i = uint32_t__heatRows; uint32_t__i > 0u; uint32_t__i--)
    {
        func__UpHttp_JsonAdd("%s%lu", (uint32_t__i == uint32_t__heatRows) ? "" : ",",
                             (unsigned long)UINT32_T__A__DayEpoch[uint32_t__i - 1u]);
    }

    func__UpHttp_JsonAdd("],\"heat\":[");
    for (uint32_t uint32_t__row = 0u; uint32_t__row < uint32_t__heatRows; uint32_t__row++)
    {
        uint32_t uint32_t__pick = uint32_t__heatRows - 1u - uint32_t__row;

        func__UpHttp_JsonAdd("%s[", (uint32_t__row == 0u) ? "" : ",");
        for (uint32_t uint32_t__h = 0u; uint32_t__h < UP_DAY_HOURS; uint32_t__h++)
        {
            func__UpHttp_JsonAdd("%s%u", (uint32_t__h == 0u) ? "" : ",",
                                 (unsigned)UINT8_T__A__Heat[uint32_t__pick][uint32_t__h]);
        }
        func__UpHttp_JsonAdd("]");
    }

    func__UpHttp_JsonAdd("]}");
    func__UpHttp_JsonSend(200);
}

/* ==================== Admin: users / مدیر: کاربران ==================== */
/**
 * @brief  [EN] Parse a role token from a form value.
 *         [FA] تبدیل توکن نقش از یک مقدار فرم.
 * @param  char__text [EN] "viewer" | "operator" | "admin" / [FA] نام نقش
 * @param  uint8_t__out [EN] UP_ROLE_* / [FA] نقش
 * @return [EN] true when recognised / [FA] در صورت تشخیص true
 */
static bool func__UpHttp_ParseRole(const char *char__text, uint8_t *uint8_t__out)
{
    if (strcmp(char__text, "admin") == 0)
    {
        *uint8_t__out = UP_ROLE_ADMIN;
        return true;
    }
    if (strcmp(char__text, "operator") == 0)
    {
        *uint8_t__out = UP_ROLE_OPERATOR;
        return true;
    }
    if (strcmp(char__text, "viewer") == 0)
    {
        *uint8_t__out = UP_ROLE_VIEWER;
        return true;
    }

    return false;
}

/**
 * @brief  [EN] Is a login name acceptable? Letters, digits, dot, dash and
 *              underscore only, so it can travel in a form value and be written
 *              to the action log without quoting rules.
 *         [FA] آیا نام ورود قابل قبول است؟ فقط حرف، رقم، نقطه، خط تیره و زیرخط،
 *              تا هم در مقدار فرم سفر کند و هم بی‌قاعدهٔ کوتیشن در گزارش
 *              اقدامات بنشیند.
 * @param  char__name [EN] candidate / [FA] نام پیشنهادی
 * @return [EN] true when acceptable / [FA] در صورت قبول true
 */
static bool func__UpHttp_NameOk(const char *char__name)
{
    size_t size_t__length = strlen(char__name);

    if ((size_t__length < 3u) || (size_t__length > UP_NAME_MAX))
    {
        return false;
    }

    for (size_t size_t__i = 0u; size_t__i < size_t__length; size_t__i++)
    {
        char char__c = char__name[size_t__i];
        bool bool__ok = (((char__c >= 'a') && (char__c <= 'z')) ||
                         ((char__c >= 'A') && (char__c <= 'Z')) ||
                         ((char__c >= '0') && (char__c <= '9')) ||
                         (char__c == '.') || (char__c == '-') || (char__c == '_'));
        if (!bool__ok)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief  [EN] GET /api/admin/users - the account list. A digest never leaves
 *              the device, so this is names, roles and timestamps only.
 *         [FA] مسیر GET /api/admin/users - فهرست حساب‌ها. چکیده هرگز از دستگاه
 *              بیرون نمی‌رود، پس این فقط نام، نقش و زمان‌هاست.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminUsers(void)
{
    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"u\":[");
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__UserCount; uint8_t__i++)
    {
        up_user_t *up_user_t__user = &UP_USER_T__A__Users[uint8_t__i];

        func__UpHttp_JsonAdd("%s[\"%s\",\"%s\",%lu,%lu,%u]", (uint8_t__i == 0u) ? "" : ",",
                             up_user_t__user->char__name,
                             func__UpHttp_RoleName(up_user_t__user->uint8_t__role),
                             (unsigned long)up_user_t__user->uint32_t__createdEpoch,
                             (unsigned long)up_user_t__user->uint32_t__lastLoginEpoch,
                             (unsigned)(up_user_t__user->uint8_t__mustChange ? 1u : 0u));
    }
    func__UpHttp_JsonAdd("]}");
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] POST /api/admin/user (action, u, p, r) - add, delete, re-password
 *              or re-role. Two refusals are deliberate: nobody may delete
 *              themselves (an accidental tap would lock the panel for everyone)
 *              and nobody may remove or demote the LAST admin (that panel has
 *              no way back except re-flashing).
 *         [FA] مسیر POST /api/admin/user (اقدام، نام، گذرواژه، نقش) - افزودن،
 *              حذف، تغییر گذرواژه یا نقش. دو رد عمدی است: هیچ‌کس نمی‌تواند
 *              خودش را حذف کند (یک ضربهٔ اشتباه پنل را برای همه قفل می‌کند) و
 *              هیچ‌کس نمی‌تواند آخرین مدیر را حذف یا تنزل دهد (آن پنل راه
 *              برگشتی جز فلش دوباره ندارد).
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminUserPost(void)
{
    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }
    if ((!UP_WEBSERVER_T__G__Server.hasArg("action")) || (!UP_WEBSERVER_T__G__Server.hasArg("u")))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }

    String up_string_t__action = UP_WEBSERVER_T__G__Server.arg("action");
    String up_string_t__name = UP_WEBSERVER_T__G__Server.arg("u");
    int8_t int8_t__index = func__UpAuth_FindUser(up_string_t__name.c_str());

    if (up_string_t__action == "add")
    {
        uint8_t uint8_t__role = 0u;

        if ((!UP_WEBSERVER_T__G__Server.hasArg("p")) || (!UP_WEBSERVER_T__G__Server.hasArg("r")))
        {
            func__UpHttp_SendErr(200, "missing");
            return;
        }
        if (!func__UpHttp_NameOk(up_string_t__name.c_str()))
        {
            func__UpHttp_SendErr(200, "bad name");
            return;
        }
        if (int8_t__index >= 0)
        {
            func__UpHttp_SendErr(200, "exists");
            return;
        }
        if (!func__UpHttp_ParseRole(UP_WEBSERVER_T__G__Server.arg("r").c_str(), &uint8_t__role))
        {
            func__UpHttp_SendErr(200, "bad role");
            return;
        }
        if (UP_WEBSERVER_T__G__Server.arg("p").length() < 6u)
        {
            func__UpHttp_SendErr(200, "password too short");
            return;
        }
        if (!func__UpAuth_AddUser(up_string_t__name.c_str(), UP_WEBSERVER_T__G__Server.arg("p").c_str(),
                                  uint8_t__role, true))
        {
            func__UpHttp_SendErr(200, "table full");
            return;
        }
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "user_add");
    }
    else if (up_string_t__action == "del")
    {
        if (int8_t__index < 0)
        {
            func__UpHttp_SendErr(200, "unknown user");
            return;
        }
        if (strcmp(up_string_t__name.c_str(), func__UpHttp_CurrentUserName()) == 0)
        {
            func__UpHttp_SendErr(200, "cannot delete yourself");
            return;
        }
        if ((UP_USER_T__A__Users[(uint8_t)int8_t__index].uint8_t__role == UP_ROLE_ADMIN) &&
            (func__UpAuth_CountRole(UP_ROLE_ADMIN) <= 1u))
        {
            func__UpHttp_SendErr(200, "last admin");
            return;
        }
        if (!func__UpAuth_DeleteUser(up_string_t__name.c_str()))
        {
            func__UpHttp_SendErr(200, "delete failed");
            return;
        }
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "user_del");
    }
    else if (up_string_t__action == "pass")
    {
        if (int8_t__index < 0)
        {
            func__UpHttp_SendErr(200, "unknown user");
            return;
        }
        if (UP_WEBSERVER_T__G__Server.arg("p").length() < 6u)
        {
            func__UpHttp_SendErr(200, "password too short");
            return;
        }
        (void)func__UpAuth_SetPassword(up_string_t__name.c_str(), UP_WEBSERVER_T__G__Server.arg("p").c_str(), true);
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "user_pass");
    }
    else if (up_string_t__action == "role")
    {
        uint8_t uint8_t__role = 0u;

        if (int8_t__index < 0)
        {
            func__UpHttp_SendErr(200, "unknown user");
            return;
        }
        if (!func__UpHttp_ParseRole(UP_WEBSERVER_T__G__Server.arg("r").c_str(), &uint8_t__role))
        {
            func__UpHttp_SendErr(200, "bad role");
            return;
        }
        if ((UP_USER_T__A__Users[(uint8_t)int8_t__index].uint8_t__role == UP_ROLE_ADMIN) &&
            (uint8_t__role != UP_ROLE_ADMIN) && (func__UpAuth_CountRole(UP_ROLE_ADMIN) <= 1u))
        {
            func__UpHttp_SendErr(200, "last admin");
            return;
        }
        (void)func__UpAuth_SetRole(up_string_t__name.c_str(), uint8_t__role);
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "user_role");
    }
    else
    {
        func__UpHttp_SendErr(200, "unknown action");
        return;
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1}");
    func__UpHttp_JsonSend(200);
}

/* ============ Admin: saved networks / مدیر: شبکه‌های ذخیره‌شده ============ */
/**
 * @brief  [EN] GET /api/admin/network - the four saved networks, which one is
 *              active, and what the radio is doing right now. Passwords are NOT
 *              in this reply: the page needs to know whether one is set, and a
 *              password that can be read back is a password that leaks through
 *              every screenshot, every support call and every browser cache.
 *         [FA] مسیر GET /api/admin/network - چهار شبکهٔ ذخیره‌شده، اینکه کدام
 *              فعال است و رادیو همین حالا چه می‌کند. رمزها در این پاسخ نیستند:
 *              صفحه فقط باید بداند رمزی تنظیم شده یا نه، و رمزی که قابل خواندن
 *              باشد از هر اسکرین‌شات، هر تماس پشتیبانی و هر کش مرورگر بیرون
 *              می‌ریزد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminNetworkGet(void)
{
    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }

    String up_string_t__nowSsid = WiFi.SSID();
    String up_string_t__nowIp = WiFi.localIP().toString();

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1,\"active\":%u,\"count\":%u,",
                         (unsigned)func__UpSettings_ActiveIndex(),
                         (unsigned)func__UpSettings_Count());
    func__UpHttp_JsonAdd("\"cur\":{\"online\":%u,\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d},",
                         (UPPANEL_STATE_T__G__State.bool__staUp ? 1u : 0u),
                         up_string_t__nowSsid.c_str(),
                         up_string_t__nowIp.c_str(),
                         (int)WiFi.RSSI());
    func__UpHttp_JsonAdd("\"items\":[");

    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_NET_PROFILE_MAX; uint8_t__i++)
    {
        up_net_profile_t *up_net_profile_t__row = func__UpSettings_Profile(uint8_t__i);

        if (uint8_t__i > 0u)
        {
            func__UpHttp_JsonAdd(",");
        }
        if (up_net_profile_t__row == NULL)
        {
            func__UpHttp_JsonAdd("{\"i\":%u,\"s\":\"\",\"a\":0,\"p\":0,\"u\":0}",
                                 (unsigned)uint8_t__i);
            continue;
        }

        func__UpHttp_JsonAdd("{\"i\":%u,\"s\":\"%s\",\"a\":%u,\"p\":%u,\"u\":%lu}",
                             (unsigned)uint8_t__i,
                             up_net_profile_t__row->char__ssid,
                             (uint8_t__i == func__UpSettings_ActiveIndex()) ? 1u : 0u,
                             (up_net_profile_t__row->char__pass[0] != '\0') ? 1u : 0u,
                             (unsigned long)up_net_profile_t__row->uint32_t__lastUsedS);
    }

    func__UpHttp_JsonAdd("]}");
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] POST /api/admin/network - the admin changes which board this
 *              panel looks at. Admin only, no exception: the network card is
 *              the one screen that can make the panel leave the machine it is
 *              supposed to be watching.
 *              `action=save` with `ssid` (and `pass`, omitted to keep the
 *              stored one), `action=use` with `i`, `action=del` with `i`.
 *              The active row is never deletable, and saving into the active
 *              row reconnects immediately - the whole point of editing an SSID
 *              is that the panel then joins it.
 *         [FA] مسیر POST /api/admin/network - مدیر عوض می‌کند که این پنل کدام
 *              برد را ببیند. فقط مدیر، بدون استثنا: کارت شبکه همان صفحه‌ای است
 *              که می‌تواند پنل را از ماشینی که باید تماشا کند جدا کند.
 *              `action=save` با `ssid` (و `pass` که اگر نیاید رمز ذخیره‌شده
 *              می‌ماند)، `action=use` با `i` و `action=del` با `i`.
 *              ردیف فعال هرگز حذف‌شدنی نیست و ذخیره روی ردیف فعال بلافاصله
 *              اتصال را نو می‌کند - تمام هدف ویرایش یک نام شبکه همین است که پنل
 *              بعدش به آن بپیوندد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminNetworkPost(void)
{
    uint8_t uint8_t__index = 0u;

    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }
    if (!UP_WEBSERVER_T__G__Server.hasArg("action"))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }

    String up_string_t__action = UP_WEBSERVER_T__G__Server.arg("action");

    if (up_string_t__action == "save")
    {
        bool bool__activeRow = false;
        int8_t int8_t__row = 0;

        if (!UP_WEBSERVER_T__G__Server.hasArg("ssid"))
        {
            func__UpHttp_SendErr(200, "missing");
            return;
        }

        /* [EN] Both arguments are copied into named objects first. Reading
                c_str() straight out of arg() gives a pointer into a temporary
                that dies at the end of the expression - the panel would then
                validate a name that no longer exists. This exact bug was
                caught by the host test's AddressSanitizer build, which is the
                whole reason that build exists.
           [FA] هر دو ورودی اول در شیء نام‌دار کپی می‌شوند. خواندن c_str() مستقیم
                از arg() اشاره‌گری به یک موقت می‌دهد که در پایان عبارت می‌میرد -
                آن‌وقت پنل نامی را بررسی می‌کند که دیگر وجود ندارد. همین اشکال
                دقیقاً با بیلد AddressSanitizer تست میزبان گرفته شد، که تمام دلیل
                وجود آن بیلد است. */
        String up_string_t__ssid = UP_WEBSERVER_T__G__Server.arg("ssid");
        String up_string_t__pass = UP_WEBSERVER_T__G__Server.arg("pass");
        const char *char__ssid = up_string_t__ssid.c_str();
        const char *char__pass = up_string_t__pass.c_str();
        bool bool__keepPass = (UP_WEBSERVER_T__G__Server.hasArg("pass") == false);

        if (!func__UpSettings_SsidOk(char__ssid))
        {
            func__UpHttp_SendErr(200, "bad network name");
            return;
        }
        if ((!bool__keepPass) && (!func__UpSettings_PassOk(char__pass)))
        {
            func__UpHttp_SendErr(200, "bad password");
            return;
        }

        int8_t__row = func__UpSettings_Put(char__ssid, char__pass, bool__keepPass);
        if (int8_t__row < 0)
        {
            func__UpHttp_SendErr(200, "table full");
            return;
        }

        bool__activeRow = ((uint8_t)int8_t__row == func__UpSettings_ActiveIndex());

        if (!func__UpSettings_Save())
        {
            func__UpHttp_SendErr(200, "flash write failed");
            return;
        }

        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "net_save");

        if (bool__activeRow)
        {
            func__UpLink_Reconnect();
        }

        func__UpHttp_JsonReset();
        func__UpHttp_JsonAdd("{\"ok\":1,\"i\":%d}", (int)int8_t__row);
        func__UpHttp_JsonSend(200);
        return;
    }

    /* [EN] The other two actions both name a row, so both are parsed and bounds
            checked in one place instead of twice.
       [FA] دو اقدام دیگر هر دو یک ردیف را نام می‌برند، پس هر دو یک‌جا و یک‌بار
            خوانده و کران‌بررسی می‌شوند، نه دو بار. */
    if (!UP_WEBSERVER_T__G__Server.hasArg("i"))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }

    int int__row = UP_WEBSERVER_T__G__Server.arg("i").toInt();

    if ((int__row < 0) || (int__row >= (int)UP_NET_PROFILE_MAX))
    {
        func__UpHttp_SendErr(200, "bad index");
        return;
    }
    uint8_t__index = (uint8_t)int__row;

    if (up_string_t__action == "use")
    {
        if (func__UpSettings_Profile(uint8_t__index) == NULL)
        {
            func__UpHttp_SendErr(200, "empty slot");
            return;
        }
        if (!func__UpSettings_SetActive(uint8_t__index))
        {
            func__UpHttp_SendErr(200, "flash write failed");
            return;
        }

        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "net_use");
        func__UpLink_Reconnect();
    }
    else if (up_string_t__action == "del")
    {
        if (uint8_t__index == func__UpSettings_ActiveIndex())
        {
            func__UpHttp_SendErr(200, "the active network cannot be deleted");
            return;
        }
        if (!func__UpSettings_Delete(uint8_t__index))
        {
            func__UpHttp_SendErr(200, "nothing to delete");
            return;
        }

        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "net_del");
    }
    else
    {
        func__UpHttp_SendErr(200, "unknown action");
        return;
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1}");
    func__UpHttp_JsonSend(200);
}

/* ==================== Admin: the only writes / مدیر: تنها نوشتنی‌ها ==================== */
/**
 * @brief  [EN] POST /api/admin/action (a) - the ONLY route in this program that
 *              puts anything on the board's wire. `charger1_off`, `charger1_on`,
 *              `charger2_off`, `charger2_on` (parameters 11/12, operator or
 *              above) and `purge` (this panel's own history, admin only).
 *              The panel records its own event for each action, so the user's
 *              timeline shows the moment somebody cut a charger, whoever did it.
 *         [FA] مسیر POST /api/admin/action (a) - تنها مسیری در این برنامه که
 *              چیزی روی سیم برد می‌گذارد. قطع/وصل شارژر ۱ و ۲ (پارامتر ۱۱/۱۲،
 *              اپراتور و بالاتر) و پاک‌سازی تاریخچهٔ خود پنل (فقط مدیر).
 *              پنل برای هر اقدام رویداد خودش را ثبت می‌کند، تا خط زمانی کاربر
 *              لحظه‌ای را که کسی شارژری را قطع کرده نشان دهد، هر کس که بوده.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminActionPost(void)
{
    if ((!UP_WEBSERVER_T__G__Server.hasArg("a")) || (!UP_WEBSERVER_T__G__Server.hasArg("cs")))
    {
        func__UpHttp_SendErr(200, "missing");
        return;
    }
    if (!func__UpHttp_RequireRole(UP_ROLE_OPERATOR))
    {
        return;
    }

    String up_string_t__action = UP_WEBSERVER_T__G__Server.arg("a");

    /* [EN] The browser sends a nonce it generated before showing the confirm
       dialog (cs = confirmation stamp). The panel refuses an action whose stamp
       it has already honoured, so a double tap or a replayed request cannot cut
       a charger twice - the kind of help an operator with wet hands needs.
       [FA] مرورگر یک نانس می‌فرستد که پیش از نمایش پنجرهٔ تأیید ساخته
       (cs = مهر تأیید). پنل اقدامی را که مهرش را قبلاً پذیرفته رد می‌کند، تا
       دو بار زدن یا تکرار درخواست نتواند شارژر را دو بار قطع کند - همان کمکی
       که اپراتور با دست‌های خیس لازم دارد. */
    String up_string_t__stamp = UP_WEBSERVER_T__G__Server.arg("cs");
    if ((up_string_t__stamp.length() > 0u) && (up_string_t__stamp == UPPANEL_STATE_T__G__State.char__LastActionStamp))
    {
        func__UpHttp_SendErr(200, "already done");
        return;
    }
    (void)snprintf(UPPANEL_STATE_T__G__State.char__LastActionStamp,
                   sizeof(UPPANEL_STATE_T__G__State.char__LastActionStamp), "%s", up_string_t__stamp.c_str());

    if (up_string_t__action == "charger1_off")
    {
        (void)func__UpLink_QueueParam(UP_PARAM_CHG1_ENABLE, 0);
        (void)func__UpHistory_WriteEvent(UP_EV_CHG_CUT, 1u, UP_SEV_WARN, 0u, 0u, 0u);
    }
    else if (up_string_t__action == "charger1_on")
    {
        (void)func__UpLink_QueueParam(UP_PARAM_CHG1_ENABLE, 1);
        (void)func__UpHistory_WriteEvent(UP_EV_CHG_ON, 1u, UP_SEV_INFO, 0u, 0u, 0u);
    }
    else if (up_string_t__action == "charger2_off")
    {
        (void)func__UpLink_QueueParam(UP_PARAM_CHG2_ENABLE, 0);
        (void)func__UpHistory_WriteEvent(UP_EV_CHG_CUT, 2u, UP_SEV_WARN, 0u, 0u, 0u);
    }
    else if (up_string_t__action == "charger2_on")
    {
        (void)func__UpLink_QueueParam(UP_PARAM_CHG2_ENABLE, 1);
        (void)func__UpHistory_WriteEvent(UP_EV_CHG_ON, 2u, UP_SEV_INFO, 0u, 0u, 0u);
    }
    else if (up_string_t__action == "purge")
    {
        if (!func__UpAuth_AtLeast(UP_ROLE_ADMIN))
        {
            func__UpHttp_SendErr(403, "forbidden");
            return;
        }
        (void)func__UpStore_PurgeAll();
        (void)func__UpHistory_WriteEvent(UP_EV_PURGE, 0u, UP_SEV_INFO, 0u, 0u, 0u);
        UINT32_T__G__UpCachedSamples = 0u;
        UINT32_T__G__UpCachedEvents = 0u;
        UINT32_T__G__UpCountersMs = 0u;
    }
    else
    {
        func__UpHttp_SendErr(200, "unknown action");
        return;
    }

    func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), up_string_t__action.c_str());

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"ok\":1}");
    func__UpHttp_JsonSend(200);
}

/**
 * @brief  [EN] GET /api/admin/audit?limit=N - who did what, newest first.
 *         [FA] مسیر GET /api/admin/audit?limit=N - چه کسی چه کرد، از جدید به قدیم.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_AdminAudit(void)
{
    uint32_t uint32_t__limit = 40u;

    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }
    if (UP_WEBSERVER_T__G__Server.hasArg("limit"))
    {
        long long__limit = strtol(UP_WEBSERVER_T__G__Server.arg("limit").c_str(), NULL, 10);
        if ((long__limit >= 1) && (long__limit <= (long)UP_AUDIT_MAX))
        {
            uint32_t__limit = (uint32_t)long__limit;
        }
    }

    func__UpHttp_JsonReset();
    func__UpHttp_JsonAdd("{\"n\":%lu,\"a\":[", (unsigned long)uint32_t__limit);
    for (uint32_t uint32_t__i = 0u; uint32_t__i < uint32_t__limit; uint32_t__i++)
    {
        up_audit_t *up_audit_t__row = func__UpAuth_AuditAt((uint8_t)uint32_t__i);

        if (up_audit_t__row == NULL)
        {
            break;
        }
        func__UpHttp_JsonAdd("%s[%lu,\"%s\",\"%s\"]", (uint32_t__i == 0u) ? "" : ",",
                             (unsigned long)up_audit_t__row->uint32_t__epoch,
                             up_audit_t__row->char__user,
                             up_audit_t__row->char__action);
    }
    func__UpHttp_JsonAdd("]}");
    func__UpHttp_JsonSend(200);
}

/* ==================== GET /api/export / برون‌بری ==================== */
/**
 * @brief  [EN] Stream the stored samples out as CSV or JSON. Streamed chunk by
 *              chunk on purpose: the file is allowed to be far larger than any
 *              buffer this device could ever hold.
 *         [FA] بیرون ریختن نمونه‌های ذخیره‌شده به‌صورت CSV یا JSON. عمداً تکه‌به‌تکه:
 *              فایل می‌تواند بسیار بزرگ‌تر از هر بافری باشد که این دستگاه
 *              بتواند داشته باشد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Export(void)
{
    static char CHAR__A__Chunk[UP_CSV_CHUNK];
    up_ring_reader_t up_ring_reader_t__reader;

    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }

    bool bool__csv = true;
    if (UP_WEBSERVER_T__G__Server.hasArg("what"))
    {
        bool__csv = (UP_WEBSERVER_T__G__Server.arg("what") != "json");
    }

    UP_WEBSERVER_T__G__Server.sendHeader("Content-Disposition",
                                         bool__csv ? "attachment; filename=user_panel_history.csv"
                                                   : "attachment; filename=user_panel_history.json");
    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    UP_WEBSERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    UP_WEBSERVER_T__G__Server.send(200, bool__csv ? "text/csv; charset=utf-8" : "application/json; charset=utf-8", "");

    if (bool__csv)
    {
        /* [EN] Human-readable first, machine-readable second: the clock column
           the operator recognises comes first, and the panel's own monotonic
           second stays next to it so a spreadsheet and the firmware can always
           be tied back together.
           [FA] اول خوانا برای انسان، بعد خوانا برای ماشین: ستون ساعتی که
           اپراتور می‌شناسد اول است و ثانیهٔ یکنوای خود پنل کنارش می‌ماند تا
           صفحهٔ گسترده و فرم‌ور همیشه به هم قابل ارجاع بمانند. */
        (void)snprintf(CHAR__A__Chunk, sizeof(CHAR__A__Chunk),
                       "epoch,abs_s,vin_mv,v24_mv,v12_mv,i1_ma,i2_ma,duty1,duty2,state1,state2,flags,flags2\r\n");
        UP_WEBSERVER_T__G__Server.sendContent(CHAR__A__Chunk);
    }
    else
    {
        UP_WEBSERVER_T__G__Server.sendContent("{\"n\":");
        (void)snprintf(CHAR__A__Chunk, sizeof(CHAR__A__Chunk), "%lu", (unsigned long)func__UpStore_SampleCount());
        UP_WEBSERVER_T__G__Server.sendContent(CHAR__A__Chunk);
        UP_WEBSERVER_T__G__Server.sendContent(",\"list\":[");
    }

    func__UpStore_ReaderInit(&up_ring_reader_t__reader);
    uint32_t uint32_t__index = 0u;

    if (func__UpStore_ReaderOpen(&up_ring_reader_t__reader, UP_F_SAMPLES, (uint32_t)sizeof(up_sample_t)))
    {
        for (uint32_t uint32_t__i = 0u; uint32_t__i < up_ring_reader_t__reader.uint32_t__count; uint32_t__i++)
        {
            up_sample_t up_sample_t__sample;

            if (!func__UpStore_ReaderNext(&up_ring_reader_t__reader, &up_sample_t__sample, (uint32_t)sizeof(up_sample_t)))
            {
                break;
            }

            /* [EN] One row formatter, the store's, so this text and the record
               layout cannot drift apart. The two extra bytes are the line
               ending (CSV) or the closing bracket (JSON).
               [FA] یک قالب‌بندی ردیف، همان‌که در store است، تا این متن و چیدمان
               رکورد از هم جدا نیفتند. آن دو بایت اضافه یعنی پایان خط (CSV) یا
               براکت بسته (JSON). */
            if (bool__csv)
            {
                CHAR__A__Chunk[0] = '\0';
                func__UpStore_CsvRow(&up_sample_t__sample, CHAR__A__Chunk,
                                     (uint32_t)sizeof(CHAR__A__Chunk) - 2u);
                strncat(CHAR__A__Chunk, "\r\n", sizeof(CHAR__A__Chunk) - strlen(CHAR__A__Chunk) - 1u);
            }
            else
            {
                size_t size_t__used;

                CHAR__A__Chunk[0] = (uint32_t__index == 0u) ? '[' : ',';
                CHAR__A__Chunk[1] = '\0';
                size_t__used = strlen(CHAR__A__Chunk);
                func__UpStore_CsvRow(&up_sample_t__sample, &CHAR__A__Chunk[size_t__used],
                                     (uint32_t)(sizeof(CHAR__A__Chunk) - size_t__used - 2u));
                strncat(CHAR__A__Chunk, "]", sizeof(CHAR__A__Chunk) - strlen(CHAR__A__Chunk) - 1u);
            }

            UP_WEBSERVER_T__G__Server.sendContent(CHAR__A__Chunk);
            uint32_t__index++;
        }
        func__UpStore_ReaderClose(&up_ring_reader_t__reader);
    }

    if (!bool__csv)
    {
        UP_WEBSERVER_T__G__Server.sendContent("]}");
    }
    UP_WEBSERVER_T__G__Server.sendContent("");

    func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), bool__csv ? "export_csv" : "export_json");
}

/* ==================== GET /api/admin/report.xlsx / گزارش اکسل ==================== */
/* ==================== Report sink ==================== */
/**
 * @brief  [EN] Where the workbook writer pushes bytes: straight into the HTTP
 *              client. The writer knows nothing about HTTP, so this tiny
 *              function is the whole bridge between the two.
 *         [FA] جایی که نویسندهٔ کتاب بایت‌ها را به آن می‌راند: مستقیم به کلاینت
 *              HTTP. نویسنده هیچ‌چیز از HTTP نمی‌داند، پس همین تابع کوچک تمام
 *              پل میان این دو است.
 * @param  char__data [EN] bytes / [FA] بایت‌ها
 * @param  uint32_t__len [EN] how many / [FA] چند تا
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_ReportSink(const char *char__data, uint32_t uint32_t__len)
{
    /* [EN] The two-argument form of sendContent is the binary-safe one: the zip
       bytes contain NULs, and strlen would stop at the first one.
       [FA] شکل دو آرگومانی sendContent همان شکل ایمن برای دادهٔ دودویی است:
       بایت‌های زیپ صفر بایت دارند و strlen روی اولین‌شان می‌ایستد. */
    (void)UP_WEBSERVER_T__G__Server.sendContent(char__data, (size_t)uint32_t__len);
}

/* ==================== Report range ==================== */
/**
 * @brief  [EN] Read "?days=" and tell the report what range to write. Anything
 *              missing, zero or unparseable means "everything": the operator
 *              asked for no filter, not for an error page.
 *         [FA] خواندن «?days=» و گفتن بازه به گزارش. هر چیز غایب، صفر یا
 *              ناخوانا یعنی «همه‌چیز»: اپراتور فیلتری نخواسته، نه صفحهٔ خطا.
 * @return [EN] days, 0 = everything / [FA] تعداد روز، صفر = همه
 */
static uint32_t func__UpHttp_ReportDays(void)
{
    if (!UP_WEBSERVER_T__G__Server.hasArg("days"))
    {
        return UP_REPORT_DAYS_ALL;
    }

    long long__days = strtol(UP_WEBSERVER_T__G__Server.arg("days").c_str(), NULL, 10);
    if ((long__days < 1L) || (long__days > (long)UP_REPORT_MAX_DAYS))
    {
        return UP_REPORT_DAYS_ALL;
    }

    return (uint32_t)long__days;
}

/* ==================== Report handler ==================== */
/**
 * @brief  [EN] GET /api/admin/report.xlsx?days=N - stream a real Excel workbook
 *              of the stored history. Admin only: this is every number the panel
 *              has, and a viewer's account is read-only by design.
 *         [FA] مسیر GET /api/admin/report.xlsx?days=N - بیرون دادن یک کتاب اکسل
 *              واقعی از تاریخچهٔ ذخیره‌شده. فقط مدیر: این همهٔ اعداد پنل است و
 *              حساب بیننده عمداً فقط-خواندنی است.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Report(void)
{
    if (!func__UpHttp_RequireRole(UP_ROLE_ADMIN))
    {
        return;
    }

    uint32_t uint32_t__days = func__UpHttp_ReportDays();

    /* [EN] The clock is read once, here, so every sheet of one report agrees on
       what "now" is even when the report takes a few seconds to stream.
       [FA] ساعت یک‌بار همین‌جا خوانده می‌شود تا همهٔ برگه‌های یک گزارش روی
       «حالا» توافق داشته باشند، حتی اگر بیرون‌دادن گزارش چند ثانیه طول بکشد. */
    func__UpHttp_RefreshCounters((uint32_t)millis());
    func__UpReport_RangeSet(uint32_t__days);

    UP_WEBSERVER_T__G__Server.sendHeader("Content-Disposition",
                                         "attachment; filename=user_panel_report.xlsx");
    UP_WEBSERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    UP_WEBSERVER_T__G__Server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    UP_WEBSERVER_T__G__Server.send(200,
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "");

    (void)func__UpReport_Write(func__UpHttp_ReportSink);

    /* [EN] Chunked transfer ends with an empty chunk - without it the browser
       keeps the connection open waiting for the file that already ended.
       [FA] انتقال تکه‌ای با یک تکهٔ خالی تمام می‌شود - بدون آن مرورگر اتصال را
       باز نگه می‌دارد و منتظر فایلی می‌ماند که تمام شده است. */
    UP_WEBSERVER_T__G__Server.sendContent("");

    if (uint32_t__days == UP_REPORT_DAYS_ALL)
    {
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "report_all");
    }
    else
    {
        func__UpAuth_LogAction(func__UpHttp_CurrentUserName(), "report_days");
    }
}

/* ==================== Routes / مسیرها ==================== */
/**
 * @brief  [EN] Register every route, and tell the server which request header
 *              this panel reads. The ESP8266 web server keeps no headers it was
 *              not asked for - without the collectHeaders line every request
 *              looks anonymous and nobody could ever log in.
 *         [FA] ثبت همهٔ مسیرها و گفتن به سرور که این پنل کدام سرصفحهٔ درخواست
 *              را می‌خواند. سرور وب ESP8266 هیچ سرصفحه‌ای را که نخواسته باشند
 *              نگه نمی‌دارد - بدون خط collectHeaders هر درخواست ناشناس به نظر
 *              می‌رسد و هیچ‌کس هرگز نمی‌تواند وارد شود.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpHttp_Begin(void)
{
    static const char *const CHAR__A__CollectedHeaders[] = { "Cookie" };

    UP_WEBSERVER_T__G__Server.collectHeaders(CHAR__A__CollectedHeaders, 1u);

    UP_WEBSERVER_T__G__Server.on("/", HTTP_GET, func__UpHttp_Root);
    UP_WEBSERVER_T__G__Server.on("/app.css", HTTP_GET, func__UpHttp_Css);
    UP_WEBSERVER_T__G__Server.on("/app.js", HTTP_GET, func__UpHttp_Js);
    UP_WEBSERVER_T__G__Server.on("/f.css", HTTP_GET, func__UpHttp_Font);
    UP_WEBSERVER_T__G__Server.on("/version", HTTP_GET, func__UpHttp_Version);

    UP_WEBSERVER_T__G__Server.on("/api/me", HTTP_GET, func__UpHttp_Me);
    UP_WEBSERVER_T__G__Server.on("/api/login", HTTP_POST, func__UpHttp_Login);
    UP_WEBSERVER_T__G__Server.on("/api/logout", HTTP_POST, func__UpHttp_Logout);
    UP_WEBSERVER_T__G__Server.on("/api/pass", HTTP_POST, func__UpHttp_Pass);
    UP_WEBSERVER_T__G__Server.on("/api/clock", HTTP_POST, func__UpHttp_ClockPost);

    UP_WEBSERVER_T__G__Server.on("/api/live", HTTP_GET, func__UpHttp_Live);
    UP_WEBSERVER_T__G__Server.on("/api/series", HTTP_GET, func__UpHttp_Series);
    UP_WEBSERVER_T__G__Server.on("/api/events", HTTP_GET, func__UpHttp_Events);
    UP_WEBSERVER_T__G__Server.on("/api/stats", HTTP_GET, func__UpHttp_Stats);

    UP_WEBSERVER_T__G__Server.on("/api/admin/clock", HTTP_GET, func__UpHttp_AdminClockGet);
    UP_WEBSERVER_T__G__Server.on("/api/admin/users", HTTP_GET, func__UpHttp_AdminUsers);
    UP_WEBSERVER_T__G__Server.on("/api/admin/user", HTTP_POST, func__UpHttp_AdminUserPost);
    UP_WEBSERVER_T__G__Server.on("/api/admin/network", HTTP_GET, func__UpHttp_AdminNetworkGet);
    UP_WEBSERVER_T__G__Server.on("/api/admin/network", HTTP_POST, func__UpHttp_AdminNetworkPost);
    UP_WEBSERVER_T__G__Server.on("/api/admin/action", HTTP_POST, func__UpHttp_AdminActionPost);
    UP_WEBSERVER_T__G__Server.on("/api/admin/audit", HTTP_GET, func__UpHttp_AdminAudit);

    UP_WEBSERVER_T__G__Server.on("/api/export", HTTP_GET, func__UpHttp_Export);
    UP_WEBSERVER_T__G__Server.on("/api/admin/report.xlsx", HTTP_GET, func__UpHttp_Report);

    UP_WEBSERVER_T__G__Server.begin();
}

#endif /* UP_HTTP_H */
