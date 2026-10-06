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
#include "esp_link_nvm.h"
#include "esp_link.h"
#include "bsp_flash.h"
#include "bsp_pwm.h"
#include "charger.h"
#include "rtos_time.h"
#include "cmsis_os2.h"

#include <stdio.h>
#include <string.h>

static unsigned g_erase_attempts;

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
    (void)uint32_t__pageAddress;
    g_erase_attempts++;
    return false;
}

bool func__BspFlash_ProgramHalfWords(uint32_t uint32_t__address,
                                     const uint16_t *uint16_t__A__Data,
                                     uint32_t uint32_t__count)
{
    (void)uint32_t__address;
    (void)uint16_t__A__Data;
    (void)uint32_t__count;
    return false;
}

bool func__EspLink_GetParam(uint8_t uint8_t__paramId,
                            uint32_t *uint32_t__value)
{
    if (uint32_t__value == NULL ||
        func__EspLink_NvmParamPersisted(uint8_t__paramId) == false)
    {
        return false;
    }
    *uint32_t__value = (uint32_t)uint8_t__paramId;
    return true;
}

/* NvmInit is not called by this failure-injection test, but the exact source
   still declares this public dependency through esp_link.h. */
bool func__EspLink_ApplyParam(uint8_t uint8_t__paramId,
                              uint32_t uint32_t__value,
                              uint32_t *uint32_t__appliedValue)
{
    (void)uint8_t__paramId;
    (void)uint32_t__value;
    (void)uint32_t__appliedValue;
    return false;
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

int main(void)
{
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

    return (g_failures == 0) ? 0 : 1;
}
