/**
 * @file    host_test_jitter.c
 * @brief   [EN] Host unit test for the Jitter module. The PRODUCTION
 *              jitter.c is compiled as-is; the EXTI board layer is faked so
 *              the test can inject edge events by hand.
 *          [FA] تست هاست ماژول Jitter: کد محصول بدون تغییر کامپایل می‌شود و
 *              لایهٔ EXTI برد بدلی است تا تست رویداد لبه را دستی تزریق کند.
 *
 * @note    [EN] The contract under test: the module LATCHES a trip until the
 *              owner clears it, the BSP event is take-and-clear (one event
 *              is consumed once), the two channels never leak into each
 *              other, and a channel number outside 1..2 is refused.
 *          [FA] قرارداد زیر تست: تریپ تا پاک‌کردن صاحبش قفل می‌ماند، رویداد
 *              BSP یک‌بارمصرف است، دو کانال به هم نشت نمی‌کنند و شمارهٔ کانال
 *              خارج از ۱..۲ رد می‌شود.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "jitter.h"
#include "bsp_exti.h"

/* ==================== Check helper / کمک‌کنندهٔ بررسی ==================== */

static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;

#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)

/* ==================== Fake EXTI layer / لایهٔ EXTI بدلی ==================== */

/* [EN] Pending edge per source, exactly like the real take-and-clear BSP.
   [FA] لبهٔ در انتظار هر منبع، دقیقاً مثل BSP واقعی که با خواندن پاک می‌شود. */
static bool     BOOL__G__A__Pending[2] = { false, false };
static uint32_t UINT32_T__G__InitCalls = 0u;

void func__BspExti_Init(void)
{
    UINT32_T__G__InitCalls++;
    BOOL__G__A__Pending[0] = false;
    BOOL__G__A__Pending[1] = false;
}

bool func__BspExti_TakeEvent(bsp_exti_src_t bsp_exti_src_t__source)
{
    bool bool__taken = false;

    if (bsp_exti_src_t__source == BSP_EXTI_JITTER1)
    {
        bool__taken = BOOL__G__A__Pending[0];
        BOOL__G__A__Pending[0] = false;
    }
    else if (bsp_exti_src_t__source == BSP_EXTI_JITTER2)
    {
        bool__taken = BOOL__G__A__Pending[1];
        BOOL__G__A__Pending[1] = false;
    }
    else
    {
        bool__taken = false;
    }

    return bool__taken;
}

/**
 * @brief  [EN] Simulate one hardware edge on a jitter input.
 *         [FA] شبیه‌سازی یک لبهٔ سخت‌افزاری روی ورودی جیتر.
 */
static void func__InjectEdge(uint8_t uint8_t__channel)
{
    BOOL__G__A__Pending[uint8_t__channel - 1u] = true;
}

int main(void)
{
    uint32_t uint32_t__i;

    printf("== Jitter host test ==\n");

    /* ---- 1. Init arms the board layer and starts with no trip ---- */
    func__Jitter_Init();
    CHECK(UINT32_T__G__InitCalls == 1u);
    CHECK(func__Jitter_ChannelTripped(1u) == false);
    CHECK(func__Jitter_ChannelTripped(2u) == false);

    /* ---- 2. a run with no edge changes nothing ---- */
    for (uint32_t__i = 0u; uint32_t__i < 10u; uint32_t__i++)
    {
        func__Jitter_Run();
    }
    CHECK(func__Jitter_ChannelTripped(1u) == false);
    CHECK(func__Jitter_ChannelTripped(2u) == false);

    /* ---- 3. one edge on channel 1 trips channel 1 only ---- */
    func__InjectEdge(1u);
    func__Jitter_Run();
    CHECK(func__Jitter_ChannelTripped(1u) == true);
    CHECK(func__Jitter_ChannelTripped(2u) == false);

    /* ---- 4. the trip is LATCHED: later runs without an edge keep it ----
       [EN] The charger decides when a trip is forgiven, not the detector.
       [FA] بخشیدن تریپ کار شارژر است نه آشکارساز. */
    for (uint32_t__i = 0u; uint32_t__i < 100u; uint32_t__i++)
    {
        func__Jitter_Run();
    }
    CHECK(func__Jitter_ChannelTripped(1u) == true);

    /* ---- 5. clearing one channel never clears the other ---- */
    func__InjectEdge(2u);
    func__Jitter_Run();
    CHECK(func__Jitter_ChannelTripped(2u) == true);
    func__Jitter_ClearChannel(1u);
    CHECK(func__Jitter_ChannelTripped(1u) == false);
    CHECK(func__Jitter_ChannelTripped(2u) == true);
    func__Jitter_ClearChannel(2u);
    CHECK(func__Jitter_ChannelTripped(2u) == false);

    /* ---- 6. the BSP event is consumed once: a cleared trip does not
               come back by itself on the next run ----
       [EN] If the event were not take-and-clear, this would re-trip.
       [FA] اگر رویداد یک‌بارمصرف نبود، تریپ خودبه‌خود برمی‌گشت. */
    func__Jitter_Run();
    CHECK(func__Jitter_ChannelTripped(1u) == false);
    CHECK(func__Jitter_ChannelTripped(2u) == false);

    /* ---- 7. an edge that arrives while already tripped is harmless ---- */
    func__InjectEdge(1u);
    func__Jitter_Run();
    func__InjectEdge(1u);
    func__Jitter_Run();
    CHECK(func__Jitter_ChannelTripped(1u) == true);
    func__Jitter_ClearChannel(1u);
    CHECK(func__Jitter_ChannelTripped(1u) == false);

    /* ---- 8. channel numbers outside 1..2 are refused, not mapped ----
       [EN] The module is 1-based on purpose (charger channels are 1 and 2);
            index 0 and 3 must read false and clearing them must be a no-op.
       [FA] شماره‌گذاری عمداً از ۱ است؛ ۰ و ۳ باید false بدهند و پاک‌کردنشان
            هیچ کاری نکند. */
    func__InjectEdge(1u);
    func__InjectEdge(2u);
    func__Jitter_Run();
    CHECK(func__Jitter_ChannelTripped(0u) == false);
    CHECK(func__Jitter_ChannelTripped(3u) == false);
    CHECK(func__Jitter_ChannelTripped(255u) == false);
    func__Jitter_ClearChannel(0u);
    func__Jitter_ClearChannel(3u);
    CHECK(func__Jitter_ChannelTripped(1u) == true);
    CHECK(func__Jitter_ChannelTripped(2u) == true);

    /* ---- 9. Init wipes both latches and re-arms the board layer ---- */
    func__Jitter_Init();
    CHECK(UINT32_T__G__InitCalls == 2u);
    CHECK(func__Jitter_ChannelTripped(1u) == false);
    CHECK(func__Jitter_ChannelTripped(2u) == false);

    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
