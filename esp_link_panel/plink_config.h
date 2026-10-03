/* plink_config.h - link/message/param constants, bench CSV header, Wi-Fi/HTTP constants, parser states.
   Included by esp_link_panel.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== Link Constants ==================== */
#define ESP_LINK_BAUD_RATE          921600u
#define ESP_LINK_RX_BUFFER_SIZE     1024u
#define ESP_LINK_SOF_BYTE0          0xAAu
#define ESP_LINK_SOF_BYTE1          0x55u
/* [EN] v2 frame, must stay byte-identical to the firmware's esp_link.h:
       SOF0 SOF1 VER TYPE LEN_LO LEN_HI [payload] CRC_LO CRC_HI
   [FA] فریم نسخهٔ ۲؛ باید بایت‌به‌بایت با esp_link.h فرم‌ور یکی بماند. */
#define ESP_LINK_PROTOCOL_VERSION   2u
#define ESP_LINK_HEADER_SIZE        6u   /* SOF0 + SOF1 + ver + type + len_lo + len_hi */
#define ESP_LINK_CRC_SIZE           2u
#define ESP_LINK_CRC16_INIT         0xFFFFu
#define ESP_LINK_CRC16_POLY         0x1021u
/* [EN] 512 since v1.16 (user order 2026-09-26): PARAMS_BULK with 83
         parameters = 1 + 83 x 5 = 416 payload bytes (was 386 for 77 in
         v1.16, 191 for 38 in v1.15). The length field is u16
         little-endian. Both boards MUST flash together.
         / [FA] از v1.16 (دستور کاربر ۲۰۲۶-۰۹-۲۶): PARAMS_BULK با ۸۳ پارامتر
         = ۱ + ۸۳ × ۵ = ۴۱۶ بایت payload (قبلاً ۳۸۶ برای ۷۷ در v1.16).
         فیلد طول u16 لیتل‌اندین است. هر دو برد باید با هم فلش شوند. */
#define ESP_LINK_MAX_PAYLOAD        512u
#define ESP_LINK_TLM_SIZE          104u
#define ESP_LINK_TLM_FIELD_OFFSET   4u
#define ESP_LINK_TLM_FIELD_COUNT    25u
#define ESP_LINK_PARAM_ITEM_SIZE    5u
#define ESP_LINK_TIMEOUT_MS         1000u
#define ESP_LINK_TX_INTERVAL_MS     120u
#define ESP_LINK_KEEPALIVE_MS       1000u
#define ESP_LINK_BROWSER_LOST_MS    10000u
#define ESP_LINK_PARAM_REFRESH_MS   30000u

/* ==================== Message Types ==================== */
#define ESP_MSG_SET_PARAM           0x01u
#define ESP_MSG_GET_PARAMS          0x02u
#define ESP_MSG_TLM_LIVE            0x10u
#define ESP_MSG_PARAM_REPORT        0x11u
#define ESP_MSG_PARAMS_BULK         0x12u

/* ==================== Parameter Constants ==================== */
/* [EN] 77 since v1.16 (user order 2026-09-26): ids 20..26 = the shared
         charge profile (section 5.7), ids 27..37 = the alarms tab (27..34
         fault supervision, 35..37 charger safety ceilings), ids 38..76 =
         UI cadence (LED/beep patterns, bands, blink, thresholds, mute),
         ids 77..82 = full/hysteresis (v1.17), ids 83..92 = two-loop CC/CV charge
         PID (v1.24: five ids fewer than v1.23, whose third gain row was
         measured to buy nothing).
         / [FA] از v1.16: شناسه‌های ۲۰..۲۶ = پروفایل شارژ مشترک (بخش 5.7)،
         شناسه‌های ۲۷..۳۷ = تب آلارم‌ها (۲۷..۳۴ نظارت فالت، ۳۵..۳۷ سقف‌های
         ایمنی شارژر)، شناسه‌های ۳۸..۷۶ = اعداد UI (الگوهای LED/بوق، باندها،
         چشمک، آستانه‌ها، میوت)، شناسه‌های ۷۷..۸۲ = فول/هیسترزیس (v1.17)، شناسه‌های ۸۳..۹۲ = PID
         دوحلقه‌ای CC/CV شارژ (v1.24)، شناسه‌های ۹۳..۱۰۷ = حدها، گین‌های
         پشتیبان و تایمرهای مرحله‌ای شارژر (v1.28، دستور کاربر ۲۰۲۶-۱۰-۰۳). */
#define ESP_PARAM_COUNT            108u
#define ESP_PARAM_CHG1_ENABLE       11u
#define ESP_PARAM_CHG2_ENABLE       12u
#define ESP_PARAM_MANUAL_TEST_MODE  19u
#define ESP_TLM_FLAG_MANUAL_MODE    0x20u

/* ==================== CAL_REFERENCE (removed from the panel in v1.7) ==================== */
/* [EN] The v1.6 CAL card was dropped in the v1.7 simplification (user order 2026-09-25): the panel
        no longer sends CAL_REFERENCE. The STM32 handler stays in the firmware, unused and harmless.
   [FA] کارت CAL در ساده‌سازی نسخهٔ ۱.۷ حذف شد (دستور کاربر ۲۰۲۶-۰۹-۲۵): پنل دیگر CAL_REFERENCE
        نمی‌فرستد. handler آن در فریم‌ور STM32 مانده است؛ بی‌استفاده و بی‌ضرر. */

/* ==================== Bench Statistics Window (spec 5.5 / 5.6 v2) ==================== */
/* [EN] The browser polls /t only every 300 ms, so the bench tools read EVERY TLM frame (10 Hz) through
        this window: POST /m restarts it (and queues one GET_PARAMS so the logged parameters are the live
        ones), GET /m reads it. All 20 t[] fields are tracked (ch1 0..6, ch2 7..13, Vin 14, V24 15, V12 16,
        Vlow 17, Vhigh 18, faults 19): sum / min / max / last frame; faults are also ORed over the window;
        seq and flags are the last frame's. The count stops at 60000 frames (100 min) so u32 sums never wrap.
   [FA] مرورگر فقط هر ۳۰۰ms /t را می‌خواند؛ پس ابزارهای بنچ تک‌تک فریم‌های TLM (۱۰ هرتز) را از این پنجره
        می‌گیرند: POST /m آن را از نو شروع می‌کند (و یک GET_PARAMS صف می‌کند تا پارامترهای ثبت‌شده همان مقادیر
        زنده باشند) و GET /m آن را می‌خواند. هر ۲۰ فیلد t[] دنبال می‌شود (کانال ۱ 0..6، کانال ۲ 7..13، Vin 14،
        V24 15، V12 16، Vlow 17، Vhigh 18، خطاها 19): مجموع / کمینه / بیشینه / آخرین فریم؛ خطاها در کل پنجره
        OR هم می‌شوند؛ seq و flags مال آخرین فریم‌اند. شمارش در ۶۰۰۰۰ فریم (۱۰۰ دقیقه) می‌ایستد تا مجموع u32 سرریز نشود. */
#define ESP_STAT_FAULT_FIELD        19u
#define ESP_STAT_MAX_FRAMES         60000u

/* ==================== Bench Data Log File (spec 5.6, CSV v3: 56 data columns per row + a one-off settings line (was 149 repeated every row)) ==================== */
/* [EN] One append-only CSV on LittleFS. The panel builds each row from the /m window (every TLM frame,
        raw included) plus the typed DMM readings and POSTs it to /benchlog/add; the ESP only validates
        (printable ASCII, newline-terminated, bounded length) and appends. The column header (the comment block
        of spec 5.6; v1.26 split the 149 into 56 data columns plus 93 settings written once - v1.12: +7 charge-profile params, v1.15: +11 alarm params, v1.16: +39 UI cadence params, v1.17: +6 full/hysteresis params, v1.24: +10 two-loop PID params, v1.25: +5 raw-count calibration columns) is written by the ESP when the file is created. Appending stops at the cap (HTTP 507) and the UI warns.
        Arduino IDE: pick a flash layout WITH a file system (ESP8266 e.g. "4MB (FS:1MB)"; ESP32 default is fine).
   [FA] یک فایل CSV فقط-افزودنی روی LittleFS. پنل هر ردیف را از پنجرهٔ /m (تک‌تک فریم‌های TLM با raw)
        و عددهای مولتی‌متر می‌سازد و به /benchlog/add می‌فرستد؛ ESP فقط بررسی (ASCII قابل چاپ، پایان با
        خط جدید، طول محدود) و اضافه می‌کند. بلوک عنوان ستون‌ها (بلوک توضیح بخش 5.6؛ نسخهٔ ۱.۲۶ آن ۱۴۹ را به ۵۶ ستون داده به‌اضافهٔ ۹۳ تنظیم که یک‌بار نوشته می‌شود تقسیم کرد - v1.12: +۷ پارامتر پروفایل شارژ، v1.15: +۱۱ پارامتر آلارم، v1.16: +۳۹ پارامتر UI، v1.17: +۶ پارامتر فول/هیسترزیس، v1.24: +۱۰ پارامتر PID دوحلقه‌ای، v1.25: +۵ ستون شمارش خام برای کالیبراسیون) را ESP هنگام ساخت فایل می‌نویسد. در سقف
        اندازه افزودن متوقف می‌شود (HTTP 507) و پنل هشدار می‌دهد.
        در Arduino IDE چیدمان فلشِ دارای فایل‌سیستم را انتخاب کنید (ESP8266 مثلاً "4MB (FS:1MB)"؛ ESP32 پیش‌فرض کافی است). */
#define ESP_BENCHLOG_PATH           "/benchlog.csv"
#define ESP_BENCHLOG_MAX_BYTES      102400u
#define ESP_BENCHLOG_MAX_POST       1536u
#define ESP_BENCHLOG_HEADER \
    "# cols:\n" \
    "#  [id]     scenario,step,duty_permille,settle_ms,sample_ms,browser_ts\n" \
    "# NOTE: the settings below are NOT data columns. They do not change during a\n" \
    "#       sweep, so they are written ONCE as a '# settings:' line (same order,\n" \
    "#       comma separated) instead of repeating on all 149 columns of every\n" \
    "#       row. If any of them IS changed mid-run, a fresh '# settings:' line\n" \
    "#       is written before the next data row, so every row is still covered\n" \
    "#       by the most recent settings line above it.\n" \
    "#       تنظیمات زیر ستون داده نیستند: در طول سوییپ عوض نمی‌شوند، پس یک‌بار\n" \
    "#       به‌صورت خط '# settings:' نوشته می‌شوند نه در هر ردیف. اگر وسط کار\n" \
    "#       عوض شوند، خط تازه‌ای پیش از ردیف بعدی نوشته می‌شود.\n" \
    "#  [settings] off1,off2,gain1,gain2,voff_in,voff_24,voff_12,med,avg,\n" \
    "#           eta1,eta2,en1,en2,ceil1,ceil2,fixon1,fix1,fixon2,fix2,manual\n" \
    "#  [settings2] chg_absorb_mv,chg_absorb_enter_mv,chg_absorb_over_mv,\n" \
    "#            chg_float_mv,chg_reentry_mv,chg_bulk_imax_ma,chg_taper_ma\n" \
    "#  [settings3] alm_disc_mv,alm_disc_deb_ms,alm_absent_mv,alm_back_mv,\n" \
    "#            alm_absent_deb_ms,alm_recover_ms,alm_in_min_mv,alm_in_max_mv,\n" \
    "#            alm_hard_ma,alm_ov_mv,alm_floor_mv\n" \
    "#  [settings4] ui_ov_led_per,ui_ov_led_duty,ui_ov_beep_per,ui_ov_beep_dur,\n" \
    "#            ui_ov_beep_cnt,ui_ov_beep_gap,ui_bl_led_per,ui_bl_led_duty,\n" \
    "#            ui_bl_beep_per,ui_bl_beep_dur,ui_bl_beep_cnt,ui_bl_beep_gap,\n" \
    "#            ui_run_start,ui_run_double,ui_run_triple,ui_run_crit,\n" \
    "#            ui_run_std_per,ui_run_tri_per,ui_run_crit_per,ui_run_crit_duty,\n" \
    "#            ui_run_crit_cnt,ui_run_std_dur,ui_run_tri_dur,ui_run_crit_dur,\n" \
    "#            ui_run_std_cnt,ui_run_dbl_cnt,ui_run_tri_cnt,ui_run_gap,\n" \
    "#            ui_green_per,ui_green_min,ui_yellow_per,ui_yellow_min,\n" \
    "#            ui_ov_thr,ui_ov_hyst,ui_lowbat_thr,ui_lowbat_clr,\n" \
    "#            ui_pct_vmin,ui_pct_vmax,ui_mute,\n" \
    "#            ui_chg_full_enter,ui_chg_full_exit,ui_chg_hyst,\n" \
    "#            ui_run_hyst,ui_run_zero,ui_run_one\n" \
    "#  [settings5] pid_i_kp,pid_i_ki,pid_i_kd,pid_i_up,pid_i_dn,\n" \
    "#            pid_v_kp,pid_v_ki,pid_v_kd,pid_v_up,pid_v_dn\n" \
    "#  [settings6] lim_absorb_max_ms,lim_absorb_arm_ma,lim_absorb_hold_ms,\n" \
    "#            lim_taper_sustain_ms,lim_pid_max_step_pm,lim_pid_out_hyst,\n" \
    "#            lim_pid_volt_filt_n,lim_backstop_mv,lim_backstop_gain_i,\n" \
    "#            lim_backstop_gain_v,lim_pid_cur_margin_ma,lim_connect_settle_ms,\n" \
    "#            lim_jit_lockout_ms,lim_manual_wdg_ms,lim_ramp_down_int_ms\n" \
    "#  [ch1]    raw1,raw1_min,raw1_max,shunt1_uv,unf1,unf1_min,unf1_max,\n" \
    "#           filt1,filt1_min,filt1_max,iest1,iest1_min,iest1_max,duty1,state1\n" \
    "#  [ch2]    raw2,raw2_min,raw2_max,shunt2_uv,unf2,unf2_min,unf2_max,\n" \
    "#           filt2,filt2_min,filt2_max,iest2,iest2_min,iest2_max,duty2,state2\n" \
    "#  [glob]   seq,flags,vin_mv,v24_mv,v12_mv,vlow_mv,vhigh_mv,faults_or\n" \
    "#  [raw]    vin_counts,v24_counts,v12_counts,vrefint_counts,vdda_mv\n" \
    "#  [dmm]    dmm_i_in_ma,dmm_vin_mv,dmm_i_bat1_ma,dmm_vbat1_mv,\n" \
    "#           dmm_i_bat2_ma,dmm_vbat2_mv,note\n" \
    "# run <n> browser_ts=<ISO from the panel page> scenario=<SOLO1|SOLO2|BOTH>\n" \
    "#  duty_list=<...>\n"

/* ==================== Wi-Fi / HTTP Constants ==================== */
#define ESP_WIFI_AP_SSID            "ChangeOver-ESP"
#define ESP_WIFI_AP_PASS            "123456789"
#define ESP_HTTP_PORT               80
#define ESP_JSON_BUFFER_SIZE        2560u   /* v1.16: p[77] needs the headroom (~950 B worst case); v1.17: p[83] adds ~70 B; v1.24: p[93] adds 10 PID values (one is 5 digits), ~1.1 KB worst case */
#define ESP_HTTP_FONT_CACHE         "public, max-age=31536000"

/* ==================== Parser States ==================== */
typedef enum
{
    ESP_RX_WAIT_SOF0 = 0,
    ESP_RX_WAIT_SOF1,
    ESP_RX_WAIT_VERSION,
    ESP_RX_WAIT_TYPE,
    ESP_RX_WAIT_LEN_LO,
    ESP_RX_WAIT_LEN_HI,
    ESP_RX_WAIT_PAYLOAD,
    ESP_RX_WAIT_CRC_LO,
    ESP_RX_WAIT_CRC_HI
} esp_rx_state_t;
