/*
 * @file    host_test_nvm.c
 * @brief   [EN] Host regression test for exact EspLink NVM implementation.
 *          [FA] تست هاست برای پیاده‌سازی دقیق NVM لینک ESP.
 *
 * Host regression test for the exact EspLink NVM implementation.
 *
 * The flash erase stub deliberately fails. That makes the retry state machine
 * observable without pretending to validate hardware flash timing or power-cut
 * behaviour. The pure record tests also exercise the implementation's CRC,
 * reserved-field, persisted-id and duplicate-id validation.
 */
#define _GNU_SOURCE   /* [EN] MAP_ANONYMOUS on the strict-c11 host run. */
#include "esp_link_nvm.h"
#include "esp_link.h"
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "charger.h"
#include "rtos_time.h"
#include "cmsis_os2.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

static unsigned g_erase_attempts;
/* [EN] Full-cycle mode (user bug 2026-10-07): when false the flash stubs
   work against a RAM page mapped AT the real NVM bank addresses, so
   SaveNow + NvmInit can be exercised end to end. Default true keeps the
   original failure-injection behaviour untouched.
   [FA] حالت چرخهٔ کامل (باگ کاربر): وقتی false است، بدل فلش روی حافظهٔ
   mmap‌شده در نشانی واقعی بانک‌های NVM کار می‌کند تا ذخیره و بازیابیِ
   هنگام بوت ‎end-to-end‎ آزموده شود. پیش‌فرض true رفتار قبلی را نگه می‌دارد. */
static bool g_flash_fails = true;
static uint32_t g_params[256];

#define NVM_HOST_MAP_BASE  0x0800E000u
#define NVM_HOST_MAP_SIZE  0x1000u

osKernelState_t osKernelGetState(void)
{
    return osKernelRunning;
}

void func__Charger_SetSuspended(bool bool__suspended)
{
    (void)bool__suspended;
}

bool func__BspPwm_IsGatePulsing(bsp_pwm_channel_t bsp_pwm_channel_t__channel)
{
    (void)bsp_pwm_channel_t__channel;
    return false;
}

void func__Rtos_DelayMilliseconds(uint32_t uint32_t__milliseconds)
{
    (void)uint32_t__milliseconds;
}

bool func__BspFlash_ErasePage(uint32_t uint32_t__pageAddress)
{
    g_erase_attempts++;
    if (g_flash_fails != false)
    {
        return false;
    }
    memset((void *)(uintptr_t)uint32_t__pageAddress, 0xFF,
           (size_t)ESP_LINK_NVM_FLASH_PAGE_SIZE);
    return true;
}

bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count)
{
    if (g_flash_fails != false)
    {
        return false;
    }
    memcpy((void *)(uintptr_t)uint32_t__address, uint16_t__A__Data,
           (size_t)uint32_t__count * sizeof(uint16_t));
    return true;
}

bool func__EspLink_GetParam(uint8_t uint8_t__paramId,
                            uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL ||
        func__EspLink_NvmParamPersisted(uint8_t__paramId) == false)
    {
        return false;
    }
    *uint32_t__value = g_params[uint8_t__paramId];
    return true;
}

/* NvmInit is not called by this failure-injection test, but the exact source
   still declares this public dependency through esp_link.h. */
bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue)
{
    if (uint32_t__appliedValue == NULL)
    {
        return false;
    }
    g_params[uint8_t__paramId] = uint32_t__value;
    *uint32_t__appliedValue = uint32_t__value;
    return true;
}

static void check(bool bool__condition, const char *const char__message)
{
    if (bool__condition == false)
    {
        (void)fprintf(stderr, "FAIL: %s\n", char__message);
        return;
    }
    (void)printf("PASS: %s\n", char__message);
}

static int g_failures;

static void require_check(bool bool__condition, const char *const char__message)
{
    if (bool__condition == false)
    {
        g_failures++;
    }
    check(bool__condition, char__message);
}

static void recalc_crc(esp_link_nvm_record_t *esp_link_nvm_record_t__record)
{
    esp_link_nvm_record_t__record->uint32_t__crc32 =
        func__EspLink_NvmCrc32(
            (const uint8_t *)esp_link_nvm_record_t__record,
            (uint32_t)(sizeof(*esp_link_nvm_record_t__record) - sizeof(uint32_t)));
}

static void tick_until_exhausted(void)
{
    unsigned uint32_t__i;

    /* One dirty mark plus 14 debounce ticks reaches run 15. The failed
       initial save and the three bounded retries then happen one per tick. */
    for (uint32_t__i = 0u; uint32_t__i < 18u; uint32_t__i++)
    {
        func__EspLink_NvmTick();
    }
}

static void params_seed_with_id(void)
{
    unsigned uint32_t__i;

    for (uint32_t__i = 0u; uint32_t__i < 256u; uint32_t__i++)
    {
        g_params[uint32_t__i] = uint32_t__i;
    }
}

int main(void)
{
    params_seed_with_id();
    esp_link_nvm_record_t esp_link_nvm_record_t__record;
    esp_link_nvm_entry_t ESP_LINK_NVM_ENTRY_T__A__Entry[2u] =
    {
        {0u, 0u, 111u},
        {20u, 0u, 222u}
    };

    (void)memset(&esp_link_nvm_record_t__record, 0, sizeof(esp_link_nvm_record_t__record));
    func__EspLink_NvmRecordBuild(&esp_link_nvm_record_t__record,
                                 7u,
                                 ESP_LINK_NVM_ENTRY_T__A__Entry,
                                 2u);
    require_check(func__EspLink_NvmRecordValidate(&esp_link_nvm_record_t__record),
                  "a builder-produced record validates");

    esp_link_nvm_record_t__record.ESP_LINK_NVM_ENTRY_T__A__Entry[1u].uint16_t__id = 0u;
    recalc_crc(&esp_link_nvm_record_t__record);
    require_check(func__EspLink_NvmRecordValidate(&esp_link_nvm_record_t__record) == false,
                  "a CRC-valid duplicate persisted id is rejected");

    func__EspLink_NvmRecordBuild(&esp_link_nvm_record_t__record,
                                 8u,
                                 ESP_LINK_NVM_ENTRY_T__A__Entry,
                                 2u);
    esp_link_nvm_record_t__record.uint16_t__reserved = 1u;
    recalc_crc(&esp_link_nvm_record_t__record);
    require_check(func__EspLink_NvmRecordValidate(&esp_link_nvm_record_t__record) == false,
                  "a CRC-valid nonzero reserved field is rejected");

    func__EspLink_NvmMarkDirty(0u);
    tick_until_exhausted();
    require_check(g_erase_attempts == 4u,
                  "the first failed save gets one initial attempt plus three retries");

    /* This is the regression: after exhaustion, a new persisted edit must
       restore the complete retry budget rather than inheriting retry=3. */
    func__EspLink_NvmMarkDirty(1u);
    tick_until_exhausted();
    require_check(g_erase_attempts == 8u,
                  "a new persisted edit receives a fresh retry budget after exhaustion");

    /* Transient ids must not wake the save machine. */
    func__EspLink_NvmMarkDirty(19u);
    tick_until_exhausted();
    require_check(g_erase_attempts == 8u,
                  "a transient edit does not schedule an NVM save");

    /* ==== full persistence cycle (user bug 2026-10-07) ====
       [EN] "Settings revert after a board reset" - the RESTORE path had
            never been tested (this file used to stub flash as always
            failing). Map RAM at the real NVM bank addresses, apply the
            calibration coefficients like the panel does, let the debounced
            save run, simulate a reboot (compiled defaults come back) and
            prove NvmInit restores every value; then save again so the
            ping-pong flips to the other bank and restore once more.
       [FA] «تنظیمات بعد از ریست برد برمی‌گردد» - مسیر بازیابی هرگز آزموده
            نشده بود. حافظه در نشانی واقعی بانک‌های NVM نگاشت می‌شود،
            ضرایب کالیبراسیون مثل پنل اعمال و ذخیرهٔ دیبانس‌شده اجرا می‌شود،
            بعد با شبیه‌سازی ریست (پیش‌فرض کامپایل) ثابت می‌شود NvmInit همه
            را برمی‌گرداند؛ سپس ذخیرهٔ دوم پینگ‌پنگ را به بانک دیگر می‌برد
            و بازیابی دوباره آزموده می‌شود. */
    {
        unsigned uint32_t__i;
        void *void_ptr__map = mmap((void *)(uintptr_t)NVM_HOST_MAP_BASE,
                                   NVM_HOST_MAP_SIZE,
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                   -1, 0);

        require_check(void_ptr__map == (void *)(uintptr_t)NVM_HOST_MAP_BASE,
                      "NVM bank addresses are mappable on the host");
        if (void_ptr__map == (void *)(uintptr_t)NVM_HOST_MAP_BASE)
        {
            /* [EN] id 5 carries -200 mV as two's complement, the wire form.
               [FA] شناسهٔ ۵ مقدار ‎-200mV‎ را به مکمل دو دارد (قالب سیم). */
            static const uint32_t UINT32_T__A__Applied[7] =
                { 12u, 34u, 1046u, 1303u, 150u, 0xFFFFFF38u, 0u };

            memset(void_ptr__map, 0xFF, NVM_HOST_MAP_SIZE);
            g_flash_fails = false;

            for (uint32_t__i = 0u; uint32_t__i < 7u; uint32_t__i++)
            {
                g_params[uint32_t__i] = UINT32_T__A__Applied[uint32_t__i];
                func__EspLink_NvmMarkDirty((uint8_t)uint32_t__i);
            }
            tick_until_exhausted();

            for (uint32_t__i = 0u; uint32_t__i < 256u; uint32_t__i++)
            {
                g_params[uint32_t__i] = 0u;
            }
            g_params[0] = 8u;  g_params[1] = 8u;
            g_params[2] = 1000u; g_params[3] = 1000u;

            func__EspLink_NvmInit();
            for (uint32_t__i = 0u; uint32_t__i < 7u; uint32_t__i++)
            {
                char char__msg[72];
                (void)snprintf(char__msg, sizeof(char__msg),
                               "id %u survives a reboot (restored from bank A)",
                               uint32_t__i);
                require_check(g_params[uint32_t__i] ==
                              UINT32_T__A__Applied[uint32_t__i], char__msg);
            }

            g_params[2] = 1200u;
            func__EspLink_NvmMarkDirty(2u);
            tick_until_exhausted();
            for (uint32_t__i = 0u; uint32_t__i < 256u; uint32_t__i++)
            {
                g_params[uint32_t__i] = 0u;
            }
            func__EspLink_NvmInit();
            require_check(g_params[2] == 1200u,
                          "the newest bank wins after the ping-pong save");
            require_check(g_params[3] == 1303u,
                          "untouched ids ride along in the newest record");
            g_flash_fails = true;
        }
    }

    return (g_failures == 0) ? 0 : 1;
}
