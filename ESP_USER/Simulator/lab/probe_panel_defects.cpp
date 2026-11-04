/*
 * @file    probe_panel_defects.cpp
 * @brief   Reproduce the panel defects on a host, using the real headers.
 * @note    The lab server concatenates this file after the stub block of
 *          Tester/host_test_latch.cpp, so SimSerialLink, SimWiFi and the
 *          little file-system shim come from the project itself. Nothing in
 *          ESP_USER is modified: the probe only includes its headers.
 * @note    Every line printed below was produced by the panel's own code
 *          compiled with -fsanitize=address,undefined.
 */
#include <cstdio>
#include <cstring>
static void func__P_Line(const char *what, bool ok, const char *detail)
{
    (void)printf("  %-56s %-8s %s\n", what, ok ? "PROVEN" : "not", detail);
}
int main(void)
{
    (void)printf("== probe: ESP_USER panel defects ==\n");
    /* 1) ReadU32 takes a uint8_t offset, callers pass up to 320. */
    {
        uint8_t buf[512];
        uint16_t i;
        for (i = 0u; i < 512u; i++)
        {
            buf[i] = (uint8_t)(i & 0xFFu);
        }
        (void)printf("[1] ReadU32(offset=4)   = %u\n", func__Simple_ReadU32(buf, 4u));
        (void)printf("[1] ReadU32(offset=260) = %u  (260 was truncated to 4)\n",
                     func__Simple_ReadU32(buf, (uint8_t)260u));
    }
    /* 2) A full diag chunk: 10 error records, each 32 bytes from offset 3. */
    {
        uint16_t idx;
        UINT8_T__G__RxType = SIMPLE_MSG_DIAG_DATA;
        UINT8_T__G__RxPayload[0] = 1u;
        UINT8_T__G__RxPayload[1] = 0u;
        UINT8_T__G__RxPayload[2] = 10u;
        for (idx = 0u; idx < 10u; idx++)
        {
            uint16_t base = (uint16_t)(3u + (uint16_t)idx * 32u);
            uint32_t ts = 1700000000u + (uint32_t)idx * 1000u;
            uint8_t k;
            for (k = 0u; k < 32u; k++)
            {
                UINT8_T__G__RxPayload[base + k] = 0u;
            }
            UINT8_T__G__RxPayload[base + 0u] = (uint8_t)(ts & 0xFFu);
            UINT8_T__G__RxPayload[base + 1u] = (uint8_t)((ts >> 8) & 0xFFu);
            UINT8_T__G__RxPayload[base + 2u] = (uint8_t)((ts >> 16) & 0xFFu);
            UINT8_T__G__RxPayload[base + 3u] = (uint8_t)((ts >> 24) & 0xFFu);
        }
        UINT16_T__G__RxLen = (uint16_t)(3u + 10u * 32u);
        BOOL__G__DiagClearPending = false;
        func__Simple_HandleFrame();
        (void)printf("[2] diag frame len=%u, 10 records sent, 10 stored\n",
                     (unsigned)UINT16_T__G__RxLen);
        for (idx = 0u; idx < 10u; idx++)
        {
            uint32_t want = 1700000000u + (uint32_t)idx * 1000u;
            uint32_t got = SIMPLE_DIAG_ERROR_T__G__Errors[idx].timestamp;
            (void)printf("[2]   rec %2u  stored=%lu  expected=%lu  %s\n",
                         (unsigned)idx, (unsigned long)got, (unsigned long)want,
                         (got == want) ? "ok" : "WRONG");
        }
    }
    /* 3) Power cycle without login resets every account. */
    LittleFS.format();
    BOOL__G__FsOk = LittleFS.begin();
    func__Simple_CryptoKeyInit();
    func__Simple_AuthLoad();
    (void)printf("[3] fresh flash: admin/admin valid = %d\n",
                 func__Simple_VerifyUserPass("admin", "admin") ? 1 : 0);
    (void)func__Simple_SetUserPass("admin", "MyStrongPass1");
    (void)printf("[3] owner sets MyStrongPass1, valid = %d\n",
                 func__Simple_VerifyUserPass("admin", "MyStrongPass1") ? 1 : 0);
    UINT32_T__G__LastRecoveryTryMs = (uint32_t)millis() - 1000000u;
    func__Simple_RecoveryArm();
    (void)printf("[3] boot with no login for 10 s -> recovery flag = %d\n",
                 func__Simple_RecoveryFlagExists() ? 1 : 0);
    func__Simple_AuthLoad();
    func__Simple_RecoveryApply();
    {
        int adminAgain = func__Simple_VerifyUserPass("admin", "admin") ? 1 : 0;
        int strongStill = func__Simple_VerifyUserPass("admin", "MyStrongPass1") ? 1 : 0;
        (void)printf("[3] next boot: admin/admin = %d, MyStrongPass1 = %d, recovery flag = %d, users = %u\n",
                     adminAgain, strongStill, BOOL__G__RecoveryHappened ? 1 : 0,
                     (unsigned)UINT8_T__G__UserCount);
    }
    /* 4) A damaged users file silently restores the factory account. */
    (void)func__Simple_SetUserPass("admin", "AnotherGoodPass");
    {
        File file__f = LittleFS.open(SIMPLE_USERS_FILE, "w");
        file__f.print("{\"e\":1,\"n\":1,\"c\":\"00\",\"m\":\"00\"}");
        file__f.close();
    }
    func__Simple_AuthLoad();
    {
        int factoryAccount = func__Simple_VerifyUserPass("admin", "admin") ? 1 : 0;
        int ownerPassword = func__Simple_VerifyUserPass("admin", "AnotherGoodPass") ? 1 : 0;
        (void)printf("[4] truncated users file: admin/admin = %d, AnotherGoodPass = %d\n",
                     factoryAccount, ownerPassword);
    }
    /* 5) One account only: a legacy three-user file must collapse to admin. */
    {
        uint8_t usersBefore;
        uint8_t usersAfter;
        int otherName;
        int adminName;
        String legacy = "{\"users\":[{\"name\":\"admin\",\"pass\":\"x\",\"role\":1},"
                        "{\"name\":\"viewer1\",\"pass\":\"y\",\"role\":0},"
                        "{\"name\":\"viewer2\",\"pass\":\"z\",\"role\":0}]}";
        usersBefore = UINT8_T__G__UserCount;
        func__Simple_AuthParse(legacy);
        usersAfter = UINT8_T__G__UserCount;
        adminName = (func__Simple_FindUser("admin") >= 0) ? 1 : 0;
        otherName = (func__Simple_FindUser("viewer1") >= 0) ? 1 : 0;
        (void)printf("[5] SIMPLE_USERS_MAX = %u, users before = %u, after a 3-user file = %u,"
                     " admin kept = %d, viewer1 kept = %d\n",
                     (unsigned)SIMPLE_USERS_MAX, (unsigned)usersBefore, (unsigned)usersAfter,
                     adminName, otherName);
        (void)func__Simple_AuthDefaults();
    }
    /* 6) Real row size against the log budget. */
    {
        char row[256];
        UINT32_T__G__TlmField[6] = 3u;
        UINT32_T__G__TlmField[13] = 2u;
        UINT32_T__G__TlmField[19] = 0x00000201u;
        UINT32_T__G__TlmField[15] = 25400u;
        UINT32_T__G__TlmField[17] = 12600u;
        UINT32_T__G__TlmField[18] = 12900u;
        UINT32_T__G__TlmField[25] = 120u;
        UINT32_T__G__TlmField[14] = 24100u;
        UINT32_T__G__TlmField[16] = 12000u;
        UINT32_T__G__TlmField[3] = 42u;
        UINT32_T__G__TlmField[10] = 77u;
        UINT32_T__G__TlmField[5] = 123456u;
        UINT32_T__G__TlmField[12] = 654321u;
        UINT8_T__G__TlmFlags = 3u;
        func__Simple_TimeSet(1762000000u, false);
        func__SimpleLog_BuildRow(row, sizeof(row), 0x0007u, 0u);
        (void)printf("[6] row = %u bytes -> 400 rows = %u bytes/day, budget %u -> %u full days\n",
                     (unsigned)strlen(row), (unsigned)((strlen(row) + 1u) * 400u),
                     (unsigned)SIMPLE_LOG_TOTAL_BUDGET_BYTES,
                     (unsigned)(SIMPLE_LOG_TOTAL_BUDGET_BYTES / ((strlen(row) + 1u) * 400u)));
    }
    /* 7) Panel size: the firmware literal, measured by the lab, against its cap. */
    (void)printf("[7] panel html literal = %u bytes, cap = %u bytes\n",
                 (unsigned)PANEL_LITERAL_BYTES,
                 (unsigned)SIMPLE_PANEL_HTML_MAX_BYTES);
    /* 8) One session at a time. */
    {
        int firstLogin;
        int secondDevice;
        int viewerLogin;
        func__Simple_AuthLogout();
        UINT32_T__G__SessionStartMs = 0u;
        firstLogin = func__Simple_AuthLogin("admin", "admin");
        secondDevice = func__Simple_AuthLogin("admin", "admin");
        viewerLogin = func__Simple_AuthLogin("bob", "1");
        (void)printf("[8] login admin = %d, same admin from a 2nd device = %d, viewer = %d\n",
                     firstLogin, secondDevice, viewerLogin);
    }
    (void)printf("== end ==\n");
    return 0;
}
