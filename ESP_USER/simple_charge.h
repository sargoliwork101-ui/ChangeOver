/**
 * @file simple_charge.h
 * @brief ESP-observed charge sessions; no inferred time across missing telemetry.
 */
#pragma once
static bool BOOL__G__ChargeSeen[2] = {false, false};
static bool BOOL__G__ChargePumping[2] = {false, false};
static uint64_t UINT64_T__G__ChargeElapsedMs[2] = {0u, 0u};
static uint32_t UINT32_T__G__ChargeFrameMs = 0u;
static uint32_t UINT32_T__G__ChargeFrameSec = 0u;
static bool BOOL__G__ChargeFrameSeen = false;
/* ==================== Session lookup ==================== */
/** @brief Find a live session for a zero-based channel; return -1 if absent. */
static int func__Simple_ChargeFind(uint8_t uint8_t__channel)
{
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChargeHistoryCount; uint8_t__i++)
    {
        const simple_charge_event_t &event = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i];
        if (event.channel == uint8_t__channel + 1u && event.status == 3u) return uint8_t__i;
    }
    return -1;
}
/* ==================== Session reset ==================== */
/** @brief Forget observation baselines after erase, link loss, or a board restart. */
static void func__Simple_ChargeReset(void)
{
    BOOL__G__ChargeFrameSeen = false;
    for (uint8_t uint8_t__ch = 0u; uint8_t__ch < 2u; uint8_t__ch++)
    {
        BOOL__G__ChargeSeen[uint8_t__ch] = false;
        BOOL__G__ChargePumping[uint8_t__ch] = false;
        UINT64_T__G__ChargeElapsedMs[uint8_t__ch] = 0u;
    }
}
/* ==================== Incomplete observation ==================== */
/** @brief Seal live sessions as unknown, keeping only their last observed timestamp. */
static void func__Simple_ChargeGap(void)
{
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChargeHistoryCount; uint8_t__i++)
    {
        simple_charge_event_t &event = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i];
        if (event.status == 3u)
        {
            event.status = 0u;
            event.durationSec = 0u;
            BOOL__G__ChargeHistDirty = true;
        }
    }
    func__Simple_ChargeReset();
}
/* ==================== Bounded session removal ==================== */
/** @brief Remove one history slot, preserving the order of all newer entries. */
static void func__Simple_ChargeRemove(uint8_t uint8_t__slot)
{
    for (uint8_t uint8_t__i = uint8_t__slot + 1u; uint8_t__i < UINT8_T__G__ChargeHistoryCount; uint8_t__i++)
        SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i - 1u] = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i];
    UINT8_T__G__ChargeHistoryCount--;
    BOOL__G__ChargeHistDirty = true;
}
/* ==================== Session retention ==================== */
/** @brief Keep today and 29 prior local dates only after the clock is confirmed. */
static void func__Simple_ChargePrune(uint32_t uint32_t__now)
{
    if (!func__Simple_TimeBootReady() || uint32_t__now < SIMPLE_TIME_VALID_MIN) return;
    int64_t int64_t__local = (int64_t)uint32_t__now + INT32_T__G__LogTzSec;
    int64_t int64_t__day = int64_t__local / 86400;
    int64_t int64_t__cutoff = (int64_t__day - 29) * 86400 - INT32_T__G__LogTzSec;
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__ChargeHistoryCount;)
    {
        if ((int64_t)SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__i].timestamp < int64_t__cutoff)
            func__Simple_ChargeRemove(uint8_t__i);
        else uint8_t__i++;
    }
}
/* ==================== Session allocation ==================== */
/** @brief Append a session, evicting only the oldest finished slot at capacity. */
static simple_charge_event_t &func__Simple_ChargeNew(uint8_t uint8_t__channel, uint32_t uint32_t__now, bool bool__startKnown)
{
    if (UINT8_T__G__ChargeHistoryCount == SIMPLE_DIAG_MAX_POWER_EVENTS)
    {
        uint8_t uint8_t__oldest = 0u;
        while (uint8_t__oldest + 1u < UINT8_T__G__ChargeHistoryCount &&
               SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[uint8_t__oldest].status == 3u) uint8_t__oldest++;
        func__Simple_ChargeRemove(uint8_t__oldest);
    }
    simple_charge_event_t &event = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[UINT8_T__G__ChargeHistoryCount++];
    event = {uint32_t__now, (uint8_t)(uint8_t__channel + 1u), 0u, bool__startKnown ? uint32_t__now : 0u, 3u};
    UINT64_T__G__ChargeElapsedMs[uint8_t__channel] = 0u;
    BOOL__G__ChargeHistDirty = true;
    return event;
}
/* ==================== Fresh telemetry observation ==================== */
/** @brief Observe channel states 1/2 as pumping and 3 as complete; preserve retry pauses. */
static void func__Simple_ChargeOnFrame(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();
    uint32_t uint32_t__nowSec = func__Simple_NowSec();
    uint32_t uint32_t__deltaMs = uint32_t__nowMs - UINT32_T__G__ChargeFrameMs;
    bool bool__clock = func__Simple_TimeBootReady() && uint32_t__nowSec >= SIMPLE_TIME_VALID_MIN;
    bool bool__gap = BOOL__G__ChargeFrameSeen && uint32_t__deltaMs > SIMPLE_LINK_TIMEOUT_MS;
    if (BOOL__G__ChargeFrameSeen && bool__clock)
    {
        int64_t int64_t__wall = (int64_t)uint32_t__nowSec - UINT32_T__G__ChargeFrameSec;
        int64_t int64_t__elapsed = uint32_t__deltaMs / 1000u;
        if (int64_t__wall < 0 || int64_t__wall > int64_t__elapsed + 2) bool__gap = true;
    }
    if (bool__gap || !bool__clock) func__Simple_ChargeGap();
    if (!bool__clock) return;
    func__Simple_ChargePrune(uint32_t__nowSec);
    for (uint8_t uint8_t__ch = 0u; uint8_t__ch < 2u; uint8_t__ch++)
    {
        uint32_t uint32_t__state = UINT32_T__G__TlmField[uint8_t__ch == 0u ? 6u : 13u];
        bool bool__pumping = uint32_t__state == 1u || uint32_t__state == 2u;
        bool bool__pause = uint32_t__state == 4u || uint32_t__state == 5u || uint32_t__state == 10u;
        int int__slot = func__Simple_ChargeFind(uint8_t__ch);
        if (int__slot < 0 && bool__pumping)
        {
            bool bool__known = BOOL__G__ChargeSeen[uint8_t__ch] && !BOOL__G__ChargePumping[uint8_t__ch];
            (void)func__Simple_ChargeNew(uint8_t__ch, uint32_t__nowSec, bool__known);
            int__slot = func__Simple_ChargeFind(uint8_t__ch);
        }
        if (int__slot >= 0)
        {
            simple_charge_event_t &event = SIMPLE_CHARGE_EVENT_T__G__ChargeHistory[int__slot];
            if (BOOL__G__ChargeFrameSeen && BOOL__G__ChargePumping[uint8_t__ch])
                UINT64_T__G__ChargeElapsedMs[uint8_t__ch] += uint32_t__deltaMs;
            uint32_t uint32_t__duration = event.startSec != 0u ? (uint32_t)(UINT64_T__G__ChargeElapsedMs[uint8_t__ch] / 1000u) : 0u;
            if (event.timestamp != uint32_t__nowSec || event.durationSec != uint32_t__duration) BOOL__G__ChargeHistDirty = true;
            event.timestamp = uint32_t__nowSec;
            event.durationSec = uint32_t__duration;
            if (!bool__pumping && !bool__pause)
            {
                event.status = uint32_t__state == 3u ? 1u : uint32_t__state <= 9u ? 2u : 0u;
                if (event.status == 0u) event.durationSec = 0u;
                BOOL__G__ChargeHistDirty = true;
            }
        }
        if (bool__pumping) BOOL__G__ChargeSeen[uint8_t__ch] = false;
        else if (!bool__pause) BOOL__G__ChargeSeen[uint8_t__ch] = uint32_t__state <= 9u;
        BOOL__G__ChargePumping[uint8_t__ch] = bool__pumping;
    }
    UINT32_T__G__ChargeFrameMs = uint32_t__nowMs;
    UINT32_T__G__ChargeFrameSec = uint32_t__nowSec;
    BOOL__G__ChargeFrameSeen = true;
}
/* ==================== Missing telemetry watchdog ==================== */
/** @brief Stop claiming a live session when UART is stale; prune confirmed old dates. */
static void func__Simple_ChargeTick(void)
{
    if (BOOL__G__ChargeFrameSeen && (uint32_t)((uint32_t)millis() - UINT32_T__G__ChargeFrameMs) > SIMPLE_LINK_TIMEOUT_MS)
        func__Simple_ChargeGap();
    func__Simple_ChargePrune(func__Simple_NowSec());
}
