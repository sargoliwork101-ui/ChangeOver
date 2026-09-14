/**
 * @file    README.md
 * @brief   [EN] CubeMX .ioc lives here. Not product firmware.
 *          [FA] فایل .ioc مکعب اینجاست. کد محصول نیست.
 */

# CubeMX

فقط فایل تنظیمات مکعب: `CubeIDE.ioc` (نام پروژه مکعب `CubeIDE` است)

`Firmware` را اینجا نگذار. `Core` / `Drivers` هم اینجا نماند.

## Save / Generate

1. STM32CubeMX را **جدا** باز کن
2. Project Manager:
   - Name: `CubeIDE`
   - Location: یک سطح بالاتر — ریشهٔ ریپو `ChangeOver`
   - Toolchain: **STM32CubeIDE**
3. GENERATE CODE → خروجی می‌رود به `../CubeIDE/`
4. فایل `.ioc` را در **همین پوشه** هم بگذار (کپی از `../CubeIDE/*.ioc`) تا جای مکعب گم نشود

الان فقط LED/بازر. ADC و PWM را Enable نکن.
