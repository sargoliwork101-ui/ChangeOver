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

/* ==================== رمزنگاری فایل‌های حساس ====================
   هدف: رمزها روی فلش خوانا نباشند و کسی نتواند فایل را دست‌کاری کند.
   ساختار:
     ۱) کلید = SHA256(salt || شناسهٔ چیپ) - یکتا برای هر برد، جایی ذخیره نمی‌شود
     ۲) دو زیرکلید از همان کلید: HMAC(K,"enc") برای رمز و HMAC(K,"mac") برای برچسب
     ۳) رمز: هر بلوک ۳۲ بایتی = HMAC(Kenc, nonce||شمارهٔ بلوک) و XOR با متن
     ۴) برچسب = HMAC(Kmac, nonce||متن رمزشده) - کوتاه‌شده به SIMPLE_CRYPTO_TAG_LEN
   قالب فایل: {"e":1,"n":nonce,"c":"هگز متن رمزشده","m":"هگز برچسب"}
   اگر فایل قالب قدیمی (خوانا) باشد، یک‌بار خوانده و بلافاصله رمزشده بازنویسی
   می‌شود تا هیچ‌وقت رمز خوانا روی فلش نماند. */

/* ==================== شناسهٔ یکتای چیپ ==================== */
static uint32_t func__Simple_ChipId(void)
{
#if defined(ESP8266)
    return (uint32_t)ESP.getChipId();
#else
    return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu);
#endif
}

/* ==================== HMAC-SHA256 (افزایشی) ====================
   استاندارد RFC 2104 روی همان SHA256 این فایل. افزایشی است چون برچسب باید
   هم روی nonce و هم روی متن رمزشده حساب شود و نیازی به بافر میانی نیست. */
typedef struct
{
    uint8_t key[64];
    uint8_t pad[64];
    simple_sha256_ctx_t ctx;
} simple_hmac_ctx_t;

static void func__Simple_HmacInit(simple_hmac_ctx_t *simple_hmac_ctx_t__ptr_c,
                                  const uint8_t *uint8_t__ptr_key, size_t size_t__keyLen)
{
    uint8_t uint8_t__i;

    memset(simple_hmac_ctx_t__ptr_c->key, 0, sizeof(simple_hmac_ctx_t__ptr_c->key));
    if (size_t__keyLen > 64u)
    {
        /* کلید بلندتر از بلوک اول هش می‌شود (اینجا پیش نمی‌آید ولی طبق استاندارد) */
        func__Simple_Sha256Init(&simple_hmac_ctx_t__ptr_c->ctx);
        func__Simple_Sha256Update(&simple_hmac_ctx_t__ptr_c->ctx, uint8_t__ptr_key, size_t__keyLen);
        func__Simple_Sha256Final(&simple_hmac_ctx_t__ptr_c->ctx, simple_hmac_ctx_t__ptr_c->key);
    }
    else
    {
        memcpy(simple_hmac_ctx_t__ptr_c->key, uint8_t__ptr_key, size_t__keyLen);
    }

    for (uint8_t__i = 0u; uint8_t__i < 64u; uint8_t__i++)
    {
        simple_hmac_ctx_t__ptr_c->pad[uint8_t__i] = (uint8_t)(simple_hmac_ctx_t__ptr_c->key[uint8_t__i] ^ 0x36u);
    }
    func__Simple_Sha256Init(&simple_hmac_ctx_t__ptr_c->ctx);
    func__Simple_Sha256Update(&simple_hmac_ctx_t__ptr_c->ctx, simple_hmac_ctx_t__ptr_c->pad, 64u);
}

static void func__Simple_HmacUpdate(simple_hmac_ctx_t *simple_hmac_ctx_t__ptr_c,
                                    const uint8_t *uint8_t__ptr_data, size_t size_t__len)
{
    func__Simple_Sha256Update(&simple_hmac_ctx_t__ptr_c->ctx, uint8_t__ptr_data, size_t__len);
}

static void func__Simple_HmacFinal(simple_hmac_ctx_t *simple_hmac_ctx_t__ptr_c, uint8_t uint8_t__ptr_out[32])
{
    uint8_t uint8_t__inner[32];
    uint8_t uint8_t__i;

    func__Simple_Sha256Final(&simple_hmac_ctx_t__ptr_c->ctx, uint8_t__inner);
    for (uint8_t__i = 0u; uint8_t__i < 64u; uint8_t__i++)
    {
        simple_hmac_ctx_t__ptr_c->pad[uint8_t__i] = (uint8_t)(simple_hmac_ctx_t__ptr_c->key[uint8_t__i] ^ 0x5Cu);
    }
    func__Simple_Sha256Init(&simple_hmac_ctx_t__ptr_c->ctx);
    func__Simple_Sha256Update(&simple_hmac_ctx_t__ptr_c->ctx, simple_hmac_ctx_t__ptr_c->pad, 64u);
    func__Simple_Sha256Update(&simple_hmac_ctx_t__ptr_c->ctx, uint8_t__inner, 32u);
    func__Simple_Sha256Final(&simple_hmac_ctx_t__ptr_c->ctx, uint8_t__ptr_out);
}

/* ==================== HMAC یک‌مرحله‌ای (برای بلوک‌های کوتاه) ==================== */
static void func__Simple_HmacSha256(const uint8_t *uint8_t__ptr_key, size_t size_t__keyLen,
                                    const uint8_t *uint8_t__ptr_msg, size_t size_t__msgLen,
                                    uint8_t uint8_t__ptr_out[32])
{
    simple_hmac_ctx_t simple_hmac_ctx_t__c;

    func__Simple_HmacInit(&simple_hmac_ctx_t__c, uint8_t__ptr_key, size_t__keyLen);
    func__Simple_HmacUpdate(&simple_hmac_ctx_t__c, uint8_t__ptr_msg, size_t__msgLen);
    func__Simple_HmacFinal(&simple_hmac_ctx_t__c, uint8_t__ptr_out);
}

/* ==================== ساخت کلید از شناسهٔ چیپ ====================
   کلید = SHA256(salt || چهار بایت شناسهٔ چیپ) - یکتا برای هر برد. */
static void func__Simple_CryptoKeyInit(void)
{
    uint8_t uint8_t__seed[4];
    uint8_t uint8_t__i;
    uint32_t uint32_t__chip = func__Simple_ChipId();
    simple_sha256_ctx_t ctx;

    for (uint8_t__i = 0u; uint8_t__i < 4u; uint8_t__i++)
    {
        uint8_t__seed[uint8_t__i] = (uint8_t)((uint32_t__chip >> (8u * (3u - uint8_t__i))) & 0xFFu);
    }
    func__Simple_Sha256Init(&ctx);
    func__Simple_Sha256Update(&ctx, (const uint8_t *)SIMPLE_CRYPTO_SALT, (size_t)strlen(SIMPLE_CRYPTO_SALT));
    func__Simple_Sha256Update(&ctx, uint8_t__seed, 4u);
    func__Simple_Sha256Final(&ctx, UINT8_T__G__CryptoKey);

    UINT32_T__G__CryptoNonce = uint32_t__chip ^ (uint32_t)millis() ^ (uint32_t)micros();
    BOOL__G__CryptoKeyReady = true;
}

/* ==================== زیرکلید enc / mac ==================== */
static void func__Simple_CryptoSubKey(const char *char__ptr_label, uint8_t uint8_t__ptr_out[32])
{
    func__Simple_HmacSha256(UINT8_T__G__CryptoKey, SIMPLE_CRYPTO_KEY_LEN,
                            (const uint8_t *)char__ptr_label, strlen(char__ptr_label),
                            uint8_t__ptr_out);
}

/* ==================== بلوک جریان کلید ====================
   بلوک n = HMAC(Kenc, nonce || n) - چون HMAC خروجی‌اش تصادفی و یکتاست،
   XOR با آن یک رمز جریانی درست می‌دهد (بدون نیاز به AES). */
static void func__Simple_CryptoBlock(const uint8_t uint8_t__ptr_encKey[32], uint32_t uint32_t__nonce,
                                     uint32_t uint32_t__blockIndex, uint8_t uint8_t__ptr_out[32])
{
    uint8_t uint8_t__msg[8];
    uint8_t uint8_t__i;

    for (uint8_t__i = 0u; uint8_t__i < 4u; uint8_t__i++)
    {
        uint8_t__msg[uint8_t__i] = (uint8_t)((uint32_t__nonce >> (8u * (3u - uint8_t__i))) & 0xFFu);
        uint8_t__msg[uint8_t__i + 4u] = (uint8_t)((uint32_t__blockIndex >> (8u * (3u - uint8_t__i))) & 0xFFu);
    }
    func__Simple_HmacSha256(uint8_t__ptr_encKey, 32u, uint8_t__msg, 8u, uint8_t__ptr_out);
}

/* ==================== بایت به هگز ==================== */
static void func__Simple_CryptoToHex(const uint8_t *uint8_t__ptr_in, size_t size_t__len, char *char__ptr_out)
{
    static const char char__hexDigits[] = "0123456789abcdef";
    size_t size_t__i;

    for (size_t__i = 0u; size_t__i < size_t__len; size_t__i++)
    {
        char__ptr_out[size_t__i * 2u] = char__hexDigits[(uint8_t__ptr_in[size_t__i] >> 4) & 0x0Fu];
        char__ptr_out[(size_t__i * 2u) + 1u] = char__hexDigits[uint8_t__ptr_in[size_t__i] & 0x0Fu];
    }
    char__ptr_out[size_t__len * 2u] = '\0';
}

/* ==================== هگز به بایت (۱=موفق) ==================== */
static bool func__Simple_CryptoFromHex(const char *char__ptr_hex, uint8_t *uint8_t__ptr_out, size_t size_t__len)
{
    size_t size_t__i;

    for (size_t__i = 0u; size_t__i < size_t__len; size_t__i++)
    {
        uint8_t uint8_t__hi;
        uint8_t uint8_t__lo;
        char char__hi = char__ptr_hex[size_t__i * 2u];
        char char__lo = char__ptr_hex[(size_t__i * 2u) + 1u];

        if (char__hi == '\0' || char__lo == '\0') return false;
        if (char__hi >= '0' && char__hi <= '9') uint8_t__hi = (uint8_t)(char__hi - '0');
        else if (char__hi >= 'a' && char__hi <= 'f') uint8_t__hi = (uint8_t)(char__hi - 'a' + 10);
        else return false;
        if (char__lo >= '0' && char__lo <= '9') uint8_t__lo = (uint8_t)(char__lo - '0');
        else if (char__lo >= 'a' && char__lo <= 'f') uint8_t__lo = (uint8_t)(char__lo - 'a' + 10);
        else return false;
        uint8_t__ptr_out[size_t__i] = (uint8_t)((uint8_t__hi << 4) | uint8_t__lo);
    }
    return true;
}

/* ==================== برچسب فایل ====================
   HMAC(Kmac, nonce || بایت‌های متن رمزشده) - روی خودِ متن رمزشده، پس هر
   دست‌کاری در فایل برچسب را باطل می‌کند. */
static void func__Simple_CryptoTag(const uint8_t uint8_t__ptr_macKey[32], uint32_t uint32_t__nonce,
                                   const String &string__hex, uint8_t uint8_t__ptr_out[32])
{
    simple_hmac_ctx_t simple_hmac_ctx_t__c;
    uint8_t uint8_t__nonceBytes[4];
    size_t size_t__i;
    size_t size_t__len = string__hex.length();

    for (size_t__i = 0u; size_t__i < 4u; size_t__i++)
    {
        uint8_t__nonceBytes[size_t__i] = (uint8_t)((uint32_t__nonce >> (8u * (3u - size_t__i))) & 0xFFu);
    }
    func__Simple_HmacInit(&simple_hmac_ctx_t__c, uint8_t__ptr_macKey, 32u);
    func__Simple_HmacUpdate(&simple_hmac_ctx_t__c, uint8_t__nonceBytes, 4u);
    for (size_t__i = 0u; size_t__i + 1u < size_t__len; size_t__i += 2u)
    {
        uint8_t uint8_t__pair[1];
        char char__two[3];
        char__two[0] = string__hex[size_t__i];
        char__two[1] = string__hex[size_t__i + 1u];
        char__two[2] = '\0';
        if (!func__Simple_CryptoFromHex(char__two, uint8_t__pair, 1u)) return;
        func__Simple_HmacUpdate(&simple_hmac_ctx_t__c, uint8_t__pair, 1u);
    }
    func__Simple_HmacFinal(&simple_hmac_ctx_t__c, uint8_t__ptr_out);
}

/* ==================== رمزگذاری متن ====================
   خروجی: {"e":1,"n":nonce,"c":"هگز","m":"هگز برچسب"}
   اگر کلید ساخته نشده باشد، متن دست‌نخورده پس داده می‌شود؛ آن‌وقت فایل مثل
   قالب قدیمی خوانده می‌شود و چیزی از دست نمی‌رود. */
static void func__Simple_CryptoEncrypt(const String &string__plain, String &string__out)
{
    uint8_t uint8_t__encKey[32];
    uint8_t uint8_t__macKey[32];
    uint8_t uint8_t__block[32];
    uint8_t uint8_t__mac[32];
    uint32_t uint32_t__nonce;
    size_t size_t__len = string__plain.length();
    size_t size_t__i;
    String string__hex;
    char char__hex[40];

    if (!BOOL__G__CryptoKeyReady)
    {
        string__out = string__plain;
        return;
    }

    func__Simple_CryptoSubKey("enc", uint8_t__encKey);
    func__Simple_CryptoSubKey("mac", uint8_t__macKey);

    /* هر نوشتن یک مقدار تازه می‌گیرد تا جریان کلید دوباره تکرار نشود */
    UINT32_T__G__CryptoNonce = (UINT32_T__G__CryptoNonce + 1u) ^ (uint32_t)millis();
    uint32_t__nonce = UINT32_T__G__CryptoNonce;

    string__hex = "";
    string__hex.reserve((size_t__len * 2u) + 2u);
    for (size_t__i = 0u; size_t__i < size_t__len; size_t__i++)
    {
        uint8_t uint8_t__cipher;
        if ((size_t__i % 32u) == 0u)
        {
            func__Simple_CryptoBlock(uint8_t__encKey, uint32_t__nonce, (uint32_t)(size_t__i / 32u), uint8_t__block);
        }
        uint8_t__cipher = (uint8_t)(((uint8_t)string__plain[size_t__i]) ^ uint8_t__block[size_t__i % 32u]);
        func__Simple_CryptoToHex(&uint8_t__cipher, 1u, char__hex);
        string__hex += char__hex;
    }

    func__Simple_CryptoTag(uint8_t__macKey, uint32_t__nonce, string__hex, uint8_t__mac);

    string__out = "{\"e\":1,\"n\":";
    string__out += String(uint32_t__nonce);
    string__out += ",\"c\":\"";
    string__out += string__hex;
    string__out += "\",\"m\":\"";
    func__Simple_CryptoToHex(uint8_t__mac, SIMPLE_CRYPTO_TAG_LEN, char__hex);
    string__out += char__hex;
    string__out += "\"}";
}

/* ==================== رمزگشایی ====================
   ۰ = رمزنگاری‌شده ولی کلید/برچسب نخورد (فایل دست‌کاری‌شده),
   ۱ = قالب قدیمی خوانا، ۲ = رمزگشایی موفق */
static uint8_t func__Simple_CryptoDecrypt(const String &string__content, String &string__out)
{
    uint8_t uint8_t__encKey[32];
    uint8_t uint8_t__macKey[32];
    uint8_t uint8_t__mac[32];
    uint8_t uint8_t__block[32];
    uint8_t uint8_t__stored[SIMPLE_CRYPTO_TAG_LEN];
    uint32_t uint32_t__nonce;
    int32_t int32_t__nPos;
    int32_t int32_t__cPos;
    int32_t int32_t__cEnd;
    int32_t int32_t__mPos;
    int32_t int32_t__mEnd;
    int32_t int32_t__k;
    size_t size_t__index;
    String string__hex;

    if (!BOOL__G__CryptoKeyReady) return 0u;

    int32_t__nPos = string__content.indexOf("\"n\":");
    int32_t__cPos = string__content.indexOf("\"c\":\"");
    int32_t__mPos = string__content.indexOf("\"m\":\"");
    if (int32_t__nPos < 0 || int32_t__cPos < 0 || int32_t__mPos < 0)
    {
        return 1u;   /* قالب آشنا نیست - فایل قدیمی خوانا */
    }
    int32_t__nPos += 4;
    int32_t__cPos += 5;
    int32_t__cEnd = string__content.indexOf("\"", int32_t__cPos);
    int32_t__mPos += 5;
    int32_t__mEnd = string__content.indexOf("\"", int32_t__mPos);
    if (int32_t__cEnd <= int32_t__cPos || int32_t__mEnd <= int32_t__mPos) return 0u;

    uint32_t__nonce = (uint32_t)string__content.substring(int32_t__nPos,
        string__content.indexOf(",", int32_t__nPos)).toInt();
    string__hex = string__content.substring(int32_t__cPos, int32_t__cEnd);

    func__Simple_CryptoSubKey("enc", uint8_t__encKey);
    func__Simple_CryptoSubKey("mac", uint8_t__macKey);

    /* ۱) برچسب را دوباره حساب کن - نبودنش یعنی فایل دست‌کاری شده */
    func__Simple_CryptoTag(uint8_t__macKey, uint32_t__nonce, string__hex, uint8_t__mac);
    if (!func__Simple_CryptoFromHex(string__content.substring(int32_t__mPos, int32_t__mEnd).c_str(),
                                    uint8_t__stored, SIMPLE_CRYPTO_TAG_LEN))
    {
        return 0u;
    }
    if (memcmp(uint8_t__mac, uint8_t__stored, SIMPLE_CRYPTO_TAG_LEN) != 0) return 0u;

    /* ۲) رمزگشایی - هر بایت با جریان کلید XOR می‌شود */
    string__out = "";
    string__out.reserve((size_t)((int32_t__cEnd - int32_t__cPos) / 2));
    size_t__index = 0u;
    for (int32_t__k = int32_t__cPos; int32_t__k + 1 < int32_t__cEnd; int32_t__k += 2)
    {
        uint8_t uint8_t__cipher[1];
        char char__two[3];
        char__two[0] = string__content[int32_t__k];
        char__two[1] = string__content[int32_t__k + 1];
        char__two[2] = '\0';
        if (!func__Simple_CryptoFromHex(char__two, uint8_t__cipher, 1u)) return 0u;
        if ((size_t__index % 32u) == 0u)
        {
            func__Simple_CryptoBlock(uint8_t__encKey, uint32_t__nonce, (uint32_t)(size_t__index / 32u), uint8_t__block);
        }
        string__out += (char)(uint8_t)(uint8_t__cipher[0] ^ uint8_t__block[size_t__index % 32u]);
        size_t__index++;
    }
    return 2u;
}

/* ==================== کاربر پیش‌فرض ====================
   admin/admin با نقش ادمین - هم برای نبودِ فایل و هم برای فایلی که برچسبش
   نخورده (دست‌کاری‌شده) استفاده می‌شود. */
static void func__Simple_AuthDefaults(void)
{
    uint8_t uint8_t__i;

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        SIMPLE_USER_T__G__Users[uint8_t__i].active = false;
    }
    (void)strncpy(SIMPLE_USER_T__G__Users[0].name, "admin", SIMPLE_USER_NAME_MAX);
    SIMPLE_USER_T__G__Users[0].name[SIMPLE_USER_NAME_MAX] = '\0';
    func__Simple_HashPass("admin", SIMPLE_USER_T__G__Users[0].passHash);
    SIMPLE_USER_T__G__Users[0].role = 1u;
    SIMPLE_USER_T__G__Users[0].active = true;
    UINT8_T__G__UserCount = 1u;
}

/* ==================== پارس متن کاربران ====================
   همان پارس سادهٔ قبلی؛ فقط روی متنی که پیش‌تر رمزگشایی شده اجرا می‌شود. */
static void func__Simple_AuthParse(const String &string__content)
{
    int32_t int32_t__pos = 0;
    uint8_t uint8_t__found = 0u;
    uint8_t uint8_t__index;

    for (uint8_t__index = 0u; uint8_t__index < SIMPLE_USERS_MAX; uint8_t__index++)
    {
        SIMPLE_USER_T__G__Users[uint8_t__index].active = false;
    }

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
            SIMPLE_USER_T__G__Users[uint8_t__found].name[SIMPLE_USER_NAME_MAX] = '\0';
            (void)strncpy(SIMPLE_USER_T__G__Users[uint8_t__found].passHash, string__pass.c_str(), 64);
            SIMPLE_USER_T__G__Users[uint8_t__found].passHash[64] = '\0';
            SIMPLE_USER_T__G__Users[uint8_t__found].role = (uint8_t)int32_t__role;
            SIMPLE_USER_T__G__Users[uint8_t__found].active = true;
            uint8_t__found++;
        }
        int32_t__pos = int32_t__roleEnd;
    }
    UINT8_T__G__UserCount = uint8_t__found;
}

/* ==================== پیدا کردن هش قدیمی ۸ کاراکتری (مهاجرت) ==================== */
static bool func__Simple_AuthHasOldHash(const String &string__content)
{
    int32_t int32_t__p = string__content.indexOf("\"pass\":\"");
    if (int32_t__p < 0) return false;
    int32_t__p += 8;
    int32_t int32_t__e = string__content.indexOf("\"", int32_t__p);
    if (int32_t__e <= int32_t__p) return false;
    return (string__content.substring(int32_t__p, int32_t__e).length() == 8u);
}

/* ==================== ذخیره کاربران (رمزنگاری‌شده) ====================
   متن خوانا در حافظه ساخته و بعد با کلید چیپ رمز می‌شود؛ روی فلش فقط متن
   رمزشده و برچسب HMAC می‌ماند. */
static void func__Simple_AuthSave(void)
{
    String string__plain;
    String string__enc;
    bool bool__first = true;
    uint8_t uint8_t__i;

    if (!BOOL__G__FsOk) return;

    string__plain = "{\"users\":[";
    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (!SIMPLE_USER_T__G__Users[uint8_t__i].active) continue;
        if (!bool__first) string__plain += ",";
        string__plain += "{\"name\":\"";
        string__plain += SIMPLE_USER_T__G__Users[uint8_t__i].name;
        string__plain += "\",\"pass\":\"";
        string__plain += SIMPLE_USER_T__G__Users[uint8_t__i].passHash;
        string__plain += "\",\"role\":";
        string__plain += String(SIMPLE_USER_T__G__Users[uint8_t__i].role);
        string__plain += "}";
        bool__first = false;
    }
    string__plain += "]}";

    func__Simple_CryptoEncrypt(string__plain, string__enc);

    File file__f = LittleFS.open(SIMPLE_USERS_FILE, "w");
    if (!file__f) return;
    file__f.print(string__enc);
    file__f.close();
}

/* ==================== پیدا کردن کاربر با نام (منفی = نبود) ==================== */
static int32_t func__Simple_FindUser(const char *char__ptr_name)
{
    uint8_t uint8_t__i;

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (!SIMPLE_USER_T__G__Users[uint8_t__i].active) continue;
        if (strcmp(SIMPLE_USER_T__G__Users[uint8_t__i].name, char__ptr_name) == 0) return (int32_t)uint8_t__i;
    }
    return -1;
}

/* ==================== تأیید رمز یک کاربر ====================
   با هش فعلی (SHA256+salt) مقایسه می‌شود؛ برای فایل‌های خیلی قدیمی که هش
   ۸ کاراکتری دارند هم مسیر سازگاری نگه داشته شده است. */
static bool func__Simple_VerifyUserPass(const char *char__ptr_name, const char *char__ptr_pass)
{
    int32_t int32_t__index = func__Simple_FindUser(char__ptr_name);
    char char__hash[65];

    if (int32_t__index < 0) return false;
    if (char__ptr_pass == NULL) return false;

    if (func__Simple_IsOldHash(SIMPLE_USER_T__G__Users[int32_t__index].passHash))
    {
        uint32_t uint32_t__oldHash = 5381u;
        uint8_t uint8_t__j = 0u;
        char char__oldHashStr[16];

        while (char__ptr_pass[uint8_t__j] != '\0')
        {
            uint32_t__oldHash = ((uint32_t__oldHash << 5) + uint32_t__oldHash) + (uint32_t)char__ptr_pass[uint8_t__j];
            uint8_t__j++;
        }
        (void)snprintf(char__oldHashStr, sizeof(char__oldHashStr), "%08lX", (unsigned long)uint32_t__oldHash);
        return (strcmp(SIMPLE_USER_T__G__Users[int32_t__index].passHash, char__oldHashStr) == 0);
    }

    func__Simple_HashPass(char__ptr_pass, char__hash);
    return (strcmp(SIMPLE_USER_T__G__Users[int32_t__index].passHash, char__hash) == 0);
}

/* ==================== تغییر رمز یک کاربر ====================
   همان مسیری که /setpass صدا می‌زند. اجازه/عدم اجازه در لایهٔ HTTP است. */
static bool func__Simple_SetUserPass(const char *char__ptr_name, const char *char__ptr_newPass)
{
    int32_t int32_t__index = func__Simple_FindUser(char__ptr_name);
    char char__hash[65];

    if (int32_t__index < 0) return false;
    if (char__ptr_newPass == NULL) return false;
    if (strlen(char__ptr_newPass) < SIMPLE_USER_PASS_MIN) return false;
    if (strlen(char__ptr_newPass) > SIMPLE_USER_PASS_MAX) return false;
    if (func__Simple_VerifyUserPass(char__ptr_name, char__ptr_newPass)) return true;

    func__Simple_HashPass(char__ptr_newPass, char__hash);
    (void)strncpy(SIMPLE_USER_T__G__Users[int32_t__index].passHash, char__hash, 64);
    SIMPLE_USER_T__G__Users[int32_t__index].passHash[64] = '\0';
    func__Simple_AuthSave();
    return true;
}

/* ==================== بازیابی رمز ادمین با دسترسی فیزیکی ====================
   رمزها یک‌طرفه هش می‌شوند و قابل خواندن نیستند؛ پس «بازیابی» یعنی برگرداندن
   کاربران به پیش‌فرض admin/admin. تنها کلید این کار، قطع و وصل برق است:
   ۱) بوت بدون ورود: بعد از SIMPLE_RECOVERY_ARM_MS اگر کسی وارد نشده باشد
      پرچم /boot_no_login.json نوشته می‌شود.
   ۲) ورود هر کاربر: پرچم پاک می‌شود (ریستارت معمولی رمزها را دست نمی‌زند).
   ۳) بوت بعدی که پرچم را ببیند: پرچم پاک و کاربران پیش‌فرض می‌شوند. */
static bool func__Simple_AuthIsDefault(void)
{
    uint8_t uint8_t__i;
    uint8_t uint8_t__activeCount = 0u;

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        if (SIMPLE_USER_T__G__Users[uint8_t__i].active)
        {
            uint8_t__activeCount++;
        }
    }
    if (uint8_t__activeCount != 1u) return false;
    if (SIMPLE_USER_T__G__Users[0].role != 1u) return false;
    return func__Simple_VerifyUserPass("admin", "admin");
}

static bool func__Simple_RecoveryFlagExists(void)
{
    if (!BOOL__G__FsOk) return false;
    return LittleFS.exists(SIMPLE_RECOVERY_FILE);
}

static void func__Simple_RecoveryArm(void)
{
    if (!BOOL__G__FsOk) return;
    if (BOOL__G__RecoveryArmed) return;                 /* همین بوت نوشته شده */
    if (func__Simple_AuthIsDefault())
    {
        BOOL__G__RecoveryArmed = true;                  /* رمزها پیش‌فرض است؛ نیازی به نوشتن فلش نیست */
        return;
    }
    if (func__Simple_RecoveryFlagExists())
    {
        BOOL__G__RecoveryArmed = true;                  /* از بوت قبلی مانده */
        return;
    }
    /* اگر فلش گیر کرد، هر چند ثانیه یک‌بار تلاش کن - نه در هر دور حلقه */
    if ((uint32_t)millis() - UINT32_T__G__LastRecoveryTryMs < SIMPLE_RECOVERY_RETRY_MS) return;
    UINT32_T__G__LastRecoveryTryMs = (uint32_t)millis();

    File file__f = LittleFS.open(SIMPLE_RECOVERY_FILE, "w");
    if (!file__f) return;
    file__f.print("{\"noLogin\":1,\"up\":");
    file__f.print((uint32_t)(millis() / 1000u));
    file__f.print("}");
    file__f.close();
    BOOL__G__RecoveryArmed = true;
}

static void func__Simple_RecoveryDisarm(void)
{
    if (!BOOL__G__FsOk) return;
    if (!func__Simple_RecoveryFlagExists()) return;
    LittleFS.remove(SIMPLE_RECOVERY_FILE);
}

/* در شروع برنامه و پیش از هر ورودی صدا زده می‌شود */
static void func__Simple_RecoveryApply(void)
{
    if (!func__Simple_RecoveryFlagExists()) return;
    LittleFS.remove(SIMPLE_RECOVERY_FILE);
    if (!func__Simple_AuthIsDefault())
    {
        func__Simple_AuthDefaults();
        func__Simple_AuthSave();
        BOOL__G__RecoveryHappened = true;
    }
}

/* ==================== بارگذاری کاربران ====================
   فایل رمزنگاری‌شده خوانده و برچسبش بررسی می‌شود:
   - برچسب درست ⇒ کاربران همان‌ها
   - قالب قدیمی خوانا ⇒ یک‌بار خوانده و رمزشده بازنویسی می‌شود
   - برچسب خراب ⇒ فایل دست‌کاری شده؛ به کاربر پیش‌فرض برمی‌گردیم */
static void func__Simple_AuthLoad(void)
{
    uint8_t uint8_t__i;
    String string__content;
    uint8_t uint8_t__state;

    for (uint8_t__i = 0u; uint8_t__i < SIMPLE_USERS_MAX; uint8_t__i++)
    {
        SIMPLE_USER_T__G__Users[uint8_t__i].active = false;
    }
    UINT8_T__G__UserCount = 0u;

    if (!BOOL__G__FsOk)
    {
        return;
    }

    if (!LittleFS.exists(SIMPLE_USERS_FILE))
    {
        func__Simple_AuthDefaults();
        func__Simple_AuthSave();
        return;
    }

    File file__f = LittleFS.open(SIMPLE_USERS_FILE, "r");
    if (!file__f)
    {
        func__Simple_AuthDefaults();
        return;
    }
    String string__raw = file__f.readString();
    file__f.close();

    uint8_t__state = func__Simple_CryptoDecrypt(string__raw, string__content);
    if (uint8_t__state == 0u)
    {
        /* رمزنگاری‌شده ولی برچسب نخورد - فایل خراب/دست‌کاری‌شده */
        func__Simple_AuthDefaults();
        func__Simple_AuthSave();
        return;
    }

    if (func__Simple_AuthHasOldHash(string__content))
    {
        /* هش خیلی قدیمی (۸ کاراکتری) - پاک و از نو بساز */
        func__Simple_AuthDefaults();
        func__Simple_AuthSave();
        return;
    }

    func__Simple_AuthParse(string__content);

    if (uint8_t__state == 1u)
    {
        /* قالب خوانای قدیمی بود؛ همین حالا رمزشده ذخیره کن */
        func__Simple_AuthSave();
    }

    if (UINT8_T__G__UserCount == 0u)
    {
        func__Simple_AuthDefaults();
        func__Simple_AuthSave();
    }
}

/* ==================== ورود - با IP سشن ==================== */
static bool func__Simple_AuthLogin(const char *char__ptr_name, const char *char__ptr_pass)
{
    int32_t int32_t__index = func__Simple_FindUser(char__ptr_name);

    if (int32_t__index < 0) return false;
    if (!func__Simple_VerifyUserPass(char__ptr_name, char__ptr_pass)) return false;

    (void)strncpy(CHAR__G__CurrentUser, char__ptr_name, SIMPLE_USER_NAME_MAX);
    CHAR__G__CurrentUser[SIMPLE_USER_NAME_MAX] = '\0';
    UINT8_T__G__CurrentRole = SIMPLE_USER_T__G__Users[int32_t__index].role;
    UINT32_T__G__SessionStartMs = (uint32_t)millis();
    /* ذخیره IP کلاینت برای سشن */
    IPAddress ip__client = SIMPLE_WEB_SERVER_T__G__Server.client().remoteIP();
    (void)snprintf(CHAR__G__SessionIP, sizeof(CHAR__G__SessionIP), "%u.%u.%u.%u", ip__client[0], ip__client[1], ip__client[2], ip__client[3]);
    /* ورود شد ⇒ این روشن‌بودن «بدون ورود» نبوده؛ پرچم بازیابی پاک می‌شود */
    BOOL__G__SessionHadLogin = true;
    func__Simple_RecoveryDisarm();
    return true;
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
