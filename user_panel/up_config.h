/**
 * @file    up_config.h
 * @brief   [EN] User-panel configuration: which network it joins, which board
 *              it reads, and every cadence / budget the panel obeys.
 *              All constants live here so nothing magic is typed twice.
 *          [FA] پیکربندی پنل کاربر: به کدام شبکه وصل می‌شود، از کدام برد
 *              می‌خواند و هر بازه/سهمیه‌ای که پنل رعایت می‌کند. همهٔ ثابت‌ها
 *              اینجا هستند تا هیچ عدد جادویی دو بار تایپ نشود.
 *
 * @note    [EN] This file belongs to the USER PANEL only. It changes nothing
 *              on the STM32 side and nothing in esp_link_panel/.
 *          [FA] این فایل فقط مال پنل کاربر است؛ نه سمت STM32 چیزی عوض می‌کند
 *              و نه داخل esp_link_panel/.
 */

#ifndef UP_CONFIG_H
#define UP_CONFIG_H

/* ==================== Identity / شناسه ==================== */
#define UP_PANEL_NAME        "user panel"      /* [EN] shown in /version / [FA] در /version دیده می‌شود */
#define UP_PANEL_VERSION     "1.0"
/* [EN] Compiled in by the Arduino builder: which binary is actually in
   the box. Shown in /version and in the page footer, because "did my
   upload land?" is asked on every bench visit.
   [FA] توسط Arduino در زمان ساخت داخل می‌رود: کدام باینری واقعاً داخل جعبه
   است. در /version و پاورقی صفحه دیده می‌شود، چون «آپلودم نشست؟» سؤال
   هر بار رفتن سر بنچ است. */
#define UP_BUILD_STAMP       __DATE__ " " __TIME__
#define UP_TARGET_BOARD      "ESP8266"          /* [EN] this sketch targets the ESP8266 / [FA] این اسکچ برای ESP8266 است */

/* ==================== Access point of THIS panel / اکسس‌پوینت خود پنل ==================== */
/* [EN] The user connects to this Wi-Fi. Password must be >= 8 characters.
   [FA] کاربر به همین وای‌فای وصل می‌شود؛ گذرواژه باید حداقل ۸ نویسه باشد. */
#define UP_AP_SSID           "ChangeOver-User"
#define UP_AP_PASS           "123456789"
#define UP_AP_CHANNEL        6
/* [EN] The panel board's own LED. NodeMCU/Wemos D1 mini wire it to GPIO2
   and drive it ACTIVE LOW, so 0 = lit. Kept in config because a bare
   ESP-12 module has no LED at all and then this is simply unused.
   [FA] LED خودِ برد پنل. NodeMCU و Wemos D1 mini آن را روی GPIO2 دارند و
   فعال-کم هستند، یعنی ۰ = روشن. در پیکربندی مانده چون ماژول خالی ESP-12
   هیچ LED ندارد و آن‌وقت این ثابت بی‌استفاده است. */
#define UP_LED_PIN           2u
#define UP_LED_ACTIVE_LOW    1u
#define UP_LED_SLOW_MS       1200u   /* [EN] "nothing new" blink / [FA] چشمک «چیز تازه‌ای نیست» */
#define UP_LED_FAST_MS       180u    /* [EN] "link down" blink / [FA] چشمک «قطع لینک» */

/* ==================== Source board / بردی که خوانده می‌شود ==================== */
/* [EN] The engineering ESP that already sits on the STM32 board (AP
   "ChangeOver-ESP", bench manual section 3). The user panel only READS it.
   [FA] همان ESP مهندسی که روی برد STM32 نشسته است. پنل کاربر فقط می‌خواند. */
#define UP_STA_SSID          "ChangeOver-ESP"
#define UP_STA_PASS          "123456789"
#define UP_SRC_HOST          "192.168.4.1"
#define UP_SRC_PORT          80
#define UP_SRC_TLM_PATH      "/t"               /* [EN] compact telemetry JSON / [FA] خلاصهٔ JSON تلمتری */
#define UP_SRC_STAT_PATH     "/m"               /* [EN] live min/max window / [FA] پنجرهٔ کمینه/بیشینه */
#define UP_SRC_SET_PATH      "/s"               /* [EN] the ONLY write we ever send / [FA] تنها نوشتنی ما */

/* ==================== Cadence / بازه‌ها (ms) ==================== */
#define UP_TLM_POLL_MS       1000u              /* [EN] read /t once a second / [FA] هر ثانیه یک‌بار /t */
#define UP_STAT_POLL_MS      10000u             /* [EN] read /m for peaks / [FA] /m برای اوج‌ها */
#define UP_SAMPLE_STORE_MS   15000u             /* [EN] one stored sample every 15 s / [FA] هر ۱۵ ثانیه یک نمونه */
#define UP_TOTALS_FLUSH_MS   120000u            /* [EN] lifetime totals flush / [FA] ذخیرهٔ جمع کل */
#define UP_SERIES_REBUILD_MS 60000u             /* [EN] chart cache rebuild / [FA] بازسازی کش نمودار */
#define UP_LINK_TIMEOUT_MS   4000u              /* [EN] older than this = link down / [FA] قدیمی‌تر = قطع */
#define UP_RETRY_MIN_MS      2000u              /* [EN] reconnect backoff floor / [FA] کف فاصلهٔ اتصال مجدد */
#define UP_RETRY_MAX_MS      30000u             /* [EN] reconnect backoff ceiling / [FA] سقف فاصلهٔ اتصال مجدد */
#define UP_HTTP_TIMEOUT_MS   600u               /* [EN] client read timeout / [FA] مهلت خواندن HTTP */
#define UP_STA_CONNECT_MS    15000u             /* [EN] Wi-Fi join timeout / [FA] مهلت پیوستن به وای‌فای */

/* ==================== Telemetry shape / شکل تلمتری ==================== */
/* [EN] The frozen wire order (ESP_AGENT_SPEC.md section 6): 28 u32 words.
   [FA] ترتیب ثابت سیم (بخش ۶ سند): ۲۸ کلمهٔ ۳۲ بیتی. */
#define UP_TLM_FIELDS        28u
#define UP_TLM_MAX_FIELDS    32u                /* [EN] tolerant headroom / [FA] حاشیهٔ تحمل */
#define UP_PARAM_COUNT       128u               /* [EN] max ids we accept / [FA] بیشترین شناسهٔ پذیرفته‌شده */

/* [EN] Ids this panel reads (never writes): 11/12 charger cut state, 74/75 the
   pack-voltage-to-percent mapping the board itself uses.
   [FA] شناسه‌هایی که می‌خواند (هرگز نمی‌نویسد): ۱۱/۱۲ وضعیت قطع شارژر و
   ۷۴/۷۵ همان نگاشت ولتاژ-به-درصدِ خودِ برد. */
#define UP_PARAM_CHG1_ENABLE 11u
#define UP_PARAM_CHG2_ENABLE 12u
#define UP_PARAM_PCT_VMIN    74u
#define UP_PARAM_PCT_VMAX    75u

/* ==================== Bounds used for sanity / کران‌های سلامت داده ==================== */
#define UP_INPUT_PRESENT_MV  21000u             /* [EN] same rule the UI uses / [FA] همان قاعدهٔ UI */
#define UP_BAT_ABSENT_MV     6000u              /* [EN] below this a half is not a battery / [FA] زیر این باتری نیست */
#define UP_OVER_VOLT_MV      28500u             /* [EN] UI over-voltage face / [FA] چهرهٔ اضافه‌ولتاژ */

/* ==================== Storage / حافظه ==================== */
/* [EN] LittleFS files. The panel NEVER grows without bound: each ring file has
   a byte budget derived from the REAL flash size, and the oldest record is
   overwritten once the budget is reached (user order 2026-10-07).
   [FA] فایل‌های LittleFS. پنل هرگز بی‌کران رشد نمی‌کند: هر فایل حلقه‌ای سهمیهٔ
   بایتی دارد که از حجم واقعی فلش مشتق می‌شود و با پر شدن، قدیمی‌ترین رکورد
   بازنویسی می‌شود (دستور کاربر ۱۴۰۵/۰۷/۱۵). */
#define UP_F_SAMPLES         "/up_samples.bin"
#define UP_F_EVENTS          "/up_events.bin"
#define UP_F_DAILY           "/up_daily.bin"
#define UP_F_TOTALS          "/up_tot.bin"
#define UP_F_USERS           "/up_users.bin"
#define UP_F_AUDIT           "/up_audit.bin"   /* [EN] who did what / [FA] چه کسی چه کرد */

#define UP_FS_SAMPLE_SHARE   42u                /* [EN] percent of used budget / [FA] درصد سهمیه */
#define UP_FS_EVENT_SHARE    22u
#define UP_FS_DAILY_SHARE    10u
#define UP_FS_RESERVE_BYTES  12288u             /* [EN] never touch this much / [FA] هرگز به این مقدار دست نمی‌زنیم */
#define UP_FS_RING_MIN_BYTES 2048u              /* [EN] a ring never shrinks below this / [FA] حلقه از این کوچک‌تر نمی‌شود */
#define UP_FS_LOW_BYTES      8192u              /* [EN] below this: purge + say so / [FA] زیر این: پاک‌سازی + اعلام */
#define UP_EVENT_HIST_BUCKETS 16u               /* [EN] charge-duration histogram / [FA] هیستوگرام مدت شارژ */

/* ==================== Auth / احراز هویت ==================== */
#define UP_USERS_MAX         8u                 /* [EN] incl. the built-in admin / [FA] با احتساب مدیر پیش‌فرض */
#define UP_NAME_MAX          12u                /* [EN] stored name length / [FA] طول نام ذخیره‌شده */
#define UP_SALT_BYTES        8u
#define UP_HASH_BYTES        32u
#define UP_HASH_ITERATIONS   600u               /* [EN] access control, not crypto-grade / [FA] کنترل دسترسی، نه رمزنگاری قوی */
#define UP_SESSION_MAX       4u
#define UP_SESSION_IDLE_S    28800u             /* [EN] 8 h idle timeout / [FA] ۸ ساعت بی‌کاری */
#define UP_SESSION_COOKIE    "up_sess"
#define UP_LOGIN_FAIL_MAX    6u                 /* [EN] then a cool-down / [FA] بعد از آن قفل موقت */
#define UP_LOGIN_LOCK_MS     60000u
#define UP_SESSION_TOKEN_LEN 32u

/* [EN] Roles: what each one may do. The numbers are stored, so they are
   append-only too - never reuse a value.
   [FA] نقش‌ها: هر کدام چه اجازه‌ای دارد. اعداد ذخیره می‌شوند، پس فقط اضافه
   می‌شوند؛ هیچ‌وقت مقدار قبلی بازاستفاده نمی‌شود. */
#define UP_ROLE_VIEWER       1u                 /* [EN] read-only / [FA] فقط تماشا */
#define UP_ROLE_OPERATOR     2u                 /* [EN] + charger cut/reconnect / [FA] + قطع/وصل شارژر */
#define UP_ROLE_ADMIN        3u                 /* [EN] + users / history / logs / [FA] + کاربران/تاریخچه/گزارش */

/* [EN] Seeded on first boot. `must change` is forced at the first login so the
   panel is never left with a factory password by accident.
   [FA] در اولین بوت ساخته می‌شوند. «باید عوض شود» در اولین ورود اجباری است تا
   پنل اتفاقی با گذرواژهٔ کارخانه‌ای رها نشود. */
#define UP_SEED_ADMIN_NAME   "admin"
#define UP_SEED_ADMIN_PASS   "admin"
#define UP_SEED_USER_NAME    "user"
#define UP_SEED_USER_PASS    "user"

/* ==================== JSON buffers / بافرهای JSON ==================== */
#define UP_JSON_BUF          4096u              /* [EN] biggest reply: /api/stats and /api/series,
                                                    three arrays of 120 points / [FA] بزرگ‌ترین پاسخ:
                                                    سه آرایهٔ ۱۲۰ نقطه‌ای */
#define UP_HTTP_BUF          3200u              /* [EN] one HTTP body line / [FA] یک خط بدنهٔ HTTP */
#define UP_SERIES_POINTS     120u               /* [EN] chart decimation target / [FA] هدف کاهش نمونه‌های نمودار */
#define UP_DAY_HOURS         24u

/* [EN] Local time offset. The clock itself is UTC (an epoch second has no
   timezone) but the panel's OWNER does: the day boundary, the hour histogram and
   the dates in the Excel report are all local. Tehran is +3:30, and the default
   below is the one the panel boots with - an admin changes it with the clock,
   because a panel that guesses the region from the browser is a panel that
   trusts a browser.
   [FA] اختلاف ساعت محلی. خودِ ساعت UTC است (ثانیهٔ مطلق منطقهٔ زمانی ندارد) ولی
   صاحب پنل منطقه دارد: مرز روز، نمودار ساعتی و تاریخ‌های گزارش اکسل همه محلی‌اند.
   تهران ‎+۳:۳۰ است و پیش‌فرض زیر همان چیزی است که پنل با آن بالا می‌آید - مدیر
   همراه تنظیم ساعت عوضش می‌کند، چون پنلی که منطقه را از مرورگر حدس بزند، پنلی
   است که به مرورگر اعتماد کرده. */
#define UP_TZ_DEFAULT_OFFSET_S  12600            /* [EN] +03:30 / [FA] ‎+۳:۳۰ */
#define UP_TZ_MIN_OFFSET_S     (-43200)          /* [EN] -12:00 / [FA] ‎−۱۲:۰۰ */
#define UP_TZ_MAX_OFFSET_S      50400            /* [EN] +14:00 / [FA] ‎+۱۴:۰۰ */

/* [EN] Excel report: the widest range the page may ask for, and the ceiling on
   the per-second sheet. A full sample ring is a megabyte of XML, which would
   take minutes to pull over the panel's own access point; the summary, daily,
   session and event sheets carry the story anyway.
   [FA] گزارش اکسل: وسیع‌ترین بازه‌ای که صفحه می‌تواند بخواهد و سقف برگهٔ
   ثانیه‌ای. حلقهٔ کامل نمونه‌ها یک مگابایت XML است که روی AP خود پنل دقیقه‌ها
   طول می‌کشد؛ برگه‌های خلاصه، روزانه، شارژها و رویدادها روایت را می‌برند. */
#define UP_REPORT_MAX_DAYS      365u
#define UP_XLSX_SAMPLE_ROWS     4000u                /* [EN] hour buckets inside one day / [FA] سبدهای ساعت در یک روز */
#define UP_HEAT_DAYS         7u                 /* [EN] heat-map rows / [FA] ردیف‌های نقشهٔ حرارتی */
#define UP_SESSIONS_LIST     24u                /* [EN] sessions in the stats reply / [FA] شارژهای فهرست آمار */

#endif /* UP_CONFIG_H */
