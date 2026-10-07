/* ==========================================================================
   sim_machine.h - the plant and the engineering board, as a model
   ==========================================================================

   [EN] WHAT THIS IS
        The user panel is a display for a machine it does not own: it reads the
        engineering panel's /t once a second and writes to /s when the operator
        cuts or connects a charger. Everything interesting about it therefore
        happens at the seam between the panel and the board - and this file is
        the other half of that seam, on a laptop.

        It is NOT a physics simulator. It is a model faithful enough to be
        useful before a board exists:
          - the pack charges when the input is present and a charger is enabled,
            and runs down when it is not,
          - the two chargers have their own state codes and currents,
          - the fault mask, the imbalance latch and the 20-cycle budget behave
            the way the firmware's documentation says they behave,
          - a battery that physically leaves for three seconds clears the
            imbalance counters - the ONE mechanism the real board uses to
            release that latch without a power cycle,
          - the third jitter trip lands in FINAL_FAULT, which only a board
            reset clears,
          - /s answers like the engineering panel's ESP does: it clamps to the
            id's envelope and ACKs, and it refuses an id it does not know.

   [FA] این چیست
        پنل کاربر نمایشگر ماشینی است که مالکش نیست: هر ثانیه /t پنل مهندسی را
        می‌خواند و هنگام قطع/وصل شارژر به /s می‌نویسد. پس هر چیز مهم دربارهٔ
        آن، درزِ بین پنل و برد اتفاق می‌افتد - و این فایل نیمهٔ دیگر همان درز
        است، روی یک لپ‌تاپ.

        شبیه‌ساز فیزیک نیست؛ مدلی است به‌قدر کافی وفادار تا پیش از وجود برد
        به‌کار بیاید: شارژ و دشارژ پک، حالت و جریان دو شارژر، ماسک خطا، قفل
        عدم‌توازن و بودجهٔ ۲۰ سیکل، پاک‌شدن خودکار با جدا شدن فیزیکی باتری
        (تنها راه آزادکردن قفل بدون خاموش‌روشن)، سقوط سومین جهش در
        FINAL_FAULT که فقط ری‌استارت برد پاکش می‌کند، و /s که مثل ESP پنل
        مهندسی گیره می‌زند، تأیید می‌دهد و شناسهٔ ناشناس را رد می‌کند.

   This is host code: it may use std::string freely. The no-allocation rule
   belongs to the sketch, not to the tool that pretends to be the board.
   این کد میزبان است و می‌تواند آزادانه از std::string استفاده کند؛ قانون
   «بدون تخصیص پویا» مال اسکچ است، نه ابزاری که نقش برد را بازی می‌کند.
*/
#ifndef UP_SIM_MACHINE_H
#define UP_SIM_MACHINE_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

/* ==================== Model constants / ثابت‌های مدل ==================== */
#define SIM_TICK_MS              200u     /* [EN] physics step / [FA] گام فیزیک */
#define SIM_VIN_MV               24000u    /* [EN] nominal 24 V input / [FA] ورودی اسمی */
#define SIM_SRC_MV               26400u    /* [EN] charger source / [FA] منبع شارژر */
#define SIM_FULL_MV              29200u    /* [EN] "full" threshold / [FA] آستانهٔ پر */
#define SIM_ABSORB_MV            28600u    /* [EN] absorb threshold / [FA] آستانهٔ جذبی */
#define SIM_EMPTY_MV             20000u    /* [EN] low-battery threshold / [FA] آستانهٔ باتری ضعیف */
#define SIM_LOAD_MA              400u      /* [EN] house load on the pack / [FA] مصرف داخلی */
#define SIM_CC_MA                1500u     /* [EN] bulk current / [FA] جریان شارژ اصلی */
#define SIM_ABSORB_MA            400u
#define SIM_FLOAT_MA             60u
#define SIM_PACK_MV_PER_MA_S     1440u     /* [EN] 1 mV per 1.44 A·s / [FA] ۱ mV به ازای هر ۱٫۴۴ آمپرسانیه */
#define SIM_IMB_LIMIT_MV         300u      /* [EN] the panel's warning line / [FA] خط هشدار پنل */
#define SIM_LATCH_CYCLES         20u       /* [EN] budget before charging blocks / [FA] بودجهٔ قفل */
#define SIM_ABSENT_RESET_MS      3000u     /* [EN] battery-swap qualifier / [FA] مهلت تشخیص تعویض باتری */
#define SIM_EPISODE_HOLD_MS      2000u     /* [EN] how long an imbalance must last / [FA] دوام لازم عدم‌توازن */
#define SIM_RIPPLE_MV            120u      /* [EN] measurement wobble / [FA] لرزش اندازه‌گیری */

/* ==================== The model / مدل ==================== */
typedef struct
{
    uint8_t  uint8_t__inputPresent;   /* [EN] 24 V input present / [FA] ورودی وصل */
    uint8_t  uint8_t__ch1Enabled;     /* [EN] id 11 / [FA] شناسهٔ ۱۱ */
    uint8_t  uint8_t__ch2Enabled;     /* [EN] id 12 / [FA] شناسهٔ ۱۲ */
    uint8_t  uint8_t__manualMode;     /* [EN] id 19 / [FA] شناسهٔ ۱۹ */
    uint8_t  uint8_t__batteryAbsent;  /* [EN] the pack is physically out / [FA] باتری جدا شده */
    uint8_t  uint8_t__imbLatched;     /* [EN] fl2 bit 1 / [FA] بیت ۱ fl2 */
    uint8_t  uint8_t__imbBlocked;     /* [EN] fl2 bit 2 / [FA] بیت ۲ fl2 */
    uint8_t  uint8_t__linkUp;         /* [EN] the network switch / [FA] کلید شبکه */
    uint8_t  uint8_t__faultMask;      /* [EN] low 7 bits / [FA] هفت بیت پایین */
    uint16_t uint16_t__seq;           /* [EN] frame counter / [FA] شمارندهٔ فریم */
    uint16_t uint16_t__duty1;
    uint16_t uint16_t__duty2;
    uint16_t uint16_t__i1Ma;
    uint16_t uint16_t__i2Ma;
    uint16_t uint16_t__st1;           /* [EN] UP_ST_* / [FA] کد حالت */
    uint16_t uint16_t__st2;
    uint16_t uint16_t__vinMv;
    uint16_t uint16_t__v24Mv;
    uint16_t uint16_t__v12Mv;
    uint16_t uint16_t__vlowMv;
    uint16_t uint16_t__vhighMv;
    uint16_t uint16_t__paramA[128];   /* [EN] last written params / [FA] آخرین پارامترها */
    uint8_t  uint8_t__paramKnown[128];
    uint16_t uint16_t__imbOffsetMv;   /* [EN] injected half difference / [FA] اختلاف تزریق‌شدهٔ دو نیمه */
    uint8_t  uint8_t__wasCharging;    /* [EN] previous charging state / [FA] حالت شارژ پیشین */
    uint32_t uint32_t__imbMv;
    uint32_t uint32_t__imbEvents;
    uint32_t uint32_t__imbCycles;
    uint32_t uint32_t__imbSinceMs;
    uint32_t uint32_t__absentSinceMs;
    uint32_t uint32_t__boots;
    uint32_t uint32_t__boards;        /* [EN] board resets / [FA] ری‌استارت برد */
    uint32_t uint32_t__jitterTrips;
    uint32_t uint32_t__nowMs;
    uint32_t uint32_t__chargeStartMs; /* [EN] for the charge-session demo / [FA] برای سیکل شارژ */
} up_sim_t;

static up_sim_t UP_SIM_T__G__Sim;

/* ==================== Reset / بازنشانی ==================== */
/**
 * @brief  [EN] Put the model in the state a machine has on a bench: input
 *              present, both chargers enabled by the board, a half-full pack,
 *              no faults, no latched imbalance. It is also what "reset the
 *              simulator" does, and it is deliberately NOT the same as
 *              func__UpSim_BoardReset() - that one models a power cycle on the
 *              STM32 side, this one models a fresh machine.
 *         [FA] مدل را در حالتی می‌گذارد که یک دستگاه روی میز آزمایش دارد.
 *              عمداً با func__UpSim_BoardReset() یکی نیست: آن یکی خاموش‌روشن سمت
 *              STM32 را مدل می‌کند و این یکی یک دستگاه تازه.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_Reset(void)
{
    memset(&UP_SIM_T__G__Sim, 0, sizeof(UP_SIM_T__G__Sim));

    UP_SIM_T__G__Sim.uint8_t__inputPresent = 1u;
    UP_SIM_T__G__Sim.uint8_t__ch1Enabled = 1u;
    UP_SIM_T__G__Sim.uint8_t__ch2Enabled = 1u;
    UP_SIM_T__G__Sim.uint8_t__linkUp = 1u;
    UP_SIM_T__G__Sim.uint16_t__vinMv = SIM_VIN_MV;
    UP_SIM_T__G__Sim.uint16_t__v24Mv = 24500u;
    UP_SIM_T__G__Sim.uint16_t__v12Mv = 12200u;
    UP_SIM_T__G__Sim.uint16_t__vhighMv = 12300u;
    UP_SIM_T__G__Sim.uint16_t__vlowMv = 12200u;
    UP_SIM_T__G__Sim.uint16_t__st1 = 1u;
    UP_SIM_T__G__Sim.uint16_t__st2 = 3u;
    UP_SIM_T__G__Sim.uint8_t__paramKnown[11u] = 1u;
    UP_SIM_T__G__Sim.uint16_t__paramA[11u] = 1u;
    UP_SIM_T__G__Sim.uint8_t__paramKnown[12u] = 1u;
    UP_SIM_T__G__Sim.uint16_t__paramA[12u] = 1u;
}

/* ==================== Small helpers / کمکی‌های کوچک ==================== */
/**
 * @brief  [EN] Move a value towards a target by at most one step - the whole of
 *              "physics" this model needs, written once instead of three times.
 *         [FA] حرکت یک مقدار به‌سوی هدف، حداکثر یک پله - تمام «فیزیک»ی که این
 *              مدل لازم دارد، یک‌بار نوشته‌شده و نه سه‌بار.
 */
static uint16_t func__UpSim_Approach(uint16_t uint16_t__now, uint16_t uint16_t__target, uint16_t uint16_t__step)
{
    if (uint16_t__now < uint16_t__target)
    {
        uint16_t uint16_t__next = (uint16_t)(uint16_t__now + uint16_t__step);
        return (uint16_t__next > uint16_t__target) ? uint16_t__target : uint16_t__next;
    }
    uint16_t uint16_t__next = (uint16_t)((uint16_t__now > uint16_t__step) ? (uint16_t__now - uint16_t__step) : 0u);
    return (uint16_t__next < uint16_t__target) ? uint16_t__target : uint16_t__next;
}

/* ==================== Faults / خطاها ==================== */
/**
 * @brief  [EN] Set or clear one fault bit the way the board reports it. The
 *              model keeps them independent on purpose: on the bench an
 *              injector sets exactly one bit and the panel must react to that
 *              bit alone.
 *         [FA] نشاندن یا پاک‌کردن یک بیت خطا. مدل عمداً مستقل نگهشان می‌دارد:
 *              روی میز، تزریق‌کننده دقیقاً یک بیت می‌نشاند و پنل باید به همان
 *              یک بیت واکنش بدهد.
 */
static void func__UpSim_FaultSet(uint8_t uint8_t__bit, uint8_t uint8_t__on)
{
    uint8_t uint8_t__mask = (uint8_t)(1u << uint8_t__bit);

    if (uint8_t__on != 0u) { UP_SIM_T__G__Sim.uint8_t__faultMask = (uint8_t)(UP_SIM_T__G__Sim.uint8_t__faultMask | uint8_t__mask); }
    else                   { UP_SIM_T__G__Sim.uint8_t__faultMask = (uint8_t)(UP_SIM_T__G__Sim.uint8_t__faultMask & (uint8_t)~uint8_t__mask); }
}

/**
 * @brief  [EN] One more jitter trip. The firmware's rule, modelled: the first
 *              two trips raise JITTER_1 and JITTER_2, the third puts the
 *              channel into FINAL_FAULT - and FINAL_FAULT is not released by
 *              any command, only by a board reset, which is why the panel tells
 *              the operator to power-cycle the board instead of pretending it
 *              has a "clear" button.
 *         [FA] یک بار دیگر جهش. قاعدهٔ فرم‌ور، مدل‌شده: دو بار اول JITTER_1 و
 *              JITTER_2، بار سوم کانال را به FINAL_FAULT می‌برد - و
 *              FINAL_FAULT با هیچ فرمانی آزاد نمی‌شود، فقط با ری‌استارت برد؛
 *              همین است که پنل به اپراتور می‌گوید برد را خاموش‌روشن کند و
 *              ادعا نمی‌کند دکمهٔ «پاک‌کردن» دارد.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_JitterTrip(void)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;

    up_sim_t__sim->uint32_t__jitterTrips++;
    if (up_sim_t__sim->uint32_t__jitterTrips == 1u)
    {
        func__UpSim_FaultSet(4u, 1u);   /* UP_FAULT_JITTER_1 */
    }
    else if (up_sim_t__sim->uint32_t__jitterTrips == 2u)
    {
        func__UpSim_FaultSet(5u, 1u);   /* UP_FAULT_JITTER_2 */
    }
    else
    {
        func__UpSim_FaultSet(4u, 1u);
        func__UpSim_FaultSet(5u, 1u);
        up_sim_t__sim->uint16_t__st1 = 7u;   /* UP_ST_FINAL_FAULT */
        up_sim_t__sim->uint16_t__st2 = 7u;
    }
}

/**
 * @brief  [EN] A power cycle on the board: the only thing that releases a
 *              FINAL_FAULT. Counters survive (they live in the battery's
 *              memory, not the board's), which is exactly why the panel's
 *              diagnostics page says what it says.
 *         [FA] خاموش‌روشن برد: تنها چیزی که FINAL_FAULT را آزاد می‌کند.
 *              شمارنده‌ها می‌مانند (حافظهٔ باتری‌اند نه برد)، و دقیقاً همین است
 *              که صفحهٔ دیاگ پنل همان حرف را می‌زند.
 */
static void func__UpSim_BoardReset(void)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;

    up_sim_t__sim->uint8_t__faultMask = 0u;
    up_sim_t__sim->uint16_t__st1 = 1u;
    up_sim_t__sim->uint16_t__st2 = 1u;
    up_sim_t__sim->uint32_t__jitterTrips = 0u;
    up_sim_t__sim->uint32_t__boards++;
}

/**
 * @brief  [EN] A real battery swap: the pack leaves for longer than the
 *              three-second qualifier, so the imbalance counters and the latch
 *              clear themselves - the board's own rule. The panel never sends
 *              id 202 to do this (the engineering ESP refuses ids past 118), and
 *              this function is how a simulator tells that story.
 *         [FA] تعویض واقعی باتری: پک بیشتر از مهلت سه‌ثانیه‌ای جدا می‌ماند، پس
 *              شمارنده‌های عدم‌توازن و قفل خودشان پاک می‌شوند - قاعدهٔ خود برد.
 *              پنل هرگز شناسهٔ ۲۰۲ را برای این کار نمی‌فرستد (ESP مهندسی
 *              شناسه‌های بالای ۱۱۸ را رد می‌کند) و این تابع راهی است که شبیه‌ساز
 *              این داستان را تعریف می‌کند.
 */
static void func__UpSim_BatterySwap(void)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;

    up_sim_t__sim->uint8_t__imbLatched = 0u;
    up_sim_t__sim->uint8_t__imbBlocked = 0u;
    up_sim_t__sim->uint32_t__imbMv = 0u;
    up_sim_t__sim->uint32_t__imbEvents = 0u;
    up_sim_t__sim->uint32_t__imbCycles = 0u;
    up_sim_t__sim->uint32_t__imbSinceMs = 0u;
    up_sim_t__sim->uint16_t__imbOffsetMv = 0u;
    up_sim_t__sim->uint8_t__wasCharging = 0u;
    up_sim_t__sim->uint16_t__vlowMv = up_sim_t__sim->uint16_t__v12Mv;
    up_sim_t__sim->uint16_t__vhighMv = (uint16_t)(up_sim_t__sim->uint16_t__v24Mv - up_sim_t__sim->uint16_t__v12Mv);
}

/**
 * @brief  [EN] Start an imbalance episode: make the two halves disagree by more
 *              than the panel's line and keep them there long enough to be
 *              noticed. The model does not decide what "long enough" means
 *              beyond the documented hold time - the point of the simulator is
 *              to drive the PANEL, not to be a second opinion about the pack.
 *         [FA] شروع یک رخداد عدم‌توازن: دو نیمه را بیشتر از خط پنل از هم دور
 *              کن و به‌قدر دوام مستندشده نگه دار. مدل بیش از آن زمان مستند،
 *              تصمیم نمی‌گیرد: هدف شبیه‌ساز به‌کار انداختن پنل است، نه نظر دوم
 *              دربارهٔ باتری.
 */
static void func__UpSim_ImbalanceEpisode(void)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;

    /* [EN] The offset is what the tick keeps re-applying; without it the two
       halves would be pulled back together on the very next step and the
       episode would last one tick.
       [FA] همان اختلافی است که تیک مدام دوباره اعمالش می‌کند؛ بی آن، دو نیمه
       در همان گام بعدی به هم کشیده می‌شوند و رخداد یک تیک طول می‌کشد. */
    up_sim_t__sim->uint16_t__imbOffsetMv = SIM_IMB_LIMIT_MV + 220u;
    up_sim_t__sim->uint16_t__vlowMv = up_sim_t__sim->uint16_t__v12Mv;
    up_sim_t__sim->uint16_t__vhighMv = (uint16_t)(up_sim_t__sim->uint16_t__v12Mv + up_sim_t__sim->uint16_t__imbOffsetMv);
    up_sim_t__sim->uint32_t__imbMv = SIM_IMB_LIMIT_MV + 220u;
    up_sim_t__sim->uint32_t__imbEvents++;
    up_sim_t__sim->uint32_t__imbSinceMs = 0u;
}

/* ==================== Physics / فیزیک ==================== */
/**
 * @brief  [EN] Advance the machine by one physics step and refresh everything
 *              the telemetry frame carries. Charging is blocked by a FINAL_FAULT
 *              on the channel - which is what makes the simulator useful: the
 *              panel's own screens then show a charger that will not come back
 *              until the board is power-cycled.
 *         [FA] یک گام فیزیک جلو و تازه‌سازی هر چیزی که فریم تلمتری می‌برد.
 *              شارژ با FINAL_FAULT روی کانال مسدود است - و همین شبیه‌ساز را
 *              سودمند می‌کند: صفحه‌های پنل شارژری را نشان می‌دهند که تا
 *              خاموش‌روشن‌کردن برد برنمی‌گردد.
 * @param  uint32_t__nowMs [EN] virtual milliseconds / [FA] میلی‌ثانیهٔ مجازی
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_Tick(uint32_t uint32_t__nowMs)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;
    uint32_t uint32_t__stepMs = uint32_t__nowMs - up_sim_t__sim->uint32_t__nowMs;

    if (uint32_t__stepMs == 0u) { return; }
    up_sim_t__sim->uint32_t__nowMs = uint32_t__nowMs;
    if (uint32_t__stepMs > 4u * SIM_TICK_MS) { uint32_t__stepMs = 4u * SIM_TICK_MS; }
    up_sim_t__sim->uint16_t__seq++;

    /* [EN] Input and pack voltage. */
    uint16_t uint16_t__vinTarget = (up_sim_t__sim->uint8_t__inputPresent != 0u) ? (uint16_t)SIM_VIN_MV : 0u;
    up_sim_t__sim->uint16_t__vinMv = func__UpSim_Approach(up_sim_t__sim->uint16_t__vinMv, uint16_t__vinTarget, 900u);

    uint16_t uint16_t__packRise = (uint16_t)((SIM_CC_MA * uint32_t__stepMs) / SIM_PACK_MV_PER_MA_S);
    uint16_t uint16_t__packFall = (uint16_t)((SIM_LOAD_MA * uint32_t__stepMs) / SIM_PACK_MV_PER_MA_S);

    if (up_sim_t__sim->uint8_t__batteryAbsent != 0u)
    {
        up_sim_t__sim->uint16_t__v24Mv = func__UpSim_Approach(up_sim_t__sim->uint16_t__v24Mv, 0u, 4000u);
        up_sim_t__sim->uint16_t__v12Mv = func__UpSim_Approach(up_sim_t__sim->uint16_t__v12Mv, 0u, 4000u);
        up_sim_t__sim->uint16_t__i1Ma = 0u;
        up_sim_t__sim->uint16_t__i2Ma = 0u;
        up_sim_t__sim->uint16_t__st1 = 0u;
        up_sim_t__sim->uint16_t__st2 = 0u;
        func__UpSim_FaultSet(6u, 1u);   /* UP_FAULT_BAT_LOST */
    }
    else
    {
        bool bool__can1 = (up_sim_t__sim->uint8_t__inputPresent != 0u) && (up_sim_t__sim->uint8_t__ch1Enabled != 0u) &&
                          (up_sim_t__sim->uint16_t__st1 != 7u) && (up_sim_t__sim->uint8_t__imbBlocked == 0u);
        bool bool__can2 = (up_sim_t__sim->uint8_t__inputPresent != 0u) && (up_sim_t__sim->uint8_t__ch2Enabled != 0u) &&
                          (up_sim_t__sim->uint16_t__st2 != 7u) && (up_sim_t__sim->uint8_t__imbBlocked == 0u);

        if (up_sim_t__sim->uint16_t__st1 != 7u)
        {
            if (bool__can1)
            {
                uint16_t uint16_t__next = up_sim_t__sim->uint16_t__v24Mv;
                if (uint16_t__next < SIM_ABSORB_MV)
                {
                    up_sim_t__sim->uint16_t__st1 = 1u;
                    up_sim_t__sim->uint16_t__i1Ma = SIM_CC_MA;
                    uint16_t__next = func__UpSim_Approach(uint16_t__next, (uint16_t)(SIM_FULL_MV + 400u), uint16_t__packRise);
                }
                else if (uint16_t__next < SIM_FULL_MV)
                {
                    up_sim_t__sim->uint16_t__st1 = 2u;
                    up_sim_t__sim->uint16_t__i1Ma = SIM_ABSORB_MA;
                    uint16_t__next = func__UpSim_Approach(uint16_t__next, (uint16_t)(SIM_FULL_MV + 100u), uint16_t__packRise);
                }
                else
                {
                    up_sim_t__sim->uint16_t__st1 = 3u;
                    up_sim_t__sim->uint16_t__i1Ma = SIM_FLOAT_MA;
                }
                up_sim_t__sim->uint16_t__v24Mv = uint16_t__next;
                up_sim_t__sim->uint16_t__duty1 = (uint16_t)(700u + (up_sim_t__sim->uint16_t__i1Ma / 10u));
            }
            else
            {
                up_sim_t__sim->uint16_t__st1 = 0u;
                up_sim_t__sim->uint16_t__i1Ma = 0u;
                up_sim_t__sim->uint16_t__duty1 = 0u;
            }
        }

        /* [EN] The lower half is the same string of cells, measured at its
           mid-point: it sits near half the pack and moves WITH it. A model that
           let the pack climb to 29 V while the lower half stayed at 13.9 V would
           report a two-volt imbalance on a perfectly healthy machine - and a
           simulator that cries wolf is worse than no simulator.
           [FA] نیمهٔ پایینی همان رشتهٔ سلول‌هاست که در نقطهٔ میانی اندازه‌گیری
           می‌شود: نزدیک نصف پک می‌نشیند و «با» آن حرکت می‌کند. مدلی که بگذارد پک
           به ۲۹ ولت برسد و نیمهٔ پایینی روی ۱۳٫۹ بماند، روی دستگاهی کاملاً سالم
           عدم‌توازن دوولتی گزارش می‌کند - و شبیه‌سازی که بی‌دلیل هشدار بدهد از
           نبودنش بدتر است. */
        uint16_t uint16_t__lowerTarget = (uint16_t)((up_sim_t__sim->uint16_t__v24Mv / 2u) + (up_sim_t__sim->uint16_t__imbOffsetMv / 2u));
        up_sim_t__sim->uint16_t__v12Mv = func__UpSim_Approach(up_sim_t__sim->uint16_t__v12Mv, uint16_t__lowerTarget,
                                                             (uint16_t)((uint16_t__packRise / 2u) + 1u));

        if (up_sim_t__sim->uint16_t__st2 != 7u)
        {
            up_sim_t__sim->uint16_t__st2 = (bool__can2) ? (uint16_t)((up_sim_t__sim->uint16_t__v12Mv < (uint16_t)(SIM_FULL_MV / 2u)) ? 1u : 3u) : 0u;
            up_sim_t__sim->uint16_t__i2Ma = (bool__can2 && (up_sim_t__sim->uint16_t__st2 == 1u)) ? (uint16_t)(SIM_CC_MA / 2u) : 0u;
            up_sim_t__sim->uint16_t__duty2 = (uint16_t)((up_sim_t__sim->uint16_t__i2Ma != 0u) ? 660u : 0u);
        }

        if ((bool__can1 == false) && (bool__can2 == false))
        {
            up_sim_t__sim->uint16_t__v24Mv = func__UpSim_Approach(up_sim_t__sim->uint16_t__v24Mv, 0u, (uint16_t)(uint16_t__packFall * 2u));
        }
        func__UpSim_FaultSet(6u, 0u);
        if (up_sim_t__sim->uint16_t__v24Mv < SIM_EMPTY_MV) { func__UpSim_FaultSet(3u, 1u); }   /* LOW_BATTERY */
    }

    /* [EN] Imbalance: the two halves, the episode and the 20-cycle budget.
            Nothing here is clever - it is the documented behaviour, written
            down where a developer can watch it happen in ten seconds. */
    up_sim_t__sim->uint16_t__vlowMv = up_sim_t__sim->uint16_t__v12Mv;
    if (up_sim_t__sim->uint16_t__imbOffsetMv != 0u)
    {
        up_sim_t__sim->uint16_t__vhighMv = (uint16_t)(up_sim_t__sim->uint16_t__v12Mv + up_sim_t__sim->uint16_t__imbOffsetMv);
    }
    else
    {
        up_sim_t__sim->uint16_t__vhighMv = (uint16_t)(up_sim_t__sim->uint16_t__v24Mv - up_sim_t__sim->uint16_t__v12Mv);
    }

    uint16_t uint16_t__half = up_sim_t__sim->uint16_t__vhighMv;
    uint16_t uint16_t__other = up_sim_t__sim->uint16_t__vlowMv;
    up_sim_t__sim->uint32_t__imbMv = (uint16_t__half > uint16_t__other) ? (uint16_t__half - uint16_t__other) : (uint16_t__other - uint16_t__half);

    if (up_sim_t__sim->uint32_t__imbMv > SIM_IMB_LIMIT_MV)
    {
        if (up_sim_t__sim->uint32_t__imbSinceMs == 0u) { up_sim_t__sim->uint32_t__imbSinceMs = uint32_t__nowMs; }
        if ((uint32_t__nowMs - up_sim_t__sim->uint32_t__imbSinceMs) >= SIM_EPISODE_HOLD_MS)
        {
            up_sim_t__sim->uint8_t__imbLatched = 1u;
        }
    }
    else if (up_sim_t__sim->uint32_t__imbSinceMs != 0u)
    {
        up_sim_t__sim->uint32_t__imbSinceMs = 0u;
        up_sim_t__sim->uint16_t__imbOffsetMv = 0u;
    }

    /* [EN] A "cycle" is a charge SESSION, not a tick: the board counts one more
       spend of the twenty-cycle budget when charging starts again while the
       latch is set. Counting ticks would reach twenty in four seconds and make
       the whole budget meaningless.
       [FA] «سیکل» یک «سیکل شارژ» است و نه یک تیک: برد با شروع دوبارهٔ شارژ در
       حالتی که قفل نشسته باشد، یک خرج دیگر از بودجهٔ بیست‌سیکلی می‌شمارد.
       شمردن تیک‌ها در چهار ثانیه به بیست می‌رسید و کل بودجه را بی‌معنا می‌کرد. */
    uint8_t uint8_t__charging = (((up_sim_t__sim->uint16_t__st1 == 1u) || (up_sim_t__sim->uint16_t__st1 == 2u) ||
                                  (up_sim_t__sim->uint16_t__st2 == 1u) || (up_sim_t__sim->uint16_t__st2 == 2u)) ? 1u : 0u);
    if ((uint8_t__charging != 0u) && (up_sim_t__sim->uint8_t__wasCharging == 0u))
    {
        if ((up_sim_t__sim->uint8_t__imbLatched != 0u) && (up_sim_t__sim->uint32_t__imbCycles < SIM_LATCH_CYCLES))
        {
            up_sim_t__sim->uint32_t__imbCycles++;
            if (up_sim_t__sim->uint32_t__imbCycles >= SIM_LATCH_CYCLES) { up_sim_t__sim->uint8_t__imbBlocked = 1u; }
        }
    }
    up_sim_t__sim->uint8_t__wasCharging = uint8_t__charging;
}

/* ==================== /t and /m together / و /t و /m با هم ==================== */
/**
 * @brief  [EN] One body that answers BOTH jobs. The panel asks for /t once a
 *              second and /m occasionally, and a simulator that has to guess
 *              which request is in flight would be a simulator with a bug in
 *              it; carrying the telemetry array and the peak arrays in one
 *              document sidesteps the whole question. The panel looks keys up
 *              by name, so extra keys cost nothing but bytes.
 *         [FA] یک بدنه که به «هر دو» کار جواب می‌دهد. پنل هر ثانیه /t و گاهی
 *              /m می‌خواهد و شبیه‌سازی که باید حدس بزند کدام درخواست در راه
 *              است، خودش یک باگ دارد؛ آوردن آرایهٔ تلمتری و آرایه‌های پنجره در
 *              یک سند، کل مسئله را دور می‌زند. پنل کلیدها را با نام پیدا می‌کند،
 *              پس کلید اضافه فقط بایت است.
 * @param  std_string__out [EN] destination body / [FA] بدنهٔ مقصد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSim_Body(std::string &std_string__out)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;
    uint32_t uint32_t__t[32];
    char char__number[24];
    uint8_t uint8_t__flags = 0u;
    uint8_t uint8_t__flags2 = 0u;

    memset(uint32_t__t, 0, sizeof(uint32_t__t));

    /* [EN] The flag byte the page reads: snapshot valid, input present,
            measurements valid, channel-1 and channel-2 charging, manual mode.
       [FA] بایت فلگی که صفحه می‌خواند. */
    if (up_sim_t__sim->uint16_t__st1 != 0u) { uint8_t__flags |= 0x08u; }
    if (up_sim_t__sim->uint16_t__st2 != 0u) { uint8_t__flags |= 0x10u; }
    if (up_sim_t__sim->uint8_t__manualMode != 0u) { uint8_t__flags |= 0x20u; }
    uint8_t__flags |= 0x01u;                                       /* snapshot */
    uint8_t__flags |= 0x04u;                                       /* measurements valid */
    if ((up_sim_t__sim->uint8_t__inputPresent != 0u) && (up_sim_t__sim->uint16_t__vinMv > 18000u)) { uint8_t__flags |= 0x02u; }

    if (up_sim_t__sim->uint8_t__imbLatched != 0u) { uint8_t__flags2 |= 0x02u; }
    if (up_sim_t__sim->uint8_t__imbBlocked != 0u) { uint8_t__flags2 |= 0x04u; }

    uint32_t__t[0] = 310u;                                  /* UP_TLM_RAW1 */
    uint32_t__t[4] = up_sim_t__sim->uint16_t__i1Ma;          /* UP_TLM_MA1_IEST */
    uint32_t__t[5] = up_sim_t__sim->uint16_t__duty1;
    uint32_t__t[6] = up_sim_t__sim->uint16_t__st1;
    uint32_t__t[11] = up_sim_t__sim->uint16_t__i2Ma;         /* UP_TLM_MA2_IEST = 11 */
    uint32_t__t[12] = up_sim_t__sim->uint16_t__duty2;
    uint32_t__t[13] = up_sim_t__sim->uint16_t__st2;
    uint32_t__t[14] = up_sim_t__sim->uint16_t__vinMv;
    uint32_t__t[15] = up_sim_t__sim->uint16_t__v24Mv;
    uint32_t__t[16] = up_sim_t__sim->uint16_t__v12Mv;
    uint32_t__t[17] = up_sim_t__sim->uint16_t__vlowMv;
    uint32_t__t[18] = up_sim_t__sim->uint16_t__vhighMv;
    uint32_t__t[19] = up_sim_t__sim->uint8_t__faultMask;
    uint32_t__t[23] = 3300u;                                 /* UP_TLM_VDDA */
    uint32_t__t[24] = up_sim_t__sim->uint32_t__imbMv;
    uint32_t__t[25] = up_sim_t__sim->uint32_t__imbEvents;
    uint32_t__t[26] = up_sim_t__sim->uint32_t__imbCycles;

    std_string__out = "{\"on\":1,\"age\":40,\"seq\":" + std::to_string(up_sim_t__sim->uint16_t__seq) +
                       ",\"fl\":" + std::to_string((unsigned)uint8_t__flags) +
                       ",\"fl2\":" + std::to_string((unsigned)uint8_t__flags2) +
                       ",\"n\":1234,\"q\":0,\"q2\":0,\"q3\":0,\"q4\":0,\"ka\":120,\"vm\":0,\"ce\":0,\"or\":" +
                       std::to_string((unsigned)up_sim_t__sim->uint8_t__faultMask) + ",\"t\":[";

    for (uint32_t i = 0u; i < 28u; i++)
    {
        snprintf(char__number, sizeof(char__number), "%lu", (unsigned long)uint32_t__t[i]);
        if (i != 0u) { std_string__out += ","; }
        std_string__out += char__number;
    }
    std_string__out += "],\"lo\":[";
    for (uint32_t i = 0u; i < 32u; i++)
    {
        snprintf(char__number, sizeof(char__number), "%lu", (unsigned long)((i < 28u) ? uint32_t__t[i] : 0u));
        if (i != 0u) { std_string__out += ","; }
        std_string__out += char__number;
    }
    std_string__out += "],\"hi\":[";
    for (uint32_t i = 0u; i < 32u; i++)
    {
        snprintf(char__number, sizeof(char__number), "%lu", (unsigned long)((i < 28u) ? uint32_t__t[i] : 0u));
        if (i != 0u) { std_string__out += ","; }
        std_string__out += char__number;
    }
    std_string__out += "],\"p\":[";
    for (uint32_t id = 0u; id < 128u; id++)
    {
        if (id != 0u) { std_string__out += ","; }
        if (up_sim_t__sim->uint8_t__paramKnown[id] != 0u)
        {
            std_string__out += std::to_string((unsigned)up_sim_t__sim->uint16_t__paramA[id]);
        }
        else
        {
            std_string__out += "null";
        }
    }
    std_string__out += "]}";
}

/* ==================== /s / نوشتن ==================== */
/**
 * @brief  [EN] The engineering panel's write route, with its real behaviour:
 *              an id outside 0..118 is refused with a 400, an id inside is
 *              clamped to that id's envelope and ACKed. Modelling the refusal
 *              matters more than it looks: it is the reason the panel has no
 *              "clear the imbalance latch" button to offer.
 *         [FA] مسیر نوشتن پنل مهندسی، با همان رفتار واقعی: شناسهٔ بیرون از
 *              ۰..۱۱۸ با ۴۰۰ رد می‌شود و شناسهٔ داخل، به پاکت خودش گیره
 *              می‌خورد و تأیید می‌شود. مدل‌کردن همان رد شدن مهم‌تر از ظاهرش
 *              است: همین دلیلِ آن است که پنل دکمهٔ «پاک‌کردن قفل عدم‌توازن»
 *              ندارد.
 * @param  char__id [EN] id text / [FA] متن شناسه
 * @param  char__value [EN] value text / [FA] متن مقدار
 * @param  int__codeOut [EN] HTTP code to answer / [FA] کد پاسخ
 * @param  std_string__bodyOut [EN] body to answer / [FA] بدنهٔ پاسخ
 * @return [EN] true when the write was accepted / [FA] در صورت پذیرش true
 */
static bool func__UpSim_SetParam(const char *char__id, const char *char__value, int *int__codeOut, std::string &std_string__bodyOut)
{
    long long__id = strtol(char__id, NULL, 10);
    long long__value = strtol(char__value, NULL, 10);

    if ((long__id < 0L) || (long__id >= 119L))
    {
        *int__codeOut = 400;
        std_string__bodyOut = "{\"ok\":0,\"err\":\"unknown param\"}";
        return false;
    }

    /* [EN] The envelopes the panel can actually use. Everything else is stored
            as it arrives, because no part of the panel writes it today. */
    long long__min = 0L;
    long long__max = 65535L;
    if (long__id == 11L || long__id == 12L || long__id == 19L) { long__max = 1L; }
    if (long__id == 13L || long__id == 14L) { long__max = 500L; }

    long long__clamped = (long__value < long__min) ? long__min : ((long__value > long__max) ? long__max : long__value);

    UP_SIM_T__G__Sim.uint8_t__paramKnown[(uint32_t)long__id] = 1u;
    UP_SIM_T__G__Sim.uint16_t__paramA[(uint32_t)long__id] = (uint16_t)long__clamped;

    if (long__id == 11L) { UP_SIM_T__G__Sim.uint8_t__ch1Enabled = (uint8_t)long__clamped; }
    if (long__id == 12L) { UP_SIM_T__G__Sim.uint8_t__ch2Enabled = (uint8_t)long__clamped; }
    if (long__id == 19L) { UP_SIM_T__G__Sim.uint8_t__manualMode = (uint8_t)long__clamped; }

    *int__codeOut = 200;
    std_string__bodyOut = "{\"ok\":1}";
    return true;
}

/* ==================== State as JSON / حالت به‌صورت JSON ==================== */
/**
 * @brief  [EN] The model as one JSON object, for the control page: everything a
 *              bench operator would otherwise read off a multimeter.
 *         [FA] مدل به‌صورت یک شیء JSON برای صفحهٔ کنترل: هر چیزی که اپراتور
 *              میز آزمایش وگرنه از مولتی‌متر می‌خواند.
 */
static std::string func__UpSim_StateJson(void)
{
    up_sim_t *up_sim_t__sim = &UP_SIM_T__G__Sim;
    char char__text[1024];

    snprintf(char__text, sizeof(char__text),
             "{\"vin\":%u,\"v24\":%u,\"v12\":%u,\"vhigh\":%u,\"vlow\":%u,"
             "\"i1\":%u,\"i2\":%u,\"duty1\":%u,\"duty2\":%u,\"st1\":%u,\"st2\":%u,"
             "\"fl\":%u,\"imb\":%lu,\"imbEvents\":%lu,\"imbCycles\":%lu,\"latched\":%u,\"blocked\":%u,"
             "\"input\":%u,\"ch1\":%u,\"ch2\":%u,\"manual\":%u,\"absent\":%u,\"link\":%u,"
             "\"seq\":%u,\"boots\":%lu,\"boards\":%lu,\"trips\":%lu,\"nowMs\":%lu,\"batteryAbsent\":%u}",
             (unsigned)up_sim_t__sim->uint16_t__vinMv, (unsigned)up_sim_t__sim->uint16_t__v24Mv,
             (unsigned)up_sim_t__sim->uint16_t__v12Mv, (unsigned)up_sim_t__sim->uint16_t__vhighMv,
             (unsigned)up_sim_t__sim->uint16_t__vlowMv, (unsigned)up_sim_t__sim->uint16_t__i1Ma,
             (unsigned)up_sim_t__sim->uint16_t__i2Ma, (unsigned)up_sim_t__sim->uint16_t__duty1,
             (unsigned)up_sim_t__sim->uint16_t__duty2, (unsigned)up_sim_t__sim->uint16_t__st1,
             (unsigned)up_sim_t__sim->uint16_t__st2, (unsigned)up_sim_t__sim->uint8_t__faultMask,
             (unsigned long)up_sim_t__sim->uint32_t__imbMv, (unsigned long)up_sim_t__sim->uint32_t__imbEvents,
             (unsigned long)up_sim_t__sim->uint32_t__imbCycles, (unsigned)up_sim_t__sim->uint8_t__imbLatched,
             (unsigned)up_sim_t__sim->uint8_t__imbBlocked, (unsigned)up_sim_t__sim->uint8_t__inputPresent,
             (unsigned)up_sim_t__sim->uint8_t__ch1Enabled, (unsigned)up_sim_t__sim->uint8_t__ch2Enabled,
             (unsigned)up_sim_t__sim->uint8_t__manualMode, (unsigned)up_sim_t__sim->uint8_t__batteryAbsent,
             (unsigned)up_sim_t__sim->uint8_t__linkUp, (unsigned)up_sim_t__sim->uint16_t__seq,
             (unsigned long)up_sim_t__sim->uint32_t__boots, (unsigned long)up_sim_t__sim->uint32_t__boards,
             (unsigned long)up_sim_t__sim->uint32_t__jitterTrips, (unsigned long)up_sim_t__sim->uint32_t__nowMs,
             (unsigned)up_sim_t__sim->uint8_t__batteryAbsent);

    return std::string(char__text);
}

#endif /* UP_SIM_MACHINE_H */
