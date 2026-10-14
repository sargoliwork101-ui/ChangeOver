#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_simple_preview.py - ساخت فایل مستقل سیمولاتور پنل ساده

چرا
     صفحهٔ پنل داخل `simple_panel.h` به‌صورت رشتهٔ PROGMEM است، پس تنها راه
     دیدنش فلش‌کردن ESP یا وصل‌شدن به وای‌فای برد بود. این اسکریپت فایل
     `simple_preview.html` را می‌سازد که مستقیم از روی دیسک باز می‌شود:
     بدون سرور، بدون برد، بدون شبکه.

     فایل «تولید» می‌شود و هرگز دستی ویرایش نمی‌شود: مارک‌آپ عیناً از
     `simple_panel.h` بیرون کشیده می‌شود، پس سیمولاتور نمی‌تواند از خود برنامهٔ
     ESP جدا بیفتد — هر تغییری در پنل، در اجرای بعدی این اسکریپت به سیمولاتور
     هم می‌آید. برنامهٔ فلش‌شده دست‌نخورده می‌ماند: هیچ‌چیز به `simple_panel.h`
     یا `simple_*.h` اضافه نمی‌شود.

کالیبره‌بودن با برنامهٔ ESP (بدون تایپ دوباره)
     - نقشهٔ ۳۱ فیلد قاب تله‌متری از جدول §۱۵ سند `ESP_AGENT_SPEC.md` خوانده
       می‌شود؛ سیمولاتور همان ترتیب و همان واحدها را می‌سازد.
     - ایندکس‌هایی که خود پنل می‌خواند (T_VIN، T_V24، T_VLOW، T_VHIGH،
       T_FAULT، T_STATE1، T_STATE2، T_IMB، T_DEAD) از `simple_panel.h` خوانده
       و با نقشهٔ سند مقابله می‌شود؛ هر اختلاف اجرا را با خطا می‌بندد تا
       سیمولاتورِ ناهم‌خوان ساخته نشود.
     - تعداد فیلد (۳۱) و اندازهٔ قاب (۱۲۸) از `simple_config.h`، نام و رمز
       وای‌فای پیش‌فرض هم از همان‌جا.
     - بیت‌های ماسک خطا و نام حالت‌های ۰..۹ شارژر از همان جدول سند می‌آید.
     - در صفحه، هر عدد مشتق با فرمول‌های خودِ پنل حساب می‌شود
       (`vToPctPack`, `vToPctHalf`, `stateText`) نه با نسخهٔ دوم.

چه چیزی مدل است (نه اندازه‌گیری)
     جریان، duty و شمارش‌های خام مدل کوچکی از برد هستند؛ پنل ساده هیچ‌کدام را
     نشان نمی‌دهد و فقط برای کامل‌بودن قاب ساخته می‌شوند. سرعت و ضریب‌ها با
     کامنت مشخص‌اند.

اجرا:  python3 esp_simple_panel/make_simple_preview.py
خروجی: esp_simple_panel/simple_preview.html
"""

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SRC = HERE / "simple_panel.h"
CFG = HERE / "simple_config.h"
SPEC = ROOT / "ESP_AGENT_SPEC.md"
BSP = ROOT / "Firmware" / "Bsp" / "Src" / "bsp_measurement.c"
OUT = HERE / "simple_preview.html"

# [FA] نام‌های ایندکس در JS پنل -> نام فیلد در جدول §۱۵ سند
PANEL_INDEX_MAP = {
    "T_VIN": "v_in_mv",
    "T_V24": "v_bat24_mv",
    "T_VLOW": "v_bat_low_mv",
    "T_VHIGH": "v_bat_high_mv",
    "T_FAULT": "fault_mask",
    "T_STATE1": "state1",
    "T_STATE2": "state2",
    "T_IMB": "imb_mv",
    "T_DEAD": "dead_mask",
}

# [FA] مدل جریان: میلی‌آمپر بر شمارش از زنجیرهٔ §۲ سند و میکروولت بر شمارش از
#      نقطهٔ مرجع بنچ همان سند (۳۱۰ شمارش ↔ ۲۷۲۰ میکروولت). این‌ها مدل‌اند نه
#      اندازه‌گیری، و پنل ساده نمایششان نمی‌دهد.
MA_PER_COUNT = 24200.0 / 27573.0
UV_PER_COUNT = 8.774

# [FA] پیش‌فرض اسلایدرها: ورودی وصل، پک نیمه‌پر، دو نیم‌باتری نزدیک هم
BOOT_VOLTAGES = {"vin": 24000, "vpack": 25500, "vlow": 12500, "vhigh": 13000}


def die(message):
    """[FA] خطای بلند: سیمولاتور ناقص یا ناهم‌خوان ساخته نشود."""
    sys.exit("make_simple_preview: " + message)


def panel_html():
    """[FA] رشتهٔ خام HTML، عیناً و بدون دست‌کاری."""
    text = SRC.read_text(encoding="utf-8")
    start = text.index('R"rawliteral(') + len('R"rawliteral(')
    end = text.rindex(')rawliteral"')
    html = text[start:end]
    if "</html>" not in html:
        die("simple_panel.h does not look like a complete page")
    return html


def panel_number_constants():
    """[FA] ثابت‌های عددی پنل که در خطوط `const NAME=12, NAME2=34` هستند.

    سیمولاتور همین‌ها را می‌خواند تا دو نسخهٔ عددی در پروژه ساخته نشود.
    """
    values = {}
    for line in SRC.read_text(encoding="utf-8").splitlines():
        if not line.startswith("const "):
            continue
        for match in re.finditer(r"([A-Z_][A-Z0-9_]*)\s*=\s*(\d+)", line):
            values.setdefault(match.group(1), int(match.group(2)))
    return values


def config_define(name):
    """[FA] یک #define عددی از simple_config.h."""
    match = re.search(r"#define\s+" + name + r"\s+(\d+)u?(?![0-9A-Za-z_])",
                      CFG.read_text(encoding="utf-8"))
    if not match:
        die("could not read %s from simple_config.h" % name)
    return int(match.group(1))


def config_string(name):
    """[FA] یک #define رشته‌ای از simple_config.h (مثل نام اکسس‌پوینت)."""
    match = re.search(r'#define\s+' + name + r'\s+"([^"]*)"',
                      CFG.read_text(encoding="utf-8"))
    if not match:
        die("could not read %s from simple_config.h" % name)
    return match.group(1)


def spec_section(number):
    """[FA] متن یک بخش شماره‌دار سند، از «## number» تا سرفصل بعدی."""
    text = SPEC.read_text(encoding="utf-8")
    start = text.index("\n## %d." % number)
    end = text.index("\n## ", start + 4)
    return text[start:end]


def spec_tlm_fields():
    """[FA] جدول §۱۵: (offset، نوع، نام) هر فیلد قاب تله‌متری.

    اگر فرم‌ور فیلدی را جابه‌جا کند، اینجا خودش می‌شکند؛ کسی با قاب
    به‌هم‌ریخته تست نمی‌کند.
    """
    rows = []
    for line in spec_section(15).splitlines():
        match = re.match(r"\|\s*(\d+)\s*\|\s*(u16|u8|u32)\s*\|\s*([a-z0-9_]+)\s*\|", line)
        if match:
            rows.append((int(match.group(1)), match.group(2), match.group(3)))
    if not rows:
        die("could not read the TLM_LIVE table from ESP_AGENT_SPEC.md section 15")
    return rows


def field_indexes(rows):
    """[FA] نام فیلد u32 -> ایندکس در آرایهٔ t[] پنل (offset 4 + 4*i)."""
    mapping = {}
    for offset, kind, name in rows:
        if kind != "u32":
            continue
        index = (offset - 4) // 4
        if (offset - 4) % 4 != 0:
            die("field %s is not 4-byte aligned" % name)
        if index in mapping.values():
            die("two fields share index %d" % index)
        mapping[name] = index
    return mapping


def spec_fault_bits():
    """[FA] بیت‌های ماسک خطا از ستون توضیح همان جدول."""
    match = re.search(r"\|\s*80\s*\|\s*u32\s*\|\s*fault_mask\s*\|\s*([^|]+)\|",
                      spec_section(15))
    if not match:
        die("could not read the fault_mask row from ESP_AGENT_SPEC.md")
    bits = []
    for part in match.group(1).split(","):
        found = re.match(r"b(\d)\s+(.+)$", part.strip())
        if found:
            bits.append({"bit": int(found.group(1)), "name": found.group(2).strip()})
    if not bits:
        die("fault_mask bits did not parse: " + match.group(1))
    return bits


def spec_charger_states():
    """[FA] نام حالت‌های شارژر از سطر «Charger states» همان سند."""
    match = re.search(r"Charger states:(.*?)\n\n", spec_section(15), re.S)
    if not match:
        die("could not read the charger states from ESP_AGENT_SPEC.md")
    text = match.group(1).replace("`", "").replace("\n", " ").strip()
    states = {}
    for part in text.split(","):
        found = re.match(r"(\d+)\s+([A-Za-z_]+)", part.strip())
        if found:
            states[int(found.group(1))] = found.group(2)
    if len(states) < 5:
        die("charger states did not parse: " + text)
    return states


def bsp_volt_scales():
    """[FA] میلی‌ولت بر هر شمارش ADC برای نت‌های ۲۴ و ۱۲ ولت و شمارش VREFINT.

    همان محاسبه‌ای که `bsp_measurement.c` انجام می‌دهد، تا شمارش خامِ
    شبیه‌سازی با ولتاژ شبیه‌سازی بخواند. این پوشه به فرم‌ور وابسته نمی‌شود:
    اگر تعریف‌ها نبود، با هشدار و شمارش صفر ادامه می‌دهیم.
    """
    if not BSP.exists():
        print("warning: %s not found - raw counts stay 0" % BSP.name)
        return None
    text = BSP.read_text(encoding="utf-8")

    def value(name):
        found = re.search(r"#define\s+" + name + r"\s+(\d+)u?(?![0-9A-Za-z_])", text)
        return int(found.group(1)) if found else None

    vref = value("BSP_MEASUREMENT_VREF_MV")
    full = value("BSP_MEASUREMENT_ADC_FULL_SCALE")
    bottom = value("BSP_MEASUREMENT_SENSE_SHUNT_OHMS")
    top24 = value("BSP_MEASUREMENT_SENSE_TOP_24V_OHMS")
    top12 = value("BSP_MEASUREMENT_DIV12_TOP_OHMS")
    if None in (vref, full, bottom, top24, top12):
        print("warning: BSP divider defines are incomplete - raw counts stay 0")
        return None
    lsb = vref / full
    return {
        "mv_per_count_24": lsb * (top24 + bottom) / bottom,
        "mv_per_count_12": lsb * (top12 + bottom) / bottom,
        "mv_per_count_adc": lsb,
        "vrefint_counts": round(1200 / vref * full),
        "vdda_mv": vref,
    }


def chart_ranges():
    """[FA] بازه‌های نمودار از simple_config.h: طول سری و گام زمانی هر نقطه."""
    return {
        "1d": {"len": config_define("SIMPLE_CHART_1D_SIZE"), "stepSec": 3600},
        "7d": {"len": config_define("SIMPLE_CHART_7D_SIZE"), "stepSec": 6 * 3600},
        "1m": {"len": config_define("SIMPLE_CHART_1M_SIZE"), "stepSec": 24 * 3600},
        "3m": {"len": config_define("SIMPLE_CHART_3M_SIZE"), "stepSec": 24 * 3600},
    }


SHIM = r"""<!-- SIM_PREVIEW_SHIM_BEGIN - سیمولاتور آفلاین، فایل تولیدشده، دستی ویرایش نشود -->
<style>
#pg3 .simbox{background:var(--cd);border:1px solid var(--ln);border-radius:var(--r);padding:12px}
#pg3 .simrow{display:flex;align-items:center;gap:10px;margin:8px 0;flex-wrap:wrap}
#pg3 .simrow>label{font-size:12px;color:var(--tx2);min-width:104px}
#pg3 input[type=range]{flex:1;min-width:140px}
#pg3 select{background:var(--in);color:var(--tx);border:1px solid var(--ln);border-radius:8px;padding:6px 8px;font-size:12px}
#pg3 .simval{font-size:12px;color:var(--tx);min-width:72px;text-align:left}
#pg3 .simhint{font-size:11px;color:var(--mu);line-height:1.9}
#pg3 .simout{font-size:12px;color:var(--tx2);line-height:2}
#pg3 .fbits{display:flex;flex-wrap:wrap;gap:8px}
#pg3 .fbits label{display:flex;align-items:center;gap:5px;font-size:11px;background:var(--in);border:1px solid var(--ln);border-radius:8px;padding:4px 8px;min-width:auto}
</style>
<script>
/* ==================== سیمولاتور آفلاین پنل ساده - تولیدشده ==================== */
(function () {
  'use strict';

  var IDX = __FIELD_INDEX__;        /* نام فیلد -> ایندکس t[]، از جدول سند */
  var TLM_N = __TLM_N__;            /* تعداد فیلدهای قاب، از simple_config.h */
  var FAULT_BITS = __FAULT_BITS__;  /* بیت‌های ماسک خطا، از سند */
  var STATES = __STATES__;          /* نام حالت‌های ۰..۹ شارژر، از سند */
  var RANGES = __RANGES__;          /* بازه‌های نمودار، از simple_config.h */
  var SCALES = __SCALES__;          /* میلی‌ولت بر شمارش ADC (یا null) */
  var WIFI_SSID = __WIFI_SSID__;
  var WIFI_PASS = __WIFI_PASS__;
  var PANEL = __PANEL__;            /* ثابت‌های عددی خود پنل */
  var BOOT = __BOOT__;              /* نقطهٔ شروع اسلایدرها */
  var MA_PER_COUNT = __MA_PER_COUNT__;
  var UV_PER_COUNT = __UV_PER_COUNT__;

  /* ---------- حالت برد شبیه‌سازی‌شده: همه‌چیز دستی ---------- */
  var sim = {
    linkUp: true,
    vin: BOOT.vin, vpack: BOOT.vpack, vlow: BOOT.vlow, vhigh: BOOT.vhigh,
    state1: 1, state2: 0,
    fault: 0, deadMask: 0, ce: 0, vm: 0
  };
  /* پارامترهای خود برد شبیه‌سازی‌شده - همان‌هایی که چراغ سبز و درصد باتری
     از آن‌ها ساخته می‌شود (۷۴/۷۵ درصد فول/خالی، ۶۶/۶۷ چشمک سبز، ۸۰/۸۱/۸۲
     هیسترزیس و ...). در حالت زنده با /sim روی برد واقعی هم ست می‌شوند. */
  var board = {
    gPer: PANEL.GREEN_PERIOD, gOff: PANEL.GREEN_MIN_OFF,
    yPer: PANEL.YELLOW_PERIOD, yOn: PANEL.YELLOW_MIN_ON,
    vmin: PANEL.BAT_VMIN, vmax: PANEL.BAT_VMAX,
    hEnt: PANEL.FULL_ENTER, hExit: PANEL.FULL_EXIT,
    cHyst: PANEL.CHG_HYST, rHyst: PANEL.RUN_HYST,
    rZero: PANEL.ZERO_EXIT, rOne: PANEL.ONE_EXIT,
    cvmin: PANEL.CHG_VMIN, cvmax: PANEL.CHG_VMAX
  };
  function boardParamsBody() {
    return ',"pvOk":1,"pv":[' + [board.gPer, board.gOff, board.yPer, board.yOn,
      board.vmin, board.vmax, board.hEnt, board.hExit, board.cHyst, board.rHyst,
      board.rZero, board.rOne, board.cvmin, board.cvmax].join(',') + ']';
  }
  function changedParamQuery() {
    var query = '';
    if (board.vmin !== PANEL.BAT_VMIN) { query += '&p74=' + board.vmin; }
    if (board.vmax !== PANEL.BAT_VMAX) { query += '&p75=' + board.vmax; }
    return query;
  }

  var counters = { t0: Date.now(), frames: 0, totalChargeSec: 13718, chargeCount: 42,
                   chargeCountCh1: 21, chargeCountCh2: 21, chargeSecCh1: 7031, chargeSecCh2: 6687,
                   resetCount: 3, boardTime: Math.floor(Date.now() / 1000) };
  var mock = { user: '', role: 0, theme: 0, ssid: WIFI_SSID, pass: WIFI_PASS,
               userPass: '',   /* رمز عوض‌شدهٔ کاربر در حالت آفلاین - خالی یعنی پیش‌فرض‌ها */
               users: [{ name: 'admin', role: 1 }] };
  var mockActive = (location.protocol === 'file:');   /* فقط وقتی فایلی باز شده باشد */
  var mockUntil = 0;                                  /* اگر سرور نبود، موقتاً جای بردی */
  var originalFetch = window.fetch ? window.fetch.bind(window) : null;
  var controls = [];                /* کنترل‌هایی که خودشان را از sim رنگ می‌کنند */

  /* ==================== مدل کوچک برد ==================== */
  function countsFromMv(mv, mvPerCount) {
    if (!SCALES || !mvPerCount) { return 0; }
    return Math.max(0, Math.round(mv / mvPerCount));
  }
  /* duty و جریان مدل‌شده: برد واقعی حلقهٔ CC/CV دارد؛ اینجا فقط می‌خواهیم
     عددهای کنار حالت شارژر متناسب و زنده دیده شوند */
  function modelDutyPermille(state) {
    if (state === 1) { return 450; }   /* BULK */
    if (state === 2) { return 300; }   /* ABSORB */
    if (state === 3) { return 120; }   /* FLOAT */
    return 0;
  }
  function modelCurrentMa(state) { return Math.round(modelDutyPermille(state) * 1.6); }
  function inputPresent() { return sim.linkUp && (sim.vin >= PANEL.INPUT_CONN_THR); }
  function uptimeSec() { return Math.floor((Date.now() - counters.t0) / 1000) + 86400; }

  function buildTelemetry() {
    var t = new Array(TLM_N);
    for (var i = 0; i < TLM_N; i++) { t[i] = 0; }
    var duty1 = modelDutyPermille(sim.state1), duty2 = modelDutyPermille(sim.state2);
    var ma1 = modelCurrentMa(sim.state1), ma2 = modelCurrentMa(sim.state2);
    var shunt1 = Math.round(ma1 * MA_PER_COUNT * UV_PER_COUNT);
    var shunt2 = Math.round(ma2 * MA_PER_COUNT * UV_PER_COUNT);
    var flags = 0x01 | 0x04;                       /* snapshot معتبر + اندازه‌گیری معتبر */
    if (inputPresent()) { flags |= 0x02; }         /* b1 = برق ورودی وصل */
    if (sim.state1 === 9) { flags |= 0x20; }       /* b5 = مود تست دستی */

    t[IDX.raw1_counts] = countsFromMv(shunt1 / 1000, SCALES && SCALES.mv_per_count_adc);
    t[IDX.shunt1_uv] = shunt1;
    t[IDX.ma1_unfiltered] = ma1;
    t[IDX.i1_filtered_ma] = ma1;
    t[IDX.iest1_ma] = ma1;
    t[IDX.duty1_permille] = duty1;
    t[IDX.state1] = sim.state1;
    t[IDX.raw2_counts] = countsFromMv(shunt2 / 1000, SCALES && SCALES.mv_per_count_adc);
    t[IDX.shunt2_uv] = shunt2;
    t[IDX.ma2_unfiltered] = ma2;
    t[IDX.i2_filtered_ma] = ma2;
    t[IDX.iest2_ma] = ma2;
    t[IDX.duty2_permille] = duty2;
    t[IDX.state2] = sim.state2;
    t[IDX.v_in_mv] = sim.linkUp ? sim.vin : 0;
    t[IDX.v_bat24_mv] = sim.linkUp ? sim.vpack : 0;
    t[IDX.v_bat12_mv] = sim.linkUp ? sim.vlow : 0;
    t[IDX.v_bat_low_mv] = sim.linkUp ? sim.vlow : 0;
    t[IDX.v_bat_high_mv] = sim.linkUp ? sim.vhigh : 0;
    t[IDX.fault_mask] = sim.linkUp ? sim.fault : 0;
    t[IDX.vin_raw_counts] = countsFromMv(sim.vin, SCALES && SCALES.mv_per_count_24);
    t[IDX.v24_raw_counts] = countsFromMv(sim.vpack, SCALES && SCALES.mv_per_count_24);
    t[IDX.v12_raw_counts] = countsFromMv(sim.vlow, SCALES && SCALES.mv_per_count_12);
    t[IDX.vrefint_counts] = SCALES ? SCALES.vrefint_counts : 0;
    t[IDX.vdda_mv] = SCALES ? SCALES.vdda_mv : 0;
    t[IDX.imb_mv] = Math.abs(sim.vhigh - sim.vlow);
    t[IDX.imb_events] = 2;
    t[IDX.imb_latched_cycles] = 0;
    t[IDX.dead_mask] = sim.deadMask;
    t[IDX.dead_charge1_s] = (sim.state1 === 1 || sim.state1 === 2) ? 240 : 0;
    t[IDX.dead_charge2_s] = (sim.state2 === 1 || sim.state2 === 2) ? 190 : 0;
    return { flags: flags, t: t };
  }

  function telemetryJson() {
    var built = buildTelemetry();
    counters.frames++;
    var body = '{"on":' + (sim.linkUp ? 1 : 0) + ',"age":' + (sim.linkUp ? 40 : 9999) +
      ',"seq":' + (counters.frames % 65536) + ',"fl":' + built.flags + ',"fl2":0' +
      ',"n":' + counters.frames + ',"ce":' + sim.ce + ',"vm":' + sim.vm +
      ',"inputPresent":' + (inputPresent() ? 1 : 0) +
      ',"user":"' + mock.user + '","role":' + mock.role + ',"theme":' + mock.theme +
      ',"boardTime":' + counters.boardTime + ',"bt":' + counters.boardTime +
      ',"timeOk":1,"recovery":0,"t":[';
    for (var i = 0; i < TLM_N; i++) { body += (i ? ',' : '') + built.t[i]; }
    return body + ']' + boardParamsBody() + '}';
  }

  function chartJson(range) {
    var conf = RANGES[range] || RANGES['1d'];
    var now = counters.boardTime;
    var pct = [], vin = [], vpack = [], time = [];
    for (var i = 0; i < conf.len; i++) {
      var phase = (i / conf.len) * Math.PI * 2;
      var wobble = Math.round(Math.sin(phase) * 4);
      pct.push(Math.max(0, Math.min(100,
        Math.round((sim.vpack - board.vmin) * 100 / (board.vmax - board.vmin)) - 6 + wobble)));
      vin.push(sim.vin ? (sim.vin - (i % 3) * 120) : 0);
      vpack.push(sim.vpack - (i % 4) * 90);
      time.push(now - (conf.len - 1 - i) * conf.stepSec);
    }
    return JSON.stringify({ range: range, count: conf.len, pct: pct, vin: vin, vpack: vpack, time: time });
  }

  function diagJson() {
    var now = counters.boardTime;
    return JSON.stringify({
      valid: sim.linkUp ? 1 : 0,
      totalChargeSec: counters.totalChargeSec + uptimeSec() - 86400,
      chargeCount: counters.chargeCount, chargeCountCh1: counters.chargeCountCh1,
      chargeCountCh2: counters.chargeCountCh2, chargeSecCh1: counters.chargeSecCh1,
      chargeSecCh2: counters.chargeSecCh2, uptimeSec: uptimeSec(),
      resetCount: counters.resetCount, boardTime: now, errCount: 2, powerCount: 2,
      errors: [
        { ts: now - 7200, fault: 128, vin: 0, vpack: 25900, vlow: 12900, vhigh: 13000, s1: 1, s2: 0 },
        { ts: now - 86400, fault: 64, vin: 24100, vpack: 25200, vlow: 12400, vhigh: 12100, s1: 8, s2: 8 }
      ],
      power: [{ off: now - 3600, on: now - 3000, dur: 600 },
              { off: now - 172800, on: now - 171000, dur: 1800 }],
      chargeHistory: [{ ts: now - 5400, ch: 1, dur: 3120 }, { ts: now - 12600, ch: 2, dur: 2880 }]
    });
  }

  /* ==================== مسیرهای شبیه‌سازی‌شده (فقط حالت آفلاین) ==================== */
  /* جواب JSON: در مرورگر شیء Response است؛ اگر نبود (محیط تست)، همان قرارداد
     را با یک شیء ساده می‌دهیم تا سیمولاتور بی‌نیاز از Fetch API هم اجرا شود */
  function jsonResponse(text, code) {
    var status = code || 200;
    if (typeof Response === 'function') {
      return new Response(text, { status: status,
        headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' } });
    }
    return { ok: status < 400, status: status,
             json: function () { return Promise.resolve(JSON.parse(text)); },
             text: function () { return Promise.resolve(text); } };
  }
  function bodyJson(options) {
    try { return JSON.parse((options && options.body) || '{}'); } catch (e) { return {}; }
  }
  function query(url, name) {
    var found = new RegExp('[?&]' + name + '=([^&]*)').exec(url);
    return found ? decodeURIComponent(found[1]) : '';
  }
  function routeName(url) {
    var found = /^\/([a-z/]+)/.exec(url.replace(location.origin || '', ''));
    return found ? found[1] : '';
  }

  function mockFetch(url, options) {
    var path = routeName(url);
    var method = ((options && options.method) || 'GET').toUpperCase();

    if (path === 't') { return jsonResponse(telemetryJson()); }
    if (path === 'charts') { return jsonResponse(chartJson(query(url, 'range') || '1d')); }
    if (path === 'diag') { return jsonResponse(diagJson()); }
    if (path === 'theme') { return jsonResponse('{"theme":' + mock.theme + '}'); }
    if (path === 'settheme') {
      mock.theme = (parseInt(query(url, 'theme') || '0', 10) === 1) ? 1 : 0;   /* فقط تیره/روشن */
      return jsonResponse('{"ok":1}');
    }
    if (path === 'login') {
      var credentials = bodyJson(options);
      var user = credentials.user !== undefined ? credentials.user : query(url, 'user');
      var pass = credentials.pass !== undefined ? credentials.pass : query(url, 'pass');
      var defaultPass = (user === 'admin') ? 'admin' : ((user === 'viewer') ? 'viewer' : null);
      var expected = (mock.userPass && user === mock.user) ? mock.userPass : defaultPass;
      if (expected !== null && pass === expected) {
        mock.user = user; mock.role = (user === 'admin') ? 1 : 0;
        return jsonResponse('{"ok":1,"user":"' + user + '","role":' + mock.role + '}');
      }
      return jsonResponse('{"ok":0,"e":"wrong"}', 401);
    }
    if (path === 'logout') { mock.user = ''; mock.role = 0; return jsonResponse('{"ok":1}'); }
    if (path === 'setpass') {
      var change = bodyJson(options);
      var target = (change.user || '').trim();
      if (!mock.user) { return jsonResponse('{"ok":0,"e":"login"}', 403); }
      if (!target || !change.new) { return jsonResponse('{"ok":0,"e":"user/new"}', 400); }
      if ((change.new || '').length < 4) { return jsonResponse('{"ok":0,"e":"pass 4-32"}', 400); }
      if (change.new2 && change.new2 !== change.new) { return jsonResponse('{"ok":0,"e":"mismatch"}', 400); }
      if (mock.role !== 1 && target !== mock.user) { return jsonResponse('{"ok":0,"e":"self"}', 403); }
      var wantOld = (mock.userPass && target === mock.user) ? mock.userPass
                                                           : ((target === 'admin') ? 'admin' : 'viewer');
      if (target === mock.user && (change.old || '') !== wantOld) {
        return jsonResponse('{\"ok\":0,\"e\":\"old\"}', 400);
      }
      if (target === mock.user) { mock.userPass = change.new; }   /* در حالت آفلاین هم رمز عوض می‌شود */
      return jsonResponse('{"ok":1}');
    }
    if (path === 'users') {
      if (mock.role !== 1) { return jsonResponse('{"ok":0,"e":"admin"}', 403); }
      if (method === 'POST') {
        var edit = bodyJson(options);
        if (edit.action === 'del') {
          mock.users = mock.users.filter(function (item) { return item.name !== edit.user; });
        } else {
          mock.users.push({ name: edit.user, role: parseInt(edit.role, 10) || 0 });
        }
        return jsonResponse('{"ok":1}');
      }
      return jsonResponse(JSON.stringify({ users: mock.users }));
    }
    if (path === 'adduser' || path === 'deluser') {
      if (mock.role !== 1) { return jsonResponse('{"ok":0,"e":"admin"}', 403); }
      var payload = bodyJson(options);
      if (path === 'adduser') {
        mock.users.push({ name: payload.user, role: parseInt(payload.role, 10) || 0 });
      } else {
        mock.users = mock.users.filter(function (item) { return item.name !== payload.user; });
      }
      return jsonResponse('{"ok":1}');
    }
    if (path === 'wifi') {
      if (mock.role !== 1) { return jsonResponse('{"ok":0,"e":"admin"}', 403); }
      if (method === 'POST') {
        var settings = bodyJson(options);
        if (settings.ssid) { mock.ssid = settings.ssid; }
        if (settings.pass) { mock.pass = settings.pass; }
        return jsonResponse('{"ok":1,"reboot":1}');
      }
      return jsonResponse('{"ssid":"' + mock.ssid + '","pass":"' + mock.pass + '"}');
    }
    if (path === 'setwifi') {
      var wifi = bodyJson(options);
      mock.ssid = wifi.ssid || mock.ssid; mock.pass = wifi.pass || mock.pass;
      return jsonResponse('{"ok":1,"reboot":1}');
    }
    if (path === 'time') {
      if (mock.role !== 1) { return jsonResponse('{"ok":0,"e":"admin"}', 403); }
      counters.boardTime = parseInt(query(url, 'ts') || '0', 10) || counters.boardTime;
      return jsonResponse('{"ok":1}');
    }
    if (path === 'clear') { return jsonResponse('{"ok":1}'); }
    if (path === 'sim') {
      /* [FA] در حالت آفلاین هم همان پارامترهای /sim برد شبیه‌سازی‌شده اعمال
         می‌شود تا تب سیمولاتور بیرون از سرور زنده هم اثر داشته باشد */
      var num = function (name) {
        var raw = query(url, name);
        return raw === '' ? null : parseInt(raw, 10);
      };
      var setNumber = function (name, field) { var v = num(name); if (v !== null && !isNaN(v)) { sim[field] = v; } };
      setNumber('vin', 'vin'); setNumber('vpack', 'vpack');
      setNumber('vlow', 'vlow'); setNumber('vhigh', 'vhigh');
      setNumber('s1', 'state1'); setNumber('s2', 'state2');
      setNumber('fault', 'fault'); setNumber('dead', 'deadMask');
      var linkFlag = num('link'); if (linkFlag !== null) { sim.linkUp = (linkFlag === 1); }
      var param = num('p74'); if (param !== null) { board.vmin = param; }
      param = num('p75'); if (param !== null) { board.vmax = param; }
      param = num('p66'); if (param !== null) { board.gPer = param; }
      param = num('p67'); if (param !== null) { board.gOff = param; }
      return jsonResponse('{"ok":1}');
    }
    return jsonResponse('{"ok":0,"e":"sim"}', 404);
  }

  /* آفلاین هر مسیر داخلی از همین‌جا جواب می‌گیرد؛ اگر سرور سیمولاتور بالا
     باشد (حالت زنده) درخواست واقعاً به سرور می‌رود */
  /* کلید آزمون: اگر خود آزمون این را true کند، هیچ‌وقت دادهٔ جعلی جای
     دادهٔ واقعی سرور را نمی‌گیرد (برای سنجش دقیق مسیر واقعی) */
  var noMock = (window.__CO_NO_MOCK__ === true);
  window.fetch = function (url, options) {
    if (mockActive && !noMock) {
      return Promise.resolve(mockFetch(String(url), options));
    }
    if (!originalFetch) { return Promise.resolve(mockFetch(String(url), options)); }
    if (!noMock && Date.now() < mockUntil) {   /* سرور چند لحظه پیش جواب نداده بود */
      return Promise.resolve(mockFetch(String(url), options));
    }
    return originalFetch(url, options).catch(function (error) {
      var path = routeName(String(url));
      if (path === 't' || path === 'charts' || path === 'diag') {
        /* یک بار جواب نداد - فقط چند ثانیه حالت شبیه‌ساز تا داده جعلی
           جای داده واقعی را برای همیشه نگیرد؛ دفعه بعد دوباره سرور امتحان می‌شود */
        mockUntil = Date.now() + 10000;
        return Promise.resolve(mockFetch(String(url), options));
      }
      throw error;
    });
  };

  /* ==================== تب چهارم: کنترل دستی برد ==================== */
  function el(tag, attrs, text) {
    var node = document.createElement(tag);
    for (var key in attrs) { if (Object.prototype.hasOwnProperty.call(attrs, key)) { node.setAttribute(key, attrs[key]); } }
    if (text !== undefined) { node.textContent = text; }
    return node;
  }

  function slider(parent, title, key, max) {
    var row = el('div', { 'class': 'simrow' });
    row.appendChild(el('label', {}, title));
    var input = el('input', { type: 'range', min: 0, max: max, step: 100, value: sim[key] });
    var readout = el('span', { 'class': 'simval' });
    function paint() {
      input.value = sim[key];
      readout.textContent = (sim[key] / 1000).toFixed(2) + ' V';
    }
    input.addEventListener('input', function () {
      sim[key] = Number(input.value);
      readout.textContent = (sim[key] / 1000).toFixed(2) + ' V';
      applySim(); simRefresh();
    });
    controls.push(paint); paint();
    row.appendChild(input); row.appendChild(readout);
    parent.appendChild(row);
  }

  function paramSlider(parent, title, id, min, max) {
    var row = el('div', { 'class': 'simrow' });
    row.appendChild(el('label', {}, title));
    var input = el('input', { type: 'range', min: min, max: max, step: 100, value: board[id] });
    var readout = el('span', { 'class': 'simval' });
    function paint() {
      input.value = board[id];
      readout.textContent = board[id] + ' mV';
    }
    input.addEventListener('input', function () {
      board[id] = Number(input.value);
      if (id === 'vmin') { board.cvmin = board[id]; }
      if (id === 'vmax') { board.cvmax = board[id]; }
      readout.textContent = board[id] + ' mV';
      applySim(); simRefresh();
    });
    controls.push(paint); paint();
    row.appendChild(input); row.appendChild(readout);
    parent.appendChild(row);
  }

  function buildSimTab() {
    var tab2 = document.getElementById('tab2');
    var pg2 = document.getElementById('pg2');
    if (!tab2 || !pg2 || document.getElementById('pg3')) { return; }

    var tab3 = el('button', { id: 'tab3', onclick: 'showTab(3)' }, 'سیمولاتور');
    tab2.parentNode.insertBefore(tab3, tab2.nextSibling);
    var page = el('div', { id: 'pg3', 'class': 'pgx' });

    var intro = el('div', { 'class': 'simbox' });
    intro.appendChild(el('div', { 'class': 'simhint' },
      'سیمولاتور آفلاین: برد STM32 همین‌جا دستی کنترل می‌شود و هیچ داده‌ای به برد واقعی نمی‌رود. ' +
      'هر عدد با فرمول‌های خود همین صفحه حساب می‌شود. ورود: admin / admin ' +
      '(برای دیدن نقش ساده: viewer / viewer) - وای‌فای پیش‌فرض: ' + WIFI_SSID));
    page.appendChild(intro);

    var voltageBox = el('div', { 'class': 'simbox' });
    voltageBox.appendChild(el('div', { 'class': 'simhint' }, 'ولتاژها (دستی)'));
    slider(voltageBox, 'ورودی ۲۴ ولت', 'vin', 30000);
    slider(voltageBox, 'پک باتری', 'vpack', 32000);
    slider(voltageBox, 'نیم‌باتری پایین', 'vlow', 16000);
    slider(voltageBox, 'نیم‌باتری بالا', 'vhigh', 16000);
    page.appendChild(voltageBox);

    /* پارامترهای خود برد: همان عددهایی که درصد فول/خالی و چشمک سبز از آن‌ها
       ساخته می‌شود؛ پنل در هر قاب /t همین‌ها را می‌خواند */
    var paramBox = el('div', { 'class': 'simbox' });
    paramBox.appendChild(el('div', { 'class': 'simhint' },
      'پارامترهای خود برد (درصد فول/خالی باتری - همان‌هایی که چراغ سبز با آن‌ها چشمک می‌زند)'));
    paramSlider(paramBox, '۰٪ باتری (پارامتر ۷۴)', 'vmin', 15000, 25000);
    paramSlider(paramBox, '۱۰۰٪ باتری (پارامتر ۷۵)', 'vmax', 25000, 32000);
    page.appendChild(paramBox);

    var stateBox = el('div', { 'class': 'simbox' });
    stateBox.appendChild(el('div', { 'class': 'simhint' }, 'حالت شارژر (همان ۰..۹ برد)'));
    [['state1', 'کانال ۱ (بالا)'], ['state2', 'کانال ۲ (پایین)']].forEach(function (pair) {
      var row = el('div', { 'class': 'simrow' });
      row.appendChild(el('label', {}, pair[1]));
      var select = el('select', {});
      Object.keys(STATES).forEach(function (value) {
        select.appendChild(el('option', { value: value }, value + ' - ' + STATES[value]));
      });
      select.addEventListener('change', function () {
        sim[pair[0]] = parseInt(select.value, 10); applySim(); simRefresh();
      });
      controls.push(function () { select.value = String(sim[pair[0]]); });
      row.appendChild(select);
      stateBox.appendChild(row);
    });
    page.appendChild(stateBox);

    var faultBox = el('div', { 'class': 'simbox' });
    faultBox.appendChild(el('div', { 'class': 'simhint' }, 'ماسک خطا (بیت‌های برد)'));
    var bitRow = el('div', { 'class': 'fbits' });
    FAULT_BITS.forEach(function (bit) {
      var box = el('label', {});
      var input = el('input', { type: 'checkbox', value: bit.bit });
      input.addEventListener('change', function () {
        if (input.checked) { sim.fault |= (1 << bit.bit); } else { sim.fault &= ~(1 << bit.bit); }
        applySim(); simRefresh();
      });
      controls.push(function () { input.checked = (sim.fault & (1 << bit.bit)) !== 0; });
      box.appendChild(input);
      box.appendChild(el('span', {}, 'b' + bit.bit + ' ' + bit.name));
      bitRow.appendChild(box);
    });
    faultBox.appendChild(bitRow);
    page.appendChild(faultBox);

    var linkBox = el('div', { 'class': 'simbox' });
    linkBox.appendChild(el('div', { 'class': 'simhint' }, 'لینک UART و سناریوهای آماده'));
    var linkRow = el('div', { 'class': 'simrow' });
    var linkInput = el('input', { type: 'checkbox' });
    linkInput.addEventListener('change', function () {
      sim.linkUp = linkInput.checked; applySim(); simRefresh();
    });
    controls.push(function () { linkInput.checked = sim.linkUp; });
    linkRow.appendChild(linkInput);
    linkRow.appendChild(el('span', {}, 'لینک وصل است'));
    linkBox.appendChild(linkRow);

    var scenarioRow = el('div', { 'class': 'simrow' });
    [['برق شهر ۲۴ ولت', function () { sim.vin = 24000; }],
     ['قطع برق شهر', function () { sim.vin = 0; }],
     ['شارژ کامل', function () { sim.vpack = 28500; sim.vlow = 14200; sim.vhigh = 14300; sim.state1 = 3; sim.state2 = 3; }],
     ['خطای فنی', function () { sim.fault |= (1 << 7); }],
     ['باتری قطع', function () { sim.fault |= (1 << 6); }],
     ['ریست سیمولاتور', function () {
        sim.vin = BOOT.vin; sim.vpack = BOOT.vpack; sim.vlow = BOOT.vlow; sim.vhigh = BOOT.vhigh;
        sim.state1 = 1; sim.state2 = 0; sim.fault = 0; sim.deadMask = 0; sim.linkUp = true;
        board.vmin = PANEL.BAT_VMIN; board.vmax = PANEL.BAT_VMAX;
        board.cvmin = PANEL.CHG_VMIN; board.cvmax = PANEL.CHG_VMAX;
     }]
    ].forEach(function (scenario) {
      var button = el('button', { 'class': 'btn sec' }, scenario[0]);
      button.addEventListener('click', function () { scenario[1](); applySim(); simRefresh(); });
      scenarioRow.appendChild(button);
    });
    linkBox.appendChild(scenarioRow);
    page.appendChild(linkBox);

    var outBox = el('div', { 'class': 'simbox' });
    outBox.appendChild(el('div', { 'class': 'simhint' }, 'خوانش مشتق (با فرمول خود پنل)'));
    outBox.appendChild(el('div', { 'class': 'simout', id: 'simOut' }));
    page.appendChild(outBox);

    pg2.parentNode.insertBefore(page, pg2.nextSibling);

    var originalShowTab = window.showTab;
    window.showTab = function (index) {
      var page3 = document.getElementById('pg3');
      var tab3Button = document.getElementById('tab3');
      if (index === 3) {
        for (var i = 0; i < 3; i++) {
          var otherPage = document.getElementById('pg' + i);
          var otherTab = document.getElementById('tab' + i);
          if (otherPage) { otherPage.classList.remove('a'); }
          if (otherTab) { otherTab.classList.remove('a'); }
        }
        if (page3) { page3.classList.add('a'); }
        if (tab3Button) { tab3Button.classList.add('a'); }
        simRefresh();
        return;
      }
      if (page3) { page3.classList.remove('a'); }
      if (tab3Button) { tab3Button.classList.remove('a'); }
      originalShowTab(index);
    };
    simRefresh();
  }

  /* ==================== اعمال روی برد شبیه‌ساز ==================== */
  function applySim() {
    var params = 'vin=' + sim.vin + '&vpack=' + sim.vpack + '&vlow=' + sim.vlow +
      '&vhigh=' + sim.vhigh + '&s1=' + sim.state1 + '&s2=' + sim.state2 +
      '&fault=' + sim.fault + '&link=' + (sim.linkUp ? 1 : 0) + '&dead=' + sim.deadMask +
      changedParamQuery();
    window.fetch('/sim?' + params, { method: 'POST' }).catch(function () {});
  }

  function simRefresh() {
    for (var i = 0; i < controls.length; i++) { controls[i](); }
    var out = document.getElementById('simOut');
    if (out) {
      out.innerHTML =
        'ورودی: ' + (sim.vin / 1000).toFixed(2) + ' V (' +
          (sim.vin >= PANEL.INPUT_CONN_THR ? 'وصل' : 'قطع') + ')' +
        ' | پک: ' + (sim.vpack / 1000).toFixed(2) + ' V = ' + vToPctPack(sim.vpack) + '٪' +
        '<br>نیم‌باتری پایین: ' + vToPctHalf(sim.vlow) + '٪ | بالا: ' + vToPctHalf(sim.vhigh) +
          '٪ | عدم تعادل: ' + Math.abs(sim.vhigh - sim.vlow) + ' mV' +
        '<br>کانال ۱: ' + stateText(sim.state1) + ' | کانال ۲: ' + stateText(sim.state2) +
        ' | خطا: ' + sim.fault + ' | لینک: ' + (sim.linkUp ? 'وصل' : 'قطع');
    }
    if (typeof poll === 'function') { poll(); }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', buildSimTab);
  } else {
    buildSimTab();
  }
})();
</script>
"""

PLACEHOLDERS = ("__FIELD_INDEX__", "__TLM_N__", "__FAULT_BITS__", "__STATES__", "__RANGES__",
                "__SCALES__", "__WIFI_SSID__", "__WIFI_PASS__", "__PANEL__", "__BOOT__",
                "__MA_PER_COUNT__", "__UV_PER_COUNT__")


def build_shim(field_map, tlm_n, bits, states, ranges, scales, panel, boot):
    """[FA] جای‌گذاری مقادیر مشتق‌شده در قالب JS سیمولاتور."""
    shim = SHIM
    for token, value in (
            ("__FIELD_INDEX__", json.dumps(field_map, ensure_ascii=False)),
            ("__TLM_N__", str(tlm_n)),
            ("__FAULT_BITS__", json.dumps(bits, ensure_ascii=False)),
            ("__STATES__", json.dumps(states, ensure_ascii=False)),
            ("__RANGES__", json.dumps(ranges, ensure_ascii=False)),
            ("__SCALES__", json.dumps(scales, ensure_ascii=False) if scales else "null"),
            ("__WIFI_SSID__", json.dumps(config_string("SIMPLE_WIFI_AP_SSID"))),
            ("__WIFI_PASS__", json.dumps(config_string("SIMPLE_WIFI_AP_PASS"))),
            ("__PANEL__", json.dumps(panel, ensure_ascii=False)),
            ("__BOOT__", json.dumps(boot, ensure_ascii=False)),
            ("__MA_PER_COUNT__", "%.6f" % MA_PER_COUNT),
            ("__UV_PER_COUNT__", "%.3f" % UV_PER_COUNT)):
        shim = shim.replace(token, value)
    for token in PLACEHOLDERS:
        if token in shim:
            die("placeholder %s was not replaced" % token)
    return shim


def main():
    html = panel_html()
    panel = panel_number_constants()
    rows = spec_tlm_fields()
    field_map = field_indexes(rows)
    bits = spec_fault_bits()
    states = spec_charger_states()
    tlm_n = config_define("SIMPLE_LINK_TLM_FIELD_COUNT")
    tlm_size = config_define("SIMPLE_LINK_TLM_SIZE")

    # [FA] مقابلهٔ پنل و سند: ایندکس هر فیلدی که پنل می‌خواند باید یکی باشد
    for index_name, field_name in PANEL_INDEX_MAP.items():
        if index_name not in panel:
            die("%s is not defined in simple_panel.h" % index_name)
        if field_name not in field_map:
            die("field %s is missing from the spec table" % field_name)
        if panel[index_name] != field_map[field_name]:
            die("panel says %s=%d but the spec puts %s at index %d" %
                (index_name, panel[index_name], field_name, field_map[field_name]))
    if len(field_map) != tlm_n:
        die("spec has %d u32 fields but simple_config.h says %d" % (len(field_map), tlm_n))
    if tlm_size != 4 + 4 * tlm_n:
        die("simple_config.h: TLM size %d does not match %d fields" % (tlm_size, tlm_n))
    for needed in ("INPUT_CONN_THR", "BAT_VMIN", "BAT_VMAX"):
        if needed not in panel:
            die("simple_panel.h does not define %s" % needed)

    shim = build_shim(field_map, tlm_n, bits, states, chart_ranges(), bsp_volt_scales(),
                      panel, BOOT_VOLTAGES)

    generated = html.replace("</head>", shim + "</head>", 1)
    if generated == html:
        die("could not inject the shim: </head> not found")
    OUT.write_text(generated, encoding="utf-8")
    print("wrote %s (%d bytes, %d TLM fields, %d fault bits)"
          % (OUT.name, len(generated), tlm_n, len(bits)))

    # [FA] نسخه‌های همگام بیرون از هدر: فقط دقیقاً یک خط جدید ابتدای رشتهٔ خام
    # حذف می‌شود تا فایل عیناً همان صفحهٔ پنل باشد (بدون shim سیمولاتور)
    synced = html[1:] if html.startswith("\n") else html
    for extra in (HERE / "index.html", HERE / "simple_panel_clean.html"):
        extra.write_text(synced, encoding="utf-8", newline="")
        print("synced %s (%d bytes)" % (extra.name, len(synced.encode("utf-8"))))


if __name__ == "__main__":
    main()
