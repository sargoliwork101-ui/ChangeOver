# MISRA Deviation Record

این فایل تا زمان انتخاب ابزار و نسخه‌ی دقیق Rule Set، قالب اولیه است.

| ID | Rule | بخش | وضعیت | دلیل | کنترل جبرانی |
|---|---|---|---|---|---|
| DEV-001 | Third-party scope | STM32 HAL | Open | کد vendor خارج از مالکیت پروژه است | تحلیل Wrapperها و بررسی Return Value |
| DEV-002 | Third-party scope | FreeRTOS | Open | هسته‌ی RTOS خارجی است | استفاده از API محدود و Static Allocation |
| DEV-003 | Tool-specific | CubeMX generated code | Open | توسط ابزار تولید می‌شود | جلوگیری از ویرایش مستقیم و ثبت نسخه‌ی ابزار |
| DEV-004 | API constness | `uart_driver.c` | Open | HAL UART با Buffer غیرconst تعریف شده است | طول و Pointer قبل از ارسال بررسی می‌شود؛ API HAL باید در Scope جدا تحلیل شود |
| DEV-005 | Terminal loop | `freertos_hooks.c` | Open | Stack overflow باید سیستم را در حالت متوقف/امن نگه دارد | Watchdog، نشانگر سخت‌افزاری و بازبینی ایمنی لازم است |
| DEV-006 | RTOS task loop | `RTOS/Src/*_task.c` | Open | Taskها ذاتاً چرخه‌ی بی‌نهایت زمان‌بندی‌شده دارند | فقط با APIهای RTOS delay/block می‌شوند و خروجی‌های توان Safe State دارند |
| DEV-007 | Terminal diagnostic write | `diagnostics_report_emergency` | Open | Stack Overflow Hook نباید Mutex بگیرد یا Block کند | فقط در مسیر terminal fault استفاده می‌شود و بلافاصله Safe State/loop اجرا می‌گردد |

## فرمت اضافه‌کردن Deviation

```text
ID:
Rule:
File / Function:
Reason:
Risk:
Mitigation:
Owner:
Review date:
Status:
```

تا وقتی ابزار تحلیل و گزارش واقعی نداریم، وضعیت پروژه «هدف MISRA C» است و نه «MISRA Compliant».
