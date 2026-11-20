/**
 * @file simple_pack_review.h
 * @brief Persistent ESP-only pack maintenance settings and replacement baselines.
 */
#pragma once
struct SimplePackReview
{
    uint32_t limit;
    uint32_t upper;
    uint32_t lower;
    uint32_t replacedAt;
    uint32_t revision;
};
static SimplePackReview SIMPLE_PACK__G__Review = {0u, 0u, 0u, 0u, 0u};
static bool BOOL__G__PackReviewInvalid = false;
/* ==================== Record encoding ==================== */
/** @brief Encode bounded canonical text with CRC16; input fields are counts and Unix seconds. */
static String func__Simple_PackEncode(const SimplePackReview &record)
{
    char text[128];
    int length = snprintf(text, sizeof(text), "PR1,%lu,%lu,%lu,%lu,%lu,",
        (unsigned long)record.limit, (unsigned long)record.upper, (unsigned long)record.lower,
        (unsigned long)record.replacedAt, (unsigned long)record.revision);
    if (length <= 0 || (size_t)length >= sizeof(text)) return String("");
    uint16_t crc = 0xFFFFu;
    for (int i = 0; i < length; i++) crc = func__Simple_Crc16(crc, (uint8_t)text[i]);
    (void)snprintf(text + length, sizeof(text) - (size_t)length, "%u", (unsigned int)crc);
    return String(text);
}
/* ==================== Record decoding ==================== */
/** @brief Decode exact canonical uint32 fields and checksum; reject overflow, truncation and trailing bytes. */
static bool func__Simple_PackDecode(const String &raw, SimplePackReview &record)
{
    if (strncmp(raw.c_str(), "PR1,", 4u) != 0 || raw.length() > 127u) return false;
    uint32_t fields[6] = {};
    size_t pos = 4u;
    for (uint8_t field = 0u; field < 6u; field++)
    {
        size_t start = pos;
        while (pos < raw.length() && raw[(unsigned int)pos] >= '0' && raw[(unsigned int)pos] <= '9')
        {
            uint32_t digit = (uint32_t)(raw[(unsigned int)pos] - '0');
            if (fields[field] > (UINT32_MAX - digit) / 10u) return false;
            fields[field] = fields[field] * 10u + digit;
            pos++;
        }
        if (pos == start || (field < 5u && raw[(unsigned int)pos++] != ',')) return false;
    }
    if (pos != raw.length() || fields[0] > 100000u || fields[5] > 65535u ||
        (fields[3] && fields[3] < SIMPLE_TIME_VALID_MIN) || (!fields[3] && (fields[1] || fields[2]))) return false;
    record = {fields[0], fields[1], fields[2], fields[3], fields[4]};
    return func__Simple_PackEncode(record) == raw;
}
/* ==================== Boot load ==================== */
/** @brief Restore only the committed bounded record; missing settings disable the reminder, corruption stays unknown. */
static void func__Simple_PackLoad(void)
{
    SIMPLE_PACK__G__Review = {0u, 0u, 0u, 0u, 0u};
    BOOL__G__PackReviewInvalid = !BOOL__G__FsOk;
    if (!BOOL__G__FsOk || !LittleFS.exists(SIMPLE_PACK_REVIEW_FILE)) return;
    File file = LittleFS.open(SIMPLE_PACK_REVIEW_FILE, "r");
    BOOL__G__PackReviewInvalid = true;
    if (!file || file.size() > 127u) { file.close(); return; }
    String raw = file.readString();
    file.close();
    SimplePackReview record;
    if (!func__Simple_PackDecode(raw, record)) return;
    SIMPLE_PACK__G__Review = record;
    BOOL__G__PackReviewInvalid = false;
}
/* ==================== Transactional save ==================== */
/** @brief Commit verified staging bytes before changing active RAM; return false without changing prior state. */
static bool func__Simple_PackSave(const SimplePackReview &record)
{
    String raw = func__Simple_PackEncode(record);
    SimplePackReview checked;
    if (!BOOL__G__FsOk || !func__Simple_PackDecode(raw, checked)) return false;
    File saved = LittleFS.open(SIMPLE_PACK_REVIEW_FILE, "r");
    bool identical = saved && saved.size() == raw.length() && saved.readString() == raw;
    saved.close();
    if (!identical && !func__Simple_StorageWrite(SIMPLE_PACK_REVIEW_FILE, raw)) return false;
    SIMPLE_PACK__G__Review = record;
    BOOL__G__PackReviewInvalid = false;
    return true;
}
/* ==================== Fresh counter gate ==================== */
/** @brief Require real extended diagnostics and recent live telemetry from the same board boot. */
static bool func__Simple_PackFresh(void)
{
    uint32_t now = (uint32_t)millis();
    return BOOL__G__PackCountersSeen && BOOL__G__TlmSeen && BOOL__G__DiagValid &&
        ((uint32_t)(now - UINT32_T__G__LastTlmMs) <= SIMPLE_LINK_TIMEOUT_MS) &&
        ((uint32_t)(now - UINT32_T__G__PackCountersMs) <= 20000u) &&
        UINT32_T__G__PackCounterReset == UINT32_T__G__ResetCount;
}
/* ==================== Public maintenance status ==================== */
/** @brief Build read-only count/status JSON without exposing administrator settings or inventing unknown work. */
static String func__Simple_PackPublic(void)
{
    bool fresh = func__Simple_PackFresh();
    bool invalid = !BOOL__G__FsOk || BOOL__G__PackReviewInvalid || (SIMPLE_PACK__G__Review.replacedAt &&
        (UINT32_T__G__ChargeCountCh1 < SIMPLE_PACK__G__Review.upper || UINT32_T__G__ChargeCountCh2 < SIMPLE_PACK__G__Review.lower));
    bool valid = fresh && !invalid;
    uint32_t count = 0u, progress = 0u;
    uint8_t state = !fresh ? 0u : invalid ? 1u : !SIMPLE_PACK__G__Review.limit ? 2u : 3u;
    if (valid)
    {
        uint32_t upper = UINT32_T__G__ChargeCountCh1 - SIMPLE_PACK__G__Review.upper;
        uint32_t lower = UINT32_T__G__ChargeCountCh2 - SIMPLE_PACK__G__Review.lower;
        count = upper > lower ? upper : lower;
        if (SIMPLE_PACK__G__Review.limit)
        {
            uint64_t scaled = (uint64_t)count * 1000u;
            uint64_t ratio = scaled / SIMPLE_PACK__G__Review.limit;
            progress = ratio > 1000u ? 1000u : (uint32_t)ratio;
            state = progress >= 1000u ? 5u : progress >= 800u ? 4u : 3u;
        }
    }
    char json[160];
    (void)snprintf(json, sizeof(json), "{\"valid\":%u,\"state\":%u,\"count\":%lu,\"progress\":%lu,\"replaced\":%u}",
        valid ? 1u : 0u, (unsigned int)state, (unsigned long)count, (unsigned long)progress,
        SIMPLE_PACK__G__Review.replacedAt ? 1u : 0u);
    return String(json);
}
