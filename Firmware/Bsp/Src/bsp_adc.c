/**
 * @file    bsp_adc.c
 * @brief   [EN] ADC+DMA board layer. ADC1 is calibrated once, then hardware
 *              continuously fills a circular two-frame buffer. The CPU only
 *              polls DMA progress when a measurement snapshot is requested.
 *              Static RAM only; no malloc (MISRA / project memory rule).
 *              Since 2026-09-22 (user order) the two charge-current frame
 *              positions are NOT taken from the free-running scan anymore:
 *              ADC2 performs one hardware-triggered conversion per channel,
 *              synchronized by the PWM timers to the exact middle of the
 *              gate ON window, and those raw counts are spliced into the
 *              returned frame. If a synchronized sample cannot be taken
 *              (gate parked, timeout, ADC2 not ready), the asynchronous
 *              scan value of that position is kept as fallback.
 *          [FA] لایهٔ برد ADC+DMA. ADC1 یک‌بار کالیبره می‌شود و سپس سخت‌افزار
 *              بافر چرخشی دو فریمی را پیوسته پر می‌کند. CPU فقط هنگام درخواست
 *              snapshot پیشرفت DMA را می‌خواند. فقط RAM استاتیک؛ بدون malloc.
 *              از ۲۰۲۶-۰۹-۲۲ (دستور کاربر) دو جایگاه جریان شارژ از اسکن
 *              آزاد گرفته نمی‌شوند: ADC2 برای هر کانال یک تبدیل تریگر
 *              سخت‌افزاری انجام می‌دهد که تایمرهای PWM آن را دقیقاً وسط پنجرهٔ
 *              ON گیت سنکرون می‌کنند و همان شمارش خام داخل فریم برگشتی
 *              قرار می‌گیرد. اگر نمونهٔ سنکرون ممکن نشد (گیت پارک، timeout،
 *              آماده‌نبودن ADC2) مقدار اسکن غیرهمزمان همان جایگاه به‌عنوان
 *              جایگزین می‌ماند.
 *
 * @note    [EN] The raw buffer is volatile because DMA changes it outside the
 *              C execution flow. HAL receives its address as an integer-shaped
 *              pointer only; transfers are configured as halfwords in the MSP.
 *          [FA] بافر خام volatile است چون DMA خارج از جریان اجرای C آن را
 *              تغییر می‌دهد. HAL فقط آدرس آن را به‌شکل اشاره‌گر عددی می‌گیرد؛
 *              انتقال‌ها در MSP به‌صورت نصف‌واژه تنظیم شده‌اند.
 */

/* ==================== Includes ==================== */
#include "bsp_adc.h"
#include "bsp_pwm.h"
#include "main.h"

#include <stddef.h>

/* [EN] Board-port timeout for the first completed DMA half-frame.
 *      [FA] timeout مخصوص پورت برد برای نخستین نیم‌فریم کامل DMA. */
#define BSP_ADC_START_TIMEOUT_MS 2u

/* ==================== Synchronized current sampling / نمونه‌برداری سنکرون جریان ==================== */

/* [EN] Cycle budget for the bounded wait between arming the ADC2 external
 *      trigger and its end-of-conversion flag. One PWM period at 50 kHz is
 *      20 us; the next mid-ON edge therefore arrives within 20 us, the
 *      conversion itself takes 20 ADC clocks (7.5 sampling + 12.5
 *      conversion) = 1.67 us at the 12 MHz ADC clock. 30 us covers the
 *      worst case with margin; the wait runs with interrupts enabled so it
 *      only ever delays this one thread.
 * [FA] بودجهٔ سیکل برای انتظار محدود بین مسلح‌کردن تریگر خارجی ADC2 و
 *      پرچم پایان تبدیل. یک دورهٔ PWM در ۵۰kHz برابر ۲۰µs است؛ یعنی لبهٔ
 *      وسط ON حداکثر تا ۲۰µs بعد می‌آید و خود تبدیل ۲۰ کلاک ADC
 *      (۷٫۵ نمونه‌برداری + ۱۲٫۵ تبدیل) = ۱٫۶۷µs در کلاک ۱۲MHz طول می‌کشد.
 *      ۳۰µs بدترین حالت را با حاشیه پوشش می‌دهد؛ انتظار با وقفه‌های فعال
 *      اجرا می‌شود پس فقط همین تسک را کُند می‌کند. */
#define BSP_ADC_SYNC_TIMEOUT_US       30u
#define BSP_ADC_CPU_CYCLES_PER_US     72u

/* [EN] Minimum gate ON width (timer ticks at 72 MHz) worth a synchronized
 *      sample: 8 ticks = 1.1 us, just above the 625 ns ADC aperture plus
 *      LM358 settling. Narrower pulses take the asynchronous fallback.
 * [FA] کمترین پهنای روشن گیت (تیک تایمر در ۷۲MHz) که ارزش نمونهٔ سنکرون
 *      دارد: ۸ تیک = ۱٫۱µs، کمی بالای دهانهٔ ۶۲۵ns ی ADC به‌علاوهٔ نشست
 *      LM358. پالس‌های باریک‌تر جایگزین غیرهمزمان می‌گیرند. */
#define BSP_ADC_SYNC_MIN_COMPARE_COUNTS 8u

/* [EN] ADC2 sampling time for the current channels: 7.5 ADC clocks. The
 *      LM358 output behind the R41(1k)/R42(10k) MCU divider is a ~0.9 kOhm
 *      source, far below the datasheet Rmax for this sampling time at
 *      12 MHz, while the sampling aperture (625 ns) stays narrow enough to
 *      sit inside a mid-ON window.
 * [FA] زمان نمونه‌برداری ADC2 برای کانال‌های جریان: ۷٫۵ کلاک ADC. خروجی
 *      LM358 پشت تقسیم R41(1k)/R42(10k) منبعی حدود ۰٫۹kΩ است، خیلی زیر
 *      Rmax دیتاشیت برای این زمان نمونه‌برداری در ۱۲MHz، و دهانهٔ
 *      نمونه‌برداری (۶۲۵ns) آن‌قدر باریک می‌ماند که داخل پنجرهٔ وسط ON
 *      جا شود. */
#define BSP_ADC_CURRENT_SAMPLE_TIME   ADC_SAMPLETIME_7CYCLES_5

/* ==================== Static State ==================== */

/* [EN] Board ADC handle is private to this STM32 BSP implementation.
 *      [FA] هندل ADC برد فقط در پیاده‌سازی BSP مخصوص STM32 خصوصی است. */
static ADC_HandleTypeDef *ADC_HANDLETYPEDEF__G__Hadc = NULL;

/* [EN] Private ADC2 backend for the PWM-synchronized current samples. It is
 *      initialized by this board port only (clock enable + HAL init +
 *      calibration); the CubeMX project knows nothing about it. ADC2 has no
 *      DMA on this family and none is needed: one conversion per channel
 *      per measurement pass, EOC polled with a cycle-budgeted wait.
 * [FA] بک‌اند خصوصی ADC2 برای نمونه‌های سنکرون جریان با PWM. فقط همین پورت
 *      برد آن را مقداردهی می‌کند (کلاک + HAL init + کالیبراسیون) و پروژهٔ
 *      CubeMX چیزی از آن نمی‌داند. ADC2 در این خانواده DMA ندارد و لازم
 *      نیست: هر پاس اندازه‌گیری یک تبدیل برای هر کانال و EOC با انتظارِ
 *      بودجهٔ سیکلی poll می‌شود. */
static ADC_HandleTypeDef ADC_HANDLETYPEDEF__G__HadcSync = {0};

/* [EN] Set when the ADC2 sync backend passed init + calibration.
 *      [FA] وقتی بک‌اند سنکرون ADC2 از init + کالیبراسیون عبور کند true. */
static bool BOOL__G__SyncReady = false;

/* [EN] Hardware-filled circular DMA buffer: 2 frames x 5 channels.
 *      [FA] بافر چرخشی پرشدهٔ سخت‌افزاری: ۲ فریم x ۵ کانال. */
static volatile uint16_t UINT16_T__G__DmaBuffer[BSP_ADC_DMA_SAMPLE_COUNT];

/* [EN] Set when calibration and DMA start succeed.
 *      [FA] وقتی کالیبراسیون و شروع DMA موفق باشد true می‌شود. */
static bool BOOL__G__Running = false;

/* ==================== BspAdc_EnableCycleCounter ==================== */
/**
 * @brief  [EN] Enable the Cortex-M3 DWT cycle counter once; it is the
 *              cycle-budget reference of the bounded sync-conversion wait.
 *              Board-port private detail, invisible to every module.
 *         [FA] شمارندهٔ سیکل DWT کورتکس-M3 را یک‌بار فعال می‌کند؛ مرجع
 *              بودجهٔ سیکلی برای انتظار محدود تبدیل سنکرون. جزئیات خصوصی
 *              پورت برد و برای ماژول‌ها نامرئی است.
 */
static void func__BspAdc_EnableCycleCounter(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ==================== BspAdc_SampleCurrentSync ==================== */
/**
 * @brief  [EN] Take ONE PWM-synchronized raw sample of one charge-current
 *              channel with ADC2: select the channel and its timer trigger
 *              (TIM2_CC2 for charger 1, TIM3_TRGO for charger 2), arm the
 *              external trigger, then wait - with a cycle budget of
 *              BSP_ADC_SYNC_TIMEOUT_US - for the hardware edge that sits at
 *              the exact middle of that gate's ON window and the following
 *              end-of-conversion. Returns false WITHOUT waiting when the
 *              gate is parked (compare 0): the primary current is zero then.
 *              Interrupts stay enabled during the short wait, so only this
 *              thread is ever delayed.
 *         [FA] یک نمونهٔ خام سنکرون با PWM از یک کانال جریان شارژ با ADC2
 *              می‌گیرد: کانال و تریگر تایمرش را انتخاب می‌کند (TIM2_CC2 برای
 *              شارژر ۱ و TIM3_TRGO برای شارژر ۲)، تریگر خارجی را مسلح و بعد
 *              با بودجهٔ BSP_ADC_SYNC_TIMEOUT_US منتظر لبهٔ سخت‌افزاری وسط
 *              پنجرهٔ ON همان گیت و پایان تبدیلی که بعدش می‌آید می‌ماند.
 *              اگر گیت پارک باشد (compare صفر) بدون انتظار false برمی‌گرداند
 *              چون جریان اولیه صفر است. در انتظار کوتاه وقفه‌ها فعال می‌مانند
 *              پس فقط همین تسک تأخیر می‌گیرد.
 * @param  bsp_pwm_channel_t__channel [EN] Logical charger channel 1 or 2 /
 *                                     کانال منطقی شارژر ۱ یا ۲
 * @param  uint16_t__counts [EN] Output raw ADC count of the mid-ON sample /
 *                               خروجی شمارش خام ADC نمونهٔ وسط ON
 * @return bool [EN] true when a synchronized sample was captured /
 *                   اگر نمونهٔ سنکرون گرفته شد true
 */
static bool func__BspAdc_SampleCurrentSync(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                           uint16_t *uint16_t__counts)
{
    uint32_t uint32_t__triggerSelection;
    uint32_t uint32_t__adcChannel;
    uint32_t uint32_t__waitStartCycles;
    uint32_t uint32_t__waitBudgetCycles;
    ADC_HandleTypeDef *ADC_HANDLETYPEDEF__hadcSync = &ADC_HANDLETYPEDEF__G__HadcSync;

    if ((BOOL__G__SyncReady == false) || (uint16_t__counts == NULL))
    {
        return false;
    }

    /* [EN] A parked gate (compare 0) never raises a mid-ON edge, and with
       the MOSFET off the primary current is zero by definition - report
       "no synchronized sample" immediately, the caller falls back to the
       asynchronous frame value (which is ~0 too).
       [FA] گیت پارک (compare صفر) هرگز لبهٔ وسط ON نمی‌سازد و با MOSFET
       خاموش جریان اولیه بنا به تعریف صفر است - بلافاصله «بدون نمونهٔ
       سنکرون» اعلام می‌شود و فراخوان به مقدار فریم غیرهمزمان برمی‌گردد
       (که آن هم حدود صفر است). */
    if (func__BspPwm_IsGatePulsing(bsp_pwm_channel_t__channel) == false)
    {
        return false;
    }

    /* [EN] Runt-pulse veto (full-program audit 2026-09-26): with compare
       1..7 the ON window (<= 1 us) is narrower than the ADC aperture
       (625 ns) plus the LM358 settling, and CCR2 = CCR1/2 lands on
       tick 0..3 - i.e. on the switching edge / its ringing, not mid-ON.
       The asynchronous scan fallback (~0 at such a duty) is the honest
       value, so report "no synchronized sample" like a parked gate.
       [FA] رد پالس کوتاه (ممیزی کل برنامه): با compare ۱..۷ پنجرهٔ ON
       (حداکثر ۱µs) از دهانهٔ ADC (۶۲۵ns) به‌علاوهٔ نشست LM358 باریک‌تر
       است و CCR2 = CCR1/2 روی تیک ۰..۳ می‌نشیند - یعنی روی لبهٔ سوییچ و
       رینگش، نه وسط ON. جایگزین اسکن غیرهمزمان (حدود صفر در چنین
       duty ای) مقدار درست است، پس مثل گیت پارک «بدون نمونهٔ سنکرون»
       اعلام می‌شود. */
    if (func__BspPwm_GetCompareCounts(bsp_pwm_channel_t__channel) <
        BSP_ADC_SYNC_MIN_COMPARE_COUNTS)
    {
        return false;
    }

    if (bsp_pwm_channel_t__channel == BSP_PWM_CHARGER_1)
    {
        uint32_t__triggerSelection = ADC_EXTERNALTRIGCONV_T2_CC2;
        uint32_t__adcChannel = ADC_CHANNEL_1;
    }
    else
    {
        uint32_t__triggerSelection = ADC_EXTERNALTRIGCONV_T3_TRGO;
        uint32_t__adcChannel = ADC_CHANNEL_7;
    }

    /* [EN] One regular conversion of the selected channel on the selected
       timer trigger. EXTSEL/EXTTRG live in CR2 on this family; arming is a
       single EXTTRG set so the next mid-ON edge starts the conversion. The
       EOC flag is cleared with an explicit 32-bit mask: a stray late
       conversion from a previous timeout must not read as a fresh sample.
       [FA] یک تبدیل regular از کانال انتخابی روی تریگر تایمر انتخابی.
       EXTSEL/EXTTRG در CR2 این خانواده‌اند؛ مسلح‌کردن فقط یک EXTTRG است تا
       لبهٔ وسط ON بعدی تبدیل را شروع کند. پرچم EOC با ماسک ۳۲بیتی صریح پاک
       می‌شود: تبدیل دیرهنگامِ جا مانده از timeout قبلی نباید نمونهٔ تازه
       حساب شود. */
    MODIFY_REG(ADC_HANDLETYPEDEF__hadcSync->Instance->CR2,
               ADC_CR2_EXTSEL,
               uint32_t__triggerSelection);
    MODIFY_REG(ADC_HANDLETYPEDEF__hadcSync->Instance->SQR3,
               ADC_SQR3_SQ1,
               uint32_t__adcChannel << ADC_SQR3_SQ1_Pos);

    CLEAR_BIT(ADC_HANDLETYPEDEF__hadcSync->Instance->SR,
              (uint32_t)ADC_FLAG_EOC);
    SET_BIT(ADC_HANDLETYPEDEF__hadcSync->Instance->CR2, ADC_CR2_EXTTRIG);
    /* [EN] Bounded wait for end of conversion (interrupts stay enabled, so
       this delays only the current thread). The DWT cycle budget protects
       against a lost trigger edge (for example a duty change racing the
       arm); on timeout the trigger is disarmed and false is returned.
       [FA] انتظار محدود برای پایان تبدیل (وقفه‌ها فعال می‌مانند پس فقط
       تسک فعلی تأخیر می‌گیرد). بودجهٔ سیکلی DWT در برابر لبهٔ گم‌شدهٔ
       تریگر (مثلاً تغییر duty که با مسلح‌شدن مسابقه می‌کند) محافظت می‌کند؛
       در timeout تریگر خلع و false برگردانده می‌شود. */
    uint32_t__waitStartCycles = DWT->CYCCNT;
    uint32_t__waitBudgetCycles =
        BSP_ADC_SYNC_TIMEOUT_US * BSP_ADC_CPU_CYCLES_PER_US;

    while (__HAL_ADC_GET_FLAG(ADC_HANDLETYPEDEF__hadcSync, ADC_FLAG_EOC) == RESET)
    {
        if ((DWT->CYCCNT - uint32_t__waitStartCycles) > uint32_t__waitBudgetCycles)
        {
            CLEAR_BIT(ADC_HANDLETYPEDEF__hadcSync->Instance->CR2, ADC_CR2_EXTTRIG);
            return false;
        }
    }

    /* [EN] Reading DR clears EOC on this family; disarm the trigger so no
       further edge starts an unwanted conversion.
       [FA] خواندن DR در این خانواده EOC را پاک می‌کند؛ تریگر خلع شود تا
       لبهٔ بعدی تبدیل ناخواسته شروع نکند. */
    *uint16_t__counts =
        (uint16_t)(ADC_HANDLETYPEDEF__hadcSync->Instance->DR & ADC_DR_DATA_Msk);
    CLEAR_BIT(ADC_HANDLETYPEDEF__hadcSync->Instance->CR2, ADC_CR2_EXTTRIG);

    return true;
}

/* ==================== BspAdc_Init ==================== */

/**
 * @brief  [EN] Select the current board ADC handle and clear the DMA buffer.
 *         [FA] هندل ADC برد فعلی را انتخاب و بافر DMA را صفر می‌کند.
 */
void func__BspAdc_Init(void)
{
    uint32_t uint32_t__i;

    ADC_HANDLETYPEDEF__G__Hadc = &hadc1;
    BOOL__G__Running = false;
    BOOL__G__SyncReady = false;

    for (uint32_t__i = 0u; uint32_t__i < BSP_ADC_DMA_SAMPLE_COUNT; uint32_t__i++)
    {
        UINT16_T__G__DmaBuffer[uint32_t__i] = 0u;
    }
}

/* ==================== BspAdc_StartSyncBackend ==================== */
/**
 * @brief  [EN] Bring up the private ADC2 synchronized-current backend: enable
 *              its clock, initialize it for ONE externally-triggered regular
 *              conversion (trigger selected per sample: TIM2_CC2 or
 *              TIM3_TRGO), set a short sampling time for the current pins
 *              and run the F1 calibration once. The shared ADC clock
 *              prescaler (PCLK2/6 = 12 MHz) is already configured by the
 *              generated ADC1 init; PA1/PA7 are already analog. ADC2 stays
 *              enabled (ADON) afterwards - arming is only the EXTTRG bit.
 *         [FA] بک‌اند خصوصی ADC2 برای نمونه‌های سنکرون جریان را بالا می‌آورد:
 *              کلاکش را فعال می‌کند، برای «یک» تبدیل regular تریگر-خارجی تنظیم
 *              می‌کند (تریگر برای هر نمونه انتخاب می‌شود: TIM2_CC2 یا
 *              TIM3_TRGO)، زمان نمونه‌برداری کوتاه برای پایه‌های جریان می‌گذارد
 *              و کالیبراسیون F1 را یک‌بار اجرا می‌کند. پیش‌تقسیم‌کنندهٔ مشترک
 *              کلاک ADC (PCLK2/6 = 12MHz) از قبل در init تولیدشدهٔ ADC1 تنظیم
 *              شده؛ PA1/PA7 هم از قبل آنالوگ‌اند. ADC2 بعد از این روشن (ADON)
 *              می‌ماند - مسلح‌کردن فقط بیت EXTTRG است.
 * @return bool [EN] true when ADC2 is initialized and calibrated /
 *                   اگر ADC2 مقداردهی و کالیبره شد true
 */
static bool func__BspAdc_StartSyncBackend(void)
{
    ADC_HandleTypeDef *ADC_HANDLETYPEDEF__hadcSync = &ADC_HANDLETYPEDEF__G__HadcSync;

    __HAL_RCC_ADC2_CLK_ENABLE();

    ADC_HANDLETYPEDEF__hadcSync->Instance = ADC2;
    ADC_HANDLETYPEDEF__hadcSync->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    ADC_HANDLETYPEDEF__hadcSync->Init.ScanConvMode = ADC_SCAN_DISABLE;
    ADC_HANDLETYPEDEF__hadcSync->Init.ContinuousConvMode = DISABLE;
    ADC_HANDLETYPEDEF__hadcSync->Init.DiscontinuousConvMode = DISABLE;
    ADC_HANDLETYPEDEF__hadcSync->Init.NbrOfDiscConversion = 1u;
    ADC_HANDLETYPEDEF__hadcSync->Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_CC2;
    ADC_HANDLETYPEDEF__hadcSync->Init.NbrOfConversion = 1u;

    if (HAL_ADC_Init(ADC_HANDLETYPEDEF__hadcSync) != HAL_OK)
    {
        return false;
    }

    /* [EN] Short sampling time for the current inputs IN1/IN7 (both live in
       SMPR2 on this family): keeps the sampling aperture narrow around the
       mid-ON instant while staying far inside the datasheet
       source-resistance limit.
       [FA] زمان نمونه‌برداری کوتاه برای ورودی‌های جریان IN1/IN7 (هر دو در
       SMPR2 این خانواده): دهانهٔ نمونه‌برداری را حول لحظهٔ وسط ON باریک نگه
       می‌دارد و هنوز خیلی داخل حد مقاومت منبع دیتاشیت است. */
    /* [EN] MODIFY_REG's set-mask is NOT shifted for us: the raw sample-time
       value must be moved to the SMPx field position, otherwise the field
       stays 0 (1.5 cycles - too fast for the ~0.9 kOhm LM358 source) and a
       stray bit lands in SMP0 (full-program audit 2026-09-26).
       [FA] ماسکِ set در MODIFY_REG خودکار شیفت نمی‌خورد: مقدار زمان
       نمونه‌برداری باید به موقعیت فیلد SMPx منتقل شود، وگرنه فیلد صفر
       می‌ماند (۱٫۵ سیکل - زیادی سریع برای منبع ۰٫۹kΩ) و یک بیت اضافه در
       SMP0 می‌نشیند (ممیزی کل برنامه). */
    MODIFY_REG(ADC_HANDLETYPEDEF__hadcSync->Instance->SMPR2,
               ADC_SMPR2_SMP1,
               ((uint32_t)BSP_ADC_CURRENT_SAMPLE_TIME << ADC_SMPR2_SMP1_Pos));
    MODIFY_REG(ADC_HANDLETYPEDEF__hadcSync->Instance->SMPR2,
               ADC_SMPR2_SMP7,
               ((uint32_t)BSP_ADC_CURRENT_SAMPLE_TIME << ADC_SMPR2_SMP7_Pos));

    if (HAL_ADCEx_Calibration_Start(ADC_HANDLETYPEDEF__hadcSync) != HAL_OK)
    {
        return false;
    }

    return true;
}

/* ==================== BspAdc_Start ==================== */

/**
 * @brief  [EN] Calibrate ADC1 and start continuous scan with circular DMA;
 *              then bring up the ADC2 synchronized-current backend and the
 *              DWT cycle counter used by its bounded wait.
 *         [FA] ADC1 را کالیبره و اسکن مداوم با DMA چرخشی را شروع می‌کند؛
 *              سپس بک‌اند ADC2 برای جریان سنکرون و شمارندهٔ سیکل DWT را که
 *              انتظار محدودش از آن استفاده می‌کند بالا می‌آورد.
 * @return bool [EN] true when calibration and HAL start succeed / اگر هر دو
 *                   کالیبراسیون و شروع HAL موفق باشند true
 */
bool func__BspAdc_Start(void)
{
    uint32_t uint32_t__dmaStartTick;
    uint32_t uint32_t__dmaCounter;

    BOOL__G__Running = false;

    if ((ADC_HANDLETYPEDEF__G__Hadc == NULL) ||
        (ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle == NULL))
    {
        return false;
    }

    /* [EN] F1 calibration must run while the ADC is not converting.
       [FA] کالیبراسیون F1 باید زمانی اجرا شود که ADC در حال تبدیل نیست. */
    if (HAL_ADCEx_Calibration_Start(ADC_HANDLETYPEDEF__G__Hadc) != HAL_OK)
    {
        return false;
    }

    /* [EN] F1 HAL accepts uint32_t* but the MSP configures halfword DMA. The
       integer-shaped cast passes the buffer address; DMA writes uint16_t items.
       [FA] HAL در F1 پارامتر uint32_t* می‌گیرد، اما MSP DMA را نصف‌واژه تنظیم
       کرده است. cast فقط آدرس بافر را می‌دهد و DMA آیتم‌های uint16_t می‌نویسد. */
    if (HAL_ADC_Start_DMA(ADC_HANDLETYPEDEF__G__Hadc,
                          (uint32_t *)(uintptr_t)&UINT16_T__G__DmaBuffer[0],
                          BSP_ADC_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        return false;
    }

    /* [EN] HAL_ADC_Start_DMA internally uses HAL_DMA_Start_IT(). Polling is
       intentional here, so suppress the DMA interrupt sources after start.
       [FA] HAL_ADC_Start_DMA در داخل از HAL_DMA_Start_IT استفاده می‌کند.
       اینجا عمداً polling داریم، پس منابع وقفهٔ DMA بعد از start خاموش می‌شوند. */
    __HAL_DMA_DISABLE_IT(ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle,
                         DMA_IT_TC | DMA_IT_HT | DMA_IT_TE);

    /* [EN] Do not expose a running reader until the first half has completed.
       This bounded startup wait prevents the initial zero-filled second half
       from being reported as a valid frame. Normal wait is about 28 us at
       12 MHz; the timeout only handles a stalled peripheral.
       [FA] تا کامل‌شدن نیمهٔ اول، خواننده را فعال اعلام نمی‌کند. این انتظار
       محدود مانع می‌شود نیمهٔ دومِ اولیه و صفرشده فریم معتبر گزارش شود.
       زمان عادی در 12MHz حدود 28us است و timeout فقط خرابی peripheral را
       پوشش می‌دهد. */
    uint32_t__dmaStartTick = HAL_GetTick();
    do
    {
        uint32_t__dmaCounter = __HAL_DMA_GET_COUNTER(ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle);
    }
    while ((uint32_t__dmaCounter > BSP_ADC_CHANNEL_COUNT) &&
           ((HAL_GetTick() - uint32_t__dmaStartTick) < BSP_ADC_START_TIMEOUT_MS));

    if (uint32_t__dmaCounter > BSP_ADC_CHANNEL_COUNT)
    {
        (void)HAL_ADC_Stop_DMA(ADC_HANDLETYPEDEF__G__Hadc);
        return false;
    }

    /* [EN] The voltage scan is running; now arm the synchronized-current
       backend. A failure here only degrades the current positions to the
       asynchronous scan values - the frame reader contract survives.
       [FA] اسکن ولتاژ راه افتاد؛ حالا بک‌اند جریان سنکرون مسلح می‌شود.
       خطای اینجا فقط جایگاه‌های جریان را به مقادیر اسکن غیرهمزمان تنزل
       می‌دهد - قرارداد خوانندهٔ فریم زنده می‌ماند. */
    if (func__BspAdc_StartSyncBackend() == true)
    {
        func__BspAdc_EnableCycleCounter();
        BOOL__G__SyncReady = true;
    }

    BOOL__G__Running = true;
    return true;
}

/* ==================== BspAdc_IsFrameReady ==================== */

/**
 * @brief  [EN] Report whether calibration and DMA start completed successfully.
 *         [FA] اعلام می‌کند کالیبراسیون و شروع DMA با موفقیت کامل شده است.
 * @return bool [EN] true after successful start / بعد از شروع موفق true
 */
bool func__BspAdc_IsFrameReady(void)
{
    return BOOL__G__Running;
}

/* ==================== BspAdc_GetRaw ==================== */

/**
 * @brief  [EN] Copy the newest completed five-sample frame. DMA CNDTR selects
 *              the half not being written; the counter is checked before and
 *              after the copy so a moving half-buffer boundary is rejected.
 *              After the stable voltage copy, the two charge-current
 *              positions are replaced by fresh PWM mid-ON synchronized
 *              ADC2 samples (user order 2026-09-22): each conversion is
 *              started by the hardware edge at the exact middle of that
 *              gate's ON window, the furthest point from both stages'
 *              switching edges and their ringing. When a synchronized
 *              sample cannot be captured (gate parked, lost edge, ADC2 not
 *              ready), the asynchronous scan value of that position stays
 *              in place as the fallback.
 *         [FA] جدیدترین فریم کامل پنج‌نمونه‌ای را کپی می‌کند. CNDTR DMA نیمه‌ای
 *              را که در حال نوشتن نیست انتخاب می‌کند؛ شمارنده قبل و بعد بررسی
 *              می‌شود تا مرز متحرک نیمه باعث کپی ناپایدار نشود. بعد از کپی
 *              پایدار ولتاژها، دو جایگاه جریان شارژ با نمونه‌های تازهٔ سنکرون
 *              وسط ON پالس PWM از ADC2 جایگزین می‌شوند (دستور کاربر
 *              ۲۰۲۶-۰۹-۲۲): هر تبدیل را لبهٔ سخت‌افزاریِ دقیقاً وسط پنجرهٔ ON
 *              همان گیت شروع می‌کند - دورترین نقطه از لبه‌های سوییچ و رینگ
 *              هر دو استیج. اگر نمونهٔ سنکرون گرفته نشود (گیت پارک، لبهٔ
 *              گم‌شده، آماده‌نبودن ADC2) مقدار اسکن غیرهمزمان همان جایگاه
 *              به‌عنوان جایگزین باقی می‌ماند.
 * @param  uint16_t__out [EN] Output array with BSP_ADC_CHANNEL_COUNT elements /
 *                            آرایهٔ خروجی با تعداد کانال‌ها
 * @return bool [EN] true when a stable frame was copied / اگر فریم پایدار کپی شد
 */
bool func__BspAdc_GetRaw(uint16_t uint16_t__out[BSP_ADC_CHANNEL_COUNT])
{
    DMA_HandleTypeDef *DMA_HANDLETYPEDEF__dmaHandle;
    uint16_t uint16_t__syncCounts;
    uint32_t uint32_t__attempt;
    uint32_t uint32_t__i;
    uint32_t uint32_t__dmaCounterBefore;
    uint32_t uint32_t__dmaCounterAfter;
    uint32_t uint32_t__sourceOffset;
    uint32_t uint32_t__savedPrimask;
    bool bool__stableWindow;

    if ((BOOL__G__Running == false) ||
        (uint16_t__out == NULL) ||
        (ADC_HANDLETYPEDEF__G__Hadc == NULL) ||
        (ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle == NULL))
    {
        return false;
    }

    DMA_HANDLETYPEDEF__dmaHandle = ADC_HANDLETYPEDEF__G__Hadc->DMA_Handle;

    for (uint32_t__attempt = 0u;
         uint32_t__attempt < BSP_ADC_DMA_RETRY_COUNT;
         uint32_t__attempt++)
    {
        uint32_t__dmaCounterBefore = __HAL_DMA_GET_COUNTER(DMA_HANDLETYPEDEF__dmaHandle);

        if ((uint32_t__dmaCounterBefore == 0u) ||
            (uint32_t__dmaCounterBefore > BSP_ADC_DMA_SAMPLE_COUNT))
        {
            continue;
        }

        /* [EN] CNDTR > 5 means DMA writes the first half, so the second half
           is complete. CNDTR <= 5 means the first half is complete.
           [FA] اگر CNDTR بزرگ‌تر از ۵ باشد DMA در نیمهٔ اول می‌نویسد، پس
           نیمهٔ دوم کامل است. اگر CNDTR <= ۵ باشد نیمهٔ اول کامل است. */
        if (uint32_t__dmaCounterBefore > BSP_ADC_CHANNEL_COUNT)
        {
            uint32_t__sourceOffset = BSP_ADC_CHANNEL_COUNT;
        }
        else
        {
            uint32_t__sourceOffset = 0u;
        }

        uint32_t__savedPrimask = __get_PRIMASK();
        __disable_irq();

        for (uint32_t__i = 0u;
             uint32_t__i < BSP_ADC_CHANNEL_COUNT;
             uint32_t__i++)
        {
            uint16_t__out[uint32_t__i] =
                UINT16_T__G__DmaBuffer[uint32_t__sourceOffset + uint32_t__i];
        }

        uint32_t__dmaCounterAfter = __HAL_DMA_GET_COUNTER(DMA_HANDLETYPEDEF__dmaHandle);
        __set_PRIMASK(uint32_t__savedPrimask);

        bool__stableWindow =
            ((uint32_t__dmaCounterBefore > BSP_ADC_CHANNEL_COUNT) ==
             (uint32_t__dmaCounterAfter > BSP_ADC_CHANNEL_COUNT));

        if (bool__stableWindow &&
            (uint32_t__dmaCounterAfter != 0u) &&
            (uint32_t__dmaCounterAfter <= BSP_ADC_DMA_SAMPLE_COUNT))
        {
            /* [EN] Splice the synchronized mid-ON current samples over the
               asynchronous scan values. This runs OUTSIDE the PRIMASK
               critical section on purpose: each sample may legally wait up
               to one PWM period (20 us) for its trigger edge, and
               interrupts must keep flowing during that wait.
               [FA] نمونه‌های سنکرون وسط ON را جای مقادیر اسکن غیرهمزمان
               می‌گذارد. این کار عمداً بیرون از بخش بحرانی PRIMASK اجرا
               می‌شود: هر نمونه ممکن است قانوناً تا یک دورهٔ PWM (۲۰µs) برای
               لبهٔ تریگرش منتظر بماند و وقفه‌ها باید در این انتظار جریان
               داشته باشند. */
            if (func__BspAdc_SampleCurrentSync(BSP_PWM_CHARGER_1,
                                               &uint16_t__syncCounts) == true)
            {
                uint16_t__out[BSP_ADC_CHANNEL_CURRENT1] = uint16_t__syncCounts;
            }

            if (func__BspAdc_SampleCurrentSync(BSP_PWM_CHARGER_2,
                                               &uint16_t__syncCounts) == true)
            {
                uint16_t__out[BSP_ADC_CHANNEL_CURRENT2] = uint16_t__syncCounts;
            }

            return true;
        }
    }

    return false;
}
