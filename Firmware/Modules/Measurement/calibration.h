/* ============================================================================
 * @file    calibration.h
 * @brief   [EN] ChangeOver compile-time calibration constants (ADC reference).
 *          [FA] ثابت‌های کالیبراسیون زمان بیلد ChangeOver (مرجع ADC).
 * ----------------------------------------------------------------------------
 * [EN] (2026-10-10) The battery CURRENT tables are NO LONGER compiled in. They are made on
 *      the panel from the bench data and stored in flash (CalLut). Until the
 *      first calibration the board reads through the gain and offset in the
 *      BSP, which start at the MAXIMUM gain and zero offset (safe direction:
 *      a high reading only slows the charge).
 * [FA] جدول‌های جریان باتری دیگر در کد کامپایل نمی‌شوند. روی پنل از داده‌های
 *      بنچ ساخته و در فلش (CalLut) ذخیره می‌شوند. تا اولین کالیبراسیون، برد با
 *      گین و آفست BSP کار می‌کند که از بیشترین گین و آفست صفر شروع می‌کنند
 *      (جهت امن: خوانش بالا فقط شارژ را کند می‌کند).
 * ============================================================================ */

#ifndef CALIBRATION_H
#define CALIBRATION_H

/* ============================================================================
 * [EN] ADC REFERENCE (VDDA) CALIBRATION - the single global scale
 *      Every voltage and every current is counts x VDDA / 4095, so VDDA is the
 *      one term common to all of them. Bench evidence (solo2_dense.csv,
 *      zero-current row) showed the input channel and the 12 V channel both
 *      over-reading by the SAME 1.19 percent - two different dividers, two
 *      different resistor sets, agreeing to 5 parts in 100000. Only a shared
 *      term can do that, and the implied real VDDA is about 3261 mV.
 *      That is also the mathematical reason the board "never calibrates": the
 *      error is a GAIN, and the only runtime calibration the product exposes
 *      (params 4/5/6) is an ADDER, which can only be right at one point.
 *
 *      HOW TO CALIBRATE THIS BOARD, once:
 *        1. Flash, then read UINT32_T__G__MeasVddaMv (it is published even
 *           while tracking is off - same watch window as MeasBattery24Mv).
 *        2. Put a DMM on VDDA / the 3.3 V rail.
 *        3. CAL_VREFINT_MV = 1200 * (DMM_VDDA_mV / MeasVddaMv_reading).
 *        4. Set CAL_VDDA_TRACKING_ENABLE to 1, rebuild, re-check all three
 *           voltages against the DMM.
 *      After that the board follows supply and temperature drift by itself.
 *
 *      WHY TRACKING SHIPS OFF: the STM32F103 stores no factory VREFINT
 *      calibration, so before step 3 the 1.20 V is only guaranteed to
 *      1.16..1.24 V (+-3.3 percent) - worse than the error being corrected.
 *      Turning it on un-calibrated would also make every reading LOWER, which
 *      moves the over-voltage cut LATER. Reading high is the safe direction.
 * [FA] کالیبراسیون مرجع ADC - تنها مقیاس سراسری.
 *      هر ولتاژ و هر جریان برابر ‎counts x VDDA / 4095‎ است. شاهد بنچ نشان داد
 *      کانال ورودی و کانال ۱۲ولت هر دو دقیقاً ۱٫۱۹٪ زیاد می‌خوانند - دو مقسم
 *      متفاوت که تا پنج در صدهزار یکی‌اند؛ فقط جملهٔ مشترک چنین می‌کند و یعنی
 *      VDDA واقعی حدود ۳۲۶۱ است. دلیل ریاضی «کالیبره نشدن» هم همین است: خطا
 *      ضربی است و تنها کالیبراسیون موجود جمعی.
 *      روش کالیبره (یک‌بار): مقدار UINT32_T__G__MeasVddaMv را بخوانید، با
 *      مولتی‌متر VDDA را اندازه بگیرید، CAL_VREFINT_MV را برابر
 *      1200 × (VDDA مولتی‌متر ÷ خوانده‌شده) بگذارید، بعد
 *      CAL_VDDA_TRACKING_ENABLE را ۱ کنید و هر سه ولتاژ را دوباره بسنجید.
 *      چرا خاموش عرضه می‌شود: F103 کالیبراسیون کارخانه‌ای VREFINT ندارد و پیش
 *      از مرحلهٔ کالیبره فقط ۱٫۱۶ تا ۱٫۲۴ ولت تضمین شده (±۳٫۳٪) که از خطای
 *      فعلی بدتر است؛ ضمناً روشن‌کردنِ کالیبره‌نشده همهٔ خوانش‌ها را کم می‌کند و
 *      قطع اضافه‌ولتاژ را دیرتر می‌اندازد. خواندن کمی بالا جهت امن است.
 * ============================================================================ */
#define CAL_VREFINT_MV                 1200u
#define CAL_VDDA_TRACKING_ENABLE          0u

#endif /* CALIBRATION_H */
