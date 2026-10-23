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
#define SIMPLE_MSG_WIFI_CLIENT_STATE    0x33u

/* ==================== سناریوی کاهش مصرف وای‌فای روی باتری ====================
   - در زمان وجود برق ورودی: وای‌فای روشنِ دائم با بیشترین قدرت آنتن.
   - روی باتری با درصد زیر حد قطع (پیش‌فرض ۲۰٪): خاموش شدن کامل و روشن نشدن.
   - روی باتری بالای حد قطع با حضور کاربر: روشنِ دائم با بیشترین قدرت آنتن.
   - روی باتری بالای حد قطع و بدون اتصال کاربر:
     * ۳۰ دقیقهٔ اول (۱۸۰۰۰۰۰ میلی‌ثانیه): روشن با قدرت آنتن پایین.
     * پس از ۳۰ دقیقه: چرخهٔ ۲ دقیقه خاموش (۱۲۰۰۰۰ میلی‌ثانیه) و ۱ دقیقه روشن
       (۶۰۰۰۰ میلی‌ثانیه) با قدرت آنتن پایین تا زمان اتصال کاربر. */
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

/* ==================== ریست دستی قفل‌ها ====================
   اسلات‌های زمان‌اجرای STM32 (شمارندهٔ رویداد و سیکل عدم‌توازن، پرچم قفل،
   ماسک حکم باتری خراب، ریست حکم باتری ضعیف) شناسهٔ ۲۰۰..۲۰۴ دارند و پارامتر کاربر نیستند؛ فقط
   از مسیر SET_PARAM نوشته می‌شوند و مقدار صفر یعنی پاک. پنل کاربر تا
   امروز هیچ نوشتنی نداشت، پس برای کارت «ریست قفل‌ها» یک صف کوچک و یک
   فرستندهٔ SET_PARAM اضافه شد. */
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
#define SIMPLE_SLOT_TX_GAP_MS           120u /* فاصلهٔ دو فریم ریست روی لینک */

/* ==================== پارامترهای خود برد ====================
   درصد فول/خالی باتری و زمان‌بندی چشمک سبز، پارامترهای خود برد هستند
   (همان‌هایی که چراغ سبز با آن‌ها چشمک می‌زند). پنل آن‌ها را از برد
   می‌خواند تا اگر کاربر روی برد عوضشان کرد، نمایش هم عوض شود. */
#define SIMPLE_MSG_GET_PARAMS           0x02u
#define SIMPLE_MSG_PARAM_REPORT         0x11u
#define SIMPLE_MSG_PARAMS_BULK          0x12u
#define SIMPLE_PARAM_COUNT              158u
#define SIMPLE_PARAM_ITEM_SIZE          5u     /* شناسه ۱ بایت + مقدار ۴ بایت */
#define SIMPLE_PARAMS_REQ_MS            30000u /* فاصله درخواست پارامترها */

/* شناسه پارامترهای مصرفی - نام‌ها از جدول خود فرم‌ور (host_test_ui.py) */
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
#define SIMPLE_PARAM_UI_COUNT           14u    /* تعداد پارامترهایی که پنل می‌خواند */

/* ==================== وای‌فای ==================== */
#define SIMPLE_WIFI_AP_SSID             "changeover_userpannel"
#define SIMPLE_WIFI_AP_PASS             "123456789"
#define SIMPLE_WIFI_FILE                "/wifi.json"
#define SIMPLE_WIFI_SSID_MAX            32u
#define SIMPLE_WIFI_PASS_MAX            32u
#define SIMPLE_HTTP_PORT                80
#define SIMPLE_JSON_BUFFER_SIZE         1024u

/* ==================== سقف صفحه ====================
   سقف مرجع حجم صفحهٔ پنل (بایت). صفحهٔ فعلی حدود ۱۴۰ کیلوبایت است؛ اگر از
   این عدد گذشت یعنی چیزی اشتباه به صفحه اضافه شده و باید بررسی شود. */
#define SIMPLE_PANEL_HTML_MAX_BYTES     160000u

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
#define SIMPLE_USER_PASS_MIN            4u     /* کوتاه‌ترین رمز قابل قبول در فرم تغییر رمز */
#define SIMPLE_USERS_FILE               "/users.json"
#define SIMPLE_SESSION_TIMEOUT_MS       1800000u

/* ==================== زمان ==================== */
#define SIMPLE_TIME_FILE                "/time.json"
#define SIMPLE_TIME_VALID_MIN           1600000000u  /* زیر این عدد یعنی ساعت تنظیم نشده */
#define SIMPLE_TIME_SYNC_DELAY_MS       3000u        /* بعد از آمدن لینک، تا فرستادن ساعت به برد */
#define SIMPLE_TIME_RESEND_MS           600000u      /* اگر برد ساعت را نگرفت، هر ۱۰ دقیقه دوباره */
#define SIMPLE_TIME_ADOPT_DIFF_SEC      300u         /* اختلافی که بعد از آن ساعت برد (RTC) بر پنل مقدم می‌شود */

/* ==================== تاریخچهٔ وضعیت (لاگ روزانه) ====================
   دستور کاربر ۲۰۲۶-۱۰-۲۳: هر پنل تاریخچهٔ وضعیت خودش را روی فلش نگه می‌دارد،
   روزی یک فایل، و ردیف فقط وقتی نوشته می‌شود که چیزی واقعاً عوض شده باشد.
   هر ردیف همان لحظه بسته می‌شود تا قطع باتری آخرین ردیف را از بین نبرد. */
#define SIMPLE_LOG_MAX_DAYS             30u
#define SIMPLE_LOG_MAX_ROWS_PER_DAY     400u
#define SIMPLE_LOG_TAIL_LINES           30u
#define SIMPLE_LOG_INDEX_PATH           "/evt-days.txt"
#define SIMPLE_LOG_LAST_PATH            "/evt-last.txt"
#define SIMPLE_LOG_FILE_PREFIX          "/evt-"
#define SIMPLE_LOG_CUT_BAND_MV          21200u   /* مرز «نزدیک قطع» باتری */
#define SIMPLE_LOG_VSTEP_MV             200u     /* پلهٔ ولتاژ پک که تغییر حساب می‌شود */
#define SIMPLE_LOG_HEARTBEAT_MS         21600000u /* هر ۶ ساعت یک ردیف «زنده‌ام» */
#define SIMPLE_LOG_TOTAL_BUDGET_BYTES     120000u   /* از این حجم به بعد، قدیمی‌ترین روز حذف می‌شود */
#define SIMPLE_LOG_MIN_INTERVAL_MS      2000u    /* ادغام چند تغییر پشت‌سرهم در یک ردیف */
#define SIMPLE_LOG_LINK_TIMEOUT_MS      5000u    /* بی‌فریم یعنی قطع ارتباط */
#define SIMPLE_LOG_HEADER \
    "# تاریخچهٔ وضعیت / state history\n" \
    "# epoch,uptime_s,change_bits,note,mains,fl,chg1,chg2,fault_mask,flags2,v24_mv,vlow_mv,vhigh_mv,imbal_mv,dead_mask,vin_mv,v12_mv,i1_ma,i2_ma,duty1_permille,duty2_permille\n" \
    "# change_bits: 1=boot 2=mains 4=chg1 8=chg2 16=fault 32=flags 64=band 128=link 256=mode 512=dead 1024=voltage_step\n" \
    "# note: 0=none 1=low-battery restart 2=link lost 3=link back 4=day full 5=heartbeat\n" \
    "# chg states: 0 off 1 bulk 2 absorb 3 float 4 bringup 5 jit_retry 6 input_wait 7 final_fault 8 bat_lost 9 manual 10 thermal_rest\n"

/* ==================== پارامترهای خود برد ==================== */
#define SIMPLE_PARAMS_FILE              "/params.json"

/* ==================== تم ==================== */
#define SIMPLE_THEME_FILE               "/theme.json"
#define SIMPLE_THEME_MAX                1u   /* 0=تیره، 1=روشن - تم سوم حذف شد */

/* ==================== رمزنگاری فایل‌های حساس ====================
   کاربران و وای‌فای روی فلش رمزنگاری‌شده ذخیره می‌شوند تا کسی نتواند با
   خواندن یا دست‌کاری فایل، رمزها را ببیند یا عوض کند.
   - کلید از شناسهٔ یکتای خود چیپ ساخته می‌شود: SHA256(salt || chipId)
   - بدنهٔ فایل با جریان کلید HMAC-SHA256 رمز می‌شود
   - برچسب HMAC-SHA256 روی متن رمزشده می‌آید: هر دست‌کاری ⇒ فایل رد می‌شود */
#define SIMPLE_CRYPTO_TAG_LEN           16u  /* بایت برچسب HMAC در فایل */
#define SIMPLE_CRYPTO_KEY_LEN           32u  /* بایت کلید مشتق‌شده */
#define SIMPLE_CRYPTO_SALT              "ChangeOver_FS_2026_v1"

/* ==================== بازیابی رمز ادمین - با دسترسی فیزیکی ====================
   قاعده: برد روشن می‌شود، این مدت می‌گذرد و هیچ کاربری وارد نمی‌شود ⇒ پرچم
   «بوت بدون ورود» روی فلش می‌نشیند. اگر بعد از آن برق قطع و وصل شود، بوت
   بعدی کاربران را به پیش‌فرض admin/admin برمی‌گرداند. با ورود هر کاربر پرچم
   پاک می‌شود تا ریستارت معمولی رمزها را عوض نکند. */
#define SIMPLE_RECOVERY_FILE            "/boot_no_login.json"
#define SIMPLE_RECOVERY_ARM_MS          10000u   /* ۱۰ ثانیه بدون ورود */
#define SIMPLE_RECOVERY_RETRY_MS        5000u    /* فاصلهٔ تلاش دوباره اگر نوشتن نشد */

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
