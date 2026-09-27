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
