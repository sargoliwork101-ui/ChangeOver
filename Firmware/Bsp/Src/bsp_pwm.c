/**
 * @file    bsp_pwm.c
 * @brief   [EN] STM32F103C8T6 PWM port for schematic MCU_PWM1/MCU_PWM2.
 *          [FA] پورت PWM برای ‎MCU_PWM1/MCU_PWM2‎ شماتیک روی STM32F103C8T6.
 *
 * @note    [EN] The .ioc initializes TIM2_CH1 on PA0 and TIM3_CH1 on PA6.
 *              Both timers RUN CONTINUOUSLY from func__BspPwm_Init with a
 *              frozen half-period phase offset (10 us at 50 kHz): the two
 *              gate pulses rise exactly one half period apart, never
 *              switch simultaneously, and the interleave can never slip
 *              because the counters are never stopped or rewritten. A
 *              channel is switched off by compare=0 alone. Each timer
 *              also carries an INTERNAL CH2 sampling trigger for the
 *              synchronized current ADC: CH2 runs in PWM mode 2 with
 *              CCR2 = CCR1/2, so its rising edge lands exactly at the
 *              middle of the gate ON window. TIM2_CC2 and TIM3 TRGO
 *              (MMS=OC2REF) feed the ADC trigger inputs; the CH2 physical
 *              pins (PA1/PA7) stay in analog ADC mode.
 *          [FA] فایل ‎.ioc مقداردهی TIM2_CH1 روی PA0 و TIM3_CH1 روی PA6 را
 *              می‌کند. از Init هر دو تایمر پیوسته با آفست فاز ثابتِ
 *              نیم‌دوره (۱۰µs در ۵۰kHz) می‌چرخند: پالس گیت دوم دقیقاً
 *              نیم‌دوره بعد از اول بالا می‌آید، دو استیج هرگز همزمان
 *              سوییچ نمی‌کنند و چون شمارنده‌ها دیگر متوقف/بازنویسی نمی‌شوند
 *              این درهم‌گذاری نمی‌لغزد. خاموشی فقط با ‎compare=0‎. هر تایمر
 *              یک تریگر داخلی CH2 برای ADC سنکرون جریان دارد: CH2 با PWM
 *              mode 2 و ‎CCR2 = CCR1/2‎، پس لبه‌اش دقیقاً وسط پنجرهٔ ON است.
 *              TIM2_CC2 و ‎TIM3 TRGO (MMS=OC2REF)‎ به تریگر ADC می‌روند؛
 *              پایه‌های ‎CH2 (PA1/PA7)‎ آنالوگ می‌مانند.
 */

#include "bsp_pwm.h"
#include "main.h"

#include <stdbool.h>
#include <stddef.h>

/* ==================== Gate phase switch / کلید فاز گیت‌ها ==================== */
/* [EN] Gate phase compile switch. The 2026-09-24 bench experiment ran
   both gates IN PHASE (1u); the data showed no measurable crosstalk
   change, so the production interleave is restored. 0u (current) =
   production design: TIM3 preset to half a period (10 us at 50 kHz,
   ARR=1439) - the two gates never switch simultaneously and the input
   ripple stays staggered. 1u = both gates rise together (experiments
   only).
   [FA] کلید کامپایل فاز گیت‌ها: آزمایش بنچ ۲۰۲۶-۰۹-۲۴ هم‌فاز (1u) اثر
   محسوسی بر کراس‌تاک نشان نداد پس درهم‌گذاری تولید برگشت. 0u (فعلی) =
   طراحی تولید: TIM3 روی نیم‌دوره (۱۰µs در ۵۰kHz با ‎ARR=1439)‎ - دو گیت
   هرگز همزمان سوییچ نمی‌کنند. 1u = هم‌فاز (فقط آزمایش). */
#define BSP_PWM_TIM3_PHASE_OFFSET_IN_PHASE 0u

/* ==================== BspPwm_GetTimer ==================== */
/**
 * @brief  [EN] Resolve a logical channel to its board timer and channel.
 *         [FA] کانال منطقی را به تایمر و کانال فیزیکی برد تبدیل می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel /
 *                                     کانال منطقی
 * @param  TIM_HandleTypeDef__timer [EN] Output timer handle / هندل تایمر خروجی
 * @param  uint32_t__halChannel [EN] Output HAL channel identifier /
 *                                   شناسهٔ کانال HAL خروجی
 * @return bool [EN] true for a valid channel / برای کانال معتبر true درست
 */
static bool func__BspPwm_GetTimer(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   TIM_HandleTypeDef **TIM_HandleTypeDef__timer,
                                   uint32_t *uint32_t__halChannel)
{
    if ((TIM_HandleTypeDef__timer == NULL) ||
        (uint32_t__halChannel == NULL))
    {
        return false;
    }

    switch (bsp_pwm_channel_t__channel)
    {
        case BSP_PWM_CHARGER_1:
            *TIM_HandleTypeDef__timer = &htim2;
            *uint32_t__halChannel = TIM_CHANNEL_1;
            break;
        case BSP_PWM_CHARGER_2:
            *TIM_HandleTypeDef__timer = &htim3;
            *uint32_t__halChannel = TIM_CHANNEL_1;
            break;
        default:
            return false;
    }

    return true;
}

/* ==================== BspPwm_SetOneDuty ==================== */
/**
 * @brief  [EN] Apply one clamped duty to one timer channel as a compare
 *              value only - the counter is never touched (both counters
 *              run continuously from Init with the frozen half-period
 *              offset). permille=0 sets compare 0 and keeps that gate
 *              low. The internal CH2 sampling trigger of the same timer
 *              moves to the middle of the new ON window in the same call
 *              (CCR2 = CCR1/2).
 *         [FA] وظیفهٔ محدودشدهٔ یک کانال را فقط به‌صورت compare اعمال
 *              می‌کند - شمارنده دست نمی‌خورد (هر دو از Init پیوسته با آفست
 *              نیم‌دوره می‌چرخند). ‎permille=0‎ یعنی compare صفر و گیت پایین.
 *              تریگر داخلی CH2 هم در همین فراخوانی به وسط پنجرهٔ ON جدید
 *              می‌رود (‎CCR2 = CCR1/2)‎.
 * @param  TIM_HandleTypeDef__timer [EN] Board timer handle / هندل تایمر برد
 * @param  uint32_t__halChannel [EN] HAL channel / کانال HAL
 * @param  uint16_t__permille [EN] Duty in 0..1000 permille /
 *                                 وظیفه در بازهٔ ۰ تا ۱۰۰۰ پرمیل
 */
static void func__BspPwm_SetOneDuty(TIM_HandleTypeDef *TIM_HandleTypeDef__timer,
                                     uint32_t uint32_t__halChannel,
                                     uint16_t uint16_t__permille)
{
    uint32_t uint32_t__autoReload;
    uint32_t uint32_t__periodCounts;
    uint32_t uint32_t__compareCounts;

    if (TIM_HandleTypeDef__timer == NULL)
    {
        return;
    }

    if (uint16_t__permille > 1000u)
    {
        uint16_t__permille = 1000u;
    }

    uint32_t__autoReload = __HAL_TIM_GET_AUTORELOAD(TIM_HandleTypeDef__timer);
    uint32_t__periodCounts = uint32_t__autoReload + 1u;
    uint32_t__compareCounts =
        (uint32_t__periodCounts * (uint32_t)uint16_t__permille) / 1000u;

    if (uint32_t__compareCounts > uint32_t__autoReload)
    {
        uint32_t__compareCounts = uint32_t__autoReload;
    }

    __HAL_TIM_SET_COMPARE(TIM_HandleTypeDef__timer,
                          uint32_t__halChannel,
                          uint32_t__compareCounts);

    /* [EN] Keep the internal CH2 sampling trigger at the exact middle of
       the ON window: CCR2 = CCR1/2. PWM mode 2 makes the CH2 output rise
       at CNT = CCR2. The OC preload latches CH1 and CH2 at the same
       update event, so the half ratio is never observed split across two
       periods. compare=0 -> CCR2=0 -> CH2 stays high with no edge =
       "no synchronized sample" (gate off, primary current zero).
       [FA] تریگر CH2 را دقیقاً وسط پنجرهٔ ON نگه می‌دارد: ‎CCR2 = CCR1/2‎؛
       حالت PWM 2 خروجی CH2 را در ‎CNT = CCR2‎ بالا می‌آورد. پیش‌بارگذاری OC
       دو compare را در همان update قفل می‌کند پس نسبت نیم هرگز بین دو
       دوره شکسته دیده نمی‌شود. ‎compare=0 ->‎ بدون لبه = «نمونهٔ سنکرونی
       نیست» (گیت خاموش، جریان اولیه صفر). */
    __HAL_TIM_SET_COMPARE(TIM_HandleTypeDef__timer,
                          TIM_CHANNEL_2,
                          uint32_t__compareCounts / 2u);
}

/* ==================== BspPwm_InitSamplingPulse ==================== */
/**
 * @brief  [EN] Configure the internal CH2 of one charger timer as the
 *              synchronized-current sampling trigger: PWM mode 2,
 *              polarity high, compare 0 at boot. The output stage is
 *              enabled so the internal OC2 signal feeds the ADC trigger
 *              mux, but the physical CH2 pins (PA1/PA7) remain in analog
 *              ADC mode - no level ever reaches a pin. Must run while the
 *              counter is still halted, before func__BspPwm_Init starts
 *              both timers.
 *         [FA] کانال داخلی CH2 یک تایمر شارژر را به‌عنوان تریگر
 *              نمونه‌برداری سنکرون تنظیم می‌کند: PWM mode 2، قطبیت high،
 *              compare صفر در بوت. مرحلهٔ خروجی فعال می‌شود تا OC2 داخلی به
 *              مالتی‌پلکس تریگر ADC برسد، اما پایه‌های ‎CH2 (PA1/PA7)‎ آنالوگ
 *              می‌مانند. باید قبل از استارت تایمرها و وقتی شمارنده متوقف
 *              است اجرا شود.
 * @param  TIM_HandleTypeDef__timer [EN] Charger timer handle / هندل تایمر شارژر
 */
static void func__BspPwm_InitSamplingPulse(TIM_HandleTypeDef *TIM_HandleTypeDef__timer)
{
    TIM_OC_InitTypeDef TIM_OC_INITTYPEDEF__samplingPulse = {0};

    TIM_OC_INITTYPEDEF__samplingPulse.OCMode = TIM_OCMODE_PWM2;
    TIM_OC_INITTYPEDEF__samplingPulse.Pulse = 0u;
    TIM_OC_INITTYPEDEF__samplingPulse.OCPolarity = TIM_OCPOLARITY_HIGH;
    TIM_OC_INITTYPEDEF__samplingPulse.OCFastMode = TIM_OCFAST_DISABLE;

    (void)HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef__timer,
                                    &TIM_OC_INITTYPEDEF__samplingPulse,
                                    TIM_CHANNEL_2);

    TIM_CCxChannelCmd(TIM_HandleTypeDef__timer->Instance,
                      TIM_CHANNEL_2,
                      TIM_CCx_ENABLE);
}

/* ==================== BspPwm_Init ==================== */
/**
 * @brief  [EN] Force both gates low, preset the frozen half-period phase,
 *              then start BOTH counters with two adjacent raw register
 *              writes (htim2->CR1|=CEN / htim3->CR1|=CEN, a few bus
 *              cycles apart at 72 MHz). HAL_TIM_PWM_Start is deliberately
 *              NOT used: its per-call latency would slip the interleave
 *              by a nondeterministic amount. TIM3 gates rise
 *              deterministically 10 us (period/2 from the live ARR) after
 *              TIM2 gates. Output stages were enabled beforehand while
 *              both counters were still halted, so no glitch reaches the
 *              pins. After this the counters never stop: off = compare 0.
 *         [FA] ابتدا هر دو گیت پایین می‌آیند و فازِ ثابت نیم‌دوره تنظیم
 *              می‌شود، سپس هر دو شمارنده با دو نوشتن رجیستری پشت‌سرهم
 *              استارت می‌شوند (چند سیکل باس فاصله در ۷۲MHz). عمداً از
 *              HAL_TIM_PWM_Start استفاده نمی‌شود: تأخیر هر فراخوانی فاز را
 *              غیرقطعی جابه‌جا می‌کرد. پالس TIM3 دقیقاً ۱۰µs (نیم‌دوره از ARR
 *              واقعی) بعد از TIM2 می‌آید. خروجی‌ها از قبل و با شمارنده‌های
 *              متوقف فعال شدند تا کلک نرسد. پس‌ازاین شمارنده‌ها هرگز
 *              متوقف نمی‌شوند؛ خاموش یعنی compare صفر.
 */
void func__BspPwm_Init(void)
{
    uint32_t uint32_t__periodCounts;
    uint32_t uint32_t__tim3StartCounts;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);

    /* [EN] Enable the output stages while both counters are still halted
       [FA] مراحل خروجی را در حالت توقفِ شمارنده فعال می‌کنیم */
    TIM_CCxChannelCmd(htim2.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    TIM_CCxChannelCmd(htim3.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);

    /* [EN] Arm the internal CH2 sampling triggers (mid-ON edges) while the
       counters are still halted; func__BspPwm_SetOneDuty keeps CCR2 at
       CCR1/2 afterwards. TIM3 also mirrors OC2REF onto TRGO so the ADC can
       trigger on it (MMS=100b); TIM2_CC2 is fed to the ADC directly.
       [FA] تریگرهای داخلی CH2 (لبه‌های وسط ON) را در حالت توقفِ شمارنده
       مسلح می‌کنیم؛ بعد از این func__BspPwm_SetOneDuty مقدار CCR2 را روی
       ‎CCR1/2‎ نگه می‌دارد. TIM3 همچنین OC2REF را روی TRGO آینه می‌کند تا ADC
       بتواند روی آن تریگر شود (‎MMS=100b)‎؛ TIM2_CC2 مستقیم به ADC می‌رود. */
    func__BspPwm_InitSamplingPulse(&htim2);
    func__BspPwm_InitSamplingPulse(&htim3);
    MODIFY_REG(htim3.Instance->CR2, TIM_CR2_MMS, TIM_TRGO_OC2REF);

    uint32_t__periodCounts = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1u;
#if (BSP_PWM_TIM3_PHASE_OFFSET_IN_PHASE != 0u)
    /* [EN] Bench experiment (user order 2026-09-24): TIM3 also starts at
       0, so both gates rise together every period.
       [FA] آزمایش بنچ (دستور کاربر ۲۰۲۶-۰۹-۲۴): TIM3 هم از صفر شروع
       می‌شود تا هر دو گیت هر دوره با هم بالا بیایند. */
    uint32_t__tim3StartCounts = 0u;
#else
    /* [EN] Production: the frozen half-period interleave.
       [FA] تولید: درهم‌گذاری ثابت نیم‌دوره. */
    uint32_t__tim3StartCounts = uint32_t__periodCounts / 2u;
#endif
    __HAL_TIM_SET_COUNTER(&htim2, 0u);
    __HAL_TIM_SET_COUNTER(&htim3, uint32_t__tim3StartCounts);

    /* [EN] Deterministic simultaneous start pair (~tens of ns skew).
       [FA] جفت استارت همزمان قطعی (لغزش چند ده نانوثانیه). */
    htim2.Instance->CR1 |= TIM_CR1_CEN;
    htim3.Instance->CR1 |= TIM_CR1_CEN;
}

/* ==================== BspPwm_SetDutyPermille ==================== */
/**
 * @brief  [EN] Set one logical charger duty in 0..1000 permille.
 *         [FA] وظیفهٔ یک شارژر منطقی را در بازهٔ ۰ تا ۱۰۰۰ پرمیل تنظیم می‌کند.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel / کانال منطقی
 * @param  uint16_t__permille [EN] Duty in permille / وظیفه بر حسب پرمیل
 */
void func__BspPwm_SetDutyPermille(bsp_pwm_channel_t bsp_pwm_channel_t__channel,
                                   uint16_t uint16_t__permille)
{
    TIM_HandleTypeDef *TIM_HandleTypeDef__timer = NULL;
    uint32_t uint32_t__halChannel = 0u;

    if (func__BspPwm_GetTimer(bsp_pwm_channel_t__channel,
                              &TIM_HandleTypeDef__timer,
                              &uint32_t__halChannel) == true)
    {
        func__BspPwm_SetOneDuty(TIM_HandleTypeDef__timer,
                                uint32_t__halChannel,
                                uint16_t__permille);
    }
}

/* ==================== BspPwm_StopAll ==================== */
/**
 * @brief  [EN] Force both gates low (compare 0). The counters keep running
 *              on purpose, so the frozen 10 us interleave survives every
 *              SafeIdle/fault episode and is still correct when a channel
 *              returns.
 *         [FA] هر دو گیت را پایین می‌آورد (compare صفر). شمارنده‌ها عمداً به
 *              چرخش می‌مانند تا درهم‌گذاری ثابت ۱۰µs در تمام قسمت‌های
 *              SafeIdle و خطا حفظ شود و هنگام بازگشت کانال درست باقی بماند.
 */
void func__BspPwm_StopAll(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0u);

    /* [EN] Also park the CH2 sampling triggers: compare 0 leaves the PWM-2
       output constant high with no edge, so no synchronized current sample
       is triggered while the gates are off (the primary current is zero).
       [FA] تریگرهای CH2 هم پارک می‌شوند: compare صفر خروجی ‎PWM-2‎ را ثابت
       بالا نگه می‌دارد بدون هیچ لبه، پس وقتی گیت‌ها خاموش‌اند هیچ نمونهٔ
       سنکرون جریانی تریگر نمی‌شود (جریان اولیه صفر است). */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0u);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0u);
}

/* ==================== BspPwm_IsGatePulsing ==================== */
/**
 * @brief  [EN] Report whether one logical charger gate is currently pulsing
 *              (compare > 0). The synchronized current ADC uses this to know
 *              whether a mid-ON trigger edge will ever come; with the gate
 *              off the primary current is zero by definition.
 *         [FA] اعلام می‌کند گیت یک شارژر منطقی الان پالس می‌زند یا نه
 *              (‎compare > 0). ADC‎ سنکرون جریان با همین می‌فهمد آیا لبهٔ
 *              تریگر وسط ON می‌آید یا نه؛ با گیت خاموش، جریان اولیه بنا
 *              به تعریف صفر است.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel / کانال منطقی
 * @return bool [EN] true while that gate pulses / وقتی گیت پالس می‌زند true
 */
bool func__BspPwm_IsGatePulsing(bsp_pwm_channel_t bsp_pwm_channel_t__channel)
{
    TIM_HandleTypeDef *TIM_HandleTypeDef__timer = NULL;
    uint32_t uint32_t__halChannel = 0u;
    uint32_t uint32_t__compareCounts;

    if (func__BspPwm_GetTimer(bsp_pwm_channel_t__channel,
                              &TIM_HandleTypeDef__timer,
                              &uint32_t__halChannel) == false)
    {
        return false;
    }

    uint32_t__compareCounts =
        __HAL_TIM_GET_COMPARE(TIM_HandleTypeDef__timer, uint32_t__halChannel);

    return (uint32_t__compareCounts > 0u);
}

/* ==================== BspPwm_GetCompareCounts ==================== */
/**
 * @brief  [EN] Read the live CH1 compare (gate ON width) in timer ticks.
 *         [FA] مقدار زندهٔ compare ی CH1 (پهنای روشن گیت) بر حسب تیک تایمر.
 * @param  bsp_pwm_channel_t__channel [EN] Logical channel / کانال منطقی
 * @return uint32_t [EN] Compare counts, 0 for an invalid channel /
 *                      شمارش compare، صفر برای کانال نامعتبر
 */
uint32_t func__BspPwm_GetCompareCounts(bsp_pwm_channel_t bsp_pwm_channel_t__channel)
{
    TIM_HandleTypeDef *TIM_HandleTypeDef__timer = NULL;
    uint32_t uint32_t__halChannel = 0u;

    if (func__BspPwm_GetTimer(bsp_pwm_channel_t__channel,
                              &TIM_HandleTypeDef__timer,
                              &uint32_t__halChannel) == false)
    {
        return 0u;
    }

    return __HAL_TIM_GET_COMPARE(TIM_HandleTypeDef__timer, uint32_t__halChannel);
}
