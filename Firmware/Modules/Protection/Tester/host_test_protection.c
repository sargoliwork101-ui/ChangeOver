/**
 * @file    host_test_protection.c
 * @brief   [EN] Host unit test for the Protection module. The PRODUCTION
 *              protection.c is compiled as-is; the fault module is faked so
 *              the test can see exactly which bit the guard touches.
 *          [FA] تست هاست ماژول Protection: کد محصول بدون تغییر کامپایل
 *              می‌شود و ماژول فالت بدلی است تا دقیقاً دیده شود کدام بیت
 *              دست‌کاری می‌شود.
 *
 * @note    [EN] The one rule this module owns: FAULT_ADC is a LIVE bit, not
 *              a latched one. It must be set while the snapshot is unusable
 *              and cleared by the very first usable snapshot - a latched
 *              FAULT_ADC would park the whole device for the rest of the
 *              power cycle.
 *          [FA] تنها قانون این ماژول: بیت ‎FAULT_ADC‎ زنده است نه قفل‌شونده؛
 *              با snapshot ناسالم ست و با نخستین snapshot سالم پاک می‌شود.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "protection.h"
#include "fault.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake fault module / ماژول فالت بدلی ==================== */

static fault_mask_t FAULT_MASK_T__G__Mask      = FAULT_NONE;
static uint32_t     UINT32_T__G__SetCalls      = 0u;
static uint32_t     UINT32_T__G__ClearCalls    = 0u;
static fault_mask_t FAULT_MASK_T__G__LastSet   = FAULT_NONE;
static fault_mask_t FAULT_MASK_T__G__LastClear = FAULT_NONE;

void func__Fault_Set(fault_mask_t fault_mask_t__bits)
{
    FAULT_MASK_T__G__Mask |= fault_mask_t__bits;
    FAULT_MASK_T__G__LastSet = fault_mask_t__bits;
    UINT32_T__G__SetCalls++;
}

void func__Fault_Clear(fault_mask_t fault_mask_t__bits)
{
    FAULT_MASK_T__G__Mask &= ~fault_mask_t__bits;
    FAULT_MASK_T__G__LastClear = fault_mask_t__bits;
    UINT32_T__G__ClearCalls++;
}

/* ==================== Test driver / رانندهٔ تست ==================== */

static measurement_snapshot_t MEASUREMENT_SNAPSHOT_T__G__Snap;

static void func__Reset(void)
{
    FAULT_MASK_T__G__Mask      = FAULT_NONE;
    UINT32_T__G__SetCalls      = 0u;
    UINT32_T__G__ClearCalls    = 0u;
    FAULT_MASK_T__G__LastSet   = FAULT_NONE;
    FAULT_MASK_T__G__LastClear = FAULT_NONE;

    MEASUREMENT_SNAPSHOT_T__G__Snap.v_in_mv       = 24000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat24_mv    = 25000u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat12_mv    = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_low_mv  = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.v_bat_high_mv = 12500u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch1_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.i_ch2_ma      = 0u;
    MEASUREMENT_SNAPSHOT_T__G__Snap.input_present = true;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid         = true;

    func__Protection_Init();
}

int main(void)
{
    uint32_t uint32_t__i;

    printf("== Protection host test ==\n");

    /* ---- 1. Init must not touch any fault bit ---- */
    func__Reset();
    CHECK(UINT32_T__G__SetCalls == 0u);
    CHECK(UINT32_T__G__ClearCalls == 0u);
    CHECK(FAULT_MASK_T__G__Mask == FAULT_NONE);

    /* ---- 2. a NULL snapshot raises exactly FAULT_ADC, nothing else ---- */
    func__Reset();
    func__Protection_Run(NULL);
    CHECK(FAULT_MASK_T__G__Mask == FAULT_ADC);
    CHECK(FAULT_MASK_T__G__LastSet == FAULT_ADC);
    CHECK(UINT32_T__G__ClearCalls == 0u);

    /* ---- 3. an invalid snapshot behaves the same ---- */
    func__Reset();
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    func__Protection_Run(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    CHECK(FAULT_MASK_T__G__Mask == FAULT_ADC);

    /* ---- 4. LIVE, not latched: the first valid snapshot clears it ----
       [EN] This is the whole point of the module. If this check ever fails,
            the device stays parked in SAFE for the rest of the power cycle.
       [FA] تمام نکتهٔ ماژول همین است؛ اگر این بررسی بیفتد، دستگاه تا پایان
            همان روشن‌بودن در حالت امن قفل می‌ماند. */
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    func__Protection_Run(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    CHECK(FAULT_MASK_T__G__Mask == FAULT_NONE);
    CHECK(FAULT_MASK_T__G__LastClear == FAULT_ADC);

    /* ---- 5. it never touches a foreign bit ----
       [EN] A battery-lost bit owned by the fault module must survive both
            directions of the ADC verdict.
       [FA] بیت قطع باتری که مال ماژول فالت است باید در هر دو جهت زنده بماند. */
    func__Reset();
    FAULT_MASK_T__G__Mask = FAULT_CHARGER_BAT_LOST;
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = false;
    func__Protection_Run(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    CHECK(FAULT_MASK_T__G__Mask == (FAULT_CHARGER_BAT_LOST | FAULT_ADC));
    MEASUREMENT_SNAPSHOT_T__G__Snap.valid = true;
    func__Protection_Run(&MEASUREMENT_SNAPSHOT_T__G__Snap);
    CHECK(FAULT_MASK_T__G__Mask == FAULT_CHARGER_BAT_LOST);

    /* ---- 6. flapping validity is followed every single pass ---- */
    func__Reset();
    for (uint32_t__i = 0u; uint32_t__i < 50u; uint32_t__i++)
    {
        MEASUREMENT_SNAPSHOT_T__G__Snap.valid = ((uint32_t__i % 2u) == 0u);
        func__Protection_Run(&MEASUREMENT_SNAPSHOT_T__G__Snap);
        if (MEASUREMENT_SNAPSHOT_T__G__Snap.valid != false)
        {
            CHECK(FAULT_MASK_T__G__Mask == FAULT_NONE);
        }
        else
        {
            CHECK(FAULT_MASK_T__G__Mask == FAULT_ADC);
        }
    }

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
