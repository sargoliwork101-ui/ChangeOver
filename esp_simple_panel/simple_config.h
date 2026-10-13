/* simple_config.h - تنظیمات پنل ساده کاربر
   فقط فارسی - این پوشه استثنای قانون دو زبانه است */

#ifndef SIMPLE_CONFIG_H
#define SIMPLE_CONFIG_H

/* ==================== لینک سریال ==================== */
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

/* ==================== انواع پیام ==================== */
#define SIMPLE_MSG_TLM_LIVE             0x10u
#define SIMPLE_MSG_TIME_SYNC            0x30u
#define SIMPLE_MSG_DIAG_REQ             0x31u
#define SIMPLE_MSG_DIAG_DATA            0x32u

/* ==================== وای‌فای ==================== */
#define SIMPLE_WIFI_AP_SSID             "changeover_userpannel"
#define SIMPLE_WIFI_AP_PASS             "123456789"
#define SIMPLE_WIFI_FILE                "/wifi.json"
#define SIMPLE_WIFI_SSID_MAX            32u
#define SIMPLE_WIFI_PASS_MAX            32u
#define SIMPLE_HTTP_PORT                80
#define SIMPLE_JSON_BUFFER_SIZE         1024u

/* ==================== سقف صفحه ==================== */
#define SIMPLE_PANEL_HTML_MAX_BYTES     120000u

/* ==================== نمودارها - با محور زمان ==================== */
#define SIMPLE_CHART_HISTORY_SIZE       60u
#define SIMPLE_CHART_1D_SIZE            24u   /* ۱ روزه: ۲۴ نقطه هر ساعت */
#define SIMPLE_CHART_7D_SIZE            28u   /* ۷ روزه: ۲۸ نقطه هر ۶ ساعت */
#define SIMPLE_CHART_1M_SIZE            30u   /* ۱ ماهه: ۳۰ نقطه هر روز */
#define SIMPLE_CHART_3M_SIZE            90u   /* ۳ ماهه: ۹۰ نقطه هر روز */
#define SIMPLE_CHARTS_FILE              "/charts.json"

/* ==================== دیاگ ==================== */
#define SIMPLE_DIAG_MAX_ERRORS          10u
#define SIMPLE_DIAG_MAX_POWER_EVENTS    20u

/* ==================== کاربر ==================== */
#define SIMPLE_USERS_MAX                8u
#define SIMPLE_USER_NAME_MAX            24u
#define SIMPLE_USER_PASS_MAX            32u
#define SIMPLE_USERS_FILE               "/users.json"
#define SIMPLE_SESSION_TIMEOUT_MS       1800000u

/* ==================== زمان ==================== */
#define SIMPLE_TIME_FILE                "/time.json"

/* ==================== تم ==================== */
#define SIMPLE_THEME_FILE               "/theme.json"
#define SIMPLE_THEME_MAX                2u

/* ==================== وضعیت پارسر ==================== */
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
