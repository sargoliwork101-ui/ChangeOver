/**
 * @file    esp_link_nvm.h
 * @brief   [EN] Panel parameter persistence in STM32 on-chip flash (v1.14).
 * @note    [EN] Every settable parameter EXCEPT the transient test modes
 *              (15..18 fixed duty, 19 manual test) is snapshotted to the
 *              last two 1 KiB flash pages with a CRC and a sequence
 *              number, alternating pages per save so a power cut mid-write
 *              can never lose the previous good record. Boot replays the
 *              newest valid record through the SAME clamped setters as
 *              the panel path, so a stale or hostile record can only land
 *              inside the compiled safety windows. Saves are debounced
 *              ~1.5 s and run in the comm task (a page erase stalls the
 *              CPU ~20..40 ms once per save).
 */
#ifndef ESP_LINK_NVM_H
#define ESP_LINK_NVM_H
#include <stdint.h>
#include <stdbool.h>
/* ==================== EspLink Nvm constants ==================== */
/*
 * [EN] The two reserved 1 KiB pages at the top of the 64 KiB bank (the linker
 *      script shrinks FLASH to 62 KiB so the application can never collide).
 *      Power-cut-safe ping-pong: each save erases the page that does NOT hold
 *      the newest record, then programs the new record there.
 */
/*
 * [EN] Address macros are guard-defined so the host test can redirect the
 *      two pages into a RAM emulation and compile the EXACT flash-state code.
 */
/*
 * [EN] v1.80: each bank is TWO 1 KiB pages now (A = 0x0800E000..E7FF,
 *      B = 0x0800E800..EFFF). The scenario-6 lamp/buzzer ids 128..131 took
 *      the persisted set past the 126 entries that fitted in a single page,
 *      and the record is written whole, so the bank - not the entry - had to
 *      grow. A save erases every 1 KiB page of the target bank before
 *      programming (ESP_LINK_NVM_FLASH_PAGE_SIZE below); ping-pong is
 *      unchanged, so a power cut mid-write still leaves the other bank
 *      intact. The old top pages 0x0800F800/FC00 are left reserved-unused:
 *      a record written there by an older firmware simply never validates.
 */
#ifndef ESP_LINK_NVM_PAGE_A_ADDR
#define ESP_LINK_NVM_PAGE_A_ADDR        0x0800E000u
#endif
#ifndef ESP_LINK_NVM_PAGE_B_ADDR
#define ESP_LINK_NVM_PAGE_B_ADDR        0x0800E800u
#endif
/*
 * [EN] Hardware erase granularity (STM32F103 medium density = 1 KiB). A bank
 *      spans ESP_LINK_NVM_PAGE_B_ADDR - ESP_LINK_NVM_PAGE_A_ADDR bytes, i.e.
 *      an exact number of these pages; the save loop erases them all.
 */
#ifndef ESP_LINK_NVM_FLASH_PAGE_SIZE
#define ESP_LINK_NVM_FLASH_PAGE_SIZE    0x400u
#endif
/*
 * [EN] Record identity: "CHO1" + format version. A bump invalidates old
 *      records (they fail validation and the compiled defaults stay).
 */
#define ESP_LINK_NVM_MAGIC              0x43484F31u
/*
 * [EN] v11 (v1.71) is a MEANING bump, not a layout bump: ids 57 and 122 kept
 *      their slots but changed units - 57 went from "critical duty percent"
 *      to "critical per-beep duration in ms", and 122 went from "band-2 gap"
 *      to "band-2 repeat interval in ms". A stored v10 record holds 57=100
 *      and 122=100, which under the new meaning would mean a 100 ms beep
 *      repeating every 100 ms. Replaying it would be wrong and loud, so the
 *      version bump deliberately invalidates it and the compiled defaults
 *      (which reproduce the old sound exactly) take over. Re-tune once from
 *      the panel after this upgrade.
 */
/*
 * [EN] Record version history: v3/v4/v5/v6/v7/v8/v9 are historical layouts;
 *      v10 added imbalance ids 108..118 and runtime slots 200..202, v11
 *      invalidated the old meanings of ids 57 and 122, and v12 is the current
 *      format after the non-contiguous scenario-5/6 additions plus scenario-7
 *      controls 137..142. Any older version fails validation and falls back to
 *      compiled defaults; the user must resend desired settings after upgrade.
 */
#define ESP_LINK_NVM_VERSION            12u
/*
 * [EN] v1.80 capacity then: 144 entries (12 + 144 x 8 + 4 = 1168 B) inside the
 *      two-page 2 KiB ping-pong bank; the persisted set was ids 0..14, 20..142
 *      except retired 72/73 and transient 76, plus runtime slots 200..203.
 *      The static assertions in esp_link_nvm.c prove the size and entry count.
 */
/*
 * [EN] v1.50: 126 slots (12 + 126 x 8 + 4 = 1024 B = exactly one 1 KiB page).
 *      v1.72 spends the last of that headroom: the three scenario-6 ids
 *      (125..127) and slot 203 bring the persisted set to exactly 126, so
 *      the cap and the real set now meet. Anything further needs a second
 *      page, and the _Static_assert in the .c is what will say so. The record stores its own entry count, so an
 *      older, shorter record still replays correctly - no version bump.
 */
/*
 * [EN] Historical v1.80 note: moving from the old 126-entry single-page
 *      layout to a 144-entry two-page bank made room for the scenario-6 face
 *      controls. The current v12 format subsequently uses the same capacity
 *      for scenario-5/6 extensions and scenario-7 ids 137..142.
 */
/*
 * [EN] 172 entries (12 + 172 x 8 + 4 = 1392 B) - still inside the
 *      2 KiB two-page bank. The seven scenario ticks take the persisted set
 *      to 163, so the old 160 cap had to grow; no version bump, the record
 *      still stores its own entry count. The active persisted set is now ids
 *      0..14, 20..170 except 70 (derived), 72/73 and 76, plus runtime slots 200..203; the
 *      thermal ids 166..168 raised the high-water mark and the 168-entry cap
 *      already covered them (v1.102).
 */
#define ESP_LINK_NVM_ENTRY_MAX         172u
/*
 * [EN] Save debounce in comm-task runs (period 100 ms -> 1.5 s after the last
 *      change; a shorter window would rewrite flash on every keystroke burst).
 */
#define ESP_LINK_NVM_SAVE_DELAY_RUNS    15u
/*
 * [EN] Max save retries after a failed verify before giving up until the next
 *      change (a genuinely worn page must not enter an erase loop).
 */
#define ESP_LINK_NVM_SAVE_RETRIES       3u
/*
 * [EN] Persisted id ranges: ALL settable configuration (0..14 = offsets,
 *      gains, filters, eta, charger enables, duty ceilings; 20..26 =
 *      charge profile; 27..37 = alarms; 38..75 = UI cadence; 77..82 =
 *      full/hysteresis; 83..92 = two-loop PID; 119..120 = charge-side percent map, v1.49; 121..122 = band-2 beep shape, v1.50; 132..133 = imbalance beep count/gap, v1.81; 134..135 = dead-battery beep count/gap, v1.82; 136 = imbalance clean-FLOAT-cycle threshold, v1.83) EXCEPT the transient
 *      test modes 15..18 (fixed duty), 19 (manual test) and 76
 *      (panel-session mute) - those must never survive a reboot. Id 76
 *      sits INSIDE the high range, so the predicate excludes it
 *      explicitly (see the .c). The PID gains are ordinary tuning numbers,
 *      so they persist like the profile does.
 */
#define ESP_LINK_NVM_PERSISTED_ID_MAX_LOW     14u
#define ESP_LINK_NVM_PERSISTED_ID_MIN_HIGH    20u
#define ESP_LINK_NVM_PERSISTED_ID_MAX_HIGH   170u
/*
 * [EN] v1.74: ids 72/73 are retired (the low-battery window became a fixed
 * constant inside the Changeover module), so they are carved out of the high
 * persisted range - storing a value no owner can read back would be dead
 * weight in the record. Mirrors ESPLINK_PARAM_RETIRED_LOWBAT_* in esp_link.h;
 * esp_link_nvm.c static-asserts the two pairs agree.
 */
#define ESP_LINK_NVM_RETIRED_ID_FIRST        72u
#define ESP_LINK_NVM_RETIRED_ID_LAST         73u
#define ESP_LINK_NVM_TRANSIENT_ID_MUTE        76u
/*
 * [EN] v1.104: id 70 is a read-only mirror of the shared q34 upper input
 * limit. It is derived at runtime, never independently stored.
 */
#define ESP_LINK_NVM_DERIVED_ID_OV_THRESH      70u
/*
 * [EN] Imbalance runtime slots (scenario 5, v10): persisted but NEVER user
 *      parameters - the module itself writes them; the panel never draws and
 *      never backs them up.
 */
#define ESP_LINK_NVM_SLOT_MIN_ID            200u
/*
 * [EN] v1.72: 203 = dead-battery latch mask (scenario 6). Same rule - the
 *      module writes it, the panel never backs it up, and it must survive a
 *      power cycle so a faulty battery stays condemned until it is replaced.
 */
#define ESP_LINK_NVM_SLOT_MAX_ID            203u
/**
 * @brief  [EN] Is this parameter id persisted to flash? (config + charge
 *              profile, never the transient test modes).
 */
bool func__EspLink_NvmParamPersisted(uint8_t uint8_t__paramId);
/**
 * @brief  [EN] One persisted parameter slot (id + applied value).
 */
typedef struct
{
    uint16_t uint16_t__id;
    uint16_t uint16_t__pad;
    uint32_t uint32_t__value;
} esp_link_nvm_entry_t;
/**
 * @brief  [EN] Flash record: header + entry list + CRC32 over all preceding
 *              bytes. 992 B for 122 entries (12 + 122 x 8 + 4), inside one
 *              1 KiB page with 32 B to spare. Do not trust this sentence:
 *              the two halves of this very comment disagreed for several
 *              releases (EN said 760 B for 93, FA said 808 B for 99) because
 *              both were hand-arithmetic nobody re-ran. The guarantee is the
 *              _Static_assert in esp_link_nvm.c, which derives the page size
 *              from the two page addresses and fails the build instead.
 */
typedef struct
{
    uint32_t uint32_t__magic;                       /* ESP_LINK_NVM_MAGIC */
    uint16_t uint16_t__version;                     /* ESP_LINK_NVM_VERSION */
    uint16_t uint16_t__seq;
    uint16_t uint16_t__count;
    uint16_t uint16_t__reserved;
    esp_link_nvm_entry_t
        ESP_LINK_NVM_ENTRY_T__A__Entry[ESP_LINK_NVM_ENTRY_MAX];
    uint32_t uint32_t__crc32;
} esp_link_nvm_record_t;
/* ==================== EspLink Nvm public functions ==================== */
/*
 * [EN] Pure record helpers - public because the host test compiles this
 *      exact code against a RAM-emulated flash.
 */
uint32_t func__EspLink_NvmCrc32(const uint8_t *uint8_t__A__Data,
                                uint32_t uint32_t__length);
bool func__EspLink_NvmRecordValidate(
    const esp_link_nvm_record_t *esp_link_nvm_record_t__record);
int8_t func__EspLink_NvmSeqCompare(uint16_t uint16_t__a, uint16_t uint16_t__b);
void func__EspLink_NvmRecordBuild(
    esp_link_nvm_record_t *esp_link_nvm_record_t__record,
    uint16_t uint16_t__seq,
    const esp_link_nvm_entry_t *ESP_LINK_NVM_ENTRY_T__A__Entry,
    uint16_t uint16_t__count);
/**
 * @brief  [EN] Boot-time load: read both pages, pick the newest CRC-valid
 *              record and replay every entry through the clamped parameter
 *              setters (fails are skipped, never fatal). Runs
 *              pre-scheduler in func__App_Init under MODULE_ESP - module
 *              Init functions only reset channel state, never the
 *              settable statics, so the loaded values survive them.
 */
void func__EspLink_NvmInit(void);
/**
 * @brief  [EN] Remember that a persisted parameter changed; a debounced save
 *              follows (see func__EspLink_NvmTick).
* @param uint8_t__paramId The SET_PARAM id / SET_PARAM
 */
void func__EspLink_NvmMarkDirty(uint8_t uint8_t__paramId);
/**
 * @brief  [EN] Comm-task housekeeping: count the debounce after the last
 *              change, then snapshot every persisted parameter and write the
 *              ping-pong record. Also retries a failed verify (bounded).
 */
void func__EspLink_NvmTick(void);
/* ==================== EspLink_NvmFlushForReset ==================== */
/**
 * @brief  [EN] Save a pending (debounced) record immediately - called before
 *              an armed reboot so a panel reset can never outrun the save
 *              that the last edit scheduled (user bug 2026-10-07: reset
 *              inside the ~1.5 s debounce window lost the new values while
 *              the panel had already echoed them as applied).
 * @return bool [EN] true = no pending save or the record was flushed and
 *              verified; false = flash erase/program/verify failed, so reset
 */
bool func__EspLink_NvmFlushForReset(void);
#endif /* ESP_LINK_NVM_H */
