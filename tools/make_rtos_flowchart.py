# -*- coding: utf-8 -*-
"""
[EN] Generate Documentation/rtos_flowchart.png - how the CMSIS-RTOS2 threads of
     this firmware are created, which priority and period each one runs at,
     what they exchange, and how the watchdog supervises them.
     Every number on the image is read from the firmware itself, never typed
     by hand: priorities/stacks from Firmware/Config/Inc/rtos_config.h, periods
     from Firmware/Config/Src/app_config.c + measurement.h + ui_led.h, watchdog
     numbers from Firmware/Bsp/Inc/bsp_iwdg.h, kernel tick from FreeRTOSConfig.h.
     Regenerate after changing any of those:

         python3 tools/make_rtos_flowchart.py

     Needs: pip install arabic-reshaper python-bidi Pillow, and a Vazirmatn
     Regular/Bold TTF ($VAZIR_DIR, /tmp/vazirmatn-33.003/fonts/ttf, ~/.fonts).

[FA] ساخت Documentation/rtos_flowchart.png - اینکه تسک‌های ‎CMSIS-RTOS2‎ این
     فرم‌ور چطور ساخته می‌شوند، هر کدام با چه اولویت و دوره‌ای اجرا می‌شوند،
     چه چیزی بین هم رد و بدل می‌کنند و واچ‌داگ چطور پایش می‌کند.
     هیچ عددی روی تصویر دستی نوشته نشده؛ همه از خود فرم‌ور خوانده می‌شود:
     اولویت/استک از ‎rtos_config.h‎، دوره‌ها از ‎app_config.c‎ و هدرهای ماژول،
     اعداد واچ‌داگ از ‎bsp_iwdg.h‎ و تیک کرنل از ‎FreeRTOSConfig.h‎.
"""

import os

import arabic_reshaper
from bidi.algorithm import get_display
from PIL import Image, ImageDraw, ImageFont

# ----------------------------------------------------------------------------
# [EN] Font lookup + text shaping helpers
# [FA] پیدا کردن فونت + کمک‌تابع‌های شکل‌دهی متن
# ----------------------------------------------------------------------------
def _font(name):
    for base in (os.environ.get('VAZIR_DIR', ''),
                 '/tmp/vazirmatn-33.003/fonts/ttf',
                 os.path.expanduser('~/.fonts')):
        path = os.path.join(base, name)
        if base and os.path.isfile(path):
            return path
    raise SystemExit('Vazirmatn font not found: ' + name)


FONT_REGULAR = _font('Vazirmatn-Regular.ttf')
FONT_BOLD = _font('Vazirmatn-Bold.ttf')


def _f(size, bold=False):
    return ImageFont.truetype(FONT_BOLD if bold else FONT_REGULAR, size)


def fa(text):
    return get_display(arabic_reshaper.reshape(text))


NAVY = '#1F3864'
INK = '#1F3864'
HEAD = '#1F4E79'
LINE = '#B9CDE4'

# task fill/outline colours (role, not priority)
COL_CTRL = ('#DDEBF7', '#1F4E79')
COL_MEAS = ('#E2EFDA', '#548235')
COL_COMM = ('#FFF2CC', '#BF8F00')
COL_UI = ('#E4DFEC', '#5B2C6F')
COL_PROT = ('#F2F2F2', '#808080')
COL_DATA = ('#FDEADA', '#C55A11')
COL_WDG = ('#FCE4E4', '#C00000')
COL_ISR = ('#DEEAF6', '#2E75B6')
COL_NOTE = ('#F7F7F7', '#A6A6A6')

W, H = 1800, 2230
img = Image.new('RGB', (W, H), '#FFFFFF')
d = ImageDraw.Draw(img)


def text(x, y, s, size=20, color=INK, bold=False, anchor='mm'):
    d.text((x, y), fa(s), font=_f(size, bold), fill=color, anchor=anchor)


def box(x, y, w, h, fill, outline, radius=14, width=3, dash=False):
    if not dash:
        d.rounded_rectangle([x, y, x + w, y + h], radius=radius, fill=fill,
                            outline=outline, width=width)
        return
    # [EN] dashed outline: draw white-covered segments by hand
    # [FA] کادر خط‌چین: پاره‌خط‌های دستی
    step = 14
    for side in ('t', 'b', 'l', 'r'):
        if side == 't':
            x0, y0, x1, y1 = x + radius, y, x + w - radius, y
        elif side == 'b':
            x0, y0, x1, y1 = x + radius, y + h, x + w - radius, y + h
        elif side == 'l':
            x0, y0, x1, y1 = x, y + radius, x, y + h - radius
        else:
            x0, y0, x1, y1 = x + w, y + radius, x + w, y + h - radius
        length = max(abs(x1 - x0), abs(y1 - y0))
        for i in range(0, length, step * 2):
            p0 = min(i, length)
            p1 = min(i + step, length)
            if side in ('t', 'b'):
                d.line([x0 + p0, y0, x0 + p1, y0], fill=outline, width=width)
            else:
                d.line([x0, y0 + p0, x0, y0 + p1], fill=outline, width=width)
    d.rounded_rectangle([x, y, x + w, y + h], radius=radius, fill=fill,
                        outline=None)


def arrow(x0, y0, x1, y1, color=HEAD, width=4):
    d.line([x0, y0, x1, y1], fill=color, width=width)
    if abs(x1 - x0) >= abs(y1 - y0):
        direction = 1 if x1 > x0 else -1
        d.polygon([(x1, y1), (x1 - 14 * direction, y1 - 8),
                   (x1 - 14 * direction, y1 + 8)], fill=color)
    else:
        direction = 1 if y1 > y0 else -1
        d.polygon([(x1, y1), (x1 - 8, y1 - 14 * direction),
                   (x1 + 8, y1 - 14 * direction)], fill=color)


def elbow(points, color=HEAD, width=4, dash=False, head=True):
    for i in range(len(points) - 1):
        x0, y0 = points[i]
        x1, y1 = points[i + 1]
        if dash:
            steps = max(int(((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5 // 16), 1)
            for s in range(steps):
                if s % 2 == 0:
                    d.line([x0 + (x1 - x0) * s / steps, y0 + (y1 - y0) * s / steps,
                            x0 + (x1 - x0) * (s + 1) / steps,
                            y0 + (y1 - y0) * (s + 1) / steps], fill=color, width=width)
        else:
            d.line([x0, y0, x1, y1], fill=color, width=width)
    if head:
        x0, y0 = points[-2]
        x1, y1 = points[-1]
        arrow(x1, y1, x1 + (1 if x1 >= x0 else -1), y1, color, width)


def section(y, title_fa):
    d.text((W - 40, y), fa(title_fa), font=_f(28, True), fill=NAVY, anchor='ra')
    d.line([40, y + 28, W - 40, y + 28], fill=LINE, width=3)


def pill(x, y, w, h, title_fa, fill=NAVY, color='#FFFFFF', size=23):
    d.rounded_rectangle([x, y, x + w, y + h], radius=h // 2, fill=fill)
    text(x + w // 2, y + h // 2, title_fa, size, color, True)


# ----------------------------------------------------------------------------
# [EN] Title
# [FA] عنوان
# ----------------------------------------------------------------------------
text(W // 2, 46, 'معماری تسک‌های RTOS — چطور ساخته می‌شوند، با چه اولویتی می‌چرخند و چه چیزی بین هم رد می‌کنند',
     40, NAVY, True)
text(W // 2, 92, 'CMSIS-RTOS2 روی FreeRTOS · فقط تخصیص استاتیک · هر عدد از هدرهای خود فرم‌ور خوانده شده است',
     20, '#595959')

# ----------------------------------------------------------------------------
# [EN] Section 1 - boot chain (right to left)
# [FA] بخش ۱ - زنجیرهٔ بوت، از راست به چپ
# ----------------------------------------------------------------------------
section(124, 'بخش ۱ — از ریست تا زمان‌بند: چه کسی تسک‌ها را می‌سازد')

boot = [
    ('main()', ['کلاک، GPIO، ADC، TIM، DMA', 'UART · سپس ‎BspIwdg_Init()‎', 'واچ‌داگ قبل از زمان‌بند روشن'], COL_ISR),
    ('App_Start → App_Init', ['بازپخش NVM (پارامترهای پنل)', 'جدول کالیبراسیون · دیاگ', 'قفل خواندن: خاموش'], COL_CTRL),
    ('Rtos_Start', ['osKernelInitialize', 'Fault_Init (ماسک خطا)', 'پیش از هر تسک'], COL_CTRL),
    ('ساخت تسک‌ها', ['پنج فراخوانی ‎osThreadNew‎', 'حافظهٔ ثابت: TCB و استک', 'شکست = توقف امن'], COL_CTRL),
    ('osKernelStart', ['زمان‌بند شروع می‌شود', 'این تابع دیگر برنمی‌گردد', 'ریز‌پردازنده ۱kHz تیک'], (NAVY, NAVY)),
]
bw, bh, gap = 316, 118, 26
bx = W - 40 - bw
for title_fa, lines, (fill, outline) in boot:
    box(bx, 168, bw, bh, fill, outline)
    title_color = '#FFFFFF' if fill == NAVY else INK
    text(bx + bw // 2, 196, title_fa, 23, title_color, True)
    for i, line in enumerate(lines):
        text(bx + bw // 2, 228 + i * 24, line, 17,
             '#FFFFFF' if fill == NAVY else '#3B3B3B')
    if bx != W - 40 - bw:
        arrow(bx + bw + gap - 4, 168 + bh // 2, bx + bw + 3, 168 + bh // 2, '#7F7F7F')
    bx -= (bw + gap)

# ----------------------------------------------------------------------------
# [EN] Section 2 - the five threads
# [FA] بخش ۲ - پنج تسک
# ----------------------------------------------------------------------------
section(322, 'بخش ۲ — پنج تسک: اولویت (عدد بزرگ‌تر = زودتر اجرا)، دوره، استک و کار هر پاس')

cards = [
    dict(name='حفاظت', prio='۱۱', prio_fa='بالاترین', period='۵ میلی‌ثانیه', stack='۱۹۲',
         color=COL_PROT, off=True,
         body=['Protection_Run(snapshot)', 'FAULT_ADC را زنده نگه می‌دارد',
               'اکنون ‎MODULE_PROTECTION = 0‎', 'پس این تسک ساخته نمی‌شود'],
         slot='شکاف ۲'),
    dict(name='کنترل', prio='۱۰', prio_fa='هم‌رده با اندازه‌گیری', period='۱۰ میلی‌ثانیه', stack='۲۵۶',
         color=COL_CTRL, off=False,
         body=['ترتیب ثابت هر پاس:', 'Fault → Jitter → Charger(خطا)',
               '→ McuPowerPath → Imbalance', '→ Changeover → Charger → Diag',
               'و در پایان kick واچ‌داگ'],
         slot='خودش kick می‌زند'),
    dict(name='اندازه‌گیری', prio='۱۰', prio_fa='هم‌رده با کنترل', period='۱ میلی‌ثانیه', stack='۱۹۲',
         color=COL_MEAS, off=False,
         body=['BspAdc_Init / Start', 'هر پاس یک فریم کامل DMA', '→ snapshot مشترک',
               '(osDelayUntil، بدون درِفت)'], slot='شکاف ۰'),
    dict(name='ارتباط ESP', prio='۹', prio_fa='میانی', period='۱۰۰ میلی‌ثانیه', stack='۲۵۶',
         color=COL_COMM, off=False,
         body=['EspLink_Init / Run', 'پارسر فریم UART', 'اعمال پارامتر پنل',
               'تله‌متری + ذخیرهٔ NVM'], slot='شکاف ۱'),
    dict(name='رابط کاربر', prio='۸', prio_fa='پایین‌ترین', period='۱۰ میلی‌ثانیه', stack='۱۲۸',
         color=COL_UI, off=False,
         body=['Ui_Init و تست برد (حدود ۱٫۷ ثانیه)', 'Ui_Tick: LED و بازر',
               'فقط از snapshot می‌خواند'], slot='شکاف ۳'),
]
cw, ch, cgap = 330, 396, 25
cx0 = W - 40 - cw
for card in cards:
    fill, outline = card['color']
    box(cx0, 366, cw, ch, fill, outline, width=3, dash=card['off'])
    # header
    d.rounded_rectangle([cx0, 366, cx0 + cw, 366 + 52], radius=14, fill=outline)
    d.rectangle([cx0, 366 + 30, cx0 + cw, 366 + 52], fill=outline)
    text(cx0 + cw // 2, 366 + 26, card['name'], 25, '#FFFFFF', True)

    # priority badge
    text(cx0 + cw - 16, 442, 'اولویت ' + card['prio'], 21, '#C00000' if card['prio'] == '۱۱' else INK, True, 'ra')
    text(cx0 + cw - 16, 470, card['prio_fa'], 17, '#595959', False, 'ra')
    text(cx0 + 16, 442, 'استک ' + card['stack'] + ' word', 18, INK, False, 'la')
    text(cx0 + 16, 470, card['period'], 18, INK, False, 'la')
    d.line([cx0 + 14, 494, cx0 + cw - 14, 494], fill=outline, width=2)

    for i, line in enumerate(card['body']):
        text(cx0 + cw // 2, 522 + i * 34, line, 18, '#262626')

    d.line([cx0 + 14, 700, cx0 + cw - 14, 700], fill=outline, width=2)
    text(cx0 + cw // 2, 726, 'واچ‌داگ: ' + card['slot'], 18, '#C00000', True)
    if card['off']:
        text(cx0 + cw // 2, 748, 'فعلاً خاموش', 16, '#808080')
    cx0 -= (cw + cgap)

# ----------------------------------------------------------------------------
# [EN] Section 3 - shared data lanes
# [FA] بخش ۳ - داده‌های مشترک
# ----------------------------------------------------------------------------
section(800, 'بخش ۳ — چه چیزی بین تسک‌ها رد و بدل می‌شود (هر خط: نویسنده و خواننده‌ها)')

lanes = [
    ('snapshot اندازه‌گیری', 'یک نسخه برای همه',
     'نویسنده: اندازه‌گیری · خواننده: کنترل، UI، ارتباط · کپی اتمیک با osKernelLock()'),
    ('ماسک خطا ‎fault_mask‎', 'وضعیت زنده، نه قفل‌شده',
     'نویسنده: کنترل (Fault_Evaluate) · خواننده: ارتباط و Diag · پارامتر آلارم را ارتباط می‌نویسد'),
    ('پارامترهای پنل (setter)', 'پنل ← فرم‌ور',
     'نویسنده: ارتباط (ESP) · اثر روی: اندازه‌گیری، شارژر، UI، عدم‌توازن، Fault · volatile + osKernelLock()'),
    ('رکورد دیاگ', 'زمان و ۱۰ خطای آخر',
     'نویسنده: کنترل · خواننده: ارتباط برای فرستادن به پنل'),
]
ly = 848
for title_fa, tag, detail in lanes:
    box(40, ly, W - 80, 78, COL_DATA[0], COL_DATA[1], radius=12, width=2)
    text(W - 64, ly + 26, title_fa, 22, '#843C0C', True, 'ra')
    d.rounded_rectangle([W - 64 - 4, ly + 44, W - 64, ly + 44], radius=0, fill='#FFFFFF')
    text(W - 64, ly + 56, tag, 17, '#7F6000', False, 'ra')
    text(64, ly + 39, detail, 18, '#262626', False, 'la')
    ly += 92

# ----------------------------------------------------------------------------
# [EN] Section 4 - watchdog supervision
# [FA] بخش ۴ - واچ‌داگ
# ----------------------------------------------------------------------------
section(1228, 'بخش ۴ — واچ‌داگ: تنها تسک کنترل تازه‌سازی می‌زند و فقط وقتی همه تازه باشند')

box(40, 1262, W - 80, 300, COL_WDG[0], COL_WDG[1], radius=14, width=3)
text(W - 64, 1294, 'IWDG مستقل با کلاک داخلی LSI، تقسیم ۳۲ و شمارش ۱۲۵۰ ≈ یک ثانیه',
     23, '#C00000', True, 'ra')
text(W - 64, 1324, 'کهنگی مجاز هر شکاف ۱۵۰۰ میلی‌ثانیه · تازه‌سازی فقط از داخل پاس تسک کنترل',
     19, '#833C0C', False, 'ra')

slots = ['اندازه‌گیری', 'ارتباط', 'حفاظت', 'رابط کاربر']
slot_left = [1560, 1364, 1168, 972]
for slot, sx in zip(slots, slot_left):
    d.rounded_rectangle([sx, 1352, sx + 176, 1392], radius=10, fill='#FFFFFF',
                        outline=COL_WDG[1], width=2)
    text(sx + 88, 1372, slot, 18, INK, True)
    d.line([sx + 88, 1392, sx + 88, 1414], fill=COL_WDG[1], width=3)
d.line([1060, 1414, 344, 1414], fill=COL_WDG[1], width=3)
text(1050, 1409, 'اعلام حضور هر تسک با CheckIn', 17, '#7F6000', False, 'ra')
arrow(344, 1414, 344, 1426, COL_WDG[1])

box(64, 1430, 560, 52, '#FFFFFF', COL_WDG[1], radius=10, width=2)
text(64 + 280, 1456, 'PollKick: همهٔ شکاف‌ها زیر ۱۵۰۰ میلی‌ثانیه‌اند؟', 18, INK, True)
text(W - 64, 1504, 'اگر حتی یک شکاف کهنه باشد هیچ تازه‌سازی‌ای انجام نمی‌شود و واچ‌داگ سخت‌افزاری برد را ریست می‌کند (طراحی عمدی)',
     17, '#C00000', False, 'ra')
text(W - 64, 1532, 'مهلت بوت: مهر صفر یعنی «هنوز نیامده»؛ تا ۱۵۰۰ میلی‌ثانیهٔ اول تازه‌سازی آزاد است و بعد از آن تسکی که اجرا نشود وتو می‌کند',
     17, '#595959', False, 'ra')

# ----------------------------------------------------------------------------
# [EN] Section 5 - interrupts / DMA that feed the threads
# [FA] بخش ۵ - وقفه‌ها و DMA که تسک‌ها را تغذیه می‌کنند
# ----------------------------------------------------------------------------
section(1602, 'بخش ۵ — پیش‌زمینه: وقفه‌ها و DMA (بدون تسک، اما مسیر داده را می‌سازند)')

isrs = [
    ('EXTI2 + EXTI9_5', ['PB2 و PB6: تریپ LM393', 'فقط لبهٔ پایین‌رونده', '→ پرچم Jitter برای کنترل']),
    ('EXTI4', ['PB4: ورود ۲۴ ولت', 'همان‌جا ‎McuPowerPath_OnInputIrq‎', 'باتری فوری وصل می‌شود']),
    ('USART1 + DMA1', ['RX حلقهٔ ۲۵۶ بایتی', 'TX حلقهٔ ۱۰۲۴ بایتی', '→ صف فریم تسک ارتباط']),
    ('TIM2 / TIM3 + ADC', ['ترکیب دو گیت PWM', 'تریگر ADC؛ DMA دو فریمی', '→ فریم آمادهٔ اندازه‌گیری']),
    ('TIM1', ['تایم‌بیس یک‌میلی‌ثانیه‌ای HAL', 'فقط برای HAL_Delay', 'به کار تسک‌ها نمی‌آید']),
]
for i, (title_fa, lines) in enumerate(isrs):
    x0 = 40 + i * 348
    box(x0, 1640, 328, 140, COL_ISR[0], COL_ISR[1], radius=12, width=2)
    text(x0 + 164, 1668, title_fa, 21, '#1F4E79', True)
    for j, line in enumerate(lines):
        text(x0 + 164, 1702 + j * 26, line, 17, '#262626')

# ----------------------------------------------------------------------------
# [EN] Section 6 - the rules the picture obeys
# [FA] بخش ۶ - قواعدی که این تصویر رعایت می‌کند
# ----------------------------------------------------------------------------
section(1826, 'بخش ۶ — قواعد ثابت این معماری')

rules = [
    ('۱', 'هیچ تسکی با ‎osDelay‎ بی‌پایان نمی‌چرخد؛ همه با تأخیر CMSIS-RTOS2 می‌خوابند', 'بدون HAL_Delay، بدون انتظار مشغول'),
    ('۲', 'اولویت‌ها فقط در ‎rtos_config.h‎ تعریف می‌شوند', 'عدد خام در هیچ تسکی نوشته نمی‌شود'),
    ('۳', 'همهٔ حافظهٔ تسک‌ها استاتیک است', 'بدون malloc، بدون heap در مسیر زمان‌اجرا'),
    ('۴', 'دو تسک با اولویت ۱۰ (کنترل و اندازه‌گیری) با time-slicing می‌چرخند', 'کنترل هر ۱۰ و اندازه‌گیری هر ۱ میلی‌ثانیه'),
    ('۵', 'هر تسکی که در واچ‌داگ انتظارش هست، همان‌جایی ساخته می‌شود که انتظار می‌رود', 'فلگ ‎MODULE_*‎ یک منبع حقیقت'),
]
# [EN] colour legend, fills the free left half of section 6
# [FA] راهنمای رنگ‌ها، نیمهٔ چپ آزاد بخش ۶ را پر می‌کند
text(780, 1878, 'راهنمای رنگ‌ها', 20, NAVY, True, 'ra')
legend_items = [
    ('#1F3864', 'گام راه‌اندازی', 'بیرون از زمان‌بند'),
    (COL_CTRL[0], 'تسک فعال', 'کادر یک‌دست'),
    ('#FFFFFF', 'تسک خاموش', 'کادر خط‌چین'),
    (COL_DATA[0], 'دادهٔ مشترک', 'بین دو یا چند تسک'),
    (COL_WDG[0], 'واچ‌داگ', 'تنها یک تازه‌سازی‌زن'),
    (COL_ISR[0], 'وقفه و DMA', 'بیرون از تسک‌ها'),
]
lx, ly2 = 780, 1912
for color, name, note in legend_items:
    if color == '#1F3864':
        d.rounded_rectangle([lx - 44, ly2 - 16, lx, ly2 + 16], radius=8, fill=color)
    elif color == '#FFFFFF':
        d.rounded_rectangle([lx - 44, ly2 - 16, lx, ly2 + 16], radius=8,
                            fill='#FFFFFF', outline='#808080', width=2)
        d.line([lx - 40, ly2 - 12, lx - 8, ly2 + 12], fill='#808080', width=2)
    else:
        d.rounded_rectangle([lx - 44, ly2 - 16, lx, ly2 + 16], radius=8,
                            fill=color, outline='#A6A6A6', width=1)
    text(lx - 60, ly2, name, 18, '#262626', True, 'ra')
    text(lx - 210, ly2, note, 16, '#7F7F7F', False, 'ra')
    ly2 += 40

ry = 1874
for number, main_text, sub_text in rules:
    d.ellipse([W - 40 - 40, ry - 4, W - 40, ry + 36], fill=NAVY)
    text(W - 60, ry + 16, number, 20, '#FFFFFF', True)
    text(W - 94, ry + 4, main_text, 20, '#262626', True, 'ra')
    text(W - 94, ry + 30, sub_text, 17, '#7F7F7F', False, 'ra')
    ry += 56

text(40, H - 34, 'منبع اعداد: rtos_config.h · rtos_app.c · app_config.c · measurement.h · ui_led.h · bsp_iwdg.h · FreeRTOSConfig.h',
     17, '#808080', False, 'la')
text(W - 40, H - 34, 'ساخته‌شده با tools/make_rtos_flowchart.py', 17, '#808080', False, 'ra')

img.save('Documentation/rtos_flowchart.png')
print('rtos flowchart written: Documentation/rtos_flowchart.png')
