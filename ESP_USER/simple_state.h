#include <LittleFS.h>
static bool BOOL__G__FsOk = false;
static bool BOOL__G__FsMountFailed = false;
static simple_rx_state_t SIMPLE_RX_STATE_T__G__RxState = SIMPLE_RX_WAIT_SOF0;
static uint8_t  UINT8_T__G__RxType = 0u;
static uint16_t UINT16_T__G__RxLen = 0u;
static uint16_t UINT16_T__G__RxIndex = 0u;
static uint8_t  UINT8_T__G__RxPayload[SIMPLE_LINK_MAX_PAYLOAD];
static uint32_t UINT32_T__G__TlmField[SIMPLE_LINK_TLM_FIELD_COUNT];
static uint8_t  UINT8_T__G__TlmFlags = 0u;
static uint8_t  UINT8_T__G__TlmFlags2 = 0u;
static uint16_t UINT16_T__G__TlmSeq = 0u;
static uint32_t UINT32_T__G__TlmFrameCount = 0u;
static uint32_t UINT32_T__G__LastTlmMs = 0u;
static bool     BOOL__G__TlmSeen = false;
static uint32_t UINT32_T__G__RxCrcError = 0u;
static uint32_t UINT32_T__G__RxVersionMismatch = 0u;
static uint16_t UINT16_T__G__RxCrc = 0u;
static uint8_t  UINT8_T__G__RxCrcLow = 0u;
/*
 * ==================== (v1.111) ====================
 */
static bool     BOOL__G__HelloSeen = false;
static uint32_t UINT32_T__G__LastHelloAckMs = 0u;
static uint32_t UINT32_T__G__LastHelloTxMs = 0u;
static uint32_t UINT32_T__G__HelloAckCount = 0u;
static uint32_t UINT32_T__G__HelloLossCount = 0u;
static uint8_t  UINT8_T__G__BoardProtoVersion = 0u;
static uint8_t  UINT8_T__G__BoardFwVersion = 0u;
static uint8_t  UINT8_T__G__BoardFlags = 0u;
static uint32_t UINT32_T__G__BoardUptimeSec = 0u;
static uint32_t UINT32_T__G__BoardRxFrames = 0u;
static uint16_t UINT16_T__G__BoardCrcErrors = 0u;
static uint16_t UINT16_T__G__BoardVerErrors = 0u;
static uint16_t UINT16_T__G__BoardParamCount = 0u;
static uint16_t UINT16_T__G__BoardPeriodMs = 0u;
static uint32_t UINT32_T__G__ChartVin[SIMPLE_CHART_HISTORY_SIZE];
static uint32_t UINT32_T__G__ChartVpack[SIMPLE_CHART_HISTORY_SIZE];
static uint32_t UINT32_T__G__ChartPct[SIMPLE_CHART_HISTORY_SIZE];
static uint32_t UINT32_T__G__ChartImb[SIMPLE_CHART_HISTORY_SIZE];
static uint8_t  UINT8_T__G__ChartIndex = 0u;
static uint8_t  UINT8_T__G__ChartCount = 0u;
static uint32_t UINT32_T__G__LastChartMs = 0u;
static uint32_t UINT32_T__G__Chart1D_Pct[SIMPLE_CHART_1D_SIZE];
static uint32_t UINT32_T__G__Chart1D_Vin[SIMPLE_CHART_1D_SIZE];
static uint32_t UINT32_T__G__Chart1D_Vpack[SIMPLE_CHART_1D_SIZE];
static uint32_t UINT32_T__G__Chart1D_Time[SIMPLE_CHART_1D_SIZE];
static uint16_t UINT16_T__G__Chart1D_Count = 0u;
static uint16_t UINT16_T__G__Chart1D_Index = 0u;
static uint32_t UINT32_T__G__Last1D_Ms = 0u;
static uint32_t UINT32_T__G__Chart7D_Pct[SIMPLE_CHART_7D_SIZE];
static uint32_t UINT32_T__G__Chart7D_Vin[SIMPLE_CHART_7D_SIZE];
static uint32_t UINT32_T__G__Chart7D_Vpack[SIMPLE_CHART_7D_SIZE];
static uint32_t UINT32_T__G__Chart7D_Time[SIMPLE_CHART_7D_SIZE];
static uint16_t UINT16_T__G__Chart7D_Count = 0u;
static uint16_t UINT16_T__G__Chart7D_Index = 0u;
static uint32_t UINT32_T__G__Last7D_Ms = 0u;
static uint32_t UINT32_T__G__Chart1M_Pct[SIMPLE_CHART_1M_SIZE];
static uint32_t UINT32_T__G__Chart1M_Vin[SIMPLE_CHART_1M_SIZE];
static uint32_t UINT32_T__G__Chart1M_Vpack[SIMPLE_CHART_1M_SIZE];
static uint32_t UINT32_T__G__Chart1M_Time[SIMPLE_CHART_1M_SIZE];
static uint16_t UINT16_T__G__Chart1M_Count = 0u;
static uint16_t UINT16_T__G__Chart1M_Index = 0u;
static uint32_t UINT32_T__G__Last1M_Ms = 0u;
static uint32_t UINT32_T__G__Chart3M_Pct[SIMPLE_CHART_3M_SIZE];
static uint32_t UINT32_T__G__Chart3M_Vin[SIMPLE_CHART_3M_SIZE];
static uint32_t UINT32_T__G__Chart3M_Vpack[SIMPLE_CHART_3M_SIZE];
static uint32_t UINT32_T__G__Chart3M_Time[SIMPLE_CHART_3M_SIZE];
static uint16_t UINT16_T__G__Chart3M_Count = 0u;
static uint16_t UINT16_T__G__Chart3M_Index = 0u;
static uint32_t UINT32_T__G__Last3M_Ms = 0u;
typedef struct
{
    uint32_t timestamp;
    uint32_t faultMask;
    uint32_t vIn;
    uint32_t vPack;
    uint32_t vLow;
    uint32_t vHigh;
    uint32_t state1;
    uint32_t state2;
} simple_diag_error_t;
typedef struct
{
    uint32_t offTime;
    uint32_t onTime;
    uint32_t durationSec;
} simple_power_event_t;
typedef struct
{
    uint32_t timestamp;
    uint8_t channel;
    uint32_t durationSec;
} simple_charge_event_t;
static simple_diag_error_t SIMPLE_DIAG_ERROR_T__G__Errors[SIMPLE_DIAG_MAX_ERRORS];
static uint8_t UINT8_T__G__DiagErrorCount = 0u;
static simple_power_event_t SIMPLE_POWER_EVENT_T__G__PowerEvents[SIMPLE_DIAG_MAX_POWER_EVENTS];
static uint8_t UINT8_T__G__PowerEventCount = 0u;
static simple_charge_event_t SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[SIMPLE_DIAG_MAX_POWER_EVENTS];
static uint8_t UINT8_T__G__ChargeHistoryCount = 0u;
static uint32_t UINT32_T__G__TotalChargeSec = 0u;
static uint32_t UINT32_T__G__ChargeCount = 0u;
static uint32_t UINT32_T__G__ChargeCountCh1 = 0u;
static uint32_t UINT32_T__G__ChargeCountCh2 = 0u;
static uint32_t UINT32_T__G__ChargeSecCh1 = 0u;
static uint32_t UINT32_T__G__ChargeSecCh2 = 0u;
static uint32_t UINT32_T__G__UptimeSec = 0u;
static uint32_t UINT32_T__G__ResetCount = 0u;
static uint32_t UINT32_T__G__BoardTimeSec = 0u;
static bool BOOL__G__DiagValid = false;
static uint32_t UINT32_T__G__LastDiagReqMs = 0u;
static uint32_t UINT32_T__G__BoardParam[SIMPLE_PARAM_COUNT];
static bool BOOL__G__BoardParamKnown[SIMPLE_PARAM_COUNT];
static bool BOOL__G__BoardParamSeen = false;
static bool BOOL__G__BoardParamsDirty = false;
static uint32_t UINT32_T__G__ParamBulkFrames = 0u;
static uint32_t UINT32_T__G__LastParamsReqMs = 0u;
static uint32_t func__Simple_BoardParamDefault(uint8_t uint8_t__id)
{
    switch (uint8_t__id)
    {
        case SIMPLE_PARAM_GREEN_PERIOD_MS:  return 1000u;
        case SIMPLE_PARAM_GREEN_MIN_OFF_MS: return 10u;
        case SIMPLE_PARAM_YELLOW_PERIOD_MS: return 1000u;
        case SIMPLE_PARAM_YELLOW_MIN_ON_MS: return 150u;
        case SIMPLE_PARAM_PCT_VMIN_MV:      return 21000u;
        case SIMPLE_PARAM_PCT_VMAX_MV:      return 29000u;
        case SIMPLE_PARAM_FULL_ENTER_PCT:   return 100u;
        case SIMPLE_PARAM_FULL_EXIT_PCT:    return 95u;
        case SIMPLE_PARAM_CHG_HYST_PCT:     return 5u;
        case SIMPLE_PARAM_RUN_HYST_PCT:     return 2u;
        case SIMPLE_PARAM_RUN_ZERO_EXIT:    return 2u;
        case SIMPLE_PARAM_RUN_ONE_EXIT:     return 3u;
        case SIMPLE_PARAM_CHG_PCT_VMIN_MV:  return 21000u;
        case SIMPLE_PARAM_CHG_PCT_VMAX_MV:  return 29000u;
        case SIMPLE_PARAM_WIFI_IDLE_LOW_POWER_MS: return SIMPLE_WIFI_IDLE_LOW_POWER_MS;
        case SIMPLE_PARAM_WIFI_CYCLE_ON_MS:       return SIMPLE_WIFI_CYCLE_ON_MS;
        case SIMPLE_PARAM_WIFI_CYCLE_OFF_MS:      return SIMPLE_WIFI_CYCLE_OFF_MS;
        case SIMPLE_PARAM_WIFI_BAT_CUTOFF_PCT:    return SIMPLE_WIFI_BAT_CUTOFF_PCT;
        case SIMPLE_PARAM_INPUT_MIN_MV:         return 21000u;
        case SIMPLE_PARAM_INPUT_MAX_MV:         return 28000u;
        case SIMPLE_PARAM_INPUT_HYST_LOW_MV:    return 500u;
        case SIMPLE_PARAM_INPUT_HYST_HIGH_MV:   return 500u;
        case SIMPLE_PARAM_OV_HYST_MV:           return 1000u;
        default:                            return 0u;
    }
}
static uint32_t func__Simple_BoardParam(uint8_t uint8_t__id)
{
    if (uint8_t__id >= SIMPLE_PARAM_COUNT) return 0u;
    if (BOOL__G__BoardParamKnown[uint8_t__id]) return UINT32_T__G__BoardParam[uint8_t__id];
    return func__Simple_BoardParamDefault(uint8_t__id);
}
static uint8_t  UINT8_T__G__SlotQueueId[SIMPLE_SLOT_QUEUE_LEN];
static uint32_t UINT32_T__G__SlotQueueValue[SIMPLE_SLOT_QUEUE_LEN];
static bool     BOOL__G__SlotQueuePending[SIMPLE_SLOT_QUEUE_LEN];
static uint32_t UINT32_T__G__LastSlotTxMs = 0u;
static uint8_t  UINT8_T__G__SlotLastId = 0u;
static uint32_t UINT32_T__G__SlotLastValue = 0u;
static bool     BOOL__G__SlotLastKnown = false;
static uint32_t UINT32_T__G__SlotRequestCount = 0u;
static uint32_t UINT32_T__G__SlotEchoCount = 0u;
static uint32_t UINT32_T__G__SlotNakCount = 0u;
static uint8_t  UINT8_T__G__SlotNakId = 0u;
static uint8_t  UINT8_T__G__SlotNakCode = 0u;
static uint8_t  UINT8_T__G__SlotBusyReject = 0u;
static const uint8_t UINT8_T__G__ParamUiIds[SIMPLE_PARAM_UI_COUNT] =
{
    SIMPLE_PARAM_GREEN_PERIOD_MS,   /* 0 */
    SIMPLE_PARAM_GREEN_MIN_OFF_MS,  /* 1 */
    SIMPLE_PARAM_YELLOW_PERIOD_MS,  /* 2 */
    SIMPLE_PARAM_YELLOW_MIN_ON_MS,  /* 3 */
    SIMPLE_PARAM_PCT_VMIN_MV,       /* 4 */
    SIMPLE_PARAM_PCT_VMAX_MV,       /* 5 */
    SIMPLE_PARAM_FULL_ENTER_PCT,    /* 6 */
    SIMPLE_PARAM_FULL_EXIT_PCT,     /* 7 */
    SIMPLE_PARAM_CHG_HYST_PCT,      /* 8 */
    SIMPLE_PARAM_RUN_HYST_PCT,      /* 9 */
    SIMPLE_PARAM_RUN_ZERO_EXIT,     /* 10 */
    SIMPLE_PARAM_RUN_ONE_EXIT,      /* 11 */
    SIMPLE_PARAM_CHG_PCT_VMIN_MV,   /* 12 */
    SIMPLE_PARAM_CHG_PCT_VMAX_MV,   /* 13 */
    SIMPLE_PARAM_INPUT_MIN_MV,      /* 14 */
    SIMPLE_PARAM_INPUT_MAX_MV,      /* 15 */
    SIMPLE_PARAM_INPUT_HYST_LOW_MV, /* 16 */
    SIMPLE_PARAM_INPUT_HYST_HIGH_MV, /* 17 */
    SIMPLE_PARAM_OV_HYST_MV          /* 18 */
};
static bool func__Simple_IsUiParamId(uint8_t uint8_t__id)
{
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_PARAM_UI_COUNT; uint8_t__i++)
    {
        if (UINT8_T__G__ParamUiIds[uint8_t__i] == uint8_t__id)
        {
            return true;
        }
    }
    return false;
}
static void func__Simple_StoreBoardParam(uint8_t uint8_t__id, uint32_t uint32_t__value)
{
    uint32_t uint32_t__prevEffective;
    if (uint8_t__id >= SIMPLE_PARAM_COUNT) return;
    uint32_t__prevEffective = func__Simple_BoardParam(uint8_t__id);
    if (func__Simple_IsUiParamId(uint8_t__id) && (uint32_t__prevEffective != uint32_t__value))
    {
        BOOL__G__BoardParamsDirty = true;
    }
    UINT32_T__G__BoardParam[uint8_t__id] = uint32_t__value;
    BOOL__G__BoardParamKnown[uint8_t__id] = true;
    BOOL__G__BoardParamSeen = true;
}
static uint32_t UINT32_T__G__TimeAnchorSec = 0u;
static uint32_t UINT32_T__G__TimeAnchorMs = 0u;
static bool BOOL__G__TimeValid = false;
static bool BOOL__G__TimePushed = false;
static bool BOOL__G__BrowserTimePending = false;
static uint32_t UINT32_T__G__TimePushMs = 0u;
static uint32_t UINT32_T__G__TimePushArmedMs = 0u;
static bool BOOL__G__TimeFromDisk = false;
static bool BOOL__G__TimeBoardSeen = false;
static uint32_t UINT32_T__G__TimeBootMs = 0u;
/* A separate wall/uptime pair stays matched even when a save precedes HELLO. */
static uint32_t UINT32_T__G__TimePairWallSec = 0u;
static uint32_t UINT32_T__G__TimePairUptimeSec = 0u;
static bool BOOL__G__TimePairValid = false;
static bool BOOL__G__TimeBoardLive = false;
static uint32_t UINT32_T__G__TimeBoardNowSec = 0u;
static uint32_t UINT32_T__G__TimeBoardRxMs = 0u;
static bool BOOL__G__TimePairSaveWanted = false;
static uint32_t UINT32_T__G__TimeFwVersion = 0u;
static uint32_t UINT32_T__G__LogLastBeatSec = 0u;
static uint32_t UINT32_T__G__LastSavedTimeSec = 0u;
static int32_t INT32_T__G__LogTzSec = 0;
static int32_t INT32_T__G__LastSavedTzSec = 0;
static uint32_t UINT32_T__G__LastChartSaveSec = 0u;
/* ==================== Running clock ==================== */
/** @brief Return epoch seconds from the active ESP anchor, or zero if unknown. */
static uint32_t func__Simple_NowSec(void)
{
    uint32_t uint32_t__elapsedSec;
    if (!BOOL__G__TimeValid) return 0u;
    uint32_t__elapsedSec = ((uint32_t)millis() - UINT32_T__G__TimeAnchorMs) / 1000u;
    return UINT32_T__G__TimeAnchorSec + uint32_t__elapsedSec;
}
/* ==================== Set clock ==================== */
/**
 * @brief Set wall time without dropping the board pair, even before HELLO.
 * @param uint32_t__epochSec Unix seconds; bool__pushToBoard Request a board sync.
 */
static void func__Simple_TimeSet(uint32_t uint32_t__epochSec, bool bool__pushToBoard)
{
    if (uint32_t__epochSec < SIMPLE_TIME_VALID_MIN) return;
    if (BOOL__G__TimeBoardLive)
    {
        uint32_t uint32_t__elapsedSec = ((uint32_t)millis() - UINT32_T__G__TimeBoardRxMs) / 1000u;
        UINT32_T__G__TimePairUptimeSec = UINT32_T__G__TimeBoardNowSec + uint32_t__elapsedSec;
        BOOL__G__TimePairValid = true;
    }
    else if (BOOL__G__TimePairValid)
    {
        int64_t int64_t__advancedUptime = (int64_t)UINT32_T__G__TimePairUptimeSec;
        int64_t__advancedUptime += (int64_t)uint32_t__epochSec - (int64_t)UINT32_T__G__TimePairWallSec;
        BOOL__G__TimePairValid = (int64_t__advancedUptime > 0) && (int64_t__advancedUptime <= UINT32_MAX);
        if (BOOL__G__TimePairValid) UINT32_T__G__TimePairUptimeSec = (uint32_t)int64_t__advancedUptime;
    }
    UINT32_T__G__TimePairWallSec = uint32_t__epochSec;
    UINT32_T__G__TimeAnchorSec = uint32_t__epochSec;
    UINT32_T__G__TimeAnchorMs = (uint32_t)millis();
    BOOL__G__TimeValid = true;
    if (bool__pushToBoard) BOOL__G__TimePushed = false;
}
/* ==================== Boot clock gate ==================== */
/** @brief Wait for board time recovery, bounded by the boot grace interval. */
static bool func__Simple_TimeBootReady(void)
{
    if (!BOOL__G__TimeFromDisk || BOOL__G__TimeBoardSeen) return true;
    return ((uint32_t)((uint32_t)millis() - UINT32_T__G__TimeBootMs) >=
            (uint32_t)SIMPLE_TIME_BOOT_GRACE_MS);
}
/* ==================== Board epoch adoption ==================== */
/**
 * @brief Accept board epoch without rewinding or replacing unconfirmed phone time.
 * @param uint32_t__epochSec Board Unix seconds.
 * @return bool True for an accepted time or matching phone confirmation.
 */
static bool func__Simple_TimeAdoptFromBoard(uint32_t uint32_t__epochSec)
{
    if (uint32_t__epochSec < SIMPLE_TIME_VALID_MIN) return false;
    BOOL__G__TimeBoardSeen = true;
    if (BOOL__G__TimeValid)
    {
        uint32_t uint32_t__panelSec = func__Simple_NowSec();
        if (BOOL__G__BrowserTimePending)
        {
            uint32_t uint32_t__diff = (uint32_t__panelSec >= uint32_t__epochSec)
                ? uint32_t__panelSec - uint32_t__epochSec : uint32_t__epochSec - uint32_t__panelSec;
            if (uint32_t__diff > 2u) return false;
            BOOL__G__BrowserTimePending = false;
        }
        if (uint32_t__epochSec <= uint32_t__panelSec)
        {
            return ((uint32_t__panelSec - uint32_t__epochSec) < SIMPLE_TIME_ADOPT_DIFF_SEC);
        }
    }
    func__Simple_TimeSet(uint32_t__epochSec, false);
    return true;
}
/* ==================== Atomic clock replacement ==================== */
/** @brief Verify a staged clock file before replacing the committed file. @return Success. */
static bool func__Simple_TimeStore(void)
{
    char char__line[192];
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    if (!BOOL__G__FsOk) return false;
    int int__length = snprintf(char__line, sizeof(char__line),
        "{\"ts\":%lu,\"tz\":%ld,\"bu\":%lu,\"bw\":%lu,\"fw\":%lu,\"beat\":%lu}",
        (unsigned long)uint32_t__nowSec, (long)INT32_T__G__LogTzSec,
        (unsigned long)(BOOL__G__TimePairValid ? UINT32_T__G__TimePairUptimeSec : 0u),
        (unsigned long)(BOOL__G__TimePairValid ? UINT32_T__G__TimePairWallSec : 0u),
        (unsigned long)UINT32_T__G__TimeFwVersion, (unsigned long)UINT32_T__G__LogLastBeatSec);
    if ((int__length <= 0) || ((size_t)int__length >= sizeof(char__line))) return false;
    File file__out = LittleFS.open(SIMPLE_TIME_FILE ".tmp", "w");
    if (!file__out) return false;
    bool bool__ok = (file__out.print(char__line) == (size_t)int__length);
    file__out.close();
    if (bool__ok)
    {
        File file__check = LittleFS.open(SIMPLE_TIME_FILE ".tmp", "r");
        bool__ok = file__check && (file__check.readString() == String(char__line));
        file__check.close();
    }
    if (bool__ok) bool__ok = LittleFS.rename(SIMPLE_TIME_FILE ".tmp", SIMPLE_TIME_FILE);
    if (!bool__ok)
    {
        (void)LittleFS.remove(SIMPLE_TIME_FILE ".tmp");
        return false;
    }
    UINT32_T__G__LastSavedTimeSec = uint32_t__nowSec;
    INT32_T__G__LastSavedTzSec = INT32_T__G__LogTzSec;
    BOOL__G__TimePairSaveWanted = false;
    return true;
}
/* ==================== Throttled clock save ==================== */
/** @brief Save every five wall minutes; persist a newly acquired board pair immediately. */
static void func__Simple_TimeSave(void)
{
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    if (!BOOL__G__TimeValid || (uint32_t__nowSec < SIMPLE_TIME_VALID_MIN)) return;
    uint32_t uint32_t__diff = (uint32_t__nowSec >= UINT32_T__G__LastSavedTimeSec)
        ? uint32_t__nowSec - UINT32_T__G__LastSavedTimeSec
        : UINT32_T__G__LastSavedTimeSec - uint32_t__nowSec;
    if (!BOOL__G__TimePairSaveWanted && (UINT32_T__G__LastSavedTimeSec != 0u) &&
        (uint32_t__diff < SIMPLE_TIME_ADOPT_DIFF_SEC) &&
        (INT32_T__G__LogTzSec == INT32_T__G__LastSavedTzSec)) return;
    (void)func__Simple_TimeStore();
}
/* ==================== Recover battery sleep time ==================== */
/**
 * @brief Reconstruct elapsed sleep from a live STM32 uptime; re-anchor after its reset.
 * @param uint32_t__uptimeSec Board uptime in seconds, zero means unavailable.
 */
static void func__Simple_TimeNoteBoard(uint32_t uint32_t__uptimeSec)
{
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    bool bool__phoneBeforeBoard = BOOL__G__BrowserTimePending && !BOOL__G__TimeBoardLive;
    bool bool__newPair = !BOOL__G__TimePairValid || bool__phoneBeforeBoard;
    if (uint32_t__uptimeSec == 0u) return;
    bool bool__restarted = (BOOL__G__TimePairValid &&
        (uint32_t__uptimeSec < UINT32_T__G__TimePairUptimeSec)) ||
        (BOOL__G__TimeBoardLive && (uint32_t__uptimeSec < UINT32_T__G__TimeBoardNowSec));
    if (BOOL__G__TimePairValid && !bool__restarted && !bool__phoneBeforeBoard)
    {
        uint32_t uint32_t__elapsedSec = uint32_t__uptimeSec - UINT32_T__G__TimePairUptimeSec;
        uint32_t uint32_t__recoveredSec = UINT32_T__G__TimePairWallSec + uint32_t__elapsedSec;
        if (uint32_t__recoveredSec > uint32_t__nowSec) uint32_t__nowSec = uint32_t__recoveredSec;
    }
    UINT32_T__G__TimeBoardNowSec = uint32_t__uptimeSec;
    UINT32_T__G__TimeBoardRxMs = (uint32_t)millis();
    BOOL__G__TimeBoardLive = true;
    if (!BOOL__G__TimeValid) return;
    func__Simple_TimeSet(uint32_t__nowSec, false);
    BOOL__G__TimeBoardSeen = true;
    if (bool__newPair || bool__restarted) BOOL__G__TimePairSaveWanted = true;
    func__Simple_TimeSave();
}
/* ==================== Clock JSON number ==================== */
/**
 * @brief Read an unsigned clock field without signed Arduino toInt overflow.
 * @param string__content JSON record; char__ptr_key Quoted key plus colon.
 * @return uint32_t Parsed value, zero for missing or malformed fields.
 */
static uint32_t func__Simple_TimeField(const String &string__content, const char *char__ptr_key)
{
    int32_t int32_t__pos = string__content.indexOf(char__ptr_key);
    uint32_t uint32_t__value = 0u;
    bool bool__digits = false;
    if (int32_t__pos < 0) return 0u;
    int32_t__pos += (int32_t)strlen(char__ptr_key);
    while ((size_t)int32_t__pos < string__content.length())
    {
        char char__digit = string__content[(size_t)int32_t__pos++];
        if ((char__digit == ',') || (char__digit == '}')) return bool__digits ? uint32_t__value : 0u;
        if ((char__digit < '0') || (char__digit > '9')) return 0u;
        uint32_t uint32_t__digit = (uint32_t)(char__digit - '0');
        if (uint32_t__value > (UINT32_MAX - uint32_t__digit) / 10u) return 0u;
        uint32_t__value = (uint32_t__value * 10u) + uint32_t__digit;
        bool__digits = true;
    }
    return 0u;
}
/* ==================== Restore clock ==================== */
/** @brief Restore only the committed clock; defer logs until the board answers or grace expires. */
static void func__Simple_TimeLoad(void)
{
    if (!BOOL__G__FsOk) return;
    File file__clock = LittleFS.open(SIMPLE_TIME_FILE, "r");
    if (!file__clock) return;
    String string__content = file__clock.readString();
    file__clock.close();
    UINT32_T__G__TimeFwVersion = func__Simple_TimeField(string__content, "\"fw\":");
    UINT32_T__G__LogLastBeatSec = func__Simple_TimeField(string__content, "\"beat\":");
    uint32_t uint32_t__savedSec = func__Simple_TimeField(string__content, "\"ts\":");
    int32_t int32_t__tzPos = string__content.indexOf("\"tz\":");
    if (int32_t__tzPos >= 0)
    {
        int32_t int32_t__tz = (int32_t)string__content.substring(int32_t__tzPos + 5).toInt();
        if ((int32_t__tz >= -50400) && (int32_t__tz <= 50400)) INT32_T__G__LogTzSec = int32_t__tz;
    }
    INT32_T__G__LastSavedTzSec = INT32_T__G__LogTzSec;
    UINT32_T__G__LastSavedTimeSec = uint32_t__savedSec;
    BOOL__G__TimeBoardLive = false;
    BOOL__G__TimePairValid = false;
    func__Simple_TimeSet(uint32_t__savedSec, false);
    UINT32_T__G__TimePairUptimeSec = func__Simple_TimeField(string__content, "\"bu\":");
    UINT32_T__G__TimePairWallSec = func__Simple_TimeField(string__content, "\"bw\":");
    BOOL__G__TimePairValid = (UINT32_T__G__TimePairUptimeSec != 0u) &&
                           (UINT32_T__G__TimePairWallSec >= SIMPLE_TIME_VALID_MIN);
    BOOL__G__TimePairSaveWanted = false;
    BOOL__G__TimeFromDisk = (uint32_t__savedSec >= SIMPLE_TIME_VALID_MIN);
    BOOL__G__TimeBoardSeen = false;
    UINT32_T__G__TimeBootMs = (uint32_t)millis();
}
static bool BOOL__G__ChartsDirty = false;
static bool BOOL__G__ChargeHistDirty = false;
static bool BOOL__G__DiagDirty = false;
/* ==================== STM32 ==================== */
static uint32_t UINT32_T__G__LastLocalFaultMask = 0u;
static bool BOOL__G__LastLocalInputPresent = false;
static bool BOOL__G__LastLocalInputValid = false;
static uint32_t UINT32_T__G__LocalPowerOffTime = 0u;
static bool BOOL__G__LocalPowerOffActive = false;
static char CHAR__G__WifiSsid[SIMPLE_WIFI_SSID_MAX + 1] = SIMPLE_WIFI_AP_SSID;
static char CHAR__G__WifiPass[SIMPLE_WIFI_PASS_MAX + 1] = SIMPLE_WIFI_AP_PASS;
static bool BOOL__G__WifiRebootPending = false;
static uint32_t UINT32_T__G__WifiRebootMs = 0u;
/* ==================== prev ==================== */
static uint32_t UINT32_T__G__PrevChargeCountCh1 = 0u;
static uint32_t UINT32_T__G__PrevChargeCountCh2 = 0u;
static bool BOOL__G__ChargeCountBaselineReady = false;
static bool BOOL__G__DiagClearPending = false;
typedef struct
{
    char name[SIMPLE_USER_NAME_MAX + 1];
    char passHash[65];
    uint8_t role; /* 0=viewer, 1=admin */
    bool active;
} simple_user_t;
static simple_user_t SIMPLE_USER_T__G__Users[SIMPLE_USERS_MAX];
static uint8_t UINT8_T__G__UserCount = 0u;
static char CHAR__G__CurrentUser[SIMPLE_USER_NAME_MAX + 1] = {0};
static uint8_t UINT8_T__G__CurrentRole = 0u;
static uint32_t UINT32_T__G__SessionStartMs = 0u;
static char CHAR__G__SessionToken[SIMPLE_SESSION_TOKEN_LEN + 1u] = {0};
static uint8_t UINT8_T__G__Theme = 0u;
static uint8_t UINT8_T__G__CryptoKey[SIMPLE_CRYPTO_KEY_LEN];
static bool BOOL__G__CryptoKeyReady = false;
static uint32_t UINT32_T__G__CryptoNonce = 0u;
static esp_web_server_t SIMPLE_WEB_SERVER_T__G__Server(SIMPLE_HTTP_PORT);
static WiFiUDP WIFI_UDP_T__G__DnsUdp;
static bool BOOL__G__DnsRunning = false;
static char CHAR__G__JsonBuffer[SIMPLE_JSON_BUFFER_SIZE];
static uint32_t UINT32_T__G__LastBrowserPollMs = 0u;
static bool BOOL__G__BrowserSeen = false;
static bool BOOL__G__WifiApActive = true;
static bool BOOL__G__WifiHighPower = true;
static bool BOOL__G__WifiClientConnected = false;
static bool BOOL__G__WifiLastReportedConnected = false;
static uint32_t UINT32_T__G__WifiLastReportMs = 0u;
static uint32_t UINT32_T__G__WifiBatteryIdleStartMs = 0u;
static bool BOOL__G__WifiBatteryIdleTracking = false;
static bool BOOL__G__WifiBatCutoffLatched = false;
static uint32_t UINT32_T__G__WifiStationSinceMs = 0u;
