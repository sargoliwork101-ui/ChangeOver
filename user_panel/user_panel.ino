/**
 * @file    user_panel.ino
 * @brief   [EN] ChangeOver - USER PANEL. A read-only display of the machine,
 *               running on its OWN ESP8266 board next to the cabinet.
 *
 *          WHAT IT IS FOR
 *            The engineering panel is a workbench tool: it shows raw telemetry,
 *            lets an engineer set any parameter and assumes the person looking
 *            at it built the machine. This program is the opposite. It is for
 *            the owner: what the machine is doing right now, how much it has
 *            charged, when the mains dropped and it ran on the battery, how
 *            long charging took, and - in Persian, in the plainest possible
 *            words - what to do about a fault. It is the dashboard, not the
 *            service menu.
 *
 *          HOW IT GETS ITS DATA
 *            It does NOT touch the STM32 firmware and it does not need a second
 *            serial port. It joins the ChangeOver-ESP access point as a client,
 *            reads `GET /t` (the live telemetry frame) and `GET /m` (the peak
 *            window) from 192.168.4.1, and stores what it reads on its own
 *            flash. If the engineering board is off, the panel keeps serving
 *            the history it already has and says the link is down - it never
 *            invents numbers.
 *
 *          WHAT IT NEVER DOES
 *            With exactly two exceptions - parameters 11 and 12, the charger
 *            enable flags, and only when an operator or an admin presses the
 *            button and confirms - this program never writes to the board. It
 *            cannot change a threshold, a calibration, a duty limit or a fault
 *            action. It cannot clear a fault either: the protocol has no such
 *            command, so the diagnostics page explains which faults a human
 *            must clear by power-cycling the board, and how.
 *
 *          FLASH DISCIPLINE
 *            Samples, events, daily rollups and the lifetime totals live on
 *            LittleFS. When the partition fills up, the OLDEST data is dropped
 *            and the journal for it is compacted (see up_store.h); the panel
 *            reports honestly how much space is left and how many days it still
 *            covers. Nothing here allocates: every buffer is static or on the
 *            stack.
 *
 *          SETUP (once, for the installer)
 *            1. Arduino IDE, board "Generic ESP8266 Module" (or LOLIN/Wemos D1
 *               mini), CPU 80 MHz, flash size 4 MB (1 MB SPIFFS + 1 MB OTA is
 *               comfortable), upload speed 115200.
 *            2. Change UP_STA_SSID / UP_STA_PASS in up_config.h if the machine's
 *               engineering network is not the default.
 *            3. Upload. The panel then opens its own network:
 *               SSID ChangeOver-User, password 123456789, at 192.168.5.1
 *               (deliberately NOT 192.168.4.1 - that is the address of the
 *               engineering AP this panel has to reach).
 *            4. Open http://192.168.5.1, log in as admin / admin and change the
 *               password. The first login is forced to do that.
 *
 *          LAYERING (include order is load-bearing, keep it)
 *            1 config  2 sha256  3 calendar  4 state  5 store  6 auth
 *            7 font  8 history  9 link  10 web assets  11 http (the server)
 *
 * @brief   [FA] ChangeOver - پنل کاربر. نمایشگر فقط-خواندنی ماشین، روی برد
 *               مستقل ESP8266 خودش، کنار تابلو.
 *
 *          این برای چیست
 *            پنل مهندسی یک ابزار کارگاهی است: تلمتری خام نشان می‌دهد، هر
 *            پارامتری را می‌شود با آن ست کرد و فرض می‌کند کسی که به آن نگاه
 *            می‌کند خودش ماشین را ساخته. این برنامه برعکس است. برای صاحب
 *            دستگاه است: همین حالا ماشین چه می‌کند، چقدر شارژ کرده، برق کِی
 *            رفته و روی باتری کار کرده، شارژ چقدر طول کشیده، و - به فارسی و
 *            ساده‌ترین زبان ممکن - برای یک خطا چه باید کرد. این داشبورد است،
 *            نه منوی سرویس.
 *
 *          داده را از کجا می‌گیرد
 *            به فرم‌ور STM32 دست نمی‌زند و به پورت سریال دوم هم نیاز ندارد. به
 *            شبکهٔ دسترسی ChangeOver-ESP به‌عنوان کلاینت وصل می‌شود، `GET /t`
 *            (فریم زندهٔ تلمتری) و `GET /m` (پنجرهٔ اوج) را از ۱۹۲.۱۶۸.۴.۱
 *            می‌خواند و آنچه خوانده روی فلش خودش ذخیره می‌کند. اگر برد مهندسی
 *            خاموش باشد، پنل همان تاریخچه‌ای را که دارد سرو می‌کند و می‌گوید
 *            لینک قطع است - هیچ عددی از خودش نمی‌سازد.
 *
 *          چه کاری هرگز نمی‌کند
 *            با دقیقاً دو استثنا - پارامترهای ۱۱ و ۱۲، همان فعال‌ساز شارژرها،
 *            آن هم فقط وقتی اپراتور یا مدیر دکمه را بزند و تأیید کند - این
 *            برنامه هیچ‌وقت روی برد نمی‌نویسد. نمی‌تواند آستانه، کالیبراسیون،
 *            سقف توان یا اقدام خطا را عوض کند. خطا هم نمی‌تواند پاک کند: چنین
 *            فرمانی در پروتکل وجود ندارد، پس صفحهٔ دیاگ توضیح می‌دهد کدام خطا
 *            را باید انسان با خاموش/روشن کردن برد پاک کند و چطور.
 *
 *          نظم فلش
 *            نمونه‌ها، رویدادها، جمع‌های روزانه و جمع کل عمر روی LittleFS
 *            می‌مانند. وقتی پارتیشن پر شود، «قدیمی‌ترین» داده دور ریخته و
 *            ژورنال آن جمع می‌شود (نگاه کنید به up_store.h)؛ پنل صادقانه
 *            می‌گوید چقدر جا مانده و هنوز چند روز را پوشش می‌دهد. هیچ‌چیزی
 *            اینجا تخصیص حافظه نمی‌گیرد: هر بافر ثابت است یا روی پشته.
 *
 *          راه‌اندازی (یک‌بار، برای نصاب)
 *            ۱. Arduino IDE، برد «Generic ESP8266 Module» (یا LOLIN/Wemos D1
 *               mini)، CPU 80 MHz، فلش 4 MB (۱ MB SPIFFS + ۱ MB OTA راحت است)،
 *               سرعت آپلود 115200.
 *            ۲. اگر شبکهٔ مهندسی ماشین پیش‌فرض نیست، UP_STA_SSID و UP_STA_PASS
 *               را در up_config.h عوض کنید.
 *            ۳. آپلود. بعد پنل شبکهٔ خودش را باز می‌کند:
 *               SSID ChangeOver-User، گذرواژه 123456789، روی 192.168.5.1
 *               (عمداً نه 192.168.4.1 - آن آدرسِ همان AP مهندسی است که این پنل
 *               باید به آن وصل شود).
 *            ۴. http://192.168.5.1 را باز کنید، با admin / admin وارد شوید و
 *               گذرواژه را عوض کنید. اولین ورود به این کار مجبور می‌کند.
 *
 *          لایه‌بندی (ترتیب include بار دارد، حفظش کنید)
 *            ۱ config  ۲ sha256  ۳ calendar  ۴ state  ۵ store  ۶ auth
 *            ۷ font  ۸ history  ۹ xlsx  ۱۰ گزارش اکسل  ۱۱ link
 *            ۱۲ دارایی‌های وب  ۱۳ http (خود سرور)
 */

#include "up_config.h"    /* 1: every constant the rest of the panel obeys  */
#include "up_sha256.h"    /* 2: salted, iterated digests for the login       */
#include "up_calendar.h"  /* 3: Jalali dates + "which local day/hour is it"  */
#include "up_state.h"     /* 4: what the panel knows right now               */
#include "up_store.h"     /* 5: the rings on flash + "drop the oldest"       */
#include "up_auth.h"      /* 6: users, sessions, action log                  */
#include "up_font.h"      /* 7: the embedded Persian font                    */
#include "up_history.h"   /* 8: turns telemetry into history and statistics  */
#include "up_xlsx.h"      /* 9: a streaming .xlsx writer (ZIP + SpreadsheetML) */
#include "up_report.h"    /* 10: what goes INTO that workbook                */
#include "up_link.h"      /* 11: Wi-Fi + the reads from the engineering board */
#include "up_web.h"       /* 12: the page itself, in PROGMEM (data, no code) */
#include "up_http.h"      /* 13: the web server, which serves number 12      */

/* ==================== Panel state / وضعیت پنل ==================== */
static uint32_t UINT32_T__G__LastLedMs = 0u;   /* [EN] last LED flip / [FA] آخرین تغییر LED */
static bool     BOOL__G__LedLit = false;       /* [EN] LED is currently on / [FA] LED روشن است */

/* ==================== Status LED / LED وضعیت ==================== */
/**
 * @brief  [EN] The one light on the box answers the one question an owner asks
 *              at the cabinet: "is it alive, and is it still getting data?"
 *              A fast blink means the link to the engineering board is down; a
 *              slow blink means everything is fine. Both are "working" - a
 *              panel with a dead link that blinks calmly would be lying.
 *         [FA] تنها چراغ روی جعبه همان یک سؤال صاحب دستگاه کنار تابلو را
 *              جواب می‌دهد: «زنده است و هنوز داده می‌گیرد؟» چشمک تند یعنی لینک
 *              برد مهندسی قطع است؛ چشمک کند یعنی همه‌چیز خوب است. هر دو یعنی
 *              «کار می‌کند» - پنلی که لینکش مرده و آرام چشمک بزند، دروغ گفته.
 * @param  uint32_t__nowMs [EN] millis() snapshot / [FA] مقدار millis()
 * @return [EN] None / [FA] ندارد
 */
static void func__UpPanel_StatusLed(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__periodMs = (func__UpState_LinkOnline()) ? UP_LED_SLOW_MS : UP_LED_FAST_MS;
    uint32_t uint32_t__halfMs = uint32_t__periodMs / 2u;
    bool bool__due = ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastLedMs) >= uint32_t__halfMs);

    if (bool__due)
    {
        UINT32_T__G__LastLedMs = uint32_t__nowMs;
        BOOL__G__LedLit = !BOOL__G__LedLit;

#if UP_LED_ACTIVE_LOW
        digitalWrite(UP_LED_PIN, BOOL__G__LedLit ? LOW : HIGH);
#else
        digitalWrite(UP_LED_PIN, BOOL__G__LedLit ? HIGH : LOW);
#endif
    }
}

/* ==================== Boot lines / خطوط راه‌اندازی ==================== */
/**
 * @brief  [EN] Six lines on the serial port. Whoever flashes this in a cold
 *              workshop at 2 a.m. should not have to open a browser to find out
 *              that LittleFS failed to mount.
 *         [FA] شش خط روی پورت سریال. هر کس این را نیمه‌شب در کارگاهی سرد فلش
 *              می‌کند، نباید برای فهمیدن اینکه LittleFS مانت نشده مرورگر باز
 *              کند.
 * @param  bool__storageOk [EN] LittleFS mounted / [FA] LittleFS مانت شد
 * @param  bool__authOk [EN] at least one account exists / [FA] دست‌کم یک حساب هست
 * @return [EN] None / [FA] ندارد
 */
static void func__UpPanel_ReportStartup(bool bool__storageOk, bool bool__authOk)
{
    Serial.println();
    Serial.println(F("ChangeOver user panel"));
    Serial.println(F(UP_BUILD_STAMP));
    Serial.println(bool__storageOk ? F("storage: ok") : F("storage: FAILED - history will not be kept"));
    Serial.println(bool__authOk ? F("users: ok") : F("users: FAILED - nobody can log in"));
    Serial.print(F("panel:  http://"));
    Serial.println(WiFi.localIP());
    Serial.print(F("board:  "));
    Serial.print(F(UP_SRC_HOST));
    Serial.print(F(" ("));
    Serial.print(F(UP_STA_SSID));
    Serial.println(F(")"));
}

/* ==================== Setup / راه‌اندازی ==================== */
/**
 * @brief  [EN] Bring each layer up in dependency order and say so on the serial
 *              port. Storage and users are reported rather than fatal: a panel
 *              whose flash died can still show the live machine, and a panel
 *              whose user table is empty still answers the network - both are
 *              more useful than a boot loop, and both are visible in /version.
 *         [FA] بالا آوردن هر لایه به ترتیب وابستگی و گفتنش روی پورت سریال.
 *              حافظه و کاربران گزارش می‌شوند و کشنده نیستند: پنلی که فلشش مرده
 *              هنوز می‌تواند ماشین زنده را نشان دهد و پنلی که جدول کاربرش خالی
 *              است هنوز به شبکه جواب می‌دهد - هر دو از یک حلقهٔ ری‌استارت مفیدترند
 *              و هر دو در /version دیده می‌شوند.
 * @return [EN] None; the sketch never returns from setup / [FA] ندارد
 */
void setup(void)
{
    bool bool__storageOk;
    bool bool__authOk;

    Serial.begin(115200);
    pinMode(UP_LED_PIN, OUTPUT);
    digitalWrite(UP_LED_PIN, UP_LED_ACTIVE_LOW ? HIGH : LOW);

    bool__storageOk = func__UpStore_Begin();   /* flash, rings, totals            */
    func__UpState_ClockRestore();              /* time of day, from the last flush */
    bool__authOk = func__UpAuth_Begin();       /* user table, seeded on first run */
    func__UpHistory_Begin();                   /* baselines for the edge detectors */
    func__UpLink_Begin();                      /* Wi-Fi: user AP + engineering STA */
    func__UpHttp_Begin();                      /* routes, then the server itself   */

    func__UpPanel_ReportStartup(bool__storageOk, bool__authOk);
}

/* ==================== Loop / حلقه ==================== */
/**
 * @brief  [EN] The whole runtime, in the order it must happen:
 *              link (talk to the board) -> history (turn frames into history)
 *              -> flush (save what changed) -> storage pressure (drop the
 *              oldest if the partition is filling) -> web server (answer the
 *              browser) -> LED.
 *              Nothing here blocks on the network: the link's state machine
 *              does one bounded step per call, so a browser request is served
 *              while a read from the board is still in flight.
 *         [FA] تمام زمان اجرا، به همان ترتیبی که باید رخ دهد:
 *              لینک (گفت‌وگو با برد) -> تاریخچه (تبدیل فریم‌ها به تاریخچه)
 *              -> ذخیره (نوشتن تغییرات) -> فشار حافظه (دور ریختن قدیمی‌ترین
 *              اگر پارتیشن پر می‌شود) -> وب‌سرور (جواب دادن به مرورگر) -> LED.
 *              هیچ‌چیز اینجا روی شبکه بلوکه نمی‌شود: ماشین حالت لینک در هر
 *              فراخوانی یک قدم محدود برمی‌دارد، پس درخواست مرورگر همان موقع
 *              جواب می‌گیرد که خواندن از برد در جریان است.
 * @return [EN] None; called in a loop by the Arduino core / [FA] ندارد
 */
void loop(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();

    func__UpLink_Loop();                                  /* reads + queued writes */
    func__UpHistory_Tick(uint32_t__nowMs);                /* edges, runs, counters */
    (void)func__UpHistory_SampleIfDue(uint32_t__nowMs);    /* the 1 s sample       */
    (void)func__UpStore_FlushIfDue();                     /* save what is dirty    */
    (void)func__UpStore_HandlePressure();                 /* drop the oldest first */
    UP_WEBSERVER_T__G__Server.handleClient();             /* the browser           */
    func__UpPanel_StatusLed(uint32_t__nowMs);

    /* [EN] Let the ESP8266's own housekeeping (Wi-Fi, DHCP, TCP) run: without
       it the panel starts answering slowly after a few thousand loops, and on
       the ESP8266 that shows up as a browser that times out.
       [FA] بگذارید کارهای داخلی خود ESP8266 (وای‌فای، DHCP، TCP) اجرا شود:
       بدون این، پنل بعد از چند هزار دور کند جواب می‌دهد و روی ESP8266 همین
       به‌شکل مرورگری دیده می‌شود که تایم‌اوت می‌خورد. */
    yield();
}
