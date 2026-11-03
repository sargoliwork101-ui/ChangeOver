/* plink_config.h - link/message/param constants, bench CSV header, Wi-Fi/HTTP constants, parser states.
   Included by ESP_Calibration.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== Link Constants ==================== */
#define ESP_LINK_BAUD_RATE          921600u
/*
 * [EN] RX ring headroom. The panel page/bench-log handlers drain the ring
 * from inside their send loops (func__Esp_DrainSerial), but streamFile
 * and other WebServer internals cannot be drained from; 2048 gives
 * ~2 s of telemetry headroom at the 100 ms cadence, so a blocking
 * stretch no longer costs frames (1024 overflowed on slow clients and
 * every lost frame showed up as the panel's yellow CRC warning).
 */
#define ESP_LINK_RX_BUFFER_SIZE     2048u
#define ESP_LINK_SOF_BYTE0          0xAAu
#define ESP_LINK_SOF_BYTE1          0x55u
/*
 * [EN] v2 frame, must stay byte-identical to the firmware's esp_link.h:
 * SOF0 SOF1 VER TYPE LEN_LO LEN_HI [payload] CRC_LO CRC_HI
 */
#define ESP_LINK_PROTOCOL_VERSION   2u
#define ESP_LINK_HEADER_SIZE        6u   /* SOF0 + SOF1 + ver + type + len_lo + len_hi */
#define ESP_LINK_CRC_SIZE           2u
#define ESP_LINK_CRC16_INIT         0xFFFFu
#define ESP_LINK_CRC16_POLY         0x1021u
/*
 * [EN] Current v2 link: PARAMS_BULK uses a u16 length and a 512-byte
 * payload ceiling. Each chunk carries at most 102 id/value items
 * (`1 + 102 x 5 = 511` payload bytes); the 143-id parameter space is
 * therefore sent in multiple chunks. Both boards MUST flash together.
 */
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
#define ESP_MSG_PARAM_NAK           0x15u
#define ESP_MSG_PARAMS_BULK         0x12u
/*
 * [EN] v1.66 direct LUT push (user order 2026-10-05). Mirror of the STM32
 * ESPLINK_MSG_LUT_* ids - the table gets its OWN messages and its OWN flash
 * block on the board, so it never travels as parameters and never touches
 * the parameter record.
 */
#define ESP_MSG_LUT_BEGIN           0x04u
#define ESP_MSG_LUT_CHUNK           0x05u
#define ESP_MSG_LUT_COMMIT          0x06u
#define ESP_MSG_LUT_RESET           0x07u
#define ESP_MSG_LUT_READ            0x08u
#define ESP_MSG_LUT_ACK             0x13u
#define ESP_MSG_LUT_DATA            0x14u
#define ESP_MSG_WIFI_CLIENT_STATE   0x33u
/*
 * [EN] Two-sided handshake (v1.111, user order 2026-10-25): this panel sends
 * HELLO every ESP_HELLO_INTERVAL_MS; the board answers HELLO_ACK carrying its
 * own receive health, so "frames arrive" and "the pair really talks both ways"
 * are no longer the same indicator. ESP_HELLO_TIMEOUT_MS is how long a fresh
 * answer keeps the link counted as confirmed.
 */
#define ESP_MSG_HELLO               0x40u
#define ESP_MSG_HELLO_ACK           0x41u
#define ESP_HELLO_LEN               8u
#define ESP_HELLO_ACK_LEN           24u
#define ESP_HELLO_INTERVAL_MS       2000u
#define ESP_HELLO_TIMEOUT_MS        6000u
#define ESP_ESP_ROLE_CALIBRATION    2u
#define ESP_ESP_FW_VERSION          121u
/*
 * [EN] Per-channel point cap, identical to CAL_LUT_POINTS_MAX on the board.
 */
#define ESP_LUT_POINTS_MAX          24u
/*
 * [EN] Hybrid Wi-Fi power-saving and antenna TX power constants:
 *      - Mains input present OR user connected: AP stays ON at maximum TX
 *        power (20.5 dBm).
 *      - On battery with no user connected:
 *        * First 30 minutes (1800000 ms): AP stays ON at low TX power (0 dBm).
 *        * After 30 minutes: AP cycles 2 minutes OFF (120000 ms) and 1 minute
 *          ON (60000 ms) at low TX power until a user connects.
 */
#define ESP_TLM_FLAG_INPUT_PRESENT  0x02u
#define ESP_WIFI_IDLE_LOW_POWER_MS  1800000u
#define ESP_WIFI_CYCLE_ON_MS        60000u
#define ESP_WIFI_CYCLE_OFF_MS       120000u
#define ESP_WIFI_BAT_CUTOFF_PCT     20u
#define ESP_WIFI_CLIENT_REPORT_MS   1000u
/*
 * [EN] v1.112 (user order 2026-10-25: "you may lower the Wi-Fi power"): on
 * battery the antenna stays at maximum only while somebody is really
 * watching the panel (a fresh HTTP request) or a device has just joined
 * the AP. After that the AP stays ON at low TX power. Before this, an
 * associated-but-idle phone kept the radio at 20.5 dBm forever and ate
 * the battery the power schedule was meant to save. Mains: always max.
 */
#define ESP_WIFI_VIEWER_HOLD_MS     600000u
#define ESP_WIFI_TX_MAX_DBM         20.5f
#define ESP_WIFI_TX_LOW_DBM         0.0f
#define ESP_WIFI_DNS_PORT           53u
#define ESP_PARAM_WIFI_IDLE_LOW_POWER_MS  143u
#define ESP_PARAM_WIFI_CYCLE_ON_MS        144u
#define ESP_PARAM_WIFI_CYCLE_OFF_MS       145u
#define ESP_PARAM_WIFI_BAT_CUTOFF_PCT     146u
/* ==================== Parameter Constants ==================== */
/*
 * [EN] Current parameter map: ids 20..26 = shared charge profile; 27..37 =
 * Fault/Charger supervision; 38..82 = UI cadence and hysteresis;
 * 83..92 = two-loop CC/CV PID; 93..107 = charger limits; 108..118 =
 * imbalance scenario 5; 119..120 = charge-side percent map;
 * 121..122 = band-2 beep shape; 123..124 = imbalance LED cadence;
 * 125..131 and 134..135 = dead-battery scenario 6; 132..133 and 136
 * = imbalance extensions; 137..142 = technical board fault scenario 7;
 * and 143..146 = Wi-Fi battery power-saving settings.
 */
#define ESP_PARAM_COUNT            171u
#define ESP_PARAM_MANUAL_TEST_MODE  19u
#define ESP_TLM_FLAG_MANUAL_MODE    0x20u
#define ESP_SLOT_QUEUE_LEN          5u
#define ESP_SLOT_EVENTS_ID          200u
#define ESP_SLOT_CYCLES_ID          201u
#define ESP_SLOT_LATCH_ID           202u
#define ESP_SLOT_DEAD_MASK_ID       203u
/*
 * [EN] v1.86: scenario-8 reset key. The STM owns the weak-battery verdict in
 *      RAM, so this slot is a command (write 0 = drop it) and nothing on the
 *      board stores it.
 */
#define ESP_SLOT_WEAK_RESET_ID      204u
#define ESP_SLOT_FIRST_ID           ESP_SLOT_EVENTS_ID
#define ESP_SLOT_LAST_ID            ESP_SLOT_WEAK_RESET_ID
/* ==================== CAL_REFERENCE (removed from the panel in v1.7) ==================== */
/*
 * [EN] The v1.6 CAL card was dropped in the v1.7 simplification (user order 2026-09-25): the panel
 * no longer sends CAL_REFERENCE. The STM32 handler stays in the firmware, unused and harmless.
 */
/* ==================== Bench Statistics Window (spec 4) ==================== */
/*
 * [EN] The browser polls /t only every 300 ms, so the bench tools read EVERY TLM frame (10 Hz) through
 * this window: POST /m restarts it (and queues one GET_PARAMS so the logged parameters are the live
 * ones), GET /m reads it. All 20 t[] fields are tracked (ch1 0..6, ch2 7..13, Vin 14, V24 15, V12 16,
 * Vlow 17, Vhigh 18, faults 19): sum / min / max / last frame; faults are also ORed over the window;
 * seq and flags are the last frame's. The count stops at 60000 frames (100 min) so u32 sums never wrap.
 */
#define ESP_STAT_FAULT_FIELD        19u
#define ESP_STAT_MAX_FRAMES         60000u
/* ==================== Bench Data Log File (spec 4, CSV v3: 56 data columns per row + a one-off settings line (was 149 repeated every row)) ==================== */
/*
 * [EN] One append-only CSV on LittleFS. The panel builds each row from the /m window (every TLM frame,
 * raw included) plus the typed DMM readings and POSTs it to /benchlog/add; the ESP only validates
 * (printable ASCII, newline-terminated, bounded length) and appends. The column header (the comment block
 * of spec 4; v1.26 split the 149 into 56 data columns plus 93 settings written once - v1.12: +7 charge-profile params, v1.15: +11 alarm params, v1.16: +39 UI cadence params, v1.17: +6 full/hysteresis params, v1.24: +10 two-loop PID params, v1.25: +5 raw-count calibration columns) is written by the ESP when the file is created. Appending stops at the cap (HTTP 507) and the UI warns.
 * Arduino IDE: pick a flash layout WITH a file system. ESP8266-01S (1 MB): LittleFS next to the ~414 KB panel and the
 * sketch may not fit. NOT measured yet - check the sketch size in the IDE before relying on this log.
 * This panel is ESP8266 only (user order 2026-11-01); the ESP32 branches were removed.
 */
#define ESP_BENCHLOG_PATH           "/benchlog.csv"
#define ESP_BENCHLOG_MAX_BYTES      102400u
#define ESP_BENCHLOG_MAX_POST       1536u
#define ESP_BENCHLOG_HEADER \
    "# cols:\n" \
    "#  [id]     scenario,step,duty_pct,settle_ms,sample_ms,browser_ts\n" \
    "# NOTE: the settings below are NOT data columns. They do not change during a\n" \
    "#       sweep, so they are written ONCE as a '# settings:' line (same order,\n" \
    "#       comma separated) instead of repeating on all the data columns of every\n" \
    "#       row. If any of them IS changed mid-run, a fresh '# settings:' line\n" \
    "#       is written before the next data row, so every row is still covered\n" \
    "#       by the most recent settings line above it.\n" \
    "#       تنظیمات زیر ستون داده نیستند: در طول سوییپ عوض نمی‌شوند، پس یک‌بار\n" \
    "#       به‌صورت خط '# settings:' نوشته می‌شوند نه در هر ردیف. اگر وسط کار\n" \
    "#       عوض شوند، خط تازه‌ای پیش از ردیف بعدی نوشته می‌شود.\n" \
    "#  [settings] off1,off2,gain1,gain2,voff_in,voff_24,voff_12,med,avg,\n" \
    "#           res9,res10,en1,en2,ceil1,ceil2,fixon1,fix1,fixon2,fix2,manual\n" \
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
    "#            tech_beep_count,tech_beep_gap_ms,tech_led_per_ms,tech_led_duty,\n" \
    "#            wifi_idle_low_pwr_ms,wifi_cycle_on_ms,wifi_cycle_off_ms,wifi_bat_cutoff_pct\n" \
    "#  [settings9] wb_en,wb_rest_ms,wb_guard_ms,wb_hold_ms,wb_led_per,wb_led_duty,\n" \
    "#            wb_beep_per,wb_beep_ms,wb_beep_cnt,wb_beep_gap,wb_block_out,\n" \
    "#            wb_rest_floor_mv\n" \
    "#  [settings10] scn1_alarm,scn2_alarm,scn3_alarm,scn4_alarm,\n" \
    "#            scn5_alarm,scn6_alarm,scn7_alarm\n" \
    "#  [settings11] thr_trig_ma,thr_cont_ms,thr_rest_ms,in_hyst_low_mv,in_hyst_high_mv\n" \
    "#  [ch1]    raw1,raw1_min,raw1_max,shunt1_uv,unf1,unf1_min,unf1_max,\n" \
    "#           filt1,filt1_min,filt1_max,iest1,iest1_min,iest1_max,duty1,state1\n" \
    "#  [ch2]    raw2,raw2_min,raw2_max,shunt2_uv,unf2,unf2_min,unf2_max,\n" \
    "#           filt2,filt2_min,filt2_max,iest2,iest2_min,iest2_max,duty2,state2\n" \
    "#  [glob]   seq,flags,vin_mv,v24_mv,v12_mv,vlow_mv,vhigh_mv,faults_or\n" \
    "#  [raw]    vin_counts,v24_counts,v12_counts,vrefint_counts,vdda_mv\n" \
    "#  [dmm]    dmm_i_in_ma,dmm_i_bat1_ma,\n" \
    "#           dmm_i_bat2_ma,note\n" \
    "# run <n> browser_ts=<ISO from the panel page> scenario=<BAT1|BAT2|BOTH>\n" \
    "#  duty_list=<...>\n"
/* ==================== Wi-Fi / HTTP Constants ==================== */
#define ESP_WIFI_AP_SSID            "ChangeOver-ESP"
#define ESP_WIFI_AP_PASS            "123456789"
#define ESP_HTTP_PORT               80
#define ESP_JSON_BUFFER_SIZE        2624u   /* v1.16: p[77] needs the headroom (~950 B worst case); v1.17: p[83] adds ~70 B; v1.24: p[93] adds 10 PID values, ~1.1 KB; v1.28: p[108] limits; v1.43: p[119] + t[28] + fl2 adds ~120 B */
#define ESP_HTTP_FONT_CACHE         "public, max-age=31536000"
/*
 * [EN] Ceiling for the web panel, checked by a static_assert in plink_http.h.
 *      The panel is streamed from PROGMEM in small HTTP chunks and was 118 KB
 *      when this project started; it is ~414 KB now and nothing ever stopped
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
 *      The current ceiling is 416000 bytes (about 414 KB of markup). On the
 *      ESP8266-01S (1 MB flash) the page is the largest single block of flash;
 *      the HTTP response is chunked so transfer size is
 *      not confused with one giant socket write.
 *      v1.52 (user order): the global send button, its handshake report and
 *      the v1.51 per-card simulator needed ~12 KiB more, so the ceiling was
 *      deliberately stepped 256 KiB -> 272 KiB.
 *      v1.57 (user order): the backup identity stamp and the bench
 *      auto-calibration card needed ~13 KiB, so the ceiling was deliberately
 *      stepped 272 KiB -> 288 KiB.
 *      v1.61 (user order): the on-page rule check-list and the firmware
 *      snippet generator needed ~8 KiB, so the ceiling was deliberately
 *      stepped 288 KiB -> 320 KiB.
 *      v1.70/v1.71 (user order): the persistent send-result card and the
 *      uniform four-band discharge form went ~1 KiB past the 320 KiB step,
 *      so the ceiling was deliberately stepped 320 KiB -> 336 KiB.
 *      v1.78..v1.84 (user orders: scenario 6's checklist, real checkboxes,
 *      independent lamp/beep controls, and the complete scenario-7 card)
 *      expanded the markup beyond the old 352 KiB step. The transfer budget
 *      is now an explicit 384000-byte ceiling, with headroom for the generated
 *      page while keeping the source/audit stamp mandatory.
 *      2026-10-07 (programmer pass: RTL comment hygiene restored by the
 *      project's own fix_rtl_comments.py): the mandatory U+200E direction
 *      marks inside the Persian explanation text pushed the served markup 8
 *      bytes past the 375 KiB step (384008). The ceiling is deliberately
 *      stepped 375 KiB to 376 KiB (384000 to 385024); behaviour, chunked
 *      transfer and the stamp/size audit are unchanged.
 *      2026-10-08 the scoped calibration review and per-row keep/apply
 *      controls add served markup; the deliberate guard is stepped to
 *      416000 bytes so the chunked page still has build-time headroom.
 */
/* ==================== Live state history log ==================== */
/*
 * [EN] v1.93 (user order 2026-10-23): every panel keeps a state history on its
 * own flash, one small CSV per calendar day of the phone that opened it.
 * A row is written only when something really changed (mains, charger
 * step, fault mask, latch flags, battery near the cut band, link lost /
 * back, board restarted) - a steady board writes nothing, so the flash is
 * not worn out and the file stays readable. Each row is opened, written
 * and CLOSED immediately: if the panel loses power at the battery cut,
 * the last row is already on flash.
 */
#define ESP_LOG_MAX_DAYS            30u
#define ESP_LOG_MAX_ROWS_PER_DAY    400u
#define ESP_LOG_TAIL_LINES          30u
#define ESP_LOG_DAY_AUTO            0xFFFFFFFFu
#define ESP_LOG_DAY_ZERO            0xFFFFFFFEu
#define ESP_LOG_RAM_ROWS            16u
/*
 * [EN] The RAM ring holds the rows the flash refused, and it must hold a WHOLE
 * row: the widest one is 241 bytes (22 numbers + 21 commas), so a 160-byte
 * slot cut a long row in half without saying so, and that half row was
 * drained into the day file later. Equal to ESP_LOG_ROW_MAX on purpose, and
 * the static_assert beside that buffer keeps the two from drifting apart.
 */
#define ESP_LOG_RAM_ROW_MAX         256u
/*
 * [EN] v1.110 (user order 2026-10-28: "why not take it from the log file
 * itself?"): the day list is no longer a file. It is read from the day
 * files themselves, so one directory scan this often is all it costs -
 * a page that asks every 10 s gets the cache, not a flash walk.
 */
#define ESP_LOG_SCAN_MIN_MS         3000u
/*
 * [EN] What the old firmware left behind: it is never read again, only deleted
 * on boot so it stops taking flash.
 */
#define ESP_LOG_LEGACY_INDEX_PATH   "/evt-days.txt"
#define ESP_LOG_CLOCK_PATH          "/evt-clock.txt"
/*
 * [EN] Written first, then renamed onto the real file, so a power cut inside
 * the save can never leave a half-written clock behind.
 */
#define ESP_LOG_CLOCK_TMP_PATH      "/evt-clock.tmp"
#define ESP_LOG_FILE_PREFIX         "/evt-"
#define ESP_LOG_CUT_BAND_MV         21200u
#define ESP_LOG_BAND_OFF_MV         21400u
#define ESP_LOG_VSTEP_MIN_MS        60000u
#define ESP_LOG_VSTEP_MV             200u
/*
 * [EN] One "still alive" row this often when nothing at all has changed. It was
 * 6 h, which meant a pack lying quietly on battery produced ONE row a
 * session and the history looked empty (user order 2026-10-27: "it is on
 * and it still writes nothing"). One hour, and the hour is counted with
 * ESP_LOG_HEARTBEAT_SEC below, not with this panel's own uptime.
 */
#define ESP_LOG_HEARTBEAT_MS         3600000u
/*
 * [EN] The same window in LOG-CLOCK seconds. On battery the board cuts this
 * panel's power every few minutes (battery saver), so a counter that starts
 * at zero on every boot never reaches an hour and the heartbeat row was
 * never written at all. The wall second is kept in the clock file, so the
 * hour survives a power cut (user order 2026-11-01).
 */
#define ESP_LOG_HEARTBEAT_SEC        (ESP_LOG_HEARTBEAT_MS / 1000u)
/*
 * [EN] v1.115: the clock file follows a written row, but not more often than
 * this. On battery the panel only lives 60 s per cycle, so the ten-minute
 * periodic save never happens and the file used to stay hours old; one small
 * write a minute keeps the anchor fresh without wearing the flash.
 */
#define ESP_LOG_CLOCK_SAVE_MIN_MS    60000u
#define ESP_LOG_DRAIN_RETRY_MS       30000u
#define ESP_HTTP_SEND_TIMEOUT_MS     1000u
#define ESP_HTTP_ROOT_MAX_MS         15000u
#define ESP_LOG_TOTAL_BUDGET_BYTES     200000u
#define ESP_LOG_MIN_INTERVAL_MS     2000u
/*
 * [EN] A queued mains cut/return is written after this short coalescing
 * window (not the full merge window): the board may cut the panel's
 * power the same instant (battery saver) and the row must reach the
 * flash first (user order 2026-10-24: "the log must be complete").
 * Repeat flips inside the window still ride along in ONE row.
 */
#define ESP_LOG_MAINS_FLUSH_MS       250u
#define ESP_LOG_LINK_TIMEOUT_MS     5000u
#define ESP_LOG_CLOCK_SAVE_MS       600000u
/*
 * [EN] After a panel power cycle, a restored board anchor gives the first
 * HELLO_ACK a bounded chance to recover the time spent unpowered. A
 * silent board never blocks the boot row for longer than this.
 */
#define ESP_LOG_BOOT_CLOCK_GRACE_MS 10000u
/*
 * [EN] A phone time that would move the clock BACKWARD by more than this is
 * refused: two phones, a wrong timezone or a manual clock change must not
 * rewind the day and split the history into two files with the same rows
 * again. Same order as the simple panel, which uses 300 s.
 */
#define ESP_LOG_CLOCK_REWIND_SEC    120u
#define ESP_LOG_CLOCK_VALID_MIN     1700000000u
/*
 * [EN] v1.118 (audit item 4): the clock had a floor but no ceiling, so a phone
 * whose date is wrong in the FUTURE (2106, a dead RTC on the phone itself)
 * was accepted and opened a day file named after that year; coming back to
 * today then looked like a huge rewind and split the history in two. No real
 * installation is running in 2100, so that is the ceiling.
 */
#define ESP_LOG_CLOCK_VALID_MAX     4102444800u
#define ESP_LOG_HEADER \
    "# state history / تاریخچهٔ وضعیت\n" \
    "# epoch,uptime_s,change_bits,note,mains,fl,chg1,chg2,fault_mask,flags2,v24_mv,vlow_mv,vhigh_mv,imbal_mv,dead_mask,vin_mv,v12_mv,i1_ma,i2_ma,duty1_permille,duty2_permille,batt_pct\n" \
    "# batt_pct: battery percent from the discharge map (param 74/75, factory 21,000..29,000 mV) / درصد باتری از نگاشت دشارژ\n" \
    "# change_bits: 1=boot 2=mains 4=chg1 8=chg2 16=fault 32=flags 64=band 128=link 256=mode 512=dead 1024=voltage_step\n" \
    "# note: 0=none 1=board came back from a low-battery stop 2=link lost 3=link back 4=day full 5=heartbeat 6=manual 7=day start\n" \
    "# chg states: 0 off 1 bulk 2 absorb 3 float 4 bringup 5 jit_retry 6 input_wait 7 final_fault 8 bat_lost 9 manual 10 thermal_rest\n"
/*
 * [EN] v1.93 (user order 2026-10-23): the state-history card at the bottom of
 * the page took the markup to 464752 bytes, so this deliberate guard is
 * stepped 448 KiB -> 464 KiB, with ~10 KiB of headroom.
 */
/*
 * [EN] v1.113: the history card now says what a clear really deleted and names
 * a firmware update, which costs about 1 KB of HTML - raised on purpose.
 */
/*
 * [EN] v1.114: adding the ESP flash memory inspector/cleaner UI and fetch
 * timeout guards added ~3 KB of HTML/JS - stepped +4 KiB on purpose.
 */
/*
 * [EN] The shared input Schmitt card (q169/q170 + help + defaults) added about
 * 0.7 KB of HTML/JS - stepped +4 KiB on purpose, guard unchanged.
 */
#define ESP_PANEL_HTML_MAX_BYTES    491520u
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
