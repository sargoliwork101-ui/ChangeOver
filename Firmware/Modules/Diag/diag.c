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

void func__Diag_Init(void)
{
    diag_record_t diag_record_t__recA;
    diag_record_t diag_record_t__recB;
    bool bool__validA, bool__validB;

    (void)memset(&DIAG_RECORD_T__G__Active, 0, sizeof(diag_record_t));
    DIAG_RECORD_T__G__Active.uint32_t__magic = DIAG_MAGIC;
    DIAG_RECORD_T__G__Active.uint16_t__version = DIAG_VERSION;
    DIAG_RECORD_T__G__Active.uint32_t__minVinMv = 0xFFFFFFFFu;
    DIAG_RECORD_T__G__Active.uint32_t__minVpackMv = 0xFFFFFFFFu;
    DIAG_RECORD_T__G__Active.uint32_t__minVlowMv = 0xFFFFFFFFu;
    DIAG_RECORD_T__G__Active.uint32_t__minVhighMv = 0xFFFFFFFFu;

    func__Diag_ReadFlash(DIAG_PAGE_A_ADDR, &diag_record_t__recA);
    func__Diag_ReadFlash(DIAG_PAGE_B_ADDR, &diag_record_t__recB);

    bool__validA = func__Diag_RecordValidate(&diag_record_t__recA);
    bool__validB = func__Diag_RecordValidate(&diag_record_t__recB);

    if (bool__validA && bool__validB)
    {
        /* [EN] Newest seq wins, wrap-safe compare (int16) */
        int16_t int16__diff = (int16_t)(diag_record_t__recA.uint16_t__seq - diag_record_t__recB.uint16_t__seq);
        if (int16__diff > 0)
        {
            (void)memcpy(&DIAG_RECORD_T__G__Active, &diag_record_t__recA, sizeof(diag_record_t));
        }
        else
        {
            (void)memcpy(&DIAG_RECORD_T__G__Active, &diag_record_t__recB, sizeof(diag_record_t));
        }
    }
    else if (bool__validA)
    {
        (void)memcpy(&DIAG_RECORD_T__G__Active, &diag_record_t__recA, sizeof(diag_record_t));
    }
    else if (bool__validB)
    {
        (void)memcpy(&DIAG_RECORD_T__G__Active, &diag_record_t__recB, sizeof(diag_record_t));
    }
    else
    {
        /* [EN] No valid record, keep defaults */
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

bool func__Diag_SaveNow(void)
{
    diag_record_t diag_record_t__new;
    bool bool__ok;
    uint32_t uint32_t__targetAddr;
    diag_record_t diag_record_t__readA, diag_record_t__readB;
    bool bool__validA, bool__validB;
    uint16_t uint16_t__newSeq = 0u;

    /* [EN] Determine newest seq */
    func__Diag_ReadFlash(DIAG_PAGE_A_ADDR, &diag_record_t__readA);
    func__Diag_ReadFlash(DIAG_PAGE_B_ADDR, &diag_record_t__readB);
    bool__validA = func__Diag_RecordValidate(&diag_record_t__readA);
    bool__validB = func__Diag_RecordValidate(&diag_record_t__readB);
    if (bool__validA && bool__validB)
    {
        uint16_t__newSeq = (diag_record_t__readA.uint16_t__seq > diag_record_t__readB.uint16_t__seq) ?
            (uint16_t)(diag_record_t__readA.uint16_t__seq + 1u) : (uint16_t)(diag_record_t__readB.uint16_t__seq + 1u);
        /* [EN] Pick the older page as target (ping-pong) */
        int16_t int16__diff = (int16_t)(diag_record_t__readA.uint16_t__seq - diag_record_t__readB.uint16_t__seq);
        uint32_t__targetAddr = (int16__diff > 0) ? DIAG_PAGE_B_ADDR : DIAG_PAGE_A_ADDR;
    }
    else if (bool__validA)
    {
        uint16_t__newSeq = (uint16_t)(diag_record_t__readA.uint16_t__seq + 1u);
        uint32_t__targetAddr = DIAG_PAGE_B_ADDR;
    }
    else if (bool__validB)
    {
        uint16_t__newSeq = (uint16_t)(diag_record_t__readB.uint16_t__seq + 1u);
        uint32_t__targetAddr = DIAG_PAGE_A_ADDR;
    }
    else
    {
        uint16_t__newSeq = 1u;
        uint32_t__targetAddr = DIAG_PAGE_A_ADDR;
    }

    DIAG_RECORD_T__G__Active.uint16_t__seq = uint16_t__newSeq;
    func__Diag_BuildRecord(&diag_record_t__new, &DIAG_RECORD_T__G__Active);

    bool__ok = func__Diag_WriteFlash(uint32_t__targetAddr, &diag_record_t__new);
    if (bool__ok)
    {
        BOOL__G__Dirty = false;
        UINT32_T__G__SaveDelaySec = 0u;
        (void)memcpy(&DIAG_RECORD_T__G__Active, &diag_record_t__new, sizeof(diag_record_t));
    }
    return bool__ok;
}
