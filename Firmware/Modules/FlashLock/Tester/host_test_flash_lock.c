/**
 * @file    host_test_flash_lock.c
 * @brief   [EN] Host test for the FlashLock module. Compiles the PRODUCTION
 *              flash_lock.c against a stubbed BSP (level, program, reset) and
 *              proves the one-time policy: locked parts are never re-programmed,
 *              a failed program never triggers a reset, and the "lock twice"
 *              case does nothing on the second call.
 *          [FA] تست هاست ماژول FlashLock. کد محصول flash_lock.c را با BSP بدلی
 *              (سطح، نوشتن، ریست) کامپایل می‌کند و سیاست یک‌بارمصرف را ثابت
 *              می‌کند: قطعهٔ قفل‌شده هرگز دوباره نوشته نمی‌شود، نوشتن شکست‌خورده
 *              هرگز ریست نمی‌دهد، و بار دوم «قفل دوباره» هیچ کاری نمی‌کند.
 *
 * @note    [EN] The option-byte sequence itself cannot be executed on a PC;
 *              it is stubbed here. Bench proof lives in the module workbook.
 *          [FA] خودِ ترتیب option byte روی کامپیوتر اجرا نمی‌شود و اینجا بدل
 *              شده است. اثبات روی میز تست در کاربرگ همان ماژول ثبت می‌شود.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "flash_lock.h"
#include "bsp_flash_lock.h"

/* ==================== Stub world / دنیای بدلی ==================== */

static bsp_flash_lock_level_t BSP_FLASH_LOCK_LEVEL__G__fakeLevel;
static bool BOOL__G__applySucceeds;
static uint32_t UINT32_T__G__applyCalls;
static uint32_t UINT32_T__G__launchCalls;
static uint32_t UINT32_T__G__levelReads;

/* ==================== Counters reset / پاک‌کردن شمارنده‌ها ==================== */

static void func__resetCounters(bsp_flash_lock_level_t bsp_flash_lock_level__level,
                                bool bool__applySucceeds)
{
    BSP_FLASH_LOCK_LEVEL__G__fakeLevel = bsp_flash_lock_level__level;
    BOOL__G__applySucceeds = bool__applySucceeds;
    UINT32_T__G__applyCalls = 0u;
    UINT32_T__G__launchCalls = 0u;
    UINT32_T__G__levelReads = 0u;
}

/* ==================== Stub BSP / بدل BSP ==================== */

bsp_flash_lock_level_t func__BspFlashLock_GetLevel(void)
{
    UINT32_T__G__levelReads++;
    return BSP_FLASH_LOCK_LEVEL__G__fakeLevel;
}

bool func__BspFlashLock_ApplyLevel1(void)
{
    UINT32_T__G__applyCalls++;

    if (!BOOL__G__applySucceeds)
    {
        return false;
    }

    /* [EN] The real part only changes level after the next reset; the stub
       flips it here so the "second call" case can be exercised. */
    /* [FA] قطعهٔ واقعی سطح را فقط بعد از ریست عوض می‌کند؛ بدل اینجا عوضش
       می‌کند تا حالت «فراخوانی دوم» قابل آزمایش باشد. */
    BSP_FLASH_LOCK_LEVEL__G__fakeLevel = BSP_FLASH_LOCK_LEVEL_PROTECTED;
    return true;
}

void func__BspFlashLock_LaunchReset(void)
{
    UINT32_T__G__launchCalls++;
}

/* ==================== Assert helper / کمک‌کنندهٔ بررسی ==================== */

static int INT__G__failures;

static void func__check(bool bool__condition, const char *char_p__what)
{
    if (!bool__condition)
    {
        printf("FAIL: %s\n", char_p__what);
        INT__G__failures++;
        return;
    }

    printf("ok  : %s\n", char_p__what);
}

/* ==================== Case 1: already protected ==================== */

static void func__caseAlreadyProtected(void)
{
    flash_lock_result_t flash_lock_result__result;

    printf("-- case 1: part already locked\n");
    func__resetCounters(BSP_FLASH_LOCK_LEVEL_PROTECTED, true);

    flash_lock_result__result = func__FlashLock_ApplyOnce();

    func__check(FLASH_LOCK_RESULT_ALREADY_PROTECTED == flash_lock_result__result,
                "locked part reports ALREADY_PROTECTED");
    func__check(0u == UINT32_T__G__applyCalls,
                "locked part never programs the option bytes again");
    func__check(0u == UINT32_T__G__launchCalls,
                "locked part never resets the board");
}

/* ==================== Case 2: open part locks once ==================== */

static void func__caseOpenPartLocks(void)
{
    flash_lock_result_t flash_lock_result__result;

    printf("-- case 2: open part, programming succeeds\n");
    func__resetCounters(BSP_FLASH_LOCK_LEVEL_OPEN, true);

    flash_lock_result__result = func__FlashLock_ApplyOnce();

    func__check(FLASH_LOCK_RESULT_JUST_LOCKED == flash_lock_result__result,
                "open part reports JUST_LOCKED");
    func__check(1u == UINT32_T__G__applyCalls,
                "open part programs the option bytes exactly once");
    func__check(1u == UINT32_T__G__launchCalls,
                "open part asks for the reload/reset exactly once");
    func__check(BSP_FLASH_LOCK_LEVEL_PROTECTED == func__BspFlashLock_GetLevel(),
                "part is protected after the call");

    /* [EN] Second call on the freshly locked part: nothing may happen. */
    /* [FA] فراخوانی دوم روی قطعهٔ تازه‌قفل‌شده: هیچ کاری نباید بشود. */
    UINT32_T__G__applyCalls = 0u;
    UINT32_T__G__launchCalls = 0u;
    flash_lock_result__result = func__FlashLock_ApplyOnce();

    func__check(FLASH_LOCK_RESULT_ALREADY_PROTECTED == flash_lock_result__result,
                "second call is a no-op");
    func__check(0u == UINT32_T__G__applyCalls,
                "second call does not program again");
    func__check(0u == UINT32_T__G__launchCalls,
                "second call does not reset again");
}

/* ==================== Case 3: programming failure ==================== */

static void func__caseProgrammingFails(void)
{
    flash_lock_result_t flash_lock_result__result;

    printf("-- case 3: open part, programming fails\n");
    func__resetCounters(BSP_FLASH_LOCK_LEVEL_OPEN, false);

    flash_lock_result__result = func__FlashLock_ApplyOnce();

    func__check(FLASH_LOCK_RESULT_FAILED == flash_lock_result__result,
                "failed programming reports FAILED");
    func__check(1u == UINT32_T__G__applyCalls,
                "failed programming is not retried in the same boot");
    func__check(0u == UINT32_T__G__launchCalls,
                "failed programming never resets the board");
    func__check(BSP_FLASH_LOCK_LEVEL_OPEN == func__BspFlashLock_GetLevel(),
                "part stays OPEN so the board still boots");
}

/* ==================== Case 4: level is asked before writing ==================== */

static void func__caseLevelAskedFirst(void)
{
    printf("-- case 4: state is read before anything is written\n");
    func__resetCounters(BSP_FLASH_LOCK_LEVEL_PROTECTED, true);

    (void)func__FlashLock_ApplyOnce();

    func__check(UINT32_T__G__levelReads >= 1u,
                "protection state is read before deciding");
    func__check(0u == UINT32_T__G__applyCalls,
                "no write happens when the state says protected");
}

/* ==================== Main ==================== */

int main(void)
{
    INT__G__failures = 0;

    func__caseAlreadyProtected();
    func__caseOpenPartLocks();
    func__caseProgrammingFails();
    func__caseLevelAskedFirst();

    printf("\nflash_lock host test: %d failure(s)\n", INT__G__failures);
    return (0 == INT__G__failures) ? 0 : 1;
}
