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
    BOOL__G__ProtectLine        = false;
    UINT32_T__G__ProtectWrites  = 0u;
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

    func__Changeover_Init();
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

    /* ---- 1. boot and the "no decision" guards ----
       [EN] Init starts in BOOT; a NULL or invalid snapshot must change
            nothing at all - no state move, no pin write.
       [FA] شروع از BOOT؛ snapshot خالی یا نامعتبر هیچ چیزی را عوض نمی‌کند. */
    func__Reset(0u);
    CHECK(func__Changeover_Evaluate(NULL, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(UINT32_T__G__ProtectWrites == 0u);

    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_BOOT);
    CHECK(UINT32_T__G__ProtectWrites == 0u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;

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

    /* ---- 6. reconnect needs mains AND 21200 mV AND 3 s (user decision ج-۲) ----
       [EN] After a cut the pack recovers, but with no mains the module must
            stay parked in SAFE for ever. Only when the input shows up does
            the 3 s reconnect filter start.
       [FA] پس از قطع، باتری بهبود می‌یابد ولی بدون برق ورودی باید برای همیشه
            در SAFE بماند. فیلتر ۳ ثانیهٔ وصل فقط با حضور ورودی شروع می‌شود. */
    func__Reset(200000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;      /* [EN] fully recovered */
    for (uint32_t__i = 0u; uint32_t__i < 20u; uint32_t__i++)
    {
        CHECK(func__Step(1000u, FAULT_NONE) == APP_STATE_SAFE);
    }
    CHECK(BOOL__G__ProtectLine == true);

    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);     /* [EN] filter starts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(BOOL__G__ProtectLine == true);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);
    CHECK(BOOL__G__ProtectLine == false);
    CHECK(UINT32_T__G__ProtectWrites == 2u);

    /* [EN] Losing the input mid-filter must abandon the reconnect.
       [FA] قطع ورودی وسط فیلتر، وصل مجدد را رها می‌کند. */
    func__Reset(300000u);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = CHANGEOVER_BAT_CRITICAL_CUT_MV - 1u;
    func__CutAndVerifyTiming();
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    (void)func__Step(10u, FAULT_NONE);
    (void)func__Step(CHANGEOVER_DURATION_MS - 500u, FAULT_NONE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = false;    /* [EN] mains lost */
    CHECK(func__Step(10u, FAULT_NONE) == APP_STATE_SAFE);
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    (void)func__Step(10u, FAULT_NONE);                        /* [EN] filter restarts */
    CHECK(func__Step(CHANGEOVER_DURATION_MS - 1u, FAULT_NONE) == APP_STATE_SAFE);
    CHECK(func__Step(1u, FAULT_NONE) == APP_STATE_INPUT);

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

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
