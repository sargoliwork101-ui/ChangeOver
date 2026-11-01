/**
 * @file    cal_lut.h
 * @brief   [EN] Runtime bench LUT storage in its OWN flash block (v1.66).
 * @note    [EN] User order 2026-10-05: the calibration table must be
 *              pushable straight into the micro instead of being pasted
 *              into calibration.h and rebuilt - and because it moves a lot
 *              of data at once, its storage path must be SEPARATE from the
 *              ordinary parameter NVM. This module is that separate path:
 *              two dedicated 1 KiB pages (0x0800F000 / 0x0800F400) that sit
 *              ABOVE the parameter NVM (0x0800E000..0x0800EFFF since v1.80),
 *              the same
 *              power-cut-safe ping-pong, its own magic/version/CRC32, and
 *              NOTHING in common with esp_link_nvm.c except the BSP flash
 *              driver. A LUT push can therefore never disturb a parameter
 *              record and vice versa.
 *              [2026-10-10] There is NO compiled table any more. The panel
 *              is the only source. Without a valid record the measurement
 *              path returns the chain value itself (BSP gain and offset),
 *              which starts at the maximum gain so a fresh board reads high,
 *              never low.
 *              Handshake: the panel stages the table (LUT_BEGIN +
 *              LUT_CHUNK), then asks for a commit (LUT_COMMIT carrying the
 *              CRC32 the panel computed). The board validates, writes,
 *              re-reads and answers LUT_ACK with its OWN CRC32 of what is
 *              now in flash. Only when the two match may the panel ask for
 *              the reboot (LUT_RESET) that lets every module start from the
 *              new table.
 */
#ifndef CAL_LUT_H
#define CAL_LUT_H
#include <stdint.h>
#include <stdbool.h>
/* ==================== CalLut constants ==================== */
/*
 * [EN] The two dedicated pages. Guard-defined so the host test can redirect
 *      them into a RAM emulation and compile this exact state machine.
 */
#ifndef CAL_LUT_PAGE_A_ADDR
#define CAL_LUT_PAGE_A_ADDR             0x0800F000u
#endif
#ifndef CAL_LUT_PAGE_B_ADDR
#define CAL_LUT_PAGE_B_ADDR             0x0800F400u
#endif
/*
 * [EN] "CLUT" + format version; a bump invalidates old records and the
 *      board falls back to the gain/offset line (never a silent half-upgrade:
 *      old power records are rejected, they are not reinterpreted as mA).
 */
#define CAL_LUT_MAGIC                   0x434C5554u
#define CAL_LUT_VERSION                 3u   /* 3: battery axis is in TENTHS of mA (v2 records are rejected, not reinterpreted) */
/*
 * [EN] Per-channel point cap. 24 points x 2 axes x 2 channels x 4 B = 384 B
 *      of record (and the same in RAM). The bench wizard runs 17..21 duty
 *      steps today, so 24 leaves room without another flash page. The
 *      _Static_assert in the .c proves the record still fits the page.
 */
#define CAL_LUT_POINTS_MAX              24u
#define CAL_LUT_POINTS_MIN              2u
/*
 * [EN] Channel ids as they travel on the link (1-based, like the panel and
 *      the schematic name the batteries).
 */
#define CAL_LUT_CHANNEL_1               1u
#define CAL_LUT_CHANNEL_2               2u
/*
 * [EN] Commit/stage status codes, echoed to the panel in LUT_ACK so a
 *      failure says WHY instead of just "nothing happened".
 */
#define CAL_LUT_ST_OK                   0u
#define CAL_LUT_ST_NO_STAGE             1u
#define CAL_LUT_ST_COUNT                2u
#define CAL_LUT_ST_MISSING              3u
#define CAL_LUT_ST_CHAIN                4u
#define CAL_LUT_ST_DIP                5u
#define CAL_LUT_ST_CRC                  6u
#define CAL_LUT_ST_FLASH                7u
#define CAL_LUT_ST_RANGE                8u
#define CAL_LUT_COUNTS_MAX              4095u
/*
 * [EN] Battery-current axis cap in TENTHS of mA (200000 = 20000 mA). Keeps the
 *      bench interpolation inside 32 bits. Real packs are far below it.
 */
#define CAL_LUT_BATTERY_TENTHS_MAX      200000u
/* ==================== CalLut types ==================== */
/**
 * @brief  [EN] One channel's table inside the flash record.
 */
typedef struct
{
    uint32_t uint32_t__points;
    uint32_t UINT32_T__A__Counts[CAL_LUT_POINTS_MAX];
    uint32_t UINT32_T__A__BatteryMa[CAL_LUT_POINTS_MAX];
} cal_lut_channel_t;
/**
 * @brief  [EN] The flash record: header + both channels + CRC32 over every
 *              preceding byte (same CRC the panel computes, so the ACK is a
 *              real end-to-end handshake and not a length check).
 */
typedef struct
{
    uint32_t uint32_t__magic;
    uint16_t uint16_t__version;
    uint16_t uint16_t__seq;
    cal_lut_channel_t CAL_LUT_CHANNEL_T__A__Channel[2];
    uint32_t uint32_t__crc32;
} cal_lut_record_t;
/* ==================== CalLut public functions ==================== */
/*
 * [EN] Pure helpers - public because the host test compiles this exact code
 *      against a RAM-emulated flash.
 */
/**
 * @brief  [EN] Reflected CRC32 (poly 0xEDB88320) - the standard zip CRC the
 *              panel's JavaScript also implements.
 */
uint32_t func__CalLut_Crc32(const uint8_t *uint8_t__A__Data,
                            uint32_t uint32_t__length);
/**
 * @brief  [EN] Validate a candidate record: magic, version, point counts,
 *              strictly increasing chain axis, non-decreasing battery-current
 *              axis (a dip would make the unsigned interpolation wrap), CRC32.
 */
bool func__CalLut_RecordValidate(const cal_lut_record_t *cal_lut_record_t__record);
/**
 * @brief  [EN] Boot load: newest valid record wins, otherwise no table
 *              is active and the chain value is used as the reading.
 */
void func__CalLut_Init(void);
/**
 * @brief  [EN] Is a flash table in charge for this channel (1 or 2)?
 */
bool func__CalLut_Active(uint8_t uint8_t__channel);
/**
 * @brief  [EN] Active point count / counts axis / mA axis of a channel.
 *              Only valid while func__CalLut_Active() is true.
 */
uint32_t func__CalLut_Points(uint8_t uint8_t__channel);
const uint32_t *func__CalLut_Counts(uint8_t uint8_t__channel);
const uint32_t *func__CalLut_BatteryMa(uint8_t uint8_t__channel);
/**
 * @brief  [EN] Start staging a new table (LUT_BEGIN). Clears the staging
 *              buffer; the ACTIVE table keeps working until the commit
 *              succeeds, so an interrupted push changes nothing. A zero
 *              count is legal and a successful commit removes that channel's
 *              previous flash override.
* @return bool false = counts out of range /
 */
bool func__CalLut_StageBegin(uint32_t uint32_t__points1,
                             uint32_t uint32_t__points2);
/**
 * @brief  [EN] Store one staged point (LUT_CHUNK).
 */
bool func__CalLut_StagePoint(uint8_t uint8_t__channel,
                             uint32_t uint32_t__index,
                             uint32_t uint32_t__counts,
                             uint32_t uint32_t__batteryMa);
/**
 * @brief  [EN] Validate + write the staged table (LUT_COMMIT). On success
 *              the RAM copy is reloaded FROM FLASH, so what the measurement
 *              path uses is exactly what survived the write.
* @param uint32_t__panelCrc32 CRC32 the panel computed / CRC32
* @param uint32_t__ptr_boardCrc32 Out: CRC32 now in flash /
* @return uint8_t CAL_LUT_ST_* /
 */
uint8_t func__CalLut_Commit(uint32_t uint32_t__panelCrc32,
                            uint32_t *uint32_t__ptr_boardCrc32);
/**
 * @brief  [EN] CRC32 of the record currently in flash (0 when none).
 */
uint32_t func__CalLut_ActiveCrc32(void);
/**
 * @brief  [EN] Arm the post-handshake reboot (LUT_RESET): the reset is NOT
 *              taken inside the link handler - the caller finishes its
 *              frame, the charger is idled, and func__CalLut_Tick() pulls
 *              the trigger a few comm runs later.
 */
void func__CalLut_RequestReset(void);
/**
 * @brief  [EN] Comm-task housekeeping: performs the armed reboot.
 */
void func__CalLut_Tick(void);
#endif /* CAL_LUT_H */
