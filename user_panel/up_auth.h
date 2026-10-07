/**
 * @file    up_auth.h
 * @brief   [EN] Users, roles, sessions and the admin action log of the user
 *              panel. Passwords are stored as salted, iterated SHA-256 digests;
 *              sessions live in RAM only (a reboot logs everybody out, which is
 *              the safe direction) and the action log is persisted so an admin
 *              can still see who did what after a restart.
 *          [FA] کاربران، نقش‌ها، نشست‌ها و گزارش اقدامات پنل کاربر. گذرواژه‌ها
 *              به‌شکل چکیدهٔ SHA-256 نمک‌دار و تکرارشده ذخیره می‌شوند؛ نشست‌ها
 *              فقط در RAM هستند (ری‌استارت همه را بیرون می‌کند و این جهتِ امن
 *              است) و گزارش اقدامات روی فلش می‌ماند تا مدیر بعد از ری‌استارت هم
 *              ببیند چه کسی چه کرد.
 */

#ifndef UP_AUTH_H
#define UP_AUTH_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "up_config.h"
#include "up_state.h"
#include "up_sha256.h"

/* ==================== Types / تایپ‌ها ==================== */
/* [EN] One user record as stored on flash. The layout is part of the file
   format: fields are only ever appended, never reordered.
   [FA] یک رکورد کاربر روی فلش. چیدمان بخشی از قالب فایل است: فیلد فقط اضافه
   می‌شود، هرگز جابه‌جا نمی‌شود. */
typedef struct
{
    char     char__name[UP_NAME_MAX + 1u];   /* [EN] login name / [FA] نام ورود */
    uint8_t  uint8_t__role;                  /* [EN] UP_ROLE_* / [FA] نقش */
    uint8_t  uint8_t__mustChange;            /* [EN] force a new password / [FA] اجبار تغییر گذرواژه */
    uint8_t  uint8_t__pad[2];                /* [EN] alignment / [FA] هم‌ترازی */
    uint8_t  uint8_t__salt[UP_SALT_BYTES];   /* [EN] per-user salt / [FA] نمک هر کاربر */
    uint8_t  uint8_t__digest[UP_HASH_BYTES]; /* [EN] iterated digest / [FA] چکیدهٔ تکرارشده */
    uint32_t uint32_t__createdEpoch;         /* [EN] 0 when the clock was unknown / [FA] صفر وقتی ساعت نامعلوم بود */
    uint32_t uint32_t__lastLoginEpoch;       /* [EN] same / [FA] همان */
} up_user_t;

typedef struct
{
    bool     bool__used;
    char     char__token[UP_SESSION_TOKEN_LEN + 1u];
    uint8_t  uint8_t__userIndex;
    uint8_t  uint8_t__role;
    uint32_t uint32_t__lastSeenMs;
    uint32_t uint32_t__startedEpoch;
} up_session_t;

/* [EN] One row of the admin action log (persisted, 24 rows ring). The text is
   stored in English/ASCII and translated by the web page, so the panel never
   has to hold Persian fonts or long strings in RAM.
   [FA] یک ردیف گزارش اقدامات مدیر (ماندگار، حلقهٔ ۲۴ ردیفی). متن به‌صورت
   کوتاه ذخیره و در صفحهٔ وب ترجمه می‌شود تا پنل مجبور نباشد متن طولانی را در
   RAM نگه دارد. */
typedef struct
{
    uint32_t uint32_t__ageS;      /* [EN] seconds since panel boot / [FA] ثانیه از بوت پنل */
    uint32_t uint32_t__epoch;     /* [EN] wall clock when known / [FA] ساعت مطلق اگر معلوم بود */
    char     char__user[UP_NAME_MAX + 1u];
    char     char__action[20u];   /* [EN] short verb, e.g. "charger1_off" / [FA] فعل کوتاه */
} up_audit_t;

/* ==================== Constants / ثابت‌ها ==================== */
#define UP_USERS_MAGIC   0x55505553u   /* [EN] "UPUS" / [FA] «UPUS» */
#define UP_USERS_VERSION 1u
#define UP_AUDIT_MAX     24u
#define UP_AUDIT_MAGIC   0x55504155u   /* [EN] "UPAU" / [FA] «UPAU» */

/* ==================== State / وضعیت ==================== */
static up_user_t    UP_USER_T__A__Users[UP_USERS_MAX];
static uint8_t      UINT8_T__G__UserCount = 0u;
static up_session_t UP_SESSION_T__A__Sessions[UP_SESSION_MAX];
static uint8_t      UINT8_T__G__CurrentRole = 0u;    /* [EN] role of this request / [FA] نقش همین درخواست */
static uint8_t      UINT8_T__G__CurrentUserIndex = 0xFFu;
static uint8_t      UINT8_T__G__LoginFails = 0u;
static uint32_t     UINT32_T__G__LoginLockUntilMs = 0u;
static uint32_t     UINT32_T__G__TokenCounter = 0u;
static up_audit_t   UP_AUDIT_T__A__Log[UP_AUDIT_MAX];
static uint8_t      UINT8_T__G__AuditCount = 0u;
static uint8_t      UINT8_T__G__AuditHead = 0u;

/* [EN] Forward declarations for the few helpers that are used by the block
   above their own definition. Everywhere else the file is written in call
   order on purpose, so a reader never has to jump backwards to find a callee.
   [FA] اعلان جلوتر برای کمکی‌هایی که بالاتر از تعریف خودشان استفاده می‌شوند.
   بقیهٔ فایل عمداً به ترتیب فراخوان نوشته شده تا خواننده برای پیدا کردن
   تابع صدا‌زده‌شده مجبور نباشد به عقب برگردد. */
static void func__UpAuth_ClearSessions(void);
static bool func__UpAuth_SaveAudit(void);
static bool func__UpAuth_LoadAudit(void);
static bool func__UpAuth_AddUser(const char *char__name, const char *char__pass, uint8_t uint8_t__role, bool bool__mustChange);

/* ==================== Password hashing / چکیدهٔ گذرواژه ==================== */
/**
 * @brief  [EN] Salted, iterated digest of a password. iteration count is in
 *              up_config.h; the loop re-hashes (previous digest || salt) so a
 *              stolen flash image costs an attacker real time per guess
 *              instead of one hash.
 *         [FA] چکیدهٔ نمک‌دار و تکرارشدهٔ گذرواژه. شمار تکرار در up_config.h
 *              است؛ هر دور (چکیدهٔ قبلی || نمک) را دوباره چکیده می‌گیرد تا یک
 *              فلش دزدیده‌شده برای هر حدس هزینهٔ واقعی داشته باشد، نه یک چکیده.
 * @param  uint8_t__salt [EN] UP_SALT_BYTES bytes / [FA] نمک
 * @param  char__pass [EN] NUL-terminated password / [FA] گذرواژه
 * @param  uint8_t__out [EN] UP_HASH_BYTES output / [FA] خروجی ۳۲ بایتی
 * @return [EN] None / [FA] ندارد
 */
static void func__UpAuth_Digest(const uint8_t *uint8_t__salt, const char *char__pass, uint8_t *uint8_t__out)
{
    uint8_t uint8_t__buffer[UP_SHA256_BLOCK_BYTES + UP_SALT_BYTES];
    uint32_t uint32_t__passLen = (uint32_t)strlen(char__pass);
    uint32_t uint32_t__copyLen = uint32_t__passLen;
    uint32_t uint32_t__round;

    if (uint32_t__copyLen > UP_SHA256_BLOCK_BYTES)
    {
        uint32_t__copyLen = UP_SHA256_BLOCK_BYTES;
    }

    memcpy(uint8_t__buffer, char__pass, uint32_t__copyLen);
    memcpy(&uint8_t__buffer[uint32_t__copyLen], uint8_t__salt, UP_SALT_BYTES);
    func__UpSha256_Buffer(uint8_t__buffer, uint32_t__copyLen + UP_SALT_BYTES, uint8_t__out);

    for (uint32_t__round = 1u; uint32_t__round < UP_HASH_ITERATIONS; uint32_t__round++)
    {
        memcpy(uint8_t__buffer, uint8_t__out, UP_SHA256_DIGEST_BYTES);
        memcpy(&uint8_t__buffer[UP_SHA256_DIGEST_BYTES], uint8_t__salt, UP_SALT_BYTES);
        func__UpSha256_Buffer(uint8_t__buffer, UP_SHA256_DIGEST_BYTES + UP_SALT_BYTES, uint8_t__out);
    }
}

/**
 * @brief  [EN] Constant-time-ish digest compare (no early exit), so a failed
 *              login does not leak how many bytes matched.
 *         [FA] مقایسهٔ چکیده بدون خروج زودهنگام، تا ورود ناموفق نشان ندهد چند
 *              بایت درست بوده است.
 * @return [EN] true when both digests are equal / [FA] اگر برابر باشند true
 */
static bool func__UpAuth_DigestEquals(const uint8_t *uint8_t__a, const uint8_t *uint8_t__b)
{
    uint8_t uint8_t__diff = 0u;
    uint32_t uint32_t__i;

    for (uint32_t__i = 0u; uint32_t__i < UP_HASH_BYTES; uint32_t__i++)
    {
        uint8_t__diff |= (uint8_t)(uint8_t__a[uint32_t__i] ^ uint8_t__b[uint32_t__i]);
    }

    return (uint8_t__diff == 0u);
}

/**
 * @brief  [EN] Fill a buffer with pseudo-random bytes for salts and session
 *              tokens. Seeded from the microsecond clock and a counter, then
 *              stretched through SHA-256: on a device with no true RNG this is
 *              the honest maximum, and it is documented as such.
 *         [FA] پر کردن بافر با بایت‌های شبه‌تصادفی برای نمک و توکن نشست.
 *              دانه از ساعت میکروثانیه و یک شمارنده گرفته و با SHA-256 کشیده
 *              می‌شود؛ روی دستگاهی بدون RNG واقعی این سقف صادقانه است و همان‌طور
 *              هم مستند شده.
 * @param  uint8_t__out [EN] destination / [FA] مقصد
 * @param  uint32_t__len [EN] how many bytes / [FA] چند بایت
 * @return [EN] None / [FA] ندارد
 */
static void func__UpAuth_RandomBytes(uint8_t *uint8_t__out, uint32_t uint32_t__len)
{
    uint8_t uint8_t__seed[16];
    uint8_t uint8_t__digest[UP_SHA256_DIGEST_BYTES];
    uint32_t uint32_t__micros = (uint32_t)micros();
    uint32_t uint32_t__clock = (uint32_t)millis();
    uint32_t uint32_t__i;

    UINT32_T__G__TokenCounter++;

    uint8_t__seed[0]  = (uint8_t)(uint32_t__micros & 0xFFu);
    uint8_t__seed[1]  = (uint8_t)((uint32_t__micros >> 8) & 0xFFu);
    uint8_t__seed[2]  = (uint8_t)((uint32_t__micros >> 16) & 0xFFu);
    uint8_t__seed[3]  = (uint8_t)((uint32_t__micros >> 24) & 0xFFu);
    uint8_t__seed[4]  = (uint8_t)(uint32_t__clock & 0xFFu);
    uint8_t__seed[5]  = (uint8_t)((uint32_t__clock >> 8) & 0xFFu);
    uint8_t__seed[6]  = (uint8_t)((uint32_t__clock >> 16) & 0xFFu);
    uint8_t__seed[7]  = (uint8_t)((uint32_t__clock >> 24) & 0xFFu);
    uint8_t__seed[8]  = (uint8_t)(UINT32_T__G__TokenCounter & 0xFFu);
    uint8_t__seed[9]  = (uint8_t)((UINT32_T__G__TokenCounter >> 8) & 0xFFu);
    uint8_t__seed[10] = (uint8_t)(UINT8_T__G__UserCount);
    uint8_t__seed[11] = (uint8_t)(UINT8_T__G__AuditCount);
    uint8_t__seed[12] = (uint8_t)(func__UpState_UptimeS() & 0xFFu);
    uint8_t__seed[13] = (uint8_t)((func__UpState_UptimeS() >> 8) & 0xFFu);
    uint8_t__seed[14] = (uint8_t)(UINT8_T__G__LoginFails);
    uint8_t__seed[15] = 0xA5u;

    func__UpSha256_Buffer(uint8_t__seed, sizeof(uint8_t__seed), uint8_t__digest);
    for (uint32_t__i = 0u; uint32_t__i < uint32_t__len; uint32_t__i++)
    {
        uint8_t__out[uint32_t__i] = uint8_t__digest[uint32_t__i % UP_SHA256_DIGEST_BYTES];
    }
}

/* ==================== User store / انبار کاربران ==================== */
/**
 * @brief  [EN] Read the users file into RAM; seed the two built-in accounts on
 *              a virgin panel.
 *         [FA] خواندن فایل کاربران در RAM؛ ساخت دو حساب پیش‌فرض روی پنل نو.
 * @return [EN] true when at least one user is usable / [FA] اگر حداقل یک کاربر آماده باشد true
 */
static bool func__UpAuth_Begin(void)
{
    File up_file_t__f;
    uint32_t uint32_t__magic = 0u;
    uint16_t uint16_t__version = 0u;
    uint16_t uint16_t__count = 0u;
    bool bool__loaded = false;

    memset(UP_USER_T__A__Users, 0, sizeof(UP_USER_T__A__Users));
    UINT8_T__G__UserCount = 0u;

    up_file_t__f = LittleFS.open(UP_F_USERS, "r");
    if (up_file_t__f)
    {
        if (up_file_t__f.read((uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic)) == (int)sizeof(uint32_t__magic))
        {
            (void)up_file_t__f.read((uint8_t *)&uint16_t__version, sizeof(uint16_t__version));
            (void)up_file_t__f.read((uint8_t *)&uint16_t__count, sizeof(uint16_t__count));
            if ((uint32_t__magic == UP_USERS_MAGIC) && (uint16_t__version == UP_USERS_VERSION) &&
                (uint16_t__count <= UP_USERS_MAX))
            {
                for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__count; uint16_t__i++)
                {
                    size_t size_t__got = up_file_t__f.read((uint8_t *)&UP_USER_T__A__Users[uint16_t__i], sizeof(up_user_t));
                    if (size_t__got != sizeof(up_user_t))
                    {
                        break;
                    }
                    UINT8_T__G__UserCount++;
                }
                bool__loaded = (UINT8_T__G__UserCount > 0u);
            }
        }
        up_file_t__f.close();
    }

    if (!bool__loaded)
    {
        (void)func__UpAuth_AddUser(UP_SEED_ADMIN_NAME, UP_SEED_ADMIN_PASS, UP_ROLE_ADMIN, true);
        (void)func__UpAuth_AddUser(UP_SEED_USER_NAME, UP_SEED_USER_PASS, UP_ROLE_VIEWER, true);
    }

    /* [EN] The action log is loaded here too. Without this line every reboot
            silently erased the record of who cut what - the one thing the log
            exists for.
       [FA] گزارش اقدامات هم همین‌جا خوانده می‌شود. بدون این خط، هر ری‌استارت
            بی‌صدا رکورد اینکه چه کسی چه چیزی را قطع کرده پاک می‌کرد - تنها
            چیزی که گزارش برایش وجود دارد. */
    (void)func__UpAuth_LoadAudit();

    return (UINT8_T__G__UserCount > 0u);
}

/**
 * @brief  [EN] Write the whole user table back to flash (it is tiny).
 *         [FA] نوشتن کل جدول کاربران روی فلش (کوچک است).
 * @return [EN] true on success / [FA] در صورت موفقیت true
 */
static bool func__UpAuth_Save(void)
{
    File up_file_t__f;
    uint32_t uint32_t__magic = UP_USERS_MAGIC;
    uint16_t uint16_t__version = UP_USERS_VERSION;
    uint16_t uint16_t__count = (uint16_t)UINT8_T__G__UserCount;
    bool bool__ok = true;

    LittleFS.remove(UP_F_USERS);
    up_file_t__f = LittleFS.open(UP_F_USERS, "w");
    if (!up_file_t__f)
    {
        return false;
    }

    bool__ok = bool__ok && (up_file_t__f.write((const uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic)) == sizeof(uint32_t__magic));
    bool__ok = bool__ok && (up_file_t__f.write((const uint8_t *)&uint16_t__version, sizeof(uint16_t__version)) == sizeof(uint16_t__version));
    bool__ok = bool__ok && (up_file_t__f.write((const uint8_t *)&uint16_t__count, sizeof(uint16_t__count)) == sizeof(uint16_t__count));
    for (uint16_t uint16_t__i = 0u; uint16_t__i < uint16_t__count; uint16_t__i++)
    {
        bool__ok = bool__ok && (up_file_t__f.write((const uint8_t *)&UP_USER_T__A__Users[uint16_t__i], sizeof(up_user_t)) == sizeof(up_user_t));
    }

    up_file_t__f.close();
    return bool__ok;
}

/**
 * @brief  [EN] Index of a user by name, or -1.
 *         [FA] شاخص کاربر با نام، یا -۱.
 * @param  char__name [EN] login name (case sensitive) / [FA] نام ورود
 * @return [EN] index / [FA] شاخص
 */
static int8_t func__UpAuth_FindUser(const char *char__name)
{
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__UserCount; uint8_t__i++)
    {
        if (strncmp(UP_USER_T__A__Users[uint8_t__i].char__name, char__name, UP_NAME_MAX) == 0)
        {
            return (int8_t)uint8_t__i;
        }
    }

    return -1;
}

/**
 * @brief  [EN] Add a user with a fresh salt and digest.
 *         [FA] افزودن کاربر با نمک و چکیدهٔ تازه.
 * @param  char__name [EN] 3..UP_NAME_MAX characters / [FA] نام ۳ تا ۱۲ نویسه
 * @param  char__pass [EN] password, min 6 enforced by the caller / [FA] گذرواژه
 * @param  uint8_t__role [EN] UP_ROLE_* / [FA] نقش
 * @param  bool__mustChange [EN] force change at first login / [FA] اجبار تغییر در اولین ورود
 * @return [EN] true when added / [FA] در صورت افزودن true
 */
static bool func__UpAuth_AddUser(const char *char__name, const char *char__pass, uint8_t uint8_t__role, bool bool__mustChange)
{
    if (UINT8_T__G__UserCount >= UP_USERS_MAX)
    {
        return false;
    }
    if (func__UpAuth_FindUser(char__name) >= 0)
    {
        return false;
    }

    up_user_t *up_user_t__user = &UP_USER_T__A__Users[UINT8_T__G__UserCount];
    memset(up_user_t__user, 0, sizeof(up_user_t));
    strncpy(up_user_t__user->char__name, char__name, UP_NAME_MAX);
    up_user_t__user->char__name[UP_NAME_MAX] = '\0';
    up_user_t__user->uint8_t__role = uint8_t__role;
    up_user_t__user->uint8_t__mustChange = bool__mustChange ? 1u : 0u;
    func__UpAuth_RandomBytes(up_user_t__user->uint8_t__salt, UP_SALT_BYTES);
    func__UpAuth_Digest(up_user_t__user->uint8_t__salt, char__pass, up_user_t__user->uint8_t__digest);
    up_user_t__user->uint32_t__createdEpoch = func__UpState_NowEpochS();
    up_user_t__user->uint32_t__lastLoginEpoch = 0u;

    UINT8_T__G__UserCount++;
    return func__UpAuth_Save();
}

/**
 * @brief  [EN] Set a new password for a user and clear the forced-change flag
 *              (the admin path; the owner path is below).
 *         [FA] گذاشتن گذرواژهٔ تازه برای کاربر و پاک‌کردن پرچم اجبار تغییر.
 * @return [EN] true on success / [FA] در صورت موفقیت true
 */
static bool func__UpAuth_SetPassword(const char *char__name, const char *char__pass, bool bool__mustChange)
{
    int8_t int8_t__index = func__UpAuth_FindUser(char__name);

    if (int8_t__index < 0)
    {
        return false;
    }

    up_user_t *up_user_t__user = &UP_USER_T__A__Users[(uint8_t)int8_t__index];
    func__UpAuth_RandomBytes(up_user_t__user->uint8_t__salt, UP_SALT_BYTES);
    func__UpAuth_Digest(up_user_t__user->uint8_t__salt, char__pass, up_user_t__user->uint8_t__digest);
    up_user_t__user->uint8_t__mustChange = bool__mustChange ? 1u : 0u;

    return func__UpAuth_Save();
}

/* ==================== Admin edits / ویرایش‌های مدیر ==================== */
/**
 * @brief  [EN] How many users hold a role (used to refuse removing the LAST
 *              admin: a panel nobody can administer is a locked panel).
 *         [FA] چند کاربر یک نقش دارند (برای رد کردن حذف «آخرین» مدیر: پنلی که
 *              هیچ‌کس نتواند اداره‌اش کند، پنل قفل‌شده است).
 * @param  uint8_t__role [EN] UP_ROLE_* / [FA] نقش
 * @return [EN] count / [FA] تعداد
 */
static uint8_t func__UpAuth_CountRole(uint8_t uint8_t__role)
{
    uint8_t uint8_t__count = 0u;

    for (uint8_t uint8_t__i = 0u; uint8_t__i < UINT8_T__G__UserCount; uint8_t__i++)
    {
        if (UP_USER_T__A__Users[uint8_t__i].uint8_t__role == uint8_t__role)
        {
            uint8_t__count++;
        }
    }

    return uint8_t__count;
}

/**
 * @brief  [EN] Remove a user by name, preserving the order of the rest.
 *         [FA] حذف کاربر با نام، با حفظ ترتیب بقیه.
 * @param  char__name [EN] login name / [FA] نام ورود
 * @return [EN] true when removed / [FA] در صورت حذف true
 */
static bool func__UpAuth_DeleteUser(const char *char__name)
{
    int8_t int8_t__index = func__UpAuth_FindUser(char__name);
    uint8_t uint8_t__index;

    if ((int8_t__index < 0) || (UINT8_T__G__UserCount <= 1u))
    {
        return false;
    }

    uint8_t__index = (uint8_t)int8_t__index;
    for (uint8_t uint8_t__i = uint8_t__index; (uint8_t__i + 1u) < UINT8_T__G__UserCount; uint8_t__i++)
    {
        UP_USER_T__A__Users[uint8_t__i] = UP_USER_T__A__Users[uint8_t__i + 1u];
    }

    memset(&UP_USER_T__A__Users[UINT8_T__G__UserCount - 1u], 0, sizeof(up_user_t));
    UINT8_T__G__UserCount--;
    func__UpAuth_ClearSessions();

    return func__UpAuth_Save();
}

/**
 * @brief  [EN] Change a user's role; existing sessions are dropped so the old
 *              role cannot keep answering requests it no longer owns.
 *         [FA] تغییر نقش کاربر؛ نشست‌های موجود بسته می‌شوند تا نقش قبلی نتواند
 *              به درخواست‌هایی جواب دهد که دیگر مالکشان نیست.
 * @param  char__name [EN] user / [FA] کاربر
 * @param  uint8_t__role [EN] new role / [FA] نقش تازه
 * @return [EN] true when changed / [FA] در صورت تغییر true
 */
static bool func__UpAuth_SetRole(const char *char__name, uint8_t uint8_t__role)
{
    int8_t int8_t__index = func__UpAuth_FindUser(char__name);

    if ((int8_t__index < 0) || (uint8_t__role < UP_ROLE_VIEWER) || (uint8_t__role > UP_ROLE_ADMIN))
    {
        return false;
    }

    UP_USER_T__A__Users[(uint8_t)int8_t__index].uint8_t__role = uint8_t__role;
    func__UpAuth_ClearSessions();

    return func__UpAuth_Save();
}

/**
 * @brief  [EN] Close every session that belongs to one user - what logging out
 *              does. Closing only "the current token" would need the token to
 *              be carried down here; closing by user is simpler, and a user who
 *              logs out on one phone wants to be out on all of them.
 *         [FA] بستن همهٔ نشست‌های یک کاربر - همان کاری که خروج می‌کند. بستن
 *              فقط «توکن فعلی» یعنی باید توکن تا اینجا حمل شود؛ بستن بر اساس
 *              کاربر ساده‌تر است و کاربری که از یک گوشی خارج می‌شود می‌خواهد
 *              از همه خارج شده باشد.
 * @param  uint8_t__userIndex [EN] index into the user table / [FA] شاخص در جدول کاربران
 * @return [EN] None / [FA] ندارد
 */
static void func__UpAuth_CloseUserSessions(uint8_t uint8_t__userIndex)
{
    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_SESSION_MAX; uint8_t__i++)
    {
        if (UP_SESSION_T__A__Sessions[uint8_t__i].uint8_t__userIndex == uint8_t__userIndex)
        {
            UP_SESSION_T__A__Sessions[uint8_t__i].bool__used = false;
            memset(UP_SESSION_T__A__Sessions[uint8_t__i].char__token, 0,
                   sizeof(UP_SESSION_T__A__Sessions[uint8_t__i].char__token));
        }
    }
}

/* ==================== Sessions / نشست‌ها ==================== */
/**
 * @brief  [EN] Drop every session (used when a user is deleted or the panel
 *              decides the whole set is stale).
 *         [FA] پاک‌کردن همهٔ نشست‌ها (وقتی کاربر حذف می‌شود یا پنل تصمیم می‌گیرد
 *              مجموعه کهنه است).
 * @return [EN] None / [FA] ندارد
 */
static void func__UpAuth_ClearSessions(void)
{
    memset(UP_SESSION_T__A__Sessions, 0, sizeof(UP_SESSION_T__A__Sessions));
}

/**
 * @brief  [EN] Create a session for a user index and return its cookie token.
 *         [FA] ساختن نشست برای یک کاربر و برگرداندن توکن کوکی آن.
 * @param  uint8_t__userIndex [EN] 0..UP_USERS_MAX-1 / [FA] شاخص کاربر
 * @param  char__tokenOut [EN] buffer of UP_SESSION_TOKEN_LEN+1 / [FA] بافر توکن
 * @return [EN] true when a session was created / [FA] در صورت ساخت true
 */
static bool func__UpAuth_OpenSession(uint8_t uint8_t__userIndex, char *char__tokenOut)
{
    uint8_t uint8_t__random[16];
    static const char CHAR__A__Hex[] = "0123456789abcdef";
    up_session_t *up_session_t__slot = NULL;

    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_SESSION_MAX; uint8_t__i++)
    {
        if (!UP_SESSION_T__A__Sessions[uint8_t__i].bool__used)
        {
            up_session_t__slot = &UP_SESSION_T__A__Sessions[uint8_t__i];
            break;
        }
    }

    if (up_session_t__slot == NULL)
    {
        /* [EN] All slots busy: evict the least recently used one - a viewer
                staring at the dashboard must never lock the admin out.
           [FA] همهٔ خانه‌ها پر: قدیمی‌ترین از نظر استفاده بیرون می‌رود تا
                بیننده‌ای که به داشبورد نگاه می‌کند جای مدیر را نگیرد. */
        uint8_t uint8_t__oldest = 0u;
        for (uint8_t uint8_t__i = 1u; uint8_t__i < UP_SESSION_MAX; uint8_t__i++)
        {
            if ((uint32_t)(millis() - UP_SESSION_T__A__Sessions[uint8_t__i].uint32_t__lastSeenMs) >
                (uint32_t)(millis() - UP_SESSION_T__A__Sessions[uint8_t__oldest].uint32_t__lastSeenMs))
            {
                uint8_t__oldest = uint8_t__i;
            }
        }
        up_session_t__slot = &UP_SESSION_T__A__Sessions[uint8_t__oldest];
    }

    func__UpAuth_RandomBytes(uint8_t__random, sizeof(uint8_t__random));
    for (uint8_t uint8_t__i = 0u; uint8_t__i < 16u; uint8_t__i++)
    {
        char__tokenOut[uint8_t__i * 2u] = CHAR__A__Hex[(uint8_t__random[uint8_t__i] >> 4) & 0x0Fu];
        char__tokenOut[uint8_t__i * 2u + 1u] = CHAR__A__Hex[uint8_t__random[uint8_t__i] & 0x0Fu];
    }
    char__tokenOut[UP_SESSION_TOKEN_LEN] = '\0';

    up_session_t__slot->bool__used = true;
    strncpy(up_session_t__slot->char__token, char__tokenOut, UP_SESSION_TOKEN_LEN);
    up_session_t__slot->char__token[UP_SESSION_TOKEN_LEN] = '\0';
    up_session_t__slot->uint8_t__userIndex = uint8_t__userIndex;
    up_session_t__slot->uint8_t__role = UP_USER_T__A__Users[uint8_t__userIndex].uint8_t__role;
    up_session_t__slot->uint32_t__lastSeenMs = (uint32_t)millis();
    up_session_t__slot->uint32_t__startedEpoch = func__UpState_NowEpochS();

    return true;
}

/**
 * @brief  [EN] Find a live session by token; expired slots are freed on the way.
 *         [FA] یافتن نشست زنده با توکن؛ خانه‌های منقضی همان‌جا آزاد می‌شوند.
 * @param  char__token [EN] cookie value, 32 hex chars / [FA] مقدار کوکی
 * @return [EN] session pointer or NULL / [FA] اشاره‌گر نشست یا NULL
 */
static up_session_t *func__UpAuth_FindSession(const char *char__token)
{
    if ((char__token == NULL) || (strlen(char__token) != UP_SESSION_TOKEN_LEN))
    {
        return NULL;
    }

    for (uint8_t uint8_t__i = 0u; uint8_t__i < UP_SESSION_MAX; uint8_t__i++)
    {
        up_session_t *up_session_t__session = &UP_SESSION_T__A__Sessions[uint8_t__i];

        if (!up_session_t__session->bool__used)
        {
            continue;
        }

        uint32_t uint32_t__idleMs = (uint32_t)(millis() - up_session_t__session->uint32_t__lastSeenMs);
        if (uint32_t__idleMs > (UP_SESSION_IDLE_S * 1000u))
        {
            up_session_t__session->bool__used = false;
            continue;
        }

        if (strncmp(up_session_t__session->char__token, char__token, UP_SESSION_TOKEN_LEN) == 0)
        {
            up_session_t__session->uint32_t__lastSeenMs = (uint32_t)millis();
            return up_session_t__session;
        }
    }

    return NULL;
}

/* ==================== Login / ورود ==================== */
/**
 * @brief  [EN] Verify credentials and open a session. A small fail counter with
 *              a cool-down makes an online guessing loop pointless while never
 *              locking a legitimate operator out for long.
 *         [FA] بررسی نام و گذرواژه و باز کردن نشست. شمارندهٔ خطای کوچک با
 *              خنک‌سازی، حلقهٔ حدس زدن آنلاین را بی‌فایده می‌کند بدون اینکه
 *              اپراتور واقعی را طولانی بیرون نگه دارد.
 * @param  char__name [EN] login name / [FA] نام ورود
 * @param  char__pass [EN] password / [FA] گذرواژه
 * @param  char__tokenOut [EN] session token buffer / [FA] بافر توکن
 * @param  uint8_t__roleOut [EN] role of the user / [FA] نقش کاربر
 * @param  bool__mustChangeOut [EN] forced change flag / [FA] پرچم اجبار تغییر
 * @return [EN] true on success / [FA] در صورت موفقیت true
 */
static bool func__UpAuth_Login(const char *char__name, const char *char__pass, char *char__tokenOut,
                               uint8_t *uint8_t__roleOut, bool *bool__mustChangeOut)
{
    uint8_t uint8_t__digest[UP_HASH_BYTES];

    if ((uint32_t)millis() < UINT32_T__G__LoginLockUntilMs)
    {
        return false;
    }

    int8_t int8_t__index = func__UpAuth_FindUser(char__name);
    if (int8_t__index < 0)
    {
        UINT8_T__G__LoginFails++;
        if (UINT8_T__G__LoginFails >= UP_LOGIN_FAIL_MAX)
        {
            UINT8_T__G__LoginFails = 0u;
            UINT32_T__G__LoginLockUntilMs = (uint32_t)millis() + UP_LOGIN_LOCK_MS;
        }
        return false;
    }

    up_user_t *up_user_t__user = &UP_USER_T__A__Users[(uint8_t)int8_t__index];
    func__UpAuth_Digest(up_user_t__user->uint8_t__salt, char__pass, uint8_t__digest);

    if (!func__UpAuth_DigestEquals(uint8_t__digest, up_user_t__user->uint8_t__digest))
    {
        UINT8_T__G__LoginFails++;
        if (UINT8_T__G__LoginFails >= UP_LOGIN_FAIL_MAX)
        {
            UINT8_T__G__LoginFails = 0u;
            UINT32_T__G__LoginLockUntilMs = (uint32_t)millis() + UP_LOGIN_LOCK_MS;
        }
        return false;
    }

    UINT8_T__G__LoginFails = 0u;
    up_user_t__user->uint32_t__lastLoginEpoch = func__UpState_NowEpochS();
    (void)func__UpAuth_Save();

    *uint8_t__roleOut = up_user_t__user->uint8_t__role;
    *bool__mustChangeOut = (up_user_t__user->uint8_t__mustChange != 0u);
    return func__UpAuth_OpenSession((uint8_t)int8_t__index, char__tokenOut);
}

/* ==================== Role gates / دروازهٔ نقش ==================== */
/**
 * @brief  [EN] Does the current request's role satisfy a requirement?
 *         [FA] آیا نقش همین درخواست شرط لازم را برآورده می‌کند؟
 * @param  uint8_t__needed [EN] UP_ROLE_* minimum / [FA] کمینهٔ نقش
 * @return [EN] true when allowed / [FA] در صورت اجازه true
 */
static bool func__UpAuth_AtLeast(uint8_t uint8_t__needed)
{
    return (UINT8_T__G__CurrentRole >= uint8_t__needed);
}

/* ==================== Action log / گزارش اقدامات ==================== */
/**
 * @brief  [EN] Append one admin action to the persistent ring.
 *         [FA] افزودن یک اقدام مدیر به حلقهٔ ماندگار.
 * @param  char__user [EN] who did it / [FA] چه کسی انجام داد
 * @param  char__action [EN] short verb, <=19 chars / [FA] فعل کوتاه
 * @return [EN] None / [FA] ندارد
 */
static void func__UpAuth_LogAction(const char *char__user, const char *char__action)
{
    up_audit_t *up_audit_t__row = &UP_AUDIT_T__A__Log[UINT8_T__G__AuditHead];

    memset(up_audit_t__row, 0, sizeof(up_audit_t));
    up_audit_t__row->uint32_t__ageS = func__UpState_UptimeS();
    up_audit_t__row->uint32_t__epoch = func__UpState_NowEpochS();
    strncpy(up_audit_t__row->char__user, char__user, UP_NAME_MAX);
    strncpy(up_audit_t__row->char__action, char__action, sizeof(up_audit_t__row->char__action) - 1u);

    UINT8_T__G__AuditHead = (uint8_t)((UINT8_T__G__AuditHead + 1u) % UP_AUDIT_MAX);
    if (UINT8_T__G__AuditCount < UP_AUDIT_MAX)
    {
        UINT8_T__G__AuditCount++;
    }

    func__UpAuth_SaveAudit();
}

/**
 * @brief  [EN] Persist the action log (24 rows, ~1 KB: cheap and worth keeping).
 *         [FA] ذخیرهٔ گزارش اقدامات (۲۴ ردیف، حدود ۱ کیلوبایت: ارزان و ارزش نگه‌داشتن دارد).
 * @return [EN] true on success / [FA] در صورت موفقیت true
 */
static bool func__UpAuth_SaveAudit(void)
{
    File up_file_t__f;
    uint32_t uint32_t__magic = UP_AUDIT_MAGIC;
    uint8_t uint8_t__count = UINT8_T__G__AuditCount;
    uint8_t uint8_t__head = UINT8_T__G__AuditHead;

    LittleFS.remove(UP_F_AUDIT);
    up_file_t__f = LittleFS.open(UP_F_AUDIT, "w");
    if (!up_file_t__f)
    {
        return false;
    }

    (void)up_file_t__f.write((const uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic));
    (void)up_file_t__f.write(&uint8_t__count, sizeof(uint8_t__count));
    (void)up_file_t__f.write(&uint8_t__head, sizeof(uint8_t__head));
    (void)up_file_t__f.write((const uint8_t *)UP_AUDIT_T__A__Log, sizeof(UP_AUDIT_T__A__Log));
    up_file_t__f.close();

    return true;
}

/**
 * @brief  [EN] Load the action log at boot.
 *         [FA] خواندن گزارش اقدامات در بوت.
 * @return [EN] true when a valid log was loaded / [FA] در صورت خواندن معتبر true
 */
static bool func__UpAuth_LoadAudit(void)
{
    File up_file_t__f;
    uint32_t uint32_t__magic = 0u;
    uint8_t uint8_t__count = 0u;
    uint8_t uint8_t__head = 0u;

    memset(UP_AUDIT_T__A__Log, 0, sizeof(UP_AUDIT_T__A__Log));
    UINT8_T__G__AuditCount = 0u;
    UINT8_T__G__AuditHead = 0u;

    up_file_t__f = LittleFS.open(UP_F_AUDIT, "r");
    if (!up_file_t__f)
    {
        return false;
    }

    (void)up_file_t__f.read((uint8_t *)&uint32_t__magic, sizeof(uint32_t__magic));
    (void)up_file_t__f.read(&uint8_t__count, sizeof(uint8_t__count));
    (void)up_file_t__f.read(&uint8_t__head, sizeof(uint8_t__head));
    if (uint32_t__magic != UP_AUDIT_MAGIC)
    {
        up_file_t__f.close();
        return false;
    }

    size_t size_t__got = up_file_t__f.read((uint8_t *)UP_AUDIT_T__A__Log, sizeof(UP_AUDIT_T__A__Log));
    up_file_t__f.close();

    if (size_t__got != sizeof(UP_AUDIT_T__A__Log))
    {
        return false;
    }

    UINT8_T__G__AuditCount = (uint8_t__count <= UP_AUDIT_MAX) ? uint8_t__count : UP_AUDIT_MAX;
    UINT8_T__G__AuditHead = (uint8_t__head < UP_AUDIT_MAX) ? uint8_t__head : 0u;
    return true;
}

/**
 * @brief  [EN] Row i of the log, newest first.
 *         [FA] ردیف i گزارش، از جدید به قدیم.
 * @param  uint8_t__i [EN] 0 = newest / [FA] صفر = جدیدترین
 * @return [EN] pointer or NULL / [FA] اشاره‌گر یا NULL
 */
static up_audit_t *func__UpAuth_AuditAt(uint8_t uint8_t__i)
{
    if (uint8_t__i >= UINT8_T__G__AuditCount)
    {
        return NULL;
    }

    uint8_t uint8_t__index = (uint8_t)((UINT8_T__G__AuditHead + UP_AUDIT_MAX - 1u - uint8_t__i) % UP_AUDIT_MAX);
    return &UP_AUDIT_T__A__Log[uint8_t__index];
}

#endif /* UP_AUTH_H */
