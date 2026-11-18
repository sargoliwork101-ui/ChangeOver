/** @file simple_chart_store.h
 * @brief Bounded, verified chart journal and atomic delta snapshots.
 */
struct simple_chart_range_t
{
    uint32_t *pct;
    uint32_t *vin;
    uint32_t *pack;
    uint32_t *time;
    uint16_t *count;
    uint16_t *index;
    uint16_t capacity;
};
static simple_chart_range_t SIMPLE_CHART_RANGE_T__G__Ranges[4] =
{
    {UINT32_T__G__Chart1D_Pct, UINT32_T__G__Chart1D_Vin, UINT32_T__G__Chart1D_Vpack, UINT32_T__G__Chart1D_Time, &UINT16_T__G__Chart1D_Count, &UINT16_T__G__Chart1D_Index, SIMPLE_CHART_1D_SIZE},
    {UINT32_T__G__Chart7D_Pct, UINT32_T__G__Chart7D_Vin, UINT32_T__G__Chart7D_Vpack, UINT32_T__G__Chart7D_Time, &UINT16_T__G__Chart7D_Count, &UINT16_T__G__Chart7D_Index, SIMPLE_CHART_7D_SIZE},
    {UINT32_T__G__Chart1M_Pct, UINT32_T__G__Chart1M_Vin, UINT32_T__G__Chart1M_Vpack, UINT32_T__G__Chart1M_Time, &UINT16_T__G__Chart1M_Count, &UINT16_T__G__Chart1M_Index, SIMPLE_CHART_1M_SIZE},
    {UINT32_T__G__Chart3M_Pct, UINT32_T__G__Chart3M_Vin, UINT32_T__G__Chart3M_Vpack, UINT32_T__G__Chart3M_Time, &UINT16_T__G__Chart3M_Count, &UINT16_T__G__Chart3M_Index, SIMPLE_CHART_3M_SIZE}
};
struct simple_chart_bucket_t
{
    uint32_t bucket;
    uint32_t count;
    uint64_t sum[3];
};
static simple_chart_bucket_t SIMPLE_CHART_BUCKET_T__G__Buckets[3] = {};
static uint8_t UINT8_T__G__ChartFileBuf[SIMPLE_CHART_FILE_MAX_BYTES];
static uint32_t UINT32_T__G__ChartSequence = 0u;
static uint32_t UINT32_T__G__ChartJournalBytes = 0u;
static uint32_t UINT32_T__G__ChartSnapshotSequence = 0u;
static uint32_t UINT32_T__G__ChartCodecPrev[5] = {};
static uint8_t UINT8_T__G__ChartSinceFull = 0u;
static bool BOOL__G__ChartCodecValid = false;
static bool BOOL__G__ChartPending = false;
static uint32_t UINT32_T__G__ChartPending[4] = {};
/* ==================== Chart storage reset ==================== */
/** @brief Reset persistence cursors; optionally remove committed and staged files. */
static void func__Simple_ChartStoreReset(bool bool__removeFiles)
{
    UINT32_T__G__ChartSequence = 0u;
    UINT32_T__G__ChartSnapshotSequence = 0u;
    UINT32_T__G__ChartJournalBytes = 0u;
    BOOL__G__ChartCodecValid = false;
    UINT8_T__G__ChartSinceFull = 0u;
    BOOL__G__ChartPending = false;
    (void)memset(SIMPLE_CHART_BUCKET_T__G__Buckets, 0, sizeof(SIMPLE_CHART_BUCKET_T__G__Buckets));
    if (bool__removeFiles && BOOL__G__FsOk)
    {
        (void)LittleFS.remove(SIMPLE_CHART_JOURNAL_FILE);
        (void)LittleFS.remove(SIMPLE_CHARTS_FILE ".tmp");
    }
}
/* ==================== Ring insertion ==================== */
/** @brief Insert an accepted sample, dropping oldest samples beyond age or capacity. */
static void func__Simple_ChartInsert(uint8_t uint8_t__range, const uint32_t *uint32_t__ptr_sample)
{
    simple_chart_range_t &range__target = SIMPLE_CHART_RANGE_T__G__Ranges[uint8_t__range];
    const uint32_t uint32_t__days[4] = {1u, 7u, 30u, 90u};
    uint32_t uint32_t__span = uint32_t__days[uint8_t__range] * 86400u;
    while (*range__target.count > 0u)
    {
        uint16_t uint16_t__oldest = (uint16_t)((*range__target.index + range__target.capacity - *range__target.count) % range__target.capacity);
        if ((uint32_t__ptr_sample[0] <= range__target.time[uint16_t__oldest]) ||
            ((uint32_t__ptr_sample[0] - range__target.time[uint16_t__oldest]) < uint32_t__span)) break;
        (*range__target.count)--;
    }
    func__Simple_ChartPushRange(range__target.pct, range__target.vin, range__target.pack, range__target.time,
        range__target.count, range__target.index, range__target.capacity,
        uint32_t__ptr_sample[0], uint32_t__ptr_sample[1], uint32_t__ptr_sample[2], uint32_t__ptr_sample[3]);
}
/* ==================== Shared sample aggregation ==================== */
/** @brief Apply one committed source sample to the daily ring and calendar-aligned longer ranges. */
static void func__Simple_ChartApply(const uint32_t *uint32_t__ptr_sample)
{
    func__Simple_ChartInsert(0u, uint32_t__ptr_sample);
    const uint32_t uint32_t__period[3] = {21600u, 86400u, 86400u};
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 3u; uint8_t__i++)
    {
        simple_chart_bucket_t &bucket__current = SIMPLE_CHART_BUCKET_T__G__Buckets[uint8_t__i];
        uint32_t uint32_t__bucket = uint32_t__ptr_sample[0] / uint32_t__period[uint8_t__i];
        simple_chart_range_t &range__target = SIMPLE_CHART_RANGE_T__G__Ranges[uint8_t__i + 1u];
        bool bool__same = bucket__current.count && (bucket__current.bucket == uint32_t__bucket);
        if (!bool__same)
        {
            bucket__current = {};
            bucket__current.bucket = uint32_t__bucket;
            func__Simple_ChartInsert((uint8_t)(uint8_t__i + 1u), uint32_t__ptr_sample);
        }
        bucket__current.count++;
        uint16_t uint16_t__slot = (uint16_t)((*range__target.index + range__target.capacity - 1u) % range__target.capacity);
        uint32_t *uint32_t__ptr_values[3] = {range__target.pct, range__target.vin, range__target.pack};
        for (uint8_t uint8_t__field = 0u; uint8_t__field < 3u; uint8_t__field++)
        {
            bucket__current.sum[uint8_t__field] += uint32_t__ptr_sample[uint8_t__field + 1u];
            uint32_t__ptr_values[uint8_t__field][uint16_t__slot] = (uint32_t)(bucket__current.sum[uint8_t__field] / bucket__current.count);
        }
        range__target.time[uint16_t__slot] = uint32_t__ptr_sample[0];
    }
}
/* ==================== Snapshot integer ==================== */
/** @brief Encode or validate one unsigned varint inside the bounded snapshot buffer. */
static bool func__Simple_ChartNumber(bool bool__write, size_t &size_t__at, size_t size_t__limit, uint64_t &uint64_t__value)
{
    if (bool__write)
    {
        uint8_t uint8_t__bytes[10];
        size_t size_t__length = SimpleLogCodec::put(uint64_t__value, uint8_t__bytes);
        if ((size_t__at + size_t__length) > size_t__limit) return false;
        (void)memcpy(UINT8_T__G__ChartFileBuf + size_t__at, uint8_t__bytes, size_t__length);
        size_t__at += size_t__length;
        return true;
    }
    uint64_t__value = 0u;
    for (uint8_t uint8_t__shift = 0u; (uint8_t__shift < 64u) && (size_t__at < size_t__limit); uint8_t__shift = (uint8_t)(uint8_t__shift+7u))
    {
        uint8_t uint8_t__byte = UINT8_T__G__ChartFileBuf[size_t__at++];
        if ((uint8_t__shift == 63u) && (uint8_t__byte > 1u)) return false;
        uint64_t__value |= (uint64_t)(uint8_t__byte & 127u) << uint8_t__shift;
        if ((uint8_t__byte & 128u) == 0u) return true;
    }
    return false;
}
/* ==================== Snapshot rings ==================== */
/** @brief Encode or validate all rings and live aggregation state; publish only on the second read pass. */
static bool func__Simple_ChartSnapshotData(bool bool__write, bool bool__publish, size_t &size_t__at, size_t size_t__limit)
{
    uint16_t uint16_t__rangeCounts[4] = {};
    uint32_t uint32_t__rangeLast[4] = {};
    for (uint8_t uint8_t__r = 0u; uint8_t__r < 4u; uint8_t__r++)
    {
        simple_chart_range_t &range__target = SIMPLE_CHART_RANGE_T__G__Ranges[uint8_t__r];
        uint64_t uint64_t__count = *range__target.count;
        if (!func__Simple_ChartNumber(bool__write, size_t__at, size_t__limit, uint64_t__count) ||
            (uint64_t__count > range__target.capacity)) return false;
        uint16_t__rangeCounts[uint8_t__r] = (uint16_t)uint64_t__count;
        uint32_t uint32_t__prev[4] = {};
        uint32_t uint32_t__previousStamp = 0u;
        for (uint16_t uint16_t__i = 0u; uint16_t__i < (uint16_t)uint64_t__count; uint16_t__i++)
        {
            uint16_t uint16_t__slot = bool__write ? (uint16_t)((*range__target.index + range__target.capacity - *range__target.count + uint16_t__i) % range__target.capacity) : uint16_t__i;
            uint32_t *uint32_t__ptr_values[4] = {range__target.time, range__target.pct, range__target.vin, range__target.pack};
            for (uint8_t uint8_t__f = 0u; uint8_t__f < 4u; uint8_t__f++)
            {
                int64_t int64_t__diff = (int64_t)uint32_t__ptr_values[uint8_t__f][uint16_t__slot] - uint32_t__prev[uint8_t__f];
                uint64_t uint64_t__zig = (int64_t__diff < 0) ? (uint64_t)(-int64_t__diff)*2u-1u : (uint64_t)int64_t__diff*2u;
                if (!func__Simple_ChartNumber(bool__write, size_t__at, size_t__limit, uint64_t__zig)) return false;
                if (uint64_t__zig > 8589934590ULL) return false;
                int64_t int64_t__decoded = (int64_t)uint32_t__prev[uint8_t__f];
                int64_t__decoded += (uint64_t__zig & 1u) ? -(int64_t)(uint64_t__zig/2u)-1 : (int64_t)(uint64_t__zig/2u);
                if ((int64_t__decoded < 0) || (int64_t__decoded > UINT32_MAX)) return false;
                if (uint8_t__f == 0u)
                {
                    if (((uint32_t)int64_t__decoded < SIMPLE_TIME_VALID_MIN) ||
                        ((uint16_t__i > 0u) && ((uint32_t)int64_t__decoded <= uint32_t__previousStamp))) return false;
                    uint32_t__previousStamp = (uint32_t)int64_t__decoded;
                }
                if ((uint8_t__f == 1u) && (int64_t__decoded > 100)) return false;
                uint32_t__prev[uint8_t__f] = (uint32_t)int64_t__decoded;
                if (!bool__write && bool__publish) uint32_t__ptr_values[uint8_t__f][uint16_t__slot] = (uint32_t)int64_t__decoded;
            }
        }
        uint32_t__rangeLast[uint8_t__r] = uint32_t__previousStamp;
        if (!bool__write && bool__publish)
        {
            *range__target.count = (uint16_t)uint64_t__count;
            *range__target.index = (uint16_t)(uint64_t__count % range__target.capacity);
        }
    }
    for (uint8_t uint8_t__r = 0u; uint8_t__r < 3u; uint8_t__r++)
    {
        simple_chart_bucket_t &bucket__current = SIMPLE_CHART_BUCKET_T__G__Buckets[uint8_t__r];
        simple_chart_bucket_t bucket__parsed = {};
        for (uint8_t uint8_t__f = 0u; uint8_t__f < 5u; uint8_t__f++)
        {
            uint64_t uint64_t__value = (uint8_t__f == 0u) ? bucket__current.bucket :
                ((uint8_t__f == 1u) ? bucket__current.count : bucket__current.sum[uint8_t__f - 2u]);
            if (!func__Simple_ChartNumber(bool__write, size_t__at, size_t__limit, uint64_t__value)) return false;
            if ((uint8_t__f < 2u) && (uint64_t__value > UINT32_MAX)) return false;
            if (!bool__write)
            {
                if (uint8_t__f == 0u) bucket__parsed.bucket = (uint32_t)uint64_t__value;
                else if (uint8_t__f == 1u) bucket__parsed.count = (uint32_t)uint64_t__value;
                else bucket__parsed.sum[uint8_t__f-2u] = uint64_t__value;
            }
        }
        if (!bool__write)
        {
            uint32_t uint32_t__periodSec = (uint8_t__r == 0u) ? 21600u : 86400u;
            if (bucket__parsed.count > uint32_t__periodSec) return false;
            if (bucket__parsed.count && ((uint16_t__rangeCounts[uint8_t__r+1u] == 0u) ||
                (bucket__parsed.bucket != uint32_t__rangeLast[uint8_t__r+1u] / uint32_t__periodSec))) return false;
            for (uint8_t uint8_t__f = 0u; uint8_t__f < 3u; uint8_t__f++)
            {
                uint64_t uint64_t__maximum = (uint64_t)bucket__parsed.count * ((uint8_t__f == 0u) ? 100u : UINT32_MAX);
                if (bucket__parsed.sum[uint8_t__f] > uint64_t__maximum) return false;
            }
            if (bool__publish) bucket__current = bucket__parsed;
        }
    }
    return true;
}
/* ==================== Atomic chart checkpoint ==================== */
/** @brief Compact the bounded journal only after a complete read-verified snapshot is committed. */
static bool func__Simple_ChartCheckpoint(void)
{
    if (!BOOL__G__FsOk) return false;
    size_t size_t__at = SIMPLE_CHART_FILE_HEADER;
    (void)memcpy(UINT8_T__G__ChartFileBuf, "CHTD\2\4\0\0", 8u);
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 4u; uint8_t__i++)
        UINT8_T__G__ChartFileBuf[8u+uint8_t__i] = (uint8_t)(UINT32_T__G__ChartSequence >> (8u*uint8_t__i));
    if (!func__Simple_ChartSnapshotData(true, false, size_t__at, sizeof(UINT8_T__G__ChartFileBuf)-2u)) return false;
    uint16_t uint16_t__crc = SimpleLogCodec::crc(UINT8_T__G__ChartFileBuf, size_t__at);
    UINT8_T__G__ChartFileBuf[size_t__at++] = (uint8_t)uint16_t__crc;
    UINT8_T__G__ChartFileBuf[size_t__at++] = (uint8_t)(uint16_t__crc >> 8);
    if (!func__Simple_StorageCommit(SIMPLE_CHARTS_FILE, UINT8_T__G__ChartFileBuf, size_t__at)) return false;
    UINT32_T__G__ChartSnapshotSequence = UINT32_T__G__ChartSequence;
    /* A crash here leaves old journal records; sequence numbers prevent duplicate replay. */
    if (LittleFS.exists(SIMPLE_CHART_JOURNAL_FILE) && !LittleFS.remove(SIMPLE_CHART_JOURNAL_FILE)) return false;
    UINT32_T__G__ChartJournalBytes = 0u;
    BOOL__G__ChartCodecValid = false;
    return true;
}
/* ==================== Journal decode ==================== */
/** @brief Validate one full/delta record; never decode a delta without its committed base. */
static bool func__Simple_ChartDecode(const uint8_t *uint8_t__ptr_frame, size_t size_t__length, uint32_t *uint32_t__ptr_values)
{
    if ((size_t__length < 6u) || (uint8_t__ptr_frame[0] != 0xa6u) || (uint8_t__ptr_frame[1] != 0xc4u) ||
        (uint8_t__ptr_frame[3] > 1u) || ((size_t)uint8_t__ptr_frame[2]+6u != size_t__length)) return false;
    uint16_t uint16_t__crc = SimpleLogCodec::crc(uint8_t__ptr_frame+2u, size_t__length-4u);
    if (((uint8_t)uint16_t__crc != uint8_t__ptr_frame[size_t__length-2u]) ||
        ((uint8_t)(uint16_t__crc>>8) != uint8_t__ptr_frame[size_t__length-1u])) return false;
    bool bool__delta = uint8_t__ptr_frame[3] != 0u;
    if (bool__delta && (!BOOL__G__ChartCodecValid || (UINT8_T__G__ChartSinceFull >= 15u))) return false;
    size_t size_t__at = 4u;
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 5u; uint8_t__i++)
    {
        uint64_t uint64_t__value = 0u;
        if (!SimpleLogCodec::get(uint8_t__ptr_frame, size_t__length-2u, size_t__at, uint64_t__value)) return false;
        if (uint64_t__value > (bool__delta ? 8589934590ULL : UINT32_MAX)) return false;
        int64_t int64_t__decoded = (int64_t)uint64_t__value;
        if (bool__delta)
        {
            int64_t__decoded = (int64_t)UINT32_T__G__ChartCodecPrev[uint8_t__i];
            int64_t__decoded += (uint64_t__value & 1u) ? -(int64_t)(uint64_t__value/2u)-1 : (int64_t)(uint64_t__value/2u);
        }
        if ((int64_t__decoded < 0) || (int64_t__decoded > UINT32_MAX)) return false;
        uint32_t__ptr_values[uint8_t__i] = (uint32_t)int64_t__decoded;
    }
    return (size_t__at == size_t__length-2u) && (uint32_t__ptr_values[1] >= SIMPLE_TIME_VALID_MIN) && (uint32_t__ptr_values[2] <= 100u);
}
/* ==================== Commit one sample ==================== */
/** @brief Append and read back a sample; only then advance RAM state and the trigger baseline. */
static bool func__Simple_ChartAppend(void)
{
    if (!BOOL__G__FsOk || (UINT32_T__G__ChartSequence == UINT32_MAX)) return false;
    /* A clear checkpoint can be newer than the surviving journal's final sequence. */
    if (BOOL__G__ChartCodecValid && UINT32_T__G__ChartCodecPrev[0] < UINT32_T__G__ChartSnapshotSequence)
        if (!func__Simple_ChartCheckpoint()) return false;
    bool bool__missingJournal = !LittleFS.exists(SIMPLE_CHART_JOURNAL_FILE);
    bool bool__missingSnapshot = UINT32_T__G__ChartSnapshotSequence && !LittleFS.exists(SIMPLE_CHARTS_FILE);
    bool bool__shortJournal = false;
    if (!bool__missingJournal)
    {
        File file__size = LittleFS.open(SIMPLE_CHART_JOURNAL_FILE, "r");
        if (!file__size) return false;
        bool__shortJournal = file__size.size() < UINT32_T__G__ChartJournalBytes;
        file__size.close();
    }
    if (bool__missingSnapshot || bool__shortJournal || (bool__missingJournal && UINT32_T__G__ChartSequence) ||
        ((UINT32_T__G__ChartJournalBytes + 56u) > SIMPLE_CHART_JOURNAL_MAX_BYTES))
        if (!func__Simple_ChartCheckpoint()) return false;
    if (!LittleFS.exists(SIMPLE_CHART_JOURNAL_FILE))
    {
        UINT32_T__G__ChartJournalBytes = 0u;
        BOOL__G__ChartCodecValid = false;
    }
    uint32_t uint32_t__values[5] = {UINT32_T__G__ChartSequence+1u, UINT32_T__G__ChartPending[0],
        UINT32_T__G__ChartPending[1], UINT32_T__G__ChartPending[2], UINT32_T__G__ChartPending[3]};
    uint8_t uint8_t__frame[56] = {0xa6u, 0xc4u, 0u, 0u};
    bool bool__delta = BOOL__G__ChartCodecValid && (UINT8_T__G__ChartSinceFull < 15u);
    uint8_t__frame[3] = bool__delta ? 1u : 0u;
    size_t size_t__length = 4u;
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 5u; uint8_t__i++)
    {
        int64_t int64_t__diff = (int64_t)uint32_t__values[uint8_t__i] - UINT32_T__G__ChartCodecPrev[uint8_t__i];
        uint64_t uint64_t__value = bool__delta ? ((int64_t__diff < 0) ? (uint64_t)(-int64_t__diff)*2u-1u : (uint64_t)int64_t__diff*2u) : uint32_t__values[uint8_t__i];
        size_t__length += SimpleLogCodec::put(uint64_t__value, uint8_t__frame+size_t__length);
    }
    uint8_t__frame[2] = (uint8_t)(size_t__length-4u);
    uint16_t uint16_t__crc = SimpleLogCodec::crc(uint8_t__frame+2u, size_t__length-2u);
    uint8_t__frame[size_t__length++] = (uint8_t)uint16_t__crc;
    uint8_t__frame[size_t__length++] = (uint8_t)(uint16_t__crc>>8);
    if (!func__Simple_StorageQuota(SIMPLE_CHART_JOURNAL_FILE, UINT32_T__G__ChartJournalBytes + size_t__length) ||
        !func__Simple_StorageRoom(size_t__length, UINT32_T__G__LogDayCode)) return false;
    File file__out = LittleFS.open(SIMPLE_CHART_JOURNAL_FILE, "a");
    if (!file__out) return false;
    bool bool__ok = (file__out.size() == UINT32_T__G__ChartJournalBytes) || file__out.truncate(UINT32_T__G__ChartJournalBytes);
    if (bool__ok) bool__ok = file__out.write(uint8_t__frame, size_t__length) == size_t__length;
    file__out.close();
    if (bool__ok)
    {
        File file__check = LittleFS.open(SIMPLE_CHART_JOURNAL_FILE, "r");
        bool__ok = file__check && file__check.seek(UINT32_T__G__ChartJournalBytes);
        for (size_t size_t__i = 0u; bool__ok && (size_t__i < size_t__length); size_t__i++)
            bool__ok = file__check.read() == uint8_t__frame[size_t__i];
        file__check.close();
    }
    if (!bool__ok)
    {
        File file__repair = LittleFS.open(SIMPLE_CHART_JOURNAL_FILE, "a");
        if (file__repair) (void)file__repair.truncate(UINT32_T__G__ChartJournalBytes);
        file__repair.close();
        return false;
    }
    UINT32_T__G__ChartSequence++;
    UINT32_T__G__ChartJournalBytes += (uint32_t)size_t__length;
    (void)memcpy(UINT32_T__G__ChartCodecPrev, uint32_t__values, sizeof(uint32_t__values));
    BOOL__G__ChartCodecValid = true;
    UINT8_T__G__ChartSinceFull = bool__delta ? (uint8_t)(UINT8_T__G__ChartSinceFull+1u) : 0u;
    func__Simple_ChartApply(UINT32_T__G__ChartPending);
    UINT32_T__G__LastChartSaveSec = UINT32_T__G__ChartPending[0];
    BOOL__G__ChartPending = false;
    BOOL__G__ChartsDirty = false;
    return true;
}
/* ==================== Pending chart save ==================== */
/** @brief Retry a pending accepted observation; explicit dirty snapshots use the atomic checkpoint path. */
static void func__Simple_ChartsSave(void)
{
    if (!BOOL__G__ChartsDirty) return;
    if (BOOL__G__ChartPending) (void)func__Simple_ChartAppend();
    else if (func__Simple_ChartCheckpoint()) BOOL__G__ChartsDirty = false;
}
/* ==================== Time OR voltage trigger ==================== */
/**
 * @brief Accept the first sample, or 3600 wall seconds OR 500 mV from the last committed sample.
 * @param uint32_t__pct Percentage; uint32_t__vin Input mV; uint32_t__vpack Pack mV.
 */
static void func__Simple_ChartAddTimed(uint32_t uint32_t__pct, uint32_t uint32_t__vin, uint32_t uint32_t__vpack)
{
    if (!func__Simple_TimeBootReady()) return;
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    if ((uint32_t__nowSec < SIMPLE_TIME_VALID_MIN) || (uint32_t__pct > 100u)) return;
    if (BOOL__G__ChartPending)
    {
        func__Simple_ChartsSave();
        if (BOOL__G__ChartPending) return;
    }
    if (UINT16_T__G__Chart1D_Count > 0u)
    {
        uint16_t uint16_t__last = (uint16_t)((UINT16_T__G__Chart1D_Index + SIMPLE_CHART_1D_SIZE - 1u) % SIMPLE_CHART_1D_SIZE);
        uint32_t uint32_t__lastSec = UINT32_T__G__Chart1D_Time[uint16_t__last];
        if (uint32_t__nowSec <= uint32_t__lastSec) return;
        uint32_t uint32_t__vinDiff = func__SimpleLog_AbsDiff(uint32_t__vin, UINT32_T__G__Chart1D_Vin[uint16_t__last]);
        uint32_t uint32_t__packDiff = func__SimpleLog_AbsDiff(uint32_t__vpack, UINT32_T__G__Chart1D_Vpack[uint16_t__last]);
        if (((uint32_t__nowSec-uint32_t__lastSec) < SIMPLE_CHART_SAMPLE_SEC) &&
            (uint32_t__vinDiff < SIMPLE_CHART_STEP_MV) && (uint32_t__packDiff < SIMPLE_CHART_STEP_MV)) return;
    }
    UINT32_T__G__ChartPending[0] = uint32_t__nowSec;
    UINT32_T__G__ChartPending[1] = uint32_t__pct;
    UINT32_T__G__ChartPending[2] = uint32_t__vin;
    UINT32_T__G__ChartPending[3] = uint32_t__vpack;
    BOOL__G__ChartPending = true;
    BOOL__G__ChartsDirty = true;
    func__Simple_ChartsSave();
}
/* ==================== Restore verified charts ==================== */
/** @brief Validate snapshots before publishing, then replay only the complete verified journal prefix. */
static void func__Simple_ChartsLoad(void)
{
    if (!BOOL__G__FsOk) return;
    func__Simple_ChartStoreReset(false);
    for (uint8_t uint8_t__r = 0u; uint8_t__r < 4u; uint8_t__r++)
    {
        *SIMPLE_CHART_RANGE_T__G__Ranges[uint8_t__r].count = 0u;
        *SIMPLE_CHART_RANGE_T__G__Ranges[uint8_t__r].index = 0u;
    }
    File file__snapshot = LittleFS.open(SIMPLE_CHARTS_FILE, "r");
    if (file__snapshot)
    {
        size_t size_t__size = file__snapshot.size();
        bool bool__ok = (size_t__size >= SIMPLE_CHART_FILE_HEADER+2u) && (size_t__size <= sizeof(UINT8_T__G__ChartFileBuf));
        if (bool__ok) bool__ok = file__snapshot.readBytes((char *)UINT8_T__G__ChartFileBuf, size_t__size) == size_t__size;
        file__snapshot.close();
        if (bool__ok)
        {
            uint16_t uint16_t__crc = SimpleLogCodec::crc(UINT8_T__G__ChartFileBuf, size_t__size-2u);
            bool__ok = (memcmp(UINT8_T__G__ChartFileBuf, "CHTD\2\4\0\0", 8u) == 0) &&
                ((uint8_t)uint16_t__crc == UINT8_T__G__ChartFileBuf[size_t__size-2u]) &&
                ((uint8_t)(uint16_t__crc>>8) == UINT8_T__G__ChartFileBuf[size_t__size-1u]);
            size_t size_t__at = SIMPLE_CHART_FILE_HEADER;
            if (bool__ok) bool__ok = func__Simple_ChartSnapshotData(false, false, size_t__at, size_t__size-2u) && (size_t__at == size_t__size-2u);
            if (bool__ok)
            {
                size_t__at = SIMPLE_CHART_FILE_HEADER;
                (void)func__Simple_ChartSnapshotData(false, true, size_t__at, size_t__size-2u);
                for (uint8_t uint8_t__i = 0u; uint8_t__i < 4u; uint8_t__i++)
                    UINT32_T__G__ChartSequence |= (uint32_t)UINT8_T__G__ChartFileBuf[8u+uint8_t__i] << (8u*uint8_t__i);
            }
        }
    }
    UINT32_T__G__ChartSnapshotSequence = UINT32_T__G__ChartSequence;
    File file__journal = LittleFS.open(SIMPLE_CHART_JOURNAL_FILE, "r");
    if (file__journal && (file__journal.size() <= SIMPLE_CHART_JOURNAL_MAX_BYTES))
    {
        while ((uint32_t)file__journal.size() > UINT32_T__G__ChartJournalBytes)
        {
            yield();
            uint8_t uint8_t__frame[56];
            if (file__journal.readBytes((char *)uint8_t__frame, 4u) != 4u) break;
            size_t size_t__length = (size_t)uint8_t__frame[2]+6u;
            if ((size_t__length > sizeof(uint8_t__frame)) ||
                (file__journal.readBytes((char *)uint8_t__frame+4u, size_t__length-4u) != size_t__length-4u)) break;
            uint32_t uint32_t__values[5];
            if (!func__Simple_ChartDecode(uint8_t__frame, size_t__length, uint32_t__values)) break;
            if (BOOL__G__ChartCodecValid && (uint32_t__values[0] != UINT32_T__G__ChartCodecPrev[0]+1u)) break;
            if (uint32_t__values[0] > UINT32_T__G__ChartSequence)
            {
                if ((uint32_t__values[0] != UINT32_T__G__ChartSequence+1u) ||
                    (uint32_t__values[1] <= func__Simple_ChartLastSec(UINT32_T__G__Chart1D_Time, UINT16_T__G__Chart1D_Count, UINT16_T__G__Chart1D_Index, SIMPLE_CHART_1D_SIZE))) break;
                func__Simple_ChartApply(uint32_t__values+1u);
                UINT32_T__G__ChartSequence = uint32_t__values[0];
            }
            UINT8_T__G__ChartSinceFull = uint8_t__frame[3] ? (uint8_t)(UINT8_T__G__ChartSinceFull+1u) : 0u;
            BOOL__G__ChartCodecValid = true;
            (void)memcpy(UINT32_T__G__ChartCodecPrev, uint32_t__values, sizeof(uint32_t__values));
            UINT32_T__G__ChartJournalBytes += (uint32_t)size_t__length;
        }
    }
    file__journal.close();
    UINT32_T__G__LastChartSaveSec = func__Simple_ChartLastSec(UINT32_T__G__Chart1D_Time, UINT16_T__G__Chart1D_Count, UINT16_T__G__Chart1D_Index, SIMPLE_CHART_1D_SIZE);
    BOOL__G__ChartsDirty = false;
}
