# CubeMX Setup

## Peripheralهای لازم

- ADC1: Scan + DMA با پنج Rank مطابق `README.md`
- TIM2_CH1 روی PA0 برای PWM1
- TIM3_CH1 روی PA6 برای PWM2
- USART1 روی PA9/PA10
- EXTI روی PB2، PB4 و PB6
- GPIO Output روی PA4، PA8، PB0، PB1، PB5، PB7، PB10 و PB11
- Debug: Serial Wire
- FreeRTOS: **Native FreeRTOS توصیه می‌شود**؛ برای CMSIS-RTOS2 باید Static Memory Attributeها جداگانه تنظیم شوند. در هر دو حالت، Static Allocation فعال و Dynamic Allocation غیرفعال باشد.

## Handleهای مورد انتظار

```text
hadc1
htim2
htim3
huart1
```

اگر CubeMX نام دیگری تولید کرد، `Integration/Src/firmware_entry.c` را اصلاح کن.

## ADC

توصیه‌ی اولیه برای شروع، DMA با Buffer پنج‌عضوی و Trigger مشخص است. برای کنترل دقیق‌تر جریان، Trigger تایمری بهتر از Continuous Conversion آزاد است. انتخاب نهایی به فرکانس نمونه‌برداری و کنترل CC/CV بستگی دارد.
