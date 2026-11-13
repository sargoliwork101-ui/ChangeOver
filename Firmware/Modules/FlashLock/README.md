# ماژول FlashLock — قفل یک‌بارمصرف خواندن حافظه

## وضعیت

خاموش. مقدار `MODULE_FLASH_LOCK` در `Firmware/Config/Inc/modules_enable.h` برابر **۰** است:
کد کامپایل می‌شود و تست هاستش در `tools/check_firmware_syntax.sh` اجرا می‌شود، ولی
`func__App_Init` هیچ‌وقت صدایش نمی‌زند و هیچ بایتی از کمینه‌های تراشه نوشته نمی‌شود.

روشن‌کردنش فقط یک کلید است: `MODULE_FLASH_LOCK 1` و یک بیلد.

## تاریخچه

- ۲۰۲۶-۱۰-۲۰ — ساخت ماژول با کلید خاموش، به‌همراه BSP اختصاصی option byte و تست هاست.

## فایل‌ها

| فایل | کار |
|---|---|
| `flash_lock.h` | نتیجهٔ سه‌حالته و دو تابع عمومی |
| `flash_lock.c` | سیاست یک‌بارمصرف؛ هیچ حالتی ذخیره نمی‌کند |
| `Tester/host_test_flash_lock.c` | تست هاست با BSP بدلی (سطح، نوشتن، ریست) |
| `Tester/run_host_test_flash_lock.sh` | اجراکنندهٔ همان تست؛ داخل دروازهٔ syntax |

تسک ندارد: یک‌بار در `func__App_Init` و پیش از شروع زمان‌بند صدا زده می‌شود، چون
پاک‌کردن بلوک option byte مثل پاک‌کردن صفحهٔ فلش، fetch کد را نگه می‌دارد.

## توابع

| تابع | کار |
|---|---|
| `func__FlashLock_IsProtected` | پرسش فقط‌خواندنی: قطعه قفل است یا نه |
| `func__FlashLock_ApplyOnce` | اگر باز بود، سطح ۱ را می‌نویسد و ریست می‌خواهد؛ اگر قفل بود، هیچ کاری نمی‌کند |
| `func__BspFlashLock_GetLevel` (BSP) | خواندن بیت `RDPRT` از ثبات `OBR` |
| `func__BspFlashLock_ApplyLevel1` (BSP) | نوشتن RDP سطح ۱ با حفظ بایت `USER` |
| `func__BspFlashLock_LaunchReset` (BSP) | بارگذاری دوبارهٔ option byte و ریست؛ برنمی‌گردد |

## پایه‌ها

پایهٔ GPIO ندارد. تنها سخت‌افزاری که لمس می‌کند بلوک option byte تراشه است و همهٔ
دسترسی به آن داخل `Firmware/Bsp/Src/bsp_flash_lock.c` می‌ماند؛ این ماژول هیچ رجیستر و
هیچ سرآیند HAL نمی‌بیند.

## پیش‌فرض امن

- `MODULE_FLASH_LOCK = 0`: هیچ نوشتنی روی option byte انجام نمی‌شود.
- اگر نوشتن شکست بخورد، قطعه **باز** می‌ماند و برد باز هم بالا می‌آید؛ قفل‌نشدن
  هرگز باعث بوت‌نشدن نمی‌شود.
- بعد از قفل‌شدن، هر بوت بعدی فقط «قفل است» را می‌بیند و دوباره تلاش نمی‌کند.

## درخت اتصال

```text
Firmware/App/Src/app.c  ──#if MODULE_FLASH_LOCK──►  Modules/FlashLock/flash_lock.h
Modules/FlashLock/flash_lock.c  ──►  Bsp/Inc/bsp_flash_lock.h
Bsp/Src/bsp_flash_lock.c  ──►  stm32f1xx_hal.h  (HAL_FLASHEx_OBProgram / OB_Launch)
Modules/FlashLock/Tester  ──►  کد محصول + BSP بدلی (بدون HAL)
```

نکتهٔ عملیاتی برای تحویل: بعد از قفل‌شدن، هر نشست SWD با پاک‌کردن کامل تراشه شروع
می‌شود؛ یعنی بانک پارامترها در `0x0800E000` و جدول کالیبراسیون در `0x0800F000` هم
می‌روند. اسکریپت `Programmer/flash_changeover.py` همین دو ناحیه را پشتیبان می‌گیرد و
برمی‌گرداند.
