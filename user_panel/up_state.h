/**
 * @file    up_state.h
 * @brief   [EN] Shared record types and the live model of the user panel:
 *              what is read from the board, what is remembered in RAM, and the
 *              fixed wording used for every status the UI shows.
 *          [FA] تایپ‌های مشترک و مدل زندهٔ پنل کاربر: آنچه از برد خوانده
 *              می‌شود، آنچه در RAM نگه داشته می‌شود و متن ثابت هر وضعیتی که
 *              رابط کاربری نشان می‌دهد.
 *
 * @note    [EN] Everything here is statically sized. The panel runs on an
 *              ESP8266 with no dynamic allocation in the hot path, so the
 *              record layouts are part of the on-flash format: changing a
 *              field changes UP_STORE_VERSION in up_store.h.
 *          [FA] همه‌چیز اندازهٔ ثابت دارد. پنل روی ESP8266 بدون تخصیص دینامیک
 *              در مسیر داغ کار می‌کند، پس چیدمان رکوردها بخشی از قالب روی
 *              فلش است: تغییر هر فیلد یعنی تغییر UP_STORE_VERSION.
 */

#ifndef UP_STATE_H
#define UP_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "up_config.h"

/* ==================== Event codes / کدهای رویداد ==================== */
/* [EN] Stored as a byte in the event ring, mapped to Persian text by the web
   page (web/app.js EV table). Appending a code is safe; changing the meaning of
   an existing number is not.
   [FA] به‌صورت یک بایت در حلقهٔ رویداد ذخیره می‌شود و صفحهٔ وب (جدول EV در
   web/app.js) آن را به متن فارسی برمی‌گرداند. اضافه‌کردن کد بی‌خطر است؛
   عوض‌کردن معنای یک عدد موجود نه. */
typedef enum
{
    UP_EV_CHARGE_START     = 1u,   /* [EN] a channel left OFF/WAIT for BULK / [FA] کانال از خاموش به شارژ رفت */
    UP_EV_CHARGE_DONE      = 2u,   /* [EN] charge finished into FLOAT / [FA] شارژ به FLOAT رسید */
    UP_EV_CHARGE_INCOMPLETE = 3u,  /* [EN] ended any other way / [FA] هر پایان دیگر */
    UP_EV_INPUT_LOST       = 4u,   /* [EN] 24 V input went away / [FA] ورودی قطع شد */
    UP_EV_INPUT_BACK       = 5u,   /* [EN] 24 V input returned / [FA] ورودی برگشت */
    UP_EV_RUN_START        = 6u,   /* [EN] work on battery began / [FA] کار روی باتری شروع شد */
    UP_EV_RUN_END          = 7u,   /* [EN] work on battery ended / [FA] کار روی باتری تمام شد */
    UP_EV_FAULT_SET        = 8u,   /* [EN] fault bit(s) appeared / [FA] بیت خطا ظاهر شد */
    UP_EV_FAULT_CLEAR      = 9u,   /* [EN] fault bit(s) went away / [FA] بیت خطا پاک شد */
    UP_EV_BOARD_BOOT       = 10u,  /* [EN] the monitored board restarted / [FA] برد تحت پایش ری‌استارت شد */
    UP_EV_PANEL_BOOT       = 11u,  /* [EN] this panel started / [FA] خود پنل روشن شد */
    UP_EV_CHG_CUT          = 12u,  /* [EN] a charger was cut by an admin / [FA] شارژر از پنل قطع شد */
    UP_EV_CHG_ON           = 13u,  /* [EN] a charger was reconnected / [FA] شارژر از پنل وصل شد */
    UP_EV_IMBALANCE        = 14u,  /* [EN] imbalance episode (board counter grew) / [FA] رویداد عدم‌توازن */
    UP_EV_PURGE            = 15u,  /* [EN] history purged by an admin / [FA] تاریخچه از پنل پاک شد */
    UP_EV_MANUAL_ON        = 16u   /* [EN] engineering manual mode seen / [FA] مود دستی مهندسی دیده شد */
} up_event_code_t;

/* [EN] Severity carried with the event so the UI can colour a row without
   needing the catalog.
   [FA] شدت همراه رویداد ذخیره می‌شود تا رابط کاربری بدون کاتالوگ هم رنگ بدهد. */
#define UP_SEV_INFO  0u
#define UP_SEV_WARN  1u
#define UP_SEV_CRIT  2u

/* ==================== Fault bits (mirror of app_types.h) / بیت‌های خطا ==================== */
/* [EN] The board's own mask, copied here so the panel can name each bit. It is
   READ-ONLY data: the panel never writes fault bits.
   [FA] همان ماسک خود برد، اینجا کپی شده تا پنل هر بیت را نام‌گذاری کند؛
   فقط خواندنی است و پنل هرگز بیتی نمی‌نویسد. */
#define UP_FAULT_ADC            (1u << 0)
#define UP_FAULT_OVERCURRENT_1  (1u << 1)
#define UP_FAULT_OVERCURRENT_2  (1u << 2)
#define UP_FAULT_LOW_BATTERY    (1u << 3)
#define UP_FAULT_JITTER_1       (1u << 4)
#define UP_FAULT_JITTER_2       (1u << 5)
#define UP_FAULT_BAT_LOST       (1u << 6)

/* [EN] Charger states, same numbering as the board (spec section 6).
   [FA] حالت‌های شارژر با همان شماره‌گذاری برد. */
#define UP_ST_OFF        0u
#define UP_ST_BULK       1u
#define UP_ST_ABSORB     2u
#define UP_ST_FLOAT      3u
#define UP_ST_BRINGUP    4u
#define UP_ST_JIT_RETRY  5u
#define UP_ST_INPUT_WAIT 6u
#define UP_ST_FINAL_FAULT 7u
#define UP_ST_BAT_LOST   8u
#define UP_ST_MANUAL     9u

/* ==================== Telemetry word order / ترتیب کلمات تلمتری ==================== */
/* [EN] The frozen wire order of the u32 words in `/t` (ESP_AGENT_SPEC.md
   section 6). Both the link parser and the history logic index with these
   names, so a field can never be read from the wrong word by accident.
   [FA] ترتیب ثابت کلمات ۳۲ بیتی در `/t` (بخش ۶ سند). هم پارسر لینک و هم منطق
   تاریخچه با این نام‌ها ایندکس می‌کنند تا هیچ فیلدی تصادفی از کلمهٔ غلط خوانده
   نشود. */
typedef enum
{
    UP_TLM_RAW1 = 0u,        /* [EN] channel-1 raw ADC counts / [FA] شمارش خام ADC کانال ۱ */
    UP_TLM_SHUNT1,           /* [EN] channel-1 shunt voltage, µV / [FA] ولتاژ شنت ۱ (µV) */
    UP_TLM_MA1_UNFILTERED,   /* [EN] channel-1 current, mA / [FA] جریان ۱ خام (mA) */
    UP_TLM_MA1_FILTERED,     /* [EN] channel-1 current, filtered mA / [FA] جریان ۱ فیلترشده */
    UP_TLM_MA1_IEST,         /* [EN] channel-1 best-estimate current, mA / [FA] جریان ۱ برآوردی */
    UP_TLM_DUTY1,            /* [EN] channel-1 duty, permille / [FA] duty کانال ۱ (پرمیل) */
    UP_TLM_STATE1,           /* [EN] charger-1 state code / [FA] کد حالت شارژر ۱ */
    UP_TLM_RAW2,             /* [EN] channel-2 raw ADC counts / [FA] شمارش خام ADC کانال ۲ */
    UP_TLM_SHUNT2,           /* [EN] channel-2 shunt voltage, µV / [FA] ولتاژ شنت ۲ */
    UP_TLM_MA2_UNFILTERED,   /* [EN] channel-2 current, mA / [FA] جریان ۲ خام */
    UP_TLM_MA2_FILTERED,     /* [EN] channel-2 current, filtered mA / [FA] جریان ۲ فیلترشده */
    UP_TLM_MA2_IEST,         /* [EN] channel-2 best-estimate current, mA / [FA] جریان ۲ برآوردی */
    UP_TLM_DUTY2,            /* [EN] channel-2 duty, permille / [FA] duty کانال ۲ */
    UP_TLM_STATE2,           /* [EN] charger-2 state code / [FA] کد حالت شارژر ۲ */
    UP_TLM_VIN,              /* [EN] 24 V input voltage, mV / [FA] ولتاژ ورودی ۲۴ ولت */
    UP_TLM_V24,              /* [EN] 24 V pack voltage, mV / [FA] ولتاژ پک ۲۴ ولت */
    UP_TLM_V12,              /* [EN] 12 V lower-battery voltage, mV / [FA] باتری پایینی ۱۲ ولت */
    UP_TLM_VLOW,             /* [EN] lower half voltage, mV / [FA] نیمهٔ پایینی */
    UP_TLM_VHIGH,            /* [EN] upper half voltage, mV / [FA] نیمهٔ بالایی */
    UP_TLM_FAULTS,           /* [EN] fault mask bits / [FA] ماسک خطا */
    UP_TLM_VIN_RAW,          /* [EN] input divider raw counts / [FA] شمارش خام تقسیم‌کنندهٔ ورودی */
    UP_TLM_V24_RAW,          /* [EN] pack divider raw counts / [FA] شمارش خام پک */
    UP_TLM_V12_RAW,          /* [EN] 12 V divider raw counts / [FA] شمارش خام ۱۲ ولت */
    UP_TLM_VREF_RAW,         /* [EN] internal reference raw counts / [FA] شمارش مرجع داخلی */
    UP_TLM_VDDA,             /* [EN] measured VDDA, mV / [FA] ولتاژ VDDA اندازه‌گیری‌شده */
    UP_TLM_IMB_MV,           /* [EN] imbalance millivolts / [FA] عدم‌توازن (mV) */
    UP_TLM_IMB_EVENTS,       /* [EN] imbalance episode counter / [FA] شمارندهٔ رویداد عدم‌توازن */
    UP_TLM_IMB_CYCLES,       /* [EN] latched cycles counter / [FA] شمارندهٔ چرخه‌های قفل‌شده */
    UP_TLM_FIELD_COUNT
} up_tlm_field_t;

/* ==================== On-flash records / رکوردهای روی فلش ==================== */
/* [EN] One stored sample: the numbers a chart needs, nothing more. 22 bytes
   each, so a 256 KB budget already holds ~11500 samples (about 2 days at the
   15 s cadence). The time stamp is MONOTONIC across reboots, not uptime, so a
   chart never shows time running backwards after a restart.
   [FA] یک نمونهٔ ذخیره‌شده: همان اعدادی که نمودار لازم دارد و نه بیشتر.
   ۲۲ بایت، پس سهمیهٔ ۲۵۶ کیلوبایتی حدود ۱۱۵۰۰ نمونه (حدود ۲ روز با بازهٔ
   ۱۵ ثانیه) جا می‌دهد. */
typedef struct
{
    uint32_t uint32_t__absS;      /* [EN] monotonic seconds since the panel's FIRST boot / [FA] ثانیهٔ یکنوا از اولین بوت پنل */
    uint16_t uint16_t__vinMv;     /* [EN] 24 V input, mV / [FA] ورودی ۲۴ ولت */
    uint16_t uint16_t__v24Mv;     /* [EN] pack, mV / [FA] پک */
    uint16_t uint16_t__v12Mv;     /* [EN] lower battery, mV / [FA] باتری پایینی */
    uint16_t uint16_t__i1Ma;      /* [EN] channel-1 battery current, mA / [FA] جریان باتری کانال ۱ */
    uint16_t uint16_t__i2Ma;      /* [EN] channel-2 battery current, mA / [FA] جریان باتری کانال ۲ */
    uint16_t uint16_t__duty1;     /* [EN] channel-1 duty, permille / [FA] duty کانال ۱ (پرمیل) */
    uint16_t uint16_t__duty2;     /* [EN] channel-2 duty, permille / [FA] duty کانال ۲ */
    uint8_t uint8_t__state1;      /* [EN] charger-1 state / [FA] حالت شارژر ۱ */
    uint8_t uint8_t__state2;      /* [EN] charger-2 state / [FA] حالت شارژر ۲ */
    uint8_t uint8_t__flags;       /* [EN] board telemetry flags / [FA] فلگ‌های تلمتری برد */
    uint8_t uint8_t__flags2;      /* [EN] imbalance status bits / [FA] بیت‌های عدم‌توازن */
} up_sample_t;

/* [EN] One stored event. `durS` is filled when the event CLOSES (a charge that
   lasted an hour is one row, not 240 start/end pairs); `open` means it is
   still running and must be closed by the next sample.
   [FA] یک رویداد ذخیره‌شده. `durS` موقع بسته‌شدن پر می‌شود (شارژ یک‌ساعته
   یک ردیف است، نه ۲۴۰ جفت شروع/پایان)؛ `open` یعنی هنوز در جریان است و باید
   با نمونهٔ بعدی بسته شود. */
typedef struct
{
    uint32_t uint32_t__absS;      /* [EN] start, monotonic seconds since first boot / [FA] شروع، ثانیهٔ یکنوا از اولین بوت */
    uint32_t uint32_t__durS;      /* [EN] duration in seconds / [FA] مدت به ثانیه */
    uint32_t uint32_t__epoch;     /* [EN] wall clock at the time, 0 when unknown / [FA] ساعت مطلق همان لحظه، صفر اگر نامعلوم */
    uint16_t uint16_t__valueA;    /* [EN] meaning depends on the code / [FA] معنا بسته به کد */
    uint16_t uint16_t__valueB;    /* [EN] second value / [FA] مقدار دوم */
    uint8_t uint8_t__code;        /* [EN] up_event_code_t / [FA] کد رویداد */
    uint8_t uint8_t__channel;     /* [EN] 1, 2 or 0 = whole unit / [FA] ۱، ۲ یا ۰ = کل دستگاه */
    uint8_t uint8_t__severity;    /* [EN] UP_SEV_* / [FA] شدت */
    uint8_t uint8_t__flags;       /* [EN] bit0 = still open / [FA] بیت ۰ = هنوز باز */
} up_event_t;
#define UP_EVENT_FLAG_OPEN 0x01u

/* [EN] One day of rolled-up statistics. Kept long after the raw samples are
   gone, which is what makes a month of "how many charges" possible on a small
   flash at all.
   [FA] آمار یک روز. بسیار بیشتر از نمونه‌های خام می‌ماند و همین است که
   «چند بار شارژ در یک ماه» را روی یک فلش کوچک ممکن می‌کند. */
typedef struct
{
    uint32_t uint32_t__dayIndex;      /* [EN] days since 2000-01-01 (panel clock) / [FA] روز از ۲۰۰۰-۰۱-۰۱ */
    uint16_t uint16_t__charges;       /* [EN] completed charges / [FA] شارژهای کامل */
    uint16_t uint16_t__incomplete;    /* [EN] charges that did not finish / [FA] شارژهای ناتمام */
    uint32_t uint32_t__chargeSeconds; /* [EN] total charging time / [FA] مجموع زمان شارژ */
    uint32_t uint32_t__runSeconds;    /* [EN] total time on battery / [FA] مجموع زمان روی باتری */
    uint32_t uint32_t__inputSeconds;  /* [EN] total time on input / [FA] مجموع زمان روی ورودی */
    uint16_t uint16_t__outages;       /* [EN] input-loss count / [FA] تعداد قطع ورودی */
    uint16_t uint16_t__boots;         /* [EN] board restarts seen / [FA] ری‌استارت‌های برد */
    uint32_t uint32_t__energyWh100;   /* [EN] charged energy, 0.01 Wh units / [FA] انرژی شارژشده (صدم وات‌ساعت) */
    uint16_t uint16_t__maxV;          /* [EN] highest pack voltage, mV / [FA] بیشترین ولتاژ پک */
    uint16_t uint16_t__minV;          /* [EN] lowest pack voltage, mV / [FA] کمترین ولتاژ پک */
    uint8_t  uint8_t__hourCharges[UP_DAY_HOURS]; /* [EN] charges started in each hour of THIS day / [FA] شارژهای شروع‌شده در هر ساعت از همین روز */
} up_daily_t;

/* [EN] Lifetime totals. Small enough to rewrite whole, which is why the panel
   can answer "since the beginning" questions without keeping every sample.
   [FA] جمع کل از ابتدا. آن‌قدر کوچک است که یک‌جا بازنویسی شود؛ همین باعث
   می‌شود پنل بدون نگه‌داشتن همهٔ نمونه‌ها به سؤال «از اول» جواب بدهد. */
typedef struct
{
    uint32_t uint32_t__magic;         /* [EN] format stamp / [FA] مهر قالب */
    uint32_t uint32_t__charges;       /* [EN] completed charges / [FA] شارژهای کامل */
    uint32_t uint32_t__incomplete;    /* [EN] unfinished charges / [FA] شارژهای ناتمام */
    uint32_t uint32_t__sumChargeS;    /* [EN] sum of charge durations / [FA] مجموع مدت شارژها */
    uint32_t uint32_t__minChargeS;    /* [EN] shortest charge / [FA] کوتاه‌ترین شارژ */
    uint32_t uint32_t__maxChargeS;    /* [EN] longest charge / [FA] بلندترین شارژ */
    uint16_t uint16_t__hist[UP_EVENT_HIST_BUCKETS]; /* [EN] duration histogram / [FA] هیستوگرام مدت */
    uint32_t uint32_t__outages;       /* [EN] input-cut count / [FA] تعداد قطع ورودی */
    uint32_t uint32_t__outageS;       /* [EN] total time without input / [FA] مجموع زمان بی‌ورودی */
    uint32_t uint32_t__runS;          /* [EN] total time on battery / [FA] مجموع زمان روی باتری */
    uint32_t uint32_t__inputS;        /* [EN] total time on input / [FA] مجموع زمان روی ورودی */
    uint32_t uint32_t__maxRunS;       /* [EN] longest single battery run / [FA] بلندترین باری */
    uint32_t uint32_t__chargeWh100;   /* [EN] charged energy, 0.01 Wh / [FA] انرژی شارژشده (صدم وات‌ساعت) */
    uint32_t uint32_t__runWh100;      /* [EN] discharged energy, 0.01 Wh / [FA] انرژی مصرف‌شده (صدم وات‌ساعت) */
    uint32_t uint32_t__boots;         /* [EN] board restarts / [FA] ری‌استارت برد */
    uint32_t uint32_t__imbEvents;     /* [EN] imbalance episodes / [FA] رویدادهای عدم‌توازن */
    uint32_t uint32_t__peakI1;        /* [EN] peak channel-1 current, mA / [FA] اوج جریان ۱ */
    uint32_t uint32_t__peakI2;        /* [EN] peak channel-2 current, mA / [FA] اوج جریان ۲ */
    uint32_t uint32_t__peakDuty1;     /* [EN] peak duty, permille / [FA] اوج duty ۱ */
    uint32_t uint32_t__peakDuty2;     /* [EN] peak duty, permille / [FA] اوج duty ۲ */
    uint16_t uint16_t__maxV;          /* [EN] highest pack voltage, mV / [FA] بیشترین ولتاژ پک */
    uint16_t uint16_t__minV;          /* [EN] lowest pack voltage, mV / [FA] کمترین ولتاژ پک */
    uint32_t uint32_t__coverageS;     /* [EN] seconds actually recorded / [FA] ثانیه‌های ثبت‌شده */
    uint32_t uint32_t__clockBaseS;    /* [EN] monotonic seconds since the first boot / [FA] ثانیهٔ یکنوا از اولین بوت */
    uint32_t uint32_t__clockEpochS;   /* [EN] wall clock captured at that same moment / [FA] ساعت مطلق همان لحظه */
    int32_t  int32_t__tzOffsetS;      /* [EN] local offset the owner set / [FA] اختلاف محلی که صاحب دستگاه ست کرده */
} up_totals_t;
/* [EN] "UPT2": the record grew a timezone field next to the clock pair. The
   stamp is bumped rather than pretending the old layout still fits - a panel
   that reads a shorter record would keep whatever happened to be in RAM.
   [FA] «UPT2»: رکورد کنار جفت ساعت، فیلد منطقهٔ زمانی گرفت. مهر عوض می‌شود
   نه اینکه ادعا کنیم چیدمان قبلی هنوز جا می‌شود - پنلی که رکورد کوتاه‌تر
   بخواند، هر چه در RAM بوده نگه می‌داشت. */
#define UP_TOTALS_MAGIC 0x55505432u

/* ==================== Live telemetry / تلمتری زنده ==================== */
/* [EN] What the last good /t frame contained. `ageMs` is how long ago it
   arrived - the single number that says whether the display is live.
   [FA] محتوای آخرین فریم سالم /t. `ageMs` می‌گوید چند وقت پیش رسیده؛ همان
   عددی که «زنده بودن» نمایش را تعیین می‌کند. */
typedef struct
{
    bool bool__seen;                  /* [EN] at least one frame ever parsed / [FA] حداقل یک فریم خوانده شده */
    uint32_t uint32_t__atMs;          /* [EN] millis() of the last good frame / [FA] زمان آخرین فریم سالم */
    uint16_t uint16_t__seq;           /* [EN] board sequence number / [FA] شمارهٔ ترتیب برد */
    uint8_t uint8_t__flags;           /* [EN] telemetry flags byte / [FA] بایت فلگ‌ها */
    uint8_t uint8_t__flags2;          /* [EN] imbalance status byte / [FA] بایت وضعیت عدم‌توازن */
    uint32_t uint32_t__frames;        /* [EN] frames parsed since boot / [FA] فریم‌های خوانده‌شده */
    uint32_t uint32_t__versionMismatch; /* [EN] vm field from /t / [FA] فیلد vm */
    uint32_t uint32_t__crcErrors;     /* [EN] ce field from /t / [FA] فیلد ce */
    uint32_t uint32_t__t[UP_TLM_MAX_FIELDS]; /* [EN] u32 telemetry words / [FA] کلمات تلمتری */
    uint8_t uint8_t__tCount;          /* [EN] how many were parsed / [FA] چند مورد خوانده شد */
    bool bool__paramKnown[UP_PARAM_COUNT];
    int32_t int32_t__param[UP_PARAM_COUNT];
} up_telemetry_t;

/* [EN] Peaks that only the board's own statistics window can catch (they can
   happen between two 1 s polls). Read from /m, never reset by this panel: the
   bench workflow owns that window.
   [FA] اوج‌هایی که فقط پنجرهٔ آماری خود برد می‌بیند (بین دو پایش ۱ ثانیه‌ای
   ممکن است رخ دهند). از /m خوانده می‌شود و این پنل هرگز آن را صفر نمی‌کند؛
   آن پنجره مالِ کار بنچ است. */
typedef struct
{
    bool bool__seen;
    uint32_t uint32_t__atMs;
    uint32_t uint32_t__count;         /* [EN] frames in the window / [FA] فریم‌های پنجره */
    uint32_t uint32_t__peakI1;
    uint32_t uint32_t__peakI2;
    uint32_t uint32_t__peakDuty1;
    uint32_t uint32_t__peakDuty2;
    uint32_t uint32_t__maxV24;
    uint32_t uint32_t__minV24;
    uint32_t uint32_t__faultOr;       /* [EN] OR of every fault seen / [FA] OR همهٔ خطاهای دیده‌شده */
} up_peaks_t;

/* [EN] Wall clock without an RTC: the browser hands its epoch over once and the
   panel counts from there. Until that happens the UI is told the time is
   unknown instead of inventing one.
   [FA] ساعت بدون RTC: مرورگر یک‌بار زمانش را می‌دهد و پنل از آن لحظه می‌شمارد.
   تا آن موقع، رابط کاربری «زمان نامعلوم» می‌گیرد، نه یک تاریخ ساختگی. */
typedef struct
{
    bool bool__valid;
    uint32_t uint32_t__baseEpochS;
    uint32_t uint32_t__baseMs;
} up_clock_t;

/* [EN] Panel-wide RAM state. Deliberately one struct so a reviewer can see the
   whole mutable surface in one screen.
   [FA] وضعیت RAM کل پنل. عمداً یک ساختار است تا بازبین کل سطح تغییرپذیر را
   در یک صفحه ببیند. */
typedef struct
{
    up_telemetry_t up_telemetry_t__tlm;
    up_peaks_t up_peaks_t__peaks;
    up_clock_t up_clock_t__clock;
    up_totals_t up_totals_t__totals;
    bool bool__clockFlashDirty;
    bool bool__storageOk;
    bool bool__storagePurging;        /* [EN] oldest data is being dropped / [FA] قدیمی‌ترین داده‌ها پاک می‌شوند */
    uint32_t uint32_t__storageUsed;
    uint32_t uint32_t__storageTotal;
    uint32_t uint32_t__lastPurgeMs;
    uint32_t uint32_t__clockBaseAtBootS; /* [EN] monotonic base restored at boot / [FA] مبنای یکنوا که در بوت بازیابی می‌شود */
    uint32_t uint32_t__vmChangedMs;   /* [EN] last time the board's version-mismatch counter grew / [FA] آخرین رشد شمارندهٔ ناهماهنگی نسخه */
    uint32_t uint32_t__ceChangedMs;   /* [EN] last time the board's CRC-error counter grew / [FA] آخرین رشد شمارندهٔ خطای CRC */
    uint32_t uint32_t__lastErrorMs;   /* [EN] last failed read from the board / [FA] آخرین خواندن ناموفق از برد */
    bool bool__staUp;                 /* [EN] joined the engineering AP / [FA] به AP مهندسی وصل است */
    uint32_t uint32_t__tlmErrors;     /* [EN] failed reads / [FA] خواندن‌های ناموفق */
    char     char__LastActionStamp[24]; /* [EN] stamp of the last accepted admin action /
                                           [FA] مهر آخرین اقدام پذیرفته‌شدهٔ مدیر */
} up_panel_state_t;

extern up_panel_state_t UPPANEL_STATE_T__G__State;

/* [EN] Split-into-steps helpers shared by the whole panel. Kept tiny and named
   after what they answer, never "util" or "helper".
   [FA] کمکی‌های گام‌به‌گام مشترک پنل. کوچک و نام‌دار بر اساس کاری که می‌کنند،
   نه «util» یا «helper». */
bool func__UpState_LinkOnline(void);
uint32_t func__UpState_AgeMs(void);
uint32_t func__UpState_UptimeS(void);
uint32_t func__UpState_NowEpochS(void);
uint32_t func__UpState_DayIndex(void);
uint32_t func__UpState_ClockHour(void);
uint32_t func__UpState_TlmWord(uint8_t uint8_t__index);
uint16_t func__UpState_BatteryPercent(uint16_t uint16_t__mv);
uint8_t  func__UpState_InputPresent(void);
uint32_t func__UpState_EpochFromAbs(uint32_t uint32_t__absS);
void     func__UpState_ClockRestore(void);
int32_t  func__UpState_TzOffsetS(void);
bool     func__UpState_TzIsValid(int32_t int32_t__tzOffsetS);
void     func__UpState_SetClock(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS);

/* ==================== Implementation / پیاده‌سازی ====================
   [EN] Included by user_panel.ino (a single translation unit), so the small
   helpers live here next to their declarations. They are intentionally tiny
   and each one answers exactly one question.
   [FA] توسط user_panel.ino (یک واحد ترجمه) وارد می‌شود، پس کمکی‌های کوچک
   همین‌جا کنار اعلانشان هستند. عمداً کوچک‌اند و هرکدام دقیقاً به یک سؤال
   جواب می‌دهند. */

up_panel_state_t UPPANEL_STATE_T__G__State;

/* ==================== Link health / سلامت لینک ==================== */
/**
 * @brief  [EN] Is the board's data fresh enough to be shown as live?
 *         [FA] دادهٔ برد آن‌قدر تازه هست که «زنده» نشان داده شود؟
 * @return [EN] true when a frame arrived within UP_LINK_TIMEOUT_MS / [FA] اگر فریم در بازهٔ مهلت رسیده باشد true
 */
bool func__UpState_LinkOnline(void)
{
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;

    if (!up_telemetry_t__tlm->bool__seen)
    {
        return false;
    }

    return (func__UpState_AgeMs() <= UP_LINK_TIMEOUT_MS);
}

/**
 * @brief  [EN] Milliseconds since the last parsed frame / [FA] میلی‌ثانیه از آخرین فریم خوانده‌شده
 * @return [EN] Age in ms; large value when nothing was ever seen / [FA] سن به ms؛ اگر چیزی دیده نشده مقدار بزرگ
 */
uint32_t func__UpState_AgeMs(void)
{
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;

    if (!up_telemetry_t__tlm->bool__seen)
    {
        return 0xFFFFFFFFu;
    }

    return (uint32_t)(millis() - up_telemetry_t__tlm->uint32_t__atMs);
}

/**
 * @brief  [EN] Seconds since this panel started / [FA] ثانیه از روشن‌شدن پنل
 * @return [EN] Uptime in seconds / [FA] زمان کارکرد به ثانیه
 */
uint32_t func__UpState_UptimeS(void)
{
    uint32_t uint32_t__msNow = (uint32_t)millis();
    uint32_t uint32_t__seconds = uint32_t__msNow / 1000u;

    return uint32_t__seconds;
}

/**
 * @brief  [EN] Wall-clock epoch seconds, or 0 when no browser has set the clock.
 *         [FA] زمان مطلق به ثانیه؛ اگر هیچ مرورگری ساعت را نداده باشد صفر.
 * @return [EN] Epoch seconds or 0 / [FA] ثانیهٔ مطلق یا صفر
 */
uint32_t func__UpState_NowEpochS(void)
{
    up_clock_t *up_clock_t__clock = &UPPANEL_STATE_T__G__State.up_clock_t__clock;

    if (!up_clock_t__clock->bool__valid)
    {
        return 0u;
    }

    uint32_t uint32_t__elapsedMs = (uint32_t)(millis() - up_clock_t__clock->uint32_t__baseMs);
    uint32_t uint32_t__elapsedS = uint32_t__elapsedMs / 1000u;
    uint32_t uint32_t__epoch = up_clock_t__clock->uint32_t__baseEpochS + uint32_t__elapsedS;

    return uint32_t__epoch;
}

/**
 * @brief  [EN] Day bucket for the daily rollups; falls back to uptime days so a
 *              panel that never saw a browser still groups consistently.
 *         [FA] سبد روزانه برای آمار روز؛ اگر ساعت مرورگر نبود از روزهای کارکرد می‌سازد.
 * @return [EN] Day index / [FA] شاخص روز
 */
uint32_t func__UpState_DayIndex(void)
{
    uint32_t uint32_t__epoch = func__UpState_NowEpochS();
    uint32_t uint32_t__day;

    if (uint32_t__epoch != 0u)
    {
        /* [EN] The LOCAL day, not the UTC one: with a UTC boundary the panel
           would roll its day over at 03:30 in Tehran and every daily bar would
           belong to the wrong date.
           [FA] روز محلی، نه UTC: با مرز UTC پنل ساعت ۳:۳۰ بامداد در تهران روزش
           عوض می‌شد و هر میلهٔ روزانه به تاریخ اشتباه می‌چسبید. */
        int32_t int32_t__localDay = func__UpCal_LocalDay(uint32_t__epoch, func__UpState_TzOffsetS());

        if (int32_t__localDay <= 0)
        {
            return 1u;
        }
        uint32_t__day = (uint32_t)int32_t__localDay;
        return uint32_t__day;
    }

    /* [EN] Before a browser has told the panel what time it is, a "day" is a
            day of uptime - and it starts at ONE, not zero, because a stored
            daily row whose day index is zero means "this slot has never been
            written". Without the +1 the panel's own first day would look like
            an empty slot and its rollups would be discarded at every reboot.
       [FA] پیش از آنکه مرورگری بگوید ساعت چند است، «روز» یعنی روزِ روشن‌بودن،
            و از یک شروع می‌شود نه صفر، چون ردیف روزانه‌ای که شمارهٔ روزش صفر
            باشد یعنی «این خانه هرگز نوشته نشده». بدون این +1، همان روز اول خود
            پنل شبیه خانهٔ خالی به نظر می‌رسید و جمع‌هایش در هر ری‌استارت دور
            ریخته می‌شد. */
    uint32_t__day = 1u + (func__UpState_UptimeS() / 86400u);
    return uint32_t__day;
}

/**
 * @brief  [EN] Hour of day (0..23) used by the heat map / [FA] ساعت شبانه‌روز (۰..۲۳) برای نقشهٔ حرارتی
 * @return [EN] Hour; 0 when the clock is unknown / [FA] ساعت؛ صفر وقتی ساعت نامعلوم است
 */
uint32_t func__UpState_ClockHour(void)
{
    uint32_t uint32_t__epoch = func__UpState_NowEpochS();

    if (uint32_t__epoch == 0u)
    {
        return 0u;
    }

    /* [EN] The hour the owner's clock shows, not the UTC hour.
       [FA] ساعتی که ساعت صاحب دستگاه نشان می‌دهد، نه ساعت UTC. */
    return (func__UpCal_LocalSecondOfDay(uint32_t__epoch, func__UpState_TzOffsetS()) / 3600u);
}

/**
 * @brief  [EN] The stored local offset, with the default substituted when the
 *              flash carries something outside the real world (-12h..+14h) -
 *              a corrupted byte must not move the panel to a timezone that does
 *              not exist.
 *         [FA] اختلاف محلی ذخیره‌شده، و اگر فلش چیزی بیرون از دنیای واقعی
 *              (‎−۱۲ تا ‎+۱۴ ساعت) داشت، پیش‌فرض جایگزین می‌شود - یک بایت خراب
 *              نباید پنل را به منطقه‌ای ببرد که وجود ندارد.
 * @return [EN] offset in seconds / [FA] اختلاف به ثانیه
 */
int32_t func__UpState_TzOffsetS(void)
{
    int32_t int32_t__tz = UPPANEL_STATE_T__G__State.up_totals_t__totals.int32_t__tzOffsetS;

    if (!func__UpState_TzIsValid(int32_t__tz))
    {
        return (int32_t)UP_TZ_DEFAULT_OFFSET_S;
    }

    return int32_t__tz;
}

/**
 * @brief  [EN] Is an offset a real place's offset? (Whole minutes between -12
 *              and +14 hours; a value with seconds in it means the bytes are not
 *              an offset at all.)
 *         [FA] آیا این اختلاف، اختلاف جایی واقعی است؟ (دقیقهٔ کامل بین ‎−۱۲ و
 *              ‎+۱۴ ساعت؛ مقداری که ثانیه داشته باشد اصلاً اختلاف نیست.)
 * @param  int32_t__tzOffsetS [EN] candidate / [FA] نامزد
 * @return [EN] true when acceptable / [FA] در صورت قبول true
 */
bool func__UpState_TzIsValid(int32_t int32_t__tzOffsetS)
{
    if ((int32_t__tzOffsetS < (int32_t)UP_TZ_MIN_OFFSET_S) ||
        (int32_t__tzOffsetS > (int32_t)UP_TZ_MAX_OFFSET_S))
    {
        return false;
    }

    return ((int32_t__tzOffsetS % 60) == 0);
}

/**
 * @brief  [EN] Set the panel clock and its local offset together, from the
 *              admin's form. The offset is validated here rather than at the
 *              call site so no future route can store an impossible one.
 *         [FA] ست کردن ساعت پنل و اختلاف محلی‌اش با هم، از فرم مدیر. اعتبارسنجی
 *              اینجا انجام می‌شود نه در محل فراخوان، تا هیچ مسیر آینده‌ای نتواند
 *              مقدار غیرممکن ذخیره کند.
 * @param  uint32_t__epochS [EN] epoch seconds / [FA] ثانیهٔ مطلق
 * @param  int32_t__tzOffsetS [EN] local offset; an invalid one falls back to the
 *              default / [FA] اختلاف محلی؛ مقدار نامعتبر به پیش‌فرض برمی‌گردد
 * @return [EN] None / [FA] ندارد
 */
void func__UpState_SetClock(uint32_t uint32_t__epochS, int32_t int32_t__tzOffsetS)
{
    up_clock_t *up_clock_t__clock = &UPPANEL_STATE_T__G__State.up_clock_t__clock;

    up_clock_t__clock->bool__valid = true;
    up_clock_t__clock->uint32_t__baseEpochS = uint32_t__epochS;
    up_clock_t__clock->uint32_t__baseMs = (uint32_t)millis();

    UPPANEL_STATE_T__G__State.up_totals_t__totals.int32_t__tzOffsetS =
        func__UpState_TzIsValid(int32_t__tzOffsetS) ? int32_t__tzOffsetS : (int32_t)UP_TZ_DEFAULT_OFFSET_S;
}

/**
 * @brief  [EN] One telemetry word with a safe default.
 *         [FA] یک کلمهٔ تلمتری با مقدار پیش‌فرض امن.
 * @param  uint8_t__index [EN] word index 0..UP_TLM_FIELD_COUNT-1 / [FA] شاخص کلمه
 * @return [EN] Word value, or 0 when out of range / [FA] مقدار کلمه یا صفر بیرون از بازه
 */
uint32_t func__UpState_TlmWord(uint8_t uint8_t__index)
{
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;

    if (uint8_t__index >= up_telemetry_t__tlm->uint8_t__tCount)
    {
        return 0u;
    }

    return up_telemetry_t__tlm->uint32_t__t[uint8_t__index];
}

/**
 * @brief  [EN] Wall clock for a moment recorded as monotonic seconds: the ONE
 *              conversion every chart, event row and CSV column goes through.
 *              (base epoch) + (record - base record) is deliberately computed in
 *              unsigned 32-bit arithmetic, so a record written before the base
 *              was refreshed still lands on the right second by wrapping.
 *         [FA] ساعت مطلق برای لحظه‌ای که به‌صورت ثانیهٔ یکنوا ثبت شده: تنها
 *              تبدیلی که هر نمودار، ردیف رویداد و ستون CSV از آن می‌گذرد.
 *              (مبنای مطلق) + (رکورد - رکورد مبنا) عمداً با حساب ۳۲ بیتی
 *              بدون علامت انجام می‌شود، تا رکوردی که پیش از تازه‌سازی مبنا
 *              نوشته شده هم با دور زدن، روی ثانیهٔ درست بیفتد.
 * @param  uint32_t__absS [EN] monotonic seconds of the record / [FA] ثانیهٔ یکنوای رکورد
 * @return [EN] epoch seconds, or 0 when no clock has ever been set
 *          [FA] ثانیهٔ مطلق، یا صفر وقتی هیچ ساعتی تنظیم نشده
 */
uint32_t func__UpState_EpochFromAbs(uint32_t uint32_t__absS)
{
    up_totals_t *up_totals_t__totals = &UPPANEL_STATE_T__G__State.up_totals_t__totals;

    if (up_totals_t__totals->uint32_t__clockEpochS == 0u)
    {
        return 0u;
    }

    uint32_t uint32_t__offset = uint32_t__absS - up_totals_t__totals->uint32_t__clockBaseS;
    uint32_t uint32_t__epoch = up_totals_t__totals->uint32_t__clockEpochS + uint32_t__offset;

    return uint32_t__epoch;
}

/**
 * @brief  [EN] Rebuild the RAM clock from the pair saved on flash, so the very
 *              first telemetry frame after a reboot already knows what time it
 *              is - before any browser has said a word.
 *         [FA] بازسازی ساعت RAM از جفتی که روی فلش ذخیره شده، تا همان اولین
 *              فریم تلمتری پس از ری‌استارت بداند ساعت چند است - پیش از آنکه
 *              هیچ مرورگری حرفی بزند.
 * @return [EN] None / [FA] ندارد
 */
void func__UpState_ClockRestore(void)
{
    up_clock_t *up_clock_t__clock = &UPPANEL_STATE_T__G__State.up_clock_t__clock;
    up_totals_t *up_totals_t__totals = &UPPANEL_STATE_T__G__State.up_totals_t__totals;

    if (up_totals_t__totals->uint32_t__clockEpochS == 0u)
    {
        return;
    }

    uint32_t uint32_t__absNow = UPPANEL_STATE_T__G__State.uint32_t__clockBaseAtBootS + func__UpState_UptimeS();
    uint32_t uint32_t__offset = uint32_t__absNow - up_totals_t__totals->uint32_t__clockBaseS;

    up_clock_t__clock->bool__valid = true;
    up_clock_t__clock->uint32_t__baseEpochS = up_totals_t__totals->uint32_t__clockEpochS + uint32_t__offset;
    up_clock_t__clock->uint32_t__baseMs = (uint32_t)millis();

    /* [EN] The offset rides along in the same record, so the first daily row
       after a reboot is already keyed on the owner's midnight.
       [FA] اختلاف ساعت در همان رکورد می‌آید، پس اولین ردیف روزانه پس از
       ری‌استارت هم از همان ابتدا بر نیمه‌شب صاحب دستگاه کلید می‌خورد. */
    if (!func__UpState_TzIsValid(up_totals_t__totals->int32_t__tzOffsetS))
    {
        up_totals_t__totals->int32_t__tzOffsetS = (int32_t)UP_TZ_DEFAULT_OFFSET_S;
    }
}

/**
 * @brief  [EN] Pack voltage to percent, using the BOARD's own mapping
 *              parameters (ids 74/75) - the same numbers the board's LEDs use,
 *              so the panel and the machine never disagree.
 *         [FA] ولتاژ پک به درصد، با همان پارامترهای نگاشت خودِ برد (۷۴/۷۵) -
 *              همان اعدادی که LEDهای برد استفاده می‌کنند، تا پنل و دستگاه
 *              هیچ‌وقت دو چیز متفاوت نگویند.
 * @param  uint16_t__mv [EN] Pack voltage in mV, 0..32000 / [FA] ولتاژ پک به mV
 * @return [EN] Percent 0..100 / [FA] درصد ۰..۱۰۰
 */
uint16_t func__UpState_BatteryPercent(uint16_t uint16_t__mv)
{
    int32_t int32_t__minMv = 21000;   /* [EN] board factory default / [FA] پیش‌فرض کارخانهٔ برد */
    int32_t int32_t__maxMv = 29000;
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;

    if (up_telemetry_t__tlm->bool__paramKnown[UP_PARAM_PCT_VMIN] &&
        up_telemetry_t__tlm->bool__paramKnown[UP_PARAM_PCT_VMAX])
    {
        int32_t__minMv = up_telemetry_t__tlm->int32_t__param[UP_PARAM_PCT_VMIN];
        int32_t__maxMv = up_telemetry_t__tlm->int32_t__param[UP_PARAM_PCT_VMAX];
    }

    int32_t int32_t__range = int32_t__maxMv - int32_t__minMv;
    if (int32_t__range <= 0)
    {
        return 0u;
    }

    int32_t int32_t__offset = (int32_t)uint16_t__mv - int32_t__minMv;
    if (int32_t__offset <= 0)
    {
        return 0u;
    }

    int32_t int32_t__scaled = int32_t__offset * 100;
    int32_t int32_t__percent = int32_t__scaled / int32_t__range;
    if (int32_t__percent > 100)
    {
        int32_t__percent = 100;
    }

    return (uint16_t)int32_t__percent;
}

/**
 * @brief  [EN] Does the board see its 24 V input? (telemetry flag bit 1)
 *         [FA] آیا برد ورودی ۲۴ ولت را می‌بیند؟ (بیت ۱ فلگ تلمتری)
 * @return [EN] 1 = present, 0 = absent / [FA] ۱ = وصل، ۰ = قطع
 */
uint8_t func__UpState_InputPresent(void)
{
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;

    if (!up_telemetry_t__tlm->bool__seen)
    {
        return 0u;
    }

    return ((up_telemetry_t__tlm->uint8_t__flags & 0x02u) != 0u) ? 1u : 0u;
}

#endif /* UP_STATE_H */
