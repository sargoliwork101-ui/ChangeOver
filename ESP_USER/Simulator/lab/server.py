#!/usr/bin/env python3
"""Panel lab - serve the real ESP_USER page against a simulated board.

Why
    The only way to see the user panel today is to flash an ESP or join its
    access point. This server takes the page straight out of
    ``ESP_USER/simple_panel.h`` (the same bytes the firmware ships) and answers
    every endpoint the page calls with a small board model, so the panel can be
    clicked through in a normal browser.

What is real
    * the HTML/JS/CSS: extracted from simple_panel.h, never copied by hand;
    * the endpoint names, arguments and JSON keys: taken from simple_http.h;
    * the finding texts: recorded from the project's own host harness
      (Tester/run_simple_tests.sh builds the same headers with ASan/UBSan).

What is simulated
    * the board: voltages, charger stages, fault bits, power events, CSV rows;
    * the flash: an in-memory list of day rows instead of LittleFS.

Usage
    python3 ESP_USER/Simulator/lab/server.py --port 8090
    python3 ESP_USER/Simulator/lab/server.py --export   # write FINDINGS.md
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
LAB_DIR = os.path.dirname(os.path.abspath(__file__))
SIM_DIR = os.path.dirname(LAB_DIR)
ESP_USER_DIR = os.path.dirname(SIM_DIR)
REPO_DIR = os.path.dirname(ESP_USER_DIR)
PANEL_SOURCE = os.path.join(ESP_USER_DIR, "simple_panel.h")
TESTER_SOURCE = os.path.join(ESP_USER_DIR, "Tester", "host_test_latch.cpp")
PROBE_SOURCE = os.path.join(LAB_DIR, "probe_panel_defects.cpp")
FINDINGS_MD = os.path.join(LAB_DIR, "FINDINGS.md")
SESSION_COOKIE = "co_sess"
SESSION_TOKEN = "0" * 64
SESSION_TIMEOUT_S = 1800
# ==================== panel page ====================
def load_panel_html():
    """Return the exact HTML string compiled into the firmware."""
    text = open(PANEL_SOURCE, encoding="utf-8").read()
    start = text.index('R"rawliteral(') + len('R"rawliteral(')
    end = text.rindex(')rawliteral"')
    return text[start:end]
def lab_badge():
    """Small strip added by the lab only, never by the firmware."""
    return (
        '<div id="labBar" style="position:fixed;left:8px;bottom:8px;z-index:99999;'
        'font:12px/1.6 Tahoma,sans-serif;background:#111827;color:#e5e7eb;'
        'border:1px solid #374151;border-radius:10px;padding:6px 10px;'
        'box-shadow:0 6px 20px rgba(0,0,0,.45)">'
        '\u0646\u0648\u0627\u0631 \u0634\u06cc\u0645\u06cc\u0648\u0644\u06cc\u062a\u0648\u0631 '
        '<a href="/lab" style="color:#7dd3fc;text-decoration:none">\u06cc\u0627\u0641\u062a\u0647\u200c\u0647\u0627</a>'
        ' \u00b7 <a href="/lab#harness" style="color:#7dd3fc;text-decoration:none">\u0627\u062c\u0631\u0627\u06cc \u0647\u0627\u0631\u0646\u0633</a>'
        '</div>'
    )
# ==================== board model ====================
class Board:
    """Just enough of the board to feed every field the panel reads."""
    def __init__(self):
        self.lock = threading.Lock()
        self.started = time.time()
        self.tick = 0
        self.mains = True
        self.stage1 = 0
        self.stage2 = 0
        self.vpack = 24800
        self.imbalance = 40
        self.dead = 0
        self.fault = 0
        self.fault_until = 0.0
        self.charge_count = 12
        self.reset_count = 3
        self.total_charge_s = 5 * 3600 + 17 * 60
        self.board_uptime = 9 * 3600
        self.board_time = int(time.time())
        self.rows = {}
        self.chart = {"1d": [], "7d": [], "1m": [], "3m": []}
        self.errors = []
        self.power = []
        self.charge_history = []
        self.latch = {"id": 200, "val": 0, "known": 1, "rep": 7, "req": 7, "nak": 0, "busy": 0, "q": 0}
        self.wifi = {"ssid": "changeover_userpannel", "pass": "123456789"}
        self.theme = 0
        self.users = [{"name": "admin", "role": 1, "pass": "admin"}]
        self.session = None
        self.session_start = 0.0
        self.logs_seeded = False
        self.single_session = False
        self.seed()
    def seed(self):
        """Fill the history so the charts and the event page are not empty."""
        now = int(time.time())
        day = time.strftime("%Y%m%d", time.localtime(now))
        rows = self.rows.setdefault(day, [])
        base = now - 3 * 3600
        script = [
            (0, 1, 0, "boot"), (1, 2, 1, "mains-on"), (2, 4, 0, "ch1-stage1"),
            (3, 16, 0, "fault"), (4, 16, 0, "fault-clear"), (5, 32, 0, "flags"),
            (6, 8, 0, "ch2-stage1"), (7, 2, 0, "mains-off"), (8, 128, 2, "link-lost"),
        ]
        for i, (step, chg, note, _tag) in enumerate(script):
            stamp = base + step * 300
            rows.append(self.row(stamp, chg, note, fault=(0x00000201 if _tag == "fault" else 0)))
        for rng in ("1d", "7d", "1m", "3m"):
            self.chart[rng] = self.make_chart(now, rng)
        self.errors = [
            {"ts": base + 900, "fault": 0x00000201, "vin": 24100, "vpack": 25400,
             "vlow": 12600, "vhigh": 12900, "s1": 7, "s2": 0},
            {"ts": base + 2400, "fault": 0x00000040, "vin": 0, "vpack": 24800,
             "vlow": 12400, "vhigh": 12800, "s1": 0, "s2": 3},
        ]
        self.power = [{"off": base + 2100, "on": base + 2220, "dur": 120}]
        self.charge_history = [
            {"ts": base + 600, "ch": 1, "dur": 4200},
            {"ts": base + 1200, "ch": 2, "dur": 3100},
        ]
        self.logs_seeded = True
    def make_chart(self, now, rng):
        step = {"1d": 3600, "7d": 21600, "1m": 86400, "3m": 86400}[rng]
        count = {"1d": 24, "7d": 28, "1m": 30, "3m": 60}[rng]
        out = []
        for i in range(count):
            stamp = now - (count - 1 - i) * step
            pct = 42 + (i * 2) % 30
            out.append({"pct": pct, "vin": 24000 + (i % 5) * 40,
                        "vpack": 21000 + pct * 80, "time": stamp})
        return out
    def row(self, stamp, chg, note, fault=0):
        """One CSV row, same 22 columns the firmware writes."""
        pct = max(0, min(100, (self.vpack - 21000) * 100 // 8000))
        return ",".join(str(v) for v in [
            stamp, 3600 + (self.tick % 600), chg, note, 1 if self.mains else 0,
            3 if self.mains else 2, self.stage1, self.stage2, fault, 0,
            self.vpack, 12600, 12900, self.imbalance, self.dead,
            24100, 12000, 1200, 900, 500, 500, pct,
        ])
    def advance(self):
        """Move the simulated board forward by roughly one second."""
        with self.lock:
            self.tick += 1
            self.board_uptime += 1
            cycle = self.tick % 120
            if cycle == 0:
                self.mains = not self.mains
                self.rows.setdefault(self.today(), []).append(
                    self.row(int(time.time()), 2, 0))
                if self.mains:
                    self.power.insert(0, {"off": int(time.time()) - 90,
                                          "on": int(time.time()), "dur": 90})
                    self.power = self.power[:20]
            if self.mains:
                self.vpack = min(28800, self.vpack + 6)
                self.stage1 = 1 + (self.tick // 40) % 9
                self.stage2 = (self.tick // 55) % 10
            else:
                self.vpack = max(21000, self.vpack - 4)
                self.stage1 = 0
                self.stage2 = 0
            self.imbalance = 20 + (self.tick * 7) % 260
            if self.tick % 90 == 0:
                self.fault = 0x00000201
                self.fault_until = time.time() + 20
            elif self.fault and time.time() > self.fault_until:
                self.fault = 0
                self.rows.setdefault(self.today(), []).append(
                    self.row(int(time.time()), 16, 0))
    def today(self):
        return time.strftime("%Y%m%d", time.localtime(time.time() + 0))
    def telemetry(self):
        with self.lock:
            t = [0] * 31
            t[3] = 42
            t[5] = 1234
            t[6] = self.stage1
            t[10] = 77
            t[12] = 4321
            t[13] = self.stage2
            t[14] = 24100 if self.mains else 0
            t[15] = self.vpack
            t[16] = 12000
            t[17] = 12600
            t[18] = 12900
            t[19] = self.fault
            t[25] = self.imbalance
            t[28] = self.dead
            now_s = int(time.time())
            return {
                "on": 1, "age": 400, "seq": self.tick & 0xFFFF,
                "fl": 3 if self.mains else 2, "fl2": 0,
                "n": self.tick, "ce": 0, "vm": 0,
                "inputPresent": 1 if self.mains else 0,
                "user": self.current_user(), "role": self.current_role(),
                "theme": self.theme,
                "boardTime": self.board_time + self.tick,
                "bt": self.board_time + self.tick,
                "timeOk": 1, "pvOk": 1, "recovery": 0, "lko": 1,
                "bpv": 2, "bfw": 118, "bfl": 1, "brx": 51234,
                "bce": 0, "bvm": 0, "bpc": 171, "bpr": 1000,
                "hac": 812, "hls": 0, "lac": 1000,
                "t": t,
                "pv": [1000, 10, 1000, 150, 21000, 29000, 100, 95,
                       5, 2, 2, 3, 21000, 29000, 21000, 28000, 500, 500, 1000],
            }
    def current_user(self):
        return self.session["user"] if self.session else ""
    def current_role(self):
        return self.session["role"] if self.session else 0
BOARD = Board()
# ==================== findings text ====================
def findings_data():
    """Every reviewed defect: summary, one measured example, one suggestion."""
    return [
        {
            "id": 1, "sev": "critical", "proof": "measured",
            "title": "یک بار خاموش/روشن بدون ورود = پاک شدن کاربران و برگشت admin/admin",
            "summary": "اگر پنل ۱۰ ثانیه بدون ورود روشن بماند، یک «پرچم بازیابی» روی حافظه می‌نویسد؛ در روشن‌شدن بعدی، اگر پرچم باشد، همهٔ کاربران حذف و فقط admin/admin ساخته می‌شود. هیچ تأیید فیزیکی یا رمزی لازم نیست.",
            "example": "با هارنس خود پروژه (همان کد C پنل، ASan/UBSan): رمز admin را به MyStrongPass1 تغییر دادم → پرچم بازیابی ساخته شد → یک «بوت» بعدی شبیه‌سازی کردم → admin/admin معتبر شد، رمز قوی باطل شد، پرچم «بازیابی انجام شد» بالا رفت.",
            "fix": "بازگردانی خودکار رمز حذف شود؛ فقط با تأیید فیزیکی (دکمهٔ داخل دستگاه یا کد یک‌بارمصرف) و با ثبت رویداد در تاریخچه.",
        },
        {
            "id": 2, "sev": "critical", "proof": "measured",
            "title": "فایل کاربران خراب یا دست‌کاری‌شده، بی‌صدا رمز را به admin/admin برمی‌گرداند",
            "summary": "اگر رمزگشایی فایل کاربران شکست بخورد (قطع برق در لحظهٔ نوشتن، بیت خراب، ویرایش دستی)، پنل به‌جای «قفل امن» رمزهای پیش‌فرض را می‌سازد و ذخیره می‌کند.",
            "example": "در همان هارنس: فایل کاربران را با محتوای ناقص بازنویسی کردم → بدون هیچ پیامی، رمز تازه باطل و admin/admin معتبر شد (تعداد کاربر ۱).",
            "fix": "در حالت خرابی دسترسی بسته شود و پیام واضح روی پنل بیاید؛ بازگردانی فقط با تأیید مالک و همراه با ثبت رویداد.",
        },
        {
            "id": 3, "sev": "critical", "proof": "measured",
            "title": "قواعد ساخت کاربر دور زده می‌شود: رمز یک‌حرفی و نقش بی‌معنا",
            "summary": "مسیر «افزودن کاربر» فقط طول صفر را رد می‌کند؛ حداقل ۴ کاراکترِ مسیر تغییر رمز اینجا اعمال نمی‌شود و نقش هم هر عددی پذیرفته می‌شود، در حالی که همه‌جای دیگر فقط ۰ (ساده) و ۱ (ادمین) معنا دارد.",
            "example": "AddUser(bob, «1») قبول شد و bob با همان رمز یک‌حرفی وارد شد؛ AddUser(eve, «1234», نقش ۹) هم قبول شد.",
            "fix": "همان اعتبارسنجی تغییر رمز اینجا هم اجرا شود و نقش فقط ۰/۱ باشد؛ نقش ۹ نباید ذخیره شود.",
        },
        {
            "id": 4, "sev": "high", "proof": "measured",
            "title": "از رکورد نهم خطا به بعد، داده از جای اشتباه خوانده می‌شود",
            "summary": "در پیام «دادهٔ عیب» آفست هر رکورد ۳۲ بایت جلو می‌رود ولی به عدد ۸ بیتی تبدیل می‌شود؛ از رکورد هشتم به بعد آفست از ۲۵۵ می‌گذرد و برمی‌گردد، پس تاریخچهٔ خطاها می‌تواند بی‌سروصدا غلط ذخیره شود.",
            "example": "یک بستهٔ ۱۰ رکوردی به همان پارسر پنل دادم: رکورد ۸ و ۹ به‌جای ۱۷۰۰۰۰۸۰۰۰ و ۱۷۰۰۰۰۹۰۰۰، زمانِ رکورد ۱ و ۲ (۱۷۰۰۰۰۰۰۰۰ و ۱۷۰۰۰۰۱۰۰۰) را گرفتند.",
            "fix": "آفست ۱۶ بیتی شود؛ رکوردهای قطعی برق در امنیت هستند و فقط مسیر خطا خراب است.",
        },
        {
            "id": 5, "sev": "high", "proof": "code",
            "title": "قفل ورود فقط ظاهری است؛ خواندن داده و ست‌کردن ساعت بدون ورود ممکن است",
            "summary": "صفحه از لحظهٔ باز شدن بدون ورود داده می‌گیرد و لایهٔ ورود فقط گرافیکی است. روی سرور، تله‌متری، نمودارها، عیب‌ها، تاریخچهٔ رویداد و دانلود CSV بدون بررسی نشست پاسخ می‌دهند و «ست‌کردن ساعت» هم بدون ورود نوشته می‌شود. روی ورود هم هیچ تأخیر یا قفلِ بعد از چند تلاش اشتباه نیست و حداقل رمز ۴ کاراکتر است.",
            "example": "در همین شبیه‌ساز: در مرورگر یک تب دیگر آدرس /t و /diag و /evt را باز کن بدون ورود — همان داده‌ای را می‌بینی که پنل بعد از ورود نشان می‌دهد؛ و /clock بدون ورود جواب ok می‌دهد (در شبیه‌ساز هم عمداً همین رفتار بازتولید شده تا ببینی).",
            "fix": "بررسی نشست برای مسیرهای خواندن حساس (دانلود و عیب‌ها)، تأخیر فزاینده روی ورود، حداقل رمز ۸، و ست‌کردن ساعت فقط با ورود یا با بازهٔ مجاز.",
        },
        {
            "id": 6, "sev": "high", "proof": "measured",
            "title": "ظرفیت تاریخچه با عددهای خودش نمی‌خواند",
            "summary": "سقف ۴۰۰ ردیف در روز و ۳۰ روز تعریف شده، ولی بودجهٔ کل حافظه ۱۲۰ کیلوبایت است. با اندازهٔ واقعی ردیف، روزهای پر فقط حدود ۳ روز جا می‌شوند.",
            "example": "ردیف واقعی ساخته‌شده با همان تابع پنل: ۸۹ بایت → ۴۰۰ ردیف = ۳۶٬۰۰۰ بایت در روز → ۱۲۰٬۰۰۰ بایت بودجه = ۳ روز کامل.",
            "fix": "یا سقف ردیف/بودجه هم‌تراز شود (مثلاً ۱۵۰ ردیف در روز) یا در متن به کاربر گفته شود «۳۰ روز فقط با حداکثر ~۹۰ ردیف در روز».",
        },
        {
            "id": 7, "sev": "high", "proof": "measured",
            "title": "حجم صفحهٔ پنل از سقف خودش گذشته و هیچ‌جا چک نمی‌شود",
            "summary": "سقف ۱۷۵٬۰۰۰ بایت تعریف شده ولی صفحه ۱۸۲٬۸۳۶ بایت است و هیچ بررسی زمان کامپایل یا تستی جلوی رشد بعدی را نمی‌گیرد.",
            "example": "اندازهٔ رشتهٔ HTML همان‌طور که در فرم‌ور کامپایل می‌شود: ۱۸۲٬۸۳۶ بایت (سقف: ۱۷۵٬۰۰۰) — یعنی ۷٬۸۳۶ بایت بیشتر.",
            "fix": "یک بررسی کامپایل‌زمان مثل ماژول کالیبراسیون اضافه شود و صفحه واقعاً زیر سقف بیاید یا سقف واقعی‌تر شود.",
        },
        {
            "id": 8, "sev": "medium", "proof": "measured",
            "title": "فقط یک نفر می‌تواند وارد شود",
            "summary": "پنل یک توکن نشست سراسری دارد؛ با ورود ادمین، حتی همان ادمین از گوشی دوم «مشغول» می‌گیرد و کاربر ساده هم تا پایان مهلت ۳۰ دقیقه‌ای نمی‌تواند وارد شود.",
            "example": "ورود ادمین = ۱؛ همان ادمین از دستگاه دوم = −۱ (مشغول)؛ کاربر ساده در همان حال = −۱.",
            "fix": "چند نشست هم‌زمان، یا حداقل ورود دوبارهٔ همان کاربر نشست قبلی را آزاد کند.",
        },
        {
            "id": 9, "sev": "medium", "proof": "code",
            "title": "شمارنده‌های عیب و قطعی برق فقط بالا می‌روند",
            "summary": "شاخهٔ «برگشت شمارندهٔ برد به صفر» خالی است؛ اگر برد شمارنده‌اش را صفر کند، پنل صفر شدن را نمی‌پذیرد و خطاهای قدیمی تا پاک‌سازی دستی می‌مانند.",
            "example": "در کد، شرط «تعداد جدید صفر است و پنل چیزی دارد» بدنهٔ خالی دارد؛ تنها راه پاک شدن، دکمهٔ پاک‌سازی است که پرچم جدا می‌فرستد.",
            "fix": "یا کاهش شمارنده پذیرفته شود، یا «تاریخچهٔ محلی» از «شمارندهٔ برد» جدا و در متن هم مشخص شود.",
        },
        {
            "id": 10, "sev": "medium", "proof": "code",
            "title": "مقایسهٔ امضای رمزنگاری با حافظهٔ مقدارنگرفته",
            "summary": "اگر محتوای هگز یک فایل ذخیره‌شده خراب باشد، تابع امضا پیش از پایان کار برمی‌گردد و بافر امضا پر نشده می‌ماند؛ بعد همان حافظه با مقدار ذخیره‌شده مقایسه می‌شود (رفتار تعریف‌نشده و خلاف قاعدهٔ MISRA).",
            "example": "در مسیر باز شدن فایل کاربران/وای‌فای، اگر یک جفت کاراکتر هگز نامعتبر باشد، خروجی تابع امضا کامل نوشته نمی‌شود ولی مقایسهٔ ۱۶ بایتی انجام می‌شود.",
            "fix": "تابع امضا در خطا هم بافر را صفر کند و وضعیت خطا برگرداند تا مقایسه انجام نشود.",
        },
        {
            "id": 11, "sev": "medium", "proof": "code",
            "title": "فریم نیمه‌کاره می‌تواند پارسر را معطل کند",
            "summary": "اگر وسط یک فریم یک بایت گم شود، پارسر تا رسیدن به طول اعلام‌شده منتظر می‌ماند و تا آن موقع فریم‌های سالم بعدی را به‌عنوان ادامهٔ همان فریم می‌خورد. تایم‌اوت «فریم نیمه‌کاره» وجود ندارد.",
            "example": "بایت گم‌شده + طول اعلام‌شدهٔ ۵۱۲ → پارسر در حالت انتظارِ بدنه می‌ماند تا ۵۱۲ بایت برسد؛ در این فاصله دادهٔ زنده از دست می‌رود.",
            "fix": "تایم‌اوت کوتاه برای هر فریم (مثلاً ۲۰۰ میلی‌ثانیه) و برگشت به حالت جست‌وجوی شروع فریم.",
        },
        {
            "id": 12, "sev": "medium", "proof": "code",
            "title": "رمزنگاری فایل‌ها در عمل پنهان‌سازی است",
            "summary": "کلید رمزنگاری از نمک ثابت داخل کد + شناسهٔ تراشه ساخته می‌شود، و رمزها با یک دور SHA-256 و یک نمک ثابت مشترک بین همهٔ دستگاه‌ها ذخیره می‌شوند. هر کس فلش را بخواند یا نسخهٔ پشتیبان فایل‌ها را داشته باشد، می‌تواند رمزها را برگرداند.",
            "example": "کلید = SHA256(«ChangeOver_FS_2026_v1» + شناسه تراشه)؛ رمز ذخیره‌شده = SHA256(«ChangeOver2026_Salt_» + رمز) بدون نمک اختصاصی هر کاربر.",
            "fix": "حداقل چند هزار دور کشش کلید با نمک تصادفی برای هر کاربر، یا اگر سطح امنیت همین است، در مستندات صریح گفته شود.",
        },
    ]
SEV_LABEL = {"critical": "بحرانی", "high": "مهم", "medium": "متوسط"}
PROOF_LABEL = {"measured": "اندازه‌گیری‌شده", "code": "از خواندن کد"}
# ==================== HTML: lab page ====================
LAB_CSS = """
:root { color-scheme: dark; }
* { box-sizing: border-box; }
body { margin:0; background:#0b1220; color:#e6edf7;
  font:14px/1.9 Tahoma, Vazirmatn, sans-serif; }
a { color:#7dd3fc; }
header { padding:22px 26px 10px; }
h1 { margin:0 0 6px; font-size:20px; }
.sub { color:#94a3b8; font-size:13px; max-width:900px; }
.wrap { display:grid; gap:14px; padding:16px 26px 60px; max-width:1100px; }
.card { background:#111a2e; border:1px solid #1f2b45; border-radius:14px; padding:16px 18px; }
.card h2 { margin:0 0 10px; font-size:15px; display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
.chip { font-size:11px; padding:2px 9px; border-radius:999px; border:1px solid; white-space:nowrap; }
.c-critical { color:#ffb4b4; border-color:#7f1d1d; background:#3b0d0d; }
.c-high { color:#ffd08a; border-color:#7c4a03; background:#3a2404; }
.c-medium { color:#bfe0ff; border-color:#1e3a5f; background:#0e2238; }
.c-proof { color:#a7f3d0; border-color:#065f46; background:#04291f; }
.c-code { color:#cbd5e1; border-color:#334155; background:#151f31; }
p { margin:6px 0; }
.lbl { color:#94a3b8; font-size:12px; display:block; margin-top:10px; }
pre { background:#0a1020; border:1px solid #1f2b45; border-radius:10px;
  padding:10px 12px; overflow:auto; direction:ltr; text-align:left;
  font:12px/1.7 ui-monospace, Consolas, monospace; color:#cfe3ff; }
.fix { border-inline-start:3px solid #22c55e; padding-inline-start:10px; }
button { font:inherit; background:#1d4ed8; color:#fff; border:0; border-radius:10px;
  padding:8px 14px; cursor:pointer; }
button:hover { background:#2563eb; }
button.ghost { background:#1e293b; }
.row { display:flex; gap:10px; flex-wrap:wrap; align-items:center; }
.note { color:#94a3b8; font-size:12px; }
#out { min-height:60px; white-space:pre-wrap; }
"""
def render_lab_page():
    cards = []
    for f in findings_data():
        cards.append(
            '<section class="card" id="f%d">'
            '<h2>%d. %s'
            '<span class="chip c-%s">%s</span>'
            '<span class="chip %s">%s</span></h2>'
            '<span class="lbl">\u062e\u0644\u0627\u0635\u0647</span><p>%s</p>'
            '<span class="lbl">\u0645\u062b\u0627\u0644 \u0648\u0627\u0642\u0639\u06cc</span><p>%s</p>'
            '<span class="lbl">\u067e\u06cc\u0634\u0646\u0647\u0627\u062f</span>'
            '<p class="fix">%s</p></section>'
            % (f["id"], f["id"], f["title"], f["sev"], SEV_LABEL[f["sev"]],
               "c-proof" if f["proof"] == "measured" else "c-code",
               PROOF_LABEL[f["proof"]], f["summary"], f["example"], f["fix"]))
    return (
        '<!doctype html><html lang="fa" dir="rtl"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width,initial-scale=1">'
        '<title>\u06cc\u0627\u0641\u062a\u0647\u200c\u0647\u0627\u06cc \u067e\u0646\u0644 ESP</title>'
        '<style>' + LAB_CSS + '</style></head><body>'
        '<header><h1>\u06cc\u0627\u0641\u062a\u0647\u200c\u0647\u0627\u06cc \u067e\u0646\u0644 \u06a9\u0627\u0631\u0628\u0631\u06cc ESP</h1>'
        '<div class="sub">\u062f\u0648\u0627\u0632\u062f\u0647 \u0645\u0648\u0631\u062f\u060c '
        '\u0647\u0631 \u06a9\u062f\u0627\u0645 \u0628\u0627 \u06cc\u06a9 \u0645\u062b\u0627\u0644 \u0648 \u06cc\u06a9 \u067e\u06cc\u0634\u0646\u0647\u0627\u062f. '
        '\u0628\u0631\u0686\u0633\u0628 \u00ab\u0627\u0646\u062f\u0627\u0632\u0647\u200c\u06af\u06cc\u0631\u06cc\u200c\u0634\u062f\u0647\u00bb '
        '\u06cc\u0639\u0646\u06cc \u062e\u0631\u0648\u062c\u06cc \u0647\u0627\u0631\u0646\u0633 \u062e\u0648\u062f \u067e\u0631\u0648\u0698\u0647 \u0627\u0633\u062a \u0648 \u067e\u0627\u06cc\u06cc\u0646 \u0645\u06cc\u200c\u062a\u0648\u0627\u0646\u06cc '
        '\u0645\u062c\u062f\u062f\u0627\u064b \u0627\u062c\u0631\u0627\u0634 \u06a9\u0646\u06cc. '
        '<a href="/">\u0628\u0627\u0632\u06af\u0634\u062a \u0628\u0647 \u067e\u0646\u0644</a></div></header>'
        '<div class="wrap">'
        + "".join(cards) +
        '<section class="card" id="tools"><h2>ابزار شیمیولیتور</h2>'
        '<p>بدون ورود همین آدرس‌ها را باز کن (ایراد ۵): '
        '<a href="/t" target="_blank">/t</a> · '
        '<a href="/diag" target="_blank">/diag</a> · '
        '<a href="/charts?range=1d" target="_blank">/charts</a> · '
        '<a href="/evt" target="_blank">/evt</a> · '
        '<a href="/evt/dl" target="_blank">/evt/dl</a></p>'
        '<div class="row"><button class="ghost" onclick="single(true)">'
        'روشن کردن قفل نشست تکی (ایراد ۸)</button>'
        '<button class="ghost" onclick="single(false)">خاموش کردن</button>'
        '<button class="ghost" onclick="resetSess()">آزاد کردن نشست</button>'
        '<span class="note" id="toolMsg"></span></div></section>'
        '<section class="card" id="harness"><h2>\u0627\u062c\u0631\u0627\u06cc \u0647\u0627\u0631\u0646\u0633 \u0631\u0648\u06cc \u06a9\u062f \u0648\u0627\u0642\u0639\u06cc</h2>'
        '<p>\u0647\u0645\u06cc\u0646 \u0647\u062f\u0631\u0647\u0627\u06cc \u067e\u0646\u0644 \u0628\u0627 \u0634\u06cc\u0645\u200c\u0647\u0627\u06cc \u0645\u0648\u062c\u0648\u062f '
        '\u06a9\u0627\u0645\u067e\u0627\u06cc\u0644 \u0648 \u0627\u062c\u0631\u0627 \u0645\u06cc\u200c\u0634\u0648\u062f (\u0628\u0627\u0631 \u0627\u0648\u0644 \u06a9\u0645\u06cc \u0637\u0648\u0644 \u0645\u06cc\u200c\u06a9\u0634\u062f).</p>'
        '<div class="row"><button onclick="runHarness()">\u0627\u062c\u0631\u0627</button>'
        '<span class="note">\u0647\u06cc\u0686 \u0641\u0627\u06cc\u0644\u06cc \u062f\u0631 \u067e\u0648\u0634\u0647\u0654 \u067e\u0646\u0644 \u062a\u063a\u06cc\u06cc\u0631 \u0646\u0645\u06cc\u200c\u06a9\u0646\u062f\u061b '
        '\u062e\u0631\u0648\u062c\u06cc \u062e\u0627\u0645 \u0627\u0633\u062a.</span></div>'
        '<pre id="out">-</pre></section>'
        '</div><script>'
        'async function single(on){const r=await fetch("/lab/api/single?on="+(on?1:0),{method:"POST"});'
        'const j=await r.json();document.getElementById("toolMsg").textContent='
        'j.single?"\u0642\u0641\u0644 \u062aک\u06cc \u0631وش\u0646 \u0634\u062f: \u0628ا\u0631 \u062fو\u0645 \u0645ی\u200c\u06af\u06cc\u0631ی \u0645ش\u063a\u0648\u0644":"\u0642\u0641\u0644 \u062aک\u06cc \u062eا\u0645\u0648ش \u0634\u062f";}'
        'async function resetSess(){await fetch("/lab/api/reset",{method:"POST"});document.getElementById("toolMsg").textContent="\u0646\u0634\u0633\u062a \u0622\u0632\u0627\u062f \u0634\u062f";}'
        'async function runHarness(){const o=document.getElementById("out");'
        'o.textContent="\\u062f\u0631 \u062d\u0627\u0644 \u06a9\u0627\u0645\u067e\u0627\u06cc\u0644...";'
        'try{const r=await fetch("/lab/harness",{method:"POST"});'
        'o.textContent=await r.text();}catch(e){o.textContent="error: "+e;}}'
        '</script></body></html>'
    )
def render_findings_md():
    lines = ["# \u06cc\u0627\u0641\u062a\u0647\u200c\u0647\u0627\u06cc \u067e\u0646\u0644 \u06a9\u0627\u0631\u0628\u0631\u06cc ESP", ""]
    for f in findings_data():
        lines.append("## %d. %s" % (f["id"], f["title"]))
        lines.append("")
        lines.append("- **\u0634\u062f\u062a:** %s \u00b7 **\u0645\u062f\u0631\u06a9:** %s"
                     % (SEV_LABEL[f["sev"]], PROOF_LABEL[f["proof"]]))
        lines.append("- **\u062e\u0644\u0627\u0635\u0647:** %s" % f["summary"])
        lines.append("- **\u0645\u062b\u0627\u0644 \u0648\u0627\u0642\u0639\u06cc:** %s" % f["example"])
        lines.append("- **\u067e\u06cc\u0634\u0646\u0647\u0627\u062f:** %s" % f["fix"])
        lines.append("")
    return "\n".join(lines)
# ==================== harness ====================
HARNESS_LOCK = threading.Lock()
HARNESS_LAST = {"text": ""}
def build_and_run_harness():
    """Compile the probe against the real panel headers and return its output."""
    with HARNESS_LOCK:
        test_text = open(TESTER_SOURCE, encoding="utf-8").read().splitlines()
        try:
            start = next(i for i, l in enumerate(test_text) if l.startswith('#include "stub_arduino.h"'))
            stop = next(i for i, l in enumerate(test_text) if l.startswith("static uint32_t UINT32_T__G__Checks"))
        except StopIteration:
            return "harness: the stub block of host_test_latch.cpp was not found"
        stub_block = "\n".join(test_text[start:stop])
        probe_body = open(PROBE_SOURCE, encoding="utf-8").read()
        work = tempfile.mkdtemp(prefix=".lab_build_", dir=SIM_DIR)
        try:
            source = os.path.join(work, "probe.cpp")
            binary = os.path.join(work, "probe")
            with open(source, "w", encoding="utf-8") as handle:
                handle.write(stub_block + "\n" + probe_body)
            build = subprocess.run(
                ["g++", "-std=gnu++17", "-Wall", "-Wextra", "-Wno-unused-function",
                 "-Wno-unused-variable", "-fsanitize=address,undefined",
                 "-fno-omit-frame-pointer", "-g",
                 "-I", os.path.join(SIM_DIR, "stubinc"), "-I", SIM_DIR,
                 "-o", binary, source],
                capture_output=True, text=True, timeout=300)
            if build.returncode != 0:
                return "BUILD FAILED\n" + build.stderr[-4000:]
            run = subprocess.run([binary], capture_output=True, text=True, timeout=120,
                                 env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0",
                                          UBSAN_OPTIONS="print_stacktrace=1"))
            text = run.stdout + ("\n" + run.stderr if run.stderr.strip() else "")
            HARNESS_LAST["text"] = text
            return text or "(no output)"
        except subprocess.TimeoutExpired:
            return "harness: timeout"
        finally:
            shutil.rmtree(work, ignore_errors=True)
# ==================== HTTP ====================
def json_bytes(payload, status=200):
    return status, "application/json", json.dumps(payload).encode("utf-8")
def text_bytes(text, status=200, mime="text/plain; charset=utf-8"):
    return status, mime, text.encode("utf-8")
class Handler(BaseHTTPRequestHandler):
    server_version = "PanelLab"
    protocol_version = "HTTP/1.1"
    def log_message(self, fmt, *args):
        return
    def body_args(self):
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length else b""
        args = {}
        if not raw:
            return args
        text = raw.decode("utf-8", "replace")
        try:
            parsed = json.loads(text)
            if isinstance(parsed, dict):
                return {k: str(v) for k, v in parsed.items()}
        except ValueError:
            pass
        for key, value in urllib.parse.parse_qsl(text):
            args[key] = value
        return args
    def do_GET(self):
        self.route("GET")
    def do_POST(self):
        self.route("POST")
    def session_ok(self):
        cookie = self.headers.get("Cookie") or ""
        for part in cookie.split(";"):
            name, _, value = part.strip().partition("=")
            if name == SESSION_COOKIE and value == SESSION_TOKEN:
                if time.time() - BOARD.session_start > SESSION_TIMEOUT_S:
                    return False
                return True
        return False
    def send(self, result):
        status, mime, payload = result
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        for key, value in getattr(self, "extra_headers", []):
            self.send_header(key, value)
        self.extra_headers = []
        self.end_headers()
        self.wfile.write(payload)
    def route(self, method):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        query = dict(urllib.parse.parse_qsl(parsed.query))
        args = dict(query)
        if method == "POST":
            args.update(self.body_args())
        self.extra_headers = []
        BOARD.advance()
        if path == "/":
            html = load_panel_html()
            html = html.replace("</body>", lab_badge() + "</body>", 1)
            return self.send(text_bytes(html, mime="text/html; charset=utf-8"))
        if path == "/lab":
            return self.send(text_bytes(render_lab_page(), mime="text/html; charset=utf-8"))
        if path == "/lab/api/single":
            BOARD.single_session = args.get("on", "0") == "1"
            return self.send(json_bytes({"ok": 1, "single": BOARD.single_session}))
        if path == "/lab/api/reset":
            BOARD.session = None
            self.extra_headers.append((
                "Set-Cookie", "%s=; Path=/; HttpOnly; Max-Age=0" % SESSION_COOKIE))
            return self.send(json_bytes({"ok": 1}))
        if path == "/lab/harness" and method == "POST":
            return self.send(text_bytes(build_and_run_harness()))
        if path == "/favicon.ico":
            return self.send((204, "text/plain", b""))
        if path == "/t":
            return self.send(json_bytes(BOARD.telemetry()))
        if path == "/charts":
            rng = args.get("range", "1d")
            data = BOARD.chart.get(rng) or BOARD.chart["1d"]
            return self.send(json_bytes({
                "range": rng, "count": len(data),
                "pct": [d["pct"] for d in data],
                "vin": [d["vin"] for d in data],
                "vpack": [d["vpack"] for d in data],
                "time": [d["time"] for d in data],
            }))
        if path == "/diag":
            return self.send(json_bytes({
                "valid": 1, "totalChargeSec": BOARD.total_charge_s,
                "chargeCount": BOARD.charge_count, "chargeCountCh1": 7,
                "chargeCountCh2": 5, "chargeSecCh1": 9000, "chargeSecCh2": 7000,
                "uptimeSec": BOARD.board_uptime, "resetCount": BOARD.reset_count,
                "boardTime": BOARD.board_time + BOARD.tick, "panelTime": int(time.time()),
                "timeOk": 1, "errCount": len(BOARD.errors),
                "powerCount": len(BOARD.power),
                "errors": BOARD.errors, "power": BOARD.power,
                "chargeHistory": BOARD.charge_history,
            }))
        if path == "/login" and method == "POST":
            user = args.get("user", "")
            password = args.get("pass", "")
            found = next((u for u in BOARD.users if u["name"] == user and u["pass"] == password), None)
            if found is None:
                return self.send(json_bytes({"ok": 0, "e": "wrong"}, 401))
            if BOARD.single_session and BOARD.session and found["role"] <= BOARD.session["role"]:
                return self.send(json_bytes({"ok": 0, "e": "busy",
                                             "msg": "\u06cc\u06a9 \u0648\u0631\u0648\u062f \u0641\u0639\u0627\u0644 \u0627\u0633\u062a"}, 409))
            BOARD.session = {"user": found["name"], "role": found["role"]}
            BOARD.session_start = time.time()
            self.extra_headers.append((
                "Set-Cookie",
                "%s=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=%d"
                % (SESSION_COOKIE, SESSION_TOKEN, SESSION_TIMEOUT_S)))
            return self.send(json_bytes({"ok": 1, "user": found["name"], "role": found["role"]}))
        if path == "/logout":
            BOARD.session = None
            self.extra_headers.append((
                "Set-Cookie", "%s=; Path=/; HttpOnly; Max-Age=0" % SESSION_COOKIE))
            return self.send(json_bytes({"ok": 1}))
        if path == "/users" and method == "GET":
            return self.send(json_bytes({"users": [{"name": u["name"], "role": u["role"]}
                                                   for u in BOARD.users]}))
        if path in ("/users", "/adduser") and method == "POST":
            action = args.get("action", "add")
            if action == "del" or path == "/deluser":
                BOARD.users = [u for u in BOARD.users if u["name"] != args.get("user", "")]
                return self.send(json_bytes({"ok": 1}))
            name = args.get("user", "")
            if not name:
                return self.send(json_bytes({"ok": 0}, 400))
            role = int(args.get("role", "0") or 0)
            existing = next((u for u in BOARD.users if u["name"] == name), None)
            if existing:
                existing["pass"] = args.get("pass", existing["pass"])
                existing["role"] = role
            else:
                BOARD.users.append({"name": name, "role": role, "pass": args.get("pass", "")})
            return self.send(json_bytes({"ok": 1}))
        if path == "/deluser" and method == "POST":
            BOARD.users = [u for u in BOARD.users if u["name"] != args.get("user", "")]
            return self.send(json_bytes({"ok": 1}))
        if path == "/setpass" and method == "POST":
            target = next((u for u in BOARD.users if u["name"] == args.get("user", "")), None)
            if target is None:
                return self.send(json_bytes({"ok": 0, "e": "user"}, 400))
            target["pass"] = args.get("new", target["pass"])
            return self.send(json_bytes({"ok": 1}))
        if path == "/theme":
            return self.send(json_bytes({"theme": BOARD.theme}))
        if path == "/settheme" and method == "POST":
            BOARD.theme = int(args.get("theme", "0") or 0)
            return self.send(json_bytes({"ok": 1}))
        if path == "/wifi" and method == "GET":
            if not self.session_ok():
                return self.send(json_bytes({"ok": 0, "e": "admin"}, 403))
            return self.send(json_bytes(BOARD.wifi))
        if path in ("/wifi", "/setwifi") and method == "POST":
            if not self.session_ok():
                return self.send(json_bytes({"ok": 0, "e": "admin"}, 403))
            BOARD.wifi = {"ssid": args.get("ssid", BOARD.wifi["ssid"]),
                          "pass": args.get("pass", BOARD.wifi["pass"])}
            return self.send(json_bytes({"ok": 1, "reboot": 1}))
        if path == "/latch":
            return self.send(json_bytes(dict(BOARD.latch, ok=1)))
        if path == "/latch/reset" and method == "POST":
            if not self.session_ok():
                return self.send(json_bytes({"ok": 0, "e": "admin"}, 403))
            BOARD.latch["rep"] += 1
            BOARD.latch["val"] = 0
            BOARD.latch["known"] = 1
            return self.send(json_bytes({"ok": 1, "n": args.get("n", "")}))
        if path == "/clock" and method == "POST":
            return self.send(json_bytes({"ok": 1, "sec": int(time.time())}))
        if path == "/time" and method == "POST":
            if not self.session_ok():
                return self.send(json_bytes({"ok": 0, "e": "admin"}, 403))
            return self.send(json_bytes({"ok": 1}))
        if path == "/clear" and method == "POST":
            if not self.session_ok():
                return self.send(json_bytes({"ok": 0, "e": "admin"}, 403))
            return self.send(json_bytes({"ok": 1}))
        if path == "/evt":
            return self.send(text_bytes(self.event_text(args.get("day")),
                                        mime="text/plain; charset=utf-8"))
        if path == "/evt/days":
            days = sorted(BOARD.rows.keys())
            return self.send(text_bytes(" ".join(days[-30:])))
        if path == "/evt/dl":
            return self.send(text_bytes(self.event_text(args.get("day")),
                                        mime="text/csv; charset=utf-8"))
        if path == "/evt/fs":
            used = sum(len(r) + 1 for rows in BOARD.rows.values() for r in rows)
            files = [{"name": "/evt-%s.csv" % d, "size": sum(len(r) + 1 for r in rows), "own": 1}
                     for d, rows in sorted(BOARD.rows.items())]
            files.append({"name": "/users.json", "size": 180, "own": 1})
            files.append({"name": "/wifi.json", "size": 120, "own": 1})
            files.append({"name": "/charts.json", "size": 2400, "own": 1})
            return self.send(json_bytes({"fs": 1, "mountFail": 0, "total": 1048576,
                                         "used": used + 2700, "files": files,
                                         "fileBytes": used + 2700, "more": 0}))
        if path in ("/evt/clr", "/evt/format", "/evt/clean") and method == "POST":
            if not self.session_ok():
                return self.send(json_bytes({"ok": 0, "e": "admin"}, 403))
            if path == "/evt/clr":
                BOARD.rows = {}
            return self.send(json_bytes({"ok": 1, "removed": 1, "freed": 1024}))
        if path == "/evt/snap" and method == "POST":
            BOARD.rows.setdefault(BOARD.today(), []).append(
                BOARD.row(int(time.time()), 0, 6))
            return self.send(json_bytes({"ok": 1}))
        return self.send(json_bytes({"ok": 0, "e": "unknown", "path": path}, 404))
    def event_text(self, day):
        header = (
            "# \u062a\u0627\u0631\u06cc\u062e\u0686\u0647\u0654 \u0648\u0636\u0639\u06cc\u062a / state history\n"
            "# epoch,uptime_s,change_bits,note,mains,fl,chg1,chg2,fault_mask,flags2,v24_mv,"
            "vlow_mv,vhigh_mv,imbal_mv,dead_mask,vin_mv,v12_mv,i1_ma,i2_ma,"
            "duty1_permille,duty2_permille,batt_pct\n")
        chosen = day if day and day in BOARD.rows else BOARD.today()
        rows = BOARD.rows.get(chosen) or []
        return header + "\n".join(rows[-40:]) + "\n"
def main(argv):
    if "--export" in argv:
        with open(FINDINGS_MD, "w", encoding="utf-8") as handle:
            handle.write(render_findings_md())
        print("written:", FINDINGS_MD)
        return 0
    port = 8090
    if "--port" in argv:
        port = int(argv[argv.index("--port") + 1])
    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    print("panel lab on http://0.0.0.0:%d  (panel: / , findings: /lab)" % port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0
if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
