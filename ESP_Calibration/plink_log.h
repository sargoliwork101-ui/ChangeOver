/* plink_log.h - live state history on the panel's own flash, one CSV per day.
   [FA] plink_log.h - تاریخچهٔ وضعیت زنده روی فلش خود پنل، روزی یک فایل CSV.
   Included by ESP_Calibration.ino (single translation unit, order matters):
   after plink_state.h and before plink_link.h (the frame parser calls the hook).
   No include guard on purpose: including twice would redefine everything.

   [EN] Second-generation rewrite, 2026-10-27 (user order: "rewrite the log from
        scratch, your own way, so it does what I asked for"). The first rewrite
        fixed the three proven defects; this one is the design I would hand to a
        production line. What is different from a plain "append a line" logger:

        1) ONE write path. Every row goes through func__EspLog_AppendRow(), so a
           refused write cannot be swallowed anywhere (v1 had four print sites).
        2) NOTHING IS LOST WHEN THE FLASH REFUSES. The row waits in the RAM ring
           and is drained back into its own day file (by the row's own epoch, not
           by "today") as soon as the flash works again.
        3) A HALF-WRITTEN LAST ROW CANNOT EAT THE NEXT ONE. A power cut in the
           middle of a row leaves no closing newline; the day file is repaired
           once when it is opened, before the next row is appended.
        4) THE INDEX CARRIES BYTE SIZES, so retention costs nothing: v1 opened up
           to 30 day files on every day change just to add up their sizes. Sizes
           are maintained row by row; an entry with an unknown size is measured
           once and cached.
        5) THE DAY'S ROW CAP EXPLAINS ITSELF. The last row of a full day is a
           marker (note 4), so a gap in the history says "the cap was reached"
           instead of looking like a dead panel.
        6) LOST ROWS ARE COUNTED. If the RAM ring overflows while the flash is
           down, the page is told how many rows were dropped (X-Log-Lost).
        7) THE TIME NEVER RUNS BACKWARD: a phone whose clock is behind cannot
           rewind the anchor.
        8) The row buffer is proven at compile time to hold the widest possible
           row, and a truncation is reported instead of written.

        Kept one for one from the old file, because they are user orders, not
        choices: boot row with no board at all, change-only rows, cut-band
        hysteresis, one voltage-step row a minute (rises in discharge always),
        mains rows inside a short window, six-hour heartbeat, day 0 for a panel
        that has never seen a phone, retention by day count and byte budget,
        RAM ring when the file system is off.
   [FA] بازنویسی نسل دوم، ۲۰۲۶-۱۰-۲۷ (دستور کاربر: «لاگ را از اول، با روش خودت
        بازنویسی کن تا خواسته‌هایم را برآورده کند»). بازنویسی اول سه ایراد ثابت
        را درست کرد؛ این یکی همان طرحی است که برای خط تولید تحویل می‌دادم.
        تفاوتش با یک «چند خط بنویس» ساده:

        ۱) یک مسیر نوشتن. هر ردیف از یک تابع می‌گذرد، پس نوشتنِ ردشده هیچ‌جا
           پنهان نمی‌شود.
        ۲) وقتی فلش نمی‌پذیرد، هیچ ردیفی گم نمی‌شود. ردیف در حلقهٔ رم صبر
           می‌کند و به‌محض سالم‌شدن فلش، در فایل روزِ خودش (بر اساس زمان خودِ
           ردیف، نه «امروز») نوشته می‌شود.
        ۳) ردیفِ نیمه‌نوشته ردیف بعدی را نمی‌خورد. اگر برق وسط یک ردیف برود،
           پایان‌بند نوشته نمی‌شود؛ فایل روز یک‌بار هنگام باز شدن تعمیر می‌شود.
        ۴) فهرست روزها حجم هر روز را هم نگه می‌دارد، پس پاک‌سازی قدیمی‌ها
           هزینه‌ای ندارد. نسخهٔ قبل برای جمع‌زدن حجم‌ها تا ۳۰ فایل را باز
           می‌کرد؛ حالا حجم با هر ردیف به‌روز می‌شود.
        ۵) سقف ردیف هر روز خودش را توضیح می‌دهد: آخرین ردیف روزِ پر، یک ردیف
           نشانه است، تا فاصلهٔ تاریخچه «سقف پر شد» بگوید نه «پنل مرد».
        ۶) ردیف‌های ازدست‌رفته شمرده می‌شوند: اگر حلقهٔ رم پر شود، صفحه
           می‌فهمد چند ردیف از دست رفته است.
        ۷) زمان هرگز عقب نمی‌رود: گوشی با ساعت عقب نمی‌تواند لنگر را برگرداند.
        ۸) بزرگ‌ترین ردیف ممکن در زمان کامپایل بررسی می‌شود و بریده‌شدن ردیف
           گزارش می‌شود، نه نوشته.

        قول‌های فایل قبلی یکی‌به‌یکی می‌مانند، چون دستور کاربرند نه انتخاب:
        ردیف بوت حتی بدون برد، ردیف فقط روی تغییر، هیسترزیس مرز قطع، حداکثر یک
        ردیف گام در دقیقه (در دشارژ هر بالا آمدن)، ردیف برق شهر داخل پنجرهٔ
        کوتاه، ضربان شش‌ساعته، روز صفر برای پنلی که گوشی ندیده، نگه‌داری
        روزانه و بایتی، و حلقهٔ رم وقتی فایل‌سیستم خاموش است. */

#include <string.h>

/* ==================== Clock / ساعت ==================== */
/* [EN] The panel has no RTC and the technical link carries no board time (that
        field exists only in the simple panel's DIAG_DATA), so the phone that
        opens the page is the only clock: now = anchor + elapsed millis. Rejected
        time cannot rewind it (see func__EspLog_TimeSet).
   [FA] پنل ساعت ندارد و لینک فنی هم زمان برد را نمی‌آورد (آن فیلد فقط در پنل
        ساده هست)، پس تنها ساعت، گوشی‌ای است که صفحه را باز می‌کند:
        اکنون = لنگر + میلی‌ثانیه‌های گذشته. زمانِ ردشده هم نمی‌تواند آن را
        عقب ببرد. */
static uint32_t UINT32_T__G__LogAnchorSec = 0u;
static uint32_t UINT32_T__G__LogAnchorMs = 0u;
static int32_t  INT32_T__G__LogTzSec = 0;
static bool     BOOL__G__LogClockValid = false;
/* [EN] THE IMPORTANT ONE: the panel is not the clock. The STM32 stays powered
        when the panel is cut, reboots or is unplugged, and its HELLO_ACK
        carries its own uptime. So the wall clock is anchored to the BOARD's
        uptime, not to millis(): the panel can lose power for an hour and still
        know the real time with no browser anywhere.
   [FA] مهم‌ترین قسمت: پنل ساعت نیست. STM32 وقتی پنل خاموش می‌شود، ری‌استارت
        می‌شود یا کابلش جدا می‌شود روشن می‌ماند و آپتایم خودش را در HELLO_ACK
        می‌فرستد. پس ساعت روی آپتایم «برد» بسته می‌شود، نه روی millis(): پنل
        می‌تواند یک ساعت بی‌برق بماند و باز هم وقتِ واقعی را بدونه، بی‌آنکه
        هیچ مرورگری باز باشد. */
static uint32_t UINT32_T__G__LogAnchorBoardSec = 0u;
static bool     BOOL__G__LogAnchorBoardValid = false;
static uint32_t UINT32_T__G__LogBoardNowSec = 0u;
static bool     BOOL__G__LogBoardAlive = false;
/* [EN] The latest time this panel has ever believed. The clock may stand still
        when nobody can tell it the time, but it NEVER goes back: a history that
        jumps backwards writes the same rows twice in two different files.
   [FA] دیرترین زمانی که این پنل تا حالا باور کرده است. ساعت وقتی کسی نتواند
        وقت را بگوید می‌ایستد، ولی هیچ‌وقت عقب نمی‌رود: تاریخچه‌ای که عقب
        بپرد، یک ردیف را دو بار در دو فایل می‌نویسد. */
static uint32_t UINT32_T__G__LogHighWaterSec = 0u;
/* [EN] set when the browser moved the clock by a lot; the next tick writes the
        row that says so.
   [FA] وقتی مرورگر ساعت را زیاد جابه‌جا کرد روشن می‌شود؛ تیک بعدی ردیفی
        می‌نویسد که همین را بگوید. */
static bool     BOOL__G__LogClockJumpPending = false;
/* [EN] The state the flash remembers from before the power went off. It is read
        ONCE at boot and used to compare "before" with "now": if any logged
        condition changed while the panel was dead, the change is written; if
        nothing changed, the panel says nothing and waits.
   [FA] وضعیتی که فلش از پیش از قطع برق به یاد دارد. یک‌بار هنگام روشن شدن
        خوانده می‌شود و «قبل» با «حالا» مقایسه می‌شود: اگر یکی از حالت‌های
        لاگ در مدت خاموشی عوض شده باشد نوشته می‌شود؛ اگر چیزی عوض نشده باشد
        پنل چیزی نمی‌گوید و منتظر می‌ماند. */
static bool     BOOL__G__LogBootSnapValid = false;
static uint32_t UINT32_T__G__LogBootV24 = 0u;
static uint32_t UINT32_T__G__LogBootFault = 0u;
static uint32_t UINT32_T__G__LogBootVlow = 0u;
static uint32_t UINT32_T__G__LogBootVhigh = 0u;
static uint8_t  UINT8_T__G__LogBootFlags2 = 0u;
static uint8_t  UINT8_T__G__LogBootChg1 = 0u;
static uint8_t  UINT8_T__G__LogBootChg2 = 0u;
static uint8_t  UINT8_T__G__LogBootMains = 0u;
static uint8_t  UINT8_T__G__LogBootDanger = 0u;
/* [EN] v1.110: the condemned-battery mask and the status byte are logged
        conditions too, so the last row carries them and the first frame after
        a restart is compared against it like every other condition.
   [FA] نسخهٔ ۱٫۱۱۰: ماسک باتری خراب و بایت وضعیت هم شرط‌های ثبت‌شده‌اند، پس
        آخرین ردیف آن‌ها را دارد و اولین فریم بعد از روشن شدن مثل بقیهٔ
        شرط‌ها با همان مقایسه می‌شود. */
static uint32_t UINT32_T__G__LogBootDead = 0u;
static uint8_t  UINT8_T__G__LogBootFl = 0u;
static uint32_t UINT32_T__G__LogLastClockSaveMs = 0u;
static bool     BOOL__G__LogClockSavedOnce = false;

/* ==================== Open day / روز باز ==================== */
/* [EN] The sentinel says "no day opened yet". Day 0 is a real day: the day of a
        panel that has never seen a phone. Storage must never wait for the clock.
   [FA] این مقدار یعنی «هنوز روزی باز نشده». روز صفر یک روز واقعی است: روزِ
        پنلی که هنوز گوشی‌ای ندیده. ذخیره‌سازی هرگز نباید منتظر ساعت بماند. */
static uint32_t UINT32_T__G__LogDayCode = 0xFFFFFFFFu;   /* ‎YYYYMMDD‎, sentinel = none */
static uint16_t UINT16_T__G__LogRowsToday = 0u;
static bool     BOOL__G__LogDayFull = false;
static bool     BOOL__G__LogBootRowPending = true;
/* [EN] 0 = the last flash write was verified; anything else = failed writes
        since the last good one. This is what lets the page tell "nothing
        happened" from "I could not write".
   [FA] صفر یعنی آخرین نوشتن روی فلش تأیید شد؛ هر مقدار دیگر یعنی تعداد
        نوشتن‌های شکست‌خورده از آخرین نوشتن سالم. همین است که به صفحه اجازه
        می‌دهد «چیزی نشده» را از «نتوانستم بنویسم» جدا کند. */
static uint8_t  UINT8_T__G__LogWriteFault = 0u;
/* [EN] Rows the RAM ring had to throw away because a newer one arrived while the
        flash was still down. Reported to the page (X-Log-Lost); never hidden.
   [FA] ردیف‌هایی که حلقهٔ رم ناچار شد دور بریزد، چون ردیف تازه‌تری رسید و فلش
        هنوز خراب بود. به صفحه گزارش می‌شود و هرگز پنهان نمی‌ماند. */
static uint16_t UINT16_T__G__LogDroppedRows = 0u;

/* ==================== Previous snapshot / وضعیت قبلی ==================== */
static bool     BOOL__G__LogHasPrev = false;
static uint8_t  UINT8_T__G__LogPrevMains = 0u;
/* [EN] v1.95: the board's general byte (hand mode, snapshot, valid
        measurement) is logged too, so a mode change is an event.
   [FA] نسخهٔ ۱٫۹۵: بایت عمومی برد (مود دستی، snapshot، معتبر بودن
        اندازه‌گیری) هم ثبت می‌شود تا تغییر مود یک رویداد باشد. */
static uint8_t  UINT8_T__G__LogPrevFl = 0u;
/* [EN] v1.95: the condemned-battery verdict is a state too, and it is
        NOT in the fault mask - it has its own field on the wire.
   [FA] حکم باتری خراب هم یک وضعیت است و در بیت‌های خطا نیست؛
        فیلد خودش را روی سیم دارد. */
static uint32_t UINT32_T__G__LogPrevDead = 0u;
/* [EN] v1.96: the pack voltage itself is a change trigger, in visible
        steps - that is what draws the charge curve in the log.
   [FA] نسخهٔ ۱٫۹۶: خودِ ولتاژ پک هم محرک تغییر است، با گام محسوس -
        همان چیزی که منحنی شارژ را در لاگ می‌کشد. */
static uint32_t UINT32_T__G__LogPrevV24 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg1 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg2 = 0u;
static uint8_t  UINT8_T__G__LogPrevFlags2 = 0u;
static uint8_t  UINT8_T__G__LogPrevBand = 0u;
static uint32_t UINT32_T__G__LogPrevFault = 0u;
static uint8_t  UINT8_T__G__LogPrevNote = 0u;   /* [EN] danger before the last restart / [FA] خطر پیش از ری‌استارت */

/* ==================== Pending change / تغییر در انتظار ==================== */
/* [EN] A firmware update was seen at this boot, so its row must be written no
        matter what.
   [FA] در همین بوت یک بروزرسانی فرم‌ور دیده شد، پس ردیفش باید بی‌قید و شرط
        نوشته شود. */
static bool     BOOL__G__LogUpdatePending = false;
/* [EN] One-shot: the next row ignores the day cap too.
   [FA] یک‌بارمصرف: ردیف بعدی سقف روز را هم نادیده می‌گیرد. */
static bool     BOOL__A__LogForceRow = false;
static uint16_t UINT16_T__G__LogPendingMask = 0u;
static uint8_t  UINT8_T__G__LogPendingNote = 0u;
static bool     BOOL__G__LogMainsQueuedValid = false;
static uint32_t UINT32_T__G__LogMainsQueuedMs = 0u;
static uint32_t UINT32_T__G__LogLastRowMs = 0u;
static bool     BOOL__G__LogLastRowValid = false;
static bool     BOOL__G__LogLinkDown = false;
/* [EN] when the boot row of this session was written; the "board silent"
        clock runs from here even if no frame ever arrived.
   [FA] لحظهٔ نوشتن ردیف بوتِ همین نشست؛ تایمرِ «برد ساکت است» از همین لحظه
        می‌شمارد، حتی اگر هیچ فریمی نرسیده باشد. */
static uint32_t UINT32_T__G__LogSessionStartMs = 0u;
/* [EN] v1.98: when the last voltage-step row was written, so a load that
        switches on and off cannot turn the step into a strobe.
   [FA] نسخهٔ ۱٫۹۸: لحظهٔ آخرین ردیف گام ولتاژ، تا باری که روشن و خاموش
        می‌شود گام را به چشمک‌زدن تبدیل نکند. */
static uint32_t UINT32_T__G__LogLastStepMs = 0u;
static bool     BOOL__G__LogLastStepValid = false;
/* [EN] the day-full marker is written once per day, never twice.
   [FA] ردیف نشانهٔ «سقف روز پر شد» یک‌بار در روز نوشته می‌شود، نه دوبار. */
static bool     BOOL__G__LogMarkerDone = false;

/* ==================== Day list / فهرست روزها ==================== */
/* [EN] The days are NOT stored anywhere: this is a cache of the directory,
        filled by func__EspLog_ScanDays from the file names on the flash.
   [FA] روزها هیچ‌جا ذخیره نمی‌شوند: این فقط حافظهٔ موقت پوشه است که
        func__EspLog_ScanDays از نام فایل‌های روی فلش پر می‌کند. */
static uint32_t UINT32_T__G__LogIndexDay[ESP_LOG_MAX_DAYS + 2u];
/* [EN] size of each day file, read from the directory in the same scan; the
        retention needs it and asking the flash once is enough.
   [FA] حجم هر فایل روز، در همان خواندن پوشه؛ پاک‌سازی قدیمی‌ها به آن نیاز
        دارد و یک بار پرسیدن از فلش کافی است. */
static uint32_t UINT32_T__G__LogIndexBytes[ESP_LOG_MAX_DAYS + 2u];
static uint8_t  UINT8_T__G__LogIndexCount = 0u;
/* [EN] RAM ring: the last rows survive when the flash cannot take them - either
        because the file system is off or because a write just failed. It is
        drained back into the day files as soon as the flash works again.
   [FA] حلقهٔ رم: آخرین ردیف‌ها وقتی فلش نمی‌پذیرد زنده می‌مانند - یا چون
        فایل‌سیستم خاموش است یا چون نوشتن شکست خورد. به‌محض سالم شدن فلش،
        همان ردیف‌ها به فایل روزهای خودشان برگردانده می‌شوند. */
static char     CHAR__G__LogRamRow[ESP_LOG_RAM_ROWS][ESP_LOG_RAM_ROW_MAX];
static uint8_t  UINT8_T__G__LogRamHead = 0u;
static uint8_t  UINT8_T__G__LogRamCount = 0u;

/* ==================== Change bits / بیت‌های تغییر ==================== */
#define ESP_LOG_CHG_BOOT    0x0001u
#define ESP_LOG_CHG_MAINS   0x0002u
#define ESP_LOG_CHG_CHG1    0x0004u
#define ESP_LOG_CHG_CHG2    0x0008u
#define ESP_LOG_CHG_FAULT   0x0010u
#define ESP_LOG_CHG_FLAGS   0x0020u
#define ESP_LOG_CHG_BAND    0x0040u
#define ESP_LOG_CHG_LINK    0x0080u
#define ESP_LOG_CHG_MODE    0x0100u   /* [EN] hand mode / data validity / [FA] مود دستی و اعتبار داده */
#define ESP_LOG_CHG_DEAD    0x0200u   /* [EN] condemned-battery verdict / [FA] حکم باتری خراب */
#define ESP_LOG_CHG_VSTEP   0x0400u   /* [EN] pack voltage moved a visible step / [FA] ولتاژ پک یک پلهٔ محسوس جابه‌جا شد */
#define ESP_LOG_CHG_CLOCK   0x0800u   /* [EN] the browser moved the clock / [FA] مرورگر ساعت را جابه‌جا کرد */
#define ESP_LOG_CHG_UPDATE  0x1000u   /* [EN] the panel was flashed with a new firmware / [FA] پنل با فرم‌ور تازه فلش شد */

/* ==================== Notes / یادداشت ردیف ==================== */
#define ESP_LOG_NOTE_NONE      0u
#define ESP_LOG_NOTE_LOW_STOP  1u   /* [EN] board came back after a low-battery stop / [FA] برگشت پس از ایست باتری ضعیف */
#define ESP_LOG_NOTE_LINK_LOST 2u
#define ESP_LOG_NOTE_LINK_BACK 3u
#define ESP_LOG_NOTE_DAY_FULL  4u   /* [EN] the last row of a full day / [FA] آخرین ردیف یک روزِ پر */
#define ESP_LOG_NOTE_HEARTBEAT 5u   /* [EN] nothing changed, the panel is alive / [FA] چیزی عوض نشد، پنل زنده است */

#define ESP_LOG_PATH_MAX       24u
/* [EN] One CSV row: 22 numbers, the widest possible being 22 x 10 digits plus
        21 commas = 241 bytes plus the terminator. The static_assert below turns
        a future column that does not fit into a build error instead of a
        silently cut row.
   [FA] یک ردیف CSV: ۲۲ عدد که بزرگ‌ترین حالتشان ۲۴۱ بایت به‌علاوهٔ پایان‌بند
        می‌شود. بررسی زمان کامپایل، ستونِ جا نشدِ آینده را به خطای ساخت تبدیل
        می‌کند، نه ردیفِ بی‌صدا بریده. */
#define ESP_LOG_ROW_MAX        256u
#define ESP_LOG_ROW_WORST_CASE 244u
static_assert(ESP_LOG_ROW_MAX >= ESP_LOG_ROW_WORST_CASE,
              "ESP_LOG_ROW_MAX must hold the widest row: 22 x 10 digits + 21 commas");

/* [EN] The value the /evt answer carries in X-Log-Fs: 1 = rows come from the
        flash, 0 = the file system is off (RAM ring), 2 = the file system is
        mounted but a write failed (RAM ring holds the newest rows).
   [FA] مقداری که پاسخ ‎/evt‎ در ‎X-Log-Fs‎ می‌گذارد: ۱ یعنی ردیف‌ها از فلش
        می‌آیند، ۰ یعنی فایل‌سیستم خاموش است (حلقهٔ رم)، ۲ یعنی فایل‌سیستم
        سوار است ولی نوشتن شکست خورد (جدیدترین ردیف‌ها در حلقهٔ رم هستند). */
#define ESP_LOG_FS_OK          1u
#define ESP_LOG_FS_OFF         0u
#define ESP_LOG_FS_WRITE_FAULT 2u

/* ==================== Time helpers / کمک‌های زمان ==================== */

/**
 * @brief  [EN] Current epoch seconds from the phone anchor, 0 when unknown.
 *         [FA] ثانیهٔ یونیکس فعلی از لنگر گوشی؛ صفر یعنی نامعلوم.
 * @return [EN] uint32_t / [FA]‎ uint32_t
 */
static uint32_t func__EspLog_NowSec(void)
{
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__panelSec;

    if (BOOL__G__LogClockValid == false)
    {
        return 0u;
    }

    /* [EN] What this panel can say on its own: the anchor plus its own uptime.
       [FA] آنچه پنل به‌تنهایی می‌تواند بگوید: لنگر به‌علاوهٔ آپتایم خودش. */
    uint32_t__elapsedMs = (uint32_t)millis() - UINT32_T__G__LogAnchorMs;
    uint32_t__panelSec = UINT32_T__G__LogAnchorSec + (uint32_t__elapsedMs / 1000u);

    /* [EN] What the BOARD can say: it kept counting while this panel was dead.
            Whichever is further ahead is the truth, so the clock never rewinds
            and never loses the time the panel was not running.
       [FA] آنچه برد می‌تواند بگوید: وقتی پنل مرده بود، برد همچنان می‌شمرد.
            هرکدام جلوتر باشد درست است، پس ساعت نه عقب می‌رود و نه وقتی را که
            پنل خاموش بوده گم می‌کند. */
    if ((BOOL__G__LogAnchorBoardValid != false) && (BOOL__G__LogBoardAlive != false) &&
        (UINT32_T__G__LogBoardNowSec >= UINT32_T__G__LogAnchorBoardSec))
    {
        uint32_t uint32_t__boardSec = UINT32_T__G__LogAnchorSec +
            (UINT32_T__G__LogBoardNowSec - UINT32_T__G__LogAnchorBoardSec);
        if (uint32_t__boardSec > uint32_t__panelSec)
        {
            uint32_t__panelSec = uint32_t__boardSec;
        }
    }
    if (uint32_t__panelSec < UINT32_T__G__LogHighWaterSec)
    {
        return UINT32_T__G__LogHighWaterSec;   /* never back */
    }
    UINT32_T__G__LogHighWaterSec = uint32_t__panelSec;
    return uint32_t__panelSec;
}

/**
 * @brief  [EN] The board said how long it has been running (HELLO_ACK).
 *              First answer after boot: anchor to it without any jump. Answer
 *              with a SMALLER uptime than the anchor means the board itself
 *              restarted, so that anchor is dead and a new one is taken.
 *         [FA] برد گفت چقدر است روشن است (HELLO_ACK). اولین پاسخ پس از بوت:
 *              بی‌هیچ پرشی به آن لنگر می‌زنیم. آپتایمِ کوچک‌تر از لنگر یعنی
 *              خودِ برد ری‌استارت شده، پس آن لنگر مرده است و لنگر تازه گرفته
 *              می‌شود.
 * @‎param  uint32_t uint32_t__boardUptimeSec [EN] board uptime, s /
 *                                           [FA]‎ آپتایم برد به ثانیه
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_NoteBoardTime(uint32_t uint32_t__boardUptimeSec)
{
    if (uint32_t__boardUptimeSec == 0u)
    {
        BOOL__G__LogBoardAlive = false;
        return;
    }
    UINT32_T__G__LogBoardNowSec = uint32_t__boardUptimeSec;
    BOOL__G__LogBoardAlive = true;

    if (BOOL__G__LogAnchorBoardValid != false)
    {
        if (uint32_t__boardUptimeSec < UINT32_T__G__LogAnchorBoardSec)
        {
            BOOL__G__LogAnchorBoardValid = false;   /* the board restarted */
        }
        else
        {
            return;   /* the anchor still stands */
        }
    }
    if (BOOL__G__LogClockValid == false)
    {
        return;
    }
    /* [EN] fresh anchor at the time the panel believes right now: no jump.
       [FA] لنگر تازه روی زمانی که پنل همین حالا باور دارد: بدون پرش. */
    UINT32_T__G__LogAnchorSec = func__EspLog_NowSec();
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    UINT32_T__G__LogAnchorBoardSec = uint32_t__boardUptimeSec;
    BOOL__G__LogAnchorBoardValid = true;
}

/**
 * @brief  [EN] Local day number (days since 1970-01-01) of an epoch second.
 *         [FA] شمارهٔ روز محلی (روز از ۱۹۷۰-۰۱-۰۱) برای یک ثانیهٔ یونیکس.
 * @‎param  uint32_t uint32_t__epochSec [EN] epoch seconds / [FA]‎ ثانیهٔ یونیکس
 * @return [EN] uint32_t / [FA]‎ uint32_t
 */
static uint32_t func__EspLog_LocalDay(uint32_t uint32_t__epochSec)
{
    int32_t int32_t__local;

    int32_t__local = (int32_t)uint32_t__epochSec + INT32_T__G__LogTzSec;
    if (int32_t__local < 0)
    {
        return 0u;
    }
    return (uint32_t)(int32_t__local / 86400);
}

/**
 * @brief  [EN] Civil date (Gregorian) of a day number, packed as YYYYMMDD.
 *         [FA] تاریخ میلادی یک شمارهٔ روز، بسته‌بندی‌شده به‌صورت YYYYMMDD.
 * @‎param  uint32_t uint32_t__days [EN] days since 1970-01-01 / [FA]‎ روز از ۱۹۷۰
 * @‎return [EN] uint32_t packed date, 0 when unknown / [FA]‎ تاریخ بسته‌بندی‌شده
 */
static uint32_t func__EspLog_DateCode(uint32_t uint32_t__days)
{
    uint32_t uint32_t__z;
    uint32_t uint32_t__era;
    uint32_t uint32_t__doe;
    uint32_t uint32_t__yoe;
    uint32_t uint32_t__doy;
    uint32_t uint32_t__mp;
    uint32_t uint32_t__year;
    uint32_t uint32_t__month;
    uint32_t uint32_t__day;

    /* [EN] Howard Hinnant's civil_from_days, all-unsigned for dates after 1970.
       [FA] الگوریتم تبدیل روز به تاریخ میلادی؛ برای تاریخ‌های بعد از ۱۹۷۰
            همه‌ی محاسبات مثبت است. */
    uint32_t__z = uint32_t__days + 719468u;
    uint32_t__era = uint32_t__z / 146097u;
    uint32_t__doe = uint32_t__z - (uint32_t__era * 146097u);
    uint32_t__yoe = (uint32_t__doe - (uint32_t__doe / 1460u) +
                     (uint32_t__doe / 36524u) - (uint32_t__doe / 146096u)) / 365u;
    uint32_t__year = uint32_t__yoe + (uint32_t__era * 400u);
    uint32_t__doy = uint32_t__doe - ((365u * uint32_t__yoe) +
                                     (uint32_t__yoe / 4u) - (uint32_t__yoe / 100u));
    uint32_t__mp = ((5u * uint32_t__doy) + 2u) / 153u;
    uint32_t__day = uint32_t__doy - (((153u * uint32_t__mp) + 2u) / 5u) + 1u;
    uint32_t__month = (uint32_t__mp < 10u) ? (uint32_t__mp + 3u) : (uint32_t__mp - 9u);
    if (uint32_t__month <= 2u)
    {
        uint32_t__year = uint32_t__year + 1u;
    }
    if ((uint32_t__year > 9999u) || (uint32_t__month > 12u) || (uint32_t__day > 31u))
    {
        return 0u;
    }
    return (uint32_t__year * 10000u) + (uint32_t__month * 100u) + uint32_t__day;
}

/**
 * @brief  [EN] Day code of "now" (0 when the phone clock is unknown).
 *         [FA] کد روز «همین حالا» (صفر وقتی ساعت گوشی معلوم نیست).
 * @return [EN] uint32_t / [FA]‎ uint32_t
 */
static uint32_t func__EspLog_TodayCode(void)
{
    uint32_t uint32_t__nowSec;

    uint32_t__nowSec = func__EspLog_NowSec();
    if (uint32_t__nowSec == 0u)
    {
        return 0u;
    }
    return func__EspLog_DateCode(func__EspLog_LocalDay(uint32_t__nowSec));
}

/**
 * @brief  [EN] Build a day file path. Day 0 (clock unknown) gets its own file.
 *         [FA] ساخت مسیر فایل یک روز. روز صفر (ساعت نامعلوم) فایل خودش را دارد.
 * @‎param  char *char__path [EN] output / [FA]‎ خروجی
 * @‎param  uint32_t uint32_t__dayCode [EN] YYYYMMDD or 0 / [FA]‎ کد روز
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_PathOf(char *char__path, uint32_t uint32_t__dayCode)
{
    (void)snprintf(char__path, ESP_LOG_PATH_MAX, "%s%08lu.csv",
                   ESP_LOG_FILE_PREFIX, (unsigned long)uint32_t__dayCode);
}

/**
 * @brief  [EN] The leading number of a CSV row (its epoch field).
 *         [FA] عدد ابتدای یک ردیف (ستون زمان آن).
 * @‎param  const char *char__row [EN] row / [FA]‎ ردیف
 * @return [EN] uint32_t epoch, 0 when the row has no clock / [FA]‎ زمان
 */
static uint32_t func__EspLog_RowEpoch(const char *char__row)
{
    uint32_t uint32_t__value = 0u;
    uint8_t uint8_t__index = 0u;

    if (char__row == NULL)
    {
        return 0u;
    }
    while ((uint8_t__index < 10u) &&
           (char__row[uint8_t__index] >= '0') &&
           (char__row[uint8_t__index] <= '9'))
    {
        uint32_t__value = (uint32_t__value * 10u) +
                          (uint32_t)(char__row[uint8_t__index] - '0');
        uint8_t__index++;
    }
    return uint32_t__value;
}

/* ==================== Clock storage / ذخیرهٔ ساعت ==================== */

/**
 * @brief  [EN] Persist the clock anchor (every 10 minutes and on adoption).
 *         [FA] ذخیرهٔ لنگر ساعت (هر ۱۰ دقیقه و هنگام گرفتن ساعت).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_ClockSave(void)
{
    File file__clock;
    uint32_t uint32_t__nowSec;
    char char__line[64];

    if ((BOOL__G__FsOk == false) || (BOOL__G__LogClockValid == false))
    {
        return;
    }
    uint32_t__nowSec = func__EspLog_NowSec();
    if (uint32_t__nowSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "w");
    if (!file__clock)
    {
        return;
    }
    /* [EN] Three fields: the wall clock, the timezone, and the BOARD uptime the
            wall clock belongs to. With those three, a panel that has just been
            switched on knows the time again without asking anybody.
       [FA] سه فیلد: ساعت، منطقهٔ زمانی، و آپتایمِ بردی که این ساعت به آن بسته
            است. با همین سه عدد، پنلی که تازه روشن شده بی‌آنکه از کسی بپرسد
            دوباره وقت را می‌داند. */
    /* [EN] v1.113: a fourth field carries the firmware version, so the next
            boot can tell "switched on again" from "flashed with new firmware"
            (user order 2026-10-28). It rides in a file that already exists and
            is never erased by clearing the history, so no new file is needed.
       [FA] نسخهٔ ۱٫۱۱۳: فیلد چهارم نسخهٔ فرم‌ور را می‌برد تا بوت بعدی بتواند
            «دوباره روشن شد» را از «فرم‌ور تازه فلش شد» تشخیص دهد (دستور کاربر
            ۲۰۲۶-۱۰-۲۸). سوار همان فایلی است که از قبل هست و با پاک‌کردن
            تاریخچه هم پاک نمی‌شود، پس فایل تازه‌ای لازم نیست. */
    (void)snprintf(char__line, sizeof(char__line), "%lu,%ld,%lu,%u\n",
                   (unsigned long)uint32_t__nowSec, (long)INT32_T__G__LogTzSec,
                   (unsigned long)((BOOL__G__LogAnchorBoardValid != false) ?
                                   UINT32_T__G__LogAnchorBoardSec : 0u),
                   (unsigned int)ESP_ESP_FW_VERSION);
    (void)file__clock.print(char__line);
    file__clock.close();
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
    BOOL__G__LogClockSavedOnce = true;
}

/**
 * @brief  [EN] Read the clock anchor back after a restart.
 *         [FA] خواندن دوبارهٔ لنگر ساعت بعد از روشن‌شدن.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_ClockLoad(void)
{
    File file__clock;
    String string__content;
    int32_t int32_t__split;
    uint32_t uint32_t__savedSec;

    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_CLOCK_PATH) == false))
    {
        return;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
    if (!file__clock)
    {
        return;
    }
    string__content = file__clock.readString();
    file__clock.close();
    int32_t__split = string__content.indexOf(',');
    if (int32_t__split <= 0)
    {
        return;
    }
    uint32_t__savedSec = (uint32_t)string__content.substring(0, int32_t__split).toInt();
    if (uint32_t__savedSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return;
    }
    UINT32_T__G__LogAnchorSec = uint32_t__savedSec;
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    {
        String string__rest = string__content.substring(int32_t__split + 1);
        int32_t int32_t__second = string__rest.indexOf(',');

        if (int32_t__second > 0)
        {
            INT32_T__G__LogTzSec = (int32_t)string__rest.substring(0, int32_t__second).toInt();
            /* [EN] v1.113: a fourth field (the firmware stamp) follows the
                    board anchor, so the anchor stops at the next comma.
               [FA] نسخهٔ ۱٫۱۱۳: فیلد چهارم (مهر فرم‌ور) بعد از لنگر برد
                    می‌آید، پس لنگر به کامای بعدی ختم می‌شود. */
            {
                String string__board = string__rest.substring(int32_t__second + 1);
                int32_t int32_t__third = string__board.indexOf(',');

                if (int32_t__third >= 0)
                {
                    string__board = string__board.substring(0, int32_t__third);
                }
                UINT32_T__G__LogAnchorBoardSec = (uint32_t)string__board.toInt();
            }
        }
        else
        {
            INT32_T__G__LogTzSec = (int32_t)string__rest.toInt();
            UINT32_T__G__LogAnchorBoardSec = 0u;
        }
    }
    /* [EN] An old file with two fields still loads; it simply has no board
            anchor, and the first HELLO_ACK makes one.
       [FA] فایل قدیمی دو‌فیلدی هم خوانده می‌شود؛ فقط لنگر برد ندارد و اولین
            HELLO_ACK لنگر را می‌سازد. */
    BOOL__G__LogAnchorBoardValid = (UINT32_T__G__LogAnchorBoardSec != 0u);
    BOOL__G__LogClockValid = true;
    UINT32_T__G__LogHighWaterSec = uint32_t__savedSec;
}

/* ==================== Firmware stamp / مهر نسخهٔ فرم‌ور ==================== */

/**
 * @brief  [EN] Where does field N (1-based, N>1) start in a comma separated
 *              line? -1 when the line has fewer fields.
 *         [FA] فیلد شمارهٔ N (از یک، برای ‎N>1)‎ در یک خط جداشده با کاما از کجا
 *              شروع می‌شود؟ اگر خط فیلد کمتری داشته باشد، منفی یک.
 * @‎param  String string__line [EN] line / [FA]‎ خط
 * @‎param  uint8_t uint8_t__field [EN] field number / [FA]‎ شمارهٔ فیلد
 * @‎return [EN] start index / [FA]‎ اندیس شروع
 */
static int32_t func__EspLog_FieldStart(String string__line, uint8_t uint8_t__field)
{
    int32_t int32_t__offset = 0;
    uint8_t uint8_t__seen = 1u;

    while (uint8_t__seen < uint8_t__field)
    {
        int32_t int32_t__comma = (int32_t)string__line.indexOf(',');

        if (int32_t__comma < 0)
        {
            return -1;
        }
        int32_t__offset += int32_t__comma + 1;
        string__line = string__line.substring((size_t)int32_t__comma + 1u);
        uint8_t__seen++;
    }
    return int32_t__offset;
}

/**
 * @brief  [EN] Fourth field of the clock file: which firmware version wrote it
 *              last. 0 = nothing remembered yet (a fresh flash), never "update".
 *         [FA] فیلد چهارم فایل ساعت: آخرین بار کدام نسخهٔ فرم‌ور نوشته است.
 *              صفر یعنی «هنوز چیزی به یاد نیست» (فلش تازه)، هرگز «بروزرسانی».
 * @‎return [EN] uint32_t version / [FA]‎ نسخه
 */
static uint32_t func__EspLog_FwStampRead(void)
{
    String string__content;
    int32_t int32_t__start;
    File file__clock;

    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_CLOCK_PATH) == false))
    {
        return 0u;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
    if (!file__clock)
    {
        return 0u;
    }
    string__content = file__clock.readString();
    file__clock.close();
    /* [EN] the fourth field starts after the third comma
       [FA] فیلد چهارم بعد از سومین کاما شروع می‌شود */
    int32_t__start = func__EspLog_FieldStart(string__content, 4u);
    if (int32_t__start < 0)
    {
        return 0u;
    }
    {
        String string__value = string__content.substring((size_t)int32_t__start);
        string__value.trim();
        if (string__value.length() == 0u)
        {
            return 0u;
        }
        return (uint32_t)string__value.toInt();
    }
}

/**
 * @brief  [EN] Rewrite the clock file with the running firmware version as its
 *              fourth field. The first three fields are kept exactly as they
 *              are, so this can never lose or rewind the clock.
 *         [FA] بازنویسی فایل ساعت با نسخهٔ فرم‌ور در حال اجرا به‌عنوان فیلد
 *              چهارم. سه فیلد اول عیناً همان‌طور که هستند می‌مانند، پس این کار
 *              هرگز ساعت را پاک یا عقب نمی‌برد.
 * @‎param  uint32_t uint32_t__version [EN] version / [FA]‎ نسخه
 * @‎return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FwStampWrite(uint32_t uint32_t__version)
{
    String string__content = String("0,0,0");
    String string__firstThree;
    int32_t int32_t__start;
    char char__line[80];
    File file__clock;

    if (BOOL__G__FsOk == false)
    {
        return;
    }
    if (LittleFS.exists(ESP_LOG_CLOCK_PATH) != false)
    {
        file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
        if (file__clock)
        {
            string__content = file__clock.readString();
            file__clock.close();
        }
    }
    string__content.trim();
    int32_t__start = func__EspLog_FieldStart(string__content, 4u);
    /* [EN] keep the three fields before the stamp; with no fourth field the
            whole line is kept.
       [FA] سه فیلد پیش از مهر نگه داشته می‌شود؛ اگر فیلد چهارم نبود، کل خط. */
    string__firstThree = (int32_t__start > 1) ?
                         string__content.substring(0, (size_t)(int32_t__start - 1)) :
                         String("0,0,0");
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "w");
    if (!file__clock)
    {
        return;
    }
    (void)snprintf(char__line, sizeof(char__line), "%s,%u\n",
                   string__firstThree.c_str(), (unsigned int)uint32_t__version);
    (void)file__clock.print(char__line);
    file__clock.close();
}

/**
 * @brief  [EN] Is this boot the first one of a new firmware? The remembered
 *              version is compared with the running one; "nothing remembered"
 *              is a fresh flash and is not an update. Either way the stamp is
 *              brought up to date.
 *         [FA] آیا این بوت اولین بوت یک فرم‌ور تازه است؟ نسخهٔ به‌یادسپرده با
 *              نسخهٔ در حال اجرا مقایسه می‌شود؛ «چیزی به یاد نیست» یعنی فلش
 *              تازه و بروزرسانی حساب نمی‌شود. در هر دو حالت مهر تازه می‌شود.
 * @‎return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FwCheck(void)
{
    uint32_t uint32_t__stored;

    uint32_t__stored = func__EspLog_FwStampRead();
    if ((uint32_t__stored != 0u) && (uint32_t__stored != (uint32_t)ESP_ESP_FW_VERSION))
    {
        BOOL__G__LogUpdatePending = true;
    }
    if (uint32_t__stored != (uint32_t)ESP_ESP_FW_VERSION)
    {
        func__EspLog_FwStampWrite((uint32_t)ESP_ESP_FW_VERSION);
    }
}

/**
 * @brief  [EN] Adopt the phone's clock (POST /clock). A time that would move the
 *              clock BACKWARD by more than two minutes is refused: two phones, a
 *              wrong timezone or a manual clock change must not rewind the day
 *              and split the history into two files with the same rows again.
 *         [FA] گرفتن ساعت گوشی. زمانی که ساعت را بیش از دو دقیقه عقب ببرد رد
 *              می‌شود: دو گوشی، منطقهٔ زمانی اشتباه یا تغییر دستی ساعت نباید
 *              روز را عقب ببرد و تاریخچه را دو تکه کند.
 * @‎param  uint32_t uint32_t__epochSec [EN] epoch seconds / [FA]‎ ثانیهٔ یونیکس
 * @‎param  int32_t int32_t__tzSec [EN] UTC offset in seconds / [FA]‎ اختلاف با UTC
 * @‎return [EN] bool, false when the time is not believable / [FA]‎ پذیرفته شد
 */
static bool func__EspLog_TimeSet(uint32_t uint32_t__epochSec, int32_t int32_t__tzSec)
{
    uint32_t uint32_t__nowSec;
    bool bool__jump = false;

    if (uint32_t__epochSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return false;
    }
    if ((int32_t__tzSec < -50400) || (int32_t__tzSec > 50400))
    {
        int32_t__tzSec = 0;
    }
    /* [EN] The browser is the boss of the clock (user order 2026-10-27): every
            time a page connects, the clock becomes the time of that machine. A
            small difference is a normal re-sync and stays silent. A big one - a
            wrong clock from yesterday, a phone in another country, a board that
            was off for a day - is applied AND written as its own row, so the
            jump in the history always explains itself.
       [FA] مرورگر ارباب ساعت است (دستور کاربر ۲۰۲۶-۱۰-۲۷): هر بار صفحه وصل
            شود، ساعت همان دستگاه می‌شود. اختلاف کوچک یک هم‌زمان‌سازی معمولی
            است و بی‌صدا می‌ماند. اختلاف بزرگ - ساعت غلطِ دیروز، گوشی در کشور
            دیگر، یا بردی که یک روز خاموش بوده - هم اعمال می‌شود و هم ردیف خودش
            را می‌نویسد تا پرشِ تاریخ در تاریخچه توضیح داشته باشد. */
    if (BOOL__G__LogClockValid != false)
    {
        uint32_t__nowSec = func__EspLog_NowSec();
        if ((uint32_t__nowSec > uint32_t__epochSec) &&
            ((uint32_t__nowSec - uint32_t__epochSec) > ESP_LOG_CLOCK_REWIND_SEC))
        {
            BOOL__G__LogClockJumpPending = true;
            bool__jump = true;
        }
        else if ((uint32_t__epochSec > uint32_t__nowSec) &&
                 ((uint32_t__epochSec - uint32_t__nowSec) > ESP_LOG_CLOCK_REWIND_SEC))
        {
            BOOL__G__LogClockJumpPending = true;
            bool__jump = true;
        }
        else
        {
            /* normal re-sync: a few seconds, nothing to report */
        }
    }
    UINT32_T__G__LogAnchorSec = uint32_t__epochSec;
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    INT32_T__G__LogTzSec = int32_t__tzSec;
    BOOL__G__LogClockValid = true;
    UINT32_T__G__LogHighWaterSec = uint32_t__epochSec;
    /* [EN] The phone says WHAT time it is; the board says how time passes from
            here on. Both anchors start at this instant.
       [FA] گوشی می‌گوید «چند ساعت است»؛ برد می‌گوید از این‌جا به بعد زمان
            چطور می‌گذرد. هر دو لنگر از همین لحظه شروع می‌کنند. */
    if ((BOOL__G__LogBoardAlive != false) && (UINT32_T__G__LogBoardNowSec != 0u))
    {
        UINT32_T__G__LogAnchorBoardSec = UINT32_T__G__LogBoardNowSec;
        BOOL__G__LogAnchorBoardValid = true;
    }
    else
    {
        BOOL__G__LogAnchorBoardValid = false;
    }
    /* [EN] v1.110 (finding of the line-by-line review, 2026-10-28): the page
            sets the clock on every poll - once a minute - and this function
            saved the anchor every time, so the flash was written 1440 times a
            day instead of the 144 that ESP_LOG_CLOCK_SAVE_MS asks for. A small
            re-sync now lives in RAM only; the anchor is saved when the clock
            really jumped, when it has never been saved, or when the 10-minute
            policy says so. A restart in between loses at most those minutes,
            and the first page that connects sets the clock again.
       [FA] نسخهٔ ۱٫۱۱۰ (یافتهٔ مرور خط‌به‌خط، ۲۰۲۶-۱۰-۲۸): صفحه با هر پرسش -
            هر یک دقیقه - ساعت را تنظیم می‌کند و این تابع هر بار لنگر را ذخیره
            می‌کرد، یعنی فلش روزی ۱۴۴۰ بار نوشته می‌شد به‌جای ۱۴۴ باری که
            ESP_LOG_CLOCK_SAVE_MS می‌خواهد. حالا هم‌زمان‌سازی کوچک فقط در رم
            می‌ماند و لنگر وقتی ذخیره می‌شود که ساعت واقعاً پرش کرده باشد، یا
            هرگز ذخیره نشده باشد، یا سیاست ۱۰ دقیقه‌ای بگوید. ری‌استارت در
            این فاصله حداکثر همان چند دقیقه را از دست می‌دهد و اولین صفحه‌ای
            که وصل شود دوباره ساعت را می‌گذارد. */
    if ((bool__jump != false) || (BOOL__G__LogClockSavedOnce == false) ||
        ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastClockSaveMs) >= ESP_LOG_CLOCK_SAVE_MS))
    {
        func__EspLog_ClockSave();
    }
    return true;
}

/* ==================== Flash write / نوشتن روی فلش ==================== */

/**
 * @brief  [EN] Write one text piece and REPORT whether every byte landed.
 *              print() already returns the count it really wrote, so a full or
 *              damaged flash is visible here instead of silent.
 *         [FA] نوشتن یک تکه متن و گزارش این‌که همهٔ بایت‌ها نشستند یا نه.
 *              خود print تعداد بایت نوشته‌شده را برمی‌گرداند، پس فلش پر یا
 *              خراب همین‌جا دیده می‌شود نه بی‌صدا.
 * @‎param  File &file__target [EN] open file / [FA]‎ فایل باز
 * @‎param  const char *char__text [EN] text / [FA]‎ متن
 * @return [EN] bool, true when the whole text was written / [FA]‎ نوشته شد
 */
static bool func__EspLog_FileText(File &file__target, const char *char__text)
{
    size_t size_t__want;
    size_t size_t__got;

    if (char__text == NULL)
    {
        return false;
    }
    size_t__want = strlen(char__text);
    if (size_t__want == 0u)
    {
        return true;
    }
    size_t__got = file__target.print(char__text);
    return (size_t__got == size_t__want);
}

/**
 * @brief  [EN] Remember that a flash write failed (saturating, so a flash that
 *              is full forever cannot wrap the counter back to "healthy").
 *         [FA] ثبت این‌که نوشتن روی فلش شکست خورد (اشباع‌شونده، تا فلشی که
 *              همیشه پر است شمارنده را به «سالم» برنگرداند).
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FaultRaise(void)
{
    if (UINT8_T__G__LogWriteFault < 0xFFu)
    {
        UINT8_T__G__LogWriteFault++;
    }
}

/**
 * @brief  [EN] Keep a row that the flash would not take, newest wins. Row N+1
 *              overwriting row 1 is deliberate: the newest state is what an
 *              operator needs, and the loss is counted, not hidden.
 *         [FA] نگه‌داشتن ردیفی که فلش نپذیرفت؛ جدیدترین می‌ماند. اینکه ردیف
 *              تازه جای قدیمی‌ترین را بگیرد عمدی است: وضعیت تازه چیزی است که
 *              اپراتور لازم دارد و ازدست‌رفتنش شمرده می‌شود، پنهان نمی‌شود.
 * @‎param  const char *char__row [EN] one CSV row / [FA]‎ یک ردیف
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_RamPush(const char *char__row)
{
    if (char__row == NULL)
    {
        return;
    }
    if (UINT8_T__G__LogRamCount >= ESP_LOG_RAM_ROWS)
    {
        if (UINT16_T__G__LogDroppedRows < 0xFFFFu)
        {
            UINT16_T__G__LogDroppedRows++;
        }
    }
    (void)strncpy(CHAR__G__LogRamRow[UINT8_T__G__LogRamHead],
                  char__row,
                  ESP_LOG_RAM_ROW_MAX - 1u);
    CHAR__G__LogRamRow[UINT8_T__G__LogRamHead][ESP_LOG_RAM_ROW_MAX - 1u] = '\0';
    UINT8_T__G__LogRamHead = (uint8_t)((UINT8_T__G__LogRamHead + 1u) % ESP_LOG_RAM_ROWS);
    if (UINT8_T__G__LogRamCount < ESP_LOG_RAM_ROWS)
    {
        UINT8_T__G__LogRamCount++;
    }
}

/* ==================== Day list / فهرست روزها ==================== */

/* [EN] v1.110 (user order 2026-10-28: "why not take it from the log file
        itself?"): the day list is READ FROM THE FLASH, not stored. One scan of
        "/" collects every day file, sorts it by day and remembers each file's
        size, so the day picker, the download and the retention all look at the
        real files. The old list file is gone: a list written beside the rows
        can fall out of step with them - a panel that was switched off came back
        with an EMPTY list while every day file was still on the flash, and a
        stale list also meant the retention never deleted anything.
   [FA] نسخهٔ ۱٫۱۱۰ (دستور کاربر ۲۰۲۶-۱۰-۲۸: «چرا از خود فایل لاگ برنمی‌داری؟»):
        فهرست روزها از خود فلش خوانده می‌شود، نه ذخیره. یک بار خواندن پوشهٔ «/»
        همهٔ فایل‌های روز را جمع می‌کند، به ترتیب روز می‌چیند و حجم هر فایل را
        نگه می‌دارد؛ پس فهرست روزهای پنل، دانلود و پاک‌سازی قدیمی‌ها همه
        فایل‌های واقعی را می‌بینند. فایل فهرست حذف شد: فهرستی که کنار ردیف‌ها
        نوشته شود می‌تواند از آن‌ها عقب بماند - پنلی که خاموش و روشن شد با
        فهرست خالی برگشت در حالی که همهٔ فایل‌های روز سر جایشان بودند، و فهرست
        کهنه یعنی پاک‌سازی هم هیچ‌وقت چیزی را حذف نمی‌کرد. */

/* [EN] When the directory was last read, so a page that refreshes every 10 s
        does not make the flash walk the directory every 10 s as well.
   [FA] زمان آخرین خواندن پوشه، تا صفحه‌ای که هر ۱۰ ثانیه تازه می‌شود پوشه را
        هم هر ۱۰ ثانیه از فلش نخواند. */
static uint32_t UINT32_T__G__LogLastScanMs = 0u;
static bool     BOOL__G__LogScanValid = false;

/**
 * @brief  [EN] Day code of one file name, or false when the name is not a day
 *              file. Only the day-file shape counts, so the clock file, the
 *              bench log and any leftover file are never read as days.
 *         [FA] کد روز از یک نام فایل، یا false وقتی نام، فایل روز نیست. فقط
 *              شکل نام فایل روز حساب می‌شود، پس فایل ساعت، لاگ بنچ و هر فایل
 *              جاماندهٔ دیگر هرگز به‌جای روز خوانده نمی‌شوند.
 * @‎param  const char *char__ptr_name [EN] name from the directory / [FA] نام از پوشه
 * @‎param  uint32_t *uint32_t__ptr_day [EN] output day code / [FA] کد روز خروجی
 * @return [EN] bool / [FA]‎ درست
 */
static bool func__EspLog_DayFromName(const char *char__ptr_name,
                                     uint32_t *uint32_t__ptr_day)
{
    uint8_t uint8_t__index = 0u;
    uint8_t uint8_t__digits = 0u;
    uint32_t uint32_t__day = 0u;

    if ((char__ptr_name == NULL) || (uint32_t__ptr_day == NULL))
    {
        return false;
    }
    while ((ESP_LOG_FILE_PREFIX[uint8_t__index] != '\0') &&
           (char__ptr_name[uint8_t__index] == ESP_LOG_FILE_PREFIX[uint8_t__index]))
    {
        uint8_t__index++;
    }
    if (ESP_LOG_FILE_PREFIX[uint8_t__index] != '\0')
    {
        return false;
    }
    while ((uint8_t__digits < 8u) &&
           (char__ptr_name[uint8_t__index] >= '0') &&
           (char__ptr_name[uint8_t__index] <= '9'))
    {
        uint32_t__day = (uint32_t__day * 10u) +
                        (uint32_t)(char__ptr_name[uint8_t__index] - '0');
        uint8_t__digits++;
        uint8_t__index++;
    }
    if (uint8_t__digits != 8u)
    {
        return false;
    }
    if ((char__ptr_name[uint8_t__index] != '.') ||
        (char__ptr_name[uint8_t__index + 1u] != 'c') ||
        (char__ptr_name[uint8_t__index + 2u] != 's') ||
        (char__ptr_name[uint8_t__index + 3u] != 'v') ||
        (char__ptr_name[uint8_t__index + 4u] != '\0'))
    {
        return false;
    }
    *uint32_t__ptr_day = uint32_t__day;
    return true;
}

/**
 * @brief  [EN] Fill the day cache from one directory scan, oldest first. Two
 *              entries more than the retention keeps are taken, so the trim
 *              below always sees the oldest day and can delete it. A page that
 *              asks again inside ESP_LOG_SCAN_MIN_MS gets the cache without
 *              touching the flash.
 *         [FA] پر کردن حافظهٔ روزها با یک بار خواندن پوشه، قدیمی‌تر اول. دو
 *              خانه بیش از سقف نگه‌داری خوانده می‌شود تا پاک‌سازی همیشه
 *              قدیمی‌ترین روز را ببیند و بتواند حذفش کند. صفحه‌ای که داخل
 *              ESP_LOG_SCAN_MIN_MS دوباره بپرسد، پاسخ را از حافظه می‌گیرد و
 *              به فلش دست نمی‌زند.
 * @‎param  bool bool__force [EN] true = read the flash now / [FA] درست = همین حالا از فلش بخوان
 * @return [EN] uint8_t number of days found / [FA]‎ تعداد روزهای پیداشده
 */
static uint8_t func__EspLog_ScanDays(bool bool__force)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();

    if (BOOL__G__FsOk == false)
    {
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false;
        return 0u;
    }
    if ((bool__force == false) && (BOOL__G__LogScanValid != false) &&
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastScanMs) < ESP_LOG_SCAN_MIN_MS))
    {
        return UINT8_T__G__LogIndexCount;
    }

    UINT32_T__G__LogLastScanMs = uint32_t__nowMs;
    BOOL__G__LogScanValid = true;
    UINT8_T__G__LogIndexCount = 0u;

    {
        Dir dir__root = LittleFS.openDir("/");

        while (dir__root.next())
        {
            uint32_t uint32_t__day = 0u;
            uint32_t uint32_t__size = (uint32_t)dir__root.fileSize();
            uint8_t uint8_t__slot;

            if (func__EspLog_DayFromName(dir__root.fileName().c_str(),
                                         &uint32_t__day) == false)
            {
                continue;
            }
            if (UINT8_T__G__LogIndexCount >= (ESP_LOG_MAX_DAYS + 2u))
            {
                break;   /* the trim below removes the oldest of what is here */
            }
            uint8_t__slot = UINT8_T__G__LogIndexCount;
            while ((uint8_t__slot > 0u) &&
                   (UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] > uint32_t__day))
            {
                UINT32_T__G__LogIndexDay[uint8_t__slot] =
                    UINT32_T__G__LogIndexDay[uint8_t__slot - 1u];
                UINT32_T__G__LogIndexBytes[uint8_t__slot] =
                    UINT32_T__G__LogIndexBytes[uint8_t__slot - 1u];
                uint8_t__slot--;
            }
            UINT32_T__G__LogIndexDay[uint8_t__slot] = uint32_t__day;
            UINT32_T__G__LogIndexBytes[uint8_t__slot] = uint32_t__size;
            UINT8_T__G__LogIndexCount++;
        }
    }
    return UINT8_T__G__LogIndexCount;
}

/**
 * @brief  [EN] Newest day that really exists on the flash, or ESP_LOG_DAY_AUTO
 *              when there is none.
 *         [FA] جدیدترین روزی که واقعاً روی فلش هست، یا ESP_LOG_DAY_AUTO وقتی
 *              هیچ روزی نیست.
 * @‎return [EN] uint32_t day code or ESP_LOG_DAY_AUTO / [FA]‎ کد روز یا AUTO
 */
static uint32_t func__EspLog_NewestDay(void)
{
    (void)func__EspLog_ScanDays(false);
    if (UINT8_T__G__LogIndexCount == 0u)
    {
        return ESP_LOG_DAY_AUTO;
    }
    return UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
}

/**
 * @brief  [EN] Delete the oldest day file and its entry.
 *         [FA] حذف قدیمی‌ترین فایل روز و خانه‌اش.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_DropOldest(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;

    if (UINT8_T__G__LogIndexCount == 0u)
    {
        return;
    }
    func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[0]);
    (void)LittleFS.remove(char__path);
    for (uint8_t__slot = 1u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] =
            UINT32_T__G__LogIndexDay[uint8_t__slot];
        UINT32_T__G__LogIndexBytes[uint8_t__slot - 1u] =
            UINT32_T__G__LogIndexBytes[uint8_t__slot];
    }
    UINT8_T__G__LogIndexCount--;
}

/**
 * @brief  [EN] Delete the oldest days when the retention count or the byte
 *              budget is passed. Nothing has to be written down afterwards:
 *              the list is the directory, so the next scan reads what is left.
 *         [FA] حذف قدیمی‌ترین روزها وقتی تعداد روزهای نگه‌داری یا بودجهٔ بایتی
 *              رد شود. بعد از آن چیزی برای نوشتن نیست: فهرست همان پوشه است،
 *              پس خواندن بعدی هر چه مانده را می‌بیند.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_TrimDays(void)
{
    uint8_t uint8_t__slot;

    while (UINT8_T__G__LogIndexCount > ESP_LOG_MAX_DAYS)
    {
        func__EspLog_DropOldest();
    }

    /* [EN] v1.96: the day count alone does not bound the flash -
            a busy day is bigger than a quiet week. Keep the whole
            history inside one byte budget and drop the oldest day
            first, so a full flash can never stop the log.
       [FA] نسخهٔ ۱٫۹۶: تعداد روز تنها فلش را کران‌دار نمی‌کند -
            یک روز شلوغ از یک هفتهٔ آرام بزرگ‌تر است. کل تاریخچه
            داخل یک بودجهٔ بایتی نگه داشته می‌شود و اول قدیمی‌ترین
            روز می‌رود، تا پر شدن فلش هرگز لاگ را نخواباند. */
    while (UINT8_T__G__LogIndexCount > 1u)
    {
        uint32_t uint32_t__bytes = 0u;

        for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            uint32_t__bytes += UINT32_T__G__LogIndexBytes[uint8_t__slot];
        }
        if (uint32_t__bytes <= ESP_LOG_TOTAL_BUDGET_BYTES)
        {
            break;
        }
        func__EspLog_DropOldest();
        BOOL__G__LogScanValid = false;
    }
}

/**
 * @brief  [EN] Count the data rows already in a day file (after a restart).
 *         [FA] شمردن ردیف‌های دادهٔ موجود یک فایل روز (بعد از روشن‌شدن).
 * @‎param  const char *char__path [EN] file / [FA]‎ فایل
 * @return [EN] uint16_t rows / [FA]‎ تعداد ردیف
 */
static uint16_t func__EspLog_CountRows(const char *char__path)
{
    File file__day;
    uint16_t uint16_t__rows = 0u;

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        if ((string__line.length() > 0u) && (string__line[0] != '#'))
        {
            uint16_t__rows++;
        }
    }
    file__day.close();
    return uint16_t__rows;
}

/**
 * @brief  [EN] A power cut in the middle of a row leaves no closing newline, so
 *              the next append would glue a good row onto a broken one and make
 *              BOTH unreadable. This runs once when a day file is opened: if the
 *              last byte is not a newline, one is added first.
 *         [FA] قطع برق وسط یک ردیف، پایان‌بند را جا می‌گذارد؛ در آن حالت ردیف
 *              بعدی به ردیف خراب می‌چسبد و هر دو ناخوانا می‌شوند. این تابع
 *              یک‌بار هنگام باز شدن فایل روز اجرا می‌شود: اگر آخرین بایت
 *              پایان‌بند نبود، اول یکی نوشته می‌شود.
 * @‎param  const char *char__path [EN] day file / [FA]‎ فایل روز
 * @return [EN] bool, true when the file ends on a line boundary / [FA]‎ سالم
 */
static bool func__EspLog_RepairTail(const char *char__path)
{
    File file__day;
    uint32_t uint32_t__size;
    bool bool__needsNewline = false;

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return true;   /* nothing to repair, and the next append reports itself */
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size > 0u)
    {
        if ((file__day.seek(uint32_t__size - 1u) == false) || (file__day.read() != '\n'))
        {
            bool__needsNewline = true;
        }
    }
    file__day.close();
    if (bool__needsNewline == false)
    {
        return true;
    }

    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    bool__needsNewline = (func__EspLog_FileText(file__day, "\n") != false);
    file__day.close();
    if (bool__needsNewline == false)
    {
        func__EspLog_FaultRaise();
    }
    return bool__needsNewline;
}

/**
 * @brief  [EN] THE one append path: one verified row plus its newline into one
 *              day file. Nothing else in this file writes a row.
 *         [FA] تنها مسیر نوشتن: یک ردیف تأییدشده به‌همراه پایان‌بندش در فایل
 *              یک روز. هیچ جای دیگری در این فایل ردیف نمی‌نویسد.
 * @‎param  uint32_t uint32_t__dayCode [EN] day / [FA]‎ روز
 * @‎param  const char *char__row [EN] row / [FA]‎ ردیف
 * @return [EN] bool written and counted / [FA]‎ نوشته شد
 */
static bool func__EspLog_AppendRow(uint32_t uint32_t__dayCode, const char *char__row)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    bool bool__ok;

    if ((BOOL__G__FsOk == false) || (char__row == NULL))
    {
        return false;
    }
    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    bool__ok = func__EspLog_FileText(file__day, char__row);
    if (bool__ok != false)
    {
        bool__ok = func__EspLog_FileText(file__day, "\n");
    }
    file__day.close();
    if (bool__ok == false)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    return true;
}

/**
 * @brief  [EN] Make sure today's file exists and its row counter is right.
 *         [FA] مطمئن شو فایل امروز هست و شمارندهٔ ردیفش درست است.
 * @return [EN] bool, false when the day file could not be opened or written
 *             / [FA]‎ آماده‌بودن
 */
static bool func__EspLog_OpenDay(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    bool bool__isNew;
    bool bool__ok = true;
    uint32_t uint32_t__dayCode;

    if (BOOL__G__FsOk == false)
    {
        return false;
    }
    uint32_t__dayCode = func__EspLog_TodayCode();
    if (uint32_t__dayCode == UINT32_T__G__LogDayCode)
    {
        return true;
    }

    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    bool__isNew = (LittleFS.exists(char__path) == false);
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    if (bool__isNew)
    {
        bool__ok = func__EspLog_FileText(file__day, ESP_LOG_HEADER);
    }
    file__day.close();

    UINT32_T__G__LogDayCode = uint32_t__dayCode;
    if ((bool__ok != false) && (bool__isNew == false))
    {
        bool__ok = func__EspLog_RepairTail(char__path);
    }
    if (bool__ok == false)
    {
        func__EspLog_FaultRaise();
    }

    UINT16_T__G__LogRowsToday = func__EspLog_CountRows(char__path);
    BOOL__G__LogDayFull = (UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY);
    BOOL__G__LogMarkerDone = BOOL__G__LogDayFull;

    /* [EN] the day list is the directory itself: read it again now that this
            day's file exists, then drop the oldest days if the count or the
            byte budget says so.
       [FA] فهرست روزها همان پوشه است: حالا که فایل امروز ساخته شد دوباره
            خوانده می‌شود، بعد اگر تعداد یا بودجهٔ بایتی حکم کرد قدیمی‌ترین
            روزها حذف می‌شوند. */
    (void)func__EspLog_ScanDays(true);
    func__EspLog_TrimDays();
    return bool__ok;
}

/**
 * @brief  [EN] Move the rows the RAM ring is holding back into the day files they
 *              belong to (each row by its OWN epoch, so a row logged before the
 *              phone clock arrived still lands in the day-zero file). Oldest
 *              first; it stops at the first refusal and keeps the rest, so a
 *              flash that is still full loses nothing.
 *         [FA] برگرداندن ردیف‌هایی که حلقهٔ رم نگه داشته به فایل روزِ خودشان
 *              (هر ردیف با زمان خودش، پس ردیفی که پیش از رسیدن ساعت گوشی ثبت
 *              شده در فایل روز صفر می‌نشیند). از قدیمی‌ترین شروع می‌شود و با
 *              اولین رد شدن می‌ایستد و بقیه را نگه می‌دارد، پس فلشی که هنوز
 *              پر است چیزی از دست نمی‌دهد.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_DrainRam(void)
{
    while (UINT8_T__G__LogRamCount > 0u)
    {
        uint8_t uint8_t__oldest;
        uint32_t uint32_t__epoch;
        uint32_t uint32_t__dayCode;

        uint8_t__oldest = (uint8_t)((UINT8_T__G__LogRamHead + ESP_LOG_RAM_ROWS -
                                     UINT8_T__G__LogRamCount) % ESP_LOG_RAM_ROWS);
        uint32_t__epoch = func__EspLog_RowEpoch(CHAR__G__LogRamRow[uint8_t__oldest]);
        if (uint32_t__epoch >= ESP_LOG_CLOCK_VALID_MIN)
        {
            uint32_t__dayCode = func__EspLog_DateCode(func__EspLog_LocalDay(uint32_t__epoch));
        }
        else
        {
            uint32_t__dayCode = 0u;
        }
        if (func__EspLog_AppendRow(uint32_t__dayCode,
                                   CHAR__G__LogRamRow[uint8_t__oldest]) == false)
        {
            return;   /* the flash is still not taking rows: keep them */
        }
        /* [EN] a drained row takes a slot in today's file like any other, so
                the day cap must count it - otherwise the file could grow past
                the cap the counter believes in.
           [FA] ردیف برگردانده‌شده هم مثل بقیه یک خانه از فایل امروز را
                می‌گیرد، پس سقف روز باید آن را بشمارد؛ وگرنه فایل از سقفی که
                شمارنده باور دارد بزرگ‌تر می‌شود. */
        if ((uint32_t__dayCode == UINT32_T__G__LogDayCode) &&
            (UINT16_T__G__LogRowsToday < 0xFFFFu))
        {
            UINT16_T__G__LogRowsToday++;
        }
        UINT8_T__G__LogRamCount--;
    }
}

/* ==================== Row write / نوشتن ردیف ==================== */

/**
 * @brief  [EN] Battery percent from the DISCHARGE map (param 74/75, factory
 *              21,000..29,000 mV), stored with every row so later analysis
 *              keeps the same scale even if the map is retuned. Unknown params
 *              fall back to the factory map.
 *         [FA] درصد باتری از نگاشت «دشارژ» (پارامتر ۷۴/۷۵، کارخانه
 *              ۲۱٬۰۰۰..۲۹٬۰۰۰ میلی‌ولت) که با هر ردیف ذخیره می‌شود تا تحلیل
 *              بعدی همان مقیاس را نگه دارد، حتی اگر نگاشت بعدا تنظیم شود.
 *              اگر پارامتر هنوز نرسیده باشد، نگاشت کارخانه به کار می‌رود.
 * @param  uint32_t__batteryMv [EN] Pack voltage in mV / ولتاژ پک
 * @return uint8_t [EN] Percent 0..100‎ / درصد
 */
static uint8_t func__EspLog_Pct(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__vminMv = 21000u;
    uint32_t uint32_t__vmaxMv = 29000u;
    uint32_t uint32_t__rangeMv;

    if (BOOL__G__ParamKnown[74] != false)
    {
        uint32_t__vminMv = UINT32_T__G__ParamApplied[74];
    }
    if (BOOL__G__ParamKnown[75] != false)
    {
        uint32_t__vmaxMv = UINT32_T__G__ParamApplied[75];
    }
    if (uint32_t__batteryMv <= uint32_t__vminMv)
    {
        return 0u;
    }
    if (uint32_t__batteryMv >= uint32_t__vmaxMv)
    {
        return 100u;
    }
    uint32_t__rangeMv = uint32_t__vmaxMv - uint32_t__vminMv;
    if (uint32_t__rangeMv == 0u)
    {
        return 0u;
    }
    return (uint8_t)(((uint32_t__batteryMv - uint32_t__vminMv) * 100u) / uint32_t__rangeMv);
}

/**
 * @brief  [EN] Build one CSV row into a caller buffer. Returns false when the
 *              row did not fit - a cut row is reported instead of written.
 *         [FA] ساخت یک ردیف CSV در بافر. اگر جا نشود false می‌دهد: ردیفِ
 *              بریده گزارش می‌شود نه نوشته.
 * @‎param  char *char__row [EN] buffer / [FA]‎ بافر
 * @‎param  size_t size_t__size [EN] buffer size / [FA]‎ اندازه
 * @‎param  uint16_t uint16_t__changeMask [EN] change bits / [FA]‎ بیت‌های تغییر
 * @‎param  uint8_t uint8_t__note [EN] note / [FA]‎ یادداشت
 * @return [EN] bool fits / [FA]‎ جا شد
 */
static bool func__EspLog_BuildRow(char *char__row, size_t size_t__size,
                                  uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    int int__written;

    int__written = snprintf(char__row, size_t__size,
                            "%lu,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                            (unsigned long)func__EspLog_NowSec(),
                            (unsigned long)((uint32_t)millis() / 1000u),
                            (unsigned int)uint16_t__changeMask,
                            (unsigned int)uint8_t__note,
                            (unsigned int)(((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u),
                            (unsigned int)UINT8_T__G__TlmFlags,
                            (unsigned long)UINT32_T__G__TlmField[6],
                            (unsigned long)UINT32_T__G__TlmField[13],
                            (unsigned long)UINT32_T__G__TlmField[19],
                            (unsigned int)UINT8_T__G__TlmFlags2,
                            (unsigned long)UINT32_T__G__TlmField[15],
                            (unsigned long)UINT32_T__G__TlmField[17],
                            (unsigned long)UINT32_T__G__TlmField[18],
                            (unsigned long)UINT32_T__G__TlmField[25],
                            (unsigned long)UINT32_T__G__TlmField[28],
                            (unsigned long)UINT32_T__G__TlmField[14],
                            (unsigned long)UINT32_T__G__TlmField[16],
                            (unsigned long)UINT32_T__G__TlmField[3],
                            (unsigned long)UINT32_T__G__TlmField[10],
                            (unsigned long)UINT32_T__G__TlmField[5],
                            (unsigned long)UINT32_T__G__TlmField[12],
                            (unsigned long)func__EspLog_Pct(UINT32_T__G__TlmField[15]));
    return (int__written > 0) && ((size_t)int__written < size_t__size);
}

/**
 * @brief  [EN] Append one row and close it at once, so a power cut right after
 *              the battery cut cannot lose it. The flash is tried first; when it
 *              refuses, the row goes to the RAM ring and the fault counter tells
 *              the page. The last row of a full day is a marker that says the
 *              cap was reached.
 *         [FA] یک ردیف را اضافه و همان لحظه می‌بندد، تا قطع برق درست بعد از
 *              قطع باتری آن را از بین نبرد. اول فلش امتحان می‌شود؛ اگر
 *              نپذیرد، ردیف به حلقهٔ رم می‌رود و شمارندهٔ خرابی به صفحه می‌گوید.
 *              آخرین ردیف روزِ پر، نشانه‌ای است که می‌گوید سقف پر شد.
 * @‎param  uint16_t uint16_t__changeMask [EN] what changed / [FA]‎ چه عوض شد
 * @‎param  uint8_t uint8_t__note [EN] row note / [FA]‎ یادداشت ردیف
 * @return [EN] bool stored somewhere / [FA]‎ ذخیره شد
 */
static bool func__EspLog_WriteRow(uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    char char__row[ESP_LOG_ROW_MAX];
    bool bool__stored = false;

    /* [EN] v1.110 (user report 2026-10-28: "it is on and it writes nothing"):
            the day cap used to be judged BEFORE the day was looked up, and the
            only place that notices a new day is OpenDay. So the first day that
            reached 400 rows blocked every later write and the new day's file
            was never created - the panel stayed silent until it was switched
            off and on again. The day is opened first, so a full day ends where
            it should: at midnight.
       [FA] نسخهٔ ۱٫۱۱۰ (گزارش کاربر ۲۰۲۶-۱۰-۲۸: «روشن است و چیزی نمی‌نویسد»):
            سقف روز «قبل از» پیدا کردن روزِ جاری بررسی می‌شد و تنها جایی که
            عوض شدن روز را می‌فهمد OpenDay است. پس اولین روزی که به ۴۰۰ ردیف
            می‌رسید هر نوشتن بعدی را می‌بست و فایل روز تازه هرگز ساخته
            نمی‌شد - پنل تا خاموش و روشن شدن بعدی ساکت می‌ماند. حالا اول روز
            باز می‌شود، پس روزِ پر همان‌جا که باید تمام می‌شود: نیمه‌شب. */
    if (BOOL__G__FsOk != false)
    {
        (void)func__EspLog_OpenDay();
    }
    /* [EN] v1.113: a forced row (the firmware-update row) ignores this cap as
            well - the user asked for it "with no conditions".
       [FA] نسخهٔ ۱٫۱۱۳: ردیف اجباری (ردیف بروزرسانی فرم‌ور) این سقف را هم
            نادیده می‌گیرد - دستور کاربر «بدون در نظر گرفتن شرط‌ها» بود. */
    if ((BOOL__A__LogForceRow == false) &&
        ((UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY) ||
         (BOOL__G__LogDayFull != false)))
    {
        BOOL__G__LogDayFull = true;
        return false;
    }
    if (func__EspLog_BuildRow(char__row, sizeof(char__row),
                              uint16_t__changeMask, uint8_t__note) == false)
    {
        func__EspLog_FaultRaise();
        return false;
    }

    if ((BOOL__G__FsOk != false) &&
        ((BOOL__G__LogDayFull == false) || (BOOL__A__LogForceRow != false)))
    {
        /* [EN] the flash is taking rows again: the ones waiting in RAM go
                first, so the history keeps its order.
           [FA] فلش دوباره ردیف می‌پذیرد: اول ردیف‌های منتظر در رم
                می‌روند تا ترتیب تاریخچه حفظ شود. */
        if (UINT8_T__G__LogRamCount != 0u)
        {
            func__EspLog_DrainRam();
        }
        bool__stored = func__EspLog_AppendRow(UINT32_T__G__LogDayCode, char__row);
    }

    if ((bool__stored == false) || (BOOL__G__FsOk == false))
    {
        func__EspLog_RamPush(char__row);
    }
    if (BOOL__G__FsOk != false)
    {
        if (bool__stored != false)
        {
            UINT8_T__G__LogWriteFault = 0u;   /* the flash works again */
        }
        else
        {
            func__EspLog_FaultRaise();
        }
    }

    /* [EN] The row counter is the day-file cap, so it only grows when the file
            really grew - a broken flash must not "fill" a file that stays empty.
            With the file system off there is no file, and the session count
            keeps its old meaning.
       [FA] شمارندهٔ ردیف سقف فایل روز است، پس فقط وقتی بالا می‌رود که فایل
            واقعاً بزرگ شده باشد - فلش خراب نباید فایلی را که خالی مانده «پر»
            کند. با فایل‌سیستم خاموش فایلی نیست و شمارش همان معنای قبلی را
            دارد. */
    if ((bool__stored != false) || (BOOL__G__FsOk == false))
    {
        if (UINT16_T__G__LogRowsToday < 0xFFFFu)
        {
            UINT16_T__G__LogRowsToday++;
        }
    }

    if ((uint16_t__changeMask & ESP_LOG_CHG_VSTEP) != 0u)
    {
        UINT32_T__G__LogLastStepMs = (uint32_t)millis();
        BOOL__G__LogLastStepValid = true;
    }
    UINT32_T__G__LogPrevV24 = UINT32_T__G__TlmField[15];
    UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    BOOL__G__LogLastRowValid = true;
    UINT16_T__G__LogPendingMask = 0u;
    UINT8_T__G__LogPendingNote = 0u;
    BOOL__G__LogMainsQueuedValid = false;
    BOOL__A__LogForceRow = false;   /* the one forced row has been written */

    /* [EN] one slot before the cap, the day writes its own obituary: a row with
            note 4 that says "the cap was reached, the rest of today is not
            recorded". Without it a full day looks like a dead panel.
       [FA] یک خانه پیش از سقف، روز خودش را توضیح می‌دهد: ردیفی با یادداشت ۴
            که می‌گوید «سقف پر شد و بقیهٔ امروز ثبت نمی‌شود». بدون آن، روزِ پر
            شبیه پنلِ مرده دیده می‌شود. */
    if ((bool__stored != false) && (BOOL__G__LogMarkerDone == false) &&
        (UINT16_T__G__LogRowsToday == (ESP_LOG_MAX_ROWS_PER_DAY - 1u)))
    {
        char char__marker[ESP_LOG_ROW_MAX];

        BOOL__G__LogMarkerDone = true;
        if (func__EspLog_BuildRow(char__marker, sizeof(char__marker),
                                  0u, ESP_LOG_NOTE_DAY_FULL) != false)
        {
            if (func__EspLog_AppendRow(UINT32_T__G__LogDayCode, char__marker) != false)
            {
                UINT16_T__G__LogRowsToday++;
            }
        }
        BOOL__G__LogDayFull = true;
        UINT16_T__G__LogPendingMask = 0u;
    }
    return true;
}

/**
 * @brief  [EN] One numeric field of a CSV row, by column index (0 = first).
 *              Missing columns read as 0, so a short row never invents values.
 *         [FA] یک فیلد عددی از یک ردیف CSV، بر اساس شمارهٔ ستون (۰ = اولی).
 *              ستون نبوده صفر خوانده می‌شود، پس ردیف کوتاه مقدار جعلی نمی‌سازد.
 * @‎param  const char *char__row [EN] one row / [FA]‎ یک ردیف
 * @‎param  uint8_t uint8_t__index [EN] column / [FA]‎ شمارهٔ ستون
 * @return [EN] value / [FA]‎ مقدار
 */
static uint32_t func__EspLog_RowFieldAt(const char *char__row, uint8_t uint8_t__index)
{
    uint8_t uint8_t__column = 0u;
    uint16_t uint16_t__cursor = 0u;
    uint32_t uint32_t__value = 0u;

    if (char__row == NULL)
    {
        return 0u;
    }
    while ((char__row[uint16_t__cursor] != '\0') &&
           (char__row[uint16_t__cursor] != '\n'))
    {
        if (char__row[uint16_t__cursor] == ',')
        {
            uint8_t__column++;
            uint16_t__cursor++;
            continue;
        }
        if (uint8_t__column == uint8_t__index)
        {
            if ((char__row[uint16_t__cursor] >= '0') &&
                (char__row[uint16_t__cursor] <= '9'))
            {
                uint32_t__value = (uint32_t__value * 10u) +
                                  (uint32_t)(char__row[uint16_t__cursor] - '0');
            }
        }
        uint16_t__cursor++;
    }
    return uint32_t__value;
}

/**
 * @brief  [EN] Read back how the world looked at the last row: the LAST LINE OF
 *              THE NEWEST DAY FILE is the memory. The log already carries the
 *              pack voltage, both halves, the fault mask, flags2, both charger
 *              phases and the mains bit, so no second file and no extra write per
 *              row is needed.
 *         [FA] خواندن این‌که دنیا در آخرین ردیف چه شکلی بود: حافظه، آخرین خطِ
 *              جدیدترین فایل روز است. خود لاگ ولتاژ پک، دو نیم‌باتری، ماسک
 *              خطا، flags2، مرحلهٔ هر دو شارژر و بیت برق شهر را دارد، پس نه
 *              فایل دوم لازم است و نه نوشتن اضافه با هر ردیف.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_LoadLastSnapshot(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    char char__tail[320];
    File file__day;
    uint32_t uint32_t__dayCode;
    uint32_t uint32_t__size;
    uint32_t uint32_t__start;
    size_t size_t__got = 0u;
    int32_t int32_t__end;
    int32_t int32_t__begin;
    uint8_t uint8_t__scan;
    uint32_t uint32_t__v24;

    BOOL__G__LogBootSnapValid = false;
    if (BOOL__G__FsOk == false)
    {
        return;
    }

    /* [EN] Newest day that really exists: today if it is there, otherwise the
            last day of the list.
       [FA] جدیدترین روزی که واقعاً هست: امروز اگر باشد، وگرنه آخرین روزِ
            فهرست. */
    uint32_t__dayCode = func__EspLog_TodayCode();
    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    if (LittleFS.exists(char__path) == false)
    {
        uint32_t__dayCode = func__EspLog_NewestDay();
        if (uint32_t__dayCode == ESP_LOG_DAY_AUTO)
        {
            return;
        }
        func__EspLog_PathOf(char__path, uint32_t__dayCode);
    }

    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return;
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size == 0u)
    {
        file__day.close();
        return;
    }
    uint32_t__start = (uint32_t__size > (uint32_t)(sizeof(char__tail) - 1u)) ?
                      (uint32_t__size - (uint32_t)(sizeof(char__tail) - 1u)) : 0u;
    (void)file__day.seek(uint32_t__start);
    while (size_t__got < (sizeof(char__tail) - 1u))
    {
        int int__byte = file__day.read();
        if (int__byte < 0)
        {
            break;
        }
        char__tail[size_t__got] = (char)int__byte;
        size_t__got++;
    }
    file__day.close();
    char__tail[size_t__got] = '\0';
    int32_t__end = (int32_t)size_t__got - 1;

    /* [EN] Skip trailing newlines, then find the start of the last LINE - and
            skip comment lines, so the file header can never be read as a row.
       [FA] پایان‌بندها رد می‌شوند، بعد شروع آخرین «خط» پیدا و خط‌های کامنت هم
            رد می‌شوند تا سرسطر فایل هرگز به‌جای ردیف خوانده نشود. */
    for (uint8_t__scan = 0u; uint8_t__scan < 8u; uint8_t__scan++)
    {
        while ((int32_t__end >= 0) &&
               ((char__tail[int32_t__end] == '\n') || (char__tail[int32_t__end] == '\r')))
        {
            int32_t__end--;
        }
        if (int32_t__end < 0)
        {
            return;
        }
        int32_t__begin = int32_t__end;
        while ((int32_t__begin > 0) && (char__tail[int32_t__begin - 1] != '\n'))
        {
            int32_t__begin--;
        }
        if ((char__tail[int32_t__begin] < '0') || (char__tail[int32_t__begin] > '9'))
        {
            int32_t__end = int32_t__begin - 1;   /* header or blank: one line up */
            continue;
        }
        break;
    }
    if (int32_t__end < 0)
    {
        return;
    }

    {
        const char *char__row = &char__tail[int32_t__begin];

        uint32_t__v24 = func__EspLog_RowFieldAt(char__row, 10u);
        if (uint32_t__v24 == 0u)
        {
            return;   /* a row without a pack voltage says nothing useful */
        }
        UINT32_T__G__LogBootV24 = uint32_t__v24;
        UINT32_T__G__LogBootVlow = func__EspLog_RowFieldAt(char__row, 11u);
        UINT32_T__G__LogBootVhigh = func__EspLog_RowFieldAt(char__row, 12u);
        UINT32_T__G__LogBootFault = func__EspLog_RowFieldAt(char__row, 8u);
        UINT8_T__G__LogBootFlags2 = (uint8_t)func__EspLog_RowFieldAt(char__row, 9u);
        UINT8_T__G__LogBootChg1 = (uint8_t)func__EspLog_RowFieldAt(char__row, 6u);
        UINT8_T__G__LogBootChg2 = (uint8_t)func__EspLog_RowFieldAt(char__row, 7u);
        UINT8_T__G__LogBootMains = (uint8_t)func__EspLog_RowFieldAt(char__row, 4u);
        /* [EN] the row stores the whole status byte; the comparison uses the
                state bits only (the mains bit has its own change bit), so the
                same mask is applied here or every boot would look like a mode
                change.
           [FA] ردیف کل بایت وضعیت را ذخیره می‌کند و مقایسه فقط بیت‌های وضعیت
                را می‌بیند (بیت برق شهر بیت تغییر خودش را دارد)، پس همان ماسک
                اینجا هم اعمال می‌شود وگرنه هر روشن شدن شبیه تغییر مود
                دیده می‌شد. */
        UINT8_T__G__LogBootFl =
            (uint8_t)(func__EspLog_RowFieldAt(char__row, 5u) & 0x3Du);
        UINT32_T__G__LogBootDead = func__EspLog_RowFieldAt(char__row, 14u);
        UINT8_T__G__LogBootDanger = (uint32_t__v24 < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
        BOOL__G__LogBootSnapValid = true;
    }
}

/**
 * @brief  [EN] Was the panel stopped while the battery sat at the cut line?
 *         [FA] پنل وقتی باتری روی مرز قطع بود خاموش شد؟
 * @‎return [EN] uint8_t 1 = the last run was in the danger band / [FA]‎ ۱ = خطر
 */
static uint8_t func__EspLog_LoadLastDanger(void)
{
    if (BOOL__G__LogBootSnapValid == false)
    {
        func__EspLog_LoadLastSnapshot();
    }
    return (BOOL__G__LogBootSnapValid != false) ? UINT8_T__G__LogBootDanger : 0u;
}

/**
 * @brief  [EN] Absolute difference of two unsigned readings.
 *         [FA] اختلاف مطلق دو خوانش بدون علامت.
 * @‎return [EN] difference / [FA]‎ اختلاف
 */
static uint32_t func__EspLog_AbsDiff(uint32_t uint32_t__a, uint32_t uint32_t__b)
{
    return (uint32_t__a >= uint32_t__b) ? (uint32_t__a - uint32_t__b)
                                        : (uint32_t__b - uint32_t__a);
}

/* ==================== Change detection / تشخیص تغییر ==================== */

/* [EN] Defined below; the queue uses it to decide whether the row may go now.
   [FA] پایین‌تر تعریف می‌شود؛ صف از آن می‌پرسد ردیف الان اجازهٔ رفتن دارد یا نه. */
static bool func__EspLog_FlushDue(uint32_t uint32_t__nowMs);

/**
 * @brief  [EN] Queue a change. Bursts inside the 2 s window are merged into one
 *              row; the row always carries the full snapshot, so no final state
 *              is lost and the flash is not hammered.
 *         [FA] ثبت یک تغییر. تغییرهای پشت‌سرهم داخل پنجرهٔ ۲ ثانیه‌ای در یک ردیف
 *              ادغام می‌شوند؛ ردیف همیشه کل وضعیت را دارد، پس وضعیت نهایی گم
 *              نمی‌شود و فلش هم زیر فشار نمی‌رود.
 * @‎param  uint16_t uint16_t__mask [EN] change bits / [FA]‎ بیت‌های تغییر
 * @‎param  uint8_t uint8_t__note [EN] note / [FA]‎ یادداشت
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Queue(uint16_t uint16_t__mask, uint8_t uint8_t__note)
{
    uint32_t uint32_t__nowMs;

    UINT16_T__G__LogPendingMask = (uint16_t)(UINT16_T__G__LogPendingMask | uint16_t__mask);
    if (uint8_t__note != ESP_LOG_NOTE_NONE)
    {
        UINT8_T__G__LogPendingNote = uint8_t__note;
    }

    uint32_t__nowMs = (uint32_t)millis();
    if (((uint16_t__mask & ESP_LOG_CHG_MAINS) != 0u) &&
        (BOOL__G__LogMainsQueuedValid == false))
    {
        BOOL__G__LogMainsQueuedValid = true;
        UINT32_T__G__LogMainsQueuedMs = uint32_t__nowMs;
    }
    if (func__EspLog_FlushDue(uint32_t__nowMs) != false)
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

/**
 * @brief  [EN] Is the pending row allowed to go to the flash now? Either the
 *              merge window has passed, or a mains flip has waited its short
 *              coalescing window (the board may cut the panel's power the same
 *              instant, so that row cannot wait two seconds).
 *         [FA] آیا ردیف در انتظار الان اجازهٔ رفتن به فلش دارد؟ یا پنجرهٔ ادغام
 *              گذشته، یا یک پرش برق شهر پنجرهٔ کوتاهش را صبر کرده (برد ممکن
 *              است همان لحظه برق پنل را ببرد، پس آن ردیف دو ثانیه صبر نمی‌کند).
 * @‎param  uint32_t uint32_t__nowMs [EN] now / [FA]‎ اکنون
 * @return [EN] bool due / [FA]‎ موقع نوشتن
 */
static bool func__EspLog_FlushDue(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__sinceRowMs;
    uint32_t uint32_t__sinceMainsMs;

    if (BOOL__G__LogLastRowValid == false)
    {
        return true;
    }
    uint32_t__sinceRowMs = uint32_t__nowMs - UINT32_T__G__LogLastRowMs;
    if (uint32_t__sinceRowMs >= ESP_LOG_MIN_INTERVAL_MS)
    {
        return true;
    }
    if (BOOL__G__LogMainsQueuedValid == false)
    {
        return false;
    }
    uint32_t__sinceMainsMs = uint32_t__nowMs - UINT32_T__G__LogMainsQueuedMs;
    return (uint32_t__sinceMainsMs >= ESP_LOG_MAINS_FLUSH_MS);
}

/**
 * @brief  [EN] Flush a queued change once its window has passed.
 *         [FA] نوشتن تغییر در انتظار، وقتی پنجره‌اش گذشت.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_FlushPending(void)
{
    if (UINT16_T__G__LogPendingMask == 0u)
    {
        return;
    }
    if (func__EspLog_FlushDue((uint32_t)millis()) != false)
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}

/**
 * @brief  [EN] Compare this frame's live state with the previous one and queue
 *              whatever changed. A steady board writes nothing.
 *         [FA] مقایسهٔ وضعیت زندهٔ این فریم با فریم قبلی و ثبت هر چه عوض شده.
 *              برد آرام هیچ چیزی نمی‌نویسد.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_DetectChanges(void)
{
    uint8_t uint8_t__mains;
    uint8_t uint8_t__chg1;
    uint8_t uint8_t__chg2;
    uint8_t uint8_t__flags2;
    uint8_t uint8_t__band;
    uint8_t uint8_t__fl;
    uint32_t uint32_t__dead;
    uint32_t uint32_t__fault;
    uint16_t uint16_t__mask = 0u;

    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;
    uint8_t__chg1 = (uint8_t)UINT32_T__G__TlmField[6];
    uint8_t__chg2 = (uint8_t)UINT32_T__G__TlmField[13];
    uint8_t__flags2 = UINT8_T__G__TlmFlags2;
    uint32_t__fault = UINT32_T__G__TlmField[19];
    /* [EN] every state bit of the general byte except the input-present
            bit, which already has its own change bit: snapshot, valid
            measurement, both charger enables and hand mode.
       [FA] همهٔ بیت‌های وضعیتِ بایت عمومی جز بیت برق ورودی، که بیت تغییر
            خودش را دارد: snapshot، معتبر بودن اندازه‌گیری، اجازهٔ هر دو
            شارژر و مود دستی. */
    uint8_t__fl = (uint8_t)(UINT8_T__G__TlmFlags & 0x3Du);
    uint32_t__dead = UINT32_T__G__TlmField[28];

    if (BOOL__G__LogHasPrev == false)
    {
        BOOL__G__LogHasPrev = true;
        if ((BOOL__G__LogBootSnapValid != false) && (BOOL__G__LogClockValid != false))
        {
            /* [EN] First frame after boot AND the flash remembers what the world
                    looked like before the power went off. Seed the comparison
                    with that, then fall through: the checks below decide whether
                    anything really changed while the panel was dead. If nothing
                    changed, no row is written - the panel waits, exactly as the
                    user ordered.
               [FA] اولین فریم بعد از روشن شدن و فلش به یاد دارد که پیش از قطع
                    برق دنیا چه شکلی بود. مقایسه با همان بذر می‌شود و پایین
                    ادامه می‌یابد: بررسی‌های زیر می‌گویند آیا در مدت خاموشی
                    چیزی واقعاً عوض شده یا نه. اگر چیزی عوض نشده باشد ردیفی
                    نوشته نمی‌شود و پنل منتظر می‌ماند - همان دستور کاربر. */
            UINT8_T__G__LogPrevMains = UINT8_T__G__LogBootMains;
            UINT8_T__G__LogPrevChg1 = UINT8_T__G__LogBootChg1;
            UINT8_T__G__LogPrevChg2 = UINT8_T__G__LogBootChg2;
            UINT8_T__G__LogPrevFlags2 = UINT8_T__G__LogBootFlags2;
            UINT32_T__G__LogPrevFault = UINT32_T__G__LogBootFault;
            UINT32_T__G__LogPrevV24 = UINT32_T__G__LogBootV24;
            /* [EN] v1.110: the condemned mask and the status byte come from the
                    last row too. They were taken from the frame in hand, so a
                    battery condemned - or a charger enabled - while the panel
                    was off never produced a row, although both are logged
                    conditions with their own change bits.
               [FA] نسخهٔ ۱٫۱۱۰: ماسک باتری خراب و بایت وضعیت هم از آخرین
                    ردیف می‌آیند. قبلاً از فریمِ در دست گرفته می‌شدند، پس
                    باتری‌ای که در مدت خاموشی حکم خرابی می‌گرفت - یا شارژری
                    که اجازه می‌گرفت - هیچ ردیفی نمی‌ساخت، در حالی که هر دو
                    شرط ثبت‌شده با بیت تغییر خودشان‌اند. */
            UINT32_T__G__LogPrevDead = UINT32_T__G__LogBootDead;
            UINT8_T__G__LogPrevFl = UINT8_T__G__LogBootFl;
            /* The danger band is re-derived from the saved pack voltage, so a
               pack that was already low before the restart is not announced
               twice; one that crossed the line while the panel was off is. */
            UINT8_T__G__LogPrevBand = (UINT32_T__G__LogBootV24 < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
            BOOL__G__LogBootSnapValid = false;
        }
        else
        {
            /* [EN] No snapshot: this frame is the baseline, nothing to compare.
               [FA] عکسی نیست: همین فریم مبنا می‌شود و چیزی برای مقایسه نیست. */
            UINT8_T__G__LogPrevMains = uint8_t__mains;
            UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
            UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
            UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
            UINT32_T__G__LogPrevFault = uint32_t__fault;
            UINT8_T__G__LogPrevBand =
                (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
            UINT8_T__G__LogPrevFl = uint8_t__fl;
            UINT32_T__G__LogPrevDead = uint32_t__dead;
            UINT32_T__G__LogPrevV24 = UINT32_T__G__TlmField[15];
            return;
        }
    }

    /* [EN] v1.98: the cut band has HYSTERESIS. It is entered below
            21,200 mV and left only above 21,400 mV, so a pack resting on the
            line - or a load switching on and off - cannot write a row every
            couple of seconds and fill the flash with one repeated event.
            v1.110: this runs AFTER the comparison is seeded, so the first
            frame after a restart obeys the same rule instead of leaving the
            band at 21,300 mV, 100 mV early.
       [FA] نسخهٔ ۱٫۹۸: مرز قطع هیسترزیس دارد. زیر ۲۱٬۲۰۰ میلی‌ولت وارد و
            فقط بالای ۲۱٬۴۰۰ میلی‌ولت خارج می‌شود؛ پس باتریِ خوابیده روی
            مرز - یا باری که روشن و خاموش می‌شود - نمی‌تواند هر دو ثانیه
            یک ردیف بنویسد و فلش را با یک اتفاق تکراری پر کند.
            نسخهٔ ۱٫۱۱۰: این محاسبه «بعد از» بذر شدن مقایسه انجام می‌شود تا
            اولین فریم بعد از روشن شدن هم همان قاعده را رعایت کند و باند را
            ۱۰۰ میلی‌ولت زودتر ترک نکند. */
    if (UINT8_T__G__LogPrevBand != 0u)
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] > ESP_LOG_BAND_OFF_MV) ? 0u : 1u;
    }
    else
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
    }

    if (uint8_t__mains != UINT8_T__G__LogPrevMains)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_MAINS);
    }
    if (uint8_t__chg1 != UINT8_T__G__LogPrevChg1)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_CHG1);
    }
    if (uint8_t__chg2 != UINT8_T__G__LogPrevChg2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_CHG2);
    }
    if (uint32_t__fault != UINT32_T__G__LogPrevFault)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_FAULT);
    }
    if (uint8_t__flags2 != UINT8_T__G__LogPrevFlags2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_FLAGS);
    }
    if (uint8_t__band != UINT8_T__G__LogPrevBand)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_BAND);
    }
    if (uint8_t__fl != UINT8_T__G__LogPrevFl)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_MODE);
    }
    if (uint32_t__dead != UINT32_T__G__LogPrevDead)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_DEAD);
    }
    /* [EN] v1.98: a step of the pack voltage, either way. A row that is
            written anyway (any other change) carries the step for free - the
            voltage is on that row regardless. Only a row whose SOLE reason is
            the step is limited to one a minute, so a load switching on and
            off cannot flood the log while a slow charge still draws its
            curve.
       [FA] نسخهٔ ۱٫۹۸: یک پلهٔ ولتاژ پک، بالا یا پایین. ردیفی که به هر حال
            نوشته می‌شود (تغییر دیگر) گام را مجانی با خود می‌برد - ولتاژ روی
            همان ردیف هست. فقط ردیفی که تنها دلیلش گام است به یکی در دقیقه
            محدود می‌شود؛ پس باری که روشن و خاموش می‌شود لاگ را پر نمی‌کند و
            شارژ آرام همچنان منحنی‌اش را می‌کشد. */
    if (func__EspLog_AbsDiff(UINT32_T__G__TlmField[15],
                             UINT32_T__G__LogPrevV24) >= ESP_LOG_VSTEP_MV)
    {
        /* [EN] v1.109 user order: while the mains are OFF (discharge) every
                RISE of the pack is recorded as its own row - the one-a-minute
                throttle stays only for falls and for charging.
           [FA] دستور کاربر نسخهٔ ۱٫۱۰۹: وقتی برق شهر قطع است (دشارژ) هر
                «بالا آمدن» پک در ردیف خودش ثبت می‌شود؛ محدودیتِ یکی در دقیقه
                فقط برای پایین‌رفتن و برای شارژ می‌ماند. */
        bool bool__riseInDischarge = ((uint8_t__mains == 0u) &&
                                      (UINT32_T__G__TlmField[15] > UINT32_T__G__LogPrevV24));
        if ((uint16_t__mask != 0u) ||
            (bool__riseInDischarge != false) ||
            (BOOL__G__LogLastStepValid == false) ||
            ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastStepMs) >= ESP_LOG_VSTEP_MIN_MS))
        {
            uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_VSTEP);
        }
    }

    if (uint16_t__mask != 0u)
    {
        func__EspLog_Queue(uint16_t__mask, ESP_LOG_NOTE_NONE);
    }

    UINT8_T__G__LogPrevMains = uint8_t__mains;
    UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
    UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
    UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
    UINT32_T__G__LogPrevFault = uint32_t__fault;
    UINT8_T__G__LogPrevBand = uint8_t__band;
    UINT8_T__G__LogPrevFl = uint8_t__fl;
    UINT32_T__G__LogPrevDead = uint32_t__dead;
    /* [EN] the pack voltage is NOT stored here: it is compared
            with the value at the last ROW, so a slow charge still
            produces one row per step.
       [FA] ولتاژ پک اینجا ذخیره نمی‌شود: با مقدار آخرین ردیف
            مقایسه می‌شود تا شارژ آرام هم هر گام یک ردیف بدهد. */
}

/* ==================== Hooks / قلاب‌ها ==================== */

/**
 * @brief  [EN] Called after every decoded TLM frame.
 *         [FA] بعد از هر فریم TLM رمزگشایی‌شده صدا زده می‌شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_OnFrame(void)
{
    /* [EN] Link back always pairs a link-lost row that was written before.
       [FA] «اتصال برقرار شد» همیشه جفتِ همان «اتصال قطع شد»ی است که قبلاً
            نوشته شده. */
    if (BOOL__G__LogLinkDown != false)
    {
        BOOL__G__LogLinkDown = false;
        func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_BACK);
    }

    /* [EN] The boot row no longer waits for a board frame (user order
          2026-10-24: "if the board was not connected it logged nothing -
          it must log"). Tick() writes it at start-up; here only the
          state-change detection runs.
       [FA] ردیف بوت دیگر منتظر فریم برد نمی‌ماند (دستور ۲۰۲۶-۱۰-۲۴: «اگر برد
          وصل نبود لاگ نمی‌گرفت - باید بگیرد»). ردیف را Tick در شروع
          می‌نویسد؛ اینجا فقط تشخیص تغییر وضعیت اجرا می‌شود. */
    func__EspLog_DetectChanges();
}

/**
 * @brief  [EN] Periodic work: merge pending rows, save the clock, drain the RAM
 *              ring when the flash is healthy again, notice a lost link. Called
 *              from loop().
 *         [FA] کار دوره‌ای: نوشتن ردیف در انتظار، ذخیرهٔ ساعت، برگرداندن
 *              ردیف‌های رم وقتی فلش سالم شد، و تشخیص قطع ارتباط. از حلقه صدا
 *              زده می‌شود.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Tick(void)
{
    uint32_t uint32_t__nowMs;

    /* [EN] Boot row FIRST and with no board at all: the panel log is the
          witness of the panel itself, so a session with a silent board still
          starts its own record (user order 2026-10-24). It carries the state
          the panel saw last time and whether that run was in the danger band.
       [FA] اول ردیف بوت، آن هم بدون هیچ بردی: لاگِ پنل شاهدِ خودِ پنل است،
          پس نشستِ بدون برد هم تاریخچهٔ خودش را شروع می‌کند (۲۰۲۶-۱۰-۲۴).
          این ردیف وضعیتِ آخرین اجرا و خطرناک‌بودن آن را هم می‌برد. */
    if (BOOL__G__LogBootRowPending != false)
    {
        uint16_t uint16_t__mask = ESP_LOG_CHG_BOOT;

        BOOL__G__LogBootRowPending = false;
        UINT32_T__G__LogSessionStartMs = (uint32_t)millis();
        UINT8_T__G__LogPrevNote = func__EspLog_LoadLastDanger();
        /* [EN] v1.113 (user order 2026-10-28: "every time I do an update one
                record must be written, with no conditions at all, and only the
                log timers zeroed"). The update joins the boot row, so one
                switch-on gives one row. The log timers are zeroed just before
                it: the merge window, the step throttle, the "last row" clock
                and the clock-save timer all start again from this row, so
                nothing from before the update can swallow or merge it.
           [FA] نسخهٔ ۱٫۱۱۳ (دستور کاربر ۲۰۲۶-۱۰-۲۸: «هر وقت بروزرسانی را زدم یک
                رکورد ضبط بشه، بدون در نظر گرفتن شرط‌ها، و فقط تایمرهای مربوط
                به لاگ صفر بشه»). بروزرسانی با ردیف بوت یکی می‌شود، پس یک
                روشن‌شدن یک ردیف می‌دهد. تایمرهای لاگ درست پیش از آن صفر
                می‌شوند: پنجرهٔ ادغام، محدودیت گام ولتاژ، ساعتِ «آخرین ردیف» و
                تایمر ذخیرهٔ ساعت از همین ردیف دوباره شروع می‌کنند تا هیچ
                چیزِ پیش از بروزرسانی آن را نبلعد یا ادغام نکند. */
        if (BOOL__G__LogUpdatePending != false)
        {
            BOOL__G__LogUpdatePending = false;
            uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_UPDATE);
            UINT32_T__G__LogLastRowMs = 0u;
            BOOL__G__LogLastRowValid = false;
            UINT32_T__G__LogLastStepMs = 0u;
            BOOL__G__LogLastStepValid = false;
            UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
            UINT16_T__G__LogPendingMask = 0u;
            UINT8_T__G__LogPendingNote = ESP_LOG_NOTE_NONE;
            BOOL__G__LogMainsQueuedValid = false;
            BOOL__A__LogForceRow = true;
        }
        (void)func__EspLog_WriteRow(uint16_t__mask,
                                    (UINT8_T__G__LogPrevNote != 0u) ?
                                    ESP_LOG_NOTE_LOW_STOP : ESP_LOG_NOTE_NONE);
    }

    /* [EN] The clock moved by more than a re-sync: say it in the history.
       [FA] ساعت بیش از یک هم‌زمان‌سازی جابه‌جا شد: در تاریخچه گفته می‌شود. */
    if (BOOL__G__LogClockJumpPending != false)
    {
        BOOL__G__LogClockJumpPending = false;
        func__EspLog_Queue(ESP_LOG_CHG_CLOCK, ESP_LOG_NOTE_NONE);
    }

    func__EspLog_FlushPending();

    uint32_t__nowMs = (uint32_t)millis();
    if (BOOL__G__LogClockValid != false)
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastClockSaveMs) >= ESP_LOG_CLOCK_SAVE_MS)
        {
            func__EspLog_ClockSave();
        }
    }

    /* [EN] Rows waiting in RAM go back to their day files as soon as the flash
            takes writes again, without waiting for the next event.
       [FA] ردیف‌های منتظر در رم، به‌محض اینکه فلش دوباره بنویسد، بی‌نیاز از
            رویداد بعدی به فایل روزهای خودشان برمی‌گردند. */
    if ((BOOL__G__FsOk != false) && (UINT8_T__G__LogRamCount != 0u))
    {
        func__EspLog_DrainRam();
    }

    /* [EN] Link-silence row: after LINK_TIMEOUT_MS with no frame. If the
          board has never spoken, the clock runs from the session start, so
          "no board" is written too - not only "board left".
       [FA] ردیف سکوت ارتباط: پس از چند میلی‌ثانیه بدون فریم. اگر برد هرگز
          حرفی نزده باشد، ساعت از شروع نشست می‌شمارد تا «برد نیست» هم ثبت
          شود، نه فقط «برد رفت». */
    if (BOOL__G__LogLinkDown == false)
    {
        bool bool__silent;

        if (BOOL__G__TlmSeen != false)
        {
            bool__silent = ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastTlmMs) >
                            ESP_LOG_LINK_TIMEOUT_MS);
        }
        else
        {
            bool__silent = (BOOL__G__LogLastRowValid != false) &&
                           ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogSessionStartMs) >
                            ESP_LOG_LINK_TIMEOUT_MS);
        }
        if (bool__silent != false)
        {
            BOOL__G__LogLinkDown = true;
            func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_LOST);
        }
    }

    /* [EN] v1.96: if nothing at all has happened for a whole heartbeat
            window, write one row that says so. It is the difference
            between "nothing changed" and "the panel stopped working":
            a board that keeps beating is a board that kept running.
       [FA] نسخهٔ ۱٫۹۶: اگر در یک بازهٔ کامل هیچ اتفاقی نیفتاده باشد، یک
            ردیف نوشته می‌شود که همین را بگوید. تفاوت «چیزی عوض نشده» با
            «پنل از کار افتاده» همین است: ضربانِ پیوسته یعنی پنل زنده ماند. */
    if (BOOL__G__LogLastRowValid != false)
    {
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >=
            ESP_LOG_HEARTBEAT_MS)
        {
            func__EspLog_Queue(0u, ESP_LOG_NOTE_HEARTBEAT);
        }
    }
}

/**
 * @brief  [EN] Boot: clock from flash, day list, boot row pending.
 *         [FA] شروع: ساعت از فلش، فهرست روزها و ردیف بوت.
 * @return [EN] None / [FA]‎ ندارد
 */
static void func__EspLog_Begin(void)
{
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    /* [EN] a panel flashed with the old firmware still carries the list file
            it used to keep. Nothing reads it any more, so it only wastes
            flash: one boot deletes it.
       [FA] پنلی که با نسخهٔ قدیمی فلش شده هنوز همان فایل فهرست را دارد.
            دیگر چیزی آن را نمی‌خواند و فقط جای فلش می‌گیرد: یک بار سر روشن
            شدن حذف می‌شود. */
    if (LittleFS.exists(ESP_LOG_LEGACY_INDEX_PATH) != false)
    {
        (void)LittleFS.remove(ESP_LOG_LEGACY_INDEX_PATH);
    }
    func__EspLog_ClockLoad();
    /* [EN] v1.113: was this flash a new firmware? The stamp lives in the clock
            file, so it survives both a restart and a history clear.
       [FA] نسخهٔ ۱٫۱۱۳: آیا این فلش، فرم‌ور تازه‌ای بود؟ مهر در فایل ساعت است،
            پس هم ری‌استارت و هم پاک‌کردن تاریخچه را از سر می‌گذراند. */
    func__EspLog_FwCheck();
    /* [EN] the day list is read from the flash, so a restart cannot lose it.
       [FA] فهرست روزها از خود فلش خوانده می‌شود، پس ری‌استارت آن را از دست
            نمی‌دهد. */
    (void)func__EspLog_ScanDays(true);
    /* [EN] read back what the world looked like before the power went off.
       [FA] خواندن دوبارهٔ این‌که پیش از قطع برق دنیا چه شکلی بود. */
    func__EspLog_LoadLastSnapshot();
}

/* ==================== Read back / خواندن دوباره ==================== */

/**
 * @brief  [EN] Where does the window of the newest rows start? It walks
 *              BACKWARD from the end of the file, so the limit is spent on the
 *              newest rows: the buffer may hold the last 18 rows of a busy day,
 *              and they are rows 383..400, never rows 1..18.
 *         [FA] پنجرهٔ جدیدترین ردیف‌ها از کجا شروع می‌شود؟ از انتهای فایل
 *              عقب‌عقب می‌رود، پس سقف بافر خرجِ جدیدترین ردیف‌ها می‌شود: بافر
 *              می‌تواند ۱۸ ردیف آخر یک روز شلوغ را نگه دارد و آن‌ها ردیف‌های
 *              آخر هستند، نه اولین ردیف‌های همان روز.
 * @‎param  File &file__day [EN] open day file / [FA]‎ فایل روز باز
 * @‎param  uint32_t uint32_t__size [EN] file size / [FA]‎ اندازهٔ فایل
 * @‎param  uint16_t uint16_t__maxRows [EN] row limit / [FA]‎ سقف ردیف
 * @‎param  size_t size_t__budget [EN] byte limit / [FA]‎ سقف بایت
 * @return [EN] offset to start from / [FA]‎ آفست شروع
 */
static uint32_t func__EspLog_TailStart(File &file__day,
                                       uint32_t uint32_t__size,
                                       uint16_t uint16_t__maxRows,
                                       size_t size_t__budget)
{
    uint32_t uint32_t__offset = uint32_t__size;
    uint32_t uint32_t__lines = 0u;
    uint32_t uint32_t__bytes = 0u;
    bool bool__cutMidRow = false;

    while (uint32_t__offset > 0u)
    {
        int int__ch;

        uint32_t__offset--;
        if (file__day.seek(uint32_t__offset) == false)
        {
            return 0u;
        }
        int__ch = file__day.read();
        if (int__ch < 0)
        {
            return 0u;
        }
        uint32_t__bytes++;
        /* [EN] the last character of a normal file closes the newest row; it is
                not a row of its own.
           [FA] آخرین کاراکتر فایل عادی، جدیدترین ردیف را می‌بندد؛ خودش ردیف
                نیست. */
        if ((int__ch == '\n') && (uint32_t__bytes > 1u))
        {
            uint32_t__lines++;
            if (uint32_t__lines >= (uint32_t)uint16_t__maxRows)
            {
                uint32_t__offset++;
                return uint32_t__offset;
            }
        }
        if (uint32_t__bytes >= (uint32_t)size_t__budget)
        {
            bool__cutMidRow = true;
            break;
        }
    }

    if ((bool__cutMidRow != false) && (uint32_t__offset > 0u))
    {
        /* [EN] the budget ran out inside a row: drop that half row, a row the
                panel cannot parse is worse than one row less.
           [FA] بودجه وسط یک ردیف تمام شد: همان نیم‌ردیف دور ریخته می‌شود؛
                ردیفی که پنل نتواند بخواندش از یک ردیف کمتر بدتر است. */
        while (uint32_t__offset < uint32_t__size)
        {
            int int__ch;

            if (file__day.seek(uint32_t__offset) == false)
            {
                return 0u;
            }
            int__ch = file__day.read();
            uint32_t__offset++;
            if (int__ch == '\n')
            {
                break;
            }
        }
    }
    return uint32_t__offset;
}

/**
 * @brief  [EN] Copy the last N rows of the RAM ring into a buffer, oldest first.
 *         [FA] کپی آخرین N ردیف حلقهٔ رم در بافر، قدیمی‌تر اول.
 * @return [EN] size_t bytes written / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_TailRam(char *char__out, size_t size_t__outSize,
                                   uint16_t uint16_t__maxLines)
{
    uint8_t uint8_t__start;
    uint8_t uint8_t__count = UINT8_T__G__LogRamCount;
    uint8_t uint8_t__item;
    size_t size_t__used = 0u;

    if (uint8_t__count == 0u)
    {
        return 0u;
    }
    uint8_t__start = (uint8_t)((UINT8_T__G__LogRamHead + ESP_LOG_RAM_ROWS - uint8_t__count) % ESP_LOG_RAM_ROWS);
    if ((uint16_t)uint8_t__count > uint16_t__maxLines)
    {
        uint8_t uint8_t__drop = (uint8_t)((uint16_t)uint8_t__count - uint16_t__maxLines);
        uint8_t__start = (uint8_t)((uint8_t__start + uint8_t__drop) % ESP_LOG_RAM_ROWS);
        uint8_t__count = (uint8_t)uint16_t__maxLines;
    }
    for (uint8_t__item = 0u; uint8_t__item < uint8_t__count; uint8_t__item++)
    {
        uint8_t uint8_t__slot = (uint8_t)((uint8_t__start + uint8_t__item) % ESP_LOG_RAM_ROWS);
        size_t size_t__len = strlen(CHAR__G__LogRamRow[uint8_t__slot]);
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__out[size_t__used], CHAR__G__LogRamRow[uint8_t__slot], size_t__len);
        size_t__used += size_t__len;
        char__out[size_t__used] = '\n';
        size_t__used++;
        char__out[size_t__used] = '\0';
    }
    return size_t__used;
}

/**
 * @brief  [EN] The RAM ring is the only history left: the card shows those rows
 *              instead of nothing.
 *         [FA] تنها تاریخچهٔ باقی‌مانده حلقهٔ رم است: کارت همان ردیف‌ها را
 *              نشان می‌دهد، نه هیچ.
 * @‎param  char *char__out [EN] output buffer / [FA]‎ بافر خروجی
 * @‎param  size_t size_t__outSize [EN] size / [FA]‎ اندازهٔ بافر
 * @‎param  uint16_t uint16_t__maxLines [EN] how many rows / [FA]‎ چند ردیف
 * @‎param  bool bool__isToday [EN] is the asked day today? / [FA]‎ روز پرسیده امروز است؟
 * @return [EN] size_t bytes written / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_TailRamOnly(char *char__out, size_t size_t__outSize,
                                        uint16_t uint16_t__maxLines, bool bool__isToday)
{
    /* [EN] The RAM ring only ever holds today's rows. Without a write fault the
            emptiness is real, and an older day must never borrow today's rows.
       [FA] حلقهٔ رم فقط ردیف‌های امروز را دارد. بدون خرابیِ نوشتن، خالی‌بودن
            واقعی است و روز قدیمی‌تر هرگز نباید ردیف‌های امروز را قرض بگیرد. */
    if ((UINT8_T__G__LogWriteFault == 0u) || (bool__isToday == false))
    {
        return 0u;
    }
    return func__EspLog_TailRam(char__out, size_t__outSize, uint16_t__maxLines);
}

/**
 * @brief  [EN] Which file does an automatic read pick? Today's file when it
 *              exists, otherwise the newest day of the list - the day the phone
 *              clock has not named yet keeps its own file and is never hidden.
 *         [FA] خواندن خودکار کدام فایل را می‌گیرد؟ فایل امروز اگر باشد، وگرنه
 *              جدیدترین روز فهرست - روزی که ساعت گوشی هنوز نامش را نبرده فایل
 *              خودش را دارد و هرگز پنهان نمی‌شود.
 * @return [EN] day code or ESP_LOG_DAY_AUTO when nothing exists / [FA]‎ کد روز
 */
static uint32_t func__EspLog_PickDay(uint32_t uint32_t__dayCode)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint32_t uint32_t__chosen = uint32_t__dayCode;

    if ((uint32_t__chosen == ESP_LOG_DAY_ZERO))
    {
        return 0u;
    }
    if ((uint32_t__chosen != ESP_LOG_DAY_AUTO) && (uint32_t__chosen != 0u))
    {
        return uint32_t__chosen;
    }

    uint32_t__chosen = func__EspLog_TodayCode();
    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        uint32_t__chosen = func__EspLog_NewestDay();
        if (uint32_t__chosen == ESP_LOG_DAY_AUTO)
        {
            return ESP_LOG_DAY_AUTO;
        }
    }
    return uint32_t__chosen;
}

/**
 * @brief  [EN] Copy the last N rows of one day into a buffer, oldest first.
 *              The window is chosen from the END of the file, so when the
 *              buffer cannot hold 30 rows the OLDEST of them are dropped,
 *              never the newest. A flash that refused the last writes is
 *              served from the RAM ring instead of an empty card.
 *         [FA] کپی آخرین N ردیف یک روز در بافر، قدیمی‌تر اول. پنجره از انتهای
 *              فایل انتخاب می‌شود، پس وقتی بافر ۳۰ ردیف را جا نمی‌دهد
 *              قدیمی‌ترین‌ها دور ریخته می‌شوند، نه جدیدترین‌ها. فلشی که آخرین
 *              نوشتن‌ها را نپذیرفت، از حلقهٔ رم خوانده می‌شود نه کارت خالی.
 * @‎param  char *char__out [EN] output buffer / [FA]‎ بافر خروجی
 * @‎param  size_t size_t__outSize [EN] buffer size / [FA]‎ اندازهٔ بافر
 * @‎param  uint32_t uint32_t__dayCode [EN] day, AUTO = newest / [FA]‎ روز
 * @‎param  uint16_t uint16_t__maxLines [EN] how many rows / [FA]‎ چند ردیف
 * @return [EN] size_t bytes written / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_Tail(char *char__out, size_t size_t__outSize,
                                uint32_t uint32_t__dayCode, uint16_t uint16_t__maxLines)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    size_t size_t__used = 0u;
    uint32_t uint32_t__chosen;
    uint32_t uint32_t__size;
    uint32_t uint32_t__start;
    bool bool__isToday;

    if ((char__out == NULL) || (size_t__outSize < 2u))
    {
        return 0u;
    }
    char__out[0] = '\0';

    /* [EN] v1.111 (user report 2026-10-28): one refused write used to hide the
            whole day file behind 16 RAM rows, so a card that had hundreds of
            rows showed a handful. The file is read first and the rows that
            never reached the flash follow it.
       [FA] نسخهٔ ۱٫۱۱۱ (گزارش کاربر ۲۰۲۶-۱۰-۲۸): یک نوشتنِ ردشده کل فایل روز
            را پشت ۱۶ ردیف رم پنهان می‌کرد، پس کارتی که صدها ردیف داشت چند
            ردیف نشان می‌داد. اول فایل خوانده می‌شود و ردیف‌هایی که به فلش
            نرسیدند پشت آن می‌آیند. */
    bool__isToday = ((uint32_t__dayCode == ESP_LOG_DAY_AUTO) ||
                     (uint32_t__dayCode == 0u) ||
                     (uint32_t__dayCode == func__EspLog_TodayCode()));
    /* [EN] No file system at all: the RAM ring is the whole history, so today's
            rows are served as they are.
       [FA] فایل‌سیستم نیست: حلقهٔ رم تمام تاریخچه است، پس ردیف‌های امروز
            همان‌طور که هستند خوانده می‌شوند. */
    if (BOOL__G__FsOk == false)
    {
        return func__EspLog_TailRam(char__out, size_t__outSize, uint16_t__maxLines);
    }

    uint32_t__chosen = func__EspLog_PickDay(uint32_t__dayCode);
    if (uint32_t__chosen == ESP_LOG_DAY_AUTO)
    {
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }

    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size == 0u)
    {
        file__day.close();
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    uint32_t__start = func__EspLog_TailStart(file__day, uint32_t__size,
                                             uint16_t__maxLines, size_t__outSize - 2u);
    if (file__day.seek(uint32_t__start) == false)
    {
        file__day.close();
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        size_t size_t__len;

        string__line.trim();
        if ((string__line.length() == 0u) || (string__line[0] == '#'))
        {
            continue;
        }
        size_t__len = (size_t)string__line.length();
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__out[size_t__used], string__line.c_str(), size_t__len);
        size_t__used += size_t__len;
        char__out[size_t__used] = '\n';
        size_t__used++;
        char__out[size_t__used] = '\0';
    }
    file__day.close();

    /* [EN] the rows that failed to reach the flash are newer than everything in
            the file, so they are appended, not substituted.
       [FA] ردیف‌هایی که به فلش نرسیدند از هر چه در فایل است جدیدترند، پس
            پشت آن‌ها می‌آیند، نه به‌جای آن‌ها. */
    if ((UINT8_T__G__LogWriteFault != 0u) && (UINT8_T__G__LogRamCount != 0u) &&
        ((size_t__used + 2u) < size_t__outSize))
    {
        size_t__used += func__EspLog_TailRam(&char__out[size_t__used],
                                             size_t__outSize - size_t__used,
                                             uint16_t__maxLines);
    }
    return size_t__used;
}

/**
 * @brief  [EN] Delete every saved day and start a fresh list.
 *         [FA] حذف همهٔ روزهای ذخیره‌شده و شروع فهرست تازه.
 * @‎return [EN] uint8_t number of files removed / [FA]‎ تعداد فایل حذف‌شده
 */
static uint8_t func__EspLog_Clear(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    uint8_t uint8_t__removed = 0u;

    UINT8_T__G__LogRamHead = 0u;
    UINT8_T__G__LogRamCount = 0u;
    if (BOOL__G__FsOk == false)
    {
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        UINT8_T__G__LogWriteFault = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        return 0u;
    }
    (void)func__EspLog_ScanDays(true);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (LittleFS.remove(char__path))
        {
            uint8_t__removed++;
        }
    }
    UINT8_T__G__LogIndexCount = 0u;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    BOOL__G__LogMarkerDone = false;
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    return uint8_t__removed;
}

/**
 * @brief  [EN] Day list as text ("YYYYMMDD YYYYMMDD ... , newest last").
 *         [FA] فهرست روزها به‌صورت متن (قدیمی‌ترین تا جدیدترین).
 * @‎param  char *char__out [EN] output / [FA]‎ خروجی
 * @‎param  size_t size_t__outSize [EN] size / [FA]‎ اندازه
 * @return [EN] size_t / [FA]‎ تعداد بایت
 */
static size_t func__EspLog_Days(char *char__out, size_t size_t__outSize)
{
    size_t size_t__used = 0u;
    uint8_t uint8_t__slot;

    if ((char__out == NULL) || (size_t__outSize < 4u))
    {
        return 0u;
    }
    char__out[0] = '\0';
    if (BOOL__G__FsOk == false)
    {
        /* [EN] v1.111: with the file system down the only day that can exist is
                today's RAM rows. The list used to stay empty until the first
                row arrived, so the day picker showed nothing at all.
           [FA] نسخهٔ ۱٫۱۱۱: با خاموش بودن فایل‌سیستم تنها روزی که می‌تواند
                باشد ردیف‌های رمِ امروز است. فهرست تا رسیدن اولین ردیف خالی
                می‌ماند و انتخاب‌گر روز هیچ چیز نشان نمی‌داد. */
        if (size_t__outSize >= 12u)
        {
            int int__ramWritten = snprintf(char__out, size_t__outSize, "%08lu",
                                           (unsigned long)func__EspLog_TodayCode());
            return (int__ramWritten > 0) ? (size_t)int__ramWritten : 0u;
        }
        return 0u;
    }
    (void)func__EspLog_ScanDays(false);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        int int__written = snprintf(&char__out[size_t__used],
                                    size_t__outSize - size_t__used,
                                    "%s%08lu",
                                    (size_t__used == 0u) ? "" : " ",
                                    (unsigned long)UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (int__written <= 0)
        {
            break;
        }
        size_t__used += (size_t)int__written;
        if (size_t__used >= (size_t__outSize - 12u))
        {
            break;
        }
    }
    return size_t__used;
}
