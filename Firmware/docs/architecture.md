# معماری اسکلت

## چرا main خالی؟

CubeMX فایل `main.c` را هر بار که کد تولید کند عوض می‌کند. اگر منطق محصول آنجا باشد، در اولین Generate دوباره می‌پرد.

پس قرارداد این است:

- `Core/` مال CubeMX است.
- `Firmware/` مال ما است و CubeMX نباید آن را بازنویسی کند.
- پل بین این دو فقط `App_Start()` است.

## جریان اجرا

```text
Reset
  HAL_Init
  SystemClock_Config
  MX_GPIO_Init / MX_ADC / MX_TIM / MX_USART / MX_DMA
  App_Start
      Actuator_Init          ← خروجی قدرت در حالت امن
      Fault_Init
      ماژول‌های Enable شده Init
      ساخت Taskهای استاتیک
      vTaskStartScheduler
         task_ui             100 ms
         task_measurement    وقتی Enable شود
         task_protection
         task_control
         task_comm
```

## وابستگی ماژول‌ها

```text
task_control
    │
    ├─ Measurement_GetSnapshot()
    ├─ Protection_Evaluate()
    ├─ Changeover_Evaluate()
    ├─ Charger_Evaluate()
    └─ Actuator_*()            فقط اینجا خروجی قدرت نوشته می‌شود

task_protection                سریع‌تر از control
    └─ Fault_Set() + Actuator_EnterSafeState()

task_ui
    └─ Ui_Show(state, faults)  فقط LED و بازر

task_comm
    └─ EspLink_*               خاموش تا UART تست شود
```

ماژول پایین‌دستی حق ندارد ماژول بالادستی را صدا بزند. مثال: `Ui` نباید `Charger` را صدا بزند؛ Task یا App وضعیت را می‌دهد.

## کم و زیاد کردن قابلیت

1. فلگ در `modules_enable.h` را ۰ یا ۱ کن.
2. اگر صفر شد، `App` و `Rtos` آن Init/Task را نمی‌سازند.
3. فایل‌های ماژول می‌توانند در پروژه بمانند؛ لینک نمی‌شوند اگر از لیست Source حذف شوند.

برای حذف کامل شارژر:

- `MODULE_CHARGER 0`
- فایل‌های `Modules/Charger/` را از Build بردار.

`Actuator` می‌ماند تا رله و PWM در حالت قطع بمانند.

## حافظه

Task، Queue، Mutex در این اسکلت **استاتیک** تعریف می‌شوند. `malloc` در Application استفاده نمی‌شود.

## اعداد

ولتاژ و جریان در ماژول‌ها `millivolt` / `milliamp` هستند، نه `float`. ضریب ADC تا کالیبراسیون در `app_config.h` به‌صورت Placeholder است و نباید برای کنترل توان جدی گرفته شود.
