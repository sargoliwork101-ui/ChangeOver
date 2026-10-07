/**
 * @file    up_link.h
 * @brief   [EN] Read-only client for the engineering panel's HTTP API. This
 *              panel joins the ChangeOver-ESP network as a station and polls
 *              `GET /t` (live telemetry) and `GET /m` (the bench window's
 *              min/max) - and nothing else. It never POSTs `/s`, so the user
 *              panel cannot alter the board by accident.
 *          [FA] کلاینت فقط-خواندنی سرویس HTTP پنل مهندسی. این پنل به‌عنوان
 *              کلاینت به شبکهٔ ChangeOver-ESP می‌رود و `GET /t` (تلمتری زنده)
 *              و `GET /m` (کمینه/بیشینهٔ پنجرهٔ بنچ) را می‌خواند - و بس.
 *              هیچ‌وقت `/s` را POST نمی‌کند، پس پنل کاربر نمی‌تواند تصادفی برد
 *              را عوض کند.
 *
 * @note    [EN] The only write this whole firmware can send is the two charger
 *              enable parameters (11/12) from the ADMIN action handler, and it
 *              goes through `func__UpLink_QueueParam` - one deliberate door.
 *          [FA] تنها نوشتنی‌ای که کل این فرم‌ور می‌تواند بفرستد دو پارامتر
 *              فعال/غیرفعال شارژر (۱۱/۱۲) از هندلر اقدام مدیر است که از
 *              `func__UpLink_QueueParam` می‌گذرد - یک درِ عمدی.
 */

#ifndef UP_LINK_H
#define UP_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ESP8266WiFi.h>
#include "up_config.h"
#include "up_state.h"

/* ==================== Constants / ثابت‌ها ==================== */
#define UP_LINK_JOB_TELEMETRY 0u   /* [EN] GET /t / [FA] خواندن /t */
#define UP_LINK_JOB_PEAKS     1u   /* [EN] GET /m / [FA] خواندن /m */

#define UP_LINK_ST_IDLE       0u
#define UP_LINK_ST_CONNECTING 1u
#define UP_LINK_ST_SENDING    2u
#define UP_LINK_ST_HEADERS    3u
#define UP_LINK_ST_BODY       4u

#define UP_LINK_READ_CHUNK    256u
#define UP_LINK_PEAK_FIELDS   32u

/* ==================== State / وضعیت ==================== */
static WiFiClient UP_WIFICLIENT_T__G__Client;
static char CHAR__G__UpResponse[UP_JSON_BUF];
static uint32_t UINT32_T__G__UpResponseLen = 0u;
static uint8_t UINT8_T__G__UpState = UP_LINK_ST_IDLE;
static uint8_t UINT8_T__G__UpJob = UP_LINK_JOB_TELEMETRY;
static uint32_t UINT32_T__G__UpJobStartedMs = 0u;
static uint32_t UINT32_T__G__UpEndsAtMs = 0u;
static bool BOOL__G__UpHeaderDone = false;
static uint16_t UINT16_T__G__UpContentLength = 0u;
static uint32_t UINT32_T__G__UpNextTelemetryMs = 0u;
static uint32_t UINT32_T__G__UpNextPeaksMs = 0u;
static uint32_t UINT32_T__G__UpRetryDelayMs = UP_RETRY_MIN_MS;
static bool BOOL__G__UpWifiStarted = false;
static bool BOOL__G__UpParamPending[UP_PARAM_COUNT];

/* ==================== Error bookkeeping / ثبت خطا ==================== */
/**
 * @brief  [EN] Remember that a read failed, with the time, so the diagnostics
 *              page can distinguish "noise right now" from "a counter that grew
 *              once, last week".
 *         [FA] به‌خاطر سپردن شکست یک خواندن همراه با زمانش، تا صفحهٔ دیاگ بتواند
 *              «نویز همین حالا» را از «شمارنده‌ای که هفتهٔ پیش یک‌بار بالا رفت»
 *              جدا کند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpLink_NoteError(void)
{
    UPPANEL_STATE_T__G__State.uint32_t__tlmErrors++;
    UPPANEL_STATE_T__G__State.uint32_t__lastErrorMs = (uint32_t)millis();
}
static int32_t INT32_T__G__UpParamValue[UP_PARAM_COUNT];

/* ==================== Tiny JSON readers / خواننده‌های کوچک JSON ==================== */
/**
 * @brief  [EN] Pointer to the first character after `"key":` in a flat JSON
 *              object, or NULL when the key is absent.
 *         [FA] اشاره‌گر به اولین نویسه بعد از `"key":` در یک شیء JSON تخت، یا
 *              NULL اگر کلید نباشد.
 * @param  char__json [EN] NUL-terminated body / [FA] بدنهٔ پایان‌یافته با NUL
 * @param  char__key [EN] key name without quotes / [FA] نام کلید بدون کوتیشن
 * @return [EN] pointer or NULL / [FA] اشاره‌گر یا NULL
 */
static const char *func__UpLink_FindKey(const char *char__json, const char *char__key)
{
    char char__needle[24];
    const char *char__at;

    snprintf(char__needle, sizeof(char__needle), "\"%s\":", char__key);
    char__at = strstr(char__json, char__needle);
    if (char__at == NULL)
    {
        return NULL;
    }

    return (char__at + strlen(char__needle));
}

/**
 * @brief  [EN] Parse the integer at a position, honouring a leading minus and
 *              refusing anything that is not `null` or a number.
 *         [FA] خواندن عدد صحیح از یک موقعیت، با پشتیبانی از منهای ابتدایی و
 *              رد کردن هر چیزی جز `null` یا عدد.
 * @param  char__at [EN] text position / [FA] موقعیت متن
 * @param  int32_t__out [EN] parsed value / [FA] مقدار خوانده‌شده
 * @param  uint32_t__consumeOut [EN] characters consumed / [FA] نویسه‌های مصرف‌شده
 * @return [EN] true when a number was parsed / [FA] در صورت خواندن عدد true
 */
static bool func__UpLink_ParseInt(const char *char__at, int32_t *int32_t__out, uint32_t *uint32_t__consumeOut)
{
    bool bool__negative = false;
    bool bool__any = false;
    uint32_t uint32_t__index = 0u;
    int32_t int32_t__value = 0;

    if ((char__at == NULL) || (char__at[0] == '\0'))
    {
        return false;
    }

    if (strncmp(char__at, "null", 4u) == 0)
    {
        *uint32_t__consumeOut = 4u;
        return false;
    }

    if (char__at[0] == '-')
    {
        bool__negative = true;
        uint32_t__index = 1u;
    }

    while ((char__at[uint32_t__index] >= '0') && (char__at[uint32_t__index] <= '9'))
    {
        int32_t int32_t__digit = (int32_t)(char__at[uint32_t__index] - '0');
        int32_t__value = (int32_t__value * 10) + int32_t__digit;
        uint32_t__index++;
        bool__any = true;
        if (uint32_t__index > 12u)
        {
            break;
        }
    }

    if (!bool__any)
    {
        return false;
    }

    *int32_t__out = bool__negative ? -int32_t__value : int32_t__value;
    *uint32_t__consumeOut = uint32_t__index;
    return true;
}

/**
 * @brief  [EN] Parse an array of numbers into a caller buffer. `null` entries
 *              are written as 0 and reported through `bool__validOut`.
 *         [FA] خواندن آرایهٔ اعداد در بافر فراخوان. عضوهای `null` صفر نوشته
 *              می‌شوند و از راه `bool__validOut` گزارش می‌شوند.
 * @param  char__json [EN] body / [FA] بدنه
 * @param  char__key [EN] array key / [FA] کلید آرایه
 * @param  uint32_t__out [EN] destination array / [FA] آرایهٔ مقصد
 * @param  uint8_t__max [EN] capacity / [FA] ظرفیت
 * @param  bool__validOut [EN] per-element validity, may be NULL / [FA] معتبری هر عضو، می‌تواند NULL باشد
 * @return [EN] number of elements seen / [FA] تعداد عضوهای دیده‌شده
 */
static uint8_t func__UpLink_ParseArray(const char *char__json, const char *char__key,
                                       uint32_t *uint32_t__out, uint8_t uint8_t__max, bool *bool__validOut)
{
    const char *char__at = func__UpLink_FindKey(char__json, char__key);
    uint8_t uint8_t__count = 0u;

    if ((char__at == NULL) || (char__at[0] != '['))
    {
        return 0u;
    }
    char__at++;

    while ((uint8_t__count < uint8_t__max) && (char__at[0] != '\0') && (char__at[0] != ']'))
    {
        int32_t int32_t__value = 0;
        uint32_t uint32_t__used = 0u;

        if (char__at[0] == ',')
        {
            char__at++;
            continue;
        }

        bool bool__ok = func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used);
        uint32_t__out[uint8_t__count] = bool__ok ? (uint32_t)int32_t__value : 0u;
        if (bool__validOut != NULL)
        {
            bool__validOut[uint8_t__count] = bool__ok;
        }
        uint8_t__count++;

        if (uint32_t__used == 0u)
        {
            break;
        }
        char__at += uint32_t__used;
    }

    return uint8_t__count;
}

/* ==================== Body application / اعمال بدنه ==================== */
/**
 * @brief  [EN] Apply a `/t` body to the live model.
 *         [FA] اعمال بدنهٔ `/t` روی مدل زنده.
 * @param  char__body [EN] JSON text / [FA] متن JSON
 * @return [EN] true when the essentials were present / [FA] اگر پایه‌ها بودند true
 */
static bool func__UpLink_ApplyTelemetry(const char *char__body)
{
    up_telemetry_t *up_telemetry_t__tlm = &UPPANEL_STATE_T__G__State.up_telemetry_t__tlm;
    const char *char__at;
    uint32_t uint32_t__used = 0u;
    int32_t int32_t__value = 0;

    char__at = func__UpLink_FindKey(char__body, "t");
    if ((char__at == NULL) || (char__at[0] != '['))
    {
        return false;
    }

    uint8_t uint8_t__fields = func__UpLink_ParseArray(char__body, "t", up_telemetry_t__tlm->uint32_t__t, UP_TLM_MAX_FIELDS, NULL);
    if (uint8_t__fields == 0u)
    {
        return false;
    }
    up_telemetry_t__tlm->uint8_t__tCount = uint8_t__fields;

    char__at = func__UpLink_FindKey(char__body, "seq");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        up_telemetry_t__tlm->uint16_t__seq = (uint16_t)int32_t__value;
    }

    char__at = func__UpLink_FindKey(char__body, "fl");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        up_telemetry_t__tlm->uint8_t__flags = (uint8_t)int32_t__value;
    }

    char__at = func__UpLink_FindKey(char__body, "fl2");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        up_telemetry_t__tlm->uint8_t__flags2 = (uint8_t)int32_t__value;
    }

    char__at = func__UpLink_FindKey(char__body, "vm");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        uint32_t uint32_t__newVm = (uint32_t)int32_t__value;
        if (uint32_t__newVm > up_telemetry_t__tlm->uint32_t__versionMismatch)
        {
            UPPANEL_STATE_T__G__State.uint32_t__vmChangedMs = (uint32_t)millis();
        }
        up_telemetry_t__tlm->uint32_t__versionMismatch = uint32_t__newVm;
    }

    char__at = func__UpLink_FindKey(char__body, "ce");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        uint32_t uint32_t__newCe = (uint32_t)int32_t__value;
        if (uint32_t__newCe > up_telemetry_t__tlm->uint32_t__crcErrors)
        {
            UPPANEL_STATE_T__G__State.uint32_t__ceChangedMs = (uint32_t)millis();
        }
        up_telemetry_t__tlm->uint32_t__crcErrors = uint32_t__newCe;
    }

    /* [EN] Params need their own parse because values are signed and may be
            `null`; the array reader above is unsigned-only by design.
       [FA] پارامترها خوانندهٔ خودشان را می‌خواهند چون مقدارها علامت‌دارند و
            ممکن است `null` باشند؛ خوانندهٔ بالا عمداً فقط بی‌علامت است. */
    char__at = func__UpLink_FindKey(char__body, "p");
    if ((char__at != NULL) && (char__at[0] == '['))
    {
        uint8_t uint8_t__index = 0u;
        char__at++;
        while ((uint8_t__index < UP_PARAM_COUNT) && (char__at[0] != '\0') && (char__at[0] != ']'))
        {
            uint32_t uint32_t__consumed = 0u;
            int32_t int32_t__param = 0;

            if (char__at[0] == ',')
            {
                char__at++;
                continue;
            }

            if (func__UpLink_ParseInt(char__at, &int32_t__param, &uint32_t__consumed))
            {
                INT32_T__G__UpParamValue[uint8_t__index] = int32_t__param;
                up_telemetry_t__tlm->bool__paramKnown[uint8_t__index] = true;
            }
            else
            {
                up_telemetry_t__tlm->bool__paramKnown[uint8_t__index] = false;
            }
            uint8_t__index++;
            if (uint32_t__consumed == 0u)
            {
                break;
            }
            char__at += uint32_t__consumed;
        }

        for (uint8_t uint8_t__copy = 0u; uint8_t__copy < UP_PARAM_COUNT; uint8_t__copy++)
        {
            up_telemetry_t__tlm->int32_t__param[uint8_t__copy] = INT32_T__G__UpParamValue[uint8_t__copy];
        }
    }

    up_telemetry_t__tlm->bool__seen = true;
    up_telemetry_t__tlm->uint32_t__atMs = (uint32_t)millis();
    up_telemetry_t__tlm->uint32_t__frames++;
    UPPANEL_STATE_T__G__State.bool__staUp = true;

    return true;
}

/**
 * @brief  [EN] Apply a `/m` body (the bench window) to the peaks model.
 *         [FA] اعمال بدنهٔ `/m` (پنجرهٔ بنچ) روی مدل اوج‌ها.
 * @param  char__body [EN] JSON text / [FA] متن JSON
 * @return [EN] true when parsed / [FA] در صورت خواندن true
 */
static bool func__UpLink_ApplyPeaks(const char *char__body)
{
    up_peaks_t *up_peaks_t__peaks = &UPPANEL_STATE_T__G__State.up_peaks_t__peaks;
    uint32_t uint32_t__hi[UP_LINK_PEAK_FIELDS];
    uint32_t uint32_t__lo[UP_LINK_PEAK_FIELDS];
    uint8_t uint8_t__count;
    const char *char__at;
    uint32_t uint32_t__used = 0u;
    int32_t int32_t__value = 0;

    uint8_t__count = func__UpLink_ParseArray(char__body, "hi", uint32_t__hi, UP_LINK_PEAK_FIELDS, NULL);
    if (uint8_t__count == 0u)
    {
        return false;
    }
    (void)func__UpLink_ParseArray(char__body, "lo", uint32_t__lo, UP_LINK_PEAK_FIELDS, NULL);

    char__at = func__UpLink_FindKey(char__body, "n");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        up_peaks_t__peaks->uint32_t__count = (uint32_t)int32_t__value;
    }

    char__at = func__UpLink_FindKey(char__body, "or");
    if (func__UpLink_ParseInt(char__at, &int32_t__value, &uint32_t__used))
    {
        up_peaks_t__peaks->uint32_t__faultOr = (uint32_t)int32_t__value;
    }

    if (uint8_t__count > UP_TLM_MA1_IEST)
    {
        up_peaks_t__peaks->uint32_t__peakI1 = uint32_t__hi[UP_TLM_MA1_IEST];
    }
    if (uint8_t__count > UP_TLM_MA2_IEST)
    {
        up_peaks_t__peaks->uint32_t__peakI2 = uint32_t__hi[UP_TLM_MA2_IEST];
    }
    if (uint8_t__count > UP_TLM_DUTY1)
    {
        up_peaks_t__peaks->uint32_t__peakDuty1 = uint32_t__hi[UP_TLM_DUTY1];
    }
    if (uint8_t__count > UP_TLM_DUTY2)
    {
        up_peaks_t__peaks->uint32_t__peakDuty2 = uint32_t__hi[UP_TLM_DUTY2];
    }
    if (uint8_t__count > UP_TLM_V24)
    {
        up_peaks_t__peaks->uint32_t__maxV24 = uint32_t__hi[UP_TLM_V24];
        if (uint32_t__lo[UP_TLM_V24] > 0u)
        {
            up_peaks_t__peaks->uint32_t__minV24 = uint32_t__lo[UP_TLM_V24];
        }
    }

    up_peaks_t__peaks->bool__seen = true;
    up_peaks_t__peaks->uint32_t__atMs = (uint32_t)millis();

    return true;
}

/* [EN] Forward declaration: the loop sends queued writes before the next read,
   and the send function is defined below with the rest of the write path.
   [FA] اعلان جلوتر: حلقه، نوشتن‌های صف‌شده را پیش از خواندن بعدی می‌فرستد و
   تابع ارسال پایین‌تر با بقیهٔ مسیر نوشتن تعریف شده است. */
static bool func__UpLink_PumpParams(void);

/* ==================== Request state machine / ماشین حالت درخواست ==================== */
/**
 * @brief  [EN] Open a connection and write the request line for a job.
 *         [FA] باز کردن اتصال و نوشتن خط درخواست برای یک کار.
 * @param  uint8_t__job [EN] UP_LINK_JOB_* / [FA] نوع کار
 * @return [EN] true when the socket opened / [FA] در صورت باز شدن سوکت true
 */
static bool func__UpLink_StartJob(uint8_t uint8_t__job)
{
    static char CHAR__G__UpRequest[192];
    const char *char__path = (uint8_t__job == UP_LINK_JOB_PEAKS) ? UP_SRC_STAT_PATH : UP_SRC_TLM_PATH;

    if (WiFi.status() != WL_CONNECTED)
    {
        UPPANEL_STATE_T__G__State.bool__staUp = false;
        return false;
    }

    if (!UP_WIFICLIENT_T__G__Client.connect(UP_SRC_HOST, UP_SRC_PORT))
    {
        func__UpLink_NoteError();
        return false;
    }

    (void)snprintf(CHAR__G__UpRequest, sizeof(CHAR__G__UpRequest),
                   "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n"
                   "User-Agent: ChangeOver-UserPanel\r\n\r\n",
                   char__path, UP_SRC_HOST);
    UP_WIFICLIENT_T__G__Client.print(CHAR__G__UpRequest);

    UINT8_T__G__UpJob = uint8_t__job;
    UINT32_T__G__UpResponseLen = 0u;
    UINT16_T__G__UpContentLength = 0u;
    BOOL__G__UpHeaderDone = false;
    UINT32_T__G__UpJobStartedMs = (uint32_t)millis();
    UINT32_T__G__UpEndsAtMs = UINT32_T__G__UpJobStartedMs + 4000u;
    UINT8_T__G__UpState = UP_LINK_ST_HEADERS;
    CHAR__G__UpResponse[0] = '\0';

    return true;
}

/**
 * @brief  [EN] Read whatever arrived, strip the HTTP headers once, and keep the
 *              body in the fixed buffer. Bounded per call so the web server
 *              keeps answering while a request is in flight.
 *         [FA] خواندن هر چه رسیده، یک‌بار برداشتن هدرهای HTTP و نگه‌داشتن بدنه
 *              در بافر ثابت. هر فراخوانی محدود است تا وب‌سرور در حین درخواست
 *              هم جواب بدهد.
 * @return [EN] true when the transfer finished / [FA] در صورت پایان انتقال true
 */
static bool func__UpLink_PumpJob(void)
{
    uint8_t uint8_t__chunk[UP_LINK_READ_CHUNK];
    uint32_t uint32_t__availableRuns = 0u;
    bool bool__finished = false;

    if ((UINT8_T__G__UpState != UP_LINK_ST_HEADERS) && (UINT8_T__G__UpState != UP_LINK_ST_BODY))
    {
        return false;
    }

    while ((uint32_t)millis() < UINT32_T__G__UpEndsAtMs)
    {
        int int__available = UP_WIFICLIENT_T__G__Client.available();

        if (int__available <= 0)
        {
            if (!UP_WIFICLIENT_T__G__Client.connected() && (UINT8_T__G__UpState == UP_LINK_ST_BODY))
            {
                bool__finished = true;
                break;
            }
            uint32_t__availableRuns++;
            if (uint32_t__availableRuns > 3u)
            {
                break;
            }
            delay(2);
            continue;
        }

        size_t size_t__want = (size_t)int__available;
        if (size_t__want > sizeof(uint8_t__chunk))
        {
            size_t__want = sizeof(uint8_t__chunk);
        }

        int int__got = UP_WIFICLIENT_T__G__Client.read(uint8_t__chunk, size_t__want);
        if (int__got <= 0)
        {
            break;
        }

        if (!BOOL__G__UpHeaderDone)
        {
            for (int int__i = 0; int__i < int__got; int__i++)
            {
                CHAR__G__UpResponse[UINT32_T__G__UpResponseLen] = (char)uint8_t__chunk[int__i];
                UINT32_T__G__UpResponseLen++;
                if (UINT32_T__G__UpResponseLen >= (UP_JSON_BUF - 1u))
                {
                    break;
                }
            }
            CHAR__G__UpResponse[UINT32_T__G__UpResponseLen] = '\0';

            char *char__body = strstr(CHAR__G__UpResponse, "\r\n\r\n");
            if (char__body != NULL)
            {
                char__body += 4;
                size_t size_t__bodyLen = strlen(char__body);
                memmove(CHAR__G__UpResponse, char__body, size_t__bodyLen + 1u);
                UINT32_T__G__UpResponseLen = (uint32_t)size_t__bodyLen;
                BOOL__G__UpHeaderDone = true;
                UINT8_T__G__UpState = UP_LINK_ST_BODY;
            }
            else if (UINT32_T__G__UpResponseLen >= (UP_JSON_BUF - 1u))
            {
                /* [EN] Header larger than the buffer: give up cleanly.
                   [FA] هدر بزرگ‌تر از بافر: تمیز انصراف می‌دهیم. */
                bool__finished = true;
                break;
            }
        }
        else
        {
            size_t size_t__room = (UP_JSON_BUF - 1u) - UINT32_T__G__UpResponseLen;
            size_t size_t__take = (size_t__room < (size_t)int__got) ? size_t__room : (size_t)int__got;

            memcpy(&CHAR__G__UpResponse[UINT32_T__G__UpResponseLen], uint8_t__chunk, size_t__take);
            UINT32_T__G__UpResponseLen += (uint32_t)size_t__take;
            CHAR__G__UpResponse[UINT32_T__G__UpResponseLen] = '\0';
        }
    }

    if ((uint32_t)millis() >= UINT32_T__G__UpEndsAtMs)
    {
        bool__finished = true;
    }

    if (bool__finished)
    {
        UP_WIFICLIENT_T__G__Client.stop();
        UINT8_T__G__UpState = UP_LINK_ST_IDLE;
        return true;
    }

    return false;
}

/* ==================== Public surface / سطح عمومی ==================== */
/**
 * @brief  [EN] Start Wi-Fi: join the engineering network as a station AND offer
 *              the user network on a DIFFERENT subnet, because the ESP8266's
 *              default soft-AP address (192.168.4.1) is exactly the address
 *              this panel has to reach on the other network, and two default
 *              subnets cannot coexist on one device.
 *         [FA] روشن‌کردن وای‌فای: پیوستن به شبکهٔ مهندسی به‌عنوان کلاینت و
 *              هم‌زمان ارائهٔ شبکهٔ کاربر روی زیرشبکه‌ای متفاوت، چون آدرس
 *              پیش‌فرض سافت‌اپ ESP8266 (۱۹۲.۱۶۸.۴.۱) دقیقاً همان آدرسی است که
 *              این پنل باید در شبکهٔ دیگر بگیرد و دو زیرشبکهٔ پیش‌فرض روی یک
 *              دستگاه جمع نمی‌شوند.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpLink_Begin(void)
{
    IPAddress up_ipaddress_t__apIp(192, 168, 5, 1);
    IPAddress up_ipaddress_t__apMask(255, 255, 255, 0);

    WiFi.mode(WIFI_AP_STA);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
    WiFi.setSleep(false);
#endif

    (void)WiFi.softAPConfig(up_ipaddress_t__apIp, up_ipaddress_t__apIp, up_ipaddress_t__apMask);
    (void)WiFi.softAP(UP_AP_SSID, UP_AP_PASS, UP_AP_CHANNEL);
    WiFi.begin(UP_STA_SSID, UP_STA_PASS);

    memset(BOOL__G__UpParamPending, 0, sizeof(BOOL__G__UpParamPending));
    memset(INT32_T__G__UpParamValue, 0, sizeof(INT32_T__G__UpParamValue));

    UINT32_T__G__UpNextTelemetryMs = (uint32_t)millis();
    UINT32_T__G__UpNextPeaksMs = UINT32_T__G__UpNextTelemetryMs + 3000u;
    UINT8_T__G__UpState = UP_LINK_ST_IDLE;
    BOOL__G__UpWifiStarted = true;
    UPPANEL_STATE_T__G__State.bool__staUp = false;
}

/**
 * @brief  [EN] Poll the two read-only endpoints on their own cadences and keep
 *              the link-health numbers honest.
 *         [FA] خواندن دو مسیر فقط-خواندنی با بازهٔ خودشان و به‌روز نگه‌داشتن
 *              اعداد سلامت لینک.
 * @return [EN] None / [FA] ندارد
 */
static void func__UpLink_Loop(void)
{
    uint32_t uint32_t__nowMs = (uint32_t)millis();

    UPPANEL_STATE_T__G__State.bool__staUp = (WiFi.status() == WL_CONNECTED);

    /* [EN] A queued charger command goes out BEFORE the next read: the operator
            pressed a button and is watching the screen, while the read only
            refreshes a chart. One write per call, so a slow board delays the
            polling instead of stalling the panel.
       [FA] فرمان شارژر در صف، پیش از خواندن بعدی می‌رود: اپراتور دکمه را زده و
            به صفحه نگاه می‌کند، در حالی که خواندن فقط نمودار را تازه می‌کند.
            در هر فراخوانی یک نوشتن، تا برد کند پایش را عقب بیندازد نه کل پنل را. */
    if (func__UpLink_PumpParams())
    {
        return;
    }

    if (UINT8_T__G__UpState != UP_LINK_ST_IDLE)
    {
        if (func__UpLink_PumpJob())
        {
            if (UINT8_T__G__UpJob == UP_LINK_JOB_PEAKS)
            {
                if (!func__UpLink_ApplyPeaks(CHAR__G__UpResponse))
                {
                    func__UpLink_NoteError();
                }
            }
            else
            {
                if (!func__UpLink_ApplyTelemetry(CHAR__G__UpResponse))
                {
                    func__UpLink_NoteError();
                }
            }
            UINT32_T__G__UpRetryDelayMs = UP_RETRY_MIN_MS;
        }
        return;
    }

    if (!UPPANEL_STATE_T__G__State.bool__staUp)
    {
        /* [EN] Not joined: retry with a backoff so a missing engineering board
                does not turn into a busy loop.
           [FA] وصل نیستیم: با عقب‌نشینی دوباره تلاش می‌کنیم تا نبودِ برد
                مهندسی به حلقهٔ داغ تبدیل نشود. */
        if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__UpNextTelemetryMs) >= UINT32_T__G__UpRetryDelayMs)
        {
            UINT32_T__G__UpNextTelemetryMs = uint32_t__nowMs;
            UINT32_T__G__UpRetryDelayMs *= 2u;
            if (UINT32_T__G__UpRetryDelayMs > UP_RETRY_MAX_MS)
            {
                UINT32_T__G__UpRetryDelayMs = UP_RETRY_MAX_MS;
            }
            (void)func__UpLink_StartJob(UP_LINK_JOB_TELEMETRY);
        }
        return;
    }

    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__UpNextTelemetryMs) >= UP_TLM_POLL_MS)
    {
        UINT32_T__G__UpNextTelemetryMs = uint32_t__nowMs;
        (void)func__UpLink_StartJob(UP_LINK_JOB_TELEMETRY);
        return;
    }

    if ((uint32_t)(uint32_t__nowMs - UINT32_T__G__UpNextPeaksMs) >= UP_STAT_POLL_MS)
    {
        UINT32_T__G__UpNextPeaksMs = uint32_t__nowMs;
        (void)func__UpLink_StartJob(UP_LINK_JOB_PEAKS);
    }
}

/**
 * @brief  [EN] Queue one parameter write. This is the ONLY write path in the
 *              firmware, it is reachable only from the admin action handler,
 *              and it never touches any id except the two charger enables.
 *         [FA] صف‌کردن یک نوشتن پارامتر. این تنها مسیر نوشتن در فرم‌ور است،
 *              فقط از هندلر اقدام مدیر قابل دسترسی است و هیچ شناسه‌ای جز دو
 *              فعال‌ساز شارژر را دست نمی‌زند.
 * @param  uint8_t__id [EN] parameter id (11 or 12 here) / [FA] شناسهٔ پارامتر
 * @param  int32_t__value [EN] value 0 or 1 / [FA] مقدار ۰ یا ۱
 * @return [EN] true when queued / [FA] در صورت صف شدن true
 */
static bool func__UpLink_QueueParam(uint8_t uint8_t__id, int32_t int32_t__value)
{
    if (uint8_t__id >= UP_PARAM_COUNT)
    {
        return false;
    }

    INT32_T__G__UpParamValue[uint8_t__id] = int32_t__value;
    BOOL__G__UpParamPending[uint8_t__id] = true;
    return true;
}

/**
 * @brief  [EN] Send queued parameter writes, one per call (POST /s). Called
 *              from the same loop that polls, so a slow board delays the next
 *              poll instead of blocking the panel.
 *         [FA] فرستادن نوشتن‌های صف‌شده، یکی در هر فراخوانی (POST /s). از همان
 *              حلقه‌ای صدا زده می‌شود که پایش می‌کند، پس برد کند، پایش بعدی را
 *              عقب می‌اندازد نه کل پنل را.
 * @return [EN] true when something was sent / [FA] در صورت ارسال چیزی true
 */
static bool func__UpLink_PumpParams(void)
{
    static char CHAR__G__UpPostBody[64];
    static char CHAR__G__UpPostRequest[224];
    uint8_t uint8_t__id;

    if ((UINT8_T__G__UpState != UP_LINK_ST_IDLE) || !UPPANEL_STATE_T__G__State.bool__staUp)
    {
        return false;
    }

    for (uint8_t__id = 0u; uint8_t__id < UP_PARAM_COUNT; uint8_t__id++)
    {
        if (BOOL__G__UpParamPending[uint8_t__id])
        {
            break;
        }
    }
    if (uint8_t__id >= UP_PARAM_COUNT)
    {
        return false;
    }

    int int__length = snprintf(CHAR__G__UpPostBody, sizeof(CHAR__G__UpPostBody),
                               "id=%u&v=%ld", (unsigned)uint8_t__id, (long)INT32_T__G__UpParamValue[uint8_t__id]);
    if (int__length <= 0)
    {
        return false;
    }

    if (!UP_WIFICLIENT_T__G__Client.connect(UP_SRC_HOST, UP_SRC_PORT))
    {
        func__UpLink_NoteError();
        return false;
    }

    (void)snprintf(CHAR__G__UpPostRequest, sizeof(CHAR__G__UpPostRequest),
                   "POST %s HTTP/1.0\r\nHost: %s\r\n"
                   "Content-Type: application/x-www-form-urlencoded\r\n"
                   "Content-Length: %u\r\nConnection: close\r\n\r\n%s",
                   UP_SRC_SET_PATH, UP_SRC_HOST, (unsigned)strlen(CHAR__G__UpPostBody), CHAR__G__UpPostBody);
    UP_WIFICLIENT_T__G__Client.print(CHAR__G__UpPostRequest);
    UP_WIFICLIENT_T__G__Client.stop();

    BOOL__G__UpParamPending[uint8_t__id] = false;

    return true;
}

#endif /* UP_LINK_H */
