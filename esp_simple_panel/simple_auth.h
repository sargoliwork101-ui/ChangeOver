/* simple_auth.h - مدیریت کاربر ساده: بیننده و ادمین
   فقط فارسی - ذخیره در LittleFS */

#include <LittleFS.h>

/* ==================== هش امن SHA256 با Salt - پیاده‌سازی سبک مستقل ==================== */
/* SHA256 سبک public domain - بدون وابستگی خارجی برای ESP8266/ESP32 */
typedef struct
{
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} simple_sha256_ctx_t;

#define SIMPLE_SHA256_ROTR(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define SIMPLE_SHA256_CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define SIMPLE_SHA256_MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIMPLE_SHA256_EP0(x) (SIMPLE_SHA256_ROTR(x,2) ^ SIMPLE_SHA256_ROTR(x,13) ^ SIMPLE_SHA256_ROTR(x,22))
#define SIMPLE_SHA256_EP1(x) (SIMPLE_SHA256_ROTR(x,6) ^ SIMPLE_SHA256_ROTR(x,11) ^ SIMPLE_SHA256_ROTR(x,25))
#define SIMPLE_SHA256_SIG0(x) (SIMPLE_SHA256_ROTR(x,7) ^ SIMPLE_SHA256_ROTR(x,18) ^ ((x) >> 3))
#define SIMPLE_SHA256_SIG1(x) (SIMPLE_SHA256_ROTR(x,17) ^ SIMPLE_SHA256_ROTR(x,19) ^ ((x) >> 10))

static const uint32_t UINT32_T__G__Sha256K[64] = {
 0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
 0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
 0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
 0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
 0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
 0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
 0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
 0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffaU,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static void func__Simple_Sha256Transform(simple_sha256_ctx_t *ctx, const uint8_t data[])
{
    uint32_t a,b,c,d,e,f,g,h,i,j,t1,t2,m[64];
    for (i=0u,j=0u; i<16u; ++i, j+=4u) m[i] = (uint32_t)(data[j] << 24) | (uint32_t)(data[j+1] << 16) | (uint32_t)(data[j+2] << 8) | (uint32_t)(data[j+3]);
    for (; i<64u; ++i) m[i] = SIMPLE_SHA256_SIG1(m[i-2u]) + m[i-7u] + SIMPLE_SHA256_SIG0(m[i-15u]) + m[i-16u];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (i=0u; i<64u; ++i) { t1 = h + SIMPLE_SHA256_EP1(e) + SIMPLE_SHA256_CH(e,f,g) + UINT32_T__G__Sha256K[i] + m[i]; t2 = SIMPLE_SHA256_EP0(a) + SIMPLE_SHA256_MAJ(a,b,c); h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2; }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}
static void func__Simple_Sha256Init(simple_sha256_ctx_t *ctx) { ctx->datalen=0u; ctx->bitlen=0u; ctx->state[0]=0x6a09e667u; ctx->state[1]=0xbb67ae85u; ctx->state[2]=0x3c6ef372u; ctx->state[3]=0xa54ff53au; ctx->state[4]=0x510e527fu; ctx->state[5]=0x9b05688cu; ctx->state[6]=0x1f83d9abu; ctx->state[7]=0x5be0cd19u; }
static void func__Simple_Sha256Update(simple_sha256_ctx_t *ctx, const uint8_t *data, size_t len) { for (size_t i=0u;i<len;++i){ ctx->data[ctx->datalen]=data[i]; ctx->datalen++; if(ctx->datalen==64u){ func__Simple_Sha256Transform(ctx,ctx->data); ctx->bitlen+=512u; ctx->datalen=0u; } } }
static void func__Simple_Sha256Final(simple_sha256_ctx_t *ctx, uint8_t hash[32]) { uint32_t i=ctx->datalen; if(ctx->datalen<56u){ ctx->data[i++]=0x80u; while(i<56u) ctx->data[i++]=0x00u; } else { ctx->data[i++]=0x80u; while(i<64u) ctx->data[i++]=0x00u; func__Simple_Sha256Transform(ctx,ctx->data); memset(ctx->data,0,56); } ctx->bitlen+=ctx->datalen*8u; ctx->data[63]=(uint8_t)(ctx->bitlen); ctx->data[62]=(uint8_t)(ctx->bitlen>>8); ctx->data[61]=(uint8_t)(ctx->bitlen>>16); ctx->data[60]=(uint8_t)(ctx->bitlen>>24); ctx->data[59]=(uint8_t)(ctx->bitlen>>32); ctx->data[58]=(uint8_t)(ctx->bitlen>>40); ctx->data[57]=(uint8_t)(ctx->bitlen>>48); ctx->data[56]=(uint8_t)(ctx->bitlen>>56); func__Simple_Sha256Transform(ctx,ctx->data); for(i=0u;i<4u;++i){ hash[i]=(uint8_t)((ctx->state[0]>> (24-i*8)) & 0xFFu); hash[i+4]=(uint8_t)((ctx->state[1]>> (24-i*8)) & 0xFFu); hash[i+8]=(uint8_t)((ctx->state[2]>> (24-i*8)) & 0xFFu); hash[i+12]=(uint8_t)((ctx->state[3]>> (24-i*8)) & 0xFFu); hash[i+16]=(uint8_t)((ctx->state[4]>> (24-i*8)) & 0xFFu); hash[i+20]=(uint8_t)((ctx->state[5]>> (24-i*8)) & 0xFFu); hash[i+24]=(uint8_t)((ctx->state[6]>> (24-i*8)) & 0xFFu); hash[i+28]=(uint8_t)((ctx->state[7]>> (24-i*8)) & 0xFFu); } }

static void func__Simple_HashPass(const char *char__ptr_pass, char *char__ptr_out)
{
    const char *char__ptr_salt = "ChangeOver2026_Salt_";
    char char__buf[96];
    (void)snprintf(char__buf, sizeof(char__buf), "%s%s", char__ptr_salt, char__ptr_pass);
    uint8_t uint8_t__hash[32];
    simple_sha256_ctx_t ctx;
    func__Simple_Sha256Init(&ctx);
    func__Simple_Sha256Update(&ctx, (const uint8_t*)char__buf, strlen(char__buf));
    func__Simple_Sha256Final(&ctx, uint8_t__hash);
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < 32u; uint8_t__i++)
    {
        (void)snprintf(&char__ptr_out[uint8_t__i * 2u], 3, "%02x", uint8_t__hash[uint8_t__i]);
    }
    char__ptr_out[64] = '\0';
}

/* بررسی هش قدیمی ۸ کاراکتری برای مهاجرت */
static bool func__Simple_IsOldHash(const char *char__ptr_hash)
{
    return (strlen(char__ptr_hash) == 8u);
}

/* ==================== بارگذاری کاربران ==================== */
static void func__Simple_AuthLoad(void)
{
    uint8_t uint8_t__index;
    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_USERS_MAX; uint8_t__index++)
    {
        SIMPLE_USER_T__G__Users[uint8_t__index].active = false;
    }
    UINT8_T__G__UserCount = 0u;

    if (!BOOL__G__FsOk)
    {
        return;
    }

    if (!LittleFS.exists(SIMPLE_USERS_FILE))
    {
        /* کاربر پیش‌فرض: admin / admin نقش ادمین */
        (void)strncpy(SIMPLE_USER_T__G__Users[0].name, "admin", SIMPLE_USER_NAME_MAX);
        func__Simple_HashPass("admin", SIMPLE_USER_T__G__Users[0].passHash);
        SIMPLE_USER_T__G__Users[0].role = 1u;
        SIMPLE_USER_T__G__Users[0].active = true;
        UINT8_T__G__UserCount = 1u;

        File file__f = LittleFS.open(SIMPLE_USERS_FILE, "w");
        if (file__f)
        {
            file__f.print("{\"users\":[{\"name\":\"admin\",\"pass\":\"");
            file__f.print(SIMPLE_USER_T__G__Users[0].passHash);
            file__f.print("\",\"role\":1}]}");
            file__f.close();
        }
        return;
    }

    // بررسی مهاجرت از هش قدیمی
    {
        File file__check = LittleFS.open(SIMPLE_USERS_FILE, "r");
        if (file__check)
        {
            String string__check = file__check.readString();
            file__check.close();
            // اگر هش قدیمی ۸ کاراکتری پیدا شد، فایل را پاک و دوباره بساز
            if (string__check.indexOf("\"pass\":\"") >= 0)
            {
                int32_t int32_t__p = string__check.indexOf("\"pass\":\"");
                int32_t__p += 8;
                int32_t int32_t__e = string__check.indexOf("\"", int32_t__p);
                if (int32_t__e > int32_t__p)
                {
                    String string__h = string__check.substring(int32_t__p, int32_t__e);
                    if (string__h.length() == 8u)
                    {
                        // هش قدیمی - پاک کن
                        LittleFS.remove(SIMPLE_USERS_FILE);
                        (void)strncpy(SIMPLE_USER_T__G__Users[0].name, "admin", SIMPLE_USER_NAME_MAX);
                        func__Simple_HashPass("admin", SIMPLE_USER_T__G__Users[0].passHash);
                        SIMPLE_USER_T__G__Users[0].role = 1u;
                        SIMPLE_USER_T__G__Users[0].active = true;
                        UINT8_T__G__UserCount = 1u;
                        File file__f = LittleFS.open(SIMPLE_USERS_FILE, "w");
                        if (file__f)
                        {
                            file__f.print("{\"users\":[{\"name\":\"admin\",\"pass\":\"");
                            file__f.print(SIMPLE_USER_T__G__Users[0].passHash);
                            file__f.print("\",\"role\":1}]}");
                            file__f.close();
                        }
                        return;
                    }
                }
            }
        }
    }

    File file__f = LittleFS.open(SIMPLE_USERS_FILE, "r");
    if (!file__f)
    {
        return;
    }
    String string__content = file__f.readString();
    file__f.close();

    /* پارس خیلی ساده: دنبال \"name\":\" و \"pass\":\" و \"role\": */
    int32_t int32_t__pos = 0;
    uint8_t uint8_t__found = 0u;
    while (uint8_t__found < SIMPLE_USERS_MAX)
    {
        int32_t int32_t__namePos = string__content.indexOf("\"name\":\"", int32_t__pos);
        if (int32_t__namePos < 0) break;
        int32_t__namePos += 8;
        int32_t int32_t__nameEnd = string__content.indexOf("\"", int32_t__namePos);
        if (int32_t__nameEnd < 0) break;
        String string__name = string__content.substring(int32_t__namePos, int32_t__nameEnd);

        int32_t int32_t__passPos = string__content.indexOf("\"pass\":\"", int32_t__nameEnd);
        if (int32_t__passPos < 0) break;
        int32_t__passPos += 8;
        int32_t int32_t__passEnd = string__content.indexOf("\"", int32_t__passPos);
        if (int32_t__passEnd < 0) break;
        String string__pass = string__content.substring(int32_t__passPos, int32_t__passEnd);

        int32_t int32_t__rolePos = string__content.indexOf("\"role\":", int32_t__passEnd);
        if (int32_t__rolePos < 0) break;
        int32_t__rolePos += 7;
        int32_t int32_t__roleEnd = string__content.indexOf("}", int32_t__rolePos);
        if (int32_t__roleEnd < 0) int32_t__roleEnd = string__content.length();
        String string__role = string__content.substring(int32_t__rolePos, int32_t__roleEnd);
        int32_t int32_t__role = string__role.toInt();

        if (string__name.length() > 0 && string__name.length() <= SIMPLE_USER_NAME_MAX)
        {
            (void)strncpy(SIMPLE_USER_T__G__Users[uint8_t__found].name, string__name.c_str(), SIMPLE_USER_NAME_MAX);
            (void)strncpy(SIMPLE_USER_T__G__Users[uint8_t__found].passHash, string__pass.c_str(), 64);
            SIMPLE_USER_T__G__Users[uint8_t__found].role = (uint8_t)int32_t__role;
            SIMPLE_USER_T__G__Users[uint8_t__found].active = true;
            uint8_t__found++;
        }
        int32_t__pos = int32_t__roleEnd;
    }
    UINT8_T__G__UserCount = uint8_t__found;
}

/* ==================== ذخیره کاربران ==================== */
static void func__Simple_AuthSave(void)
{
    if (!BOOL__G__FsOk) return;
    File file__f = LittleFS.open(SIMPLE_USERS_FILE, "w");
    if (!file__f) return;
    file__f.print("{\"users\":[");
    bool bool__first = true;
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (!SIMPLE_USER_T__G__Users[uint8_t__i].active) continue;
        if (!bool__first) file__f.print(",");
        file__f.print("{\"name\":\"");
        file__f.print(SIMPLE_USER_T__G__Users[uint8_t__i].name);
        file__f.print("\",\"pass\":\"");
        file__f.print(SIMPLE_USER_T__G__Users[uint8_t__i].passHash);
        file__f.print("\",\"role\":");
        file__f.print(SIMPLE_USER_T__G__Users[uint8_t__i].role);
        file__f.print("}");
        bool__first = false;
    }
    file__f.print("]}");
    file__f.close();
}

/* ==================== ورود - با IP سشن ==================== */
static bool func__Simple_AuthLogin(const char *char__ptr_name, const char *char__ptr_pass)
{
    char char__hash[65];
    func__Simple_HashPass(char__ptr_pass, char__hash);
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (!SIMPLE_USER_T__G__Users[uint8_t__i].active) continue;
        if (strcmp(SIMPLE_USER_T__G__Users[uint8_t__i].name, char__ptr_name) == 0)
        {
            bool bool__match = false;
            if (func__Simple_IsOldHash(SIMPLE_USER_T__G__Users[uint8_t__i].passHash))
            {
                // Fallback قدیمی برای مهاجرت
                uint32_t uint32_t__oldHash = 5381u;
                uint8_t uint8_t__j = 0u;
                while (char__ptr_pass[uint8_t__j] != '\0') { uint32_t__oldHash = ((uint32_t__oldHash << 5) + uint32_t__oldHash) + (uint32_t)char__ptr_pass[uint8_t__j]; uint8_t__j++; }
                char char__oldHashStr[16];
                (void)snprintf(char__oldHashStr, sizeof(char__oldHashStr), "%08lX", (unsigned long)uint32_t__oldHash);
                if (strcmp(SIMPLE_USER_T__G__Users[uint8_t__i].passHash, char__oldHashStr) == 0) bool__match = true;
            }
            else
            {
                if (strcmp(SIMPLE_USER_T__G__Users[uint8_t__i].passHash, char__hash) == 0) bool__match = true;
            }

            if (bool__match)
            {
                (void)strncpy(CHAR__G__CurrentUser, char__ptr_name, SIMPLE_USER_NAME_MAX);
                UINT8_T__G__CurrentRole = SIMPLE_USER_T__G__Users[uint8_t__i].role;
                UINT32_T__G__SessionStartMs = (uint32_t)millis();
                // ذخیره IP کلاینت برای سشن
                IPAddress ip__client = SIMPLE_WEB_SERVER_T__G__Server.client().remoteIP();
                (void)snprintf(CHAR__G__SessionIP, sizeof(CHAR__G__SessionIP), "%u.%u.%u.%u", ip__client[0], ip__client[1], ip__client[2], ip__client[3]);
                return true;
            }
        }
    }
    return false;
}

/* ==================== خروج ==================== */
static void func__Simple_AuthLogout(void)
{
    CHAR__G__CurrentUser[0] = '\0';
    UINT8_T__G__CurrentRole = 0u;
    UINT32_T__G__SessionStartMs = 0u;
    CHAR__G__SessionIP[0] = '\0';
}

/* ==================== چک سشن - با IP ==================== */
static bool func__Simple_SessionValid(void)
{
    if (CHAR__G__CurrentUser[0] == '\0') return false;
    uint32_t uint32_t__now = (uint32_t)millis();
    if ((uint32_t__now - UINT32_T__G__SessionStartMs) > SIMPLE_SESSION_TIMEOUT_MS)
    {
        func__Simple_AuthLogout();
        return false;
    }
    // بررسی IP سشن - اگر IP عوض شده، سشن نامعتبر (جلوگیری از hijack ساده)
    IPAddress ip__now = SIMPLE_WEB_SERVER_T__G__Server.client().remoteIP();
    char char__ipNow[24];
    (void)snprintf(char__ipNow, sizeof(char__ipNow), "%u.%u.%u.%u", ip__now[0], ip__now[1], ip__now[2], ip__now[3]);
    // اگر IP ذخیره شده خالی نیست و متفاوت است، سشن را باطل کن (به جز 0.0.0.0 در تست)
    if (CHAR__G__SessionIP[0] != '\0' && strcmp(CHAR__G__SessionIP, char__ipNow) != 0)
    {
        // در حالت AP معمولاً IP ثابت 192.168.4.x است - اگر عوض شد، لاگ اوت
        // برای سادگی در AP، فقط اگر هر دو IP معتبر باشند چک کن
        if (ip__now[0] != 0u)
        {
            func__Simple_AuthLogout();
            return false;
        }
    }
    return true;
}

/* ==================== آیا ادمین ==================== */
static bool func__Simple_IsAdmin(void)
{
    if (!func__Simple_SessionValid()) return false;
    return (UINT8_T__G__CurrentRole == 1u);
}

/* ==================== افزودن کاربر ==================== */
static bool func__Simple_AddUser(const char *char__ptr_name, const char *char__ptr_pass, uint8_t uint8_t__role)
{
    if (!func__Simple_IsAdmin()) return false;
    if (strlen(char__ptr_name) == 0u || strlen(char__ptr_name) > SIMPLE_USER_NAME_MAX) return false;
    if (strlen(char__ptr_pass) == 0u || strlen(char__ptr_pass) > SIMPLE_USER_PASS_MAX) return false;

    /* اگر وجود دارد آپدیت کن */
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (SIMPLE_USER_T__G__Users[uint8_t__i].active &&
            strcmp(SIMPLE_USER_T__G__Users[uint8_t__i].name, char__ptr_name) == 0)
        {
            func__Simple_HashPass(char__ptr_pass, SIMPLE_USER_T__G__Users[uint8_t__i].passHash);
            SIMPLE_USER_T__G__Users[uint8_t__i].role = uint8_t__role;
            func__Simple_AuthSave();
            return true;
        }
    }

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (!SIMPLE_USER_T__G__Users[uint8_t__i].active)
        {
            (void)strncpy(SIMPLE_USER_T__G__Users[uint8_t__i].name, char__ptr_name, SIMPLE_USER_NAME_MAX);
            func__Simple_HashPass(char__ptr_pass, SIMPLE_USER_T__G__Users[uint8_t__i].passHash);
            SIMPLE_USER_T__G__Users[uint8_t__i].role = uint8_t__role;
            SIMPLE_USER_T__G__Users[uint8_t__i].active = true;
            if (uint8_t__i >= UINT8_T__G__UserCount) UINT8_T__G__UserCount = (uint8_t)(uint8_t__i + 1u);
            func__Simple_AuthSave();
            return true;
        }
    }
    return false;
}

/* ==================== حذف کاربر ==================== */
static bool func__Simple_DeleteUser(const char *char__ptr_name)
{
    if (!func__Simple_IsAdmin()) return false;
    if (strcmp(char__ptr_name, "admin") == 0) return false; /* ادمین اصلی حذف نشود */
    uint8_t uint8_t__i;
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (SIMPLE_USER_T__G__Users[uint8_t__i].active &&
            strcmp(SIMPLE_USER_T__G__Users[uint8_t__i].name, char__ptr_name) == 0)
        {
            SIMPLE_USER_T__G__Users[uint8_t__i].active = false;
            func__Simple_AuthSave();
            return true;
        }
    }
    return false;
}
