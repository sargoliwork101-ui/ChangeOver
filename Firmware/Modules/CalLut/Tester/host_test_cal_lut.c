/**
 * @file    host_test_cal_lut.c
 * @brief   [EN] Host unit test for the CalLut module. The PRODUCTION
 *              cal_lut.c is compiled as-is, including its real page
 *              addresses: the harness mmaps RAM at 0x0800F000 so the
 *              module's direct flash reads land on an emulated page pair,
 *              and the erase/program BSP calls write into the same RAM.
 * @note    [EN] What is proven here: a fresh board falls back to the
 *              gain and offset fallback, a staged table must be complete and
 *              monotonic, the panel CRC must match byte for byte, a
 *              successful commit survives a reload, the ping-pong page
 *              alternation works, and a torn/corrupt record is rejected
 *              without ever disturbing the active table.
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "cal_lut.h"
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "esp_link.h"
/* ==================== Check helper ==================== */
static int INT32_T__G__Checks = 0;
static int INT32_T__G__Fails  = 0;
#define CHECK(cond) do { \
        INT32_T__G__Checks++; \
        if (!(cond)) { INT32_T__G__Fails++; \
            printf("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (0)
/* ==================== Emulated flash ==================== */
#define HOST_PAGE_SIZE   (CAL_LUT_PAGE_B_ADDR - CAL_LUT_PAGE_A_ADDR)
#define HOST_MAP_BASE    (CAL_LUT_PAGE_A_ADDR & ~0xFFFu)
#define HOST_MAP_SIZE    0x2000u
static uint32_t UINT32_T__G__EraseCalls   = 0u;
static uint32_t UINT32_T__G__ProgramCalls = 0u;
static bool     BOOL__G__FlashFails       = false;   /* [EN] simulate a dead sector */
static uint32_t UINT32_T__G__SuspendCalls = 0u;
static bool     BOOL__G__ChargerSuspended = false;
static bool     BOOL__G__GatePulsing      = false;
bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress)
{
    if (BOOL__G__FlashFails != false)
    {
        return false;
    }
    UINT32_T__G__EraseCalls++;
    memset((void *)(uintptr_t)uint32_t__pageAddress, 0xFF, HOST_PAGE_SIZE);
    return true;
}
bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__data,
                                     uint32_t uint32_t__count)
{
    if ((BOOL__G__FlashFails != false) || (uint16_t__A__data == NULL))
    {
        return false;
    }
    UINT32_T__G__ProgramCalls++;
    memcpy((void *)(uintptr_t)uint32_t__address,
           uint16_t__A__data,
           (size_t)uint32_t__count * sizeof(uint16_t));
    return true;
}
bool func__BspPwm_IsGatePulsing(bsp_pwm_channel_t bsp_pwm_channel_t__channel)
{
    (void)bsp_pwm_channel_t__channel;
    return BOOL__G__GatePulsing;
}
void func__BspPwm_StopAll(void)
{
    BOOL__G__GatePulsing = false;
}
/*
 * [EN] ESP-Link UART double: captures every transmitted frame byte so the
 * LUT_ACK regression below can read the real ACK the module emits.
 */
static uint8_t  UINT8_T__G__TxCap[600];
static uint16_t UINT16_T__G__TxCapLen = 0u;
bool func__BspUart_Write(const uint8_t *uint8_t__data,
                         uint16_t uint16_t__length)
{
    if (((uint32_t)UINT16_T__G__TxCapLen + (uint32_t)uint16_t__length) <=
        sizeof(UINT8_T__G__TxCap))
    {
        memcpy(&UINT8_T__G__TxCap[UINT16_T__G__TxCapLen],
               uint8_t__data, (size_t)uint16_t__length);
        UINT16_T__G__TxCapLen = (uint16_t)(UINT16_T__G__TxCapLen +
                                           uint16_t__length);
    }
    return true;
}
bool func__BspUart_ReadByte(uint8_t *uint8_t__byte)
{
    (void)uint8_t__byte;
    return false;
}
void func__BspUart_Init(void)
{
}
/*
 * [EN] esp_link.c's reset branch flushes the debounced NVM save; this test
 * does not exercise persistence, so the host double reports success.
 */
bool func__EspLink_NvmFlushForReset(void)
{
    return true;
}
void func__Charger_SetSuspended(bool bool__suspended)
{
    BOOL__G__ChargerSuspended = bool__suspended;
    UINT32_T__G__SuspendCalls++;
}
void func__Rtos_DelayMilliseconds(uint32_t uint32_t__milliseconds)
{
    (void)uint32_t__milliseconds;       /* [EN] no clock on the host */
}
/*
 * [EN] Kernel lock stubs: the production commit swaps the active record under
 *      osKernelLock(). On the PC there is no scheduler, so the lock is a no-op.
 */
int32_t osKernelLock(void);
int32_t osKernelLock(void)
{
    return 0;
}
int32_t osKernelRestoreLock(int32_t int32_t__state);
int32_t osKernelRestoreLock(int32_t int32_t__state)
{
    return int32_t__state;
}
int32_t osKernelGetState(void);
int32_t osKernelGetState(void)
{
    return 2;                           /* [EN] osKernelRunning */
}
/* ==================== Panel-side CRC / CRC ==================== */
/*
 * [EN] Byte-for-byte the CRC32 the panel computes over the staged content:
 * per channel the point count as one byte, then every (counts, mA)
 * pair little endian. If this drifts from the module's own version the
 * commit is refused - which is exactly the end-to-end property the
 * handshake exists for.
 */
static uint32_t func__HostCrc32Byte(uint32_t uint32_t__crc, uint8_t uint8_t__byte)
{
    uint8_t uint8_t__bit;
    uint32_t__crc ^= (uint32_t)uint8_t__byte;
    for (uint8_t__bit = 0u; uint8_t__bit < 8u; uint8_t__bit++)
    {
        if ((uint32_t__crc & 1u) != 0u)
        {
            uint32_t__crc = (uint32_t__crc >> 1) ^ 0xEDB88320u;
        }
        else
        {
            uint32_t__crc = (uint32_t__crc >> 1);
        }
    }
    return uint32_t__crc;
}
static uint32_t UINT32_T__G__A__Points[2];
static uint32_t UINT32_T__G__A__Chain[2][CAL_LUT_POINTS_MAX];
static uint32_t UINT32_T__G__A__BatteryMa[2][CAL_LUT_POINTS_MAX];
/* ==================== Host u32 reader / u32 ==================== */
/**
 * @brief  [EN] Read one little-endian u32 from a captured link frame.
* @param uint8_t__ptr_data Capture buffer /
* @param uint16_t__offset Byte offset /
* @return uint32_t Decoded value /
 */
static uint32_t func__HostReadU32(const uint8_t *uint8_t__ptr_data,
                                  uint16_t uint16_t__offset)
{
    return ((uint32_t)uint8_t__ptr_data[uint16_t__offset] |
            ((uint32_t)uint8_t__ptr_data[uint16_t__offset + 1u] << 8) |
            ((uint32_t)uint8_t__ptr_data[uint16_t__offset + 2u] << 16) |
            ((uint32_t)uint8_t__ptr_data[uint16_t__offset + 3u] << 24));
}
static uint32_t func__HostPanelCrc(void)
{
    uint32_t uint32_t__crc = 0xFFFFFFFFu;
    uint8_t  uint8_t__channel;
    uint32_t uint32_t__i;
    uint8_t  uint8_t__b;
    for (uint8_t__channel = 0u; uint8_t__channel < 2u; uint8_t__channel++)
    {
        uint32_t__crc = func__HostCrc32Byte(
            uint32_t__crc, (uint8_t)UINT32_T__G__A__Points[uint8_t__channel]);
        for (uint32_t__i = 0u;
             uint32_t__i < UINT32_T__G__A__Points[uint8_t__channel];
             uint32_t__i++)
        {
            uint32_t uint32_t__pair[2];
            uint32_t__pair[0] = UINT32_T__G__A__Chain[uint8_t__channel][uint32_t__i];
            uint32_t__pair[1] = UINT32_T__G__A__BatteryMa[uint8_t__channel][uint32_t__i];
            for (uint8_t__b = 0u; uint8_t__b < 8u; uint8_t__b++)
            {
                uint32_t__crc = func__HostCrc32Byte(
                    uint32_t__crc,
                    (uint8_t)((uint32_t__pair[uint8_t__b / 4u] >>
                               (8u * (uint8_t__b % 4u))) & 0xFFu));
            }
        }
    }
    return (uint32_t__crc ^ 0xFFFFFFFFu);
}
/**
 * @brief  [EN] Stage a simple straight table on both channels.
 */
static void func__StageTable(uint32_t uint32_t__points, uint32_t uint32_t__powerStep)
{
    uint8_t  uint8_t__channel;
    uint32_t uint32_t__i;
    CHECK(func__CalLut_StageBegin(uint32_t__points, uint32_t__points) == true);
    for (uint8_t__channel = 0u; uint8_t__channel < 2u; uint8_t__channel++)
    {
        UINT32_T__G__A__Points[uint8_t__channel] = uint32_t__points;
        for (uint32_t__i = 0u; uint32_t__i < uint32_t__points; uint32_t__i++)
        {
            uint32_t uint32_t__chain = 100u + (uint32_t__i * 100u);
            uint32_t uint32_t__power = uint32_t__i * uint32_t__powerStep;
            UINT32_T__G__A__Chain[uint8_t__channel][uint32_t__i] = uint32_t__chain;
            UINT32_T__G__A__BatteryMa[uint8_t__channel][uint32_t__i] = uint32_t__power;
            CHECK(func__CalLut_StagePoint((uint8_t)(uint8_t__channel + 1u),
                                          uint32_t__i,
                                          uint32_t__chain,
                                          uint32_t__power) == true);
        }
    }
}
int main(void)
{
    void     *void_ptr__map;
    uint32_t  uint32_t__crc;
    uint32_t  uint32_t__boardCrc;
    uint32_t  uint32_t__firstCrc;
    uint32_t  uint32_t__secondCrc;
    uint8_t   uint8_t__status;
    printf("== CalLut host test ==\n");
    /*
 * [EN] Emulate the two dedicated flash pages at their real addresses so
 * the module's direct reads work unmodified.
 */
    void_ptr__map = mmap((void *)(uintptr_t)HOST_MAP_BASE, HOST_MAP_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (void_ptr__map == MAP_FAILED)
    {
        printf("FAIL: cannot emulate the flash pages\n");
        return 1;
    }
    memset(void_ptr__map, 0xFF, HOST_MAP_SIZE);     /* [EN] erased flash */
    /* ---- 1. a fresh board: no record, gain and offset line stays in charge ---- */
    func__CalLut_Init();
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_1) == false);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_2) == false);
    CHECK(func__CalLut_ActiveCrc32() == 0u);
    /* ---- 2. the point count is range checked at LUT_BEGIN ---- */
    CHECK(func__CalLut_StageBegin(CAL_LUT_POINTS_MIN - 1u, 4u) == false);
    CHECK(func__CalLut_StageBegin(CAL_LUT_POINTS_MAX + 1u, 4u) == false);
    CHECK(func__CalLut_StageBegin(0u, 0u) == false);
    CHECK(func__CalLut_StageBegin(CAL_LUT_POINTS_MIN, CAL_LUT_POINTS_MAX) == true);
    /* ---- 3. a commit without a begin is refused ---- */
    func__CalLut_Init();
    uint8_t__status = func__CalLut_Commit(0u, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_NO_STAGE);
    /* ---- 4. a staged table with a hole never reaches flash ---- */
    CHECK(func__CalLut_StageBegin(4u, 4u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 0u, 100u, 0u) == true);
    uint8_t__status = func__CalLut_Commit(0u, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_MISSING);
    CHECK(UINT32_T__G__EraseCalls == 0u);
    /*
 * ---- 4b. duplicate delivery is idempotent and cannot hide a hole ----
 * [EN] A lost ACK makes the sender retry an already received index. The
 * duplicate must not consume another missing-point slot; otherwise
 * repeated delivery can make an incomplete table look complete.
 */
    CHECK(func__CalLut_StageBegin(4u, 4u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 0u, 100u, 0u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 1u, 200u, 1000u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 1u, 201u, 1001u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 1u, 202u, 1002u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_2, 0u, 100u, 0u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_2, 1u, 200u, 1000u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_2, 1u, 201u, 1001u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_2, 1u, 202u, 1002u) == true);
    uint8_t__status = func__CalLut_Commit(0u, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_MISSING);
    CHECK(UINT32_T__G__EraseCalls == 0u);
    /* ---- 5. a point index outside the staged count is refused ---- */
    CHECK(func__CalLut_StageBegin(4u, 4u) == true);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 4u, 100u, 0u) == false);
    CHECK(func__CalLut_StagePoint(3u, 0u, 100u, 0u) == false);      /* [EN] no channel 3 */
    /*
 * ---- 6. the chain axis must be strictly increasing ----
 * [EN] A flat or falling chain axis would divide by zero or run the
 * interpolation backwards.
 */
    func__StageTable(4u, 1000u);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 2u, 100u, 2000u) == true);
    UINT32_T__G__A__Chain[0][2] = 100u;
    uint8_t__status = func__CalLut_Commit(func__HostPanelCrc(), &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_CHAIN);
    /*
 * ---- 7. the mA axis may be flat but never dip ----
 * [EN] An unsigned dip wraps the interpolation to about 4e9 mA.
 */
    func__StageTable(4u, 1000u);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 2u, 300u, 500u) == true);
    UINT32_T__G__A__BatteryMa[0][2] = 500u;
    uint8_t__status = func__CalLut_Commit(func__HostPanelCrc(), &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_DIP);
    /*
 * ---- 7b. every count must fit the 12-bit ADC (<= 4095) ----
 * [EN] A count above full scale can never be produced by the ADC, so the
 * table is refused rather than silently clipped.
 */
    func__StageTable(4u, 1000u);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 3u, 4096u, 3000u) == true);
    UINT32_T__G__A__Chain[0][3] = 4096u;
    uint8_t__status = func__CalLut_Commit(func__HostPanelCrc(), &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_RANGE);
    /*
 * ---- 7c. the battery axis is capped (tenths of mA) so the bench math stays
 */
    func__StageTable(4u, 1000u);
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 3u, 400u,
                                  CAL_LUT_BATTERY_TENTHS_MAX + 1u) == true);
    uint8_t__status = func__CalLut_Commit(func__HostPanelCrc(), &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_RANGE);
    /* ---- 8. a wrong panel CRC is refused and flash is untouched ---- */
    func__StageTable(5u, 1000u);
    uint32_t__crc = func__HostPanelCrc();
    uint8_t__status = func__CalLut_Commit(uint32_t__crc ^ 0x5A5A5A5Au, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_CRC);
    CHECK(UINT32_T__G__EraseCalls == 0u);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_1) == false);
    /* ---- 9. the happy path: commit, verify, reload ---- */
    func__StageTable(5u, 1000u);
    uint32_t__crc = func__HostPanelCrc();
    uint8_t__status = func__CalLut_Commit(uint32_t__crc, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_OK);
    CHECK(uint32_t__boardCrc == uint32_t__crc);
    CHECK(UINT32_T__G__EraseCalls == 1u);
    CHECK(UINT32_T__G__ProgramCalls >= 1u);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_1) == true);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_2) == true);
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_1) == 5u);
    CHECK(func__CalLut_Counts(CAL_LUT_CHANNEL_1)[0] == 100u);
    CHECK(func__CalLut_BatteryMa(CAL_LUT_CHANNEL_1)[4] == 4000u);
    /*
 * [EN] ActiveCrc32() reports the CRC of the RECORD in flash, which is a
 * different number from the panel content CRC; what matters is that
 * it is non-zero and stable across a reload.
 */
    CHECK(func__CalLut_ActiveCrc32() != 0u);
    uint32_t__firstCrc = func__CalLut_ActiveCrc32();
    /*
 * [EN] The charger must have been suspended around the write and
 * released afterwards - a flash write while a gate pulses is the
 * one thing this module must never do.
 */
    CHECK(UINT32_T__G__SuspendCalls >= 2u);
    CHECK(BOOL__G__ChargerSuspended == false);
    /*
 * [EN] A reboot (Init again) must find the record and keep it.
 */
    func__CalLut_Init();
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_1) == true);
    CHECK(func__CalLut_ActiveCrc32() == uint32_t__firstCrc);
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_2) == 5u);
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_1) == 5u);
    /*
 * ---- 10. the second commit lands on the OTHER page (ping-pong) ----
 * [EN] Writing the same page twice would leave a window with no valid
 * record at all; the newest sequence number decides on boot.
 */
    func__StageTable(6u, 2000u);
    uint32_t__crc = func__HostPanelCrc();
    uint8_t__status = func__CalLut_Commit(uint32_t__crc, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_OK);
    CHECK(UINT32_T__G__EraseCalls == 2u);
    CHECK(func__CalLut_ActiveCrc32() != uint32_t__firstCrc);
    uint32_t__secondCrc = func__CalLut_ActiveCrc32();
    func__CalLut_Init();
    CHECK(func__CalLut_ActiveCrc32() == uint32_t__secondCrc);
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_1) == 6u);
    /*
 * ---- 10b. LUT_READ returns the active values, including channel 2 ----
 * [EN] This is deliberately checked against the captured DATA frames,
 * not merely against counts or the record CRC: the panel's before /
 * after comparison needs the actual chain and power numbers.
 */
    {
        const uint16_t uint16_t__framePayloadLength =
            (uint16_t)(3u + (8u * 6u));
        const uint16_t uint16_t__frameLength =
            (uint16_t)(6u + uint16_t__framePayloadLength + 2u);
        const uint16_t uint16_t__secondFrame = uint16_t__frameLength;
        func__EspLink_HostTest_Reset();
        UINT16_T__G__TxCapLen = 0u;
        CHECK(func__EspLink_HostTest_HandleLutFrame(0x08u, NULL, 0u) == true);
        CHECK(UINT16_T__G__TxCapLen == (uint16_t)(2u * uint16_t__frameLength));
        CHECK(UINT8_T__G__TxCap[3] == 0x14u && UINT8_T__G__TxCap[6] == 1u);
        CHECK(UINT8_T__G__TxCap[8] == 6u &&
              func__HostReadU32(UINT8_T__G__TxCap, 9u) == 100u &&
              func__HostReadU32(UINT8_T__G__TxCap, 13u) == 0u);
        CHECK(UINT8_T__G__TxCap[uint16_t__secondFrame + 6u] == 2u &&
              UINT8_T__G__TxCap[uint16_t__secondFrame + 8u] == 6u &&
              func__HostReadU32(UINT8_T__G__TxCap,
                                (uint16_t)(uint16_t__secondFrame + 9u)) == 100u &&
              func__HostReadU32(UINT8_T__G__TxCap,
                                (uint16_t)(uint16_t__secondFrame + 13u)) == 0u);
        CHECK(func__HostReadU32(UINT8_T__G__TxCap, 49u) == 600u &&
              func__HostReadU32(UINT8_T__G__TxCap, 53u) == 10000u);
    }
    /*
 * ---- 11. a corrupted newest record falls back to the older one ----
 * [EN] Flip one payload byte of whichever page is newest; validation
 * must reject it and boot must pick the surviving record instead
 * of running on a half-written table.
 */
    {
        const cal_lut_record_t *cal_lut_record_t__pageA =
            (const cal_lut_record_t *)(uintptr_t)CAL_LUT_PAGE_A_ADDR;
        uint32_t uint32_t__newestAddr =
            (cal_lut_record_t__pageA->CAL_LUT_CHANNEL_T__A__Channel[0]
                 .uint32_t__points == 6u)
                ? CAL_LUT_PAGE_A_ADDR
                : CAL_LUT_PAGE_B_ADDR;
        volatile uint8_t *uint8_t_ptr__newest =
            (volatile uint8_t *)(uintptr_t)uint32_t__newestAddr;
        uint8_t_ptr__newest[16] = (uint8_t)(uint8_t_ptr__newest[16] ^ 0xFFu);
        func__CalLut_Init();
        CHECK(func__CalLut_ActiveCrc32() == uint32_t__firstCrc);
        CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_1) == 5u);
    }
    /*
 * ---- 11b. zero points are a real override deletion -----------------
 * [EN] Start from the surviving record where both channels have points,
 * then commit a valid channel 1 and zero points for channel 2. The
 * new record must remain valid, keep channel 1, and remove channel
 * 2's flash override; zero is not a missing-frame error.
 */
    CHECK(func__CalLut_StageBegin(5u, 0u) == true);
    UINT32_T__G__A__Points[0] = 5u;
    UINT32_T__G__A__Points[1] = 0u;
    for (uint32_t uint32_t__i = 0u; uint32_t__i < 5u; uint32_t__i++)
    {
        UINT32_T__G__A__Chain[0][uint32_t__i] = 100u + (100u * uint32_t__i);
        UINT32_T__G__A__BatteryMa[0][uint32_t__i] = 1000u * uint32_t__i;
        CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, uint32_t__i,
                                      UINT32_T__G__A__Chain[0][uint32_t__i],
                                      UINT32_T__G__A__BatteryMa[0][uint32_t__i]) == true);
    }
    uint32_t__crc = func__HostPanelCrc();
    uint8_t__status = func__CalLut_Commit(uint32_t__crc, &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_OK);
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_1) == 5u);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_2) == false);
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_2) == 0u);
    func__CalLut_Init();
    CHECK(func__CalLut_Points(CAL_LUT_CHANNEL_1) == 5u);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_2) == false);
    /*
 * [EN] The wire contract is also checked after the deletion: channel 2
 * still gets its own independent LUT_DATA frame, with count zero.
 */
    {
        const uint16_t uint16_t__frame1 = (uint16_t)(6u + 3u + (8u * 5u) + 2u);
        const uint16_t uint16_t__frame2 = (uint16_t)(6u + 3u + 2u);
        func__EspLink_HostTest_Reset();
        UINT16_T__G__TxCapLen = 0u;
        CHECK(func__EspLink_HostTest_HandleLutFrame(0x08u, NULL, 0u) == true);
        CHECK(UINT16_T__G__TxCapLen == (uint16_t)(uint16_t__frame1 + uint16_t__frame2));
        CHECK(UINT8_T__G__TxCap[uint16_t__frame1 + 6u] == 2u &&
              UINT8_T__G__TxCap[uint16_t__frame1 + 8u] == 0u);
    }
    /* ---- 12. both pages unreadable: back to the gain and offset line ---- */
    memset(void_ptr__map, 0xFF, HOST_MAP_SIZE);
    func__CalLut_Init();
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_1) == false);
    CHECK(func__CalLut_ActiveCrc32() == 0u);
    /* ---- 13. a dead flash sector is reported, never silently ignored ---- */
    BOOL__G__FlashFails = true;
    func__StageTable(5u, 1000u);
    uint8_t__status = func__CalLut_Commit(func__HostPanelCrc(), &uint32_t__boardCrc);
    CHECK(uint8_t__status == CAL_LUT_ST_FLASH);
    CHECK(func__CalLut_Active(CAL_LUT_CHANNEL_1) == false);
    BOOL__G__FlashFails = false;
    /*
 * ---- 14. housekeeping without an armed reset must do nothing ----
 * [EN] The reset itself is NOT exercised on the host: the test double
 * for stm32f1xx.h fails loudly if NVIC_SystemReset is ever reached.
 */
    UINT32_T__G__SuspendCalls = 0u;
    func__CalLut_Tick();
    func__CalLut_Tick();
    CHECK(UINT32_T__G__SuspendCalls == 0u);
    /*
 * ---- 15. LUT_ACK echoes the STAGED counts, not the active table ----
 * [EN] User board bug 2026-10-07: the ACK used to carry
 * func__CalLut_Points() (the ACTIVE table). On a fresh board that
 * is 0, the ESP rejected its own first push's good ACKs and the
 * panel said "board did not answer the send stage". The staged
 * count here is deliberately active+1 so a regression to the old
 * echo can never pass by coincidence.
 */
    {
        uint8_t uint8_t__n1 =
            (uint8_t)(func__CalLut_Points(CAL_LUT_CHANNEL_1) + 1u);
        uint8_t UINT8_T__A__Begin[2];
        uint8_t UINT8_T__A__Chunk[3u + (8u * 24u)];
        uint16_t uint16_t__len = 3u;
        uint32_t uint32_t__i;
        if ((uint8_t__n1 < (uint8_t)CAL_LUT_POINTS_MIN) ||
            (uint8_t__n1 > (uint8_t)CAL_LUT_POINTS_MAX))
        {
            /*
 * [EN] Empty active table (0+1 < MIN) or full one: fall back to
 * MIN, which still differs from the active count in both cases.
 */
            uint8_t__n1 = (uint8_t)CAL_LUT_POINTS_MIN;
        }
        UINT8_T__A__Begin[0] = uint8_t__n1;
        UINT8_T__A__Begin[1] = 0u;
        UINT8_T__A__Chunk[0] = 1u;
        UINT8_T__A__Chunk[1] = 0u;
        UINT8_T__A__Chunk[2] = uint8_t__n1;
        for (uint32_t__i = 0u; uint32_t__i < (uint32_t)uint8_t__n1;
             uint32_t__i++)
        {
            uint32_t uint32_t__pair[2];
            uint8_t uint8_t__b;
            uint32_t__pair[0] = 100u + (100u * uint32_t__i);
            uint32_t__pair[1] = 1000u + (1000u * uint32_t__i);
            for (uint8_t__b = 0u; uint8_t__b < 8u; uint8_t__b++)
            {
                UINT8_T__A__Chunk[uint16_t__len] =
                    (uint8_t)((uint32_t__pair[uint8_t__b / 4u] >>
                               (8u * (uint8_t__b % 4u))) & 0xFFu);
                uint16_t__len++;
            }
        }
        func__EspLink_HostTest_Reset();
        UINT16_T__G__TxCapLen = 0u;
        CHECK(func__EspLink_HostTest_HandleLutFrame(0x04u, UINT8_T__A__Begin,
                                                    2u) == true);
        CHECK(UINT16_T__G__TxCapLen == 16u);   /* SOF2 ver type len2 pl8 crc2 */
        CHECK(UINT8_T__G__TxCap[3] == 0x13u);  /* LUT_ACK */
        CHECK(UINT8_T__G__TxCap[6] == 1u);     /* stage = BEGIN */
        CHECK(UINT8_T__G__TxCap[7] == 0u);     /* status = OK */
        CHECK(UINT8_T__G__TxCap[8] == uint8_t__n1); /* staged n1, not active */
        CHECK(UINT8_T__G__TxCap[9] == 0u);     /* staged n2 */
        UINT16_T__G__TxCapLen = 0u;
        CHECK(func__EspLink_HostTest_HandleLutFrame(0x05u, UINT8_T__A__Chunk,
                                                    uint16_t__len) == true);
        CHECK(UINT16_T__G__TxCapLen == 16u);
        CHECK(UINT8_T__G__TxCap[6] == 2u);     /* stage = CHUNK */
        CHECK(UINT8_T__G__TxCap[7] == 0u);     /* status = OK */
        CHECK(UINT8_T__G__TxCap[8] == uint8_t__n1);
        CHECK(UINT8_T__G__TxCap[9] == 0u);
    }
    /*
 * ---- 12. the 4095 boundary is accepted (last check: it writes flash) ----
 * [EN] Full scale is the largest legal count, so 4095 must commit OK.
 */
    func__StageTable(4u, 1000u);
    UINT32_T__G__A__Chain[0][3] = 4095u;
    CHECK(func__CalLut_StagePoint(CAL_LUT_CHANNEL_1, 3u, 4095u, 3000u) == true);
    CHECK(func__CalLut_Commit(func__HostPanelCrc(), &uint32_t__boardCrc) == CAL_LUT_ST_OK);
    printf("checks: %d, fails: %d\n", INT32_T__G__Checks, INT32_T__G__Fails);
    return (INT32_T__G__Fails == 0) ? 0 : 1;
}
