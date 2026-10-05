/**
 * @file    bsp_iwdg.c
 * @brief   [EN] Direct-register IWDG driver + multi-task liveness monitor
 *              (no HAL IWDG sources needed - the CubeIDE Drivers tree does
 *              not vendor stm32f1xx_hal_iwdg, same as the flash driver).
 *          [FA] درایور مستقیم رجیستری IWDG + ناظر زنده‌بودن چندتسکی (بدون
 *              نیاز به سورس HAL مثل درایور فلش).
 */

#include "bsp_iwdg.h"
#include "modules_enable.h"

#include "stm32f1xx.h"

/* [EN] IWDG key words / کلیدهای IWDG. */
#define BSP_IWDG_KEY_ENABLE_ACCESS 0x5555u
#define BSP_IWDG_KEY_RELOAD        0xAAAAu
#define BSP_IWDG_KEY_START         0xCCCCu

/* [EN] Prescaler /32 (PR=3) + reload 1250 = nominal 1.0 s at 40 kHz LSI
   (real LSI 30..60 kHz -> 0.67..1.33 s; see the header note).
   [FA] پری‌اسکیلر ۳۲/ + ریلود ۱۲۵۰ = اسمی ۱٫۰ ثانیه (واقعی ۰٫۶۷..۱٫۳۳). */
#define BSP_IWDG_PRESCALER_DIV32 3u
#define BSP_IWDG_RELOAD_1S       1250u

/* [EN] Bounded spins (no tick yet at Init time; core runs 72 MHz, LSI ready
   is microseconds, register sync a few LSI cycles - these bounds are
   generous, not timings).
   [FA] چرخش‌های کران‌دار (هنگام Init هنوز تیک نیست). */
#define BSP_IWDG_LSI_READY_SPIN 400000u
#define BSP_IWDG_SYNC_SPIN      400000u

/* [EN] Bit set when the same MODULE flag that makes rtos_app.c create the
   thread is on - the supervised set can never disagree with the created
   set. Control is the pumper (existence = its own creation guard).
   [FA] بیت هر تسک با همان فلگی ست می‌شود که thread را می‌سازد - مجموعهٔ
   تحت‌نظر هرگز با ساخته‌شده‌ها اختلاف ندارد. کنترل پمپ‌زن است. */
#define BSP_IWDG_EXPECT_MEAS \
    ((uint32_t)MODULE_MEASUREMENT << (uint32_t)BSP_IWDG_SLOT_MEASUREMENT)
#define BSP_IWDG_EXPECT_COMM \
    ((uint32_t)MODULE_ESP << (uint32_t)BSP_IWDG_SLOT_COMM)
#define BSP_IWDG_EXPECT_PROT \
    ((uint32_t)MODULE_PROTECTION << (uint32_t)BSP_IWDG_SLOT_PROTECTION)
#define BSP_IWDG_EXPECT_UI \
    ((uint32_t)MODULE_UI << (uint32_t)BSP_IWDG_SLOT_UI)
#define BSP_IWDG_EXPECT_MASK \
    (BSP_IWDG_EXPECT_MEAS | BSP_IWDG_EXPECT_COMM | BSP_IWDG_EXPECT_PROT | \
     BSP_IWDG_EXPECT_UI)

/* [EN] "Control task exists" = the exact creation guard in rtos_app.c; if it
   is off nobody could kick, so the watchdog stays stopped (documented in
   the header).
   [FA] «تسک کنترل هست» = همان گارد ساخت در rtos_app.c؛ اگر خاموش است
   واچ‌داگ عمداً روشن نمی‌شود. */
#if (MODULE_CHANGEOVER || MODULE_CHARGER || MODULE_JITTER || MODULE_MCU_POWER_PATH)
#define BSP_IWDG_PUMPER_EXISTS 1u
#else
#define BSP_IWDG_PUMPER_EXISTS 0u
#endif

/* [EN] Last check-in tick per slot; 0 = never. Written by task context,
   read by the control task - aligned u32 is atomic on Cortex-M3, and a
   torn read only delays one kick decision by one pass (60x margin).
   [FA] آخرین تیک اعلام حضور هر شکاف؛ ۰ = هرگز. خواندن پاره فقط یک پاس
   تصمیم را عقب می‌اندازد (حاشیهٔ ۶۰ برابری). */
static volatile uint32_t UINT32_T__A__G__IwdgLastCheckInMs[BSP_IWDG_SLOT_COUNT] = {0u};

/* ==================== BspIwdg_Init ==================== */

/**
 * @brief  [EN] Bring the independent watchdog up: enable the LSI and wait
 *              for it to be ready, unlock the IWDG registers, program the
 *              /32 prescaler with a 1250 reload (about 1 second at the
 *              nominal 40 kHz LSI), freeze the counter while the core is
 *              halted so a debugger session never resets the board, then
 *              start it. Runs once from main() before the scheduler, so the
 *              first kick has to arrive inside the timeout - the control
 *              task kicks on every pass and its first pass is milliseconds
 *              after the scheduler starts. In a module combination without
 *              a control task nobody could ever kick, so the watchdog is
 *              deliberately left stopped instead of resetting the board in
 *              a loop.
 *         [FA] واچ‌داگ مستقل را بالا می‌آورد: LSI را روشن می‌کند و منتظر
 *              آماده‌شدنش می‌ماند، رجیسترهای IWDG را باز می‌کند، پیش‌مقیاس ۳۲/
 *              را با بارگذاری مجدد ۱۲۵۰ تنظیم می‌کند (حدود یک ثانیه با LSI
 *              اسمی ۴۰ کیلوهرتز)، شمارنده را هنگام halt شدن هسته فریز می‌کند
 *              تا جلسهٔ دیباگ هرگز برد را ریست نکند، و بعد آن را شروع می‌کند.
 *              یک‌بار از main و پیش از زمان‌بند اجرا می‌شود، پس اولین kick باید
 *              داخل تایم‌اوت برسد؛ تسک کنترل هر پاس kick می‌زند و اولین پاسش
 *              میلی‌ثانیه‌ها بعد از شروع زمان‌بند است. در ترکیبی که تسک کنترل
 *              ندارد هیچ‌کس نمی‌تواند kick بزند، پس واچ‌داگ عمداً خاموش می‌ماند
 *              به‌جای اینکه برد را در حلقه ریست کند.
 */
void func__BspIwdg_Init(void)
{
#if (BSP_IWDG_PUMPER_EXISTS != 0u)
    uint32_t uint32_t__spin;

    /* [EN] Freeze the watchdog while the core is halted so debugging never
       resets the board (DBGMCU is always clocked on F1).
       [FA] فریز واچ‌داگ با halt تا دیباگ برد را ریست نکند. */
    DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;

    /* [EN] LSI on + bounded wait for ready (starting the IWDG would force
       LSI on anyway; explicit is deterministic).
       [FA] روشن‌کردن LSI + انتظار کران‌دار برای آماده‌شدن. */
    RCC->CSR |= RCC_CSR_LSION;
    uint32_t__spin = BSP_IWDG_LSI_READY_SPIN;
    while (((RCC->CSR & RCC_CSR_LSIRDY) == 0u) && (uint32_t__spin > 0u))
    {
        uint32_t__spin--;
    }

    /* [EN] Unlock, program prescaler + reload, wait the register sync, then
       reload once and start. From here a kick must arrive every <0.67 s.
       [FA] بازگشایی، تنظیم پری‌اسکیلر + ریلود، انتظار سینک، یک ریلود و شروع.
       از اینجا kick باید هر کمتر از ۰٫۶۷ ثانیه برسد. */
    IWDG->KR = BSP_IWDG_KEY_ENABLE_ACCESS;
    IWDG->PR = BSP_IWDG_PRESCALER_DIV32;
    IWDG->RLR = BSP_IWDG_RELOAD_1S;
    uint32_t__spin = BSP_IWDG_SYNC_SPIN;
    while (((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) &&
           (uint32_t__spin > 0u))
    {
        uint32_t__spin--;
    }
    IWDG->KR = BSP_IWDG_KEY_RELOAD;
    IWDG->KR = BSP_IWDG_KEY_START;
#else
    /* [EN] No control task in this module combination: nobody could kick, so
       the watchdog deliberately stays stopped (header documents this).
       [FA] در این ترکیب تسک کنترل نیست پس واچ‌داگ عمداً خاموش می‌ماند. */
#endif
}

/* ==================== BspIwdg_CheckIn ==================== */

/**
 * @brief  [EN] Record one proof-of-life for a supervised task by stamping
 *              its slot with the current time. Two guards make the call
 *              harmless from anywhere: an out-of-range slot index is
 *              dropped, and so is a slot that this module combination does
 *              not expect (BSP_IWDG_EXPECT_MASK), so a task that is
 *              compiled out can never hold the kick decision hostage.
 *         [FA] یک اعلام حضور تسک تحت‌نظر را با مهرزدنِ زمان فعلی روی شکاف آن
 *              ثبت می‌کند. دو نگهبان فراخوانی را از هر جایی بی‌خطر می‌کنند:
 *              شمارهٔ شکاف بیرون از بازه دور ریخته می‌شود و شکافی که این ترکیب
 *              ماژول‌ها انتظارش را ندارد (‎BSP_IWDG_EXPECT_MASK‎) هم همین‌طور،
 *              پس تسکی که کامپایل نشده هرگز نمی‌تواند تصمیم kick را گروگان
 *              بگیرد.
 * @param  bsp_iwdg_slot_t__slot [EN] Which supervised task is checking in,
 *                                   range 0..BSP_IWDG_SLOT_COUNT-1 /
 *                                   کدام تسک تحت‌نظر اعلام حضور می‌کند، بازهٔ
 *                                   ۰ تا ‎BSP_IWDG_SLOT_COUNT-1‎
 * @param  uint32_t__nowMs       [EN] Current FreeRTOS tick in ms (the tick
 *                                   runs at 1 kHz), free to wrap /
 *                                   تیک فعلی فری‌آرتاس بر حسب میلی‌ثانیه (تیک
 *                                   ۱ کیلوهرتز است) و سرریزش اشکالی ندارد
 */
void func__BspIwdg_CheckIn(bsp_iwdg_slot_t bsp_iwdg_slot_t__slot,
                           uint32_t uint32_t__nowMs)
{
    if (((uint32_t)bsp_iwdg_slot_t__slot < (uint32_t)BSP_IWDG_SLOT_COUNT) &&
        ((BSP_IWDG_EXPECT_MASK &
          ((uint32_t)1u << (uint32_t)bsp_iwdg_slot_t__slot)) != 0u))
    {
        UINT32_T__A__G__IwdgLastCheckInMs[bsp_iwdg_slot_t__slot] = uint32_t__nowMs;
    }
}

/* ==================== BspIwdg_PollKick ==================== */

/**
 * @brief  [EN] Refresh the watchdog if, and only if, every expected slot
 *              checked in no longer than BSP_IWDG_STALE_MS ago. One stale
 *              task is enough to withhold the kick and let the watchdog
 *              reset the board, which is the whole point: a system where
 *              one task died is not healthy just because another one is
 *              still looping. The age arithmetic is wrap-safe, and a slot
 *              that never checked in (stamp 0) doubles as the boot grace -
 *              kicks flow until the clock itself reaches the stale limit.
 *              The control task calls this every pass; its own liveness
 *              needs no slot because making the call IS the proof.
 *         [FA] واچ‌داگ را تازه می‌کند اگر و فقط اگر همهٔ شکاف‌های موردانتظار
 *              حداکثر ‎BSP_IWDG_STALE_MS‎ پیش اعلام حضور کرده باشند. یک تسکِ
 *              کهنه کافی است تا kick نگه داشته شود و واچ‌داگ برد را ریست کند،
 *              و فلسفهٔ کار هم همین است: سیستمی که یک تسکش مرده، فقط چون تسک
 *              دیگری هنوز می‌چرخد سالم نیست. حساب سن در برابر سرریز امن است و
 *              شکافی که هرگز اعلام حضور نکرده (مهر صفر) نقش مهلت بوت را هم
 *              بازی می‌کند؛ تا وقتی خود ساعت به حد کهنگی برسد kick ادامه دارد.
 *              تسک کنترل هر پاس این را صدا می‌زند؛ زنده‌بودن خودش شکاف
 *              نمی‌خواهد چون همین صدازدن سندِ آن است.
 * @param  uint32_t__nowMs [EN] Current FreeRTOS tick in ms (the tick runs
 *                             at 1 kHz), free to wrap /
 *                             تیک فعلی فری‌آرتاس بر حسب میلی‌ثانیه (تیک
 *                             ۱ کیلوهرتز است) و سرریزش اشکالی ندارد
 */
void func__BspIwdg_PollKick(uint32_t uint32_t__nowMs)
{
#if (BSP_IWDG_PUMPER_EXISTS != 0u)
    uint32_t uint32_t__slot;

    /* [EN] u32 wrap-safe age: (now - last) works across the 49-day tick
       rollover. last = 0 (never checked in) also doubles as the boot grace:
       kicks flow until now reaches STALE_MS, by which time every created
       task has checked in many times over - a task that never runs vetoes
       from then on, which is the correct fail-safe.
       [FA] سنِ امن در برابر چرخش u32؛ مقدار ۰ (هرگز اعلام حضور) همان مهلت
       بوت هم هست: تا رسیدن now به STALE_MS همهٔ تسک‌ها بارها اعلام حضور
       کرده‌اند و تسکی که هرگز اجرا نشود از آن به بعد وتو می‌کند. */
    for (uint32_t__slot = 0u;
         uint32_t__slot < (uint32_t)BSP_IWDG_SLOT_COUNT;
         uint32_t__slot++)
    {
        if ((BSP_IWDG_EXPECT_MASK & ((uint32_t)1u << uint32_t__slot)) != 0u)
        {
            if ((uint32_t__nowMs -
                 UINT32_T__A__G__IwdgLastCheckInMs[uint32_t__slot]) >=
                BSP_IWDG_STALE_MS)
            {
                return;
            }
        }
    }

    IWDG->KR = BSP_IWDG_KEY_RELOAD;
#else
    (void)uint32_t__nowMs;
#endif
}
