/**
 * @file    host_test_changeover.c
 * @brief   [EN] Host unit test for the Changeover module. The PRODUCTION
 *              changeover.c is compiled as-is and driven through a fake
 *              clock and fake collaborators (BSP GPIO, imbalance veto,
 *              dead-battery veto). No board, no RTOS, no HAL.
 *          [FA] تست هاست ماژول Changeover: همان فایل محصول کامپایل می‌شود و
 *              با ساعت مصنوعی و همکارهای بدلی (GPIO برد، وتوی عدم‌توازن،
 *              وتوی باتری خراب) رانده می‌شود. بدون برد، بدون RTOS و HAL.
 *
 * @note    [EN] The harness only provides the symbols changeover.c needs to
 *              link; it never re-implements any decision the module makes.
 *          [FA] هارنس فقط نمادهای لازم برای لینک را می‌دهد و هیچ تصمیمی از
 *              تصمیم‌های ماژول را بازنویسی نمی‌کند.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "changeover.h"
#include "bsp_gpio.h"
#include "modules_enable.h"

#if MODULE_IMBALANCE
#include "imbalance.h"
#endif

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake world / دنیای بدلی ==================== */

/* [EN] Fake kernel clock in ticks; the test moves it by hand.
   [FA] ساعت بدلی کرنل بر حسب تیک؛ تست خودش آن را جلو می‌برد. */
static uint32_t UINT32_T__G__FakeTick = 0u;

/* [EN] Last value written to the battery-protect line, and a write counter so
       a test can prove the module does NOT re-drive an already-driven pin.
   [FA] آخرین مقدار نوشته‌شده روی خط محافظ باتری و شمارندهٔ نوشتن، تا تست
       ثابت کند ماژول پایهٔ از قبل رانده‌شده را دوباره نمی‌راند. */
static bool     BOOL__G__ProtectLine  = false;
static uint32_t UINT32_T__G__ProtectWrites = 0u;

/* [EN] Collaborator verdicts the test drives. / [FA] نظر همکارهای بدلی. */
static bool BOOL__G__ImbalanceBlocks = false;
static bool BOOL__G__DeadBlocks      = false;

uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickCount(void)
{
    return UINT32_T__G__FakeTick;
}

void func__BspGpio_Write(bsp_gpio_id_t bsp_gpio_id_t__id, bool bool__asserted)
{
    /* [EN] Only the battery-protect line may be touched by this module.
       [FA] این ماژول تنها اجازهٔ دست‌زدن به خط محافظ باتری را دارد. */
    CHECK(bsp_gpio_id_t__id == BSP_GPIO_PROTECT_BATTERY);
    BOOL__G__ProtectLine = bool__asserted;
    UINT32_T__G__ProtectWrites++;
}

/* [EN] 1 kHz kernel: one tick is one millisecond, like the real config.
   [FA] کرنل یک‌کیلوهرتز: هر تیک یک میلی‌ثانیه، مثل پیکربندی واقعی. */
uint32_t func__Rtos_MillisecondsToTicks(uint32_t uint32_t__milliseconds)
{
    return uint32_t__milliseconds;
}

#if MODULE_IMBALANCE
void func__Imbalance_GetOutputs(imbalance_outputs_t *imbalance_outputs_t__outputs)
{
    if (imbalance_outputs_t__outputs != NULL)
    {
        imbalance_outputs_t__outputs->uint32_t__imbalanceMv   = 0u;
        imbalance_outputs_t__outputs->bool__episode           = false;
        imbalance_outputs_t__outputs->uint32_t__events        = 0u;
        imbalance_outputs_t__outputs->bool__latched           = BOOL__G__ImbalanceBlocks;
        imbalance_outputs_t__outputs->uint32_t__latchedCycles = 0u;
        imbalance_outputs_t__outputs->bool__blockOutput       = BOOL__G__ImbalanceBlocks;
        imbalance_outputs_t__outputs->bool__chargingAllowed   = true;
        imbalance_outputs_t__outputs->bool__beepDue           = false;
        imbalance_outputs_t__outputs->bool__halvesMismatch    = false;
    }
}
#endif

#if MODULE_CHARGER
bool func__Charger_DeadBlocksOutput(void);
bool func__Charger_DeadBlocksOutput(void)
{
    return BOOL__G__DeadBlocks;
}
#endif

/* ==================== Test driver / رانندهٔ تست ==================== */

static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

/**
 * @brief  [EN] Advance the fake clock and run one evaluation pass.
 *         [FA] ساعت بدلی را جلو می‌برد و یک گام ارزیابی اجرا می‌کند.
 */
static app_state_t func__Step(uint32_t uint32_t__advanceMs, fault_mask_t fault_mask_t__faults)
{
    UINT32_T__G__FakeTick += uint32_t__advanceMs;
    return func__Changeover_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap, fault_mask_t__faults);
}

/**
 * @brief  [EN] Fresh module + fresh fake world, battery healthy on mains.
 *         [FA] ماژول و دنیای بدلی نو؛ باتری سالم با برق شهر.
 */
static void func__Reset(uint32_t uint32_t__startTick)
{
    UINT32_T__G__FakeTick       = uint32_t__startTick;
    BOOL__G__ImbalanceBlocks    = false;
    BOOL__G__DeadBlocks         = false;

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv       = 230000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv    = 25000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv    = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv  = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid         = true;

    /* [EN] Simulate the safe boot pin, then the first healthy sample that
       permits connection. Clear the write counter so each test measures only
       its own transitions. [FA] بوت امن و سپس نمونهٔ سالمِ مجوز وصل را شبیه‌سازی کن. */
    func__Changeover_Init();
    BOOL__G__ProtectLine = true;
    (void)func__Changeover_Evaluate(&MEASUREMENT_SNAPSHOT_T__G__Snap, FAULT_NONE);
    UINT32_T__G__ProtectWrites = 0u;
}

/**
 * @brief  [EN] Drive the pack low enough to cut and hold it for the full
 *              3 s filter, then assert the cut happened exactly on time.
 *         [FA] باتری را تا حد قطع پایین می‌برد و کل فیلتر ۳ ثانیه نگه می‌دارد،
 *              سپس بررسی می‌کند قطع دقیقاً سر وقت رخ داده است.
 */
static void func__CutAndVerifyTiming(void)
{
    (void)func__Step(10u, FAULT_NONE);          /* [EN] arms the cut timer / مسلح‌کردن تایمر */
    CHECK(BOOL__G__ProtectLine == false);

    (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);       /* [EN] one ms short / یک میلی‌ثانیه کم */

    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
}

int main(void)
{
    app_state_t app_state_t__state;
    uint32_t    uint32_t__i;

    printf("== Changeover host test ==\n");

    /* ---- 1. boot remains protected until a valid, healthy sample ----
       [EN] Invalid data, a blocking fault, or voltage below the existing
            21.2 V hysteresis-clear threshold must not connect the output.
       [FA] دادهٔ نامعتبر، خطای مسدودکننده یا ولتاژ زیر حد ۲۱٫۲ ولت نباید
            خروجی را وصل کند. */
    func__Reset(0u);
    func__Changeover_Init();
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    UINT32_T__G__ProtectWrites = 0u;

    CHECK(func__Changeover_Evaluate(NULL, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_RECONNECT_MV - 1u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    CHECK(func__Step(10u, FAULT_ADC) == APP_STATE_FAULT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_RECONNECT_MV;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 1u);

    /* [EN] A healthy no-mains sample connects immediately after validation.
       [FA] نمونهٔ سالم بدون ورودی پس از اعتبارسنجی مسیر را وصل می‌کند. */
    func__Reset(100u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;

    /* ---- 2. plain mapping from input presence ---- */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    /* ---- 3. any fault wins, and it never moves the pin ---- */
    CHECK(func__Step(10u, FAULT_ADC) == APP_STATE_FAULT);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);

    /* ---- 3b. charger-only faults must NOT disable battery cut or BATTERY state ---- */
    func__Reset(500u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    CHECK(func__Step(10u, FAULT_CHARGER_TECHNICAL) == APP_STATE_BATTERY);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_CHARGER_TECHNICAL | FAULT_CHARGER_BAT_LOST);
    CHECK(BOOL__G__ProtectLine == false);
    (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_CHARGER_TECHNICAL | FAULT_CHARGER_BAT_LOST);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(func__Step(1u, FAULT_CHARGER_TECHNICAL | FAULT_CHARGER_BAT_LOST) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);

    /* ---- 4. critical cut: below 20800 mV, 3 s filter ---- */
    func__Reset(1000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    CHECK(UINT32_T__G__ProtectWrites == 1u);

    /* [EN] Once cut, further passes keep SAFE and do NOT re-drive the pin.
       [FA] پس از قطع، گام‌های بعدی SAFE می‌مانند و پایه دوباره رانده نمی‌شود. */
    for (uint32_t__i = 0u; uint32_t__i < 10u; uint32_t__i++)
    {
        CHECK(func__Step(100u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(UINT32_T__G__ProtectWrites == 1u);

    /* ---- 5. v1.81 regression: the low-battery LATCH alone drives the cut ----
       [EN] The pack dips under 21000 mV (latch closes), then recovers to
            21100 mV - inside the 21000..21200 hysteresis band - in the middle
            of the 3 s window. Before v1.81 that froze the countdown and the
            weak pack kept feeding the load. The latch only opens at 21200 mV,
            so the cut must still land exactly 3 s after the window opened.
       [FA] باتری زیر ۲۱۰۰۰ می‌رود (قفل بسته می‌شود) و وسط پنجرهٔ ۳ ثانیه به
            ۲۱۱۰۰ - داخل نوار هیسترزیس - برمی‌گردد. پیش از ۱٫۸۱ این کار شمارش
            را می‌خواباند. قفل فقط در ۲۱۲۰۰ باز می‌شود، پس قطع باید سر همان
            ۳ ثانیه بیفتد. */
    func__Reset(50000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);                       /* [EN] arm / مسلح */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV + 100u;
    (void)func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);

    /* [EN] Mirror case: a real recovery to 21200 mV opens the latch and the
            countdown is abandoned - no cut, ever.
       [FA] حالت آینه: بهبود واقعی تا ۲۱۲۰۰ قفل را باز می‌کند و شمارش رها
            می‌شود؛ هیچ قطعی رخ نمی‌دهد. */
    func__Reset(90000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CLEAR_MV;
    for (uint32_t__i = 0u; uint32_t__i < 20u; uint32_t__i++)
    {
        CHECK(func__Step(500u, FAULT_NONE) == APP_STATE_BATTERY);
    }
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    /* ---- ‎6. reconnect with mains: 30 s stable input + 21200 mV + 3 s filter ----
       [EN] With the mains present the original user decisions (ج-۲ and
            2026-10-12) still govern: the input must be stable for 30 s and the
            pack must hold 21200 mV for the whole 3 s filter.
       [FA] با حضور برق شهر همان تصمیم‌های قبلی (ج-۲ و ۲۰۲۶-۱۰-۱۲) حاکم است:
            ورودی باید ۳۰ ثانیه پایدار باشد و باتری ۳ ثانیه در ۲۱۲۰۰ بماند. */
    func__Reset(200000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;    /* [EN] cut happens with no mains */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();

    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;     /* [EN] mains returns after the cut */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;      /* [EN] fully recovered */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] input stable starts */
    CHECK(func__Step(CHANGEOVER_INPUT_STABLE_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] 30 s done, 3 s filter starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 2u);

    /* ---- 6a. no mains, recovered pack: 10 s unbroken healthy + 3 s filter ----
       [EN] User decision 2026-10-23: the pack that recovers after the cut (or is
            replaced) must come back on its own, but only after the healthy
            voltage has held WITHOUT A BREAK for CHANGEOVER_DEF_HEALTHY_HOLD_MS.
            One dip inside the window restarts it; the state is BATTERY because
            the mains really is absent.
       [FA] تصمیم کاربر ۲۰۲۶-۱۰-۲۳: باتری‌ای که بعد از قطع بازیابی (یا تعویض)
            می‌شود باید خودش برگردد، اما فقط بعد از اینکه ولتاژ سالم بدون هیچ
            وقفه‌ای به‌مدت پنجرهٔ نگهداری دوام آورد. یک افت وسط پنجره آن را از
            صفر شروع می‌کند؛ حالت BATTERY است چون برق شهر واقعاً نیست. */
    func__Reset(250000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] healthy window opens */
    CHECK(func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS - 2000u, FAULT_NONE) == APP_STATE_SAFE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 21000u;      /* [EN] one dip -> restart */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);                      /* [EN] window not finished / پنجره تمام نشده */
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);      /* [EN] hold done, 3 s filter starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_BATTERY);   /* [EN] back on battery, no mains */
    CHECK(BOOL__G__ProtectLine == false);

    /* ---- 6b. the 30 s stable-input clock must keep running DURING a cut ----
       [EN] Regression for the 2026-10-20 audit: the cut branch returns early,
            so a stamp that was only written below it froze at its pre-cut
            value. If the mains dropped and came back while the cut condition
            was still true, the reconnect check measured that stale stamp and
            the 30 s order of 2026-10-12 was skipped. Here the input is present
            for 40 s, then the pack is cut, the input is lost AND restored
            while the cut still holds, and only then does the pack recover:
            the reconnect must wait a FULL 30 s counted from the restore.
       [FA] رگرسیون ممیزی ۲۰۲۶-۱۰-۲۰: شاخهٔ قطع زودتر return می‌کرد، پس مهری
            که فقط پایین‌تر از آن نوشته می‌شد روی مقدار پیش از قطع می‌خوابید.
            اگر برق ورودی در طول قطع می‌رفت و برمی‌گشت، بررسی وصل مجدد همان مهر
            کهنه را می‌سنجید و تاخیر ۳۰ ثانیه دور زده می‌شد. اینجا ورودی ۴۰
            ثانیه حاضر است، بعد قطع رخ می‌دهد، ورودی در طول قطع می‌رود و
            برمی‌گردد و بعد باتری بهبود می‌یابد: وصل مجدد باید کامل ۳۰ ثانیه از
            لحظهٔ برگشت ورودی صبر کند. */
    func__Reset(700000u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(func__Step(40000u, FAULT_NONE) == APP_STATE_INPUT);   /* [EN] stamp is now 40 s old */

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_LOW_ALARM_CUT_MV - 100u;
    (void)func__Step(10u, FAULT_NONE);
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);

    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;      /* [EN] mains lost mid-cut */
    (void)func__Step(10u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;       /* [EN] restored mid-cut */
    (void)func__Step(10u, FAULT_NONE);

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 22000u;        /* [EN] cut clears */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);

    /* [EN] 12 s after the restore: the old code would already have reconnected
            (stale stamp 40 s old + the 3 s filter); the 30 s clock must hold.
       [FA] ۱۲ ثانیه بعد از برگشت ورودی: کد قدیم همان‌جا وصل می‌کرد؛ ساعت ۳۰
            ثانیه باید نگه دارد. */
    for (uint32_t__i = 0u; uint32_t__i < 4u; uint32_t__i++)
    {
        CHECK(func__Step(3000u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(BOOL__G__ProtectLine == true);

    (void)func__Step(CHANGEOVER_INPUT_STABLE_MS - 12010u - 1u, FAULT_NONE); /* [EN] just short */
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);        /* [EN] 30 s done, 3 s starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);

    /* ---- 6c. cycle guard: a really weak pack must not loop forever ----
       [EN] User decision 2026-10-23: the recovered pack may reconnect with no
            mains, but if it cuts again inside CHANGEOVER_DEF_CYCLE_GUARD_MS the
            pack is really weak and auto-reconnect without mains latches off
            until the mains returns. Here: mains-free reconnect, a quick
            re-cut, then 40 s of healthy voltage - it must stay SAFE.
       [FA] تصمیم کاربر ۲۰۲۶-۱۰-۲۳: باتریِ بازیابی‌شده می‌تواند بدون برق شهر
            وصل شود، اما اگر داخل پنجرهٔ محافظ چرخه دوباره قطع شود یعنی واقعاً
            ضعیف است و وصل خودکار بدون برق شهر تا برگشت برق شهر قفل می‌شود.
            اینجا: وصل بدون برق شهر، قطع سریع بعدش، و بعد ۴۰ ثانیه ولتاژ سالم -
            باید SAFE بماند. */
    func__Reset(800000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] healthy window opens */
    CHECK(func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);      /* [EN] hold done, 3 s starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);

    /* [EN] The weak pack sags again a few seconds later: cut, and the guard
            latches because this is well inside the 60 s window.
       [FA] باتری ضعیف چند ثانیه بعد دوباره افت می‌کند: قطع، و چون داخل
            پنجرهٔ ۶۰ ثانیه است محافظ قفل می‌شود. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);

    /* [EN] Even a long healthy stretch cannot undo the latch without mains.
       [FA] حتی ولتاژ سالم طولانی هم بدون برق شهر قفل را باز نمی‌کند. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    for (uint32_t__i = 0u; uint32_t__i < 10u; uint32_t__i++)
    {
        CHECK(func__Step(4000u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(BOOL__G__ProtectLine == true);

    /* [EN] The mains is the documented recovery from the latch: 30 s + 3 s.
       [FA] برق شهر همان راه بازیابیِ مستند است: ۳۰ ثانیه + ۳ ثانیه. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_INPUT_STABLE_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);

    /* ---- 6d. scenario 8: weak battery verdict, block, rest and one retry ----
       [EN] User order 2026-10-23: a pack that reconnects with no mains and
            then cuts again inside the guard window is weak. The verdict must
            latch, hold the battery path cut, keep the alarm face alive, and
            fall only after its own quiet-rest window with a healthy pack -
            then exactly one retry is allowed. The shape parameters must be
            reachable and clamped like every other scenario.
       [FA] دستور کاربر ۲۰۲۶-۱۰-۲۳: باتری‌ای که بدون برق شهر وصل می‌شود و
            داخل پنجرهٔ محافظ دوباره قطع می‌کند ضعیف است. حکم باید قفل شود،
            مسیر باتری را بسته نگه دارد، چهرهٔ هشدار را زنده بدارد، و فقط بعد
            از پنجرهٔ استراحتِ آرام با پک سالم پاک شود - آن‌وقت دقیقاً یک
            تلاش مجدد مجاز است. پارامترهای شکل هم باید مثل بقیهٔ سناریوها
            در دسترس و گیره‌خورده باشند. */
    func__Reset(950000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    CHECK(func__Changeover_WeakVerdictActive() == false);

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;          /* [EN] recovers, no mains */
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
    CHECK(BOOL__G__ProtectLine == false);                          /* [EN] first mains-free reconnect */

    /* [EN] The weak pack sags again a few seconds later -> verdict.
       [FA] باتری ضعیف چند ثانیه بعد دوباره افت می‌کند ← حکم. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);   /* [EN] 3 s filter arms / فیلتر مسلح */
    CHECK(func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Changeover_WeakVerdictActive() == true);
    CHECK(BOOL__G__ProtectLine == true);

    /* [EN] Healthy voltage alone must NOT reconnect while the verdict holds,
            for the whole rest window except the last millisecond.
       [FA] ولتاژ سالم به‌تنهایی تا آخرین میلی‌ثانیهٔ پنجرهٔ استراحت نباید وصل کند. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(CHANGEOVER_DEF_WEAK_REST_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Changeover_WeakVerdictActive() == true);
    CHECK(BOOL__G__ProtectLine == true);

    /* [EN] One millisecond later the verdict falls and the single retry runs:
            the 3 s filter, then the pack is back on the output with no mains.
       [FA] یک میلی‌ثانیه بعد حکم پاک می‌شود و تنها تلاش مجدد اجرا می‌شود:
            فیلتر ۳ ثانیه، بعد باتری بدون برق شهر روی خروجی. */
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Changeover_WeakVerdictActive() == false);
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);

    /* [EN] Shape parameters: clamped on write, readable back, foreign ids rejected.
       [FA] پارامترهای شکل: هنگام نوشتن گیره، برگشت‌پذیر و شناسهٔ بیگانه رد. */
    {
        uint32_t uint32_t__applied = 0u;
        uint32_t uint32_t__readBack = 0u;

        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS, 0u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 0u);                            /* [EN] 0 = solid red is legal */
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT, 2u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 5u);                            /* [EN] floor */
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT, 20u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 10u);                           /* [EN] ceiling */
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS, 5u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 20u);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_MS, 1000u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 60000u);
        CHECK(func__Changeover_GetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT, &uint32_t__readBack) == true);
        CHECK(uint32_t__readBack == 10u);
        CHECK(func__Changeover_SetParam(1u, 0u, &uint32_t__applied) == false);
        CHECK(func__Changeover_GetParam(1u, &uint32_t__readBack) == false);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE, 1u, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_COUNT,
                                        CHANGEOVER_DEF_WEAK_BEEP_COUNT, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_BEEP_LEN_MS,
                                        CHANGEOVER_DEF_WEAK_BEEP_LEN_MS, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_DUTY_PCT,
                                        CHANGEOVER_DEF_WEAK_LED_DUTY_PCT, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_LED_PERIOD_MS,
                                        CHANGEOVER_DEF_WEAK_LED_PERIOD_MS, &uint32_t__applied) == true);
        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_REST_MS,
                                        CHANGEOVER_DEF_WEAK_REST_MS, &uint32_t__applied) == true);
    }

    /* [EN] Switching the scenario off releases a latched verdict immediately.
       [FA] خاموش‌کردن سناریو، حکم قفل‌شده را فوری آزاد می‌کند. */
    func__Reset(960000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
    CHECK(func__Changeover_WeakVerdictActive() == true);
    {
        uint32_t uint32_t__applied = 0u;

        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE, 0u, &uint32_t__applied) == true);
        CHECK(uint32_t__applied == 0u);
    }
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
    (void)func__Step(10u, FAULT_NONE);
    CHECK(func__Changeover_WeakVerdictActive() == false);
    {
        uint32_t uint32_t__applied = 0u;

        CHECK(func__Changeover_SetParam(CHANGEOVER_PARAM_WEAK_ENABLE,
                                        CHANGEOVER_DEF_WEAK_ENABLE, &uint32_t__applied) == true);
    }

    /* [EN] Scenario-8 reset key (slot 204, the panel button): one call drops a
          latched verdict and restarts the mains-free reconnect bookkeeping so
          the single retry is available again. With no verdict latched it is a
          harmless no-op that reports false.
       [FA] کلید ریست سناریوی ۸ (اسلات ۲۰۴، دکمهٔ پنل): یک فراخوانی حکم قفل را
          پاک و سابقهٔ وصلِ بدون برق شهر را از نو می‌کند تا همان تنها تلاش
          مجدد آزاد شود. بدون حکم، کار بی‌خطری است و false برمی‌گرداند. */
    {
        uint32_t uint32_t__applied = 0u;

        func__Reset(980000u);
        MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        func__CutAndVerifyTiming();
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 25000u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DEF_HEALTHY_HOLD_MS, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
        MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
        (void)func__Step(10u, FAULT_NONE);
        (void)func__Step(CHANGEOVER_DURATION_MS, FAULT_NONE);
        CHECK(func__Changeover_WeakVerdictActive() == true);
        CHECK(func__Changeover_WeakVerdictReset() == true);
        CHECK(func__Changeover_WeakVerdictActive() == false);
        CHECK(func__Changeover_WeakVerdictReset() == false);
        CHECK(uint32_t__applied == 0u);
    }

#if MODULE_IMBALANCE
    /* ---- 7. scenario-5 veto: a latched imbalance never reaches the output ---- */
    func__Reset(400000u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    BOOL__G__ImbalanceBlocks = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] immediate, no 3 s */
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    /* [EN] A hard fault still outranks the veto. / [FA] فالت سخت بالاتر از وتوست. */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(10u, FAULT_ADC) == APP_STATE_FAULT);
    BOOL__G__ImbalanceBlocks = false;
#endif

#if MODULE_CHARGER
    /* ---- 8. scenario-6 veto: a condemned pack never reaches the output ---- */
    func__Reset(500000u);
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_INPUT);
    BOOL__G__DeadBlocks = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    BOOL__G__DeadBlocks = false;
#endif

    /* ---- 9. the 49.7-day tick wrap must not break the 3 s filter ----
       [EN] Start the cut window a few ms before the tick counter wraps; the
            unsigned subtraction has to carry the elapsed time across zero.
       [FA] پنجرهٔ قطع چند میلی‌ثانیه پیش از سرریز شمارندهٔ تیک شروع می‌شود؛
            تفریق بدون‌علامت باید زمان سپری‌شده را از صفر رد کند. */
    func__Reset(0xFFFFFF00u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    CHECK(UINT32_T__G__FakeTick < 0xFFFFFF00u);               /* [EN] really wrapped */

    /* ---- 10. the state getter mirrors the evaluation ---- */
    func__Reset(600000u);
    app_state_t__state = func__Step(10u, FAULT_NONE);
    CHECK(app_state_t__state == APP_STATE_INPUT);

    /* ---- 11. boot validation with no mains, healthy battery ---- */
    /* [EN] Init must keep the battery cut until a valid sample arrives; with
       no mains, a healthy 24 V pack then connects and remains connected.
       [FA] Init تا نمونهٔ معتبر مسیر باتری را قطع نگه می‌دارد؛ بدون ورودی،
       پک سالم ۲۴ ولت پس از نمونه وصل می‌شود و وصل می‌ماند. */
    func__Reset(700000u);
    func__Changeover_Init();
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 1u);
    UINT32_T__G__ProtectWrites = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BATTERY);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 1u);

    for (uint32_t__i = 0u; uint32_t__i < 300u; uint32_t__i++)
    {
        (void)func__Step(10u, FAULT_NONE);
    }
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 1u);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
