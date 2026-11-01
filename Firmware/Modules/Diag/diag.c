/**
 * @file    diag.c
 * @brief   [EN] Diagnostic logger: charge time/count, 10 last faults, power events, min/max.
 *               Flash ping-pong at 0x0800F800 / 0x0800FC00, CRC32, debounced save.
 */
/* ==================== Includes ==================== */
#include "diag.h"
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "cmsis_os2.h"
#include "modules_enable.h"
#if MODULE_CHARGER
#include "charger.h"
#endif
#include <string.h>
/* ==================== Defines ==================== */
#define DIAG_SAVE_DELAY_SEC           60u
/*
 * [EN] v1.113 (user order 2026-10-28: "there must be a save while charging
 * too"): while a channel keeps charging, the record used to be saved only
 * when the charge stopped, so a mains cut in the middle of a long charge
 * threw away every charge second since it started. The record is now
 * written during the charge as well, at most once per this many charge
 * seconds: losing at most ten minutes instead of the whole session.
 */
#define DIAG_SAVE_CHARGE_SEC          600u
/*
 * [EN] A gap longer than this between two charge calls means the channel was
 * NOT charging in between (the caller only calls while it charges), so the
 * idle time is not charge time. Two seconds because the caller comes with
 * the control period, a few milliseconds apart.
 */
#define DIAG_CHARGE_GAP_SEC           2u
#define DIAG_CRC32_POLY               0xEDB88320u
/* ==================== Static Variables ==================== */
static diag_record_t DIAG_RECORD_T__G__Active;
static diag_record_t DIAG_RECORD_T__G__Stage;
static bool BOOL__G__Dirty = false;
static uint32_t UINT32_T__G__SaveDelaySec = 0u;
static uint32_t UINT32_T__G__PowerOffTime = 0u;
static bool BOOL__G__PowerOffActive = false;
/*
 * [EN] v1.113 time base (audit 2026-10-28): uptime used to be the NUMBER of
 * control passes, one "second" every hundred of them. A pass that costs
 * more than its 10 ms delay therefore made the board's second longer than
 * a real second, and the panel anchors its wall clock on this uptime
 * (HELLO_ACK), so the recovered clock ran slow. Uptime now comes from the
 * kernel tick counter, which cannot drift with the workload.
 */
static uint32_t UINT32_T__G__KernelUptimeLastTicks = 0u;
static uint32_t UINT32_T__G__KernelUptimeWrapSec = 0u;
/*
 * [EN] Charge accounting by DELTA of real seconds, per channel and for the
 * total. A first call only takes the starting point, so a start in the
 * middle of a second can never add a whole second that did not happen.
 */
static bool     BOOL__G__ChargeBaseValid = false;
static uint32_t UINT32_T__G__LastChargeUptimeSec[2] = { 0u, 0u };
static uint32_t UINT32_T__G__LastTotalUptimeSec = 0u;
static uint32_t UINT32_T__G__ChargeSecSinceSave = 0u;
/* ==================== CRC32 ==================== */
/**
 * @brief  [EN] Compute CRC32 (IEEE) over data.
* @param uint8_t__A__Data Data bytes /
* @param uint32_t__len Length /
* @return uint32_t CRC32 /
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
* @param diag_record_t__rec Record pointer /
* @return bool true if valid /
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
* @param diag_record_t__dst Destination /
* @param diag_record_t__src Source without CRC / CRC
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
* @param uint32_t__addr Flash address /
* @param diag_record_t__rec Output /
 */
static void func__Diag_ReadFlash(uint32_t uint32_t__addr, diag_record_t *diag_record_t__rec)
{
    (void)memcpy(diag_record_t__rec, (const void *)uint32_t__addr, sizeof(diag_record_t));
}
/* ==================== Diag_WriteFlash ==================== */
/**
 * @brief  [EN] Write record to flash page (erase + program).
* @param uint32_t__addr Page address /
* @param diag_record_t__rec Record /
* @return bool Success /
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
* @return void None /
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
    /*
 * [EN] Page A: validate in the Stage scratch, park a valid copy in
 * Active and keep only its sequence number as a scalar.
 */
    func__Diag_ReadFlash(DIAG_PAGE_A_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validA = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqA = 0u;
    if (bool__validA)
    {
        uint16_t__seqA = DIAG_RECORD_T__G__Stage.uint16_t__seq;
        (void)memcpy(&DIAG_RECORD_T__G__Active, &DIAG_RECORD_T__G__Stage, sizeof(diag_record_t));
    }
    /*
 * [EN] Page B: same scratch, same treatment.
 */
    func__Diag_ReadFlash(DIAG_PAGE_B_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validB = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqB = 0u;
    if (bool__validB)
    {
        uint16_t__seqB = DIAG_RECORD_T__G__Stage.uint16_t__seq;
    }
    /*
 * [EN] B takes over only when it alone is valid, or when both records
 * are valid and B is not older than A (wrap-safe int16 compare, the
 * same rule as before); otherwise the parked A (or the compiled
 * defaults when neither page is valid) stays in Active.
 */
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
    BOOL__G__Dirty = false;
    UINT32_T__G__SaveDelaySec = 0u;
    /*
 * [EN] v1.113: the charge accounting starts from the first call, so a boot
 * in the middle of a second adds nothing that did not happen.
 */
    BOOL__G__ChargeBaseValid = false;
    UINT32_T__G__LastChargeUptimeSec[0] = 0u;
    UINT32_T__G__LastChargeUptimeSec[1] = 0u;
    UINT32_T__G__LastTotalUptimeSec = 0u;
    UINT32_T__G__ChargeSecSinceSave = 0u;
    UINT32_T__G__KernelUptimeLastTicks = osKernelGetTickCount();
    UINT32_T__G__KernelUptimeWrapSec = 0u;
    (void)memcpy(&DIAG_RECORD_T__G__Stage, &DIAG_RECORD_T__G__Active, sizeof(diag_record_t));
}
/* ==================== Diag_UptimeSeconds ==================== */
/**
 * @brief  [EN] Seconds this board has really been running, read from the
 *              kernel tick counter instead of counting control passes.
 *              The 32-bit tick counter wraps after ~49.7 days at 1 kHz, so a
 *              step backwards is carried into a separate second counter:
 *              uptime stays monotonic for the life of the product.
* @return uptime in seconds /
 */
static uint32_t func__Diag_UptimeSeconds(void)
{
    uint32_t uint32_t__tickFrequency = osKernelGetTickFreq();
    uint32_t uint32_t__ticks = osKernelGetTickCount();
    if (uint32_t__tickFrequency == 0u)
    {
        /*
 * [EN] A dead kernel clock must not freeze the uptime at zero: keep the
 * value that is already known.
 */
        return DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
    }
    if (uint32_t__ticks < UINT32_T__G__KernelUptimeLastTicks)
    {
        UINT32_T__G__KernelUptimeWrapSec +=
            (0xFFFFFFFFu / uint32_t__tickFrequency) + 1u;
    }
    UINT32_T__G__KernelUptimeLastTicks = uint32_t__ticks;
    return UINT32_T__G__KernelUptimeWrapSec +
           (uint32_t__ticks / uint32_t__tickFrequency);
}
/* ==================== Diag_IsCharging ==================== */
/**
 * @brief  [EN] Is at least one channel charging right now? Read straight from
 *              the charger instead of guessing from the call pattern, so the
 *              save policy follows the real state even when a call is missed.
* @return true when a channel is active /
 */
static bool func__Diag_IsCharging(void)
{
#if MODULE_CHARGER
    if (func__Charger_IsChannelActive(0u) != false)
    {
        return true;
    }
    if (func__Charger_IsChannelActive(1u) != false)
    {
        return true;
    }
#endif
    return false;
}
/* ==================== Diag_SaveDue ==================== */
/**
 * @brief  [EN] May the pending change go to the flash now?
 *              Outside a charge the 60 s debounce already decided. Inside a
 *              charge the flash is spared until DIAG_SAVE_CHARGE_SEC of charge
 *              has piled up, because a charge second changes a counter every
 *              single second and the flash does not deserve 3600 writes a day.
* @return true when a save may run /
 */
static bool func__Diag_SaveDue(void)
{
    if (func__Diag_IsCharging() == false)
    {
        return true;
    }
    return (UINT32_T__G__ChargeSecSinceSave >= DIAG_SAVE_CHARGE_SEC);
}
/* ==================== Diag_Tick ==================== */
void func__Diag_Tick(void)
{
    DIAG_RECORD_T__G__Active.uint32_t__uptimeSec = func__Diag_UptimeSeconds();
    if (BOOL__G__Dirty)
    {
        if (UINT32_T__G__SaveDelaySec > 0u)
        {
            UINT32_T__G__SaveDelaySec--;
        }
        else if (func__Diag_SaveDue() != false)
        {
            (void)func__Diag_SaveNow();
        }
        else
        {
            /*
 * [EN] charging: the change waits for the next charge-save slot.
 */
        }
    }
}
/* ==================== Diag_SetBoardTime ==================== */
void func__Diag_SetBoardTime(uint32_t uint32_t__unixSec)
{
    int32_t int32_t__lock;
    uint32_t uint32_t__uptime;
    uint32_t uint32_t__newBase;
    uint32_t uint32_t__oldBase;
    uint32_t uint32_t__diff;
    if (uint32_t__unixSec < 1000000000u) return; /* sanity: after 2001 */
    int32_t__lock = osKernelLock();
    uint32_t__uptime = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
    if (uint32_t__unixSec >= uint32_t__uptime)
    {
        uint32_t__newBase = uint32_t__unixSec - uint32_t__uptime;
    }
    else
    {
        uint32_t__newBase = uint32_t__unixSec;
    }
    uint32_t__oldBase = DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase;
    uint32_t__diff = (uint32_t__newBase >= uint32_t__oldBase)
                         ? (uint32_t__newBase - uint32_t__oldBase)
                         : (uint32_t__oldBase - uint32_t__newBase);
    DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase = uint32_t__newBase;
    if ((uint32_t__oldBase == 0u) || (uint32_t__diff >= 300u))
    {
        BOOL__G__Dirty = true;
        if ((UINT32_T__G__SaveDelaySec == 0u) || (UINT32_T__G__SaveDelaySec > 2u))
        {
            UINT32_T__G__SaveDelaySec = 2u;
        }
    }
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
/* ==================== Diag_GetBoardTime ==================== */
uint32_t func__Diag_GetBoardTime(void)
{
    if (DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase == 0u)
    {
        return 0u;
    }
    return DIAG_RECORD_T__G__Active.uint32_t__boardTimeBase + DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
}
/* ==================== Diag_AddChargeSec ==================== */
void func__Diag_AddChargeSec(uint8_t uint8_t__channel)
{
    uint32_t uint32_t__uptimeSec = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec;
    uint32_t uint32_t__deltaSec;
    uint8_t  uint8_t__index = (uint8_t__channel == 0u) ? 0u : 1u;
    int32_t  int32_t__lock;
    /*
 * [EN] Nothing has moved since the last call: no lock, no work. Both of
 * these live in the control task, so the plain read cannot race.
 */
    if ((BOOL__G__ChargeBaseValid != false) &&
        (UINT32_T__G__LastChargeUptimeSec[uint8_t__index] == uint32_t__uptimeSec) &&
        (UINT32_T__G__LastTotalUptimeSec == uint32_t__uptimeSec))
    {
        return;
    }
    int32_t__lock = osKernelLock();
    /*
 * [EN] First call after boot: only remember where time started.
 */
    if (BOOL__G__ChargeBaseValid == false)
    {
        UINT32_T__G__LastChargeUptimeSec[0] = uint32_t__uptimeSec;
        UINT32_T__G__LastChargeUptimeSec[1] = uint32_t__uptimeSec;
        UINT32_T__G__LastTotalUptimeSec = uint32_t__uptimeSec;
        BOOL__G__ChargeBaseValid = true;
        if (int32_t__lock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__lock);
        }
        return;
    }
    /*
 * [EN] This channel's share: the real seconds that passed since its own
 * last call, whatever the call rate was.
 */
    uint32_t__deltaSec = uint32_t__uptimeSec - UINT32_T__G__LastChargeUptimeSec[uint8_t__index];
    /*
 * [EN] A long gap is not charge time: the channel was idle, and this call
 * opens a new charge session at this very second.
 */
    if (uint32_t__deltaSec > DIAG_CHARGE_GAP_SEC)
    {
        uint32_t__deltaSec = 0u;
        UINT32_T__G__LastChargeUptimeSec[uint8_t__index] = uint32_t__uptimeSec;
    }
    if (uint32_t__deltaSec != 0u)
    {
        if (uint8_t__index == 0u)
        {
            DIAG_RECORD_T__G__Active.uint32_t__chargeSecCh1 += uint32_t__deltaSec;
        }
        else
        {
            DIAG_RECORD_T__G__Active.uint32_t__chargeSecCh2 += uint32_t__deltaSec;
        }
        UINT32_T__G__LastChargeUptimeSec[uint8_t__index] = uint32_t__uptimeSec;
    }
    /*
 * [EN] The total counts each wall second ONCE, even when both channels
 * charge in that same second (user order: total charge time is the
 * time the product spent charging, not the sum of the channels).
 */
    if (uint32_t__uptimeSec != UINT32_T__G__LastTotalUptimeSec)
    {
        uint32_t uint32_t__totalDeltaSec =
            uint32_t__uptimeSec - UINT32_T__G__LastTotalUptimeSec;
        if (uint32_t__totalDeltaSec > DIAG_CHARGE_GAP_SEC)
        {
            uint32_t__totalDeltaSec = 0u;
        }
        DIAG_RECORD_T__G__Active.uint32_t__totalChargeSec += uint32_t__totalDeltaSec;
        UINT32_T__G__LastTotalUptimeSec = uint32_t__uptimeSec;
        if (UINT32_T__G__ChargeSecSinceSave < 0xFFFFFFFFu)
        {
            UINT32_T__G__ChargeSecSinceSave += uint32_t__totalDeltaSec;
        }
    }
    /*
 * [EN] v1.113: the 60 s debounce is armed ONCE, when a fresh change
 * arrives after a save (Dirty was false). It is never re-armed while
 * the change keeps coming - that constant re-arming was exactly what
 * kept the record out of the flash for a whole charge.
 */
    if (BOOL__G__Dirty == false)
    {
        UINT32_T__G__SaveDelaySec = DIAG_SAVE_DELAY_SEC;
    }
    BOOL__G__Dirty = true;
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
/* ==================== Diag_RecordChargeComplete ==================== */
void func__Diag_RecordChargeComplete(uint8_t uint8_t__channel)
{
    int32_t int32_t__lock = osKernelLock();
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
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
/* ==================== Diag_RecordFault ==================== */
void func__Diag_RecordFault(fault_mask_t fault_mask_t__faults, const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    int32_t int32_t__lock;
    uint8_t uint8_t__idx;
    uint32_t uint32_t__now;
    /* [EN] Do not log FAULT_NONE or transient */
    if (fault_mask_t__faults == 0u)
    {
        return;
    }
    int32_t__lock = osKernelLock();
    uint32_t__now = func__Diag_GetBoardTime();
    if (uint32_t__now == 0u)
    {
        uint32_t__now = DIAG_RECORD_T__G__Active.uint32_t__uptimeSec; /* fallback to uptime */
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
#if MODULE_CHARGER
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__state1 =
        UINT32_T__G__ChargerDiag[0u * CHG_DIAG_CHANNEL_STRIDE + CHG_DIAG_IDX_STATE];
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__state2 =
        UINT32_T__G__ChargerDiag[1u * CHG_DIAG_CHANNEL_STRIDE + CHG_DIAG_IDX_STATE];
#else
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__state1 = 0u;
    DIAG_RECORD_T__G__Active.diag_error_t__A__Errors[0].uint32_t__state2 = 0u;
#endif
    if (DIAG_RECORD_T__G__Active.uint8_t__errorCount < DIAG_MAX_ERRORS)
    {
        DIAG_RECORD_T__G__Active.uint8_t__errorCount++;
    }
    /*
 * [EN] Update counters: decode by FAULT_* name from app_types.h, never by
 * raw bit number, so a bit renumber moves both sides together.
 * uint32_t__ovCount counts FAULT_ADC; the field name is legacy and stays,
 * because the stored record layout must not change.
 */
    if ((fault_mask_t__faults & FAULT_CHARGER_BAT_LOST) != 0u) DIAG_RECORD_T__G__Active.uint32_t__batLostCount++;
    if ((fault_mask_t__faults & FAULT_CHARGER_TECHNICAL) != 0u) DIAG_RECORD_T__G__Active.uint32_t__techFaultCount++;
    if ((fault_mask_t__faults & FAULT_ADC) != 0u) DIAG_RECORD_T__G__Active.uint32_t__ovCount++;
    BOOL__G__Dirty = true;
    UINT32_T__G__SaveDelaySec = 3u;
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
/* ==================== Diag_RecordPowerOff ==================== */
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
/* ==================== Diag_RecordPowerOn ==================== */
void func__Diag_RecordPowerOn(void)
{
    int32_t int32_t__lock;
    uint32_t uint32_t__now;
    uint8_t uint8_t__idx;
    if (!BOOL__G__PowerOffActive) return;
    BOOL__G__PowerOffActive = false;
    int32_t__lock = osKernelLock();
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
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
/* ==================== Diag_ClearHistory ==================== */
void func__Diag_ClearHistory(void)
{
    int32_t int32_t__lock = osKernelLock();
    bool bool__hadHistory =
        ((DIAG_RECORD_T__G__Active.uint8_t__errorCount != 0u) ||
         (DIAG_RECORD_T__G__Active.uint8_t__powerEventCount != 0u));
    DIAG_RECORD_T__G__Active.uint8_t__errorCount = 0u;
    DIAG_RECORD_T__G__Active.uint8_t__powerEventCount = 0u;
    (void)memset(DIAG_RECORD_T__G__Active.diag_error_t__A__Errors, 0,
                 sizeof(DIAG_RECORD_T__G__Active.diag_error_t__A__Errors));
    (void)memset(DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents, 0,
                 sizeof(DIAG_RECORD_T__G__Active.diag_power_event_t__A__PowerEvents));
    BOOL__G__PowerOffActive = false;
    /*
 * [EN] a cleared history starts its charge-save budget from zero as well,
 * so the next save is not postponed by seconds that are already gone.
 */
    UINT32_T__G__ChargeSecSinceSave = 0u;
    if (bool__hadHistory)
    {
        BOOL__G__Dirty = true;
        UINT32_T__G__SaveDelaySec = 2u;
    }
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
/* ==================== Diag_UpdateMinMax ==================== */
void func__Diag_UpdateMinMax(const measurement_snapshot_t *measurement_snapshot_t__snap)
{
    int32_t int32_t__lock;
    if (measurement_snapshot_t__snap == NULL || !measurement_snapshot_t__snap->valid) return;
    int32_t__lock = osKernelLock();
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
    if (int32_t__lock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__lock);
    }
}
const diag_record_t *func__Diag_GetRecord(void)
{
    return &DIAG_RECORD_T__G__Active;
}
/* ==================== Diag_LockRecord ==================== */
int32_t func__Diag_LockRecord(void)
{
    return osKernelLock();
}
/* ==================== Diag_UnlockRecord ==================== */
void func__Diag_UnlockRecord(int32_t int32_t__savedLock)
{
    if (int32_t__savedLock >= 0)
    {
        (void)osKernelRestoreLock(int32_t__savedLock);
    }
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
* @return bool true when the flash write succeeded /
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
#if MODULE_CHARGER
    /*
 * [EN] Defer the diag save while another flash writer (NVM / CalLut)
 * already holds the charger suspend window; BOOL__G__Dirty stays set so
 * the next 1 s tick retries cleanly without overlapping FPEC accesses.
 */
    if (func__Charger_IsSuspended() != false)
    {
        return false;
    }
#endif
    /*
 * [EN] Judge page A in the Stage scratch; keep only its sequence.
 */
    func__Diag_ReadFlash(DIAG_PAGE_A_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validA = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqA = 0u;
    if (bool__validA)
    {
        uint16_t__seqA = DIAG_RECORD_T__G__Stage.uint16_t__seq;
    }
    /*
 * [EN] Judge page B in the same scratch.
 */
    func__Diag_ReadFlash(DIAG_PAGE_B_ADDR, &DIAG_RECORD_T__G__Stage);
    bool__validB = func__Diag_RecordValidate(&DIAG_RECORD_T__G__Stage);
    uint16_t__seqB = 0u;
    if (bool__validB)
    {
        uint16_t__seqB = DIAG_RECORD_T__G__Stage.uint16_t__seq;
    }
    /*
 * [EN] Decide the next sequence and the target page (the older one),
 * using the wrap-safe int16 difference for both sequence advancement
 * and page selection.
 */
    if (bool__validA && bool__validB)
    {
        int16__seqDiff = (int16_t)(uint16_t__seqA - uint16_t__seqB);
        if (int16__seqDiff > 0)
        {
            uint16_t__newSeq = (uint16_t)(uint16_t__seqA + 1u);
            uint32_t__targetAddr = DIAG_PAGE_B_ADDR;
        }
        else
        {
            uint16_t__newSeq = (uint16_t)(uint16_t__seqB + 1u);
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
    /*
 * [EN] Build the outgoing record in the scratch and write it; on
 * success Active adopts the built copy (CRC included), as before.
 */
    DIAG_RECORD_T__G__Active.uint16_t__seq = uint16_t__newSeq;
    func__Diag_BuildRecord(&DIAG_RECORD_T__G__Stage, &DIAG_RECORD_T__G__Active);
    /*
 * [EN] Blind-window rule (see the docstring above): suspend the
 * charger and stop both gates for the erase+program stall, then let
 * the next control pass resume. The suspend flag is always cleared
 * right after the write, on success and on failure alike.
 */
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
        int32_t int32_t__lock = osKernelLock();
        BOOL__G__Dirty = false;
        UINT32_T__G__SaveDelaySec = 0u;
        /*
 * [EN] the flash now holds every charge second counted so far.
 */
        UINT32_T__G__ChargeSecSinceSave = 0u;
        (void)memcpy(&DIAG_RECORD_T__G__Active, &DIAG_RECORD_T__G__Stage, sizeof(diag_record_t));
        if (int32_t__lock >= 0)
        {
            (void)osKernelRestoreLock(int32_t__lock);
        }
    }
    return bool__ok;
}
