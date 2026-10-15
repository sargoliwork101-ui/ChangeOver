/**
 * @file    diag.c
 * @brief   [EN] Diagnostic logger: charge time/count, 10 last faults, power events, min/max.
 *               Flash ping-pong at 0x0800F800 / 0x0800FC00, CRC32, debounced save.
 *          [FA] لاگر تشخیصی: زمان و تعداد شارژ، ۱۰ خطای آخر، رویدادهای برق، کمینه/بیشینه.
 *               فلش پینگ‌پنگ در ‎0x0800F800 / 0x0800FC00‎ با CRC32 و ذخیره دیبانس.
 */

/* ==================== Includes ==================== */
#include "diag.h"
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "modules_enable.h"
#if MODULE_CHARGER
#include "charger.h"
#endif
#include <string.h>

/* ==================== Defines ==================== */
#define DIAG_SAVE_DELAY_SEC           60u
#define DIAG_CRC32_POLY               0xEDB88320u

/* ==================== Static Variables ==================== */
static diag_record_t DIAG_RECORD_T__G__Active;
static diag_record_t DIAG_RECORD_T__G__Stage;
static bool BOOL__G__Dirty = false;
static uint32_t UINT32_T__G__SaveDelaySec = 0u;
static uint32_t UINT32_T__G__PowerOffTime = 0u;
static bool BOOL__G__PowerOffActive = false;

/* ==================== CRC32 ==================== */

/**
 * @brief  [EN] Compute CRC32 (IEEE) over data.
 *         [FA] محاسبه CRC32 روی داده.
 * @param  uint8_t__A__Data [EN] Data bytes / بایت‌ها
 * @param  uint32_t__len [EN] Length / طول
 * @return uint32_t [EN] CRC32 / مقدار
 */
static uint32_t func__Diag_Crc32(const uint8_t *uint8_t__A__Data, uint32_t uint32_t__len)
{
    uint32_t uint32_t__crc = 0xFFFFFFFFu;
    uint32_t uint32_t__i, uint32_t__j;
    for (uint32_t__i = 0u; uint32_t__i < uint32_t__len; uint32_t__i++)
    {
        uint32_t__crc ^= (uint32_t)uint8_t__A__Data[uint32_t__i];
        for (uint32_t__j = 0u; uint32_t__j < 8u; uint32_t__j++)
        {
            if ((uint32_t__crc & 1u) != 0u)
            {
                uint32_t__crc = (uint32_t__crc >> 1) ^ DIAG_CRC32_POLY;
            }
            else
            {
                uint32_t__crc >>= 1;
            }
        }
    }
    return ~uint32_t__crc;
}

/* ==================== Record Validate ==================== */

/**
 * @brief  [EN] Validate a flash record (magic, version, CRC).
 *         [FA] اعتبارسنجی رکورد فلش.
 * @param  diag_record_t__rec [EN] Record pointer / اشاره‌گر
 * @return bool [EN] true if valid / سالم
 */
static bool func__Diag_RecordValidate(const diag_record_t *diag_record_t__rec)
{
    uint32_t uint32_t__calcCrc;
    uint32_t uint32_t__len;

    if (diag_record_t__rec == NULL)
    {
        return false;
    }
    if (diag_record_t__rec->uint32_t__magic != DIAG_MAGIC)
    {
        return false;
    }
    if (diag_record_t__rec->uint16_t__version != DIAG_VERSION)
    {
        return false;
    }
    if (diag_record_t__rec->uint8_t__errorCount > DIAG_MAX_ERRORS)
    {
        return false;
    }
    if (diag_record_t__rec->uint8_t__powerEventCount > DIAG_MAX_POWER_EVENTS)
    {
        return false;
    }

    uint32_t__len = (uint32_t)((const uint8_t *)&diag_record_t__rec->uint32_t__crc32 - (const uint8_t *)diag_record_t__rec);
    uint32_t__calcCrc = func__Diag_Crc32((const uint8_t *)diag_record_t__rec, uint32_t__len);

    return (uint32_t__calcCrc == diag_record_t__rec->uint32_t__crc32);
}

/* ==================== Build Record ==================== */

/**
 * @brief  [EN] Build record with CRC.
 *         [FA] ساخت رکورد با CRC.
 * @param  diag_record_t__dst [EN] Destination / مقصد
 * @param  diag_record_t__src [EN] Source without CRC / مبدا بدون CRC
 */
static void func__Diag_BuildRecord(diag_record_t *diag_record_t__dst, const diag_record_t *diag_record_t__src)
{
    uint32_t uint32_t__len;
    (void)memcpy(diag_record_t__dst, diag_record_t__src, sizeof(diag_record_t));
    diag_record_t__dst->uint32_t__crc32 = 0u;
    uint32_t__len = (uint32_t)((uint8_t *)&diag_record_t__dst->uint32_t__crc32 - (uint8_t *)diag_record_t__dst);
    diag_record_t__dst->uint32_t__crc32 = func__Diag_Crc32((const uint8_t *)diag_record_t__dst, uint32_t__len);
}

/* ==================== Flash Helpers ==================== */

/**
 * @brief  [EN] Read record from flash address.
 *         [FA] خواندن رکورد از آدرس فلش.
 * @param  uint32_t__addr [EN] Flash address / آدرس فلش
 * @param  diag_record_t__rec [EN] Output / خروجی
 */
static void func__Diag_ReadFlash(uint32_t uint32_t__addr, diag_record_t *diag_record_t__rec)
{
    (void)memcpy(diag_record_t__rec, (const void *)uint32_t__addr, sizeof(diag_record_t));
}

/**
 * @brief  [EN] Write record to flash page (erase + program).
 *         [FA] نوشتن رکورد در صفحه فلش.
 * @param  uint32_t__addr [EN] Page address / آدرس صفحه
 * @param  diag_record_t__rec [EN] Record / رکورد
 * @return bool [EN] Success / موفقیت
 */
static bool func__Diag_WriteFlash(uint32_t uint32_t__addr, const diag_record_t *diag_record_t__rec)
{
    bool bool__ok;
    const uint16_t *uint16_t__ptr;
    uint32_t uint32_t__halfCount;

    bool__ok = func__BspFlash_ErasePage(uint32_t__addr);
    if (!bool__ok)
    {
        return false;
    }

    uint16_t__ptr = (const uint16_t *)diag_record_t__rec;
    uint32_t__halfCount = (uint32_t)(sizeof(diag_record_t) / 2u);

    return func__BspFlash_ProgramHalfWords(uint32_t__addr, uint16_t__ptr, uint32_t__halfCount);
}

/* ==================== Public Functions ==================== */

/* ==================== Diag Init ==================== */

/**
 * @brief  [EN] Load the newest valid diag record from the two flash pages.
 *              Page A is parked in the Active record when valid, then page B
 *              is judged in the Stage scratch; only the sequence numbers
 *              cross between the two reads as scalars, so no record copy
 *              ever sits on the caller stack (RAM/stack fix 2026-10-15: two
 *              static records used to cost ~1.4 KB of .bss for one boot read).
 *         [FA] تازه‌ترین رکورد سالم از دو صفحهٔ فلش بار می‌شود: نسخهٔ صفحهٔ
 *              ‎A‎ اگر سالم باشد در رکورد فعال پارک می‌شود و صفحهٔ ‎B‎ در
 *              بافر ‎Stage‎ داوری می‌شود؛ بین دو خواندن فقط شماره‌های ترتیب
 *              به‌صورت اسکالر می‌مانند و هیچ نسخهٔ رکوردی روی استک صدازننده
 *              نمی‌نشیند.
 * @return void [EN] None / ندارد
 */
void func__Diag_Init(void)
{
    bool bool__validA;
    bool bool__validB;
    bool bool__bWins;
    uint16_t uint16_t__seqA;
    uint16_t uint16_t__seqB;
    int16_t int16__seqDiff;

    (void)memset(&DIAG_RECORD_T__G__Active, 0, sizeof(diag_record_t));
    DIAG_RECORD_T__G__Active.uint32_t__magic = DIAG_MAGIC;
    DIAG_RECORD_T__G__Active.uint16_t__version = DIAG_VERSION;
    DIAG_RECORD_T__G__Active.uint32_t__minVinMv = 0xFFFFFFFFu;
    DIAG_RECORD_T__G__Active.uint32_t__minVpackMv = 0xFFFFFFFFu;
    DIAG_RECORD_T__G__Active.uint32_t__minVlowMv = 0xFFFFFFFFu;
    DIAG_RECORD_T__G__Active.uint32_t__minVhighMv = 0xFFFFFFFFu;

    /* [EN] Page A: validate in the Stage scratch, park a valid copy in
       Active and keep only its sequence number as a scalar.
       [FA] صفحهٔ ‎A‎: در ‎Stage‎ اعتبارسنجی می‌شود، نسخهٔ سالم در ‎Active‎
       پارک و فقط شمارهٔ ترتیبش به‌صورت اسکالر نگه داشته می‌شود. */
    func__Diag_ReadFlash(DIAG_PAGE_A_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validA = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqA = 0u;
    if (bool__validA)
    {
        uint16_t__seqA = DIAG_RECORD_T__G__Stage.uint16_t__seq;
        (void)memcpy(&DIAG_RECORD_T__G__Active, &DIAG_RECORD_T__G__Stage, sizeof(diag_record_t));
    }

    /* [EN] Page B: same scratch, same treatment.
       [FA] صفحهٔ ‎B‎: همان بافر کمکی و همان رفتار. */
    func__Diag_ReadFlash(DIAG_PAGE_B_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validB = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqB = 0u;
    if (bool__validB)
    {
        uint16_t__seqB = DIAG_RECORD_T__G__Stage.uint16_t__seq;
    }

    /* [EN] B takes over only when it alone is valid, or when both records
       are valid and B is not older than A (wrap-safe int16 compare, the
       same rule as before); otherwise the parked A (or the compiled
       defaults when neither page is valid) stays in Active.
       [FA] ‎B‎ فقط وقتی برنده است که تنها نسخهٔ سالم باشد، یا هر دو سالم
       باشند و ‎B‎ قدیمی‌تر از ‎A‎ نباشد؛ وگرنه ‎A‎ پارک‌شده (یا پیش‌فرض‌ها
       وقتی هیچ صفحه‌ای سالم نیست) در ‎Active‎ می‌ماند. */
    bool__bWins = false;
    if (bool__validB)
    {
        if (!bool__validA)
        {
            bool__bWins = true;
        }
        else
        {
            int16__seqDiff = (int16_t)(uint16_t__seqA - uint16_t__seqB);
            if (int16__seqDiff <= 0)
            {
                bool__bWins = true;
            }
        }
    }
    if (bool__bWins)
    {
        (void)memcpy(&DIAG_RECORD_T__G__Active, &DIAG_RECORD_T__G__Stage, sizeof(diag_record_t));
    }

    DIAG_RECORD_T__G__Active.uint32_t__uptimeSec = 0u;
    DIAG_RECORD_T__G__Active.uint32_t__resetCount++;
    BOOL__G__Dirty = true;
    UINT32_T__G__SaveDelaySec = 5u; /* save soon after boot */

    (void)memcpy(&DIAG_RECORD_T__G__Stage, &DIAG_RECORD_T__G__Active, sizeof(diag_record_t));
}

void func__Diag_Tick(void)
{
    DIAG_RECORD_T__G__Active.uint32_t__uptimeSec++;

    if (BOOL__G__Dirty)
    {
        if (UINT32_T__G__SaveDelaySec > 0u)
        {
            UINT32_T__G__SaveDelaySec--;
        }
        else
        {
            (void)func__Diag_SaveNow();
        }
    }
}

void func__Diag_SetBoardTime(uint32_t uint32_t__unixSec)
{
    if (uint32_t__unixSec < 1000000000u) return; /* sanity: after 2001 */

    uint32_t uint32_t__uptime = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
    if (uint32_t__unixSec >= uint32_t__uptime)
    {
        DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase = uint32_t__unixSec - uint32_t__uptime;
    }
    else
    {
        DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase = uint32_t__unixSec;
    }
    BOOL__G__Dirty = true;
    UINT32_T__G__SaveDelaySec = 2u;
}

uint32_t func__Diag_GetBoardTime(void)
{
    if (DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase == 0u)
    {
        return 0u;
    }
    return DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase + DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
}

void func__Diag_AddChargeSec(uint8_t uint8_t__channel, uint32_t uint32_t__sec)
{
    DIAG_RECORD_T__G__Active.uint32_t__totalChargeSec += uint32_t__sec;
    if (uint8_t__channel == 0u)
    {
        DIAG_RECORD_T__G__Active.uint32_t__chargeSecCh1 += uint32_t__sec;
    }
    else
    {
        DIAG_RECORD_T__G__Active.uint32_t__chargeSecCh2 += uint32_t__sec;
    }
    BOOL__G__Dirty = true;
    if (UINT32_T__G__SaveDelaySec == 0u)
    {
        UINT32_T__G__SaveDelaySec = DIAG_SAVE_DELAY_SEC;
    }
}

void func__Diag_RecordChargeComplete(uint8_t uint8_t__channel)
{
    DIAG_RECORD_T__G__Active.uint32_t__chargeCount++;
    if (uint8_t__channel == 0u)
    {
        DIAG_RECORD_T__G__Active.uint32_t__chargeCountCh1++;
    }
    else
    {
        DIAG_RECORD_T__G__Active.uint32_t__chargeCountCh2++;
    }
    BOOL__G__Dirty = true;
    UINT32_T__G__SaveDelaySec = 5u;
}

void func__Diag_RecordFault(fault_mask_t fault_mask_t__faults, const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    uint8_t uint8_t__idx;
    uint32_t uint32_t__now = func__Diag_GetBoardTime();
    if (uint32_t__now == 0u)
    {
        uint32_t__now = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec; /* fallback to uptime */
    }

    /* [EN] Do not log FAULT_NONE or transient */
    if (fault_mask_t__faults == 0u)
    {
        return;
    }

    /* [EN] Shift old errors, newest at 0 */
    for (uint8_t__idx = DIAG_MAX_ERRORS - 1u; uint8_t__idx > 0u; uint8_t__idx--)
    {
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[uint8_t__idx] =
            DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[uint8_t__idx - 1u];
    }

    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__timestamp = uint32_t__now;
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__faultMask = fault_mask_t__faults;
    if (measurement_snapshot_t__snap != NULL && measurement_snapshot_t__snap->valid)
    {
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vInMv = measurement_snapshot_t__snap->v_in_mv;
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vPackMv = measurement_snapshot_t__snap->v_bat24_mv;
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vLowMv = measurement_snapshot_t__snap->v_bat_low_mv;
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vHighMv = measurement_snapshot_t__snap->v_bat_high_mv;
    }
    else
    {
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vInMv = 0u;
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vPackMv = 0u;
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vLowMv = 0u;
        DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__vHighMv = 0u;
    }
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__state1 = 0u;
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__state2 = 0u;

    if (DIAG_RECORD_T__G__Active.uint8_t__errorCount < DIAG_MAX_ERRORS)
    {
        DIAG_RECORD_T__G__Active.uint8_t__errorCount++;
    }

    /* [EN] Update counters */
    if ((fault_mask_t__faults & (1u << 6)) != 0u) DIAG_RECORD_T__G__Active.uint32_t__batLostCount++;
    if ((fault_mask_t__faults & (1u << 7)) != 0u) DIAG_RECORD_T__G__Active.uint32_t__techFaultCount++;
    if ((fault_mask_t__faults & 1u) != 0u) DIAG_RECORD_T__G__Active.uint32_t__ovCount++; /* reuse for ADC as generic */

    BOOL__G__Dirty = true;
    UINT32_T__G__SaveDelaySec = 3u;
}

void func__Diag_RecordPowerOff(void)
{
    if (BOOL__G__PowerOffActive) return;
    BOOL__G__PowerOffActive = true;
    UINT32_T__G__PowerOffTime = func__Diag_GetBoardTime();
    if (UINT32_T__G__PowerOffTime == 0u)
    {
        UINT32_T__G__PowerOffTime = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
    }
}

void func__Diag_RecordPowerOn(void)
{
    uint32_t uint32_t__now;
    uint8_t uint8_t__idx;

    if (!BOOL__G__PowerOffActive) return;
    BOOL__G__PowerOffActive = false;

    uint32_t__now = func__Diag_GetBoardTime();
    if (uint32_t__now == 0u)
    {
        uint32_t__now = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
    }

    /* [EN] Shift power events */
    for (uint8_t__idx = DIAG_MAX_POWER_EVENTS - 1u; uint8_t__idx > 0u; uint8_t__idx--)
    {
        DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents[uint8_t__idx] =
            DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents[uint8_t__idx - 1u];
    }

    DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents[0].uint32_t__offTime = UINT32_T__G__PowerOffTime;
    DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents[0].uint32_t__onTime = uint32_t__now;
    if (uint32_t__now >= UINT32_T__G__PowerOffTime)
    {
        DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents[0].uint32_t__durationSec = uint32_t__now - UINT32_T__G__PowerOffTime;
    }
    else
    {
        DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents[0].uint32_t__durationSec = 0u;
    }

    if (DIAG_RECORD_T__G__Active.uint8_t__powerEventCount < DIAG_MAX_POWER_EVENTS)
    {
        DIAG_RECORD_T__G__Active.uint8_t__powerEventCount++;
    }

    BOOL__G__Dirty = true;
    UINT32_T__G__SaveDelaySec = 3u;
}

void func__Diag_UpdateMinMax(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    if (measurement_snapshot_t__snap == NULL || !measurement_snapshot_t__snap->valid) return;

    if (measurement_snapshot_t__snap->v_in_mv > DIAG_RECORD_T__G__Active.uint32_t__maxVinMv)
        DIAG_RECORD_T__G__Active.uint32_t__maxVinMv = measurement_snapshot_t__snap->v_in_mv;
    if (measurement_snapshot_t__snap->v_in_mv < DIAG_RECORD_T__G__Active.uint32_t__minVinMv)
        DIAG_RECORD_T__G__Active.uint32_t__minVinMv = measurement_snapshot_t__snap->v_in_mv;

    if (measurement_snapshot_t__snap->v_bat24_mv > DIAG_RECORD_T__G__Active.uint32_t__maxVpackMv)
        DIAG_RECORD_T__G__Active.uint32_t__maxVpackMv = measurement_snapshot_t__snap->v_bat24_mv;
    if (measurement_snapshot_t__snap->v_bat24_mv < DIAG_RECORD_T__G__Active.uint32_t__minVpackMv)
        DIAG_RECORD_T__G__Active.uint32_t__minVpackMv = measurement_snapshot_t__snap->v_bat24_mv;

    if (measurement_snapshot_t__snap->v_bat_low_mv > DIAG_RECORD_T__G__Active.uint32_t__maxVlowMv)
        DIAG_RECORD_T__G__Active.uint32_t__maxVlowMv = measurement_snapshot_t__snap->v_bat_low_mv;
    if (measurement_snapshot_t__snap->v_bat_low_mv < DIAG_RECORD_T__G__Active.uint32_t__minVlowMv)
        DIAG_RECORD_T__G__Active.uint32_t__minVlowMv = measurement_snapshot_t__snap->v_bat_low_mv;

    if (measurement_snapshot_t__snap->v_bat_high_mv > DIAG_RECORD_T__G__Active.uint32_t__maxVhighMv)
        DIAG_RECORD_T__G__Active.uint32_t__maxVhighMv = measurement_snapshot_t__snap->v_bat_high_mv;
    if (measurement_snapshot_t__snap->v_bat_high_mv < DIAG_RECORD_T__G__Active.uint32_t__minVhighMv)
        DIAG_RECORD_T__G__Active.uint32_t__minVhighMv = measurement_snapshot_t__snap->v_bat_high_mv;

    uint32_t uint32_t__imb = 0u;
    if (measurement_snapshot_t__snap->v_bat_low_mv > measurement_snapshot_t__snap->v_bat_high_mv)
        uint32_t__imb = measurement_snapshot_t__snap->v_bat_low_mv - measurement_snapshot_t__snap->v_bat_high_mv;
    else
        uint32_t__imb = measurement_snapshot_t__snap->v_bat_high_mv - measurement_snapshot_t__snap->v_bat_low_mv;

    if (uint32_t__imb > DIAG_RECORD_T__G__Active.uint32_t__maxImbMv)
        DIAG_RECORD_T__G__Active.uint32_t__maxImbMv = uint32_t__imb;

    if (measurement_snapshot_t__snap->i_ch1_ma > DIAG_RECORD_T__G__Active.uint32_t__maxIch1Ma)
        DIAG_RECORD_T__G__Active.uint32_t__maxIch1Ma = measurement_snapshot_t__snap->i_ch1_ma;
    if (measurement_snapshot_t__snap->i_ch2_ma > DIAG_RECORD_T__G__Active.uint32_t__maxIch2Ma)
        DIAG_RECORD_T__G__Active.uint32_t__maxIch2Ma = measurement_snapshot_t__snap->i_ch2_ma;
}

const diag_record_t *func__Diag_GetRecord(void)
{
    return &DIAG_RECORD_T__G__Active;
}

/* ==================== Diag SaveNow ==================== */

/**
 * @brief  [EN] Save the Active record to the older flash page (ping-pong).
 *              Both pages are judged one at a time in the Stage scratch and
 *              only their sequence numbers survive as scalars (RAM/stack
 *              fix 2026-10-15: three local records used to need ~2 KB of
 *              caller stack - more than the whole control-task stack).
 *              Runs only from the Diag tick context, so the scratch has a
 *              single owner. The flash write itself runs with the charger
 *              suspended and both PWM gates stopped (audit fix 2026-10-15):
 *              a page erase stalls the whole CPU for ~20..40 ms on this
 *              single-bank part, and the NVM/CalLut saves already forbid
 *              switching inside that blind window - this save now follows
 *              the same rule. SaveNow runs in the control task, the very
 *              context that owns charger evaluation, so StopAll acts at
 *              once (no polling) and the duty mirrors stay untouched; the
 *              next control pass resumes regulation seamlessly.
 *         [FA] نوشتن فلش با شارژرِ معلق و هر دو گیت PWM متوقف انجام می‌شود
 *              (اصلاح ممیزی ۲۰۲۶-۱۰-۱۵): پاک‌کردن صفحه روی این قطعهٔ تک‌بانک
 *              کل ‎CPU‎ را حدود ۲۰ تا ۴۰ میلی‌ثانیه نگه می‌دارد و
 *              ذخیره‌های ‎NVM/CalLut‎ همین حالا هم سوییچینگ داخل آن
 *              پنجرهٔ کور را ممنوع کرده‌اند - این ذخیره هم از همان قانون
 *              پیروی می‌کند. ‎SaveNow‎ در تسک کنترل اجرا می‌شود، یعنی
 *              همان زمینه‌ای که مالک ارزیابی شارژر است، پس ‎StopAll‎
 *              بی‌درنگ اثر می‌کند (بدون نظرسنجی) و آینه‌های duty دست‌نخورده
 *              می‌مانند؛ پاس بعدی کنترل، تنظیم را یکپارچه ادامه می‌دهد.
 *         [FA] رکورد فعال در صفحهٔ قدیمی‌تر فلش ذخیره می‌شود (پینگ‌پنگ).
 *              هر دو صفحه یکی‌یکی در بافر ‎Stage‎ داوری می‌شوند و فقط
 *              شماره‌های ترتیب به‌صورت اسکالر می‌مانند؛ قبلاً سه رکورد
 *              محلی حدود ۲KB از استک صدازننده می‌خواستند — بیشتر از کل
 *              استک تسک کنترل. فقط از زمینهٔ تیک Diag صدا زده می‌شود، پس
 *              بافر کمکی تک‌مالک است.
 * @return bool [EN] true when the flash write succeeded / موفقیت نوشتن فلش
 */
bool func__Diag_SaveNow(void)
{
    bool bool__ok;
    bool bool__validA;
    bool bool__validB;
    uint32_t uint32_t__targetAddr;
    uint16_t uint16_t__seqA;
    uint16_t uint16_t__seqB;
    uint16_t uint16_t__newSeq;
    int16_t int16__seqDiff;

    /* [EN] Judge page A in the Stage scratch; keep only its sequence.
       [FA] داوری صفحهٔ ‎A‎ در ‎Stage‎؛ فقط شمارهٔ ترتیب نگه داشته می‌شود. */
    func__Diag_ReadFlash(DIAG_PAGE_A_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validA = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqA = 0u;
    if (bool__validA)
    {
        uint16_t__seqA = DIAG_RECORD_T__G__Stage.uint16_t__seq;
    }

    /* [EN] Judge page B in the same scratch.
       [FA] داوری صفحهٔ ‎B‎ در همان بافر کمکی. */
    func__Diag_ReadFlash(DIAG_PAGE_B_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validB = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqB = 0u;
    if (bool__validB)
    {
        uint16_t__seqB = DIAG_RECORD_T__G__Stage.uint16_t__seq;
    }

    /* [EN] Decide the next sequence and the target page (the older one),
       with the exact same comparisons as before: plain greater-than for
       the sequence, wrap-safe int16 difference for the page choice.
       [FA] تعیین شمارهٔ ترتیب بعدی و صفحهٔ مقصد (همان قدیمی‌تر) با دقیقاً
       همان مقایسه‌های قبلی. */
    if (bool__validA && bool__validB)
    {
        if (uint16_t__seqA > uint16_t__seqB)
        {
            uint16_t__newSeq = (uint16_t)(uint16_t__seqA + 1u);
        }
        else
        {
            uint16_t__newSeq = (uint16_t)(uint16_t__seqB + 1u);
        }
        int16__seqDiff = (int16_t)(uint16_t__seqA - uint16_t__seqB);
        if (int16__seqDiff > 0)
        {
            uint32_t__targetAddr = DIAG_PAGE_B_ADDR;
        }
        else
        {
            uint32_t__targetAddr = DIAG_PAGE_A_ADDR;
        }
    }
    else if (bool__validA)
    {
        uint16_t__newSeq = (uint16_t)(uint16_t__seqA + 1u);
        uint32_t__targetAddr = DIAG_PAGE_B_ADDR;
    }
    else if (bool__validB)
    {
        uint16_t__newSeq = (uint16_t)(uint16_t__seqB + 1u);
        uint32_t__targetAddr = DIAG_PAGE_A_ADDR;
    }
    else
    {
        uint16_t__newSeq = 1u;
        uint32_t__targetAddr = DIAG_PAGE_A_ADDR;
    }

    /* [EN] Build the outgoing record in the scratch and write it; on
       success Active adopts the built copy (CRC included), as before.
       [FA] رکورد خروجی در بافر کمکی ساخته و نوشته می‌شود؛ در موفقیت،
       ‎Active‎ همان نسخهٔ ساخته‌شده را می‌گیرد. */
    DIAG_RECORD_T__G__Active.uint16_t__seq = uint16_t__newSeq;
    func__Diag_BuildRecord(&DIAG_RECORD_T__G__Stage, &DIAG_RECORD_T__G__Active);

    /* [EN] Blind-window rule (see the docstring above): suspend the
       charger and stop both gates for the erase+program stall, then let
       the next control pass resume. The suspend flag is always cleared
       right after the write, on success and on failure alike.
       [FA] قانون پنجرهٔ کور (توضیح بالای تابع): برای مدت استالِ پاک‌کردن
       و نوشتن، شارژر معلق و هر دو گیت متوقف می‌شوند و بعد پاس بعدی کنترل
       ادامه می‌دهد. پرچم تعلیق بلافاصله بعد از نوشتن، در موفقیت و شکست،
       پاک می‌شود. */
#if MODULE_CHARGER
    func__Charger_SetSuspended(true);
    func__BspPwm_StopAll();
#endif

    bool__ok = func__Diag_WriteFlash(uint32_t__targetAddr, &DIAG_RECORD_T__G__Stage);

#if MODULE_CHARGER
    func__Charger_SetSuspended(false);
#endif

    if (bool__ok)
    {
        BOOL__G__Dirty = false;
        UINT32_T__G__SaveDelaySec = 0u;
        (void)memcpy(&DIAG_RECORD_T__G__Active, &DIAG_RECORD_T__G__Stage, sizeof(diag_record_t));
    }
    return bool__ok;
}
