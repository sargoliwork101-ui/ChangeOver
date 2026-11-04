# ابزارهای مرکزی و بررسی مرز پروژه

## چرا checkerها مرکزی هستند

هر checker باید در `tools/` یک نسخه داشته باشد؛ checker میان چند پوشه/ماژول مرز می‌کشد و نباید داخل یکی از طرف‌های همان مرز کپی شود. برای نمونه، checker ماژول‌ها و دادهٔ مرزهای آن‌ها مرکزی هستند و به‌صورت محلی در `Firmware/Modules/<Module>/` تکثیر نمی‌شوند.

| مورد | مسیر | نقش |
| --- | --- | --- |
| checker قرارداد ماژول‌ها | `tools/check_module_contract.py` | بررسی مرکزی وابستگی بین ماژول‌ها |
| دادهٔ مرزهای ماژول‌ها | `tools/module_boundaries.json` | قواعد مشترک و فهرست enforcement ماژول‌ها |
| قرارداد یک ماژول | `Firmware/Modules/<Module>/CONTRACT.md` | وعدهٔ همان ماژول به callerهایش |
| تست یک ماژول | `Firmware/Modules/<Module>/Tester/` | تست‌های همان ماژول |

checker ماژول‌ها در حالت فعلی با `enforced` خالی گزارش می‌دهد و تا وقتی ماژولی عمداً به آن افزوده نشود، strict mode چیزی را enforce نمی‌کند. جزئیات: `python3 tools/check_module_contract.py --help`.

## قراردادهای مرزی سه برد

```sh
python3 tools/check_project_contract.py
python3 tools/check_project_contract.py --json
python3 -m unittest tools/test_check_project_contract.py
```

این checker قراردادهای `ESP_USER`، `ESP_Calibration` و `STM32` را بررسی می‌کند. دو ESP کاربرد جدا دارند و هم‌زمان به STM32 وصل نمی‌شوند. ریشه‌های STM32 در `tools/project_boundaries.json` آمده‌اند؛ `Calibration Data/` یک دادهٔ مشترک کالیبراسیون است، نه ریشهٔ مالکیت یک برد.

Checker بررسی می‌کند که هر ورودی بلافاصله زیر ریشهٔ مخزن به یکی از ریشه‌های سه پروژه، مسیرهای shared/outside، فایل پشتیبان ریشه یا استثنای صریح تخصیص یافته باشد. همچنین routeهای ثبت‌شده در دو اسکچ ESP را با فهرست routeهای قرارداد و ثابت‌های UART مشخص‌شده را با تعریف source مقایسه می‌کند.

این ابزار مالکیت تک‌تک فایل‌های تو‌در‌تو را اثبات نمی‌کند؛ جزئیات جدول ورودی/خروجی HTTP یا کل schema بار UART را با کد تطبیق نمی‌دهد؛ و از تغییر هم‌زمان قرارداد/manifest و source جلوگیری نمی‌کند. بنابراین consistency checker محلی است، نه گیت PR، ممیزی امنیتی یا جایگزین بازبینی مالک پروتکل.

برای اجباری‌کردن آن، CI باید به‌عنوان required status check تنظیم شود و مسیرهای قرارداد/manifest/source نیازمند بازبینی مالک باشند. وجود checker یا workflow به‌تنهایی branch protection ایجاد نمی‌کند. فعلاً این README ادعایی دربارهٔ اجباری‌بودن CI یا branch protection ندارد.

`Tester/` و `Simulator/` در ریشهٔ enclosing board باقی می‌مانند؛ checker فعلی برای آن‌ها مالکیت فایل‌به‌فایل جداگانه ادعا نمی‌کند. `Documentation/`، `tools/` و `Calibration Data/` پشتیبانی مشترک بیرون از سه ریشهٔ برد هستند.
