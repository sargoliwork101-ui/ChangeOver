# -*- coding: utf-8 -*-
"""Generate Documentation/charging_flowchart.png (charging flowchart, Vazirmatn).

Needs: pip install arabic-reshaper python-bidi Pillow, plus the Vazirmatn
Regular/Bold TTFs. Font lookup: $VAZIR_DIR, /tmp/vazirmatn-33.003/fonts/ttf,
~/.fonts. Colors are meaningful (legend drawn on the image):
blue = charging, green = charge complete, yellow = question/decision,
red = fault/stop, gray = wait/pause, navy = start/end.
Content mirrors charger.c (func__Charger_Evaluate + RegulateChannel).
"""
import math, os
import arabic_reshaper
from bidi.algorithm import get_display
from PIL import Image, ImageDraw, ImageFont

def _font(name):
    for base in (os.environ.get('VAZIR_DIR', ''), '/tmp/vazirmatn-33.003/fonts/ttf',
                 os.path.expanduser('~/.fonts')):
        p = os.path.join(base, name)
        if base and os.path.isfile(p):
            return p
    raise SystemExit('Vazirmatn font not found: ' + name)

FR = _font('Vazirmatn-Regular.ttf')
FB = _font('Vazirmatn-Bold.ttf')
def F(sz, bold=False): return ImageFont.truetype(FB if bold else FR, sz)
def fa(s): return get_display(arabic_reshaper.reshape(s))

BLUE_F, BLUE_O = '#DDEBF7', '#1F4E79'    # در حال شارژ
GREEN_F, GREEN_O = '#C6EFCE', '#548235'  # شارژ کامل
YEL_F, YEL_O = '#FFE699', '#BF8F00'      # سؤال و تصمیم
RED_F, RED_O = '#FFC7CE', '#C00000'      # خطا و توقف
GRY_F, GRY_O = '#EDEDED', '#808080'      # صبر و مکث
NAVY = '#1F3864'                          # شروع و پایان
HEAD = '#1F4E79'; INK = '#1F3864'

SY = 1.22  # vertical stretch so rows stay far apart
def Y(v): return int(round(v * SY))

W, H = 1600, int(4230 * SY) + 20
img = Image.new('RGB', (W, H), '#FFFFFF')
d = ImageDraw.Draw(img)

def box(x, y, w, h, fill, outline, r=18, lw=3):
    d.rounded_rectangle([x, Y(y), x+w, Y(y)+h], radius=r, fill=fill, outline=outline, width=lw)
def pill(cx, y, w, text, fill=NAVY):
    box(cx-w//2, y, w, 60, fill, fill, r=30)
    d.text((cx, Y(y)+30), fa(text), font=F(29, True), fill='#FFFFFF', anchor='mm')
def txt(cx, y, lines, sz=24, color=INK, gap=34):
    for i, ln in enumerate(lines):
        d.text((cx, Y(y) + i*gap), fa(ln), font=F(sz, i == 0), fill=color, anchor='mm')
def arrow(x1, y1, x2, y2, color=HEAD, lw=4):
    d.line([x1, Y(y1), x2, Y(y2)], fill=color, width=lw)
    d.polygon([(x2, Y(y2)), (x2-9, Y(y2)-16), (x2+9, Y(y2)-16)], fill=color)
def elbow(points, color=HEAD, lw=4, dash=False, head=True):
    points = [(x, Y(y)) for (x, y) in points]
    for i in range(len(points)-1):
        x1, y1 = points[i]; x2, y2 = points[i+1]
        if dash:
            dist = math.hypot(x2-x1, y2-y1); steps = max(int(dist//18), 1)
            for s in range(steps):
                if s % 2 == 0:
                    d.line([x1+(x2-x1)*s/steps, y1+(y2-y1)*s/steps,
                            x1+(x2-x1)*(s+1)/steps, y1+(y2-y1)*(s+1)/steps], fill=color, width=lw)
        else:
            d.line([x1, y1, x2, y2], fill=color, width=lw)
    if head:
        x2, y2 = points[-1]
        d.polygon([(x2, y2), (x2-16, y2-9), (x2-16, y2+9)], fill=color)
def diamond(cx, cy, w, h, lines, sz=24):
    cy = Y(cy)
    d.polygon([(cx, cy-h//2), (cx+w//2, cy), (cx, cy+h//2), (cx-w//2, cy)], fill=YEL_F, outline=YEL_O, width=3)
    n = len(lines)
    for i, ln in enumerate(lines):
        d.text((cx, cy + (i-(n-1)/2)*34), fa(ln), font=F(sz), fill=INK, anchor='mm')
def label_h(x, y, s, color=INK, sz=23):
    t = fa(s); w = d.textlength(t, font=F(sz, True))
    d.rectangle([x-w/2-10, Y(y)-20, x+w/2+10, Y(y)+20], fill='#FFFFFF')
    d.text((x, Y(y)), t, font=F(sz, True), fill=color, anchor='mm')
def label_v(x, y, s, color=INK, sz=23):
    t = fa(s)
    tmp = ImageDraw.Draw(Image.new('RGB', (10, 10)))
    w = int(tmp.textlength(t, font=F(sz, True))) + 24
    tile = Image.new('RGB', (w, 44), '#FFFFFF')
    td = ImageDraw.Draw(tile)
    td.text((w//2, 22), t, font=F(sz, True), fill=color, anchor='mm')
    tile = tile.rotate(90, expand=True)
    img.paste(tile, (int(x - tile.width/2), int(Y(y) - tile.height/2)))
def section(y, s):
    d.text((1560, Y(y)), fa(s), font=F(27, True), fill=NAVY, anchor='ra')
    d.line([40, Y(y)+30, 1560, Y(y)+30], fill='#B9CDE4', width=3)
def side_r(cy, lines, fill, outline, dash_color):
    box(1090, cy-52, 440, 104, fill, outline)
    txt(1310, cy-34, lines, 22, gap=32)
    elbow([(1090, cy), (1019, cy)], dash_color, dash=True)

CX = 800
d.text((CX, Y(6)), fa('فلوچارت شارژ — از بررسی‌های کلی تا فرمان خروجی'), font=F(34, True), fill=NAVY, anchor='ma')

# ================= بخش ۱: بررسی‌های کلی =================
section(58, 'بخش ۱: بررسی‌های کلی — در هر پاس، قبل از شارژ')
pill(CX, 108, 360, 'شروع پاس شارژر')
arrow(CX, 168, CX, 190)

gates = [
    (250, ['ذخیرهٔ حافظه (NVM)', 'در حال انجام است؟'], 'نه',
     (['هر دو خروجی صفر می‌شود و این پاس تمام', 'شارژ از پاس بعد خودش ادامه پیدا می‌کند'], GRY_F, GRY_O, GRY_O)),
    (430, ['قفل خطای فنی (خطای ۷)', 'از قبل ست شده؟'], 'نه',
     (['قطع نهایی: رله باز و هر دو خروجی صفر', 'تا خاموش و روشن‌شدن برد سر جایش می‌ماند'], RED_F, RED_O, RED_O)),
    (610, ['مود تست دستی پنل', 'فعال است؟'], None,
     (['duty را پنل می‌فرستد و گیت‌های باتری کنار می‌روند', '۳ ثانیه سکوت لینک = خروج خودکار از مود'], GRY_F, GRY_O, GRY_O)),
    (790, ['سیم باتری قطع است؟', '(خطای ۶)'], 'نه',
     (['کانال متوقف و خروجی صفر می‌شود', 'با برگشت باتری و پاک‌شدن خطا، کانال آزاد می‌شود'], RED_F, RED_O, RED_O)),
    (970, ['اندازه‌گیری سالم است و حالت', 'کلی دستگاه عادی است؟'], 'بله',
     (['توقف امن: همهٔ خروجی‌ها صفر', 'قفل‌های خطا (نهایی، قطع باتری، حرارتی) پاک نمی‌شوند'], GRY_F, GRY_O, GRY_O)),
    (1150, ['ولتاژ ورودی بین', '۲۲ و ۲۸ ولت است؟'], 'بله',
     (['صبر برای ورودی (INPUT_WAIT): شارژ صفر', 'با درست‌شدن ورودی، شارژ خودش ادامه پیدا می‌کند'], GRY_F, GRY_O, GRY_O)),
]
prev_bottom = 168
for cy, lines, down_label, (slines, sf, so, dc) in gates:
    diamond(CX, cy, 440, 120, lines)
    arrow(CX, prev_bottom, CX, cy-60)
    if down_label:
        label_v(CX, (prev_bottom + cy-60)//2, down_label)
    side_r(cy, slines, sf, so, dc)
    prev_bottom = cy+60

# خطای ۷ — بررسی جدا و موازی
arrow(CX, 1210, CX, 1330)
box(1090, 1298, 440, 128, RED_F, RED_O)
txt(1310, 1326, ['بررسی جدا و موازی: خطای فنی (۷)', 'duty بالای ۲۰٪ ولی جریان صفر؟ یا رله باز همراه تریپ؟', 'همان لحظه قفل فنی + ثبت خطا در حافظهٔ دیاگ'], 22, gap=32)
elbow([(1090, 1362), (1019, 1362)], RED_O, dash=True)
label_v(CX, 1270, 'بله')

# ================= بخش ۲: بررسی هر کانال =================
section(1470, 'بخش ۲: بررسی هر کانال — کانال ۱ و ۲ جدا از هم')
pill(CX, 1522, 360, 'برای هر کانال')
arrow(CX, 1582, CX, 1600)

cgates = [
    (1660, ['کانال از پنل', 'خاموش شده؟'], 'نه',
     (['کانال خاموش می‌ماند؛ خروجی صفر', 'با وصل از پنل، شارژ با شروع نرم از ۱٪ آغاز می‌شود'], GRY_F, GRY_O, GRY_O)),
    (1830, ['قفل باتری خراب یا بودجهٔ', 'عدم‌توازن تمام شده؟'], 'نه',
     (['شارژ این کانال بسته است و خروجی صفر', 'باتری خراب: تا تعویض باتری؛ عدم‌توازن: تا پایان بودجه'], RED_F, RED_O, RED_O)),
    (2000, ['ولتاژ باتری بین ۲ و ۱۵', 'ولت است؟'], 'بله',
     (['کانال خاموش و منتظر می‌ماند', 'همین جلوی شارژِ باتریِ قطع یا کاملاً خراب را می‌گیرد'], GRY_F, GRY_O, GRY_O)),
    (2170, ['جریان از ۹۵۰ میلی‌آمپر', 'بالاتر رفته؟'], 'نه',
     (['ریست کانال به خاموشی؛ شروع بعدی از شروع نرم', 'حفاظت سریعِ اضافه‌جریان با JIT سخت‌افزاری است'], RED_F, RED_O, RED_O)),
]
prev_bottom = 1582
for cy, lines, down_label, (slines, sf, so, dc) in cgates:
    diamond(CX, cy, 440, 110, lines)
    arrow(CX, prev_bottom, CX, cy-55)
    if down_label:
        label_v(CX, (prev_bottom + cy-55)//2, down_label)
    side_r(cy, slines, sf, so, dc)
    prev_bottom = cy+55

# ================= بخش ۳: فازهای شارژ =================
section(2320, 'بخش ۳: فازهای شارژ')
arrow(CX, 2225, CX, 2410)
diamond(CX, 2470, 440, 120, ['اتصال باتری ۱۵ ثانیه', 'پایدار بوده؟'])
side_r(2470, ['نه: کانال خاموش می‌ماند', 'شمارش از لحظهٔ دیده‌شدن ولتاژ باتری شروع می‌شود'], GRY_F, GRY_O, GRY_O)
arrow(CX, 2530, CX, 2610)
label_v(CX, 2570, 'بله')

box(585, 2614, 430, 116, BLUE_F, BLUE_O)
txt(CX, 2644, ['شارژ سریع (BULK)', 'شروع نرم از duty یک‌درصد؛ تنظیم‌گر PID', 'جریان را تا سقف ۶۵۰ میلی‌آمپر نگه می‌دارد'], 23, gap=33)
box(1090, 2614, 440, 116, GRY_F, GRY_O)
txt(1310, 2644, ['داغ‌شدن: ۱ ساعت نزدیکِ سقف جریان؟', '۱۰ دقیقه استراحت (زرد چشمک‌زن) و بعد ادامه', 'شمارش فقط در شارژ سریع و نزدیک سقف است'], 22, gap=33)
elbow([(1090, 2672), (1019, 2672)], GRY_O, dash=True)
box(40, 2614, 400, 116, GRY_F, GRY_O)
txt(240, 2644, ['تریپ JIT (اضافه‌جریان سخت‌افزاری)', 'توقف فوری؛ پس از ۳ ثانیه احیای یکی‌یکی با duty کمتر', 'تریپ سومِ همان کانال = خطای نهایی و قطع رله'], 21, gap=33)
elbow([(585, 2672), (444, 2672)], GRY_O, dash=True)

arrow(CX, 2730, CX, 2770)
diamond(CX, 2840, 380, 130, ['ولتاژ به ۱۴٫۳ رسید؟'])
elbow([(610, 2840), (548, 2840), (548, 2658), (581, 2658)], HEAD)
label_v(548, 2749, 'نه')
arrow(CX, 2905, CX, 2945)
label_v(CX, 2925, 'بله')
box(585, 2949, 430, 130, BLUE_F, BLUE_O)
txt(CX, 2979, ['جذب (ABSORB)', 'نگه‌داری ۱۴٫۴ ولت با PID؛ بالای ۱۴٫۶ کم‌کردن پله‌ای', 'سقف پشتیبان همیشه ۱۴٫۸ ولت است'], 23, gap=33)
elbow([(585, 2985), (505, 2985), (505, 2694), (581, 2694)], RED_O)
label_v(505, 2840, 'زیر ۱۴٫۳؟ برگرد به شارژ سریع', RED_O)
box(40, 2949, 400, 150, RED_F, RED_O)
txt(240, 2979, ['شمارش موازیِ باتری خراب (سناریوی ۶)', 'فقط وقتی کانال واقعاً شارژ می‌کند می‌شمارد', 'کامل‌شدن شارژ یا مکثِ بلندتر از ۱ دقیقه: صفر', 'رسیدن به ۲۴ ساعتِ بی‌نتیجه → قفل (بخش ۲)'], 21, gap=33)
elbow([(585, 3024), (444, 3024)], RED_O, dash=True)

arrow(CX, 3079, CX, 3119)
diamond(CX, 3204, 430, 160, ['کامل شد؟ ۱۰ دقیقه در جذب و جریان', 'زیر ۵۰ میلی‌آمپر برای ۶۰ ثانیه', 'یا سقف امنِ ۱ ساعت (پس از کم‌شدن جریان)'])
elbow([(585, 3204), (548, 3204), (548, 3014), (581, 3014)], HEAD)
label_v(548, 3109, 'نه')
arrow(CX, 3284, CX, 3324)
label_v(CX, 3304, 'بله')
box(585, 3328, 430, 116, GREEN_F, GREEN_O)
txt(CX, 3358, ['نگه‌داری (FLOAT) — شارژ کامل', 'duty صفر و پمپ پارک؛ باتری روی ۱۳٫۵ ولت', 'سبز ثابت و زرد خاموش می‌شود'], 23, gap=33)
arrow(CX, 3444, CX, 3484)
diamond(CX, 3554, 380, 130, ['ولتاژ زیر ۱۲٫۸ افتاد؟', 'یعنی باتری دارد خالی می‌شود'])
elbow([(610, 3554), (462, 3554), (462, 2672), (581, 2672)], HEAD)
label_v(462, 3113, 'بله: شارژ از نو')
arrow(CX, 3619, CX, 3659)
label_v(CX, 3639, 'نه')
pill(CX, 3663, 340, 'باتری آمادهٔ کار')

# ================= فرمان‌های خروجی =================
section(3790, 'فرمان‌ها و خروجی‌ها — شارژر به کجا فرمان می‌دهد')
outs = [
    (40, BLUE_F, BLUE_O, ['فرمان شارژ: duty حساب‌شده (با گیرهٔ سقف)', 'به PWM هر کانال می‌رود؛ در همهٔ توقف‌ها صفر']),
    (560, NAVY, NAVY, ['رلهٔ قطع نهایی: در خطای نهایی باز می‌شود', 'با قطع ورودی، کویل رها می‌شود تا باتری خالی نشود']),
    (1080, GREEN_F, GREEN_O, ['اطلاع به بقیه: چراغ و بوق، پنل و نمایش وضعیت', 'وتوی خروجی دستگاه وقتی باتری خراب قفل است']),
]
for x, fll, oln, lines in outs:
    box(x, 3840, 480, 104, fll, oln)
    for i, ln in enumerate(lines):
        d.text((x+240, Y(3868) + i*34), fa(ln), font=F(21, i == 0),
               fill='#FFFFFF' if fll == NAVY else INK, anchor='mm')

# راهنمای رنگ‌ها + پانویس
d.text((1568, Y(4022)), fa('معنای رنگ‌ها:'), font=F(24, True), fill=INK, anchor='ra')
items = [(BLUE_F, BLUE_O, 'در حال شارژ'), (GREEN_F, GREEN_O, 'شارژ کامل'),
         (YEL_F, YEL_O, 'سؤال و تصمیم'), (RED_F, RED_O, 'خطا و توقف'),
         (GRY_F, GRY_O, 'صبر و مکث'), (NAVY, NAVY, 'شروع و پایان')]
cur = 1330
for fillc, outc, lab in items:
    t = fa(lab); w = d.textlength(t, font=F(23, True))
    d.rectangle([cur+14, Y(4012), cur+44, Y(4012)+30], fill=fillc, outline=outc, width=2)
    d.text((cur, Y(4027)), t, font=F(23, True), fill=INK, anchor='rm')
    cur -= (w + 78)
d.text((CX, Y(4090)), fa('عددها و ترتیب بررسی‌ها از کد فعلی برد (charger.c) — هر ایرادی دیدی بگو اول چک کنیم، بعد اصلاح کنم.'), font=F(22), fill='#808080', anchor='ma')

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                   'Documentation', 'charging_flowchart.png')
img.save(out)
print('saved', out)
