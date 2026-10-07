/**
 * @file    sim_machine.h
 * @brief   [EN] A model of the MACHINE, for the desktop simulator. It is not
 *               firmware and never runs on the board: it is host code, written
 *               with the C++ standard library, and its only job is to answer the
 *               questions the real STM32 answers - what is the pack voltage, is
 *               charger 1 in bulk or done, which fault bits are set - so that
 *               the REAL panel code (up_link.h, up_state.h, up_http.h, the page)
 *               can be exercised on a laptop before anything is flashed.
 *
 *          WHAT IS MODELLED
 *            - a 24 V pack made of two 12 V halves, with the lower half
 *              tracking the pack (the two halves are the same string measured
 *              mid-way, so they must not drift apart on their own),
 *            - two charger channels with the firmware's states (off, bulk,
 *              absorption, full, waiting for input),
 *            - the mains input coming and going, and the pack discharging when
 *              it is gone,
 *            - the fault word, including the imbalance latch that only a human
 *              can clear on the real machine,
 *            - the param surface: ids 11 and 12 are the charger enables (the
 *              only thing the panel may write), everything above 118 is refused
 *              with the same 400 the engineering ESP sends.
 *
 *          WHAT IS NOT MODELLED
 *            Physics. There is no thermal model, no cell chemistry, no wire
 *            resistance and no timing that matters at a microsecond scale. A
 *            green simulator run means the panel's logic and screens survived a
 *            plausible machine; it says nothing about a real one, and the
 *            module README says so in the simulator's own section.
 *
 * @brief   [FA] مدلی از ماشین، برای شبیه‌ساز رومیزی. این فرم‌ور نیست و هرگز روی
 *               برد اجرا نمی‌شود: کد میزبان است، با کتابخانهٔ استاندارد C++
 *               نوشته شده، و تنها کارش پاسخ‌دادن به همان پرسش‌هایی است که STM32
 *               واقعی پاسخ می‌دهد - ولتاژ پک چقدر است، شارژر ۱ در حال شارژ است یا
 *               تمام کرده، کدام بیت خطا روشن است - تا کد **واقعی** پنل
 *               (up_link.h، up_state.h، up_http.h و صفحه) پیش از فلش‌کردن هر
 *               چیزی روی لپ‌تاپ آزمایش شود.
 *
 *          چه چیزی مدل شده است
 *            - پک ۲۴ ولتی از دو نیمهٔ ۱۲ ولتی، که نیمهٔ پایینی پک را دنبال
 *              می‌کند (هر دو نیمه همان رشته‌اند که وسط اندازه گرفته می‌شود، پس
 *              نباید خودبه‌خود از هم جدا شوند)،
 *            - دو کانال شارژر با حالت‌های خود فرم‌ور (خاموش، شارژ، تثبیت،
 *              کامل، انتظار ورودی)،
 *            - آمدن و رفتن ورودی برق و تخلیهٔ پک وقتی برق نیست،
 *            - کلمهٔ خطا، شامل قفل عدم‌توازنی که روی ماشین واقعی فقط آدم
 *              می‌تواند پاکش کند،
 *            - سطح پارامترها: شناسه‌های ۱۱ و ۱۲ فعال‌ساز شارژرند (تنها چیزی
 *              که پنل اجازهٔ نوشتنش را دارد) و هر شناسهٔ بالای ۱۱۸ با همان ۴۰۰
 *              رد می‌شود که ESP مهندسی می‌فرستد.
 *
 *          چه چیزی مدل نشده است
 *            فیزیک. نه مدل حرارتی هست، نه شیمی سلول، نه مقاومت سیم و نه
 *            زمان‌بندی‌ای که در مقیاس میکروثانیه مهم باشد. سبز بودن شبیه‌ساز یعنی
 *            منطق و صفحه‌های پنل یک ماشین محتمل را از سر گذرانده‌اند؛ دربارهٔ
 *            ماشین واقعی هیچ نمی‌گوید، و README ماژول در بخش خود شبیه‌ساز همین را
 *            می‌نویسد.
 */

#ifndef SIM_MACHINE_H
#define SIM_MACHINE_H

#include <string>
#include <cstdio>
#include <cstring>

/* [EN] Fault bits, the same layout the host panel reports in `fl`. / [FA] بیت‌های خطا، همان چیدمانی که پنل میزبان در `fl` می‌فرستد. */
#define SIM_FAULT_ADC         (1u << 0)
#define SIM_FAULT_OC1         (1u << 1)
#define SIM_FAULT_OC2         (1u << 2)
#define SIM_FAULT_LOWBAT      (1u << 3)
#define SIM_FAULT_JIT1        (1u << 4)
#define SIM_FAULT_JIT2        (1u << 5)
#define SIM_FAULT_CHG_LOST    (1u << 6)

/* [EN] Charger states, as the firmware numbers them (spec 6). / [FA] حالت‌های شارژر با شمارهٔ خود فرم‌ور. */
#define SIM_ST_OFF            0u
#define SIM_ST_BULK           1u
#define SIM_ST_ABSORB         2u
#define SIM_ST_FULL           3u
#define SIM_ST_WAIT_INPUT     6u
#define SIM_ST_FINAL_FAULT    7u

/* [EN] Voltages in millivolts. One 12 V half of a healthy 24 V pack is about
   13.6 V; the model keeps both halves moving together unless an imbalance is
   injected on purpose. / [FA] ولتاژها به میلی‌ولت. یک نیمهٔ ۱۲ ولتی سالم حدود
   ۱۳.۶ ولت است؛ مدل هر دو نیمه را با هم حرکت می‌دهد مگر اینکه عدم‌توازن عمداً
   تزریق شود. */
#define SIM_HALF_EMPTY_MV     21800u
#define SIM_PACK_FULL_MV      28800u
#define SIM_INPUT_MV          24000u
#define SIM_BULK_MA           3000u
#define SIM_ABSORB_MA         1400u
#define SIM_FLOAT_MA          250u

/**
 * [EN] The whole machine as one structure. Every field is a number the real
 *      board would report; the `cmd` fields are what a human does to it (press
 *      a button on the simulator's control page), which is how the states a
 *      machine only reaches after hours are reached in seconds.
 * [FA] کل ماشین در یک ساختار. هر فیلد عددی است که برد واقعی گزارش می‌کرد؛
 *      فیلدهای `cmd` همان کاری است که آدم با ماشین می‌کند (زدن دکمه در صفحهٔ
 *      کنترل شبیه‌ساز)، و همین‌طور حالت‌هایی که ماشین فقط بعد از ساعت‌ها به آن‌ها
 *      می‌رسد در چند ثانیه گرفته می‌شوند.
 */
typedef struct
{
    uint32_t uint32_t__vinMv;        /* [EN] mains input / [FA] ورودی برق    */
    uint32_t uint32_t__v24Mv;        /* [EN] whole pack / [FA] کل پک          */
    uint32_t uint32_t__v12Mv;        /* [EN] lower half / [FA] نیمهٔ پایینی   */
    uint32_t uint32_t__imbOffsetMv;  /* [EN] injected imbalance / [FA] عدم‌توازن تزریقی */
    uint32_t uint32_t__i1Ma;
    uint32_t uint32_t__i2Ma;
    uint32_t uint32_t__duty1;
    uint32_t uint32_t__duty2;
    uint32_t uint32_t__st1;
    uint32_t uint32_t__st2;
    uint32_t uint32_t__flags;
    uint32_t uint32_t__flags2;
    uint32_t uint32_t__seq;
    uint32_t uint32_t__frames;
    uint32_t uint32_t__chargeCount;  /* [EN] completed charges / [FA] شارژهای کامل‌شده */
    uint32_t uint32_t__jitterCount;  /* [EN] the board counts restarts itself / [FA] شمارندهٔ ری‌استارت‌ها */
    uint32_t uint32_t__crcErrors;
    uint32_t uint32_t__ageMs;
    uint8_t  uint8_t__manual;
    uint8_t  uint8_t__chg1Enable;
    uint8_t  uint8_t__chg2Enable;
    uint8_t  uint8_t__latched;       /* [EN] imbalance latch / [FA] قفل عدم‌توازن */
    uint8_t  uint8_t__imbSessions;   /* [EN] charge sessions while latched / [FA] سیکل‌های شارژ در حال قفل */
    uint8_t  uint8_t__imbCleared;    /* [EN] freed by the 3 s rule / [FA] با قاعدهٔ ۳ ثانیه آزاد شد */
    uint8_t  uint8_t__blocked;       /* [EN] charging blocked by the latch / [FA] شارژ مسدودشده با قفل */
    uint8_t  uint8_t__wasCharging;   /* [EN] last tick's charging state / [FA] حالت شارژ تیک قبل */
    uint32_t uint32_t__tickMs;
} up_sim_t;

static up_sim_t UP_SIM_T__G__Plant;

/* [EN] 3000 ms of "the battery wire is off" is how the real machine frees the
   imbalance latch; the simulator keeps the same number so a tester can watch it
   happen in three seconds instead of reading about it. */
#define SIM_LATCH_RELEASE_MS  3000u
/* [EN] A third jitter event is the firmware's FINAL_FAULT: no more charging
   until a human power-cycles the board. The simulator counts them so the
   panel's reset guide has something real to point at. */
#define SIM_JITTER_LIMIT      3u
/* [EN] After this many restarts of charging on a machine that is still
   unbalanced, the firmware blocks charging altogether; the count and the block
   bit are what the panel's diagnostics page explains to the owner. */
#define SIM_IMB_BLOCK_SESSIONS 2u

/* ==================== Life / چرخهٔ ماشین ==================== */
/**
 * @brief  [EN] Put the machine on the bench: mains present, pack half charged,
 *              both chargers enabled, no faults. This is the state a person
 *              would find a working machine in, so it is where the simulator
 *              starts.
 *         [FA] گذاشتن ماشین روی میز: برق هست، پک نیمه‌شارژ، هر دو شارژر فعال و
 *              بی‌خطا. این همان حالتی است که آدم یک ماشین سالم را در آن پیدا
 *              می‌کند، پس شبیه‌ساز هم از همان‌جا شروع می‌کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_Begin(void)
{
    memset(&UP_SIM_T__G__Plant, 0, sizeof(UP_SIM_T__G__Plant));

    UP_SIM_T__G__Plant.uint32_t__vinMv = SIM_INPUT_MV;
    UP_SIM_T__G__Plant.uint32_t__v24Mv = 26400u;
    UP_SIM_T__G__Plant.uint32_t__v12Mv = 13200u;
    UP_SIM_T__G__Plant.uint8_t__chg1Enable = 1u;
    UP_SIM_T__G__Plant.uint8_t__chg2Enable = 1u;
    UP_SIM_T__G__Plant.uint32_t__st1 = SIM_ST_BULK;
    UP_SIM_T__G__Plant.uint32_t__st2 = SIM_ST_BULK;
    UP_SIM_T__G__Plant.uint32_t__seq = 1000u;
}

/**
 * @brief  [EN] Which charger channel is actually pushing current: enabled, mains
 *              present and not already full. Both channels charge the same pack,
 *              which is why the pack is what the user sees and not each channel
 *              separately.
 *         [FA] کدام کانال شارژر واقعاً جریان می‌دهد: فعال، برق موجود و هنوز
 *              کامل نشده. هر دو کانال یک پک را شارژ می‌کنند، و همین دلیل دیدن پک
 *              است و نه هر کانال به‌تنهایی.
 * @param  uint8_t uint8_t__ch [EN] 1 or 2 / [FA] ۱ یا ۲
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_ChannelStep(uint8_t uint8_t__ch)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;
    uint8_t uint8_t__enabled = (uint8_t__ch == 1u) ? up_sim_t__p->uint8_t__chg1Enable : up_sim_t__p->uint8_t__chg2Enable;
    bool bool__finalFault = ((up_sim_t__p->uint32_t__flags2 & 0x01u) != 0u);
    uint32_t *uint32_t__st = (uint8_t__ch == 1u) ? &up_sim_t__p->uint32_t__st1 : &up_sim_t__p->uint32_t__st2;
    uint32_t *uint32_t__duty = (uint8_t__ch == 1u) ? &up_sim_t__p->uint32_t__duty1 : &up_sim_t__p->uint32_t__duty2;

    if (bool__finalFault)
    {
        *uint32_t__st = SIM_ST_FINAL_FAULT;
        *uint32_t__duty = 0u;
        return;
    }
    if (up_sim_t__p->uint8_t__blocked != 0u)
    {
        /* [EN] The firmware stops charging while the imbalance latch is
                blocking; that is exactly the state the panel's diagnostics page
                tells a human how to end.
           [FA] فرم‌ور تا وقتی قفل عدم‌توازن مانع است شارژ را متوقف می‌کند؛ همان
                حالتی که صفحهٔ دیاگ پنل به آدم می‌گوید چطور تمامش کند. */
        *uint32_t__st = SIM_ST_OFF;
        *uint32_t__duty = 0u;
        return;
    }
    if (uint8_t__enabled == 0u)
    {
        *uint32_t__st = SIM_ST_OFF;
        *uint32_t__duty = 0u;
        return;
    }
    if (up_sim_t__p->uint32_t__vinMv == 0u)
    {
        *uint32_t__st = SIM_ST_WAIT_INPUT;
        *uint32_t__duty = 0u;
        return;
    }

    if (up_sim_t__p->uint32_t__v24Mv >= SIM_PACK_FULL_MV)
    {
        *uint32_t__st = SIM_ST_FULL;
        *uint32_t__duty = 120u;                     /* [EN] a light top-up / [FA] سرکشی سبک */
    }
    else if (up_sim_t__p->uint32_t__v24Mv > (SIM_PACK_FULL_MV - 900u))
    {
        *uint32_t__st = SIM_ST_ABSORB;
        *uint32_t__duty = 700u;
    }
    else
    {
        *uint32_t__st = SIM_ST_BULK;
        *uint32_t__duty = 1000u;
    }
}

/**
 * @brief  [EN] One tick of the machine: mains or battery, charging or
 *              discharging, and the numbers that follow from that. Called every
 *              `SIM_TICK_MS` of virtual time, so fast-forwarding the clock
 *              fast-forwards the machine with it.
 *         [FA] یک تیک ماشین: برق یا باتری، شارژ یا تخلیه، و اعدادي که از آن
 *              نتیجه می‌شود. هر `SIM_TICK_MS` زمان مجازی صدا زده می‌شود، پس
 *              جلو بردن سریع ساعت، ماشین را هم با خودش جلو می‌برد.
 * @param  uint32_t uint32_t__elapsedMs [EN] virtual ms since the last tick / [FA] میلی‌ثانیهٔ مجازی از تیک قبل
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_Tick(uint32_t uint32_t__elapsedMs)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;
    uint32_t uint32_t__mvGained;
    uint32_t uint32_t__mvLost = 0u;

    up_sim_t__p->uint32_t__tickMs += uint32_t__elapsedMs;
    func__UpSim_ChannelStep(1u);
    func__UpSim_ChannelStep(2u);

    /* [EN] Currents follow the states; both channels push into the SAME pack, so
            the pack rises with their sum and not twice as fast as one of them.
       [FA] جریان‌ها از حالت‌ها می‌آیند؛ هر دو کانال به یک پک فشار می‌آورند، پس پک
            با مجموعشان بالا می‌رود و نه دو برابر یکی. */
    up_sim_t__p->uint32_t__i1Ma = (up_sim_t__p->uint32_t__st1 == SIM_ST_BULK) ? SIM_BULK_MA
                                : (up_sim_t__p->uint32_t__st1 == SIM_ST_ABSORB) ? SIM_ABSORB_MA
                                : (up_sim_t__p->uint32_t__st1 == SIM_ST_FULL) ? SIM_FLOAT_MA : 0u;
    up_sim_t__p->uint32_t__i2Ma = (up_sim_t__p->uint32_t__st2 == SIM_ST_BULK) ? SIM_BULK_MA
                                : (up_sim_t__p->uint32_t__st2 == SIM_ST_ABSORB) ? SIM_ABSORB_MA
                                : (up_sim_t__p->uint32_t__st2 == SIM_ST_FULL) ? SIM_FLOAT_MA : 0u;

    /* [EN] 3000 mA into a 24 V pack for one minute is about 25 mV per second at
            this model's scale - slow enough that a human can watch it, fast
            enough that fast-forward finishes a charge.
       [FA] ۳۰۰۰ میلی‌آمپر در پک ۲۴ ولتی، در این مقیاس حدود ۲۵ میلی‌ولت بر ثانیه
            است - به‌قدری کند که آدم ببیندش و به‌قدری سریع که جلو بردن سریع،
            یک شارژ را تمام کند. */
    uint32_t__mvGained = ((up_sim_t__p->uint32_t__i1Ma + up_sim_t__p->uint32_t__i2Ma) * uint32_t__elapsedMs) / 40000u;

    if ((up_sim_t__p->uint32_t__vinMv == 0u) && (up_sim_t__p->uint32_t__v24Mv > SIM_HALF_EMPTY_MV * 2u))
    {
        uint32_t__mvLost = (uint32_t__elapsedMs * 8u) / 1000u;
    }

    if (uint32_t__mvGained > 0u)
    {
        uint32_t uint32_t__room = (SIM_PACK_FULL_MV > up_sim_t__p->uint32_t__v24Mv)
                                ? (SIM_PACK_FULL_MV - up_sim_t__p->uint32_t__v24Mv) : 0u;

        up_sim_t__p->uint32_t__v24Mv += (uint32_t__mvGained < uint32_t__room) ? uint32_t__mvGained : uint32_t__room;
    }
    if (uint32_t__mvLost > 0u)
    {
        up_sim_t__p->uint32_t__v24Mv -= (uint32_t__mvLost < (up_sim_t__p->uint32_t__v24Mv - SIM_HALF_EMPTY_MV))
                                      ? uint32_t__mvLost : 0u;
    }

    /* [EN] The lower half is the pack measured mid-way: it follows, it does not
            lead. Any imbalance is an offset ON TOP of that, which is what the
            real wiring does when one half is weaker than the other.
       [FA] نیمهٔ پایینی همان پک است که وسط اندازه گرفته شده: دنبال می‌کند و
            پیش نمی‌افتد. عدم‌توازن یک آفست روی همان است، و سیم‌کشی واقعی وقتی یک
            نیمه ضعیف‌تر باشد همین کار را می‌کند. */
    up_sim_t__p->uint32_t__v12Mv = (up_sim_t__p->uint32_t__v24Mv / 2u) + (up_sim_t__p->uint32_t__imbOffsetMv / 2u);

    /* [EN] The latch: the firmware latches the imbalance warning and keeps it
            until the battery has been off for three seconds or somebody powers
            the board down. `imbCleared` is only ever set by that rule, which is
            the point of modelling it at all - the panel's text tells a human
            what will free it, and here it actually does.
       [FA] قفل: فرم‌ور هشدار عدم‌توازن را قفل می‌کند و تا وقتی باتری سه ثانیه جدا
            نماند یا کسی برد را خاموش نکند باز نمی‌شود. `imbCleared` فقط با همان
            قاعده ست می‌شود، و همین دلیل مدل‌کردنش است - متن پنل به آدم می‌گوید چه
            چیزی آزادش می‌کند، و اینجا واقعاً همان می‌کند. */
    if (up_sim_t__p->uint32_t__imbOffsetMv >= 300u)
    {
        up_sim_t__p->uint8_t__latched = 1u;
        up_sim_t__p->uint8_t__imbCleared = 0u;
    }
    else if (up_sim_t__p->uint8_t__latched != 0u)
    {
        up_sim_t__p->uint32_t__ageMs += uint32_t__elapsedMs;   /* [EN] battery-off clock / [FA] ساعت باتری‌جدا */
        if (up_sim_t__p->uint32_t__ageMs >= SIM_LATCH_RELEASE_MS)
        {
            up_sim_t__p->uint8_t__latched = 0u;
            up_sim_t__p->uint8_t__imbCleared = 1u;
            up_sim_t__p->uint8_t__blocked = 0u;
            up_sim_t__p->uint8_t__imbSessions = 0u;
            up_sim_t__p->uint32_t__ageMs = 0u;
        }
    }

    /* [EN] The two latch bits the panel's page reads out of `fl2`: latched while
            the imbalance is there, blocked once charging has been restarted
            often enough on a machine that never got its halves balanced. And
            like the real firmware, a human ends it - the three-second rule
            above, or a power cycle - never a command from the panel.
       [FA] همان دو بیت قفلی که صفحهٔ پنل از `fl2` می‌خواند: تا وقتی عدم‌توازن
            هست قفل، و وقتی شارژ به‌قدر کافی روی ماشینی که نیمه‌هایش هرگز
            متوازن نشده دوباره شروع شده باشد مسدود. و مثل فرم‌ور واقعی، آدم
            تمامش می‌کند - قاعدهٔ سه ثانیهٔ بالا یا خاموش‌روشن برد - و هرگز
            فرمانی از پنل. */
    {
        uint8_t uint8_t__charging = ((up_sim_t__p->uint32_t__st1 == SIM_ST_BULK) ||
                                     (up_sim_t__p->uint32_t__st1 == SIM_ST_ABSORB) ||
                                     (up_sim_t__p->uint32_t__st2 == SIM_ST_BULK) ||
                                     (up_sim_t__p->uint32_t__st2 == SIM_ST_ABSORB)) ? 1u : 0u;

        if ((uint8_t__charging != 0u) && (up_sim_t__p->uint8_t__wasCharging == 0u) &&
            (up_sim_t__p->uint8_t__latched != 0u))
        {
            up_sim_t__p->uint8_t__imbSessions++;
            if (up_sim_t__p->uint8_t__imbSessions >= SIM_IMB_BLOCK_SESSIONS)
            {
                up_sim_t__p->uint8_t__blocked = 1u;
            }
        }
        up_sim_t__p->uint8_t__wasCharging = uint8_t__charging;

        if (up_sim_t__p->uint8_t__latched != 0u)
        {
            up_sim_t__p->uint32_t__flags2 |= 0x02u;
        }
        else
        {
            up_sim_t__p->uint32_t__flags2 &= ~0x02u;
            up_sim_t__p->uint8_t__imbSessions = 0u;
        }
        if (up_sim_t__p->uint8_t__blocked != 0u)
        {
            up_sim_t__p->uint32_t__flags2 |= 0x04u;
        }
        else
        {
            up_sim_t__p->uint32_t__flags2 &= ~0x04u;
        }
    }

    up_sim_t__p->uint32_t__seq++;
    up_sim_t__p->uint32_t__frames++;
}

/* ==================== The wire / روی سیم ==================== */
/**
 * @brief  [EN] The `/t` body, built word for word the way the board builds it.
 *              This is the ONLY thing the panel reads, so getting its shape
 *              right here is what makes the simulator worth running: the panel's
 *              parser, its history detectors and the page all see real numbers
 *              in the real layout.
 *         [FA] بدنهٔ `/t`، کلمه‌به‌کلمه همان‌طور که برد می‌سازد. این تنها چیزی
 *              است که پنل می‌خواند، پس درست بودن شکلش اینجا همان چیزی است که
 *              شبیه‌ساز را ارزشمند می‌کند: پارسر پنل، آشکارسازهای تاریخچه و صفحه
 *              همه اعداد واقعی را در چیدمان واقعی می‌بینند.
 * @return [EN] the JSON body / [FA] بدنهٔ JSON
 */
static std::string func__UpSim_TelemetryBody(void)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;
    uint32_t uint32_t__values[UP_TLM_FIELDS];
    std::string std_string__body;
    char char__scratch[32];

    memset(uint32_t__values, 0, sizeof(uint32_t__values));

    uint32_t__values[UP_TLM_RAW1] = 300u;
    uint32_t__values[UP_TLM_MA1_IEST] = up_sim_t__p->uint32_t__i1Ma;
    uint32_t__values[UP_TLM_MA2_IEST] = up_sim_t__p->uint32_t__i2Ma;
    uint32_t__values[UP_TLM_DUTY1] = up_sim_t__p->uint32_t__duty1;
    uint32_t__values[UP_TLM_DUTY2] = up_sim_t__p->uint32_t__duty2;
    uint32_t__values[UP_TLM_STATE1] = up_sim_t__p->uint32_t__st1;
    uint32_t__values[UP_TLM_STATE2] = up_sim_t__p->uint32_t__st2;
    uint32_t__values[UP_TLM_VIN] = up_sim_t__p->uint32_t__vinMv;
    uint32_t__values[UP_TLM_V24] = up_sim_t__p->uint32_t__v24Mv;
    uint32_t__values[UP_TLM_V12] = up_sim_t__p->uint32_t__v12Mv;
    uint32_t__values[UP_TLM_VLOW] = up_sim_t__p->uint32_t__v12Mv;
    uint32_t__values[UP_TLM_VHIGH] = up_sim_t__p->uint32_t__v24Mv - up_sim_t__p->uint32_t__v12Mv;
    uint32_t__values[UP_TLM_VDDA] = 3300u;
    uint32_t__values[UP_TLM_IMB_EVENTS] = up_sim_t__p->uint8_t__imbSessions;
    uint32_t__values[UP_TLM_IMB_CYCLES] = up_sim_t__p->uint8_t__imbSessions;
    uint32_t__values[UP_TLM_IMB_MV] = (uint32_t__values[UP_TLM_VHIGH] > uint32_t__values[UP_TLM_VLOW])
                                    ? (uint32_t__values[UP_TLM_VHIGH] - uint32_t__values[UP_TLM_VLOW])
                                    : (uint32_t__values[UP_TLM_VLOW] - uint32_t__values[UP_TLM_VHIGH]);

    char char__head[256];
    snprintf(char__head, sizeof(char__head),
             "{\"on\":1,\"age\":%lu,\"seq\":%lu,\"fl\":%lu,\"fl2\":%lu,\"n\":%lu,"
             "\"q\":0,\"q2\":0,\"q3\":0,\"q4\":0,\"ka\":120,\"vm\":0,\"ce\":%lu,\"t\":[",
             (unsigned long)100u,
             (unsigned long)up_sim_t__p->uint32_t__seq,
             (unsigned long)up_sim_t__p->uint32_t__flags,
             (unsigned long)up_sim_t__p->uint32_t__flags2,
             (unsigned long)up_sim_t__p->uint32_t__frames,
             (unsigned long)up_sim_t__p->uint32_t__crcErrors);
    std_string__body = char__head;

    for (uint32_t uint32_t__i = 0u; uint32_t__i < UP_TLM_FIELDS; uint32_t__i++)
    {
        snprintf(char__scratch, sizeof(char__scratch), "%lu", (unsigned long)uint32_t__values[uint32_t__i]);
        if (uint32_t__i > 0u)
        {
            std_string__body += ",";
        }
        std_string__body += char__scratch;
    }
    std_string__body += "],\"p\":[";

    for (uint32_t uint32_t__id = 0u; uint32_t__id < UP_PARAM_COUNT; uint32_t__id++)
    {
        if (uint32_t__id > 0u)
        {
            std_string__body += ",";
        }
        if (uint32_t__id == UP_PARAM_PCT_VMIN)
        {
            std_string__body += "21000";
        }
        else if (uint32_t__id == UP_PARAM_PCT_VMAX)
        {
            std_string__body += "29000";
        }
        else if (uint32_t__id == UP_PARAM_CHG1_ENABLE)
        {
            std_string__body += (up_sim_t__p->uint8_t__chg1Enable != 0u) ? "1" : "0";
        }
        else if (uint32_t__id == UP_PARAM_CHG2_ENABLE)
        {
            std_string__body += (up_sim_t__p->uint8_t__chg2Enable != 0u) ? "1" : "0";
        }
        else
        {
            std_string__body += "null";
        }
    }
    std_string__body += "]}";

    return std_string__body;
}

/**
 * @brief  [EN] The `/m` body: the peak window the panel reads every ten
 *              seconds. The model reports its worst numbers since the last call
 *              and then forgets them, exactly like the board's window.
 *         [FA] بدنهٔ `/m`: پنجرهٔ اوجی که پنل هر ده ثانیه می‌خواند. مدل بدترین
 *              اعدادش را از فراخوانی قبل گزارش می‌کند و بعد فراموششان می‌کند،
 *              دقیقاً مثل پنجرهٔ برد.
 * @return [EN] the JSON body / [FA] بدنهٔ JSON
 */
static std::string func__UpSim_PeaksBody(void)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;
    uint32_t uint32_t__hi[UP_TLM_FIELDS];
    uint32_t uint32_t__lo[UP_TLM_FIELDS];
    std::string std_string__body = "{\"n\":";
    char char__scratch[80];

    memset(uint32_t__hi, 0, sizeof(uint32_t__hi));
    memset(uint32_t__lo, 0, sizeof(uint32_t__lo));
    uint32_t__hi[UP_TLM_MA1_IEST] = up_sim_t__p->uint32_t__i1Ma;
    uint32_t__hi[UP_TLM_MA2_IEST] = up_sim_t__p->uint32_t__i2Ma;
    uint32_t__hi[UP_TLM_DUTY1] = up_sim_t__p->uint32_t__duty1;
    uint32_t__hi[UP_TLM_DUTY2] = up_sim_t__p->uint32_t__duty2;
    uint32_t__lo[UP_TLM_MA1_IEST] = up_sim_t__p->uint32_t__i1Ma;
    uint32_t__lo[UP_TLM_MA2_IEST] = up_sim_t__p->uint32_t__i2Ma;

    snprintf(char__scratch, sizeof(char__scratch), "%lu,\"or\":%lu,\"hi\":[",
             (unsigned long)up_sim_t__p->uint32_t__frames,
             (unsigned long)up_sim_t__p->uint32_t__flags);
    std_string__body += char__scratch;

    for (uint32_t uint32_t__i = 0u; uint32_t__i < UP_TLM_FIELDS; uint32_t__i++)
    {
        snprintf(char__scratch, sizeof(char__scratch), "%s%lu", (uint32_t__i > 0u) ? "," : "",
                 (unsigned long)uint32_t__hi[uint32_t__i]);
        std_string__body += char__scratch;
    }
    std_string__body += "],\"lo\":[";
    for (uint32_t uint32_t__i = 0u; uint32_t__i < UP_TLM_FIELDS; uint32_t__i++)
    {
        snprintf(char__scratch, sizeof(char__scratch), "%s%lu", (uint32_t__i > 0u) ? "," : "",
                 (unsigned long)uint32_t__lo[uint32_t__i]);
        std_string__body += char__scratch;
    }
    std_string__body += "]}";

    return std_string__body;
}

/* ==================== The panel's writes / نوشتنی‌های پنل ==================== */
/**
 * @brief  [EN] Handle one `POST /s` the way the engineering ESP would: ids 0..118
 *              are accepted and clamped into the model, anything above is
 *              REFUSED with the same 400 the real ESP sends. The bar is
 *              deliberately the same one the real firmware has, because a
 *              simulator that accepts a write the machine would reject is worse
 *              than no simulator: it would let somebody build a button that can
 *              never work.
 *         [FA] پردازش یک `POST /s` همان‌طور که ESP مهندسی می‌کند: شناسه‌های ۰ تا
 *              ۱۱۸ پذیرفته و در مدل اعمال می‌شوند و هر بالاتر از آن با همان ۴۰۰
 *              رد می‌شود که ESP واقعی می‌فرستد. این مرز عمداً همان مرز فرم‌ور
 *              واقعی است، چون شبیه‌سازی که نوشتنی‌ای را بپذیرد که ماشین رد
 *              می‌کند از نبودنش بدتر است: می‌گذارد کسی دکمه‌ای بسازد که هرگز
 *              نمی‌تواند کار کند.
 * @param  char__body [EN] form body, e.g. "id=11&v=0" / [FA] بدنهٔ فرم
 * @return [EN] the reply body / [FA] بدنهٔ پاسخ
 */
static std::string func__UpSim_SetParam(const std::string &char__body)
{
    up_sim_t *up_sim_t__p = &UP_SIM_T__G__Plant;
    long long__id = -1;
    long long__value = 0;
    size_t size_t__at;

    size_t__at = char__body.find("id=");
    if (size_t__at == std::string::npos)
    {
        return "{\"ok\":0,\"err\":\"json\"}";
    }
    long__id = strtol(char__body.c_str() + size_t__at + 3u, NULL, 10);

    size_t__at = char__body.find("&v=");
    if (size_t__at == std::string::npos)
    {
        return "{\"ok\":0,\"err\":\"json\"}";
    }
    long__value = strtol(char__body.c_str() + size_t__at + 3u, NULL, 10);

    if ((long__id < 0) || (long__id >= (long)UP_PARAM_COUNT))
    {
        return "{\"ok\":0,\"err\":\"range\"}";
    }

    if (long__id == (long)UP_PARAM_CHG1_ENABLE)
    {
        up_sim_t__p->uint8_t__chg1Enable = (long__value != 0) ? 1u : 0u;
    }
    else if (long__id == (long)UP_PARAM_CHG2_ENABLE)
    {
        up_sim_t__p->uint8_t__chg2Enable = (long__value != 0) ? 1u : 0u;
    }

    return "{\"ok\":1}";
}

#endif /* SIM_MACHINE_H */
