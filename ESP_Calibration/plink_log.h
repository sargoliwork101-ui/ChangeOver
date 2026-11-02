/*
 * plink_log.h - live state history on the panel's own flash, one CSV per day.
 * Included by ESP_Calibration.ino (single translation unit, order matters):
 * after plink_state.h and before plink_link.h (the frame parser calls the hook).
 * No include guard on purpose: including twice would redefine everything.
 * [EN] Second-generation rewrite, 2026-10-27 (user order: "rewrite the log from
 * scratch, your own way, so it does what I asked for"). The first rewrite
 * fixed the three proven defects; this one is the design I would hand to a
 * production line. What is different from a plain "append a line" logger:
 * 1) ONE write path. Every row goes through func__EspLog_AppendRow(), so a
 * refused write cannot be swallowed anywhere (v1 had four print sites).
 * 2) NOTHING IS LOST WHEN THE FLASH REFUSES. The row waits in the RAM ring
 * and is drained back into its own day file (by the row's own epoch, not
 * by "today") as soon as the flash works again.
 * 3) A HALF-WRITTEN LAST ROW CANNOT EAT THE NEXT ONE. A power cut in the
 * middle of a row leaves no closing newline; the day file is repaired
 * once when it is opened, before the next row is appended.
 * 4) THE INDEX CARRIES BYTE SIZES, so retention costs nothing: v1 opened up
 * to 30 day files on every day change just to add up their sizes. Sizes
 * are maintained row by row; an entry with an unknown size is measured
 * once and cached.
 * 5) THE DAY'S ROW CAP EXPLAINS ITSELF. The last row of a full day is a
 * marker (note 4), so a gap in the history says "the cap was reached"
 * instead of looking like a dead panel.
 * 6) LOST ROWS ARE COUNTED. If the RAM ring overflows while the flash is
 * down, the page is told how many rows were dropped (X-Log-Lost).
 * 7) THE TIME NEVER RUNS BACKWARD: a phone whose clock is behind cannot
 * rewind the anchor.
 * 8) The row buffer is proven at compile time to hold the widest possible
 * row, and a truncation is reported instead of written.
 * Kept one for one from the old file, because they are user orders, not
 * choices: boot row with no board at all, change-only rows, cut-band
 * hysteresis, one voltage-step row a minute (rises in discharge always),
 * mains rows inside a short window, one-hour heartbeat (counted on the log
 * clock, so a battery power cut cannot reset it), day 0 for a panel
 * that has never seen a phone, retention by day count and byte budget,
 * RAM ring when the file system is off.
 */
#include <string.h>
/* ==================== Clock ==================== */
/*
 * [EN] The panel has no RTC and the technical link carries no board time (that
 * field exists only in the simple panel's DIAG_DATA), so the phone that
 * opens the page is the only clock: now = anchor + elapsed millis. Rejected
 * time cannot rewind it (see func__EspLog_TimeSet).
 */
static uint32_t UINT32_T__G__LogAnchorSec = 0u;
static uint32_t UINT32_T__G__LogAnchorMs = 0u;
static int32_t  INT32_T__G__LogTzSec = 0;
static bool     BOOL__G__LogClockValid = false;
/*
 * [EN] THE IMPORTANT ONE: the panel is not the clock. The STM32 stays powered
 * when the panel is cut, reboots or is unplugged, and its HELLO_ACK
 * carries its own uptime. So the wall clock is anchored to the BOARD's
 * uptime, not to millis(): the panel can lose power for an hour and still
 * know the real time with no browser anywhere.
 */
static uint32_t UINT32_T__G__LogAnchorBoardSec = 0u;
static bool     BOOL__G__LogAnchorBoardValid = false;
static uint32_t UINT32_T__G__LogBoardNowSec = 0u;
static bool     BOOL__G__LogBoardAlive = false;
/*
 * [EN] The latest time this panel has ever believed. The clock may stand still
 * when nobody can tell it the time, but it NEVER goes back: a history that
 * jumps backwards writes the same rows twice in two different files.
 */
static uint32_t UINT32_T__G__LogHighWaterSec = 0u;
/*
 * [EN] set when the browser moved the clock by a lot; the next tick writes the
 * row that says so.
 */
static bool     BOOL__G__LogClockJumpPending = false;
/*
 * [EN] The state the flash remembers from before the power went off. It is read
 * ONCE at boot and used to compare "before" with "now": if any logged
 * condition changed while the panel was dead, the change is written; if
 * nothing changed, the panel says nothing and waits.
 */
static bool     BOOL__G__LogBootSnapValid = false;
static uint32_t UINT32_T__G__LogBootV24 = 0u;
static uint32_t UINT32_T__G__LogBootFault = 0u;
static uint32_t UINT32_T__G__LogBootVlow = 0u;
static uint32_t UINT32_T__G__LogBootVhigh = 0u;
static uint8_t  UINT8_T__G__LogBootFlags2 = 0u;
static uint8_t  UINT8_T__G__LogBootChg1 = 0u;
static uint8_t  UINT8_T__G__LogBootChg2 = 0u;
static uint8_t  UINT8_T__G__LogBootMains = 0u;
static uint8_t  UINT8_T__G__LogBootDanger = 0u;
/*
 * [EN] v1.110: the condemned-battery mask and the status byte are logged
 * conditions too, so the last row carries them and the first frame after
 * a restart is compared against it like every other condition.
 */
static uint32_t UINT32_T__G__LogBootDead = 0u;
static uint8_t  UINT8_T__G__LogBootFl = 0u;
static uint32_t UINT32_T__G__LogLastClockSaveMs = 0u;
static bool     BOOL__G__LogClockSavedOnce = false;
/*
 * [EN] A restored board anchor needs one fresh HELLO_ACK before the first row
 * is stamped. The wait is bounded so a silent board still gets its boot
 * row, and a browser clock can release it early.
 */
static bool     BOOL__G__LogBootClockPending = false;
static uint32_t UINT32_T__G__LogBootMs = 0u;
/* ==================== Open day ==================== */
/*
 * [EN] The sentinel says "no day opened yet". Day 0 is a real day: the day of a
 * panel that has never seen a phone. Storage must never wait for the clock.
 */
static uint32_t UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
static uint16_t UINT16_T__G__LogRowsToday = 0u;
static bool     BOOL__G__LogDayFull = false;
static bool     BOOL__G__LogBootRowPending = true;
/*
 * [EN] 0 = the last flash write was verified; anything else = failed writes
 * since the last good one. This is what lets the page tell "nothing
 * happened" from "I could not write".
 */
static uint8_t  UINT8_T__G__LogWriteFault = 0u;
static uint16_t UINT16_T__G__LogDroppedRows = 0u;
static bool     BOOL__G__LogDrainWait = false;
static uint32_t UINT32_T__G__LogLastDrainMs = 0u;
/* ==================== Previous snapshot ==================== */
static bool     BOOL__G__LogHasPrev = false;
static uint8_t  UINT8_T__G__LogPrevMains = 0u;
/*
 * [EN] v1.95: the board's general byte (hand mode, snapshot, valid
 * measurement) is logged too, so a mode change is an event.
 */
static uint8_t  UINT8_T__G__LogPrevFl = 0u;
/*
 * [EN] v1.95: the condemned-battery verdict is a state too, and it is
 * NOT in the fault mask - it has its own field on the wire.
 */
static uint32_t UINT32_T__G__LogPrevDead = 0u;
/*
 * [EN] v1.96: the pack voltage itself is a change trigger, in visible
 * steps - that is what draws the charge curve in the log.
 */
static uint32_t UINT32_T__G__LogPrevV24 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg1 = 0u;
static uint8_t  UINT8_T__G__LogPrevChg2 = 0u;
static uint8_t  UINT8_T__G__LogPrevFlags2 = 0u;
static uint8_t  UINT8_T__G__LogPrevBand = 0u;
static uint32_t UINT32_T__G__LogPrevFault = 0u;
static uint8_t  UINT8_T__G__LogPrevNote = 0u;
/* ==================== Pending change ==================== */
/*
 * [EN] A firmware update was seen at this boot, so its row must be written no
 * matter what.
 */
static bool     BOOL__G__LogUpdatePending = false;
/*
 * [EN] One-shot: the next row ignores the day cap too.
 */
static bool     BOOL__A__LogForceRow = false;
static uint16_t UINT16_T__G__LogPendingMask = 0u;
static uint8_t  UINT8_T__G__LogPendingNote = 0u;
static bool     BOOL__G__LogMainsQueuedValid = false;
static uint32_t UINT32_T__G__LogMainsQueuedMs = 0u;
static uint32_t UINT32_T__G__LogLastRowMs = 0u;
static bool     BOOL__G__LogLastRowValid = false;
/*
 * [EN] The wall second the last written row belongs to. The heartbeat is judged
 * with THIS, not with millis(): on battery the board switches this panel off
 * every few minutes, so an uptime counter starts again from zero and the
 * hour is never reached (user order 2026-11-01). It rides in the clock file,
 * so the hour also survives the power cut.
 */
static uint32_t UINT32_T__G__LogLastBeatSec = 0u;
static bool     BOOL__G__LogLastBeatValid = false;
/*
 * [EN] v1.115: a row was written, so the clock file should follow it. Handled
 * by Tick, rate limited by ESP_LOG_CLOCK_SAVE_MIN_MS.
 */
static bool     BOOL__G__LogClockSaveWanted = false;
/*
 * [EN] v1.116: a new board anchor was taken in this session and the flash does
 * not have it yet. Written by the next Tick at once (one write per wake, not
 * rate limited): without it the next wake had to guess how long the board had
 * been running, and every guess threw away one sleep - the field report was a
 * night that fell six hours behind and a day whose first rows were missing.
 */
static bool     BOOL__G__LogAnchorSaveWanted = false;
static bool     BOOL__G__LogLinkDown = false;
/*
 * [EN] when the boot row of this session was written; the "board silent"
 * clock runs from here even if no frame ever arrived.
 */
static uint32_t UINT32_T__G__LogSessionStartMs = 0u;
/*
 * [EN] v1.98: when the last voltage-step row was written, so a load that
 * switches on and off cannot turn the step into a strobe.
 */
static uint32_t UINT32_T__G__LogLastStepMs = 0u;
static bool     BOOL__G__LogLastStepValid = false;
/*
 * [EN] the day-full marker is written once per day, never twice.
 */
static bool     BOOL__G__LogMarkerDone = false;
/* ==================== Day list ==================== */
/*
 * [EN] The days are NOT stored anywhere: this is a cache of the directory,
 * filled by func__EspLog_ScanDays from the file names on the flash.
 */
static uint32_t UINT32_T__G__LogIndexDay[ESP_LOG_MAX_DAYS + 2u];
/*
 * [EN] size of each day file, read from the directory in the same scan; the
 * retention needs it and asking the flash once is enough.
 */
static uint32_t UINT32_T__G__LogIndexBytes[ESP_LOG_MAX_DAYS + 2u];
static uint8_t  UINT8_T__G__LogIndexCount = 0u;
/*
 * [EN] RAM ring: the last rows survive when the flash cannot take them - either
 * because the file system is off or because a write just failed. It is
 * drained back into the day files as soon as the flash works again.
 */
static char     CHAR__G__LogRamRow[ESP_LOG_RAM_ROWS][ESP_LOG_RAM_ROW_MAX];
static uint8_t  UINT8_T__G__LogRamHead = 0u;
static uint8_t  UINT8_T__G__LogRamCount = 0u;
/* ==================== Change bits ==================== */
#define ESP_LOG_CHG_BOOT    0x0001u
#define ESP_LOG_CHG_MAINS   0x0002u
#define ESP_LOG_CHG_CHG1    0x0004u
#define ESP_LOG_CHG_CHG2    0x0008u
#define ESP_LOG_CHG_FAULT   0x0010u
#define ESP_LOG_CHG_FLAGS   0x0020u
#define ESP_LOG_CHG_BAND    0x0040u
#define ESP_LOG_CHG_LINK    0x0080u
#define ESP_LOG_CHG_MODE    0x0100u
#define ESP_LOG_CHG_DEAD    0x0200u
#define ESP_LOG_CHG_VSTEP   0x0400u
#define ESP_LOG_CHG_CLOCK   0x0800u
#define ESP_LOG_CHG_UPDATE  0x1000u
/* ==================== Notes ==================== */
#define ESP_LOG_NOTE_NONE      0u
#define ESP_LOG_NOTE_LOW_STOP  1u
#define ESP_LOG_NOTE_LINK_LOST 2u
#define ESP_LOG_NOTE_LINK_BACK 3u
#define ESP_LOG_NOTE_DAY_FULL  4u
#define ESP_LOG_NOTE_HEARTBEAT 5u
#define ESP_LOG_NOTE_MANUAL    6u
/*
 * [EN] v1.117: the first row of a day file: "the log of this day starts here".
 * It carries the real second of the first wake after midnight, never a fake
 * 00:00 (user order 2026-11-02: "the moment the file is made, write a record").
 */
#define ESP_LOG_NOTE_DAY_START 7u
#define ESP_LOG_PATH_MAX       24u
/*
 * [EN] One CSV row: 22 numbers, the widest possible being 22 x 10 digits plus
 * 21 commas = 241 bytes plus the terminator. The static_assert below turns
 * a future column that does not fit into a build error instead of a
 * silently cut row.
 */
#define ESP_LOG_ROW_MAX        256u
#define ESP_LOG_ROW_WORST_CASE 244u
static_assert(ESP_LOG_ROW_MAX >= ESP_LOG_ROW_WORST_CASE,
              "ESP_LOG_ROW_MAX must hold the widest row: 22 x 10 digits + 21 commas");
/*
 * [EN] The RAM ring keeps a copy of the row when the flash refuses it, so its
 * slot must hold the same widest row. This assert is what stops the two sizes
 * from drifting apart again (160 bytes cut long rows in silence).
 */
static_assert(ESP_LOG_RAM_ROW_MAX >= ESP_LOG_ROW_WORST_CASE,
              "ESP_LOG_RAM_ROW_MAX must hold the widest row, or the RAM ring cuts it");
/*
 * [EN] The value the /evt answer carries in X-Log-Fs: 1 = rows come from the
 * flash, 0 = the file system is off (RAM ring), 2 = the file system is
 * mounted but a write failed (RAM ring holds the newest rows).
 */
#define ESP_LOG_FS_OK          1u
#define ESP_LOG_FS_OFF         0u
#define ESP_LOG_FS_WRITE_FAULT 2u
/* ==================== Time helpers ==================== */
/**
 * @brief  [EN] Current epoch seconds from the phone anchor, 0 when unknown.
* @return uint32_t / uint32_t
 */
static uint32_t func__EspLog_NowSec(void)
{
    uint32_t uint32_t__elapsedMs;
    uint32_t uint32_t__panelSec;
    if (BOOL__G__LogClockValid == false)
    {
        return 0u;
    }
    /*
 * [EN] What this panel can say on its own: the anchor plus its own uptime.
 */
    uint32_t__elapsedMs = (uint32_t)millis() - UINT32_T__G__LogAnchorMs;
    uint32_t__panelSec = UINT32_T__G__LogAnchorSec + (uint32_t__elapsedMs / 1000u);
    /*
 * [EN] What the BOARD can say: it kept counting while this panel was dead.
 * Whichever is further ahead is the truth, so the clock never rewinds
 * and never loses the time the panel was not running.
 */
    if ((BOOL__G__LogAnchorBoardValid != false) && (BOOL__G__LogBoardAlive != false) &&
        (UINT32_T__G__LogBoardNowSec >= UINT32_T__G__LogAnchorBoardSec))
    {
        uint32_t uint32_t__boardSec = UINT32_T__G__LogAnchorSec +
            (UINT32_T__G__LogBoardNowSec - UINT32_T__G__LogAnchorBoardSec);
        if (uint32_t__boardSec > uint32_t__panelSec)
        {
            uint32_t__panelSec = uint32_t__boardSec;
        }
    }
    if (uint32_t__panelSec < UINT32_T__G__LogHighWaterSec)
    {
        return UINT32_T__G__LogHighWaterSec;   /* never back */
    }
    UINT32_T__G__LogHighWaterSec = uint32_t__panelSec;
    return uint32_t__panelSec;
}
/**
 * @brief  [EN] The board said how long it has been running (HELLO_ACK).
 *              First answer after boot: anchor to it without any jump. Answer
 *              with a SMALLER uptime than the anchor means the board itself
 *              restarted, so that anchor is dead and a new one is taken.
* @param uint32_t uint32_t__boardUptimeSec board uptime, s /
* @return None /
 */
static void func__EspLog_NoteBoardTime(uint32_t uint32_t__boardUptimeSec)
{
    if (uint32_t__boardUptimeSec == 0u)
    {
        BOOL__G__LogBoardAlive = false;
        return;
    }
    UINT32_T__G__LogBoardNowSec = uint32_t__boardUptimeSec;
    BOOL__G__LogBoardAlive = true;
    if (BOOL__G__LogAnchorBoardValid != false)
    {
        if (uint32_t__boardUptimeSec < UINT32_T__G__LogAnchorBoardSec)
        {
            BOOL__G__LogAnchorBoardValid = false;   /* the board restarted */
        }
        else
        {
            return;   /* the anchor still stands */
        }
    }
    if (BOOL__G__LogClockValid == false)
    {
        return;
    }
    /*
 * [EN] fresh anchor at the time the panel believes right now: no jump.
 */
    UINT32_T__G__LogAnchorSec = func__EspLog_NowSec();
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    UINT32_T__G__LogAnchorBoardSec = uint32_t__boardUptimeSec;
    BOOL__G__LogAnchorBoardValid = true;
    BOOL__G__LogAnchorSaveWanted = true;
}
/* ==================== Boot clock grace ==================== */
/**
 * @brief  [EN] May the first automatic row be stamped now? A restored board
 *              anchor waits for one live uptime answer, but never longer than
 *              ESP_LOG_BOOT_CLOCK_GRACE_MS. No saved anchor means no wait.
* @param uint32_t uint32_t__nowMs current millis(), ms /
* @return bool ready /
 */
static bool func__EspLog_BootClockReady(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__waitedMs;
    if (BOOL__G__LogBootClockPending == false)
    {
        return true;
    }
    if ((BOOL__G__LogBoardAlive != false) ||
        (BOOL__G__LogClockValid == false) ||
        (BOOL__G__LogAnchorBoardValid == false))
    {
        BOOL__G__LogBootClockPending = false;
        return true;
    }
    uint32_t__waitedMs = uint32_t__nowMs - UINT32_T__G__LogBootMs;
    if (uint32_t__waitedMs >= ESP_LOG_BOOT_CLOCK_GRACE_MS)
    {
        BOOL__G__LogBootClockPending = false;
        return true;
    }
    return false;
}
/**
 * @brief  [EN] Local day number (days since 1970-01-01) of an epoch second.
* @param uint32_t uint32_t__epochSec epoch seconds /
* @return uint32_t / uint32_t
 */
static uint32_t func__EspLog_LocalDay(uint32_t uint32_t__epochSec)
{
    int32_t int32_t__local;
    int32_t__local = (int32_t)uint32_t__epochSec + INT32_T__G__LogTzSec;
    if (int32_t__local < 0)
    {
        return 0u;
    }
    return (uint32_t)(int32_t__local / 86400);
}
/**
 * @brief  [EN] Civil date (Gregorian) of a day number, packed as YYYYMMDD.
* @param uint32_t uint32_t__days days since 1970-01-01 /
* @return uint32_t packed date, 0 when unknown /
 */
static uint32_t func__EspLog_DateCode(uint32_t uint32_t__days)
{
    uint32_t uint32_t__z;
    uint32_t uint32_t__era;
    uint32_t uint32_t__doe;
    uint32_t uint32_t__yoe;
    uint32_t uint32_t__doy;
    uint32_t uint32_t__mp;
    uint32_t uint32_t__year;
    uint32_t uint32_t__month;
    uint32_t uint32_t__day;
    /*
 * [EN] Howard Hinnant's civil_from_days, all-unsigned for dates after 1970.
 */
    uint32_t__z = uint32_t__days + 719468u;
    uint32_t__era = uint32_t__z / 146097u;
    uint32_t__doe = uint32_t__z - (uint32_t__era * 146097u);
    uint32_t__yoe = (uint32_t__doe - (uint32_t__doe / 1460u) +
                     (uint32_t__doe / 36524u) - (uint32_t__doe / 146096u)) / 365u;
    uint32_t__year = uint32_t__yoe + (uint32_t__era * 400u);
    uint32_t__doy = uint32_t__doe - ((365u * uint32_t__yoe) +
                                     (uint32_t__yoe / 4u) - (uint32_t__yoe / 100u));
    uint32_t__mp = ((5u * uint32_t__doy) + 2u) / 153u;
    uint32_t__day = uint32_t__doy - (((153u * uint32_t__mp) + 2u) / 5u) + 1u;
    uint32_t__month = (uint32_t__mp < 10u) ? (uint32_t__mp + 3u) : (uint32_t__mp - 9u);
    if (uint32_t__month <= 2u)
    {
        uint32_t__year = uint32_t__year + 1u;
    }
    if ((uint32_t__year > 9999u) || (uint32_t__month > 12u) || (uint32_t__day > 31u))
    {
        return 0u;
    }
    return (uint32_t__year * 10000u) + (uint32_t__month * 100u) + uint32_t__day;
}
/**
 * @brief  [EN] Day code of "now" (0 when the phone clock is unknown).
* @return uint32_t / uint32_t
 */
static uint32_t func__EspLog_TodayCode(void)
{
    uint32_t uint32_t__nowSec;
    uint32_t__nowSec = func__EspLog_NowSec();
    if (uint32_t__nowSec == 0u)
    {
        return 0u;
    }
    return func__EspLog_DateCode(func__EspLog_LocalDay(uint32_t__nowSec));
}
/**
 * @brief  [EN] Build a day file path. Day 0 (clock unknown) gets its own file.
* @param char *char__path output /
* @param uint32_t uint32_t__dayCode YYYYMMDD or 0 /
* @return None /
 */
static void func__EspLog_PathOf(char *char__path, uint32_t uint32_t__dayCode)
{
    (void)snprintf(char__path, ESP_LOG_PATH_MAX, "%s%08lu.csv",
                   ESP_LOG_FILE_PREFIX, (unsigned long)uint32_t__dayCode);
}
/**
 * @brief  [EN] The leading number of a CSV row (its epoch field).
* @param const char *char__row row /
* @return uint32_t epoch, 0 when the row has no clock /
 */
static uint32_t func__EspLog_RowEpoch(const char *char__row)
{
    uint32_t uint32_t__value = 0u;
    uint8_t uint8_t__index = 0u;
    if (char__row == NULL)
    {
        return 0u;
    }
    while ((uint8_t__index < 10u) &&
           (char__row[uint8_t__index] >= '0') &&
           (char__row[uint8_t__index] <= '9'))
    {
        uint32_t__value = (uint32_t__value * 10u) +
                          (uint32_t)(char__row[uint8_t__index] - '0');
        uint8_t__index++;
    }
    return uint32_t__value;
}
/* ==================== Clock storage ==================== */
/**
 * @brief  [EN] Write one line into the clock file ATOMICALLY: the temporary file
 *              first, the rename at the end.
 *              An in-place "w" truncates the old clock the moment it opens, so
 *              a power cut inside the write - routine on battery, where the
 *              board cuts this panel's power - used to leave an empty clock and
 *              lose the time, the timezone and the heartbeat second with it.
 *              lfs_rename replaces the destination in one step, so the clock is
 *              always either the whole old line or the whole new one.
* @param const char *char__ptr_line full line, newline included /
 * @return bool true when the clock file now holds it /
 */
static bool func__EspLog_ClockStore(const char *char__ptr_line)
{
    File file__clock;
    size_t size_t__want;
    size_t size_t__got;
    if ((BOOL__G__FsOk == false) || (char__ptr_line == NULL))
    {
        return false;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_TMP_PATH, "w");
    if (!file__clock)
    {
        return false;
    }
    size_t__want = strlen(char__ptr_line);
    size_t__got = (size_t)file__clock.print(char__ptr_line);
    file__clock.close();
    if (size_t__got != size_t__want)
    {
        (void)LittleFS.remove(ESP_LOG_CLOCK_TMP_PATH);   /* half a file is no clock */
        return false;
    }
    if (LittleFS.rename(ESP_LOG_CLOCK_TMP_PATH, ESP_LOG_CLOCK_PATH) == false)
    {
        (void)LittleFS.remove(ESP_LOG_CLOCK_TMP_PATH);
        return false;
    }
    return true;
}
/**
 * @brief  [EN] Persist the clock anchor (every 10 minutes and on adoption).
* @return None /
 */
static void func__EspLog_ClockSave(void)
{
    uint32_t uint32_t__nowSec;
    char char__line[64];
    if ((BOOL__G__FsOk == false) || (BOOL__G__LogClockValid == false))
    {
        return;
    }
    uint32_t__nowSec = func__EspLog_NowSec();
    if (uint32_t__nowSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return;
    }
    /*
 * [EN] Three fields: the wall clock, the timezone, and the BOARD uptime the
 * wall clock belongs to. With those three, a panel that has just been
 * switched on knows the time again without asking anybody.
 */
    /*
 * [EN] v1.113: a fourth field carries the firmware version, so the next
 * boot can tell "switched on again" from "flashed with new firmware"
 * (user order 2026-10-28). It rides in a file that already exists and
 * is never erased by clearing the history, so no new file is needed.
 */
    /*
 * [EN] Save a matched pair: this wall second belongs to the board uptime
 * received most recently. Saving the old anchor uptime here would
 * count the interval since that anchor twice after the next boot.
 * With no live board answer there is no trustworthy pair, so 0.
 */
    {
        uint32_t uint32_t__boardUptimeSec = 0u;
        if ((BOOL__G__LogBoardAlive != false) && (UINT32_T__G__LogBoardNowSec != 0u))
        {
            uint32_t__boardUptimeSec = UINT32_T__G__LogBoardNowSec;
        }
        /*
 * [EN] v1.116: the board is silent in this session, but the pair this panel
 * already holds is still true - the board keeps counting while the panel is
 * off, so its uptime grew by the same wall seconds. Writing 0 here (v1.114
 * did) threw away the only clock that survives a sleep: the next wake
 * restarted from the saved second and counted only its own uptime, so a
 * night on battery fell hours behind and the early rows landed in the
 * previous day's file (field report 2026-11-02: "the first hours of the day
 * are missing, writing started six hours later"). Advance the pair instead.
 */
        else if ((BOOL__G__LogAnchorBoardValid != false) &&
                 (UINT32_T__G__LogAnchorBoardSec != 0u) &&
                 (uint32_t__nowSec >= UINT32_T__G__LogAnchorSec))
        {
            uint32_t__boardUptimeSec = UINT32_T__G__LogAnchorBoardSec +
                (uint32_t__nowSec - UINT32_T__G__LogAnchorSec);
            UINT32_T__G__LogAnchorBoardSec = uint32_t__boardUptimeSec;
            UINT32_T__G__LogAnchorSec = uint32_t__nowSec;
            UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        }
        /*
 * [EN] A fifth field carries the wall second of the last written row, so the
 * heartbeat hour is not restarted by a power cut. Zero = not known yet,
 * exactly like the board anchor.
 */
        (void)snprintf(char__line, sizeof(char__line), "%lu,%ld,%lu,%u,%lu\n",
                       (unsigned long)uint32_t__nowSec, (long)INT32_T__G__LogTzSec,
                       (unsigned long)uint32_t__boardUptimeSec,
                       (unsigned int)ESP_ESP_FW_VERSION,
                       (unsigned long)((BOOL__G__LogLastBeatValid != false) ?
                                       UINT32_T__G__LogLastBeatSec : 0u));
    }
    if (func__EspLog_ClockStore(char__line) == false)
    {
        return;   /* the old clock stays; the timer is not moved, so we retry */
    }
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
    BOOL__G__LogClockSavedOnce = true;
}
/*
 * [EN] Defined below; the clock reader uses it to find the fifth field.
 */
static int32_t func__EspLog_FieldStart(String string__line, uint8_t uint8_t__field);
/**
 * @brief  [EN] Read the clock anchor back after a restart.
* @return None /
 */
static void func__EspLog_ClockLoad(void)
{
    File file__clock;
    String string__content;
    int32_t int32_t__split;
    uint32_t uint32_t__savedSec;
    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_CLOCK_PATH) == false))
    {
        return;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
    if (!file__clock)
    {
        return;
    }
    string__content = file__clock.readString();
    file__clock.close();
    int32_t__split = string__content.indexOf(',');
    if (int32_t__split <= 0)
    {
        return;
    }
    uint32_t__savedSec = (uint32_t)string__content.substring(0, int32_t__split).toInt();
    if (uint32_t__savedSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return;
    }
    UINT32_T__G__LogAnchorSec = uint32_t__savedSec;
    UINT32_T__G__LogAnchorMs = (uint32_t)millis();
    {
        String string__rest = string__content.substring(int32_t__split + 1);
        int32_t int32_t__second = string__rest.indexOf(',');
        if (int32_t__second > 0)
        {
            INT32_T__G__LogTzSec = (int32_t)string__rest.substring(0, int32_t__second).toInt();
            /*
 * [EN] v1.113: a fourth field (the firmware stamp) follows the
 * board anchor, so the anchor stops at the next comma.
 */
            {
                String string__board = string__rest.substring(int32_t__second + 1);
                int32_t int32_t__third = string__board.indexOf(',');
                if (int32_t__third >= 0)
                {
                    string__board = string__board.substring(0, int32_t__third);
                }
                UINT32_T__G__LogAnchorBoardSec = (uint32_t)string__board.toInt();
            }
        }
        else
        {
            INT32_T__G__LogTzSec = (int32_t)string__rest.toInt();
            UINT32_T__G__LogAnchorBoardSec = 0u;
        }
    }
    /*
 * [EN] An old file with two fields still loads; it simply has no board
 * anchor, and the first HELLO_ACK makes one.
 */
    BOOL__G__LogAnchorBoardValid = (UINT32_T__G__LogAnchorBoardSec != 0u);
    BOOL__G__LogClockValid = true;
    UINT32_T__G__LogHighWaterSec = uint32_t__savedSec;
    /*
 * [EN] The fifth field is the wall second of the last written row; an older
 * file has none, and the first row of this session makes one.
 */
    {
        int32_t int32_t__beatStart = func__EspLog_FieldStart(string__content, 5u);
        if (int32_t__beatStart > 0)
        {
            String string__beat = string__content.substring((size_t)int32_t__beatStart);
            uint32_t uint32_t__beatSec;
            string__beat.trim();
            uint32_t__beatSec = (uint32_t)string__beat.toInt();
            if (uint32_t__beatSec >= ESP_LOG_CLOCK_VALID_MIN)
            {
                UINT32_T__G__LogLastBeatSec = uint32_t__beatSec;
                BOOL__G__LogLastBeatValid = true;
            }
        }
    }
}
/* ==================== Firmware stamp ==================== */
/**
 * @brief  [EN] Where does field N (1-based, N>1) start in a comma separated
 *              line? -1 when the line has fewer fields.
* @param String string__line line /
* @param uint8_t uint8_t__field field number /
* @return start index /
 */
static int32_t func__EspLog_FieldStart(String string__line, uint8_t uint8_t__field)
{
    int32_t int32_t__offset = 0;
    uint8_t uint8_t__seen = 1u;
    while (uint8_t__seen < uint8_t__field)
    {
        int32_t int32_t__comma = (int32_t)string__line.indexOf(',');
        if (int32_t__comma < 0)
        {
            return -1;
        }
        int32_t__offset += int32_t__comma + 1;
        string__line = string__line.substring((size_t)int32_t__comma + 1u);
        uint8_t__seen++;
    }
    return int32_t__offset;
}
/**
 * @brief  [EN] Fourth field of the clock file: which firmware version wrote it
 *              last. 0 = nothing remembered yet (a fresh flash), never "update".
* @return uint32_t version /
 */
static uint32_t func__EspLog_FwStampRead(void)
{
    String string__content;
    int32_t int32_t__start;
    File file__clock;
    if ((BOOL__G__FsOk == false) || (LittleFS.exists(ESP_LOG_CLOCK_PATH) == false))
    {
        return 0u;
    }
    file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
    if (!file__clock)
    {
        return 0u;
    }
    string__content = file__clock.readString();
    file__clock.close();
    /*
 * [EN] the fourth field starts after the third comma
 */
    int32_t__start = func__EspLog_FieldStart(string__content, 4u);
    if (int32_t__start < 0)
    {
        return 0u;
    }
    {
        String string__value = string__content.substring((size_t)int32_t__start);
        string__value.trim();
        if (string__value.length() == 0u)
        {
            return 0u;
        }
        return (uint32_t)string__value.toInt();
    }
}
/**
 * @brief  [EN] Rewrite the clock file with the running firmware version as its
 *              fourth field. The first three fields and the fifth (the
 *              heartbeat second) are kept as they are, so this can never
 *              lose or rewind the clock.
* @param uint32_t uint32_t__version version /
* @return None /
 */
static void func__EspLog_FwStampWrite(uint32_t uint32_t__version)
{
    String string__content = String("0,0,0");
    String string__firstThree;
    String string__beat = String("0");
    int32_t int32_t__start;
    int32_t int32_t__beatStart;
    char char__line[80];
    File file__clock;
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    if (LittleFS.exists(ESP_LOG_CLOCK_PATH) != false)
    {
        file__clock = LittleFS.open(ESP_LOG_CLOCK_PATH, "r");
        if (file__clock)
        {
            string__content = file__clock.readString();
            file__clock.close();
        }
    }
    string__content.trim();
    int32_t__start = func__EspLog_FieldStart(string__content, 4u);
    /*
 * [EN] Keep the clock fields that already exist. A legacy three-field line
 * is kept whole; a two-field line gets an explicit zero board anchor
 * so the firmware stamp can never be mistaken for an uptime.
 */
    if (int32_t__start > 1)
    {
        string__firstThree = string__content.substring(0, (size_t)(int32_t__start - 1));
    }
    else if (func__EspLog_FieldStart(string__content, 3u) > 1)
    {
        string__firstThree = string__content;
    }
    else if (func__EspLog_FieldStart(string__content, 2u) > 1)
    {
        string__firstThree = string__content + String(",0");
    }
    else
    {
        string__firstThree = String("0,0,0");
    }
    /*
 * [EN] The heartbeat second (fifth field) is carried over untouched: a stamp
 * write must never restart the silent-hour clock.
 */
    int32_t__beatStart = func__EspLog_FieldStart(string__content, 5u);
    if (int32_t__beatStart > 0)
    {
        string__beat = string__content.substring((size_t)int32_t__beatStart);
        string__beat.trim();
        if (string__beat.length() == 0u)
        {
            string__beat = String("0");
        }
    }
    (void)snprintf(char__line, sizeof(char__line), "%s,%u,%s\n",
                   string__firstThree.c_str(), (unsigned int)uint32_t__version,
                   string__beat.c_str());
    (void)func__EspLog_ClockStore(char__line);
}
/**
 * @brief  [EN] Is this boot the first one of a new firmware? The remembered
 *              version is compared with the running one; "nothing remembered"
 *              is a fresh flash and is not an update. Either way the stamp is
 *              brought up to date.
* @return None /
 */
static void func__EspLog_FwCheck(void)
{
    uint32_t uint32_t__stored;
    uint32_t__stored = func__EspLog_FwStampRead();
    if ((uint32_t__stored != 0u) && (uint32_t__stored != (uint32_t)ESP_ESP_FW_VERSION))
    {
        BOOL__G__LogUpdatePending = true;
    }
    if (uint32_t__stored != (uint32_t)ESP_ESP_FW_VERSION)
    {
        func__EspLog_FwStampWrite((uint32_t)ESP_ESP_FW_VERSION);
    }
}
/**
 * @brief  [EN] Adopt the phone's clock (POST /clock). A time that would move the
 *              clock BACKWARD by more than two minutes is refused: two phones, a
 *              wrong timezone or a manual clock change must not rewind the day
 *              and split the history into two files with the same rows again.
* @param uint32_t uint32_t__epochSec epoch seconds /
* @param int32_t int32_t__tzSec UTC offset in seconds / UTC
* @return bool, false when the time is not believable /
 */
static bool func__EspLog_TimeSet(uint32_t uint32_t__epochSec, int32_t int32_t__tzSec)
{
    uint32_t uint32_t__nowSec;
    bool bool__jump = false;
    if (uint32_t__epochSec < ESP_LOG_CLOCK_VALID_MIN)
    {
        return false;
    }
    if ((int32_t__tzSec < -50400) || (int32_t__tzSec > 50400))
    {
        int32_t__tzSec = 0;
    }
    /*
 * [EN] The browser is the boss of the clock (user order 2026-10-27): every
 * time a page connects, the clock becomes the time of that machine. A
 * small difference is a normal re-sync and stays silent. A big one - a
 * wrong clock from yesterday, a phone in another country, a board that
 * was off for a day - is applied AND written as its own row, so the
 * jump in the history always explains itself.
 */
    if (BOOL__G__LogClockValid != false)
    {
        uint32_t__nowSec = func__EspLog_NowSec();
        if ((uint32_t__nowSec > uint32_t__epochSec) &&
            ((uint32_t__nowSec - uint32_t__epochSec) > ESP_LOG_CLOCK_REWIND_SEC))
        {
            BOOL__G__LogClockJumpPending = true;
            bool__jump = true;
        }
        else if ((uint32_t__epochSec > uint32_t__nowSec) &&
                 ((uint32_t__epochSec - uint32_t__nowSec) > ESP_LOG_CLOCK_REWIND_SEC))
        {
            BOOL__G__LogClockJumpPending = true;
            bool__jump = true;
        }
        else
        {
            /* normal re-sync: a few seconds, nothing to report */
        }
    }
    {
        /*
 * [EN] v1.116: remember the pair that is being replaced. The first page poll
 * of a session often arrives before the board's first HELLO_ACK, and the old
 * code dropped the board anchor in exactly that case, then saved a 0 into the
 * clock file - the root of the "the first hours of the day are missing"
 * report. The board keeps counting while this panel is off, so its uptime
 * advanced by the same wall seconds: carry the pair forward.
 */
        const uint32_t uint32_t__oldAnchorSec = UINT32_T__G__LogAnchorSec;
        const uint32_t uint32_t__oldBoardSec = UINT32_T__G__LogAnchorBoardSec;
        const bool bool__oldBoardValid = BOOL__G__LogAnchorBoardValid;
        UINT32_T__G__LogAnchorSec = uint32_t__epochSec;
        UINT32_T__G__LogAnchorMs = (uint32_t)millis();
        INT32_T__G__LogTzSec = int32_t__tzSec;
        BOOL__G__LogClockValid = true;
        UINT32_T__G__LogHighWaterSec = uint32_t__epochSec;
        /*
 * [EN] The phone says WHAT time it is; the board says how time passes from
 * here on.
 */
        if ((BOOL__G__LogBoardAlive != false) && (UINT32_T__G__LogBoardNowSec != 0u))
        {
            UINT32_T__G__LogAnchorBoardSec = UINT32_T__G__LogBoardNowSec;
            BOOL__G__LogAnchorBoardValid = true;
        }
        else if ((bool__oldBoardValid != false) && (uint32_t__oldBoardSec != 0u))
        {
            int64_t int64_t__boardSec = (int64_t)uint32_t__oldBoardSec +
                ((int64_t)uint32_t__epochSec - (int64_t)uint32_t__oldAnchorSec);
            if (int64_t__boardSec > 0)
            {
                UINT32_T__G__LogAnchorBoardSec = (uint32_t)int64_t__boardSec;
                BOOL__G__LogAnchorBoardValid = true;
            }
            else
            {
                BOOL__G__LogAnchorBoardValid = false;
            }
        }
        else
        {
            BOOL__G__LogAnchorBoardValid = false;
        }
    }
    /*
 * [EN] An accepted browser clock is an absolute time source too, so it
 * releases any boot-row grace wait immediately.
 */
    BOOL__G__LogBootClockPending = false;
    /*
 * [EN] v1.110 (finding of the line-by-line review, 2026-10-28): the page
 * sets the clock on every poll - once a minute - and this function
 * saved the anchor every time, so the flash was written 1440 times a
 * day instead of the 144 that ESP_LOG_CLOCK_SAVE_MS asks for. A small
 * re-sync now lives in RAM only; the anchor is saved when the clock
 * really jumped, when it has never been saved, or when the 10-minute
 * policy says so. A restart in between loses at most those minutes,
 * and the first page that connects sets the clock again.
 */
    if ((bool__jump != false) || (BOOL__G__LogClockSavedOnce == false) ||
        ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastClockSaveMs) >= ESP_LOG_CLOCK_SAVE_MS))
    {
        func__EspLog_ClockSave();
    }
    return true;
}
/* ==================== Flash write ==================== */
/**
 * @brief  [EN] Write one text piece and REPORT whether every byte landed.
 *              print() already returns the count it really wrote, so a full or
 *              damaged flash is visible here instead of silent.
* @param File &file__target open file /
* @param const char *char__text text /
* @return bool, true when the whole text was written /
 */
static bool func__EspLog_FileText(File &file__target, const char *char__text)
{
    size_t size_t__want;
    size_t size_t__got;
    if (char__text == NULL)
    {
        return false;
    }
    size_t__want = strlen(char__text);
    if (size_t__want == 0u)
    {
        return true;
    }
    size_t__got = file__target.print(char__text);
    return (size_t__got == size_t__want);
}
/**
 * @brief  [EN] Remember that a flash write failed (saturating, so a flash that
 *              is full forever cannot wrap the counter back to "healthy").
* @return None /
 */
static void func__EspLog_FaultRaise(void)
{
    if (UINT8_T__G__LogWriteFault < 0xFFu)
    {
        UINT8_T__G__LogWriteFault++;
    }
}
/**
 * @brief  [EN] Keep a row that the flash would not take, newest wins. Row N+1
 *              overwriting row 1 is deliberate: the newest state is what an
 *              operator needs, and the loss is counted, not hidden.
* @param const char *char__row one CSV row /
* @return None /
 */
static void func__EspLog_RamPush(const char *char__row)
{
    if (char__row == NULL)
    {
        return;
    }
    if (UINT8_T__G__LogRamCount >= ESP_LOG_RAM_ROWS)
    {
        if (UINT16_T__G__LogDroppedRows < 0xFFFFu)
        {
            UINT16_T__G__LogDroppedRows++;
        }
    }
    (void)strncpy(CHAR__G__LogRamRow[UINT8_T__G__LogRamHead],
                  char__row,
                  ESP_LOG_RAM_ROW_MAX - 1u);
    CHAR__G__LogRamRow[UINT8_T__G__LogRamHead][ESP_LOG_RAM_ROW_MAX - 1u] = '\0';
    UINT8_T__G__LogRamHead = (uint8_t)((UINT8_T__G__LogRamHead + 1u) % ESP_LOG_RAM_ROWS);
    if (UINT8_T__G__LogRamCount < ESP_LOG_RAM_ROWS)
    {
        UINT8_T__G__LogRamCount++;
    }
}
/* ==================== Day list ==================== */
/*
 * [EN] v1.110 (user order 2026-10-28: "why not take it from the log file
 * itself?"): the day list is READ FROM THE FLASH, not stored. One scan of
 * "/" collects every day file, sorts it by day and remembers each file's
 * size, so the day picker, the download and the retention all look at the
 * real files. The old list file is gone: a list written beside the rows
 * can fall out of step with them - a panel that was switched off came back
 * with an EMPTY list while every day file was still on the flash, and a
 * stale list also meant the retention never deleted anything.
 */
/*
 * [EN] When the directory was last read, so a page that refreshes every 10 s
 * does not make the flash walk the directory every 10 s as well.
 */
static uint32_t UINT32_T__G__LogLastScanMs = 0u;
static bool     BOOL__G__LogScanValid = false;
/**
 * @brief  [EN] Convert a raw directory entry into a canonical root path that
 *              always begins with '/'. Every internal log/FS routine uses this
 *              single "/..." representation.
* @param const char *char__ptr_entry directory entry /
* @param char *char__outPath output buffer /
* @param size_t size_t__outSize buffer capacity /
* @return bool true when valid /
 */
static bool func__EspLog_ToRootPath(const char *char__ptr_entry,
                                    char *char__outPath,
                                    size_t size_t__outSize)
{
    int int__n;
    if ((char__outPath == NULL) || (size_t__outSize < 3u))
    {
        return false;
    }
    char__outPath[0] = '\0';
    if ((char__ptr_entry == NULL) || (char__ptr_entry[0] == '\0'))
    {
        return false;
    }
    if (char__ptr_entry[0] == '/')
    {
        if (char__ptr_entry[1] == '\0')
        {
            return false;
        }
        int__n = snprintf(char__outPath, size_t__outSize, "%s", char__ptr_entry);
    }
    else
    {
        int__n = snprintf(char__outPath, size_t__outSize, "/%s", char__ptr_entry);
    }
    if ((int__n <= 1) || ((size_t)int__n >= size_t__outSize))
    {
        char__outPath[0] = '\0';
        return false;
    }
    return true;
}
/**
 * @brief  [EN] Day code of one canonical root path ("/evt-YYYYMMDD.csv"), or
 *              false when the path is not a day file.
* @param const char *char__ptr_path canonical "/..." path / «/»
* @param uint32_t *uint32_t__ptr_day output day code /
* @return bool /
 */
static bool func__EspLog_DayFromName(const char *char__ptr_path,
                                     uint32_t *uint32_t__ptr_day)
{
    const char *char__ptr_prefix = ESP_LOG_FILE_PREFIX;
    uint8_t uint8_t__idx = 0u;
    uint8_t uint8_t__digits = 0u;
    uint32_t uint32_t__day = 0u;
    if ((char__ptr_path == NULL) || (uint32_t__ptr_day == NULL))
    {
        return false;
    }
    while ((char__ptr_prefix[uint8_t__idx] != '\0') &&
           (char__ptr_path[uint8_t__idx] == char__ptr_prefix[uint8_t__idx]))
    {
        uint8_t__idx++;
    }
    if (char__ptr_prefix[uint8_t__idx] != '\0')
    {
        return false;
    }
    while ((uint8_t__digits < 8u) &&
           (char__ptr_path[uint8_t__idx] >= '0') &&
           (char__ptr_path[uint8_t__idx] <= '9'))
    {
        uint32_t__day = (uint32_t__day * 10u) +
                        (uint32_t)(char__ptr_path[uint8_t__idx] - '0');
        uint8_t__digits++;
        uint8_t__idx++;
    }
    if (uint8_t__digits != 8u)
    {
        return false;
    }
    if ((char__ptr_path[uint8_t__idx] != '.') ||
        (char__ptr_path[uint8_t__idx + 1u] != 'c') ||
        (char__ptr_path[uint8_t__idx + 2u] != 's') ||
        (char__ptr_path[uint8_t__idx + 3u] != 'v') ||
        (char__ptr_path[uint8_t__idx + 4u] != '\0'))
    {
        return false;
    }
    *uint32_t__ptr_day = uint32_t__day;
    return true;
}
/* ==================== Day scan helper ==================== */
/**
 * @brief  [EN] Insert one discovered day file into the sorted cache.
* @param const char *char__ptr_name file name /
* @param uint32_t uint32_t__size file size in bytes /
* @return None /
 */
static void func__EspLog_RecordDayEntry(const char *char__ptr_name,
                                        uint32_t uint32_t__size)
{
    uint32_t uint32_t__day = 0u;
    uint8_t uint8_t__slot;
    if (func__EspLog_DayFromName(char__ptr_name, &uint32_t__day) == false)
    {
        return;
    }
    if (UINT8_T__G__LogIndexCount >= (ESP_LOG_MAX_DAYS + 2u))
    {
        return;
    }
    uint8_t__slot = UINT8_T__G__LogIndexCount;
    while ((uint8_t__slot > 0u) &&
           (UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] > uint32_t__day))
    {
        UINT32_T__G__LogIndexDay[uint8_t__slot] =
            UINT32_T__G__LogIndexDay[uint8_t__slot - 1u];
        UINT32_T__G__LogIndexBytes[uint8_t__slot] =
            UINT32_T__G__LogIndexBytes[uint8_t__slot - 1u];
        uint8_t__slot--;
    }
    UINT32_T__G__LogIndexDay[uint8_t__slot] = uint32_t__day;
    UINT32_T__G__LogIndexBytes[uint8_t__slot] = uint32_t__size;
    UINT8_T__G__LogIndexCount++;
}
/**
 * @brief  [EN] Fill the day cache from one directory scan, oldest first. Two
 *              entries more than the retention keeps are taken, so the trim
 *              below always sees the oldest day and can delete it. A page that
 *              asks again inside ESP_LOG_SCAN_MIN_MS gets the cache without
 *              touching the flash.
* @param bool bool__force true = read the flash now / =
* @return uint8_t number of days found /
 */
static uint8_t func__EspLog_ScanDays(bool bool__force)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    if (BOOL__G__FsOk == false)
    {
        UINT8_T__G__LogIndexCount = 0u;
        BOOL__G__LogScanValid = false;
        return 0u;
    }
    if ((bool__force == false) && (BOOL__G__LogScanValid != false) &&
        ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastScanMs) < ESP_LOG_SCAN_MIN_MS))
    {
        return UINT8_T__G__LogIndexCount;
    }
    UINT32_T__G__LogLastScanMs = uint32_t__nowMs;
    BOOL__G__LogScanValid = true;
    UINT8_T__G__LogIndexCount = 0u;
    {
        Dir dir__root = LittleFS.openDir("/");
        while (dir__root.next())
        {
            char char__rootPath[40];
            String string__entry = dir__root.fileName();
            if (func__EspLog_ToRootPath(string__entry.c_str(),
                                        char__rootPath,
                                        sizeof(char__rootPath)) != false)
            {
                func__EspLog_RecordDayEntry(char__rootPath,
                                            (uint32_t)dir__root.fileSize());
            }
            if (UINT8_T__G__LogIndexCount >= (ESP_LOG_MAX_DAYS + 2u))
            {
                break;
            }
        }
    }
    return UINT8_T__G__LogIndexCount;
}
/**
 * @brief  [EN] Newest day that really exists on the flash, or ESP_LOG_DAY_AUTO
 *              when there is none.
* @return uint32_t day code or ESP_LOG_DAY_AUTO / AUTO
 */
static uint32_t func__EspLog_NewestDay(void)
{
    (void)func__EspLog_ScanDays(false);
    if (UINT8_T__G__LogIndexCount == 0u)
    {
        return ESP_LOG_DAY_AUTO;
    }
    return UINT32_T__G__LogIndexDay[UINT8_T__G__LogIndexCount - 1u];
}
/**
 * @brief  [EN] Delete the oldest day file and its entry.
* @return None /
 */
static void func__EspLog_DropOldest(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    if (UINT8_T__G__LogIndexCount == 0u)
    {
        return;
    }
    func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[0]);
    (void)LittleFS.remove(char__path);
    for (uint8_t__slot = 1u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        UINT32_T__G__LogIndexDay[uint8_t__slot - 1u] =
            UINT32_T__G__LogIndexDay[uint8_t__slot];
        UINT32_T__G__LogIndexBytes[uint8_t__slot - 1u] =
            UINT32_T__G__LogIndexBytes[uint8_t__slot];
    }
    UINT8_T__G__LogIndexCount--;
}
/**
 * @brief  [EN] Delete the oldest days when the retention count or the byte
 *              budget is passed. Nothing has to be written down afterwards:
 *              the list is the directory, so the next scan reads what is left.
* @return None /
 */
static void func__EspLog_TrimDays(void)
{
    uint8_t uint8_t__slot;
    while (UINT8_T__G__LogIndexCount > ESP_LOG_MAX_DAYS)
    {
        func__EspLog_DropOldest();
    }
    /*
 * [EN] v1.96: the day count alone does not bound the flash -
 * a busy day is bigger than a quiet week. Keep the whole
 * history inside one byte budget and drop the oldest day
 * first, so a full flash can never stop the log.
 */
    while (UINT8_T__G__LogIndexCount > 1u)
    {
        uint32_t uint32_t__bytes = 0u;
        for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
        {
            uint32_t__bytes += UINT32_T__G__LogIndexBytes[uint8_t__slot];
        }
        if (uint32_t__bytes <= ESP_LOG_TOTAL_BUDGET_BYTES)
        {
            break;
        }
        func__EspLog_DropOldest();
        BOOL__G__LogScanValid = false;
    }
}
/**
 * @brief  [EN] Count the data rows already in a day file (after a restart).
* @param const char *char__path file /
* @return uint16_t rows /
 */
static uint16_t func__EspLog_CountRows(const char *char__path)
{
    File file__day;
    uint16_t uint16_t__rows = 0u;
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return 0u;
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        if ((string__line.length() > 0u) && (string__line[0] != '#'))
        {
            /*
             * [EN] v1.117: the day-file marker (note 7) is the file's own first
             * line, not a history row, so it never spends a slot of the day cap.
             */
            int32_t int32_t__note = func__EspLog_FieldStart(string__line, 4u);
            uint32_t uint32_t__note = 0u;
            if (int32_t__note > 0)
            {
                String string__note = string__line.substring((size_t)int32_t__note);
                int32_t int32_t__comma = (int32_t)string__note.indexOf(',');
                if (int32_t__comma >= 0)
                {
                    string__note = string__note.substring(0u, (size_t)int32_t__comma);
                }
                uint32_t__note = (uint32_t)string__note.toInt();
            }
            if (uint32_t__note == (uint32_t)ESP_LOG_NOTE_DAY_START)
            {
                continue;
            }
            uint16_t__rows++;
        }
    }
    file__day.close();
    return uint16_t__rows;
}
/**
 * @brief  [EN] A power cut in the middle of a row leaves no closing newline, so
 *              the next append would glue a good row onto a broken one and make
 *              BOTH unreadable. This runs once when a day file is opened: if the
 *              last byte is not a newline, one is added first.
* @param const char *char__path day file /
* @return bool, true when the file ends on a line boundary /
 */
static bool func__EspLog_RepairTail(const char *char__path)
{
    File file__day;
    uint32_t uint32_t__size;
    bool bool__needsNewline = false;
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return true;   /* nothing to repair, and the next append reports itself */
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size > 0u)
    {
        if ((file__day.seek(uint32_t__size - 1u) == false) || (file__day.read() != '\n'))
        {
            bool__needsNewline = true;
        }
    }
    file__day.close();
    if (bool__needsNewline == false)
    {
        return true;
    }
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    bool__needsNewline = (func__EspLog_FileText(file__day, "\n") != false);
    file__day.close();
    if (bool__needsNewline == false)
    {
        func__EspLog_FaultRaise();
    }
    return bool__needsNewline;
}
/**
 * @brief  [EN] Is this a day code a file name can carry? Only the eight-digit
 *              YYYYMMDD range is; the two control values (AUTO and ZERO) and
 *              the "no day open yet" sentinel are not days at all.
 * @param  uint32_t uint32_t__dayCode day code (YYYYMMDD or 0)
 * @return bool true when a day file may be named after it
 */
static bool func__EspLog_DayCodeValid(uint32_t uint32_t__dayCode)
{
    return (uint32_t__dayCode < 100000000u);
}
/**
 * @brief  [EN] THE one append path: one verified row plus its newline into one
 *              day file. Nothing else in this file writes a row.
* @param uint32_t uint32_t__dayCode day /
* @param const char *char__row row /
* @return bool written and counted /
 */
static bool func__EspLog_AppendRow(uint32_t uint32_t__dayCode, const char *char__row)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    bool bool__ok;
    if ((BOOL__G__FsOk == false) || (char__row == NULL))
    {
        return false;
    }
    /*
 * [EN] v1.113: a day code outside the file-name range would create
 * "evt-4294967295.csv" - a file the day scan, the retention and the panel
 * all ignore, so its rows would vanish without a word. Refuse instead; the
 * caller keeps the row in RAM.
 */
    if (func__EspLog_DayCodeValid(uint32_t__dayCode) == false)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    bool__ok = func__EspLog_FileText(file__day, char__row);
    if (bool__ok != false)
    {
        bool__ok = func__EspLog_FileText(file__day, "\n");
    }
    file__day.close();
    if (bool__ok == false)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    return true;
}
/**
 * @brief  [EN] Make sure today's file exists and its row counter is right.
 * @return [EN] bool, false when the day file could not be opened or written
 */
/*
 * [EN] v1.117: defined below; the day opener writes the day's first row itself.
 */
static bool func__EspLog_BuildRow(char *char__row, size_t size_t__size,
                                  uint16_t uint16_t__changeMask, uint8_t uint8_t__note);
/**
 * @brief  [EN] Open (and remember) the day file of "now".
* @return bool opened */
static bool func__EspLog_OpenDay(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    bool bool__isNew;
    bool bool__ok = true;
    uint32_t uint32_t__dayCode;
    if (BOOL__G__FsOk == false)
    {
        return false;
    }
    uint32_t__dayCode = func__EspLog_TodayCode();
    if (uint32_t__dayCode == UINT32_T__G__LogDayCode)
    {
        return true;
    }
    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    bool__isNew = (LittleFS.exists(char__path) == false);
    file__day = LittleFS.open(char__path, "a");
    if (!file__day)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    if (bool__isNew)
    {
        bool__ok = func__EspLog_FileText(file__day, ESP_LOG_HEADER);
    }
    file__day.close();
    UINT32_T__G__LogDayCode = uint32_t__dayCode;
    if ((bool__ok != false) && (bool__isNew == false))
    {
        bool__ok = func__EspLog_RepairTail(char__path);
    }
    if (bool__ok == false)
    {
        func__EspLog_FaultRaise();
    }
    UINT16_T__G__LogRowsToday = func__EspLog_CountRows(char__path);
    /*
 * [EN] v1.117: a brand new day file opens with its own row, so the history
 * never starts with an unexplained gap - the day begins where the panel woke.
 * Only with a real clock: a "start" line with no time would be noise in the
 * no-clock file. The heartbeat is deliberately NOT touched here, so the
 * hourly rows keep landing on the hour.
 */
    if ((bool__isNew != false) && (bool__ok != false) &&
        (BOOL__G__LogClockValid != false))
    {
        char char__startRow[ESP_LOG_ROW_MAX];
        if (func__EspLog_BuildRow(char__startRow, sizeof(char__startRow),
                                  0u, ESP_LOG_NOTE_DAY_START) != false)
        {
            if (func__EspLog_AppendRow(uint32_t__dayCode, char__startRow) != false)
            {
                /*
                 * [EN] the marker does not use a slot of the day cap: it is the
                 * file's own first line, not a history row the panel reports.
                 */
                BOOL__G__LogAnchorSaveWanted = true;
            }
        }
    }
    BOOL__G__LogDayFull = (UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY);
    BOOL__G__LogMarkerDone = BOOL__G__LogDayFull;
    /*
 * [EN] the day list is the directory itself: read it again now that this
 * day's file exists, then drop the oldest days if the count or the
 * byte budget says so.
 */
    (void)func__EspLog_ScanDays(true);
    func__EspLog_TrimDays();
    return bool__ok;
}
/**
 * @brief  [EN] Move the rows the RAM ring is holding back into the day files they
 *              belong to (each row by its OWN epoch, so a row logged before the
 *              phone clock arrived still lands in the day-zero file). Oldest
 *              first; it stops at the first refusal and keeps the rest, so a
 *              flash that is still full loses nothing.
* @return None /
 */
static void func__EspLog_DrainRam(void)
{
    while (UINT8_T__G__LogRamCount > 0u)
    {
        uint8_t uint8_t__oldest;
        uint32_t uint32_t__epoch;
        uint32_t uint32_t__dayCode;
        uint8_t__oldest = (uint8_t)((UINT8_T__G__LogRamHead + ESP_LOG_RAM_ROWS -
                                     UINT8_T__G__LogRamCount) % ESP_LOG_RAM_ROWS);
        uint32_t__epoch = func__EspLog_RowEpoch(CHAR__G__LogRamRow[uint8_t__oldest]);
        if (uint32_t__epoch >= ESP_LOG_CLOCK_VALID_MIN)
        {
            uint32_t__dayCode = func__EspLog_DateCode(func__EspLog_LocalDay(uint32_t__epoch));
        }
        else
        {
            uint32_t__dayCode = 0u;
        }
        if (func__EspLog_AppendRow(uint32_t__dayCode,
                                   CHAR__G__LogRamRow[uint8_t__oldest]) == false)
        {
            BOOL__G__LogDrainWait = true;
            UINT32_T__G__LogLastDrainMs = (uint32_t)millis();
            return;   /* the flash is still not taking rows: keep them */
        }
        /*
 * [EN] a drained row takes a slot in today's file like any other, so
 * the day cap must count it - otherwise the file could grow past
 * the cap the counter believes in.
 */
        if ((uint32_t__dayCode == UINT32_T__G__LogDayCode) &&
            (UINT16_T__G__LogRowsToday < 0xFFFFu))
        {
            UINT16_T__G__LogRowsToday++;
        }
        UINT8_T__G__LogRamCount--;
    }
    BOOL__G__LogDrainWait = false;
    UINT8_T__G__LogWriteFault = 0u;
}
/* ==================== Row write ==================== */
/**
 * @brief  [EN] Battery percent from the DISCHARGE map (param 74/75, factory
 *              21,000..29,000 mV), stored with every row so later analysis
 *              keeps the same scale even if the map is retuned. Unknown params
 *              fall back to the factory map.
* @param uint32_t__batteryMv Pack voltage in mV /
* @return uint8_t Percent 0..100 /
 */
static uint8_t func__EspLog_Pct(uint32_t uint32_t__batteryMv)
{
    uint32_t uint32_t__vminMv = 21000u;
    uint32_t uint32_t__vmaxMv = 29000u;
    uint32_t uint32_t__rangeMv;
    if (BOOL__G__ParamKnown[74] != false)
    {
        uint32_t__vminMv = UINT32_T__G__ParamApplied[74];
    }
    if (BOOL__G__ParamKnown[75] != false)
    {
        uint32_t__vmaxMv = UINT32_T__G__ParamApplied[75];
    }
    if (uint32_t__batteryMv <= uint32_t__vminMv)
    {
        return 0u;
    }
    if (uint32_t__batteryMv >= uint32_t__vmaxMv)
    {
        return 100u;
    }
    uint32_t__rangeMv = uint32_t__vmaxMv - uint32_t__vminMv;
    if (uint32_t__rangeMv == 0u)
    {
        return 0u;
    }
    return (uint8_t)(((uint32_t__batteryMv - uint32_t__vminMv) * 100u) / uint32_t__rangeMv);
}
/**
 * @brief  [EN] Build one CSV row into a caller buffer. Returns false when the
 *              row did not fit - a cut row is reported instead of written.
* @param char *char__row buffer /
* @param size_t size_t__size buffer size /
* @param uint16_t uint16_t__changeMask change bits /
* @param uint8_t uint8_t__note note /
* @return bool fits /
 */
static bool func__EspLog_BuildRow(char *char__row, size_t size_t__size,
                                  uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    int int__written;
    int__written = snprintf(char__row, size_t__size,
                            "%lu,%lu,%u,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                            (unsigned long)func__EspLog_NowSec(),
                            (unsigned long)((uint32_t)millis() / 1000u),
                            (unsigned int)uint16_t__changeMask,
                            (unsigned int)uint8_t__note,
                            (unsigned int)(((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u),
                            (unsigned int)UINT8_T__G__TlmFlags,
                            (unsigned long)UINT32_T__G__TlmField[6],
                            (unsigned long)UINT32_T__G__TlmField[13],
                            (unsigned long)UINT32_T__G__TlmField[19],
                            (unsigned int)UINT8_T__G__TlmFlags2,
                            (unsigned long)UINT32_T__G__TlmField[15],
                            (unsigned long)UINT32_T__G__TlmField[17],
                            (unsigned long)UINT32_T__G__TlmField[18],
                            (unsigned long)UINT32_T__G__TlmField[25],
                            (unsigned long)UINT32_T__G__TlmField[28],
                            (unsigned long)UINT32_T__G__TlmField[14],
                            (unsigned long)UINT32_T__G__TlmField[16],
                            (unsigned long)UINT32_T__G__TlmField[3],
                            (unsigned long)UINT32_T__G__TlmField[10],
                            (unsigned long)UINT32_T__G__TlmField[5],
                            (unsigned long)UINT32_T__G__TlmField[12],
                            (unsigned long)func__EspLog_Pct(UINT32_T__G__TlmField[15]));
    return (int__written > 0) && ((size_t)int__written < size_t__size);
}
/**
 * @brief  [EN] Append one row and close it at once, so a power cut right after
 *              the battery cut cannot lose it. The flash is tried first; when it
 *              refuses, the row goes to the RAM ring and the fault counter tells
 *              the page. The last row of a full day is a marker that says the
 *              cap was reached.
* @param uint16_t uint16_t__changeMask what changed /
* @param uint8_t uint8_t__note row note /
* @return bool stored somewhere /
 */
static bool func__EspLog_WriteRow(uint16_t uint16_t__changeMask, uint8_t uint8_t__note)
{
    char char__row[ESP_LOG_ROW_MAX];
    bool bool__stored = false;
    /*
 * [EN] v1.110 (user report 2026-10-28: "it is on and it writes nothing"):
 * the day cap used to be judged BEFORE the day was looked up, and the
 * only place that notices a new day is OpenDay. So the first day that
 * reached 400 rows blocked every later write and the new day's file
 * was never created - the panel stayed silent until it was switched
 * off and on again. The day is opened first, so a full day ends where
 * it should: at midnight.
 */
    /*
 * [EN] v1.113: when the day file could not be opened, the day code still
 * belongs to the PREVIOUS day (or to the "no day yet" sentinel), so an
 * append here would file today's row under yesterday - or under a name no
 * reader knows. The row goes to the RAM ring instead and is drained into
 * its own day file once the flash answers again.
 */
    bool bool__dayOpen = true;
    if (BOOL__G__FsOk != false)
    {
        bool__dayOpen = func__EspLog_OpenDay();
    }
    /*
 * [EN] v1.113: a forced row (the firmware-update row) ignores this cap as
 * well - the user asked for it "with no conditions".
 */
    if ((BOOL__A__LogForceRow == false) &&
        ((UINT16_T__G__LogRowsToday >= ESP_LOG_MAX_ROWS_PER_DAY) ||
         (BOOL__G__LogDayFull != false)))
    {
        BOOL__G__LogDayFull = true;
        return false;
    }
    if (func__EspLog_BuildRow(char__row, sizeof(char__row),
                              uint16_t__changeMask, uint8_t__note) == false)
    {
        func__EspLog_FaultRaise();
        return false;
    }
    if ((BOOL__G__FsOk != false) && (bool__dayOpen != false) &&
        ((BOOL__G__LogDayFull == false) || (BOOL__A__LogForceRow != false)))
    {
        /*
 * [EN] the flash is taking rows again: the ones waiting in RAM go
 * first, so the history keeps its order.
 */
        if (UINT8_T__G__LogRamCount != 0u)
        {
            func__EspLog_DrainRam();
        }
        bool__stored = func__EspLog_AppendRow(UINT32_T__G__LogDayCode, char__row);
    }
    if ((bool__stored == false) || (BOOL__G__FsOk == false))
    {
        func__EspLog_RamPush(char__row);
    }
    if (BOOL__G__FsOk != false)
    {
        if (bool__stored != false)
        {
            BOOL__G__LogDrainWait = false;
            UINT8_T__G__LogWriteFault = 0u;   /* the flash works again */
        }
        else
        {
            BOOL__G__LogDrainWait = true;
            UINT32_T__G__LogLastDrainMs = (uint32_t)millis();
            func__EspLog_FaultRaise();
        }
    }
    /*
 * [EN] The row counter is the day-file cap, so it only grows when the file
 * really grew - a broken flash must not "fill" a file that stays empty.
 * With the file system off there is no file, and the session count
 * keeps its old meaning.
 */
    if ((bool__stored != false) || (BOOL__G__FsOk == false))
    {
        if (UINT16_T__G__LogRowsToday < 0xFFFFu)
        {
            UINT16_T__G__LogRowsToday++;
        }
    }
    if ((uint16_t__changeMask & ESP_LOG_CHG_VSTEP) != 0u)
    {
        UINT32_T__G__LogLastStepMs = (uint32_t)millis();
        BOOL__G__LogLastStepValid = true;
    }
    UINT32_T__G__LogPrevV24 = UINT32_T__G__TlmField[15];
    UINT32_T__G__LogLastRowMs = (uint32_t)millis();
    BOOL__G__LogLastRowValid = true;
    /*
 * [EN] The heartbeat hour restarts from THIS row, and it restarts on the log
 * clock, so it still means one whole hour after the board has switched
 * this panel off and on again.
 */
    if (BOOL__G__LogClockValid != false)
    {
        UINT32_T__G__LogLastBeatSec = func__EspLog_NowSec();
        BOOL__G__LogLastBeatValid = true;
        /*
 * [EN] v1.115: keep the anchor on the flash next to the row it belongs to.
 * Tick does the write, at most once a minute, so a busy day cannot wear
 * the flash and a battery session still leaves a fresh anchor behind.
 */
        BOOL__G__LogClockSaveWanted = true;
    }
    UINT16_T__G__LogPendingMask = 0u;
    UINT8_T__G__LogPendingNote = 0u;
    BOOL__G__LogMainsQueuedValid = false;
    BOOL__A__LogForceRow = false;   /* the one forced row has been written */
    /*
 * [EN] one slot before the cap, the day writes its own obituary: a row with
 * note 4 that says "the cap was reached, the rest of today is not
 * recorded". Without it a full day looks like a dead panel.
 */
    if ((bool__stored != false) && (BOOL__G__LogMarkerDone == false) &&
        (UINT16_T__G__LogRowsToday == (ESP_LOG_MAX_ROWS_PER_DAY - 1u)))
    {
        char char__marker[ESP_LOG_ROW_MAX];
        BOOL__G__LogMarkerDone = true;
        if (func__EspLog_BuildRow(char__marker, sizeof(char__marker),
                                  0u, ESP_LOG_NOTE_DAY_FULL) != false)
        {
            if (func__EspLog_AppendRow(UINT32_T__G__LogDayCode, char__marker) != false)
            {
                UINT16_T__G__LogRowsToday++;
            }
        }
        BOOL__G__LogDayFull = true;
        UINT16_T__G__LogPendingMask = 0u;
    }
    return true;
}
/**
 * @brief  [EN] One numeric field of a CSV row, by column index (0 = first).
 *              Missing columns read as 0, so a short row never invents values.
* @param const char *char__row one row /
* @param uint8_t uint8_t__index column /
* @return value /
 */
static uint32_t func__EspLog_RowFieldAt(const char *char__row, uint8_t uint8_t__index)
{
    uint8_t uint8_t__column = 0u;
    uint16_t uint16_t__cursor = 0u;
    uint32_t uint32_t__value = 0u;
    if (char__row == NULL)
    {
        return 0u;
    }
    while ((char__row[uint16_t__cursor] != '\0') &&
           (char__row[uint16_t__cursor] != '\n'))
    {
        if (char__row[uint16_t__cursor] == ',')
        {
            uint8_t__column++;
            uint16_t__cursor++;
            continue;
        }
        if (uint8_t__column == uint8_t__index)
        {
            if ((char__row[uint16_t__cursor] >= '0') &&
                (char__row[uint16_t__cursor] <= '9'))
            {
                uint32_t__value = (uint32_t__value * 10u) +
                                  (uint32_t)(char__row[uint16_t__cursor] - '0');
            }
        }
        uint16_t__cursor++;
    }
    return uint32_t__value;
}
/**
 * @brief  [EN] Read back how the world looked at the last row: the LAST LINE OF
 *              THE NEWEST DAY FILE is the memory. The log already carries the
 *              pack voltage, both halves, the fault mask, flags2, both charger
 *              phases and the mains bit, so no second file and no extra write per
 *              row is needed.
* @return None /
 */
static void func__EspLog_LoadLastSnapshot(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    char char__tail[320];
    File file__day;
    uint32_t uint32_t__dayCode;
    uint32_t uint32_t__size;
    uint32_t uint32_t__start;
    size_t size_t__got = 0u;
    int32_t int32_t__end;
    int32_t int32_t__begin;
    uint8_t uint8_t__scan;
    uint32_t uint32_t__v24;
    BOOL__G__LogBootSnapValid = false;
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    /*
 * [EN] Newest day that really exists: today if it is there, otherwise the
 * last day of the list.
 */
    uint32_t__dayCode = func__EspLog_TodayCode();
    func__EspLog_PathOf(char__path, uint32_t__dayCode);
    if (LittleFS.exists(char__path) == false)
    {
        uint32_t__dayCode = func__EspLog_NewestDay();
        if (uint32_t__dayCode == ESP_LOG_DAY_AUTO)
        {
            return;
        }
        func__EspLog_PathOf(char__path, uint32_t__dayCode);
    }
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return;
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size == 0u)
    {
        file__day.close();
        return;
    }
    uint32_t__start = (uint32_t__size > (uint32_t)(sizeof(char__tail) - 1u)) ?
                      (uint32_t__size - (uint32_t)(sizeof(char__tail) - 1u)) : 0u;
    (void)file__day.seek(uint32_t__start);
    while (size_t__got < (sizeof(char__tail) - 1u))
    {
        int int__byte = file__day.read();
        if (int__byte < 0)
        {
            break;
        }
        char__tail[size_t__got] = (char)int__byte;
        size_t__got++;
    }
    file__day.close();
    char__tail[size_t__got] = '\0';
    int32_t__end = (int32_t)size_t__got - 1;
    /*
 * [EN] Skip trailing newlines, then find the start of the last LINE - and
 * skip comment lines, so the file header can never be read as a row.
 */
    for (uint8_t__scan = 0u; uint8_t__scan < 8u; uint8_t__scan++)
    {
        while ((int32_t__end >= 0) &&
               ((char__tail[int32_t__end] == '\n') || (char__tail[int32_t__end] == '\r')))
        {
            int32_t__end--;
        }
        if (int32_t__end < 0)
        {
            return;
        }
        int32_t__begin = int32_t__end;
        while ((int32_t__begin > 0) && (char__tail[int32_t__begin - 1] != '\n'))
        {
            int32_t__begin--;
        }
        if ((char__tail[int32_t__begin] < '0') || (char__tail[int32_t__begin] > '9'))
        {
            int32_t__end = int32_t__begin - 1;   /* header or blank: one line up */
            continue;
        }
        break;
    }
    if (int32_t__end < 0)
    {
        return;
    }
    {
        const char *char__row = &char__tail[int32_t__begin];
        uint32_t__v24 = func__EspLog_RowFieldAt(char__row, 10u);
        if (uint32_t__v24 == 0u)
        {
            return;   /* a row without a pack voltage says nothing useful */
        }
        UINT32_T__G__LogBootV24 = uint32_t__v24;
        UINT32_T__G__LogBootVlow = func__EspLog_RowFieldAt(char__row, 11u);
        UINT32_T__G__LogBootVhigh = func__EspLog_RowFieldAt(char__row, 12u);
        UINT32_T__G__LogBootFault = func__EspLog_RowFieldAt(char__row, 8u);
        UINT8_T__G__LogBootFlags2 = (uint8_t)func__EspLog_RowFieldAt(char__row, 9u);
        UINT8_T__G__LogBootChg1 = (uint8_t)func__EspLog_RowFieldAt(char__row, 6u);
        UINT8_T__G__LogBootChg2 = (uint8_t)func__EspLog_RowFieldAt(char__row, 7u);
        UINT8_T__G__LogBootMains = (uint8_t)func__EspLog_RowFieldAt(char__row, 4u);
        /*
 * [EN] the row stores the whole status byte; the comparison uses the
 * state bits only (the mains bit has its own change bit), so the
 * same mask is applied here or every boot would look like a mode
 * change.
 */
        UINT8_T__G__LogBootFl =
            (uint8_t)(func__EspLog_RowFieldAt(char__row, 5u) & 0x3Du);
        UINT32_T__G__LogBootDead = func__EspLog_RowFieldAt(char__row, 14u);
        UINT8_T__G__LogBootDanger = (uint32_t__v24 < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
        BOOL__G__LogBootSnapValid = true;
    }
}
/**
 * @brief  [EN] Was the panel stopped while the battery sat at the cut line?
* @return uint8_t 1 = the last run was in the danger band / =
 */
static uint8_t func__EspLog_LoadLastDanger(void)
{
    if (BOOL__G__LogBootSnapValid == false)
    {
        func__EspLog_LoadLastSnapshot();
    }
    return (BOOL__G__LogBootSnapValid != false) ? UINT8_T__G__LogBootDanger : 0u;
}
/**
 * @brief  [EN] Absolute difference of two unsigned readings.
* @return difference /
 */
static uint32_t func__EspLog_AbsDiff(uint32_t uint32_t__a, uint32_t uint32_t__b)
{
    return (uint32_t__a >= uint32_t__b) ? (uint32_t__a - uint32_t__b)
                                        : (uint32_t__b - uint32_t__a);
}
/* ==================== Change detection ==================== */
/*
 * [EN] Defined below; the queue uses it to decide whether the row may go now.
 */
static bool func__EspLog_FlushDue(uint32_t uint32_t__nowMs);
/**
 * @brief  [EN] Queue a change. Bursts inside the 2 s window are merged into one
 *              row; the row always carries the full snapshot, so no final state
 *              is lost and the flash is not hammered.
* @param uint16_t uint16_t__mask change bits /
* @param uint8_t uint8_t__note note /
* @return None /
 */
static void func__EspLog_Queue(uint16_t uint16_t__mask, uint8_t uint8_t__note)
{
    uint32_t uint32_t__nowMs;
    UINT16_T__G__LogPendingMask = (uint16_t)(UINT16_T__G__LogPendingMask | uint16_t__mask);
    if (uint8_t__note != ESP_LOG_NOTE_NONE)
    {
        UINT8_T__G__LogPendingNote = uint8_t__note;
    }
    uint32_t__nowMs = (uint32_t)millis();
    if (((uint16_t__mask & ESP_LOG_CHG_MAINS) != 0u) &&
        (BOOL__G__LogMainsQueuedValid == false))
    {
        BOOL__G__LogMainsQueuedValid = true;
        UINT32_T__G__LogMainsQueuedMs = uint32_t__nowMs;
    }
    if (func__EspLog_FlushDue(uint32_t__nowMs) != false)
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}
/**
 * @brief  [EN] Is the pending row allowed to go to the flash now? Either the
 *              merge window has passed, or a mains flip has waited its short
 *              coalescing window (the board may cut the panel's power the same
 *              instant, so that row cannot wait two seconds).
* @param uint32_t uint32_t__nowMs now /
* @return bool due /
 */
static bool func__EspLog_FlushDue(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__sinceRowMs;
    uint32_t uint32_t__sinceMainsMs;
    /*
 * [EN] The first automatic row of a restored session waits for the boot
 * clock grace; queued changes ride with the boot row instead of
 * being stamped with the stale flash clock.
 */
    if (func__EspLog_BootClockReady(uint32_t__nowMs) == false)
    {
        return false;
    }
    if (BOOL__G__LogBootRowPending != false)
    {
        return false;
    }
    if (BOOL__G__LogLastRowValid == false)
    {
        return true;
    }
    uint32_t__sinceRowMs = uint32_t__nowMs - UINT32_T__G__LogLastRowMs;
    if (uint32_t__sinceRowMs >= ESP_LOG_MIN_INTERVAL_MS)
    {
        return true;
    }
    if (BOOL__G__LogMainsQueuedValid == false)
    {
        return false;
    }
    uint32_t__sinceMainsMs = uint32_t__nowMs - UINT32_T__G__LogMainsQueuedMs;
    return (uint32_t__sinceMainsMs >= ESP_LOG_MAINS_FLUSH_MS);
}
/**
 * @brief  [EN] Flush a queued change once its window has passed.
* @return None /
 */
static void func__EspLog_FlushPending(void)
{
    /*
 * [EN] v1.113: a queued NOTE is a reason to write on its own. The heartbeat
 * row carries no change bit at all, so testing the mask alone left it
 * waiting for the next real change instead of leaving on its own schedule.
 */
    if ((UINT16_T__G__LogPendingMask == 0u) && (UINT8_T__G__LogPendingNote == ESP_LOG_NOTE_NONE))
    {
        return;
    }
    if (func__EspLog_FlushDue((uint32_t)millis()) != false)
    {
        (void)func__EspLog_WriteRow(UINT16_T__G__LogPendingMask, UINT8_T__G__LogPendingNote);
    }
}
/**
 * @brief  [EN] Compare this frame's live state with the previous one and queue
 *              whatever changed. A steady board writes nothing.
* @return None /
 */
static void func__EspLog_DetectChanges(void)
{
    uint8_t uint8_t__mains;
    uint8_t uint8_t__chg1;
    uint8_t uint8_t__chg2;
    uint8_t uint8_t__flags2;
    uint8_t uint8_t__band;
    uint8_t uint8_t__fl;
    uint32_t uint32_t__dead;
    uint32_t uint32_t__fault;
    uint16_t uint16_t__mask = 0u;
    uint8_t__mains = ((UINT8_T__G__TlmFlags & 0x02u) != 0u) ? 1u : 0u;
    uint8_t__chg1 = (uint8_t)UINT32_T__G__TlmField[6];
    uint8_t__chg2 = (uint8_t)UINT32_T__G__TlmField[13];
    uint8_t__flags2 = UINT8_T__G__TlmFlags2;
    uint32_t__fault = UINT32_T__G__TlmField[19];
    /*
 * [EN] every state bit of the general byte except the input-present
 * bit, which already has its own change bit: snapshot, valid
 * measurement, both charger enables and hand mode.
 */
    uint8_t__fl = (uint8_t)(UINT8_T__G__TlmFlags & 0x3Du);
    uint32_t__dead = UINT32_T__G__TlmField[28];
    if (BOOL__G__LogHasPrev == false)
    {
        BOOL__G__LogHasPrev = true;
        if ((BOOL__G__LogBootSnapValid != false) && (BOOL__G__LogClockValid != false))
        {
            /*
 * [EN] First frame after boot AND the flash remembers what the world
 * looked like before the power went off. Seed the comparison
 * with that, then fall through: the checks below decide whether
 * anything really changed while the panel was dead. If nothing
 * changed, no row is written - the panel waits, exactly as the
 * user ordered.
 */
            UINT8_T__G__LogPrevMains = UINT8_T__G__LogBootMains;
            UINT8_T__G__LogPrevChg1 = UINT8_T__G__LogBootChg1;
            UINT8_T__G__LogPrevChg2 = UINT8_T__G__LogBootChg2;
            UINT8_T__G__LogPrevFlags2 = UINT8_T__G__LogBootFlags2;
            UINT32_T__G__LogPrevFault = UINT32_T__G__LogBootFault;
            UINT32_T__G__LogPrevV24 = UINT32_T__G__LogBootV24;
            /*
 * [EN] v1.110: the condemned mask and the status byte come from the
 * last row too. They were taken from the frame in hand, so a
 * battery condemned - or a charger enabled - while the panel
 * was off never produced a row, although both are logged
 * conditions with their own change bits.
 */
            UINT32_T__G__LogPrevDead = UINT32_T__G__LogBootDead;
            UINT8_T__G__LogPrevFl = UINT8_T__G__LogBootFl;
            /* The danger band is re-derived from the saved pack voltage, so a
               pack that was already low before the restart is not announced
               twice; one that crossed the line while the panel was off is. */
            UINT8_T__G__LogPrevBand = (UINT32_T__G__LogBootV24 < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
            BOOL__G__LogBootSnapValid = false;
        }
        else
        {
            /*
 * [EN] No snapshot: this frame is the baseline, nothing to compare.
 */
            UINT8_T__G__LogPrevMains = uint8_t__mains;
            UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
            UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
            UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
            UINT32_T__G__LogPrevFault = uint32_t__fault;
            UINT8_T__G__LogPrevBand =
                (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
            UINT8_T__G__LogPrevFl = uint8_t__fl;
            UINT32_T__G__LogPrevDead = uint32_t__dead;
            UINT32_T__G__LogPrevV24 = UINT32_T__G__TlmField[15];
            return;
        }
    }
    /*
 * [EN] v1.98: the cut band has HYSTERESIS. It is entered below
 * 21,200 mV and left only above 21,400 mV, so a pack resting on the
 * line - or a load switching on and off - cannot write a row every
 * couple of seconds and fill the flash with one repeated event.
 * v1.110: this runs AFTER the comparison is seeded, so the first
 * frame after a restart obeys the same rule instead of leaving the
 * band at 21,300 mV, 100 mV early.
 */
    if (UINT8_T__G__LogPrevBand != 0u)
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] > ESP_LOG_BAND_OFF_MV) ? 0u : 1u;
    }
    else
    {
        uint8_t__band = (UINT32_T__G__TlmField[15] < ESP_LOG_CUT_BAND_MV) ? 1u : 0u;
    }
    if (uint8_t__mains != UINT8_T__G__LogPrevMains)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_MAINS);
    }
    if (uint8_t__chg1 != UINT8_T__G__LogPrevChg1)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_CHG1);
    }
    if (uint8_t__chg2 != UINT8_T__G__LogPrevChg2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_CHG2);
    }
    if (uint32_t__fault != UINT32_T__G__LogPrevFault)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_FAULT);
    }
    if (uint8_t__flags2 != UINT8_T__G__LogPrevFlags2)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_FLAGS);
    }
    if (uint8_t__band != UINT8_T__G__LogPrevBand)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_BAND);
    }
    if (uint8_t__fl != UINT8_T__G__LogPrevFl)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_MODE);
    }
    if (uint32_t__dead != UINT32_T__G__LogPrevDead)
    {
        uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_DEAD);
    }
    /*
 * [EN] v1.98: a step of the pack voltage, either way. A row that is
 * written anyway (any other change) carries the step for free - the
 * voltage is on that row regardless. Only a row whose SOLE reason is
 * the step is limited to one a minute, so a load switching on and
 * off cannot flood the log while a slow charge still draws its
 * curve.
 */
    if (func__EspLog_AbsDiff(UINT32_T__G__TlmField[15],
                             UINT32_T__G__LogPrevV24) >= ESP_LOG_VSTEP_MV)
    {
        /*
 * [EN] v1.109 user order: while the mains are OFF (discharge) every
 * RISE of the pack is recorded as its own row - the one-a-minute
 * throttle stays only for falls and for charging.
 */
        bool bool__riseInDischarge = ((uint8_t__mains == 0u) &&
                                      (UINT32_T__G__TlmField[15] > UINT32_T__G__LogPrevV24));
        if ((uint16_t__mask != 0u) ||
            (bool__riseInDischarge != false) ||
            (BOOL__G__LogLastStepValid == false) ||
            ((uint32_t)((uint32_t)millis() - UINT32_T__G__LogLastStepMs) >= ESP_LOG_VSTEP_MIN_MS))
        {
            uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_VSTEP);
        }
    }
    if (uint16_t__mask != 0u)
    {
        func__EspLog_Queue(uint16_t__mask, ESP_LOG_NOTE_NONE);
    }
    UINT8_T__G__LogPrevMains = uint8_t__mains;
    UINT8_T__G__LogPrevChg1 = uint8_t__chg1;
    UINT8_T__G__LogPrevChg2 = uint8_t__chg2;
    UINT8_T__G__LogPrevFlags2 = uint8_t__flags2;
    UINT32_T__G__LogPrevFault = uint32_t__fault;
    UINT8_T__G__LogPrevBand = uint8_t__band;
    UINT8_T__G__LogPrevFl = uint8_t__fl;
    UINT32_T__G__LogPrevDead = uint32_t__dead;
    /*
 * [EN] the pack voltage is NOT stored here: it is compared
 * with the value at the last ROW, so a slow charge still
 * produces one row per step.
 */
}
/* ==================== Hooks ==================== */
/**
 * @brief  [EN] Called after every decoded TLM frame.
* @return None /
 */
static void func__EspLog_OnFrame(void)
{
    /*
 * [EN] Link back always pairs a link-lost row that was written before.
 */
    if (BOOL__G__LogLinkDown != false)
    {
        BOOL__G__LogLinkDown = false;
        func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_BACK);
    }
    /*
 * [EN] The boot row no longer waits for a board frame (user order
 * 2026-10-24: "if the board was not connected it logged nothing -
 * it must log"). Tick() writes it at start-up; here only the
 * state-change detection runs.
 */
    func__EspLog_DetectChanges();
}
/**
 * @brief  [EN] Is the silent-window row due? The window is counted on the LOG
 *              clock - wall seconds from the board anchor, and its place is
 *              kept in the clock file - so the hour survives a power cut. This
 *              panel's own uptime is only the fallback for a session with no
 *              clock yet. On battery the board cuts this panel's power every
 *              few minutes (battery saver), so an uptime counter started again
 *              from zero and the row was never written at all
 *              (user order 2026-11-01: "it must count the hour, from the board
 *              clock").
* @param uint32_t uint32_t__nowMs current millis(), ms /
* @return bool due /
 */
static bool func__EspLog_HeartbeatDue(uint32_t uint32_t__nowMs)
{
    uint32_t uint32_t__nowSec;
    if (BOOL__G__LogClockValid != false)
    {
        uint32_t__nowSec = func__EspLog_NowSec();
        if (BOOL__G__LogLastBeatValid == false)
        {
            /* first verdict of this session: seed the window, never fire */
            UINT32_T__G__LogLastBeatSec = uint32_t__nowSec;
            BOOL__G__LogLastBeatValid = true;
            return false;
        }
        if (uint32_t__nowSec < UINT32_T__G__LogLastBeatSec)
        {
            /* the clock was set back: the hour starts again from here */
            UINT32_T__G__LogLastBeatSec = uint32_t__nowSec;
            return false;
        }
        if ((uint32_t__nowSec - UINT32_T__G__LogLastBeatSec) >= ESP_LOG_HEARTBEAT_SEC)
        {
            return true;
        }
    }
    return ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastRowMs) >=
            ESP_LOG_HEARTBEAT_MS);
}
/**
 * @brief  [EN] Periodic work: merge pending rows, save the clock, drain the RAM
 *              ring when the flash is healthy again, notice a lost link. Called
 *              from loop().
* @return None /
 */
static void func__EspLog_Tick(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    /*
 * [EN] v1.115: a switch-on only speaks when it has something to say - see
 * the note inside. The silent-hour clock no longer waits for a row to
 * exist, so a session that writes nothing still writes its heartbeat.
 */
    if ((BOOL__G__LogBootRowPending != false) &&
        (func__EspLog_BootClockReady(uint32_t__nowMs) != false))
    {
        /*
 * [EN] v1.115 (user order 2026-11-02: "why should it write 'the panel is on'
 * every time? I do not need it at all"). A plain switch-on is not an
 * event: on battery the board cycles this panel every three minutes, so
 * the old boot row filled the day with 20 rows an hour, hit the
 * 400-row cap after 20 hours and left the rest of the day unlogged -
 * the "a couple of hours in the middle are missing" report. A row is
 * written here ONLY when it carries a message: a firmware update, or a
 * previous run that stopped in the battery danger band.
 */
        bool bool__speak = false;
        uint16_t uint16_t__mask = UINT16_T__G__LogPendingMask;
        BOOL__G__LogBootRowPending = false;
        UINT32_T__G__LogSessionStartMs = (uint32_t)millis();
        UINT8_T__G__LogPrevNote = func__EspLog_LoadLastDanger();
        if (UINT8_T__G__LogPrevNote != 0u)
        {
            /* this row really does open a session, so it keeps the boot bit */
            bool__speak = true;
            uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_BOOT);
        }
        /*
 * [EN] v1.113 (user order 2026-10-28: "every time I do an update one
 * record must be written, with no conditions at all, and only the
 * log timers zeroed"). The update joins the boot row, so one
 * switch-on gives one row. The log timers are zeroed just before
 * it: the merge window, the step throttle, the "last row" clock
 * and the clock-save timer all start again from this row, so
 * nothing from before the update can swallow or merge it.
 */
        if (BOOL__G__LogUpdatePending != false)
        {
            BOOL__G__LogUpdatePending = false;
            bool__speak = true;
            uint16_t__mask = (uint16_t)(uint16_t__mask | ESP_LOG_CHG_BOOT | ESP_LOG_CHG_UPDATE);
            UINT32_T__G__LogLastRowMs = 0u;
            BOOL__G__LogLastRowValid = false;
            UINT32_T__G__LogLastStepMs = 0u;
            BOOL__G__LogLastStepValid = false;
            UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
            UINT16_T__G__LogPendingMask = 0u;
            UINT8_T__G__LogPendingNote = ESP_LOG_NOTE_NONE;
            BOOL__G__LogMainsQueuedValid = false;
            BOOL__A__LogForceRow = true;
        }
        if (bool__speak != false)
        {
            (void)func__EspLog_WriteRow(uint16_t__mask,
                                        (UINT8_T__G__LogPrevNote != 0u) ?
                                        ESP_LOG_NOTE_LOW_STOP : ESP_LOG_NOTE_NONE);
        }
    }
    /*
 * [EN] The clock moved by more than a re-sync: say it in the history.
 */
    if (BOOL__G__LogClockJumpPending != false)
    {
        BOOL__G__LogClockJumpPending = false;
        func__EspLog_Queue(ESP_LOG_CHG_CLOCK, ESP_LOG_NOTE_NONE);
    }
    /*
 * [EN] v1.117: the first wake after midnight opens the new day right away, so
 * the file exists from that second (with its own "day starts here" row) even
 * if nothing else happens for hours. Opening an existing file writes nothing,
 * so a reboot in the same day cannot duplicate the row.
 */
    if ((BOOL__G__FsOk != false) && (BOOL__G__LogClockValid != false) &&
        (func__EspLog_BootClockReady(uint32_t__nowMs) != false))
    {
        uint32_t uint32_t__today = func__EspLog_TodayCode();
        if ((uint32_t__today != 0u) && (uint32_t__today != UINT32_T__G__LogDayCode))
        {
            (void)func__EspLog_OpenDay();
        }
    }
    func__EspLog_FlushPending();
    uint32_t__nowMs = (uint32_t)millis();
    if (BOOL__G__LogClockValid != false)
    {
        uint32_t uint32_t__sinceSaveMs = (uint32_t)(uint32_t__nowMs - UINT32_T__G__LogLastClockSaveMs);
        /*
 * [EN] v1.116: a freshly taken board anchor goes to the flash at once. One
 * small write per wake is the price of never losing a sleep; the row-driven
 * save below stays rate limited.
 */
        if (BOOL__G__LogAnchorSaveWanted != false)
        {
            func__EspLog_ClockSave();
            BOOL__G__LogAnchorSaveWanted = false;
        }
        bool bool__due = (uint32_t__sinceSaveMs >= ESP_LOG_CLOCK_SAVE_MS);
        /*
 * [EN] v1.115: a written row asks for a fresh anchor. On battery a session
 * lasts 60 s, so the ten-minute periodic save never came and the file
 * stayed hours behind; this one is rate limited so the flash is not worn.
 */
        if ((BOOL__G__LogClockSaveWanted != false) &&
            (uint32_t__sinceSaveMs >= ESP_LOG_CLOCK_SAVE_MIN_MS))
        {
            bool__due = true;
        }
        if (bool__due != false)
        {
            func__EspLog_ClockSave();
            BOOL__G__LogClockSaveWanted = false;
        }
    }
    /*
 * [EN] Rows waiting in RAM go back to their day files when the flash takes
 * writes again, throttled by ESP_LOG_DRAIN_RETRY_MS after any refusal
 * so loop() never hammers SPI flash on every iteration.
 */
    if ((BOOL__G__FsOk != false) && (UINT8_T__G__LogRamCount != 0u))
    {
        uint32_t uint32_t__sinceDrainMs = uint32_t__nowMs - UINT32_T__G__LogLastDrainMs;
        if ((BOOL__G__LogDrainWait == false) ||
            (uint32_t__sinceDrainMs >= ESP_LOG_DRAIN_RETRY_MS))
        {
            func__EspLog_DrainRam();
        }
    }
    /*
 * [EN] Link-silence row: after LINK_TIMEOUT_MS with no frame. If the
 * board has never spoken, the clock runs from the session start, so
 * "no board" is written too - not only "board left".
 */
    if (BOOL__G__LogLinkDown == false)
    {
        bool bool__silent;
        if (BOOL__G__TlmSeen != false)
        {
            bool__silent = ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LastTlmMs) >
                            ESP_LOG_LINK_TIMEOUT_MS);
        }
        else
        {
            bool__silent = (BOOL__G__LogLastRowValid != false) &&
                           ((uint32_t)(uint32_t__nowMs - UINT32_T__G__LogSessionStartMs) >
                            ESP_LOG_LINK_TIMEOUT_MS);
        }
        if (bool__silent != false)
        {
            BOOL__G__LogLinkDown = true;
            func__EspLog_Queue(ESP_LOG_CHG_LINK, ESP_LOG_NOTE_LINK_LOST);
        }
    }
    /*
 * [EN] v1.96: if nothing at all has happened for a whole heartbeat
 * window, write one row that says so. It is the difference
 * between "nothing changed" and "the panel stopped working":
 * a board that keeps beating is a board that kept running.
 * v1.114: the window is measured on the LOG clock instead of this
 * panel's uptime, which the battery saver resets every few minutes.
 */
    /*
 * [EN] v1.115: this used to need a row to exist first (LogLastRowValid), and
 * a switch-on used to provide it. With the plain boot row gone, a quiet
 * battery session would never have written a heartbeat at all, so the
 * window is judged on the log clock alone; the beat second comes from the
 * clock file, which is why it survives the power cut.
 */
    if (func__EspLog_HeartbeatDue(uint32_t__nowMs) != false)
    {
        func__EspLog_Queue(0u, ESP_LOG_NOTE_HEARTBEAT);
        /*
 * [EN] save the new beat second at once - the next power cut may come
 * before the periodic save, and the row must not repeat.
 */
        if (BOOL__G__LogClockValid != false)
        {
            func__EspLog_ClockSave();
            BOOL__G__LogClockSaveWanted = false;
        }
    }
}
/**
 * @brief  [EN] Boot: clock from flash, day list, boot row pending.
* @return None /
 */
static void func__EspLog_Begin(void)
{
    UINT32_T__G__LogLastClockSaveMs = (uint32_t)millis();
    UINT32_T__G__LogBootMs = (uint32_t)millis();
    BOOL__G__LogBootClockPending = false;
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    if (BOOL__G__FsOk == false)
    {
        return;
    }
    /*
 * [EN] a panel flashed with the old firmware still carries the list file
 * it used to keep. Nothing reads it any more, so it only wastes
 * flash: one boot deletes it.
 */
    if (LittleFS.exists(ESP_LOG_LEGACY_INDEX_PATH) != false)
    {
        (void)LittleFS.remove(ESP_LOG_LEGACY_INDEX_PATH);
    }
    func__EspLog_ClockLoad();
    /*
 * [EN] Only a restored, matched board anchor needs the first live answer.
 * A fresh flash, a two-field legacy clock, or an already live board
 * starts writing immediately.
 */
    BOOL__G__LogBootClockPending =
        ((BOOL__G__LogClockValid != false) &&
         (BOOL__G__LogAnchorBoardValid != false) &&
         (BOOL__G__LogBoardAlive == false));
    /*
 * [EN] v1.113: was this flash a new firmware? The stamp lives in the clock
 * file, so it survives both a restart and a history clear.
 */
    func__EspLog_FwCheck();
    /*
 * [EN] the day list is read from the flash, so a restart cannot lose it.
 */
    (void)func__EspLog_ScanDays(true);
    /*
 * [EN] read back what the world looked like before the power went off.
 */
    func__EspLog_LoadLastSnapshot();
}
/* ==================== Manual snapshot ==================== */
/**
 * @brief  [EN] Record one state row unconditionally when the user presses
 *              Refresh, and zero only the log timers so every log window
 *              restarts from this row (user order 2026-10-28).
* @return bool stored /
 */
static bool func__EspLog_SnapshotNow(void)
{
    uint32_t uint32_t__nowMs;
    uint16_t uint16_t__mask;
    bool bool__stored;
    uint32_t__nowMs = (uint32_t)millis();
    uint16_t__mask = UINT16_T__G__LogPendingMask;
    UINT32_T__G__LogLastRowMs = 0u;
    BOOL__G__LogLastRowValid = false;
    UINT32_T__G__LogLastStepMs = 0u;
    BOOL__G__LogLastStepValid = false;
    UINT32_T__G__LogMainsQueuedMs = 0u;
    BOOL__G__LogMainsQueuedValid = false;
    UINT32_T__G__LogLastClockSaveMs = uint32_t__nowMs;
    UINT32_T__G__LogLastScanMs = 0u;
    BOOL__G__LogScanValid = false;
    if (BOOL__G__TlmSeen == false)
    {
        UINT32_T__G__LogSessionStartMs = uint32_t__nowMs;
    }
    UINT16_T__G__LogPendingMask = 0u;
    UINT8_T__G__LogPendingNote = ESP_LOG_NOTE_NONE;
    BOOL__A__LogForceRow = true;
    bool__stored = func__EspLog_WriteRow(uint16_t__mask, ESP_LOG_NOTE_MANUAL);
    BOOL__A__LogForceRow = false;
    UINT32_T__G__LogLastScanMs = 0u;
    BOOL__G__LogScanValid = false;
    return bool__stored;
}
/* ==================== Read back ==================== */
/**
 * @brief  [EN] Where does the window of the newest rows start? It walks
 *              BACKWARD from the end of the file, so the limit is spent on the
 *              newest rows: the buffer may hold the last 18 rows of a busy day,
 *              and they are rows 383..400, never rows 1..18.
* @param File &file__day open day file /
* @param uint32_t uint32_t__size file size /
* @param uint16_t uint16_t__maxRows row limit /
* @param size_t size_t__budget byte limit /
* @return offset to start from /
 */
static uint32_t func__EspLog_TailStart(File &file__day,
                                       uint32_t uint32_t__size,
                                       uint16_t uint16_t__maxRows,
                                       size_t size_t__budget)
{
    uint32_t uint32_t__offset = uint32_t__size;
    uint32_t uint32_t__lines = 0u;
    uint32_t uint32_t__bytes = 0u;
    bool bool__cutMidRow = false;
    while (uint32_t__offset > 0u)
    {
        int int__ch;
        uint32_t__offset--;
        if (file__day.seek(uint32_t__offset) == false)
        {
            return 0u;
        }
        int__ch = file__day.read();
        if (int__ch < 0)
        {
            return 0u;
        }
        uint32_t__bytes++;
        /*
 * [EN] the last character of a normal file closes the newest row; it is
 * not a row of its own.
 */
        if ((int__ch == '\n') && (uint32_t__bytes > 1u))
        {
            uint32_t__lines++;
            if (uint32_t__lines >= (uint32_t)uint16_t__maxRows)
            {
                uint32_t__offset++;
                return uint32_t__offset;
            }
        }
        if (uint32_t__bytes >= (uint32_t)size_t__budget)
        {
            bool__cutMidRow = true;
            break;
        }
    }
    if ((bool__cutMidRow != false) && (uint32_t__offset > 0u))
    {
        /*
 * [EN] the budget ran out inside a row: drop that half row, a row the
 * panel cannot parse is worse than one row less.
 */
        while (uint32_t__offset < uint32_t__size)
        {
            int int__ch;
            if (file__day.seek(uint32_t__offset) == false)
            {
                return 0u;
            }
            int__ch = file__day.read();
            uint32_t__offset++;
            if (int__ch == '\n')
            {
                break;
            }
        }
    }
    return uint32_t__offset;
}
/**
 * @brief  [EN] Copy the last N rows of the RAM ring into a buffer, oldest first.
* @return size_t bytes written /
 */
static size_t func__EspLog_TailRam(char *char__out, size_t size_t__outSize,
                                   uint16_t uint16_t__maxLines)
{
    uint8_t uint8_t__start;
    uint8_t uint8_t__count = UINT8_T__G__LogRamCount;
    uint8_t uint8_t__item;
    size_t size_t__used = 0u;
    if (uint8_t__count == 0u)
    {
        return 0u;
    }
    uint8_t__start = (uint8_t)((UINT8_T__G__LogRamHead + ESP_LOG_RAM_ROWS - uint8_t__count) % ESP_LOG_RAM_ROWS);
    if ((uint16_t)uint8_t__count > uint16_t__maxLines)
    {
        uint8_t uint8_t__drop = (uint8_t)((uint16_t)uint8_t__count - uint16_t__maxLines);
        uint8_t__start = (uint8_t)((uint8_t__start + uint8_t__drop) % ESP_LOG_RAM_ROWS);
        uint8_t__count = (uint8_t)uint16_t__maxLines;
    }
    for (uint8_t__item = 0u; uint8_t__item < uint8_t__count; uint8_t__item++)
    {
        uint8_t uint8_t__slot = (uint8_t)((uint8_t__start + uint8_t__item) % ESP_LOG_RAM_ROWS);
        size_t size_t__len = strlen(CHAR__G__LogRamRow[uint8_t__slot]);
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__out[size_t__used], CHAR__G__LogRamRow[uint8_t__slot], size_t__len);
        size_t__used += size_t__len;
        char__out[size_t__used] = '\n';
        size_t__used++;
        char__out[size_t__used] = '\0';
    }
    return size_t__used;
}
/**
 * @brief  [EN] The RAM ring is the only history left: the card shows those rows
 *              instead of nothing.
* @param char *char__out output buffer /
* @param size_t size_t__outSize size /
* @param uint16_t uint16_t__maxLines how many rows /
* @param bool bool__isToday is the asked day today? /
* @return size_t bytes written /
 */
static size_t func__EspLog_TailRamOnly(char *char__out, size_t size_t__outSize,
                                        uint16_t uint16_t__maxLines, bool bool__isToday)
{
    /*
 * [EN] The RAM ring only ever holds today's rows. Without a write fault the
 * emptiness is real, and an older day must never borrow today's rows.
 */
    if ((UINT8_T__G__LogWriteFault == 0u) || (bool__isToday == false))
    {
        return 0u;
    }
    return func__EspLog_TailRam(char__out, size_t__outSize, uint16_t__maxLines);
}
/**
 * @brief  [EN] Which file does an automatic read pick? Today's file when it
 *              exists, otherwise the newest day of the list - the day the phone
 *              clock has not named yet keeps its own file and is never hidden.
* @return day code or ESP_LOG_DAY_AUTO when nothing exists /
 */
static uint32_t func__EspLog_PickDay(uint32_t uint32_t__dayCode)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint32_t uint32_t__chosen = uint32_t__dayCode;
    if ((uint32_t__chosen == ESP_LOG_DAY_ZERO))
    {
        return 0u;
    }
    if ((uint32_t__chosen != ESP_LOG_DAY_AUTO) && (uint32_t__chosen != 0u))
    {
        return uint32_t__chosen;
    }
    uint32_t__chosen = func__EspLog_TodayCode();
    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        uint32_t__chosen = func__EspLog_NewestDay();
        if (uint32_t__chosen == ESP_LOG_DAY_AUTO)
        {
            return ESP_LOG_DAY_AUTO;
        }
    }
    return uint32_t__chosen;
}
/**
 * @brief  [EN] Copy the last N rows of one day into a buffer, oldest first.
 *              The window is chosen from the END of the file, so when the
 *              buffer cannot hold 30 rows the OLDEST of them are dropped,
 *              never the newest. A flash that refused the last writes is
 *              served from the RAM ring instead of an empty card.
* @param char *char__out output buffer /
* @param size_t size_t__outSize buffer size /
* @param uint32_t uint32_t__dayCode day, AUTO = newest /
* @param uint16_t uint16_t__maxLines how many rows /
* @return size_t bytes written /
 */
static size_t func__EspLog_Tail(char *char__out, size_t size_t__outSize,
                                uint32_t uint32_t__dayCode, uint16_t uint16_t__maxLines)
{
    char char__path[ESP_LOG_PATH_MAX];
    File file__day;
    size_t size_t__used = 0u;
    uint32_t uint32_t__chosen;
    uint32_t uint32_t__size;
    uint32_t uint32_t__start;
    bool bool__isToday;
    if ((char__out == NULL) || (size_t__outSize < 2u))
    {
        return 0u;
    }
    char__out[0] = '\0';
    /*
 * [EN] v1.111 (user report 2026-10-28): one refused write used to hide the
 * whole day file behind 16 RAM rows, so a card that had hundreds of
 * rows showed a handful. The file is read first and the rows that
 * never reached the flash follow it.
 */
    bool__isToday = ((uint32_t__dayCode == ESP_LOG_DAY_AUTO) ||
                     (uint32_t__dayCode == 0u) ||
                     (uint32_t__dayCode == func__EspLog_TodayCode()));
    /*
 * [EN] No file system at all: the RAM ring is the whole history, so today's
 * rows are served as they are.
 */
    if (BOOL__G__FsOk == false)
    {
        return func__EspLog_TailRam(char__out, size_t__outSize, uint16_t__maxLines);
    }
    uint32_t__chosen = func__EspLog_PickDay(uint32_t__dayCode);
    if (uint32_t__chosen == ESP_LOG_DAY_AUTO)
    {
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    func__EspLog_PathOf(char__path, uint32_t__chosen);
    if (LittleFS.exists(char__path) == false)
    {
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    file__day = LittleFS.open(char__path, "r");
    if (!file__day)
    {
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    uint32_t__size = (uint32_t)file__day.size();
    if (uint32_t__size == 0u)
    {
        file__day.close();
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    uint32_t__start = func__EspLog_TailStart(file__day, uint32_t__size,
                                             uint16_t__maxLines, size_t__outSize - 2u);
    if (file__day.seek(uint32_t__start) == false)
    {
        file__day.close();
        return func__EspLog_TailRamOnly(char__out, size_t__outSize,
                                        uint16_t__maxLines, bool__isToday);
    }
    while (file__day.available())
    {
        String string__line = file__day.readStringUntil('\n');
        size_t size_t__len;
        string__line.trim();
        if ((string__line.length() == 0u) || (string__line[0] == '#'))
        {
            continue;
        }
        size_t__len = (size_t)string__line.length();
        if ((size_t__used + size_t__len + 2u) >= size_t__outSize)
        {
            break;
        }
        (void)memcpy(&char__out[size_t__used], string__line.c_str(), size_t__len);
        size_t__used += size_t__len;
        char__out[size_t__used] = '\n';
        size_t__used++;
        char__out[size_t__used] = '\0';
    }
    file__day.close();
    /*
 * [EN] the rows that failed to reach the flash are newer than everything in
 * the file, so they are appended, not substituted.
 */
    if ((UINT8_T__G__LogWriteFault != 0u) && (UINT8_T__G__LogRamCount != 0u) &&
        ((size_t__used + 2u) < size_t__outSize))
    {
        size_t__used += func__EspLog_TailRam(&char__out[size_t__used],
                                             size_t__outSize - size_t__used,
                                             uint16_t__maxLines);
    }
    return size_t__used;
}
/**
 * @brief  [EN] Delete every saved day and start a fresh list.
* @return uint8_t number of files removed /
 */
static uint8_t func__EspLog_Clear(void)
{
    char char__path[ESP_LOG_PATH_MAX];
    uint8_t uint8_t__slot;
    uint8_t uint8_t__removed = 0u;
    UINT8_T__G__LogRamHead = 0u;
    UINT8_T__G__LogRamCount = 0u;
    if (BOOL__G__FsOk == false)
    {
        UINT16_T__G__LogRowsToday = 0u;
        BOOL__G__LogDayFull = false;
        UINT8_T__G__LogWriteFault = 0u;
        UINT16_T__G__LogDroppedRows = 0u;
        return 0u;
    }
    (void)func__EspLog_ScanDays(true);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        func__EspLog_PathOf(char__path, UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (LittleFS.remove(char__path))
        {
            uint8_t__removed++;
        }
    }
    UINT8_T__G__LogIndexCount = 0u;
    BOOL__G__LogScanValid = false;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    BOOL__G__LogMarkerDone = false;
    BOOL__G__LogDrainWait = false;
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    return uint8_t__removed;
}
/**
 * @brief  [EN] Day list as text ("YYYYMMDD YYYYMMDD ... , newest last").
* @param char *char__out output /
* @param size_t size_t__outSize size /
* @return size_t /
 */
static size_t func__EspLog_Days(char *char__out, size_t size_t__outSize)
{
    size_t size_t__used = 0u;
    uint8_t uint8_t__slot;
    if ((char__out == NULL) || (size_t__outSize < 4u))
    {
        return 0u;
    }
    char__out[0] = '\0';
    if (BOOL__G__FsOk == false)
    {
        /*
 * [EN] v1.111: with the file system down the only day that can exist is
 * today's RAM rows. The list used to stay empty until the first
 * row arrived, so the day picker showed nothing at all.
 */
        if (size_t__outSize >= 12u)
        {
            int int__ramWritten = snprintf(char__out, size_t__outSize, "%08lu",
                                           (unsigned long)func__EspLog_TodayCode());
            return (int__ramWritten > 0) ? (size_t)int__ramWritten : 0u;
        }
        return 0u;
    }
    (void)func__EspLog_ScanDays(false);
    for (uint8_t__slot = 0u; uint8_t__slot < UINT8_T__G__LogIndexCount; uint8_t__slot++)
    {
        int int__written = snprintf(&char__out[size_t__used],
                                    size_t__outSize - size_t__used,
                                    "%s%08lu",
                                    (size_t__used == 0u) ? "" : " ",
                                    (unsigned long)UINT32_T__G__LogIndexDay[uint8_t__slot]);
        if (int__written <= 0)
        {
            break;
        }
        size_t__used += (size_t)int__written;
        if (size_t__used >= (size_t__outSize - 12u))
        {
            break;
        }
    }
    return size_t__used;
}
/* ==================== Flash inspection & cleanup ==================== */
/**
 * @brief  [EN] Check whether a canonical "/..." root path belongs to the
 *              current calibration panel firmware.
* @param const char *char__ptr_path canonical "/..." path / «/»
* @return bool true when owned by this firmware /
 */
static bool func__EspLog_IsOwnFile(const char *char__ptr_path)
{
    uint32_t uint32_t__day = 0u;
    if ((char__ptr_path == NULL) || (char__ptr_path[0] != '/'))
    {
        return false;
    }
    if (func__EspLog_DayFromName(char__ptr_path, &uint32_t__day) != false)
    {
        return true;
    }
    if (strcmp(char__ptr_path, ESP_LOG_CLOCK_PATH) == 0)
    {
        return true;
    }
    if (strcmp(char__ptr_path, ESP_BENCHLOG_PATH) == 0)
    {
        return true;
    }
    return false;
}
/**
 * @brief  [EN] Copy a sanitized canonical "/..." path into out for JSON output.
* @param const char *char__ptr_src canonical "/..." path /
* @param char *char__out destination /
* @param size_t size_t__outSize capacity /
* @return None /
 */
static void func__EspLog_SanitizeFileName(const char *char__ptr_src,
                                          char *char__out,
                                          size_t size_t__outSize)
{
    size_t size_t__idx = 0u;
    if ((char__out == NULL) || (size_t__outSize == 0u))
    {
        return;
    }
    if (char__ptr_src == NULL)
    {
        char__out[0] = '\0';
        return;
    }
    while ((char__ptr_src[size_t__idx] != '\0') &&
           ((size_t__idx + 1u) < size_t__outSize))
    {
        char char__ch = char__ptr_src[size_t__idx];
        if ((char__ch < 0x20) || (char__ch > 0x7E) ||
            (char__ch == '"') || (char__ch == '\\'))
        {
            char__ch = '?';
        }
        char__out[size_t__idx] = char__ch;
        size_t__idx++;
    }
    char__out[size_t__idx] = '\0';
}
/**
 * @brief  [EN] Build a JSON summary of LittleFS capacity and root files so the
 *              operator can inspect leftover files from older sketches.
* @param char *char__out JSON buffer /
* @param size_t size_t__outSize buffer size /
* @return size_t bytes written /
 */
static size_t func__EspLog_FsStatusJson(char *char__out, size_t size_t__outSize)
{
    uint32_t uint32_t__totalBytes = 0u;
    uint32_t uint32_t__usedBytes = 0u;
    uint32_t uint32_t__sumBytes = 0u;
    uint16_t uint16_t__shown = 0u;
    uint16_t uint16_t__more = 0u;
    size_t size_t__used = 0u;
    int int__written;
    if ((char__out == NULL) || (size_t__outSize < 64u))
    {
        return 0u;
    }
    if (BOOL__G__FsOk != false)
    {
        FSInfo fsInfo__stat;
        if (LittleFS.info(fsInfo__stat))
        {
            uint32_t__totalBytes = (uint32_t)fsInfo__stat.totalBytes;
            uint32_t__usedBytes = (uint32_t)fsInfo__stat.usedBytes;
        }
    }
    int__written = snprintf(char__out, size_t__outSize,
                            "{\"fs\":%u,\"mountFail\":%u,\"total\":%lu,\"used\":%lu,\"files\":[",
                            (BOOL__G__FsOk != false) ? 1u : 0u,
                            (BOOL__G__FsMountFailed != false) ? 1u : 0u,
                            (unsigned long)uint32_t__totalBytes,
                            (unsigned long)uint32_t__usedBytes);
    if ((int__written <= 0) || ((size_t)int__written >= size_t__outSize))
    {
        char__out[0] = '\0';
        return 0u;
    }
    size_t__used = (size_t)int__written;
    if (BOOL__G__FsOk != false)
    {
        Dir dir__root = LittleFS.openDir("/");
        while (dir__root.next())
        {
            char char__rootPath[40];
            char char__safeName[40];
            String string__raw = dir__root.fileName();
            uint32_t uint32_t__fsize = (uint32_t)dir__root.fileSize();
            if (func__EspLog_ToRootPath(string__raw.c_str(),
                                        char__rootPath,
                                        sizeof(char__rootPath)) != false)
            {
                bool bool__own = func__EspLog_IsOwnFile(char__rootPath);
                uint32_t__sumBytes += uint32_t__fsize;
                func__EspLog_SanitizeFileName(char__rootPath, char__safeName, sizeof(char__safeName));
                if ((uint16_t__shown < 24u) && ((size_t__used + 96u) < size_t__outSize))
                {
                    int int__item = snprintf(&char__out[size_t__used],
                                             size_t__outSize - size_t__used,
                                             "%s{\"name\":\"%s\",\"size\":%lu,\"own\":%u}",
                                             (uint16_t__shown == 0u) ? "" : ",",
                                             char__safeName,
                                             (unsigned long)uint32_t__fsize,
                                             (bool__own != false) ? 1u : 0u);
                    if ((int__item > 0) && ((size_t__used + (size_t)int__item) < size_t__outSize))
                    {
                        size_t__used += (size_t)int__item;
                        uint16_t__shown++;
                    }
                }
                else
                {
                    uint16_t__more++;
                }
            }
        }
        if ((uint32_t__usedBytes == 0u) && (uint32_t__sumBytes > 0u))
        {
            uint32_t__usedBytes = uint32_t__sumBytes;
        }
    }
    int__written = snprintf(&char__out[size_t__used],
                            size_t__outSize - size_t__used,
                            "],\"fileBytes\":%lu,\"more\":%u}",
                            (unsigned long)uint32_t__sumBytes,
                            (unsigned int)uint16_t__more);
    if ((int__written > 0) && ((size_t__used + (size_t)int__written) < size_t__outSize))
    {
        size_t__used += (size_t)int__written;
    }
    return size_t__used;
}
/**
 * @brief  [EN] Remove leftover files from older programs while keeping all
 *              files owned by the running firmware intact.
* @param uint32_t *uint32_t__ptr_freedBytes bytes freed /
* @return uint16_t removed file count /
 */
static uint16_t func__EspLog_CleanForeignFiles(uint32_t *uint32_t__ptr_freedBytes)
{
    uint16_t uint16_t__removed = 0u;
    uint32_t uint32_t__freed = 0u;
    uint8_t uint8_t__pass;
    if (uint32_t__ptr_freedBytes != NULL)
    {
        *uint32_t__ptr_freedBytes = 0u;
    }
    if (BOOL__G__FsOk == false)
    {
        return 0u;
    }
    for (uint8_t__pass = 0u; uint8_t__pass < 8u; uint8_t__pass++)
    {
        char char__batch[8][40];
        uint32_t uint32_t__sizes[8];
        uint8_t uint8_t__count = 0u;
        uint8_t uint8_t__idx;
        Dir dir__root = LittleFS.openDir("/");
        while (dir__root.next() && (uint8_t__count < 8u))
        {
            char char__rootPath[40];
            String string__nm = dir__root.fileName();
            if ((func__EspLog_ToRootPath(string__nm.c_str(),
                                         char__rootPath,
                                         sizeof(char__rootPath)) != false) &&
                (func__EspLog_IsOwnFile(char__rootPath) == false))
            {
                (void)snprintf(char__batch[uint8_t__count], sizeof(char__batch[0]), "%s", char__rootPath);
                uint32_t__sizes[uint8_t__count] = (uint32_t)dir__root.fileSize();
                uint8_t__count++;
            }
        }
        if (uint8_t__count == 0u)
        {
            break;
        }
        {
            uint8_t uint8_t__passRemoved = 0u;
            for (uint8_t__idx = 0u; uint8_t__idx < uint8_t__count; uint8_t__idx++)
            {
                if (LittleFS.remove(char__batch[uint8_t__idx]) != false)
                {
                    uint16_t__removed++;
                    uint8_t__passRemoved++;
                    uint32_t__freed += uint32_t__sizes[uint8_t__idx];
                }
            }
            if (uint8_t__passRemoved == 0u)
            {
                break;
            }
        }
    }
    BOOL__G__LogScanValid = false;
    if (uint32_t__ptr_freedBytes != NULL)
    {
        *uint32_t__ptr_freedBytes = uint32_t__freed;
    }
    return uint16_t__removed;
}
/**
 * @brief  [EN] Reset cached day/RAM log counters after an explicit full format,
 *              and re-persist the active phone clock if one was already known.
* @return None /
 */
static void func__EspLog_ResetStateAfterFormat(void)
{
    UINT8_T__G__LogIndexCount = 0u;
    BOOL__G__LogScanValid = false;
    UINT32_T__G__LogDayCode = 0xFFFFFFFFu;
    UINT16_T__G__LogRowsToday = 0u;
    BOOL__G__LogDayFull = false;
    BOOL__G__LogMarkerDone = false;
    UINT8_T__G__LogRamHead = 0u;
    UINT8_T__G__LogRamCount = 0u;
    BOOL__G__LogDrainWait = false;
    UINT8_T__G__LogWriteFault = 0u;
    UINT16_T__G__LogDroppedRows = 0u;
    /* the clock file was erased, so its fifth field (the beat second) is gone */
    BOOL__G__LogLastBeatValid = false;
    UINT32_T__G__LogLastBeatSec = 0u;
    BOOL__G__LogClockSaveWanted = false;
    BOOL__G__LogAnchorSaveWanted = false;
    if ((BOOL__G__FsOk != false) && (BOOL__G__LogClockValid != false))
    {
        func__EspLog_ClockSave();
    }
}
