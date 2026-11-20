/**
 * @file simple_pack_http.h
 * @brief Authenticated ESP-only pack maintenance routes.
 */
#pragma once
#include "pack_review_panel.h"
/* ==================== Browser asset ==================== */
/** @brief Stream the shared maintenance UI from flash in bounded chunks. */
static void func__Simple_HttpPackScript(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.setContentLength(sizeof(SIMPLE_PACK_REVIEW_JS) - 1u);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/javascript; charset=utf-8", "");
    for (size_t offset = 0u; offset < sizeof(SIMPLE_PACK_REVIEW_JS) - 1u; offset += 1024u)
    {
        size_t remaining = sizeof(SIMPLE_PACK_REVIEW_JS) - 1u - offset;
        SIMPLE_WEB_SERVER_T__G__Server.sendContent_P(SIMPLE_PACK_REVIEW_JS + offset, remaining < 1024u ? remaining : 1024u);
        func__Simple_DrainSerial();
        yield();
    }
}
/* ==================== Public read-only status ==================== */
/** @brief Return server-derived work and reminder status without editable settings. */
static void func__Simple_HttpPack(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", func__Simple_PackPublic());
}
/* ==================== Administrator gate ==================== */
/** @brief Reject unauthenticated maintenance operations before reading or writing settings. */
static bool func__Simple_PackAdmin(void)
{
    SIMPLE_WEB_SERVER_T__G__Server.sendHeader("Cache-Control", "no-store");
    if (func__Simple_IsAdmin()) return true;
    SIMPLE_WEB_SERVER_T__G__Server.send(403, "application/json", "{\"ok\":0,\"e\":\"admin\"}");
    return false;
}
/* ==================== Saved administrator view ==================== */
/** @brief Return persisted maintenance values and a revision for duplicate/stale-request rejection. */
static void func__Simple_HttpPackSettings(void)
{
    if (!func__Simple_PackAdmin()) return;
    char json[256];
    (void)snprintf(json, sizeof(json), "{\"ok\":1,\"invalid\":%u,\"limit\":%lu,\"at\":%lu,\"revision\":%lu}",
        BOOL__G__PackReviewInvalid ? 1u : 0u, (unsigned long)SIMPLE_PACK__G__Review.limit,
        (unsigned long)SIMPLE_PACK__G__Review.replacedAt, (unsigned long)SIMPLE_PACK__G__Review.revision);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", json);
}
/* ==================== Revision gate ==================== */
/** @brief Accept only an exact canonical current revision; reject retries after an already committed mutation. */
static bool func__Simple_PackRevision(void)
{
    String revision;
    if (func__Simple_GetArg("revision", revision) && revision == String((unsigned long)SIMPLE_PACK__G__Review.revision) &&
        SIMPLE_PACK__G__Review.revision < UINT32_MAX) return true;
    SIMPLE_WEB_SERVER_T__G__Server.send(409, "application/json", "{\"ok\":0,\"e\":\"revision\"}");
    return false;
}
/* ==================== Threshold save ==================== */
/** @brief Save only the requested bounded limit; preserve replacement date and independent baselines. */
static void func__Simple_HttpPackSet(void)
{
    if (!func__Simple_PackAdmin() || !func__Simple_PackRevision()) return;
    String text;
    int32_t limit = 0;
    if (!func__Simple_GetArg("limit", text) || !func__Simple_ParseInt(text.c_str(), &limit) ||
        limit < 0 || limit > 100000 || text != String((unsigned long)limit))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"limit\"}");
        return;
    }
    if (BOOL__G__PackReviewInvalid)
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(409, "application/json", "{\"ok\":0,\"e\":\"record\"}");
        return;
    }
    SimplePackReview next = SIMPLE_PACK__G__Review;
    if (next.limit != (uint32_t)limit || !LittleFS.exists(SIMPLE_PACK_REVIEW_FILE))
    {
        next.limit = (uint32_t)limit;
        next.revision++;
    }
    if (!func__Simple_PackSave(next)) SIMPLE_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0,\"e\":\"storage\"}");
    else func__Simple_HttpPackSettings();
}
/* ==================== Replacement save ==================== */
/** @brief Snapshot fresh per-channel counters and trusted ESP time after explicit confirmation; never command STM32. */
static void func__Simple_HttpPackReplace(void)
{
    if (!func__Simple_PackAdmin() || !func__Simple_PackRevision()) return;
    String confirm;
    if (!func__Simple_GetArg("confirm", confirm) || confirm != "REPLACE_PACK")
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(400, "application/json", "{\"ok\":0,\"e\":\"confirm\"}");
        return;
    }
    if (!func__Simple_PackFresh() || !func__Simple_TimeBootReady() || func__Simple_NowSec() < SIMPLE_TIME_VALID_MIN ||
        (SIMPLE_PACK__G__Review.replacedAt && func__Simple_NowSec() < SIMPLE_PACK__G__Review.replacedAt))
    {
        SIMPLE_WEB_SERVER_T__G__Server.send(409, "application/json", "{\"ok\":0,\"e\":\"data\"}");
        return;
    }
    SimplePackReview next = SIMPLE_PACK__G__Review;
    next.upper = UINT32_T__G__ChargeCountCh1;
    next.lower = UINT32_T__G__ChargeCountCh2;
    next.replacedAt = func__Simple_NowSec();
    next.revision++;
    if (!func__Simple_PackSave(next))
        SIMPLE_WEB_SERVER_T__G__Server.send(503, "application/json", "{\"ok\":0,\"e\":\"storage\"}");
    else func__Simple_HttpPackSettings();
}
