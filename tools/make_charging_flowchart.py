# -*- coding: utf-8 -*-
"""Generate Documentation/charging_flowchart.png (charging flowchart, Vazirmatn).

Needs: pip install arabic-reshaper python-bidi Pillow, plus the Vazirmatn
Regular/Bold TTFs. Font lookup: $VAZIR_DIR, /tmp/vazirmatn-33.003/fonts/ttf,
~/.fonts. Colors are meaningful (see the legend drawn on the image):
blue = charging, green = charge complete, yellow = question/decision,
red = fault/stop, gray = wait/pause, navy = start/end.
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

SY = 1.38  # vertical stretch: keep rows far apart so nothing overlaps
def Y(v): return int(round(v * SY))

W, H = 1600, int(1700 * SY) + 10
img = Image.new('RGB', (W, H), '#FFFFFF')
d = ImageDraw.Draw(img)

def box(x, y, w, h, fill, outline, r=18, lw=3):
    d.rounded_rectangle([x, Y(y), x+w, Y(y)+h], radius=r, fill=fill, outline=outline, width=lw)
def pill(cx, y, w, text):
    box(cx-w//2, y, w, 64, NAVY, NAVY, r=32)
    d.text((cx, Y(y)+32), fa(text), font=F(30, True), fill='#FFFFFF', anchor='mm')
def txt(cx, y, lines, sz=25, bold_first=False, color=INK, gap=36):
    for i, ln in enumerate(lines):
        d.text((cx, Y(y) + i*gap), fa(ln), font=F(sz, bold_first and i == 0), fill=color, anchor='mm')
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
def diamond(cx, cy, w, h, lines):
    cy = Y(cy)
    d.polygon([(cx, cy-h//2), (cx+w//2, cy), (cx, cy+h//2), (cx-w//2, cy)], fill=YEL_F, outline=YEL_O, width=3)
    n = len(lines)
    for i, ln in enumerate(lines):
        d.text((cx, cy + (i-(n-1)/2)*34), fa(ln), font=F(24), fill=INK, anchor='mm')
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

CX = 800
d.text((CX, Y(8)), fa('فلوچارت شارژ — هر کانال جدا (باتری ۱۲ ولت)'), font=F(34, True), fill=NAVY, anchor='ma')
pill(CX, 60, 300, 'شروع (برد روشن)')
arrow(CX, 124, CX, 154)
diamond(CX, 224, 400, 140, ['ورودی بین ۲۲ و ۲۸ ولت', 'و باتری وصل است؟'])
box(1090, 174, 440, 100, GRY_F, GRY_O)
txt(1310, 199, ['توقف و صبر (INPUT_WAIT)', 'شارژ صفر؛ هر وقت درست شد ادامه'], 24, True, gap=34)
elbow([(1000, 224), (1090, 224)], HEAD, head=True)
elbow([(1310, 174), (1310, 92), (954, 92)], HEAD)
label_h(1045, 224, 'نه')
arrow(CX, 294, CX, 344)
label_v(CX, 319, 'بله')
box(585, 348, 430, 88, BLUE_F, BLUE_O)
txt(CX, 374, ['آرام‌سازی ۱۵ ثانیه بعد از وصل باتری', 'شروع نرم، بدون پرش'], 24, True, gap=34)
arrow(CX, 436, CX, 456)
box(585, 460, 430, 104, BLUE_F, BLUE_O)
txt(CX, 488, ['شارژ سریع (BULK)', 'جریان کامل، حداکثر ۶۵۰ میلی‌آمپر'], 25, True, gap=36)
arrow(CX, 564, CX, 584)
diamond(CX, 654, 380, 140, ['ولتاژ به ۱۴٫۳ رسید؟'])
elbow([(610, 654), (548, 654), (548, 486), (581, 486)], HEAD)
label_v(548, 583, 'نه')
arrow(CX, 724, CX, 774)
label_v(CX, 749, 'بله')
box(585, 778, 430, 128, BLUE_F, BLUE_O)
txt(CX, 808, ['جذب (ABSORB)', 'ولتاژ ثابت ۱۴٫۴ ولت؛ جریان کم‌کم کم می‌شود', 'بالای ۱۴٫۶ یعنی کم‌کردن پله‌ای جریان'], 24, True, gap=34)
elbow([(585, 814), (505, 814), (505, 512), (581, 512)], RED_O)
label_v(505, 663, 'زیر ۱۴٫۳؟ برگرد به شارژ سریع', RED_O)
arrow(CX, 906, CX, 926)
diamond(CX, 1011, 420, 170, ['۱۰ دقیقه در جذب گذشت', 'و جریان زیر ۵۰ میلی‌آمپر،', '۶۰ ثانیه ثابت ماند؟'])
elbow([(590, 1011), (548, 1011), (548, 842), (581, 842)], HEAD)
label_v(548, 926, 'نه')
arrow(CX, 1096, CX, 1146)
label_v(CX, 1121, 'بله')
box(585, 1150, 430, 128, GREEN_F, GREEN_O)
txt(CX, 1178, ['نگه‌داری (FLOAT) — شارژ کامل', 'باتری روی ۱۳٫۵ ولت نگه داشته می‌شود', 'سبز ثابت، زرد خاموش'], 24, True, gap=34)
arrow(CX, 1278, CX, 1298)
diamond(CX, 1368, 380, 140, ['ولتاژ زیر ۱۲٫۸ افتاد؟', 'یعنی باتری دارد خالی می‌شود'])
elbow([(610, 1368), (462, 1368), (462, 538), (581, 538)], HEAD)
label_v(462, 928, 'بله: شارژ از نو')
arrow(CX, 1438, CX, 1488)
label_v(CX, 1463, 'نه')
pill(CX, 1492, 300, 'باتری آمادهٔ کار')

box(1090, 460, 440, 128, RED_F, RED_O)
txt(1310, 490, ['خطای ۶: سیم باتری کشیده شد', 'شارژ همان لحظه صفر می‌شود', 'با وصل دوباره و ۱۵ ثانیه آرام‌سازی برمی‌گردد'], 23, True, gap=33)
elbow([(1090, 512), (1019, 512)], RED_O, dash=True)
box(1090, 778, 440, 128, RED_F, RED_O)
txt(1310, 808, ['خطای ۷: کانال بی‌جریان (فنی)', 'قفل فنی؛ شارژ آن کانال بسته می‌شود', 'فقط با خاموش و روشن‌کردن پاک می‌شود'], 23, True, gap=33)
elbow([(1090, 842), (1019, 842)], RED_O, dash=True)
box(1090, 1150, 440, 118, GRY_F, GRY_O)
txt(1310, 1178, ['داغ‌شدن: ۱ ساعت جریان کامل', '۱۰ دقیقه استراحت، بعد ادامهٔ شارژ', 'در استراحت زرد چشمک می‌زند'], 23, True, gap=33)
elbow([(1090, 1209), (1019, 1209)], GRY_O, dash=True)
box(40, 778, 400, 118, GRY_F, GRY_O)
txt(240, 806, ['تریپ JIT (جریان ناگهانی)', 'کانال می‌ایستد؛ احیا یکی‌یکی', 'و به ترتیب تریپ انجام می‌شود'], 23, True, gap=33)
elbow([(585, 837), (444, 837)], GRY_O, dash=True)
box(40, 1150, 400, 150, RED_F, RED_O)
txt(240, 1178, ['باتری خراب (سناریوی ۶)', '۲۴ ساعت شارژ فعالِ بی‌نتیجه؟', 'قفل و قرمز ثابت؛ شارژ دیگر نمی‌کند', 'پاک‌شدن: نبودِ باتری برای ۳ ثانیه'], 23, True, gap=33)
elbow([(585, 1225), (444, 1225)], RED_O, dash=True)

d.text((1568, Y(1562)), fa('معنای رنگ‌ها:'), font=F(24, True), fill=INK, anchor='ra')
items = [(BLUE_F, BLUE_O, 'در حال شارژ'), (GREEN_F, GREEN_O, 'شارژ کامل'),
         (YEL_F, YEL_O, 'سؤال و تصمیم'), (RED_F, RED_O, 'خطا و توقف'),
         (GRY_F, GRY_O, 'صبر و مکث'), (NAVY, NAVY, 'شروع و پایان')]
cur = 1330
for fillc, outc, lab in items:
    t = fa(lab); w = d.textlength(t, font=F(23, True))
    d.rectangle([cur+14, Y(1552), cur+44, Y(1552)+30], fill=fillc, outline=outc, width=2)
    d.text((cur, Y(1567)), t, font=F(23, True), fill=INK, anchor='rm')
    cur -= (w + 78)
d.text((CX, Y(1628)), fa('عددها از کد فعلی برد (charger.h) — هر ایرادی دیدی بگو اول چک کنیم، بعد اصلاح کنم.'), font=F(22), fill='#808080', anchor='ma')

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                   'Documentation', 'charging_flowchart.png')
img.save(out)
print('saved', out)
