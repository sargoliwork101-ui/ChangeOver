/* plink_config.h - link/message/param constants, bench CSV header, Wi-Fi/HTTP constants, parser states.
   Included by esp_link_panel.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== Link Constants ==================== */
#define ESP_LINK_BAUD_RATE          921600u
/* [EN] RX ring headroom. The panel page/bench-log handlers drain the ring
       from inside their send loops (func__Esp_DrainSerial), but streamFile
       and other WebServer internals cannot be drained from; 2048 gives
       ~2 s of telemetry headroom at the 100 ms cadence, so a blocking
       stretch no longer costs frames (1024 overflowed on slow clients and
       every lost frame showed up as the panel's yellow CRC warning).
   [FA] حاشیهٔ حلقهٔ RX. هندلرهای صفحه/لاگ بنچ حلقه را از داخل حلقهٔ ارسال
       تخلیه می‌کنند ‎(func__Esp_DrainSerial)‎، ولی ‎streamFile‎ و بخش‌های
       داخلی وب‌سرور از بیرون قابل تخلیه نیستند؛ ‎2048‎ حدود ‎2s‎ حاشیه با
       کادانس ‎100ms‎ می‌دهد تا یک کشیدگی بلوکه‌کننده دیگر فریم هزینه نکند. */
#define ESP_LINK_RX_BUFFER_SIZE     2048u
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
/* [EN] Current v2 link: PARAMS_BULK uses a u16 length and a 512-byte
         payload ceiling. Each chunk carries at most 102 id/value items
         (`1 + 102 x 5 = 511` payload bytes); the 143-id parameter space is
         therefore sent in multiple chunks. Both boards MUST flash together.
         / [FA] لینک v2 فعلی: طول PARAMS_BULK شانزده‌بیتی و سقف payload برابر
         ۵۱۲ بایت است. هر تکه حداکثر ۱۰۲ جفت شناسه/مقدار می‌برد (۱ + ۱۰۲×۵ =
         ۵۱۱ بایت) و فضای ۱۴۳شناسه‌ای در چند تکه ارسال می‌شود. هر دو برد باید
         با هم فلش شوند. */
#define ESP_LINK_MAX_PAYLOAD        512u
#define ESP_LINK_TLM_SIZE          128u
#define ESP_LINK_TLM_FIELD_OFFSET   4u
#define ESP_LINK_TLM_FIELD_COUNT    31u
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
/* [EN] v1.66 direct LUT push (user order 2026-10-05). Mirror of the STM32
   ESPLINK_MSG_LUT_* ids - the table gets its OWN messages and its OWN flash
   block on the board, so it never travels as parameters and never touches
   the parameter record.
   [FA] ارسال مستقیم جدول (v1.66): آینهٔ شناسه‌های STM32. جدول پیام‌های خودش و
   بلوک فلش خودش را دارد، پس هرگز به شکل پارامتر سفر نمی‌کند و رکورد
   پارامترها را دست نمی‌زند. */
#define ESP_MSG_LUT_BEGIN           0x04u
#define ESP_MSG_LUT_CHUNK           0x05u
#define ESP_MSG_LUT_COMMIT          0x06u
#define ESP_MSG_LUT_RESET           0x07u
#define ESP_MSG_LUT_READ            0x08u
#define ESP_MSG_LUT_ACK             0x13u
#define ESP_MSG_LUT_DATA            0x14u
/* [EN] Per-channel point cap, identical to CAL_LUT_POINTS_MAX on the board.
   [FA] سقف نقاط هر کانال، برابر CAL_LUT_POINTS_MAX روی برد. */
#define ESP_LUT_POINTS_MAX          24u

/* ==================== Parameter Constants ==================== */
/* [EN] Current parameter map: ids 20..26 = shared charge profile; 27..37 =
         Fault/Charger supervision; 38..82 = UI cadence and hysteresis;
         83..92 = two-loop CC/CV PID; 93..107 = charger limits; 108..118 =
         imbalance scenario 5; 119..120 = charge-side percent map;
         121..122 = band-2 beep shape; 123..124 = imbalance LED cadence;
         125..131 and 134..135 = dead-battery scenario 6; 132..133 and 136
         = imbalance extensions; and 137..142 = technical board fault scenario 7.
         / [FA] نقشهٔ فعلی پارامترها: ۲۰..۲۶ پروفایل شارژ، ۲۷..۳۷ نظارت ‎Fault/Charger‎،
         ۳۸..۸۲ cadence و hysteresis رابط، ۸۳..۹۲ PID دوحلقه‌ای، ۹۳..۱۰۷ حدهای
         شارژر، ۱۰۸..۱۱۸ سناریوی ۵، ۱۱۹..۱۲۰ نگاشت درصد سمت شارژ، ۱۲۱..۱۲۲
         شکل بوق باند ۲، ۱۲۳..۱۲۴ cadence چراغ عدم‌توازن، ۱۲۵..۱۳۱ و ۱۳۴..۱۳۵
         سناریوی ۶، ۱۳۲..۱۳۳ و ۱۳۶ توسعهٔ سناریوی ۵ و ۱۳۷..۱۴۲ سناریوی ۷ خطای فنی برد. */
#define ESP_PARAM_COUNT            143u /* [EN] v1.84 ends at technical-fault id 142; ids 137..142 own scenario 7. / [FA] نسخه ۱٫۸۴ با شناسهٔ ۱۴۲ سناریوی خطای فنی تمام می‌شود؛ ۱۳۷..۱۴۲ متعلق به سناریوی ۷ است. */
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
   [FA] مرورگر فقط هر ۳۰۰‎ms /t‎ را می‌خواند؛ پس ابزارهای بنچ تک‌تک فریم‌های TLM (۱۰ هرتز) را از این پنجره
        می‌گیرند: ‎POST /m‎ آن را از نو شروع می‌کند (و یک GET_PARAMS صف می‌کند تا پارامترهای ثبت‌شده همان مقادیر
        زنده باشند) و ‎GET /m‎ آن را می‌خواند. هر ۲۰ فیلد t[] دنبال می‌شود (کانال ۱ ‎0..6‎، کانال ۲ ‎7..13‎، Vin 14،
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
        و عددهای مولتی‌متر می‌سازد و به /‎benchlog/add‎ می‌فرستد؛ ESP فقط بررسی (ASCII قابل چاپ، پایان با
        خط جدید، طول محدود) و اضافه می‌کند. بلوک عنوان ستون‌ها (بلوک توضیح بخش 5.6؛ نسخهٔ ۱.۲۶ آن ۱۴۹ را به ۵۶ ستون داده به‌اضافهٔ ۹۳ تنظیم که یک‌بار نوشته می‌شود تقسیم کرد - v1.12: +۷ پارامتر profile شارژ، v1.15: +۱۱ پارامتر آلارم، v1.16: +۳۹ پارامتر UI، v1.17: +۶ پارامتر فول/hysteresis، v1.24: +۱۰ پارامتر PID دوحلقه‌ای، v1.25: +۵ ستون شمارش خام برای کالیبراسیون) را ESP هنگام ساخت فایل می‌نویسد. در سقف
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
    "#            ui_ov_thr,ui_ov_hyst,retired_72,retired_73,\n" \
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
    "#  [settings7] imb_rest_mv,imb_disch_mv,imb_rest_wait_ms,imb_chg_wait_ms,\n" \
    "#            imb_event_ms,imb_event_hys_mv,imb_event_max,imb_beep_per_ms,\n" \
    "#            imb_beep_ms,imb_block_out,imb_chg_cycles\n" \
    "#  [settings8] ui_chg_pct_vmin,ui_chg_pct_vmax,ui_run_dbl_dur,ui_run_dbl_gap,\n" \
    "#            imb_blink_per_ms,imb_blink_duty,\n" \
    "#            dead_timeout_ms,dead_reset_gap_ms,dead_block_out,\n" \
    "#            dead_beep_per_ms,dead_beep_ms,dead_blink_per_ms,dead_blink_duty,\n" \
    "#            imb_beep_count,imb_beep_gap_ms,dead_beep_count,dead_beep_gap_ms,\n" \
    "#            imb_clean_full_cycles,tech_beep_per_ms,tech_beep_len_ms,\n" \
    "#            tech_beep_count,tech_beep_gap_ms,tech_led_per_ms,tech_led_duty\n" \
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
#define ESP_JSON_BUFFER_SIZE        2624u   /* v1.16: p[77] needs the headroom (~950 B worst case); v1.17: p[83] adds ~70 B; v1.24: p[93] adds 10 PID values, ~1.1 KB; v1.28: p[108] limits; v1.43: p[119] + t[28] + fl2 adds ~120 B */
#define ESP_HTTP_FONT_CACHE         "public, max-age=31536000"
/* [EN] Ceiling for the web panel, checked by a static_assert in plink_http.h.
 *      The panel is streamed from PROGMEM in small HTTP chunks and was 118 KB
 *      when this project started; it is 194 KB now and nothing ever stopped
 *      it growing. Chunking keeps a stalled ESP8266 client from losing the
 *      tail, which contains the settings sub-pages and the scripts.
 *      The ceiling turns the next large addition into a compile error that
 *      has to be answered on purpose. v1.43 (user order 2026-10-04, charge
 *      scenario rebuilt into one usable page: definition of full, the mV
 *      ladder moved next to it and the yellow-blink arithmetic shown) is
 *      such an answer, and so is v1.44, which gave the five remaining
 *      scenario cards the same treatment (what triggers them, the numbered
 *      settings and the derived numbers the board will actually use): the
 *      page needed ~16 KB more, so the step went 208 -> 224 -> 256 KB in two
 *      deliberate moves, each tied to a named user order, not a drift.
 *      248 KB of markup is still safe in PROGMEM (the sketch uses a fraction
 *      of the 1 MB image); the HTTP response is chunked so transfer size is
 *      not confused with one giant socket write.
 * [FA] سقف پنل وب، با static_assert در plink_http.h بررسی می‌شود. پنل با یک
 *      send_P می‌رود و اول کار ۱۱۸ کیلوبایت بود؛ حالا ۱۹۴ است و هیچ‌چیز جلوی
 *      رشدش را نگرفته بود. روی ESP8266 یعنی حدود ۱۴۰ نوشتن پیاپی TCP، و لینکِ
 *      گیرکرده انتقال را هر جا که باشد می‌برد — مرورگر صفحه‌ای بدون انتهایش را
 *      نشان می‌دهد، یعنی بدون زیرصفحه‌های تنظیمات و اسکریپت‌ها. ۲۰۸ کیلوبایت
 *      سقف، افزودنی بزرگ بعدی را به خطای بیلد تبدیل می‌کند که باید آگاهانه
 *      جواب داده شود. نسخهٔ ۱٫۴۳ (دستور کاربر ۲۰۲۶-۱۰-۰۴: کارت سناریوی شارژ
 *      یک صفحهٔ کاربردی شود — تعریف فول، آوردن حد ولتاژ کنارش و نمایش حساب
 *      چشمک زرد) همان جواب است: صفحه حدود ۸ کیلوبایت بیشتر از پلهٔ ۲۰۸ لازم
 *      داشت، و نسخهٔ ۱٫۴۴ همین کار را برای پنج کارت سناریوی باقی‌مانده کرد
 *      (شرط ورود، تنظیم‌های شماره‌دار و اعداد محاسبه‌شده‌ای که برد به‌کار
 *      می‌برد) که ~۱۶ کیلوبایت دیگر خواست؛ پس پله در دو حرکت عمدی از ۲۰۸ به
 *      ۲۲۴ و بعد ۲۵۶ کیلوبایت رفت — هر دو پای یک دستور مشخص، نه رانش.
 *
 *      v1.52 (دستور کاربر ۲۰۲۶-۱۰-۰۵: یک کلید سراسری برای ارسال تنظیمات و
 *      پیام تأیید بعد از دست‌دادن، به‌علاوهٔ شبیه‌ساز زندهٔ کارت‌ها در v1.51)
 *      حدود ۱۲ کیلوبایت دیگر لازم داشت؛ پله یک حرکت عمدی دیگر از ۲۵۶ به
 *      ۲۷۲ کیلوبایت رفت — باز هم پای یک دستور مشخص، نه رانش.
 *      v1.52 (user order): the global send button, its handshake report and
 *      the v1.51 per-card simulator needed ~12 KiB more, so the ceiling was
 *      deliberately stepped 256 KiB -> 272 KiB.
 *
 *      v1.57 (دستور کاربر ۲۰۲۶-۱۰-۰۵: «پشتیبان‌گیری را کامل کن» و
 *      «کالیبراسیون را مستقیم از داده‌برداری بنچ با تأیید کاربر روی برد
 *      بریز») شناسنامهٔ فایل پشتیبان، عبور دادن فایل از قوانین مشترک، و
 *      کارت کالیبراسیون خودکار (برازش کمترین‌مربعات، پیش‌نمایش مقدار فعلی/
 *      پیشنهادی، هشدار کیفیت و پشتیبان خودکار پیش از نوشتن) حدود ۱۳
 *      کیلوبایت لازم داشت؛ پله یک حرکت عمدی دیگر از ۲۷۲ به ۲۸۸ کیلوبایت.
 *      v1.57 (user order): the backup identity stamp and the bench
 *      auto-calibration card needed ~13 KiB, so the ceiling was deliberately
 *      stepped 272 KiB -> 288 KiB.
 *
 *      v1.61 (دستور کاربر ۲۰۲۶-۱۰-۰۵: «شرط‌ها را توی پنل بیاور و خطا را به
 *      کاربر بگو» و «یک خروجی بده که در کد میکرو کپی کنم») فهرست شرط‌های
 *      قابل‌خواندن و تولیدکنندهٔ کد calibration.h حدود ۸ کیلوبایت لازم
 *      داشت؛ پله یک حرکت عمدی دیگر از ۲۸۸ به ۳۲۰ کیلوبایت.
 *      v1.61 (user order): the on-page rule check-list and the firmware
 *      snippet generator needed ~8 KiB, so the ceiling was deliberately
 *      stepped 288 KiB -> 320 KiB.
 *
 *      ‎v1.70/v1.71 (‎دستور کاربر ۲۰۲۶-۱۰-۰۵: «نتیجهٔ ارسال باید کامل نشان داده
 *      شود و بپرسد دوباره بفرستم» و «باندهای دشارژ را یک‌شکل کن؛ فقط گپ
 *      مشترک باشد») کارت ماندگار نتیجهٔ ارسال و یک‌دست‌کردن چهار باند دشارژ
 *      حدود ۱ کیلوبایت از سقف ۳۲۰ جلو زد؛ پله یک حرکت عمدی دیگر از ۳۲۰ به
 *      ۳۳۶ کیلوبایت.
 *      v1.70/v1.71 (user order): the persistent send-result card and the
 *      uniform four-band discharge form went ~1 KiB past the 320 KiB step,
 *      so the ceiling was deliberately stepped 320 KiB -> 336 KiB.
 *
 *      v1.78..v1.84 (user orders: scenario 6's checklist, real checkboxes,
 *      independent lamp/beep controls, and the complete scenario-7 card)
 *      expanded the markup beyond the old 352 KiB step. The transfer budget
 *      is now an explicit 384000-byte ceiling, with headroom for the generated
 *      page while keeping the source/audit stamp mandatory.
 *      [FA] چک‌لیست سناریوی ۶، چک‌باکس‌های واقعی، تنظیم مستقل چراغ/بوق و
 *      کارت کامل سناریوی ۷ از سقف قدیمی ۳۵۲ کیلوبایت عبور کردند؛ سقف انتقال
 *      اکنون صریحاً ۳۸۴۰۰۰ بایت است و ممیزی stamp و اندازه همچنان اجباری است.
 *
 *      2026-10-07 (programmer pass: RTL comment hygiene restored by the
 *      project's own fix_rtl_comments.py): the mandatory U+200E direction
 *      marks inside the Persian explanation text pushed the served markup 8
 *      bytes past the 375 KiB step (384008). The ceiling is deliberately
 *      stepped 375 KiB to 376 KiB (384000 to 385024); behaviour, chunked
 *      transfer and the stamp/size audit are unchanged.
 *
 *      [FA] ۲۰۲۶-۱۰-۰۷ (پاس برنامه‌نویس: بهداشت کامنت راست‌به‌چپ با خود ابزار
 *      پروژه احیا شد): علامت‌های جهت اجباری داخل متن توضیح فارسی، مارک‌آپ را
 *      ۸ بایت از پلهٔ ۳۷۵ کیلوبایت جلو زد (۳۸۴۰۰۸). سقف عمداً از ۳۷۵ به ۳۷۶
 *      کیلوبایت (۳۸۴۰۰۰ به ۳۸۵۰۲۴) پله خورد؛ رفتار، انتقال تکه‌ای و ممیزی
 *      مهر/اندازه بدون تغییر است.
 *
 *      2026-10-08 the scoped calibration review and per-row keep/apply
 *      controls add served markup; the deliberate guard is stepped to
 *      416000 bytes so the chunked page still has build-time headroom.
 *      [FA] در ۲۰۲۶-۱۰-۰۸ ممیزی دامنه‌دار کالیبراسیون و کنترل حفظ/اعمال
 *      ردیفی به مارک‌آپ افزوده شد؛ سقف عمداً با حاشیه به ۴۱۶۰۰۰ بایت
 *      پله خورد تا صفحهٔ chunked در بیلد جا داشته باشد.
 */
#define ESP_PANEL_HTML_MAX_BYTES    416000u

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
