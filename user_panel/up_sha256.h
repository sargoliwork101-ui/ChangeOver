/**
 * @file    up_sha256.h
 * @brief   [EN] Self-contained SHA-256. Used ONLY to store login passwords as
 *              salted, iterated digests - never to invent a security claim the
 *              link cannot support (this is a local Wi-Fi access point with no
 *              TLS, so the honest description is "access control").
 *          [FA] SHA-256 خودکفا. فقط برای نگه‌داشتن گذرواژه‌ها به‌شکل چکیدهٔ
 *              نمک‌دار و تکرارشده استفاده می‌شود - نه برای ادعای امنیتی‌ای که
 *              این لینک نمی‌تواند پشتیبانی کند (یک اکسس‌پوینت محلی بدون TLS
 *              است؛ پس توصیف صادقانه «کنترل دسترسی» است).
 *
 * @note    [EN] No dynamic allocation, no Arduino dependency: the same code
 *              compiles on the ESP and in the host test.
 *          [FA] بدون تخصیص دینامیک و بدون وابستگی به آردوینو: همان کد روی ESP
 *              و در تست هاست کامپایل می‌شود.
 */

#ifndef UP_SHA256_H
#define UP_SHA256_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define UP_SHA256_DIGEST_BYTES 32u
#define UP_SHA256_BLOCK_BYTES  64u

typedef struct
{
    uint32_t uint32_t__state[8];
    uint32_t uint32_t__bitLenLow;
    uint32_t uint32_t__bitLenHigh;
    uint8_t  uint8_t__buffer[UP_SHA256_BLOCK_BYTES];
    uint32_t uint32_t__bufferLen;
} up_sha256_context_t;

/* ==================== Round constants / ثابت‌های دور ==================== */
static const uint32_t UINT32_T__A__ShaK[64] =
{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define UP_ROTR32(x, n) (((x) >> (n)) | ((x) << (32u - (n))))

/* ==================== Block transform / تبدیل بلوک ==================== */
/**
 * @brief  [EN] Process one 64-byte block into the running state.
 *         [FA] پردازش یک بلوک ۶۴ بایتی در وضعیت جاری.
 * @param  up_sha256_context_t__ctx [EN] context, updated in place / [FA] زمینه، به‌جای خودش به‌روز می‌شود
 * @param  uint8_t__block [EN] 64 bytes, already big-endian by construction / [FA] ۶۴ بایت
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSha256_Block(up_sha256_context_t *up_sha256_context_t__ctx, const uint8_t *uint8_t__block)
{
    uint32_t uint32_t__w[64];
    uint32_t uint32_t__i;

    for (uint32_t__i = 0u; uint32_t__i < 16u; uint32_t__i++)
    {
        uint32_t uint32_t__offset = uint32_t__i * 4u;
        uint32_t__w[uint32_t__i] = ((uint32_t)uint8_t__block[uint32_t__offset] << 24)
                                 | ((uint32_t)uint8_t__block[uint32_t__offset + 1u] << 16)
                                 | ((uint32_t)uint8_t__block[uint32_t__offset + 2u] << 8)
                                 | ((uint32_t)uint8_t__block[uint32_t__offset + 3u]);
    }

    for (uint32_t__i = 16u; uint32_t__i < 64u; uint32_t__i++)
    {
        uint32_t uint32_t__s0 = UP_ROTR32(uint32_t__w[uint32_t__i - 15u], 7u) ^ UP_ROTR32(uint32_t__w[uint32_t__i - 15u], 18u) ^ (uint32_t__w[uint32_t__i - 15u] >> 3);
        uint32_t uint32_t__s1 = UP_ROTR32(uint32_t__w[uint32_t__i - 2u], 17u) ^ UP_ROTR32(uint32_t__w[uint32_t__i - 2u], 19u) ^ (uint32_t__w[uint32_t__i - 2u] >> 10);
        uint32_t__w[uint32_t__i] = uint32_t__w[uint32_t__i - 16u] + uint32_t__s0 + uint32_t__w[uint32_t__i - 7u] + uint32_t__s1;
    }

    uint32_t uint32_t__a = up_sha256_context_t__ctx->uint32_t__state[0];
    uint32_t uint32_t__b = up_sha256_context_t__ctx->uint32_t__state[1];
    uint32_t uint32_t__c = up_sha256_context_t__ctx->uint32_t__state[2];
    uint32_t uint32_t__d = up_sha256_context_t__ctx->uint32_t__state[3];
    uint32_t uint32_t__e = up_sha256_context_t__ctx->uint32_t__state[4];
    uint32_t uint32_t__f = up_sha256_context_t__ctx->uint32_t__state[5];
    uint32_t uint32_t__g = up_sha256_context_t__ctx->uint32_t__state[6];
    uint32_t uint32_t__h = up_sha256_context_t__ctx->uint32_t__state[7];

    for (uint32_t__i = 0u; uint32_t__i < 64u; uint32_t__i++)
    {
        uint32_t uint32_t__s1 = UP_ROTR32(uint32_t__e, 6u) ^ UP_ROTR32(uint32_t__e, 11u) ^ UP_ROTR32(uint32_t__e, 25u);
        uint32_t uint32_t__ch = (uint32_t__e & uint32_t__f) ^ ((~uint32_t__e) & uint32_t__g);
        uint32_t uint32_t__temp1 = uint32_t__h + uint32_t__s1 + uint32_t__ch + UINT32_T__A__ShaK[uint32_t__i] + uint32_t__w[uint32_t__i];
        uint32_t uint32_t__s0 = UP_ROTR32(uint32_t__a, 2u) ^ UP_ROTR32(uint32_t__a, 13u) ^ UP_ROTR32(uint32_t__a, 22u);
        uint32_t uint32_t__maj = (uint32_t__a & uint32_t__b) ^ (uint32_t__a & uint32_t__c) ^ (uint32_t__b & uint32_t__c);
        uint32_t uint32_t__temp2 = uint32_t__s0 + uint32_t__maj;

        uint32_t__h = uint32_t__g;
        uint32_t__g = uint32_t__f;
        uint32_t__f = uint32_t__e;
        uint32_t__e = uint32_t__d + uint32_t__temp1;
        uint32_t__d = uint32_t__c;
        uint32_t__c = uint32_t__b;
        uint32_t__b = uint32_t__a;
        uint32_t__a = uint32_t__temp1 + uint32_t__temp2;
    }

    up_sha256_context_t__ctx->uint32_t__state[0] += uint32_t__a;
    up_sha256_context_t__ctx->uint32_t__state[1] += uint32_t__b;
    up_sha256_context_t__ctx->uint32_t__state[2] += uint32_t__c;
    up_sha256_context_t__ctx->uint32_t__state[3] += uint32_t__d;
    up_sha256_context_t__ctx->uint32_t__state[4] += uint32_t__e;
    up_sha256_context_t__ctx->uint32_t__state[5] += uint32_t__f;
    up_sha256_context_t__ctx->uint32_t__state[6] += uint32_t__g;
    up_sha256_context_t__ctx->uint32_t__state[7] += uint32_t__h;
}

/* ==================== Streaming interface / رابط جریانی ==================== */
/**
 * @brief  [EN] Start a new digest / [FA] شروع چکیدهٔ تازه
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSha256_Init(up_sha256_context_t *up_sha256_context_t__ctx)
{
    up_sha256_context_t__ctx->uint32_t__state[0] = 0x6a09e667u;
    up_sha256_context_t__ctx->uint32_t__state[1] = 0xbb67ae85u;
    up_sha256_context_t__ctx->uint32_t__state[2] = 0x3c6ef372u;
    up_sha256_context_t__ctx->uint32_t__state[3] = 0xa54ff53au;
    up_sha256_context_t__ctx->uint32_t__state[4] = 0x510e527fu;
    up_sha256_context_t__ctx->uint32_t__state[5] = 0x9b05688cu;
    up_sha256_context_t__ctx->uint32_t__state[6] = 0x1f83d9abu;
    up_sha256_context_t__ctx->uint32_t__state[7] = 0x5be0cd19u;
    up_sha256_context_t__ctx->uint32_t__bitLenLow = 0u;
    up_sha256_context_t__ctx->uint32_t__bitLenHigh = 0u;
    up_sha256_context_t__ctx->uint32_t__bufferLen = 0u;
}

/**
 * @brief  [EN] Absorb bytes / [FA] جذب بایت‌ها
 * @param  uint8_t__data [EN] pointer to bytes / [FA] اشاره‌گر به بایت‌ها
 * @param  uint32_t__len [EN] count, 0..4096 typical / [FA] تعداد
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSha256_Update(up_sha256_context_t *up_sha256_context_t__ctx, const uint8_t *uint8_t__data, uint32_t uint32_t__len)
{
    uint32_t uint32_t__i = 0u;

    while (uint32_t__i < uint32_t__len)
    {
        uint32_t uint32_t__space = UP_SHA256_BLOCK_BYTES - up_sha256_context_t__ctx->uint32_t__bufferLen;
        uint32_t uint32_t__take = uint32_t__len - uint32_t__i;

        if (uint32_t__take > uint32_t__space)
        {
            uint32_t__take = uint32_t__space;
        }

        memcpy(&up_sha256_context_t__ctx->uint8_t__buffer[up_sha256_context_t__ctx->uint32_t__bufferLen],
               &uint8_t__data[uint32_t__i], uint32_t__take);
        up_sha256_context_t__ctx->uint32_t__bufferLen += uint32_t__take;
        uint32_t__i += uint32_t__take;

        if (up_sha256_context_t__ctx->uint32_t__bufferLen == UP_SHA256_BLOCK_BYTES)
        {
            func__UpSha256_Block(up_sha256_context_t__ctx, up_sha256_context_t__ctx->uint8_t__buffer);
            up_sha256_context_t__ctx->uint32_t__bitLenLow += 512u;
            if (up_sha256_context_t__ctx->uint32_t__bitLenLow < 512u)
            {
                up_sha256_context_t__ctx->uint32_t__bitLenHigh++;
            }
            up_sha256_context_t__ctx->uint32_t__bufferLen = 0u;
        }
    }
}

/**
 * @brief  [EN] Finish and write the 32-byte digest.
 *         [FA] پایان و نوشتن چکیدهٔ ۳۲ بایتی.
 * @param  uint8_t__out [EN] caller-provided 32-byte buffer / [FA] بافر ۳۲ بایتی فراخوان
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSha256_Final(up_sha256_context_t *up_sha256_context_t__ctx, uint8_t *uint8_t__out)
{
    uint32_t uint32_t__i = up_sha256_context_t__ctx->uint32_t__bufferLen;
    uint32_t uint32_t__low;
    uint32_t uint32_t__high;

    up_sha256_context_t__ctx->uint32_t__bitLenLow += up_sha256_context_t__ctx->uint32_t__bufferLen * 8u;
    if (up_sha256_context_t__ctx->uint32_t__bitLenLow < (up_sha256_context_t__ctx->uint32_t__bufferLen * 8u))
    {
        up_sha256_context_t__ctx->uint32_t__bitLenHigh++;
    }
    uint32_t__low = up_sha256_context_t__ctx->uint32_t__bitLenLow;
    uint32_t__high = up_sha256_context_t__ctx->uint32_t__bitLenHigh;

    up_sha256_context_t__ctx->uint8_t__buffer[uint32_t__i] = 0x80u;
    uint32_t__i++;

    if (uint32_t__i > 56u)
    {
        while (uint32_t__i < 64u)
        {
            up_sha256_context_t__ctx->uint8_t__buffer[uint32_t__i] = 0u;
            uint32_t__i++;
        }
        func__UpSha256_Block(up_sha256_context_t__ctx, up_sha256_context_t__ctx->uint8_t__buffer);
        uint32_t__i = 0u;
    }

    while (uint32_t__i < 56u)
    {
        up_sha256_context_t__ctx->uint8_t__buffer[uint32_t__i] = 0u;
        uint32_t__i++;
    }

    up_sha256_context_t__ctx->uint8_t__buffer[56] = (uint8_t)((uint32_t__high >> 24) & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[57] = (uint8_t)((uint32_t__high >> 16) & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[58] = (uint8_t)((uint32_t__high >> 8) & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[59] = (uint8_t)(uint32_t__high & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[60] = (uint8_t)((uint32_t__low >> 24) & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[61] = (uint8_t)((uint32_t__low >> 16) & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[62] = (uint8_t)((uint32_t__low >> 8) & 0xFFu);
    up_sha256_context_t__ctx->uint8_t__buffer[63] = (uint8_t)(uint32_t__low & 0xFFu);

    func__UpSha256_Block(up_sha256_context_t__ctx, up_sha256_context_t__ctx->uint8_t__buffer);

    for (uint32_t__i = 0u; uint32_t__i < 8u; uint32_t__i++)
    {
        uint32_t uint32_t__value = up_sha256_context_t__ctx->uint32_t__state[uint32_t__i];
        uint8_t__out[uint32_t__i * 4u]      = (uint8_t)((uint32_t__value >> 24) & 0xFFu);
        uint8_t__out[uint32_t__i * 4u + 1u] = (uint8_t)((uint32_t__value >> 16) & 0xFFu);
        uint8_t__out[uint32_t__i * 4u + 2u] = (uint8_t)((uint32_t__value >> 8) & 0xFFu);
        uint8_t__out[uint32_t__i * 4u + 3u] = (uint8_t)(uint32_t__value & 0xFFu);
    }
}

/* ===================================== One-shot ==================== */
/**
 * @brief  [EN] Hash a memory buffer in one call.
 *         [FA] چکیده‌گرفتن از یک بافر در یک فراخوانی.
 * @param  uint8_t__data [EN] bytes to hash / [FA] بایت‌های ورودی
 * @param  uint32_t__len [EN] length in bytes / [FA] طول به بایت
 * @param  uint8_t__out [EN] 32-byte output / [FA] خروجی ۳۲ بایتی
 * @return [EN] None / [FA] ندارد
 */
static void func__UpSha256_Buffer(const uint8_t *uint8_t__data, uint32_t uint32_t__len, uint8_t *uint8_t__out)
{
    up_sha256_context_t up_sha256_context_t__ctx;

    func__UpSha256_Init(&up_sha256_context_t__ctx);
    func__UpSha256_Update(&up_sha256_context_t__ctx, uint8_t__data, uint32_t__len);
    func__UpSha256_Final(&up_sha256_context_t__ctx, uint8_t__out);
}

#endif /* UP_SHA256_H */
