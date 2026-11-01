# نقشه‌راه Contributor برای قراردادها و مرزبندی بردها

## وضعیت فعلی

سه قرارداد اکنون جداگانه و کنار پروژهٔ مربوطه قرار دارند:

- [`ESP_USER/CONTRACT.md`](../ESP_USER/CONTRACT.md)
- [`ESP_Calibration/CONTRACT.md`](../ESP_Calibration/CONTRACT.md)
- [`Firmware/CONTRACT.md`](../Firmware/CONTRACT.md)

این‌ها سه نقش مستقل دارند: دو ESP برای کاربردهای جدا هستند و هم‌زمان به STM32 وصل نمی‌شوند. `Calibration Data/` دادهٔ مشترک کالیبراسیون است، نه ریشهٔ مالکیت یک برد. فایل `tools/project_boundaries.json` فقط مرزبندی ریشه‌های سطح اول را اعلام می‌کند.

در ممیزی فعلی، routeهای ثبت‌شده در دو اسکچ ESP با فهرست ماشینی قراردادها و ثابت‌های UART قفل‌شده با تعریف منبع تطبیق داده شدند. نقص ادعای قبلی «مالکیت کامل پوشه‌ها» اصلاح شد: checker اکنون همهٔ ورودی‌های بلافاصله زیر ریشهٔ مخزن را طبقه‌بندی می‌کند، اما مالکیت تک‌تک فایل‌های داخل پوشه‌ها را اثبات نمی‌کند. همچنین README صریحاً می‌گوید checker به‌تنهایی گیت PR یا قفل تغییر پروتکل نیست.

اصلاحات قراردادی مهم: reset-slotهای 200–204 در مسیر `SET_PARAM` مستند شدند؛ نگاشت ADC/pin و قطبیت چند ورودی/خروجی در قرارداد STM32 دقیق‌تر شد؛ رفتار احراز هویت واقعی پنل Calibration ثبت شد. هیچ کد اجرایی برد یا فایل داخلی `Firmware/Modules/` در این دور تغییر نکرده است.

## آنچه Contributor باید اجرا و تحویل دهد

### مرحلهٔ 1 — بازبینی read-only و اثبات سازگاری

1. ابتدا `git status --short --untracked-files=all` را ثبت کند و هیچ فایل محلیِ نامرتبط را پاک، revert، stage یا بازنویسی نکند.
2. هر سه قرارداد را با کد فعلی و `ESP_AGENT_SPEC.md` تطبیق دهد؛ اختلاف‌ها را با مسیر فایل و شمارهٔ خط ثبت کند، نه با حدس.
3. این اجراها را انجام دهد و خروجی/نسخهٔ ابزار را به PR پیوست کند:

   ```sh
   python3 tools/check_project_contract.py
   python3 -m unittest tools/test_check_project_contract.py
   bash ESP_USER/Tester/run_simple_tests.sh
   bash ESP_Calibration/Tester/run_esp_tests.sh
   bash tools/check_firmware_syntax.sh
   python3 tools/audit_consistency.py
   bash tools/check_ai_rules.sh
   python3 tools/strip_fa_comments.py --check
   ```

4. تست‌های DOM که به `jsdom` نیاز دارند جداگانه اجرا شوند؛ اگر dependency نصب نیست، آن‌ها را «skip / اجرا نشده» گزارش کند، نه PASS. تست host شبیه‌ساز، آزمون رادیو/فلش یا تأیید سخت‌افزار واقعی محسوب نمی‌شود.
5. قراردادهای HTTP را route به route بررسی کند: method/path، query/body، احراز هویت واقعی، status/content type و شکل پاسخ؛ قرارداد فعلیِ checker فقط routeها را خودکار مقایسه می‌کند، نه جزئیات این جدول‌ها.
6. ذخیره‌سازی LittleFS را در برابر open/read/write/remove، نام فایل، سیاست مهاجرت/legacy و سقف‌های گزارش‌شده بررسی کند. حدهای benchlog و LUT را با source تأیید کند؛ هر حدِ تأییدنشده را به‌عنوان سؤال باز گزارش کند.
7. payload هر پیام UART را با جهت، نوع، طول، ترتیب بایت، endian، CRC و شرایط رد/ACK مقایسه کند. `CAL_REFERENCE (0x03)` را فرمان فعال معرفی نکند؛ در مستندات فعلی رزروشده/بدون handler است.

### مرحلهٔ 2 — بازبینی فیزیکی و آزمون مرزی

1. نقشهٔ UART، ADC، ورودی دیجیتال، PWM، enable، رله و حفاظت را با `board_pins.h`، BSP، CubeMX/CubeIDE و شماتیک تطبیق دهد.
2. ترتیب ADC و polarity سیگنال‌ها را جداگانه گزارش کند؛ برای pin map نهایی، تأیید شماتیک/مالک سخت‌افزار لازم است.
3. اگر برد واقعی در دسترس نیست، آزمون bench را «مانده» ثبت کند. host test و static audit جای آزمون سیم‌کشی، UART فیزیکی یا رفتار بعد از قطع برق را نمی‌گیرند.
4. هر آزمون cross-board را با یک ESP در هر نوبت انجام دهد؛ ESP_USER و ESP_Calibration هم‌زمان وصل نشوند.

### مرحلهٔ 3 — تصمیم‌هایی که Contributor حق تأیید یک‌طرفهٔ آن‌ها را ندارد

Contributor باید سؤال/پیشنهاد ارائه کند و تا تأیید مالک پروتکل یا سخت‌افزار، فقط مستند کند؛ نباید خودسرانه تغییر اجرایی دهد:

- تغییر شناسه/معنی پیام UART، payload، version، CRC، endian، baud، role یا پارامترهای reset-slot؛
- تغییر route عمومی/امنیتی، احراز هویت، رفتار destructive، ساختار ذخیره‌سازی یا سقف‌های فایل/داده؛
- تغییر pin map، polarity الکتریکی یا رفتار خروجی‌های ایمنی؛
- هر تغییر firmware، به‌ویژه `Firmware/Modules/` که در این درخواست خارج از scope است؛
- انتقال مالکیت `Calibration Data/` یا تغییر نحوهٔ استفاده از داده‌های آن؛
- تغییر credentialهای AP یا اضافه‌کردن authentication. قرارداد فعلی، کلید ثابت پیش‌فرض `123456789` و نبودِ app-level login در ESP_Calibration را ثبت می‌کند؛ این‌ها یافتهٔ رفتار فعلی‌اند، نه تأیید امنیتی یا مجوز تغییر.

تغییر مستندات، checker و تست‌های خود checker مجاز است، به شرط آن‌که قرارداد از source جدا یا مبهم نشود و هیچ تغییر firmware به‌صورت ضمنی وارد PR نگردد.

### مرحلهٔ 4 — قفل‌کردن در CI و تنظیمات مخزن

این بخش هنوز از سمت مخزن/تنظیمات GitHub تکمیل نشده است و باید جداگانه انجام شود:

1. یک workflow CI اضافه شود که checker، تست‌های regression آن، تست‌های ESP، firmware syntax/host suite، consistency audit و قواعد مخزن را اجرا کند. شکست هر مرحله باید وضعیت ناموفق بدهد. چون `.github/` اکنون در ریشه وجود ندارد و checker همهٔ ورودی‌های ریشه را طبقه‌بندی می‌کند، هنگام ساخت workflow باید `.github` به‌عنوان پشتیبانی shared در `outside_directories` ثبت شود؛ آزمون checker هم باید پس از این تغییر سبز بماند.
2. همان status check در GitHub به‌عنوان **required** تنظیم شود و branch protection ادغام بدون آن را ببندد. صرف وجود workflow یا اجرای محلی این الزام را ایجاد نمی‌کند.
3. `CODEOWNERS` برای سه قرارداد، manifest/checker، routeها و فایل‌های تعریف interface تعیین شود؛ سپس در branch protection، تأیید مالک برای این مسیرها الزامی شود. فایل CODEOWNERS به‌تنهایی بدون تنظیمات required review قفل ایجاد نمی‌کند.
4. هر PR که هم‌زمان manifest و source/contract را تغییر می‌دهد باید review مالک protocol داشته باشد؛ checker repository-local نمی‌تواند خودش تغییر هم‌زمانِ «انتظار» و «پیاده‌سازی» را ممنوع کند.
5. پس از تنظیمات، یک PR آزمایشیِ عمداً ناسازگار بررسی شود تا معلوم شود CI واقعاً آن را می‌بندد؛ نتیجهٔ مورد انتظار باید شکست required check یا نبود review مالک باشد.

## معیار پایان کار Contributor

PR فقط وقتی برای بازبینی نهایی آماده است که: (الف) هر اختلاف کد/قرارداد یا اصلاح مستنداتی با منبع و شواهد تست ارائه شده باشد؛ (ب) تست‌های قابل اجرا سبز و تست‌های skipشده صریح ثبت شده باشند؛ (پ) هیچ تغییر خارج از scope یا تغییر فایل محلی نامرتبط وارد نشده باشد؛ (ت) تصمیم‌های protocol/physical/security به مالک ارجاع شده باشند؛ و (ث) وضعیت واقعی CI و branch protection گزارش شده باشد، نه این‌که از checker محلی به‌عنوان گیت اجباری یاد شود.
