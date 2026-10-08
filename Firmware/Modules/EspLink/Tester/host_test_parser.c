/*
 * @file    host_test_parser.c
 * @brief   [EN] Host regression test for the production EspLink parser.
 *          [FA] تست هاست برای پارسر تولیدی لینک ESP.
 *
 * Host regression test for the production EspLink byte parser and the
 * one-shot LUT_RESET authorization predicate. The production esp_link.c is
 * compiled directly; only its host probes replace the UART/module graph.
 */
#include "esp_link.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

static unsigned g_checks;
static unsigned g_failures;

#define CHECK(condition) do { \
    g_checks++; \
    if (!(condition)) { \
        g_failures++; \
        (void)fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    } \
} while (0)

static uint16_t crc16_byte(uint16_t crc, uint8_t byte)
{
    uint8_t bit;

    crc ^= (uint16_t)byte << 8;
    for (bit = 0u; bit < 8u; bit++)
    {
        crc = ((crc & 0x8000u) != 0u) ?
            (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint16_t frame_crc(uint8_t version,
                          uint8_t type,
                          uint8_t lengthLo,
                          uint8_t lengthHi,
                          const uint8_t *payload,
                          uint16_t length)
{
    uint16_t crc = (uint16_t)ESPLINK_CRC16_INIT;
    uint16_t index;

    crc = crc16_byte(crc, version);
    crc = crc16_byte(crc, type);
    crc = crc16_byte(crc, lengthLo);
    crc = crc16_byte(crc, lengthHi);
    for (index = 0u; index < length; index++)
    {
        crc = crc16_byte(crc, payload[index]);
    }
    return crc;
}

static void feed_frame(uint8_t type,
                       const uint8_t *payload,
                       uint16_t length,
                       bool corruptCrc)
{
    uint16_t crc;
    uint16_t index;

    crc = frame_crc((uint8_t)ESPLINK_PROTOCOL_VERSION,
                    type,
                    (uint8_t)(length & 0xFFu),
                    (uint8_t)(length >> 8),
                    payload,
                    length);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE0);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE1);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_PROTOCOL_VERSION);
    func__EspLink_HostTest_FeedByte(type);
    func__EspLink_HostTest_FeedByte((uint8_t)(length & 0xFFu));
    func__EspLink_HostTest_FeedByte((uint8_t)(length >> 8));
    for (index = 0u; index < length; index++)
    {
        func__EspLink_HostTest_FeedByte(payload[index]);
    }
    func__EspLink_HostTest_FeedByte((uint8_t)((crc & 0xFFu) ^
                                              (corruptCrc ? 1u : 0u)));
    func__EspLink_HostTest_FeedByte((uint8_t)(crc >> 8));
}

int main(void)
{
    uint8_t crcPayload = 0u;
    uint8_t crcType;
    uint16_t crc;
    bool foundCrcHighAa = false;

    printf("== EspLink parser host test ==\n");
    func__EspLink_HostTest_Reset();

    /* A normal zero-length frame is accepted. */
    feed_frame(0x01u, NULL, 0u, false);
    CHECK(func__EspLink_HostTest_AcceptedFrames() == 1u);
    CHECK(func__EspLink_HostTest_CrcErrors() == 0u);

    /* Version mismatch: the mismatch byte is not AA, so the following frame
       starts normally. */
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE0);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE1);
    func__EspLink_HostTest_FeedByte(0x7Fu);
    feed_frame(0x02u, NULL, 0u, false);
    CHECK(func__EspLink_HostTest_VersionErrors() == 1u);
    CHECK(func__EspLink_HostTest_AcceptedFrames() == 2u);

    /* An impossible length whose high byte is AA must retain that AA. */
    func__EspLink_HostTest_Reset();
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE0);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE1);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_PROTOCOL_VERSION);
    func__EspLink_HostTest_FeedByte(0x03u);
    func__EspLink_HostTest_FeedByte(0x00u);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE0);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE1);
    func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_PROTOCOL_VERSION);
    func__EspLink_HostTest_FeedByte(0x04u);
    func__EspLink_HostTest_FeedByte(0x00u);
    func__EspLink_HostTest_FeedByte(0x00u);
    crc = frame_crc((uint8_t)ESPLINK_PROTOCOL_VERSION, 0x04u, 0u, 0u, NULL, 0u);
    func__EspLink_HostTest_FeedByte((uint8_t)crc);
    func__EspLink_HostTest_FeedByte((uint8_t)(crc >> 8));
    CHECK(func__EspLink_HostTest_AcceptedFrames() == 1u);

    /* CRC error with CRC-HI == AA: the following 55 must be consumed as
       SOF1, not discarded. Search a tiny valid frame space for such a CRC so
       this test exercises the exact byte boundary rather than assuming one. */
    for (crcType = 0u; crcType != 0xFFu; crcType++)
    {
        crc = frame_crc((uint8_t)ESPLINK_PROTOCOL_VERSION,
                        crcType, 1u, 0u, &crcPayload, 1u);
        if ((uint8_t)(crc >> 8) == (uint8_t)ESPLINK_SOF_BYTE0)
        {
            foundCrcHighAa = true;
            break;
        }
    }
    CHECK(foundCrcHighAa);
    if (foundCrcHighAa)
    {
        func__EspLink_HostTest_Reset();
        feed_frame(crcType, &crcPayload, 1u, true);
        func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_SOF_BYTE1);
        func__EspLink_HostTest_FeedByte((uint8_t)ESPLINK_PROTOCOL_VERSION);
        func__EspLink_HostTest_FeedByte(0x05u);
        func__EspLink_HostTest_FeedByte(0x00u);
        func__EspLink_HostTest_FeedByte(0x00u);
        crc = frame_crc((uint8_t)ESPLINK_PROTOCOL_VERSION, 0x05u,
                        0u, 0u, NULL, 0u);
        func__EspLink_HostTest_FeedByte((uint8_t)crc);
        func__EspLink_HostTest_FeedByte((uint8_t)(crc >> 8));
        CHECK(func__EspLink_HostTest_CrcErrors() == 1u);
        CHECK(func__EspLink_HostTest_AcceptedFrames() == 1u);
    }

    /* LUT_RESET is one-shot and requires a fresh successful commit ACK. A
       successful zero-point commit is intentionally accepted even though no
       active flash table remains. */
    func__EspLink_HostTest_Reset();
    CHECK(func__EspLink_HostTest_TryReset(true, true) == false);
    func__EspLink_HostTest_RecordCommitAck(false);
    CHECK(func__EspLink_HostTest_TryReset(true, true) == false);
    func__EspLink_HostTest_RecordCommitAck(true);
    CHECK(func__EspLink_HostTest_TryReset(false, true) == false);
    CHECK(func__EspLink_HostTest_TryReset(true, false) == true);
    CHECK(func__EspLink_HostTest_TryReset(true, true) == false);
    func__EspLink_HostTest_RecordCommitAck(true);
    CHECK(func__EspLink_HostTest_TryReset(true, true) == true);

    printf("checks: %u, failures: %u\n", g_checks, g_failures);
    return (g_failures == 0u) ? 0 : 1;
}
