/* ==========================================================================
   user_panel/web/app.js - [EN] User dashboard application (no dependencies).
       Served by the ESP from PROGMEM as /app.js. The file is deliberately
       self-contained: an access point with no internet must still render the
       whole panel, so there is no framework, no CDN and no build step.
       Sections are separated with the project's marker style
       ( /* ==================== Name ==================== * / ) so scrolling and
       reviewing stay fast, exactly as AI_AGENT_RULES.md asks for C sources.
   [FA] اپلیکیشن پنل کاربر (بدون هیچ وابستگی). توسط ESP از PROGMEM سرو می‌شود؛
       چون پنل روی یک اکسس‌پوینت بدون اینترنت بالا می‌آید، نه فریم‌ورکی، نه CDN
       و نه مرحلهٔ بیلد وجود دارد. بخش‌ها با همان سبک جداکنندهٔ قوانین پروژه
       جدا شده‌اند تا مرور و اسکرول سریع بماند.
   ========================================================================== */
'use strict';

/* ==================== API endpoints / مسیرهای سرویس ==================== */
const API = {
  me:       '/api/me',
  login:    '/api/login',
  logout:   '/api/logout',
  pass:     '/api/pass',
  clock:    '/api/clock',
  live:     '/api/live',
  series:   '/api/series',
  events:   '/api/events',
  stats:    '/api/stats',
  adminClock: '/api/admin/clock',
  users:    '/api/admin/users',
  userOp:   '/api/admin/user',
  action:   '/api/admin/action',
  audit:    '/api/admin/audit',
  report:   '/api/admin/report.xlsx',
  export:   '/api/export'
};

/* [EN] The Excel report's range, in days: 0 means "everything". It lives
   outside the DOM on purpose - the stats view is rebuilt every few seconds and
   a refresh must not throw the admin's choice away.
   [FA] بازهٔ گزارش اکسل، به روز: صفر یعنی «همه». عمداً بیرون DOM نگه داشته
   می‌شود - بخش آمار هر چند ثانیه از نو ساخته می‌شود و تازه‌سازی نباید انتخاب
   مدیر را دور بریزد. */
let REPORT_DAYS = 0;

/* ==================== Display constants / ثابت‌های نمایش ==================== */
const REFRESH = {
  liveMs: 2000,      /* [EN] dashboard live poll / [FA] پایش زندهٔ داشبورد */
  slowMs: 15000,      /* [EN] stats+events refresh / [FA] تازه‌سازی آمار و رویدادها */
  ringMax: 160,       /* [EN] client-side live ring (≈5 min at 2 s) / [FA] حلقهٔ محلی زنده */
  linkStaleMs: 5000   /* [EN] older than this = link down / [FA] قدیمی‌تر از این = قطع */
};

/* ==================== Telemetry field map / نقشهٔ فیلدهای تلمتری ====================
   [EN] ONE table for the whole file. ESP_AGENT_SPEC.md section 6 defines the
   order of the u32 telemetry words; an index typed by hand in five places is
   exactly the class of defect this project's audit script exists to catch, so
   the numbers live here once and are named after their meaning.
   [FA] یک جدول برای کل فایل. بخش ۶ سند، ترتیب کلمات تلمتری را تعریف می‌کند؛
   عددی که دستی در پنج جا تکرار شود دقیقاً همان ایرادی است که ممیز پروژه برای
   گرفتنش نوشته شده. پس اعداد فقط اینجا و با نام معنادار می‌آیند. */
const TI = {
  raw1: 0, shunt1: 1, ma1Unf: 2, i1Filt: 3, iest1: 4, duty1: 5, st1: 6,
  raw2: 7, shunt2: 8, ma2Unf: 9, i2Filt: 10, iest2: 11, duty2: 12, st2: 13,
  vin: 14, v24: 15, v12: 16, vlow: 17, vhigh: 18, faults: 19,
  vinRaw: 20, v24Raw: 21, v12Raw: 22, vref: 23, vdda: 24,
  imbMv: 25, imbEv: 26, imbCyc: 27
};

/* [EN] Charger states, translated from firmware state1/state2 (spec §6).
   [FA] حالت‌های شارژر، برگردان‌شده از state1/state2 فرم‌ور. */
const CHG_STATE = {
  0: { t: 'خاموش',        s: 'mu' },
  1: { t: 'شارژ سریع',    s: 'ac' },
  2: { t: 'تثبیت ولتاژ',  s: 'ac' },
  3: { t: 'شارژ کامل',    s: 'ok' },
  4: { t: 'آماده‌سازی',   s: 'mu' },
  5: { t: 'تلاش مجدد',    s: 'wa' },
  6: { t: 'انتظار ورودی', s: 'mu' },
  7: { t: 'خطای نهایی',   s: 'er' },
  8: { t: 'قطع باتری',    s: 'er' },
  9: { t: 'دستی (تعمیر)', s: 'vi' }
};

/* [EN] Event codes written by the ESP history ring.
   [FA] کدهای رویداد که حلقهٔ تاریخچهٔ ESP می‌نویسد. */
const EV = {
  1:  { t: 'شروع شارژ',            i: 'ac', s: 'ok' },
  2:  { t: 'شارژ کامل شد',         i: 'ok', s: 'ok' },
  3:  { t: 'شارژ ناتمام ماند',     i: 'wa', s: 'wa' },
  4:  { t: 'قطع ورودی ۲۴ ولت',     i: 'er', s: 'wa' },
  5:  { t: 'بازگشت ورودی ۲۴ ولت',  i: 'ok', s: 'ok' },
  6:  { t: 'شروع کار روی باتری',   i: 'wa', s: 'wa' },
  7:  { t: 'پایان کار روی باتری',  i: 'ok', s: 'ok' },
  8:  { t: 'خطا فعال شد',          i: 'er', s: 'er' },
  9:  { t: 'خطا پاک شد',           i: 'ok', s: 'ok' },
  10: { t: 'برد ریست شد',          i: 'mu', s: 'mu' },
  11: { t: 'راه‌اندازی پنل',       i: 'ac', s: 'mu' },
  12: { t: 'قطع شارژ از پنل',      i: 'wa', s: 'wa' },
  13: { t: 'وصل شارژ از پنل',      i: 'ok', s: 'ok' },
  14: { t: 'رویداد عدم‌توازن',     i: 'vi', s: 'wa' },
  15: { t: 'پاک‌کردن تاریخچه',     i: 'mu', s: 'mu' },
  16: { t: 'مود دستی فعال شد',     i: 'vi', s: 'wa' }
};

/* ==================== Fault catalog / دیکشنری خطاها ====================
   [EN] Written for the OPERATOR, not for the engineer: one plain sentence,
   what it means for the machine, the likely causes and the numbered steps.
   `reset` = the board itself must be restarted before the machine works
   again (a latched fault), so the panel must say so instead of offering a
   button that cannot exist on this link.
   [FA] برای اپراتور نوشته شده نه مهندس: یک جملهٔ ساده، معنایش برای دستگاه،
   علت‌های محتمل و گام‌های شماره‌دار. `reset` یعنی خودِ برد باید ری‌استارت
   شود (خطای قفل‌شده)، پس پنل باید صریح بگوید، نه دکمه‌ای نشان دهد که روی
   این لینک وجود ندارد.
   ========================================================================= */
const CATALOG = {
  E01: { sev: 'er', title: 'خرابی اندازه‌گیری برد', reset: true,
    say: 'برد دیگر مطمئن نیست ولتاژ و جریان را درست می‌خواند. همهٔ حفاظت‌ها روی همین خواندن‌ها تصمیم می‌گیرند، پس شارژ تا رفع اشکال متوقف می‌ماند.',
    causes: ['قطع یا شل‌شدن سیم سنسور', 'نویز شدید روی مسیر ADC', 'خرابی مدار اندازه‌گیری روی برد'],
    steps: ['برد را با کلید اصلی خاموش و روشن کنید', 'کابل سنسورهای جریان و ولتاژ را چک کنید', 'اگر برنگشت، برد را برای بررسی سخت‌افزاری بیاورید'],
    src: 'fault_mask بیت ۰' },
  E02: { sev: 'er', title: 'اضافه‌جریان کانال بالایی', reset: false,
    say: 'جریان شارژر بالایی از حد سخت گذشته و برد خودش آن کانال را قطع کرده است.',
    causes: ['اتصال کوتاه یا باتری خراب', 'تنظیم اشتباه سقف جریان', 'اتصال بد ترمینال باتری'],
    steps: ['باتری و سیم‌های بالایی را چک کنید', 'دوباره شارژ را از بخش مدیریت وصل کنید', 'اگر سه بار تکرار شد، برد منتظر ری‌استارت می‌شود'],
    src: 'fault_mask بیت ۱' },
  E03: { sev: 'er', title: 'اضافه‌جریان کانال پایینی', reset: false,
    say: 'جریان شارژر پایینی از حد سخت گذشته و برد خودش آن کانال را قطع کرده است.',
    causes: ['اتصال کوتاه یا باتری خراب', 'تنظیم اشتباه سقف جریان', 'اتصال بد ترمینال باتری'],
    steps: ['باتری و سیم‌های پایینی را چک کنید', 'دوباره شارژ را از بخش مدیریت وصل کنید', 'اگر سه بار تکرار شد، برد منتظر ری‌استارت می‌شود'],
    src: 'fault_mask بیت ۲' },
  E04: { sev: 'wa', title: 'باتری ضعیف', reset: false,
    say: 'ولتاژ باتری پایین آمده است. دستگاه روی باتری کار می‌کند و انرژی کمی باقی مانده.',
    causes: ['قطع طولانی ورودی', 'باتری فرسوده', 'مصرف بیشتر از ظرفیت باتری'],
    steps: ['ورودی ۲۴ ولت را وصل کنید', 'مصرف را کم کنید', 'باتری را در فرصت مناسب بازرسی کنید'],
    src: 'fault_mask بیت ۳' },
  E05: { sev: 'wa', title: 'حفاظت افزونگی ترانس کانال بالا', reset: false,
    say: 'مدار حفاظت ترانسفورماتور بالایی یک‌بار عمل کرده و برد آن کانال را تا تلاش مجدد متوقف کرده است.',
    causes: ['اشباع ترانسفورماتور', 'بار سنگین لحظه‌ای', 'ولتاژ ورودی unstable'],
    steps: ['صبر کنید تا برد خودش دوباره تلاش کند', 'اتصال باتری را بررسی کنید', 'اگر تکرار شد سقف جریان را کم کنید'],
    src: 'fault_mask بیت ۴' },
  E06: { sev: 'wa', title: 'حفاظت افزونگی ترانس کانال پایین', reset: false,
    say: 'مدار حفاظت ترانسفورماتور پایینی عمل کرده و برد آن کانال را تا تلاش مجدد متوقف کرده است.',
    causes: ['اشباع ترانسفورماتور', 'بار سنگین لحظه‌ای', 'ولتاژ ورودی ناپایدار'],
    steps: ['صبر کنید تا برد خودش دوباره تلاش کند', 'اتصال باتری را بررسی کنید', 'اگر تکرار شد سقف جریان را کم کنید'],
    src: 'fault_mask بیت ۵' },
  E07: { sev: 'er', title: 'قطع سیم باتری', reset: false,
    say: 'برد تشخیص داده که سیم یکی از باتری‌ها وصل نیست (یا باتری برداشته شده) در حالی که شارژر فعال بوده است.',
    causes: ['سیم باتری شل یا قطع', 'باتری برداشته شده', 'ترمینال اکسیدشده'],
    steps: ['سیم و ترمینال باتری‌ها را محکم کنید', 'پس از برگشت باتری، آلارم خودش پاک می‌شود', 'اگر پاک نشد یک‌بار برد را ری‌استارت کنید'],
    src: 'fault_mask بیت ۶' },
  E08: { sev: 'wa', title: 'عدم‌توازن دو باتری', reset: false,
    say: 'اختلاف ولتاژ دو باتری ۱۲ ولتی از حد مجاز گذشته و برد این وضعیت را ثبت کرده است.',
    causes: ['باتری ضعیف‌تر در یک نیمه', 'اتصال سست در یک نیمه', 'عمر متفاوت دو باتری'],
    steps: ['ولتاژ دو نیمه را در همین صفحه ببینید', 'ترمینال‌ها را چک کنید', 'در صورت تکرار، باتری‌ها را جداگانه تست کنید'],
    src: 'fl2 (سناریو ۶)' },
  E09: { sev: 'er', title: 'کانال بالایی نیازمند ری‌استارت برد', reset: true,
    say: 'کانال بالایی پس از سه حفاظت پیاپی قفل شده است. تا خودِ برد خاموش و روشن نشود، این کانال شارژ نمی‌کند.',
    causes: ['اتصال کوتاه مکرر', 'باتری معیوب', 'مشکل سخت‌افزاری کانال'],
    steps: ['باتری و کابل بالایی را بررسی کنید', 'برد را خاموش و روشن کنید', 'اگر بلافاصله دوباره قفل شد، شارژ نکنید و خبر بدهید'],
    src: 'state1 = ۷' },
  E10: { sev: 'er', title: 'کانال پایینی نیازمند ری‌استارت برد', reset: true,
    say: 'کانال پایینی پس از سه حفاظت پیاپی قفل شده است. تا خودِ برد خاموش و روشن نشود، این کانال شارژ نمی‌کند.',
    causes: ['اتصال کوتاه مکرر', 'باتری معیوب', 'مشکل سخت‌افزاری کانال'],
    steps: ['باتری و کابل پایینی را بررسی کنید', 'برد را خاموش و روشن کنید', 'اگر بلافاصله دوباره قفل شد، شارژ نکنید و خبر بدهید'],
    src: 'state2 = ۷' },
  E11: { sev: 'er', title: 'ارتباط با برد قطع است', reset: false,
    say: 'پنل هیچ داده‌ای از برد دریافت نمی‌کند. اعداد نمایش‌داده‌شده مربوط به آخرین لحظهٔ اتصال است و ممکن است دیگر درست نباشند.',
    causes: ['برد خاموش است', 'سیم UART شل شده', 'فاصله/نویز زیاد', 'برد در حال ری‌استارت'],
    steps: ['چراغ برد و فیوزها را چک کنید', 'سیم‌های ارتباطی را محکم کنید', 'اگر لازم شد برد را خاموش و روشن کنید'],
    src: 'age فریم تله‌متری' },
  E12: { sev: 'er', title: 'نسخهٔ نرم‌افزاری دو برد ناهماهنگ', reset: false,
    say: 'برد و پنل با نسخه‌های متفاوتی فلش شده‌اند و پیام‌ها همدیگر را نمی‌فهمند؛ نتیجه‌اش صفحهٔ خالی است.',
    causes: ['فقط یکی از دو برد به‌روزرسانی شده', 'فلش نیمه‌کاره'],
    steps: ['هر دو برد را با یک نسخه دوباره فلش کنید', 'پس از فلش، هر دو را ری‌استارت کنید'],
    src: 'vm در تلمتری' },
  E13: { sev: 'wa', title: 'نویز روی خط ارتباطی', reset: false,
    say: 'شماری از پیام‌ها به‌خاطر نویز کنار گذاشته شدند. پنل کار می‌کند ولی ممکن است بعضی اعداد دیر برسند.',
    causes: ['سیم ارتباطی بلند یا بدون شیلد', 'عبور کابل کنار ترانس', 'زمین مشترک ناکافی'],
    steps: ['سیم UART را کوتاه و دور از ترانس کنید', 'زمین مشترک را بررسی کنید', 'کابل شیلددار استفاده کنید'],
    src: 'ce در تلمتری' },
  E14: { sev: 'in', title: 'کار روی باتری', reset: false,
    say: 'ورودی ۲۴ ولت قطع است و دستگاه از باتری تغذیه می‌کند. این وضعیت عادی است، فقط بدانید باتری در حال خالی‌شدن است.',
    causes: ['قطع برق شهری', 'کلید ورودی خاموش', 'ورودی زیر حد مجاز'],
    steps: ['اگر قطع برق نیست، کلید و فیوز ورودی را چک کنید'],
    src: 'fl بیت ۱' },
  E15: { sev: 'wa', title: 'ولتاژ ورودی زیاد', reset: false,
    say: 'ولتاژ ورودی از حد مجاز بالاتر رفته است. برد برای محافظت، شارژ را متوقف می‌کند.',
    causes: ['منبع ورودی تنظیم‌نشده', 'اضافه‌ولتاژ شبکه', 'اتصال اشتباه منبع'],
    steps: ['منبع را روی ۲۴ ولت تنظیم کنید', 'تا برگشت ولتاژ، ورودی را جدا نگه دارید'],
    src: 'v_in در برابر آستانهٔ ۲۸ ولت' },
  E16: { sev: 'er', title: 'باتری وصل نیست یا خوانده نمی‌شود', reset: false,
    say: 'ولتاژ یکی از دو نیمهٔ باتری تقریباً صفر است؛ یعنی برد باتری نمی‌بیند.',
    causes: ['باتری جدا شده', 'فیوز باتری سوخته', 'سیم قطع'],
    steps: ['اتصال باتری‌ها را چک کنید', 'فیوز را بررسی کنید', 'ولتاژ دو نیمه را در داشبورد ببینید'],
    src: 'v_low / v_high زیر ۶ ولت' },
  E17: { sev: 'wa', title: 'شارژر در تلاش مجدد', reset: false,
    say: 'کانال شارژ پس از یک حفاظت منتظر است و چند لحظهٔ دیگر خودش دوباره شروع می‌کند.',
    causes: ['حفاظت ترانسفورماتور عمل کرده', 'افت لحظه‌ای ورودی'],
    steps: ['چند دقیقه صبر کنید', 'اگر چند بار پشت‌سرهم شد، آمار و دیاگ را ببینید'],
    src: 'state = ۵' },
  E18: { sev: 'in', title: 'شارژر منتظر ورودی', reset: false,
    say: 'کانال شارژ آماده است ولی ورودی ۲۴ ولت در حد لازم نیست، پس شارژ شروع نمی‌شود.',
    causes: ['ورودی قطع یا کم', 'ولتاژ ورودی زیر ۲۲ ولت'],
    steps: ['ولتاژ ورودی را ببینید', 'منبع را چک کنید'],
    src: 'state = ۶' },
  E19: { sev: 'in', title: 'شارژ یک کانال از پنل قطع شده', reset: false,
    say: 'یک مدیر شارژ این کانال را عمداً قطع کرده است. تا وصل نشود، این کانال شارژ نمی‌کند.',
    causes: ['قطع دستی برای سرویس'],
    steps: ['اگر سرویس تمام شده، از بخش مدیریت دوباره وصل کنید'],
    src: 'پارامتر ۱۱/۱۲' },
  E20: { sev: 'vi', title: 'مود تعمیرات فعال است', reset: false,
    say: 'کسی از پنل مهندسی، برد را در حالت دستی گذاشته است. در این حالت حفاظت‌های خودکار باتری کنار می‌روند.',
    causes: ['کار کالیبراسیون روی بنچ', 'پنل مهندسی باز مانده'],
    steps: ['اگر کسی مشغول کار نیست، مود دستی را از پنل مهندسی ببندید', 'پس از ۳ ثانیه سکوت، برد خودش از مود دستی بیرون می‌آید'],
    src: 'fl بیت ۵' },
  E21: { sev: 'in', title: 'در حال آماده‌سازی اندازه‌گیری', reset: false,
    say: 'برد تازه روشن شده و چند لحظه طول می‌کشد تا خواندن‌ها معتبر شوند.',
    causes: ['تازه روشن شده'],
    steps: ['چند ثانیه صبر کنید'],
    src: 'fl بیت ۲ (بی‌اعتبار)' },
  E22: { sev: 'wa', title: 'حافظهٔ پنل پر شده است', reset: false,
    say: 'حافظهٔ داخلی پنل به سقف تعیین‌شده رسیده و قدیمی‌ترین داده‌ها خودکار پاک می‌شوند تا نوشتن ادامه پیدا کند. آمار روزانه از بین نمی‌رود.',
    causes: ['ثبت طولانی‌مدت', 'بخش حافظهٔ کوچک انتخاب‌شده در آردوینو'],
    steps: ['برای تاریخچهٔ بیشتر، در آردوینو یک طرح پارتیشن با حافظهٔ بزرگ‌تر فلش کنید', 'اگر لازم نیست، خروجی بگیرید و تاریخچه را پاک کنید'],
    src: 'وضعیت حافظهٔ پنل' }
};

/* ==================== Utilities / ابزارها ==================== */
const $  = (s, r) => (r || document).querySelector(s);
const $$ = (s, r) => Array.prototype.slice.call((r || document).querySelectorAll(s));
const pad2 = (v) => (v < 10 ? '0' : '') + v;
/* [EN] "‎+۳:۳۰" from an offset in minutes. / [FA] «‎+۳:۳۰» از اختلاف به دقیقه. */
function tzLabel(tzMin) {
  const sign = (tzMin < 0) ? '−' : '+';
  const a = Math.abs(tzMin);
  return sign + fa(Math.floor(a / 60)) + ':' + fa(pad2(a % 60));
}

/* [EN] Persian digits for everything the USER reads; codes and IDs stay Latin.
   [FA] اعداد فارسی برای هر چیزی که کاربر می‌خواند؛ کدها لاتین می‌مانند. */
let LATIN_DIGITS = false;
const FA_DIGITS = '۰۱۲۳۴۵۶۷۸۹';
function fa(s) {
  const t = String(s);
  if (LATIN_DIGITS) { return t; }
  return t.replace(/[0-9]/g, (d) => FA_DIGITS[+d]);
}
function num(v, digits) {
  const n = Number(v);
  if (!isFinite(n)) { return '—'; }
  const d = (digits === undefined) ? 0 : digits;
  return fa(n.toFixed(d).replace(/\B(?=(\d{3})+(?!\d))/g, '٬'));
}
function esc(s) {
  return String(s === undefined || s === null ? '' : s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

/* ==================== Time / زمان ==================== */
/* [EN] The board has no RTC. Until a browser hands its clock over, all times
   are "since panel start" and the UI says so instead of inventing a date.
   [FA] برد ساعتِ تقویمی ندارد. تا وقتی مرورگر ساعتش را ندهد، همهٔ زمان‌ها
   «از زمان روشن‌شدن پنل» هستند و پنل همان را می‌گوید، نه یک تاریخ ساختگی. */
function dur(sec) {
  const s = Math.max(0, Math.round(Number(sec) || 0));
  if (s < 60) { return fa(s) + ' ثانیه'; }
  const m = Math.floor(s / 60), h = Math.floor(m / 60), d = Math.floor(h / 24);
  if (s < 3600) { return fa(m) + ' دقیقه'; }
  if (h < 24) { return fa(h) + ' ساعت و ' + fa(m % 60) + ' دقیقه'; }
  return fa(d) + ' روز و ' + fa(h % 24) + ' ساعت';
}
function durShort(sec) {
  const s = Math.max(0, Math.round(Number(sec) || 0));
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
  if (s < 3600) { return fa(m) + 'د ' + fa(s % 60) + 'ث'; }
  return fa(h) + 'س ' + fa(m) + 'د';
}
/* [EN] Jalali conversion (Birashk-free, the standard arithmetic algorithm).
   [FA] تبدیل تاریخ میلادی به شمسی با الگوریتم حسابی استاندارد. */
function jalaliOf(gy, gm, gd) {
  const gDaysInMonth = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  let gy2 = (gm > 2) ? (gy + 1) : gy;
  let days = 355666 + (365 * gy) + Math.floor((gy2 + 3) / 4) - Math.floor((gy2 + 99) / 100)
    + Math.floor((gy2 + 399) / 400) + gd;
  for (let i = 0; i < gm - 1; i++) { days += gDaysInMonth[i]; }
  let jy = -1595 + (33 * Math.floor(days / 12053));
  days %= 12053;
  jy += 4 * Math.floor(days / 1461);
  days %= 1461;
  if (days > 365) { jy += Math.floor((days - 1) / 365); days = (days - 1) % 365; }
  const jm = (days < 186) ? (1 + Math.floor(days / 31)) : (7 + Math.floor((days - 186) / 30));
  const jd = 1 + ((days < 186) ? (days % 31) : ((days - 186) % 30));
  return { y: jy, m: jm, d: jd };
}
const J_MONTH = ['فروردین', 'اردیبهشت', 'خرداد', 'تیر', 'مرداد', 'شهریور',
                 'مهر', 'آبان', 'آذر', 'دی', 'بهمن', 'اسفند'];
/* [EN] The inverse of jalaliOf: a date the admin TYPES in Persian comes back as
   a Gregorian day so it can become an epoch second. Both directions live next to
   each other on purpose - a pair that drifts apart is a date that jumps a day.
   [FA] عکس jalaliOf: تاریخی که مدیر به فارسی «تایپ» می‌کند، به روز میلادی
   برمی‌گردد تا به ثانیهٔ مطلق تبدیل شود. هر دو جهت عمداً کنار هم‌اند - جفتی که
   از هم جدا بیفتد، تاریخی است که یک روز می‌پرد. */
function gregorianOfJalali(jy, jm, jd) {
  let days = -355668 + (365 * (jy + 1595)) + (Math.floor((jy + 1595) / 33) * 8)
    + Math.floor((((jy + 1595) % 33) + 3) / 4) + jd
    + ((jm < 7) ? ((jm - 1) * 31) : (((jm - 7) * 30) + 186));
  let gy = 400 * Math.floor(days / 146097);
  days %= 146097;
  if (days > 36524) { days--; gy += 100 * Math.floor(days / 36524); days %= 36524; if (days >= 365) { days++; } }
  gy += 4 * Math.floor(days / 1461);
  days %= 1461;
  if (days > 365) { gy += Math.floor((days - 1) / 365); days = (days - 1) % 365; }
  let gd = days + 1;
  const leap = ((gy % 4 === 0 && gy % 100 !== 0) || (gy % 400 === 0));
  const sal = [0, 31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  let gm = 1;
  while (gm <= 12 && gd > sal[gm]) { gd -= sal[gm]; gm++; }
  return { y: gy, m: gm, d: gd };
}

/* [EN] A Jalali date + local time + the panel's own offset → epoch seconds.
   [FA] تاریخ شمسی + ساعت محلی + اختلاف خود پنل ← ثانیهٔ مطلق. */
function epochFromJalali(jy, jm, jd, hh, mi, tzMin) {
  const g = gregorianOfJalali(jy, jm, jd);
  return Math.floor(Date.UTC(g.y, g.m - 1, g.d, hh, mi, 0) / 1000) - (tzMin * 60);
}

/* [EN] The panel's own wall clock, independent of the viewer's timezone: shift
   the epoch by the PANEL's offset and read it back in UTC. A viewer in another
   country then sees the machine's time, not their own - which is the only
   useful clock on this page.
   [FA] ساعت دیواری خود پنل، مستقل از منطقهٔ مرورگر: ثانیهٔ مطلق را با اختلاف
   «پنل» جابه‌جا کن و با UTC بخوان. بینندهٔ کشوری دیگر آن‌وقت ساعت ماشین را
   می‌بیند نه ساعت خودش - و این تنها ساعت مفید این صفحه است. */
function panelParts(unixSec, tzMin) {
  const d = new Date((unixSec + ((tzMin || 0) * 60)) * 1000);
  return { y: d.getUTCFullYear(), mo: d.getUTCMonth() + 1, d: d.getUTCDate(),
           h: d.getUTCHours(), mi: d.getUTCMinutes(), s: d.getUTCSeconds() };
}
function panelJalaliStr(unixSec, tzMin) {
  const p = panelParts(unixSec, tzMin);
  const j = jalaliOf(p.y, p.mo, p.d);
  return fa(j.d) + ' ' + J_MONTH[j.m - 1] + ' ' + fa(j.y);
}
function panelClockStr(unixSec, tzMin) {
  const p = panelParts(unixSec, tzMin);
  return fa(pad2(p.h)) + ':' + fa(pad2(p.mi));
}

function jalaliStr(unixSec) {
  const d = new Date(unixSec * 1000);
  const j = jalaliOf(d.getFullYear(), d.getMonth() + 1, d.getDate());
  return fa(j.d) + ' ' + J_MONTH[j.m - 1] + ' ' + fa(j.y);
}
function clockStr(unixSec) {
  const d = new Date(unixSec * 1000);
  return fa(pad2(d.getHours())) + ':' + fa(pad2(d.getMinutes())) + ':' + fa(pad2(d.getSeconds()));
}
function stampStr(unixSec, uptimeSec) {
  if (S.live && S.live.now && S.live.now.wallValid && unixSec > 1600000000) {
    return jalaliStr(unixSec) + ' — ' + clockStr(unixSec);
  }
  if (uptimeSec !== undefined && uptimeSec !== null) { return 'از روشن‌شدن: ' + dur(uptimeSec) + ' پیش'; }
  return 'زمان نامعلوم';
}
function agoStr(unixSec) {
  if (!S.live || !S.live.now || !S.live.now.wallValid || !(unixSec > 1600000000)) { return '—'; }
  const dt = Math.max(0, S.nowEpoch - unixSec);
  if (dt < 45) { return 'همین حالا'; }
  return dur(dt) + ' پیش';
}

/* ==================== Toast + modal / پیام و تأیید ==================== */
function toast(msg, kind) {
  const ic = { ok: 'M4 13l5 5L20 6', er: 'M12 8v5M12 17h.01M10.3 3.9 2.6 17a2 2 0 0 0 1.7 3h15.4a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0Z',
               wa: 'M12 9v4M12 17h.01M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18Z', mu: 'M12 8h.01M11 12h1v5h1' }[kind || 'mu'];
  const d = document.createElement('div');
  d.className = 'toast ' + (kind || 'mu');
  d.innerHTML = '<svg class="ic" viewBox="0 0 24 24"><path d="' + ic + '" stroke="currentColor"/></svg><div>' + esc(msg) + '</div>';
  $('#toasts').appendChild(d);
  setTimeout(() => { d.style.opacity = '0'; d.style.transform = 'translateY(6px)'; d.style.transition = 'all .25s'; }, 3400);
  setTimeout(() => d.remove(), 3800);
}
function say(msg) { const n = $('#sr-live'); if (n) { n.textContent = msg; } }
function confirmBox(title, html, yesLabel, onYes) {
  const m = $('#modal');
  $('#modal-title').textContent = title;
  $('#modal-body').innerHTML = html;
  $('#modal-yes').textContent = yesLabel || 'تأیید';
  $('#modal-yes').className = 'btn';
  m.hidden = false;
  const close = () => { m.hidden = true; $('#modal-yes').onclick = null; $('#modal-no').onclick = null; };
  $('#modal-no').onclick = close;
  $('#modal-yes').onclick = () => { close(); onYes(); };
}
function confirmDanger(title, html, yesLabel, onYes) {
  confirmBox(title, html, yesLabel, onYes);
  $('#modal-yes').className = 'btn er';
}

/* ==================== API client / کلاینت سرویس ==================== */
let AUTH_FAILED = false;
async function api(path, opts) {
  const o = Object.assign({ headers: { 'Accept': 'application/json' }, cache: 'no-store' }, opts || {});
  let r;
  try { r = await fetch(path, o); }
  catch (e) { throw new Error('ارتباط با پنل برقرار نشد'); }
  if (r.status === 401) { AUTH_FAILED = true; showLogin('برای دیدن اطلاعات وارد شوید'); throw new Error('auth'); }
  if (!r.ok) { throw new Error('خطای ' + r.status); }
  const txt = await r.text();
  try { return txt ? JSON.parse(txt) : {}; }
  catch (e) { throw new Error('پاسخ نامعتبر از پنل'); }
}
async function apiPost(path, obj) {
  const body = Object.keys(obj || {}).map(k => encodeURIComponent(k) + '=' + encodeURIComponent(obj[k])).join('&');
  return api(path, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
}

/* ==================== App state / وضعیت برنامه ==================== */
const S = {
  me: null, live: null, series: null, events: null, stats: null,
  view: 'dash', ring: [], timer: null, slowTimer: null, nowEpoch: 0, nowTzMin: 0,
  hw: { storage: {}, version: {} }, busy: false
};
const can = (what) => {
  const r = S.me ? S.me.role : '';
  if (what === 'admin') { return r === 'admin'; }
  if (what === 'operate') { return r === 'admin' || r === 'operator'; }
  return !!r;
};

/* ==================== SVG chart library / کتابخانهٔ نمودار ====================
   [EN] Hand-drawn SVG, no library: the page must work offline and the payload
   must stay small enough for an ESP8266 to serve. Every chart gets a tooltip
   and keyboard-free hover, and every number on an axis is Persian.
   [FA] نمودارهای SVG دست‌ساز بدون کتابخانه: صفحه باید آفلاین کار کند و حجم
   پاسخ‌ها برای ESP8266 کوچک بماند. هر نمودار راهنمای شناور دارد و همهٔ
   اعداد محورها فارسی‌اند. */
const SVG_NS = 'http://www.w3.org/2000/svg';
function svgEl(tag, attrs) {
  const n = document.createElementNS(SVG_NS, tag);
  if (attrs) { Object.keys(attrs).forEach(k => n.setAttribute(k, attrs[k])); }
  return n;
}
function niceMax(v) {
  if (!isFinite(v) || v <= 0) { return 1; }
  const p = Math.pow(10, Math.floor(Math.log10(v)));
  const f = v / p;
  const step = (f <= 1) ? 1 : (f <= 2) ? 2 : (f <= 5) ? 5 : 10;
  return step * p;
}
let TIP = null;
function tipShow(html, ev) {
  if (!TIP) { TIP = document.createElement('div'); TIP.className = 'tip'; document.body.appendChild(TIP); }
  TIP.innerHTML = html; TIP.style.display = 'block';
  const w = TIP.offsetWidth, h = TIP.offsetHeight;
  let x = ev.clientX + 14, y = ev.clientY - h - 12;
  if (x + w > window.innerWidth - 8) { x = ev.clientX - w - 14; }
  if (y < 8) { y = ev.clientY + 16; }
  TIP.style.left = x + 'px'; TIP.style.top = y + 'px';
}
function tipHide() { if (TIP) { TIP.style.display = 'none'; } }

/* [EN] Multi-series line/area chart. series = [{name,color,data,fill,dash}]
   where data is an array of numbers (nulls allowed) sharing the same x index.
   [FA] نمودار خطی/سطحی چندسری. هر سری آرایه‌ای از اعداد (null مجاز) است. */
function chartLine(host, opt) {
  const W = opt.w || 640, H = opt.h || 190, P = { t: 14, r: 10, b: 22, l: 42 };
  const iw = W - P.l - P.r, ih = H - P.t - P.b;
  const series = (opt.series || []).filter(s => s && s.data && s.data.length);
  const all = [].concat.apply([], series.map(s => s.data.filter(v => v !== null && isFinite(v))));
  const yMin = (opt.yMin !== undefined) ? opt.yMin : Math.min.apply(null, all.concat([0]));
  const yMax = (opt.yMax !== undefined) ? opt.yMax : niceMax(Math.max.apply(null, all.concat([1])));
  const n = Math.max.apply(null, series.map(s => s.data.length).concat([2]));
  const X = (i) => P.l + iw - (n <= 1 ? iw / 2 : (i / (n - 1)) * iw);   /* RTL-friendly: newest at left edge? no - time grows left→right reversed */
  const Y = (v) => P.t + ih - ((v - yMin) / (yMax - yMin || 1)) * ih;
  host.innerHTML = '';
  const svg = svgEl('svg', { viewBox: '0 0 ' + W + ' ' + H, class: 'chart', preserveAspectRatio: 'none', role: 'img' });
  const gid = 'g' + Math.random().toString(36).slice(2, 8);

  for (let k = 0; k <= 3; k++) {
    const v = yMin + (yMax - yMin) * (k / 3), y = Y(v);
    svg.appendChild(svgEl('line', { x1: P.l, x2: W - P.r, y1: y, y2: y, class: 'grid-line' }));
    const t = svgEl('text', { x: P.l - 6, y: y + 3, 'text-anchor': 'end' });
    t.textContent = fa(Math.round(v));
    svg.appendChild(t);
  }
  /* [EN] Labels ride the SAME axis function as the data. The newest sample is
     drawn at the LEFT edge (RTL, like the rest of the page), so label 0 - which
     is the oldest - belongs at the right edge. Before this was flipped, every
     time axis read backwards: the newest tick carried the oldest time.
     [FA] برچسب‌ها همان تابع محور داده را سوار می‌شوند. جدیدترین نمونه در لبهٔ
     چپ رسم می‌شود (راست‌به‌چپ، مثل بقیهٔ صفحه)، پس برچسب صفر - که قدیمی‌ترین
     است - جای‌اش لبهٔ راست است. قبلاً برعکس بود و هر محور زمان وارونه خوانده
     می‌شد: تیک جدید، قدیمی‌ترین زمان را نشان می‌داد. */
  if (opt.xLabels && opt.xLabels.length) {
    const many = opt.xLabels.length;
    opt.xLabels.forEach((lab, i) => {
      const x = P.l + iw - (many <= 1 ? 0 : (i / (many - 1)) * iw);
      const t = svgEl('text', { x: x, y: H - 5, 'text-anchor': i === 0 ? 'end' : (i === many - 1 ? 'start' : 'middle') });
      t.textContent = lab; svg.appendChild(t);
    });
  }
  /* [EN] input-present shading: the single most useful annotation on the page.
     [FA] سایهٔ حضور ورودی: مفیدترین نشانهٔ این صفحه. */
  if (opt.bands && opt.bands.length) {
    opt.bands.forEach(b => {
      const x0 = X(b.from), x1 = X(b.to);
      svg.appendChild(svgEl('rect', {
        x: Math.min(x0, x1), y: P.t, width: Math.abs(x1 - x0), height: ih,
        fill: b.color || 'rgba(47,214,163,.09)'
      }));
    });
  }
  if (opt.marks && opt.marks.length) {
    opt.marks.forEach(m => {
      const x = X(m.i);
      svg.appendChild(svgEl('circle', { cx: x, cy: P.t + 3, r: 3, fill: m.color || '#a78bfa' }));
      svg.appendChild(svgEl('line', { x1: x, x2: x, y1: P.t + 6, y2: P.t + ih, stroke: m.color || '#a78bfa', 'stroke-width': 1, 'stroke-dasharray': '2 3', opacity: .55 }));
    });
  }
  series.forEach(s => {
    const pts = [];
    s.data.forEach((v, i) => { if (v !== null && isFinite(v)) { pts.push([X(i), Y(v)]); } });
    if (pts.length < 2) { return; }
    const d = pts.map((p, i) => (i ? 'L' : 'M') + p[0].toFixed(1) + ' ' + p[1].toFixed(1)).join(' ');
    if (s.fill) {
      const area = d + ' L' + pts[pts.length - 1][0].toFixed(1) + ' ' + (P.t + ih) + ' L' + pts[0][0].toFixed(1) + ' ' + (P.t + ih) + ' Z';
      const grad = svgEl('linearGradient', { id: gid + s.name, x1: 0, y1: 0, x2: 0, y2: 1 });
      grad.appendChild(svgEl('stop', { offset: '0%', 'stop-color': s.color, 'stop-opacity': '.42' }));
      grad.appendChild(svgEl('stop', { offset: '100%', 'stop-color': s.color, 'stop-opacity': '.02' }));
      const defs = svgEl('defs'); defs.appendChild(grad); svg.appendChild(defs);
      svg.appendChild(svgEl('path', { d: area, fill: 'url(#' + gid + s.name + ')', class: 'area' }));
    }
    svg.appendChild(svgEl('path', {
      d: d, class: 'ln', stroke: s.color, 'stroke-dasharray': s.dash || 'none'
    }));
  });
  /* [EN] Hover layer: nearest x index, one tooltip for every series.
     [FA] لایهٔ شناور: نزدیک‌ترین نمونه، یک راهنما برای همهٔ سری‌ها. */
  const hover = svgEl('rect', { x: P.l, y: P.t, width: iw, height: ih, fill: 'transparent', style: 'cursor:crosshair' });
  const vline = svgEl('line', { x1: 0, x2: 0, y1: P.t, y2: P.t + ih, stroke: '#5b9dff', 'stroke-width': 1, opacity: 0 });
  svg.appendChild(vline);
  hover.addEventListener('mousemove', (ev) => {
    const box = svg.getBoundingClientRect();
    const px = ((ev.clientX - box.left) / box.width) * W;
    const frac = (P.l + iw - px) / iw;
    const idx = Math.max(0, Math.min(n - 1, Math.round(frac * (n - 1))));
    const x = X(idx);
    vline.setAttribute('x1', x); vline.setAttribute('x2', x); vline.setAttribute('opacity', .8);
    let html = '<b>' + (opt.xTip ? opt.xTip(idx) : fa(idx)) + '</b>';
    series.forEach(s => {
      const v = s.data[idx];
      html += '<br>' + s.name + ': ' + (v === null || v === undefined || !isFinite(v) ? '—' : fa(Math.round(v * 100) / 100));
    });
    tipShow(html, ev);
  });
  hover.addEventListener('mouseleave', () => { vline.setAttribute('opacity', 0); tipHide(); });
  svg.appendChild(hover);
  host.appendChild(svg);
}

/* [EN] Vertical bar chart. bars = [{label, value, color, tip}]
   [FA] نمودار میله‌ای عمودی. */
function chartBars(host, bars, opt) {
  const o = opt || {};
  const W = o.w || 640, H = o.h || 190, P = { t: 12, r: 8, b: 26, l: 36 };
  const iw = W - P.l - P.r, ih = H - P.t - P.b;
  const max = o.yMax || niceMax(Math.max.apply(null, bars.map(b => b.value).concat([1])));
  host.innerHTML = '';
  const svg = svgEl('svg', { viewBox: '0 0 ' + W + ' ' + H, class: 'chart', preserveAspectRatio: 'none', role: 'img' });
  for (let k = 0; k <= 3; k++) {
    const v = max * (k / 3), y = P.t + ih - (v / max) * ih;
    svg.appendChild(svgEl('line', { x1: P.l, x2: W - P.r, y1: y, y2: y, class: 'grid-line' }));
    const t = svgEl('text', { x: P.l - 6, y: y + 3, 'text-anchor': 'end' }); t.textContent = fa(Math.round(v)); svg.appendChild(t);
  }
  const step = iw / Math.max(1, bars.length);
  const bw = Math.max(2, Math.min(o.maxBar || 26, step * 0.68));
  bars.forEach((b, i) => {
    const h = Math.max(b.value > 0 ? 2 : 0, (b.value / max) * ih);
    const x = P.l + iw - (i + 0.5) * step - bw / 2;
    const y = P.t + ih - h;
    const r = svgEl('rect', { x: x, y: y, width: bw, height: h, rx: Math.min(4, bw / 2), class: 'bar', fill: b.color || '#5b9dff' });
    const tt = svgEl('title'); tt.textContent = b.tip || (b.label + ': ' + fa(b.value)); r.appendChild(tt);
    r.addEventListener('mouseenter', (ev) => tipShow('<b>' + esc(b.label) + '</b><br>' + (b.tip || fa(b.value)), ev));
    r.addEventListener('mousemove', (ev) => tipShow('<b>' + esc(b.label) + '</b><br>' + (b.tip || fa(b.value)), ev));
    r.addEventListener('mouseleave', tipHide);
    svg.appendChild(r);
    if (b.label && (bars.length <= 14 || i % Math.ceil(bars.length / 8) === 0)) {
      const t = svgEl('text', { x: x + bw / 2, y: H - 8, 'text-anchor': 'middle' });
      t.textContent = b.label; svg.appendChild(t);
    }
  });
  host.appendChild(svg);
}

/* [EN] Donut for time shares / [FA] دونات برای سهم زمان‌ها */
function chartDonut(host, parts, centerTop, centerBot) {
  const W = 190, H = 190, cx = W / 2, cy = H / 2, r = 68, sw = 22;
  const total = parts.reduce((a, p) => a + p.value, 0) || 1;
  host.innerHTML = '';
  const svg = svgEl('svg', { viewBox: '0 0 ' + W + ' ' + H, class: 'chart', style: 'max-height:200px', role: 'img' });
  let acc = 0;
  const C = 2 * Math.PI * r;
  parts.forEach(p => {
    const frac = p.value / total;
    const seg = svgEl('circle', {
      cx: cx, cy: cy, r: r, fill: 'none', stroke: p.color, 'stroke-width': sw,
      'stroke-dasharray': (frac * C).toFixed(2) + ' ' + C.toFixed(2),
      'stroke-dashoffset': (-acc * C).toFixed(2), transform: 'rotate(-90 ' + cx + ' ' + cy + ')'
    });
    const tt = svgEl('title'); tt.textContent = p.name + ': ' + fa(Math.round(frac * 100)) + '٪'; seg.appendChild(tt);
    seg.addEventListener('mouseenter', (ev) => tipShow('<b>' + esc(p.name) + '</b><br>' + fa(Math.round(frac * 100)) + '٪', ev));
    seg.addEventListener('mousemove', (ev) => tipShow('<b>' + esc(p.name) + '</b><br>' + fa(Math.round(frac * 100)) + '٪', ev));
    seg.addEventListener('mouseleave', tipHide);
    svg.appendChild(seg);
    acc += frac;
  });
  if (centerTop) {
    const t1 = svgEl('text', { x: cx, y: cy + 2, 'text-anchor': 'middle', class: 'lbl', style: 'font-size:15px' });
    t1.textContent = centerTop; svg.appendChild(t1);
    const t2 = svgEl('text', { x: cx, y: cy + 19, 'text-anchor': 'middle' });
    t2.textContent = centerBot || ''; svg.appendChild(t2);
  }
  host.appendChild(svg);
}

/* [EN] Heatmap: rows = metrics, cols = hours of day (RTL order: hour 0 at right)
   [FA] نقشهٔ حرارتی: ردیف = سنجه، ستون = ساعت شبانه‌روز (ساعت ۰ سمت راست) */
function chartHeat(host, matrix, opt) {
  const o = opt || {}, rows = matrix.length, cols = matrix[0] ? matrix[0].length : 0;
  const cw = o.cw || 17, ch = o.ch || 22, P = { t: 6, r: 26, b: 18, l: 6 };
  const W = P.l + P.r + cols * cw, H = P.t + P.b + rows * ch;
  host.innerHTML = '';
  const svg = svgEl('svg', { viewBox: '0 0 ' + W + ' ' + H, class: 'chart', role: 'img', style: 'max-height:' + H + 'px' });
  const colors = o.colors || ['#101827', '#132b3f', '#17584f', '#1f9e78', '#2fd6a3'];
  matrix.forEach((row, ri) => {
    row.forEach((v, ci) => {
      const level = (v <= 0) ? 0 : Math.min(colors.length - 1, 1 + Math.floor((v / (o.max || 1)) * (colors.length - 2)));
      const x = P.l + (cols - 1 - ci) * cw, y = P.t + ri * ch;
      const r = svgEl('rect', { x: x, y: y, width: cw - 2, height: ch - 2, rx: 4, fill: colors[level] });
      const tt = svgEl('title'); tt.textContent = (o.rowNames ? o.rowNames[ri] + ' — ' : '') + 'ساعت ' + fa(ci) + ': ' + fa(v);
      r.appendChild(tt);
      r.addEventListener('mouseenter', (ev) => tipShow((o.rowNames ? esc(o.rowNames[ri]) + ' — ' : '') + 'ساعت ' + fa(ci) + '<br>' + esc(o.unitName || '') + ' ' + fa(v), ev));
      r.addEventListener('mousemove', (ev) => tipShow((o.rowNames ? esc(o.rowNames[ri]) + ' — ' : '') + 'ساعت ' + fa(ci) + '<br>' + esc(o.unitName || '') + ' ' + fa(v), ev));
      r.addEventListener('mouseleave', tipHide);
      svg.appendChild(r);
    });
    if (o.rowNames && o.rowNames[ri]) {
      const t = svgEl('text', { x: W - P.r + 4, y: P.t + ri * ch + ch / 2 + 3 });
      t.textContent = o.rowNames[ri]; svg.appendChild(t);
    }
  });
  [0, 6, 12, 18].forEach(h => {
    const t = svgEl('text', { x: P.l + (cols - 1 - h) * cw + cw / 2, y: H - 4, 'text-anchor': 'middle' });
    t.textContent = fa(h); svg.appendChild(t);
  });
  host.appendChild(svg);
}

/* ==================== Shared renderers / سازنده‌های مشترک ==================== */
function card(title, icon, body, extra) {
  return '<div class="card' + (extra && extra.cls ? ' ' + extra.cls : '') + '">' +
    (title ? '<div class="head"><span class="t">' + (icon || '') + '<span>' + title + '</span></span>' +
      (extra && extra.head ? extra.head : '') + '</div>' : '') + body + '</div>';
}
const ICO = {
  bolt: '<svg class="ic" viewBox="0 0 24 24"><path d="M13 2 4 14h6l-1 8 9-12h-6l1-8Z"/></svg>',
  bat:  '<svg class="ic" viewBox="0 0 24 24"><rect x="2" y="7" width="17" height="10" rx="2.5"/><path d="M22 10v4"/></svg>',
  plug: '<svg class="ic" viewBox="0 0 24 24"><path d="M9 3v6M15 3v6M6 9h12v2a6 6 0 0 1-6 6 6 6 0 0 1-6-6V9ZM12 17v4"/></svg>',
  chart:'<svg class="ic" viewBox="0 0 24 24"><path d="M4 20V6M4 20h16M8 20v-7M13 20v-11M18 20v-4"/></svg>',
  warn: '<svg class="ic" viewBox="0 0 24 24"><path d="M10.3 3.9 2.6 17a2 2 0 0 0 1.7 3h15.4a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0ZM12 9v4M12 17h.01"/></svg>',
  ok:   '<svg class="ic" viewBox="0 0 24 24"><path d="M4 13l5 5L20 6"/></svg>',
  clock:'<svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg>',
  gear: '<svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"/></svg>',
  user: '<svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="8" r="3.6"/><path d="M5 20c1.2-4 4-6 7-6s5.8 2 7 6"/></svg>',
  dl:   '<svg class="ic" viewBox="0 0 24 24"><path d="M12 3v12m0 0 4-4m-4 4-4-4M4 19h16"/></svg>',
  info: '<svg class="ic" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 8h.01"/></svg>',
  link: '<svg class="ic" viewBox="0 0 24 24"><path d="M10 13a5 5 0 0 0 7 0l2-2a5 5 0 0 0-7-7l-1 1M14 11a5 5 0 0 0-7 0l-2 2a5 5 0 0 0 7 7l1-1"/></svg>',
  trash:'<svg class="ic" viewBox="0 0 24 24"><path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/></svg>'
};
const badge = (txt, kind) => '<span class="badge ' + (kind || 'mu') + '">' + txt + '</span>';

/* [EN] Charge percentage, derived from the board's OWN mapping parameters
   (ids 74/75) instead of a hard-coded pair - step by step, never one line.
   [FA] درصد شارژ، از پارامترهای خود برد (۷۴/۷۵) مشتق می‌شود، نه عدد ثابت؛
   گام‌به‌گام محاسبه می‌شود، نه یک‌خطی. */
function packPercent(mv) {
  const p = (S.live && S.live.par) ? S.live.par : null;
  const minMv = (p && p[74]) ? p[74] : 21000;
  const maxMv = (p && p[75]) ? p[75] : 29000;
  const range = maxMv - minMv;
  if (range <= 0) { return 0; }
  const offset = mv - minMv;
  const scaled = offset * 100;
  const pct = Math.floor(scaled / range);
  return Math.max(0, Math.min(100, pct));
}
function halfStatus(state) {
  const s = CHG_STATE[state] || { t: 'نامعلوم', s: 'mu' };
  return s;
}

/* ==================== Chart data helpers / کمکی‌های داده ==================== */
function seriesFromLive(ring) {
  const out = { t: [], vin: [], v24: [], v12: [], i1: [], i2: [], d1: [], d2: [] };
  (ring || []).forEach(s => {
    if (!s) { return; }
    out.t.push(s.t); out.vin.push(s.vin); out.v24.push(s.v24); out.v12.push(s.v12);
    out.i1.push(s.i1); out.i2.push(s.i2); out.d1.push(s.d1); out.d2.push(s.d2);
  });
  return out;
}

/* ==================== Login view / صفحهٔ ورود ==================== */
function showLogin(msg) {
  stopPolling();
  $('#hdr').hidden = true; $('#nav').hidden = true; $('#ftr').hidden = true;
  $('#view').innerHTML =
    '<div class="login-wrap"><form class="login" id="login-form" autocomplete="on">' +
    '<div class="login-brand"><span class="logo"></span><h1>پنل کاربر ChangeOver</h1>' +
    '<p>وضعیت و آمار دستگاه تغییر مسیر تغذیه</p></div>' +
    (msg ? '<div class="card flat" style="margin-bottom:14px;border-color:rgba(247,195,60,.35)">' + badge(ICO.warn + ' توجه', 'wa') + ' <span class="sub">' + esc(msg) + '</span></div>' : '') +
    '<div class="field"><label for="lu">نام کاربری</label>' +
    '<input class="inp" id="lu" name="u" autocomplete="username" required autofocus placeholder="admin"></div>' +
    '<div class="field"><label for="lp">گذرواژه</label>' +
    '<input class="inp" id="lp" name="p" type="password" autocomplete="current-password" required placeholder="••••••"></div>' +
    '<div class="err-text" id="login-err" style="min-height:20px"></div>' +
    '<button class="btn wide" type="submit">ورود به پنل</button>' +
    '<div class="login-foot" id="login-foot">اگر گذرواژه را نمی‌دانید، از مدیر دستگاه بخواهید</div>' +
    '</form></div>';
  $('#login-form').addEventListener('submit', async (ev) => {
    ev.preventDefault();
    const u = $('#lu').value.trim(), p = $('#lp').value;
    const err = $('#login-err'); err.textContent = '';
    if (!u || !p) { err.textContent = 'نام کاربری و گذرواژه را وارد کنید'; return; }
    const btn = $('#login-form button[type=submit]');
    btn.disabled = true; btn.textContent = 'در حال بررسی…';
    try {
      const r = await apiPost(API.login, { u: u, p: p });
      if (!r.ok) { err.textContent = r.err || 'نام کاربری یا گذرواژه درست نیست'; btn.disabled = false; btn.textContent = 'ورود به پنل'; return; }
      AUTH_FAILED = false;
      S.me = { user: r.user, role: r.role, mustChange: !!r.must };
      /* [EN] The clock is NOT pushed from here any more: only an admin sets the
         time (owner's instruction). See the clock card in the admin view.
         [FA] ساعت دیگر از اینجا فرستاده نمی‌شود: فقط مدیر وقت را ست می‌کند
         (دستور صاحب دستگاه). کارت ساعت در بخش مدیریت را ببینید. */
      await boot();
      if (r.must) { setTimeout(() => askChangePassword(true), 450); }
    } catch (e) {
      err.textContent = (e.message === 'auth') ? 'نام کاربری یا گذرواژه درست نیست' : e.message;
      btn.disabled = false; btn.textContent = 'ورود به پنل';
    }
  });
}

/* [EN] Password change (also the forced first-login path).
   [FA] تغییر گذرواژه (همان مسیر اجباریِ اولین ورود). */
function askChangePassword(forced) {
  confirmBox(forced ? 'تغییر گذرواژهٔ اولیه' : 'تغییر گذرواژه',
    (forced ? '<p class="sub" style="margin-bottom:10px">برای امنیت دستگاه، گذرواژهٔ پیش‌فرض باید همین حالا عوض شود.</p>' : '') +
    '<div class="field"><label>گذرواژهٔ فعلی</label><input class="inp" id="p0" type="password" autocomplete="current-password"></div>' +
    '<div class="field"><label>گذرواژهٔ جدید (حداقل ۶ نویسه)</label><input class="inp" id="p1" type="password" autocomplete="new-password"></div>' +
    '<div class="field"><label>تکرار گذرواژهٔ جدید</label><input class="inp" id="p2" type="password" autocomplete="new-password"></div>' +
    '<div class="err-text" id="p-err" style="min-height:18px"></div>',
    'ثبت گذرواژه', async () => {
      const p0 = $('#p0').value, p1 = $('#p1').value, p2 = $('#p2').value;
      if (p1.length < 6) { toast('گذرواژهٔ جدید کوتاه است (حداقل ۶ نویسه)', 'er'); if (forced) { setTimeout(() => askChangePassword(true), 300); } return; }
      if (p1 !== p2) { toast('دو گذرواژهٔ جدید یکسان نیستند', 'er'); if (forced) { setTimeout(() => askChangePassword(true), 300); } return; }
      try {
        const r = await apiPost(API.pass, { p0: p0, p1: p1 });
        if (r.ok) { toast('گذرواژه تغییر کرد', 'ok'); S.me.mustChange = false; renderFoot(); }
        else { toast(r.err || 'تغییر گذرواژه انجام نشد', 'er'); if (forced) { setTimeout(() => askChangePassword(true), 300); } }
      } catch (e) { toast(e.message, 'er'); if (forced) { setTimeout(() => askChangePassword(true), 300); } }
    });
  $('#modal-no').textContent = forced ? 'بعداً' : 'انصراف';
}

/* ==================== Dashboard view / داشبورد ==================== */
function renderDash() {
  const L = S.live;
  if (!L) { $('#view').innerHTML = skeletonView(); return; }
  const t = L.t || [];
  const tw = (i) => (t[i] === undefined ? 0 : t[i]);
  const onBattery = !L.flags.input;
  const st1 = tw(TI.st1), st2 = tw(TI.st2);
  const v12 = tw(TI.v12), v24 = tw(TI.v24), vin = tw(TI.vin);
  const vLow = tw(TI.vlow), vHigh = tw(TI.vhigh);
  const i1 = tw(TI.iest1), i2 = tw(TI.iest2);
  const pct1 = packPercent(vHigh), pct2 = packPercent(vLow);
  const charging = (st1 === 1 || st1 === 2 || st2 === 1 || st2 === 2);
  const full = (st1 === 3 && st2 === 3);
  const heroKind = onBattery ? 'run' : (charging ? 'run' : (full ? 'ok' : 'ok'));
  const heroIcon = onBattery ? ICO.bat : (charging ? ICO.bolt : ICO.ok);
  const heroTitle = onBattery ? 'دستگاه روی باتری کار می‌کند'
    : (charging ? 'ورودی وصل است و باتری در حال شارژ است'
      : (full ? 'ورودی وصل است و باتری‌ها پر هستند' : 'ورودی وصل است'));
  const heroNote = onBattery
    ? 'ورودی ۲۴ ولت قطع شده؛ انرژی از باتری‌ها تأمین می‌شود. آخرین قطع: ' + lastEventAgo(4)
    : ('ورودی ' + num(vin / 1000, 1) + ' ولت — ' + (charging ? 'شارژ در جریان است' : 'شارژ فعال نیست'));
  const eToday = L.today || {};
  const ring = seriesFromLive(S.ring);

  $('#view').innerHTML =
    '<section class="hero">' +
      '<div class="hero-state">' +
        '<div class="hero-badge ' + heroKind + '">' + heroIcon.replace('<svg class="ic"', '<svg') + '</div>' +
        '<div><div class="hero-title">' + heroTitle + '</div><div class="hero-note">' + heroNote + '</div>' +
        '<div class="row" style="margin-top:9px">' +
          (L.link.online ? badge('داده زنده', 'ok') : badge('ارتباط قطع — داده‌ها کهنه است', 'er')) +
          (L.flags.manual ? badge('مود تعمیرات', 'vi') : '') +
          (L.faultBits.length ? badge(num(L.faultBits.length) + ' خطای فعال', 'er') : badge('بدون خطا', 'ok')) +
        '</div></div>' +
      '</div>' +
      '<div class="hero-facts">' +
        fact('ورودی ۲۴ ولت', num(vin / 1000, 1) + ' V', L.flags.input ? 'ok' : 'wa') +
        fact('پک باتری', num(v24 / 1000, 1) + ' V', '') +
        fact('شارژ امروز', num(eToday.charges || 0) + ' بار', 'ac') +
        fact('زمان روی باتری امروز', durShort(eToday.runS || 0), 'wa') +
      '</div>' +
    '</section>' +

    '<section class="grid">' +
      batteryCard('باتری بالایی', 'کانال ۱', vHigh, pct1, st1, tw(TI.duty1), i1) +
      batteryCard('باتری پایینی', 'کانال ۲', vLow, pct2, st2, tw(TI.duty2), i2) +
      card('ورودی و ارتباط', ICO.plug,
        '<div class="grid tight">' +
          factKV('ولتاژ ورودی', num(vin / 1000, 1) + ' V') +
          factKV('وضعیت', L.flags.input ? 'وصل' : 'قطع') +
          factKV('سن آخرین داده', num(Math.round((L.link.ageMs || 0) / 1000)) + ' ثانیه') +
          factKV('فریم‌های دریافتی', num(L.link.frames || 0)) +
        '</div>' +
        '<div class="hr"></div>' +
        '<div class="sub">' + (L.link.vm > 0 ? 'نسخهٔ برد و پنل ناهماهنگ است.' : 'نسخهٔ ارتباطی برد و پنل هماهنگ است.') +
        ' خطاهای CRC: ' + num(L.link.ce || 0) + '</div>', { cls: 'flat' }) +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.chart + '<span>روند ۲۴ ساعت گذشته</span></span>' +
      '<span class="s" id="trend-sum">—</span></div>' +
      '<div id="ch-trend"><div class="sk tile"></div></div>' +
      '<div class="legend"><span><i class="ln" style="background:#5b9dff"></i> پک ۲۴ ولت</span>' +
      '<span><i class="ln" style="background:#2fd6a3"></i> باتری پایینی</span>' +
      '<span><i class="ln" style="background:#f7c33c"></i> ورودی</span>' +
      '<span><i style="background:rgba(47,214,163,.16)"></i> بازهٔ وصل بودن ورودی</span></div>' +
    '</section>' +

    '<section class="grid wide">' +
      card('ولتاژها — چند دقیقهٔ آخر', ICO.bolt, '<div id="ch-v"><div class="sk tile"></div></div>') +
      card('جریان شارژ — چند دقیقهٔ آخر', ICO.chart, '<div id="ch-i"><div class="sk tile"></div></div>') +
    '</section>' +

    '<section class="grid">' +
      card('رویدادهای اخیر', ICO.clock, '<div id="recent-events"><div class="sk line"></div><div class="sk line"></div></div>') +
      card('حافظهٔ پنل', ICO.info, storageHTML(), { cls: 'flat' }) +
    '</section>';

  /* trend chart from /api/series */
  drawTrend();
  /* live mini charts from the client ring */
  drawLiveCharts(ring);
  drawRecentEvents();
}

function fact(k, v, kind) {
  return '<div class="fact"><div class="k">' + k + '</div><div class="v ' + (kind || '') + '">' + v + '</div></div>';
}
function factKV(k, v) { return '<div class="fact"><div class="k">' + k + '</div><div class="v">' + v + '</div></div>'; }

/* [EN] One battery card: percentage from the board's own mapping, the live
   charger state, duty and current — all read, never written from here.
   [FA] کارت هر باتری: درصد از نگاشت خود برد، حالت زندهٔ شارژر، duty و جریان. */
function batteryCard(title, ch, mv, pct, state, duty, current) {
  const st = halfStatus(state);
  const kind = pct >= 60 ? 'ok' : (pct >= 30 ? 'mid' : 'low');
  return card(title, ICO.bat,
    '<div class="bat">' +
      '<div class="bat-vis"><div class="bat-shell"><div class="bat-fill ' + kind + '" style="height:' + Math.max(3, pct) + '%"></div></div></div>' +
      '<div class="bat-info">' +
        '<div class="bat-title">' + badge(ch, 'ac') + badge(st.t, st.s) + '</div>' +
        '<div class="bat-v n">' + num(mv / 1000, 2) + ' <small style="font-size:.5em">ولت</small></div>' +
        '<div class="bar" style="margin:8px 0 6px"><i class="' + (kind === 'ok' ? 'ok' : (kind === 'mid' ? 'wa' : 'er')) + '" style="width:' + pct + '%"></i></div>' +
        '<div class="bat-row"><span>شارژ: <b>' + num(pct) + '٪</b></span>' +
        '<span>جریان: <b>' + num(current) + ' mA</b></span>' +
        '<span>توان: <b>' + num((mv * current) / 1000000, 1) + ' W</b></span></div>' +
        '<div class="bat-row"><span>وضعیت PWM: <b>' + (duty > 0 ? num(duty / 10, 1) + '٪' : 'خاموش') + '</b></span></div>' +
      '</div>' +
    '</div>', { cls: 'flat' });
}
function storageHTML() {
  const st = S.hw.storage || {};
  const used = st.usedBytes || 0, total = st.totalBytes || 1;
  const pct = Math.min(100, Math.round((used / total) * 100));
  return '<div class="grid tight">' +
    factKV('نمونه‌های ثبت‌شده', num(st.samples || 0)) +
    factKV('رویدادهای ثبت‌شده', num(st.events || 0)) +
    factKV('روزهای آمارگیری', num(st.days || 0)) +
    factKV('ظرفیت', num(Math.round(total / 1024)) + ' کیلوبایت') +
    '</div>' +
    '<div style="margin-top:10px"><div class="bar"><i class="' + (pct > 92 ? 'er' : pct > 75 ? 'wa' : 'ok') + '" style="width:' + pct + '%"></i></div>' +
    '<div class="sub" style="margin-top:6px">' + num(pct) + '٪ از سهمیهٔ حافظهٔ پنل استفاده شده است' +
    (st.purging ? ' — قدیمی‌ترین داده‌ها خودکار پاک می‌شوند' : '') + '</div></div>';
}
function lastEventAgo(code) {
  const ev = (S.events && S.events.list) ? S.events.list : [];
  for (let i = 0; i < ev.length; i++) { if (ev[i].code === code) { return agoStr(ev[i].t); } }
  return 'ثبت نشده';
}
function drawTrend() {
  const host = $('#ch-trend');
  if (!host) { return; }
  const se = S.series;
  if (!se || !se.n) { host.innerHTML = emptyBox('هنوز دادهٔ کافی برای نمودار ثبت نشده است'); return; }
  const v24 = se.v24.map(v => v / 1000), v12 = se.v12.map(v => v / 1000), vin = (se.vin || []).map(v => v / 1000);
  const bands = (se.vin || []).map((v, i) => ({ on: v > 21000, i: i }));
  /* [EN] contiguous input-present spans become shading bands.
     [FA] بازه‌های پیوستهٔ حضور ورودی به نوارهای سایه تبدیل می‌شوند. */
  const bandList = [];
  let start = null;
  bands.forEach((b, i) => {
    if (b.on && start === null) { start = i; }
    if ((!b.on || i === bands.length - 1) && start !== null) { bandList.push({ from: start, to: i }); start = null; }
  });
  const labels = se.label || [];
  chartLine(host, {
    h: 210, series: [
      { name: 'پک ۲۴ ولت', color: '#5b9dff', data: v24, fill: true },
      { name: 'باتری پایینی', color: '#2fd6a3', data: v12 },
      { name: 'ورودی', color: '#f7c33c', data: vin, dash: '4 3' }
    ],
    bands: bandList, xLabels: labels,
    xTip: (i) => (se.tip && se.tip[i]) ? se.tip[i] : fa(i)
  });
  const sum = $('#trend-sum');
  if (sum && S.stats && S.stats.kpi) {
    sum.textContent = 'شارژ در این بازه: ' + num(S.stats.kpi.charges || 0) + ' بار';
  }
}
function drawLiveCharts(ring) {
  const hv = $('#ch-v'), hi = $('#ch-i');
  if (!hv || !hi || !ring.t.length) { return; }
  const n = ring.t.length;
  const vLabels = [0, 1, 2, 3].map(k => durShort(Math.round(((n - 1 - k * Math.floor(n / 3)) * REFRESH.liveMs) / 1000)));
  chartLine(hv, {
    h: 168, series: [
      { name: 'ورودی', color: '#f7c33c', data: ring.vin.map(v => v / 1000) },
      { name: 'پک ۲۴', color: '#5b9dff', data: ring.v24.map(v => v / 1000) },
      { name: 'پایینی', color: '#2fd6a3', data: ring.v12.map(v => v / 1000), fill: true }
    ], xLabels: vLabels
  });
  chartLine(hi, {
    h: 168, series: [
      { name: 'کانال بالا', color: '#5b9dff', data: ring.i1, fill: true },
      { name: 'کانال پایین', color: '#a78bfa', data: ring.i2 }
    ], xLabels: vLabels
  });
}
function drawRecentEvents() {
  const host = $('#recent-events');
  if (!host) { return; }
  const list = (S.events && S.events.list) ? S.events.list.slice(0, 7) : [];
  if (!list.length) { host.innerHTML = emptyBox('هنوز رویدادی ثبت نشده است'); return; }
  host.innerHTML = '<div class="tw"><table><tbody>' + list.map(e => {
    const d = EV[e.code] || { t: 'رویداد ' + e.code, i: 'mu', s: 'mu' };
    return '<tr><td style="white-space:nowrap">' + badge(e.ch ? 'کانال ' + fa(e.ch) : 'کل دستگاه', d.s) + '</td>' +
      '<td>' + d.t + (e.dur ? ' <span class="sub">(' + durShort(e.dur) + ')</span>' : '') + '</td>' +
      '<td class="sub" style="white-space:nowrap">' + agoStr(e.t) + '</td></tr>';
  }).join('') + '</tbody></table></div>';
}
function emptyBox(text) {
  return '<div style="padding:26px 10px;text-align:center;color:var(--mu)">' +
    '<svg viewBox="0 0 24 24" style="width:34px;height:34px;stroke:var(--mu2);fill:none;stroke-width:1.6;margin-bottom:8px"><rect x="3" y="4" width="18" height="16" rx="3"/><path d="M7 15l3-3 2 2 4-4"/></svg>' +
    '<div>' + esc(text) + '</div></div>';
}
function skeletonView() {
  return '<section class="card"><div class="sk line" style="width:40%"></div><div class="sk tile"></div></section>' +
    '<section class="grid"><div class="card"><div class="sk tile"></div></div><div class="card"><div class="sk tile"></div></div></section>';
}

/* ==================== Stats view / صفحهٔ آمار ==================== */
/* ==================== Excel report / گزارش اکسل ====================
   [EN] A dedicated section, not a button hidden in a menu: the whole history of
   the panel as ONE real .xlsx file with five sheets (summary, daily, charges,
   events, samples). The file is built on the panel itself, sheet by sheet, so
   there is no cloud, no internet and no computer in the middle. The range is
   chosen here and travels as ?days=N; "everything" sends no parameter at all.
   [FA] یک بخش جداگانه، نه دکمه‌ای پنهان در منو: کل تاریخچهٔ پنل به‌صورت یک فایل
   xlsx واقعی با پنج برگه (خلاصه، روزانه، شارژها، رویدادها، نمونه‌ها). فایل روی
   خود پنل و برگه‌به‌برگه ساخته می‌شود؛ نه ابر، نه اینترنت، نه کامپیوتر وسط. بازه
   همین‌جا انتخاب و در ?days=N فرستاده می‌شود؛ «همه» هیچ پارامتری نمی‌فرستد. */
function reportHref() {
  return API.report + (REPORT_DAYS ? '?days=' + REPORT_DAYS : '');
}
function reportRangeText() {
  return REPORT_DAYS ? 'بازهٔ ' + num(REPORT_DAYS) + ' روز گذشته' : 'همهٔ تاریخچهٔ ذخیره‌شده';
}
function reportSectionHTML() {
  if (!can('admin')) {
    return card('گزارش اکسل', ICO.dl,
      '<div class="sub">فایل اکسل با پنج برگه (خلاصه، روزانه، شارژها، رویدادها، نمونه‌ها) با حساب <b>مدیر</b> گرفته می‌شود؛ ' +
      'اگر به حساب مدیر دسترسی ندارید، از خروجی CSV و JSON همین صفحه استفاده کنید.</div>');
  }
  const choices = [[0, 'همه'], [7, '۷ روز'], [30, '۳۰ روز'], [90, '۹۰ روز']];
  let buttons = '';
  for (let i = 0; i < choices.length; i++) {
    buttons += '<button type="button" data-report-days="' + choices[i][0] + '"' +
      (REPORT_DAYS === choices[i][0] ? ' class="on"' : '') + '>' + choices[i][1] + '</button>';
  }
  return card('گزارش اکسل', ICO.dl,
    '<div class="sub">پنل خودش یک فایل <b>xlsx</b> واقعی می‌سازد - باز کردنی در اکسل، لیبره‌آفیس و گوگل‌شیت - با پنج برگه: ' +
    '<b>خلاصه</b>، <b>روزانه</b>، <b>شارژها</b>، <b>رویدادها</b> و <b>نمونه‌ها</b>. تاریخ‌ها شمسی‌اند، اعداد عدد واقعی‌اند ' +
    '(فرمول و نمودار روی‌شان کار می‌کند) و برگه‌ها از راست به چپ باز می‌شوند.</div>' +
    '<div class="row" style="margin:13px 0"><span class="seg" id="rep-seg">' + buttons + '</span>' +
    '<span class="s" id="rep-range">' + reportRangeText() + '</span></div>' +
    '<div class="row"><a class="btn" id="rep-dl" href="' + reportHref() + '" download="user_panel_report.xlsx">' +
    ICO.dl + ' دریافت فایل اکسل</a>' +
    '<a class="btn ghost" href="' + API.export + '?what=csv" download="user_panel_history.csv">خروجی CSV</a></div>' +
    '<div class="hr"></div>' +
    '<div class="sub">برگهٔ «نمونه‌ها» برای آنکه فایل روی موبایل هم باز شود تا ۴۰۰۰ ردیف آخرِ بازه را می‌آورد؛ ' +
    'بقیهٔ برگه‌ها کامل‌اند. ساخت فایل چند ثانیه طول می‌کشد و در گزارش اقدامات ثبت می‌شود.</div>');
}
function bindReport() {
  if (!$('#rep-seg')) { return; }
  $$('#rep-seg button').forEach(b => b.addEventListener('click', () => {
    REPORT_DAYS = Number(b.getAttribute('data-report-days')) || 0;
    $$('#rep-seg button').forEach(x => x.classList.toggle('on', x === b));
    const link = $('#rep-dl'), label = $('#rep-range');
    if (link) { link.setAttribute('href', reportHref()); }
    if (label) { label.textContent = reportRangeText(); }
  }));
}
function renderStats() {
  const k = (S.stats && S.stats.kpi) ? S.stats.kpi : null;
  if (!k) { $('#view').innerHTML = skeletonView(); return; }
  const sess = (S.stats.sessions || []);
  const daily = (S.stats.daily || []);
  /* [EN] Session rows are [endEpoch, done, durationSeconds] - exactly what
     /api/stats sends, so there is no index table to keep in step.
     [FA] ردیف شارژ یعنی [زمان پایان، کامل؟، مدت به ثانیه] - همان که
     /api/stats می‌فرستد، تا جدول شاخص‌ها برای هم‌خوان نگه‌داشتن لازم نباشد. */
  const dutyBars = sess.slice(-24).map(s => ({
    label: '', value: Math.round((s[2] || 0) / 60),
    color: s[1] === 1 ? '#2fd6a3' : '#f7c33c',
    tip: (s[0] ? stampStr(s[0]) + ' — ' : '') + 'شارژ ' + (s[1] === 1 ? 'کامل' : 'ناتمام') + ': ' + dur(s[2] || 0)
  }));
  /* [EN] d[0] is the first second of that day, so the axis says a real date.
     It is 0 when the panel has never been given a clock - the bar is still
     drawn, it just cannot be dated, and the page says so instead of inventing
     a day number.
     [FA] d[0] اولین ثانیهٔ آن روز است، پس محور یک تاریخ واقعی می‌گوید. وقتی
     پنل هرگز ساعت نگرفته باشد صفر است - میله باز هم کشیده می‌شود، فقط تاریخ
     نمی‌گیرد و صفحه همین را می‌گوید نه اینکه شمارهٔ روز از خودش بسازد. */
  const dayBars = daily.slice(-30).map(d => ({
    label: d[0] ? fa(new Date(d[0] * 1000).getDate()) : '—',
    value: d[1] || 0, color: '#5b9dff',
    tip: (d[0] ? jalaliStr(d[0]) + ' — ' : '') + 'شارژ کامل‌شده: ' + fa(d[1] || 0) + ' — زمان روی باتری: ' + dur(d[3] || 0)
  }));
  const inShare = (k.inputS || 0), runShare = (k.runS || 0);
  const heat = S.stats.heat || [];

  $('#view').innerHTML =
    '<section class="grid tight">' +
      kpi('تعداد شارژ کامل‌شده', num(k.charges || 0), 'طی ' + num(k.days || 0) + ' روز ثبت‌شده', 'ok', ICO.ok) +
      kpi('شارژ ناتمام', num(k.incomplete || 0), 'قطع‌شده وسط کار', 'wa', ICO.warn) +
      kpi('میانگین مدت شارژ', durShort(k.avgChargeS || 0), 'کوتاه‌ترین ' + durShort(k.minChargeS || 0), 'ac', ICO.clock) +
      kpi('بلندترین شارژ', durShort(k.maxChargeS || 0), 'میانه ' + durShort(k.medianChargeS || 0), 'ac', ICO.chart) +
      kpi('قطع‌شدن ورودی', num(k.outages || 0) + ' بار', 'مجموع قطعی ' + durShort(k.outageS || 0), 'er', ICO.plug) +
      kpi('مجموع زمان روی باتری', durShort(runShare), 'طولانی‌ترین باری ' + durShort(k.maxRunS || 0), 'wa', ICO.bat) +
      kpi('انرژی شارژشده', num((k.chargeWh10 || 0) / 10, 1) + ' Wh', 'تخمین از ولتاژ و جریان', 'vi', ICO.bolt) +
      kpi('اوج جریان', num(k.peakI1 || 0) + ' / ' + num(k.peakI2 || 0), 'کانال بالا / پایین (mA)', 'ac', ICO.chart) +
    '</section>' +

    '<section class="col-2">' +
      card('تعداد شارژ در روزهای اخیر', ICO.chart,
        '<div id="ch-days"><div class="sk tile"></div></div>' +
        '<div class="sub" style="margin-top:6px">هر میله یک روز است؛ ارتفاع آن تعداد شارژ کامل‌شدهٔ آن روز.</div>') +
      card('سهم زمان‌ها', ICO.clock,
        '<div id="ch-donut"></div>' +
        '<div class="legend"><span><i style="background:#2fd6a3"></i> ورودی وصل</span>' +
        '<span><i style="background:#f7c33c"></i> روی باتری</span></div>' +
        '<div class="sub" style="margin-top:8px">مجموع ثبت‌شده: ' + dur((inShare + runShare) || 0) + '</div>') +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.clock + '<span>مدت هر شارژ (آخرین ' + num(dutyBars.length) + ' شارژ)</span></span>' +
      '<span class="s">سبز = کامل، زرد = ناتمام</span></div>' +
      '<div id="ch-sess"><div class="sk tile"></div></div>' +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.chart + '<span>شارژ شدن در ساعت‌های شبانه‌روز</span></span>' +
      '<span class="s">ستون‌ها ساعت ۰ تا ۲۳</span></div>' +
      '<div id="ch-heat"></div>' +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.chart + '<span>روند میانگین ولتاژ در بازهٔ ثبت‌شده</span></span></div>' +
      '<div id="ch-long"><div class="sk tile"></div></div>' +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.info + '<span>ریز آمار</span></span>' +
      '<span class="s">همهٔ اعداد از حافظهٔ خود پنل محاسبه می‌شوند</span></div>' +
      '<div class="grid tight">' +
        factKV('کمترین ولتاژ دیده‌شده', num((k.minV || 0) / 1000, 1) + ' V') +
        factKV('بیشترین ولتاژ دیده‌شده', num((k.maxV || 0) / 1000, 1) + ' V') +
        factKV('اوج جریان کانال بالا', num(k.peakI1 || 0) + ' mA') +
        factKV('اوج جریان کانال پایین', num(k.peakI2 || 0) + ' mA') +
        factKV('رویدادهای عدم‌توازن', num(k.imbEvents || 0)) +
        factKV('تعداد روشن‌شدن برد', num(k.boots || 0)) +
        factKV('انرژی تخمینی مصرف‌شده', num((k.runWh10 || 0) / 10, 1) + ' Wh') +
        factKV('بازهٔ ثبت‌شده', dur(k.coverageS || 0)) +
      '</div>' +
      '<div class="hr"></div>' +
      '<div class="row"><button class="btn ghost sm" id="btn-refresh-stats" type="button">تازه‌سازی</button>' +
      (can('admin') ? '<a class="btn ghost sm" href="' + API.export + '?what=csv" download="user_panel_history.csv">خروجی CSV</a>' : '') +
      '</div>' +
    '</section>' +
    reportSectionHTML();

  chartBars($('#ch-days'), dayBars.length ? dayBars : [{ label: '', value: 0 }], { h: 200, yMax: Math.max(1, niceMax(Math.max.apply(null, dayBars.map(b => b.value).concat([1])))) });
  chartDonut($('#ch-donut'), [
    { name: 'ورودی وصل', value: inShare, color: '#2fd6a3' },
    { name: 'روی باتری', value: runShare, color: '#f7c33c' }
  ], num(Math.round((inShare / ((inShare + runShare) || 1)) * 100)) + '٪', 'روی ورودی');
  chartBars($('#ch-sess'), dutyBars.length ? dutyBars : [{ label: '', value: 0 }], { h: 190, maxBar: 34 });
  if (heat.length) {
    const rowNames = (S.stats.heatDays || []).map(d => d ? jalaliStr(d) : 'روز');
    chartHeat($('#ch-heat'), heat, { rowNames: rowNames, max: Math.max(1, k.maxChargesPerHour || 1), unitName: 'شارژ' });
  } else { $('#ch-heat').innerHTML = emptyBox('برای این نمودار هنوز داده‌ای نیست'); }
  const longSeries = (S.series && S.series.n) ? S.series : null;
  if (longSeries) {
    const step = Math.max(1, Math.floor(longSeries.n / 90));
    chartLine($('#ch-long'), {
      h: 190, series: [
        { name: 'پک ۲۴', color: '#5b9dff', data: sampleEvery(longSeries.v24.map(v => v / 1000), step) },
        { name: 'پایینی', color: '#2fd6a3', data: sampleEvery(longSeries.v12.map(v => v / 1000), step) }
      ]
    });
  } else { $('#ch-long').innerHTML = emptyBox('دادهٔ کافی نیست'); }
  const rf = $('#btn-refresh-stats');
  if (rf) { rf.addEventListener('click', () => { loadSlow(true); toast('در حال تازه‌سازی…'); }); }
  bindReport();
}
function sampleEvery(arr, step) {
  if (step <= 1) { return arr; }
  const out = [];
  for (let i = 0; i < arr.length; i += step) { out.push(arr[i]); }
  return out;
}
function kpi(title, value, desc, kind, icon) {
  return '<div class="kpi ' + (kind || '') + '"><div class="k">' + (icon || '') + '<span>' + title + '</span></div>' +
    '<div class="v">' + value + '</div><div class="d">' + desc + '</div></div>';
}

/* ==================== Diagnostics view / صفحهٔ دیاگ ==================== */
function renderDiag() {
  const L = S.live;
  const active = L ? L.faultCodes.slice() : [];
  const activeSet = {};
  active.forEach(c => { activeSet[c] = true; });
  const needReset = active.filter(c => CATALOG[c] && CATALOG[c].reset);
  const checks = healthChecks();

  const activeHTML = active.length
    ? active.map(c => faultCard(c, true)).join('<div style="height:11px"></div>')
    : '<div class="fault ok"><span class="fault-code">OK</span><div class="fault-body">' +
      '<div class="fault-title">هیچ خطایی فعال نیست</div>' +
      '<div class="fault-say">دستگاه در وضعیت عادی کار می‌کند. اگر رفتار غیرعادی دیدید، فهرست زیر را بخوانید: هر خطایی که ممکن است رخ دهد، با زبان ساده توضیح داده شده است.</div></div></div>';

  $('#view').innerHTML =
    '<section class="card' + (needReset.length ? ' ' : '') + '"' + (needReset.length ? ' style="border-color:rgba(255,101,119,.42)"' : '') + '>' +
      '<div class="head"><span class="t">' + ICO.warn + '<span>وضعیت خطاها</span></span>' +
      '<span class="s">' + (active.length ? num(active.length) + ' مورد فعال' : 'همه‌چیز سالم') + '</span></div>' +
      (needReset.length
        ? '<div class="card raise" style="margin-bottom:13px;border-color:rgba(255,101,119,.45)">' +
          '<div class="fault-title">' + badge(ICO.warn + ' نیاز به ری‌استارت برد', 'er') + '</div>' +
          '<div class="fault-say">برای برگشتن دستگاه به کار عادی، خودِ برد باید خاموش و روشن شود. تا آن لحظه، پنل عمداً هیچ دکمه‌ای برای «پاک‌کردن» این خطا نشان نمی‌دهد؛ چون روی این ارتباط، چنین فرمانی وجود ندارد و نمایش دکمه‌ای که کار نکند بدتر از نداشتن آن است.</div>' +
          '<ol class="steps"><li>باتری‌ها و ورودی را در وضعیت امن بگذارید (قطع شارژ از بخش مدیریت).</li>' +
          '<li>کلید اصلی برد را خاموش کنید و ۱۰ ثانیه صبر کنید.</li>' +
          '<li>کلید را روشن کنید و همین صفحه را تازه کنید.</li>' +
          '<li>اگر خطا برگشت، شارژ را وصل نکنید و به تعمیرکار خبر دهید.</li></ol>' +
          '<div class="hr"></div>' + resetGuideHTML() +
          '</div>'
        : '') +
      activeHTML +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.ok + '<span>بررسی سلامت بخش‌ها</span></span>' +
      '<span class="s">' + (L ? 'آخرین به‌روزرسانی: ' + agoStr((S.nowEpoch || Math.floor(Date.now() / 1000)) - 1) : '—') + '</span></div>' +
      '<div class="grid tight">' + checks.map(c =>
        '<div class="check ' + c.kind + '">' + c.icon + '<div><div class="ck-t">' + c.t + '</div><div class="ck-s">' + c.s + '</div></div></div>').join('') +
      '</div>' +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.info + '<span>فهرست کامل خطاهای ممکن (دیاگ)</span></span>' +
      '<span class="s">' + num(Object.keys(CATALOG).length) + ' کد</span></div>' +
      '<div class="sub" style="margin-bottom:10px">این فهرست مثل دیاگ خودرو است: هر کد، معنی ساده، علت‌های محتمل و کاری که باید بکنید. کدهای فعال بالای همین صفحه با حاشیهٔ رنگی می‌آیند.</div>' +
      Object.keys(CATALOG).map(c => faultCard(c, false, activeSet[c])).join('<div style="height:10px"></div>') +
    '</section>';
  const g = $('#reset-guide');
  if (g) { g.innerHTML = resetGuideHTML(); }
}

/* [EN] One catalog entry rendered for the operator: code, plain sentence,
   causes, numbered steps, and whether the board must be restarted.
   [FA] یک ورودی کاتالوگ برای اپراتور: کد، جملهٔ ساده، علت‌ها، گام‌های
   شماره‌دار و اینکه آیا برد باید ری‌استارت شود. */
function faultCard(code, live, isActive) {
  const c = CATALOG[code];
  if (!c) { return ''; }
  const kind = isActive ? c.sev : (live ? c.sev : 'mu');
  return '<div class="fault ' + kind + '">' +
    '<span class="fault-code">' + esc(code) + '</span>' +
    '<div class="fault-body">' +
      '<div class="fault-title">' + esc(c.title) +
        (isActive ? badge('فعلاً فعال', c.sev === 'er' ? 'er' : 'wa') : '') +
        (c.reset ? badge('نیاز به ری‌استارت برد', 'er') : '') +
        (c.sev === 'in' ? badge('اطلاع', 'ac') : (c.sev === 'wa' ? badge('هشدار', 'wa') : '')) +
      '</div>' +
      '<div class="fault-say">' + esc(c.say) + '</div>' +
      '<div class="fault-meta"><span>علت‌های محتمل: ' + c.causes.map(esc).join(' • ') + '</span>' +
      '<span>منبع داده: ' + esc(c.src) + '</span></div>' +
      '<ol class="steps">' + c.steps.map(s => '<li>' + esc(s) + '</li>').join('') + '</ol>' +
    '</div></div>';
}
function resetGuideHTML() {
  return '<div class="fault-title" style="margin-bottom:6px">' + badge(ICO.gear + ' راهنمای ری‌استارت', 'ac') + '</div>' +
    '<div class="fault-say">روی این دستگاه، ری‌استارت برد از راه دور ممکن نیست: خط ریست به برد نرفته و پروتکل هم فرمان ریست ندارد. پس ری‌استارت دستی انجام می‌شود:</div>' +
    '<ol class="steps"><li>اگر شارژ در جریان است، از بخش «مدیریت» هر دو کانال را قطع کنید.</li>' +
    '<li>کلید اصلی ورودی برد را خاموش کنید و ۱۰ ثانیه صبر کنید.</li>' +
    '<li>کلید را روشن کنید؛ پنل خودش دوباره داده می‌گیرد و جدول خطاها تازه می‌شود.</li>' +
    '<li>پس از روشن‌شدن، شارژرها را از بخش مدیریت وصل کنید.</li></ol>';
}
/* [EN] Health checklist: each row answers "is this part working?" in words.
   [FA] چک‌لیست سلامت: هر ردیف به زبان ساده می‌گوید آن بخش کار می‌کند یا نه. */
function healthChecks() {
  const L = S.live;
  if (!L) { return [{ kind: 'wa', icon: ICO.info, t: 'در انتظار داده', s: 'هنوز داده‌ای از برد نرسیده است' }]; }
  const t = L.t || [];
  const tw = (i) => (t[i] === undefined ? 0 : t[i]);
  const out = [];
  out.push(L.link.online
    ? { kind: 'ok', icon: ICO.ok, t: 'ارتباط با برد', s: 'داده‌ها زنده می‌رسند (' + num(Math.round((L.link.ageMs || 0) / 1000)) + ' ثانیه پیش)' }
    : { kind: 'er', icon: ICO.warn, t: 'ارتباط با برد', s: 'قطعی است؛ اعداد ممکن است کهنه باشند' });
  out.push(L.flags.measValid
    ? { kind: 'ok', icon: ICO.ok, t: 'اندازه‌گیری', s: 'خواندن ولتاژ و جریان معتبر است' }
    : { kind: 'wa', icon: ICO.info, t: 'اندازه‌گیری', s: 'در حال آماده‌سازی یا نامعتبر' });
  out.push(L.flags.input
    ? { kind: 'ok', icon: ICO.ok, t: 'ورودی ۲۴ ولت', s: 'وصل است (' + num(tw(TI.vin) / 1000, 1) + ' ولت)' }
    : { kind: 'wa', icon: ICO.bat, t: 'ورودی ۲۴ ولت', s: 'قطع است؛ دستگاه روی باتری کار می‌کند' });
  const chk = (n, st, mA) => {
    const s = CHG_STATE[st] || { t: 'نامعلوم', s: 'mu' };
    return { kind: (st === 7 || st === 8) ? 'er' : (st === 3 ? 'ok' : (st === 0 ? 'mu' : 'wa')),
      icon: (st === 7 || st === 8) ? ICO.warn : (st === 3 ? ICO.ok : ICO.bolt),
      t: 'شارژر کانال ' + n, s: s.t + ' — جریان ' + num(mA) + ' میلی‌آمپر' };
  };
  out.push(chk('۱', tw(TI.st1), tw(TI.iest1)));
  out.push(chk('۲', tw(TI.st2), tw(TI.iest2)));
  const vLow = tw(TI.vlow), vHigh = tw(TI.vhigh);
  out.push((vLow > 6000 && vHigh > 6000)
    ? { kind: 'ok', icon: ICO.bat, t: 'باتری‌ها', s: 'هر دو نیمه خوانده می‌شوند' }
    : { kind: 'er', icon: ICO.warn, t: 'باتری‌ها', s: 'یک نیمه ولتاژ معتبر ندارد' });
  out.push({ kind: (L.imb && Math.abs(L.imb.mv || 0) > 300) ? 'wa' : 'ok', icon: ICO.chart,
    t: 'توازن دو نیمه', s: 'اختلاف ' + num(Math.abs((L.imb && L.imb.mv) || 0)) + ' میلی‌ولت' });
  const st = S.hw.storage || {};
  const used = st.usedBytes || 0, total = st.totalBytes || 1;
  out.push({ kind: (used / total) > 0.92 ? 'er' : (used / total) > 0.75 ? 'wa' : 'ok', icon: ICO.info,
    t: 'حافظهٔ پنل', s: num(Math.round((used / total) * 100)) + '٪ پر شده' + (st.purging ? ' — پاک‌سازی خودکار فعال' : '') });
  out.push(L.now && L.now.wallValid
    ? { kind: 'ok', icon: ICO.clock, t: 'ساعت پنل', s: 'تنظیم است (' + panelJalaliStr(S.nowEpoch, S.nowTzMin) + ' — ' + panelClockStr(S.nowEpoch, S.nowTzMin) + ')' }
    : { kind: 'wa', icon: ICO.clock, t: 'ساعت پنل', s: 'با مرورگر تنظیم می‌شود؛ الان زمان نسبی است' });
  return out;
}

/* ==================== Admin view / صفحهٔ مدیریت ==================== */
function renderAdmin() {
  if (!can('admin')) { $('#view').innerHTML = card('دسترسی', ICO.warn, '<div class="sub">این بخش فقط برای مدیر است.</div>'); return; }
  const users = (S.users && S.users.list) ? S.users.list : [];
  const audit = (S.audit && S.audit.list) ? S.audit.list : [];
  const clock = S.clock || {};
  const ck = (clock.set && clock.epoch) ? clock : null;
  const now = deviceNow();
  /* [EN] If the clock is already set, the form starts from the PANEL's own date
     and time so the admin only tunes it; otherwise it starts from this device.
     [FA] اگر ساعت از قبل تنظیم شده باشد، فرم با تاریخ و ساعت «خود پنل» پر
     می‌شود تا مدیر فقط تنظیمش کند؛ وگرنه با ساعت این دستگاه. */
  const ckDP = (ck && ck.date && ck.date.indexOf('/') > 0) ? ck.date.split('/') : null;
  const ckTP = (ck && ck.time && ck.time.indexOf(':') > 0) ? ck.time.split(':') : null;
  const ckPre = {
    y: ckDP ? String(parseInt(ckDP[0], 10)) : String(now.jy),
    m: ckDP ? String(parseInt(ckDP[1], 10)) : String(now.jm),
    d: ckDP ? String(parseInt(ckDP[2], 10)) : String(now.jd),
    h: ckTP ? String(parseInt(ckTP[0], 10)) : String(now.h),
    i: ckTP ? String(parseInt(ckTP[1], 10)) : String(now.mi)
  };
  $('#view').innerHTML =

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.clock + '<span>ساعت پنل</span></span>' +
      '<span class="s">فقط مدیر تنظیم می‌کند</span></div>' +
      (ck
        ? '<div class="row" style="margin-bottom:10px">' + badge('تنظیم است', 'ok') +
          '<b>' + esc(ck.date) + '</b><span class="sub">— ' + esc(ck.time) + '</span>' +
          '<span class="sub">منطقهٔ ' + tzLabel(ck.tzMin) + '</span>' +
          '<span class="sub">(میلادی ' + esc(ck.gregorian) + ')</span></div>'
        : '<div class="row" style="margin-bottom:10px">' + badge('تنظیم نشده', 'wa') +
          '<span class="sub">تا وقتی ساعت ست نشود، تاریخ رویدادها و گزارش روزانه معنا ندارد.</span></div>') +
      '<div class="sub" style="margin-bottom:10px">پنل ساعت را از مرورگر نمی‌گیرد؛ ' +
        'تاریخ و ساعت را همین‌جا وارد کنید (یا با دکمهٔ اول از ساعت دستگاه خودتان پر کنید) و «ثبت» را بزنید. ' +
        'اختلاف ساعت، مرز «روز» و نمودار ساعت‌های شبانه‌روز را تعیین می‌کند. دقت تنظیم یک دقیقه است.</div>' +
      '<div class="grid tight">' +
        '<div class="field" style="margin:0"><label>سال شمسی</label><input class="inp" id="ck-y" type="number" min="1390" max="1420" value="' + ckPre.y + '"></div>' +
        '<div class="field" style="margin:0"><label>ماه</label><input class="inp" id="ck-m" type="number" min="1" max="12" value="' + ckPre.m + '"></div>' +
        '<div class="field" style="margin:0"><label>روز</label><input class="inp" id="ck-d" type="number" min="1" max="31" value="' + ckPre.d + '"></div>' +
        '<div class="field" style="margin:0"><label>ساعت</label><input class="inp" id="ck-h" type="number" min="0" max="23" value="' + ckPre.h + '"></div>' +
        '<div class="field" style="margin:0"><label>دقیقه</label><input class="inp" id="ck-i" type="number" min="0" max="59" value="' + ckPre.i + '"></div>' +
      '</div>' +
      '<div class="grid tight" style="margin-top:10px">' +
        '<div class="field" style="margin:0"><label>منطقهٔ زمانی</label><select class="inp" id="ck-tz">' +
          [[210, 'تهران ‎+۳:۳۰'], [240, 'دبی ‎+۴:۰۰'], [180, 'بغداد/استانبول ‎+۳:۰۰'], [270, 'کابل ‎+۴:۳۰'],
           [300, 'کراچی ‎+۵:۰۰'], [0, 'UTC ‎+۰:۰۰'], [-300, 'نیویورک ‎−۵:۰۰'], [-480, 'لس‌آنجلس ‎−۸:۰۰'],
           [600, 'سیدنی ‎+۱۰:۰۰']]
            .map(([v, t]) => '<option value="' + v + '"' + (((ck ? ck.tzMin : now.tzMin) === v) ? ' selected' : '') + '>' + t + '</option>').join('') +
        '</select></div>' +
        '<div class="field" style="margin:0"><label>یا اختلاف دستی (دقیقه، مثلاً 210)</label><input class="inp" id="ck-tz-manual" type="number" min="-720" max="840" placeholder="خالی = از فهرست"></div>' +
      '</div>' +
      '<div class="row" style="margin-top:12px">' +
        '<button class="btn ghost" id="ck-fill" type="button">' + ICO.clock + ' ساعت این دستگاه را بگذار</button>' +
        '<button class="btn" id="ck-save" type="button">ثبت ساعت پنل</button>' +
      '</div>' +
    '</section>' +

    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.gear + '<span>عملیات مجاز</span></span>' +
      '<span class="s">فقط کارهایی که این ارتباط واقعاً پشتیبانی می‌کند</span></div>' +
      '<div class="sub" style="margin-bottom:12px">قطع و وصل شارژرها، تنها فرمانی است که از پنل به برد می‌رود. هر عملیات در گزارش اقدامات ثبت می‌شود.</div>' +
      '<div class="grid tight">' +
        opBtn('charger1_off', 'قطع شارژر بالایی', 'er', ICO.warn) +
        opBtn('charger1_on', 'وصل شارژر بالایی', 'ok', ICO.bolt) +
        opBtn('charger2_off', 'قطع شارژر پایینی', 'er', ICO.warn) +
        opBtn('charger2_on', 'وصل شارژر پایینی', 'ok', ICO.bolt) +
      '</div>' +
      '<div class="hr"></div>' +
      '<div class="grid tight">' +
        opBtn('purge', 'پاک‌کردن تاریخچهٔ پنل', 'wa', ICO.trash) +
        '<a class="btn ghost" href="' + API.export + '?what=csv" download="user_panel_history.csv">' + ICO.dl + ' خروجی CSV</a>' +
        '<a class="btn ghost" href="' + API.export + '?what=json" download="user_panel_history.json">' + ICO.dl + ' خروجی JSON</a>' +
        '<a class="btn ghost" href="' + API.report + '" download="user_panel_report.xlsx">' + ICO.dl + ' گزارش اکسل</a>' +
      '</div>' +
      '<div class="hr"></div>' +
      resetGuideHTML() +
    '</section>' +

    '<section class="grid wide">' +
      card('کاربران', ICO.user,
        '<div class="tw"><table><thead><tr><th>کاربر</th><th>نقش</th><th>آخرین ورود</th><th></th></tr></thead><tbody>' +
        users.map(u =>
          '<tr><td><b>' + esc(u[0]) + '</b>' + (u[4] ? ' ' + badge('باید رمز عوض کند', 'wa') : '') + '</td>' +
          '<td>' + roleBadge(u[1]) + '</td>' +
          '<td class="sub">' + (u[3] ? agoStr(u[3]) : '—') + '</td>' +
          '<td style="white-space:nowrap">' +
            '<button class="btn ghost sm" data-u-role="' + esc(u[0]) + '">نقش</button> ' +
            '<button class="btn ghost sm" data-u-pass="' + esc(u[0]) + '">رمز</button> ' +
            (u[0] === 'admin' ? '' : '<button class="btn ghost sm" data-u-del="' + esc(u[0]) + '">حذف</button>') +
          '</td></tr>').join('') +
        '</tbody></table></div>' +
        '<div class="hr"></div>' +
        '<div class="grid tight">' +
          '<div class="field" style="margin:0"><label>نام کاربر جدید</label><input class="inp" id="nu-name" placeholder="مثلاً operator1"></div>' +
          '<div class="field" style="margin:0"><label>نقش</label><select class="inp" id="nu-role">' +
            '<option value="viewer">بیننده — فقط نمایش</option>' +
            '<option value="operator">اپراتور — نمایش + قطع/وصل شارژر</option>' +
            '<option value="admin">مدیر — همه‌چیز</option></select></div>' +
          '<div class="field" style="margin:0"><label>گذرواژهٔ اولیه</label><input class="inp" id="nu-pass" placeholder="حداقل ۶ نویسه"></div>' +
        '</div>' +
        '<div class="row" style="margin-top:10px"><button class="btn" id="nu-add" type="button">افزودن کاربر</button>' +
        '<span class="sub">کاربر جدید در اولین ورود باید گذرواژه را عوض کند.</span></div>') +
      card('گزارش اقدامات', ICO.info,
        (audit.length ? '<div class="tw"><table><thead><tr><th>زمان</th><th>کاربر</th><th>اقدام</th></tr></thead><tbody>' +
          audit.map(a => '<tr><td class="sub" style="white-space:nowrap">' + agoStr(a[0]) + '</td><td><b>' + esc(a[1]) + '</b></td><td>' + esc(a[2]) + '</td></tr>').join('') +
          '</tbody></table></div>' : emptyBox('هنوز اقدامی ثبت نشده است')), { cls: 'flat' }) +
    '</section>' +

    '<section class="card flat">' +
      '<div class="head"><span class="t">' + ICO.info + '<span>تنظیمات نمایش</span></span></div>' +
      '<div class="grid tight">' +
        '<div class="field" style="margin:0"><label>نمایش اعداد</label><select class="inp" id="set-digits">' +
          '<option value="fa">فارسی (۱۲۳)</option><option value="lat">لاتین (123)</option></select></div>' +
        '<div class="field" style="margin:0"><label>سرعت تازه‌سازی داشبورد</label><select class="inp" id="set-rate">' +
          '<option value="1000">سریع (۱ ثانیه)</option><option value="2000">معمولی (۲ ثانیه)</option>' +
          '<option value="5000">کم‌مصرف (۵ ثانیه)</option></select></div>' +
      '</div>' +
    '</section>';
  bindAdmin();
}
function opBtn(action, label, kind, icon) {
  return '<button class="btn ' + (kind === 'er' ? 'er' : (kind === 'ok' ? 'ok' : 'ghost')) + '" data-op="' + action + '">' +
    (icon || '') + ' ' + label + '</button>';
}
function roleBadge(role) {
  const m = { admin: ['مدیر', 'er'], operator: ['اپراتور', 'wa'], viewer: ['بیننده', 'ac'] }[role] || ['نامعلوم', 'mu'];
  return badge(m[0], m[1]);
}
function bindAdmin() {
  $$('#view [data-op]').forEach(b => b.addEventListener('click', () => adminAction(b.getAttribute('data-op'))));
  const ckFill = $('#ck-fill');
  if (ckFill) { ckFill.addEventListener('click', fillClockFromDevice); }
  const ckSave = $('#ck-save');
  if (ckSave) { ckSave.addEventListener('click', saveClock); }
  $$('#view [data-u-del]').forEach(b => b.addEventListener('click', () => {
    const u = b.getAttribute('data-u-del');
    confirmDanger('حذف کاربر', '<p>کاربر <b>' + esc(u) + '</b> حذف شود؟</p>', 'حذف کن', async () => {
      await userOp({ action: 'del', u: u }); toast('کاربر حذف شد', 'ok'); loadAdmin();
    });
  }));
  $$('#view [data-u-pass]').forEach(b => b.addEventListener('click', () => {
    const u = b.getAttribute('data-u-pass');
    confirmBox('گذرواژهٔ جدید برای ' + u,
      '<div class="field"><label>گذرواژهٔ جدید</label><input class="inp" id="rp" type="password"></div>' +
      '<div class="field"><label>تکرار</label><input class="inp" id="rp2" type="password"></div>' +
      '<div class="sub">کاربر باید در ورود بعدی رمز را عوض کند.</div>', 'ثبت', async () => {
        const p = $('#rp').value;
        if (p.length < 6) { toast('گذرواژه کوتاه است', 'er'); return; }
        if (p !== $('#rp2').value) { toast('دو گذرواژه یکسان نیستند', 'er'); return; }
        await userOp({ action: 'pass', u: u, p: p }); toast('گذرواژه ثبت شد', 'ok'); loadAdmin();
      });
  }));
  $$('#view [data-u-role]').forEach(b => b.addEventListener('click', () => {
    const u = b.getAttribute('data-u-role');
    confirmBox('نقش کاربر ' + u,
      '<div class="field"><label>نقش جدید</label><select class="inp" id="rr">' +
      '<option value="viewer">بیننده — فقط نمایش</option>' +
      '<option value="operator">اپراتور — نمایش + قطع/وصل شارژر</option>' +
      '<option value="admin">مدیر — همه‌چیز</option></select></div>', 'ثبت', async () => {
        await userOp({ action: 'role', u: u, r: $('#rr').value }); toast('نقش تغییر کرد', 'ok'); loadAdmin();
      });
  }));
  const add = $('#nu-add');
  if (add) { add.addEventListener('click', async () => {
    const n = $('#nu-name').value.trim(), p = $('#nu-pass').value, r = $('#nu-role').value;
    if (n.length < 3) { toast('نام کاربری کوتاه است', 'er'); return; }
    if (p.length < 6) { toast('گذرواژه کوتاه است', 'er'); return; }
    await userOp({ action: 'add', u: n, p: p, r: r });
    toast('کاربر افزوده شد', 'ok'); loadAdmin();
  }); }
  const sd = $('#set-digits'), sr = $('#set-rate');
  if (sd) { sd.value = LATIN_DIGITS ? 'lat' : 'fa'; sd.addEventListener('change', () => { LATIN_DIGITS = (sd.value === 'lat'); localStorage.setItem('up_digits', sd.value); render(); }); }
  if (sr) { sr.value = String(REFRESH.liveMs); sr.addEventListener('change', () => { localStorage.setItem('up_rate', sr.value); location.reload(); }); }
}
async function userOp(obj) {
  try { const r = await apiPost(API.userOp, obj); if (!r.ok) { toast(r.err || 'انجام نشد', 'er'); } return r; }
  catch (e) { toast(e.message, 'er'); return { ok: 0 }; }
}
function adminAction(action) {
  const names = { charger1_off: 'قطع شارژر بالایی', charger1_on: 'وصل شارژر بالایی',
                  charger2_off: 'قطع شارژر پایینی', charger2_on: 'وصل شارژر پایینی', purge: 'پاک‌کردن تاریخچهٔ پنل' };
  const isPurge = (action === 'purge');
  const html = '<p>' + esc(names[action] || action) + ' انجام شود؟</p>' +
    (action.endsWith('_off') ? '<p class="sub">با قطع شارژ، باتری‌ها آسیب نمی‌بینند؛ شارژ بعداً از همین صفحه وصل می‌شود.</p>' : '') +
    (isPurge ? '<p class="sub">همهٔ نمونه‌ها و رویدادهای ثبت‌شده پاک می‌شوند. این کار برگشت‌پذیر نیست؛ اگر لازم است اول خروجی بگیرید.</p>' : '');
  const ask = action.endsWith('_off') || isPurge ? confirmDanger : confirmBox;
  /* [EN] cs = confirmation stamp, made once per dialog and sent with the
     action. The panel remembers the last one it honoured and refuses a repeat,
     so a double tap (or a replayed request) cannot cut a charger twice.
     [FA] cs = مهر تأیید، یک‌بار برای هر پنجره ساخته می‌شود و همراه اقدام
     می‌رود. پنل آخرین مهر پذیرفته‌شده را یادش می‌ماند و تکرار را رد می‌کند،
     تا دوبار زدن (یا تکرار درخواست) نتواند شارژر را دو بار قطع کند. */
  const cs = String(Date.now()) + '-' + Math.random().toString(36).slice(2, 8);
  ask('تأیید عملیات', html, 'انجام بده', async () => {
    try {
      const r = await apiPost(API.action, { a: action, cs: cs });
      if (r.ok) { toast('انجام شد', 'ok'); loadSlow(true); }
      else { toast(r.err || 'انجام نشد', 'er'); }
    } catch (e) { toast(e.message, 'er'); }
  });
}
async function loadAdmin() {
  if (!can('admin')) { return; }
  try {
    const [u, a, c] = await Promise.all([api(API.users), api(API.audit + '?limit=40'), api(API.adminClock)]);
    S.users = { list: u.u || [] }; S.audit = { list: a.a || [] }; S.clock = c || {};
    if (S.view === 'admin') { renderAdmin(); }
  } catch (e) { /* [EN] keep the last view / [FA] نمای قبلی می‌ماند */ }
}

/* ==================== Help view / راهنما ==================== */
function renderHelp() {
  $('#view').innerHTML =
    '<section class="card">' +
      '<div class="head"><span class="t">' + ICO.info + '<span>این پنل چه کاری می‌کند؟</span></span></div>' +
      '<div class="fault-say">این صفحه فقط <b>نمایش</b> است: وضعیت زندهٔ دستگاه، آمار شارژ و باتری، و دیاگ خطاها. هیچ تنظیماتی از این‌جا عوض نمی‌شود؛ فقط مدیر می‌تواند شارژرها را قطع/وصل کند و کاربران را مدیریت کند.</div>' +
      '<ol class="steps">' +
        '<li><b>داشبورد:</b> همین حالا روی باتری هستیم یا روی ورودی؟ باتری‌ها چقدر پر هستند؟ روند ۲۴ ساعت گذشته چه شکلی است؟</li>' +
        '<li><b>آمار و ارقام:</b> چند بار شارژ شده، هر شارژ چقدر طول کشیده، چند بار ورودی قطع شده، چقدر انرژی جابه‌جا شده.</li>' +
        '<li><b>دیاگ خطاها:</b> همهٔ خطاهای ممکن با زبان ساده، علت و راه‌حل — مثل دیاگ خودرو.</li>' +
        '<li><b>مدیریت:</b> کاربران، گزارش اقدامات، قطع/وصل شارژر، خروجی گرفتن از تاریخچه.</li>' +
      '</ol>' +
    '</section>' +
    '<section class="grid wide">' +
      card('رنگ‌ها چه می‌گویند؟', ICO.chart,
        '<div class="grid tight">' +
          '<div class="check ok">' + ICO.ok + '<div><div class="ck-t">سبز</div><div class="ck-s">سالم، کامل، یا ورودی وصل</div></div></div>' +
          '<div class="check wa">' + ICO.warn + '<div><div class="ck-t">زرد</div><div class="ck-s">هشدار یا در جریان بودن کار</div></div></div>' +
          '<div class="check er">' + ICO.warn + '<div><div class="ck-t">قرمز</div><div class="ck-s">خطای فعال یا قطع ارتباط</div></div></div>' +
          '<div class="check">' + ICO.info + '<div><div class="ck-t">بنفش</div><div class="ck-s">وضعیت تعمیرات یا عدم‌توازن</div></div></div>' +
        '</div>') +
      card('چطور وصل شوم؟', ICO.link,
        '<ol class="steps">' +
          '<li>با گوشی یا لپ‌تاپ به وای‌فای پنل وصل شوید.</li>' +
          '<li>در مرورگر آدرس پنل را باز کنید.</li>' +
          '<li>با نام کاربری و گذرواژه‌ای که مدیر داده وارد شوید.</li>' +
          '<li>اولین بار، گذرواژهٔ پیش‌فرض را عوض کنید.</li>' +
        '</ol>' +
        '<div class="hr"></div>' +
        '<div class="sub">اگر «ارتباط با برد قطع است» دیدید یعنی پنل به برد نمی‌رسد: چراغ برد، فیوز و سیم ارتباطی را چک کنید.</div>') +
      card('ساعت پنل', ICO.clock,
        '<div class="fault-say">برد ساعت تقویمی ندارد، پس پنل ساعت را از مرورگر شما می‌گیرد. اگر هیچ مرورگری باز نباشد، زمان رویدادها نسبی ثبت می‌شود («۴ دقیقه پیش»).</div>') +
      card('درباره', ICO.info,
        '<div class="grid tight">' +
          factKV('نام پنل', 'پنل کاربر ChangeOver') +
          factKV('نسخه', esc(S.hw.version.fw || '—')) +
          factKV('شناسهٔ ساخت', '<span class="mono">' + esc(S.hw.version.build || '—') + '</span>') +
          factKV('تاریخچه', 'روی حافظهٔ خود پنل') +
        '</div>') +
    '</section>';
}

/* ==================== Render + router / رندر و مسیریابی ==================== */
function render() {
  if (AUTH_FAILED || !S.me) { return; }
  $$('#nav button').forEach(b => {
    const v = b.getAttribute('data-view');
    b.classList.toggle('a', v === S.view);
    if (v === 'admin') { b.hidden = !can('admin'); }
  });
  const navAdmin = $('#nav button[data-admin]');
  if (navAdmin) {
    const n = (S.live && S.live.faultCodes ? S.live.faultCodes.length : 0);
    navAdmin.classList.toggle('hot', n > 0);
  }
  switch (S.view) {
    case 'stats': renderStats(); break;
    case 'diag': renderDiag(); break;
    case 'admin': renderAdmin(); break;
    case 'help': renderHelp(); break;
    default: renderDash(); break;
  }
}
function setView(v) {
  if (v === 'admin' && !can('admin')) { v = 'dash'; }
  S.view = v;
  location.hash = '#' + v;
  render();
  window.scrollTo({ top: 0, behavior: 'smooth' });
  say('بخش ' + v + ' باز شد');
}
function renderFoot() {
  $('#ftr').hidden = false;
  $('#ftr-build').textContent = 'شناسهٔ ساخت: ' + (S.hw.version.build || '—');
  $('#ftr-link').textContent = 'دادهٔ ' + (S.live && S.live.link.online ? 'زنده' : 'کهنه') + ' — ' + (S.live ? num(Math.round((S.live.link.ageMs || 0) / 1000)) + ' ثانیه پیش' : '—');
  $('#user-name').textContent = S.me.user;
  $('#user-role').textContent = ({ admin: 'مدیر', operator: 'اپراتور', viewer: 'بیننده' })[S.me.role] || '';
  const f = $('#ftr-logout');
  if (f && !f.dataset.bound) { f.dataset.bound = '1'; f.addEventListener('click', async () => { try { await apiPost(API.logout, {}); } catch (e) { /* ignore */ } S.me = null; showLogin('از حساب خارج شدید'); }); }
}

/* ==================== Data loading / بارگذاری داده ==================== */
function uptimeSec() { return S.live && S.live.now ? S.live.now.uptimeS : 0; }
async function loadLive() {
  const L = await api(API.live);
  S.live = L;
  /* [EN] The storage card and the diagnostics checklist both read
     S.hw.storage; /api/live is where those numbers arrive, so they are copied
     here instead of each view guessing a different source.
     [FA] کارت حافظه و چک‌لیست دیاگ هر دو S.hw.storage را می‌خوانند؛ این
     اعداد در /api/live می‌آیند، پس همین‌جا کپی می‌شوند تا هر نما منبع
     دیگری حدس نزند. */
  S.hw.storage = (L && L.store) ? L.store : {};
  /* [EN] `nowEpoch` is the PANEL's time (it is used to say how long ago an
     event happened), and it is only meaningful once the admin has set the
     clock; `nowTzMin` is the panel's own offset. Neither comes from the viewer.
     [FA] `nowEpoch` ساعت «پنل» است (برای گفتن اینکه رویداد چند وقت پیش رخ
     داده) و فقط وقتی معنا دارد که مدیر ساعت را ست کرده باشد؛ `nowTzMin` هم
     اختلاف خود پنل است. هیچ‌کدام از دستگاه بیننده نمی‌آید. */
  S.nowEpoch = (L.now && L.now.wallValid) ? L.now.wall : 0;
  S.nowTzMin = (L.now && L.now.tzMin !== undefined) ? L.now.tzMin : 0;
  const t = L.t || [];
  const tw = (i) => (t[i] === undefined ? 0 : t[i]);
  S.ring.push({
    t: uptimeSec(), vin: tw(TI.vin), v24: tw(TI.v24), v12: tw(TI.v12),
    i1: tw(TI.iest1), i2: tw(TI.iest2), d1: tw(TI.duty1), d2: tw(TI.duty2)
  });
  if (S.ring.length > REFRESH.ringMax) { S.ring.shift(); }
  paintHeader(L);
  if (S.view === 'dash') { renderDash(); }
  else if (S.view === 'diag') { renderDiag(); }
  renderFoot();
}
async function loadSlow(force) {
  const [se, ev, st] = await Promise.all([
    api(API.series + '?hours=24&points=120'),
    api(API.events + '?limit=60'),
    api(API.stats + '?days=30')
  ]);
  S.series = se; S.events = { list: ev.e || [] }; S.stats = st;
  if (force) { render(); }
  else if (S.view === 'stats' || S.view === 'dash') { render(); }
}
function paintHeader(L) {
  const chip = $('#link-chip'), txt = $('#link-text');
  const online = L.link.online;
  const stale = (L.link.ageMs || 0) > REFRESH.linkStaleMs;
  chip.classList.toggle('on', online && !stale);
  chip.classList.toggle('warn', online && stale);
  txt.textContent = !online ? 'ارتباط با برد قطع' : (stale ? 'داده‌ها دیر می‌رسند' : 'داده زنده');
  $('#clock-text').textContent = (L.now && L.now.wallValid)
    ? panelJalaliStr(S.nowEpoch, S.nowTzMin) + ' — ' + panelClockStr(S.nowEpoch, S.nowTzMin)
    : 'ساعت تنظیم نشده';
  $('#clock-text').title = (L.now && L.now.wallValid)
    ? 'ساعت خود پنل (منطقهٔ ' + tzLabel(S.nowTzMin) + ')'
    : 'مدیر باید از بخش مدیریت ساعت پنل را تنظیم کند';
}
/* [EN] Every error the panel can answer with, in words. The device sends a
   short ASCII token and the page translates it, so the firmware never carries
   Persian text for an error path.
   [FA] هر خطایی که پنل می‌تواند برگرداند، با کلمات. دستگاه یک توکن کوتاه
   ASCII می‌فرستد و صفحه ترجمه‌اش می‌کند، تا فرم‌ور هیچ‌وقت برای مسیر خطا متن
   فارسی حمل نکند. */
function errFa(err) {
  return ({
    'missing': 'فیلدهای لازم پر نشده است',
    'implausible': 'تاریخ بیرون از محدودهٔ معقول است (۲۰۲۰ تا ۲۰۳۳)',
    'bad tz': 'اختلاف ساعت نامعتبر است',
    'auth': 'ابتدا وارد شوید',
    'forbidden': 'اجازهٔ این کار را ندارید',
    'change password': 'اول گذرواژهٔ خود را عوض کنید',
    'already done': 'این عمل همین حالا انجام شده بود',
    'unknown action': 'این عمل شناخته نشد',
    'unknown user': 'کاربر پیدا نشد',
    'exists': 'چنین کاربری از قبل هست',
    'bad name': 'نام کاربری نامعتبر است (۳ تا ۱۲ نویسه: حرف، رقم، نقطه، خط تیره)',
    'bad role': 'نقش نامعتبر است',
    'password too short': 'گذرواژه باید دست‌کم ۶ نویسه باشد',
    'wrong password': 'گذرواژهٔ فعلی درست نیست',
    'table full': 'ظرفیت کاربران پر است (۸ نفر)',
    'cannot delete yourself': 'حساب خودتان را نمی‌توانید حذف کنید',
    'last admin': 'آخرین مدیر را نمی‌توان حذف یا تنزل داد',
    'delete failed': 'حذف انجام نشد',
    'bad login': 'نام کاربری یا گذرواژه درست نیست',
    'range': 'بازهٔ درخواستی نامعتبر است',
    'reply too big': 'پاسخ از حافظهٔ پنل بزرگ‌تر شد'
  })[err] || ('خطای ' + err);
}

/* ==================== The panel's clock (admin) / ساعت پنل (مدیر) ==================== */
/* [EN] The admin's own device time, offered as a convenient starting point for
   the form - never applied on its own.
   [FA] ساعت دستگاه خود مدیر، به‌عنوان نقطهٔ شروع راحت برای فرم - هرگز خودش
   اعمال نمی‌شود. */
function deviceNow() {
  const d = new Date();
  const j = jalaliOf(d.getFullYear(), d.getMonth() + 1, d.getDate());
  return { jy: j.y, jm: j.m, jd: j.d, h: d.getHours(), mi: d.getMinutes(), tzMin: -d.getTimezoneOffset() };
}
async function saveClock() {
  const jy = parseInt($('#ck-y').value, 10), jm = parseInt($('#ck-m').value, 10), jd = parseInt($('#ck-d').value, 10);
  const hh = parseInt($('#ck-h').value, 10), mi = parseInt($('#ck-i').value, 10);
  const manual = $('#ck-tz-manual').value;
  const tzMin = (manual !== '') ? parseInt(manual, 10) : parseInt($('#ck-tz').value, 10);
  if (!jy || !jm || !jd || isNaN(hh) || isNaN(mi)) { toast('تاریخ و ساعت را کامل وارد کنید', 'er'); return; }
  if (jm < 1 || jm > 12 || jd < 1 || jd > 31 || hh < 0 || hh > 23 || mi < 0 || mi > 59) { toast('تاریخ یا ساعت بیرون از محدوده است', 'er'); return; }
  if (isNaN(tzMin) || tzMin < -720 || tzMin > 840) { toast('اختلاف ساعت باید بین ‎-۷۲۰ و ‎+۸۴۰ دقیقه باشد', 'er'); return; }
  try {
    const r = await apiPost(API.clock, { t: epochFromJalali(jy, jm, jd, hh, mi, tzMin), tz: tzMin });
    if (r.ok) { toast('ساعت پنل ثبت شد: ' + r.date + ' ' + r.time, 'ok'); loadLive().catch(() => {}); loadAdmin(); }
    else { toast(errFa(r.err), 'er'); }
  } catch (e) { toast(e.message, 'er'); }
}
function fillClockFromDevice() {
  const n = deviceNow();
  $('#ck-y').value = n.jy; $('#ck-m').value = n.jm; $('#ck-d').value = n.jd;
  $('#ck-h').value = n.h; $('#ck-i').value = n.mi;
  const opt = Array.prototype.slice.call($('#ck-tz').options).filter(o => parseInt(o.value, 10) === n.tzMin)[0];
  if (opt) { opt.selected = true; $('#ck-tz-manual').value = ''; }
  else { $('#ck-tz-manual').value = n.tzMin; }
  toast('تاریخ و ساعت این دستگاه در فرم گذاشته شد — حالا «ثبت» را بزنید', 'ok');
}
function startPolling() {
  stopPolling();
  S.timer = setInterval(() => { loadLive().catch(onPollError); }, REFRESH.liveMs);
  S.slowTimer = setInterval(() => { loadSlow(false).catch(() => {}); }, REFRESH.slowMs);
}
function stopPolling() {
  if (S.timer) { clearInterval(S.timer); S.timer = null; }
  if (S.slowTimer) { clearInterval(S.slowTimer); S.slowTimer = null; }
}
let errCount = 0;
function onPollError(e) {
  errCount++;
  if (errCount === 3) { toast('ارتباط با برد برقرار نشد', 'er'); }
  if (S.live) { S.live.link.online = false; paintHeader(S.live); }
}

/* ==================== Boot / راه‌اندازی ==================== */
async function boot() {
  const hv = await api('/version').catch(() => ({}));
  S.hw.version = hv || {};
  $('#hdr').hidden = false; $('#nav').hidden = false; $('#ftr').hidden = false;
  renderFoot();
  await loadLive().catch(() => {});
  await loadSlow(true).catch(() => {});
  if (can('admin')) { loadAdmin(); }
  startPolling();
  render();
}
async function main() {
  const saved = localStorage.getItem('up_digits');
  LATIN_DIGITS = (saved === 'lat');
  const rate = parseInt(localStorage.getItem('up_rate') || '0', 10);
  if (rate >= 1000 && rate <= 10000) { REFRESH.liveMs = rate; }

  $$('#nav button').forEach(b => b.addEventListener('click', () => setView(b.getAttribute('data-view'))));
  $('#user-chip').addEventListener('click', () => setView('admin'));
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) { stopPolling(); }
    else if (S.me) { loadLive().catch(() => {}); loadSlow(false).catch(() => {}); startPolling(); }
  });
  window.addEventListener('hashchange', () => {
    const v = (location.hash || '#dash').slice(1);
    if (v && v !== S.view) { S.view = v; render(); }
  });
  /* [EN] No clock heartbeat any more - see saveClock().
     [FA] دیگر ضربان ساعت نداریم - saveClock() را ببینید. */

  try {
    const me = await api(API.me);
    if (me && me.user) {
      S.me = { user: me.user, role: me.role, mustChange: !!me.must };
      await boot();
      if (me.must) { setTimeout(() => askChangePassword(true), 600); }
      return;
    }
  } catch (e) { /* [EN] 401 → login screen / [FA] ۴۰۱ یعنی صفحهٔ ورود */ }
  showLogin('');
}
document.addEventListener('DOMContentLoaded', main);
window.addEventListener('beforeunload', stopPolling);
