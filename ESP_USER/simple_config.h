#ifndef SIMPLE_CONFIG_H
#define SIMPLE_CONFIG_H
#define SIMPLE_LINK_BAUD_RATE           921600u
#define SIMPLE_LINK_RX_BUFFER_SIZE      2048u
#define SIMPLE_LINK_SOF_BYTE0           0xAAu
#define SIMPLE_LINK_SOF_BYTE1           0x55u
#define SIMPLE_LINK_PROTOCOL_VERSION    2u
#define SIMPLE_LINK_HEADER_SIZE         6u
#define SIMPLE_LINK_CRC_SIZE            2u
#define SIMPLE_LINK_CRC16_INIT          0xFFFFu
#define SIMPLE_LINK_CRC16_POLY          0x1021u
#define SIMPLE_LINK_MAX_PAYLOAD         512u
#define SIMPLE_LINK_TLM_SIZE            128u
#define SIMPLE_LINK_TLM_FIELD_OFFSET    4u
#define SIMPLE_LINK_TLM_FIELD_COUNT     31u
#define SIMPLE_LINK_TIMEOUT_MS          1500u
#define SIMPLE_MSG_HELLO                0x40u
#define SIMPLE_MSG_HELLO_ACK            0x41u
#define SIMPLE_HELLO_LEN                8u
#define SIMPLE_HELLO_ACK_LEN            24u
#define SIMPLE_HELLO_INTERVAL_MS        2000u
#define SIMPLE_HELLO_TIMEOUT_MS         6000u
#define SIMPLE_ESP_ROLE_USER            1u
#define SIMPLE_ESP_FW_VERSION           118u
#define SIMPLE_TLM_FLAG_LINK_OK         0x40u
#define SIMPLE_MSG_TLM_LIVE             0x10u
#define SIMPLE_MSG_TIME_SYNC            0x30u
#define SIMPLE_MSG_DIAG_REQ             0x31u
#define SIMPLE_MSG_DIAG_DATA            0x32u
#define SIMPLE_MSG_WIFI_CLIENT_STATE    0x33u
#define SIMPLE_TLM_FLAG_INPUT_PRESENT   0x02u
#define SIMPLE_WIFI_IDLE_LOW_POWER_MS   1800000u
#define SIMPLE_WIFI_CYCLE_ON_MS         60000u
#define SIMPLE_WIFI_CYCLE_OFF_MS        120000u
#define SIMPLE_WIFI_CYCLE_PERIOD_MS     180000u
#define SIMPLE_WIFI_BAT_CUTOFF_PCT      20u
#define SIMPLE_WIFI_CLIENT_HOLD_MS      10000u
#define SIMPLE_WIFI_CLIENT_REPORT_MS    1000u
#define SIMPLE_WIFI_TX_MAX_DBM          20.5f
#define SIMPLE_WIFI_TX_LOW_DBM          0.0f
#define SIMPLE_WIFI_DNS_PORT            53u
/*
 * ==================== (v1.112) ====================
 */
#define SIMPLE_WIFI_VIEWER_HOLD_MS      600000u
#define SIMPLE_MSG_SET_PARAM            0x01u
#define SIMPLE_MSG_PARAM_NAK            0x15u
#define SIMPLE_SLOT_QUEUE_LEN           5u
#define SIMPLE_SLOT_EVENTS_ID           200u
#define SIMPLE_SLOT_CYCLES_ID           201u
#define SIMPLE_SLOT_LATCH_ID            202u
#define SIMPLE_SLOT_DEAD_MASK_ID        203u
#define SIMPLE_SLOT_WEAK_RESET_ID       204u
#define SIMPLE_SLOT_FIRST_ID            SIMPLE_SLOT_EVENTS_ID
#define SIMPLE_SLOT_LAST_ID             SIMPLE_SLOT_WEAK_RESET_ID
#define SIMPLE_SLOT_TX_GAP_MS           120u
#define SIMPLE_MSG_GET_PARAMS           0x02u
#define SIMPLE_MSG_PARAM_REPORT         0x11u
#define SIMPLE_MSG_PARAMS_BULK          0x12u
#define SIMPLE_PARAM_COUNT              171u
#define SIMPLE_PARAM_ITEM_SIZE          5u
#define SIMPLE_PARAMS_REQ_MS            30000u
#define SIMPLE_PARAM_GREEN_PERIOD_MS    66u
#define SIMPLE_PARAM_GREEN_MIN_OFF_MS   67u
#define SIMPLE_PARAM_YELLOW_PERIOD_MS   68u
#define SIMPLE_PARAM_YELLOW_MIN_ON_MS   69u
#define SIMPLE_PARAM_PCT_VMIN_MV        74u
#define SIMPLE_PARAM_PCT_VMAX_MV        75u
#define SIMPLE_PARAM_FULL_ENTER_PCT     77u
#define SIMPLE_PARAM_FULL_EXIT_PCT      78u
#define SIMPLE_PARAM_CHG_HYST_PCT       79u
#define SIMPLE_PARAM_RUN_HYST_PCT       80u
#define SIMPLE_PARAM_RUN_ZERO_EXIT      81u
#define SIMPLE_PARAM_RUN_ONE_EXIT       82u
#define SIMPLE_PARAM_CHG_PCT_VMIN_MV    119u
#define SIMPLE_PARAM_CHG_PCT_VMAX_MV    120u
#define SIMPLE_PARAM_WIFI_IDLE_LOW_POWER_MS 143u
#define SIMPLE_PARAM_WIFI_CYCLE_ON_MS       144u
#define SIMPLE_PARAM_WIFI_CYCLE_OFF_MS      145u
#define SIMPLE_PARAM_WIFI_BAT_CUTOFF_PCT    146u
#define SIMPLE_PARAM_INPUT_MIN_MV         33u
#define SIMPLE_PARAM_INPUT_MAX_MV         34u
#define SIMPLE_PARAM_INPUT_HYST_LOW_MV    169u
#define SIMPLE_PARAM_INPUT_HYST_HIGH_MV   170u
#define SIMPLE_PARAM_OV_HYST_MV          71u
#define SIMPLE_PARAM_UI_COUNT           19u
#define SIMPLE_WIFI_AP_SSID             "changeover_userpannel"
#define SIMPLE_WIFI_AP_PASS             "123456789"
#define SIMPLE_WIFI_FILE                "/wifi.json"
#define SIMPLE_WIFI_SSID_MAX            32u
#define SIMPLE_WIFI_PASS_MAX            32u
#define SIMPLE_HTTP_PORT                80
#define SIMPLE_JSON_BUFFER_SIZE         1024u
#define SIMPLE_PANEL_HTML_MAX_BYTES     175000u
#define SIMPLE_CHART_HISTORY_SIZE       60u
#define SIMPLE_CHART_1D_SIZE            24u
#define SIMPLE_CHART_7D_SIZE            28u
#define SIMPLE_CHART_1M_SIZE            30u
#define SIMPLE_CHART_3M_SIZE            90u
#define SIMPLE_CHARTS_FILE              "/charts.bin"
#define SIMPLE_CHARTS_LEGACY_FILE       "/charts.json"
#define SIMPLE_CHART_FILE_HEADER        8u
#define SIMPLE_CHART_FILE_MAX_BYTES     2048u
#define SIMPLE_DIAG_MAX_ERRORS          10u
#define SIMPLE_DIAG_MAX_POWER_EVENTS    20u
#define SIMPLE_USERS_MAX                1u
#define SIMPLE_USER_NAME_MAX            24u
#define SIMPLE_USER_PASS_MAX            32u
#define SIMPLE_USER_PASS_MIN            4u
#define SIMPLE_USERS_FILE               "/users.json"
#define SIMPLE_SESSION_TIMEOUT_MS       1800000u
#define SIMPLE_SESSION_COOKIE           "co_sess"
#define SIMPLE_SESSION_TOKEN_LEN        64u
#define SIMPLE_TIME_FILE                "/time.json"
#define SIMPLE_TIME_VALID_MIN           1600000000u
#define SIMPLE_TIME_SYNC_DELAY_MS       3000u
#define SIMPLE_TIME_RESEND_MS           600000u
#define SIMPLE_TIME_ADOPT_DIFF_SEC      300u
/* [EN] After a power cut the clock is restored from the flash, but the board
 * is the one that kept counting.  Rows wait this long for the board's time so
 * the first rows of the boot never land on a stale day. */
#define SIMPLE_TIME_BOOT_GRACE_MS       8000u
#define SIMPLE_LOG_MAX_DAYS             90u
/* [EN] Three months kept.  A realistic day is ~14 rows = ~285 B with the
 * compact codec, so 90 days stay near 26 kB - far inside the 120 kB budget. */
#define SIMPLE_LOG_MAX_ROWS_PER_DAY     400u
#define SIMPLE_LOG_TAIL_LINES           30u
#define SIMPLE_LOG_DAY_AUTO             0xFFFFFFFFu
#define SIMPLE_LOG_DAY_ZERO             0xFFFFFFFEu
#define SIMPLE_LOG_RAM_ROWS             16u
#define SIMPLE_LOG_RAM_ROW_MAX          160u
#define SIMPLE_LOG_INDEX_PATH           "/evt-days.txt"
#define SIMPLE_LOG_SCAN_MIN_MS          2000u
#define SIMPLE_LOG_LAST_PATH            "/evt-last.txt"
#define SIMPLE_LOG_FILE_PREFIX          "/evt-"
#define SIMPLE_LOG_CUT_BAND_MV          21200u
#define SIMPLE_LOG_BAND_OFF_MV          21400u
#define SIMPLE_LOG_VSTEP_MIN_MS         60000u
#define SIMPLE_LOG_VSTEP_MV             200u
#define SIMPLE_LOG_HEARTBEAT_MS         3600000u
#define SIMPLE_LOG_DRAIN_RETRY_MS       30000u
#define SIMPLE_HTTP_SEND_TIMEOUT_MS     1000u
#define SIMPLE_HTTP_ROOT_MAX_MS         15000u
#define SIMPLE_CHARGE_HISTORY_FILE      "/charge_hist.json"
#define SIMPLE_DIAG_FILE                "/diag.json"
#define SIMPLE_LOG_TOTAL_BUDGET_BYTES     120000u
/* [EN] The log may never eat the whole flash: the settings, the charts, the
 * diagnostics and the charge history live there too.  On a 64 kB file system
 * this leaves 44 kB for the history, i.e. three months of normal days. */
#define SIMPLE_LOG_RESERVE_BYTES          20000u
#define SIMPLE_LOG_MIN_BUDGET_BYTES       16000u
#define SIMPLE_LOG_MIN_INTERVAL_MS      2000u
/*
 * [EN] A queued mains cut/return is written after this short coalescing
 * window (not the full merge window): the board may cut the panel's
 * power the same instant (battery saver) and the row must reach the
 * flash first (user order 2026-10-24: "the log must be complete").
 * Repeat flips inside the window still ride along in ONE row.
 */
#define SIMPLE_LOG_MAINS_FLUSH_MS       250u
#define SIMPLE_LOG_LINK_TIMEOUT_MS      5000u
#define SIMPLE_LOG_HEADER \
    "# تاریخچهٔ وضعیت / state history\n" \
    "# epoch,uptime_s,change_bits,note,mains,fl,chg1,chg2,fault_mask,flags2,v24_mv,vlow_mv,vhigh_mv,imbal_mv,dead_mask,vin_mv,v12_mv,i1_ma,i2_ma,duty1_permille,duty2_permille,batt_pct\n" \
    "# batt_pct: درصد باتری از نگاشت دشارژ (پارامتر ۷۴/۷۵، کارخانه ۲۱۰۰۰..۲۹۰۰۰ میلی‌ولت) / battery percent from the discharge map\n" \
    "# change_bits: 1=boot 2=mains 4=chg1 8=chg2 16=fault 32=flags 64=band 128=link 256=mode 512=dead 1024=voltage_step\n" \
    "# note: 0=none 1=low-battery restart 2=link lost 3=link back 4=day full 5=heartbeat 6=manual\n" \
    "# chg states: 0 off 1 bulk 2 absorb 3 float 4 bringup 5 jit_retry 6 input_wait 7 final_fault 8 bat_lost 9 manual 10 thermal_rest\n"
#define SIMPLE_PARAMS_FILE              "/params.json"
#define SIMPLE_THEME_FILE               "/theme.json"
#define SIMPLE_THEME_MAX                0u
#define SIMPLE_CRYPTO_TAG_LEN           16u
#define SIMPLE_CRYPTO_KEY_LEN           32u
#define SIMPLE_CRYPTO_SALT              "ChangeOver_FS_2026_v1"
#define SIMPLE_RECOVERY_FILE            "/boot_no_login.json"
#define SIMPLE_RECOVERY_ARM_MS          10000u
#define SIMPLE_RECOVERY_RETRY_MS        5000u
typedef enum
{
    SIMPLE_RX_WAIT_SOF0 = 0,
    SIMPLE_RX_WAIT_SOF1,
    SIMPLE_RX_WAIT_VERSION,
    SIMPLE_RX_WAIT_TYPE,
    SIMPLE_RX_WAIT_LEN_LO,
    SIMPLE_RX_WAIT_LEN_HI,
    SIMPLE_RX_WAIT_PAYLOAD,
    SIMPLE_RX_WAIT_CRC_LO,
    SIMPLE_RX_WAIT_CRC_HI
} simple_rx_state_t;
#endif
